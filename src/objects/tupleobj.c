#include "tupleobj.h"

#include <assert.h>
#include <string.h>

#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object_util.h"
#include <machine/globals.h>
#include <machine/machine.h>
#include <utilities/unreachable.h>

enum ow_tuple_obj_impl_subtype {
    TUPLE_INNER = 0,
    TUPLE_SLICE = 1,
    TUPLE_CONS  = 2,
};

struct ow_tuple_obj_meta {
    size_t _subtype_and_length;
};


#define TUPLE_META_SUBTYPE_SHIFT  (sizeof(size_t) * 8 - 2)
#define TUPLE_META_SUBTYPE_MASK   ((size_t)3 << TUPLE_META_SUBTYPE_SHIFT)

#define ow_tuple_obj_meta_assign(meta, sub_type, length) \
    do {                                                 \
        assert(!(length & TUPLE_META_SUBTYPE_MASK));     \
        (meta)._subtype_and_length =                     \
            ((size_t)sub_type << TUPLE_META_SUBTYPE_SHIFT) | (size_t)length; \
    } while (0)                                                 \
// ^^^ ow_tuple_obj_meta_assign() ^^^

#define ow_tuple_obj_meta_subtype(meta) \
    ((enum ow_tuple_obj_impl_subtype)((meta)._subtype_and_length >> TUPLE_META_SUBTYPE_SHIFT))

#define ow_tuple_obj_meta_length(meta) \
    ((meta)._subtype_and_length & ~TUPLE_META_SUBTYPE_MASK)


#define TUPLE_OBJ_HEAD \
    OW_EXTENDED_OBJECT_HEAD \
    struct ow_tuple_obj_meta tuple_meta; \
// ^^^ TUPLE_OBJ_HEAD ^^^

struct ow_tuple_obj {
    TUPLE_OBJ_HEAD
};

struct ow_tuple_obj_impl_inner {
    TUPLE_OBJ_HEAD
    struct ow_object *elems[];
};

struct ow_tuple_obj_impl_slice {
    TUPLE_OBJ_HEAD
    struct ow_tuple_obj_impl_inner *tuple;
    size_t begin_index;
};

struct ow_tuple_obj_impl_cons {
    TUPLE_OBJ_HEAD
    struct ow_tuple_obj *tuple1;
    struct ow_tuple_obj *tuple2;
};

static void ow_tuple_obj_gc_visitor(void *_obj, int op) {
    struct ow_tuple_obj *const self = _obj;

    switch (ow_tuple_obj_meta_subtype(self->tuple_meta)) {
    case TUPLE_INNER:
        for (size_t i = 0, n = ow_tuple_obj_meta_length(self->tuple_meta); i < n; i++)
            ow_objmem_visit_object(((struct ow_tuple_obj_impl_inner *)self)->elems[i], op);
        break;

    case TUPLE_SLICE:
        ow_objmem_visit_object(((struct ow_tuple_obj_impl_slice *)self)->tuple, op);
        break;

    case TUPLE_CONS:
        ow_objmem_visit_object(((struct ow_tuple_obj_impl_cons *)self)->tuple1, op);
        ow_objmem_visit_object(((struct ow_tuple_obj_impl_cons *)self)->tuple2, op);
        break;

    default:
        ow_unreachable();
    }
}

struct ow_tuple_obj *ow_tuple_obj_new(
    struct ow_machine *om, struct ow_object *elems[], size_t elem_count
) {
    struct ow_tuple_obj_impl_inner *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->tuple, elem_count
        ),
        struct ow_tuple_obj_impl_inner
    );
    ow_tuple_obj_meta_assign(obj->tuple_meta, TUPLE_INNER, elem_count);

    if (elems) {
        memcpy(obj->elems, elems, elem_count * sizeof *elems);
        ow_object_write_barrier_n(ow_object_from(obj), elems, elem_count);
    } else {
        struct ow_object *const nil = om->globals->value_nil;
        for (size_t i = 0; i < elem_count; i++)
            obj->elems[i] = nil;
    }
    return (struct ow_tuple_obj *)obj;
}

