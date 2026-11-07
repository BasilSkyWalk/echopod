#include <stdint.h>

#include "core1_region.h"
#include "core1_ticks.h"
#include "device/core1_control.h"
#include "device/core1_mp3.h"
#include "registers.h"

void core1_main(void);

static Mp3Decoder decoder;
static Core1Mp3 mp3;
static Core1Stream stream;
static Equalizer equalizer;

void core1_main(void)
{
    register_write(CORE1_SHARED_READY, CORE1_READY_MAGIC);
    __asm__ volatile("dsb" ::: "memory");
    register_write(CORE1_MAILBOX_B2A_COMMAND, CORE1_READY_COMMAND);
    register_write(CORE1_MAILBOX_B2A_DATA, 0);
    if (register_read(CORE1_SHARED_COMMAND) != CORE1_COMMAND_STREAM) {
        return;
    }
    core1_stream_init(&stream, core1_region(), core1_mp3_decoder(&mp3, &decoder));
    core1_stream_set_equalizer(&stream, &equalizer,
                               (volatile Core1EqualizerShare *)(uintptr_t)CORE1_SHARED_EQUALIZER);
    core1_ticks_start();
    core1_ticks_measure(&stream);
    for (;;) {
        core1_stream_serve(&stream);
    }
}
