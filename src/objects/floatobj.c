#include "floatobj.h"

#include <assert.h>
#include <stddef.h>

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_float_obj {
	OW_OBJECT_HEAD
	double value;
};

struct ow_float_obj *ow_float_obj_new(struct ow_machine *om, double val) {
	struct ow_float_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->float_, 0),
		struct ow_float_obj);
	obj->value = val;
	assert(ow_float_obj_value(obj) == val);
	return obj;
}

static const struct ow_native_func_def float_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(float_) = {
	.name      = "Float",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_float_obj),
	.methods   = float_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};
