#pragma once

#include <stddef.h>

struct ow_machine;

/// Function object.
struct ow_func_obj;

/// Create a function.
struct ow_func_obj *ow_func_obj_new(
	struct ow_machine *om, unsigned char *code, size_t code_size);
