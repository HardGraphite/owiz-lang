#include "arrayobj.h"

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/array.h>

struct ow_array_obj {
    OW_OBJECT_HEAD
    struct ow_array array;
};

static void ow_array_obj_finalizer(struct ow_object *obj) {
    struct ow_array_obj *const self = ow_object_cast(obj, struct ow_array_obj);
    ow_array_fini(&self->array);
}

static void ow_array_obj_gc_visitor(void *_obj, int op) {
    struct ow_array_obj *const self = _obj;
    for (size_t i = 0, n = ow_array_size(&self->array); i < n; i++)
        ow_objmem_visit_object(ow_array_at(&self->array, i), op);
}

struct ow_array_obj *ow_array_obj_new(
    struct ow_machine *om, struct ow_object *elems[], size_t elem_count
) {
    struct ow_array_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->array),
        struct ow_array_obj
    );
    ow_array_init(&obj->array, elem_count);
    if (elems) {
        for (size_t i = 0; i < elem_count; i++)
            ow_array_append(&obj->array, elems[i]);
        ow_object_assert_no_write_barrier(obj);
    }
    assert(ow_array_obj_data(obj) == &obj->array);
    return obj;
}

OW_BICLS_DEF_CLASS_EX(
    array,
    "Array",
    false,
    ow_array_obj_finalizer,
    ow_array_obj_gc_visitor,
)
