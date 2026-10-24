#include <stdbool.h>
#include <stdint.h>

#include "check.h"
#include "core/permutation.h"

enum {
    LARGEST = 65536,
    SMALL_SIZES = 300,
    ORDER_LENGTH = 40
};

static uint8_t seen[LARGEST];

static bool visits_each_index_once(uint32_t size, uint32_t seed)
{
    Permutation permutation = permutation_make(size, seed);
    for (uint32_t index = 0; index < size; index++) {
        seen[index] = 0;
    }
    for (uint32_t index = 0; index < size; index++) {
        uint32_t value = permutation_at(&permutation, index);
        if (value >= size || seen[value] != 0) {
            return false;
        }
        seen[value] = 1;
    }
    return true;
}

static void every_small_size_is_a_reordering(void)
{
    for (uint32_t size = 1; size <= SMALL_SIZES; size++) {
        CHECK(visits_each_index_once(size, size * 7 + 3));
    }
}

static void the_largest_list_is_a_reordering(void)
{
    CHECK(visits_each_index_once(LARGEST - 1, 99));
    CHECK(visits_each_index_once(LARGEST, 5));
}

static void the_same_seed_gives_the_same_order(void)
{
    Permutation first = permutation_make(ORDER_LENGTH, 11);
    Permutation second = permutation_make(ORDER_LENGTH, 11);
    for (uint32_t index = 0; index < ORDER_LENGTH; index++) {
        CHECK_EQUAL(permutation_at(&first, index), permutation_at(&second, index));
    }
}

static void different_seeds_give_different_orders(void)
{
    Permutation first = permutation_make(ORDER_LENGTH, 11);
    Permutation second = permutation_make(ORDER_LENGTH, 12345);
    int differences = 0;
    for (uint32_t index = 0; index < ORDER_LENGTH; index++) {
        differences += permutation_at(&first, index) != permutation_at(&second, index) ? 1 : 0;
    }
    CHECK(differences > ORDER_LENGTH / 2);
}

static void the_order_is_not_the_list_order(void)
{
    Permutation permutation = permutation_make(ORDER_LENGTH, 7);
    int in_place = 0;
    for (uint32_t index = 0; index < ORDER_LENGTH; index++) {
        in_place += permutation_at(&permutation, index) == index ? 1 : 0;
    }
    CHECK(in_place < ORDER_LENGTH / 4);
}

static void neighbours_rarely_stay_neighbours(void)
{
    Permutation permutation = permutation_make(ORDER_LENGTH * 100, 3);
    int kept = 0;
    for (uint32_t index = 0; index + 1 < ORDER_LENGTH * 100; index++) {
        uint32_t here = permutation_at(&permutation, index);
        uint32_t next = permutation_at(&permutation, index + 1);
        kept += next == here + 1 ? 1 : 0;
    }
    CHECK(kept < ORDER_LENGTH);
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(every_small_size_is_a_reordering),
        TEST_CASE(the_largest_list_is_a_reordering),
        TEST_CASE(the_same_seed_gives_the_same_order),
        TEST_CASE(different_seeds_give_different_orders),
        TEST_CASE(the_order_is_not_the_list_order),
        TEST_CASE(neighbours_rarely_stay_neighbours),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
