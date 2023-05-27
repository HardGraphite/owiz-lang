#pragma once

#include <stdbool.h>

#include "object_util.h"
#include <utilities/attributes.h>

struct ow_machine;

/// Bool object.
struct ow_bool_obj;

/// Create a bool object. There shall be only one true and one false object in a context.
struct ow_bool_obj *_ow_bool_obj_new(struct ow_machine *om, bool x);
/// Get bool value.
ow_static_forceinline bool ow_bool_obj_value(const struct ow_bool_obj *self);

ow_static_forceinline bool ow_bool_obj_value(const struct ow_bool_obj *self) {
    return *(const bool *)((const unsigned char *)self + OW_OBJECT_SIZE);
}
