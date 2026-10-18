#include <stddef.h>

#include "builder_internal.h"

typedef enum TrackField {
    FIELD_TITLE,
    FIELD_ARTIST,
    FIELD_ALBUM_ARTIST,
    FIELD_ALBUM,
    FIELD_GENRE,
    FIELD_FILE_NAME
} TrackField;

typedef struct SortContext {
    const LibraryBuilder *builder;
    const LibraryModel *model;
    TrackField field;
} SortContext;

static uint32_t field_of(const TrackRecord *track, TrackField field)
{
    switch (field) {
    case FIELD_TITLE:
        return track->title;
    case FIELD_ARTIST:
        return track->artist;
    case FIELD_ALBUM_ARTIST:
        return track->album_artist;
    case FIELD_ALBUM:
        return track->album;
    case FIELD_GENRE:
        return track->genre;
    case FIELD_FILE_NAME:
        return track->file_name;
    }
    return track->title;
}

static uint32_t track_field(const LibraryBuilder *builder, uint32_t track, TrackField field)
{
    return field_of(builder_track(builder, track), field);
}

static int compare_numbers(uint32_t left, uint32_t right)
{
    return left < right ? -1 : left > right ? 1 : 0;
}

static int by_field_key(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    return builder_compare_keys(sort->builder, track_field(sort->builder, left, sort->field),
                                track_field(sort->builder, right, sort->field));
}

static uint16_t *identity(Work *work, uint32_t count)
{
    uint16_t *items = work_alloc(work, count * 2);
    for (uint32_t index = 0; items != NULL && index < count; index++) {
        items[index] = (uint16_t)index;
    }
    return items;
}

static bool build_name_index(const LibraryBuilder *builder, Work *work, TrackField field,
                             NameIndex *index, uint16_t *scratch)
{
    uint32_t count = builder->track_count;
    SortContext sort = {.builder = builder, .field = field};
    uint16_t *order = identity(work, count);
    index->group_of_track = work_alloc(work, count * 2);
    index->display = work_alloc(work, count * 4);
    if (work->failed) {
        return false;
    }
    stable_sort(order, scratch, count, by_field_key, &sort);
    index->count = 0;
    for (uint32_t position = 0; position < count; position++) {
        uint32_t text = track_field(builder, order[position], field);
        if (position == 0 ||
            builder_compare_keys(builder, track_field(builder, order[position - 1], field), text) !=
                0) {
            index->display[index->count++] = text;
        }
        index->group_of_track[order[position]] = (uint16_t)(index->count - 1);
    }
    return true;
}

static int by_album_key(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    int artist =
        builder_compare_keys(sort->builder, track_field(sort->builder, left, FIELD_ALBUM_ARTIST),
                             track_field(sort->builder, right, FIELD_ALBUM_ARTIST));
    return artist != 0
               ? artist
               : builder_compare_keys(sort->builder, track_field(sort->builder, left, FIELD_ALBUM),
                                      track_field(sort->builder, right, FIELD_ALBUM));
}

static uint16_t most_common_genre_track(const LibraryBuilder *builder, const uint16_t *members,
                                        uint32_t count)
{
    uint16_t best = members[0];
    uint32_t best_count = 0;
    for (uint32_t index = 0; index < count; index++) {
        uint32_t genre = builder_track(builder, members[index])->genre;
        bool seen = false;
        for (uint32_t earlier = 0; earlier < index && !seen; earlier++) {
            seen = builder_track(builder, members[earlier])->genre == genre;
        }
        uint32_t occurrences = 0;
        for (uint32_t other = index; !seen && other < count; other++) {
            occurrences += builder_track(builder, members[other])->genre == genre;
        }
        if (occurrences > best_count) {
            best_count = occurrences;
            best = members[index];
        }
    }
    return best;
}

