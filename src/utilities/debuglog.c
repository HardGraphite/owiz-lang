#include <config/options.h>

#if OW_DEBUG_LOGGING

#include "debuglog.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef _MSC_VER
#    include <stdatomic.h>
#else
#    include <compat/msvc_stdatomic.h>
#endif

#include <config/definitions.h>

FILE *_ow_debuglog_stream = NULL;
volatile int _ow_debuglog_level = INT_MAX;

static void _debuglog_fini(void) {
    if (!_ow_debuglog_stream)
        return;
    if (_ow_debuglog_stream != stderr && _ow_debuglog_stream != stdout) {
        fclose(_ow_debuglog_stream);
        _ow_debuglog_stream = NULL;
    }
}

static volatile atomic_int debuglog_initializing;

static void _debuglog_init(void) {
    int lock_state = 0;
    while (!atomic_compare_exchange_strong(&debuglog_initializing, &lock_state, 1))
        lock_state = 0;

    assert(!_ow_debuglog_stream);
#ifdef OW_DEBUG_LOGGING_FILE
    _ow_debuglog_stream = fopen(OW_DEBUG_LOGGING_FILE, "w");
    if (!_ow_debuglog_stream)
        _ow_debuglog_stream = stderr;
#else // !OW_DEBUG_LOGGING_FILE
    _ow_debuglog_stream = stderr;
#endif // OW_DEBUG_LOGGING_FILE

#ifdef OW_DEBUG_LOGGING_ENVNAME
    const char *const default_level = getenv(OW_DEBUG_LOGGING_ENVNAME);
    if (default_level)
        _ow_debuglog_level = atoi(default_level);
    else
#endif // OW_DEBUG_LOGGING_ENVNAME
    _ow_debuglog_level = 0;

    at_quick_exit(_debuglog_fini);

    atomic_store(&debuglog_initializing, 0);
}

#define debuglog_try_init() \
    (!_ow_debuglog_stream ? _debuglog_init() : (void)0)

static const char *const debuglog_level_name[(size_t)_OW_DEBUGLOG_COUNT] = {
    [(size_t)OW_DEBUGLOG_CRIT] = "CRITICAL",
    [(size_t)OW_DEBUGLOG_ERR ] = "ERROR",
    [(size_t)OW_DEBUGLOG_WARN] = "WARNING",
    [(size_t)OW_DEBUGLOG_INFO] = "INFO",
    [(size_t)OW_DEBUGLOG_DBG ] = "DEBUG",
};

ow_printf_fn_attrs(3, 4) void _ow_debuglog_print(
    const char *where, int level, ow_printf_fn_arg_fmtstr const char *fmt, ...
) {
    debuglog_try_init();
    if (level > _ow_debuglog_level)
        return;

    assert(level >= 0 && level < (int)_OW_DEBUGLOG_COUNT);
    const char *const level_name = debuglog_level_name[level];
    const clock_t current_time = clock();

    char buffer[256];
    char *buffer_p = buffer;
    int printf_n;
    va_list fmt_args;

    printf_n = snprintf(
        buffer, sizeof buffer, "[OWIZ/%s#%s@%.3f] ",
        where, level_name, ((double)current_time / CLOCKS_PER_SEC)
    );
    assert(printf_n > 0);
    buffer_p += printf_n;

    va_start(fmt_args, fmt);
    printf_n = vsnprintf(buffer_p, (size_t)(buffer + sizeof buffer - buffer_p), fmt, fmt_args);
    va_end(fmt_args);
    assert(printf_n > 0);
    buffer_p += printf_n;

    assert(buffer_p <= buffer + sizeof buffer);
    *buffer_p++ = '\n';

    fwrite(buffer, 1, (size_t)(buffer_p - buffer), _ow_debuglog_stream);
}

#endif // OW_DEBUG_LOGGING

_Static_assert(1, ""); // Avoid empty translation unit.
