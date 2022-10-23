#pragma once

struct ow_machine;
struct ow_object;

/// Global data for a context.
struct ow_machine_globals {
	struct ow_object *value_nil;
	struct ow_object *value_true;
	struct ow_object *value_false;
};

/// Create globals.
struct ow_machine_globals *ow_machine_globals_new(struct ow_machine *om);
/// Destroy globals.
void ow_machine_globals_del(struct ow_machine_globals *mg);

void _ow_machine_globals_gc_marker(struct ow_machine *om, struct ow_machine_globals *mg);
