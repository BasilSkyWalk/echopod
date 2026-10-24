#include "check.h"
#include "device/sd_card.h"

enum {
    LOG_CAPACITY = 256,
    BUSY_TRIES = 3
};

typedef struct FakeCard {
    bool present;
    bool answers_cmd8;
    uint32_t cmd8_echo;
    bool high_capacity;
    int busy_tries;
    int zero_rca_answers;
    uint32_t csd[4];
    bool wide_bus;
    int log_count;
    SdCommand log[LOG_CAPACITY];
    SdClock clock;
    uint8_t bus_width;
    uint32_t delays_ms;
    bool stopped;
} FakeCard;

static bool answer(FakeCard *card, SdCommand command, uint32_t response[4])
{
    switch (command.index) {
    case 8:
        response[0] = card->cmd8_echo != 0 ? card->cmd8_echo : command.argument & 0xFFF;
        return card->answers_cmd8;
    case 41:
        if (card->busy_tries > 0) {
            card->busy_tries--;
            response[0] = 0x00FF8000;
        } else {
            response[0] = 0x80FF8000 | (card->high_capacity ? 0x40000000u : 0);
        }
        return true;
    case 3:
        response[0] = card->zero_rca_answers-- > 0 ? 0 : 0x12340500;
        return true;
    case 9:
        for (int index = 0; index < 4; index++) {
            response[index] = card->csd[index];
        }
        return true;
    case 6:
        return card->wide_bus;
    default:
        response[0] = 0x900;
        return true;
    }
}

static bool fake_command(void *context, SdCommand command, uint32_t response[4])
{
    FakeCard *card = context;
    if (card->log_count < LOG_CAPACITY) {
        card->log[card->log_count++] = command;
    }
    return card->present && answer(card, command, response);
}

static void fake_set_clock(void *context, SdClock clock)
{
    ((FakeCard *)context)->clock = clock;
}

static void fake_set_bus_width(void *context, uint8_t bits)
{
    ((FakeCard *)context)->bus_width = bits;
}

static void fake_delay(void *context, uint32_t milliseconds)
{
    ((FakeCard *)context)->delays_ms += milliseconds;
}

static bool fake_present(void *context)
{
    return ((FakeCard *)context)->present;
}

static void fake_stop(void *context)
{
    ((FakeCard *)context)->stopped = true;
}

static SdHost host_for(FakeCard *card)
{
    return (SdHost){fake_command, fake_set_clock, fake_set_bus_width,
                    fake_delay,   fake_present,   fake_stop,
                    card};
}

static void set_bits(uint32_t csd[4], int start, int size, uint32_t value)
{
    for (int bit = 0; bit < size; bit++) {
        int position = start + bit;
        if ((value >> bit) & 1u) {
            csd[position / 32] |= 1u << (position % 32);
        }
    }
}

static FakeCard sdhc_card(uint32_t c_size)
{
    FakeCard card = {.present = true,
                     .answers_cmd8 = true,
                     .high_capacity = true,
                     .busy_tries = BUSY_TRIES,
                     .wide_bus = true};
    set_bits(card.csd, 126, 2, 1);
    set_bits(card.csd, 48, 22, c_size);
    return card;
}

static FakeCard sdsc_v1_card(void)
{
    FakeCard card = {.present = true, .answers_cmd8 = false, .busy_tries = 1, .wide_bus = true};
    set_bits(card.csd, 80, 4, 9);
    set_bits(card.csd, 62, 12, 4095);
    set_bits(card.csd, 47, 3, 7);
    return card;
}

static const SdCommand *find_command(const FakeCard *card, uint8_t index, int occurrence)
{
    for (int position = 0; position < card->log_count; position++) {
        if (card->log[position].index == index && occurrence-- == 0) {
            return &card->log[position];
        }
    }
    return NULL;
}

static void an_sdhc_card_initialises_with_its_size_and_a_4_bit_bus(void)
{
    FakeCard card = sdhc_card(0x00EDC7);
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK(sd.high_capacity);
    CHECK_EQUAL(0x1234, sd.rca);
    CHECK_EQUAL((0x00EDC7 + 1) * 1024, sd.sector_count);
    CHECK_EQUAL(4, sd.bus_width);
    CHECK_EQUAL(4, card.bus_width);
    CHECK_EQUAL(SD_CLOCK_TRANSFER, card.clock);
}

static void the_commands_follow_the_sd_init_order(void)
{
    static const uint8_t expected[] = {0,  8, 55, 41, 55, 41, 55, 41, 55,
                                       41, 2, 3,  9,  7,  55, 42, 55, 6};
    FakeCard card = sdhc_card(100);
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK_EQUAL(sizeof expected, card.log_count);
    for (int index = 0; index < card.log_count && index < (int)sizeof expected; index++) {
        CHECK_EQUAL(expected[index], card.log[index].index);
    }
}

