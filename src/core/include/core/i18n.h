#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/string_builder.h"
#include "core/strings.h"

enum {
    I18N_HEADER_BYTES = 12,
    I18N_VERSION = 1,
    I18N_MAX_ARGUMENTS = 10
};

bool i18n_attach(const uint8_t *pack, size_t size);
void i18n_detach(void);
const char *i18n_text(StringId id);
const char *i18n_english(StringId id);
void i18n_format(StringBuilder *builder, StringId pattern, const char *const *arguments,
                 uint32_t count);
