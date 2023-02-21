#include "tupleobj.h"

#include <assert.h>
#include <string.h>

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include <machine/globals.h>
#include <machine/machine.h>
#include <utilities/unreachable.h>

#define TUPLE_OBJ_HEAD \
	OW_EXTENDED_OBJECT_HEAD \
	size_t elem_count; \
// ^^^ TUPLE_OBJ_HEAD ^^^

struct ow_tuple_obj {
	TUPLE_OBJ_HEAD
};

enum ow_tuple_obj_impl_subtype {
	TUPLE_INNER = 0,
	TUPLE_SLICE = 1,
	TUPLE_CONS  = 2,
};

static void ow_tuple_obj_impl_set_subtype(
	struct ow_object_meta *meta, enum ow_tuple_obj_impl_subtype t) {
	ow_object_meta_set_subtype(meta, (uint8_t)t);
}

static enum ow_tuple_obj_impl_subtype ow_tuple_obj_impl_get_subtype(
	const struct ow_object_meta *meta) {
	return (enum ow_tuple_obj_impl_subtype)ow_object_meta_get_subtype(meta);
}

struct ow_tuple_obj_impl_inner {
	TUPLE_OBJ_HEAD
	struct ow_object *elems[];
};

struct ow_tuple_obj_impl_slice {
	TUPLE_OBJ_HEAD
	struct ow_tuple_obj_impl_inner *tuple;
	size_t begin_index;
};

struct ow_tuple_obj_impl_cons {
	TUPLE_OBJ_HEAD
	struct ow_tuple_obj *tuple1;
	struct ow_tuple_obj *tuple2;
};

static void ow_tuple_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
	ow_unused_var(om);
	assert(ow_class_obj_is_base(om->builtin_classes->tuple, ow_object_class(obj)));
	struct ow_tuple_obj *const self = ow_object_cast(obj, struct ow_tuple_obj);

	switch (ow_tuple_obj_impl_get_subtype(&self->_meta)) {
	case TUPLE_INNER:
		for (size_t i = 0, n = self->elem_count; i < n; i++) {
			ow_objmem_object_gc_marker(
				om, ((struct ow_tuple_obj_impl_inner *)self)->elems[i]);
		}
		break;

	case TUPLE_SLICE:
		ow_objmem_object_gc_marker(
			om, ow_object_from(((struct ow_tuple_obj_impl_slice *)self)->tuple));
		break;

	case TUPLE_CONS:
		ow_objmem_object_gc_marker(
			om, ow_object_from(((struct ow_tuple_obj_impl_cons *)self)->tuple1));
		ow_objmem_object_gc_marker(
			om, ow_object_from(((struct ow_tuple_obj_impl_cons *)self)->tuple2));
		break;

	default:
		ow_unreachable();
	}
}

struct ow_tuple_obj *ow_tuple_obj_new(
		struct ow_machine *om, struct ow_object *elems[], size_t elem_count) {
	struct ow_tuple_obj_impl_inner *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->tuple, elem_count),
		struct ow_tuple_obj_impl_inner);
	ow_tuple_obj_impl_set_subtype(&obj->_meta, TUPLE_INNER);
	obj->elem_count = elem_count;
	if (elems) {
		memcpy(obj->elems, elems, elem_count * sizeof *elems);
	} else {
		struct ow_object *const nil = om->globals->value_nil;
		for (size_t i = 0; i < elem_count; i++)
			obj->elems[i] = nil;
	}
	return (struct ow_tuple_obj *)obj;
}

