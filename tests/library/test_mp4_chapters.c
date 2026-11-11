#include <string.h>

#include "check.h"
#include "library/mp4_chapters.h"
#include "memory_source.h"

enum {
    CAPACITY = 8192,
    UNITS_PER_MS = 10000
};

typedef struct Buffer {
    uint8_t bytes[CAPACITY];
    uint32_t size;
} Buffer;

typedef struct ChapterSpec {
    uint32_t start_ms;
    const char *title;
} ChapterSpec;

static Mp4Chapters chapters;

static void put8(Buffer *buffer, uint8_t value)
{
    buffer->bytes[buffer->size++] = value;
}

static void put32(Buffer *buffer, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        put8(buffer, (uint8_t)(value >> shift));
    }
}

static void put64(Buffer *buffer, uint64_t value)
{
    put32(buffer, (uint32_t)(value >> 32));
    put32(buffer, (uint32_t)value);
}

static void put_text(Buffer *buffer, const char *text, uint32_t length)
{
    memcpy(buffer->bytes + buffer->size, text, length);
    buffer->size += length;
}

static uint32_t begin(Buffer *buffer, const char *type)
{
    uint32_t start = buffer->size;
    put32(buffer, 0);
    put_text(buffer, type, 4);
    return start;
}

static void end(Buffer *buffer, uint32_t start)
{
    uint32_t size = buffer->size - start;
    for (uint32_t index = 0; index < 4; index++) {
        buffer->bytes[start + index] = (uint8_t)(size >> (24 - 8 * index));
    }
}

static void put_chpl(Buffer *buffer, const ChapterSpec *specs, uint32_t count)
{
    uint32_t chpl = begin(buffer, "chpl");
    put32(buffer, 0x01000000u);
    put32(buffer, 0);
    put8(buffer, (uint8_t)count);
    for (uint32_t index = 0; index < count; index++) {
        uint32_t length = (uint32_t)strlen(specs[index].title);
        put64(buffer, (uint64_t)specs[index].start_ms * UNITS_PER_MS);
        put8(buffer, (uint8_t)length);
        put_text(buffer, specs[index].title, length);
    }
    end(buffer, chpl);
}

static void put_moov(Buffer *buffer, const ChapterSpec *specs, uint32_t count)
{
    uint32_t moov = begin(buffer, "moov");
    uint32_t mvhd = begin(buffer, "mvhd");
    put32(buffer, 0);
    end(buffer, mvhd);
    uint32_t udta = begin(buffer, "udta");
    uint32_t meta = begin(buffer, "meta");
    put32(buffer, 0);
    end(buffer, meta);
    put_chpl(buffer, specs, count);
    end(buffer, udta);
    end(buffer, moov);
}

static void put_ftyp(Buffer *buffer)
{
    uint32_t ftyp = begin(buffer, "ftyp");
    put_text(buffer, "M4B ", 4);
    put32(buffer, 0);
    end(buffer, ftyp);
}

static void put_mdat(Buffer *buffer)
{
    uint32_t mdat = begin(buffer, "mdat");
    put_text(buffer, "0123456789abcdef", 16);
    end(buffer, mdat);
}

static const ChapterSpec BOOK[] = {{0, "Intro"}, {90000, "Part 2"}, {180500, ""}};

static void book_with_moov_first(Buffer *buffer)
{
    put_ftyp(buffer);
    put_moov(buffer, BOOK, 3);
    put_mdat(buffer);
}

static bool read_chapters(const Buffer *buffer)
{
    MemorySource memory = {.bytes = buffer->bytes, .size = buffer->size};
    return mp4_chapters_read(&chapters, memory_source(&memory), buffer->size);
}

static void chapters_come_with_their_starts(void)
{
    static Buffer buffer;
    buffer.size = 0;
    book_with_moov_first(&buffer);
    CHECK(read_chapters(&buffer));
    CHECK_EQUAL(3, chapters.count);
    CHECK_EQUAL(0, mp4_chapter_start_ms(&chapters, 0));
    CHECK_EQUAL(90000, mp4_chapter_start_ms(&chapters, 1));
    CHECK_EQUAL(180000, mp4_chapter_start_ms(&chapters, 2));
}

static void a_position_belongs_to_the_last_chapter_that_started(void)
{
    static Buffer buffer;
    buffer.size = 0;
    book_with_moov_first(&buffer);
    CHECK(read_chapters(&buffer));
    CHECK_EQUAL(0, mp4_chapter_at(&chapters, 0));
    CHECK_EQUAL(0, mp4_chapter_at(&chapters, 89999));
    CHECK_EQUAL(1, mp4_chapter_at(&chapters, 90000));
    CHECK_EQUAL(1, mp4_chapter_at(&chapters, 179999));
    CHECK_EQUAL(2, mp4_chapter_at(&chapters, 180000));
    CHECK_EQUAL(2, mp4_chapter_at(&chapters, UINT32_MAX));
}

