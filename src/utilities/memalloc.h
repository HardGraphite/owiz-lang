#pragma once

#include <stdbool.h>
#include <stddef.h>

#define ow_malloc(size)                ow_mem_allocate((size))
#define ow_realloc(pointer, new_size)  ow_mem_reallocate((pointer), (new_size))
#define ow_free(pointer)               ow_mem_deallocate((pointer))

/// Allocate memory like `malloc()`.
void *ow_mem_allocate(size_t size);
/// Re-allocate memory like `realloc()`.
void *ow_mem_reallocate(void *ptr, size_t size);
/// Deallocate memory like `free()`.
void ow_mem_deallocate(void *ptr);
/// Allocate virtual memory like `mmap()` or `VirtualAlloc()`.
void *ow_mem_allocate_virtual(size_t size);
/// Deallocate virtual memory like `munmap()` or `VirtualFree()`.
bool ow_mem_deallocate_virtual(void *ptr, size_t size);
/// Get memory page size.
size_t ow_mem_get_pagesize(void);
