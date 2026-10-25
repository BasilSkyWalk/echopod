#include "library_compare.h"

#include <stdio.h>
#include <string.h>

#include "library/library.h"

enum {
    TEXT_CAPACITY = 2048
};

typedef struct Image {
    const uint8_t *bytes;
    uint32_t size;
} Image;

typedef struct Pair {
    Library expected;
    Library actual;
    bool same;
} Pair;

static bool read_image(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const Image *image = context;
    if (offset > image->size || length > image->size - offset) {
        return false;
    }
    memcpy(buffer, image->bytes + offset, length);
    return true;
}

static void differ(Pair *pair, const char *what, uint32_t index, const char *field)
{
    if (pair->same) {
        fprintf(stderr, "    library differs: %s %u %s\n", what, index, field);
    }
    pair->same = false;
}

static void same_number(Pair *pair, const char *what, uint32_t index, const char *field,
                        uint32_t expected, uint32_t actual)
{
    if (expected != actual) {
        differ(pair, what, index, field);
        fprintf(stderr, "    expected %u, got %u\n", expected, actual);
    }
}

static void same_text(Pair *pair, const char *what, uint32_t index, const char *field,
                      uint32_t expected, uint32_t actual)
{
    static char left[TEXT_CAPACITY];
    static char right[TEXT_CAPACITY];
    bool read = library_read_string(&pair->expected, expected, left, sizeof left) &&
                library_read_string(&pair->actual, actual, right, sizeof right);
    if (!read || strcmp(left, right) != 0) {
        differ(pair, what, index, field);
        fprintf(stderr, "    expected \"%s\", got \"%s\"\n", read ? left : "?", read ? right : "?");
    }
}

static void same_links(Pair *pair, const char *what, uint32_t index, uint32_t expected_first,
                       uint32_t actual_first, uint32_t count)
{
    for (uint32_t link = 0; link < count; link++) {
        uint16_t left = 0;
        uint16_t right = 0;
        if (!library_read_index(&pair->expected, LIBRARY_LINKS, expected_first + link, &left) ||
            !library_read_index(&pair->actual, LIBRARY_LINKS, actual_first + link, &right) ||
            left != right) {
            differ(pair, what, index, "links");
            fprintf(stderr, "    link %u: expected %u, got %u\n", link, left, right);
            return;
        }
    }
}

static void compare_tracks(Pair *pair)
{
    for (uint32_t index = 0; index < library_count(&pair->expected, LIBRARY_TRACKS); index++) {
        LibraryTrack left;
        LibraryTrack right;
        if (!library_read_track(&pair->expected, index, &left) ||
            !library_read_track(&pair->actual, index, &right)) {
            differ(pair, "track", index, "unreadable");
            return;
        }
        same_text(pair, "track", index, "title", left.title, right.title);
        same_text(pair, "track", index, "path", left.path, right.path);
        same_number(pair, "track", index, "album", left.album, right.album);
        same_number(pair, "track", index, "artist", left.artist, right.artist);
        same_number(pair, "track", index, "genre", left.genre, right.genre);
        same_number(pair, "track", index, "folder", left.folder, right.folder);
        same_number(pair, "track", index, "duration", left.duration_ms, right.duration_ms);
        same_number(pair, "track", index, "disc", left.disc, right.disc);
        same_number(pair, "track", index, "number", left.number, right.number);
    }
}

static void compare_albums(Pair *pair)
{
    for (uint32_t index = 0; index < library_count(&pair->expected, LIBRARY_ALBUMS); index++) {
        LibraryAlbum left;
        LibraryAlbum right;
        if (!library_read_album(&pair->expected, index, &left) ||
            !library_read_album(&pair->actual, index, &right)) {
            differ(pair, "album", index, "unreadable");
            return;
        }
        same_text(pair, "album", index, "title", left.title, right.title);
        same_number(pair, "album", index, "album artist", left.album_artist, right.album_artist);
        same_number(pair, "album", index, "genre", left.genre, right.genre);
        same_number(pair, "album", index, "year", left.year, right.year);
        same_number(pair, "album", index, "first track", left.first_track, right.first_track);
        same_number(pair, "album", index, "track count", left.track_count, right.track_count);
        same_number(pair, "album", index, "cover", left.cover, right.cover);
    }
}

