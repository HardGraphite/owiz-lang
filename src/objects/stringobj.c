#include "stringobj.h"

#include <assert.h>
#include <string.h>

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/unicode.h>
#include <utilities/unreachable.h>

enum ow_string_obj_impl_subtype {
    STR_INNER = 0,
    STR_SLICE = 1,
    STR_CONS  = 2,
};

struct ow_string_obj_meta {
    size_t _subtype_and_size; // subtype | number of bytes
    size_t _length; // number of characters
};

#define STR_META_SUBTYPE_SHIFT  (sizeof(size_t) * 8 - 2)
#define STR_META_SUBTYPE_MASK   ((size_t)3 << STR_META_SUBTYPE_SHIFT)

#define ow_string_obj_meta_assign(meta, sub_type, size, length) \
    do {                                                        \
        assert(!(size & STR_META_SUBTYPE_MASK));                \
        assert(size >= length);                                 \
        (meta)._subtype_and_size =                              \
            ((size_t)sub_type << STR_META_SUBTYPE_SHIFT) | (size_t)size; \
        (meta)._length = (size_t)length;                        \
    } while (0)                                                 \
// ^^^ ow_string_obj_meta_assign() ^^^

#define ow_string_obj_meta_subtype(meta) \
    ((enum ow_string_obj_impl_subtype)((meta)._subtype_and_size >> STR_META_SUBTYPE_SHIFT))

#define ow_string_obj_meta_size(meta) \
    ((meta)._subtype_and_size & ~STR_META_SUBTYPE_MASK)

#define ow_string_obj_meta_length(meta) \
    ((meta)._length)

#define STRING_OBJ_HEAD \
    OW_EXTENDED_OBJECT_HEAD \
    struct ow_string_obj_meta str_meta; \
// ^^^ STRING_OBJ_HEAD ^^^

struct ow_string_obj {
    STRING_OBJ_HEAD
    unsigned char data[];
};

struct ow_string_obj_impl_inner {
    STRING_OBJ_HEAD
    char bytes[];
};

struct ow_string_obj_impl_slice {
    STRING_OBJ_HEAD
    struct ow_string_obj_impl_inner *str;
    size_t begin_offset; // Offset in bytes.
};

struct ow_string_obj_impl_cons {
    STRING_OBJ_HEAD
    struct ow_string_obj *str1;
    struct ow_string_obj *str2;
};

static void ow_string_obj_gc_visitor(void *_obj, int op) {
    struct ow_string_obj *const self = _obj;

    switch (ow_string_obj_meta_subtype(self->str_meta)) {
    case STR_INNER:
        break;

    case STR_SLICE:
        ow_objmem_visit_object(((struct ow_string_obj_impl_slice *)self)->str, op);
        break;

    case STR_CONS:
        ow_objmem_visit_object(((struct ow_string_obj_impl_cons *)self)->str1, op);
        ow_objmem_visit_object(((struct ow_string_obj_impl_cons *)self)->str2, op);
        break;

    default:
        ow_unreachable();
    }
}

struct ow_string_obj *ow_string_obj_new(
    struct ow_machine *om, const char *s, size_t n
) {
    if (n == (size_t)-1)
        n = strlen(s);

    int u8_len = ow_u8_strlen_s((const ow_char8_t *)s, n);
    if (ow_unlikely(u8_len < 0)) {
        n = 0;
        u8_len = 0;
    }

    struct ow_string_obj_impl_inner *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om,
            OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->string,
            (ow_round_up_to(sizeof(void *), n + 1) / OW_OBJECT_FIELD_SIZE)
        ),
        struct ow_string_obj_impl_inner
    );

    ow_string_obj_meta_assign(obj->str_meta, STR_INNER, n, (size_t)u8_len);
    memcpy(obj->bytes, s, n);
    obj->bytes[n] = '\0';
    return (struct ow_string_obj *)obj;
}

