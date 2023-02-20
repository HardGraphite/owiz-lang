#include "machine.h"

#include <assert.h>

#ifndef NDEBUG
#	include <string.h>
#endif // NDEBUG

#include "globals.h"
#include "invoke.h"
#include "modmgr.h"
#include "symbols.h"
#include "sysparam.h"
#include <objects/classes.h>
#include <objects/memory.h>
#include <objects/symbolobj.h>
#include <utilities/memalloc.h>

static size_t stack_size(void) {
	const size_t n_min = 64;
	const size_t n = ow_sysparam.stack_size;
	return n > n_min ? n : n_min;
}

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
	ow_callstack_init(&om->callstack, stack_size());

	if (ow_unlikely(ow_sysparam.verbose_memory))
		ow_objmem_context_verbose(om->objmem_context, true);

	int status;
	status = ow_machine_run(
		om, om->globals->module_base, false, &(struct ow_object *){NULL});
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

void ow_machine_setjmp(struct ow_machine *om, struct ow_machine_jmpbuf *jb) {
	jb->sp = om->callstack.regs.sp;
	jb->fp = om->callstack.regs.fp;
	jb->fi = om->callstack.frame_info_list.current;
}

bool ow_machine_longjmp(struct ow_machine *om, struct ow_machine_jmpbuf *jb) {
	if (!(
		jb->sp && jb->fp && jb->fi &&
		(struct ow_object **)jb->sp <= om->callstack.regs.sp &&
		(struct ow_object **)jb->fp <= om->callstack.regs.fp &&
		(struct ow_object **)jb->sp >= (struct ow_object **)jb->fp - 1
	)) {
		return false;
	}
	for (struct ow_callstack_frame_info *fi =
			om->callstack.frame_info_list.current; ; fi = fi->_next) {
		if (!fi)
			return false;
		if (fi == jb->fi)
			break;
	}

	om->callstack.regs.sp = jb->sp;
	om->callstack.regs.fp = jb->fp;
	while (om->callstack.frame_info_list.current != jb->fi) {
		assert(om->callstack.frame_info_list.current);
		ow_callstack_frame_info_list_leave(&om->callstack.frame_info_list);
	}

	return true;
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
