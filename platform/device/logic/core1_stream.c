#include "device/core1_stream.h"

#include <stddef.h>

#include "device/mp3_probe.h"

enum {
    MP3_OUTPUT_BITS = 24,
    MILLISECONDS_PER_SECOND = 1000
};

static void publish(void)
{
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

static void observe(void)
{
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
}

static uint32_t smaller(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static void start_flac(void *context, FlacInput input, FlacInfo info, uint32_t first_sample,
                       const bool *starved)
{
    Core1Flac *flac = context;
    (void)first_sample;
    flac->starved = starved;
    flac_start(flac->decoder, input, info);
}

static Core1DecodeResult decode_flac(void *context, Core1Block *block)
{
    Core1Flac *flac = context;
    FlacDecoder *decoder = flac->decoder;
    uint32_t frame_start = flac_position(decoder);
    FlacResult result = flac_decode_frame(decoder);
    if (*flac->starved) {
        flac_rewind(decoder, frame_start);
        return CORE1_STARVED;
    }
    if (result == FLAC_END) {
        return CORE1_FINISHED;
    }
    if (result != FLAC_OK) {
        return CORE1_FAILED;
    }
    for (int channel = 0; channel < FLAC_MAX_CHANNELS; channel++) {
        block->channels[channel] =
            flac_channel(decoder, channel < decoder->info.channels ? channel : 0);
    }
    block->first_sample = decoder->frame_first_sample;
    block->samples = decoder->block_samples;
    block->position = flac_position(decoder);
    return CORE1_DECODED;
}

Core1Decoder core1_flac_decoder(Core1Flac *flac, FlacDecoder *decoder)
{
    *flac = (Core1Flac){.decoder = decoder, .starved = NULL};
    return (Core1Decoder){start_flac, decode_flac, flac};
}

void core1_stream_init(Core1Stream *stream, StreamRegion region, Core1Decoder decoder)
{
    *stream = (Core1Stream){.region = region, .decoder = decoder};
}

static void copy_from_ring(const StreamRegion *region, uint32_t position, uint8_t *out,
                           uint32_t length)
{
    uint32_t first = position % region->ring_bytes;
    uint32_t part = smaller(length, region->ring_bytes - first);
    for (uint32_t index = 0; index < part; index++) {
        out[index] = region->ring[first + index];
    }
    for (uint32_t index = part; index < length; index++) {
        out[index] = region->ring[index - part];
    }
}

static bool read_ring(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    Core1Stream *stream = context;
    volatile StreamControl *control = stream->region.control;
    if (control->generation != stream->generation) {
        return false;
    }
    uint32_t position = stream->start_offset + offset;
    if (position + length > control->written) {
        stream->starved = true;
        return false;
    }
    observe();
    copy_from_ring(&stream->region, position, buffer, length);
    return true;
}

static void adopt(Core1Stream *stream, uint32_t generation)
{
    volatile StreamControl *control = stream->region.control;
    observe();
    FlacInfo info = {
        .sample_rate = control->sample_rate,
        .channels = (uint8_t)control->channels,
        .bits_per_sample = (uint8_t)control->bits_per_sample,
        .min_block = (uint16_t)control->min_block,
        .max_block = (uint16_t)control->max_block,
        .total_samples = control->total_samples,
    };
    stream->generation = generation;
    stream->start_offset = control->start_offset;
    stream->end_offset = control->end_offset;
    stream->channels =
        control->channels < FLAC_MAX_CHANNELS ? control->channels : FLAC_MAX_CHANNELS;
    stream->slot = 0;
    stream->finished = false;
    stream->starved = false;
    FlacInput input = {
        .read = read_ring, .context = stream, .size = stream->end_offset - stream->start_offset};
    stream->decoder.start(stream->decoder.context, input, info, control->first_sample,
                          &stream->starved);
    control->released = stream->start_offset;
    publish();
    control->acked_generation = generation;
}

static void copy_channels(Core1Stream *stream, uint32_t slot, const Core1Block *block)
{
    for (uint32_t channel = 0; channel < stream->channels; channel++) {
        const int32_t *from = block->channels[channel];
        int32_t *to = stream->region.samples[slot][channel];
        for (uint32_t index = 0; index < block->samples; index++) {
            to[index] = from[index];
        }
    }
}

static void fill_slot(Core1Stream *stream, Core1DecodeResult result, const Core1Block *block)
{
    volatile StreamControl *control = stream->region.control;
    volatile StreamSlot *slot = &control->slots[stream->slot];
    uint32_t outcome = AUDIO_FRAME_ERROR;
    if (result == CORE1_DECODED) {
        copy_channels(stream, stream->slot, block);
        slot->first_sample = block->first_sample;
        slot->samples = block->samples;
        control->released = stream->start_offset + block->position;
        outcome = AUDIO_FRAME_READY;
    } else {
        slot->first_sample = 0;
        slot->samples = 0;
        outcome = result == CORE1_FINISHED ? AUDIO_FRAME_END : AUDIO_FRAME_ERROR;
        stream->finished = true;
    }
    slot->generation = stream->generation;
    slot->result = outcome;
    publish();
    slot->state = STREAM_SLOT_FULL;
    stream->slot = (stream->slot + 1) % STREAM_SLOTS;
}

bool core1_stream_step(Core1Stream *stream)
{
    volatile StreamControl *control = stream->region.control;
    uint32_t generation = control->generation;
    if (generation != stream->generation) {
        if (generation == 0) {
            stream->generation = 0;
            return false;
        }
        adopt(stream, generation);
        return true;
    }
    if (generation == 0 || stream->finished) {
        return false;
    }
    if (control->slots[stream->slot].state != STREAM_SLOT_EMPTY) {
        return false;
    }
    stream->starved = false;
    Core1Block block = {0};
    Core1DecodeResult result = stream->decoder.decode(stream->decoder.context, &block);
    if (control->generation != stream->generation) {
        return true;
    }
    if (result == CORE1_STARVED) {
        stream->starved = false;
        return false;
    }
    fill_slot(stream, result, &block);
    return true;
}

void core1_source_init(Core1Source *source, StreamRegion region, FlacFileOpener files,
                       FlacDecoder *decoder, uint32_t fill_budget)
{
    *source = (Core1Source){.region = region, .fill_budget = fill_budget};
    flac_source_init(&source->flac, files, decoder);
}

static void stop_streaming(Core1Source *source)
{
    if (source->streaming) {
        source->region.control->generation = 0;
        source->streaming = false;
    }
    flac_source_close(&source->flac);
    if (source->file_open) {
        source->flac.files.close(source->flac.files.context);
        source->file_open = false;
    }
}

static uint32_t next_generation(Core1Source *source)
{
    source->generation++;
    if (source->generation == 0) {
        source->generation = 1;
    }
    return source->generation;
}

static bool open_stream(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track)
{
    Core1Source *source = context;
    stop_streaming(source);
    if (!flac_source_open_at(&source->flac, path, start_ms, track)) {
        return false;
    }
    const FlacInfo *info = &track->stream;
    FlacDecoder *decoder = source->flac.decoder;
    source->input = decoder->bits.input;
    source->start_offset = flac_position(decoder);
    source->end_offset = source->input.size;
    source->written = source->start_offset;
    source->slot = 0;
    source->read_failed = false;
    source->bits_per_sample = info->bits_per_sample;
    source->channels = info->channels;
    volatile StreamControl *control = source->region.control;
    control->sample_rate = info->sample_rate;
    control->channels = info->channels;
    control->bits_per_sample = info->bits_per_sample;
    control->min_block = info->min_block;
    control->max_block = info->max_block;
    control->total_samples = info->total_samples;
    control->first_sample = 0;
    control->start_offset = source->start_offset;
    control->end_offset = source->end_offset;
    control->written = source->written;
    publish();
    control->generation = next_generation(source);
    source->streaming = true;
    return true;
}

void core1_source_fill(Core1Source *source)
{
    if (!source->streaming || source->read_failed) {
        return;
    }
    volatile StreamControl *control = source->region.control;
    uint32_t released =
        control->acked_generation == source->generation ? control->released : source->start_offset;
    observe();
    uint32_t target = smaller(released + source->region.ring_bytes, source->end_offset);
    uint32_t budget = source->fill_budget;
    while (source->written < target && budget > 0) {
        uint32_t position = source->written % source->region.ring_bytes;
        uint32_t length = smaller(smaller(target - source->written, budget),
                                  source->region.ring_bytes - position);
        void *destination = (void *)(uintptr_t)&source->region.ring[position];
        if (!source->input.read(source->input.context, source->written, destination, length)) {
            source->read_failed = true;
            return;
        }
        source->written += length;
        budget -= length;
        publish();
        control->written = source->written;
    }
}

static void drop_stale_slots(Core1Source *source)
{
    volatile StreamControl *control = source->region.control;
    for (uint32_t index = 0; index < STREAM_SLOTS; index++) {
        volatile StreamSlot *slot = &control->slots[index];
        if (slot->state == STREAM_SLOT_FULL && slot->generation != source->generation) {
            slot->state = STREAM_SLOT_EMPTY;
        }
    }
}

static AudioFrameResult next_frame(void *context, AudioFrame *frame)
{
    Core1Source *source = context;
    core1_source_fill(source);
    if (!source->streaming || source->read_failed) {
        return AUDIO_FRAME_ERROR;
    }
    drop_stale_slots(source);
    volatile StreamSlot *slot = &source->region.control->slots[source->slot];
    if (slot->state != STREAM_SLOT_FULL) {
        return AUDIO_FRAME_WAIT;
    }
    observe();
    if (slot->result != AUDIO_FRAME_READY) {
        return slot->result == AUDIO_FRAME_END ? AUDIO_FRAME_END : AUDIO_FRAME_ERROR;
    }
    int32_t (*channels)[FLAC_MAX_BLOCK] = source->region.samples[source->slot];
    *frame = (AudioFrame){
        .left = channels[0],
        .right = channels[source->channels > 1 ? 1 : 0],
        .first_sample = slot->first_sample,
        .samples = slot->samples,
        .bits = source->bits_per_sample,
    };
    return AUDIO_FRAME_READY;
}

static void release_frame(void *context)
{
    Core1Source *source = context;
    source->region.control->slots[source->slot].state = STREAM_SLOT_EMPTY;
    source->slot = (source->slot + 1) % STREAM_SLOTS;
}

static void close_stream(void *context)
{
    stop_streaming(context);
}

AudioSource core1_source_interface(Core1Source *source)
{
    return (AudioSource){open_stream, next_frame, release_frame, close_stream, source};
}

static bool open_mp3(void *context, const char *path, uint32_t start_ms, AudioTrackInfo *track)
{
    Core1Source *source = context;
    stop_streaming(source);
    FlacFileOpener files = source->flac.files;
    if (!files.open(files.context, path, &source->input)) {
        return false;
    }
    source->file_open = true;
    Mp3Stream mp3;
    if (!mp3_probe(source->input, &mp3)) {
        stop_streaming(source);
        return false;
    }
    uint32_t target = (uint32_t)((uint64_t)start_ms * mp3.sample_rate / MILLISECONDS_PER_SECOND);
    target = target < mp3.total_samples ? target : mp3.total_samples;
    source->start_offset = mp3_seek_offset(&mp3, target);
    source->end_offset = mp3.audio_end;
    source->written = source->start_offset;
    source->slot = 0;
    source->read_failed = false;
    source->bits_per_sample = MP3_OUTPUT_BITS;
    source->channels = mp3.channels;
    *track = (AudioTrackInfo){.stream = {.sample_rate = mp3.sample_rate,
                                         .channels = mp3.channels,
                                         .bits_per_sample = 0,
                                         .min_block = mp3.samples_per_frame,
                                         .max_block = mp3.samples_per_frame,
                                         .total_samples = mp3.total_samples},
                              .audio_bytes = mp3.audio_end - mp3.audio_start,
                              .codec = AUDIO_CODEC_MP3};
    volatile StreamControl *control = source->region.control;
    control->sample_rate = mp3.sample_rate;
    control->channels = mp3.channels;
    control->bits_per_sample = MP3_OUTPUT_BITS;
    control->min_block = mp3.samples_per_frame;
    control->max_block = mp3.samples_per_frame;
    control->total_samples = mp3.total_samples;
    control->first_sample = target;
    control->start_offset = source->start_offset;
    control->end_offset = source->end_offset;
    control->written = source->written;
    publish();
    control->generation = next_generation(source);
    source->streaming = true;
    return true;
}

AudioSource core1_source_mp3_interface(Core1Source *source)
{
    return (AudioSource){open_mp3, next_frame, release_frame, close_stream, source};
}
