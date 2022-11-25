#pragma once

#include <stddef.h>

struct ow_machine;

/// String object, using UTF-8 string.
struct ow_string_obj;

/// Create a string object. `n = -1` to calc string length automatically.
struct ow_string_obj *ow_string_obj_new(
	struct ow_machine *om, const char *s, size_t n);
/// Create a substring. Param `pos` and `len` can be out of range.
struct ow_string_obj *ow_string_obj_slice(
	struct ow_machine *om, struct ow_string_obj *str, size_t pos, size_t len);
/// Concatenate two strings.
struct ow_string_obj *ow_string_obj_concat(
	struct ow_machine *om, struct ow_string_obj *str1, struct ow_string_obj *str2);
/// Copy string data to a buffer. Return number of copied bytes.
/// Param `pos` and `len` can be out of range. Terminating NUL will not be copied.
size_t ow_string_obj_copy(
	const struct ow_string_obj *self,
	size_t pos, size_t len, char *buf, size_t buf_size);
/// Flatten a string object and get its data (NUL-terminated string and size).
/// Param `s_size` can be NULL, if do not need the string size.
const char *ow_string_obj_flatten(
	struct ow_machine *om, struct ow_string_obj *self, size_t *s_size);
/// Get number of bytes in the string.
size_t ow_string_obj_size(const struct ow_string_obj *self);
/// Get number of characters in the string.
size_t ow_string_obj_length(const struct ow_string_obj *self);
