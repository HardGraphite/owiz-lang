#include "strings.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#	include <compats/msvc_stdatomic.h>
#else
#	include <stdatomic.h>
#endif

ow_nodiscard char *ow_strdup(const char *s) {
	const size_t n = strlen(s) + 1;
	char *const new_s = malloc(n);
	return memcpy(new_s, s, n);
}

struct ow_sharedstr {
	atomic_uint ref_cnt;
	size_t size;
	char   data[];
};

ow_nodiscard struct ow_sharedstr *ow_sharedstr_new(const char *s, size_t n) {
	if (n == (size_t)-1)
		n = strlen(s);
	struct ow_sharedstr *const ss = malloc(sizeof(struct ow_sharedstr) + n + 1);
	ss->ref_cnt = 1;
	ss->size = n;
	memcpy(ss->data, s, n);
	ss->data[n] = '\0';
	return ss;
}

static void ow_sharedstr_del(struct ow_sharedstr *ss) {
	assert(!atomic_load(&ss->ref_cnt));
	free(ss);
}

struct ow_sharedstr *ow_sharedstr_ref(struct ow_sharedstr *ss) {
	atomic_fetch_add(&ss->ref_cnt, 1);
	return ss;
}

void ow_sharedstr_unref(struct ow_sharedstr *ss) {
	assert(atomic_load(&ss->ref_cnt));
	const unsigned int old_cnt = atomic_fetch_sub(&ss->ref_cnt, 1);
	if (ow_unlikely(old_cnt == 1))
		ow_sharedstr_del(ss);
}

const char *ow_sharedstr_data(struct ow_sharedstr *ss) {
	assert(atomic_load(&ss->ref_cnt));
	return ss->data;
}

size_t ow_sharedstr_size(struct ow_sharedstr *ss) {
	assert(atomic_load(&ss->ref_cnt));
	return ss->size;
}

void ow_dynamicstr_init(struct ow_dynamicstr *ds, size_t n) {
	if (n < 7)
		n = 7;
	ds->_str = malloc(n + 1);
	ds->_cap = n;
	ds->_len = 0;
}

void ow_dynamicstr_fini(struct ow_dynamicstr *ds) {
	free(ds->_str);
}

void ow_dynamicstr_reserve(struct ow_dynamicstr *ds, size_t n) {
	if (ds->_cap < n) {
		ds->_str = realloc(ds->_str, n + 1);
		ds->_cap = n;
	}
}

void ow_dynamicstr_assign(struct ow_dynamicstr *ds, const char *s, size_t n) {
	ow_dynamicstr_clear(ds);
	ow_dynamicstr_append(ds, s, n);
}

void ow_dynamicstr_append(struct ow_dynamicstr *ds, const char *s, size_t n) {
	const size_t new_len = ds->_len + n;
	if (new_len > ds->_cap) {
		size_t new_cap = ds->_cap * 2;
		if (new_len > new_cap)
			new_cap = new_len;
		ds->_str = realloc(ds->_str, new_cap + 1);
		ds->_cap = new_cap;
	}
	memcpy(ds->_str + ds->_len, s, n);
	ds->_str[new_len] = '\0';
	ds->_len = new_len;
}

void ow_dynamicstr_append_char(struct ow_dynamicstr *ds, char c) {
	if (ds->_len + 1 > ds->_cap) {
		const size_t new_cap = ds->_cap * 2;
		ds->_str = realloc(ds->_str, new_cap + 1);
		ds->_cap = new_cap;
		assert(new_cap >= ds->_len + 1);
	}
	ds->_str[ds->_len] = c;
	ds->_len++;
	ds->_str[ds->_len] = '\0';
}
