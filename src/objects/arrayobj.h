#pragma once

#include <stddef.h>

#include "object_util.h"
#include <utilities/attributes.h>

struct ow_array;
struct ow_machine;
struct ow_object;

/// Array object.
struct ow_array_obj;

/// Create an array object from a vector of elements.
/// If param `elem_count` is 0, the created array will be empty.
/// If param `elems` is NULL, the space for elements will be reserved.
struct ow_array_obj *ow_array_obj_new(
    struct ow_machine *om, struct ow_object *elems[], size_t elem_count);
/// Get the array data.
ow_static_inline struct ow_array *ow_array_obj_data(struct ow_array_obj *self);

////////////////////////////////////////////////////////////////////////////////

ow_static_inline struct ow_array *ow_array_obj_data(struct ow_array_obj *self) {
    return (struct ow_array *)((const unsigned char *)self + OW_OBJECT_HEAD_SIZE);
}
