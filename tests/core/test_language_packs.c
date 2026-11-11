#include <stdio.h>
#include <string.h>

#include "check.h"
#include "core/i18n.h"
#include "settings/settings.h"

enum {
    PATH_CAPACITY = 1024,
    PACK_LIMIT = 0x3000
};

static const char PACK_PATHS[] = ECHO_LANGUAGE_PACK_PATHS;

static uint32_t read_pack(const char *path, uint8_t *pack)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    size_t size = fread(pack, 1, PACK_LIMIT + 1, file);
    fclose(file);
    return (uint32_t)size;
}

static void every_language_has_a_pack_that_attaches(void)
{
    static uint8_t pack[PACK_LIMIT + 1];
    uint32_t count = 0;
    const char *cursor = PACK_PATHS;
    while (*cursor != '\0') {
        char path[PATH_CAPACITY];
        size_t length = strcspn(cursor, "|");
        CHECK(length < sizeof path);
        memcpy(path, cursor, length);
        path[length] = '\0';
        uint32_t size = read_pack(path, pack);
        CHECK(size > 0 && size <= PACK_LIMIT);
        CHECK(i18n_attach(pack, size));
        i18n_detach();
        count++;
        cursor += length + (cursor[length] == '|' ? 1u : 0u);
    }
    CHECK_EQUAL(LANGUAGE_COUNT, count);
}

static uint32_t read_language_pack(Language language, uint8_t *pack)
{
    const char *cursor = PACK_PATHS;
    for (int index = 0; index < (int)language && cursor != NULL; index++) {
        cursor = strchr(cursor, '|');
        cursor = cursor != NULL ? cursor + 1 : NULL;
    }
    if (cursor == NULL) {
        return 0;
    }
    char path[PATH_CAPACITY];
    size_t length = strcspn(cursor, "|");
    CHECK(length < sizeof path);
    memcpy(path, cursor, length);
    path[length] = '\0';
    return read_pack(path, pack);
}

static void polish_translates_the_menus(void)
{
    static uint8_t pack[PACK_LIMIT + 1];
    uint32_t size = read_language_pack(LANGUAGE_POLISH, pack);
    CHECK(size > 0 && size <= PACK_LIMIT);
    CHECK(i18n_attach(pack, size));
    CHECK(strcmp("Uruchom ponownie, aby zastosować", i18n_text(STR_LANGUAGE_RESTART)) == 0);
    CHECK(strcmp("Polski", i18n_text(STR_LANGUAGE_PL)) == 0);
    i18n_detach();
}

static void indonesian_is_the_last_language_and_translates_the_menus(void)
{
    static uint8_t pack[PACK_LIMIT + 1];
    CHECK_EQUAL(LANGUAGE_INDONESIAN + 1, LANGUAGE_COUNT);
    CHECK(strstr(strrchr(PACK_PATHS, '|'), "id.bin") != NULL);
    uint32_t size = read_language_pack(LANGUAGE_INDONESIAN, pack);
    CHECK(size > 0 && size <= PACK_LIMIT);
    CHECK(i18n_attach(pack, size));
    CHECK(strcmp("Pengaturan", i18n_text(STR_MENU_SETTINGS)) == 0);
    CHECK(strcmp("Mulai ulang untuk menerapkan", i18n_text(STR_LANGUAGE_RESTART)) == 0);
    CHECK(strcmp("Bahasa Indonesia", i18n_text(STR_LANGUAGE_ID)) == 0);
    i18n_detach();
}

static void every_translation_has_its_own_chapters_title(void)
{
    static uint8_t pack[PACK_LIMIT + 1];
    const char *cursor = PACK_PATHS;
    uint32_t checked = 0;
    while (*cursor != '\0') {
        char path[PATH_CAPACITY];
        size_t length = strcspn(cursor, "|");
        memcpy(path, cursor, length);
        path[length] = '\0';
        uint32_t size = read_pack(path, pack);
        CHECK(i18n_attach(pack, size));
        if (strstr(path, "/en.bin") == NULL) {
            CHECK(strcmp(i18n_english(STR_MENU_CHAPTERS), i18n_text(STR_MENU_CHAPTERS)) != 0);
            checked++;
        }
        i18n_detach();
        cursor += length + (cursor[length] == '|' ? 1u : 0u);
    }
    CHECK_EQUAL(LANGUAGE_COUNT - 1, checked);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_language_has_a_pack_that_attaches),
        TEST_CASE(every_translation_has_its_own_chapters_title),
        TEST_CASE(polish_translates_the_menus),
        TEST_CASE(indonesian_is_the_last_language_and_translates_the_menus),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
