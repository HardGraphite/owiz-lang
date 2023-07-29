#include <assert.h>
#include <string.h>

#include <owiz.h>
#include "test_util.h"

static void test_create(void) {
    owiz_machine_t *const om = owiz_create();
    TEST_ASSERT_NE(om, NULL);
    owiz_destroy(om);
}

static void test_simple_values(owiz_machine_t *om) {
    int status;
    bool tmp_bool;
    int64_t tmp_int64;
    double tmp_double;
    size_t tmp_size;
    const char *tmp_char_p;
    char tmp_char_buf[64];

    const int64_t smallint_min = INTPTR_MIN / 2; // See "src/objects/smallint.h".

    assert(owiz_drop(om, 0) == 0);
    owiz_push_nil(om); // 1
    owiz_push_bool(om, true); // 2
    owiz_push_bool(om, false); // 3
    for (int i = 0; i < 100; i++)
        owiz_push_int(om, i - 50); // 4 ~ 103
    for (int i = 0; i < 100; i++)
        owiz_push_int(om, (int64_t)i + smallint_min - 50); // 104 ~ 203
    for (int i = 0; i < 100; i++)
        owiz_push_int(om, (int64_t)i + INT64_MAX - 100); // 204 ~ 303
    for (int i = 0; i < 17; i++)
        owiz_push_float(om, ((double)i - 7) / 8.0); // 304 ~ 320
    owiz_push_symbol(om, "print", (size_t)-1); // 321
    owiz_push_string(om, "Hello, world!!!", 13); // 322
    // TODO: Test strings with complicated structure.

    const int stack_depth = owiz_drop(om, 0);
    TEST_ASSERT_EQ(stack_depth, 322);

    status = owiz_read_nil(om, stack_depth + 1);
    TEST_ASSERT_EQ(status, OWIZ_ERR_INDEX);

    status = owiz_read_nil(om, 1);
    TEST_ASSERT_EQ(status, 0);

    for (int i = 2; i <= stack_depth; i++) {
        status = owiz_read_nil(om, i);
        TEST_ASSERT_EQ(status, OWIZ_ERR_TYPE);
    }

    status = owiz_read_bool(om, 2, &tmp_bool);
    TEST_ASSERT_EQ(status, 0);
    TEST_ASSERT_EQ(tmp_bool, true);

    status = owiz_read_bool(om, 3, &tmp_bool);
    TEST_ASSERT_EQ(status, 0);
    TEST_ASSERT_EQ(tmp_bool, false);

    for (int i = 0; i < 100; i++) {
        status = owiz_read_int(om, i + 4, &tmp_int64);
        TEST_ASSERT_EQ(status, 0);
        TEST_ASSERT_EQ(tmp_int64, (int64_t)(i - 50));
    }

    for (int i = 0; i < 100; i++) {
        status = owiz_read_int(om, i + 104, &tmp_int64);
        TEST_ASSERT_EQ(status, 0);
        TEST_ASSERT_EQ(tmp_int64, (int64_t)i + smallint_min - 50);
    }

    for (int i = 0; i < 100; i++) {
        status = owiz_read_int(om, i + 204, &tmp_int64);
        TEST_ASSERT_EQ(status, 0);
        TEST_ASSERT_EQ(tmp_int64, (int64_t)i + INT64_MAX - 100);
    }

    for (int i = 0; i < 17; i++) {
        status = owiz_read_float(om, i + 304, &tmp_double);
        TEST_ASSERT_EQ(status, 0);
        TEST_ASSERT_EQ(tmp_double, ((double)i - 7) / 8.0);
    }

    status = owiz_read_symbol(om, 321, &tmp_char_p, &tmp_size);
    TEST_ASSERT_EQ(status, 0);
    TEST_ASSERT_EQ(tmp_size, 5);
    TEST_ASSERT(!strcmp(tmp_char_p, "print"));

    status = owiz_read_string(om, 322, &tmp_char_p, &tmp_size);
    TEST_ASSERT_EQ(status, 0);
    TEST_ASSERT_EQ(tmp_size, 13);
    TEST_ASSERT(!strcmp(tmp_char_p, "Hello, world!"));

    status = owiz_read_string_to(om, 322, tmp_char_buf, sizeof tmp_char_buf);
    TEST_ASSERT_EQ(status, 13);
    TEST_ASSERT(!strcmp(tmp_char_buf, "Hello, world!"));

    status = owiz_read_string_to(om, 322, tmp_char_buf, 1);
    TEST_ASSERT_EQ(status, OWIZ_ERR_FAIL);

    owiz_drop(om, -1);
}

