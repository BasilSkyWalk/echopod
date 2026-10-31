#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"
#include "hal/audio.h"
#include "sim_mp4.h"
#include "sim_opus.h"

typedef enum SimCodec {
    SIM_CODEC_NONE,
    SIM_CODEC_FLAC,
    SIM_CODEC_MP3,
    SIM_CODEC_WAV,
    SIM_CODEC_MP4,
    SIM_CODEC_OPUS
} SimCodec;

typedef struct SimDecoder {
    SimCodec codec;
    drflac *flac;
    drmp3 mp3;
    drwav wav;
    SimMp4 mp4;
    SimOpus opus;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t audio_bytes;
    uint64_t total_frames;
    uint64_t frame;
} SimDecoder;

bool sim_decoder_open(SimDecoder *decoder, const char *host_path);
uint64_t sim_decoder_read_stereo(SimDecoder *decoder, float *frames, uint64_t count);
bool sim_decoder_seek(SimDecoder *decoder, uint64_t frame);
void sim_decoder_close(SimDecoder *decoder);
HalAudioFormat sim_decoder_format(const SimDecoder *decoder);
