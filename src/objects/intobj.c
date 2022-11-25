#include "intobj.h"

#include <assert.h>
#include <stddef.h>

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_int_obj {
	OW_OBJECT_HEAD
	int64_t value;
};

struct ow_int_obj *_ow_int_obj_new(struct ow_machine *om, int64_t val) {
	struct ow_int_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->int_, 0),
		struct ow_int_obj);
	obj->value = val;
	assert(ow_int_obj_value(obj) == val);
	return obj;
}

static const struct ow_native_func_def int_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(int_) = {
	.name      = "Int",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_int_obj),
	.methods   = int_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
};
