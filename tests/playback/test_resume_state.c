#include <string.h>

#include "check.h"
#include "core/hash.h"
#include "playback/resume_state.h"

enum {
    SHUFFLE_FIELD = 6,
    CHECKSUM_FIELD = 40
};

static ResumeState sample_state(void)
{
    return (ResumeState){
        .library_fingerprint = 0xCAFEF00Du,
        .plan =
            {
                .source = {.kind = BROWSE_GROUP_ALBUMS,
                           .group = LIBRARY_GENRES,
                           .subject = 3,
                           .artist = LIBRARY_NONE},
                .anchor_track = 1234,
                .shuffle = SHUFFLE_ALBUMS,
                .seed = 0x12345678u,
            },
        .repeat = REPEAT_ALL,
        .volume = 37,
        .cursor = {.position = 17, .album_track = 4},
        .track = 1240,
        .position_ms = 183456,
    };
}

static void rewrite_checksum(uint8_t *bytes)
{
    uint32_t checksum = hash_update(HASH_START, bytes, CHECKSUM_FIELD);
    for (int index = 0; index < 4; index++) {
        bytes[CHECKSUM_FIELD + index] = (uint8_t)(checksum >> (8 * index));
    }
}

static void a_state_survives_a_round_trip(void)
{
    ResumeState original = sample_state();
    uint8_t bytes[RESUME_STATE_SIZE];
    ResumeState decoded;
    resume_state_encode(&original, bytes);
    CHECK(resume_state_decode(bytes, sizeof bytes, &decoded));
    CHECK_EQUAL(original.library_fingerprint, decoded.library_fingerprint);
    CHECK_EQUAL(original.plan.source.kind, decoded.plan.source.kind);
    CHECK_EQUAL(original.plan.source.group, decoded.plan.source.group);
    CHECK_EQUAL(original.plan.source.subject, decoded.plan.source.subject);
    CHECK_EQUAL(original.plan.source.artist, decoded.plan.source.artist);
    CHECK_EQUAL(original.plan.anchor_track, decoded.plan.anchor_track);
    CHECK_EQUAL(original.plan.shuffle, decoded.plan.shuffle);
    CHECK_EQUAL(original.plan.seed, decoded.plan.seed);
    CHECK_EQUAL(original.repeat, decoded.repeat);
    CHECK_EQUAL(original.volume, decoded.volume);
    CHECK_EQUAL(original.cursor.position, decoded.cursor.position);
    CHECK_EQUAL(original.cursor.album_track, decoded.cursor.album_track);
    CHECK_EQUAL(original.track, decoded.track);
    CHECK_EQUAL(original.position_ms, decoded.position_ms);
}

static void a_wrong_magic_is_rejected(void)
{
    ResumeState state = sample_state();
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    bytes[0] = 'X';
    rewrite_checksum(bytes);
    CHECK(!resume_state_decode(bytes, sizeof bytes, &state));
}

static void a_corrupted_byte_fails_the_checksum(void)
{
    ResumeState state = sample_state();
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    bytes[33] ^= 0x01;
    CHECK(!resume_state_decode(bytes, sizeof bytes, &state));
}

static void a_short_file_is_rejected(void)
{
    ResumeState state = sample_state();
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    CHECK(!resume_state_decode(bytes, RESUME_STATE_SIZE - 1, &state));
}

static void unknown_modes_are_rejected(void)
{
    ResumeState state = sample_state();
    uint8_t bytes[RESUME_STATE_SIZE];
    resume_state_encode(&state, bytes);
    bytes[SHUFFLE_FIELD] = 9;
    rewrite_checksum(bytes);
    CHECK(!resume_state_decode(bytes, sizeof bytes, &state));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_state_survives_a_round_trip),       TEST_CASE(a_wrong_magic_is_rejected),
        TEST_CASE(a_corrupted_byte_fails_the_checksum), TEST_CASE(a_short_file_is_rejected),
        TEST_CASE(unknown_modes_are_rejected),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
