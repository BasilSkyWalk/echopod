#include "library/library_browse.h"

#include "core/format.h"
#include "core/string_builder.h"
#include "core/utf8.h"

enum {
    PATH_CAPACITY = 256,
    ROOT_FOLDER = 0
};

static BrowseItem make_item(BrowseItemKind kind, LibrarySection group, uint32_t index)
{
    return (BrowseItem){.kind = kind, .group = group, .index = (uint16_t)index};
}

static LibrarySection group_section_of(BrowseKind kind)
{
    switch (kind) {
    case BROWSE_ALBUM_ARTISTS:
        return LIBRARY_ALBUM_ARTISTS;
    case BROWSE_GENRES:
        return LIBRARY_GENRES;
    case BROWSE_PLAYLISTS:
    case BROWSE_PLAYLIST_TRACKS:
        return LIBRARY_PLAYLISTS;
    case BROWSE_ARTISTS:
    case BROWSE_ALBUMS:
    case BROWSE_ALL_SONGS:
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_ALBUM_TRACKS:
    case BROWSE_FOLDER:
        return LIBRARY_ARTISTS;
    }
    return LIBRARY_ARTISTS;
}

BrowseQuery browse_root_query(BrowseKind kind)
{
    return (BrowseQuery){
        .kind = kind, .group = group_section_of(kind), .subject = 0, .artist = LIBRARY_NONE};
}

static bool track_matches(const Library *library, BrowseQuery query, uint32_t track_index)
{
    LibraryTrack track;
    if (query.artist == LIBRARY_NONE) {
        return true;
    }
    return library_read_track(library, track_index, &track) && track.artist == query.artist;
}

static uint32_t group_link_count(const Library *library, BrowseQuery query)
{
    LibraryGroup group;
    return library_read_group(library, query.group, query.subject, &group) ? group.link_count : 0;
}

static uint32_t album_track_count(const Library *library, BrowseQuery query)
{
    LibraryAlbum album;
    if (!library_read_album(library, query.subject, &album)) {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t track = album.first_track; track < album.first_track + album.track_count;
         track++) {
        count += track_matches(library, query, track) ? 1 : 0;
    }
    return count;
}

static uint32_t folder_item_count(const Library *library, uint32_t subject)
{
    LibraryFolder folder;
    if (!library_read_folder(library, subject, &folder)) {
        return 0;
    }
    return (uint32_t)folder.subfolder_count + folder.track_count;
}

uint32_t browse_count(const Library *library, BrowseQuery query)
{
    switch (query.kind) {
    case BROWSE_ARTISTS:
        return library_music_count(library, LIBRARY_ARTISTS);
    case BROWSE_ALBUM_ARTISTS:
        return library_music_count(library, LIBRARY_ALBUM_ARTISTS);
    case BROWSE_GENRES:
        return library_music_count(library, LIBRARY_GENRES);
    case BROWSE_ALBUMS:
        return library_music_count(library, LIBRARY_ALBUMS);
    case BROWSE_ALL_SONGS:
        return library_music_count(library, LIBRARY_TITLE_ORDER);
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_PLAYLIST_TRACKS:
        return group_link_count(library, query);
    case BROWSE_ALBUM_TRACKS:
        return album_track_count(library, query);
    case BROWSE_FOLDER:
        return folder_item_count(library, query.subject);
    case BROWSE_PLAYLISTS:
        return library_count(library, LIBRARY_PLAYLISTS);
    }
    return 0;
}

static bool table_item(const Library *library, LibrarySection section, uint32_t position,
                       BrowseItem *item)
{
    if (position >= library_music_count(library, section)) {
        return false;
    }
    BrowseItemKind kind = section == LIBRARY_ALBUMS ? BROWSE_ITEM_ALBUM : BROWSE_ITEM_GROUP;
    *item = make_item(kind, section, position);
    return true;
}

static bool linked_item(const Library *library, uint32_t link, BrowseItemKind kind,
                        BrowseItem *item)
{
    uint16_t value = 0;
    if (!library_read_index(library, LIBRARY_LINKS, link, &value)) {
        return false;
    }
    *item = make_item(kind, LIBRARY_ARTISTS, value);
    return true;
}

static bool title_order_item(const Library *library, uint32_t position, BrowseItem *item)
{
    uint16_t track = 0;
    if (!library_read_index(library, LIBRARY_TITLE_ORDER, position, &track)) {
        return false;
    }
    *item = make_item(BROWSE_ITEM_TRACK, LIBRARY_ARTISTS, track);
    return true;
}

