#pragma once

#include "object_util.h"

// BICLS means builtin class

/// Definition variable name.
#define OW_BICLS_CLASS_DEF_NAME(CLASS) \
    __ow_bic_##CLASS##_class_def

/// Definition variable name.
#define OW_BICLS_CLASS_DEF_EX_NAME(CLASS) \
    __ow_bic_##CLASS##_class_def_ex

/// Define a definition variable.
#define OW_BICLS_CLASS_DEF(CLASS) \
    const struct ow_native_class_def OW_BICLS_CLASS_DEF_NAME(CLASS)

/// Define a definition variable.
#define OW_BICLS_CLASS_DEF_EX(CLASS) \
    const struct ow_native_class_def_ex OW_BICLS_CLASS_DEF_EX_NAME(CLASS)

/// Define and initialize a definition variable.
/// The parameters are: CLASS, NAME, EXTENDED, FINALIZER, GC_VISITOR, METHOD-DEF-LIST.
#define OW_BICLS_DEF_CLASS_EX(CLASS, NAME, EXTENDED, FINALIZER, GC_VISITOR, ...) \
    OW_BICLS_DEF_CLASS_EX_1(CLASS, CLASS, NAME, EXTENDED, FINALIZER, GC_VISITOR, __VA_ARGS__)

/// Like `OW_BICLS_DEF_CLASS_EX()`, but allows a different variable name from class variable.
#define OW_BICLS_DEF_CLASS_EX_1( \
    VAR_SUFFIX, CLASS, NAME, EXTENDED, FINALIZER, GC_VISITOR, ... \
)                                                                 \
    static const struct ow_native_func_def CLASS##_methods[] = {  \
        __VA_ARGS__                                               \
        {NULL, NULL, 0, 0},                                       \
    };                                                            \
    OW_BICLS_CLASS_DEF_EX(VAR_SUFFIX) = {                         \
        .name = NAME,                                             \
        .data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_##CLASS##_obj), \
        .methods = CLASS##_methods,                               \
        .finalizer = FINALIZER,                                   \
        .gc_visitor = GC_VISITOR,                                 \
        .extended = EXTENDED,                                     \
    };                                                            \
// ^^^ OW_BICLS_DEF_CLASS_EX_1() ^^^
