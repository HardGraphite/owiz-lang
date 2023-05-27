#include "moduleobj.h"

#include "cfuncobj.h"
#include "classes.h"
#include "classobj.h"
#include "classes_util.h"
#include "memory.h"
#include "natives.h"
#include "object_util.h"
#include "symbolobj.h"
#include <machine/invoke.h>
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/dynlib.h>
#include <utilities/hashmap.h>
#include <utilities/memalloc.h>

struct module_dynlib_list_node {
    ow_dynlib_t *lib_handle;
    struct module_dynlib_list_node *next;
};

struct module_dynlib_list {
    struct module_dynlib_list_node *first_node;
};

static void module_dynlib_list_init(struct module_dynlib_list *list) {
    list->first_node = NULL;
}

static void module_dynlib_list_fini(struct module_dynlib_list *list) {
    for (struct module_dynlib_list_node *node = list->first_node; node; ) {
        struct module_dynlib_list_node *const next_node = node->next;
        ow_dynlib_close(node->lib_handle);
        ow_free(node);
        node = next_node;
    }
    list->first_node = NULL;
}

static void module_dynlib_list_add(
    struct module_dynlib_list *list, ow_dynlib_t *lib_handle
) {
    struct module_dynlib_list_node *const node =
        ow_malloc(sizeof(struct module_dynlib_list_node));
    node->lib_handle = lib_handle;
    node->next = list->first_node;
    list->first_node = node;
}

struct ow_module_obj {
    OW_OBJECT_HEAD
    struct ow_hashmap globals_map; // { name, index + 1 }
    struct ow_array globals;
    struct ow_symbol_obj *name; // Optional.
    int (*finalizer)(struct ow_machine *);
    struct module_dynlib_list dynlib_list;
};

static void ow_module_obj_init(struct ow_module_obj *self) {
    ow_hashmap_init(&self->globals_map, 0);
    ow_array_init(&self->globals, 0);
    self->name = NULL;
    self->finalizer = NULL;
    module_dynlib_list_init(&self->dynlib_list);
}

static void ow_module_obj_fini(struct ow_module_obj *self) {
    ow_array_fini(&self->globals);
    ow_hashmap_fini(&self->globals_map);
    module_dynlib_list_fini(&self->dynlib_list);
}

static void ow_module_obj_finalizer(struct ow_machine *om, struct ow_object *obj) {
    ow_unused_var(om);
    assert(ow_class_obj_is_base(om->builtin_classes->module, ow_object_class(obj)));
    struct ow_module_obj *const self = ow_object_cast(obj, struct ow_module_obj);
    if (self->finalizer) {
        ow_machine_call_native(
            om, self, self->finalizer, 0, NULL, &(struct ow_object *){NULL});
    }
    ow_module_obj_fini(self);
}

static int _globals_map_gc_walker(void *ctx, const void *key, void *val) {
    struct ow_machine *const om = ctx;
    struct ow_object *const key_obj = (struct ow_object *)key;
    ow_unused_var(val);
    ow_objmem_object_gc_marker(om, key_obj);
    return 0;
}

static void ow_module_obj_gc_marker(struct ow_machine *om, struct ow_object *obj) {
    ow_unused_var(om);
    assert(ow_class_obj_is_base(om->builtin_classes->module, ow_object_class(obj)));
    struct ow_module_obj *const self = ow_object_cast(obj, struct ow_module_obj);

    ow_hashmap_foreach(&self->globals_map, _globals_map_gc_walker, om);
    for (size_t i = 0, n = ow_array_size(&self->globals); i < n; i++)
        ow_objmem_object_gc_marker(om, ow_array_at(&self->globals, i));
    if (ow_likely(self->name))
        ow_objmem_object_gc_marker(om, ow_object_from(self->name));
}

struct ow_module_obj *ow_module_obj_new(struct ow_machine *om) {
    struct ow_module_obj *const obj = ow_object_cast(
        ow_objmem_allocate(om, om->builtin_classes->module, 0),
        struct ow_module_obj);
    ow_module_obj_init(obj);
    return obj;
}

