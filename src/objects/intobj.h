#pragma once

#include <stdint.h>

#include "object.h"
#include "object_util.h"
#include "smallint.h"
#include <utilities/attributes.h>

struct ow_machine;
struct ow_object;

/// Integer object
struct ow_int_obj;

/// Make a small int or create an int object.
ow_static_forceinline struct ow_object *ow_int_obj_or_smallint(struct ow_machine *om, int64_t val);
/// Create an int object.
struct ow_int_obj *_ow_int_obj_new(struct ow_machine *om, int64_t val);
/// Get int value.
ow_static_forceinline int64_t ow_int_obj_value(struct ow_int_obj *self);

ow_static_forceinline struct ow_object *ow_int_obj_or_smallint(
    struct ow_machine *om, int64_t val
) {
    if (ow_likely(val >= OW_SMALLINT_MIN && val <= OW_SMALLINT_MAX))
        return ow_smallint_to_ptr(val);
    return ow_object_from(_ow_int_obj_new(om, val));
}

ow_static_forceinline int64_t ow_int_obj_value(struct ow_int_obj *self) {
    return *(const int64_t *)((const unsigned char *)self + OW_OBJECT_SIZE);
}