static size_t _ow_tuple_obj_copy_impl(
		const struct ow_tuple_obj *tuple,
		size_t pos, size_t len, struct ow_object *buf[], size_t buf_size) {
	assert(pos + len <= tuple->elem_count);

	switch (ow_tuple_obj_impl_get_subtype(&tuple->_meta)) {
	case TUPLE_INNER: {
		struct ow_tuple_obj_impl_inner *const tuple_inner =
			(struct ow_tuple_obj_impl_inner *)tuple;
		struct ow_object **const begin_ptr = tuple_inner->elems + pos;
		struct ow_object **end_ptr = begin_ptr + len;
		struct ow_object **const max_ptr = begin_ptr + buf_size;
		if (ow_unlikely(end_ptr > max_ptr))
			end_ptr = max_ptr;
		const size_t cp_n = (size_t)(end_ptr - begin_ptr);
		memcpy(buf, begin_ptr, cp_n * sizeof(void *));
		return cp_n;
	}

	case TUPLE_SLICE: {
		struct ow_tuple_obj_impl_slice *const tuple_slice =
			(struct ow_tuple_obj_impl_slice *)tuple;
		struct ow_object **const begin_ptr =
			tuple_slice->tuple->elems + tuple_slice->begin_index + pos;
		struct ow_object **end_ptr = begin_ptr + len;
		struct ow_object **const max_ptr = begin_ptr + buf_size;
		if (ow_unlikely(end_ptr > max_ptr))
			end_ptr = max_ptr;
		const size_t cp_n = (size_t)(end_ptr - begin_ptr);
		memcpy(buf, begin_ptr, cp_n * sizeof(void *));
		return cp_n;
	}

	case TUPLE_CONS: {
		struct ow_tuple_obj_impl_cons *const tuple_cons =
			(struct ow_tuple_obj_impl_cons *)tuple;
		const size_t tuple1_size = tuple_cons->tuple1->elem_count;
		if (tuple1_size < pos) {
			return _ow_tuple_obj_copy_impl(
				tuple_cons->tuple2, pos - tuple1_size, len, buf, buf_size);
		} else {
			size_t cp_n = 0;
			cp_n += _ow_tuple_obj_copy_impl(
				tuple_cons->tuple1, pos, len, buf, buf_size);
			cp_n += _ow_tuple_obj_copy_impl(
				tuple_cons->tuple2,
				0, len - (tuple1_size - pos),
				buf + tuple1_size, buf_size - tuple1_size);
			return cp_n;
		}
	}

	default:
		ow_unreachable();
	}
}

size_t ow_tuple_obj_copy(
		struct ow_tuple_obj *self, size_t pos, size_t len,
		struct ow_object *buf[], size_t buf_sz) {
	if (ow_unlikely(pos + len >= self->elem_count)) {
		if (pos >= self->elem_count) {
			pos = self->elem_count;
			len = 0;
		} else {
			len = self->elem_count - pos;
		}
	}

	return _ow_tuple_obj_copy_impl(self, pos, len, buf, buf_sz);
}

struct ow_tuple_obj *ow_tuple_obj_slice(
		struct ow_machine *om, struct ow_tuple_obj *tuple, size_t pos, size_t len) {
	if (ow_unlikely(pos == 0 && len >= tuple->field_count))
		return tuple;

	if (ow_unlikely(pos + len >= tuple->field_count)) {
		if (pos >= tuple->field_count) {
			pos = tuple->field_count;
			len = 0;
		} else {
			len = tuple->field_count - pos;
		}
	}

	enum ow_tuple_obj_impl_subtype tuple_type =
		ow_tuple_obj_impl_get_subtype(&tuple->_meta);

	if (len <= 2) {
		struct ow_object *buffer[2];
		ow_tuple_obj_copy(tuple, pos, len, buffer, 2);
		return ow_tuple_obj_new(om, buffer, len);
	} else if (tuple_type == TUPLE_INNER || tuple_type == TUPLE_SLICE) {
	inner_or_slice_tuple:;
		const size_t efc =
			OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj_impl_slice) -
			OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj);
		struct ow_tuple_obj_impl_slice *const obj = ow_object_cast(
			ow_objmem_allocate(om, om->builtin_classes->tuple, efc),
			struct ow_tuple_obj_impl_slice);
		ow_tuple_obj_impl_set_subtype(&obj->_meta, TUPLE_SLICE);
		obj->elem_count = len;

		if (tuple_type == TUPLE_INNER) {
			obj->tuple = (struct ow_tuple_obj_impl_inner *)tuple;
			obj->begin_index = pos;
		} else /* (tuple_type == TUPLE_SLICE) */ {
			struct ow_tuple_obj_impl_slice *const tuple_slice =
				(struct ow_tuple_obj_impl_slice *)tuple;
			obj->tuple = tuple_slice->tuple;
			obj->begin_index = tuple_slice->begin_index + pos;
		}
		return (struct ow_tuple_obj *)obj;
	} else if (tuple_type == TUPLE_CONS) {
		ow_tuple_obj_flatten(om, tuple, NULL);
		tuple_type = ow_tuple_obj_impl_get_subtype(&tuple->_meta);
		assert(tuple_type == TUPLE_INNER || tuple_type == TUPLE_SLICE);
		goto inner_or_slice_tuple;
	} else {
		ow_unreachable();
	}
}

