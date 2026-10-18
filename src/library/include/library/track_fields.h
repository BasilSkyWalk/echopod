#pragma once

#include <stdint.h>

#include "library/library_builder.h"
#include "library/track_tags.h"

enum {
    TRACK_FIELD_TEXT_CAPACITY = 768
};

typedef struct TrackFields {
    char title[TRACK_FIELD_TEXT_CAPACITY];
    char album[TRACK_FIELD_TEXT_CAPACITY];
    BuilderTrack track;
} TrackFields;

uint32_t track_fields_number(const char *text);
uint32_t track_fields_year(const char *text);
void track_fields_from_tags(const char *path, const TrackTags *tags, TrackFields *fields);
