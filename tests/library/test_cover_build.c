#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "library/cover_build.h"
#include "library/covers.h"
#include "library/library.h"
#include "library/library_scan.h"

enum {
    MAX_FILES = 24,
    PATH_CAPACITY = 64,
    COVER_SIDE = 112,
    PNG_ROW = 1 + COVER_SIDE * 3,
    PNG_RAW = COVER_SIDE * PNG_ROW,
    PNG_CAPACITY = PNG_RAW + 128,
    MEMORY_SIZE = 384 * 1024,
    LIBRARY_CAPACITY = 4 * 1024 * 1024,
    COVERS_CAPACITY = 1024 * 1024,
    SCRATCH_SIZE = 16 * 1024 * 1024,
    RECORD_SLOT = 8 + 4 + COVER_BUILD_RECORD_BYTES,
    MAX_OGGS = 14,
    OGG_CAPACITY = 128 * 1024,
    PAGE_SEGMENTS = 4,
    FRONT = 3,
    BACK = 4
};

typedef struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint16_t pixel;
} Color;

static const Color RED = {255, 0, 0, 0xF800};
static const Color GREEN = {0, 255, 0, 0x07E0};
static const Color BLUE = {0, 0, 255, 0x001F};
static const Color WHITE = {255, 255, 255, 0xFFFF};

typedef struct CardFile {
    char path[PATH_CAPACITY];
    const uint8_t *bytes;
    uint32_t size;
    uint32_t modified;
} CardFile;

typedef struct Card {
    CardFile files[MAX_FILES];
    uint32_t count;
    ScanEntry listing[MAX_FILES];
    uint32_t listed;
    uint32_t cursor;
    uint8_t pngs[MAX_FILES][PNG_CAPACITY];
    uint32_t png_count;
    uint8_t *flac;
    uint32_t flac_size;
    uint8_t oggs[MAX_OGGS][OGG_CAPACITY];
    uint32_t ogg_count;
} Card;

typedef struct Picture {
    uint32_t type;
    const Color *color;
    int damage;
} Picture;

typedef struct OggWriter {
    uint8_t *start;
    uint8_t *at;
    uint32_t sequence;
} OggWriter;

typedef struct Buffer {
    uint8_t *bytes;
    uint32_t capacity;
} Buffer;

static Card card;
static Buffer library_file;
static Buffer covers_file;
static Buffer scratch_file;
static uint32_t covers_capacity = COVERS_CAPACITY;

static bool child_of(const char *dir, const char *path, char *name, bool *directory)
{
    size_t length = strlen(dir);
    while (length > 0 && dir[length - 1] == '/') {
        length--;
    }
    size_t prefix = length + 1;
    if (strncmp(path, dir, length) != 0 || path[length] != '/') {
        return false;
    }
    const char *rest = path + prefix;
    const char *slash = strchr(rest, '/');
    size_t name_length = slash != NULL ? (size_t)(slash - rest) : strlen(rest);
    memcpy(name, rest, name_length);
    name[name_length] = '\0';
    *directory = slash != NULL;
    return name_length > 0;
}

static void list_child(const char *name, bool directory, uint32_t modified)
{
    for (uint32_t index = 0; index < card.listed; index++) {
        if (strcmp(card.listing[index].name, name) == 0) {
            return;
        }
    }
    ScanEntry *entry = &card.listing[card.listed++];
    entry->directory = directory;
    entry->modified = directory ? 0 : modified;
    snprintf(entry->name, sizeof entry->name, "%s", name);
}

static bool card_open_dir(void *context, const char *path)
{
    (void)context;
    card.listed = 0;
    card.cursor = 0;
    for (uint32_t index = 0; index < card.count; index++) {
        char name[PATH_CAPACITY];
        bool directory = false;
        if (child_of(path, card.files[index].path, name, &directory)) {
            list_child(name, directory, card.files[index].modified);
        }
    }
    return card.listed > 0;
}

static bool card_next_entry(void *context, ScanEntry *entry)
{
    (void)context;
    if (card.cursor == card.listed) {
        return false;
    }
    *entry = card.listing[card.cursor++];
    return true;
}

static void card_close_dir(void *context)
{
    (void)context;
}

