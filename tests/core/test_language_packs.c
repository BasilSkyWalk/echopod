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

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_language_has_a_pack_that_attaches),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
