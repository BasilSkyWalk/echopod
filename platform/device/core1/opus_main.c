#include <stddef.h>
#include <stdint.h>

#include "core1_region.h"
#include "core1_ticks.h"
#include "device/core1_control.h"
#include "device/core1_opus.h"
#include "registers.h"

void core1_main(void);

static uint8_t packet[CORE1_OPUS_PACKET_BYTES] __attribute__((section(".core1_big"), aligned(4)));
static int16_t output[CORE1_OPUS_MAX_FRAME * FLAC_MAX_CHANNELS]
    __attribute__((section(".core1_big"), aligned(4)));
static Core1Opus opus;
static Core1Stream stream;

void core1_main(void)
{
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    Core1OpusMemory memory = {(void *)(uintptr_t)CORE1_OPUS_STATE, CORE1_OPUS_STATE_BYTES, packet,
                              output};
    Core1Decoder decoder = core1_opus_decoder(&opus, memory);
    core1_stream_init(&stream, core1_region(), decoder);
    core1_ticks_start();
    core1_ticks_measure(&stream);
    core1_stream_set_decoder(&stream, STREAM_CODEC_OPUS, decoder);
    for (;;) {
        core1_stream_step(&stream);
    }
}
