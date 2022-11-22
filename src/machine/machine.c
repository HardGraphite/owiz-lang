#include "machine.h"

#include <assert.h>

#ifndef NDEBUG
#	include <string.h>
#endif // NDEBUG

#include "globals.h"
#include "invoke.h"
#include "modmgr.h"
#include "symbols.h"
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

	om->module_manager = ow_module_manager_new(om);
	om->common_symbols = ow_common_symbols_new(om);
	om->globals = ow_machine_globals_new(om);
	ow_callstack_init(&om->callstack, 500); // TODO: Configurable stack size.

	int status;
	status = ow_machine_run(om, om->globals->module_base, &(struct ow_object *){NULL});
	ow_unused_var(status);
	assert(!status);

	assert(!ow_objmem_test_ngc(om));

	return om;
}

void ow_machine_del(struct ow_machine *om) {
	assert(!ow_objmem_test_ngc(om));

	// The following lines are trying to delete all objects to avoid memory leaks.
	// But the operations are quite dangerous and are not allowed anywhere else.

	ow_callstack_clear(&om->callstack);
	ow_module_manager_del(om->module_manager);
	om->module_manager = NULL;
	ow_objmem_gc(om, 0);

	_ow_builtin_classes_cleanup(om, om->builtin_classes);
	ow_common_symbols_del(om->common_symbols);
	ow_machine_globals_del(om->globals);
	om->common_symbols = NULL;
	om->globals = NULL;
	ow_objmem_gc(om, 0);

	ow_symbol_pool_del(om->symbol_pool);
	_ow_builtin_classes_del(om, om->builtin_classes, true);
	ow_objmem_context_del(om->objmem_context);
	ow_callstack_fini(&om->callstack);

	ow_free(om);
}

void _om_machine_gc_marker(struct ow_machine *om) {
	_ow_builtin_classes_gc_marker(om, om->builtin_classes);
	if (ow_likely(om->module_manager))
		_ow_module_manager_gc_marker(om, om->module_manager);
	if (ow_likely(om->common_symbols))
		_ow_common_symbols_gc_marker(om, om->common_symbols);
	if (ow_likely(om->globals))
		_ow_machine_globals_gc_marker(om, om->globals);
	_ow_callstack_gc_marker(om, &om->callstack);
	_ow_symbol_pool_gc_handler(om, om->symbol_pool); // Must be called at last.
}
