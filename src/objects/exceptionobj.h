#pragma once

#include <stddef.h>

struct ow_class_obj;
struct ow_machine;
struct ow_object;

/// Base exception object.
struct ow_exception_obj;

/// A frame information in stored an exception.
struct ow_exception_obj_frame_info {
	struct ow_object *function;
	const unsigned char *ip;
};

/// Create a standard exception object. Pass `exc_type=NULL` to use base exception.
struct ow_exception_obj *ow_exception_new(
	struct ow_machine *om, struct ow_class_obj *exc_type, struct ow_object *data);
/// Get exception data.
struct ow_object *ow_exception_obj_data(const struct ow_exception_obj *self);
/// Append a frame info.
void ow_exception_obj_backtrace_append(
	struct ow_exception_obj *self,
	const struct ow_exception_obj_frame_info *info);
/// Get a vector of frame info.
const struct ow_exception_obj_frame_info *ow_exception_obj_backtrace(
	struct ow_exception_obj *self, size_t *count);
