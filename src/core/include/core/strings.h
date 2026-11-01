#pragma once

typedef enum StringId {
#define STRING(id, text) id,
#include "core/strings_all.def"
#undef STRING
    STRING_COUNT
} StringId;
