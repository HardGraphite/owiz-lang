#include "classobj.h"

#include <assert.h>

#include "cfuncobj.h"
#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include "symbolobj.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/attributes.h>
#include <utilities/hashmap.h>
#include <utilities/round.h>

struct ow_class_obj {
	OW_OBJECT_HEAD
	struct _ow_class_obj_pub_info pub_info;
	struct ow_hashmap attrs_and_methods_map; // { name, field_index_or_method_index }
	struct ow_hashmap statics_map; // { name, static_member_object }
	struct ow_array methods;
	void (*finalizer2)(struct ow_machine *, void *);
};

static void ow_class_obj_init(struct ow_class_obj *self) {
	self->pub_info.basic_field_count = 0;
	self->pub_info.super_class = 0;
	self->pub_info.class_name = 0;
	self->pub_info.finalizer = NULL;
	self->pub_info.gc_marker = NULL;
	ow_hashmap_init(&self->attrs_and_methods_map, 0);
	ow_hashmap_init(&self->statics_map, 0);
	ow_array_init(&self->methods, 0);
	self->finalizer2 = NULL;
	assert(_ow_class_obj_pub_info(self) == &self->pub_info);
}

void _ow_class_obj_fini(struct ow_class_obj *self) {
	ow_array_fini(&self->methods);
	ow_hashmap_fini(&self->statics_map);
	ow_hashmap_fini(&self->attrs_and_methods_map);
}

static void ow_class_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->class_, ow_object_class(obj)));
	struct ow_class_obj *const self = ow_object_cast(obj, struct ow_class_obj);
	_ow_class_obj_fini(self);
}

static int _attrs_and_methods_map_gc_walker(void *ctx, const void *key, void *val) {
	struct ow_machine *const om = ctx;
	struct ow_object *const key_obj = (struct ow_object *)key;
	ow_unused_var(val);
	ow_objmem_object_gc_marker(om, key_obj);
	return 0;
}

static int _statics_map_gc_walker(void *ctx, const void *key, void *val) {
	struct ow_machine *const om = ctx;
	struct ow_object *const key_obj = (struct ow_object *)key;
	struct ow_object *const val_obj = val;
	ow_objmem_object_gc_marker(om, key_obj);
	ow_objmem_object_gc_marker(om, val_obj);
	return 0;
}

static void ow_class_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	assert(ow_class_obj_is_base(om->builtin_classes->class_, ow_object_class(obj)));
	struct ow_class_obj *const self = ow_object_cast(obj, struct ow_class_obj);

	if (ow_likely(self->pub_info.class_name))
		ow_objmem_object_gc_marker(om, ow_object_from(self->pub_info.class_name));
	if (ow_likely(self->pub_info.super_class))
		ow_objmem_object_gc_marker(om, ow_object_from(self->pub_info.super_class));
	ow_hashmap_foreach(&self->attrs_and_methods_map, _attrs_and_methods_map_gc_walker, om);
	ow_hashmap_foreach(&self->statics_map, _statics_map_gc_walker, om);
	for (size_t i = 0, n = ow_array_size(&self->methods); i < n; i++)
		ow_objmem_object_gc_marker(om, ow_array_at(&self->methods, i));
}

struct ow_class_obj *ow_class_obj_new(struct ow_machine *om) {
	struct ow_class_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->class_, 0),
		struct ow_class_obj);
	ow_class_obj_init(obj);
	return obj;
}

static void _finalizer_wrapper(struct ow_machine *om, struct ow_object *obj) {
	assert(obj->_class->finalizer2);
	obj->_class->finalizer2(om, (unsigned char *)obj + OW_OBJECT_SIZE);
}

void ow_class_obj_load_native_def(
		struct ow_machine *om, struct ow_class_obj *self,
		struct ow_class_obj *super, const struct ow_native_class_def *def) {
	assert(!ow_hashmap_size(&self->attrs_and_methods_map) && !ow_array_size(&self->methods));
	assert(!self->finalizer2 || super == om->builtin_classes->object);

	self->pub_info.super_class = super;
	if (ow_likely(def->name))
		self->pub_info.class_name = ow_symbol_obj_new(om, def->name, (size_t)-1);
	if (def->finalizer) {
		self->pub_info.finalizer = _finalizer_wrapper;
		self->finalizer2 = def->finalizer;
	} else {
		self->pub_info.finalizer = NULL;
		self->finalizer2 = NULL;
	}
	self->pub_info.gc_marker = NULL;

	const size_t field_count =
		ow_round_up_to(OW_OBJECT_FIELD_SIZE, def->data_size) / OW_OBJECT_FIELD_SIZE;
	size_t method_count = 0;
	for (const ow_native_name_func_pair_t *p = def->methods; p->func; p++)
		method_count++;

	size_t total_field_count = field_count;
	size_t total_method_count = method_count;
	if (ow_likely(super)) {
		total_field_count += super->pub_info.basic_field_count;

		const size_t super_method_count = ow_array_size(&super->methods);
		total_method_count += super_method_count;
	}

	ow_array_reserve(&self->methods, total_method_count);
	ow_hashmap_reserve(
		&self->attrs_and_methods_map, total_field_count + total_method_count);

	if (ow_likely(super)) {
		ow_hashmap_extend(
			&self->attrs_and_methods_map, ow_symbol_obj_hashmap_funcs,
			&super->attrs_and_methods_map);
		ow_array_extend(&self->methods, &super->methods);
	}

	ow_objmem_push_ngc(om);

	for (size_t i = 0; i < method_count; i++) {
		const ow_native_name_func_pair_t method_def = def->methods[i];
		struct ow_cfunc_obj *const func_obj =
			ow_cfunc_obj_new(om, method_def.name, method_def.func);
		struct ow_symbol_obj *const name_obj =
			ow_symbol_obj_new(om, method_def.name, (size_t)-1);
		ow_class_obj_set_method_y(self, name_obj, ow_object_from(func_obj));
	}

	ow_objmem_pop_ngc(om);

	self->pub_info.native_field_count = total_field_count;
	self->pub_info.basic_field_count =
		self->pub_info.native_field_count +
		ow_hashmap_size(&self->attrs_and_methods_map) - ow_array_size(&self->methods);

	if (ow_unlikely(self->methods._cap - self->methods._len > self->methods._len / 8))
		ow_array_shrink(&self->methods);
	if (ow_unlikely(self->attrs_and_methods_map._size
			> self->attrs_and_methods_map._bucket_count + 4))
		ow_hashmap_shrink(&self->attrs_and_methods_map);
}