static uint16_t earliest_year(const LibraryBuilder *builder, const uint16_t *members,
                              uint32_t count)
{
    uint16_t year = 0;
    for (uint32_t index = 0; index < count; index++) {
        uint16_t candidate = builder_track(builder, members[index])->year;
        if (candidate != 0 && (year == 0 || candidate < year)) {
            year = candidate;
        }
    }
    return year;
}

static int by_track_order(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    const TrackRecord *a = builder_track(sort->builder, left);
    const TrackRecord *b = builder_track(sort->builder, right);
    int disc = compare_numbers(a->disc, b->disc);
    int number = compare_numbers(a->number, b->number);
    if (disc != 0 || number != 0) {
        return disc != 0 ? disc : number;
    }
    return builder_compare_keys(sort->builder, field_of(a, sort->field), field_of(b, sort->field));
}

static void describe_album(const LibraryBuilder *builder, const LibraryModel *model,
                           AlbumModel *album, uint16_t *scratch)
{
    const uint16_t *members = model->album_members + album->member_start;
    uint16_t first = members[0];
    uint16_t genre_track = most_common_genre_track(builder, members, album->track_count);
    album->first_seen = first;
    album->title = builder_track(builder, first)->album;
    album->album_artist = model->album_artists.group_of_track[first];
    album->genre = model->genres.group_of_track[genre_track];
    album->year = earliest_year(builder, members, album->track_count);
    SortContext sort = {.builder = builder, .field = FIELD_TITLE};
    stable_sort(model->album_members + album->member_start, scratch, album->track_count,
                by_track_order, &sort);
}

static bool group_albums(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                         uint16_t *scratch)
{
    uint32_t count = builder->track_count;
    SortContext sort = {.builder = builder};
    model->album_members = identity(work, count);
    model->albums = work_alloc(work, (count > 0 ? count : 1) * (uint32_t)sizeof(AlbumModel));
    if (work->failed) {
        return false;
    }
    stable_sort(model->album_members, scratch, count, by_album_key, &sort);
    model->album_count = 0;
    for (uint32_t position = 0; position < count; position++) {
        if (position == 0 || by_album_key(&sort, model->album_members[position - 1],
                                          model->album_members[position]) != 0) {
            model->albums[model->album_count++] =
                (AlbumModel){.member_start = (uint16_t)position, .track_count = 0};
        }
        model->albums[model->album_count - 1].track_count++;
    }
    for (uint32_t album = 0; album < model->album_count; album++) {
        describe_album(builder, model, &model->albums[album], scratch);
    }
    return true;
}

static int by_first_seen(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    return compare_numbers(sort->model->albums[left].first_seen,
                           sort->model->albums[right].first_seen);
}

static int by_album_title(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    const LibraryModel *model = sort->model;
    const AlbumModel *a = &model->albums[left];
    const AlbumModel *b = &model->albums[right];
    int title = builder_compare_keys(sort->builder, a->title, b->title);
    if (title != 0) {
        return title;
    }
    return builder_compare_keys(sort->builder, model->album_artists.display[a->album_artist],
                                model->album_artists.display[b->album_artist]);
}

static bool order_albums(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                         uint16_t *scratch)
{
    SortContext sort = {.builder = builder, .model = model};
    uint16_t *order = identity(work, model->album_count);
    AlbumModel *ordered = work_alloc(work, (model->album_count > 0 ? model->album_count : 1u) *
                                               (uint32_t)sizeof(AlbumModel));
    if (work->failed) {
        return false;
    }
    stable_sort(order, scratch, model->album_count, by_first_seen, &sort);
    stable_sort(order, scratch, model->album_count, by_album_title, &sort);
    for (uint32_t index = 0; index < model->album_count; index++) {
        ordered[index] = model->albums[order[index]];
    }
    model->albums = ordered;
    return true;
}

