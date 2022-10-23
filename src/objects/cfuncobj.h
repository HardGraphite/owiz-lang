#pragma once

#include "natives.h"
#include "object.h"

struct ow_machine;

/// C function object.
struct ow_cfunc_obj {
	OW_OBJECT_HEAD
	const char *name;
	ow_native_func_t code;
};

/// Create a cfunc object.
struct ow_cfunc_obj *ow_cfunc_obj_new(
	struct ow_machine *om, const char *name, ow_native_func_t code);
