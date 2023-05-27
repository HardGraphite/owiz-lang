#pragma once

#include <stddef.h>

#include <compat/kw_static.h>

/// UTF-8 byte.
typedef unsigned char ow_char8_t;
/// UTF-32 char.
typedef unsigned int ow_wchar_t;

/// Convert Unicode code point to UTF-8 character. Return character length.
/// If the code point is illegal, return 0.
size_t ow_u8_from_unicode(ow_wchar_t code, ow_char8_t utf8_char_buf[OW_PARAMARRAY_STATIC 4]);
/// Convert UTF-8 character to Unicode code point. Return character length.
/// If the code point is illegal, return 0.
size_t ow_u8_to_unicode(ow_wchar_t *code, const ow_char8_t utf8_char[]);

size_t _ow_u8_charlen_mb(ow_char8_t u8_first_byte);
/// Calculate UTF-8 character length (number of bytes). Return 0 if it is illegal.
static inline size_t ow_u8_charlen(ow_char8_t u8_first_byte);
/// Calculate length of next UTF-8 character. If illegal byte is found,
/// return `-1 - off`, where `off` is the offset to the byte that is illegal.
int ow_u8_charlen_s(const ow_char8_t *u8_str, size_t n_bytes);

/// Get number of UTF-8 characters.
size_t ow_u8_strlen(const ow_char8_t *u8_str);
/// Check each byte and get number of UTF-8 characters. If illegal byte is found,
/// return `-1 - off` where `off` is the offset to the byte that is illegal.
int ow_u8_strlen_s(const ow_char8_t *u8_str, size_t n_bytes);

/// Get number of columns needed to display the given character.
size_t ow_u8_charwidth(const ow_char8_t u8_char[]);
/// Get number of columns needed to display the given string.
/// Param `end` shall be a pointer to the position after last character or NULL.
size_t ow_u8_strwidth(const ow_char8_t *u8_str, const ow_char8_t *end);

/// Move UTF-8 char pointer to `n_chars`-th char. If error occurs, return NULL.
/// If out of range, return the pointer to the ending NUL char.
ow_char8_t *ow_u8_strrmprefix(const ow_char8_t *u8_str, size_t n_chars);

static inline size_t ow_u8_charlen(ow_char8_t u8_first_byte) {
    return (u8_first_byte & 0x80) == 0 ? 1 : _ow_u8_charlen_mb(u8_first_byte);
}