static void command_arguments_and_responses_match_the_facts_sheet(void)
{
    FakeCard card = sdhc_card(100);
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK(find_command(&card, 0, 0)->initialise);
    CHECK_EQUAL(SD_RESPONSE_NONE, find_command(&card, 0, 0)->response);
    CHECK_EQUAL(0x1AA, find_command(&card, 8, 0)->argument);
    CHECK_EQUAL(0x40FF8000, find_command(&card, 41, 0)->argument);
    CHECK_EQUAL(SD_RESPONSE_SHORT_NO_CRC, find_command(&card, 41, 0)->response);
    CHECK_EQUAL(SD_RESPONSE_LONG, find_command(&card, 2, 0)->response);
    CHECK_EQUAL(0x12340000, find_command(&card, 9, 0)->argument);
    CHECK_EQUAL(SD_RESPONSE_LONG, find_command(&card, 9, 0)->response);
    CHECK_EQUAL(0x12340000, find_command(&card, 7, 0)->argument);
    CHECK_EQUAL(SD_RESPONSE_SHORT_BUSY, find_command(&card, 7, 0)->response);
    CHECK_EQUAL(0, find_command(&card, 42, 0)->argument);
    CHECK_EQUAL(2, find_command(&card, 6, 0)->argument);
    CHECK(find_command(&card, 16, 0) == NULL);
}

static void the_card_is_asked_again_every_10_ms_while_busy(void)
{
    FakeCard card = sdhc_card(100);
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK(card.delays_ms >= BUSY_TRIES * 10);
}

static void a_v1_card_is_standard_capacity_with_a_block_length_set(void)
{
    FakeCard card = sdsc_v1_card();
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK(!sd.high_capacity);
    CHECK_EQUAL(0x00FF8000, find_command(&card, 41, 0)->argument);
    CHECK_EQUAL(512, find_command(&card, 16, 0)->argument);
    CHECK_EQUAL(4096u << 9, sd.sector_count);
}

static void standard_capacity_cards_take_byte_addresses(void)
{
    SdCard standard = {.high_capacity = false};
    SdCard high = {.high_capacity = true};
    CHECK_EQUAL(10 * 512, sd_card_address(&standard, 10));
    CHECK_EQUAL(10, sd_card_address(&high, 10));
}

static void a_zero_rca_is_asked_for_again(void)
{
    FakeCard card = sdhc_card(100);
    card.zero_rca_answers = 2;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK_EQUAL(0x1234, sd.rca);
    CHECK(find_command(&card, 3, 2) != NULL);
}

static void a_card_that_always_gives_a_zero_rca_fails(void)
{
    FakeCard card = sdhc_card(100);
    card.zero_rca_answers = 10;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(!sd_card_init(&sd, &host));
    CHECK(card.stopped);
}

static void a_card_without_a_wide_bus_stays_at_1_bit(void)
{
    FakeCard card = sdhc_card(100);
    card.wide_bus = false;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(sd_card_init(&sd, &host));
    CHECK_EQUAL(1, sd.bus_width);
    CHECK_EQUAL(0, card.bus_width);
}

static void a_card_that_stays_busy_fails_after_100_tries(void)
{
    FakeCard card = sdhc_card(100);
    card.busy_tries = 1000;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(!sd_card_init(&sd, &host));
    CHECK(find_command(&card, 41, 99) != NULL);
    CHECK(find_command(&card, 41, 100) == NULL);
    CHECK(card.stopped);
}

static void no_card_fails_without_sending_commands(void)
{
    FakeCard card = sdhc_card(100);
    card.present = false;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(!sd_card_init(&sd, &host));
    CHECK_EQUAL(0, card.log_count);
}

static void a_wrong_cmd8_echo_fails(void)
{
    FakeCard card = sdhc_card(100);
    card.cmd8_echo = 0x0AA;
    SdHost host = host_for(&card);
    SdCard sd;
    CHECK(!sd_card_init(&sd, &host));
    CHECK(find_command(&card, 41, 0) == NULL);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(an_sdhc_card_initialises_with_its_size_and_a_4_bit_bus),
        TEST_CASE(the_commands_follow_the_sd_init_order),
        TEST_CASE(command_arguments_and_responses_match_the_facts_sheet),
        TEST_CASE(the_card_is_asked_again_every_10_ms_while_busy),
        TEST_CASE(a_v1_card_is_standard_capacity_with_a_block_length_set),
        TEST_CASE(standard_capacity_cards_take_byte_addresses),
        TEST_CASE(a_zero_rca_is_asked_for_again),
        TEST_CASE(a_card_that_always_gives_a_zero_rca_fails),
        TEST_CASE(a_card_without_a_wide_bus_stays_at_1_bit),
        TEST_CASE(a_card_that_stays_busy_fails_after_100_tries),
        TEST_CASE(no_card_fails_without_sending_commands),
        TEST_CASE(a_wrong_cmd8_echo_fails),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
