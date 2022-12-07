#include "modmgr.h"

#include <stdlib.h>
#include <string.h>

#include <compiler/compiler.h>
#include <compiler/error.h>
#include <machine/machine.h>
#include <machine/sysparam.h>
#include <modules/modules_util.h>
#include <objects/arrayobj.h>
#include <objects/classes.h>
#include <objects/exceptionobj.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <objects/stringobj.h>
#include <utilities/array.h>
#include <utilities/dynlib.h>
#include <utilities/filesystem.h>
#include <utilities/hashmap.h>
#include <utilities/malloc.h>
#include <utilities/process.h>
#include <utilities/stream.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

#include <config/definitions.h>
#include <config/module_list.h>

struct ow_module_manager {
	struct ow_hashmap modules; // {name, module}
	struct ow_array_obj *path_array;
	struct ow_module_obj *temp_module; // Nullable.
	struct ow_machine *machine;
};

static void _add_default_paths(struct ow_module_manager *mm) {
	if (ow_sysparam.default_paths) {
		for (const char *path = ow_sysparam.default_paths;
				*path; path += strlen(path))
			ow_module_manager_add_path(mm, path);
		return;
	}

	const ow_path_char_t *const exe_path = ow_current_exe_path();
	if (exe_path) {
#if _IS_WINDOWS_
		ow_module_manager_add_path(
			mm, ow_path_join(ow_path_parent(exe_path), L"lib"));
#elif _IS_POSIX_
		ow_module_manager_add_path(
			mm, ow_path_join(ow_path_parent(exe_path), "../lib/ow"));
#endif
	}

	const char *const home_path = ow_fs_home_dir();
	if (home_path) {
#if _IS_WINDOWS_
		// TODO: Add user's local library path.
#elif _IS_POSIX_
		ow_module_manager_add_path(
			mm, ow_path_join(home_path, ".local/lib/ow"));
	}
#endif
}

struct ow_module_manager *ow_module_manager_new(struct ow_machine *om) {
	struct ow_module_manager *const mm = ow_malloc(sizeof(struct ow_module_manager));
	ow_hashmap_init(&mm->modules, 8);
	mm->temp_module = NULL;
	mm->path_array = ow_array_obj_new(om, NULL, 0);
	mm->machine = om;
	_add_default_paths(mm);
	return mm;
}

static int mm_del_walker(void *ctx, const void *key, void *val) {
	ow_unused_var(ctx), ow_unused_var(val);
	struct ow_sharedstr *const name = (void *)key;
	ow_sharedstr_unref(name);
	return 0;
}

void ow_module_manager_del(struct ow_module_manager *mm) {
	ow_hashmap_foreach(&mm->modules, mm_del_walker, NULL);
	ow_hashmap_fini(&mm->modules);
	ow_free(mm);
}

struct ow_array_obj *ow_module_manager_path(struct ow_module_manager *mm) {
	return mm->path_array;
}

bool ow_module_manager_add_path(
		struct ow_module_manager *mm, const char *path_str) {
	bool ok = true;

	const ow_path_char_t *path = OW_PATH_FROM_STR(path_str);
	if (ow_fs_exists(path)) {
		path = ow_fs_absolute(path);
		path_str =
#if _IS_WINDOWS_
			ow_winpath_to_str(path);
#else // !_IS_WINDOWS_
			path;
#endif // _IS_WINDOWS_
		struct ow_array *const paths = ow_array_obj_data(mm->path_array);
		for (size_t i = 0, n = ow_array_size(paths); i < n; i++) {
			struct ow_object *const path_o = ow_array_at(paths, i);
			if (ow_smallint_check(path_o) ||
					ow_object_class(path_o) != mm->machine->builtin_classes->string)
				continue;
			const char *const str = ow_string_obj_flatten(
				mm->machine, ow_object_cast(path_o, struct ow_string_obj), NULL);
			if (strcmp(path_str, str) == 0) {
				ok = false;
				break;
			}
		}
		if (ok) {
			struct ow_string_obj *const path_str_o =
				ow_string_obj_new(mm->machine, path_str, (size_t)-1);
			ow_array_append(paths, path_str_o);
		}
#if _IS_WINDOWS_
		ow_free((void *)path_str);
#endif // _IS_WINDOWS_
	} else {
		ok = false;
	}

	return ok;
}

#define ELEM(NAME) extern OW_BIMOD_MODULE_DEF(NAME) ;
OW_EMBEDDED_MODULE_LIST
#undef ELEM

static const struct ow_native_module_def *const _embedded_modules[] = {
#define ELEM(NAME) & OW_BIMOD_MODULE_DEF_NAME(NAME) ,
	OW_EMBEDDED_MODULE_LIST
#undef ELEM
};

