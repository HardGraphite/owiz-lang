#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "object.h"
#include <utilities/attributes.h>

struct ow_machine;
struct ow_object;

/// An array of objects. The capacity is fixed after creation.
struct ow_objslots_obj {
    OW_EXTENDED_OBJECT_HEAD
    struct ow_object *_slots[];
};

/// Create object slots. `len` is the capacity; `init_val` is the value to fill
/// in the slots when creating, which can be `NULL` to use `nil`.
struct ow_objslots_obj *ow_objslots_obj_new(
    struct ow_machine *om, size_t len, struct ow_object *init_val);

/// Get number of slots.
ow_static_forceinline size_t
ow_objslots_obj_length(const struct ow_objslots_obj * obj) {
    static_assert(
        offsetof(struct ow_objslots_obj, _slots) ==
            offsetof(struct ow_object, _fields) + sizeof(struct ow_object *) * 1,
        "_slots[i] == _fields[i + 1]"
    );
    assert(obj->field_count >= 1);
    return obj->field_count - 1;
}

/// Get object in slots by index. Return `NULL` if index is out of range.
ow_static_inline struct ow_object *
ow_objslots_obj_get(struct ow_objslots_obj *obj, size_t index) {
    if (ow_unlikely(index >= ow_objslots_obj_length(obj)))
        return NULL;
    return obj->_slots[index];
}

/// Put object in slots by index. Return false if index is out of range.
ow_static_inline bool ow_objslots_obj_set(
    struct ow_objslots_obj *obj, size_t index, struct ow_object *value
) {
    if (ow_unlikely(index + 1 >= obj->field_count))
        return false;
    obj->_slots[index] = value;
    ow_object_write_barrier(obj, value);
    return true;
}

/// An array of objects. The maximum capacity is fixed after creation.
struct ow_objslotz_obj {
    OW_EXTENDED_OBJECT_HEAD
    size_t _slots_count; // In-use slots
    struct ow_object *_slots[];
};

/// Create object slots. `max_len` is the maximum capacity; `init_len` is the
/// initial capacity when creating, which must not be greater than `max_len`;
/// `init_val` is the value to fill in the slots when creating, which can be
/// `NULL` to use `nil`.
struct ow_objslotz_obj *ow_objslotz_obj_new(
    struct ow_machine *om, size_t max_len,
    size_t init_len, struct ow_object *init_val);

/// Get maximum number of slots, including those not in use.
ow_static_forceinline size_t
ow_objslotz_obj_max_length(const struct ow_objslotz_obj * obj) {
    static_assert(
        offsetof(struct ow_objslotz_obj, _slots) ==
        offsetof(struct ow_object, _fields) + sizeof(struct ow_object *) * 2,
        "_slots[i] == _fields[i + 1]"
    );
    assert(obj->field_count >= 2);
    return obj->field_count - 2;
}

/// Get number of slots that are in use.
ow_static_forceinline size_t
ow_objslotz_obj_length(const struct ow_objslotz_obj * obj) {
    return obj->_slots_count;
}

/// Extend the available slots region to `length() + n`, which should not be
/// greater than the maximum capacity. Otherwise, return false and do nothing.
bool ow_objslotz_obj_extend(
    struct ow_objslotz_obj *obj, size_t n, struct ow_object *init_val);

/// Shrink the available slots region to `length() - n`, which should not be
/// less than zero. Otherwise, return false and do nothing.
bool ow_objslotz_obj_shrink(struct ow_objslotz_obj *obj, size_t n);

/// Get object in slots by index. Return `NULL` if index is out of range.
ow_static_inline struct ow_object *
ow_objslotz_obj_get(struct ow_objslotz_obj *obj, size_t index) {
    if (ow_unlikely(index >= ow_objslotz_obj_length(obj)))
        return NULL;
    return obj->_slots[index];
}

/// Put object in slots by index. Return false if index is out of range.
ow_static_inline bool ow_objslotz_obj_set(
    struct ow_objslotz_obj *obj, size_t index, struct ow_object *value
) {
    if (ow_unlikely(index + 1 >= obj->field_count))
        return false;
    obj->_slots[index] = value;
    ow_object_write_barrier(obj, value);
    return true;
}
