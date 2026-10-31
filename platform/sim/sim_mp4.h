#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "codec/alac.h"
#include "codec/mp4.h"

typedef struct SimMp4 {
    FILE *file;
    uint32_t file_size;
    Mp4Track track;
    Mp4Stream stream;
    FlacInput records;
    AlacDecoder *decoder;
    void *aac;
    const int32_t *interleaved;
    uint8_t *frame;
    uint32_t frame_capacity;
    uint32_t position;
    uint32_t frame_first;
    uint32_t buffered;
    uint32_t used;
    uint32_t target;
    uint32_t next_sample;
} SimMp4;

bool sim_mp4_open(SimMp4 *mp4, const char *path);
uint64_t sim_mp4_read(SimMp4 *mp4, float *frames, uint64_t count, uint32_t channels);
bool sim_mp4_seek(SimMp4 *mp4, uint64_t frame);
void sim_mp4_close(SimMp4 *mp4);
uint32_t sim_mp4_playable(const SimMp4 *mp4);
