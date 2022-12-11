#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <utilities/attributes.h>
#include <utilities/platform.h>

#if _IS_WINDOWS_

#include <stddef.h> // wchar_t

typedef wchar_t ow_path_char_t;

#define _OW_PATH_CHR(C)  L ## C
#define _OW_PATH_STR(S)  L ## S
#define OW_PATH_CHR(C)   _OW_PATH_CHR(C)
#define OW_PATH_STR(S)   _OW_PATH_STR(S)
#define OW_PATH_CHR_PRI  "lc"
#define OW_PATH_STR_PRI  "ls"

#else // !_IS_WINDOWS_

typedef char ow_path_char_t;

#define OW_PATH_CHR(C)   C
#define OW_PATH_STR(S)   S
#define OW_PATH_CHR_PRI  "c"
#define OW_PATH_STR_PRI  "s"

#endif // _IS_WINDOWS_

/// Duplicate a path. Deallocate the path with `ow_free()` to avoid a memory leak.
ow_nodiscard ow_path_char_t *ow_path_dup(const ow_path_char_t *path);
/// Copy path string like `memmove()`, adding terminating NUL, but without length. Assume that `dest` is beg enough.
ow_path_char_t *ow_path_move(ow_path_char_t *dest, const ow_path_char_t *src);
/// Copy path string like `memmove()`. Param `len` is number of chars. Assume that `dest` is beg enough.
ow_path_char_t *ow_path_move_n(ow_path_char_t *dest, const ow_path_char_t *src, size_t len);
/// Get path string length (number of `ow_path_char_t` characters).
size_t ow_path_len(const ow_path_char_t *path);
/// Get file name of a path. Thread-local buffer is used.
const ow_path_char_t *ow_path_filename(const ow_path_char_t *path);
/// Get file name without the final extension. Thread-local buffer is used.
const ow_path_char_t *ow_path_stem(const ow_path_char_t *path);
/// Get file name extension. Thread-local buffer is used.
const ow_path_char_t *ow_path_extension(const ow_path_char_t *path);
/// Get the path of the parent path. Thread-local buffer is used.
const ow_path_char_t *ow_path_parent(const ow_path_char_t *path);
/// Concatenates two path strings. Thread-local buffer is used.
const ow_path_char_t *ow_path_concat(
	const ow_path_char_t *path1, const ow_path_char_t *path2);
/// Join two paths. Thread-local buffer is used.
const ow_path_char_t *ow_path_join(
	const ow_path_char_t *path1, const ow_path_char_t *path2);
/// Replace or add file name extension. Param `new_ext` is optional. Thread-local buffer is used.
const ow_path_char_t *ow_path_replace_extension(
	const ow_path_char_t *path, const ow_path_char_t *new_ext);

#if _IS_WINDOWS_

/// Convert path to UTF-8 string. Deallocate the string with `ow_free()` to avoid a memory leak.
ow_nodiscard char *ow_winpath_to_str(const ow_path_char_t *path);
/// Convert UTF-8 string to path. Return `NULL` if error occurs. Thread-local buffer is used.
const ow_path_char_t *ow_winpath_from_str(const char *path_str);

#define OW_PATH_FROM_STR(STR) ow_winpath_from_str((STR))

#else

#define OW_PATH_FROM_STR(STR) (STR)

#endif

/// File types.
enum ow_fs_filetype {
	OW_FS_FT_REG = 0x01, ///< Is a regular file.
	OW_FS_FT_DIR = 0x02, ///< Is a directory.
	OW_FS_FT_LNK = 0x04, ///< Is a symbolic link.
};

/// Callback function for `ow_fs_iter_dir()`. Param `file` is a pointer to a local buffer.
typedef int (*ow_fs_dir_walker_t)(void *arg, const ow_path_char_t *file);

/// Check whether a path exists.
bool ow_fs_exists(const ow_path_char_t *path);
/// Compose an absolute path. Return NULL if failed. Thread-local buffer is used.
const ow_path_char_t *ow_fs_absolute(const ow_path_char_t *path);
/// Get file type. On success, return `OW_FS_FT_XXX` values; on failure, return `-1`.
int ow_fs_filetype(const ow_path_char_t *path);
/// Visit files in a directory.
int ow_fs_iter_dir(const ow_path_char_t *dir, ow_fs_dir_walker_t func, void *arg);

/// Get home directory of current user. Return NULL if unknown. Thread-local buffer might be used.
const ow_path_char_t *ow_fs_home_dir(void);

/// Get internal thread-local path string buffer.
ow_path_char_t *_ow_fs_buffer(size_t *buf_sz);
