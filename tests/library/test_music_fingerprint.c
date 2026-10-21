#include <stdio.h>
#include <string.h>

#include "check.h"
#include "library/music_fingerprint.h"

enum {
    MAX_NODES = 64
};

typedef struct Node {
    const char *parent;
    const char *name;
    bool directory;
    uint32_t size;
    uint32_t modified;
} Node;

typedef struct Tree {
    Node nodes[MAX_NODES];
    int count;
    const char *open_paths[FINGERPRINT_MAX_DEPTH];
    int cursors[FINGERPRINT_MAX_DEPTH];
    int opens;
    int open_dirs;
    int most_open;
} Tree;

static Tree tree;
static FingerprintWork work;
static char paths[FINGERPRINT_MAX_DEPTH][FINGERPRINT_PATH_CAPACITY];

static bool open_dir(void *context, uint32_t level, const char *path)
{
    Tree *state = context;
    for (int index = 0; index < state->count; index++) {
        if (strcmp(state->nodes[index].parent, path) == 0 || strcmp(path, "/") == 0) {
            strncpy(paths[level], path, FINGERPRINT_PATH_CAPACITY - 1);
            state->open_paths[level] = paths[level];
            state->cursors[level] = 0;
            state->opens++;
            state->open_dirs++;
            state->most_open =
                state->open_dirs > state->most_open ? state->open_dirs : state->most_open;
            return true;
        }
    }
    return false;
}

static bool next_entry(void *context, uint32_t level, FingerprintEntry *entry)
{
    Tree *state = context;
    while (state->cursors[level] < state->count) {
        const Node *node = &state->nodes[state->cursors[level]++];
        if (strcmp(node->parent, state->open_paths[level]) == 0) {
            entry->directory = node->directory;
            entry->size = node->size;
            entry->modified = node->modified;
            strncpy(entry->name, node->name, sizeof entry->name - 1);
            entry->name[sizeof entry->name - 1] = '\0';
            return true;
        }
    }
    return false;
}

static void close_dir(void *context, uint32_t level)
{
    Tree *state = context;
    state->open_paths[level] = NULL;
    state->open_dirs--;
}

static const FingerprintFiles FILES = {open_dir, next_entry, close_dir, &tree};

static void add(const char *parent, const char *name, bool directory, uint32_t size,
                uint32_t modified)
{
    tree.nodes[tree.count++] = (Node){parent, name, directory, size, modified};
}

static void music_card(void)
{
    memset(&tree, 0, sizeof tree);
    add("/", "AC DC", true, 0, 1);
    add("/AC DC", "Back In Black", true, 0, 2);
    add("/AC DC/Back In Black", "01 - Hells Bells.flac", false, 31000000, 100);
    add("/AC DC/Back In Black", "02 - Shoot to Thrill.flac", false, 29000000, 101);
    add("/AC DC/Back In Black", "cover.jpg", false, 80000, 102);
    add("/", "Mix.m3u8", false, 400, 3);
    add("/", ".echo", true, 0, 4);
    add("/.echo", "library.db", false, 4194304, 5);
}

static MusicFingerprint fingerprint(void)
{
    return music_fingerprint(&FILES, &work);
}

static Node *node_named(const char *name)
{
    for (int index = 0; index < tree.count; index++) {
        if (strcmp(tree.nodes[index].name, name) == 0) {
            return &tree.nodes[index];
        }
    }
    return NULL;
}

static void the_same_card_gives_the_same_fingerprint(void)
{
    music_card();
    MusicFingerprint first = fingerprint();
    MusicFingerprint second = fingerprint();
    CHECK(first.complete);
    CHECK(music_fingerprints_match(first, second));
    CHECK_EQUAL(6u, first.entries);
}

static void the_test_card_keeps_its_stored_fingerprint(void)
{
    music_card();
    MusicFingerprint print = fingerprint();
    CHECK(print.complete);
    CHECK_EQUAL(3518657026u, print.first);
    CHECK_EQUAL(2077149385u, print.second);
    CHECK_EQUAL(6u, print.entries);
}

