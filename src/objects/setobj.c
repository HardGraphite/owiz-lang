#include "setobj.h"

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/hashmap.h>

struct ow_set_obj {
	OW_OBJECT_HEAD
	struct ow_hashmap data; // {object, NULL}
};

static void ow_set_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->set, ow_object_class(obj)));
	struct ow_set_obj *const self = ow_object_cast(obj, struct ow_set_obj);
	ow_hashmap_fini(&self->data);
}

static int _ow_set_obj_gc_marker_walker(void *arg, const void *key, void *val) {
	struct ow_machine *const om = arg;
	ow_objmem_object_gc_marker(om, (struct ow_object *)key);
	ow_unused_var(val);
	assert(val == NULL);
	return 0;
}

static void ow_set_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->set, ow_object_class(obj)));
	struct ow_set_obj *const self = ow_object_cast(obj, struct ow_set_obj);
	ow_hashmap_foreach(&self->data, _ow_set_obj_gc_marker_walker, om);
}

struct ow_set_obj *ow_set_obj_new(struct ow_machine *om) {
	struct ow_set_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->set, 0),
		struct ow_set_obj);
	ow_hashmap_init(&obj->data, 0);
	return obj;
}

void ow_set_obj_insert(
		struct ow_machine *om, struct ow_set_obj *self,	struct ow_object *val) {
	struct ow_hashmap_funcs mf = OW_OBJECT_HASHMAP_FUNCS_INIT(om);
	ow_hashmap_set(&self->data, &mf, val, NULL);
}

size_t ow_set_obj_length(const struct ow_set_obj *self) {
	return ow_hashmap_size(&self->data);
}

struct _ow_set_obj_foreach_walker_wrapper_arg {
	int (*walker)(void *, struct ow_object *);
	void *walker_arg;
};

static int _ow_set_obj_foreach_walker_wrapper(
		void *_arg, const void *key, void *val) {
	struct _ow_set_obj_foreach_walker_wrapper_arg *const arg = _arg;
	ow_unused_var(val);
	assert(val == NULL);
	return arg->walker(arg->walker_arg, (void *)key);
}

int ow_set_obj_foreach(
		const struct ow_set_obj *self,
		int (*walker)(void *arg, struct ow_object *elem), void *arg) {
	return ow_hashmap_foreach(
		&self->data, _ow_set_obj_foreach_walker_wrapper,
		&(struct _ow_set_obj_foreach_walker_wrapper_arg){walker, arg});
}

static const struct ow_native_func_def set_methods[] = {
	{NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(set) = {
	.name      = "Set",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_set_obj),
	.methods   = set_methods,
	.finalizer = ow_set_obj_finalizer,
	.gc_marker = ow_set_obj_gc_marker,
	.extended  = false,
};