void ow_class_obj_load_native_def_ex(
	struct ow_machine *om, struct ow_class_obj *self,
	struct ow_class_obj *super,	const struct ow_native_class_def_ex *def) {
	ow_class_obj_load_native_def(om, self, super, (const struct ow_native_class_def *)def);
	self->finalizer2 = NULL;
	self->pub_info.finalizer = def->finalizer;
	self->pub_info.gc_marker = def->gc_marker;
}

void ow_class_obj_clear(struct ow_machine *om, struct ow_class_obj *self) {
	ow_unused_var(om);
	self->pub_info.basic_field_count = 0;
	self->pub_info.native_field_count = 0;
	self->pub_info.super_class = 0;
	self->pub_info.class_name = 0;
	self->pub_info.finalizer = NULL;
	self->pub_info.gc_marker = NULL;
	ow_hashmap_clear(&self->attrs_and_methods_map);
	ow_hashmap_clear(&self->statics_map);
	ow_array_clear(&self->methods);
	self->finalizer2 = NULL;
}

size_t ow_class_obj_find_attribute(
		const struct ow_class_obj *self, const struct ow_symbol_obj *name) {
	const intptr_t index = (intptr_t)ow_hashmap_get(
		&self->attrs_and_methods_map, ow_symbol_obj_hashmap_funcs, name);
	if (ow_unlikely(index < 0))
		return (size_t)-1;
	return (size_t)index;
}

size_t ow_class_obj_find_method(
		const struct ow_class_obj *self, const struct ow_symbol_obj *name) {
	const intptr_t index = (intptr_t)ow_hashmap_get(
		&self->attrs_and_methods_map, ow_symbol_obj_hashmap_funcs, name);
	if (ow_unlikely(index >= 0))
		return (size_t)-1;
	return (size_t)(-1 - index);
}

struct ow_object *ow_class_obj_get_method(
		const struct ow_class_obj *self, size_t index) {
	if (ow_unlikely(index >= ow_array_size(&self->methods)))
		return NULL;
	return ow_array_at(&self->methods, index);
}

bool ow_class_obj_set_method(
		struct ow_class_obj *self, size_t index, struct ow_object *method) {
	if (ow_unlikely(index >= ow_array_size(&self->methods)))
		return false;
	ow_array_at(&self->methods, index) = method;
	return true;
}

size_t ow_class_obj_set_method_y(
		struct ow_class_obj *self,
		const struct ow_symbol_obj *name, struct ow_object *method) {
	size_t index = ow_class_obj_find_method(self, name);
	if (index == (size_t)-1) {
		index = ow_array_size(&self->methods);
		ow_array_append(&self->methods, method);
		ow_hashmap_set(
			&self->attrs_and_methods_map, ow_symbol_obj_hashmap_funcs,
			name, (void *)(-1 - (intptr_t)index));
	} else {
		ow_array_at(&self->methods, index) = method;
	}
	return index;
}

struct ow_object *ow_class_obj_get_static(
		const struct ow_class_obj *self, const struct ow_symbol_obj *name) {
	return ow_hashmap_get(&self->statics_map, ow_symbol_obj_hashmap_funcs, name);
}

void ow_class_obj_set_static(
		struct ow_class_obj *self,
		const struct ow_symbol_obj *name, struct ow_object *val) {
	ow_hashmap_set(&self->statics_map, ow_symbol_obj_hashmap_funcs, name, val);
}

static const struct ow_native_name_func_pair class_methods[] = {
	{NULL, NULL},
};

OW_BICLS_CLASS_DEF_EX(class_) = {
	.name      = "Class",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_class_obj),
	.methods   = class_methods,
	.finalizer = ow_class_obj_finalizer,
	.gc_marker = ow_class_obj_gc_marker,
};
