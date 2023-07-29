#include "unicode.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include <utilities/attributes.h>
#include <utilities/unreachable.h>

static_assert(sizeof(ow_char8_t) == sizeof(char), "wrong size of ow_char8_t");
static_assert(sizeof(ow_wchar_t) == 4, "wrong size of ow_wchar_t");

size_t ow_u8_from_unicode(
    ow_wchar_t code, ow_char8_t utf8_char_buf[OW_PARAMARRAY_STATIC 4]
) {
    if (code < 0x80) {
        utf8_char_buf[0] = (ow_char8_t)code;
        return 1;
    }
    if (code <= 0x7ff) {
        utf8_char_buf[0] = 0xc0 | (ow_char8_t)(code >> 6);
        utf8_char_buf[1] = 0x80 | (ow_char8_t)(code & 0x3f);
        return 2;
    }
    if (code <= 0xffff) {
        utf8_char_buf[0] = 0xe0 | (ow_char8_t)(code >> 12);
        utf8_char_buf[1] = 0x80 | (ow_char8_t)((code >> 6) & 0x3f);
        utf8_char_buf[2] = 0x80 | (ow_char8_t)(code & 0x3f);
        return 3;
    }
    if (code <= 0x1fffff) {
        utf8_char_buf[0] = 0xf0 | (ow_char8_t)(code >> 18);
        utf8_char_buf[1] = 0x80 | (ow_char8_t)((code >> 12) & 0x3f);
        utf8_char_buf[2] = 0x80 | (ow_char8_t)((code >> 6) & 0x3f);
        utf8_char_buf[3] = 0x80 | (ow_char8_t)(code & 0x3f);
        return 4;
    }
    return 0;
}

size_t ow_u8_to_unicode(ow_wchar_t *code, const ow_char8_t utf8_char[]) {
    const size_t n = ow_u8_charlen_s(utf8_char, 4);
    switch (n) {
    case 0:
        break;
    case 1:
        *code = utf8_char[0];
        break;
    case 2:
        *code =
            ((utf8_char[0] & 0x1f) << 6) |
            ((utf8_char[1] & 0x3f)     ) ;
        break;
    case 3:
        *code =
            ((utf8_char[0] & 0x0f) << 12) |
            ((utf8_char[1] & 0x3f) <<  6) |
            ((utf8_char[2] & 0x3f)      ) ;
        break;
    case 4:
        *code =
            ((utf8_char[0] & 0x07) << 18) |
            ((utf8_char[1] & 0x3f) << 12) |
            ((utf8_char[2] & 0x3f) <<  6) |
            ((utf8_char[3] & 0x3f)      ) ;
        break;
    default:
        ow_unreachable();
    }
    return n;
}

size_t _ow_u8_charlen_mb(ow_char8_t u8_first_byte) {
    assert((u8_first_byte & 0x80) != 0);
    if ((u8_first_byte & 0xe0) == 0xc0)
        return 2;
    if ((u8_first_byte & 0xf0) == 0xe0)
        return 3;
    if ((u8_first_byte & 0xf8) == 0xf0)
        return 4;
    return 0;
}

int ow_u8_charlen_s(const ow_char8_t *u8_str, size_t n_bytes) {
    if (ow_unlikely(n_bytes < 1))
        return -1 - 0;
    const ow_char8_t c1 = u8_str[0];
    if ((c1 & 0x80) == 0x00)
        return 1;
    if (ow_unlikely(n_bytes < 2))
        return -1 - 0;
    const ow_char8_t c2 = u8_str[1];
    if (ow_unlikely((c2 & 0xc0) != 0x80))
        return -1 - 1;
    if ((c1 & 0xe0) == 0xc0)
        return 2;
    if (ow_unlikely(n_bytes < 3))
        return -1 - 0;
    const ow_char8_t c3 = u8_str[2];
    if (ow_unlikely((c3 & 0xc0) != 0x80))
        return -1 - 2;
    if ((c1 & 0xf0) == 0xe0)
        return 3;
    if (ow_unlikely(n_bytes < 4))
        return -1 - 0;
    const ow_char8_t c4 = u8_str[3];
    if (ow_unlikely((c4 & 0xc0) != 0x80))
        return -1 - 3;
    if ((c1 & 0xf8) == 0xf0)
        return 4;
    return -1 - 0;
}

