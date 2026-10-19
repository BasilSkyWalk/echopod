#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    FINGERPRINT_NAME_CAPACITY = 768,
    FINGERPRINT_PATH_CAPACITY = 1024,
    FINGERPRINT_MAX_DEPTH = 32
};

typedef struct FingerprintEntry {
    bool directory;
    uint32_t size;
    uint32_t modified;
    char name[FINGERPRINT_NAME_CAPACITY];
} FingerprintEntry;

typedef struct FingerprintFiles {
    bool (*open_dir)(void *context, const char *path);
    bool (*next_entry)(void *context, FingerprintEntry *entry);
    void (*close_dir)(void *context);
    void *context;
} FingerprintFiles;

typedef struct MusicFingerprint {
    bool complete;
    uint32_t first;
    uint32_t second;
    uint32_t entries;
} MusicFingerprint;

typedef struct FingerprintLevel {
    uint32_t path_length;
    uint32_t next_index;
} FingerprintLevel;

typedef struct FingerprintWork {
    char path[FINGERPRINT_PATH_CAPACITY];
    FingerprintLevel levels[FINGERPRINT_MAX_DEPTH];
    FingerprintEntry entry;
} FingerprintWork;

MusicFingerprint music_fingerprint(const FingerprintFiles *files, FingerprintWork *work);
bool music_fingerprints_match(MusicFingerprint left, MusicFingerprint right);
