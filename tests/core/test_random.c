#include "check.h"
#include "core/random.h"

enum {
    SEQUENCE_LENGTH = 8,
    DRAWS = 600,
    FACES = 6
};

static void the_same_seed_gives_the_same_sequence(void)
{
    Random first = random_seeded(42);
    Random second = random_seeded(42);
    for (int draw = 0; draw < SEQUENCE_LENGTH; draw++) {
        CHECK_EQUAL(random_next(&first), random_next(&second));
    }
}

static void different_seeds_give_different_sequences(void)
{
    Random first = random_seeded(1);
    Random second = random_seeded(2);
    int differences = 0;
    for (int draw = 0; draw < SEQUENCE_LENGTH; draw++) {
        differences += random_next(&first) != random_next(&second) ? 1 : 0;
    }
    CHECK(differences > 0);
}

static void a_zero_seed_still_produces_numbers(void)
{
    Random random = random_seeded(0);
    CHECK(random_next(&random) != 0);
}

static void bounded_draws_stay_below_the_bound_and_cover_it(void)
{
    Random random = random_seeded(7);
    int seen[FACES] = {0};
    for (int draw = 0; draw < DRAWS; draw++) {
        uint32_t face = random_below(&random, FACES);
        CHECK(face < FACES);
        if (face < FACES) {
            seen[face]++;
        }
    }
    for (int face = 0; face < FACES; face++) {
        CHECK(seen[face] > 0);
    }
}

static void a_bound_of_one_always_gives_zero(void)
{
    Random random = random_seeded(9);
    CHECK_EQUAL(0, random_below(&random, 1));
    CHECK_EQUAL(0, random_below(&random, 0));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(the_same_seed_gives_the_same_sequence),
        TEST_CASE(different_seeds_give_different_sequences),
        TEST_CASE(a_zero_seed_still_produces_numbers),
        TEST_CASE(bounded_draws_stay_below_the_bound_and_cover_it),
        TEST_CASE(a_bound_of_one_always_gives_zero),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