struct ow_tuple_obj *ow_tuple_obj_concat(
		struct ow_machine *om, struct ow_tuple_obj *tuple1, struct ow_tuple_obj *tuple2) {
	const size_t tuple1_size = tuple1->elem_count, tuple2_size = tuple2->elem_count;
	const size_t res_size = tuple1_size + tuple2_size;

	if (res_size <= 2) {
		struct ow_object *buffer[2];
		ow_tuple_obj_copy(tuple1, 0, (size_t)-1, buffer, sizeof buffer);
		ow_tuple_obj_copy(
			tuple2, 0, (size_t)-1, buffer + tuple1_size, sizeof buffer - tuple1_size);
		return ow_tuple_obj_new(om, buffer, res_size);
	}

	if (ow_unlikely(!tuple1_size))
		return tuple2;
	if (ow_unlikely(!tuple2_size))
		return tuple1;

	const size_t efc =
		OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj_impl_cons) -
		OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj);
	struct ow_tuple_obj_impl_cons *const obj = ow_object_cast(
		ow_objmem_allocate(om, om->builtin_classes->tuple, efc),
		struct ow_tuple_obj_impl_cons);
	ow_tuple_obj_impl_set_subtype(&obj->_meta, TUPLE_CONS);
	obj->elem_count = res_size;
	obj->tuple1 = tuple1;
	obj->tuple2 = tuple2;
	return (struct ow_tuple_obj *)obj;
}

struct ow_object **ow_tuple_obj_flatten(
		struct ow_machine *om, struct ow_tuple_obj *self, size_t *size) {
	const enum ow_tuple_obj_impl_subtype tuple_type
		= ow_tuple_obj_impl_get_subtype(&self->_meta);

	if (tuple_type == TUPLE_CONS) {
		struct ow_tuple_obj *const _tuple_inner =
			ow_tuple_obj_new(om, NULL, self->elem_count);
		assert(ow_tuple_obj_impl_get_subtype(&_tuple_inner->_meta) == TUPLE_INNER);
		struct ow_tuple_obj_impl_inner *const tuple_inner =
			(struct ow_tuple_obj_impl_inner *)_tuple_inner;
		ow_tuple_obj_copy(self, 0, (size_t)-1, tuple_inner->elems, self->elem_count);

		static_assert(sizeof(struct ow_tuple_obj_impl_slice) ==
			sizeof(struct ow_tuple_obj_impl_cons), "");
		struct ow_tuple_obj_impl_slice *const tuple_slice =
			(struct ow_tuple_obj_impl_slice *)self;
		ow_tuple_obj_impl_set_subtype(&tuple_slice->_meta, TUPLE_SLICE);
		tuple_slice->tuple = tuple_inner;
		tuple_slice->begin_index = 0;
	}

	if (tuple_type == TUPLE_SLICE) {
		struct ow_tuple_obj_impl_slice *const tuple_slice =
			(struct ow_tuple_obj_impl_slice *)self;
		if (size)
			*size = tuple_slice->elem_count;
		return tuple_slice->tuple->elems + tuple_slice->begin_index;
	} else if (tuple_type == TUPLE_INNER) {
		struct ow_tuple_obj_impl_inner *const tuple_inner =
			(struct ow_tuple_obj_impl_inner *)self;
		if (size)
			*size = tuple_inner->elem_count;
		return tuple_inner->elems;
	} else {
		ow_unreachable();
	}
}

size_t ow_tuple_obj_length(const struct ow_tuple_obj *self) {
	return self->elem_count;
}

struct ow_object *ow_tuple_obj_get(const struct ow_tuple_obj *self, size_t index) {
	if (ow_unlikely(index >= self->elem_count))
		return NULL;

	switch (ow_tuple_obj_impl_get_subtype(&self->_meta)) {
	case TUPLE_INNER:
		return ((struct ow_tuple_obj_impl_inner *)self)->elems[index];

	case TUPLE_SLICE: {
		struct ow_tuple_obj_impl_slice *const tuple_slice =
			(struct ow_tuple_obj_impl_slice *)self;
		return tuple_slice->tuple->elems[tuple_slice->begin_index + index];
	}

	case TUPLE_CONS: {
		struct ow_tuple_obj_impl_cons *const tuple_cons =
			(struct ow_tuple_obj_impl_cons *)self;
		const size_t tuple1_elem_count = tuple_cons->tuple1->elem_count;
		if (index < tuple1_elem_count)
			self = tuple_cons->tuple1;
		else
			self = tuple_cons->tuple2, index -= tuple1_elem_count;
		return ow_tuple_obj_get(self, index);
	}

	default:
		ow_unreachable();
	}
}

static const struct ow_native_func_def tuple_methods[] = {
	{NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(tuple) = {
	.name      = "Tuple",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_tuple_obj),
	.methods   = tuple_methods,
	.finalizer = NULL,
	.gc_marker = ow_tuple_obj_gc_marker,
	.extended  = true,
};