static bool card_read(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const CardFile *file = context;
    if (offset > file->size || length > file->size - offset) {
        return false;
    }
    memcpy(buffer, file->bytes + offset, length);
    return true;
}

static bool card_open_file(void *context, const char *path, ByteSource *source, uint32_t *size)
{
    (void)context;
    for (uint32_t index = 0; index < card.count; index++) {
        if (strcmp(card.files[index].path, path) == 0) {
            *source = (ByteSource){.read = card_read, .context = &card.files[index]};
            *size = card.files[index].size;
            return true;
        }
    }
    return false;
}

static void card_close_file(void *context)
{
    (void)context;
}

static bool buffer_read(void *context, uint32_t offset, void *bytes, uint32_t length)
{
    const Buffer *buffer = context;
    if (offset > buffer->capacity || length > buffer->capacity - offset) {
        return false;
    }
    memcpy(bytes, buffer->bytes + offset, length);
    return true;
}

static bool buffer_write(void *context, uint32_t offset, const void *bytes, uint32_t length)
{
    Buffer *buffer = context;
    if (offset > buffer->capacity || length > buffer->capacity - offset) {
        return false;
    }
    memcpy(buffer->bytes + offset, bytes, length);
    return true;
}

static uint8_t *put_be32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)(value >> 24);
    at[1] = (uint8_t)(value >> 16);
    at[2] = (uint8_t)(value >> 8);
    at[3] = (uint8_t)value;
    return at + 4;
}

static uint8_t *put_chunk_start(uint8_t *at, uint32_t length, const char *type)
{
    at = put_be32(at, length);
    memcpy(at, type, 4);
    return at + 4;
}

static uint32_t adler32(const uint8_t *bytes, uint32_t length)
{
    uint32_t low = 1;
    uint32_t high = 0;
    for (uint32_t index = 0; index < length; index++) {
        low = (low + bytes[index]) % 65521u;
        high = (high + low) % 65521u;
    }
    return high << 16 | low;
}

static void fill_rows(uint8_t *raw, Color color)
{
    for (uint32_t row = 0; row < COVER_SIDE; row++) {
        uint8_t *line = raw + row * PNG_ROW;
        line[0] = 0;
        for (uint32_t column = 0; column < COVER_SIDE; column++) {
            line[1 + column * 3] = color.red;
            line[2 + column * 3] = color.green;
            line[3 + column * 3] = color.blue;
        }
    }
}

static uint32_t make_png(uint8_t *png, Color color)
{
    static uint8_t raw[PNG_RAW];
    static const uint8_t SIGNATURE[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    static const uint8_t HEADER_TAIL[5] = {8, 2, 0, 0, 0};
    static const uint8_t STORED[7] = {0x78,
                                      0x01,
                                      0x01,
                                      PNG_RAW & 0xFF,
                                      PNG_RAW >> 8,
                                      (uint8_t)~PNG_RAW,
                                      (uint8_t)(~PNG_RAW >> 8)};
    fill_rows(raw, color);
    uint8_t *at = png;
    memcpy(at, SIGNATURE, sizeof SIGNATURE);
    at = put_chunk_start(at + sizeof SIGNATURE, 13, "IHDR");
    at = put_be32(put_be32(at, COVER_SIDE), COVER_SIDE);
    memcpy(at, HEADER_TAIL, sizeof HEADER_TAIL);
    at = put_be32(at + sizeof HEADER_TAIL, 0);
    at = put_chunk_start(at, sizeof STORED + PNG_RAW + 4, "IDAT");
    memcpy(at, STORED, sizeof STORED);
    memcpy(at + sizeof STORED, raw, PNG_RAW);
    at = put_be32(put_be32(at + sizeof STORED + PNG_RAW, adler32(raw, PNG_RAW)), 0);
    at = put_be32(put_chunk_start(at, 0, "IEND"), 0);
    return (uint32_t)(at - png);
}

static uint8_t *put_le32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
    return at + 4;
}

