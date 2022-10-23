#include "cfuncobj.h"

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_cfunc_obj *ow_cfunc_obj_new(
		struct ow_machine *om, const char *name, ow_native_func_t code) {
	struct ow_cfunc_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->cfunc, 0),
		struct ow_cfunc_obj);
	obj->name = name;
	obj->code = code;
	return obj;
}

static const struct ow_native_name_func_pair cfunc_methods[] = {
	{NULL, NULL},
};

OW_BICLS_CLASS_DEF_EX(cfunc) = {
	.name      = "NativeFunc",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_cfunc_obj),
	.methods   = cfunc_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
};
