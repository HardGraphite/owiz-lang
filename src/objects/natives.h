#pragma once

#include <ow.h>

struct ow_machine;
struct ow_module_obj;
struct ow_object;

struct ow_native_class_def_ex {
	const char *name;
	size_t data_size;
	const ow_native_func_def_t *methods;
	void (*finalizer)(struct ow_machine *, struct ow_object *);
	void (*gc_marker)(struct ow_machine *, struct ow_object *);
	bool extended;
};
