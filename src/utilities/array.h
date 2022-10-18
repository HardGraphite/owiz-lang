#pragma once

#include <stddef.h>

#include <utilities/attributes.h>

/// Dynamic array of pointers.
struct ow_array {
	void **_arr;
	size_t _cap;
	size_t _len;
};

/// Initialize an array.
void ow_array_init(struct ow_array *arr, size_t n);
/// Finalize an array.
void ow_array_fini(struct ow_array *arr);
/// Reserve space for elements.
void ow_array_reserve(struct ow_array *arr, size_t n);
/// Append element.
void ow_array_append(struct ow_array *arr, void *elem);
/// Remove the last element. DO NOT call this function on an empty array.
ow_static_inline void ow_array_drop(struct ow_array *arr) { arr->_len--; }
/// Delete all elements.
ow_static_inline void ow_array_clear(struct ow_array *arr) { arr->_len = 0; }
/// Get number of elements.
ow_static_inline size_t ow_array_size(const struct ow_array *arr) { return arr->_len; }
/// Get pointer to the element array.
ow_static_inline void **ow_array_data(struct ow_array *arr) { return arr->_arr; }
/// Get reference to one element.
#define ow_array_at(arr, index) ((arr)->_arr[(index)])
/// Get reference to the last element.
#define ow_array_last(arr) ow_array_at(arr, (arr)->_len - 1)

/// Dynamic array, whose elements can be of any size.
struct ow_xarray {
	unsigned char *_arr;
	size_t         _cap;
	size_t         _len;
};

void _ow_xarray_init(struct ow_xarray *arr, size_t sz, size_t n);
void _ow_xarray_reserve(struct ow_xarray *arr, size_t sz, size_t n);
void _ow_xarray_append_slow(struct ow_xarray *arr, size_t sz, const void *elem_ptr);

/// Initialize an array.
#define ow_xarray_init(arr, elem_type, n) \
	_ow_xarray_init((arr), sizeof(elem_type), (n))
/// Finalize an array.
void ow_xarray_fini(struct ow_xarray *arr);
/// Reserve space for elements.
#define ow_xarray_reserve(arr, elem_type, n) \
	_ow_xarray_reserve((arr), sizeof(elem_type), (n))
/// Append element.
#define ow_xarray_append(arr, elem_type, elem) \
	(ow_likely((arr)->_len < (arr)->_cap) ? \
	(void)(ow_xarray_at(arr, elem_type, (arr)->_len++) = (elem)) : \
	_ow_xarray_append_slow((arr), sizeof(elem_type), &(elem)))
/// Remove the last element. DO NOT call this function on an empty array.
ow_static_inline void ow_xarray_drop(struct ow_xarray *arr) { arr->_len--; }
/// Delete all elements.
ow_static_inline void ow_xarray_clear(struct ow_xarray *arr) { arr->_len = 0; }
/// Get number of elements.
ow_static_inline size_t ow_xarray_size(const struct ow_xarray *arr) { return arr->_len; }
/// Get pointer to the element array.
#define ow_xarray_data(arr, elem_type) ((elem_type *)(arr)->_arr)
/// Get reference to one element.
#define ow_xarray_at(arr, elem_type, index) (ow_xarray_data(arr, elem_type)[(index)])
/// Get reference to the last element.
#define ow_xarray_last(arr, elem_type) \
	ow_xarray_at((arr), elem_type, (arr)->_len - 1)
