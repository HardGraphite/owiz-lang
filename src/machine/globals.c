#include "globals.h"

#include <assert.h>

#include "machine.h"
#include "modmgr.h"
#include <objects/boolobj.h>
#include <objects/objmem.h>
#include <objects/nilobj.h>
#include <objects/object.h>
#include <utilities/memalloc.h>

static void ow_machine_globals_gc_visitor(void *_ptr, int op) {
    struct ow_machine_globals *const mg = _ptr;
    for (size_t i = 0; i < sizeof *mg / sizeof(struct ow_symbol_obj *); i++)
        ow_objmem_visit_object(((struct ow_symbol_obj **)mg)[i], op);
}

struct ow_machine_globals *ow_machine_globals_new(struct ow_machine *om) {
    struct ow_machine_globals *const mg = ow_malloc(sizeof(struct ow_machine_globals));
    ow_objmem_push_ngc(om);
    mg->value_nil = ow_object_from(_ow_nil_obj_new(om));
    mg->value_true = ow_object_from(_ow_bool_obj_new(om, 1));
    mg->value_false = ow_object_from(_ow_bool_obj_new(om, 0));
    mg->module_base = ow_module_manager_load(om->module_manager, "__base__", 0, NULL);
    mg->module_sys = ow_module_manager_load(om->module_manager, "sys", 0, NULL);
    ow_objmem_add_gc_root(om, mg, ow_machine_globals_gc_visitor);
    ow_objmem_pop_ngc(om);
    assert(mg->module_base);
    assert(mg->module_sys);
    return mg;
}

void ow_machine_globals_del(struct ow_machine *om, struct ow_machine_globals *mg) {
    ow_objmem_remove_gc_root(om, mg);
    ow_free(mg);
}
