#include "stream.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <utilities/attributes.h>
#include <utilities/malloc.h>

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

struct ow_istream_sv {
	const char *begin;
	const char *end;
	const char *current;
};

struct ow_istream *ow_istream_open(const ow_path_char_t *path) {
	FILE *const fp =
#if _IS_WINDOWS_
		_wfopen
#else
		fopen
#endif
		(path, OW_PATH_STR("r"));
	return (struct ow_istream *)fp;
}

struct ow_istream *ow_istream_open_mem(const char *s, size_t n) {
	struct ow_istream_sv *const res = ow_malloc(sizeof(struct ow_istream_sv));
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
		ow_free(ptr_rm_tag(s));
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

struct ow_iostream_sb {
	char *begin;
	char *end;
	char *current;
};

static_assert(
	offsetof(struct ow_istream_sv, begin) == offsetof(struct ow_iostream_sb, begin) &&
	offsetof(struct ow_istream_sv, end) == offsetof(struct ow_iostream_sb, end) &&
	offsetof(struct ow_istream_sv, current) == offsetof(struct ow_iostream_sb, current),
	"");

struct ow_iostream *ow_iostream_open(const ow_path_char_t *path, bool readable) {
	FILE *const fp =
#if _IS_WINDOWS_
		_wfopen
#else
		fopen
#endif
		(path, readable ? OW_PATH_STR("w+") : OW_PATH_STR("w"));
	return (struct ow_iostream *)fp;
}

struct ow_iostream *ow_iostream_open_mem(size_t n) {
	if (n < 32)
		n = 32;
	struct ow_iostream_sb *const res = ow_malloc(sizeof(struct ow_iostream_sb));
	res->begin = ow_malloc(n);
	res->end = res->begin + n;
	res->current = res->begin;
	return ptr_add_tag(res);
}

struct ow_iostream *ow_iostream_stdout(void) {
	return (struct ow_iostream *)stdout;
}

struct ow_iostream *ow_iostream_stderr(void) {
	return (struct ow_iostream *)stderr;
}

void ow_iostream_close(struct ow_iostream *s) {
	if (!ptr_is_tagged(s)) {
		fclose((FILE *)s);
		return;
	}

	struct ow_iostream_sb *const sb = ptr_rm_tag(s);
	ow_free(sb->begin);
	ow_free(sb);
}

bool ow_iostream_eof(struct ow_iostream *s) {
	return ow_istream_eof((struct ow_istream *)s);
}

int ow_iostream_getc(struct ow_iostream *s) {
	return ow_istream_getc((struct ow_istream *)s);
}

bool ow_iostream_gets(struct ow_iostream *s, char *buf, size_t buf_sz) {
	return ow_istream_gets((struct ow_istream *)s, buf, buf_sz);
}

size_t ow_iostream_read(struct ow_iostream *s, void *buf, size_t buf_sz) {
	return ow_istream_read((struct ow_istream *)s, buf, buf_sz);
}

int ow_iostream_putc(struct ow_iostream *s, int ch) {
	if (!ptr_is_tagged(s))
		return fputc(ch, (FILE *)s);

	const char c = (char)ch;
	return ow_iostream_write(s, &c, 1) == 1 ? ch : EOF;
}

bool ow_iostream_puts(struct ow_iostream *s, const char *str) {
	if (!ptr_is_tagged(s))
		return fputs(str, (FILE *)s);

	const size_t str_len = strlen(str);
	return ow_iostream_write(s, str, str_len) == str_len;
}

size_t ow_iostream_write(struct ow_iostream *s, const void *data, size_t size) {
	if (!ptr_is_tagged(s))
		return fwrite(data, 1, size, (FILE *)s);

	struct ow_iostream_sb *const sb = ptr_rm_tag(s);
	char *new_current = sb->current + size;
	if (ow_unlikely(new_current > sb->end)) {
		const size_t min_size = new_current - sb->begin;
		size_t new_size = (sb->end - sb->begin) * 2;
		if (new_size < min_size)
			new_size = min_size;
		const size_t current_off = sb->current - sb->begin;
		sb->begin = ow_realloc(sb->begin, new_size);
		sb->end = sb->begin + new_size;
		sb->current = sb->begin + current_off;
		new_current = sb->current + size;
	}
	memcpy(sb->current, data, size);
	sb->current = new_current;
	return size;
}

bool ow_istream_data(
		struct ow_iostream *s, const char *str_range[OW_PARAMARRAY_STATIC 2]) {
	if (!ptr_is_tagged(s))
		return false;

	struct ow_iostream_sb *const sb = ptr_rm_tag(s);
	str_range[0] = sb->begin;
	str_range[1] = sb->end;
	return true;
}
