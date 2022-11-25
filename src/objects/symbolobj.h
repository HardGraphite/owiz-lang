#pragma once

#include <stddef.h>

struct ow_hashmap_funcs;
struct ow_machine;

/// Symbol pool, storing existing symbol objects.
struct ow_symbol_pool;

/// Create a symbol pool.
struct ow_symbol_pool *ow_symbol_pool_new(void);
/// Destroy a symbol pool.
void ow_symbol_pool_del(struct ow_symbol_pool *sp);
/// GC handler. Must be called after other GC markers.
void _ow_symbol_pool_gc_handler(struct ow_machine *om, struct ow_symbol_pool *sp);

/// Symbol object.
struct ow_symbol_obj;

/// Create a symbol object or find an existing one. `n = -1` to calc string length automatically.
struct ow_symbol_obj *ow_symbol_obj_new(struct ow_machine *om, const char *s, size_t n);
/// Get symbol string size (number of bytes).
size_t ow_symbol_obj_size(const struct ow_symbol_obj *self);
/// Get symbol string bytes.
const char *ow_symbol_obj_data(const struct ow_symbol_obj *self);

/// Hash map functions for hash maps that use symbol objects as keys.
extern const struct ow_hashmap_funcs ow_symbol_obj_hashmap_funcs;
