#pragma once

struct ow_machine;
struct ow_module_obj;
struct ow_object;

/// Invoke an invocable object.
/// The object and arguments shall have been pushed to stack in order.
/// If the procedure returns successfully, the return value will be stored
/// to `*res_out`, and 0 will be returned;
/// if an exception is thrown, the exception will be stored to `*res_out`,
/// and -1 will be returned.
int ow_machine_invoke(struct ow_machine *om, int argc, struct ow_object **res_out);

/// Call an invocable object with arguments.
int ow_machine_call(
	struct ow_machine *om, struct ow_object *func,
	int argc, struct ow_object *argv[], struct ow_object **res_out);

/// Wrap and call a native function with arguments. Not recommended.
int ow_machine_call_native(
	struct ow_machine *om,
	struct ow_module_obj *mod, int (*func)(struct ow_machine *),
	int argc, struct ow_object *argv[], struct ow_object **res_out);

/// Run a module.
int ow_machine_run(struct ow_machine *om,
	struct ow_module_obj *module, struct ow_object **res_out);
