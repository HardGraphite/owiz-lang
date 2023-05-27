#include "memalloc.h"

#include <assert.h>
#include <stdlib.h>

#include <utilities/attributes.h>
#include <utilities/platform.h>

#if _IS_POSIX_
#    include <sys/mman.h>
#    include <unistd.h>
#    ifndef MAP_ANONYMOUS
#        define MAP_ANONYMOUS MAP_ANON
#    endif // MAP_ANONYMOUS
#elif _IS_WINDOWS_
#    include <Windows.h>
#else
#    include <utilities/pragmas.h>
ow_pragma_message("virtual memory APIs are unknown for this platform")
#endif

ow_malloc_fn_attrs(1, size)
void *ow_mem_allocate(size_t size) {
    return malloc(size);
}

ow_realloc_fn_attrs(2, size)
void *ow_mem_reallocate(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void ow_mem_deallocate(void *ptr) {
    free(ptr);
}

ow_malloc_fn_attrs(1, size)
void *ow_mem_allocate_virtual(size_t size) {
#if _IS_POSIX_
    const int prot  = PROT_READ | PROT_WRITE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void *const ptr = mmap(NULL, size, prot, flags, -1, 0);
    return ptr != MAP_FAILED ? ptr : NULL;
#elif _IS_WINDOWS_
    const DWORD type = MEM_RESERVE | MEM_COMMIT;
    const DWORD prot = PAGE_READWRITE;
    void *const ptr  = VirtualAlloc(NULL, size, type, prot);
    return ptr;
#else
    return ow_mem_allocate(size);
#endif
}

bool ow_mem_deallocate_virtual(void *ptr, size_t size) {
    bool ok;
#if _IS_POSIX_
    ok = munmap(ptr, size) == 0;
#elif _IS_WINDOWS_
    ow_unused_var(size);
    ok = VirtualFree(ptr, 0, MEM_RELEASE);
#else
    ow_unused_var(size);
    ow_mem_deallocate(ptr);
    ok = true;
#endif
    return ok;
}

size_t ow_mem_get_pagesize(void) {
#if _IS_POSIX_
    const long sz = sysconf(_SC_PAGESIZE);
    assert(sz > 0);
    return (size_t)sz;
#elif _IS_WINDOWS_
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return 4 * 1024; // 4 KiB
#endif
}