static bool order_tracks(const LibraryBuilder *builder, Work *work, LibraryModel *model)
{
    uint32_t count = builder->track_count;
    model->final_tracks = work_alloc(work, count * 2 + 2);
    model->track_album = work_alloc(work, count * 2 + 2);
    if (work->failed) {
        return false;
    }
    uint32_t position = 0;
    for (uint32_t album = 0; album < model->album_count; album++) {
        AlbumModel *record = &model->albums[album];
        record->first_track = (uint16_t)position;
        for (uint32_t member = 0; member < record->track_count; member++) {
            model->final_tracks[position] = model->album_members[record->member_start + member];
            model->track_album[position++] = (uint16_t)album;
        }
    }
    return true;
}

static int by_chronology(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    const AlbumModel *a = &sort->model->albums[left];
    const AlbumModel *b = &sort->model->albums[right];
    int year = compare_numbers(a->year, b->year);
    return year != 0 ? year : builder_compare_keys(sort->builder, a->title, b->title);
}

static int by_title_only(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    return builder_compare_keys(sort->builder, sort->model->albums[left].title,
                                sort->model->albums[right].title);
}

static void count_group_albums(const LibraryModel *model, const NameIndex *index, GroupLinks *links,
                               uint32_t *seen)
{
    for (uint32_t album = 0; album < model->album_count; album++) {
        const AlbumModel *record = &model->albums[album];
        for (uint32_t member = 0; member < record->track_count; member++) {
            uint16_t group =
                index->group_of_track[model->final_tracks[record->first_track + member]];
            if (seen[group] != album + 1) {
                seen[group] = album + 1;
                links->count[group]++;
            }
        }
    }
}

static void fill_group_albums(const LibraryModel *model, const NameIndex *index, GroupLinks *links,
                              uint32_t *seen)
{
    for (uint32_t album = 0; album < model->album_count; album++) {
        const AlbumModel *record = &model->albums[album];
        for (uint32_t member = 0; member < record->track_count; member++) {
            uint16_t group =
                index->group_of_track[model->final_tracks[record->first_track + member]];
            if (seen[group] != album + 1) {
                seen[group] = album + 1;
                links->albums[links->first[group] + links->count[group]++] = (uint16_t)album;
            }
        }
    }
}

static bool build_group_links(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                              const NameIndex *index, GroupLinks *links, IndexCompare order,
                              uint16_t *scratch)
{
    uint32_t groups = index->count > 0 ? index->count : 1u;
    links->first = work_alloc(work, groups * 4);
    links->count = work_alloc(work, groups * 2);
    uint32_t *seen = work_alloc(work, groups * 4);
    if (work->failed) {
        return false;
    }
    for (uint32_t group = 0; group < groups; group++) {
        links->count[group] = 0;
        seen[group] = 0;
    }
    count_group_albums(model, index, links, seen);
    links->total = 0;
    for (uint32_t group = 0; group < index->count; group++) {
        links->first[group] = links->total;
        links->total += links->count[group];
        links->count[group] = 0;
        seen[group] = 0;
    }
    links->albums = work_alloc(work, links->total * 2 + 2);
    if (work->failed) {
        return false;
    }
    fill_group_albums(model, index, links, seen);
    SortContext sort = {.builder = builder, .model = model};
    for (uint32_t group = 0; group < index->count; group++) {
        stable_sort(links->albums + links->first[group], scratch, links->count[group], order,
                    &sort);
    }
    return true;
}

static int by_folder_name(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    uint32_t a = builder_folder(sort->builder, left)->name;
    uint32_t b = builder_folder(sort->builder, right)->name;
    int key = builder_compare_keys(sort->builder, a, b);
    return key != 0 ? key : builder_compare_raw(sort->builder, a, b);
}

typedef struct FolderTree {
    uint16_t *children;
    uint32_t *child_start;
    uint16_t *child_count;
    uint16_t *new_index;
    uint16_t *order;
    uint16_t *track_start_of_node;
    uint16_t *tracks;
    uint16_t *track_count_of_node;
} FolderTree;

