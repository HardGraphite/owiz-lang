#include "nilobj.h"

#include <assert.h>

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_nil_obj {
	OW_OBJECT_HEAD
};

struct ow_nil_obj *_ow_nil_obj_new(struct ow_machine *om) {
	return ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->nil, 0),
		struct ow_nil_obj);
}

static const struct ow_native_func_def nil_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(nil) = {
	.name      = "Nil",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_nil_obj),
	.methods   = nil_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};
