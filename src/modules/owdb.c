#include "modules_util.h"

#include <owiz.h>
#include <machine/machine.h>
#include <objects/objmem.h>


#include <config/options.h>

#if OW_DEBUG_MEMORY

//# memory_usage()
//# Print current memory usage on standard output.
static int func_memory_usage(struct ow_machine *om) {
    ow_objmem_print_usage(om->objmem_context, NULL);
    return 0;
}

#endif // OW_DEBUG_MEMORY

static const struct ow_native_func_def functions[] = {
#if OW_DEBUG_MEMORY
    {"memory_usage", func_memory_usage, 0, 0},
#endif // OW_DEBUG_MEMORY
    {NULL, NULL, 0, 0},
};

OW_BIMOD_MODULE_DEF(owdb) = {
    .name = "owdb",
    .functions = functions,
    .finalizer = NULL,
};
