#include "nilobj.h"

#include <assert.h>

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>

struct ow_nil_obj {
    OW_OBJECT_HEAD
};

struct ow_nil_obj *_ow_nil_obj_new(struct ow_machine *om) {
    return ow_object_cast(
        ow_objmem_allocate_ex(om, OW_OBJMEM_ALLOC_SURV, om->builtin_classes->nil, 0),
        struct ow_nil_obj
    );
}

OW_BICLS_DEF_CLASS_EX(
    nil,
    "Nil",
    false,
    NULL,
    NULL,
)
