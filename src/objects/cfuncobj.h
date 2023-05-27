#pragma once

#include "funcspec.h"
#include "natives.h"
#include "object.h"

struct ow_machine;
struct ow_module_obj;

/// C function object.
struct ow_cfunc_obj {
    OW_OBJECT_HEAD
    struct ow_func_spec func_spec;
    struct ow_module_obj *module; // Optional.
    const char *name;
    ow_native_func_t code;
};

/// Create a cfunc object.
struct ow_cfunc_obj *ow_cfunc_obj_new(
    struct ow_machine *om, struct ow_module_obj *module,
    const char *name, ow_native_func_t code, const struct ow_func_spec *spec);
