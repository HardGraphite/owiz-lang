#pragma once

#include <stdbool.h>
#include <stddef.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;

/// Same with `owiz_native_func_t`.
typedef int (*ow_native_func_t)(struct ow_machine *);

/// Same with `OWIZ_NATIVE_FUNC_VARIADIC_ARGC`.
#define OW_NATIVE_FUNC_VARIADIC_ARGC(ARGC)  (-1 - (ARGC))

/// Same with `struct owiz_native_func_def`.
struct ow_native_func_def {
    const char      *name;
    ow_native_func_t func;
    int              argc;
    unsigned int     oarg;
};

/// Same with `struct owiz_native_class_def`.
struct ow_native_class_def {
    const char                      *name;
    size_t                           data_size;
    const struct ow_native_func_def *methods;
    void (*finalizer)(struct ow_machine *, void *);
};

/// Extended `struct ow_native_class_def`.
struct ow_native_class_def_ex {
    const char *name;
    size_t data_size;
    const struct ow_native_func_def *methods;
    void (*finalizer)(struct ow_machine *, struct ow_object *);
    void (*gc_marker)(struct ow_machine *, struct ow_object *);
    bool extended;
};

/// Same with `struct owiz_native_module_def`.
struct ow_native_module_def {
    const char                      *name;
    const struct ow_native_func_def *functions;
    ow_native_func_t                 finalizer;
};
