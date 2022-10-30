#pragma once

/// Function common specification.
struct ow_func_spec {
	unsigned char pa_cnt; ///< Number of positional arguments.
	unsigned int  lc_cnt; ///< Number of local variables.
};
