#pragma once

#include <stdbool.h>
#include <stddef.h>

// Input stream.
struct ow_istream;
// Input/output stream.
struct ow_iostream;

// Open a file as input stream.
struct ow_istream *ow_istream_open(const char *path);
// Take a string as an input stream.
struct ow_istream *ow_istream_open_mem(const char *s, size_t n);
// Get stdin.
struct ow_istream *ow_istream_stdin(void);
// Close an input stream.
void ow_istream_close(struct ow_istream *s);
// Check whether reaches the end of stream.
bool ow_istream_eof(struct ow_istream *s);
// Get next char. Return EOF on failure.
int ow_istream_getc(struct ow_istream *s);
// Get next line.
bool ow_istream_gets(struct ow_istream *s, char *buf, size_t buf_sz);
// Read data.
size_t ow_istream_read(struct ow_istream *s, void *buf, size_t buf_sz);
