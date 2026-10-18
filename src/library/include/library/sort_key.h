#pragma once

#include <stddef.h>

enum {
    SORT_KEY_CAPACITY = 1024
};

size_t sort_key_make(const char *text, char *key, size_t capacity);
int sort_key_compare(const char *left, const char *right);
size_t case_fold_make(const char *text, size_t length, char *folded, size_t capacity);
