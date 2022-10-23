#pragma once

struct ow_class_obj;
struct ow_machine;

#define OW_BICLS_LIST0 \
	ELEM(object) \
	ELEM(class_) \
// ^^^ OW_BICLS_LIST0 ^^^

#define OW_BICLS_LIST \
	ELEM(bool_)       \
	ELEM(cfunc)       \
	ELEM(func)        \
	ELEM(nil)         \
	ELEM(symbol)      \
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
/// Clear all classes.
void _ow_builtin_classes_cleanup(struct ow_machine *om, struct ow_builtin_classes * bic);
/// Destroy a `struct ow_builtin_classes`.
void _ow_builtin_classes_del(
	struct ow_machine *om, struct ow_builtin_classes * bic, _Bool finalize_classes);

void _ow_builtin_classes_gc_marker(struct ow_machine *om, struct ow_builtin_classes * bic);
