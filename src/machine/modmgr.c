#include "modmgr.h"

#include <string.h>

#include <modules/modules_util.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <utilities/hashmap.h>
#include <utilities/malloc.h>
#include <utilities/strings.h>

#include <config/definitions.h>
#include <config/module_list.h>

struct ow_module_manager {
	struct ow_hashmap modules; // {name, module}
	struct ow_module_obj *temp_module; // Nullable.
	struct ow_machine *machine;
};

struct ow_module_manager *ow_module_manager_new(struct ow_machine *om) {
	struct ow_module_manager *const mm = ow_malloc(sizeof(struct ow_module_manager));
	ow_hashmap_init(&mm->modules, 8);
	mm->temp_module = NULL;
	mm->machine = om;
	return mm;
}

static int mm_del_walker(void *ctx, const void *key, void *val) {
	ow_unused_var(ctx), ow_unused_var(val);
	struct ow_sharedstr *const name = (void *)key;
	ow_sharedstr_unref(name);
	return 0;
}

void ow_module_manager_del(struct ow_module_manager *mm) {
	ow_hashmap_foreach(&mm->modules, mm_del_walker, NULL);
	ow_hashmap_fini(&mm->modules);
	ow_free(mm);
}

#define ELEM(NAME) extern OW_BIMOD_MODULE_DEF(NAME) ;
OW_EMBEDDED_MODULE_LIST
#undef ELEM

static const struct ow_native_module_def *const _embedded_modules[] = {
#define ELEM(NAME) & OW_BIMOD_MODULE_DEF_NAME(NAME) ,
	OW_EMBEDDED_MODULE_LIST
#undef ELEM
};

struct ow_module_obj *ow_module_manager_load(
		struct ow_module_manager *mm, const char *name, int flags) {
	struct ow_sharedstr *const name_ss = ow_sharedstr_new(name, (size_t)-1);

	if (!(flags & OW_MODMGR_RELOAD)) {
		struct ow_module_obj *const module =
			ow_hashmap_get(&mm->modules, &ow_sharedstr_hashmap_funcs, name_ss);
		if (module) {
			mm->temp_module = module;
			goto return_module;
		}
	}

	for (size_t i = 0; i < sizeof _embedded_modules / sizeof _embedded_modules[0]; i++) {
		const struct ow_native_module_def *const def = _embedded_modules[i];
		if (strcmp(def->name, name) == 0) {
			assert(!mm->temp_module);
			mm->temp_module = ow_module_obj_new(mm->machine);
			ow_module_obj_load_native_def(mm->machine, mm->temp_module, def);
			goto return_new_loaded;
		}
	}

	// TODO: Load module from file.

	assert(!mm->temp_module);
	goto return_module;

return_new_loaded:
	assert(mm->temp_module);
	if (!(flags &OW_MODMGR_NOCACHE)) {
		ow_hashmap_set(
			&mm->modules, &ow_sharedstr_hashmap_funcs,
			ow_sharedstr_ref(name_ss), mm->temp_module);
	}

return_module:
	ow_sharedstr_unref(name_ss);
	struct ow_module_obj *const module = mm->temp_module;
	mm->temp_module = NULL;
	return module;
}

static int mm_gc_marker_walker(void *ctx, const void *key, void *val) {
	ow_unused_var(key);
	struct ow_machine *const machine = ctx;
	struct ow_module_obj *const module = val;
	ow_objmem_object_gc_marker(machine, ow_object_from(module));
	return 0;
}

void _ow_module_manager_gc_marker(
		struct ow_machine *om, struct ow_module_manager *mm) {
	assert(om == mm->machine);
	ow_hashmap_foreach(&mm->modules, mm_gc_marker_walker, mm->machine);
	if (mm->temp_module)
		ow_objmem_object_gc_marker(om, ow_object_from(mm->temp_module));
}
