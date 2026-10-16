#include "device/core1_stream.h"

#include <stddef.h>

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

void core1_stream_init(Core1Stream *stream, StreamRegion region, FlacDecoder *decoder)
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
    stream->slot = 0;
    stream->finished = false;
    stream->starved = false;
    FlacInput input = {
        .read = read_ring, .context = stream, .size = stream->end_offset - stream->start_offset};
    flac_start(stream->decoder, input, info);
    control->released = stream->start_offset;
    publish();
    control->acked_generation = generation;
}

static void copy_channels(Core1Stream *stream, uint32_t slot)
{
    const FlacDecoder *decoder = stream->decoder;
    for (uint32_t channel = 0; channel < decoder->info.channels; channel++) {
        const int32_t *from = flac_channel(decoder, (int)channel);
        int32_t *to = stream->region.samples[slot][channel];
        for (uint32_t index = 0; index < decoder->block_samples; index++) {
            to[index] = from[index];
        }
    }
}

static void fill_slot(Core1Stream *stream, FlacResult result)
{
    volatile StreamControl *control = stream->region.control;
    volatile StreamSlot *slot = &control->slots[stream->slot];
    uint32_t outcome = AUDIO_FRAME_ERROR;
    if (result == FLAC_OK) {
        copy_channels(stream, stream->slot);
        slot->first_sample = stream->decoder->frame_first_sample;
        slot->samples = stream->decoder->block_samples;
        control->released = stream->start_offset + flac_position(stream->decoder);
        outcome = AUDIO_FRAME_READY;
    } else {
        slot->first_sample = 0;
        slot->samples = 0;
        outcome = result == FLAC_END ? AUDIO_FRAME_END : AUDIO_FRAME_ERROR;
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
    FlacDecoder *decoder = stream->decoder;
    uint32_t frame_start = flac_position(decoder);
    stream->starved = false;
    FlacResult result = flac_decode_frame(decoder);
    if (control->generation != stream->generation) {
        return true;
    }
    if (stream->starved) {
        flac_rewind(decoder, frame_start);
        stream->starved = false;
        return false;
    }
    fill_slot(stream, result);
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
}

static uint32_t next_generation(Core1Source *source)
{
    source->generation++;
    if (source->generation == 0) {
        source->generation = 1;
    }
    return source->generation;
}

static bool open_stream(void *context, const char *path, uint32_t start_ms, FlacInfo *info)
{
    Core1Source *source = context;
    stop_streaming(source);
    if (!flac_source_open_at(&source->flac, path, start_ms, info)) {
        return false;
    }
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
