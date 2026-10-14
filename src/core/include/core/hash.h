#pragma once

#include <stddef.h>
#include <stdint.h>

#define HASH_START UINT32_C(2166136261)

uint32_t hash_update(uint32_t hash, const void *bytes, size_t length);
