#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <utilities/attributes.h>
#include <utilities/wordsize.h>

#if OW_WORDSIZE == 64
#	include <stdint.h>
#endif

struct ow_object;

/// GC info and other data.
struct ow_object_meta {
#if OW_WORDSIZE == 64
	uint64_t _data;
#	define OW_OBJMETA_NEXT_MASK    0x0000ffffffffffff
#	define OW_OBJMETA_FLAGS_SHIFT  48
#elif OW_WORDSIZE == 32
	struct ow_object *_next;
	uint32_t          _flags;
#else
#	error "unexpected OW_WORDSIZE value"
#endif
};

enum ow_object_meta_flag {
	OW_OBJMETA_FLAG_EXTENDED  = 0, // Has extra fields. If set, field 0 shall be the number of actual fields (uintptr_t).
	OW_OBJMETA_FLAG_MARKED    = 1, // GC mark.
};

ow_static_forceinline struct ow_object *ow_object_meta_get_next(
		const struct ow_object_meta *meta) {
#if OW_WORDSIZE == 64
	return (struct ow_object *)(meta->_data & OW_OBJMETA_NEXT_MASK);
#elif OW_WORDSIZE == 32
	return meta->_next;
#endif
}

ow_static_forceinline void ow_object_meta_set_next(
		struct ow_object_meta *meta, struct ow_object *next) {
#if OW_WORDSIZE == 64
	assert(!((uintptr_t)next & ~OW_OBJMETA_NEXT_MASK));
	meta->_data = (meta->_data & ~OW_OBJMETA_NEXT_MASK) | (uintptr_t)next;
#elif OW_WORDSIZE == 32
	meta->_next = next;
#endif
}

ow_static_forceinline bool ow_object_meta_get_flag(
		const struct ow_object_meta *meta, enum ow_object_meta_flag flag) {
#if OW_WORDSIZE == 64
	return meta->_data & (UINT64_C(1) << ((uint64_t)flag + OW_OBJMETA_FLAGS_SHIFT));
#elif OW_WORDSIZE == 32
	return meta->_flag & (UINT32_C(1) << (uint32_t)flag);
#endif
}

ow_static_forceinline void ow_object_meta_set_flag(
		struct ow_object_meta *meta, enum ow_object_meta_flag flag) {
#if OW_WORDSIZE == 64
	meta->_data |= (UINT64_C(1) << ((uint64_t)flag + OW_OBJMETA_FLAGS_SHIFT));
#elif OW_WORDSIZE == 32
	meta->_flag |= (UINT32_C(1) << (uint32_t)flag);
#endif
}

ow_static_forceinline void ow_object_meta_clear_flag(
		struct ow_object_meta *meta, enum ow_object_meta_flag flag) {
#if OW_WORDSIZE == 64
	meta->_data &= ~(UINT64_C(1) << ((uint64_t)flag + OW_OBJMETA_FLAGS_SHIFT));
#elif OW_WORDSIZE == 32
	meta->_flag &= ~(UINT32_C(1) << (uint32_t)flag);
#endif
}
