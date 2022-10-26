#pragma once

#include <stdbool.h>
#include <stddef.h>

struct ow_machine;
struct ow_native_module_def;
struct ow_native_module_def_ex;
struct ow_object;
struct ow_symbol_obj;

/// Module object.
struct ow_module_obj;

/// Create an empty module.
struct ow_module_obj *ow_module_obj_new(struct ow_machine *om);
/// Initialize module object from a native definition. The module must be empty.
void ow_module_obj_load_native_def(
	struct ow_machine *om, struct ow_module_obj *self,
	const struct ow_native_module_def *def);
/// Initialize module object from a native definition. The module must be empty.
void ow_module_obj_load_native_def_ex(
	struct ow_machine *om, struct ow_module_obj *self,
	const struct ow_native_module_def_ex *def);
/// Get global index by name. If not exists, return -1.
size_t ow_module_obj_find_global(
	const struct ow_module_obj *self, const struct ow_symbol_obj *name);
/// Get global by index. If not exists, return NULL.
struct ow_object *ow_module_obj_get_global(
	const struct ow_module_obj *self, size_t index);
/// Set global by index. If not exists, return false.
bool ow_module_obj_set_global(
	struct ow_module_obj *self, size_t index, struct ow_object *value);
/// Set or add global by name. Return its index.
size_t ow_module_obj_set_global_y(
	struct ow_module_obj *self, const struct ow_symbol_obj *name, struct ow_object *value);
