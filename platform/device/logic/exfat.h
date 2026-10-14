#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "device/fat.h"

bool exfat_find_entry(FatVolume *volume, FatFile *directory, const char *name, size_t length,
                      FatFile *found);
