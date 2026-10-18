#include <stddef.h>

#include "builder_internal.h"

enum {
    HEADER_SIZE = 128,
    SECTION_COUNT = 10,
    VERSION = 2,
    ALIGNMENT = 4,
    TRACK_RECORD = 24,
    ALBUM_RECORD = 16,
    GROUP_RECORD = 12,
    FOLDER_RECORD = 16,
    INDEX_RECORD = 2,
    BUFFER_SIZE = 512
};

typedef enum Section {
    SECTION_TRACKS,
    SECTION_ALBUMS,
    SECTION_ARTISTS,
    SECTION_ALBUM_ARTISTS,
    SECTION_GENRES,
    SECTION_FOLDERS,
    SECTION_TITLE_ORDER,
    SECTION_PLAYLISTS,
    SECTION_LINKS,
    SECTION_STRINGS
} Section;

typedef struct Writer {
    ByteSink sink;
    uint32_t offset;
    uint32_t used;
    bool failed;
    uint8_t buffer[BUFFER_SIZE];
} Writer;

typedef struct Layout {
    uint32_t offset[SECTION_COUNT];
    uint32_t count[SECTION_COUNT];
    uint16_t record_size[SECTION_COUNT];
    uint32_t end;
} Layout;

typedef struct StringPass {
    LibraryBuilder *builder;
    const LibraryModel *model;
    uint32_t *path_offsets;
    uint32_t length;
    Writer *writer;
} StringPass;

static void flush(Writer *writer)
{
    if (writer->used > 0 && !writer->failed) {
        writer->failed = !writer->sink.write(writer->sink.context, writer->offset - writer->used,
                                             writer->buffer, writer->used);
    }
    writer->used = 0;
}

static void put_byte(Writer *writer, uint8_t value)
{
    writer->buffer[writer->used++] = value;
    writer->offset++;
    if (writer->used == BUFFER_SIZE) {
        flush(writer);
    }
}

static void put_u16(Writer *writer, uint32_t value)
{
    put_byte(writer, (uint8_t)value);
    put_byte(writer, (uint8_t)(value >> 8));
}

static void put_u32(Writer *writer, uint32_t value)
{
    put_u16(writer, value & 0xFFFF);
    put_u16(writer, value >> 16);
}

static void pad_to(Writer *writer, uint32_t offset)
{
    while (writer->offset < offset) {
        put_byte(writer, 0);
    }
}

static void put_text(Writer *writer, const char *text)
{
    for (; *text != '\0'; text++) {
        put_byte(writer, (uint8_t)*text);
    }
}

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void add_string(StringPass *pass, uint32_t string)
{
    uint32_t *output = builder_output_offset(pass->builder, string);
    if (*output == BUILDER_UNSET) {
        *output = pass->length;
        pass->length += text_length(builder_text(pass->builder, string)) + 1;
        if (pass->writer != NULL) {
            put_text(pass->writer, builder_text(pass->builder, string));
            put_byte(pass->writer, 0);
        }
    } else if (pass->writer != NULL && *output == pass->length) {
        pass->length += text_length(builder_text(pass->builder, string)) + 1;
        put_text(pass->writer, builder_text(pass->builder, string));
        put_byte(pass->writer, 0);
    }
}

static uint32_t path_length(const LibraryBuilder *builder, const TrackRecord *track)
{
    uint32_t length = text_length(builder_text(builder, track->file_name));
    for (uint16_t node = track->folder; node != 0; node = builder_folder(builder, node)->parent) {
        length += text_length(builder_text(builder, builder_folder(builder, node)->name)) + 1;
    }
    return length;
}

static void put_folder_path(Writer *writer, const LibraryBuilder *builder, uint16_t node)
{
    uint16_t chain[64];
    uint32_t depth = 0;
    for (; node != 0 && depth < 64; node = builder_folder(builder, node)->parent) {
        chain[depth++] = node;
    }
    while (depth > 0) {
        put_text(writer, builder_text(builder, builder_folder(builder, chain[--depth])->name));
        put_byte(writer, '/');
    }
}

