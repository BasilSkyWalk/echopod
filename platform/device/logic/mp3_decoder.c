#include "device/mp3_decoder.h"

#include <stddef.h>

enum {
    FRACTION_BITS = 28,
    ROUNDING = 1 << (FRACTION_BITS - MP3_OUTPUT_BITS),
    OUTPUT_SHIFT = FRACTION_BITS + 1 - MP3_OUTPUT_BITS,
    LIMIT = 1 << FRACTION_BITS
};

static void clear_overlap(Mp3Decoder *decoder)
{
    mad_fixed_t *cells = &decoder->overlap[0][0][0];
    for (size_t index = 0; index < sizeof decoder->overlap / sizeof cells[0]; index++) {
        cells[index] = 0;
    }
}

void mp3_decoder_start(Mp3Decoder *decoder, FlacInput source)
{
    mad_stream_init(&decoder->stream);
    decoder->stream.main_data = &decoder->main_data;
    mad_frame_init(&decoder->frame);
    decoder->frame.overlap = &decoder->overlap;
    clear_overlap(decoder);
    mad_synth_init(&decoder->synth);
    decoder->source = source;
    decoder->buffer_start = 0;
    decoder->buffered = 0;
    decoder->refill = true;
    decoder->guarded = false;
    mad_stream_buffer(&decoder->stream, decoder->input, 0);
}

static uint32_t kept_bytes(const Mp3Decoder *decoder)
{
    const unsigned char *next = decoder->stream.next_frame;
    if (next == NULL) {
        return decoder->buffered;
    }
    return (uint32_t)(decoder->stream.bufend - next);
}

static Mp3Result refill(Mp3Decoder *decoder)
{
    if (decoder->guarded) {
        return MP3_FINISHED;
    }
    uint32_t keep = kept_bytes(decoder);
    uint32_t consumed = decoder->buffered - keep;
    for (uint32_t index = 0; index < keep; index++) {
        decoder->input[index] = decoder->input[consumed + index];
    }
    decoder->buffer_start += consumed;
    decoder->buffered = keep;
    mad_stream_buffer(&decoder->stream, decoder->input, keep);
    uint32_t at = decoder->buffer_start + keep;
    uint32_t left = decoder->source.size - at;
    uint32_t want = MP3_INPUT_BYTES - keep < left ? MP3_INPUT_BYTES - keep : left;
    if (want > 0 &&
        !decoder->source.read(decoder->source.context, at, decoder->input + keep, want)) {
        return MP3_STARVED;
    }
    decoder->buffered = keep + want;
    if (at + want == decoder->source.size) {
        for (uint32_t index = 0; index < MAD_BUFFER_GUARD; index++) {
            decoder->input[decoder->buffered + index] = 0;
        }
        decoder->buffered += MAD_BUFFER_GUARD;
        decoder->guarded = true;
    }
    mad_stream_buffer(&decoder->stream, decoder->input, decoder->buffered);
    decoder->refill = false;
    return MP3_DECODED;
}

static int32_t to_output(mad_fixed_t sample)
{
    int64_t value = (int64_t)sample + ROUNDING;
    if (value >= LIMIT) {
        value = LIMIT - 1;
    } else if (value < -LIMIT) {
        value = -LIMIT;
    }
    return (int32_t)(value >> OUTPUT_SHIFT);
}

static void convert(Mp3Decoder *decoder)
{
    struct mad_pcm *pcm = &decoder->synth.pcm;
    for (unsigned channel = 0; channel < pcm->channels; channel++) {
        mad_fixed_t *samples = pcm->samples[channel];
        for (unsigned index = 0; index < pcm->length; index++) {
            samples[index] = to_output(samples[index]);
        }
    }
}

Mp3Result mp3_decode(Mp3Decoder *decoder)
{
    for (;;) {
        if (decoder->refill) {
            Mp3Result filled = refill(decoder);
            if (filled != MP3_DECODED) {
                return filled;
            }
        }
        if (mad_frame_decode(&decoder->frame, &decoder->stream) == -1) {
            if (decoder->stream.error == MAD_ERROR_BUFLEN) {
                decoder->refill = true;
                continue;
            }
            if (MAD_RECOVERABLE(decoder->stream.error)) {
                continue;
            }
            return MP3_FAILED;
        }
        mad_synth_frame(&decoder->synth, &decoder->frame);
        convert(decoder);
        return MP3_DECODED;
    }
}

uint32_t mp3_decoder_position(const Mp3Decoder *decoder)
{
    uint32_t position = decoder->buffer_start + decoder->buffered - kept_bytes(decoder);
    return position < decoder->source.size ? position : decoder->source.size;
}

uint32_t mp3_decoder_samples(const Mp3Decoder *decoder)
{
    return decoder->synth.pcm.length;
}

uint8_t mp3_decoder_channels(const Mp3Decoder *decoder)
{
    return (uint8_t)decoder->synth.pcm.channels;
}

const int32_t *mp3_decoder_channel(const Mp3Decoder *decoder, int channel)
{
    return (const int32_t *)decoder->synth.pcm.samples[channel];
}
