#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <owiz.h>
#include "test_util.h"

static void make_random_data(owiz_machine_t *om, intmax_t seed) {
    const int N = 500;

    const int top = owiz_drop(om, 0);

    for (int i = 0; i < N; i++) {
        char buffer[64];
        snprintf(buffer, sizeof buffer, "<<<<<<<< No. %ji-%i >>>>>>>>", seed, i);
        owiz_push_nil(om); // #1
        owiz_push_bool(om, i & 1); // #2
        owiz_push_int(om, seed + i); // #3
        owiz_push_float(om, (double)(seed + i)); // #4
        owiz_push_string(om, buffer, (size_t)-1); // #5
        owiz_push_symbol(om, buffer, (size_t)-1); // #6
        owiz_make_tuple(om, 6);
    }
    owiz_make_array(om, (size_t)N);

    TEST_ASSERT_EQ(owiz_drop(om, 0), top + 1);
}

static void check_random_data(owiz_machine_t *om, intmax_t seed) {
    const int N = 500;

    const int top = owiz_drop(om, 0);

    TEST_ASSERT_EQ(owiz_read_array(om, 0, 0), (size_t)N);

    for (int i = 0; i < N; i++) {
        owiz_read_array(om, 0, i + 1);
        TEST_ASSERT_EQ(owiz_read_tuple(om, 0, -1), 6);
        const int index_base = top + 1;

        bool bool_val;
        intmax_t int_val;
        double float_val;
        const char *string_val;
        char buffer[64];
        snprintf(buffer, sizeof buffer, "<<<<<<<< No. %ji-%i >>>>>>>>", seed, i);
        TEST_ASSERT_EQ(owiz_read_nil(om, index_base + 1), 0); // #1
        TEST_ASSERT_EQ(owiz_read_bool(om, index_base + 2, &bool_val), 0); // #2
        TEST_ASSERT_EQ(bool_val, i & 1);
        TEST_ASSERT_EQ(owiz_read_int(om, index_base + 3, &int_val), 0); // #3
        TEST_ASSERT_EQ(int_val, seed + i);
        TEST_ASSERT_EQ(owiz_read_float(om, index_base + 4, &float_val), 0); // #4
        TEST_ASSERT_EQ(float_val, (double)(seed + i));
        string_val = NULL;
        TEST_ASSERT_EQ(owiz_read_string(om, index_base + 5, &string_val, NULL), 0); // #5
        TEST_ASSERT_EQ(strcmp(string_val, buffer), 0);
        string_val = NULL;
        TEST_ASSERT_EQ(owiz_read_symbol(om, index_base + 6, &string_val, NULL), 0); // #6
        TEST_ASSERT_EQ(strcmp(string_val, buffer), 0);

        TEST_ASSERT_EQ(owiz_drop(om, 6 + 1), top);
    }

    TEST_ASSERT_EQ(owiz_drop(om, 0), top);
}

static char long_str_buf[64 * 1024]; // Is it big enough to be allocated in big space?

static void make_random_large_object(owiz_machine_t *om, intmax_t seed) {
    if (!long_str_buf[0])
        memset(long_str_buf, ' ', sizeof long_str_buf);
    char buffer[64];
    snprintf(buffer, 64, "%063ji", seed);
    memcpy(long_str_buf, buffer, 63);
    owiz_push_string(om, long_str_buf, sizeof long_str_buf);
}

static void check_random_large_object(owiz_machine_t *om, intmax_t seed) {
    assert(long_str_buf[0]);
    char buffer[64];
    snprintf(buffer, 64, "%063ji", seed);
    memcpy(long_str_buf, buffer, 63);
    const char *s;
    size_t n;
    TEST_ASSERT_EQ(owiz_read_string(om, 0, &s, &n), 0);
    TEST_ASSERT_EQ(n, sizeof long_str_buf);
    TEST_ASSERT_EQ(memcmp(s, long_str_buf, sizeof long_str_buf), 0);
}

