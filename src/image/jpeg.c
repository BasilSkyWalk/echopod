#include <stddef.h>

#include "jpeg_internal.h"

enum {
    MARKER_SOF0 = 0xC0,
    MARKER_SOF1 = 0xC1,
    MARKER_SOF2 = 0xC2,
    MARKER_DHT = 0xC4,
    MARKER_SOI = 0xD8,
    MARKER_EOI = 0xD9,
    MARKER_SOS = 0xDA,
    MARKER_DQT = 0xDB,
    MARKER_DRI = 0xDD,
    MARKER_APP14 = 0xEE,
    MARKER_RST0 = 0xD0,
    MARKER_RST7 = 0xD7,
    MARKER_TEM = 0x01,
    BYTE_FF = 0xFF,
    BLOCK_SIDE = 8,
    MAX_SAMPLING = 4,
    PRECISION = 8,
    MIN_OUTPUT_SIZE = COVER_GRID_SIZE,
    MAX_SHIFT = 3,
    HUFFMAN_COUNTS = 16,
    MAX_SCANS = 128,
    ADOBE_LENGTH = 12,
    END_OF_BAND = 15,
    ZERO_RUN = 16
};

typedef enum Step {
    STEP_CONTINUE,
    STEP_DONE,
    STEP_FAILED
} Step;

static uint32_t next_marker(ImageReader *reader)
{
    while (!reader->failed) {
        if (image_reader_byte(reader) != BYTE_FF) {
            continue;
        }
        uint8_t code = image_reader_byte(reader);
        while (code == BYTE_FF && !reader->failed) {
            code = image_reader_byte(reader);
        }
        if (code != 0x00) {
            return code;
        }
    }
    return 0;
}

static bool read_segment_length(ImageReader *reader, uint32_t *length)
{
    uint32_t value = image_reader_be16(reader);
    *length = value >= 2 ? value - 2 : 0;
    return !reader->failed && value >= 2;
}

static bool read_quant(JpegDecoder *decoder, uint32_t length)
{
    ImageReader *reader = decoder->reader;
    while (length > 0) {
        uint32_t info = image_reader_byte(reader);
        uint32_t wide = info >> 4;
        uint32_t table = info & 0x0F;
        uint32_t needed = 1 + JPEG_BLOCK * (wide ? 2 : 1);
        if (table >= JPEG_TABLES || wide > 1 || needed > length) {
            return false;
        }
        for (uint32_t index = 0; index < JPEG_BLOCK; index++) {
            uint32_t value = wide ? image_reader_be16(reader) : image_reader_byte(reader);
            decoder->quant[table][JPEG_ZIGZAG[index]] = (int32_t)value;
        }
        decoder->quant_present[table] = true;
        length -= needed;
    }
    return !reader->failed;
}

static bool read_one_huffman(JpegDecoder *decoder, uint32_t *length)
{
    ImageReader *reader = decoder->reader;
    uint8_t counts[HUFFMAN_COUNTS];
    uint8_t values[256];
    uint32_t info = image_reader_byte(reader);
    uint32_t total = 0;
    image_reader_read(reader, counts, HUFFMAN_COUNTS);
    for (uint32_t index = 0; index < HUFFMAN_COUNTS; index++) {
        total += counts[index];
    }
    uint32_t table = info & 0x0F;
    uint32_t kind = info >> 4;
    if (reader->failed || total > 256 || 17 + total > *length || table >= JPEG_TABLES || kind > 1) {
        return false;
    }
    image_reader_read(reader, values, total);
    *length -= 17 + total;
    JpegHuffman *target = kind == 0 ? &decoder->dc[table] : &decoder->ac[table];
    return !reader->failed && jpeg_build_huffman(target, counts, values, total);
}

static bool read_huffman(JpegDecoder *decoder, uint32_t length)
{
    while (length > 0) {
        if (!read_one_huffman(decoder, &length)) {
            return false;
        }
    }
    return true;
}

