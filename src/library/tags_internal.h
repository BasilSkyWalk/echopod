#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "library/track_tags.h"

typedef enum TagEncoding {
    TAG_LATIN1,
    TAG_UTF8,
    TAG_UTF16LE,
    TAG_UTF16BE
} TagEncoding;

typedef struct TagReader {
    ByteSource source;
    uint32_t size;
    TrackTags *tags;
    bool present[TAG_FIELD_COUNT];
} TagReader;

bool tag_read(const TagReader *reader, uint32_t offset, void *buffer, uint32_t length);
uint16_t tag_le16(const uint8_t *bytes);
uint32_t tag_le32(const uint8_t *bytes);
uint64_t tag_le64(const uint8_t *bytes);
uint16_t tag_be16(const uint8_t *bytes);
uint32_t tag_be32(const uint8_t *bytes);
uint64_t tag_be64(const uint8_t *bytes);
bool tag_same_text(const char *text, uint32_t length, const char *wanted);
bool tag_same_text_folded(const char *text, uint32_t length, const char *wanted);
void tag_mark_present(TagReader *reader, TagField field);
void tag_decode(char *out, const uint8_t *text, uint32_t length, TagEncoding encoding);
void tag_offer(TagReader *reader, TagField field, const uint8_t *text, uint32_t length,
               TagEncoding encoding);
void tag_offer_decoded(TagReader *reader, TagField field, const char *text);
void tag_offer_picture(TagReader *reader, TagPicture picture);
void tag_set_duration(TagReader *reader, double seconds);

bool tags_read_flac(TagReader *reader);
bool tags_read_ogg(TagReader *reader);
bool tags_read_vorbis_comment(TagReader *reader, ByteSource source, uint32_t offset,
                              uint32_t limit);
bool tags_read_id3v2(TagReader *reader, uint32_t offset, uint32_t *tag_size);
void tags_read_id3v1(TagReader *reader);
const char *tags_id3_genre(uint32_t index);
bool tags_read_mp3(TagReader *reader);
bool tags_read_mp4(TagReader *reader);
bool tags_read_wav(TagReader *reader);
bool tags_read_dsf(TagReader *reader);
bool tags_read_dff(TagReader *reader);
bool tags_read_ape(TagReader *reader);
bool tags_read_wma(TagReader *reader);