static void compare_groups(Pair *pair, LibrarySection section, const char *what)
{
    for (uint32_t index = 0; index < library_count(&pair->expected, section); index++) {
        LibraryGroup left;
        LibraryGroup right;
        if (!library_read_group(&pair->expected, section, index, &left) ||
            !library_read_group(&pair->actual, section, index, &right)) {
            differ(pair, what, index, "unreadable");
            return;
        }
        same_text(pair, what, index, "name", left.name, right.name);
        same_number(pair, what, index, "link count", left.link_count, right.link_count);
        same_links(pair, what, index, left.first_link, right.first_link, left.link_count);
    }
}

static void compare_folders(Pair *pair)
{
    for (uint32_t index = 0; index < library_count(&pair->expected, LIBRARY_FOLDERS); index++) {
        LibraryFolder left;
        LibraryFolder right;
        if (!library_read_folder(&pair->expected, index, &left) ||
            !library_read_folder(&pair->actual, index, &right)) {
            differ(pair, "folder", index, "unreadable");
            return;
        }
        same_text(pair, "folder", index, "name", left.name, right.name);
        same_number(pair, "folder", index, "parent", left.parent, right.parent);
        same_number(pair, "folder", index, "subfolders", left.subfolder_count,
                    right.subfolder_count);
        same_number(pair, "folder", index, "tracks", left.track_count, right.track_count);
        same_links(pair, "folder", index, left.first_link, right.first_link,
                   (uint32_t)left.subfolder_count + left.track_count);
    }
}

static void compare_title_order(Pair *pair)
{
    for (uint32_t index = 0; index < library_count(&pair->expected, LIBRARY_TITLE_ORDER); index++) {
        uint16_t left = 0;
        uint16_t right = 0;
        if (!library_read_index(&pair->expected, LIBRARY_TITLE_ORDER, index, &left) ||
            !library_read_index(&pair->actual, LIBRARY_TITLE_ORDER, index, &right) ||
            left != right) {
            differ(pair, "title order", index, "position");
            return;
        }
    }
}

static void compare_counts(Pair *pair)
{
    static const char *const NAMES[LIBRARY_SECTION_COUNT] = {
        "tracks",  "albums",      "artists",   "album artists", "genres",
        "folders", "title order", "playlists", "links",         "strings"};
    for (int section = 0; section < LIBRARY_STRINGS; section++) {
        same_number(pair, "section", (uint32_t)section, NAMES[section],
                    library_count(&pair->expected, (LibrarySection)section),
                    library_count(&pair->actual, (LibrarySection)section));
        same_number(pair, "music", (uint32_t)section, NAMES[section],
                    library_music_count(&pair->expected, (LibrarySection)section),
                    library_music_count(&pair->actual, (LibrarySection)section));
    }
}

bool library_compare(const uint8_t *expected, uint32_t expected_size, const uint8_t *actual,
                     uint32_t actual_size)
{
    Image left = {expected, expected_size};
    Image right = {actual, actual_size};
    Pair pair = {.same = true};
    if (library_open(&pair.expected, (ByteSource){read_image, &left}, expected_size) !=
            LIBRARY_OK ||
        library_open(&pair.actual, (ByteSource){read_image, &right}, actual_size) != LIBRARY_OK) {
        fprintf(stderr, "    library differs: one side doesn't open\n");
        return false;
    }
    compare_counts(&pair);
    if (!pair.same) {
        return false;
    }
    compare_tracks(&pair);
    compare_albums(&pair);
    compare_groups(&pair, LIBRARY_ARTISTS, "artist");
    compare_groups(&pair, LIBRARY_ALBUM_ARTISTS, "album artist");
    compare_groups(&pair, LIBRARY_GENRES, "genre");
    compare_groups(&pair, LIBRARY_PLAYLISTS, "playlist");
    compare_folders(&pair);
    compare_title_order(&pair);
    return pair.same;
}
