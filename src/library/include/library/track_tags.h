#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/byte_source.h"

enum {
    TRACK_TAG_TEXT_CAPACITY = 256
};

typedef enum TagFormat {
    TAG_FORMAT_UNKNOWN,
    TAG_FORMAT_FLAC,
    TAG_FORMAT_OGG,
    TAG_FORMAT_MP3,
    TAG_FORMAT_MP4,
    TAG_FORMAT_WAV,
    TAG_FORMAT_WMA,
    TAG_FORMAT_APE,
    TAG_FORMAT_DSF,
    TAG_FORMAT_DFF
} TagFormat;

typedef enum TagField {
    TAG_TITLE,
    TAG_ARTIST,
    TAG_ALBUM_ARTIST,
    TAG_ALBUM,
    TAG_GENRE,
    TAG_DATE,
    TAG_DISC,
    TAG_TRACK,
    TAG_FIELD_COUNT
} TagField;

typedef enum TagPictureKind {
    TAG_PICTURE_NONE,
    TAG_PICTURE_RAW,
    TAG_PICTURE_BASE64_FLAC_BLOCK
} TagPictureKind;

typedef struct TagPicture {
    TagPictureKind kind;
    bool front;
    uint32_t offset;
    uint32_t length;
} TagPicture;

typedef struct TrackTags {
    char text[TAG_FIELD_COUNT][TRACK_TAG_TEXT_CAPACITY];
    uint32_t duration_ms;
    TagPicture picture;
} TrackTags;

TagFormat track_tags_format_for_name(const char *name);
bool track_tags_read(ByteSource source, uint32_t size, TagFormat format, TrackTags *tags);
