#include <stddef.h>
#include <stdint.h>

#include "core1_region.h"
#include "core1_ticks.h"
#include "device/core1_aac.h"
#include "device/core1_control.h"
#include "registers.h"

enum {
    HEAP_ALIGNMENT = 8,
    SLOT_ROWS = STREAM_SLOTS * FLAC_MAX_CHANNELS,
    ROW_BYTES = FLAC_MAX_BLOCK * sizeof(int32_t),
    USED_ROW_BYTES = CORE1_AAC_FRAME_SAMPLES * sizeof(int32_t),
    WINDOW_BYTES = ROW_BYTES - USED_ROW_BYTES
};

_Static_assert(USED_ROW_BYTES % HEAP_ALIGNMENT == 0 && ROW_BYTES % HEAP_ALIGNMENT == 0,
               "heap windows must stay aligned");

void core1_main(void);
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *pointer);

static size_t window_used[SLOT_ROWS];
static Core1Aac aac;
static Core1Stream stream;

static uint8_t *window_start(uint32_t row)
{
    return (uint8_t *)(uintptr_t)(CORE1_STREAM_SAMPLES + row * ROW_BYTES + USED_ROW_BYTES);
}

void *malloc(size_t size)
{
    size_t rounded = (size + HEAP_ALIGNMENT - 1) & ~(size_t)(HEAP_ALIGNMENT - 1);
    for (uint32_t row = 0; rounded >= size && row < SLOT_ROWS; row++) {
        if (rounded <= WINDOW_BYTES - window_used[row]) {
            void *block = window_start(row) + window_used[row];
            window_used[row] += rounded;
            return block;
        }
    }
    return NULL;
}

void *calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) {
        return NULL;
    }
    uint8_t *block = malloc(count * size);
    for (size_t index = 0; block != NULL && index < count * size; index++) {
        block[index] = 0;
    }
    return block;
}

void free(void *pointer)
{
    (void)pointer;
}

static void reset_heap(void)
{
    for (uint32_t row = 0; row < SLOT_ROWS; row++) {
        window_used[row] = 0;
    }
}

void core1_main(void)
{
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    Core1Decoder decoder = core1_aac_decoder(&aac, reset_heap);
    core1_stream_init(&stream, core1_region(), decoder);
    core1_ticks_start();
    core1_ticks_measure(&stream);
    core1_stream_set_decoder(&stream, STREAM_CODEC_AAC, decoder);
    for (;;) {
        core1_stream_step(&stream);
    }
}
