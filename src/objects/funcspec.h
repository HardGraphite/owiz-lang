#pragma once

/// Function common specification.
struct ow_func_spec {
    int           arg_cnt;   ///< Number of arguments. See `OWIZ_NATIVE_FUNC_VARIADIC_ARGC()` for variadic.
    unsigned int  oarg_cnt;  ///< Number of optional arguments.
    unsigned int  local_cnt; ///< Number of local variables.
};
