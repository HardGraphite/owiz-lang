#pragma once

#include <stdbool.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;
struct ow_symbol_obj;

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

/// Call method of an object with arguments. The first argument is the object itself.
/// If param `argv` is NULL, the method (a placeholder) and the arguments
/// shall have been pushed like using `ow_machine_invoke()`.
int ow_machine_call_method(
    struct ow_machine *om, struct ow_symbol_obj *method_name,
    int argc, struct ow_object *argv[], struct ow_object **res_out);

/// Wrap and call a native function with arguments. Not recommended.
int ow_machine_call_native(
    struct ow_machine *om,
    struct ow_module_obj *mod, int (*func)(struct ow_machine *),
    int argc, struct ow_object *argv[], struct ow_object **res_out);

/// Run a module. Call the initializer and main function (optional).
int ow_machine_run(struct ow_machine *om,
    struct ow_module_obj *module, bool call_main, struct ow_object **res_out);
