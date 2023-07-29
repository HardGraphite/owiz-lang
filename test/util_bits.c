#include <stdint.h>

#include "test_util.h"
#include "utilities/bits.h"

static void test_bits_count_tz_u32(void) {
    for (unsigned int i = 1; i < 32; i++) {
        unsigned int result;
        result = ow_bits_count_tz(UINT32_C(1) << i);
        TEST_ASSERT_EQ(result, i);
        result = ow_bits_count_tz((UINT32_C(1) << i) | (UINT32_C(1) << 31));
        TEST_ASSERT_EQ(result, i);
    }
}

static void test_bits_count_tz_u64(void) {
    for (unsigned int i = 1; i < 64; i++) {
        unsigned int result;
        result = ow_bits_count_tz(UINT64_C(1) << i);
        TEST_ASSERT_EQ(result, i);
        result = ow_bits_count_tz((UINT64_C(1) << i) | (UINT64_C(1) << 63));
        TEST_ASSERT_EQ(result, i);
    }
}

int main(void) {
    test_bits_count_tz_u32();
    test_bits_count_tz_u64();
}