static bool read_adobe(JpegDecoder *decoder, uint32_t length)
{
    static const uint8_t ADOBE[5] = {'A', 'd', 'o', 'b', 'e'};
    uint8_t bytes[ADOBE_LENGTH];
    if (length < ADOBE_LENGTH) {
        return image_reader_skip(decoder->reader, length);
    }
    image_reader_read(decoder->reader, bytes, ADOBE_LENGTH);
    bool adobe = true;
    for (uint32_t index = 0; index < sizeof ADOBE; index++) {
        adobe = adobe && bytes[index] == ADOBE[index];
    }
    if (adobe) {
        decoder->adobe_rgb = bytes[11] == 0;
    }
    return image_reader_skip(decoder->reader, length - ADOBE_LENGTH);
}

static uint32_t ceiling(uint32_t value, uint32_t divisor)
{
    return (value + divisor - 1) / divisor;
}

static uint32_t choose_shift(uint32_t width, uint32_t height)
{
    uint32_t side = width < height ? width : height;
    uint32_t shift = MAX_SHIFT;
    while (shift > 0 && (side >> shift) < MIN_OUTPUT_SIZE) {
        shift--;
    }
    return shift;
}

static bool read_component(JpegDecoder *decoder, JpegComponent *component)
{
    ImageReader *reader = decoder->reader;
    component->id = image_reader_byte(reader);
    uint32_t sampling = image_reader_byte(reader);
    component->h = sampling >> 4;
    component->v = sampling & 0x0F;
    component->quant = image_reader_byte(reader);
    component->predictor = 0;
    return component->h >= 1 && component->h <= MAX_SAMPLING && component->v >= 1 &&
           component->v <= MAX_SAMPLING && component->quant < JPEG_TABLES;
}

static void size_components(JpegDecoder *decoder)
{
    decoder->mcus_wide = ceiling(decoder->width, BLOCK_SIDE * decoder->h_max);
    decoder->mcus_high = ceiling(decoder->height, BLOCK_SIDE * decoder->v_max);
    for (uint32_t index = 0; index < decoder->component_count; index++) {
        JpegComponent *component = &decoder->components[index];
        uint32_t width = ceiling(decoder->width * component->h, decoder->h_max);
        uint32_t height = ceiling(decoder->height * component->v, decoder->v_max);
        component->blocks_wide = ceiling(width, BLOCK_SIDE);
        component->blocks_high = ceiling(height, BLOCK_SIDE);
        uint32_t sample_width = (decoder->h_max / component->h) << decoder->shift;
        uint32_t sample_height = (decoder->v_max / component->v) << decoder->shift;
        cover_grid_set_sampling(decoder->grid, index, sample_width, sample_height);
    }
}

static bool sampling_divides(const JpegDecoder *decoder)
{
    for (uint32_t index = 0; index < decoder->component_count; index++) {
        const JpegComponent *component = &decoder->components[index];
        if (decoder->h_max % component->h != 0 || decoder->v_max % component->v != 0) {
            return false;
        }
    }
    return true;
}

static CoverImageResult read_frame(JpegDecoder *decoder, uint32_t length)
{
    ImageReader *reader = decoder->reader;
    uint32_t precision = image_reader_byte(reader);
    decoder->height = image_reader_be16(reader);
    decoder->width = image_reader_be16(reader);
    decoder->component_count = image_reader_byte(reader);
    uint32_t count = decoder->component_count;
    if (reader->failed || decoder->frame_seen || length != 6 + 3 * count) {
        return COVER_IMAGE_CORRUPT;
    }
    if (precision != PRECISION || (count != 1 && count != 3) || decoder->height == 0) {
        return COVER_IMAGE_UNSUPPORTED;
    }
    decoder->h_max = 1;
    decoder->v_max = 1;
    for (uint32_t index = 0; index < count; index++) {
        JpegComponent *component = &decoder->components[index];
        if (!read_component(decoder, component)) {
            return COVER_IMAGE_CORRUPT;
        }
        decoder->h_max = component->h > decoder->h_max ? component->h : decoder->h_max;
        decoder->v_max = component->v > decoder->v_max ? component->v : decoder->v_max;
    }
    if (!sampling_divides(decoder)) {
        return COVER_IMAGE_UNSUPPORTED;
    }
    if (!cover_grid_init(decoder->grid, decoder->width, decoder->height)) {
        return COVER_IMAGE_TOO_BIG;
    }
    decoder->shift = choose_shift(decoder->width, decoder->height);
    decoder->outputs = BLOCK_SIDE >> decoder->shift;
    jpeg_prepare_kernel(decoder);
    size_components(decoder);
    decoder->frame_seen = true;
    return reader->failed ? COVER_IMAGE_CORRUPT : COVER_IMAGE_OK;
}

