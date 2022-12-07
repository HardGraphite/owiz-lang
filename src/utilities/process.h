#pragma once

#include <utilities/filesystem.h> // ow_path_char_t

/// Get path to current executable file. Return `NULL` if it is unknown.
/// The return value might be a filesystem function internal buffer.
const ow_path_char_t *ow_current_exe_path(void);