struct ow_string_obj *ow_string_obj_slice(
    struct ow_machine *om, struct ow_string_obj *str, size_t pos, size_t len
) {
    if (ow_unlikely(pos == 0 && len >= ow_string_obj_meta_length(str->str_meta)))
        return str;

    if (ow_unlikely(pos + len >= ow_string_obj_meta_length(str->str_meta))) {
        if (pos >= ow_string_obj_meta_length(str->str_meta)) {
            pos = ow_string_obj_meta_length(str->str_meta);
            len = 0;
        } else {
            len = ow_string_obj_meta_length(str->str_meta) - pos;
        }
    }

    enum ow_string_obj_impl_subtype str_type =
        ow_string_obj_meta_subtype(str->str_meta);

    if (len <= 12) {
        char buffer[12 * 4];
        const size_t size =
            ow_string_obj_copy(str, pos, len, buffer, sizeof buffer);
        return ow_string_obj_new(om, buffer, size);
    } else if (str_type == STR_INNER || str_type == STR_SLICE) {
    inner_or_slice_str:;
        const size_t efc =
            OW_OBJ_STRUCT_FIELD_COUNT(struct ow_string_obj_impl_slice) -
            OW_OBJ_STRUCT_FIELD_COUNT(struct ow_string_obj);
        struct ow_string_obj_impl_slice *const obj = ow_object_cast(
            ow_objmem_allocate_ex(
                om, OW_OBJMEM_ALLOC_AUTO,
                om->builtin_classes->string, efc
            ),
            struct ow_string_obj_impl_slice
        );
        ow_object_assert_no_write_barrier(obj);
        // Don't know size yet. Initialize `str_meta` later.

        size_t obj_str_size;
        if (str_type == STR_INNER) {
            struct ow_string_obj_impl_inner *const inner_str =
                (struct ow_string_obj_impl_inner *)str;
            const ow_char8_t *const begin_ptr =
                ow_u8_strrmprefix((const ow_char8_t *)inner_str->bytes, pos);
            assert(begin_ptr);
            const ow_char8_t *const end_ptr = ow_u8_strrmprefix(begin_ptr, len);
            assert(end_ptr);
            obj_str_size = (size_t)(end_ptr - begin_ptr);
            obj->str = inner_str;
            obj->begin_offset = (size_t)((const char *)begin_ptr - inner_str->bytes);
        } else /* (str_type == STR_SLICE) */ {
            struct ow_string_obj_impl_slice *const slice_str =
                (struct ow_string_obj_impl_slice *)str;
            const ow_char8_t *const slice_str_ptr =
                (const ow_char8_t *)(slice_str->str->bytes + slice_str->begin_offset);
            const ow_char8_t *const begin_ptr = ow_u8_strrmprefix(slice_str_ptr, pos);
            assert(begin_ptr);
            const ow_char8_t *const end_ptr = ow_u8_strrmprefix(begin_ptr, len);
            assert(end_ptr);
            obj_str_size = (size_t)(end_ptr - begin_ptr);
            obj->str = slice_str->str;
            obj->begin_offset =
                (size_t)(begin_ptr - (const ow_char8_t *)slice_str->str->bytes);
        }

        ow_string_obj_meta_assign(obj->str_meta, STR_SLICE, obj_str_size, len);

        return (struct ow_string_obj *)obj;
    } else if (str_type == STR_CONS) {
        ow_string_obj_flatten(om, str, NULL);
        str_type = ow_string_obj_meta_subtype(str->str_meta);
        assert(str_type == STR_INNER || str_type == STR_SLICE);
        goto inner_or_slice_str;
    } else {
        ow_unreachable();
    }
}

struct ow_string_obj *ow_string_obj_concat(
    struct ow_machine *om,
    struct ow_string_obj *str1, struct ow_string_obj *str2
) {
    const size_t str1_size = ow_string_obj_meta_size(str1->str_meta);
    const size_t str2_size = ow_string_obj_meta_size(str2->str_meta);
    const size_t res_size = str1_size + str2_size;

    if (res_size <= 16) {
        char buffer[16];
        const size_t size1 = ow_string_obj_copy(
            str1, 0, (size_t)-1, buffer, sizeof buffer);
        const size_t size2 = ow_string_obj_copy(
            str2, 0, (size_t)-1, buffer + size1, sizeof buffer - size1);
        ow_unused_var(size2);
        assert(size1 + size2 == res_size);
        return ow_string_obj_new(om, buffer, res_size);
    }

    if (ow_unlikely(!str1_size))
        return str2;
    if (ow_unlikely(!str2_size))
        return str1;

    const size_t efc =
        OW_OBJ_STRUCT_FIELD_COUNT(struct ow_string_obj_impl_cons) -
        OW_OBJ_STRUCT_FIELD_COUNT(struct ow_string_obj);
    struct ow_string_obj_impl_cons *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->string, efc
        ),
        struct ow_string_obj_impl_cons
    );
    ow_object_assert_no_write_barrier(obj);
    ow_string_obj_meta_assign(
        obj->str_meta, STR_CONS, res_size,
        ow_string_obj_meta_length(str1->str_meta)
            + ow_string_obj_meta_length(str2->str_meta)
    );
    obj->str1 = str1;
    obj->str2 = str2;
    return (struct ow_string_obj *)obj;
}