static size_t _ow_tuple_obj_copy_impl(
    const struct ow_tuple_obj *tuple,
    size_t pos, size_t len, struct ow_object *buf[], size_t buf_size
) {
    assert(pos + len <= ow_tuple_obj_meta_length(tuple->tuple_meta));

    switch (ow_tuple_obj_meta_subtype(tuple->tuple_meta)) {
    case TUPLE_INNER: {
        struct ow_tuple_obj_impl_inner *const tuple_inner =
            (struct ow_tuple_obj_impl_inner *)tuple;
        struct ow_object **const begin_ptr = tuple_inner->elems + pos;
        struct ow_object **end_ptr = begin_ptr + len;
        struct ow_object **const max_ptr = begin_ptr + buf_size;
        if (ow_unlikely(end_ptr > max_ptr))
            end_ptr = max_ptr;
        const size_t cp_n = (size_t)(end_ptr - begin_ptr);
        memcpy(buf, begin_ptr, cp_n * sizeof(void *));
        return cp_n;
    }

    case TUPLE_SLICE: {
        struct ow_tuple_obj_impl_slice *const tuple_slice =
            (struct ow_tuple_obj_impl_slice *)tuple;
        struct ow_object **const begin_ptr =
            tuple_slice->tuple->elems + tuple_slice->begin_index + pos;
        struct ow_object **end_ptr = begin_ptr + len;
        struct ow_object **const max_ptr = begin_ptr + buf_size;
        if (ow_unlikely(end_ptr > max_ptr))
            end_ptr = max_ptr;
        const size_t cp_n = (size_t)(end_ptr - begin_ptr);
        memcpy(buf, begin_ptr, cp_n * sizeof(void *));
        return cp_n;
    }

    case TUPLE_CONS: {
        struct ow_tuple_obj_impl_cons *const tuple_cons =
            (struct ow_tuple_obj_impl_cons *)tuple;
        const size_t tuple1_size =
            ow_tuple_obj_meta_length(tuple_cons->tuple1->tuple_meta);
        if (tuple1_size < pos) {
            return _ow_tuple_obj_copy_impl(
                tuple_cons->tuple2, pos - tuple1_size, len, buf, buf_size);
        } else {
            size_t cp_n = 0;
            cp_n += _ow_tuple_obj_copy_impl(
                tuple_cons->tuple1, pos, len, buf, buf_size);
            cp_n += _ow_tuple_obj_copy_impl(
                tuple_cons->tuple2,
                0, len - (tuple1_size - pos),
                buf + tuple1_size, buf_size - tuple1_size);
            return cp_n;
        }
    }

    default:
        ow_unreachable();
    }
}

size_t ow_tuple_obj_copy(
    struct ow_tuple_obj *self, size_t pos, size_t len,
    struct ow_object *buf_obj, struct ow_object *buf[], size_t buf_sz
) {
    const size_t self_length = ow_tuple_obj_meta_length(self->tuple_meta);

    if (ow_unlikely(pos + len >= self_length)) {
        if (pos >= self_length) {
            pos = self_length;
            len = 0;
        } else {
            len = self_length - pos;
        }
    }

    const size_t cp_n = _ow_tuple_obj_copy_impl(self, pos, len, buf, buf_sz);
    if (buf_obj)
        ow_object_write_barrier_n(buf_obj, buf, cp_n);
    return cp_n;
}

