#include "library/library.h"

#include "core/hash.h"
#include "core/utf8.h"

enum {
    MAGIC_SIZE = 4,
    VERSION_FIELD = 4,
    SECTION_COUNT_FIELD = 6,
    SECTION_TABLE = 8,
    SECTION_ENTRY_SIZE = 12,
    TRACK_SIZE = 24,
    ALBUM_SIZE = 16,
    GROUP_SIZE = 12,
    FOLDER_SIZE = 16,
    INDEX_SIZE = 2,
    STRING_SIZE = 1,
    BITS_PER_BYTE = 8
};

static const uint8_t MAGIC[MAGIC_SIZE] = {'E', 'M', 'D', 'B'};

static const uint16_t RECORD_SIZES[LIBRARY_SECTION_COUNT] = {
    [LIBRARY_TRACKS] = TRACK_SIZE,      [LIBRARY_ALBUMS] = ALBUM_SIZE,
    [LIBRARY_ARTISTS] = GROUP_SIZE,     [LIBRARY_ALBUM_ARTISTS] = GROUP_SIZE,
    [LIBRARY_GENRES] = GROUP_SIZE,      [LIBRARY_FOLDERS] = FOLDER_SIZE,
    [LIBRARY_TITLE_ORDER] = INDEX_SIZE, [LIBRARY_PLAYLISTS] = GROUP_SIZE,
    [LIBRARY_LINKS] = INDEX_SIZE,       [LIBRARY_STRINGS] = STRING_SIZE,
};

static uint16_t u16_at(const uint8_t *bytes, uint32_t offset)
{
    return (uint16_t)(bytes[offset] | bytes[offset + 1] << BITS_PER_BYTE);
}

static uint32_t u32_at(const uint8_t *bytes, uint32_t offset)
{
    return (uint32_t)u16_at(bytes, offset) | (uint32_t)u16_at(bytes, offset + 2) << 16;
}

static bool read_source(const Library *library, uint32_t offset, void *buffer, uint32_t length)
{
    return library->source.read(library->source.context, offset, buffer, length);
}

static bool magic_matches(const uint8_t *header)
{
    for (int index = 0; index < MAGIC_SIZE; index++) {
        if (header[index] != MAGIC[index]) {
            return false;
        }
    }
    return true;
}

static LibrarySectionInfo decode_section(const uint8_t *header, int section)
{
    uint32_t entry = (uint32_t)(SECTION_TABLE + section * SECTION_ENTRY_SIZE);
    return (LibrarySectionInfo){
        .offset = u32_at(header, entry),
        .count = u32_at(header, entry + 4),
        .record_size = u16_at(header, entry + 8),
        .music = u16_at(header, entry + 10),
    };
}

static bool section_fits(LibrarySectionInfo section, uint16_t expected_size, uint32_t size)
{
    uint64_t end = (uint64_t)section.offset + (uint64_t)section.count * section.record_size;
    return section.record_size == expected_size && section.offset >= LIBRARY_HEADER_SIZE &&
           end <= size;
}

static LibraryStatus decode_header(Library *library, const uint8_t *header, uint32_t size)
{
    if (!magic_matches(header)) {
        return LIBRARY_BAD_MAGIC;
    }
    if (u16_at(header, VERSION_FIELD) != LIBRARY_VERSION) {
        return LIBRARY_BAD_VERSION;
    }
    if (u16_at(header, SECTION_COUNT_FIELD) != LIBRARY_SECTION_COUNT) {
        return LIBRARY_BAD_LAYOUT;
    }
    for (int section = 0; section < LIBRARY_SECTION_COUNT; section++) {
        LibrarySectionInfo info = decode_section(header, section);
        if (!section_fits(info, RECORD_SIZES[section], size)) {
            return LIBRARY_BAD_LAYOUT;
        }
        library->sections[section] = info;
    }
    return LIBRARY_OK;
}

static bool read_record(const Library *library, LibrarySection section, uint32_t index,
                        uint8_t *record)
{
    LibrarySectionInfo info = library->sections[section];
    if (index >= info.count) {
        return false;
    }
    return read_source(library, info.offset + index * info.record_size, record, info.record_size);
}

static bool is_group_section(LibrarySection section)
{
    return section == LIBRARY_ARTISTS || section == LIBRARY_ALBUM_ARTISTS ||
           section == LIBRARY_GENRES || section == LIBRARY_PLAYLISTS;
}

static bool is_index_section(LibrarySection section)
{
    return section == LIBRARY_TITLE_ORDER || section == LIBRARY_LINKS;
}

LibraryStatus library_open(Library *library, ByteSource source, uint32_t size)
{
    *library = (Library){.source = source};
    uint8_t header[LIBRARY_HEADER_SIZE];
    if (size < LIBRARY_HEADER_SIZE || !read_source(library, 0, header, sizeof header)) {
        return LIBRARY_UNREADABLE;
    }
    LibraryStatus status = decode_header(library, header, size);
    if (status != LIBRARY_OK) {
        *library = (Library){.source = source};
    }
    return status;
}

uint32_t library_count(const Library *library, LibrarySection section)
{
    return library->sections[section].count;
}