static bool find_component(const JpegDecoder *decoder, uint32_t id, uint32_t *index)
{
    for (uint32_t component = 0; component < decoder->component_count; component++) {
        if (decoder->components[component].id == id) {
            *index = component;
            return true;
        }
    }
    return false;
}

static bool read_scan_header(JpegDecoder *decoder, uint32_t length, JpegScan *scan)
{
    ImageReader *reader = decoder->reader;
    scan->count = image_reader_byte(reader);
    if (scan->count < 1 || scan->count > decoder->component_count ||
        length != 4 + 2 * scan->count) {
        return false;
    }
    for (uint32_t index = 0; index < scan->count; index++) {
        uint32_t id = image_reader_byte(reader);
        uint32_t tables = image_reader_byte(reader);
        uint32_t member = 0;
        if (!find_component(decoder, id, &member) || (tables >> 4) >= JPEG_TABLES ||
            (tables & 0x0F) >= JPEG_TABLES) {
            return false;
        }
        scan->members[index] = member;
        decoder->components[member].dc_table = tables >> 4;
        decoder->components[member].ac_table = tables & 0x0F;
    }
    scan->start = image_reader_byte(reader);
    scan->end = image_reader_byte(reader);
    uint32_t approximation = image_reader_byte(reader);
    scan->high = approximation >> 4;
    scan->low = approximation & 0x0F;
    return !reader->failed;
}

static bool scan_valid(const JpegDecoder *decoder, const JpegScan *scan)
{
    if (!decoder->progressive) {
        return scan->start == 0 && scan->end == 63 && scan->high == 0 && scan->low == 0;
    }
    bool dc = scan->start == 0;
    return scan->end <= 63 && scan->start <= scan->end && (!dc || scan->end == 0) &&
           (dc || scan->count == 1) && scan->low <= 13;
}

static bool tables_present(const JpegDecoder *decoder, const JpegScan *scan)
{
    for (uint32_t index = 0; index < scan->count; index++) {
        const JpegComponent *component = &decoder->components[scan->members[index]];
        bool needs_dc = scan->start == 0 && scan->high == 0;
        bool needs_ac = scan->end > 0;
        if (!decoder->quant_present[component->quant] ||
            (needs_dc && !decoder->dc[component->dc_table].present) ||
            (needs_ac && !decoder->ac[component->ac_table].present)) {
            return false;
        }
    }
    return true;
}

static void clear_coefficients(JpegDecoder *decoder)
{
    for (uint32_t index = 0; index < JPEG_BLOCK; index++) {
        decoder->coefficients[index] = 0;
    }
}

static bool decode_whole_block(JpegDecoder *decoder, JpegComponent *component)
{
    JpegBits *bits = &decoder->bits;
    clear_coefficients(decoder);
    int32_t size = jpeg_decode_symbol(bits, &decoder->dc[component->dc_table]);
    if (size < 0 || size > 16) {
        return false;
    }
    int32_t difference = jpeg_receive_extend(bits, (uint32_t)size);
    component->predictor = (int32_t)((uint32_t)component->predictor + (uint32_t)difference);
    decoder->coefficients[0] = component->predictor;
    for (uint32_t index = 1; index < JPEG_BLOCK;) {
        int32_t symbol = jpeg_decode_symbol(bits, &decoder->ac[component->ac_table]);
        if (symbol < 0) {
            return false;
        }
        uint32_t run = (uint32_t)symbol >> 4;
        uint32_t value_size = (uint32_t)symbol & 0x0F;
        if (value_size == 0) {
            if (run != END_OF_BAND) {
                break;
            }
            index += ZERO_RUN;
            continue;
        }
        index += run;
        if (index >= JPEG_BLOCK) {
            return false;
        }
        decoder->coefficients[JPEG_ZIGZAG[index++]] = jpeg_receive_extend(bits, value_size);
    }
    return !bits->failed;
}

