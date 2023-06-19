#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hash.h" // ow_hash_t

struct _ow_hashmap_node {
    struct _ow_hashmap_node *next_node; // nullable
    ow_hash_t key_hash;
    void *key;
    void *value;
};

struct _ow_hashmap_bucket {
    struct _ow_hashmap_node *nodes; // nullable
};

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

/// Traverse through the hash map.
/// As an usafe trick, `__node_p` is the pointer to current node.
#define ow_hashmap_foreach_1(__map, KEY_TP, KEY_VAR, VAL_TP, VAL_VAR, STMT) \
    do {                                                                    \
        struct _ow_hashmap_bucket *const __buckets = (__map)->_buckets;     \
        const size_t __bucket_cnt = (__map)->_bucket_count;                 \
        for (size_t __i = 0; __i < __bucket_cnt; __i++) {                   \
            for (struct _ow_hashmap_node *__node_p = __buckets[__i].nodes;  \
                __node_p != NULL; __node_p = __node_p->next_node            \
            ) {                                                             \
                KEY_TP KEY_VAR = (KEY_TP)__node_p->key;                     \
                VAL_TP VAL_VAR = (VAL_TP)__node_p->value;                   \
                { STMT }                                                    \
            }                                                               \
        }                                                                   \
    } while (0)                                                             \
// ^^^ ow_hashmap_foreach_1() ^^^
