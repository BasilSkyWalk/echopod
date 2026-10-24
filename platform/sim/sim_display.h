#pragma once

#include <stdbool.h>

bool sim_display_open(int scale);
void sim_display_close(void);
void sim_display_set_title(const char *title);
bool sim_display_save_bmp(const char *path);
