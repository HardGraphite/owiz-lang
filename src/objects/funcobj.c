#include "funcobj.h"

#include <string.h>

#include "classes.h"
#include "classes_util.h"
#include "classobj.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include <machine/machine.h>
#include <utilities/attributes.h>
#include <utilities/memalloc.h>

struct ow_func_obj_constants {
    size_t size;
    struct ow_object *data[];
};

struct ow_func_obj_symbols {
    size_t size;
    struct ow_symbol_obj *data[];
};

static const size_t _zero_size = 0;
#define empty_ow_func_obj_constants ((const struct ow_func_obj_constants *)&_zero_size)
#define empty_ow_func_obj_symbols ((const struct ow_func_obj_symbols *)&_zero_size)

static void ow_func_obj_finalizer(struct ow_object *obj) {
    struct ow_func_obj *const self = ow_object_cast(obj, struct ow_func_obj);

    if (ow_likely(self->constants != empty_ow_func_obj_constants))
        ow_free((void *)self->constants);
    if (ow_likely(self->symbols != empty_ow_func_obj_symbols))
        ow_free((void *)self->symbols);
}

static void ow_func_obj_gc_visitor(void *_obj, int op) {
    struct ow_func_obj *const self = _obj;
    ow_objmem_visit_object(self->module, op);
    for (size_t i = 0, n = self->constants->size; i < n; i++)
        ow_objmem_visit_object(self->constants->data[i], op);
    for (size_t i = 0, n = self->symbols->size; i < n; i++)
        ow_objmem_visit_object(self->symbols->data[i], op);
}

struct ow_func_obj *ow_func_obj_new(
    struct ow_machine *om, struct ow_module_obj *mod,
    struct ow_object *constants[], size_t constant_count,
    struct ow_symbol_obj *symbols[], size_t symbol_count,
    unsigned char *code, size_t code_size,
    const struct ow_func_spec *spec
) {
    struct ow_func_obj *const obj = ow_object_cast(
        ow_objmem_allocate_ex(
            om,
            OW_OBJMEM_ALLOC_SURV,
            om->builtin_classes->func,
            (ow_round_up_to(sizeof(void *), code_size) / sizeof(void *))
        ),
        struct ow_func_obj);
    obj->func_spec = *spec;
    obj->module = mod;
    if (constant_count) {
        const size_t n_bytes = constant_count * sizeof(void *);
        obj->constants = ow_malloc(sizeof(struct ow_func_obj_constants) + n_bytes);
        ((struct ow_func_obj_constants *)obj->constants)->size = constant_count;
        memcpy(((struct ow_func_obj_constants *)obj->constants)->data, constants, n_bytes);
        ow_object_write_barrier_n(ow_object_from(obj), constants, constant_count);
    } else {
        obj->constants = empty_ow_func_obj_constants;
    }
    if (symbol_count) {
        const size_t n_bytes = symbol_count * sizeof(void *);
        obj->symbols = ow_malloc(sizeof(struct ow_func_obj_symbols) + n_bytes);
        ((struct ow_func_obj_symbols *)obj->symbols)->size = symbol_count;
        memcpy(((struct ow_func_obj_symbols *)obj->symbols)->data, symbols, n_bytes);
    } else {
        obj->symbols = empty_ow_func_obj_symbols;
    }
    obj->code_size = code_size;
    memcpy(obj->code, code, code_size);
    return obj;
}

struct ow_object *ow_func_obj_get_constant(struct ow_func_obj *self, size_t index) {
    if (ow_unlikely(index >= self->constants->size))
        return NULL;
    return self->constants->data[index];
}

struct ow_symbol_obj *ow_func_obj_get_symbol(struct ow_func_obj *self, size_t index) {
    if (ow_unlikely(index >= self->symbols->size))
        return NULL;
    return self->symbols->data[index];
}

const unsigned char *ow_func_obj_code(
    const struct ow_func_obj *self, size_t *size_out
) {
    if (size_out)
        *size_out = self->code_size;
    return self->code;
}

OW_BICLS_DEF_CLASS_EX(
    func,
    "Func",
    true,
    ow_func_obj_finalizer,
    ow_func_obj_gc_visitor,
)
