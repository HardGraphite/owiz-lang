#pragma once

#include <stdbool.h>

struct ow_machine;

/// Bool object.
struct ow_bool_obj;

/// Create a bool object. There shall be only one true and one false object in a context.
struct ow_bool_obj *_ow_bool_obj_new(struct ow_machine *om, bool x);
