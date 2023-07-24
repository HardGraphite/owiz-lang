#include "objslots.h"

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include <machine/globals.h>
#include <machine/machine.h>

/*===== ow_objslots_obj ======================================================*/

static void ow_objslots_obj_gc_visitor(void *_obj, int op) {
    struct ow_objslots_obj *const obj = _obj;
    for (size_t i = 0, n = ow_objslots_obj_length(obj); i < n; i++)
        ow_objmem_visit_object(obj->_slots[i], op);
}

struct ow_objslots_obj *ow_objslots_obj_new(
    struct ow_machine *om, size_t len, struct ow_object *init_val
) {
    struct ow_objslots_obj *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->objslots, len
        ),
        struct ow_objslots_obj
    );
    if (!init_val)
        init_val = om->globals->value_nil;
    for (size_t i = 0; i < len; i++)
        obj->_slots[i] = init_val;
    ow_object_assert_no_write_barrier_2(obj, init_val);
    return obj;
}

OW_BICLS_DEF_CLASS_EX(
    objslots,
    "Slots",
    true,
    NULL,
    ow_objslots_obj_gc_visitor,
)

/*===== ow_objslotz_obj ======================================================*/

static void ow_objslotz_obj_gc_visitor(void *_obj, int op) {
    struct ow_objslotz_obj *const obj = _obj;
    for (size_t i = 0, n = ow_objslotz_obj_length(obj); i < n; i++)
        ow_objmem_visit_object(obj->_slots[i], op);
}

struct ow_objslotz_obj *ow_objslotz_obj_new(
    struct ow_machine *om, size_t max_len,
    size_t init_len, struct ow_object *init_val
) {
    struct ow_objslotz_obj *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->objslotz, max_len
        ),
        struct ow_objslotz_obj
    );
    obj->_slots_count = init_len;
    if (!init_val)
        init_val = om->globals->value_nil;
    for (size_t i = 0; i < init_len; i++)
        obj->_slots[i] = init_val;
    ow_object_assert_no_write_barrier_2(obj, init_val);
    return obj;
}

bool ow_objslotz_obj_extend(
    struct ow_objslotz_obj *obj, size_t n, struct ow_object *init_val
) {
    const size_t old_slots_count = obj->_slots_count;
    const size_t new_slots_count = old_slots_count + n;
    if (ow_unlikely(new_slots_count > ow_objslotz_obj_max_length(obj)))
        return false;
    obj->_slots_count = new_slots_count;
    for (size_t i = old_slots_count; i < new_slots_count; i++)
        obj->_slots[i] = init_val;
    ow_object_write_barrier(obj, init_val);
    return true;
}

/// Shrink the available slots region to `length() - n`, which should not be
/// less than zero. Otherwise, return false and do nothing.
bool ow_objslotz_obj_shrink(struct ow_objslotz_obj *obj, size_t n) {
    const size_t new_slots_count = obj->_slots_count - n;
    if (ow_unlikely(new_slots_count > obj->_slots_count))
        return false;
    obj->_slots_count = new_slots_count;
    return true;
}

OW_BICLS_DEF_CLASS_EX(
    objslotz,
    "Slotz",
    true,
    NULL,
    ow_objslotz_obj_gc_visitor,
)
