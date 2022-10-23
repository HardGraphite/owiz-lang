#pragma once

struct ow_machine;

/// Nil object.
struct ow_nil_obj;

/// Create a nil object. There shall be only one nil object in a context.
struct ow_nil_obj *_ow_nil_obj_new(struct ow_machine *om);