static bool collect_children(const LibraryBuilder *builder, Work *work, FolderTree *tree,
                             uint16_t *scratch)
{
    uint32_t nodes = builder->folder_count;
    tree->children = work_alloc(work, nodes * 2);
    tree->child_start = work_alloc(work, nodes * 4);
    tree->child_count = work_alloc(work, nodes * 2);
    if (work->failed) {
        return false;
    }
    for (uint32_t node = 0; node < nodes; node++) {
        tree->child_count[node] = 0;
    }
    for (uint32_t node = 1; node < nodes; node++) {
        tree->child_count[builder_folder(builder, node)->parent]++;
    }
    uint32_t total = 0;
    for (uint32_t node = 0; node < nodes; node++) {
        tree->child_start[node] = total;
        total += tree->child_count[node];
        tree->child_count[node] = 0;
    }
    for (uint32_t node = 1; node < nodes; node++) {
        uint16_t parent = builder_folder(builder, node)->parent;
        tree->children[tree->child_start[parent] + tree->child_count[parent]++] = (uint16_t)node;
    }
    SortContext sort = {.builder = builder};
    for (uint32_t node = 0; node < nodes; node++) {
        stable_sort(tree->children + tree->child_start[node], scratch, tree->child_count[node],
                    by_folder_name, &sort);
    }
    return true;
}

static bool number_folders(const LibraryBuilder *builder, Work *work, FolderTree *tree)
{
    uint32_t nodes = builder->folder_count;
    tree->new_index = work_alloc(work, nodes * 2);
    tree->order = work_alloc(work, nodes * 2);
    uint16_t *stack = work_alloc(work, nodes * 2);
    uint16_t *next = work_alloc(work, nodes * 2);
    if (work->failed) {
        return false;
    }
    uint32_t depth = 1;
    uint32_t numbered = 1;
    stack[0] = 0;
    next[0] = 0;
    tree->new_index[0] = 0;
    tree->order[0] = 0;
    while (depth > 0) {
        uint16_t node = stack[depth - 1];
        if (next[depth - 1] >= tree->child_count[node]) {
            depth--;
            continue;
        }
        uint16_t child = tree->children[tree->child_start[node] + next[depth - 1]++];
        tree->new_index[child] = (uint16_t)numbered;
        tree->order[numbered++] = child;
        stack[depth] = child;
        next[depth++] = 0;
    }
    return true;
}

static int by_folder_track(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    uint16_t a = sort->model->final_tracks[left];
    uint16_t b = sort->model->final_tracks[right];
    return by_track_order(context, a, b);
}

static bool collect_folder_tracks(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                                  FolderTree *tree, uint16_t *scratch)
{
    uint32_t nodes = builder->folder_count;
    uint32_t count = builder->track_count;
    tree->track_start_of_node = work_alloc(work, nodes * 2);
    tree->track_count_of_node = work_alloc(work, nodes * 2);
    tree->tracks = work_alloc(work, count * 2 + 2);
    if (work->failed) {
        return false;
    }
    for (uint32_t node = 0; node < nodes; node++) {
        tree->track_count_of_node[node] = 0;
    }
    for (uint32_t position = 0; position < count; position++) {
        tree->track_count_of_node[builder_track(builder, model->final_tracks[position])->folder]++;
    }
    uint32_t total = 0;
    for (uint32_t node = 0; node < nodes; node++) {
        tree->track_start_of_node[node] = (uint16_t)total;
        total += tree->track_count_of_node[node];
        tree->track_count_of_node[node] = 0;
    }
    for (uint32_t position = 0; position < count; position++) {
        uint16_t node = builder_track(builder, model->final_tracks[position])->folder;
        tree->tracks[tree->track_start_of_node[node] + tree->track_count_of_node[node]++] =
            (uint16_t)position;
    }
    SortContext sort = {.builder = builder, .model = model, .field = FIELD_FILE_NAME};
    for (uint32_t node = 0; node < nodes; node++) {
        stable_sort(tree->tracks + tree->track_start_of_node[node], scratch,
                    tree->track_count_of_node[node], by_folder_track, &sort);
    }
    return true;
}

