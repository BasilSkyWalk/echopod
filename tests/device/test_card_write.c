#include "check.h"
#include "device/card_write.h"

typedef enum CallKind {
    CALL_DATA,
    CALL_STOP,
    CALL_READY
} CallKind;

typedef struct Call {
    CallKind kind;
    uint8_t command;
    uint32_t address;
    uint32_t count;
    const uint32_t *words;
} Call;

typedef struct FakeCard {
    Call calls[8];
    int call_count;
    bool data_fails;
    bool stop_fails;
    bool ready_fails;
} FakeCard;

static void record(FakeCard *card, Call call)
{
    if (card->call_count < 8) {
        card->calls[card->call_count] = call;
    }
    card->call_count++;
}

static bool fake_write_data(void *context, uint8_t command, uint32_t address, uint32_t count,
                            const uint32_t *words)
{
    FakeCard *card = context;
    record(card, (Call){CALL_DATA, command, address, count, words});
    return !card->data_fails;
}

static bool fake_stop(void *context)
{
    FakeCard *card = context;
    record(card, (Call){CALL_STOP, 0, 0, 0, NULL});
    return !card->stop_fails;
}

static bool fake_wait_ready(void *context)
{
    FakeCard *card = context;
    record(card, (Call){CALL_READY, 0, 0, 0, NULL});
    return !card->ready_fails;
}

static FakeCard card;
static CardWriter writer;
static uint32_t words[4 * 128];

static void start(void)
{
    card = (FakeCard){.call_count = 0};
    writer = (CardWriter){fake_write_data, fake_stop, fake_wait_ready, &card};
}

static void one_block_is_a_single_write_then_a_ready_wait(void)
{
    start();
    CHECK(card_write(&writer, 0x1234, 1, words));
    CHECK_EQUAL(2, card.call_count);
    CHECK_EQUAL(CALL_DATA, card.calls[0].kind);
    CHECK_EQUAL(CARD_WRITE_SINGLE_BLOCK, card.calls[0].command);
    CHECK_EQUAL(0x1234, card.calls[0].address);
    CHECK_EQUAL(1, card.calls[0].count);
    CHECK(card.calls[0].words == words);
    CHECK_EQUAL(CALL_READY, card.calls[1].kind);
}

static void several_blocks_are_one_multiple_write_a_stop_and_a_ready_wait(void)
{
    start();
    CHECK(card_write(&writer, 0x40, 4, words));
    CHECK_EQUAL(3, card.call_count);
    CHECK_EQUAL(CALL_DATA, card.calls[0].kind);
    CHECK_EQUAL(CARD_WRITE_MULTIPLE_BLOCK, card.calls[0].command);
    CHECK_EQUAL(0x40, card.calls[0].address);
    CHECK_EQUAL(4, card.calls[0].count);
    CHECK(card.calls[0].words == words);
    CHECK_EQUAL(CALL_STOP, card.calls[1].kind);
    CHECK_EQUAL(CALL_READY, card.calls[2].kind);
}

static void a_failed_multiple_write_still_stops_and_waits_but_fails(void)
{
    start();
    card.data_fails = true;
    CHECK(!card_write(&writer, 0x40, 4, words));
    CHECK_EQUAL(3, card.call_count);
    CHECK_EQUAL(CALL_STOP, card.calls[1].kind);
    CHECK_EQUAL(CALL_READY, card.calls[2].kind);
}

static void a_failed_stop_skips_the_ready_wait_and_fails(void)
{
    start();
    card.stop_fails = true;
    CHECK(!card_write(&writer, 0x40, 4, words));
    CHECK_EQUAL(2, card.call_count);
    CHECK_EQUAL(CALL_STOP, card.calls[1].kind);
}

static void a_card_that_never_becomes_ready_fails_the_write(void)
{
    start();
    card.ready_fails = true;
    CHECK(!card_write(&writer, 0x40, 4, words));
    start();
    card.ready_fails = true;
    CHECK(!card_write(&writer, 0x40, 1, words));
}

static void a_failed_single_write_does_not_wait(void)
{
    start();
    card.data_fails = true;
    CHECK(!card_write(&writer, 0x40, 1, words));
    CHECK_EQUAL(1, card.call_count);
}

static void no_blocks_send_nothing(void)
{
    start();
    CHECK(card_write(&writer, 0x40, 0, words));
    CHECK_EQUAL(0, card.call_count);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(one_block_is_a_single_write_then_a_ready_wait),
        TEST_CASE(several_blocks_are_one_multiple_write_a_stop_and_a_ready_wait),
        TEST_CASE(a_failed_multiple_write_still_stops_and_waits_but_fails),
        TEST_CASE(a_failed_stop_skips_the_ready_wait_and_fails),
        TEST_CASE(a_card_that_never_becomes_ready_fails_the_write),
        TEST_CASE(a_failed_single_write_does_not_wait),
        TEST_CASE(no_blocks_send_nothing),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
