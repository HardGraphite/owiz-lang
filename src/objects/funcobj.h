#pragma once

#include <stddef.h>

#include "funcspec.h"
#include "object.h"
#include <utilities/attributes.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;
struct ow_symbol_obj;

struct ow_func_obj_constants;
struct ow_func_obj_symbols;

/// Function object.
struct ow_func_obj {
	OW_EXTENDED_OBJECT_HEAD
	struct ow_func_spec func_spec;
	struct ow_module_obj *module;
	const struct ow_func_obj_constants *constants;
	const struct ow_func_obj_symbols *symbols;
	size_t code_size;
	unsigned char code[];
};

/// Create a function.
struct ow_func_obj *ow_func_obj_new(
	struct ow_machine *om, struct ow_module_obj *mod,
	struct ow_object *constants[], size_t constant_count,
	struct ow_symbol_obj *symbols[], size_t symbol_count,
	unsigned char *code, size_t code_size,
	const struct ow_func_spec *spec);
/// Get constant by index. If the index is out of range, return NULL.
struct ow_object *ow_func_obj_get_constant(struct ow_func_obj *self, size_t index);
/// Get symbols by index. If the index is out of range, return NULL.
struct ow_symbol_obj *ow_func_obj_get_symbol(struct ow_func_obj *self, size_t index);
