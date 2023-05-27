#include "dynlib.h"

#include <assert.h>

#include <utilities/platform.h>
#include <utilities/pragmas.h>

#if _IS_WINDOWS_
#    include <Windows.h>
static_assert(sizeof(HMODULE) == sizeof(ow_dynlib_t), "");
#elif _IS_POSIX_
#    include <dlfcn.h>
static_assert(sizeof(void *) == sizeof(ow_dynlib_t), "");
#else
ow_pragma_message("thread utilities are not implemeted")
#endif

ow_nodiscard ow_dynlib_t ow_dynlib_open(const ow_path_char_t *file) {
#if _IS_WINDOWS_
    return LoadLibraryW(file);
#elif _IS_POSIX_
    return dlopen(file, RTLD_LAZY);
#else
    return NULL;
#endif
}

void ow_dynlib_close(ow_dynlib_t lib) {
#if _IS_WINDOWS_
    FreeLibrary(lib);
#elif _IS_POSIX_
    dlclose(lib);
#else
    ow_unused_var(lib);
#endif
}

void *ow_dynlib_symbol(ow_dynlib_t lib, const char *name) {
#if _IS_WINDOWS_
    return (void *)GetProcAddress(lib, name);
#elif _IS_POSIX_
    return dlsym(lib, name);
#else
    ow_unused_var(lib), ow_unused_var(name);
    return NULL;
#endif
}
