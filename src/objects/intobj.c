#include "intobj.h"

#include <assert.h>
#include <stddef.h>

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_int_obj {
    OW_OBJECT_HEAD
    int64_t value;
};

struct ow_int_obj *_ow_int_obj_new(struct ow_machine *om, int64_t val) {
    struct ow_int_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->int_),
        struct ow_int_obj);
    obj->value = val;
    assert(ow_int_obj_value(obj) == val);
    return obj;
}

OW_BICLS_DEF_CLASS_EX_1(
    int_,
    int,
    "Int",
    false,
    NULL,
    NULL,
)
