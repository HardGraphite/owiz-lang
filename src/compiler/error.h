#pragma once

#include <stdarg.h>

#include "location.h"
#include <utilities/attributes.h>

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
ow_printf_fn_attrs(3, 4)
void ow_syntax_error_format(
	struct ow_syntax_error *error, const struct ow_source_range *loc,
	ow_printf_fn_arg_fmtstr const char *msg_fmt, ...);
/// Similar to `ow_syntax_error_format()`, but use `va_list`.
ow_printf_fn_attrs(3, 0)
void ow_syntax_error_vformat(
	struct ow_syntax_error *error, const struct ow_source_range *loc,
	ow_printf_fn_arg_fmtstr const char *msg_fmt, va_list args);
