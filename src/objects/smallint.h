#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <utilities/attributes.h>

struct ow_object;

/// Small int is an integer that is small enough to be hold in a pointer.
typedef intptr_t ow_smallint_t;

#define OW_SMALLINT_MIN (INTPTR_MIN / 2)
#define OW_SMALLINT_MAX (INTPTR_MAX / 2)

/// Check whether an object pointer is a small int.
ow_static_forceinline bool ow_smallint_check(const struct ow_object *obj_ptr) {
	return (uintptr_t)obj_ptr & 1;
}

/// Convert small int to object pointer.
ow_static_forceinline struct ow_object *ow_smallint_to_ptr(ow_smallint_t val) {
	void *const ptr = (void *)((val << 1) | 1);
	assert((ow_smallint_t)ptr >> 1 == val);
	return ptr;
}

/// Try to convert small int to object pointer. If overflows, return NULL.
ow_static_forceinline struct ow_object *ow_smallint_try_to_ptr(ow_smallint_t val) {
	void *const ptr = (void *)((val << 1) | 1);
	if (ow_likely((ow_smallint_t)ptr >> 1 == val))
		return ptr;
	return NULL;
}

/// Convert object pointer to small int.
ow_static_forceinline ow_smallint_t ow_smallint_from_ptr(const struct ow_object *ptr) {
	assert(ow_smallint_check(ptr));
	return (ow_smallint_t)ptr >> 1;
}
