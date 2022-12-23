#include "stream.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <utilities/attributes.h>
#include <utilities/malloc.h>

struct ow_stream {
	OW_STREAM_HEAD
};

typedef void (*sop_close_t)(struct ow_stream *);
typedef bool (*sop_eof_t)(const struct ow_stream *);
typedef size_t (*sop_read_t)(struct ow_stream *, void *, size_t);
typedef size_t (*sop_write_t)(struct ow_stream *, const void *, size_t);
typedef int (*sop_getc_t)(struct ow_stream *);
typedef bool (*sop_gets_t)(struct ow_stream *, char *, size_t);
typedef int (*sop_putc_t)(struct ow_stream *, int);
typedef bool (*sop_puts_t)(struct ow_stream *, const char *);

void ow_stream_close(struct ow_stream *stream) {
	stream->_ops->close(stream);
	ow_free(stream);
}

bool ow_stream_eof(const struct ow_stream *stream) {
	return stream->_ops->eof(stream);
}

size_t ow_stream_read(struct ow_stream *stream, void *buf, size_t buf_sz) {
	return stream->_ops->read(stream, buf, buf_sz);
}

size_t ow_stream_write(struct ow_stream *stream, const void *data, size_t size) {
	return stream->_ops->write(stream, data, size);
}

int ow_stream_getc(struct ow_stream *stream) {
	return stream->_ops->getc(stream);
}

bool ow_stream_gets(struct ow_stream *stream, char *buf, size_t buf_sz) {
	return stream->_ops->gets(stream, buf, buf_sz);
}

int ow_stream_putc(struct ow_stream *stream, int ch) {
	return stream->_ops->putc(stream, ch);
}

bool ow_stream_puts(struct ow_stream *stream, const char *str) {
	return stream->_ops->puts(stream, str);
}

struct ow_stream *ow_stream_open_file(const ow_path_char_t *file, int flags) {
	struct ow_file_stream *const fs = ow_malloc(sizeof(struct ow_file_stream));
	if (ow_file_stream_open(fs, file, flags))
		return (struct ow_stream *)fs;
	ow_free(fs);
	return NULL;
}

struct ow_stream *ow_stream_open_string(const char *str, size_t len, int flags) {
	if (flags & OW_STREAM_OPEN_WRITE) {
		struct ow_string_stream *const ss =
			ow_malloc(sizeof(struct ow_string_stream));
		ow_string_stream_open(ss);
		if (len)
			ow_string_stream_write(ss, str, len);
		return (struct ow_stream *)ss;
	} else {
		struct ow_strview_stream *const ss =
			ow_malloc(sizeof(struct ow_strview_stream));
		ow_strview_stream_open(ss, str, len);
		return (struct ow_stream *)ss;
	}
}

static struct ow_file_stream stdin_stream = {
	._ops = &ow_file_stream_operations,
	._open_flags = OW_STREAM_OPEN_READ,
	._file = NULL,
};

static struct ow_file_stream stdout_stream = {
	._ops = &ow_file_stream_operations,
	._open_flags = OW_STREAM_OPEN_WRITE,
	._file = NULL,
};

static struct ow_file_stream stderr_stream = {
	._ops = &ow_file_stream_operations,
	._open_flags = OW_STREAM_OPEN_WRITE,
	._file = NULL,
};

struct ow_stream *ow_stream_stdin(void) {
	if (ow_unlikely(!stdin_stream._file))
		stdin_stream._file = stdin;
	return (struct ow_stream *)&stdin_stream;
}

struct ow_stream *ow_stream_stdout(void) {
	if (ow_unlikely(!stdout_stream._file))
		stdout_stream._file = stdout;
	return (struct ow_stream *)&stdout_stream;
}

struct ow_stream *ow_stream_stderr(void) {
	if (ow_unlikely(!stderr_stream._file))
		stderr_stream._file = stderr;
	return (struct ow_stream *)&stderr_stream;
}

const struct ow_stream_operations ow_file_stream_operations = {
	.close = (sop_close_t)ow_file_stream_close,
	.eof   = (sop_eof_t)  ow_file_stream_eof  ,
	.read  = (sop_read_t) ow_file_stream_read ,
	.write = (sop_write_t)ow_file_stream_write,
	.getc  = (sop_getc_t) ow_file_stream_getc ,
	.gets  = (sop_gets_t) ow_file_stream_gets ,
	.putc  = (sop_putc_t) ow_file_stream_putc ,
	.puts  = (sop_puts_t) ow_file_stream_puts ,
};