static size_t _ow_string_obj_copy_impl(
    const struct ow_string_obj *str,
    size_t pos, size_t len, char *buf, size_t buf_size
) {
    assert(pos + len <= ow_string_obj_meta_length(str->str_meta));

    switch (ow_string_obj_meta_subtype(str->str_meta)) {
    case STR_INNER: {
        struct ow_string_obj_impl_inner *const str_inner =
            (struct ow_string_obj_impl_inner *)str;
        const ow_char8_t *const begin_ptr =
            ow_u8_strrmprefix((const ow_char8_t *)str_inner->bytes, pos);
        const ow_char8_t *end_ptr = ow_u8_strrmprefix(begin_ptr, len);
        const ow_char8_t *const max_ptr = begin_ptr + buf_size;
        if (ow_unlikely(end_ptr > max_ptr))
            end_ptr = max_ptr;
        const size_t size = (size_t)(end_ptr - begin_ptr);
        memcpy(buf, begin_ptr, size);
        return size;
    }

    case STR_SLICE: {
        struct ow_string_obj_impl_slice *const str_slice =
            (struct ow_string_obj_impl_slice *)str;
        const ow_char8_t *const begin_ptr = ow_u8_strrmprefix(
            (const ow_char8_t *)(str_slice->str->bytes + str_slice->begin_offset), pos);
        const ow_char8_t *end_ptr = ow_u8_strrmprefix(begin_ptr, len);
        const ow_char8_t *const max_ptr = begin_ptr + buf_size;
        if (ow_unlikely(end_ptr > max_ptr))
            end_ptr = max_ptr;
        const size_t size = (size_t)(end_ptr - begin_ptr);
        memcpy(buf, begin_ptr, size);
        return size;
    }

    case STR_CONS: {
        struct ow_string_obj_impl_cons *const str_cons =
            (struct ow_string_obj_impl_cons *)str;
        const size_t str_cons_str1_length =
            ow_string_obj_meta_length(str_cons->str1->str_meta);

        if (str_cons_str1_length < pos) {
            return _ow_string_obj_copy_impl(
                str_cons->str2, pos - str_cons_str1_length, len, buf,
                buf_size);
        }
        const size_t size1 =
            _ow_string_obj_copy_impl(str_cons->str1, pos, len, buf, buf_size);
        const size_t size2 =
            _ow_string_obj_copy_impl(
                str_cons->str2,
                0, len - (str_cons_str1_length - pos),
                buf + size1, buf_size - size1);
        return size1 + size2;
    }

    default:
        ow_unreachable();
    }
}

size_t ow_string_obj_copy(
    const struct ow_string_obj *self,
    size_t pos, size_t len, char *buf, size_t buf_size
) {
    const size_t self_length = ow_string_obj_meta_length(self->str_meta);

    if (ow_unlikely(pos + len >= self_length)) {
        if (pos >= self_length) {
            pos = self_length;
            len = 0;
        } else {
            len = self_length - pos;
        }
    }

    return _ow_string_obj_copy_impl(self, pos, len, buf, buf_size);
}

const char *ow_string_obj_flatten(
    struct ow_machine *om, struct ow_string_obj *self, size_t *s_size
) {
    const enum ow_string_obj_impl_subtype str_type
        = ow_string_obj_meta_subtype(self->str_meta);

    if (str_type == STR_CONS) {
        const size_t self_size = ow_string_obj_meta_size(self->str_meta);
        const size_t self_length = ow_string_obj_meta_length(self->str_meta);

        struct ow_string_obj_impl_inner *const str_inner = ow_object_cast(
            ow_objmem_allocate_ex(
                om,
                OW_OBJMEM_ALLOC_AUTO,
                om->builtin_classes->string,
                (ow_round_up_to(sizeof(void *), self_size + 1) / sizeof(void *))
            ),
            struct ow_string_obj_impl_inner
        );
        ow_string_obj_meta_assign(str_inner->str_meta, STR_INNER, self_size, self_length);
        ow_string_obj_copy(self, 0, (size_t)-1, str_inner->bytes, self_size);
        str_inner->bytes[self_size] = '\0';

        static_assert(sizeof(struct ow_string_obj_impl_slice) ==
            sizeof(struct ow_string_obj_impl_cons), "");
        struct ow_string_obj_impl_slice *const str_slice =
            (struct ow_string_obj_impl_slice *)self;
        ow_string_obj_meta_assign(str_slice->str_meta, STR_SLICE, self_size, self_length);
        str_slice->str = str_inner;
        str_slice->begin_offset = 0;
        ow_object_write_barrier(str_slice, str_inner);

        if (s_size)
            *s_size = self_size;
        return str_inner->bytes;
    } else if (str_type == STR_SLICE) {
        struct ow_string_obj_impl_slice *const str_slice =
            (struct ow_string_obj_impl_slice *)self;
        if (s_size)
            *s_size = ow_string_obj_meta_size(str_slice->str_meta);
        return str_slice->str->bytes + str_slice->begin_offset;
    } else if (str_type == STR_INNER) {
        struct ow_string_obj_impl_inner *const str_inner =
            (struct ow_string_obj_impl_inner *)self;
        if (s_size)
            *s_size = ow_string_obj_meta_size(str_inner->str_meta);
        return str_inner->bytes;
    } else {
        ow_unreachable();
    }
}

size_t ow_string_obj_size(const struct ow_string_obj *self) {
    return ow_string_obj_meta_size(self->str_meta);
}

size_t ow_string_obj_length(const struct ow_string_obj *self) {
    return ow_string_obj_meta_length(self->str_meta);
}

OW_BICLS_DEF_CLASS_EX(
    string,
    "String",
    true,
    NULL,
    ow_string_obj_gc_visitor,
)