static void test_all_garbage(owiz_machine_t *om) {
    const int N = 10000;

    char buffer[1024];
    memset(buffer, ' ', sizeof buffer - 1);
    buffer[sizeof buffer - 1] = 0;

    const int top_base = owiz_drop(om, 0);
    (void)top_base;

    for (int i = 0; i < N; i++) {
        owiz_push_string(om, buffer, sizeof buffer - 1);
        owiz_drop(om, 1);
        assert(owiz_drop(om, 0) == top_base);
    }
}

static void test_massive_garbage(owiz_machine_t *om) {
    const intmax_t N = 100;

    const int top_base = owiz_drop(om, 0);
    (void)top_base;

    make_random_data(om, N);
    check_random_data(om, N);
    assert(owiz_drop(om, 0) == top_base + 1);

    for (intmax_t i = 0; i < N; i++) {
        make_random_data(om, i);
        owiz_drop(om, 1);
        assert(owiz_drop(om, 0) == top_base + 1);
    }

    check_random_data(om, N);
    owiz_drop(om, 1);
    assert(owiz_drop(om, 0) == top_base);
}

static void test_massive_survivors(owiz_machine_t *om) {
    const intmax_t N = 100, M = 8;

    const int top_base = owiz_drop(om, 0);
    (void)top_base;

    for (intmax_t j = 0; j < M; j++) {
        make_random_data(om, N - j);
        check_random_data(om, N - j);
    }
    assert(owiz_drop(om, 0) == top_base + M);

    for (intmax_t i = 0; i < N; i++) {
        for (intmax_t j = 0; j < M; j++)
            make_random_data(om, i);
        owiz_drop(om, (int)M);
        assert(owiz_drop(om, 0) == top_base + M);
    }

    for (intmax_t j = M - 1; j >= INTMAX_C(0); j--) {
        check_random_data(om, N - j);
        owiz_drop(om, 1);
    }
    assert(owiz_drop(om, 0) == top_base);
}

static void test_large_object(owiz_machine_t *om) {
    const intmax_t N = 200;

    const int top_base = owiz_drop(om, 0);
    (void)top_base;

    make_random_data(om, N);
    check_random_data(om, N);
    make_random_large_object(om, N);
    check_random_large_object(om, N);

    for (int i = 0; i < N; i++) {
        make_random_data(om, i);
        make_random_large_object(om, i);
        owiz_drop(om, 2);
        assert(owiz_drop(om, 0) == top_base + 2);
    }

    check_random_large_object(om, N);
    owiz_drop(om, 1);
    check_random_data(om, N);
    owiz_drop(om, 1);
    assert(owiz_drop(om, 0) == top_base);
}

static void test_complex_references(owiz_machine_t *om) {
    owiz_make_module(om, "TEST", NULL, OWIZ_MKMOD_EMPTY);
    const int module_index = owiz_drop(om, 0);

    owiz_push_float(om, 3.14);
    owiz_store_attribute(om, module_index, "pi");
    assert(owiz_drop(om, 0) == module_index);

    test_massive_garbage(om);

    double temp_float;
    TEST_ASSERT_EQ(owiz_load_attribute(om, module_index, "pi"), 0);
    TEST_ASSERT_EQ(owiz_read_float(om, 0, &temp_float), 0);
    TEST_ASSERT_EQ(temp_float, 3.14);

    owiz_drop(om, 2);
    assert(owiz_drop(om, 0) == module_index - 1);
}

int main(void) {
    // owiz_sysctl(OWIZ_CTL_VERBOSE, "M", 1);
    owiz_sysctl(OWIZ_CTL_STACKSIZE, &(size_t){64 * 1024}, sizeof(size_t));
    owiz_machine_t *om = owiz_create();
    test_all_garbage(om);
    test_massive_garbage(om);
    test_massive_survivors(om);
    test_large_object(om);
    test_complex_references(om);
    owiz_destroy(om);
}
