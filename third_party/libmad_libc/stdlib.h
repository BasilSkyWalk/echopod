#pragma once

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *pointer);
void *realloc(void *pointer, size_t size);
void abort(void);
