#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h> // uintptr_t

#include "objmeta.h"
#include "smallint.h"
#include <utilities/attributes.h>

struct ow_class_obj;
struct ow_object;

void ow_objmem_record_o2y_object(struct ow_object *); // Declared in "objmem.h"

/// Common head of any object struct.
#define OW_OBJECT_HEAD \
    struct ow_object_meta _meta; \
// ^^^ OW_OBJECT_HEAD ^^^

/// Common head of object structs that has extra fields.
#define OW_EXTENDED_OBJECT_HEAD \
    OW_OBJECT_HEAD       \
    size_t field_count;  \
// ^^^ OW_EXTENDED_OBJECT_HEAD ^^^

/// Object.
struct ow_object {

    /*
     * ## Layout
     *
     * +--------+
     * | `HEAD` |
     * +--------+  ---  ---
     * |        |   ^    ^
     * | native |   |    |
     * | fields |   |    |
     * |        | basic  |
     * +--------+ fields |
     * | non-   |   |    |
     * | native |   | `_fields`
     * | fields |   |    |
     * |        |   v    |
     * +--------+  ---   |
     * |        |   ^    |
     * |extended| extra  |
     * | region | fields |
     * |        |   v    v
     * +--------+  ---  ---
     *
     * ## Terms
     *
     * - FIELD -- A slot to store data in object. The size of a field is `sizeof(void *)`.
     * - NON-NATIVE FILED -- A `struct ow_object *` member variable.
     * - NATIVE FILED -- A field that holds native data, which may not be an object pointer.
     * - BASIC FIELDS -- Native or non-native fields of a fixed sized object.
     * - EXTRA FIELDS -- Extended region of an object. Not all classes allow extra fields.
     * - NATIVE CLASS -- Classes whose instances have native fields.
     *
     * ## Notes
     *
     * - Pure native classes cannot inherit class other than `Object`. Classes
     *  inherited from pure native classes can only add non-native fields.
     * - Classes allow extra fields are not inheritable.
     * - Classes allow extra fields must be native class.
     * - Objects that can have extra fields shall store the number of fields
     *  in the first field as a `size_t` value.
     * - Normally, do not access fields directly (because of write barrier).
     *  use `ow_object_write_barrier()` where it is required otherwise.
     *
     */

    OW_OBJECT_HEAD
    struct ow_object *_fields[];
};

/*
 * ## Small int as object pointer
 *
 * A `struct ow_object *` variable does not always hold a pointer to an object.
 * If the LSB of a `struct ow_object *` variable `x` is `1`, then it actually holds
 * a small int, and its values is `(intptr_t)x >> 1`. The header file "objmeta.h"
 * provides utilities for small int judgement and conversions.
 */

/// Convert from object struct pointer to `struct ow_object *`.
#define ow_object_from(obj_ptr) \
    ((struct ow_object *)(obj_ptr))

/// Convert from `struct ow_object *` to other object struct pointer.
#define ow_object_cast(obj_ptr, type) \
    ((type *)(obj_ptr))

/// Object write barrier. Place this after where a value is stored into an object.
/// Function `ow_object_set_field()` already uses such barrier.
#define ow_object_write_barrier(__obj, __val) \
    do {                                      \
        if (ow_unlikely(ow_object_meta_test_(OLD, (__obj)->_meta))) { \
            if (!ow_smallint_check(ow_object_from((__val))) &&        \
                !ow_object_meta_test_(OLD, (__val)->_meta))           \
                ow_objmem_record_o2y_object(ow_object_from((__obj))); \
        }                                     \
    } while (0)                               \
// ^^^ ow_object_write_barrier() ^^^

/// Object write barrier for an array of values. See `ow_object_write_barrier()`.
ow_static_forceinline void ow_object_write_barrier_n(
    struct ow_object *obj, struct ow_object *val_arr[], size_t var_cnt
) {
    if (!ow_object_meta_test_(OLD, obj->_meta))
        return;
    for (size_t i = 0; i < var_cnt; i++) {
        struct ow_object *const val = val_arr[i];
        if (!ow_smallint_check(val) && !ow_object_meta_test_(OLD, val->_meta)) {
            ow_objmem_record_o2y_object(obj);
            return;
        }
    }
}

/// Assert that no write barrier is needed.
#define ow_object_assert_no_write_barrier(__obj) \
    assert(!ow_object_meta_test_(OLD, (__obj)->_meta))

/// Assert that no write barrier is needed.
#define ow_object_assert_no_write_barrier_2(__obj, __val) \
    assert(                                      \
        !ow_object_meta_test_(OLD, (__obj)->_meta) || \
        ow_smallint_check((__val)) ||            \
        ow_object_meta_test_(OLD, (__val)->_meta)\
    )                                            \
// ^^^ ow_object_assert_no_write_barrier_2() ^^^

/// Get class of an object.
ow_static_forceinline struct ow_class_obj *
ow_object_class(const struct ow_object *obj) {
    assert(!ow_smallint_check(obj));
    return ow_object_meta_load_(CLS, struct ow_class_obj *, obj->_meta);
}

/// Get field by index. No bounds checking.
ow_static_forceinline struct ow_object *
ow_object_get_field(const struct ow_object *obj, size_t index) {
    assert(!ow_smallint_check(obj));
    return obj->_fields[index];
}

/// Set field by index with write barrier. No bounds checking.
ow_static_forceinline void ow_object_set_field(
    struct ow_object *obj,
    size_t index, struct ow_object *value
) {
    assert(!ow_smallint_check(obj));
    obj->_fields[index] = value;
    ow_object_write_barrier(obj, value);
}

/// Template of hash map functions for hash maps that use objects as keys,
/// whose `context` field shall be filled with current ow_machine struct.
extern const struct ow_hashmap_funcs _ow_object_hashmap_funcs_tmpl;

/// Initializer for a `struct ow_hashmap_funcs` for hash maps that use objects as keys.
#define OW_OBJECT_HASHMAP_FUNCS_INIT(OM_PTR) \
    { \
        .key_equal = _ow_object_hashmap_funcs_tmpl.key_equal, \
        .key_hash = _ow_object_hashmap_funcs_tmpl.key_hash, \
        .context = (OM_PTR), \
    } \
// ^^^ OW_OBJECT_HASHMAP_FUNCS_INIT ^^^
