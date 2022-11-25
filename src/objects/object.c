#include "object.h"

#include <assert.h>

#include "classes.h"
#include "classes_util.h"
#include "natives.h"
#include "object_util.h"

static_assert(
	sizeof(struct ow_object) ==
		(sizeof(struct ow_object_meta) + sizeof(struct ow_class_obj *)),
	"bad size of struct ow_object"
);

static_assert(OW_OBJECT_SIZE == sizeof(struct ow_object), "OW_OBJECT_SIZE is incorrect");

static const struct ow_native_func_def object_methods[] = {
		{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(object) = {
	.name      = "Object",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_object),
	.methods   = object_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
};