static void a_position_before_the_first_chapter_belongs_to_it(void)
{
    static const ChapterSpec LATE[] = {{5000, "A"}, {9000, "B"}};
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    put_moov(&buffer, LATE, 2);
    CHECK(read_chapters(&buffer));
    CHECK_EQUAL(0, mp4_chapter_at(&chapters, 100));
}

static void a_moov_after_the_media_is_found(void)
{
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    put_mdat(&buffer);
    put_moov(&buffer, BOOK, 3);
    CHECK(read_chapters(&buffer));
    CHECK_EQUAL(3, chapters.count);
}

static void a_sixty_four_bit_atom_size_is_refused(void)
{
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    put32(&buffer, 1);
    put_text(&buffer, "mdat", 4);
    put64(&buffer, 16 + 16);
    put_text(&buffer, "0123456789abcdef", 16);
    put_moov(&buffer, BOOK, 3);
    CHECK(!read_chapters(&buffer));
    CHECK_EQUAL(0, chapters.count);
}

static void more_chapters_than_are_kept_are_counted_but_not_placed(void)
{
    static ChapterSpec many[MP4_CHAPTERS_MAX];
    static Buffer buffer;
    for (uint32_t index = 0; index < MP4_CHAPTERS_MAX; index++) {
        many[index] = (ChapterSpec){index * 1000, ""};
    }
    buffer.size = 0;
    put_ftyp(&buffer);
    put_moov(&buffer, many, MP4_CHAPTERS_MAX);
    CHECK(read_chapters(&buffer));
    CHECK_EQUAL(MP4_CHAPTERS_MAX, chapters.count);
    CHECK_EQUAL((MP4_CHAPTERS_KEPT - 1) * 1000,
                mp4_chapter_start_ms(&chapters, MP4_CHAPTERS_KEPT - 1));
}

static void a_chapter_longer_than_the_gap_can_hold_is_refused(void)
{
    static const ChapterSpec LONG[] = {{0, "A"}, {70000000, "B"}};
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    put_moov(&buffer, LONG, 2);
    CHECK(!read_chapters(&buffer));
    CHECK_EQUAL(0, chapters.count);
}

static void a_file_without_chapters_reads_as_none(void)
{
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    uint32_t moov = begin(&buffer, "moov");
    uint32_t mvhd = begin(&buffer, "mvhd");
    put32(&buffer, 0);
    end(&buffer, mvhd);
    end(&buffer, moov);
    CHECK(!read_chapters(&buffer));
    CHECK_EQUAL(0, chapters.count);

    buffer.size = 0;
    put_ftyp(&buffer);
    put_moov(&buffer, BOOK, 0);
    CHECK(!read_chapters(&buffer));
    CHECK_EQUAL(0, chapters.count);
}

static void chapters_that_go_back_in_time_are_refused(void)
{
    static const ChapterSpec BACKWARDS[] = {{0, "A"}, {5000, "B"}, {4000, "C"}};
    static Buffer buffer;
    buffer.size = 0;
    put_ftyp(&buffer);
    put_moov(&buffer, BACKWARDS, 3);
    CHECK(!read_chapters(&buffer));
    CHECK_EQUAL(0, chapters.count);
}

static void a_damaged_file_is_never_read_outside_itself(void)
{
    static Buffer buffer;
    buffer.size = 0;
    book_with_moov_first(&buffer);
    for (uint32_t cut = 0; cut <= buffer.size; cut++) {
        MemorySource memory = {.bytes = buffer.bytes, .size = cut};
        bool read = mp4_chapters_read(&chapters, memory_source(&memory), cut);
        CHECK(!read || chapters.count == 3);
    }
    for (uint32_t byte = 0; byte < buffer.size; byte++) {
        Buffer damaged = buffer;
        damaged.bytes[byte] ^= 0xFF;
        read_chapters(&damaged);
    }
}

static void a_failing_source_reads_as_none(void)
{
    static Buffer buffer;
    buffer.size = 0;
    book_with_moov_first(&buffer);
    MemorySource memory = {.bytes = buffer.bytes, .size = buffer.size, .failing = true};
    CHECK(!mp4_chapters_read(&chapters, memory_source(&memory), buffer.size));
    CHECK_EQUAL(0, chapters.count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(chapters_come_with_their_starts),
        TEST_CASE(a_position_belongs_to_the_last_chapter_that_started),
        TEST_CASE(a_position_before_the_first_chapter_belongs_to_it),
        TEST_CASE(a_moov_after_the_media_is_found),
        TEST_CASE(a_sixty_four_bit_atom_size_is_refused),
        TEST_CASE(more_chapters_than_are_kept_are_counted_but_not_placed),
        TEST_CASE(a_chapter_longer_than_the_gap_can_hold_is_refused),
        TEST_CASE(a_file_without_chapters_reads_as_none),
        TEST_CASE(chapters_that_go_back_in_time_are_refused),
        TEST_CASE(a_damaged_file_is_never_read_outside_itself),
        TEST_CASE(a_failing_source_reads_as_none),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
