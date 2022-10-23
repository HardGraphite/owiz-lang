#include "funcobj.h"

#include <string.h>

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_func_obj {
	OW_EXTENDED_OBJECT_HEAD
	size_t code_size;
	unsigned char code[];
};

struct ow_func_obj *ow_func_obj_new(
		struct ow_machine *om, unsigned char *code, size_t code_size) {
	struct ow_func_obj *const obj = ow_object_cast(
		ow_objmem_allocate(
			om, om->builtin_classes->func,
			(ow_round_up_to(sizeof(void *), code_size) / sizeof(void *))),
		struct ow_func_obj);
	obj->code_size = code_size;
	memcpy(obj->code, code, code_size);
	return obj;
}

static const struct ow_native_name_func_pair func_methods[] = {
	{NULL, NULL},
};

OW_BICLS_CLASS_DEF_EX(func) = {
	.name      = "Func",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_func_obj),
	.methods   = func_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
};
