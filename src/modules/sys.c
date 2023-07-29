#include "modules_util.h"

#include <owiz.h>
#include <machine/machine.h>
#include <machine/modmgr.h>
#include <objects/object.h>
#include <objects/objmem.h>

//# path() :: Array[String]
//# Get the module path array.
static int func_path(struct ow_machine *om) {
    struct ow_module_manager *const mm = om->module_manager;
    struct ow_array_obj *const path_o = ow_module_manager_path(mm);
    *++om->callstack.regs.sp = ow_object_from(path_o);
    return 1;
}

//# add_path(path :: String) :: Bool
//# Try to add a module path.
static int func_add_path(struct ow_machine *om) {
    const char *path;
    if (owiz_read_args(om, OWIZ_RDARG_MKEXC, "s", &path, NULL) != 0)
        return -1;

    struct ow_module_manager *const mm = om->module_manager;
    const bool ok = ow_module_manager_add_path(mm, path);
    owiz_push_bool(om, ok);
    return 1;
}

//# gc()
//# Run garbage collection.
static int func_gc(struct ow_machine *om) {
    ow_objmem_gc(om, OW_OBJMEM_GC_AUTO);
    return 0;
}

static const struct ow_native_func_def functions[] = {
    {"path", func_path, 0, 0},
    {"add_path", func_add_path, 1, 0},
    {"gc", func_gc, 0, 0},
    {NULL, NULL, 0, 0},
};

OW_BIMOD_MODULE_DEF(sys) = {
    .name = "sys",
    .functions = functions,
    .finalizer = NULL,
};