bool ow_file_stream_open(
		struct ow_file_stream *stream, const ow_path_char_t *file, int flags) {
	ow_path_char_t open_mode[8];
	ow_path_char_t *p = open_mode;
	if (flags & OW_STREAM_OPEN_READ) {
		if (flags & OW_STREAM_OPEN_APPEND)
			*p++ = OW_PATH_CHR('a'), *p++ = OW_PATH_CHR('+');
		else if (flags & OW_STREAM_OPEN_WRITE)
			*p++ = flags & OW_STREAM_OPEN_CREATE ? OW_PATH_CHR('w') : OW_PATH_CHR('r'),
			*p++ = OW_PATH_CHR('+');
		else
			*p++ = OW_PATH_CHR('r');
	} else if (flags & OW_STREAM_OPEN_WRITE) {
		if (flags & OW_STREAM_OPEN_APPEND)
			*p++ = OW_PATH_CHR('a');
		else
			*p++ = OW_PATH_CHR('w');
	} else if (flags & OW_STREAM_OPEN_APPEND) {
		*p++ = OW_PATH_CHR('a');
	} else {
		*p++ = OW_PATH_CHR('r');
	}
	if (flags & OW_STREAM_OPEN_BINARY)
		*p++ = OW_PATH_CHR('b');
	*p = 0;
	FILE *const fp =
#if _IS_WINDOWS_
		_wfopen
#else
		fopen
#endif
			(file, open_mode);
	if (!fp)
		return false;

	stream->_ops = &ow_file_stream_operations;
	stream->_open_flags = flags;
	stream->_file = fp;
	return true;
}

void ow_file_stream_close(struct ow_file_stream *stream) {
	fclose(stream->_file);
	stream->_file = NULL;
}

bool ow_file_stream_eof(const struct ow_file_stream *stream) {
	return feof(stream->_file);
}

size_t ow_file_stream_read(
		struct ow_file_stream *stream, void *buf, size_t buf_sz) {
	return fread(buf, 1, buf_sz, stream->_file);
}

size_t ow_file_stream_write(
		struct ow_file_stream *stream, const void *data, size_t size) {
	return fwrite(data, 1, size, stream->_file);
}

int ow_file_stream_getc(struct ow_file_stream *stream) {
	return fgetc(stream->_file);
}

bool ow_file_stream_gets(struct ow_file_stream *stream, char *buf, size_t buf_sz) {
	assert(buf_sz <= INT_MAX);
	return fgets(buf, (int)buf_sz, stream->_file);
}

int ow_file_stream_putc(struct ow_file_stream *stream, int ch) {
	return fputc(ch, stream->_file);
}

bool ow_file_stream_puts(struct ow_file_stream *stream, const char *str) {
	return fputs(str, stream->_file) || !*str;
}

const struct ow_stream_operations ow_strview_stream_operations = {
	.close = (sop_close_t)ow_strview_stream_close,
	.eof   = (sop_eof_t)  ow_strview_stream_eof  ,
	.read  = (sop_read_t) ow_strview_stream_read ,
	.write = (sop_write_t)ow_strview_stream_write,
	.getc  = (sop_getc_t) ow_strview_stream_getc ,
	.gets  = (sop_gets_t) ow_strview_stream_gets ,
	.putc  = (sop_putc_t) ow_strview_stream_putc ,
	.puts  = (sop_puts_t) ow_strview_stream_puts ,
};

void ow_strview_stream_open(
		struct ow_strview_stream *stream, const char *str, size_t len) {
	stream->_ops = &ow_strview_stream_operations;
	stream->_open_flags = OW_STREAM_OPEN_READ;
	stream->begin = str;
	stream->end = str + (len == (size_t)-1 ? strlen(str) : len);
	stream->current = str;
}

void ow_strview_stream_close(struct ow_strview_stream *stream) {
	ow_unused_var(stream);
}

bool ow_strview_stream_eof(const struct ow_strview_stream *stream) {
	assert(stream->current <= stream->end);
	return stream->current == stream->end;
}

size_t ow_strview_stream_read(
		struct ow_strview_stream *stream, void *buf, size_t buf_sz) {
	assert(stream->current <= stream->end);
	const size_t rest_sz = stream->end - stream->current;
	const size_t n = rest_sz < buf_sz ? rest_sz : buf_sz;
	memcpy(buf, stream->current, n);
	stream->current += n;
	return n;
}

