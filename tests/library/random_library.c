#include "random_library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    TEXT_CAPACITY = 512,
    ENTRIES_PER_PLAYLIST = 40
};

static const char *const WORDS[] = {"love",         "night",
                                    "blue",         "fire",
                                    "dream",        "heart",
                                    "road",         "light",
                                    "rain",         "gold",
                                    "river",        "storm",
                                    "ocean",        "shadow",
                                    "city",         "dance",
                                    "echo",         "wild",
                                    "stone",        "glass",
                                    "silver",       "north",
                                    "summer",       "ghost",
                                    "time",         "star",
                                    "home",         "\xc3\xa9t\xc3\xa9",
                                    "na\xc3\xafve", "caf\xc3\xa9",
                                    "\xc3\x85ngel", "M\xc3\xb6tley",
                                    "\xc3\x9clfur", "sm\xc3\xb6rg\xc3\xa5s"};

static const char *const GENRES[] = {
    "Rock", "rock",    "Pop",   "Jazz",       "Electronic",    "Metal", "Classical",     "Hip-Hop",
    "Folk", "Ambient", "Blues", "Soundtrack", "Unknown Genre", "Indie", "\xc3\x89lectro"};

static const char *const EXTENSIONS[] = {".flac", ".mp3", ".m4a", ".ogg", ".wav"};

typedef struct Random {
    uint32_t state;
} Random;

static uint32_t next(Random *random)
{
    uint32_t value = random->state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random->state = value;
    return value;
}

static uint32_t below(Random *random, uint32_t limit)
{
    return limit == 0 ? 0 : next(random) % limit;
}

static char *keep(RandomLibrary *library, const char *text)
{
    if (library->string_count == library->string_capacity) {
        library->string_capacity = library->string_capacity * 2 + 64;
        library->strings =
            realloc(library->strings, library->string_capacity * sizeof library->strings[0]);
    }
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    memcpy(copy, text, length + 1);
    library->strings[library->string_count++] = copy;
    return copy;
}

static void words(Random *random, char *out, size_t capacity, uint32_t count)
{
    out[0] = '\0';
    for (uint32_t index = 0; index < count; index++) {
        const char *word = WORDS[below(random, sizeof WORDS / sizeof WORDS[0])];
        size_t used = strlen(out);
        snprintf(out + used, capacity - used, "%s%c%s", index == 0 ? "" : " ",
                 index == 0 && word[0] >= 'a' && word[0] <= 'z' ? (char)(word[0] - 32) : word[0],
                 word + 1);
    }
}

static void artist_name(Random *random, uint32_t artist, char *out, size_t capacity)
{
    Random own = {.state = 0x9E3779B9u ^ (artist * 2654435761u)};
    char base[TEXT_CAPACITY];
    words(&own, base, sizeof base, 1 + below(&own, 2));
    switch (below(random, 10)) {
    case 0:
        snprintf(out, capacity, "The %s", base);
        break;
    case 1:
        snprintf(out, capacity, "the %s", base);
        break;
    case 2:
        for (size_t index = 0; base[index] != '\0'; index++) {
            base[index] =
                base[index] >= 'A' && base[index] <= 'Z' ? (char)(base[index] + 32) : base[index];
        }
        snprintf(out, capacity, "%s", base);
        break;
    default:
        snprintf(out, capacity, "%s", base);
        break;
    }
}

static void folder_safe(const char *text, char *out, size_t capacity)
{
    size_t used = 0;
    for (size_t index = 0; text[index] != '\0' && used + 1 < capacity; index++) {
        out[used++] = text[index] == '/' ? '_' : text[index];
    }
    out[used] = '\0';
}

static int compare_paths(const void *left, const void *right)
{
    const unsigned char *a = (const unsigned char *)((const BuilderTrack *)left)->path;
    const unsigned char *b = (const unsigned char *)((const BuilderTrack *)right)->path;
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    int x = *a == '/' ? 1 : *a == '\0' ? 0 : *a + 1;
    int y = *b == '/' ? 1 : *b == '\0' ? 0 : *b + 1;
    return x - y;
}

