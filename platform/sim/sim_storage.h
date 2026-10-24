#pragma once

#include <stdbool.h>
#include <stddef.h>

void sim_storage_configure(const char *card_directory, const char *system_directory);
bool sim_storage_host_path(const char *path, char *buffer, size_t capacity);
void sim_storage_close_all(void);