static void a_folder_that_cannot_be_opened_counts_as_empty(void)
{
    music_card();
    add("/", "Empty", true, 0, 6);
    add("/AC DC", "Live", true, 0, 7);
    MusicFingerprint print = fingerprint();
    CHECK(print.complete);
    CHECK_EQUAL(732559337u, print.first);
    CHECK_EQUAL(1706284886u, print.second);
    CHECK_EQUAL(8u, print.entries);
}

static void hidden_entries_do_not_count(void)
{
    music_card();
    MusicFingerprint before = fingerprint();
    add("/", ".fseventsd", true, 0, 9);
    add("/.fseventsd", "0000000001", false, 77, 9);
    add("/AC DC/Back In Black", "._01 - Hells Bells.flac", false, 4096, 9);
    node_named("library.db")->modified = 999;
    CHECK(music_fingerprints_match(before, fingerprint()));
}

static void a_retagged_file_changes_it(void)
{
    music_card();
    MusicFingerprint before = fingerprint();
    node_named("02 - Shoot to Thrill.flac")->modified = 500;
    CHECK(!music_fingerprints_match(before, fingerprint()));
}

static void a_resized_file_changes_it(void)
{
    music_card();
    MusicFingerprint before = fingerprint();
    node_named("cover.jpg")->size = 81000;
    CHECK(!music_fingerprints_match(before, fingerprint()));
}

static void a_new_or_renamed_file_changes_it(void)
{
    music_card();
    MusicFingerprint before = fingerprint();
    add("/AC DC/Back In Black", "03 - What Do You Do for Money Honey.flac", false, 28000000, 103);
    CHECK(!music_fingerprints_match(before, fingerprint()));
    music_card();
    node_named("Mix.m3u8")->name = "Mix 2.m3u8";
    CHECK(!music_fingerprints_match(before, fingerprint()));
}

static void moving_a_file_to_another_folder_changes_it(void)
{
    music_card();
    MusicFingerprint before = fingerprint();
    node_named("cover.jpg")->parent = "/AC DC";
    CHECK(!music_fingerprints_match(before, fingerprint()));
}

static void each_folder_is_opened_once(void)
{
    music_card();
    fingerprint();
    CHECK_EQUAL(3, tree.opens);
}

static void every_folder_is_closed_after_the_walk(void)
{
    music_card();
    fingerprint();
    CHECK_EQUAL(3, tree.most_open);
    CHECK_EQUAL(0, tree.open_dirs);
}

static void a_card_deeper_than_the_limit_is_incomplete_and_closed(void)
{
    static char parents[FINGERPRINT_MAX_DEPTH + 2][FINGERPRINT_PATH_CAPACITY];
    memset(&tree, 0, sizeof tree);
    strcpy(parents[0], "/");
    for (int level = 0; level <= FINGERPRINT_MAX_DEPTH; level++) {
        add(parents[level], "d", true, 0, 1);
        snprintf(parents[level + 1], FINGERPRINT_PATH_CAPACITY, "%s%sd", parents[level],
                 level == 0 ? "" : "/");
    }
    CHECK(!fingerprint().complete);
    CHECK_EQUAL(0, tree.open_dirs);
}

static void a_card_without_a_root_is_not_complete(void)
{
    memset(&tree, 0, sizeof tree);
    CHECK(!fingerprint().complete);
}

static void incomplete_fingerprints_never_match(void)
{
    MusicFingerprint broken = {.complete = false};
    CHECK(!music_fingerprints_match(broken, broken));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_same_card_gives_the_same_fingerprint),
        TEST_CASE(the_test_card_keeps_its_stored_fingerprint),
        TEST_CASE(a_folder_that_cannot_be_opened_counts_as_empty),
        TEST_CASE(hidden_entries_do_not_count),
        TEST_CASE(a_retagged_file_changes_it),
        TEST_CASE(a_resized_file_changes_it),
        TEST_CASE(a_new_or_renamed_file_changes_it),
        TEST_CASE(moving_a_file_to_another_folder_changes_it),
        TEST_CASE(each_folder_is_opened_once),
        TEST_CASE(every_folder_is_closed_after_the_walk),
        TEST_CASE(a_card_deeper_than_the_limit_is_incomplete_and_closed),
        TEST_CASE(a_card_without_a_root_is_not_complete),
        TEST_CASE(incomplete_fingerprints_never_match),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
