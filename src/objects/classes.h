#pragma once

struct ow_class_obj;
struct ow_machine;

#define OW_BICLS_LIST0 \
    ELEM(object) \
    ELEM(class_) \
// ^^^ OW_BICLS_LIST0 ^^^

#define OW_BICLS_LIST \
    ELEM(array)       \
    ELEM(bool_)       \
    ELEM(cfunc)       \
    ELEM(exception)   \
    ELEM(float_)      \
    ELEM(func)        \
    ELEM(int_)        \
    ELEM(map)         \
    ELEM(module)      \
    ELEM(nil)         \
    ELEM(set)         \
    ELEM(objslots)    \
    ELEM(objslotz)    \
    ELEM(stream)      \
    ELEM(string)      \
    ELEM(symbol)      \
    ELEM(tuple)       \
// ^^^ OW_BICLS_LIST ^^^

/// A collection of builtin classes.
struct ow_builtin_classes {
#define ELEM(NAME) struct ow_class_obj * NAME;
    OW_BICLS_LIST0
    OW_BICLS_LIST
#undef ELEM
};

/// Create a `struct ow_builtin_classes`. Not fully initialized.
struct ow_builtin_classes *_ow_builtin_classes_new(struct ow_machine *om);
/// Load data.
void _ow_builtin_classes_setup(struct ow_machine *om, struct ow_builtin_classes *bic);
/// Destroy a `struct ow_builtin_classes`.
void _ow_builtin_classes_del(struct ow_machine *om, struct ow_builtin_classes * bic);
