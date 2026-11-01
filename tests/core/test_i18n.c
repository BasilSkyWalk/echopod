#include <string.h>

#include "check.h"
#include "core/i18n.h"

enum {
    PACK_CAPACITY = 4096
};

static void put_u16(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static size_t build_pack(uint8_t *pack, const char *on, const char *off)
{
    memcpy(pack, "ELNG", 4);
    put_u16(pack + 4, I18N_VERSION);
    put_u16(pack + 6, STRING_COUNT);
    uint32_t base = I18N_HEADER_BYTES + 2 * STRING_COUNT;
    uint32_t used = 0;
    for (int id = 0; id < STRING_COUNT; id++) {
        const char *text = id == STR_ON ? on : id == STR_OFF ? off : "";
        put_u16(pack + I18N_HEADER_BYTES + 2 * id, used);
        size_t length = strlen(text) + 1;
        memcpy(pack + base + used, text, length);
        used += (uint32_t)length;
    }
    uint32_t total = base + used;
    put_u16(pack + 8, total);
    put_u16(pack + 10, 0);
    return total;
}

static void english_is_used_without_a_pack(void)
{
    i18n_detach();
    CHECK(strcmp("On", i18n_text(STR_ON)) == 0);
    CHECK(strcmp("Off", i18n_text(STR_OFF)) == 0);
    CHECK(strcmp("", i18n_text(STRING_COUNT)) == 0);
    CHECK(strcmp("", i18n_text((StringId)-1)) == 0);
}

static void an_attached_pack_replaces_english(void)
{
    uint8_t pack[PACK_CAPACITY];
    size_t size = build_pack(pack, "Ein", "Aus");
    CHECK(i18n_attach(pack, size));
    CHECK(strcmp("Ein", i18n_text(STR_ON)) == 0);
    CHECK(strcmp("Aus", i18n_text(STR_OFF)) == 0);
    CHECK(strcmp("On", i18n_english(STR_ON)) == 0);
    i18n_detach();
    CHECK(strcmp("On", i18n_text(STR_ON)) == 0);
}

static void an_empty_translation_falls_back_to_english(void)
{
    uint8_t pack[PACK_CAPACITY];
    size_t size = build_pack(pack, "Ein", "");
    CHECK(i18n_attach(pack, size));
    CHECK(strcmp("Ein", i18n_text(STR_ON)) == 0);
    CHECK(strcmp("Off", i18n_text(STR_OFF)) == 0);
    i18n_detach();
}

static void a_pack_for_another_catalogue_is_refused(void)
{
    uint8_t pack[PACK_CAPACITY];
    size_t size = build_pack(pack, "Ein", "Aus");
    put_u16(pack + 6, STRING_COUNT + 1);
    CHECK(!i18n_attach(pack, size));
    CHECK(strcmp("On", i18n_text(STR_ON)) == 0);
}

static void a_damaged_pack_is_refused(void)
{
    uint8_t pack[PACK_CAPACITY];
    size_t size = build_pack(pack, "Ein", "Aus");
    CHECK(!i18n_attach(pack, size - 1));
    pack[0] = 'X';
    CHECK(!i18n_attach(pack, size));
    CHECK(!i18n_attach(NULL, 0));
    build_pack(pack, "Ein", "Aus");
    put_u16(pack + I18N_HEADER_BYTES, 0x7000);
    CHECK(!i18n_attach(pack, size));
    CHECK(strcmp("On", i18n_text(STR_ON)) == 0);
}

static void placeholders_are_replaced_in_any_order(void)
{
    uint8_t pack[PACK_CAPACITY];
    size_t size = build_pack(pack, "{1} von {0} {2}", "");
    char buffer[32];
    StringBuilder builder = string_builder_make(buffer, sizeof buffer);
    const char *const arguments[] = {"3", "9"};
    CHECK(i18n_attach(pack, size));
    i18n_format(&builder, STR_ON, arguments, 2);
    CHECK(strcmp("9 von 3 ", buffer) == 0);
    i18n_detach();
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(english_is_used_without_a_pack),
        TEST_CASE(an_attached_pack_replaces_english),
        TEST_CASE(an_empty_translation_falls_back_to_english),
        TEST_CASE(a_pack_for_another_catalogue_is_refused),
        TEST_CASE(a_damaged_pack_is_refused),
        TEST_CASE(placeholders_are_replaced_in_any_order),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
