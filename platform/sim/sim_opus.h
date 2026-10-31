#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "codec/ogg.h"

enum {
    SIM_OPUS_MAX_FRAME = 5760,
    SIM_OPUS_PACKET_BYTES = 8192
};

typedef struct SimOpus {
    FILE *file;
    uint32_t file_size;
    OggOpusInfo info;
    OggReader reader;
    void *decoder;
    uint8_t packet[SIM_OPUS_PACKET_BYTES];
    int16_t output[SIM_OPUS_MAX_FRAME * 2];
    uint32_t frame_first;
    uint32_t buffered;
    uint32_t used;
    uint32_t target;
} SimOpus;

bool sim_opus_open(SimOpus *opus, const char *path);
uint64_t sim_opus_read(SimOpus *opus, float *frames, uint64_t count, uint32_t channels);
bool sim_opus_seek(SimOpus *opus, uint64_t frame);
void sim_opus_close(SimOpus *opus);
uint32_t sim_opus_playable(const SimOpus *opus);