struct ow_tuple_obj *ow_tuple_obj_slice(
    struct ow_machine *om, struct ow_tuple_obj *tuple, size_t pos, size_t len
) {
    if (ow_unlikely(pos == 0 && len >= tuple->field_count))
        return tuple;

    if (ow_unlikely(pos + len >= tuple->field_count)) {
        if (pos >= tuple->field_count) {
            pos = tuple->field_count;
            len = 0;
        } else {
            len = tuple->field_count - pos;
        }
    }

    enum ow_tuple_obj_impl_subtype tuple_type =
        ow_tuple_obj_meta_subtype(tuple->tuple_meta);

    if (len <= 2) {
        struct ow_object *buffer[2];
        ow_tuple_obj_copy(tuple, pos, len, NULL, buffer, 2);
        return ow_tuple_obj_new(om, buffer, len);
    } else if (tuple_type == TUPLE_INNER || tuple_type == TUPLE_SLICE) {
    inner_or_slice_tuple:;
        const size_t efc =
            OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj_impl_slice) -
            OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj);
        struct ow_tuple_obj_impl_slice *const obj = ow_object_cast(
            ow_objmem_allocate_ex(
                om, OW_OBJMEM_ALLOC_AUTO,
                om->builtin_classes->tuple, efc
            ),
            struct ow_tuple_obj_impl_slice
        );
        ow_tuple_obj_meta_assign(obj->tuple_meta, TUPLE_SLICE, len);

        if (tuple_type == TUPLE_INNER) {
            obj->tuple = (struct ow_tuple_obj_impl_inner *)tuple;
            obj->begin_index = pos;
        } else /* (tuple_type == TUPLE_SLICE) */ {
            struct ow_tuple_obj_impl_slice *const tuple_slice =
                (struct ow_tuple_obj_impl_slice *)tuple;
            obj->tuple = tuple_slice->tuple;
            obj->begin_index = tuple_slice->begin_index + pos;
        }
        ow_object_write_barrier(obj, obj->tuple);
        return (struct ow_tuple_obj *)obj;
    } else if (tuple_type == TUPLE_CONS) {
        ow_tuple_obj_flatten(om, tuple, NULL);
        tuple_type = ow_tuple_obj_meta_subtype(tuple->tuple_meta);
        assert(tuple_type == TUPLE_INNER || tuple_type == TUPLE_SLICE);
        goto inner_or_slice_tuple;
    } else {
        ow_unreachable();
    }
}

struct ow_tuple_obj *ow_tuple_obj_concat(
    struct ow_machine *om, struct ow_tuple_obj *tuple1, struct ow_tuple_obj *tuple2
) {
    const size_t tuple1_size = ow_tuple_obj_meta_length(tuple1->tuple_meta);
    const size_t tuple2_size = ow_tuple_obj_meta_length(tuple2->tuple_meta);
    const size_t res_size = tuple1_size + tuple2_size;

    if (res_size <= 4) {
        struct ow_object *buffer[4];
        ow_tuple_obj_copy(
            tuple1, 0, (size_t)-1,
            NULL, buffer, sizeof buffer
        );
        ow_tuple_obj_copy(
            tuple2, 0, (size_t)-1,
            NULL, buffer + tuple1_size, sizeof buffer - tuple1_size
        );
        return ow_tuple_obj_new(om, buffer, res_size);
    }

    if (ow_unlikely(!tuple1_size))
        return tuple2;
    if (ow_unlikely(!tuple2_size))
        return tuple1;

    const size_t efc =
        OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj_impl_cons) -
        OW_OBJ_STRUCT_FIELD_COUNT(struct ow_tuple_obj);
    struct ow_tuple_obj_impl_cons *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om, OW_OBJMEM_ALLOC_AUTO,
            om->builtin_classes->tuple, efc
        ),
        struct ow_tuple_obj_impl_cons
    );
    ow_tuple_obj_meta_assign(obj->tuple_meta, TUPLE_CONS, res_size);
    obj->tuple1 = tuple1;
    obj->tuple2 = tuple2;
    ow_object_write_barrier(obj, tuple1);
    ow_object_write_barrier(obj, tuple2);
    return (struct ow_tuple_obj *)obj;
}

