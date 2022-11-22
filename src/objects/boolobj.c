#include "boolobj.h"

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_bool_obj {
	OW_OBJECT_HEAD
	bool value;
};

struct ow_bool_obj *_ow_bool_obj_new(struct ow_machine *om, bool x) {
	struct ow_bool_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->bool_, 0),
		struct ow_bool_obj);
	obj->value = x;
	return obj;
}

static const struct ow_native_func_def bool_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(bool_) = {
	.name      = "Bool",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_bool_obj),
	.methods   = bool_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
};
