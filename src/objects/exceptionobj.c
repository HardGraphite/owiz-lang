#include "exceptionobj.h"

#include <stdio.h>

#include "cfuncobj.h"
#include "classes.h"
#include "classes_util.h"
#include "classobj.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include "stringobj.h"
#include "symbolobj.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/stream.h>

struct ow_exception_obj {
	OW_OBJECT_HEAD
	struct ow_xarray backtrace;
	struct ow_object *data;
};

static void ow_exception_obj_finalizer(
		struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(
		om->builtin_classes->exception, ow_object_class(obj)));
	struct ow_exception_obj *const self =
		ow_object_cast(obj, struct ow_exception_obj);
	ow_xarray_fini(&self->backtrace);
}

static void ow_exception_obj_gc_marker(
		struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(
		om->builtin_classes->exception, ow_object_class(obj)));
	struct ow_exception_obj *const self =
		ow_object_cast(obj, struct ow_exception_obj);
	for (size_t i = 0, n = ow_xarray_size(&self->backtrace); i < n; i++) {
		ow_objmem_object_gc_marker(om, ow_xarray_at(
			&self->backtrace, struct ow_exception_obj_frame_info, i).function);
	}
	ow_objmem_object_gc_marker(om, self->data);
}

struct ow_exception_obj *ow_exception_new(
		struct ow_machine *om, struct ow_class_obj *exc_type,
		struct ow_object *data) {
	if (ow_unlikely(!exc_type))
		exc_type = om->builtin_classes->exception;
	assert(ow_class_obj_is_base(om->builtin_classes->exception, exc_type));
	assert(_ow_class_obj_pub_info(om->builtin_classes->exception)->basic_field_count
		== _ow_class_obj_pub_info(exc_type)->basic_field_count);

	struct ow_exception_obj *const obj = ow_object_cast(
		ow_objmem_allocate(om, exc_type, 0),
		struct ow_exception_obj);
	ow_xarray_init(&obj->backtrace, struct ow_exception_obj_frame_info, 4);
	obj->data = data;
	return obj;
}

struct ow_object *ow_exception_obj_data(const struct ow_exception_obj *self) {
	return self->data;
}

void ow_exception_obj_backtrace_append(
		struct ow_exception_obj *self,
		const struct ow_exception_obj_frame_info *info) {
	ow_xarray_append(&self->backtrace, struct ow_exception_obj_frame_info, *info);
}

const struct ow_exception_obj_frame_info *ow_exception_obj_backtrace(
		struct ow_exception_obj *self, size_t *count) {
	if (count)
		*count = ow_xarray_size(&self->backtrace);
	return  ow_xarray_data(&self->backtrace, struct ow_exception_obj_frame_info);
}

void ow_exception_obj_print(
		struct ow_machine *om, struct ow_exception_obj *self,
		struct ow_iostream *stream, int flags) {
	char buffer[128];

	if (flags & 1) {
		struct ow_symbol_obj *const name_sym =
			_ow_class_obj_pub_info(ow_object_class(ow_object_from(self)))->class_name;
		snprintf(buffer, sizeof buffer, "Exception `%s': ", ow_symbol_obj_data(name_sym));
		ow_iostream_puts(stream, buffer);

		if (!ow_smallint_check(self->data) &&
				ow_object_class(self->data) == om->builtin_classes->string) {
			struct ow_string_obj *const str_o =
				ow_object_cast(self->data, struct ow_string_obj);
			ow_iostream_puts(stream, ow_string_obj_flatten(om, str_o, NULL));
		} else {
			// TODO: Print data of other type.
			ow_iostream_puts(stream, "...");
		}
		ow_iostream_putc(stream, '\n');
	}

	if (flags & 2) {
		if (ow_xarray_size(&self->backtrace))
			ow_iostream_puts(stream, "Stack trace:\n");

		for (size_t i = 0, n = ow_xarray_size(&self->backtrace); i < n; i++) {
			const struct ow_exception_obj_frame_info *const fi =
				&ow_xarray_at(&self->backtrace, struct ow_exception_obj_frame_info, i);
			struct ow_object *const func = fi->function;
			const char *func_name;
			assert(!ow_smallint_check(func));
			struct ow_class_obj *const func_class = ow_object_class(func);
			if (func_class == om->builtin_classes->cfunc)
				func_name = ow_object_cast(func, struct ow_cfunc_obj)->name;
			else
				func_name = "???"; // TODO: Get name of other type of functions.
			snprintf(buffer, sizeof buffer, " [%zu] %s @%p", i, func_name, (void *)fi->ip);
			ow_iostream_puts(stream, buffer);
			ow_iostream_putc(stream, '\n');
		}
	}
}

static const struct ow_native_func_def exception_methods[] = {
	{NULL, NULL, 0},
};

OW_BICLS_CLASS_DEF_EX(exception) = {
	.name      = "Exception",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_exception_obj),
	.methods   = exception_methods,
	.finalizer = ow_exception_obj_finalizer,
	.gc_marker = ow_exception_obj_gc_marker,
};