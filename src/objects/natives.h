#pragma once

#include <stdbool.h>
#include <stddef.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;

typedef void (*ow_objmem_obj_fields_visitor_t)(void *, int); // Declared in "objmem.h"

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
    void (*finalizer)(void *);
};

/// Extended `struct ow_native_class_def`.
struct ow_native_class_def_ex {
    const char *name;
    size_t data_size;
    const struct ow_native_func_def *methods;
    void (*finalizer)(struct ow_object *);
    ow_objmem_obj_fields_visitor_t gc_visitor;
    bool extended;
};

/// A native class def for a non-native class.
struct ow_native_class_def_nn {
    const char *name;
    const char *const *attributes;
    const struct ow_native_func_def *methods;
};

/// Same with `struct owiz_native_module_def`.
struct ow_native_module_def {
    const char                      *name;
    const struct ow_native_func_def *functions;
    void (*finalizer)(void);
};
