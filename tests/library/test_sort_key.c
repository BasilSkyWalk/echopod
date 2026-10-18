#include <string.h>

#include "check.h"
#include "library/sort_key.h"

typedef struct KeyCase {
    const char *text;
    const char *key;
} KeyCase;

static const KeyCase CASES[] = {
    {"The Beatles", "beatles"},
    {"Bj\xc3\xb6rk", "bjork"},
    {"  The  Who ", "who"},
    {"Theatre", "theatre"},
    {"the", "the"},
    {"The ", "the"},
    {"\xc3\x86ON Flux", "\xc3\xa6on flux"},
    {"Stra\xc3\x9f"
     "e",
     "strasse"},
    {"\xef\xac\x81ne Day", "fine day"},
    {"\xef\xbc\xa6\xef\xbd\x95\xef\xbd\x8c\xef\xbd\x8c "
     "\xef\xbc\xb7\xef\xbd\x89\xef\xbd\x84\xef\xbd\x94"
     "\xef\xbd\x88",
     "full width"},
    {"\xc3\x81g\xc3\xa6tis byrjun", "ag\xc3\xa6tis byrjun"},
    {"\xce\xa3\xce\x8a\xce\xa3\xce\xa5\xce\xa6\xce\x9f\xce\xa3",
     "\xcf\x83\xce\xb9\xcf\x83\xcf\x85\xcf\x86\xce\xbf\xcf\x83"},
    {"Ti\xc3\xabsto", "tiesto"},
    {"\xc4\x90or\xc4\x91"
     "e",
     "\xc4\x91or\xc4\x91"
     "e"},
    {"\xc4\xbflibre", "l\xc2\xb7libre"},
    {"\xc2\xbd Life", "1\xe2\x81\x84"
                      "2 life"},
    {"\xe2\x85\xab", "xii"},
    {"the\xc2\xa0"
     "band",
     "band"},
    {"\xc4\xb0stanbul", "istanbul"},
    {"\xc7\x85"
     "emal",
     "dzemal"},
    {"AC/DC", "ac/dc"},
    {"\xe3\x82\xa8\xe3\x83\xb3\xe3\x82\xb8\xe3\x82\xa7\xe3\x83\xab",
     "\xe3\x82\xa8\xe3\x83\xb3\xe3\x82\xb7\xe3\x82\xa7\xe3\x83\xab"},
    {"\xea\xb0\x80", "\xe1\x84\x80\xe1\x85\xa1"},
    {"\xed\x95\x9c\xea\xb5\xad \xeb\x85\xb8\xeb\x9e\x98",
     "\xe1\x84\x92\xe1\x85\xa1\xe1\x86\xab\xe1\x84\x80\xe1\x85\xae\xe1\x86\xa8 "
     "\xe1\x84\x82\xe1\x85\xa9\xe1\x84\x85\xe1\x85\xa2"},
    {"\xef\xbd\xb6\xef\xbe\x9e\xef\xbd\xb7\xef\xbe\x9e", "\xe3\x82\xab\xe3\x82\xad"},
};

static void keys_match_the_pc_indexer(void)
{
    for (size_t index = 0; index < sizeof CASES / sizeof CASES[0]; index++) {
        char key[SORT_KEY_CAPACITY];
        sort_key_make(CASES[index].text, key, sizeof key);
        CHECK_EQUAL(0, strcmp(CASES[index].key, key));
    }
}

static void comparison_follows_the_keys(void)
{
    CHECK(sort_key_compare("The Beatles", "Bj\xc3\xb6rk") < 0);
    CHECK(sort_key_compare("Bj\xc3\xb6rk", "bjork") == 0);
    CHECK(sort_key_compare("ABBA", "abba") == 0);
    CHECK(sort_key_compare("Zebra", "apple") > 0);
}

static void a_long_text_is_cut_at_the_capacity(void)
{
    char text[3000];
    memset(text, 'A', sizeof text - 1);
    text[sizeof text - 1] = '\0';
    char key[16];
    CHECK_EQUAL(15u, sort_key_make(text, key, sizeof key));
    CHECK_EQUAL(0, strcmp("aaaaaaaaaaaaaaa", key));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(keys_match_the_pc_indexer),
        TEST_CASE(comparison_follows_the_keys),
        TEST_CASE(a_long_text_is_cut_at_the_capacity),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