void ow_tuple_obj_flatten(
    struct ow_machine *om, struct ow_tuple_obj *self, size_t *size
) {
    const enum ow_tuple_obj_impl_subtype tuple_type
        = ow_tuple_obj_meta_subtype(self->tuple_meta);

    if (tuple_type == TUPLE_CONS) {
        const size_t self_len = ow_tuple_obj_meta_length(self->tuple_meta);

        struct ow_tuple_obj *const _tuple_inner = ow_tuple_obj_new(om, NULL, self_len);
        assert(ow_tuple_obj_meta_subtype(_tuple_inner->tuple_meta) == TUPLE_INNER);
        struct ow_tuple_obj_impl_inner *const tuple_inner =
            (struct ow_tuple_obj_impl_inner *)_tuple_inner;
        ow_tuple_obj_copy(
            self, 0, (size_t)-1,
            ow_object_from(tuple_inner), tuple_inner->elems, self_len
        );

        static_assert(sizeof(struct ow_tuple_obj_impl_slice) ==
            sizeof(struct ow_tuple_obj_impl_cons), "");
        struct ow_tuple_obj_impl_slice *const tuple_slice =
            (struct ow_tuple_obj_impl_slice *)self;
        ow_tuple_obj_meta_assign(tuple_slice->tuple_meta, TUPLE_SLICE, self_len);
        tuple_slice->tuple = tuple_inner;
        tuple_slice->begin_index = 0;
        ow_object_write_barrier(tuple_slice, tuple_inner);

        if (size)
            *size = self_len;
        // return tuple_inner->elems;
    } else if (tuple_type == TUPLE_SLICE) {
        struct ow_tuple_obj_impl_slice *const tuple_slice =
            (struct ow_tuple_obj_impl_slice *)self;
        if (size)
            *size = ow_tuple_obj_meta_length(tuple_slice->tuple_meta);
        // return tuple_slice->tuple->elems + tuple_slice->begin_index;
    } else if (tuple_type == TUPLE_INNER) {
        struct ow_tuple_obj_impl_inner *const tuple_inner =
            (struct ow_tuple_obj_impl_inner *)self;
        if (size)
            *size = ow_tuple_obj_meta_length(tuple_inner->tuple_meta);
        // return tuple_inner->elems;
    } else {
        ow_unreachable();
    }

    /*
     * Due to safety concerns, pointer to elements is not returned.
     */
}

size_t ow_tuple_obj_length(const struct ow_tuple_obj *self) {
    return ow_tuple_obj_meta_length(self->tuple_meta);
}

struct ow_object *ow_tuple_obj_get(const struct ow_tuple_obj *self, size_t index) {
    if (ow_unlikely(index >= ow_tuple_obj_meta_length(self->tuple_meta)))
        return NULL;

    switch (ow_tuple_obj_meta_subtype(self->tuple_meta)) {
    case TUPLE_INNER:
        return ((struct ow_tuple_obj_impl_inner *)self)->elems[index];

    case TUPLE_SLICE: {
        struct ow_tuple_obj_impl_slice *const tuple_slice =
            (struct ow_tuple_obj_impl_slice *)self;
        return tuple_slice->tuple->elems[tuple_slice->begin_index + index];
    }

    case TUPLE_CONS: {
        struct ow_tuple_obj_impl_cons *const tuple_cons =
            (struct ow_tuple_obj_impl_cons *)self;
        const size_t tuple1_elem_count =
            ow_tuple_obj_meta_length(tuple_cons->tuple1->tuple_meta);
        if (index < tuple1_elem_count)
            self = tuple_cons->tuple1;
        else
            self = tuple_cons->tuple2, index -= tuple1_elem_count;
        return ow_tuple_obj_get(self, index);
    }

    default:
        ow_unreachable();
    }
}

OW_BICLS_DEF_CLASS_EX(
    tuple,
    "Tuple",
    true,
    NULL,
    ow_tuple_obj_gc_visitor,
)