static void ogg_packet(OggWriter *writer, const uint8_t *packet, uint32_t length)
{
    uint32_t done = 0;
    bool finished = false;
    bool continued = false;
    while (!finished) {
        uint8_t *page = writer->at;
        uint32_t segments = 0;
        uint32_t body = 0;
        while (segments < PAGE_SEGMENTS && !finished) {
            uint32_t left = length - done - body;
            uint32_t lace = left < 255 ? left : 255;
            page[27 + segments++] = (uint8_t)lace;
            body += lace;
            finished = lace < 255;
        }
        memcpy(page, "OggS", 4);
        page[4] = 0;
        page[5] = continued ? 1 : 0;
        memset(page + 6, 0, 8);
        put_le32(put_le32(put_le32(page + 14, 1), writer->sequence++), 0);
        page[26] = (uint8_t)segments;
        memcpy(page + 27 + segments, packet + done, body);
        writer->at = page + 27 + segments + body;
        done += body;
        continued = true;
    }
}

static uint8_t *put_comment(uint8_t *at, const char *key, const char *value)
{
    uint32_t length = (uint32_t)(strlen(key) + 1 + strlen(value));
    at = put_le32(at, length);
    return at + sprintf((char *)at, "%s=%s", key, value);
}

static char *base64_encode(const uint8_t *bytes, uint32_t length, char *out)
{
    static const char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (uint32_t index = 0; index < length; index += 3) {
        uint32_t left = length - index;
        uint32_t bits = (uint32_t)bytes[index] << 16;
        bits |= left > 1 ? (uint32_t)bytes[index + 1] << 8 : 0;
        bits |= left > 2 ? bytes[index + 2] : 0;
        *out++ = ALPHABET[bits >> 18];
        *out++ = ALPHABET[(bits >> 12) & 63];
        *out++ = left > 1 ? ALPHABET[(bits >> 6) & 63] : '=';
        *out++ = left > 2 ? ALPHABET[bits & 63] : '=';
    }
    *out = '\0';
    return out;
}

static void picture_text(Picture picture, char *text)
{
    static uint8_t block[PNG_CAPACITY + 64];
    static uint8_t png[PNG_CAPACITY];
    uint32_t png_size = make_png(png, *picture.color);
    uint8_t *at = put_be32(block, picture.type);
    at = put_be32(at, 9);
    memcpy(at, "image/png", 9);
    at = put_be32(at + 9, 0);
    memset(at, 0, 16);
    at = put_be32(at + 16, png_size);
    memcpy(at, png, png_size);
    char *end = base64_encode(block, (uint32_t)(at + png_size - block), text);
    if (picture.damage != 0) {
        (picture.damage > 0 ? text : end)[picture.damage] = '*';
    }
}

static uint32_t make_opus(uint8_t *file, const char *album, uint32_t number,
                          const Picture *pictures, uint32_t picture_count)
{
    static const uint8_t HEAD[19] = {'O', 'p', 'u',  's',  'H', 'e', 'a', 'd', 1, 2,
                                     0,   0,   0x80, 0xBB, 0,   0,   0,   0,   0};
    static uint8_t tags[OGG_CAPACITY];
    static char text[OGG_CAPACITY];
    char track[16];
    OggWriter writer = {file, file, 0};
    ogg_packet(&writer, HEAD, sizeof HEAD);
    memcpy(tags, "OpusTags", 8);
    uint8_t *at = put_le32(put_le32(tags + 8, 0), 3 + picture_count);
    snprintf(track, sizeof track, "%u", (unsigned)number);
    at = put_comment(at, "ALBUM", album);
    at = put_comment(at, "ALBUMARTIST", "Tester");
    at = put_comment(at, "TRACKNUMBER", track);
    for (uint32_t index = 0; index < picture_count; index++) {
        picture_text(pictures[index], text);
        at = put_comment(at, "METADATA_BLOCK_PICTURE", text);
    }
    ogg_packet(&writer, tags, (uint32_t)(at - tags));
    CHECK((uint32_t)(writer.at - file) <= OGG_CAPACITY);
    return (uint32_t)(writer.at - file);
}

static void add_file(const char *path, const uint8_t *bytes, uint32_t size);

