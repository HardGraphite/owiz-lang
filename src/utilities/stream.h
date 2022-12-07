#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <compat/kw_static.h>
#include <utilities/filesystem.h> // ow_path_char_t

/// Input stream.
struct ow_istream;

/// Open a file as input stream.
struct ow_istream *ow_istream_open(const ow_path_char_t *path);
/// Take a string as an input stream.
struct ow_istream *ow_istream_open_mem(const char *s, size_t n);
/// Get stdin.
struct ow_istream *ow_istream_stdin(void);
/// Close an input stream.
void ow_istream_close(struct ow_istream *s);
/// Check whether reaches the end of stream.
bool ow_istream_eof(struct ow_istream *s);
/// Get next char. Return EOF on failure.
int ow_istream_getc(struct ow_istream *s);
/// Get next line.
bool ow_istream_gets(struct ow_istream *s, char *buf, size_t buf_sz);
/// Read data.
size_t ow_istream_read(struct ow_istream *s, void *buf, size_t buf_sz);

/// Input/output stream.
struct ow_iostream;

/// Open a file as IO stream.
struct ow_iostream *ow_iostream_open(const ow_path_char_t *path, bool readable);
/// Open an in-memory IO stream.
struct ow_iostream *ow_iostream_open_mem(size_t n);
/// Get stdout.
struct ow_iostream *ow_iostream_stdout(void);
/// Get stderr.
struct ow_iostream *ow_iostream_stderr(void);
/// Close an IO stream.
void ow_iostream_close(struct ow_iostream *s);
/// Check whether reaches the end of stream.
bool ow_iostream_eof(struct ow_iostream *s);
/// Get next char. Return EOF on failure.
int ow_iostream_getc(struct ow_iostream *s);
/// Get next line.
bool ow_iostream_gets(struct ow_iostream *s, char *buf, size_t buf_sz);
/// Read data.
size_t ow_iostream_read(struct ow_iostream *s, void *buf, size_t buf_sz);
/// Print a char.
int ow_iostream_putc(struct ow_iostream *s, int ch);
/// Print a string.
bool ow_iostream_puts(struct ow_iostream *s, const char *str);
/// Write data.
size_t ow_iostream_write(struct ow_iostream *s, const void *data, size_t size);
/// Get the content of string stream, if the stream is opened using ow_iostream_open_mem().
bool ow_istream_data(struct ow_iostream *s, const char *str_range[OW_PARAMARRAY_STATIC 2]);
