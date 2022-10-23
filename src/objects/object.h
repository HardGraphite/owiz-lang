#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h> // uintptr_t

#include "objmeta.h"
#include "smallint.h"
#include <utilities/attributes.h>

struct ow_class_obj;

/// Common head of any object struct.
#define OW_OBJECT_HEAD \
	struct ow_object_meta _meta; \
	struct ow_class_obj *_class; \
// ^^^ OW_OBJECT_HEAD ^^^

/// Common head of object structs that has extra fields.
#define OW_EXTENDED_OBJECT_HEAD \
	OW_OBJECT_HEAD         \
	uintptr_t field_count; \
// ^^^ OW_EXTENDED_OBJECT_HEAD ^^^

/// Object.
struct ow_object {
	OW_OBJECT_HEAD
	struct ow_object *_fields[];
};

/// Get class of an object.
ow_static_forceinline struct ow_class_obj *ow_object_class(
		const struct ow_object *obj) {
	assert(!ow_smallint_check(obj));
	return obj->_class;
}

/// Get field by index. No bounds checking.
ow_static_forceinline struct ow_object *ow_object_get_field(
		const struct ow_object *obj, size_t index) {
	return obj->_fields[index];
}

/// Set field by index. No bounds checking.
ow_static_forceinline void ow_object_set_field(
		struct ow_object *obj, size_t index, struct ow_object *value) {
	obj->_fields[index] = value;
}

/// Convert from object struct pointer to `struct ow_object *`.
#define ow_object_from(obj_ptr) \
	((struct ow_object *)(obj_ptr))

/// Convert from `struct ow_object *` to other object struct pointer.
#define ow_object_cast(obj_ptr, type) \
	((type *)(obj_ptr))
