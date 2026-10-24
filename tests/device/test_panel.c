#include "check.h"
#include "device/panel.h"

enum {
    LOG_CAPACITY = 512
};

typedef enum EntryKind {
    ENTRY_COMMAND,
    ENTRY_DATA,
    ENTRY_DELAY
} EntryKind;

typedef struct Entry {
    EntryKind kind;
    uint32_t value;
} Entry;

typedef struct Recorder {
    Entry entries[LOG_CAPACITY];
    int count;
} Recorder;

static void record(Recorder *recorder, EntryKind kind, uint32_t value)
{
    if (recorder->count < LOG_CAPACITY) {
        recorder->entries[recorder->count++] = (Entry){kind, value};
    }
}

static void record_command(void *context, uint8_t command)
{
    record(context, ENTRY_COMMAND, command);
}

static void record_data(void *context, uint8_t value)
{
    record(context, ENTRY_DATA, value);
}

static void record_pixels(void *context, const uint16_t *pixels, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++) {
        record(context, ENTRY_DATA, (uint8_t)(pixels[index] >> 8));
        record(context, ENTRY_DATA, (uint8_t)pixels[index]);
    }
}

static void record_delay(void *context, uint32_t milliseconds)
{
    record(context, ENTRY_DELAY, milliseconds);
}

static PanelBus recording_bus(Recorder *recorder)
{
    *recorder = (Recorder){.count = 0};
    return (PanelBus){record_command, record_data, record_pixels, record_delay, recorder};
}

#define C(value) {ENTRY_COMMAND, value}
#define D(value) {ENTRY_DATA, value}
#define W(value) {ENTRY_DELAY, value}

static const Entry VARIANT_B_INIT[] = {
    C(0x11), W(120),  C(0x36), D(0x60), C(0x3A), D(0x05), C(0xA4), D(0x44), D(0x44), C(0xB2),
    D(0x0C), D(0x0C), D(0x00), D(0x33), D(0x33), C(0xB7), D(0x35), C(0xBB), D(0x1A), C(0xC0),
    D(0x2C), C(0xC2), D(0x01), C(0xC3), D(0x0B), C(0xC4), D(0x20), C(0xC6), D(0x0F), C(0xD0),
    D(0xA4), D(0xA1), C(0xD6), D(0xA1), C(0xE0), D(0xF0), D(0x06), D(0x0B), D(0x07), D(0x07),
    D(0x24), D(0x2E), D(0x32), D(0x46), D(0x37), D(0x13), D(0x13), D(0x2D), D(0x33), C(0xE1),
    D(0xF0), D(0x02), D(0x06), D(0x09), D(0x08), D(0x05), D(0x29), D(0x44), D(0x42), D(0x38),
    D(0x14), D(0x14), D(0x2A), D(0x30), C(0x2A), D(0x00), D(0x00), D(0x00), D(0xEF), C(0x2B),
    D(0x00), D(0x23), D(0x00), D(0xCC), C(0x21), C(0x29), C(0x2C),
};

static const Entry VARIANT_A_INIT[] = {
    C(0x11), W(120),  C(0xDF), D(0x5A), D(0x69), D(0x02), D(0x01), C(0x36), D(0x60),
    C(0x3A), D(0x05), C(0xB2), D(0x0B), D(0x0B), D(0x00), D(0x33), D(0x33), C(0xB7),
    D(0x75), C(0xBB), D(0x28), C(0xC0), D(0x2C), C(0xC2), D(0x01), C(0xC3), D(0x1F),
    C(0xC5), D(0x2A), C(0xC6), D(0x13), C(0xD0), D(0xA7), C(0xD0), D(0xA4), D(0xA1),
    C(0xD6), D(0xA1), C(0xE0), D(0xF0), D(0x05), D(0x0A), D(0x06), D(0x06), D(0x03),
    D(0x2B), D(0x32), D(0x43), D(0x36), D(0x11), D(0x10), D(0x2B), D(0x32), C(0xE1),
    D(0xF0), D(0x08), D(0x0C), D(0x0B), D(0x09), D(0x24), D(0x2B), D(0x22), D(0x43),
    D(0x38), D(0x15), D(0x16), D(0x2F), D(0x37), C(0x21), C(0x29), C(0x2C),
};

