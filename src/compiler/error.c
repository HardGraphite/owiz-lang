#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <utilities/attributes.h>
#include <utilities/strings.h>

void ow_syntax_error_init(struct ow_syntax_error *error) {
    memset(error, 0, sizeof *error);
}

void ow_syntax_error_fini(struct ow_syntax_error *error) {
    ow_syntax_error_clear(error);
}

void ow_syntax_error_copy(
    struct ow_syntax_error *dst, const struct ow_syntax_error *src
) {
    ow_syntax_error_clear(dst);
    dst->location = src->location;
    if (src->message)
        dst->message = ow_sharedstr_ref(src->message);
}

void ow_syntax_error_clear(struct ow_syntax_error *error) {
    if (error->message) {
        ow_sharedstr_unref(error->message);
        error->message = NULL;
    }
}

ow_printf_fn_attrs(3, 4)
void ow_syntax_error_format(
    struct ow_syntax_error *error, const struct ow_source_range *loc,
    ow_printf_fn_arg_fmtstr const char *msg_fmt, ...
) {
    va_list ap;
    va_start(ap, msg_fmt);
    ow_syntax_error_vformat(error, loc, msg_fmt, ap);
    va_end(ap);
}

ow_printf_fn_attrs(3, 0)
void ow_syntax_error_vformat(
    struct ow_syntax_error *error, const struct ow_source_range *loc,
    ow_printf_fn_arg_fmtstr const char *msg_fmt, va_list args
) {
    error->location = *loc;
    if (ow_unlikely(error->location.begin.line > error->location.end.line))
        error->location.begin.line = error->location.end.line;
    if (ow_unlikely(error->location.begin.line == error->location.end.line
            && error->location.begin.column > error->location.end.column))
        error->location.begin.column = error->location.end.column;

    char msg_buf[128];
    const int n = vsnprintf(msg_buf, sizeof msg_buf, msg_fmt, args);
    if (error->message)
        ow_sharedstr_unref(error->message);
    assert(n > 0);
    error->message = ow_sharedstr_new(msg_buf, (size_t)n);
}
