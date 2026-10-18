#include <stddef.h>

#include "builder_internal.h"
#include "core/hash.h"
#include "library/sort_key.h"

enum {
    PATH_CAPACITY = 1024,
    MAX_PARTS = 64,
    MAX_DEPTH = 64,
    EMPTY_SLOT = 0
};

typedef struct Span {
    uint32_t start;
    uint32_t length;
} Span;

typedef struct Parts {
    char text[PATH_CAPACITY];
    Span spans[MAX_PARTS];
    uint32_t count;
} Parts;

typedef struct ResolveScratch {
    char entry[PATH_CAPACITY];
    char joined[PATH_CAPACITY];
    char normal[PATH_CAPACITY];
    char folded[PATH_CAPACITY];
    char track[PATH_CAPACITY];
    Parts raw;
    Parts wanted;
    Parts candidate;
    Span kept[MAX_PARTS];
} ResolveScratch;

static ResolveScratch *scratch_of(const LibraryModel *model)
{
    return model->paths.scratch;
}

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint32_t power_of_two_above(uint32_t value)
{
    uint32_t result = 16;
    while (result < value * 2) {
        result *= 2;
    }
    return result;
}

static uint32_t append_folded(char *out, uint32_t used, const char *text, uint32_t length)
{
    if (used >= PATH_CAPACITY - 1) {
        return used;
    }
    return used + (uint32_t)case_fold_make(text, length, out + used, PATH_CAPACITY - used);
}

static uint32_t folded_track_path(const LibraryBuilder *builder, const TrackRecord *track,
                                  char *out)
{
    uint16_t chain[MAX_DEPTH];
    uint32_t depth = 0;
    for (uint16_t node = track->folder; node != 0 && depth < MAX_DEPTH;
         node = builder_folder(builder, node)->parent) {
        chain[depth++] = node;
    }
    uint32_t used = 0;
    out[0] = '\0';
    while (depth > 0) {
        const char *name = builder_text(builder, builder_folder(builder, chain[--depth])->name);
        used = append_folded(out, used, name, text_length(name));
        used = used < PATH_CAPACITY - 1 ? used + 1 : used;
        out[used - 1] = '/';
        out[used] = '\0';
    }
    const char *file = builder_text(builder, track->file_name);
    return append_folded(out, used, file, text_length(file));
}

static const TrackRecord *track_at(const LibraryBuilder *builder, const LibraryModel *model,
                                   uint32_t position)
{
    return builder_track(builder, model->final_tracks[position]);
}

static uint32_t folded_name_hash(const LibraryBuilder *builder, const TrackRecord *track,
                                 char *folded)
{
    const char *file = builder_text(builder, track->file_name);
    uint32_t length = (uint32_t)case_fold_make(file, text_length(file), folded, PATH_CAPACITY);
    return hash_update(HASH_START, folded, length);
}

static bool same_folded_path(const LibraryBuilder *builder, const LibraryModel *model,
                             uint32_t position, const char *wanted, uint32_t length)
{
    char *folded = scratch_of(model)->track;
    uint32_t found = folded_track_path(builder, track_at(builder, model, position), folded);
    if (found != length) {
        return false;
    }
    for (uint32_t index = 0; index < length; index++) {
        if (folded[index] != wanted[index]) {
            return false;
        }
    }
    return true;
}

static void index_exact(const LibraryBuilder *builder, LibraryModel *model, uint32_t position)
{
    char *folded = scratch_of(model)->folded;
    uint32_t length = folded_track_path(builder, track_at(builder, model, position), folded);
    uint32_t hash = hash_update(HASH_START, folded, length);
    for (uint32_t probe = 0;; probe++) {
        uint16_t *slot = &model->paths.exact[(hash + probe) & model->paths.exact_mask];
        if (*slot == EMPTY_SLOT || same_folded_path(builder, model, *slot - 1u, folded, length)) {
            *slot = (uint16_t)(position + 1);
            return;
        }
    }
}

static void index_name(const LibraryBuilder *builder, LibraryModel *model, uint32_t position)
{
    uint32_t hash =
        folded_name_hash(builder, track_at(builder, model, position), scratch_of(model)->folded);
    uint16_t *head = &model->paths.by_name[hash & model->paths.by_name_mask];
    model->paths.next_same_name[position] = *head;
    *head = (uint16_t)(position + 1);
}