struct ow_module_manager_locator ow_module_manager_search(
		struct ow_module_manager *mm, const char *name) {
	struct ow_module_manager_locator result;

#define RETURN_RESULT(TYPE, DATA_TYPE, DATA) \
	do { \
		result.type = (TYPE), result . DATA_TYPE = (DATA); \
		goto return_result; \
	} while (false) \
// ^^^ RETURN_RESULT ^^^

	// Embedded modules.
	for (size_t i = 0; i < sizeof _embedded_modules / sizeof _embedded_modules[0]; i++) {
		const struct ow_native_module_def *const def = _embedded_modules[i];
		if (strcmp(def->name, name) == 0)
			RETURN_RESULT(OW_MODMGR_ML_EMBEDDED, native_def, def);
	}

	// External modules.
	struct ow_array *const paths = ow_array_obj_data(mm->path_array);
	for (size_t i = 0, n = ow_array_size(paths); i < n; i++) {
		struct ow_object *const path_o = ow_array_at(paths, i);
		if (ow_smallint_check(path_o) ||
				ow_object_class(path_o) != mm->machine->builtin_classes->string)
			continue;
		const char *const str = ow_string_obj_flatten(
			mm->machine, ow_object_cast(path_o, struct ow_string_obj), NULL);
#if _IS_WINDOWS_
		ow_path_char_t *_name_as_path = ow_path_dup(ow_winpath_from_str(name));
		const ow_path_char_t *path =
			ow_path_join(ow_winpath_from_str(str), _name_as_path);
		ow_free(_name_as_path);
#else // !_IS_WINDOWS_
		const ow_path_char_t *path = ow_path_join(str, name);
#endif // _IS_WINDOWS_
		path = ow_path_replace_extension(path, OW_PATH_STR(OW_FILENAME_EXTENSION_SRC));
		if (ow_unlikely(ow_fs_exists(path)))
			RETURN_RESULT(OW_MODMGR_ML_SOURCE, file_path, path);
		path = ow_path_replace_extension(path, OW_PATH_STR(OW_FILENAME_EXTENSION_BTC));
		if (ow_unlikely(ow_fs_exists(path)))
			RETURN_RESULT(OW_MODMGR_ML_BYTECODE, file_path, path);
		path = ow_path_replace_extension(path, OW_PATH_STR(OW_FILENAME_EXTENSION_OBJ));
		if (ow_unlikely(ow_fs_exists(path)))
			RETURN_RESULT(OW_MODMGR_ML_DYNLIB, file_path, path);
	}

	// Not found.
	result.type = OW_MODMGR_ML_NONE, result.native_def = NULL;

#undef RETURN_RESULT

return_result:
	return result;
}

static void _load_module_not_found(
		struct ow_module_manager *mm, const char *name,
		struct ow_exception_obj **exc) {
	if (exc) {
		*exc = ow_exception_format(
			mm->machine, NULL, "cannot find module `%s'", name);
	}
}

static void _load_module_from_embedded(
		struct ow_module_manager *mm, const struct ow_native_module_def *def) {
	mm->temp_module = ow_module_obj_new(mm->machine);
	ow_module_obj_load_native_def(mm->machine, mm->temp_module, def);
}

static bool _load_module_from_dynlib(
		struct ow_module_manager *mm, const char *name,
		const ow_path_char_t *file_path, struct ow_exception_obj **exc) {
	char sym_buf[128];
	const char *const sym_prefix = OW_BIMOD_MODULE_DEF_NAME_PREFIX_STR;
	const size_t sym_prefix_len  = sizeof(OW_BIMOD_MODULE_DEF_NAME_PREFIX_STR) - 1;
	const char *const sym_suffix = OW_BIMOD_MODULE_DEF_NAME_SUFFIX_STR;
	const size_t sym_suffix_len  = sizeof(OW_BIMOD_MODULE_DEF_NAME_SUFFIX_STR) - 1;
	const size_t name_len        = strlen(name);
	if (sym_prefix_len + name_len + sym_suffix_len >= sizeof sym_buf) {
		if (exc) {
			*exc = ow_exception_format(
				mm->machine, NULL, "module name `%s' is too long", name);
		}
		return false;
	}
	char *p = sym_buf;
	memcpy(p, sym_prefix, sym_prefix_len);
	p += sym_prefix_len;
	memcpy(p, name, name_len);
	p += name_len;
	memcpy(p, sym_suffix, sym_suffix_len);
	p += sym_suffix_len;
	*p = '\0';
	assert(p < sym_buf + sizeof sym_buf);

	ow_dynlib_t lib = ow_dynlib_open(file_path);
	if (!lib) {
		if (exc) {
			*exc = ow_exception_format(
				mm->machine, NULL,
				"cannot open module file `%" OW_PATH_STR_PRI "'", file_path);
		}
		return false;
	}
	const struct ow_native_module_def *const def = ow_dynlib_symbol(lib, sym_buf);
	if (!def) {
		if (exc) {
			*exc = ow_exception_format(
				mm->machine, NULL,
				"invalid module file `%" OW_PATH_STR_PRI "'", file_path);
		}
		return false;
	}
	// ow_dynlib_close(lib);
	// TODO: Collect lib handles.
	_load_module_from_embedded(mm, def);
	return true;
}