static void add_opus(const char *path, const char *album, uint32_t number, const Picture *pictures,
                     uint32_t picture_count)
{
    CHECK(card.ogg_count < MAX_OGGS);
    uint8_t *file = card.oggs[card.ogg_count++];
    add_file(path, file, make_opus(file, album, number, pictures, picture_count));
}

static void add_png(const char *path, Color color)
{
    uint8_t *png = card.pngs[card.png_count++];
    add_file(path, png, make_png(png, color));
}

static uint8_t *read_fixture(const char *name, uint32_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", ECHO_FIXTURE_DIR, name);
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    fseek(file, 0, SEEK_END);
    *size = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *bytes = malloc(*size);
    CHECK(fread(bytes, 1, *size, file) == *size);
    fclose(file);
    return bytes;
}

static void add_file(const char *path, const uint8_t *bytes, uint32_t size)
{
    CardFile *file = &card.files[card.count++];
    snprintf(file->path, sizeof file->path, "%s", path);
    file->bytes = bytes;
    file->size = size;
    file->modified = 1000;
}

static void add_album(const char *album, Color color)
{
    char path[PATH_CAPACITY];
    uint8_t *png = card.pngs[card.png_count++];
    snprintf(path, sizeof path, "/%s/01.flac", album);
    add_file(path, card.flac, card.flac_size);
    snprintf(path, sizeof path, "/%s/cover.png", album);
    add_file(path, png, make_png(png, color));
}

static CardFile *find_file(const char *path)
{
    for (uint32_t index = 0; index < card.count; index++) {
        if (strcmp(card.files[index].path, path) == 0) {
            return &card.files[index];
        }
    }
    return NULL;
}

static void replace_cover(const char *album, Color color)
{
    char path[PATH_CAPACITY];
    snprintf(path, sizeof path, "/%s/cover.png", album);
    CardFile *file = find_file(path);
    CHECK(file != NULL);
    uint8_t *png = card.pngs[card.png_count++];
    file->size = make_png(png, color);
    file->bytes = png;
    file->modified++;
}

static void replace_root_cover(Color color)
{
    CardFile *file = find_file("/cover.png");
    CHECK(file != NULL);
    uint8_t *png = card.pngs[card.png_count++];
    file->size = make_png(png, color);
    file->bytes = png;
    file->modified++;
}

static void remove_file(const char *path)
{
    uint32_t kept = 0;
    for (uint32_t index = 0; index < card.count; index++) {
        if (strcmp(card.files[index].path, path) != 0) {
            card.files[kept++] = card.files[index];
        }
    }
    card.count = kept;
}

static void remove_album(const char *album)
{
    char prefix[PATH_CAPACITY];
    int length = snprintf(prefix, sizeof prefix, "/%s/", album);
    uint32_t kept = 0;
    for (uint32_t index = 0; index < card.count; index++) {
        if (strncmp(card.files[index].path, prefix, (size_t)length) != 0) {
            card.files[kept++] = card.files[index];
        }
    }
    card.count = kept;
}

static void reset(void)
{
    if (card.flac == NULL) {
        card.flac = read_fixture("tags/untagged.flac", &card.flac_size);
        library_file = (Buffer){calloc(LIBRARY_CAPACITY, 1), LIBRARY_CAPACITY};
        covers_file = (Buffer){calloc(COVERS_CAPACITY, 1), COVERS_CAPACITY};
        scratch_file = (Buffer){calloc(SCRATCH_SIZE, 1), SCRATCH_SIZE};
    }
    card.count = 0;
    card.png_count = 0;
    card.ogg_count = 0;
    covers_capacity = COVERS_CAPACITY;
    memset(covers_file.bytes, 0, COVERS_CAPACITY);
}