bool builder_index_paths(const LibraryBuilder *builder, Work *work, LibraryModel *model)
{
    uint32_t count = model->track_count;
    uint32_t slots = power_of_two_above(count);
    model->paths.exact = work_alloc(work, slots * 2);
    model->paths.by_name = work_alloc(work, slots * 2);
    model->paths.next_same_name = work_alloc(work, count * 2 + 2);
    model->paths.scratch = work_alloc(work, (uint32_t)sizeof(ResolveScratch));
    if (work->failed) {
        return false;
    }
    model->paths.exact_mask = slots - 1;
    model->paths.by_name_mask = slots - 1;
    for (uint32_t slot = 0; slot < slots; slot++) {
        model->paths.exact[slot] = EMPTY_SLOT;
        model->paths.by_name[slot] = EMPTY_SLOT;
    }
    for (uint32_t position = 0; position < count; position++) {
        index_exact(builder, model, position);
        index_name(builder, model, position);
    }
    return true;
}

static bool contains_url(const char *entry)
{
    for (const char *cursor = entry; cursor[0] != '\0'; cursor++) {
        if (cursor[0] == ':' && cursor[1] == '/' && cursor[2] == '/') {
            return true;
        }
    }
    return false;
}

static void split_parts(const char *path, uint32_t length, Parts *parts)
{
    parts->count = 0;
    uint32_t start = 0;
    for (uint32_t index = 0; index <= length && parts->count < MAX_PARTS; index++) {
        if (index < length && path[index] != '/') {
            continue;
        }
        parts->spans[parts->count] = (Span){.start = start, .length = index - start};
        parts->count++;
        start = index + 1;
    }
}

static bool is_dots(const char *text, Span span, uint32_t dots)
{
    if (span.length != dots) {
        return false;
    }
    for (uint32_t index = 0; index < dots; index++) {
        if (text[span.start + index] != '.') {
            return false;
        }
    }
    return true;
}

static uint32_t leading_slashes(const char *path, uint32_t length)
{
    if (length >= 2 && path[0] == '/' && path[1] == '/' && (length < 3 || path[2] != '/')) {
        return 2;
    }
    return length >= 1 && path[0] == '/' ? 1 : 0;
}

static uint32_t keep_components(const char *path, const Parts *raw, uint32_t initial, Span *kept)
{
    uint32_t count = 0;
    for (uint32_t index = 0; index < raw->count; index++) {
        Span span = raw->spans[index];
        if (span.length == 0 || is_dots(path, span, 1)) {
            continue;
        }
        bool parent = is_dots(path, span, 2);
        bool keep_parent =
            (initial == 0 && count == 0) || (count > 0 && is_dots(path, kept[count - 1], 2));
        if (!parent || keep_parent) {
            kept[count++] = span;
        } else if (count > 0) {
            count--;
        }
    }
    return count;
}

static uint32_t copy_span(char *out, uint32_t used, const char *path, Span span)
{
    for (uint32_t byte = 0; byte < span.length && used + 1 < PATH_CAPACITY; byte++) {
        out[used++] = path[span.start + byte];
    }
    return used;
}

static uint32_t normalize(ResolveScratch *scratch, const char *path, uint32_t length)
{
    char *out = scratch->normal;
    split_parts(path, length, &scratch->raw);
    uint32_t initial = leading_slashes(path, length);
    uint32_t count = keep_components(path, &scratch->raw, initial, scratch->kept);
    uint32_t used = 0;
    for (uint32_t index = 0; index < initial; index++) {
        out[used++] = '/';
    }
    for (uint32_t index = 0; index < count; index++) {
        used = copy_span(out, used, path, scratch->kept[index]);
        if (index + 1 < count && used + 1 < PATH_CAPACITY) {
            out[used++] = '/';
        }
    }
    if (used == 0) {
        out[used++] = '.';
    }
    out[used] = '\0';
    return used;
}

static uint32_t find_exact(const LibraryBuilder *builder, const LibraryModel *model,
                           const char *candidate, uint32_t length)
{
    ResolveScratch *scratch = scratch_of(model);
    char *folded = scratch->folded;
    uint32_t normal_length = normalize(scratch, candidate, length);
    uint32_t folded_length =
        (uint32_t)case_fold_make(scratch->normal, normal_length, folded, PATH_CAPACITY);
    uint32_t hash = hash_update(HASH_START, folded, folded_length);
    for (uint32_t probe = 0; probe <= model->paths.exact_mask; probe++) {
        uint16_t slot = model->paths.exact[(hash + probe) & model->paths.exact_mask];
        if (slot == EMPTY_SLOT) {
            return BUILDER_UNSET;
        }
        if (same_folded_path(builder, model, slot - 1u, folded, folded_length)) {
            return slot - 1u;
        }
    }
    return BUILDER_UNSET;
}

