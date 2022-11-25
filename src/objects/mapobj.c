#include "mapobj.h"

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/hashmap.h>

struct ow_map_obj {
	OW_OBJECT_HEAD
	struct ow_hashmap map;
};

static void ow_map_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->map, ow_object_class(obj)));
	struct ow_map_obj *const self = ow_object_cast(obj, struct ow_map_obj);
	ow_hashmap_fini(&self->map);
}

static int _ow_map_obj_gc_marker_walker(void *arg, const void *key, void *val) {
	struct ow_machine *const om = arg;
	ow_objmem_object_gc_marker(om, (struct ow_object *)key);
	ow_objmem_object_gc_marker(om, val);
	return 0;
}

static void ow_map_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->map, ow_object_class(obj)));
	struct ow_map_obj *const self = ow_object_cast(obj, struct ow_map_obj);
	ow_hashmap_foreach(&self->map, _ow_map_obj_gc_marker_walker, om);
}

struct ow_map_obj *ow_map_obj_new(struct ow_machine *om) {
	struct ow_map_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->map, 0),
		struct ow_map_obj);
	ow_hashmap_init(&obj->map, 0);
	return obj;
}

static const struct ow_native_func_def map_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(map) = {
	.name      = "Map",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_map_obj),
	.methods   = map_methods,
	.finalizer = ow_map_obj_finalizer,
	.gc_marker = ow_map_obj_gc_marker,
};