size_t ow_u8_strlen(const ow_char8_t *u8_str) {
    size_t len = 0;
    while (true) {
        const ow_char8_t c = *u8_str;
        if (ow_unlikely(!c))
            return len;
        const size_t n = ow_u8_charlen(c);
        if (ow_unlikely(!n))
            goto error;
        u8_str += n;
        len++;
    }
    ow_unreachable();
error:
    return len;
}

int ow_u8_strlen_s(const ow_char8_t *u8_str, size_t n_bytes) {
    size_t len = 0;
    const ow_char8_t *p = u8_str, *const p_end = p + n_bytes;
    if (ow_unlikely(!n_bytes))
        return 0;
    while (true) {
        if (ow_unlikely(!*p))
            goto ret_len;
        const size_t n = ow_u8_charlen_s(p, 4);
        if (ow_unlikely(!n))
            goto error;
        p += n;
        len++;
        if (ow_unlikely(p >= p_end)) {
            if (ow_unlikely(p > p_end)) {
                p -= n;
                goto error;
            }
        ret_len:
            assert((size_t)(int)len == len);
            return (int)len;
        }
    }
    ow_unreachable();
error:
    return -1 - (int)(p - u8_str);
}

static const ow_wchar_t charwidth_table[] = {
    0x01100, 0x01160,
    0x02329, 0x0232B,
    0x02E80, 0x0303F,
    0x03040, 0x0A4D0,
    0x0AC00, 0x0D7A4,
    0x0F900, 0x0FB00,
    0x0FE10, 0x0FE1A,
    0x0FE30, 0x0FE70,
    0x0FF00, 0x0FF61,
    0x0FFE0, 0x0FFE7,
    0x1F300, 0x1F650,
    0x1F900, 0x1FA00,
    0x20000, 0x2FFFE,
    0x30000, 0x3FFFE,
};

static size_t charwidth(ow_wchar_t code_point) {
    if (code_point < 0x80) {
        return ow_likely(isprint((int)code_point)) ?
            1 : (ow_likely(isspace((int)code_point)) ? 1 : 0);
    }

    const size_t table_size = sizeof charwidth_table / sizeof charwidth_table[0];
    for (size_t i = 0; i < table_size; i += 2) {
        if (code_point < charwidth_table[i])
            return 1;
        if (code_point < charwidth_table[i + 1])
            return 2;
    }

    return 0;
}

static size_t char_width_and_next(const ow_char8_t **str_ref) {
    ow_wchar_t code_point;
    const ow_char8_t *const str = *str_ref;
    const size_t len = ow_u8_to_unicode(&code_point, str);
    if (ow_unlikely(!len)) {
        *str_ref = str + 1;
        return 0;
    }
    *str_ref = str + len;
    return charwidth(code_point);
}

size_t ow_u8_charwidth(const ow_char8_t u8_char[]) {
    return char_width_and_next(&u8_char);
}

size_t ow_u8_strwidth(const ow_char8_t *u8_str, const ow_char8_t *end) {
    size_t width = 0;
    if (!end)
        end = (const ow_char8_t *)UINTPTR_MAX;
    while (*u8_str && u8_str < end)
        width += char_width_and_next(&u8_str);
    return width;
}

ow_char8_t *ow_u8_strrmprefix(const ow_char8_t *u8_str, size_t n_chars) {
    while (n_chars-- > 0) {
        const ow_char8_t c = *u8_str;
        if (ow_unlikely(!c))
            break;
        const size_t n = ow_u8_charlen(c);
        if (ow_unlikely(!n))
            return NULL;
        u8_str += n;
    }
    return (ow_char8_t *)u8_str;
}
