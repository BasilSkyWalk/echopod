#include <string.h>

#include "check.h"
#include "core/volume_path.h"

static void the_internal_root_maps_to_the_volume_root(void)
{
    CHECK(strcmp(volume_internal_path("/:internal"), "/") == 0);
    CHECK(strcmp(volume_internal_path("/:internal/"), "/") == 0);
}

static void a_path_inside_keeps_its_rest(void)
{
    CHECK(strcmp(volume_internal_path("/:internal/Artist/01 Song.flac"), "/Artist/01 Song.flac") ==
          0);
}

static void card_paths_stay_on_the_card(void)
{
    CHECK(volume_internal_path("/") == NULL);
    CHECK(volume_internal_path("/Artist/01.flac") == NULL);
    CHECK(volume_internal_path("/:internals/x.flac") == NULL);
    CHECK(volume_internal_path("/:intern") == NULL);
    CHECK(volume_internal_path(":internal/x.flac") == NULL);
    CHECK(volume_internal_path("/.echo/library.db") == NULL);
}

static void only_the_exact_name_is_the_internal_folder(void)
{
    CHECK(volume_is_internal_name(":internal", 9));
    CHECK(volume_is_internal_name(":internal/x", 9));
    CHECK(!volume_is_internal_name(":internal", 8));
    CHECK(!volume_is_internal_name("internal", 8));
    CHECK(!volume_is_internal_name(":Internal", 9));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_internal_root_maps_to_the_volume_root),
        TEST_CASE(a_path_inside_keeps_its_rest),
        TEST_CASE(card_paths_stay_on_the_card),
        TEST_CASE(only_the_exact_name_is_the_internal_folder),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