static void make_track(RandomLibrary *library, Random *random, uint32_t index, uint32_t artists)
{
    BuilderTrack *track = &library->tracks[index];
    uint32_t artist = below(random, artists);
    uint32_t album = below(random, 4);
    char artist_text[TEXT_CAPACITY];
    char album_artist_text[TEXT_CAPACITY];
    char album_text[TEXT_CAPACITY];
    char title_text[TEXT_CAPACITY];
    char folder_artist[TEXT_CAPACITY];
    char folder_album[TEXT_CAPACITY];
    char path[TEXT_CAPACITY * 3];
    artist_name(random, artist, artist_text, sizeof artist_text);
    bool compilation = below(random, 12) == 0;
    bool guessed = below(random, 3) == 0;
    snprintf(album_artist_text, sizeof album_artist_text, "%s",
             compilation ? "Various Artists" : artist_text);
    Random album_random = {.state = (artist + 1) * 40503u + album * 7919u + 17u};
    if (below(random, 25) == 0) {
        snprintf(album_text, sizeof album_text, "Greatest Hits");
    } else {
        words(&album_random, album_text, sizeof album_text, 1 + below(&album_random, 3));
    }
    words(random, title_text, sizeof title_text, 1 + below(random, 4));
    if (below(random, 30) == 0) {
        snprintf(title_text, sizeof title_text, "Intro");
    }
    uint8_t disc = below(random, 10) == 0 ? 2 : 1;
    uint8_t number = (uint8_t)below(random, 21);
    folder_safe(album_artist_text, folder_artist, sizeof folder_artist);
    folder_safe(album_text, folder_album, sizeof folder_album);
    const char *extension = EXTENSIONS[below(random, sizeof EXTENSIONS / sizeof EXTENSIONS[0])];
    uint32_t shape = below(random, 40);
    if (shape == 0) {
        snprintf(path, sizeof path, "%02u - %s %u%s", number, title_text, index, extension);
        album_text[0] = '\0';
    } else if (shape == 1) {
        snprintf(path, sizeof path, "%s/%s/%s/CD%u/%02u %s %u%s", GENRES[below(random, 4)],
                 folder_artist, folder_album, disc, number, title_text, index, extension);
    } else if (shape == 2) {
        snprintf(path, sizeof path, "Loose/%s %u%s", title_text, index, extension);
    } else {
        snprintf(path, sizeof path, "%s/%s/%u-%02u - %s %u%s", folder_artist, folder_album, disc,
                 number, title_text, index, extension);
    }
    track->path = keep(library, path);
    track->title = keep(library, title_text);
    track->artist = keep(library, artist_text);
    track->album_artist = keep(library, guessed ? artist_text : album_artist_text);
    track->album_artist_guessed = guessed;
    track->album = keep(library, album_text);
    track->genre = keep(library, GENRES[below(random, sizeof GENRES / sizeof GENRES[0])]);
    track->year = below(random, 8) == 0 ? 0 : (uint16_t)(1960 + below(&album_random, 66));
    track->disc = disc;
    track->number = number;
    track->duration_ms = 60000 + below(random, 400000);
}

static void windows_path(const char *path, char *out, size_t capacity)
{
    size_t used = (size_t)snprintf(out, capacity, "D:\\Music\\");
    for (size_t index = 0; path[index] != '\0' && used + 1 < capacity; index++) {
        out[used++] = path[index] == '/' ? '\\' : path[index];
    }
    out[used] = '\0';
}

static void upper_path(const char *path, char *out, size_t capacity)
{
    size_t used = 0;
    for (size_t index = 0; path[index] != '\0' && used + 1 < capacity; index++) {
        char letter = path[index];
        out[used++] = letter >= 'a' && letter <= 'z' ? (char)(letter - 32) : letter;
    }
    out[used] = '\0';
}

static char *playlist_entry(RandomLibrary *library, Random *random, const char *folder)
{
    char text[TEXT_CAPACITY * 4];
    const char *path = library->tracks[below(random, library->track_count)].path;
    switch (below(random, 7)) {
    case 0:
        snprintf(text, sizeof text, "%s%s", folder[0] != '\0' ? "../" : "", path);
        break;
    case 1:
        snprintf(text, sizeof text, "/%s", path);
        break;
    case 2:
        windows_path(path, text, sizeof text);
        break;
    case 3:
        upper_path(path, text, sizeof text);
        break;
    case 4:
        snprintf(text, sizeof text, "Missing/%u.flac", below(random, 1000));
        break;
    case 5:
        snprintf(text, sizeof text, "http://example.com/%u.mp3", below(random, 1000));
        break;
    default:
        snprintf(text, sizeof text, "%s", path);
        break;
    }
    return keep(library, text);
}

static void make_playlists(RandomLibrary *library, Random *random, uint32_t count)
{
    library->playlists = calloc(count > 0 ? count : 1, sizeof library->playlists[0]);
    library->playlist_count = count;
    for (uint32_t index = 0; index < count; index++) {
        RandomPlaylist *playlist = &library->playlists[index];
        char name[TEXT_CAPACITY];
        words(random, name, sizeof name, 1 + below(random, 2));
        playlist->name = keep(library, name);
        playlist->folder = keep(library, below(random, 2) == 0 ? "Playlists" : "");
        playlist->count = below(random, ENTRIES_PER_PLAYLIST);
        playlist->entries = calloc(playlist->count + 1, sizeof playlist->entries[0]);
        for (uint32_t entry = 0; entry < playlist->count; entry++) {
            playlist->entries[entry] = playlist_entry(library, random, playlist->folder);
        }
    }
}

void random_library_make(RandomLibrary *library, uint32_t tracks, uint32_t playlists, uint32_t seed)
{
    memset(library, 0, sizeof *library);
    Random random = {.state = seed * 2654435761u + 1u};
    library->tracks = calloc(tracks > 0 ? tracks : 1, sizeof library->tracks[0]);
    library->track_count = tracks;
    uint32_t artists = tracks / 12 + 1;
    for (uint32_t index = 0; index < tracks; index++) {
        make_track(library, &random, index, artists);
    }
    qsort(library->tracks, tracks, sizeof library->tracks[0], compare_paths);
    make_playlists(library, &random, tracks > 0 ? playlists : 0);
}

void random_library_free(RandomLibrary *library)
{
    for (uint32_t index = 0; index < library->string_count; index++) {
        free(library->strings[index]);
    }
    for (uint32_t index = 0; index < library->playlist_count; index++) {
        free(library->playlists[index].entries);
    }
    free(library->strings);
    free(library->playlists);
    free(library->tracks);
    memset(library, 0, sizeof *library);
}
