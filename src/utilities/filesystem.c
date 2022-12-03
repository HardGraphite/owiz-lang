#include "filesystem.h"

#include <assert.h>
#include <string.h>

#if _IS_WINDOWS_
#	include <wctype.h>
#	include <wchar.h>
#else
#	include <ctype.h>
#endif

#include <utilities/attributes.h>
#include <utilities/malloc.h>
#include <utilities/pragmas.h>

#if _IS_POSIX_

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef PATH_MAX
ow_pragma_message("PATH_MAX not defined")
#	define PATH_MAX 4096
#endif // PATH_MAX

#define PREFERRED_DIR_SEP '/'

#elif _IS_WINDOWS_

#include <Windows.h>

#define PATH_MAX MAX_PATH

#define PREFERRED_DIR_SEP L'\\'

#else

ow_pragma_message("file system utilities are not implemeted")

#endif

static ow_path_char_t path_buffer[PATH_MAX];

static bool is_dir_sep(ow_path_char_t ch) {
#if _IS_POSIX_
	return ch == '/';
#elif _IS_WINDOWS_
	return ch == L'\\' || ch == L'/';
#endif
}

static ow_path_char_t *rfind_dir_sep(const ow_path_char_t *path) {
#if _IS_POSIX_
	return strrchr(path, '/');
#elif _IS_WINDOWS_
	ow_path_char_t *const p1 = wcsrchr(path, L'\\');
	if (!p1)
		return wcsrchr(path, L'/');
	ow_path_char_t *const p2 = wcsrchr(p1, L'/');
	if (!p2)
		return p1;
	return p2;
#endif
}

static ow_path_char_t *rfind_dot(const ow_path_char_t *path) {
#if _IS_POSIX_
	return strrchr(path, '.');
#elif _IS_WINDOWS_
	return wcsrchr(path, L'.');
#endif
}

ow_path_char_t *ow_path_dup(const ow_path_char_t *path) {
	const size_t size = (ow_path_len(path) + 1) * sizeof(ow_path_char_t);
	ow_path_char_t *s = ow_malloc(size);
	return memcpy(s, path, size);
}

ow_path_char_t *ow_path_move(ow_path_char_t *dest, const ow_path_char_t *src) {
	return ow_path_move_n(dest, src, ow_path_len(src) + 1);
}

ow_path_char_t *ow_path_move_n(
		ow_path_char_t *dest, const ow_path_char_t *src, size_t len) {
	return memmove(dest, src, len * sizeof(ow_path_char_t));
}

size_t ow_path_len(const ow_path_char_t *path) {
#if _IS_POSIX_
	return strlen(path);
#elif _IS_WINDOWS_
	return wcslen(path);
#endif
}

const ow_path_char_t *ow_path_filename(const ow_path_char_t *path) {
	const ow_path_char_t *const last_dir_sep = rfind_dir_sep(path);
	if (ow_unlikely(!last_dir_sep))
		return ow_path_move(path_buffer, path);
	if (ow_unlikely(last_dir_sep[1] == 0)) {
		path_buffer[0] = 0; // empty filename
		return path_buffer;
	}
	const ow_path_char_t *const s = last_dir_sep + 1;
	assert(ow_path_len(s) < PATH_MAX);
	return ow_path_move(path_buffer, s);
}

const ow_path_char_t *ow_path_stem(const ow_path_char_t *path) {
	const ow_path_char_t *last_dir_sep = rfind_dir_sep(path);
	if (last_dir_sep) {
		if (ow_unlikely(last_dir_sep[1] == 0)) {
			const size_t path_len = ow_path_len(path);
			assert(path_len < PATH_MAX);
			ow_path_move_n(path_buffer, path, path_len - 1);
			path_buffer[path_len - 1] = 0;
			return ow_path_stem(path_buffer);
		}
		ow_path_move(path_buffer, last_dir_sep + 1);
	} else {
		if (path != path_buffer) {
			assert(ow_path_len(path) < PATH_MAX);
			ow_path_move(path_buffer, path);
		}
	}
	ow_path_char_t *const last_dot = rfind_dot(path_buffer);
	if (last_dot && last_dot != path_buffer)
		*last_dot = 0;
	return path_buffer;
}

const ow_path_char_t *ow_path_extension(const ow_path_char_t *path) {
	const ow_path_char_t *const last_dot = rfind_dot(path);
	if (ow_unlikely(!last_dot)) {
		path_buffer[0] = 0;
		return path_buffer;
	}
	if (ow_unlikely(last_dot[1] == 0)) {
		if (last_dot == path || is_dir_sep(last_dot[-1]))
			path_buffer[0] = 0;
		else
			path_buffer[0] = PREFERRED_DIR_SEP, path_buffer[1] = 0;
		return path_buffer;
	}
	if (ow_unlikely(last_dot == path || is_dir_sep(last_dot[-1]))) {
		path_buffer[0] = 0;
		return path_buffer;
	}
	return ow_path_move(path_buffer, last_dot);
}

const ow_path_char_t *ow_path_parent(const ow_path_char_t *path) {
	ow_path_char_t *const last_dir_sep = rfind_dir_sep(path);
	if (ow_unlikely(!last_dir_sep))
		return ow_path_join(path, OW_PATH_STR(".."));
	const size_t n = last_dir_sep - path;
	assert(n < PATH_MAX);
	if (ow_unlikely(n == 0)) {
		path_buffer[0] = PREFERRED_DIR_SEP, path_buffer[1] = 0;
		return path_buffer;
	}
	ow_path_move_n(path_buffer, path, n);
	path_buffer[n] = 0;
#if _IS_WINDOWS_
	if (n == 2 && path[1] == L':' && iswalpha(path[0])) { // "X:/"
		path_buffer[2] = L'\\', path_buffer[3] = 0;
		return path_buffer;
	}
#endif // _IS_WINDOWS_
	if (ow_unlikely(last_dir_sep[1] == 0)) {
		ow_path_char_t *p = last_dir_sep;
		while (p > path && is_dir_sep(*p))
			p--;
		p[1] = 0;
	}
	return path_buffer;
}

