#pragma once

#include <utilities/attributes.h> // ow_nodiscard
#include <utilities/filesystem.h> // ow_path_char_t

/// Pointer to loaded dynamic library.
typedef void *ow_dynlib_t;

/// Open a dynamic library. Return `NULL` if fails.
ow_nodiscard ow_dynlib_t ow_dynlib_open(const ow_path_char_t *file);
/// Close a dynamic library.
void ow_dynlib_close(ow_dynlib_t lib);
/// Get address of a symbol in the library. Return `NULL` if the symbol is not found.
void *ow_dynlib_symbol(ow_dynlib_t lib, const char *name);
