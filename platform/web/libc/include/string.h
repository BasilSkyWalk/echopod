#pragma once

#include <stddef.h>

void *memcpy(void *restrict target, const void *restrict source, size_t length);
void *memmove(void *target, const void *source, size_t length);
void *memset(void *target, int value, size_t length);
int memcmp(const void *first, const void *second, size_t length);
int strcmp(const char *first, const char *second);
char *strncpy(char *restrict target, const char *restrict source, size_t length);
size_t strlen(const char *text);