static ScanResult rescan_options(bool keep, bool default_cover, CoverStats *stats)
{
    static uint8_t memory[MEMORY_SIZE];
    ScanFileSystem files = {card_open_dir,  card_next_entry, card_close_dir,
                            card_open_file, card_close_file, NULL};
    ScanStats scan_stats;
    memset(library_file.bytes, 0, LIBRARY_CAPACITY);
    ScanJob scan = {.files = files,
                    .memory = memory,
                    .size = sizeof memory,
                    .scratch = {buffer_read, buffer_write, &scratch_file, SCRATCH_SIZE},
                    .sink = {buffer_write, &library_file},
                    .capacity = LIBRARY_CAPACITY};
    CHECK_EQUAL(SCAN_OK, library_scan(&scan, &scan_stats));
    covers_file.capacity = covers_capacity;
    CoverJob job = {.files = files,
                    .library = {buffer_read, &library_file},
                    .library_size = LIBRARY_CAPACITY,
                    .sink = {buffer_write, &covers_file},
                    .previous = {keep ? buffer_read : NULL, &covers_file},
                    .capacity = covers_capacity,
                    .memory = memory,
                    .size = sizeof memory,
                    .default_cover = default_cover};
    return cover_build(&job, stats);
}

static ScanResult rescan(bool keep, CoverStats *stats)
{
    return rescan_options(keep, false, stats);
}

static uint32_t album_key(const char *title)
{
    Library library;
    CHECK_EQUAL(LIBRARY_OK,
                library_open(&library, (ByteSource){buffer_read, &library_file}, LIBRARY_CAPACITY));
    for (uint32_t index = 0; index < library_count(&library, LIBRARY_ALBUMS); index++) {
        LibraryAlbum album;
        char text[PATH_CAPACITY];
        CHECK(library_read_album(&library, index, &album));
        CHECK(library_read_string(&library, album.title, text, sizeof text));
        if (strcmp(text, title) == 0) {
            return library_album_key(&library, index);
        }
    }
    CHECK(false);
    return 0;
}

static uint16_t cover_pixel(const char *title)
{
    static uint16_t pixels[COVER_FULL_PIXELS];
    Covers covers;
    uint32_t record = 0;
    CHECK(covers_open(&covers, (ByteSource){buffer_read, &covers_file}, covers_capacity));
    if (!covers_find(&covers, album_key(title), &record)) {
        return 0;
    }
    CHECK(covers_read(&covers, record, COVER_FULL, pixels));
    return pixels[COVER_FULL_PIXELS / 2];
}

static uint16_t default_pixel(void)
{
    static uint16_t pixels[COVER_FULL_PIXELS];
    Covers covers;
    uint32_t record = 0;
    CHECK(covers_open(&covers, (ByteSource){buffer_read, &covers_file}, covers_capacity));
    if (!covers_find(&covers, ECHO_MINI_DEFAULT_COVER, &record)) {
        return 0;
    }
    CHECK(covers_read(&covers, record, COVER_FULL, pixels));
    return pixels[COVER_FULL_PIXELS / 2];
}

static void three_albums(void)
{
    reset();
    add_album("Alpha", RED);
    add_album("Beta", GREEN);
    add_album("Gamma", BLUE);
}

