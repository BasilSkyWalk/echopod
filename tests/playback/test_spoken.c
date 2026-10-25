#include "check.h"
#include "playback/spoken.h"
#include "spoken_library.h"

static SpokenLibrary spoken;

static void folders_named_audiobooks_or_podcasts_hold_spoken_word(void)
{
    CHECK_EQUAL(SPOKEN_AUDIOBOOK, spoken_kind_of("/Audiobooks/Book/01.mp3"));
    CHECK_EQUAL(SPOKEN_AUDIOBOOK, spoken_kind_of("AUDIOBOOK/Book.m4a"));
    CHECK_EQUAL(SPOKEN_PODCAST, spoken_kind_of("/podcasts/Show/Episode.mp3"));
    CHECK_EQUAL(SPOKEN_PODCAST, spoken_kind_of("Podcast/Episode.mp3"));
}

static void other_paths_are_music(void)
{
    CHECK_EQUAL(SPOKEN_NONE, spoken_kind_of("/Music/Audiobooks/x.mp3"));
    CHECK_EQUAL(SPOKEN_NONE, spoken_kind_of("/Audiobooks.mp3"));
    CHECK_EQUAL(SPOKEN_NONE, spoken_kind_of("/Audiobookshelf/x.mp3"));
    CHECK_EQUAL(SPOKEN_NONE, spoken_kind_of(""));
}

static void chapters_of_one_book_share_a_bookmark(void)
{
    uint32_t first = spoken_bookmark_key("/Audiobooks/Book/01.mp3");
    CHECK_EQUAL(first, spoken_bookmark_key("Audiobooks/Book/02.mp3"));
    CHECK(first != spoken_bookmark_key("/Audiobooks/Other/01.mp3"));
}

static void loose_books_and_episodes_have_their_own_bookmark(void)
{
    CHECK(spoken_bookmark_key("/Audiobooks/One.mp3") != spoken_bookmark_key("/Audiobooks/Two.mp3"));
    CHECK(spoken_bookmark_key("/Podcasts/Show/A.mp3") !=
          spoken_bookmark_key("/Podcasts/Show/B.mp3"));
    CHECK_EQUAL(spoken_track_key("/Podcasts/Show/A.mp3"),
                spoken_bookmark_key("Podcasts/Show/A.mp3"));
}

static void the_end_of_a_track_is_its_last_tenth_up_to_30_seconds(void)
{
    CHECK(!spoken_near_end(0, 0));
    CHECK(!spoken_near_end(500000, 600000));
    CHECK(spoken_near_end(570000, 600000));
    CHECK(!spoken_near_end(80000, 100000));
    CHECK(spoken_near_end(90000, 100000));
}

static void the_shelves_are_found_under_the_music_root(void)
{
    Library *library = spoken_library_open(&spoken);
    uint16_t books = 0;
    uint16_t shows = 0;
    CHECK(spoken_shelf_folder(library, SPOKEN_AUDIOBOOK, &books));
    CHECK(spoken_shelf_folder(library, SPOKEN_PODCAST, &shows));
    CHECK(books != shows);
    CHECK(!spoken_shelf_folder(library, SPOKEN_NONE, &books));
}

static void a_book_folder_key_matches_its_chapters(void)
{
    Library *library = &spoken.library;
    uint16_t track = spoken_library_track(library, SPOKEN_CHAPTER_TWO_PATH);
    LibraryTrack record;
    uint32_t key = 0;
    CHECK(library_read_track(library, track, &record));
    CHECK(spoken_folder_key(library, record.folder, &key));
    CHECK_EQUAL(spoken_bookmark_key(SPOKEN_CHAPTER_ONE_PATH), key);
}

static void chapters_are_counted_in_folder_order(void)
{
    Library *library = &spoken.library;
    uint16_t three = spoken_library_track(library, SPOKEN_CHAPTER_THREE_PATH);
    LibraryTrack record;
    uint16_t chapter = 0;
    uint16_t found = 0;
    CHECK(spoken_chapter_of(library, three, &chapter));
    CHECK_EQUAL(2, chapter);
    CHECK(library_read_track(library, three, &record));
    CHECK(spoken_chapter_track(library, record.folder, 2, &found));
    CHECK_EQUAL(three, found);
    CHECK(!spoken_chapter_track(library, record.folder, 3, &found));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(folders_named_audiobooks_or_podcasts_hold_spoken_word),
        TEST_CASE(other_paths_are_music),
        TEST_CASE(chapters_of_one_book_share_a_bookmark),
        TEST_CASE(loose_books_and_episodes_have_their_own_bookmark),
        TEST_CASE(the_end_of_a_track_is_its_last_tenth_up_to_30_seconds),
        TEST_CASE(the_shelves_are_found_under_the_music_root),
        TEST_CASE(a_book_folder_key_matches_its_chapters),
        TEST_CASE(chapters_are_counted_in_folder_order),
    };
    int result = test_run_all(cases, TEST_CASE_COUNT(cases));
    spoken_library_close(&spoken);
    return result;
}