static void add_path(StringPass *pass, uint32_t position)
{
    const TrackRecord *track = builder_track(pass->builder, pass->model->final_tracks[position]);
    if (pass->writer == NULL) {
        pass->path_offsets[position] = pass->length;
    } else {
        put_folder_path(pass->writer, pass->builder, track->folder);
        put_text(pass->writer, builder_text(pass->builder, track->file_name));
        put_byte(pass->writer, 0);
    }
    pass->length += path_length(pass->builder, track) + 1;
}

static void add_names(StringPass *pass, const NameIndex *index)
{
    for (uint32_t group = 0; group < index->count; group++) {
        add_string(pass, index->display[group]);
    }
}

static void run_string_pass(StringPass *pass)
{
    const LibraryModel *model = pass->model;
    pass->length = 1;
    for (uint32_t position = 0; position < model->track_count; position++) {
        add_string(pass, builder_track(pass->builder, model->final_tracks[position])->title);
        add_path(pass, position);
    }
    for (uint32_t album = 0; album < model->album_count; album++) {
        add_string(pass, model->albums[album].title);
    }
    add_names(pass, &model->artists);
    add_names(pass, &model->album_artists);
    add_names(pass, &model->genres);
    for (uint32_t folder = 0; folder < model->folder_count; folder++) {
        add_string(pass, model->folders[folder].name);
    }
}

static void plan(Layout *layout, const LibraryModel *model, uint32_t strings)
{
    uint32_t links = model->artist_links.total + model->album_artist_links.total +
                     model->genre_links.total + model->folder_link_count;
    const uint32_t counts[SECTION_COUNT] = {model->track_count,
                                            model->album_count,
                                            model->artists.count,
                                            model->album_artists.count,
                                            model->genres.count,
                                            model->folder_count,
                                            model->track_count,
                                            0,
                                            links,
                                            strings};
    const uint16_t sizes[SECTION_COUNT] = {
        TRACK_RECORD,  ALBUM_RECORD, GROUP_RECORD, GROUP_RECORD, GROUP_RECORD,
        FOLDER_RECORD, INDEX_RECORD, GROUP_RECORD, INDEX_RECORD, 1};
    uint32_t offset = HEADER_SIZE;
    for (int section = 0; section < SECTION_COUNT; section++) {
        offset = (offset + ALIGNMENT - 1) & ~(uint32_t)(ALIGNMENT - 1);
        layout->offset[section] = offset;
        layout->count[section] = counts[section];
        layout->record_size[section] = sizes[section];
        offset += counts[section] * sizes[section];
    }
    layout->end = offset;
}

static void write_header(Writer *writer, const Layout *layout)
{
    put_u32(writer, 0);
    put_u16(writer, VERSION);
    put_u16(writer, SECTION_COUNT);
    for (int section = 0; section < SECTION_COUNT; section++) {
        put_u32(writer, layout->offset[section]);
        put_u32(writer, layout->count[section]);
        put_u16(writer, layout->record_size[section]);
        put_u16(writer, 0);
    }
    pad_to(writer, HEADER_SIZE);
}

static void write_tracks(Writer *writer, const LibraryBuilder *builder, const LibraryModel *model,
                         const uint32_t *path_offsets)
{
    for (uint32_t position = 0; position < model->track_count; position++) {
        const TrackRecord *track = builder_track(builder, model->final_tracks[position]);
        put_u32(writer, *builder_output_offset(builder, track->title));
        put_u32(writer, path_offsets[position]);
        put_u16(writer, model->track_album[position]);
        put_u16(writer, model->artists.group_of_track[model->final_tracks[position]]);
        put_u16(writer, model->genres.group_of_track[model->final_tracks[position]]);
        put_u16(writer, model->track_folder[position]);
        put_u32(writer, track->duration_ms);
        put_byte(writer, track->disc);
        put_byte(writer, track->number);
        put_u16(writer, 0);
    }
}

static void write_albums(Writer *writer, const LibraryBuilder *builder, const LibraryModel *model)
{
    for (uint32_t album = 0; album < model->album_count; album++) {
        const AlbumModel *record = &model->albums[album];
        put_u32(writer, *builder_output_offset(builder, record->title));
        put_u16(writer, record->album_artist);
        put_u16(writer, record->genre);
        put_u16(writer, record->year);
        put_u16(writer, record->first_track);
        put_u16(writer, record->track_count);
        put_u16(writer, BUILDER_NONE);
    }
}

