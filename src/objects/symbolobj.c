#include "symbolobj.h"

#include <assert.h>
#include <string.h>

#include "classes.h"
#include "classes_util.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/hash.h>
#include <utilities/hashmap.h>
#include <utilities/memalloc.h>
#include <utilities/round.h>

struct ow_symbol_pool {
    struct ow_hashmap symbols; // { ow_symbol_obj, ow_symbol_obj }
};

struct ow_symbol_obj {
    OW_EXTENDED_OBJECT_HEAD
    ow_hash_t hash;
    size_t    size;
    char      str[];
};

static void ow_symbol_pool_weak_refs_visitor(void *_ptr, int op) {
    // TODO: Do not scan during fast GC. All symbol objects are old.

    struct ow_symbol_pool *const sp = _ptr;
    struct ow_array unreachable_symbols;
    ow_array_init(&unreachable_symbols, 0);

    ow_hashmap_foreach_1(&sp->symbols, void *, key, void *, val, {
        (ow_unused_var(key), ow_unused_var(val));
        assert(key == val);

        /*
         * FIXME: use `ow_objmem_visit_weak_ref()`.
         * The following is a modified copy of `ow_objmem_visit_weak_ref()`
         * so that the key and value can be updated both while they will not
         * be added to the `unreachable_symbols` at the same time.
         * But `ow_objmem_visit_weak_ref()` it self should be used instead.
         */

        const enum ow_objmem_weak_ref_visit_op __op = (enum ow_objmem_weak_ref_visit_op)(op);
        if (__op != OW_OBJMEM_WEAK_REF_VISIT_MOVE) {
            assert(__op == OW_OBJMEM_WEAK_REF_VISIT_FINI || __op == OW_OBJMEM_WEAK_REF_VISIT_FINI_Y);
            if (__op == OW_OBJMEM_WEAK_REF_VISIT_FINI_Y &&
                ow_object_meta_test_(OLD, ((struct ow_object *)(__node_p->key))->_meta)
            ) {
                break;
            }
            if (!ow_object_meta_test_(MRK, ((struct ow_object *)(__node_p->key))->_meta)) {
                ow_array_append(&unreachable_symbols, ( (__node_p->key) ));
            }
        } else {
            if (_ow_objmem_visit_object_do_move((struct ow_object **)&(__node_p->key)))
                __node_p->value = __node_p->key;
            else
                assert(__node_p->value == __node_p->key);
        }

        /*
         * End of the copied code.
         */

    });

    for (size_t i = 0, n = ow_array_size(&unreachable_symbols); i < n; i++) {
        void *const key = ow_array_at(&unreachable_symbols, i);
        bool ok = ow_hashmap_remove(&sp->symbols, &ow_symbol_obj_hashmap_funcs, key);
        // TODO: remove unused references when iterating.
        ow_unused_var(ok);
        assert(ok);
    }
    ow_array_fini(&unreachable_symbols);
}

struct ow_symbol_pool *ow_symbol_pool_new(struct ow_machine *om) {
    struct ow_symbol_pool *const sp = ow_malloc(sizeof(struct ow_symbol_pool));
    ow_hashmap_init(&sp->symbols, 64);
    ow_objmem_register_weak_ref(om, sp, ow_symbol_pool_weak_refs_visitor);
    return sp;
}

void ow_symbol_pool_del(struct ow_machine *om, struct ow_symbol_pool *sp) {
    ow_objmem_remove_weak_ref(om, sp);
    ow_hashmap_fini(&sp->symbols);
    ow_free(sp);
}

static bool _ow_symbol_pool_find_key_equal(
    void *ctx, const void *key_new, const void *key_stored
) {
    const size_t n = (size_t)ctx;
    const char *const s = key_new;
    const struct ow_symbol_obj *const sym_obj = key_stored;
    if (n != sym_obj->size)
        return false;
    return memcmp(s, sym_obj->str, n) == 0;
}

static ow_hash_t _ow_symbol_pool_find_key_hash(void *ctx, const void *key_new) {
    const size_t n = (size_t)ctx;
    const char *const s = key_new;
    return ow_hash_bytes(s, n);
}

static struct ow_symbol_obj *ow_symbol_pool_find(
    struct ow_symbol_pool *sp, const char *s, size_t n
) {
    const struct ow_hashmap_funcs mf = {
        _ow_symbol_pool_find_key_equal,
        _ow_symbol_pool_find_key_hash,
        (void *)n,
    };
    return ow_hashmap_get(&sp->symbols, &mf, s);
}

static bool _ow_symbol_pool_add_key_equal(
    void *ctx, const void *key_new, const void *key_stored
) {
    ow_unused_var(ctx);
    ow_unused_var(key_new);
    ow_unused_var(key_stored);
    assert(strcmp(((struct ow_symbol_obj *)key_new)->str,
        ((struct ow_symbol_obj *)key_stored)->str) != 0);
    return false;
}

static ow_hash_t _ow_symbol_pool_add_key_hash(void *ctx, const void *key_new) {
    ow_unused_var(ctx);
    const struct ow_symbol_obj *const sym_obj = key_new;
    return sym_obj->hash;
}

static const struct ow_hashmap_funcs _ow_symbol_pool_add_mf = {
    _ow_symbol_pool_add_key_equal,
    _ow_symbol_pool_add_key_hash,
    NULL,
};

static void ow_symbol_pool_add(
    struct ow_symbol_pool *sp, struct ow_symbol_obj *obj
) {
    ow_hashmap_set(&sp->symbols, &_ow_symbol_pool_add_mf, obj, obj);
}

struct ow_symbol_obj *ow_symbol_obj_new(
    struct ow_machine *om, const char *s, size_t n
) {
    struct ow_symbol_obj *obj;

    if (ow_unlikely(n == (size_t)-1))
        n = strlen(s);
    obj = ow_symbol_pool_find(om->symbol_pool, s, n);
    if (ow_likely(obj))
        return obj;

    obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om,
            OW_OBJMEM_ALLOC_SURV,
            om->builtin_classes->symbol,
            (ow_round_up_to(sizeof(void *), n + 1) / sizeof(void *))
        ),
        struct ow_symbol_obj
    );
    obj->hash = ow_hash_bytes(s, n);
    obj->size = n;
    memcpy(obj->str, s, n);
    obj->str[n] = '\0';

    ow_symbol_pool_add(om->symbol_pool, obj);

    return obj;
}

size_t ow_symbol_obj_size(const struct ow_symbol_obj *self) {
    return self->size;
}

const char *ow_symbol_obj_data(const struct ow_symbol_obj *self) {
    return self->str;
}

static bool _ow_symbol_obj_hashmap_funcs_key_equal(
    void *ctx, const void *key_new, const void *key_stored
) {
    ow_unused_var(ctx);
    const bool res = key_new == key_stored;
    assert(!strcmp(((struct ow_symbol_obj *)key_new)->str,
        ((struct ow_symbol_obj *)key_stored)->str) == res);
    return res;
}

static ow_hash_t _ow_symbol_obj_hashmap_funcs_key_hash(
    void *ctx, const void *key_new
) {
    ow_unused_var(ctx);
    const struct ow_symbol_obj *const sym_obj = key_new;
    return sym_obj->hash;
}

const struct ow_hashmap_funcs ow_symbol_obj_hashmap_funcs = {
    _ow_symbol_obj_hashmap_funcs_key_equal,
    _ow_symbol_obj_hashmap_funcs_key_hash,
    NULL,
};

OW_BICLS_DEF_CLASS_EX(
    symbol,
    "Symbol",
    true,
    NULL,
    NULL,
)
