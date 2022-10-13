#include "stream.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utilities/attributes.h>

struct ow_istream_sv {
	const char *begin;
	const char *end;
	const char *current;
};

static bool ptr_is_tagged(void *p) {
	return (uintptr_t)p & 1;
}

static void *ptr_add_tag(void *p) {
	assert(!ptr_is_tagged(p));
	return (void *)((uintptr_t)p | 1);
}

static void *ptr_rm_tag(void *p) {
	assert(ptr_is_tagged(p));
	return (void *)((uintptr_t)p - 1);
}

struct ow_istream *ow_istream_open(const char *path) {
	FILE *const fp = fopen(path, "r");
	return (struct ow_istream *)fp;
}

struct ow_istream *ow_istream_open_mem(const char *s, size_t n) {
	struct ow_istream_sv *const res = malloc(sizeof(struct ow_istream_sv));
	res->begin = s;
	res->end = s + (n == (size_t)-1 ? strlen(s) : n);
	res->current = s;
	return ptr_add_tag(res);
}

struct ow_istream *ow_istream_stdin(void) {
	return (struct ow_istream *)stdin;
}

void ow_istream_close(struct ow_istream *s) {
	if (ptr_is_tagged(s))
		free(ptr_rm_tag(s));
	else
		fclose((FILE *)s);
}

bool ow_istream_eof(struct ow_istream *s) {
	if (!ptr_is_tagged(s))
		return feof((FILE *)s);

	const struct ow_istream_sv *const sv = ptr_rm_tag(s);
	assert(sv->current <= sv->end);
	return sv->current == sv->end;
}

int ow_istream_getc(struct ow_istream *s) {
	if (!ptr_is_tagged(s))
		return fgetc((FILE *)s);

	struct ow_istream_sv *const sv = ptr_rm_tag(s);
	assert(sv->current <= sv->end);
	if (ow_unlikely(sv->current == sv->end))
		return EOF;
	return *sv->current++;
}

bool ow_istream_gets(struct ow_istream *s, char *buf, size_t buf_sz) {
	if (!ptr_is_tagged(s))
		return fgets(buf, (int)buf_sz, (FILE *)s);

	struct ow_istream_sv *const sv = ptr_rm_tag(s);
	assert(sv->current <= sv->end);
	const char *const lf_pos = strchr(sv->current, '\n');
	const size_t read_n = lf_pos ?
		sv->current + buf_sz > lf_pos + 1 ? (size_t)(lf_pos + 1 - sv->current) : buf_sz - 1 :
		sv->current + buf_sz > sv->end ? (size_t)(sv->end - sv->current) : buf_sz - 1;
	memcpy(buf, sv->current, read_n);
	sv->current += read_n;
	return buf[read_n - 1] == '\n';
}

size_t ow_istream_read(struct ow_istream *s, void *buf, size_t buf_sz) {
	if (!ptr_is_tagged(s))
		return fread(buf, 1, buf_sz, (FILE *)s);

	struct ow_istream_sv *const sv = ptr_rm_tag(s);
	assert(sv->current <= sv->end);
	const size_t rest_sz = sv->end - sv->current;
	const size_t n = rest_sz < buf_sz ? rest_sz : buf_sz;
	memcpy(buf, sv->current, n);
	sv->current += n;
	return n;
}