static bool group_link_item(const Library *library, BrowseQuery query, uint32_t position,
                            BrowseItem *item)
{
    LibraryGroup group;
    if (!library_read_group(library, query.group, query.subject, &group) ||
        position >= group.link_count) {
        return false;
    }
    BrowseItemKind kind =
        query.kind == BROWSE_PLAYLIST_TRACKS ? BROWSE_ITEM_TRACK : BROWSE_ITEM_ALBUM;
    return linked_item(library, group.first_link + position, kind, item);
}

static bool album_track_item(const Library *library, BrowseQuery query, uint32_t position,
                             BrowseItem *item)
{
    LibraryAlbum album;
    if (!library_read_album(library, query.subject, &album)) {
        return false;
    }
    uint32_t seen = 0;
    for (uint32_t track = album.first_track; track < album.first_track + album.track_count;
         track++) {
        if (track_matches(library, query, track) && seen++ == position) {
            *item = make_item(BROWSE_ITEM_TRACK, LIBRARY_ARTISTS, track);
            return true;
        }
    }
    return false;
}

static bool folder_item(const Library *library, uint32_t subject, uint32_t position,
                        BrowseItem *item)
{
    LibraryFolder folder;
    if (!library_read_folder(library, subject, &folder) ||
        position >= (uint32_t)folder.subfolder_count + folder.track_count) {
        return false;
    }
    BrowseItemKind kind =
        position < folder.subfolder_count ? BROWSE_ITEM_FOLDER : BROWSE_ITEM_TRACK;
    return linked_item(library, folder.first_link + position, kind, item);
}

bool browse_item(const Library *library, BrowseQuery query, uint32_t position, BrowseItem *item)
{
    switch (query.kind) {
    case BROWSE_ARTISTS:
    case BROWSE_ALBUM_ARTISTS:
    case BROWSE_GENRES:
    case BROWSE_PLAYLISTS:
        return table_item(library, group_section_of(query.kind), position, item);
    case BROWSE_ALBUMS:
        return table_item(library, LIBRARY_ALBUMS, position, item);
    case BROWSE_ALL_SONGS:
        return title_order_item(library, position, item);
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_PLAYLIST_TRACKS:
        return group_link_item(library, query, position, item);
    case BROWSE_ALBUM_TRACKS:
        return album_track_item(library, query, position, item);
    case BROWSE_FOLDER:
        return folder_item(library, query.subject, position, item);
    }
    return false;
}

static uint16_t artist_filter(BrowseQuery parent)
{
    bool from_artist = parent.kind == BROWSE_GROUP_ALBUMS && parent.group == LIBRARY_ARTISTS;
    return from_artist ? parent.subject : LIBRARY_NONE;
}

static BrowseQuery query_of(BrowseKind kind, LibrarySection group, uint16_t subject,
                            uint16_t artist)
{
    return (BrowseQuery){.kind = kind, .group = group, .subject = subject, .artist = artist};
}

bool browse_child(BrowseQuery parent, BrowseItem item, BrowseQuery *child)
{
    switch (item.kind) {
    case BROWSE_ITEM_GROUP:
        *child =
            query_of(item.group == LIBRARY_PLAYLISTS ? BROWSE_PLAYLIST_TRACKS : BROWSE_GROUP_ALBUMS,
                     item.group, item.index, LIBRARY_NONE);
        return true;
    case BROWSE_ITEM_ALBUM:
        *child = query_of(BROWSE_ALBUM_TRACKS, LIBRARY_ARTISTS, item.index, artist_filter(parent));
        return true;
    case BROWSE_ITEM_FOLDER:
        *child = query_of(BROWSE_FOLDER, LIBRARY_ARTISTS, item.index, LIBRARY_NONE);
        return true;
    case BROWSE_ITEM_TRACK:
        return false;
    }
    return false;
}

static void copy_span(char *target, size_t capacity, const char *source, size_t length)
{
    size_t kept = length < capacity ? length : capacity - 1;
    kept = utf8_complete_length(source, kept);
    for (size_t index = 0; index < kept; index++) {
        target[index] = source[index];
    }
    target[kept] = '\0';
}

static void copy_file_stem(char *target, size_t capacity, const char *path)
{
    size_t start = 0;
    size_t length = 0;
    for (; path[length] != '\0'; length++) {
        start = path[length] == '/' ? length + 1 : start;
    }
    size_t end = length;
    for (size_t index = length; index > start + 1; index--) {
        if (path[index - 1] == '.') {
            end = index - 1;
            break;
        }
    }
    copy_span(target, capacity, path + start, end - start);
}

