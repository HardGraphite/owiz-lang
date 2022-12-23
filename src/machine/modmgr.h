#pragma once

#include <stdbool.h>

struct ow_array_obj;
struct ow_exception_obj;
struct ow_istream;
struct ow_machine;
struct ow_module_obj;

/// Module manager.
struct ow_module_manager;

/// Representation of type and location of a module.
/// Field `file_path` might be a filesystem function internal buffer.
struct ow_module_manager_locator {
	enum {
		OW_MODMGR_ML_NONE,     ///< Not exists.
		OW_MODMGR_ML_EMBEDDED, ///< Builtin (embedded) module, a native definition.
		OW_MODMGR_ML_DYNLIB,   ///< Dynamic library, a file path.
		OW_MODMGR_ML_SOURCE,   ///< Source code file, a file path.
		OW_MODMGR_ML_BYTECODE, ///< Bytecode file, a file path.
	} type;
	union {
		const void *native_def;
		const void *file_path;
	} ;
};

#define OW_MODMGR_RELOAD    0x0001
#define OW_MODMGR_NOCACHE   0x0002

/// Create a module manager.
struct ow_module_manager *ow_module_manager_new(struct ow_machine *om);
/// Destroy a module manager.
void ow_module_manager_del(struct ow_module_manager *mm);
/// Get module path array.
struct ow_array_obj *ow_module_manager_path(struct ow_module_manager *mm);
/// Add a module path.
bool ow_module_manager_add_path(struct ow_module_manager *mm, const char *path_str);
/// Search for a module by name.
struct ow_module_manager_locator ow_module_manager_search(
	struct ow_module_manager *mm, const char *name);
/// Load module by name. If fails, return NULL, and make an exception if `exc` is given.
struct ow_module_obj *ow_module_manager_load(
	struct ow_module_manager *mm, const char *name, int flags,
	struct ow_exception_obj **exc);

void _ow_module_manager_gc_marker(struct ow_machine *om, struct ow_module_manager *mm);
