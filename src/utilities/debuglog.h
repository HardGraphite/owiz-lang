#pragma once

#include <config/options.h>

#if OW_DEBUG_LOGGING

#include <stdio.h>

#include <utilities/attributes.h>

/// Logging levels.
enum ow_debuglog_level {
    OW_DEBUGLOG_CRIT = 0, ///< critical
    OW_DEBUGLOG_ERR  = 1, ///< error
    OW_DEBUGLOG_WARN = 2, ///< warning
    OW_DEBUGLOG_INFO = 3, ///< information
    OW_DEBUGLOG_DBG  = 4, ///< debug
    _OW_DEBUGLOG_COUNT
};

/// Print to debug log. `where` is a string describing where the message comes from;
/// `level` is the suffix of a `enum ow_debuglog_level` value; the rest are
/// like the arguments for `printf()`.
#define ow_debuglog_print(where, level, ...) \
    (((int)(OW_DEBUGLOG_##level) <= _ow_debuglog_level) ?  \
        _ow_debuglog_print((where), (OW_DEBUGLOG_##level), __VA_ARGS__) : (void)0)

/// Get current logging stream (`FILE *`).
#define ow_debuglog_stream() \
    _ow_debuglog_stream

/// Execute the statement at the specified logging leve.
#define ow_debuglog_when(level, STATEMENT) \
    do {                                   \
        if ((int)(OW_DEBUGLOG_##level) > _ow_debuglog_level) \
            break;                         \
        STATEMENT                          \
    } while (0)                            \
// ^^^ ow_debuglog_when() ^^^

extern FILE *_ow_debuglog_stream;
extern volatile int _ow_debuglog_level;

ow_printf_fn_attrs(3, 4) void
_ow_debuglog_print(const char *, int, ow_printf_fn_arg_fmtstr const char *, ...);

#else // !OW_DEBUG_LOGGING

#define ow_debuglog_print(...)  ((void)0)
#define ow_debuglog_stream()    ((void *)0)
#define ow_debuglog_when(...)   ((void)0)

#endif // OW_DEBUG_LOGGING
