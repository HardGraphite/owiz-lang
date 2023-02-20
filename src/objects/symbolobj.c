#include "symbolobj.h"

#include <assert.h>
#include <string.h>

#include "classes.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/hash.h>
#include <utilities/hashmap.h>
#include <utilities/memalloc.h>
#include <utilities/round.h>

struct ow_symbol_pool {
	struct ow_hashmap symbols; // { ow_symbol_obj, ow_symbol_obj }
};

struct ow_symbol_obj {
	OW_EXTENDED_OBJECT_HEAD
	ow_hash_t hash;
	size_t    size;
	char      str[];
};

struct ow_symbol_pool *ow_symbol_pool_new(void) {
	struct ow_symbol_pool *const sp = ow_malloc(sizeof(struct ow_symbol_pool));
	ow_hashmap_init(&sp->symbols, 64);
	return sp;
}

void ow_symbol_pool_del(struct ow_symbol_pool *sp) {
	ow_hashmap_fini(&sp->symbols);
	ow_free(sp);
}

int _ow_symbol_pool_gc_handler_pool_walker(void *arg, const void *key, void *val) {
	struct ow_array *const unreachable_symbols = arg;
	struct ow_symbol_obj *const sym = val;
	ow_unused_var(key);
	assert(key == val);
	const bool sym_marked =
		ow_object_meta_get_flag(&ow_object_from(sym)->_meta, OW_OBJMETA_FLAG_MARKED);
	if (ow_unlikely(!sym_marked))
		ow_array_append(unreachable_symbols, sym);
	return 0;
}

void _ow_symbol_pool_gc_handler(struct ow_machine *om, struct ow_symbol_pool *sp) {
	// Assume that all reachable symbols have been marked.

	ow_unused_var(om);
	assert(om->symbol_pool == sp);

	struct ow_array unreachable_symbols;
	ow_array_init(&unreachable_symbols, 0);
	ow_hashmap_foreach(
		&sp->symbols, _ow_symbol_pool_gc_handler_pool_walker, &unreachable_symbols);
	for (size_t i = 0, n = ow_array_size(&unreachable_symbols); i < n; i++) {
		void *const key = ow_array_at(&unreachable_symbols, i);
		ow_hashmap_remove(&sp->symbols, &ow_symbol_obj_hashmap_funcs, key);
	}
	ow_array_fini(&unreachable_symbols);
}

static bool _ow_symbol_pool_find_key_equal(
		void *ctx, const void *key_new, const void *key_stored) {
	const size_t n = (size_t)ctx;
	const char *const s = key_new;
	const struct ow_symbol_obj *const sym_obj = key_stored;
	if (n != sym_obj->size)
		return false;
	return memcmp(s, sym_obj->str, n) == 0;
}

static ow_hash_t _ow_symbol_pool_find_key_hash(
		void *ctx, const void *key_new) {
	const size_t n = (size_t)ctx;
	const char *const s = key_new;
	return ow_hash_bytes(s, n);
}

static struct ow_symbol_obj *ow_symbol_pool_find(
		struct ow_symbol_pool *sp, const char *s, size_t n) {
	const struct ow_hashmap_funcs mf = {
		_ow_symbol_pool_find_key_equal,
		_ow_symbol_pool_find_key_hash,
		(void *)n,
	};
	return ow_hashmap_get(&sp->symbols, &mf, s);
}

static bool _ow_symbol_pool_add_key_equal(
		void *ctx, const void *key_new, const void *key_stored) {
	ow_unused_var(ctx);
	ow_unused_var(key_new);
	ow_unused_var(key_stored);
	assert(!strcmp(((struct ow_symbol_obj *)key_new)->str,
		((struct ow_symbol_obj *)key_stored)->str));
	return false;
}

static ow_hash_t _ow_symbol_pool_add_key_hash(
		void *ctx, const void *key_new) {
	ow_unused_var(ctx);
	const struct ow_symbol_obj *const sym_obj = key_new;
	return sym_obj->hash;
}

static const struct ow_hashmap_funcs _ow_symbol_pool_add_mf = {
		_ow_symbol_pool_add_key_equal,
		_ow_symbol_pool_add_key_hash,
		NULL,
};

static void ow_symbol_pool_add(
		struct ow_symbol_pool *sp, struct ow_symbol_obj *obj) {
	ow_hashmap_set(&sp->symbols, &_ow_symbol_pool_add_mf, obj, obj);
}

struct ow_symbol_obj *ow_symbol_obj_new(
		struct ow_machine *om, const char *s, size_t n) {
	struct ow_symbol_obj *obj;

	if (ow_unlikely(n == (size_t)-1))
		n = strlen(s);
	obj = ow_symbol_pool_find(om->symbol_pool, s, n);
	if (ow_likely(obj))
		return obj;

	obj = ow_object_cast(
		ow_objmem_allocate(
			om, om->builtin_classes->symbol,
			(ow_round_up_to(sizeof(void *), n + 1) / sizeof(void *))),
		struct ow_symbol_obj);
	obj->hash = ow_hash_bytes(s, n);
	obj->size = n;
	memcpy(obj->str, s, n);
	obj->str[n] = '\0';

	ow_symbol_pool_add(om->symbol_pool, obj);

	return obj;
}

size_t ow_symbol_obj_size(const struct ow_symbol_obj *self) {
	return self->size;
}

const char *ow_symbol_obj_data(const struct ow_symbol_obj *self) {
	return self->str;
}

static bool _ow_symbol_obj_hashmap_funcs_key_equal(
		void *ctx, const void *key_new, const void *key_stored) {
	ow_unused_var(ctx);
	const bool res = key_new == key_stored;
	assert(!strcmp(((struct ow_symbol_obj *)key_new)->str,
		((struct ow_symbol_obj *)key_stored)->str) == res);
	return res;
}

static ow_hash_t _ow_symbol_obj_hashmap_funcs_key_hash(
		void *ctx, const void *key_new) {
	ow_unused_var(ctx);
	const struct ow_symbol_obj *const sym_obj = key_new;
	return sym_obj->hash;
}

const struct ow_hashmap_funcs ow_symbol_obj_hashmap_funcs = {
	_ow_symbol_obj_hashmap_funcs_key_equal,
	_ow_symbol_obj_hashmap_funcs_key_hash,
	NULL,
};

static const struct ow_native_func_def symbol_methods[] = {
	{NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(symbol) = {
	.name      = "Symbol",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_symbol_obj),
	.methods   = symbol_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = true,
};