void ow_module_obj_load_native_def(
    struct ow_machine *om, struct ow_module_obj *self,
    const struct ow_native_module_def *def
) {
    assert(!ow_hashmap_size(&self->globals_map) && !ow_array_size(&self->globals));

    size_t func_count = 0;
    for (const struct ow_native_func_def *p = def->functions; p->func; p++)
        func_count++;
    ow_hashmap_reserve(&self->globals_map, func_count);
    ow_array_reserve(&self->globals, func_count);

    ow_objmem_push_ngc(om);

    for (size_t i = 0; i < func_count; i++) {
        const struct ow_native_func_def func_def = def->functions[i];
        struct ow_cfunc_obj *const func_obj = ow_cfunc_obj_new(
            om, self, func_def.name, func_def.func,
            &(struct ow_func_spec){func_def.argc, func_def.oarg, 0});
        struct ow_symbol_obj *const name_obj =
            ow_symbol_obj_new(om, func_def.name, (size_t)-1);
        ow_module_obj_set_global_y(self, name_obj, ow_object_from(func_obj));
    }

    ow_objmem_pop_ngc(om);

    self->name = def->name ? ow_symbol_obj_new(om, def->name, (size_t)-1) : NULL;
    self->finalizer = def->finalizer;
}

size_t ow_module_obj_find_global(
    const struct ow_module_obj *self, const struct ow_symbol_obj *name
) {
    const uintptr_t index = (uintptr_t)ow_hashmap_get(
        &self->globals_map, &ow_symbol_obj_hashmap_funcs, name);
    return (size_t)index - 1;
}

struct ow_object *ow_module_obj_get_global(
    const struct ow_module_obj *self, size_t index
) {
    if (ow_unlikely(index >= ow_array_size(&self->globals)))
        return NULL;
    return ow_array_at(&self->globals, index);
}

struct ow_object *ow_module_obj_get_global_y(
    const struct ow_module_obj *self, const struct ow_symbol_obj *name
) {
    const uintptr_t index_raw = (uintptr_t)ow_hashmap_get(
        &self->globals_map, &ow_symbol_obj_hashmap_funcs, name);
    if (ow_unlikely(!index_raw))
        return NULL;
    const size_t index = (size_t)index_raw - 1;
    assert(index < ow_array_size(&self->globals));
    return ow_array_at(&self->globals, index);
}

bool ow_module_obj_set_global(
    struct ow_module_obj *self, size_t index, struct ow_object *value
) {
    if (ow_unlikely(index >= ow_array_size(&self->globals)))
        return false;
    ow_array_at(&self->globals, index) = value;
    return true;
}

size_t ow_module_obj_set_global_y(
    struct ow_module_obj *self,
    const struct ow_symbol_obj *name, struct ow_object *value
) {
    size_t index = ow_module_obj_find_global(self, name);
    if (index == (size_t)-1) {
        index = ow_array_size(&self->globals);
        ow_array_append(&self->globals, value);
        ow_hashmap_set(
            &self->globals_map, &ow_symbol_obj_hashmap_funcs,
            name, (void *)(index + 1));
    } else {
        ow_array_at(&self->globals, index) = value;
    }
    return index;
}

size_t ow_module_obj_global_count(const struct ow_module_obj *self) {
    const size_t n = ow_array_size(&self->globals);
    assert(n == ow_hashmap_size(&self->globals_map));
    return n;
}

struct _ow_module_obj_foreach_global_context {
    const struct ow_module_obj *module;
    int(*walker)(void *, struct ow_symbol_obj *, size_t, struct ow_object *);
    void *arg;
};

static int _ow_module_obj_foreach_global_walker(
    void *_ctx, const void *key, void *val
) {
    struct _ow_module_obj_foreach_global_context *const ctx = _ctx;
    struct ow_symbol_obj *const name = (void *)key;
    const size_t index = (uintptr_t)val - 1;
    struct ow_object *const value = ow_array_at(&ctx->module->globals, index);
    return ctx->walker(ctx->arg, name, index, value);
}

int ow_module_obj_foreach_global(
    const struct ow_module_obj *self,
    int(*walker)(void *, struct ow_symbol_obj *, size_t, struct ow_object *), void *arg
) {
    return ow_hashmap_foreach(
        &self->globals_map,
        _ow_module_obj_foreach_global_walker,
        &(struct _ow_module_obj_foreach_global_context) {self, walker, arg}
    );
}

void ow_module_obj_keep_dynlib(struct ow_module_obj *self, void *lib_handle) {
    module_dynlib_list_add(&self->dynlib_list, lib_handle);
}

static const struct ow_native_func_def module_methods[] = {
    {NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(module) = {
    .name      = "Module",
    .data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_module_obj),
    .methods   = module_methods,
    .finalizer = ow_module_obj_finalizer,
    .gc_marker = ow_module_obj_gc_marker,
    .extended  = false,
};