static void test_containers(owiz_machine_t *om) {
    assert(owiz_drop(om, 0) == 0);

    const int N = 100;
#define PREPARE_ELEMS \
    owiz_drop(om, -1); \
    for (int i = 0; i < N; i++) { \
        owiz_push_int(om, i); \
        owiz_dup(om, 1); \
    } \
    assert(owiz_drop(om, 0) == (N * 2)); \
// ^^^ PUSH_ELEMS ^^^

    PREPARE_ELEMS
    owiz_make_array(om, N * 2);
    TEST_ASSERT_EQ(owiz_drop(om, 0), 1);
    TEST_ASSERT_EQ(owiz_read_array(om, 0, 0), (size_t)(N * 2));
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            const size_t status1 = owiz_read_array(om, 0, (size_t)(i * 2 + j + 1));
            TEST_ASSERT_EQ(status1, 0);
            intmax_t elem_val;
            const int status2 = owiz_read_int(om, 0, &elem_val);
            TEST_ASSERT_EQ(status2, 0);
            TEST_ASSERT_EQ(elem_val, (intmax_t)i);
            owiz_drop(om, 1);
        }
    }
    owiz_read_array(om, 0, (size_t)-1);
    TEST_ASSERT_EQ(owiz_drop(om, 0), N * 2 + 1);

    PREPARE_ELEMS
    owiz_make_tuple(om, N * 2);
    TEST_ASSERT_EQ(owiz_drop(om, 0), 1);
    TEST_ASSERT_EQ(owiz_read_tuple(om, 0, 0), (size_t)(N * 2));
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < 2; j++) {
            const size_t status1 = owiz_read_tuple(om, 0, (size_t)(i * 2 + j + 1));
            TEST_ASSERT_EQ(status1, 0);
            intmax_t elem_val;
            const int status2 = owiz_read_int(om, 0, &elem_val);
            TEST_ASSERT_EQ(status2, 0);
            TEST_ASSERT_EQ(elem_val, (intmax_t)i);
            owiz_drop(om, 1);
        }
    }
    owiz_read_tuple(om, 0, (size_t)-1);
    TEST_ASSERT_EQ(owiz_drop(om, 0), N * 2 + 1);

    PREPARE_ELEMS
    owiz_make_set(om, N * 2);
    TEST_ASSERT_EQ(owiz_drop(om, 0), 1);
    TEST_ASSERT_EQ(owiz_read_set(om, 0, 0), (size_t)N);
    owiz_read_set(om, 0, -1);
    TEST_ASSERT_EQ(owiz_drop(om, 0), N + 1);

    PREPARE_ELEMS
    owiz_make_map(om, N);
    TEST_ASSERT_EQ(owiz_drop(om, 0), 1);
    TEST_ASSERT_EQ(owiz_read_map(om, 0, OWIZ_RDMAP_GETLEN), (size_t)N);
    for (int i = 0; i < N; i++) {
        owiz_push_int(om, i);
        const size_t status1 = owiz_read_map(om, 1, 2);
        TEST_ASSERT_EQ(status1, 0);
        intmax_t val;
        const int status2 = owiz_read_int(om, 0, &val);
        TEST_ASSERT_EQ(status2, 0);
        TEST_ASSERT_EQ(val, (intmax_t)i);
        owiz_drop(om, 2);
    }
    owiz_read_map(om, 0, OWIZ_RDMAP_EXPAND);
    TEST_ASSERT_EQ(owiz_drop(om, 0), N * 2 + 1);

#undef PREPARE_ELEMS
    owiz_drop(om, -1);
}

static void test_load_and_store(owiz_machine_t *om) {
    int status;
    int64_t tmp_int64;

    assert(owiz_drop(om, 0) == 0);

    for (int i = 1; i <= 16; i++)
        owiz_push_int(om, i);

    for (int i = 1; i <= 16; i++) {
        status = owiz_load_local(om, i);
        TEST_ASSERT_EQ(status, 0);
        owiz_read_int(om, 0, &tmp_int64);
        TEST_ASSERT_EQ(tmp_int64, (int64_t)i);
        status = owiz_store_local(om, 1);
        TEST_ASSERT_EQ(status, 0);
        owiz_read_int(om, 1, &tmp_int64);
        TEST_ASSERT_EQ(tmp_int64, (int64_t)i);
    }

    status = owiz_load_local(om, 17);
    TEST_ASSERT_EQ(status, OWIZ_ERR_INDEX);

    status = owiz_store_local(om, 17);
    TEST_ASSERT_EQ(status, OWIZ_ERR_INDEX);

    TEST_ASSERT_EQ(owiz_drop(om, 0), 16);
    owiz_drop(om, -1);
}

int main(void) {
    test_create();
    owiz_machine_t *const om = owiz_create();
    test_simple_values(om);
    test_containers(om);
    test_load_and_store(om);
    owiz_destroy(om);
}
