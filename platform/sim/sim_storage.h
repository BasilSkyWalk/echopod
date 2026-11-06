#pragma once

#include <stdbool.h>
#include <stddef.h>

void sim_storage_configure(const char *card_directory, const char *system_directory);
void sim_storage_configure_internal(const char *internal_directory);
bool sim_storage_host_path(const char *path, char *buffer, size_t capacity);
void sim_storage_close_all(void);
