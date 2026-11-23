#pragma once

#include <stdint.h>

enum {
    APP_STARTUP_MAGIC = 0x50555453
};

typedef struct AppStartup {
    uint32_t magic;
    void (*start)(void);
} AppStartup;

extern const AppStartup APP_STARTUP;