static uint32_t write_groups(Writer *writer, const LibraryBuilder *builder, const NameIndex *index,
                             const GroupLinks *links, uint32_t first_link)
{
    for (uint32_t group = 0; group < index->count; group++) {
        put_u32(writer, *builder_output_offset(builder, index->display[group]));
        put_u32(writer, first_link + links->first[group]);
        put_u16(writer, links->count[group]);
        put_u16(writer, 0);
    }
    return first_link + links->total;
}

static void write_folders(Writer *writer, const LibraryBuilder *builder, const LibraryModel *model,
                          uint32_t first_link)
{
    for (uint32_t folder = 0; folder < model->folder_count; folder++) {
        const FolderModel *record = &model->folders[folder];
        put_u32(writer, *builder_output_offset(builder, record->name));
        put_u16(writer, record->parent);
        put_u16(writer, record->subfolder_count);
        put_u32(writer, first_link + record->first_link);
        put_u16(writer, record->track_count);
        put_u16(writer, 0);
    }
}

static void write_indices(Writer *writer, const uint16_t *values, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        put_u16(writer, values[index]);
    }
}

static void write_links(Writer *writer, const LibraryModel *model)
{
    write_indices(writer, model->artist_links.albums, model->artist_links.total);
    write_indices(writer, model->album_artist_links.albums, model->album_artist_links.total);
    write_indices(writer, model->genre_links.albums, model->genre_links.total);
    write_indices(writer, model->folder_links, model->folder_link_count);
}

static void write_records(Writer *writer, LibraryBuilder *builder, const LibraryModel *model,
                          const Layout *layout, const uint32_t *path_offsets)
{
    write_header(writer, layout);
    write_tracks(writer, builder, model, path_offsets);
    pad_to(writer, layout->offset[SECTION_ALBUMS]);
    write_albums(writer, builder, model);
    pad_to(writer, layout->offset[SECTION_ARTISTS]);
    uint32_t links = write_groups(writer, builder, &model->artists, &model->artist_links, 0);
    pad_to(writer, layout->offset[SECTION_ALBUM_ARTISTS]);
    links = write_groups(writer, builder, &model->album_artists, &model->album_artist_links, links);
    pad_to(writer, layout->offset[SECTION_GENRES]);
    links = write_groups(writer, builder, &model->genres, &model->genre_links, links);
    pad_to(writer, layout->offset[SECTION_FOLDERS]);
    write_folders(writer, builder, model, links);
    pad_to(writer, layout->offset[SECTION_TITLE_ORDER]);
    write_indices(writer, model->title_order, model->track_count);
    pad_to(writer, layout->offset[SECTION_LINKS]);
    write_links(writer, model);
    pad_to(writer, layout->offset[SECTION_STRINGS]);
}

BuilderResult builder_encode(LibraryBuilder *builder, const LibraryModel *model, Work *work,
                             ByteSink sink, uint32_t *length)
{
    uint32_t *path_offsets = work_alloc(work, model->track_count * 4 + 4);
    if (path_offsets == NULL) {
        return BUILDER_OUT_OF_MEMORY;
    }
    *builder_output_offset(builder, builder->empty_string) = 0;
    StringPass pass = {.builder = builder, .model = model, .path_offsets = path_offsets};
    run_string_pass(&pass);
    Layout layout;
    plan(&layout, model, pass.length);
    Writer writer = {.sink = sink, .offset = 0, .used = 0, .failed = false};
    write_records(&writer, builder, model, &layout, path_offsets);
    put_byte(&writer, 0);
    StringPass emit = {
        .builder = builder, .model = model, .path_offsets = path_offsets, .writer = &writer};
    run_string_pass(&emit);
    flush(&writer);
    static const uint8_t MAGIC[4] = {'E', 'M', 'D', 'B'};
    if (writer.failed || !sink.write(sink.context, 0, MAGIC, sizeof MAGIC)) {
        return BUILDER_WRITE_FAILED;
    }
    *length = layout.end;
    return BUILDER_OK;
}
