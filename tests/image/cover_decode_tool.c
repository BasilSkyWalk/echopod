#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/cover_image.h"

enum {
    SCRATCH_BYTES = 384 * 1024,
    RESULT_EXIT_BASE = 10
};

typedef struct Memory {
    uint8_t *bytes;
    uint32_t size;
} Memory;

static bool read_memory(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    const Memory *memory = context;
    if (offset > memory->size || length > memory->size - offset) {
        return false;
    }
    memcpy(buffer, memory->bytes + offset, length);
    return true;
}

static bool load(const char *path, Memory *memory)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    memory->bytes = malloc((size_t)size + 1);
    memory->size = (uint32_t)size;
    bool read =
        memory->bytes != NULL && fread(memory->bytes, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    return read;
}

static CoverImageResult decode(Memory *memory, uint16_t *full, uint16_t *half)
{
    static uint8_t scratch[SCRATCH_BYTES];
    ImageSource source = {read_memory, memory, 0, memory->size};
    return cover_image_decode(source, scratch, sizeof scratch, full, half);
}

static bool write_record(const char *path, const uint16_t *full, const uint16_t *half)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    bool written = fwrite(full, 2, COVER_GRID_CELLS, file) == COVER_GRID_CELLS &&
                   fwrite(half, 2, COVER_GRID_HALF_CELLS, file) == COVER_GRID_HALF_CELLS;
    return fclose(file) == 0 && written;
}

static uint32_t next_random(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

static void mutate(Memory *target, const Memory *original, uint32_t *state)
{
    memcpy(target->bytes, original->bytes, original->size);
    target->size = original->size;
    uint32_t kind = next_random(state) % 3;
    if (kind == 0) {
        target->size = next_random(state) % (original->size + 1);
        return;
    }
    uint32_t changes = 1 + next_random(state) % (kind == 1 ? 4 : 64);
    for (uint32_t change = 0; change < changes && original->size > 0; change++) {
        target->bytes[next_random(state) % original->size] = (uint8_t)next_random(state);
    }
}

static int fuzz(const char *path, uint32_t rounds, uint32_t seed)
{
    static uint16_t full[COVER_GRID_CELLS];
    static uint16_t half[COVER_GRID_HALF_CELLS];
    Memory original;
    if (!load(path, &original)) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    Memory mutated = {malloc(original.size + 1), original.size};
    uint32_t state = seed;
    uint32_t decoded = 0;
    for (uint32_t round = 0; round < rounds; round++) {
        mutate(&mutated, &original, &state);
        decoded += decode(&mutated, full, half) == COVER_IMAGE_OK;
    }
    printf("%s: %u of %u mutations still decoded\n", path, decoded, rounds);
    free(mutated.bytes);
    free(original.bytes);
    return 0;
}

int main(int argc, char **argv)
{
    static uint16_t full[COVER_GRID_CELLS];
    static uint16_t half[COVER_GRID_HALF_CELLS];
    if (argc == 5 && strcmp(argv[1], "--fuzz") == 0) {
        return fuzz(argv[2], (uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]));
    }
    Memory memory;
    if (argc != 3 || !load(argv[1], &memory)) {
        fprintf(stderr, "usage: cover_decode_tool IMAGE RECORD | --fuzz IMAGE ROUNDS SEED\n");
        return 1;
    }
    CoverImageResult result = decode(&memory, full, half);
    free(memory.bytes);
    if (result != COVER_IMAGE_OK) {
        return RESULT_EXIT_BASE + (int)result;
    }
    return write_record(argv[2], full, half) ? 0 : 1;
}