static bool decode_dc_first(JpegDecoder *decoder, JpegComponent *component, uint32_t low)
{
    JpegBits *bits = &decoder->bits;
    clear_coefficients(decoder);
    int32_t size = jpeg_decode_symbol(bits, &decoder->dc[component->dc_table]);
    if (size < 0 || size > 16) {
        return false;
    }
    int32_t difference = jpeg_receive_extend(bits, (uint32_t)size);
    component->predictor = (int32_t)((uint32_t)component->predictor + (uint32_t)difference);
    decoder->coefficients[0] = (int32_t)((uint32_t)component->predictor << low);
    return !bits->failed;
}

static bool decode_dc_refine(JpegDecoder *decoder, uint32_t low)
{
    clear_coefficients(decoder);
    decoder->coefficients[0] = (int32_t)(jpeg_read_bits(&decoder->bits, 1) << low);
    return !decoder->bits.failed;
}

static bool decode_ac_first(JpegDecoder *decoder, JpegComponent *component, const JpegScan *scan)
{
    JpegBits *bits = &decoder->bits;
    clear_coefficients(decoder);
    if (decoder->eob_run > 0) {
        decoder->eob_run--;
        return true;
    }
    for (uint32_t index = scan->start; index <= scan->end;) {
        int32_t symbol = jpeg_decode_symbol(bits, &decoder->ac[component->ac_table]);
        if (symbol < 0) {
            return false;
        }
        uint32_t run = (uint32_t)symbol >> 4;
        uint32_t value_size = (uint32_t)symbol & 0x0F;
        if (value_size == 0) {
            if (run < END_OF_BAND) {
                decoder->eob_run = (int32_t)((1u << run) - 1u + jpeg_read_bits(bits, run));
                break;
            }
            index += ZERO_RUN;
            continue;
        }
        index += run;
        if (index > scan->end) {
            return false;
        }
        int32_t value = jpeg_receive_extend(bits, value_size);
        decoder->coefficients[JPEG_ZIGZAG[index++]] = (int32_t)((uint32_t)value << scan->low);
    }
    return !bits->failed;
}

static bool decode_unit(JpegDecoder *decoder, const JpegScan *scan, uint32_t member,
                        JpegBlockKind *kind)
{
    JpegComponent *component = &decoder->components[member];
    if (!decoder->progressive) {
        *kind = JPEG_BLOCK_WHOLE;
        return decode_whole_block(decoder, component);
    }
    if (scan->start == 0 && scan->high == 0) {
        *kind = JPEG_BLOCK_DC_FIRST;
        return decode_dc_first(decoder, component, scan->low);
    }
    if (scan->start == 0) {
        *kind = JPEG_BLOCK_DC_DELTA;
        return decode_dc_refine(decoder, scan->low);
    }
    *kind = JPEG_BLOCK_AC_DELTA;
    return decode_ac_first(decoder, component, scan);
}

static bool decode_block_at(JpegDecoder *decoder, const JpegScan *scan, uint32_t member,
                            uint32_t block_x, uint32_t block_y)
{
    JpegBlockKind kind = JPEG_BLOCK_WHOLE;
    if (!decode_unit(decoder, scan, member, &kind)) {
        return false;
    }
    jpeg_output_block(decoder, member, block_x, block_y, kind);
    return true;
}

static void restart_scan(JpegDecoder *decoder)
{
    jpeg_restart(&decoder->bits);
    for (uint32_t index = 0; index < decoder->component_count; index++) {
        decoder->components[index].predictor = 0;
    }
    decoder->eob_run = 0;
}

