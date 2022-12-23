#pragma once

#include <stddef.h>

struct ow_machine;
struct ow_object;

/// Tuple object. An immutable container.
struct ow_tuple_obj;

/// Create a tuple object from a vector of elements.
/// If param `elems` is NULL, the tuple will be filled with nils.
struct ow_tuple_obj *ow_tuple_obj_new(
	struct ow_machine *om, struct ow_object *elems[], size_t elem_count);
/// Copy elements to a buffer. Return number of copied elements.
size_t ow_tuple_obj_copy(
	struct ow_tuple_obj *self, size_t pos, size_t len,
	struct ow_object *buf[], size_t buf_sz);
/// Create a sub-tuple. Param `pos` and `len` can be out of range.
struct ow_tuple_obj *ow_tuple_obj_slice(
	struct ow_machine *om, struct ow_tuple_obj *tuple, size_t pos, size_t len);
/// Concatenate two tuples.
struct ow_tuple_obj *ow_tuple_obj_concat(
	struct ow_machine *om, struct ow_tuple_obj *tuple1, struct ow_tuple_obj *tuple2);
/// Flatten a string object and get its elements. Param `size` can be NULL.
struct ow_object **ow_tuple_obj_flatten(
	struct ow_machine *om, struct ow_tuple_obj *self, size_t *size);
/// Get number of elements.
size_t ow_tuple_obj_length(const struct ow_tuple_obj *self);
/// Get element by 0-based index. Return NULL if the index is out of range.
struct ow_object *ow_tuple_obj_get(const struct ow_tuple_obj *self, size_t index);