size_t ow_strview_stream_write(
		struct ow_strview_stream *stream, const void *data, size_t size) {
	ow_unused_var(stream), ow_unused_var(data), ow_unused_var(size);
	return 0;
}

int ow_strview_stream_getc(struct ow_strview_stream *stream) {
	assert(stream->current <= stream->end);
	if (ow_unlikely(stream->current == stream->end))
		return EOF;
	return *stream->current++;
}

bool ow_strview_stream_gets(struct ow_strview_stream *stream, char *buf, size_t buf_sz) {
	assert(stream->current <= stream->end);
	const char *const lf_pos = strchr(stream->current, '\n');
	const size_t read_n = lf_pos ?
		stream->current + buf_sz > lf_pos + 1 ?
			(size_t)(lf_pos + 1 - stream->current) : buf_sz - 1 :
		stream->current + buf_sz > stream->end ?
			(size_t)(stream->end - stream->current) : buf_sz - 1;
	memcpy(buf, stream->current, read_n);
	stream->current += read_n;
	return buf[read_n - 1] == '\n';
}

int ow_strview_stream_putc(struct ow_strview_stream *stream, int ch) {
	ow_unused_var(stream), ow_unused_var(ch);
	return EOF;
}

bool ow_strview_stream_puts(struct ow_strview_stream *stream, const char *str) {
	ow_unused_var(stream), ow_unused_var(str);
	return false;
}

const struct ow_stream_operations ow_string_stream_operations = {
	.close = (sop_close_t)ow_string_stream_close,
	.eof   = (sop_eof_t)  ow_string_stream_eof  ,
	.read  = (sop_read_t) ow_string_stream_read ,
	.write = (sop_write_t)ow_string_stream_write,
	.getc  = (sop_getc_t) ow_string_stream_getc ,
	.gets  = (sop_gets_t) ow_string_stream_gets ,
	.putc  = (sop_putc_t) ow_string_stream_putc ,
	.puts  = (sop_puts_t) ow_string_stream_puts ,
};

void ow_string_stream_open(struct ow_string_stream *stream) {
	stream->_ops = &ow_string_stream_operations;
	stream->_open_flags = OW_STREAM_OPEN_READ | OW_STREAM_OPEN_WRITE;
	stream->begin   = NULL;
	stream->end     = NULL;
	stream->current = NULL;
}

void ow_string_stream_close(struct ow_string_stream *stream) {
	if (stream->begin)
		ow_free(stream->begin);
}

bool ow_string_stream_eof(const struct ow_string_stream *stream) {
	return ow_strview_stream_eof((const struct ow_strview_stream *)stream);
}

size_t ow_string_stream_read(
		struct ow_string_stream *stream, void *buf, size_t buf_sz) {
	return ow_strview_stream_read((struct ow_strview_stream *)stream, buf, buf_sz);
}

size_t ow_string_stream_write(
		struct ow_string_stream *stream, const void *data, size_t size) {
	char *new_current = stream->current + size;
	if (ow_unlikely(new_current > stream->end)) {
		const size_t min_size = new_current - stream->begin;
		size_t new_size = (stream->end - stream->begin) * 2;
		if (new_size < min_size)
			new_size = min_size;
		const size_t current_off = stream->current - stream->begin;
		stream->begin = ow_realloc(stream->begin, new_size);
		stream->end = stream->begin + new_size;
		stream->current = stream->begin + current_off;
		new_current = stream->current + size;
	}
	memcpy(stream->current, data, size);
	stream->current = new_current;
	return size;
}

int ow_string_stream_getc(struct ow_string_stream *stream) {
	return ow_strview_stream_getc((struct ow_strview_stream *)stream);
}

bool ow_string_stream_gets(struct ow_string_stream *stream, char *buf, size_t buf_sz) {
	return ow_strview_stream_gets((struct ow_strview_stream *)stream, buf, buf_sz);
}

int ow_string_stream_putc(struct ow_string_stream *stream, int ch) {
	const char c = (char)ch;
	return ow_string_stream_write(stream, &c, 1) == 1 ? ch : EOF;
}

bool ow_string_stream_puts(struct ow_string_stream *stream, const char *str) {
	const size_t str_len = strlen(str);
	return ow_string_stream_write(stream, str, str_len) == str_len;
}

char *ow_string_stream_data(const struct ow_string_stream *stream, size_t *size) {
	if (size)
		*size = (size_t)(stream->end - stream->begin);
	return stream->begin;
}
