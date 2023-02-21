#include "arrayobj.h"

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/array.h>

struct ow_array_obj {
	OW_OBJECT_HEAD
	struct ow_array array;
};

static void ow_array_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->array, ow_object_class(obj)));
	struct ow_array_obj *const self = ow_object_cast(obj, struct ow_array_obj);
	ow_array_fini(&self->array);
}

static void ow_array_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->array, ow_object_class(obj)));
	struct ow_array_obj *const self = ow_object_cast(obj, struct ow_array_obj);
	for (size_t i = 0, n = ow_array_size(&self->array); i < n; i++)
		ow_objmem_object_gc_marker(om, ow_array_at(&self->array, i));
}

struct ow_array_obj *ow_array_obj_new(
		struct ow_machine *om, struct ow_object *elems[], size_t elem_count) {
	struct ow_array_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->array, 0),
		struct ow_array_obj);
	ow_array_init(&obj->array, elem_count);
	if (elems) {
		for (size_t i = 0; i < elem_count; i++)
			ow_array_append(&obj->array, elems[i]);
	}
	assert(ow_array_obj_data(obj) == &obj->array);
	return obj;
}

static const struct ow_native_func_def array_methods[] = {
	{NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(array) = {
	.name      = "Array",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_array_obj),
	.methods   = array_methods,
	.finalizer = ow_array_obj_finalizer,
	.gc_marker = ow_array_obj_gc_marker,
	.extended  = false,
};
