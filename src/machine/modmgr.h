#pragma once

struct ow_machine;
struct ow_module_obj;

/// Module manager.
struct ow_module_manager;

#define OW_MODMGR_RELOAD    0x0001
#define OW_MODMGR_NOCACHE   0x0002

/// Create a module manager.
struct ow_module_manager *ow_module_manager_new(struct ow_machine *om);
/// Destroy a module manager.
void ow_module_manager_del(struct ow_module_manager *mm);
/// Load module by name. If not found, return NULL.
struct ow_module_obj *ow_module_manager_load(
	struct ow_module_manager *mm, const char *name, int flags);

void _ow_module_manager_gc_marker(struct ow_machine *om, struct ow_module_manager *mm);
