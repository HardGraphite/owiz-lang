#pragma once

#include "object_util.h"
#include <utilities/attributes.h>

struct ow_machine;

/// Floating-point object.
struct ow_float_obj;

/// Create a float object.
struct ow_float_obj *ow_float_obj_new(struct ow_machine *om, double val);
/// Get float value.
ow_static_forceinline double ow_float_obj_value(const struct ow_float_obj *self);

ow_static_forceinline double ow_float_obj_value(const struct ow_float_obj *self) {
    return *(const double *)((const unsigned char *)self + OW_OBJECT_SIZE);
}
