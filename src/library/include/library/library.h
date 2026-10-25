#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library/byte_source.h"

typedef enum LibrarySection {
    LIBRARY_TRACKS,
    LIBRARY_ALBUMS,
    LIBRARY_ARTISTS,
    LIBRARY_ALBUM_ARTISTS,
    LIBRARY_GENRES,
    LIBRARY_FOLDERS,
    LIBRARY_TITLE_ORDER,
    LIBRARY_PLAYLISTS,
    LIBRARY_LINKS,
    LIBRARY_STRINGS
} LibrarySection;

enum {
    LIBRARY_SECTION_COUNT = LIBRARY_STRINGS + 1,
    LIBRARY_VERSION = 3,
    LIBRARY_HEADER_SIZE = 128,
    LIBRARY_NONE = 0xFFFF
};

typedef enum LibraryStatus {
    LIBRARY_OK,
    LIBRARY_UNREADABLE,
    LIBRARY_BAD_MAGIC,
    LIBRARY_BAD_VERSION,
    LIBRARY_BAD_LAYOUT
} LibraryStatus;

typedef struct LibrarySectionInfo {
    uint32_t offset;
    uint32_t count;
    uint16_t record_size;
    uint16_t music;
} LibrarySectionInfo;

typedef struct Library {
    ByteSource source;
    LibrarySectionInfo sections[LIBRARY_SECTION_COUNT];
} Library;

typedef struct LibraryTrack {
    uint32_t title;
    uint32_t path;
    uint16_t album;
    uint16_t artist;
    uint16_t genre;
    uint16_t folder;
    uint32_t duration_ms;
    uint8_t disc;
    uint8_t number;
} LibraryTrack;

typedef struct LibraryAlbum {
    uint32_t title;
    uint16_t album_artist;
    uint16_t genre;
    uint16_t year;
    uint16_t first_track;
    uint16_t track_count;
    uint16_t cover;
} LibraryAlbum;

typedef struct LibraryGroup {
    uint32_t name;
    uint32_t first_link;
    uint16_t link_count;
} LibraryGroup;

typedef struct LibraryFolder {
    uint32_t name;
    uint16_t parent;
    uint16_t subfolder_count;
    uint32_t first_link;
    uint16_t track_count;
} LibraryFolder;

LibraryStatus library_open(Library *library, ByteSource source, uint32_t size);
uint32_t library_count(const Library *library, LibrarySection section);
uint32_t library_music_count(const Library *library, LibrarySection section);
bool library_read_track(const Library *library, uint32_t index, LibraryTrack *track);
bool library_read_album(const Library *library, uint32_t index, LibraryAlbum *album);
bool library_read_group(const Library *library, LibrarySection section, uint32_t index,
                        LibraryGroup *group);
bool library_read_folder(const Library *library, uint32_t index, LibraryFolder *folder);
bool library_read_index(const Library *library, LibrarySection section, uint32_t position,
                        uint16_t *value);
bool library_read_string(const Library *library, uint32_t offset, char *buffer, size_t capacity);
uint32_t library_fingerprint(const Library *library);
uint32_t library_album_key(const Library *library, uint32_t album_index);
