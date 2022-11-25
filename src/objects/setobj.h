#pragma once

struct ow_machine;
struct ow_object;

/// Set object.
struct ow_set_obj;

/// Create an empty set object.
struct ow_set_obj *ow_set_obj_new(struct ow_machine *om);
