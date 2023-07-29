#include "cfuncobj.h"

#include "classes.h"
#include "classes_util.h"
#include "classobj.h"
#include "objmem.h"
#include "object_util.h"
#include <machine/machine.h>

static void ow_cfunc_obj_gc_visitor(void *_obj, int op) {
    struct ow_cfunc_obj *const self = _obj;
    if (self->module)
        ow_objmem_visit_object(self->module, op);
}

struct ow_cfunc_obj *ow_cfunc_obj_new(
    struct ow_machine *om, struct ow_module_obj *module,
    const char *name, ow_native_func_t code, const struct ow_func_spec *spec
) {
    struct ow_cfunc_obj *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_SURV,
            om->builtin_classes->cfunc, 0
        ),
        struct ow_cfunc_obj
    );
    obj->func_spec = *spec;
    obj->module = module;
    obj->name = name;
    obj->code = code;
    ow_object_assert_no_write_barrier_2(obj, ow_object_from(module));
    return obj;
}

OW_BICLS_DEF_CLASS_EX(
    cfunc,
    "NativeFunc",
    false,
    NULL,
    ow_cfunc_obj_gc_visitor,
)
