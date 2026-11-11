#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    PAGE_BYTES = 65536,
    ALIGNMENT = 8
};

extern unsigned char __heap_base;

static uintptr_t heap_top;

void *memcpy(void *restrict target, const void *restrict source, size_t length)
{
    unsigned char *to = target;
    const unsigned char *from = source;
    for (size_t index = 0; index < length; index++) {
        to[index] = from[index];
    }
    return target;
}

void *memmove(void *target, const void *source, size_t length)
{
    unsigned char *to = target;
    const unsigned char *from = source;
    if (to < from) {
        return memcpy(target, source, length);
    }
    for (size_t index = length; index > 0; index--) {
        to[index - 1] = from[index - 1];
    }
    return target;
}

void *memset(void *target, int value, size_t length)
{
    unsigned char *to = target;
    for (size_t index = 0; index < length; index++) {
        to[index] = (unsigned char)value;
    }
    return target;
}

int memcmp(const void *first, const void *second, size_t length)
{
    const unsigned char *one = first;
    const unsigned char *other = second;
    for (size_t index = 0; index < length; index++) {
        if (one[index] != other[index]) {
            return one[index] < other[index] ? -1 : 1;
        }
    }
    return 0;
}

int strcmp(const char *first, const char *second)
{
    size_t index = 0;
    while (first[index] != '\0' && first[index] == second[index]) {
        index++;
    }
    return (unsigned char)first[index] - (unsigned char)second[index];
}

char *strncpy(char *restrict target, const char *restrict source, size_t length)
{
    size_t index = 0;
    for (; index < length && source[index] != '\0'; index++) {
        target[index] = source[index];
    }
    for (; index < length; index++) {
        target[index] = '\0';
    }
    return target;
}

size_t strlen(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static bool reach(uintptr_t end)
{
    uintptr_t have = (uintptr_t)__builtin_wasm_memory_size(0) * PAGE_BYTES;
    if (end <= have) {
        return true;
    }
    uintptr_t pages = (end - have + PAGE_BYTES - 1) / PAGE_BYTES;
    return __builtin_wasm_memory_grow(0, pages) != (size_t)-1;
}

void *malloc(size_t size)
{
    if (heap_top == 0) {
        heap_top = (uintptr_t)&__heap_base;
    }
    uintptr_t start = (heap_top + ALIGNMENT - 1) & ~(uintptr_t)(ALIGNMENT - 1);
    if (!reach(start + size)) {
        return NULL;
    }
    heap_top = start + size;
    return (void *)start;
}

void free(void *block)
{
    (void)block;
}
