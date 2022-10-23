#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hash.h" // ow_hash_t

struct _ow_hashmap_bucket;

/// Hash map, whose keys and values are pointers.
struct ow_hashmap {
	size_t _size;
	size_t _bucket_count;
	struct _ow_hashmap_bucket *_buckets;
};

/// Functions used for hash map querying and manipulating.
struct ow_hashmap_funcs {
	bool (*key_equal)(void *ctx, const void *key_new, const void *key_stored);
	ow_hash_t (*key_hash)(void *ctx, const void *key_new);
	void *context;
};

/// Callback function to visit elements in a hash map.
typedef int (*ow_hashmap_walker_t)(void *arg, const void *key, void *val);

/// Initialize the hash map.
void ow_hashmap_init(struct ow_hashmap *map, size_t n);
/// Finalize the hash map.
void ow_hashmap_fini(struct ow_hashmap *map);
/// Reserve space for more elements.
void ow_hashmap_reserve(struct ow_hashmap *map, size_t size);
/// Shrink to fit the number of elements.
void ow_hashmap_shrink(struct ow_hashmap *map);
/// Insert elements from another map.
void ow_hashmap_extend(
	struct ow_hashmap *map, const struct ow_hashmap_funcs *mf, struct ow_hashmap *other);
/// Delete element.
bool ow_hashmap_remove(
	struct ow_hashmap *map, const struct ow_hashmap_funcs *mf, const void *key);
/// Delete all elements.
void ow_hashmap_clear(struct ow_hashmap *map);
/// Insert or assign.
void ow_hashmap_set(
	struct ow_hashmap *map, const struct ow_hashmap_funcs *mf, const void *key, void *val);
/// Find element. If not exist, return NULL.
void *ow_hashmap_get(
	const struct ow_hashmap *map, const struct ow_hashmap_funcs *mf, const void *key);
/// Traverse through the hash map.
int ow_hashmap_foreach(
	const struct ow_hashmap *map, ow_hashmap_walker_t walker, void *arg);
/// Get the number of elements.
static inline size_t ow_hashmap_size(const struct ow_hashmap *map) { return map->_size; }
