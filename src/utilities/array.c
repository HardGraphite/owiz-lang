#include "array.h"

#include <assert.h>
#include <string.h>

#include <utilities/malloc.h>

#define OW_ARRAY_ELEM_SZ sizeof(void *)

void ow_array_init(struct ow_array *arr, size_t n) {
	arr->_arr = n ? ow_malloc(OW_ARRAY_ELEM_SZ * n) : NULL;
	arr->_cap = n;
	arr->_len = 0;
}

void ow_array_fini(struct ow_array *arr) {
	if (ow_likely(arr->_arr))
		ow_free(arr->_arr);
}

void ow_array_reserve(struct ow_array *arr, size_t n) {
	if (arr->_cap < n) {
		arr->_arr = ow_realloc(arr->_arr, OW_ARRAY_ELEM_SZ * n);
		arr->_cap = n;
	}
}

void ow_array_shrink(struct ow_array *arr) {
	assert(arr->_len <= arr->_cap);
	if (arr->_len == arr->_cap)
		return;
	const size_t n_bytes = OW_ARRAY_ELEM_SZ * arr->_len;
	void *const new_data = ow_malloc(n_bytes);
	memcpy(new_data, arr->_arr, n_bytes);
	ow_free(arr->_arr);
	arr->_cap = arr->_len;
	arr->_arr = new_data;
}

void ow_array_append(struct ow_array *arr, void *elem) {
	const size_t new_len = arr->_len + 1;
	if (ow_unlikely(new_len > arr->_cap)) {
		const size_t old_cap = arr->_cap;
		const size_t new_cap = ow_likely(old_cap) ? old_cap * 2 : 2;
		arr->_arr = ow_realloc(arr->_arr, OW_ARRAY_ELEM_SZ * new_cap);
		arr->_cap = new_cap;
		assert(new_len <= arr->_cap);
	}
	arr->_arr[arr->_len] = elem;
	arr->_len = new_len;
}

void ow_array_extend(struct ow_array *arr, struct ow_array *other) {
	ow_array_reserve(arr, arr->_len + other->_len);
	memcpy(arr->_arr + arr->_len, other->_arr, OW_ARRAY_ELEM_SZ * other->_len);
	arr->_len += other->_len;
}

void _ow_xarray_init(struct ow_xarray *arr, size_t sz, size_t n) {
	arr->_arr = n ? ow_malloc(sz * n) : NULL;
	arr->_cap = n;
	arr->_len = 0;
}

void ow_xarray_fini(struct ow_xarray *arr) {
	if (ow_likely(arr->_arr))
		ow_free(arr->_arr);
}

void _ow_xarray_reserve(struct ow_xarray *arr, size_t sz, size_t n) {
	if (arr->_cap < n) {
		arr->_arr = ow_realloc(arr->_arr, sz * n);
		arr->_cap = n;
	}
}

void _ow_xarray_append_slow(struct ow_xarray *arr, size_t sz, const void *elem_ptr) {
	const size_t new_len = arr->_len + 1;
	if (ow_likely(new_len > arr->_cap)) {
		const size_t old_cap = arr->_cap;
		const size_t new_cap = ow_likely(old_cap) ? old_cap * 2 : 2;
		arr->_arr = ow_realloc(arr->_arr, sz * new_cap);
		arr->_cap = new_cap;
		assert(new_len <= arr->_cap);
	}
	memcpy(&arr->_arr[arr->_len * sz], elem_ptr, sz);
	arr->_len = new_len;
}
