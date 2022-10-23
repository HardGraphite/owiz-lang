#include "machine.h"

#ifndef NDEBUG
#	include <string.h>
#endif // NDEBUG

#include "globals.h"
#include <objects/classes.h>
#include <objects/memory.h>
#include <objects/symbolobj.h>
#include <utilities/malloc.h>

struct ow_machine *ow_machine_new(void) {
	struct ow_machine *const om = ow_malloc(sizeof(struct ow_machine));

#ifndef NDEBUG
	memset(om, 0, sizeof *om);
#endif // NDEBUG

	om->objmem_context = ow_objmem_context_new();
	om->builtin_classes = _ow_builtin_classes_new(om);
	om->symbol_pool = ow_symbol_pool_new();
	_ow_builtin_classes_setup(om, om->builtin_classes);

	om->globals = ow_machine_globals_new(om);

	assert(!ow_objmem_test_ngc(om));

	return om;
}

void ow_machine_del(struct ow_machine *om) {
	assert(!ow_objmem_test_ngc(om));

	// The following lines are trying to delete all objects to avoid memory leaks.
	// But the operations are quite dangerous and are not allowed anywhere else.

	ow_objmem_gc(om, 0);

	_ow_builtin_classes_cleanup(om, om->builtin_classes);
	ow_machine_globals_del(om->globals);
	om->globals = NULL;
	ow_objmem_gc(om, 0);

	ow_symbol_pool_del(om->symbol_pool);
	_ow_builtin_classes_del(om, om->builtin_classes, true);
	ow_objmem_context_del(om->objmem_context);

	ow_free(om);
}

void _om_machine_gc_marker(struct ow_machine *om) {
	_ow_builtin_classes_gc_marker(om, om->builtin_classes);
	if (ow_likely(om->globals))
		_ow_machine_globals_gc_marker(om, om->globals);
	_ow_symbol_pool_gc_handler(om, om->symbol_pool); // Must be called at last.
}
