#pragma once

#include <stdint.h>
#include <stddef.h>

#include <utilities/attributes.h>

typedef uint32_t ow_hash_t;

/// Hash function for 32-bit integer.
ow_static_inline ow_hash_t ow_hash_int(int32_t val);
/// Hash function for 64-bit integer.
ow_static_inline ow_hash_t ow_hash_int64(int64_t val);
/// Hash function for pointer.
ow_static_inline ow_hash_t ow_hash_pointer(const void *ptr);
/// Hash function for double.
ow_hash_t ow_hash_double(double val);
/// Hash function for byte array.
ow_hash_t ow_hash_bytes(const void *data, size_t size);

ow_static_inline ow_hash_t ow_hash_int(int32_t val) {
	return (ow_hash_t)val;
}

ow_static_inline ow_hash_t ow_hash_int64(int64_t val) {
	return (ow_hash_t)val;
}

ow_static_inline ow_hash_t ow_hash_pointer(const void *ptr) {
	return (ow_hash_t)(uintptr_t)ptr;
}