static uint32_t join(const char *folder, const char *path, uint32_t length, char *out)
{
    uint32_t used = 0;
    if (path[0] != '/' && folder[0] != '\0') {
        for (; folder[used] != '\0' && used < PATH_CAPACITY / 2; used++) {
            out[used] = folder[used];
        }
        if (out[used - 1] != '/') {
            out[used++] = '/';
        }
    }
    for (uint32_t index = 0; index < length && used + 1 < PATH_CAPACITY; index++) {
        out[used++] = path[index];
    }
    out[used] = '\0';
    return used;
}

static uint32_t fold_parts(Parts *raw, const char *path, uint32_t length, Parts *parts)
{
    split_parts(path, length, raw);
    uint32_t used = 0;
    parts->count = 0;
    for (uint32_t index = 0; index < raw->count; index++) {
        Span span = raw->spans[index];
        if (span.length == 0 || is_dots(path, span, 1) || is_dots(path, span, 2)) {
            continue;
        }
        uint32_t folded = (uint32_t)case_fold_make(path + span.start, span.length,
                                                   parts->text + used, PATH_CAPACITY - used);
        parts->spans[parts->count++] = (Span){.start = used, .length = folded};
        used += folded < PATH_CAPACITY - used ? folded : 0;
    }
    return parts->count;
}

static bool same_span(const Parts *left, Span a, const Parts *right, Span b)
{
    if (a.length != b.length) {
        return false;
    }
    for (uint32_t index = 0; index < a.length; index++) {
        if (left->text[a.start + index] != right->text[b.start + index]) {
            return false;
        }
    }
    return true;
}

static uint32_t common_suffix(const Parts *left, const Parts *right)
{
    uint32_t length = 0;
    while (length < left->count && length < right->count &&
           same_span(left, left->spans[left->count - 1 - length], right,
                     right->spans[right->count - 1 - length])) {
        length++;
    }
    return length;
}

typedef struct Best {
    uint32_t score;
    uint32_t position;
    uint32_t at_score;
} Best;

static void consider(Best *best, uint32_t score, uint32_t position)
{
    if (best->at_score == 0 || score > best->score) {
        *best = (Best){.score = score, .position = position, .at_score = 1};
    } else if (score == best->score) {
        best->at_score++;
        best->position = position > best->position ? position : best->position;
    }
}

static uint32_t find_by_suffix(const LibraryBuilder *builder, const LibraryModel *model)
{
    ResolveScratch *scratch = scratch_of(model);
    const Parts *wanted = &scratch->wanted;
    Span last = wanted->spans[wanted->count - 1];
    uint32_t hash = hash_update(HASH_START, wanted->text + last.start, last.length);
    Best best = {.at_score = 0};
    for (uint16_t link = model->paths.by_name[hash & model->paths.by_name_mask]; link != EMPTY_SLOT;
         link = model->paths.next_same_name[link - 1]) {
        uint32_t position = link - 1u;
        uint32_t length =
            folded_track_path(builder, track_at(builder, model, position), scratch->track);
        fold_parts(&scratch->raw, scratch->track, length, &scratch->candidate);
        Parts *candidate = &scratch->candidate;
        if (candidate->count > 0 &&
            same_span(wanted, last, candidate, candidate->spans[candidate->count - 1])) {
            consider(&best, common_suffix(wanted, candidate), position);
        }
    }
    return best.at_score == 1 ? best.position : BUILDER_UNSET;
}

uint32_t builder_resolve_entry(const LibraryBuilder *builder, const LibraryModel *model,
                               const char *folder, const char *entry)
{
    ResolveScratch *scratch = scratch_of(model);
    if (contains_url(entry)) {
        return BUILDER_UNSET;
    }
    uint32_t length = 0;
    for (; entry[length] != '\0' && length + 1 < PATH_CAPACITY; length++) {
        scratch->entry[length] = entry[length] == '\\' ? '/' : entry[length];
    }
    scratch->entry[length] = '\0';
    uint32_t joined = join(folder, scratch->entry, length, scratch->joined);
    uint32_t found = find_exact(builder, model, scratch->joined, joined);
    if (found == BUILDER_UNSET) {
        found = find_exact(builder, model, scratch->entry, length);
    }
    if (found != BUILDER_UNSET) {
        return found;
    }
    uint32_t parts = fold_parts(&scratch->raw, scratch->entry, length, &scratch->wanted);
    return parts > 0 ? find_by_suffix(builder, model) : BUILDER_UNSET;
}
