#pragma once

#include <stdarg.h>

#include "location.h"

/// Error info reported by a compiler.
struct ow_syntax_error {
	struct ow_source_range location;
	struct ow_sharedstr *message;
};

/// Initialize error.
void ow_syntax_error_init(struct ow_syntax_error *error);
/// Finalize error.
void ow_syntax_error_fini(struct ow_syntax_error *error);
/// Copy an error.
void ow_syntax_error_copy(
	struct ow_syntax_error *dst, const struct ow_syntax_error *src);
/// Clear error data.
void ow_syntax_error_clear(struct ow_syntax_error *error);
/// Set location and message.
void ow_syntax_error_format(
	struct ow_syntax_error *error, const struct ow_source_range *loc,
	const char *msg_fmt, ...);
/// Similar to `ow_syntax_error_format()`, but use `va_list`.
void ow_syntax_error_vformat(
	struct ow_syntax_error *error, const struct ow_source_range *loc,
	const char *msg_fmt, va_list args);
