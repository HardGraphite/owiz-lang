#pragma once

#include <stddef.h>

#include "funcspec.h"
#include "object_util.h"
#include <utilities/attributes.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;
struct ow_symbol_obj;

/// Function object.
struct ow_func_obj;

/// Create a function.
struct ow_func_obj *ow_func_obj_new(
	struct ow_machine *om, struct ow_module_obj *mod,
	struct ow_object *constants[], size_t constant_count,
	struct ow_symbol_obj *symbols[], size_t symbol_count,
	unsigned char *code, size_t code_size,
	struct ow_func_spec spec);
/// Get constant by index. If the index is out of range, return NULL.
struct ow_object *ow_func_obj_get_constant(struct ow_func_obj *self, size_t index);
/// Get symbols by index. If the index is out of range, return NULL.
struct ow_symbol_obj *ow_func_obj_get_symbol(struct ow_func_obj *self, size_t index);
/// Get code sequence.
const unsigned char *ow_func_obj_code(const struct ow_func_obj *self, size_t *size_out);
/// Get function specification.
ow_static_forceinline const struct ow_func_spec *ow_func_obj_func_spec(
	const struct ow_func_obj *self);

ow_static_forceinline const struct ow_func_spec *ow_func_obj_func_spec(
		const struct ow_func_obj *self) {
	return (const struct ow_func_spec *)
		((const unsigned char *)self + OW_OBJECT_SIZE + sizeof(size_t));
}