static bool group_text(const Library *library, BrowseItem item, BrowseText text)
{
    LibraryGroup group;
    return library_read_group(library, item.group, item.index, &group) &&
           library_read_string(library, group.name, text.label, text.label_capacity);
}

static bool album_text(const Library *library, BrowseItem item, BrowseText text)
{
    LibraryAlbum album;
    if (!library_read_album(library, item.index, &album)) {
        return false;
    }
    if (album.year > 0) {
        StringBuilder builder = string_builder_make(text.detail, text.detail_capacity);
        string_builder_append_unsigned(&builder, album.year);
    }
    return library_read_string(library, album.title, text.label, text.label_capacity);
}

static bool track_label(const Library *library, BrowseQuery query, LibraryTrack track,
                        BrowseText text)
{
    if (query.kind != BROWSE_FOLDER) {
        return library_read_string(library, track.title, text.label, text.label_capacity);
    }
    char path[PATH_CAPACITY];
    if (!library_read_string(library, track.path, path, sizeof path)) {
        return false;
    }
    copy_file_stem(text.label, text.label_capacity, path);
    return true;
}

static bool track_text(const Library *library, BrowseQuery query, BrowseItem item, BrowseText text)
{
    LibraryTrack track;
    if (!library_read_track(library, item.index, &track)) {
        return false;
    }
    format_duration(track.duration_ms, text.detail, text.detail_capacity);
    return track_label(library, query, track, text);
}

static bool folder_text(const Library *library, BrowseItem item, BrowseText text)
{
    LibraryFolder folder;
    return library_read_folder(library, item.index, &folder) &&
           library_read_string(library, folder.name, text.label, text.label_capacity);
}

bool browse_item_text(const Library *library, BrowseQuery query, BrowseItem item, BrowseText text)
{
    text.label[0] = '\0';
    text.detail[0] = '\0';
    switch (item.kind) {
    case BROWSE_ITEM_GROUP:
        return group_text(library, item, text);
    case BROWSE_ITEM_ALBUM:
        return album_text(library, item, text);
    case BROWSE_ITEM_TRACK:
        return track_text(library, query, item, text);
    case BROWSE_ITEM_FOLDER:
        return folder_text(library, item, text);
    }
    return false;
}

static bool fixed_title(const char *title, char *buffer, size_t capacity)
{
    StringBuilder builder = string_builder_make(buffer, capacity);
    string_builder_append(&builder, title);
    return true;
}

static bool subject_title(const Library *library, BrowseQuery query, char *buffer, size_t capacity)
{
    LibraryGroup group;
    LibraryAlbum album;
    LibraryFolder folder;
    switch (query.kind) {
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_PLAYLIST_TRACKS:
        return library_read_group(library, query.group, query.subject, &group) &&
               library_read_string(library, group.name, buffer, capacity);
    case BROWSE_ALBUM_TRACKS:
        return library_read_album(library, query.subject, &album) &&
               library_read_string(library, album.title, buffer, capacity);
    case BROWSE_FOLDER:
        return library_read_folder(library, query.subject, &folder) &&
               library_read_string(library, folder.name, buffer, capacity);
    case BROWSE_ARTISTS:
    case BROWSE_ALBUM_ARTISTS:
    case BROWSE_GENRES:
    case BROWSE_ALBUMS:
    case BROWSE_ALL_SONGS:
    case BROWSE_PLAYLISTS:
        return false;
    }
    return false;
}

bool browse_title(const Library *library, BrowseQuery query, char *buffer, size_t capacity)
{
    switch (query.kind) {
    case BROWSE_ARTISTS:
        return fixed_title("Artists", buffer, capacity);
    case BROWSE_ALBUM_ARTISTS:
        return fixed_title("Album Artists", buffer, capacity);
    case BROWSE_GENRES:
        return fixed_title("Genres", buffer, capacity);
    case BROWSE_ALBUMS:
        return fixed_title("Albums", buffer, capacity);
    case BROWSE_ALL_SONGS:
        return fixed_title("All Songs", buffer, capacity);
    case BROWSE_PLAYLISTS:
        return fixed_title("Playlists", buffer, capacity);
    case BROWSE_FOLDER:
        if (query.subject == ROOT_FOLDER) {
            return fixed_title("Folders", buffer, capacity);
        }
        return subject_title(library, query, buffer, capacity);
    case BROWSE_GROUP_ALBUMS:
    case BROWSE_ALBUM_TRACKS:
    case BROWSE_PLAYLIST_TRACKS:
        return subject_title(library, query, buffer, capacity);
    }
    return false;
}
