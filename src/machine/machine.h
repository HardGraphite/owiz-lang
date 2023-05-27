#pragma once

#include "stack.h"

struct ow_builtin_classes;
struct ow_machine_globals;
struct ow_module_manager;
struct ow_objmem_context;
struct ow_symbol_pool;

/// Ow context.
struct ow_machine {
    struct ow_objmem_context *objmem_context;
    struct ow_builtin_classes *builtin_classes;
    struct ow_symbol_pool *symbol_pool;
    struct ow_module_manager *module_manager;
    struct ow_common_symbols *common_symbols;
    struct ow_machine_globals *globals;
    struct ow_callstack callstack;
};

/// Jump buffer.
struct ow_machine_jmpbuf {
    void *sp, *fp, *fi;
};

/// Create a context.
struct ow_machine *ow_machine_new(void);
/// Destroy a context.
void ow_machine_del(struct ow_machine *om);

/// Store context.
void ow_machine_setjmp(struct ow_machine *om, struct ow_machine_jmpbuf *jb);
/// Jump back to stored context.
_Bool ow_machine_longjmp(struct ow_machine *om, struct ow_machine_jmpbuf *jb);

void _om_machine_gc_marker(struct ow_machine *om);
