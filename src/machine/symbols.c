#include "symbols.h"

#include <objects/objmem.h>
#include <objects/object.h>
#include <objects/symbolobj.h>
#include <utilities/memalloc.h>

#pragma pack(push, 1)

static const char *const sym_strings[] = {
#define ELEM(NAME, STRING) STRING,
    OW_COMSYM_LIST
#undef ELEM
};

#pragma pack(pop)

static void ow_common_symbols_gc_visitor(void *_ptr, int op) {
    struct ow_common_symbols *const cs = _ptr;
    for (size_t i = 0; i < sizeof *cs / sizeof(struct ow_symbol_obj *); i++)
        ow_objmem_visit_object(((struct ow_symbol_obj **)cs)[i], op);
}

struct ow_common_symbols *ow_common_symbols_new(struct ow_machine *om) {
    struct ow_common_symbols *const cs = ow_malloc(sizeof(struct ow_common_symbols));
    ow_objmem_push_ngc(om);
    for (size_t i = 0; i < sizeof *cs / sizeof(struct ow_symbol_obj *); i++)
        ((struct ow_symbol_obj **)cs)[i] = ow_symbol_obj_new(om, sym_strings[i], (size_t)-1);
    ow_objmem_add_gc_root(om, cs, ow_common_symbols_gc_visitor);
    ow_objmem_pop_ngc(om);
    return cs;
}

void ow_common_symbols_del(struct ow_machine *om, struct ow_common_symbols *cs) {
    ow_objmem_remove_gc_root(om, cs);
    ow_free(cs);
}