static bool write_folder_models(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                                const FolderTree *tree)
{
    uint32_t nodes = builder->folder_count;
    model->folder_count = (uint16_t)nodes;
    model->folders = work_alloc(work, nodes * (uint32_t)sizeof(FolderModel));
    model->folder_links = work_alloc(work, (nodes + builder->track_count) * 2 + 2);
    model->track_folder = work_alloc(work, builder->track_count * 2 + 2);
    if (work->failed) {
        return false;
    }
    uint32_t links = 0;
    for (uint32_t index = 0; index < nodes; index++) {
        uint16_t node = tree->order[index];
        const FolderNode *folder = builder_folder(builder, node);
        model->folders[index] =
            (FolderModel){.name = folder->name,
                          .parent = node == 0 ? BUILDER_NONE : tree->new_index[folder->parent],
                          .subfolder_count = tree->child_count[node],
                          .track_count = tree->track_count_of_node[node],
                          .node = node,
                          .first_link = links};
        for (uint32_t child = 0; child < tree->child_count[node]; child++) {
            model->folder_links[links++] =
                tree->new_index[tree->children[tree->child_start[node] + child]];
        }
        for (uint32_t track = 0; track < tree->track_count_of_node[node]; track++) {
            uint16_t position = tree->tracks[tree->track_start_of_node[node] + track];
            model->folder_links[links++] = position;
            model->track_folder[position] = (uint16_t)index;
        }
    }
    model->folder_link_count = links;
    return true;
}

static bool build_folders(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                          uint16_t *scratch)
{
    FolderTree tree;
    return collect_children(builder, work, &tree, scratch) &&
           number_folders(builder, work, &tree) &&
           collect_folder_tracks(builder, work, model, &tree, scratch) &&
           write_folder_models(builder, work, model, &tree);
}

static int by_title_then_artist(const void *context, uint16_t left, uint16_t right)
{
    const SortContext *sort = context;
    const TrackRecord *a = builder_track(sort->builder, sort->model->final_tracks[left]);
    const TrackRecord *b = builder_track(sort->builder, sort->model->final_tracks[right]);
    int title = builder_compare_keys(sort->builder, a->title, b->title);
    return title != 0 ? title : builder_compare_keys(sort->builder, a->artist, b->artist);
}

static bool build_title_order(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                              uint16_t *scratch)
{
    SortContext sort = {.builder = builder, .model = model};
    model->title_order = identity(work, builder->track_count + 1);
    if (work->failed) {
        return false;
    }
    stable_sort(model->title_order, scratch, builder->track_count, by_title_then_artist, &sort);
    return true;
}

static bool build_name_indexes(const LibraryBuilder *builder, Work *work, LibraryModel *model,
                               uint16_t *scratch)
{
    return build_name_index(builder, work, FIELD_ALBUM_ARTIST, &model->album_artists, scratch) &&
           build_name_index(builder, work, FIELD_ARTIST, &model->artists, scratch) &&
           build_name_index(builder, work, FIELD_GENRE, &model->genres, scratch);
}

bool builder_build_model(LibraryBuilder *builder, Work *work, LibraryModel *model)
{
    *model = (LibraryModel){.track_count = builder->track_count};
    uint32_t largest =
        builder->track_count > builder->folder_count ? builder->track_count : builder->folder_count;
    uint16_t *scratch = work_alloc(work, largest * 2 + 2);
    if (scratch == NULL) {
        return false;
    }
    return build_name_indexes(builder, work, model, scratch) &&
           group_albums(builder, work, model, scratch) &&
           order_albums(builder, work, model, scratch) && order_tracks(builder, work, model) &&
           build_group_links(builder, work, model, &model->artists, &model->artist_links,
                             by_chronology, scratch) &&
           build_group_links(builder, work, model, &model->album_artists,
                             &model->album_artist_links, by_chronology, scratch) &&
           build_group_links(builder, work, model, &model->genres, &model->genre_links,
                             by_title_only, scratch) &&
           build_folders(builder, work, model, scratch) &&
           build_title_order(builder, work, model, scratch);
}