static void check_log(const Recorder *recorder, const Entry *expected, int count)
{
    CHECK_EQUAL(count, recorder->count);
    for (int index = 0; index < count && index < recorder->count; index++) {
        CHECK_EQUAL(expected[index].kind, recorder->entries[index].kind);
        CHECK_EQUAL(expected[index].value, recorder->entries[index].value);
    }
}

static void variant_b_sends_its_init_stream(void)
{
    Recorder recorder;
    PanelBus bus = recording_bus(&recorder);
    panel_send_init(&bus, PANEL_VARIANT_B);
    check_log(&recorder, VARIANT_B_INIT, (int)(sizeof VARIANT_B_INIT / sizeof VARIANT_B_INIT[0]));
}

static void variant_a_sends_its_init_stream(void)
{
    Recorder recorder;
    PanelBus bus = recording_bus(&recorder);
    panel_send_init(&bus, PANEL_VARIANT_A);
    check_log(&recorder, VARIANT_A_INIT, (int)(sizeof VARIANT_A_INIT / sizeof VARIANT_A_INIT[0]));
}

static void a_window_maps_visible_rows_to_panel_rows_from_35(void)
{
    static const Entry expected[] = {
        C(0x2A), D(0x00), D(0x00), D(0x01), D(0x3F), C(0x2B),
        D(0x00), D(0x23), D(0x00), D(0xCC), C(0x2C),
    };
    Recorder recorder;
    PanelBus bus = recording_bus(&recorder);
    panel_begin_write(&bus, (PanelWindow){0, 0, 320, 170});
    check_log(&recorder, expected, (int)(sizeof expected / sizeof expected[0]));
}

static void a_partial_window_sets_its_columns_and_rows(void)
{
    static const Entry expected[] = {
        C(0x2A), D(0x00), D(0x0A), D(0x01), D(0x09), C(0x2B),
        D(0x00), D(0x45), D(0x00), D(0x66), C(0x2C),
    };
    Recorder recorder;
    PanelBus bus = recording_bus(&recorder);
    panel_begin_write(&bus, (PanelWindow){10, 34, 256, 34});
    check_log(&recorder, expected, (int)(sizeof expected / sizeof expected[0]));
}

static void pixels_are_sent_high_byte_first(void)
{
    static const uint16_t pixels[] = {0xF800, 0x1234};
    static const Entry expected[] = {D(0xF8), D(0x00), D(0x12), D(0x34)};
    Recorder recorder;
    PanelBus bus = recording_bus(&recorder);
    bus.pixels(bus.context, pixels, 2);
    check_log(&recorder, expected, 4);
}

static void a_bus_word_puts_the_byte_on_bits_16_to_23(void)
{
    CHECK_EQUAL(0x00AB0000u, panel_bus_word(0, 0xAB, false));
}

static void a_data_byte_sets_dc_on_bit_25(void)
{
    CHECK_EQUAL(0x02AB0000u, panel_bus_word(0, 0xAB, true));
}

static void a_bus_word_keeps_other_port_bits_and_clears_bus_bits(void)
{
    uint32_t shadow = 0x00001000u | 0x80000001u | 0x03FF0000u;
    CHECK_EQUAL(0x80001001u | 0x00120000u, panel_bus_word(shadow, 0x12, false));
}

static void the_write_strobe_is_bit_24(void)
{
    CHECK_EQUAL(0x01000000u, PANEL_BUS_WR);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(variant_b_sends_its_init_stream),
        TEST_CASE(variant_a_sends_its_init_stream),
        TEST_CASE(a_window_maps_visible_rows_to_panel_rows_from_35),
        TEST_CASE(a_partial_window_sets_its_columns_and_rows),
        TEST_CASE(pixels_are_sent_high_byte_first),
        TEST_CASE(a_bus_word_puts_the_byte_on_bits_16_to_23),
        TEST_CASE(a_data_byte_sets_dc_on_bit_25),
        TEST_CASE(a_bus_word_keeps_other_port_bits_and_clears_bus_bits),
        TEST_CASE(the_write_strobe_is_bit_24),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
