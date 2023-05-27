#pragma once

#include <stddef.h>

#include <utilities/attributes.h>

struct ow_hashmap_funcs;

/// Duplicate a NUL-terminated string.
ow_nodiscard char *ow_strdup(const char *s);

/// Shared immutable string.
struct ow_sharedstr;

/// Create shared string from existing string. `n = -1` to auto calc length.
ow_nodiscard struct ow_sharedstr *ow_sharedstr_new(const char *s, size_t n);
/// Increase reference to the string. Return the string itself.
struct ow_sharedstr *ow_sharedstr_ref(struct ow_sharedstr *ss);
/// Decrease reference to the string.
void ow_sharedstr_unref(struct ow_sharedstr *ss);
/// Get data (aka pointer to chars) of the string.
const char *ow_sharedstr_data(struct ow_sharedstr *ss);
/// Get size (aka length in byte) of the string.
size_t ow_sharedstr_size(struct ow_sharedstr *ss);

/// Hash map functions for hash maps that use shared strings as keys.
extern const struct ow_hashmap_funcs ow_sharedstr_hashmap_funcs;

/// String with dynamic length.
struct ow_dynamicstr {
    char  *_str;
    size_t _cap;
    size_t _len;
};

/// Initialize a string.
void ow_dynamicstr_init(struct ow_dynamicstr *ds, size_t n);
/// Finalize a string.
void ow_dynamicstr_fini(struct ow_dynamicstr *ds);
/// Reserve capacity.
void ow_dynamicstr_reserve(struct ow_dynamicstr *ds, size_t n);
/// Assign to the string.
void ow_dynamicstr_assign(struct ow_dynamicstr *ds, const char *s, size_t n);
/// Append string.
void ow_dynamicstr_append(struct ow_dynamicstr *ds, const char *s, size_t n);
/// Append char.
void ow_dynamicstr_append_char(struct ow_dynamicstr *ds, char c);
/// Delete string contents.
ow_static_inline void ow_dynamicstr_clear(struct ow_dynamicstr *ds) { ds->_len = 0; }
/// Get string data, which is a NUL-terminated string.
ow_static_inline char *ow_dynamicstr_data(const struct ow_dynamicstr *ds) { return ds->_str; }
/// Get string size (aka length in byte).
ow_static_inline size_t ow_dynamicstr_size(const struct ow_dynamicstr *ds) { return ds->_len; }
