#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <utilities/filesystem.h> // ow_path_char_t

#define OW_STREAM_OPEN_READ       0x0001
#define OW_STREAM_OPEN_WRITE      0x0002
#define OW_STREAM_OPEN_APPEND     0x0004
#define OW_STREAM_OPEN_CREATE     0x0008
#define OW_STREAM_OPEN_BINARY     0x0010

/// Input/output stream.
struct ow_stream;

/// Common head of a stream struct.
#define OW_STREAM_HEAD \
	const struct ow_stream_operations *_ops; \
	int _open_flags; \
// ^^^ OW_STREAM_HEAD ^^^

/// Functions to perform operations on a stream.
struct ow_stream_operations {
	void (*close)(struct ow_stream *stream);
	bool (*eof)(const struct ow_stream *stream);
	size_t (*read)(struct ow_stream *stream, void *buf, size_t buf_sz);
	size_t (*write)(struct ow_stream *stream, const void *data, size_t size);
	int (*getc)(struct ow_stream *stream);
	bool (*gets)(struct ow_stream *stream, char *buf, size_t buf_sz);
	int (*putc)(struct ow_stream *stream, int ch);
	bool (*puts)(struct ow_stream *stream, const char *str);
};

/// Destroy the stream.
void ow_stream_close(struct ow_stream *stream);
/// Check whether reaches end of stream.
bool ow_stream_eof(const struct ow_stream *stream);
/// Read data to buffer.
size_t ow_stream_read(struct ow_stream *stream, void *buf, size_t buf_sz);
/// Write data.
size_t ow_stream_write(struct ow_stream *stream, const void *data, size_t size);
/// Get next char. Return EOF on failure.
int ow_stream_getc(struct ow_stream *stream);
/// Get next line.
bool ow_stream_gets(struct ow_stream *stream, char *buf, size_t buf_sz);
/// Print a char.
int ow_stream_putc(struct ow_stream *stream, int ch);
/// Print a string.
bool ow_stream_puts(struct ow_stream *stream, const char *str);

/// Create a file stream.
struct ow_stream *ow_stream_open_file(const ow_path_char_t *file, int flags);
/// Create a string stream or string view stream (without write flag).
struct ow_stream *ow_stream_open_string(const char *str, size_t len, int flags);
/// Get stdin stream.
struct ow_stream *ow_stream_stdin(void);
/// Get stdout stream.
struct ow_stream *ow_stream_stdout(void);
/// Get stderr stream.
struct ow_stream *ow_stream_stderr(void);

/// File stream.
struct ow_file_stream {
	OW_STREAM_HEAD
	void *_file;
};

extern const struct ow_stream_operations ow_file_stream_operations;
bool ow_file_stream_open(struct ow_file_stream *stream, const ow_path_char_t *file, int flags);
void ow_file_stream_close(struct ow_file_stream *stream);
bool ow_file_stream_eof(const struct ow_file_stream *stream);
size_t ow_file_stream_read(struct ow_file_stream *stream, void *buf, size_t buf_sz);
size_t ow_file_stream_write(struct ow_file_stream *stream, const void *data, size_t size);
int ow_file_stream_getc(struct ow_file_stream *stream);
bool ow_file_stream_gets(struct ow_file_stream *stream, char *buf, size_t buf_sz);
int ow_file_stream_putc(struct ow_file_stream *stream, int ch);
bool ow_file_stream_puts(struct ow_file_stream *stream, const char *str);

/// String view stream, read-only.
struct ow_strview_stream {
	OW_STREAM_HEAD
	const char *begin;
	const char *end;
	const char *current;
};

extern const struct ow_stream_operations ow_strview_stream_operations;
bool ow_strview_stream_open(struct ow_strview_stream *stream, const char *str, size_t len);
void ow_strview_stream_close(struct ow_strview_stream *stream);
bool ow_strview_stream_eof(const struct ow_strview_stream *stream);
size_t ow_strview_stream_read(struct ow_strview_stream *stream, void *buf, size_t buf_sz);
size_t ow_strview_stream_write(struct ow_strview_stream *stream, const void *data, size_t size);
int ow_strview_stream_getc(struct ow_strview_stream *stream);
bool ow_strview_stream_gets(struct ow_strview_stream *stream, char *buf, size_t buf_sz);
int ow_strview_stream_putc(struct ow_strview_stream *stream, int ch);
bool ow_strview_stream_puts(struct ow_strview_stream *stream, const char *str);

/// String stream.
struct ow_string_stream {
	OW_STREAM_HEAD
	char *begin;
	char *end;
	char *current;
};

extern const struct ow_stream_operations ow_string_stream_operations;
bool ow_string_stream_open(struct ow_string_stream *stream);
void ow_string_stream_close(struct ow_string_stream *stream);
bool ow_string_stream_eof(const struct ow_string_stream *stream);
size_t ow_string_stream_read(struct ow_string_stream *stream, void *buf, size_t buf_sz);
size_t ow_string_stream_write(struct ow_string_stream *stream, const void *data, size_t size);
int ow_string_stream_getc(struct ow_string_stream *stream);
bool ow_string_stream_gets(struct ow_string_stream *stream, char *buf, size_t buf_sz);
int ow_string_stream_putc(struct ow_string_stream *stream, int ch);
bool ow_string_stream_puts(struct ow_string_stream *stream, const char *str);
char *ow_string_stream_data(const struct ow_string_stream *stream, size_t *size);
