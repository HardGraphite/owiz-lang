#include "mapobj.h"

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/hashmap.h>

struct ow_map_obj {
    OW_OBJECT_HEAD
    struct ow_hashmap map;
};

static void ow_map_obj_finalizer(struct ow_object *obj) {
    struct ow_map_obj *const self = ow_object_cast(obj, struct ow_map_obj);
    ow_hashmap_fini(&self->map);
}

static void ow_map_obj_gc_visitor(void *_obj, int op) {
    struct ow_map_obj *const self = _obj;
    ow_hashmap_foreach_1(&self->map, void *, key, void *, val, {
        (ow_unused_var(key), ow_unused_var(val));
        ow_objmem_visit_object(__node_p->key, op);
        ow_objmem_visit_object(__node_p->value, op);
    });
}

struct ow_map_obj *ow_map_obj_new(struct ow_machine *om) {
    struct ow_map_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->map),
        struct ow_map_obj);
    ow_hashmap_init(&obj->map, 0);
    return obj;
}

size_t ow_map_obj_length(const struct ow_map_obj *self) {
    return ow_hashmap_size(&self->map);
}

void ow_map_obj_set(
    struct ow_machine *om, struct ow_map_obj *self,
    struct ow_object *key, struct ow_object *val
) {
    struct ow_hashmap_funcs mf = OW_OBJECT_HASHMAP_FUNCS_INIT(om);
    ow_hashmap_set(&self->map, &mf, key, val);
    ow_object_write_barrier(self, key);
    ow_object_write_barrier(self, val);
}

struct ow_object *ow_map_obj_get(
    struct ow_machine *om, struct ow_map_obj *self, struct ow_object *key
) {
    struct ow_hashmap_funcs mf = OW_OBJECT_HASHMAP_FUNCS_INIT(om);
    return ow_hashmap_get(&self->map, &mf, key);
}

int ow_map_obj_foreach(
    const struct ow_map_obj *self,
    int (*walker)(void *arg, struct ow_object *key, struct ow_object *val), void *arg
) {
    return ow_hashmap_foreach(&self->map, (ow_hashmap_walker_t)walker, arg);
}

OW_BICLS_DEF_CLASS_EX(
    map,
    "Map",
    false,
    ow_map_obj_finalizer,
    ow_map_obj_gc_visitor,
)