static bool _load_module_from_source(
		struct ow_module_manager *mm, const ow_path_char_t *file_path,
		struct ow_exception_obj **exc) {
	struct ow_istream *const file_stream = ow_istream_open(file_path);
	if (!file_stream) {
		if (exc) {
			*exc = ow_exception_format(
				mm->machine, NULL,
				"cannot open module file `%" OW_PATH_STR_PRI "'", file_path);
		}
		return false;
	}

	struct ow_compiler *const compiler = ow_compiler_new(mm->machine);
	mm->temp_module = ow_module_obj_new(mm->machine);
#if _IS_WINDOWS_
	ow_path_char_t *_path_as_str = ow_winpath_to_str(file_path);
	struct ow_sharedstr *const file_name_ss =
		ow_sharedstr_new(_path_as_str, (size_t)-1);
	ow_free(_path_as_str);
#else // !_IS_WINDOWS_
	struct ow_sharedstr *const file_name_ss =
		ow_sharedstr_new(file_path, (size_t)-1);
#endif

	const bool ok = ow_compiler_compile(
		compiler, file_stream, file_name_ss, 0, mm->temp_module);
	if (ow_unlikely(!ok)) {
		struct ow_syntax_error *const err = ow_compiler_error(compiler);
		if (exc) {
			*exc = ow_exception_format(
				mm->machine, NULL, "%" OW_PATH_STR_PRI " : %u:%u-%u:%u : %s\n",
				file_path,
				err->location.begin.line, err->location.begin.column,
				err->location.end.line, err->location.end.column,
				ow_sharedstr_data(err->message));
		}
	}

	ow_sharedstr_unref(file_name_ss);
	ow_compiler_del(compiler);
	ow_istream_close(file_stream);
	return ok;
}

static bool _load_module_from_bytecode(
		struct ow_module_manager *mm, const ow_path_char_t *file_path,
		struct ow_exception_obj **exc) {
	// TODO: Load module from bytecode file.
	if (exc) {
		*exc = ow_exception_format(
			mm->machine, NULL,
			"invalid module file `%" OW_PATH_STR_PRI "'", file_path);
	}
	return false;
}

/// Load new module to `mm->temp_module`.
static bool _load_module(
		struct ow_module_manager *mm, const char *name,
		struct ow_exception_obj **exc) {
	const struct ow_module_manager_locator ml = ow_module_manager_search(mm, name);
	switch (ml.type) {
	case OW_MODMGR_ML_NONE:
		_load_module_not_found(mm, name, exc);
		return false;
	case OW_MODMGR_ML_EMBEDDED:
		_load_module_from_embedded(mm, ml.native_def);
		return true;
	case OW_MODMGR_ML_DYNLIB:
		return _load_module_from_dynlib(mm, name, ml.file_path, exc);
	case OW_MODMGR_ML_SOURCE:
		return _load_module_from_source(mm, ml.file_path, exc);
	case OW_MODMGR_ML_BYTECODE:
		return _load_module_from_bytecode(mm, ml.file_path, exc);
	default:
		ow_unreachable();
	}
}

struct ow_module_obj *ow_module_manager_load(
		struct ow_module_manager *mm, const char *name, int flags,
		struct ow_exception_obj **exc) {
	struct ow_sharedstr *const name_ss = ow_sharedstr_new(name, (size_t)-1);

	do {
		if (!(flags & OW_MODMGR_RELOAD)) {
			struct ow_module_obj *const module =
				ow_hashmap_get(&mm->modules, &ow_sharedstr_hashmap_funcs, name_ss);
			if (module) {
				mm->temp_module = module;
				break;
			}
		}

		if (_load_module(mm, name, exc)) {
			assert(mm->temp_module);
			if (!(flags & OW_MODMGR_NOCACHE)) {
				ow_hashmap_set(
					&mm->modules, &ow_sharedstr_hashmap_funcs,
					ow_sharedstr_ref(name_ss), mm->temp_module);
			}
			break;
		} else {
			assert(!mm->temp_module); // Not found.
			break;
		}
	} while (false);

	ow_sharedstr_unref(name_ss);
	struct ow_module_obj *const module = mm->temp_module;
	mm->temp_module = NULL;
	return module;
}

static int mm_gc_marker_walker(void *ctx, const void *key, void *val) {
	ow_unused_var(key);
	struct ow_machine *const machine = ctx;
	struct ow_module_obj *const module = val;
	ow_objmem_object_gc_marker(machine, ow_object_from(module));
	return 0;
}

void _ow_module_manager_gc_marker(
		struct ow_machine *om, struct ow_module_manager *mm) {
	assert(om == mm->machine);
	ow_hashmap_foreach(&mm->modules, mm_gc_marker_walker, mm->machine);
	if (mm->path_array)
		ow_objmem_object_gc_marker(om, ow_object_from(mm->path_array));
	if (mm->temp_module)
		ow_objmem_object_gc_marker(om, ow_object_from(mm->temp_module));
}
