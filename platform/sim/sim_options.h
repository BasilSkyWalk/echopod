#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    SIM_DEFAULT_SCALE = 3,
    SIM_MAX_SCALE = 8,
    SIM_DEFAULT_BATTERY_PERCENT = 80,
    SIM_FULL_BATTERY_PERCENT = 100
};

typedef struct SimOptions {
    int scale;
    uint32_t frame_limit;
    bool headless;
    const char *screenshot_path;
    const char *key_script;
    const char *card_directory;
    const char *system_directory;
    uint8_t battery_percent;
    bool battery_charging;
} SimOptions;

bool sim_options_parse(int argument_count, char **arguments, SimOptions *options);
