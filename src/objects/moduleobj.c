#include "moduleobj.h"

#include "cfuncobj.h"
#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include "symbolobj.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/hashmap.h>

struct ow_module_obj {
	OW_OBJECT_HEAD
	struct ow_hashmap globals_map; // { name, index + 1 }
	struct ow_array globals;
	struct ow_symbol_obj *name; // Optional.
	void (*finalizer)(ow_machine_t *, struct ow_module_obj *);
};

static void ow_module_obj_init(struct ow_module_obj *self) {
	ow_hashmap_init(&self->globals_map, 0);
	ow_array_init(&self->globals, 0);
	self->name = NULL;
	self->finalizer = NULL;
}

static void ow_module_obj_fini(struct ow_module_obj *self) {
	ow_array_fini(&self->globals);
	ow_hashmap_fini(&self->globals_map);
}

static void ow_module_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->module, ow_object_class(obj)));
	struct ow_module_obj *const self = ow_object_cast(obj, struct ow_module_obj);
	if (self->finalizer)
		self->finalizer(om, self);
	ow_module_obj_fini(self);
}

static int _globals_map_gc_walker(void *ctx, const void *key, void *val) {
	struct ow_machine *const om = ctx;
	struct ow_object *const key_obj = (struct ow_object *)key;
	ow_unused_var(val);
	ow_objmem_object_gc_marker(om, key_obj);
	return 0;
}

static void ow_module_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->module, ow_object_class(obj)));
	struct ow_module_obj *const self = ow_object_cast(obj, struct ow_module_obj);

	ow_hashmap_foreach(&self->globals_map, _globals_map_gc_walker, om);
	for (size_t i = 0, n = ow_array_size(&self->globals); i < n; i++)
		ow_objmem_object_gc_marker(om, ow_array_at(&self->globals, i));
	if (ow_likely(self->name))
		ow_objmem_object_gc_marker(om, ow_object_from(self->name));
}

struct ow_module_obj *ow_module_obj_new(struct ow_machine *om) {
	struct ow_module_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->module, 0),
		struct ow_module_obj);
	ow_module_obj_init(obj);
	return obj;
}

void ow_module_obj_load_native_def(
		struct ow_machine *om, struct ow_module_obj *self,
		const struct ow_native_module_def *def) {
	assert(!ow_hashmap_size(&self->globals_map) && !ow_array_size(&self->globals));

	size_t func_count = 0;
	for (const ow_native_name_func_pair_t *p = def->functions; p->func; p++)
		func_count++;
	ow_hashmap_reserve(&self->globals_map, func_count);
	ow_array_reserve(&self->globals, func_count);

	ow_objmem_push_ngc(om);

	for (size_t i = 0; i < func_count; i++) {
		const ow_native_name_func_pair_t method_def = def->functions[i];
		struct ow_cfunc_obj *const func_obj =
			ow_cfunc_obj_new(om, method_def.name, method_def.func);
		struct ow_symbol_obj *const name_obj =
			ow_symbol_obj_new(om, method_def.name, (size_t)-1);
		ow_module_obj_set_global_y(self, name_obj, ow_object_from(func_obj));
	}

	ow_objmem_pop_ngc(om);

	self->name = def->name ? ow_symbol_obj_new(om, def->name, (size_t)-1) : NULL;
	self->finalizer = NULL; // TODO: Set finalizer.
	assert(!def->finalizer); // Not implemented.
}

void ow_module_obj_load_native_def_ex(
		struct ow_machine *om, struct ow_module_obj *self,
		const struct ow_native_module_def_ex *def) {
	ow_module_obj_load_native_def(om, self, (const struct ow_native_module_def *)def);
	self->finalizer = def->finalizer;
}

size_t ow_module_obj_find_global(
		const struct ow_module_obj *self, const struct ow_symbol_obj *name) {
	const uintptr_t index = (uintptr_t)ow_hashmap_get(
		&self->globals_map, ow_symbol_obj_hashmap_funcs, name);
	if (ow_unlikely(!index))
		return (size_t)-1;
	return (size_t)(index - 1);
}

struct ow_object *ow_module_obj_get_global(
		const struct ow_module_obj *self, size_t index) {
	if (ow_unlikely(index >= ow_array_size(&self->globals)))
		return NULL;
	return ow_array_at(&self->globals, index);
}

bool ow_module_obj_set_global(
		struct ow_module_obj *self, size_t index, struct ow_object *value) {
	if (ow_unlikely(index >= ow_array_size(&self->globals)))
		return false;
	ow_array_at(&self->globals, index) = value;
	return true;
}

size_t ow_module_obj_set_global_y(
		struct ow_module_obj *self,
		const struct ow_symbol_obj *name, struct ow_object *value) {
	size_t index = ow_module_obj_find_global(self, name);
	if (index == (size_t)-1) {
		index = ow_array_size(&self->globals);
		ow_array_append(&self->globals, value);
		ow_hashmap_set(
			&self->globals_map, ow_symbol_obj_hashmap_funcs,
			name, (void *)(index + 1));
	} else {
		ow_array_at(&self->globals, index) = value;
	}
	return index;
}

static const struct ow_native_name_func_pair module_methods[] = {
	{NULL, NULL},
};

OW_BICLS_CLASS_DEF_EX(module) = {
	.name      = "Module",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_module_obj),
	.methods   = module_methods,
	.finalizer = ow_module_obj_finalizer,
	.gc_marker = ow_module_obj_gc_marker,
};