static bool restart_due(JpegDecoder *decoder, uint32_t *units)
{
    if (decoder->restart_interval == 0) {
        return false;
    }
    if (*units == decoder->restart_interval) {
        restart_scan(decoder);
        *units = 0;
    }
    (*units)++;
    return true;
}

static bool decode_mcu(JpegDecoder *decoder, const JpegScan *scan, uint32_t mcu_x, uint32_t mcu_y)
{
    for (uint32_t index = 0; index < scan->count; index++) {
        uint32_t member = scan->members[index];
        const JpegComponent *component = &decoder->components[member];
        for (uint32_t v = 0; v < component->v; v++) {
            for (uint32_t h = 0; h < component->h; h++) {
                uint32_t block_x = mcu_x * component->h + h;
                uint32_t block_y = mcu_y * component->v + v;
                if (!decode_block_at(decoder, scan, member, block_x, block_y)) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool decode_interleaved(JpegDecoder *decoder, const JpegScan *scan)
{
    uint32_t units = 0;
    const JpegComponent *first = &decoder->components[scan->members[0]];
    for (uint32_t mcu_y = 0; mcu_y < decoder->mcus_high; mcu_y++) {
        if (jpeg_block_row_past_crop(decoder, scan->members[0], mcu_y * first->v)) {
            return true;
        }
        for (uint32_t mcu_x = 0; mcu_x < decoder->mcus_wide; mcu_x++) {
            restart_due(decoder, &units);
            if (!decode_mcu(decoder, scan, mcu_x, mcu_y)) {
                return false;
            }
        }
    }
    return true;
}

static bool decode_single(JpegDecoder *decoder, const JpegScan *scan)
{
    uint32_t units = 0;
    uint32_t member = scan->members[0];
    const JpegComponent *component = &decoder->components[member];
    for (uint32_t block_y = 0; block_y < component->blocks_high; block_y++) {
        if (jpeg_block_row_past_crop(decoder, member, block_y)) {
            return true;
        }
        for (uint32_t block_x = 0; block_x < component->blocks_wide; block_x++) {
            restart_due(decoder, &units);
            if (!decode_block_at(decoder, scan, member, block_x, block_y)) {
                return false;
            }
        }
    }
    return true;
}

static bool band_kept(const JpegDecoder *decoder, const JpegScan *scan)
{
    for (uint32_t index = scan->start; index <= scan->end; index++) {
        if (jpeg_coefficient_used(decoder, JPEG_ZIGZAG[index])) {
            return true;
        }
    }
    return false;
}

static bool scan_skippable(const JpegDecoder *decoder, const JpegScan *scan)
{
    bool ac = scan->start > 0;
    return decoder->progressive && ac && (scan->high > 0 || !band_kept(decoder, scan));
}

static bool decode_scan(JpegDecoder *decoder, const JpegScan *scan)
{
    jpeg_bits_reset(&decoder->bits, decoder->reader);
    decoder->eob_run = 0;
    for (uint32_t index = 0; index < decoder->component_count; index++) {
        decoder->components[index].predictor = 0;
    }
    bool decoded =
        scan->count == 1 ? decode_single(decoder, scan) : decode_interleaved(decoder, scan);
    jpeg_skip_entropy(decoder->reader);
    return decoded;
}

static Step run_scan(JpegDecoder *decoder, uint32_t length, uint32_t *scans)
{
    JpegScan scan;
    if (!decoder->frame_seen || !read_scan_header(decoder, length, &scan) ||
        !scan_valid(decoder, &scan) || ++*scans > MAX_SCANS) {
        return STEP_FAILED;
    }
    if (scan_skippable(decoder, &scan)) {
        jpeg_skip_entropy(decoder->reader);
        return STEP_CONTINUE;
    }
    if (!tables_present(decoder, &scan) || !decode_scan(decoder, &scan)) {
        return STEP_FAILED;
    }
    for (uint32_t index = 0; index < scan.count; index++) {
        decoder->scanned |= 1u << scan.members[index];
    }
    bool complete = decoder->scanned == (1u << decoder->component_count) - 1u;
    return !decoder->progressive && complete ? STEP_DONE : STEP_CONTINUE;
}

static CoverImageResult read_segment(JpegDecoder *decoder, uint32_t marker, uint32_t *scans,
                                     Step *step)
{
    ImageReader *reader = decoder->reader;
    uint32_t length = 0;
    *step = STEP_CONTINUE;
    if (!read_segment_length(reader, &length)) {
        return COVER_IMAGE_CORRUPT;
    }
    if (marker == MARKER_SOF0 || marker == MARKER_SOF1 || marker == MARKER_SOF2) {
        decoder->progressive = marker == MARKER_SOF2;
        return read_frame(decoder, length);
    }
    if ((marker >= 0xC3 && marker <= 0xCF) && marker != MARKER_DHT) {
        return COVER_IMAGE_UNSUPPORTED;
    }
    bool read = true;
    if (marker == MARKER_DHT) {
        read = read_huffman(decoder, length);
    } else if (marker == MARKER_DQT) {
        read = read_quant(decoder, length);
    } else if (marker == MARKER_DRI) {
        decoder->restart_interval = image_reader_be16(reader);
        read = length == 2 && !reader->failed;
    } else if (marker == MARKER_APP14) {
        read = read_adobe(decoder, length);
    } else if (marker == MARKER_SOS) {
        *step = run_scan(decoder, length, scans);
        return *step == STEP_FAILED ? COVER_IMAGE_CORRUPT : COVER_IMAGE_OK;
    } else {
        read = image_reader_skip(reader, length);
    }
    return read ? COVER_IMAGE_OK : COVER_IMAGE_CORRUPT;
}

static bool standalone(uint32_t marker)
{
    return marker == MARKER_TEM || (marker >= MARKER_RST0 && marker <= MARKER_RST7);
}

static CoverImageResult read_markers(JpegDecoder *decoder)
{
    uint32_t scans = 0;
    bool decoded_any = false;
    for (;;) {
        uint32_t marker = next_marker(decoder->reader);
        if (marker == 0 || marker == MARKER_EOI) {
            return decoded_any ? COVER_IMAGE_OK : COVER_IMAGE_CORRUPT;
        }
        if (standalone(marker) || marker == MARKER_SOI) {
            continue;
        }
        Step step = STEP_CONTINUE;
        CoverImageResult result = read_segment(decoder, marker, &scans, &step);
        decoded_any = decoded_any || (marker == MARKER_SOS && result == COVER_IMAGE_OK);
        if (result != COVER_IMAGE_OK || step == STEP_DONE) {
            return result;
        }
    }
}

static CoverColor color_of(const JpegDecoder *decoder)
{
    if (!decoder->frame_seen || decoder->component_count == 1) {
        return COVER_COLOR_GRAY;
    }
    const JpegComponent *components = decoder->components;
    bool rgb_ids = components[0].id == 'R' && components[1].id == 'G' && components[2].id == 'B';
    return decoder->adobe_rgb || rgb_ids ? COVER_COLOR_RGB : COVER_COLOR_YCBCR;
}

CoverImageResult jpeg_decode(ImageReader *reader, ImageArena *arena, CoverImage *image)
{
    if (image_reader_byte(reader) != BYTE_FF || image_reader_byte(reader) != MARKER_SOI) {
        return COVER_IMAGE_UNSUPPORTED;
    }
    JpegDecoder *decoder = image_arena_take(arena, sizeof *decoder);
    if (decoder == NULL) {
        return COVER_IMAGE_NO_MEMORY;
    }
    for (uint32_t table = 0; table < JPEG_TABLES; table++) {
        decoder->dc[table].present = false;
        decoder->ac[table].present = false;
        decoder->quant_present[table] = false;
    }
    decoder->reader = reader;
    decoder->grid = image->grid;
    decoder->frame_seen = false;
    decoder->adobe_rgb = false;
    decoder->restart_interval = 0;
    decoder->scanned = 0;
    CoverImageResult result = read_markers(decoder);
    image->color = color_of(decoder);
    return result;
}