uint32_t library_music_count(const Library *library, LibrarySection section)
{
    LibrarySectionInfo info = library->sections[section];
    bool counted = section <= LIBRARY_GENRES || section == LIBRARY_TITLE_ORDER;
    return counted && info.music < info.count ? info.music : info.count;
}

bool library_read_track(const Library *library, uint32_t index, LibraryTrack *track)
{
    uint8_t record[TRACK_SIZE];
    if (!read_record(library, LIBRARY_TRACKS, index, record)) {
        return false;
    }
    *track = (LibraryTrack){
        .title = u32_at(record, 0),
        .path = u32_at(record, 4),
        .album = u16_at(record, 8),
        .artist = u16_at(record, 10),
        .genre = u16_at(record, 12),
        .folder = u16_at(record, 14),
        .duration_ms = u32_at(record, 16),
        .disc = record[20],
        .number = record[21],
    };
    return true;
}

bool library_read_album(const Library *library, uint32_t index, LibraryAlbum *album)
{
    uint8_t record[ALBUM_SIZE];
    if (!read_record(library, LIBRARY_ALBUMS, index, record)) {
        return false;
    }
    *album = (LibraryAlbum){
        .title = u32_at(record, 0),
        .album_artist = u16_at(record, 4),
        .genre = u16_at(record, 6),
        .year = u16_at(record, 8),
        .first_track = u16_at(record, 10),
        .track_count = u16_at(record, 12),
        .cover = u16_at(record, 14),
    };
    return true;
}

bool library_read_group(const Library *library, LibrarySection section, uint32_t index,
                        LibraryGroup *group)
{
    uint8_t record[GROUP_SIZE];
    if (!is_group_section(section) || !read_record(library, section, index, record)) {
        return false;
    }
    *group = (LibraryGroup){
        .name = u32_at(record, 0),
        .first_link = u32_at(record, 4),
        .link_count = u16_at(record, 8),
    };
    return true;
}

bool library_read_folder(const Library *library, uint32_t index, LibraryFolder *folder)
{
    uint8_t record[FOLDER_SIZE];
    if (!read_record(library, LIBRARY_FOLDERS, index, record)) {
        return false;
    }
    *folder = (LibraryFolder){
        .name = u32_at(record, 0),
        .parent = u16_at(record, 4),
        .subfolder_count = u16_at(record, 6),
        .first_link = u32_at(record, 8),
        .track_count = u16_at(record, 12),
    };
    return true;
}

bool library_read_index(const Library *library, LibrarySection section, uint32_t position,
                        uint16_t *value)
{
    uint8_t record[INDEX_SIZE];
    if (!is_index_section(section) || !read_record(library, section, position, record)) {
        return false;
    }
    *value = u16_at(record, 0);
    return true;
}

static size_t terminated_length(const char *buffer, uint32_t length)
{
    for (uint32_t index = 0; index < length; index++) {
        if (buffer[index] == '\0') {
            return index;
        }
    }
    return utf8_complete_length(buffer, length);
}

bool library_read_string(const Library *library, uint32_t offset, char *buffer, size_t capacity)
{
    LibrarySectionInfo strings = library->sections[LIBRARY_STRINGS];
    if (capacity == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (offset >= strings.count) {
        return false;
    }
    uint32_t available = strings.count - offset;
    uint32_t wanted = capacity - 1 < available ? (uint32_t)(capacity - 1) : available;
    if (!read_source(library, strings.offset + offset, buffer, wanted)) {
        buffer[0] = '\0';
        return false;
    }
    buffer[terminated_length(buffer, wanted)] = '\0';
    return true;
}

static uint32_t hash_word(uint32_t hash, uint32_t value)
{
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16),
                        (uint8_t)(value >> 24)};
    return hash_update(hash, bytes, sizeof bytes);
}

uint32_t library_fingerprint(const Library *library)
{
    uint32_t hash = HASH_START;
    for (int section = 0; section < LIBRARY_SECTION_COUNT; section++) {
        hash = hash_word(hash, library->sections[section].offset);
        hash = hash_word(hash, library->sections[section].count);
    }
    return hash;
}

enum {
    KEY_TEXT_CAPACITY = 256,
    KEY_SEPARATOR = 0x1F
};

static size_t text_length(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static uint32_t hash_string(const Library *library, uint32_t hash, uint32_t offset)
{
    char text[KEY_TEXT_CAPACITY];
    library_read_string(library, offset, text, sizeof text);
    return hash_update(hash, text, text_length(text));
}

uint32_t library_album_key(const Library *library, uint32_t album_index)
{
    LibraryAlbum album;
    LibraryGroup artist;
    uint8_t separator = KEY_SEPARATOR;
    if (!library_read_album(library, album_index, &album) ||
        !library_read_group(library, LIBRARY_ALBUM_ARTISTS, album.album_artist, &artist)) {
        return 0;
    }
    uint32_t hash = hash_string(library, HASH_START, artist.name);
    hash = hash_update(hash, &separator, sizeof separator);
    return hash_string(library, hash, album.title);
}
