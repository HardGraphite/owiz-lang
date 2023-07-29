#include "process.h"

#include <utilities/platform.h>
#include <utilities/platform.h>

#if defined(_WIN32)

#include <Windows.h>

static bool get_current_exe_path(ow_path_char_t *buffer, size_t buffer_size) {
    const DWORD n = GetModuleFileNameW(NULL, buffer, (DWORD)buffer_size);
    return n;
}

#elif defined(__linux__) || defined(__linux)

#include <unistd.h>

static bool get_current_exe_path(ow_path_char_t *buffer, size_t buffer_size) {
    const ssize_t n = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if (n <= 0)
        return false;
    buffer[(size_t)n] = '\0';
    return true;
}

#elif defined(__APPLE__)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mach-o/dyld.h>
#include <sys/param.h>

static bool get_current_exe_path(ow_path_char_t *buffer, size_t buffer_size) {
    uint32_t buffer_size_u32 = (uint32_t)buffer_size;
    return _NSGetExecutablePath(buffer, &buffer_size_u32) == 0;
}

#elif defined(__NetBSD__)

#include <unistd.h>

static bool get_current_exe_path(ow_path_char_t *buffer, size_t buffer_size) {
    const ssize_t n = readlink("/proc/curproc/exe", buffer, buffer_size - 1);
    if (n <= 0)
        return false;
    buffer[(size_t)n] = '\0';
    return true;
}

#elif defined(__FreeBSD__) || defined(__FreeBSD)

#include <sys/types.h>
#include <sys/sysctl.h>

static bool get_current_exe_path(ow_path_char_t *buffer, size_t buffer_size) {
    size_t size = buffer_size - 1;
    int name[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    if (sysctl(name, 4, buffer, &buffer_size, NULL, 0) != 0)
        return false;
    buffer[size] = '\0';
    return true;
}

#else

#define get_current_exe_path(P, N) false

#endif

const ow_path_char_t *ow_current_exe_path(void) {
    size_t buffer_size;
    ow_path_char_t *const buffer = _ow_fs_buffer(&buffer_size);
    // https://stackoverflow.com/questions/1023306
    return get_current_exe_path(buffer, buffer_size) ? buffer : NULL;
}
