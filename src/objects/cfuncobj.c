#include "cfuncobj.h"

#include "classes.h"
#include "classes_util.h"
#include "classobj.h"
#include "memory.h"
#include "object_util.h"
#include <machine/machine.h>

static void ow_cfunc_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
    ow_unused_var(om);
    assert(ow_class_obj_is_base(om->builtin_classes->cfunc, ow_object_class(obj)));
    struct ow_cfunc_obj *const self = ow_object_cast(obj, struct ow_cfunc_obj);
    if (self->module)
        ow_objmem_object_gc_marker(om, ow_object_from(self->module));
}

struct ow_cfunc_obj *ow_cfunc_obj_new(
    struct ow_machine *om, struct ow_module_obj *module,
    const char *name, ow_native_func_t code, const struct ow_func_spec *spec
) {
    struct ow_cfunc_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->cfunc, 0),
        struct ow_cfunc_obj);
    obj->func_spec = *spec;
    obj->module = module;
    obj->name = name;
    obj->code = code;
    return obj;
}

static const struct ow_native_func_def cfunc_methods[] = {
    {NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(cfunc) = {
    .name      = "NativeFunc",
    .data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_cfunc_obj),
    .methods   = cfunc_methods,
    .finalizer = NULL,
    .gc_marker = ow_cfunc_obj_gc_marker,
    .extended  = false,
};
