#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "test_util.h"
#include "utilities/bitset.h"

static void mem_fill_zero(void *mem, size_t mem_len) {
    memset(mem, 0, mem_len);
}

static void mem_fill_one(void *mem, size_t mem_len) {
    memset(mem, 0xff, mem_len);
}

static bool mem_all_zero(const void *mem, size_t mem_len) {
    const char *mem1 = (const char *)mem;
    for (size_t i = 0; i < mem_len; i++) {
        if (mem1[i])
            return false;
    }
    return true;
}

static bool mem_all_one(const void *mem, size_t mem_len) {
    const unsigned char *mem1 = (const unsigned char *)mem;
    for (size_t i = 0; i < mem_len; i++) {
        if (mem1[i] != 0xff)
            return false;
    }
    return true;
}

static bool num_in_array(size_t num, const size_t array[], size_t array_len) {
    for (size_t i = 0; i < array_len; i++) {
        if (num == array[i])
            return true;
    }
    return false;
}

static void test_bitset_clear(void) {
    char data[ow_bitset_required_size(256) * 2];
    const size_t data_size = sizeof data;
    const size_t half_data_size = data_size / 2;
    mem_fill_one(data, data_size);

    struct ow_bitset *const bitset = (struct ow_bitset *)data;
    const size_t bitset_size = half_data_size;
    ow_bitset_clear(bitset, bitset_size);

    TEST_ASSERT(mem_all_zero(data, half_data_size)); // First half is cleared.
    TEST_ASSERT(mem_all_one(data + half_data_size, half_data_size)); // Second half is untouched.
}

static void test_bitset_read_and_modify(void) {
    char data[3][ow_bitset_required_size(256)];
    struct ow_bitset *const bitset = (struct ow_bitset *)data[1];
    const size_t bitset_length = 256;
    mem_fill_one(data, sizeof data);

    for (size_t i = 0; i < bitset_length; i++) {
        mem_fill_zero(data[1], sizeof data[1]);

        ow_bitset_set_bit(bitset, i);
        TEST_ASSERT(!mem_all_zero(data[1], sizeof data[1]));

        for (size_t j = 0; j < bitset_length; j++) {
            const bool bit_is_set = i == j;
            TEST_ASSERT_EQ(ow_bitset_test_bit(bitset, j), bit_is_set);
        }

        ow_bitset_reset_bit(bitset, i);
        TEST_ASSERT(mem_all_zero(data[1], sizeof data[1]));

        ow_bitset_try_set_bit(bitset, i);
        TEST_ASSERT(!mem_all_zero(data[1], sizeof data[1]));

        for (size_t j = 0; j < bitset_length; j++) {
            const bool bit_is_set = i == j;
            TEST_ASSERT_EQ(ow_bitset_test_bit(bitset, j), bit_is_set);
        }

        ow_bitset_try_reset_bit(bitset, i);
        TEST_ASSERT(mem_all_zero(data[1], sizeof data[1]));
    }
}

static void test_bitset_foreach(void) {
    char data[ow_bitset_required_size(256)];
    struct ow_bitset *const bitset = (struct ow_bitset *)data;
    const size_t bitset_size = sizeof data;

    const size_t bit_indices[] = {0, 1, 2, 4, 8, 25, 100, 254, 255};
    const size_t bit_indices_len = sizeof bit_indices / sizeof bit_indices[0];

    ow_bitset_clear(bitset, bitset_size);
    for (size_t i = 0; i < bit_indices_len; i++)
        ow_bitset_set_bit(bitset, bit_indices[i]);

    size_t count = 0;
    ow_bitset_foreach_set(bitset, bitset_size, index, {
        TEST_ASSERT(num_in_array(index, bit_indices, bit_indices_len));
        count++;
    });
    TEST_ASSERT_EQ(count, bit_indices_len);
}

int main(void) {
    test_bitset_clear();
    test_bitset_read_and_modify();
    test_bitset_foreach();
}
