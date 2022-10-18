#include "hashmap.h"

#include <assert.h>

#include <utilities/attributes.h>
#include <utilities/malloc.h>

struct _ow_hashmap_node {
	struct _ow_hashmap_node *next_node; // nullable
	ow_hash_t key_hash;
	void  *key;
	void  *value;
};

struct _ow_hashmap_bucket {
	struct _ow_hashmap_node *nodes; // nullable
};

static_assert(offsetof(struct _ow_hashmap_node, next_node)
	== offsetof(struct _ow_hashmap_bucket, nodes), "");

typedef struct _ow_hashmap_node node_t;
typedef struct _ow_hashmap_bucket bucket_t;

void ow_hashmap_init(struct ow_hashmap *map, size_t n) {
	map->_size = 0;
	map->_bucket_count = n;
	map->_buckets = ow_malloc(sizeof(bucket_t) * n);
	for (size_t i = 0; i < n; i++)
		map->_buckets[i].nodes = NULL;
}

void ow_hashmap_fini(struct ow_hashmap *map) {
	ow_hashmap_clear(map);
	ow_free(map->_buckets);
}

void ow_hashmap_reserve(struct ow_hashmap *map, size_t size) {
	if (ow_unlikely(size <= map->_bucket_count))
		return;

	bucket_t *const new_bucket_vec = ow_malloc(sizeof(bucket_t) * size);
	for (size_t i = 0; i < size; i++)
		new_bucket_vec[i].nodes = NULL;
	bucket_t *const old_bucket_vec = map->_buckets;
	const size_t old_bucket_cnt = map->_bucket_count;

	for (size_t old_bucket_i = 0; old_bucket_i < old_bucket_cnt; old_bucket_i++) {
		node_t *old_node_p = old_bucket_vec[old_bucket_i].nodes;
		while (old_node_p != NULL) {
			const size_t new_bucket_i = old_node_p->key_hash % size;
			bucket_t *const new_bucket = new_bucket_vec + new_bucket_i;
			node_t *new_prev_node_p = (node_t *)new_bucket;
			while (new_prev_node_p->next_node)
				new_prev_node_p = new_prev_node_p->next_node;
			new_prev_node_p->next_node = old_node_p;
			node_t *next_node_p = old_node_p->next_node;
			old_node_p->next_node = NULL;
			old_node_p = next_node_p;
		}
	}

	map->_buckets = new_bucket_vec;
	map->_bucket_count = size;
	ow_free(old_bucket_vec);
}

bool ow_hashmap_remove(
		struct ow_hashmap *map, const struct ow_hashmap_funcs *mf,
		const void *key) {
	const ow_hash_t hash = mf->key_hash(mf->context, key);
	bucket_t *const bucket = map->_buckets + (hash % map->_bucket_count);
	node_t *node_p = (node_t *)bucket;
	while (1) {
		node_t *next_node_p = node_p->next_node;
		if (next_node_p == NULL)
			return false;
		if (next_node_p->key_hash == hash &&
				mf->key_equal(mf->context, key, next_node_p->key)) {
			node_p->next_node = next_node_p->next_node;
			ow_free(next_node_p);
			return true;
		}
		node_p = next_node_p;
	}
}

void ow_hashmap_clear(struct ow_hashmap *map) {
	if (ow_unlikely(!map->_size))
		return;

	bucket_t *const buckets = map->_buckets;
	const size_t bucket_cnt = map->_bucket_count;
	for (size_t i = 0; i < bucket_cnt; i++) {
		node_t *node_p = buckets[i].nodes;
		while (node_p != NULL) {
			node_t *const next_node_p = node_p->next_node;
			ow_free(node_p);
			node_p = next_node_p;
		}
		buckets[i].nodes = NULL;
	}
	map->_size = 0;
}

void ow_hashmap_set(
		struct ow_hashmap *map, const struct ow_hashmap_funcs *mf,
		const void *key, void *val) {
	if (ow_unlikely(map->_size > map->_bucket_count))
		ow_hashmap_reserve(map, (map->_size - 1) * 2);

	const ow_hash_t hash = mf->key_hash(mf->context, key);
	bucket_t *const bucket = map->_buckets + (hash % map->_bucket_count);
	node_t *node_p = (node_t *)bucket;
	while (1) {
		node_t *next_node_p = node_p->next_node;
		if (next_node_p == NULL)
			break;
		if (next_node_p->key_hash == hash &&
				mf->key_equal(mf->context, key, next_node_p->key)) {
			next_node_p->value = val;
			return;
		}
		node_p = next_node_p;
	}

	node_t *const new_node = ow_malloc(sizeof(node_t));
	new_node->next_node = NULL;
	new_node->key_hash = hash;
	new_node->key = (void *)key;
	new_node->value = val;
	node_p->next_node = new_node;
	map->_size++;
}

void *ow_hashmap_get(
		const struct ow_hashmap *map, const struct ow_hashmap_funcs *mf,
		const void *key) {
	const ow_hash_t hash = mf->key_hash(mf->context, key);
	bucket_t *const bucket = map->_buckets + (hash % map->_bucket_count);
	node_t *node_p = (node_t *)bucket;
	while (1) {
		node_t *next_node_p = node_p->next_node;
		if (next_node_p == NULL)
			return NULL;
		if (next_node_p->key_hash == hash &&
				mf->key_equal(mf->context, key, next_node_p->key))
			return next_node_p->value;
		node_p = next_node_p;
	}
}

int ow_hashmap_foreach(
		const struct ow_hashmap *map, ow_hashmap_walker_t walker, void *arg) {
	bucket_t *const buckets = map->_buckets;
	const size_t bucket_cnt = map->_bucket_count;
	for (size_t i = 0; i < bucket_cnt; i++) {
		for (node_t *node_p = buckets[i].nodes;
			node_p != NULL; node_p = node_p->next_node) {
			int ret = walker(arg, node_p->key, node_p->value);
			if (ret)
				return ret;
		}
	}
	return 0;
}
