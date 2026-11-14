#pragma once

#include <stdint.h>

#include "ui/toast.h"

typedef enum ConfKey {
    CONF_DEFAULT_COVER,
    CONF_COUNT
} ConfKey;

typedef struct EchoConf {
    uint8_t values[CONF_COUNT];
} EchoConf;

void conf_file_load(EchoConf *conf, Toast *toast, uint32_t now_ms);
const char *conf_key_name(ConfKey key);
