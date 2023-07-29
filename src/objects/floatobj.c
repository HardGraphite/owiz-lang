#include "floatobj.h"

#include <assert.h>
#include <stddef.h>

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_float_obj {
    OW_OBJECT_HEAD
    double value;
};

struct ow_float_obj *ow_float_obj_new(struct ow_machine *om, double val) {
    struct ow_float_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->float_),
        struct ow_float_obj
    );
    obj->value = val;
    assert(ow_float_obj_value(obj) == val);
    return obj;
}

OW_BICLS_DEF_CLASS_EX_1(
    float_,
    float,
    "Float",
    false,
    NULL,
    NULL,
)