const ow_path_char_t *ow_path_concat(
		const ow_path_char_t *path1, const ow_path_char_t *path2) {
	const size_t path1_len = ow_path_len(path1);
	assert(path1_len + ow_path_len(path2) < PATH_MAX);
	ow_path_move_n(path_buffer, path1, path1_len);
	ow_path_move(path_buffer + path1_len, path2);
	return path_buffer;
}

const ow_path_char_t *ow_path_join(
		const ow_path_char_t *path1, const ow_path_char_t *path2) {
	if (ow_unlikely(is_dir_sep(path2[0])))
		return path2 == path_buffer ? path2 : ow_path_move(path_buffer, path2);
	const size_t path1_len = ow_path_len(path1);
	if (ow_unlikely(is_dir_sep(path1[path1_len - 1])))
		return ow_path_concat(path1, path2);
	assert(path1_len + ow_path_len(path2) < PATH_MAX);;
	ow_path_move_n(path_buffer, path1, path1_len);
	path_buffer[path1_len] = PREFERRED_DIR_SEP;
	ow_path_move(path_buffer + path1_len + 1, path2);
	return path_buffer;
}

#if _IS_WINDOWS_

char *ow_winpath_to_str(const ow_path_char_t *path) {
	const int u8str_len =
		WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
	assert(u8str_len > 0);
	char *const u8str = ow_malloc((size_t)u8str_len);
	WideCharToMultiByte(CP_UTF8, 0, path, -1, u8str, u8str_len, NULL, NULL);
	return u8str;
}

ow_path_char_t *ow_winpath_from_str(const char *path_str) {
	const bool ok =
		MultiByteToWideChar(CP_UTF8, 0, path_str, -1, path_buffer, PATH_MAX);
	return ok ? ow_path_dup(path_buffer) : NULL;
}

#endif // _IS_WINDOWS_

bool ow_fs_exists(const ow_path_char_t *path) {
#if _IS_POSIX_
	return access(path, F_OK) == 0;
#elif _IS_WINDOWS_
	return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
#endif
}

const ow_path_char_t *ow_fs_absolute(const ow_path_char_t *path) {
#if _IS_POSIX_
	return realpath(path, path_buffer);
#elif _IS_WINDOWS_
	const bool ok = GetFullPathNameW(path, PATH_MAX, path_buffer, NULL);
	return ok ? path_buffer : NULL;
#endif
}

int ow_fs_filetype(const ow_path_char_t *path) {
#if _IS_POSIX_
	struct stat file_stat;
	if (ow_unlikely(stat(path, &file_stat)))
		return -1;
	int result = 0;
	if (S_ISREG(file_stat.st_mode))
		result |= OW_FS_FT_REG;
	if (S_ISDIR(file_stat.st_mode))
		result |= OW_FS_FT_DIR;
	if (S_ISLNK(file_stat.st_mode))
		result |= OW_FS_FT_LNK;
	return result;
#elif _IS_WINDOWS_
	const DWORD attr = GetFileAttributesW(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return -1;
	int result = 0;
	if (attr & FILE_ATTRIBUTE_NORMAL)
		result |= OW_FS_FT_REG;
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		result |= OW_FS_FT_DIR;
	if (attr & FILE_ATTRIBUTE_REPARSE_POINT)
		result |= OW_FS_FT_LNK;
	return result;
#endif
}

int ow_fs_iter_dir(
		const ow_path_char_t *dir_path, ow_fs_dir_walker_t func, void *arg) {
#if _IS_POSIX_
	DIR *dir;
	struct dirent *dir_entry;
	if (!(dir = opendir(dir_path)))
		return 0;

	int ret = 0;
	while ((dir_entry = readdir(dir))) {
		const ow_path_char_t *const name = dir_entry->d_name;
		if (ow_unlikely(name[0] == '.')) {
			const char c = name[1];
			if (ow_unlikely(c == '\0' || (c == '.' && name[2] == '\0')))
				continue;
		}
		if ((ret = func(arg, name)) != 0)
			break;
	}
	closedir(dir);
	return ret;
#elif _IS_WINDOWS_
	size_t dir_len = ow_path_len(dir_path);
	if (ow_unlikely(dir_len > (MAX_PATH - 3)))
		return 0; // Too long.
	ow_path_char_t *const dir = ow_path_move_n(path_buffer, dir_path, dir_len);
	dir[dir_len] = L'\\', dir[dir_len + 1] = L'*', dir[dir_len + 2] = 0;

	WIN32_FIND_DATAW ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	if ((hFind = FindFirstFileW(dir, &ffd)) == INVALID_HANDLE_VALUE)
		return 0;
	int ret = 0;
	do {
		const DWORD attr = ffd.dwFileAttributes;
		if (!(attr & FILE_ATTRIBUTE_DIRECTORY || attr & FILE_ATTRIBUTE_NORMAL))
			continue;
		if ((ret = func(arg, ffd.cFileName)) != 0)
			break;
	} while (FindNextFileW(hFind, &ffd));
	FindClose(hFind);
	return ret;
#endif
}
