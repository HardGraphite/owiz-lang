#include "globals.h"

#include <objects/boolobj.h>
#include <objects/memory.h>
#include <objects/nilobj.h>
#include <objects/object.h>
#include <utilities/malloc.h>

struct ow_machine_globals *ow_machine_globals_new(struct ow_machine *om) {
	struct ow_machine_globals *const mg = ow_malloc(sizeof(struct ow_machine_globals));
	ow_objmem_push_ngc(om);
	mg->value_nil = ow_object_from(_ow_nil_obj_new(om));
	mg->value_true = ow_object_from(_ow_bool_obj_new(om, 1));
	mg->value_false = ow_object_from(_ow_bool_obj_new(om, 0));
	ow_objmem_pop_ngc(om);
	return mg;
}

void ow_machine_globals_del(struct ow_machine_globals *mg) {
	ow_free(mg);
}

void _ow_machine_globals_gc_marker(
		struct ow_machine *om, struct ow_machine_globals *mg) {
	ow_objmem_object_gc_marker(om, ow_object_from(mg->value_nil));
	ow_objmem_object_gc_marker(om, ow_object_from(mg->value_true));
	ow_objmem_object_gc_marker(om, ow_object_from(mg->value_false));
}