static void the_first_build_decodes_every_album(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.albums);
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, stats.reused);
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void an_unchanged_card_reuses_every_cover(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void a_new_album_is_the_only_one_decoded(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    add_album("Delta", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(WHITE.pixel, cover_pixel("Delta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void replaced_art_is_decoded_again(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    replace_cover("Beta", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(2u, stats.reused);
    CHECK_EQUAL(WHITE.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
}

static void a_removed_album_frees_its_place_for_a_new_one(void)
{
    CoverStats stats;
    three_albums();
    covers_capacity = COVERS_HEADER_SIZE + 3 * RECORD_SLOT;
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    remove_album("Beta");
    add_album("Delta", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(2u, stats.reused);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(WHITE.pixel, cover_pixel("Delta"));
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void a_grown_file_is_built_again_with_room_for_every_album(void)
{
    CoverStats stats;
    three_albums();
    covers_capacity = COVERS_HEADER_SIZE + 2 * RECORD_SLOT;
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(2u, stats.covers);
    CHECK_EQUAL(1u, stats.missing);
    covers_capacity = COVERS_CAPACITY;
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, stats.reused);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.reused);
}

static void an_album_without_art_loses_its_cover(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    remove_album("Beta");
    add_file("/Beta/01.flac", card.flac, card.flac_size);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(2u, stats.covers);
    CHECK_EQUAL(1u, stats.missing);
    CHECK_EQUAL(0u, cover_pixel("Beta"));
}

static void a_file_without_stamps_is_built_again(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    memset(covers_file.bytes + 12, 0, 4);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, stats.reused);
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
}

static void a_damaged_index_is_ignored(void)
{
    CoverStats stats;
    uint8_t first[8];
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    memcpy(first, covers_file.bytes + COVERS_HEADER_SIZE, sizeof first);
    memcpy(covers_file.bytes + COVERS_HEADER_SIZE, covers_file.bytes + COVERS_HEADER_SIZE + 8, 8);
    memcpy(covers_file.bytes + COVERS_HEADER_SIZE + 8, first, sizeof first);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, stats.reused);
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
}

static void without_a_previous_file_everything_is_decoded(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(SCAN_OK, rescan(false, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, stats.reused);
}

static void opus_art_is_decoded_and_kept(void)
{
    CoverStats stats;
    const Picture pictures[] = {{BACK, &GREEN, 0}, {FRONT, &RED, 0}};
    reset();
    add_opus("/Opus/01.opus", "Opus", 1, pictures, 2);
    add_png("/Opus/cover.png", BLUE);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(1u, stats.covers);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(RED.pixel, cover_pixel("Opus"));
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(1u, stats.reused);
    CHECK_EQUAL(RED.pixel, cover_pixel("Opus"));
}

static void albums_sharing_a_folder_find_art_in_later_tracks(void)
{
    CoverStats stats;
    const Picture red = {FRONT, &RED, 0};
    const Picture green = {BACK, &GREEN, 0};
    const Picture broken_header = {FRONT, &BLUE, 21};
    const Picture broken_image = {FRONT, &BLUE, -100};
    reset();
    add_opus("/Mixed/a1.opus", "A", 1, NULL, 0);
    add_opus("/Mixed/a2.opus", "A", 2, &red, 1);
    add_opus("/Mixed/b1.opus", "B", 1, &broken_header, 1);
    add_opus("/Mixed/b2.opus", "B", 2, NULL, 0);
    add_opus("/Mixed/b3.opus", "B", 3, NULL, 0);
    add_opus("/Mixed/b4.opus", "B", 4, &green, 1);
    add_opus("/Mixed/c1.opus", "C", 1, NULL, 0);
    for (uint32_t number = 1; number <= 5; number++) {
        char path[PATH_CAPACITY];
        snprintf(path, sizeof path, "/Mixed/d%u.opus", (unsigned)number);
        add_opus(path, "D", number, number == 5 ? &red : NULL, number == 5 ? 1 : 0);
    }
    add_opus("/Mixed/e1.opus", "E", 1, &broken_image, 1);
    add_opus("/Mixed/e2.opus", "E", 2, &red, 1);
    add_png("/Mixed/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(5u, stats.albums);
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(1u, stats.missing);
    CHECK_EQUAL(RED.pixel, cover_pixel("A"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("B"));
    CHECK_EQUAL(WHITE.pixel, cover_pixel("C"));
    CHECK_EQUAL(WHITE.pixel, cover_pixel("D"));
    CHECK_EQUAL(0u, cover_pixel("E"));
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(4u, stats.reused);
}

static void the_default_cover_is_cached_only_when_enabled(void)
{
    CoverStats stats;
    three_albums();
    add_file("/Bare/01.flac", card.flac, card.flac_size);
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(4u, stats.albums);
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(1u, stats.missing);
    CHECK_EQUAL(WHITE.pixel, default_pixel());
    CHECK_EQUAL(0u, cover_pixel("Bare"));
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(SCAN_OK, rescan_options(true, false, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, default_pixel());
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
}

static void the_default_cover_uses_the_folder_art_names(void)
{
    CoverStats stats;
    reset();
    add_album("Alpha", RED);
    add_png("/FOLDER.PNG", GREEN);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(GREEN.pixel, default_pixel());
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(2u, stats.covers);
    CHECK_EQUAL(1u, stats.reused);
    CHECK_EQUAL(WHITE.pixel, default_pixel());
}

static void an_unchanged_default_cover_is_reused(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(4u, stats.reused);
    CHECK_EQUAL(WHITE.pixel, default_pixel());
}

static void disabling_the_default_drops_its_entry(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(SCAN_OK, rescan_options(true, false, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(0u, default_pixel());
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(WHITE.pixel, default_pixel());
}

static void removing_the_root_art_drops_the_default(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    remove_file("/cover.png");
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(0u, default_pixel());
}

static void replaced_default_art_is_decoded_again(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    replace_root_cover(BLUE);
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(BLUE.pixel, default_pixel());
}

static void the_default_fits_beside_every_album(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    covers_capacity = COVERS_HEADER_SIZE + 4 * RECORD_SLOT;
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(4u, stats.covers);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(WHITE.pixel, default_pixel());
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void albums_keep_their_slots_when_the_default_cannot_fit(void)
{
    CoverStats stats;
    three_albums();
    add_png("/cover.png", WHITE);
    covers_capacity = COVERS_HEADER_SIZE + 3 * RECORD_SLOT;
    CHECK_EQUAL(SCAN_OK, rescan_options(true, true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(1u, stats.missing);
    CHECK_EQUAL(0u, default_pixel());
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

static void add_apple_double(const char *path)
{
    static const uint8_t HEADER[] = {0x00, 0x05, 0x16, 0x07, 0x00, 0x02, 0x00, 0x00,
                                     'M',  'a',  'c',  ' ',  'O',  'S',  ' ',  'X'};
    add_file(path, HEADER, sizeof HEADER);
}

static void mac_litter(void)
{
    static const uint8_t JUNK[64] = {0};
    add_file("/.fseventsd/fseventsd-uuid", JUNK, sizeof JUNK);
    add_file("/.Spotlight-V100/Store-V2/0/store.db", JUNK, sizeof JUNK);
    add_file("/.Trashes/501/old.flac", card.flac, card.flac_size);
    add_file("/Alpha/.DS_Store", JUNK, sizeof JUNK);
    add_apple_double("/Alpha/._01.flac");
    add_apple_double("/Alpha/._cover.png");
    add_apple_double("/._Beta");
    add_file("/Beta/._02.flac", card.flac, card.flac_size);
}

static void a_card_written_on_a_mac_keeps_the_covers_the_computer_made(void)
{
    CoverStats stats;
    three_albums();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.covers);
    mac_litter();
    CHECK_EQUAL(SCAN_OK, rescan(true, &stats));
    CHECK_EQUAL(3u, stats.albums);
    CHECK_EQUAL(3u, stats.covers);
    CHECK_EQUAL(3u, stats.reused);
    CHECK_EQUAL(0u, stats.missing);
    CHECK_EQUAL(RED.pixel, cover_pixel("Alpha"));
    CHECK_EQUAL(GREEN.pixel, cover_pixel("Beta"));
    CHECK_EQUAL(BLUE.pixel, cover_pixel("Gamma"));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_card_written_on_a_mac_keeps_the_covers_the_computer_made),
        TEST_CASE(the_first_build_decodes_every_album),
        TEST_CASE(an_unchanged_card_reuses_every_cover),
        TEST_CASE(a_new_album_is_the_only_one_decoded),
        TEST_CASE(replaced_art_is_decoded_again),
        TEST_CASE(a_removed_album_frees_its_place_for_a_new_one),
        TEST_CASE(a_grown_file_is_built_again_with_room_for_every_album),
        TEST_CASE(an_album_without_art_loses_its_cover),
        TEST_CASE(a_file_without_stamps_is_built_again),
        TEST_CASE(a_damaged_index_is_ignored),
        TEST_CASE(without_a_previous_file_everything_is_decoded),
        TEST_CASE(opus_art_is_decoded_and_kept),
        TEST_CASE(albums_sharing_a_folder_find_art_in_later_tracks),
        TEST_CASE(the_default_cover_is_cached_only_when_enabled),
        TEST_CASE(the_default_cover_uses_the_folder_art_names),
        TEST_CASE(an_unchanged_default_cover_is_reused),
        TEST_CASE(disabling_the_default_drops_its_entry),
        TEST_CASE(removing_the_root_art_drops_the_default),
        TEST_CASE(replaced_default_art_is_decoded_again),
        TEST_CASE(the_default_fits_beside_every_album),
        TEST_CASE(albums_keep_their_slots_when_the_default_cannot_fit),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
