#include <config/options.h>

#if OW_DEBUG_LOGGING

#include "debuglog.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _MSC_VER
#    include <stdatomic.h>
#else
#    include <compat/msvc_stdatomic.h>
#endif

#include <utilities/pragmas.h>
#include <utilities/strings.h>

#include <config/definitions.h>

FILE *_ow_debuglog_stream = NULL;
volatile int _ow_debuglog_level = INT_MAX;

static const char *const debuglog_level_name[(size_t)_OW_DEBUGLOG_COUNT] = {
    [(size_t)OW_DEBUGLOG_CRIT] = "CRITICAL",
    [(size_t)OW_DEBUGLOG_ERR ] = "ERROR",
    [(size_t)OW_DEBUGLOG_WARN] = "WARNING",
    [(size_t)OW_DEBUGLOG_INFO] = "INFO",
    [(size_t)OW_DEBUGLOG_DBG ] = "DEBUG",
};

static void debuglog_load_config(int *level, const char **file) {
#if 1
    ow_pragma_message(
        "Debug logging is enabled. "
        "Set environment variable `" OW_DEBUG_LOGGING_ENVNAME "=[LEVEL][:FILE]' "
        "to control the logging level and output file. "
        "`LEVEL' is a number or a prefix of level name; leave it empty to disable. "
        "`FILE' is the path to a writable file; leave it empty for stderr. "
        "The default value is `" OW_DEBUG_LOGGING_DEFAULT "'."
    )
#endif

    *level = -1;
    *file = NULL;

    const char *conf_str = getenv(OW_DEBUG_LOGGING_ENVNAME);
    if (!conf_str)
        conf_str = OW_DEBUG_LOGGING_DEFAULT;

    const char *colon_pos = strchr(conf_str, ':');
    if (colon_pos && colon_pos[1])
        *file = colon_pos + 1;
    else
        colon_pos = conf_str + strlen(conf_str);

    const long level_num = strtol(conf_str, NULL, 10);
    if (level_num || conf_str[0] == '0') {
        *level = (int)level_num;
    } else {
        char level_str[16];
        size_t level_str_len = (size_t)(colon_pos - conf_str);
        if (level_str_len >= sizeof level_str)
            level_str_len = sizeof level_str - 1;
        memcpy(level_str, conf_str, level_str_len);
        level_str[level_str_len] = 0;
        ow_str_to_upper(level_str, level_str_len);
        for (int i = 0; i < (int)_OW_DEBUGLOG_COUNT; i++) {
            if (ow_str_starts_with(debuglog_level_name[i], level_str)) {
                *level = i;
                break;
            }
        }
    }
}

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

    int conf_level;
    const char *conf_file;
    debuglog_load_config(&conf_level, &conf_file);

    _ow_debuglog_level = conf_level;

    assert(!_ow_debuglog_stream);
    if (conf_file) {
        _ow_debuglog_stream = fopen(conf_file, "w");
        if (!_ow_debuglog_stream)
            _ow_debuglog_stream = stderr;
    } else {
        _ow_debuglog_stream = stderr;
    }

    at_quick_exit(_debuglog_fini);

    atomic_store(&debuglog_initializing, 0);
}

#define debuglog_try_init() \
    (!_ow_debuglog_stream ? _debuglog_init() : (void)0)

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
