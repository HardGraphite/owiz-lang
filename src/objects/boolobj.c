#include "boolobj.h"

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
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
        ow_objmem_allocate_ex(om, OW_OBJMEM_ALLOC_SURV, om->builtin_classes->bool_, 0),
        struct ow_bool_obj
    );
    obj->value = x;
    assert(ow_bool_obj_value(obj) == x);
    return obj;
}

OW_BICLS_DEF_CLASS_EX_1(
    bool_,
    bool,
    "Bool",
    false,
    NULL,
    NULL,
)
