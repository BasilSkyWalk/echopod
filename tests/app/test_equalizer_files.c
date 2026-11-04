#include "check.h"
#include "equalizer_files.h"
#include "fake_hal.h"

#include <string.h>

static bool has_name(const EqualizerFileList *list, const char *name)
{
    for (int index = 0; index < list->count; index++) {
        if (strcmp(list->names[index], name) == 0) {
            return true;
        }
    }
    return false;
}

static void the_scan_keeps_txt_files_and_drops_other_entries(void)
{
    EqualizerFileList list;
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "b.TXT", false, false);
    fake_hal_add_directory_entry("/EQ", "a.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "notes.mp3", false, false);
    fake_hal_add_directory_entry("/EQ", "album", true, false);
    fake_hal_add_directory_entry("/EQ", ".hidden.txt", false, true);
    fake_hal_add_directory_entry("/EQ", "._a.txt", false, false);
    equalizer_files_scan(&list);
    CHECK_EQUAL(2, list.count);
    CHECK(has_name(&list, "a.txt"));
    CHECK(has_name(&list, "b.TXT"));
}

static void the_scan_stops_at_eight_files(void)
{
    EqualizerFileList list;
    fake_hal_reset();
    fake_hal_add_directory_entry("/EQ", "1.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "2.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "3.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "4.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "5.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "6.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "7.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "8.txt", false, false);
    fake_hal_add_directory_entry("/EQ", "9.txt", false, false);
    equalizer_files_scan(&list);
    CHECK_EQUAL(EQUALIZER_FILES_MAX, list.count);
    CHECK_EQUAL(EQUALIZER_FILES_MAX, fake_hal_dir_next_count());
    CHECK(!has_name(&list, "9.txt"));
}

static void a_missing_folder_lists_nothing(void)
{
    EqualizerFileList list;
    fake_hal_reset();
    equalizer_files_scan(&list);
    CHECK_EQUAL(0, list.count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_scan_keeps_txt_files_and_drops_other_entries),
        TEST_CASE(the_scan_stops_at_eight_files),
        TEST_CASE(a_missing_folder_lists_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
