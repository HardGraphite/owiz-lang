#pragma once

struct ow_machine;
struct ow_object;

/// Map object.
struct ow_map_obj;

/// Create an empty map object.
struct ow_map_obj *ow_map_obj_new(struct ow_machine *om);
