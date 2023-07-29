#include "classobj.h"

#include <assert.h>
#include <string.h>

#include "cfuncobj.h"
#include "classes.h"
#include "classes_util.h"
#include "exceptionobj.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include "symbolobj.h"
#include <machine/machine.h>
#include <utilities/array.h>
#include <utilities/attributes.h>
#include <utilities/hashmap.h>
#include <utilities/round.h>

/// Add attribute. Return false if the attribute exists.
static bool ow_class_obj_add_attribute_y(
    struct ow_class_obj *self, const struct ow_symbol_obj *name, size_t field_index);

/// Set or add method by name. Return its index.
static size_t ow_class_obj_add_or_set_method_y(
    struct ow_class_obj *self,
    const struct ow_symbol_obj *name, struct ow_object *method);

struct ow_class_obj {
    OW_OBJECT_HEAD
    struct ow_class_obj_pub_info pub_info;
    struct ow_hashmap attrs_and_methods_map; // { name, (field_index + 1) or (-1 - method_index) }
    struct ow_hashmap statics_map; // { name, static_member_object }
    struct ow_array methods;
    void (*finalizer2)(void *);
};

static void ow_class_obj_init(struct ow_class_obj *self) {
    memset(&self->pub_info, 0, sizeof self->pub_info);
    ow_hashmap_init(&self->attrs_and_methods_map, 0);
    ow_hashmap_init(&self->statics_map, 0);
    ow_array_init(&self->methods, 0);
    self->finalizer2 = NULL;
    assert(ow_class_obj_pub_info(self) == &self->pub_info);
}

static void ow_class_obj_fini(struct ow_class_obj *self) {
    ow_array_fini(&self->methods);
    ow_hashmap_fini(&self->statics_map);
    ow_hashmap_fini(&self->attrs_and_methods_map);
}

static void ow_class_obj_finalizer(struct ow_object *obj) {
    struct ow_class_obj *const self = ow_object_cast(obj, struct ow_class_obj);
    ow_class_obj_fini(self);
}

static void ow_class_obj_gc_visitor(void *_obj, int op) {
    struct ow_object *const obj = _obj;
    struct ow_class_obj *const self = ow_object_cast(obj, struct ow_class_obj);

    if (ow_likely(self->pub_info.class_name))
        ow_objmem_visit_object(self->pub_info.class_name, op);
    if (ow_likely(self->pub_info.super_class))
        ow_objmem_visit_object(self->pub_info.super_class, op);
    ow_hashmap_foreach_1(&self->attrs_and_methods_map, void *, name, size_t, index, {
        (ow_unused_var(name), ow_unused_var(index));
        ow_objmem_visit_object(__node_p->key, op);
    });
    ow_hashmap_foreach_1(&self->statics_map, void *, name, void *, attr, {
        (ow_unused_var(name), ow_unused_var(attr));
        ow_objmem_visit_object(__node_p->key, op);
        ow_objmem_visit_object(__node_p->value, op);
    });
    for (size_t i = 0, n = ow_array_size(&self->methods); i < n; i++)
        ow_objmem_visit_object(ow_array_at(&self->methods, i), op);
}

struct ow_class_obj *ow_class_obj_new(struct ow_machine *om) {
    struct ow_class_obj *const obj = ow_object_cast(
        ow_objmem_allocate_ex(om, OW_OBJMEM_ALLOC_SURV, om->builtin_classes->class_, 0),
        struct ow_class_obj
    );
    ow_class_obj_init(obj);
    return obj;
}

static void _finalizer_wrapper(struct ow_object *obj) {
    struct ow_class_obj *const obj_class = ow_object_class(obj);
    assert(obj_class->finalizer2);
    obj_class->finalizer2((char *)obj + OW_OBJECT_HEAD_SIZE);
}

void ow_class_obj_load_native_def(
    struct ow_machine *om, struct ow_class_obj *self,
    const struct ow_native_class_def *def, struct ow_module_obj *func_mod
) {
    assert(!ow_hashmap_size(&self->attrs_and_methods_map) && !ow_array_size(&self->methods));

    struct ow_class_obj *const super = om->builtin_classes->object;

    self->pub_info.super_class = super;
    if (ow_likely(def->name))
        self->pub_info.class_name = ow_symbol_obj_new(om, def->name, (size_t)-1);
    if (def->finalizer) {
        self->pub_info.finalizer = _finalizer_wrapper;
        self->finalizer2 = def->finalizer;
    } else {
        self->pub_info.finalizer = NULL;
        self->finalizer2 = NULL;
    }
    self->pub_info.gc_visitor = NULL;

    const size_t def_field_count =
        ow_round_up_to(OW_OBJECT_FIELD_SIZE, def->data_size) / OW_OBJECT_FIELD_SIZE;
    size_t def_method_count = 0;
    for (const struct ow_native_func_def *p = def->methods; p->func; p++)
        def_method_count++;

    const size_t total_field_count = def_field_count;
    assert(!super->pub_info.basic_field_count);
    const size_t total_method_count_approx =
        def_method_count + ow_array_size(&super->methods);

    ow_array_reserve(&self->methods, total_method_count_approx);
    ow_hashmap_reserve(
        &self->attrs_and_methods_map,
        total_field_count + total_method_count_approx
    );

    ow_objmem_push_ngc(om);

    for (size_t i = 0; i < def_method_count; i++) {
        const struct ow_native_func_def method_def = def->methods[i];
        struct ow_symbol_obj *const name_obj = ow_symbol_obj_new(
            om, method_def.name, (size_t)-1
        );
        struct ow_cfunc_obj *const func_obj = ow_cfunc_obj_new(
            om, func_mod, method_def.name, method_def.func,
            &(struct ow_func_spec){method_def.argc, method_def.oarg, 0}
        );
        ow_class_obj_add_or_set_method_y(self, name_obj, ow_object_from(func_obj));
    }

    ow_objmem_pop_ngc(om);

    ow_array_shrink(&self->methods);
    ow_hashmap_shrink(&self->attrs_and_methods_map);

    self->pub_info.native_field_count = total_field_count; // All fields are native.
    self->pub_info.basic_field_count = total_field_count;
    self->pub_info.has_extra_fields = false;
}

void ow_class_obj_load_native_def_ex(
    struct ow_machine *om, struct ow_class_obj *self,
    const struct ow_native_class_def_ex *def, struct ow_module_obj *func_mod
) {
    ow_class_obj_load_native_def(
        om, self, (const struct ow_native_class_def *)def, func_mod
    );
    self->finalizer2 = NULL;
    self->pub_info.has_extra_fields = def->extended;
    self->pub_info.finalizer = def->finalizer;
    self->pub_info.gc_visitor = def->gc_visitor;
}

struct ow_exception_obj *ow_class_obj_load_native_def_nn(
    struct ow_machine *om, struct ow_class_obj *self,
    struct ow_class_obj *super, const struct ow_native_class_def_nn *def,
    struct ow_module_obj *func_mod
) {
    assert(!ow_hashmap_size(&self->attrs_and_methods_map) && !ow_array_size(&self->methods));

    if (!super)
        super = om->builtin_classes->object;

    self->pub_info.super_class = super;
    if (ow_likely(def->name))
        self->pub_info.class_name = ow_symbol_obj_new(om, def->name, (size_t)-1);
    self->pub_info.finalizer = NULL;
    self->finalizer2 = NULL;
    self->pub_info.gc_visitor = NULL;

    size_t def_field_count = 0;
    size_t def_method_count = 0;
    for (const char *const *p = def->attributes; p; p++)
        def_field_count++;
    for (const struct ow_native_func_def *p = def->methods; p->func; p++)
        def_method_count++;

    if (ow_unlikely(super->pub_info.has_extra_fields)) {
        // Cannot inherit from a class that allows extra fields.
        return ow_exception_format(
            om, NULL, "class `%s' is not inheritable",
            ow_symbol_obj_data(super->pub_info.class_name)
        );
    }
    const size_t super_field_count  = super->pub_info.basic_field_count;
    const size_t super_method_count = ow_array_size(&super->methods);

    const size_t total_field_count         = super_field_count + def_field_count;
    const size_t total_method_count_approx = super_method_count + def_method_count;

    ow_array_reserve(&self->methods, total_method_count_approx);
    ow_hashmap_reserve(
        &self->attrs_and_methods_map,
        total_field_count + total_method_count_approx
    );

    if (ow_likely(super)) {
        ow_array_extend(&self->methods, &super->methods);
        ow_hashmap_extend(
            &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs,
            &super->attrs_and_methods_map
        );
    }

    ow_objmem_push_ngc(om);

    for (size_t i = 0; i < def_field_count; i++) {
        const char *const attr_name = def->attributes[i];
        struct ow_symbol_obj *const name_obj = ow_symbol_obj_new(
            om, attr_name, (size_t)-1
        );
        const size_t attr_field_index = super_field_count + i;
        if (!ow_class_obj_add_attribute_y(self, name_obj, attr_field_index)) {
            ow_objmem_pop_ngc(om); // !!
            return ow_exception_format(
                om, NULL, "duplicate attribute name `%s'", attr_name
            );
        }
    }

    for (size_t i = 0; i < def_method_count; i++) {
        const struct ow_native_func_def method_def = def->methods[i];
        struct ow_symbol_obj *const name_obj = ow_symbol_obj_new(
            om, method_def.name, (size_t)-1
        );
        struct ow_cfunc_obj *const func_obj = ow_cfunc_obj_new(
            om, func_mod, method_def.name, method_def.func,
            &(struct ow_func_spec){method_def.argc, method_def.oarg, 0}
        );
        ow_class_obj_add_or_set_method_y(self, name_obj, ow_object_from(func_obj));
    }

    ow_objmem_pop_ngc(om);

    ow_array_shrink(&self->methods);
    ow_hashmap_shrink(&self->attrs_and_methods_map);

    self->pub_info.native_field_count = super->pub_info.native_field_count;
    self->pub_info.basic_field_count = total_field_count;
    self->pub_info.has_extra_fields = false;

    return NULL;
}

size_t ow_class_obj_find_attribute(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name
) {
    const intptr_t index = (intptr_t)ow_hashmap_get(
        &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs, name);
    if (ow_unlikely(index <= 0))
        return (size_t)-1;
    return (size_t)(index - 1);
}

static bool ow_class_obj_add_attribute_y(
    struct ow_class_obj *self,
    const struct ow_symbol_obj *name, size_t field_index
) {
    if (ow_hashmap_get(
        &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs, name
    )) {
        return false; // Exists.
    }
    // TODO: Combine check add insert operations.

    ow_hashmap_set(
        &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs,
        name, (void *)(-1 - (intptr_t)field_index)
    );
    ow_object_assert_no_write_barrier_2(self, ow_object_from(name));
    return true;
}

size_t ow_class_obj_find_method(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name
) {
    const intptr_t index = (intptr_t)ow_hashmap_get(
        &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs, name);
    if (ow_unlikely(index >= 0))
        return (size_t)-1;
    return (size_t)(-1 - index);
}

struct ow_object *ow_class_obj_get_method(
    const struct ow_class_obj *self, size_t index
) {
    if (ow_unlikely(index >= ow_array_size(&self->methods)))
        return NULL;
    return ow_array_at(&self->methods, index);
}

static size_t ow_class_obj_add_or_set_method_y(
    struct ow_class_obj *self,
    const struct ow_symbol_obj *name, struct ow_object *method
) {
    size_t index = ow_class_obj_find_method(self, name);
    if (index == (size_t)-1) {
        index = ow_array_size(&self->methods);
        ow_array_append(&self->methods, method);
        ow_hashmap_set(
            &self->attrs_and_methods_map, &ow_symbol_obj_hashmap_funcs,
            name, (void *)(-1 - (intptr_t)index)
        );
        ow_object_assert_no_write_barrier_2(self, ow_object_from(name));
    } else {
        ow_array_at(&self->methods, index) = method;
    }
    ow_object_write_barrier(self, method);
    return index;
}

struct ow_object *ow_class_obj_get_static(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name
) {
    return ow_hashmap_get(&self->statics_map, &ow_symbol_obj_hashmap_funcs, name);
}

void ow_class_obj_set_static(
    struct ow_class_obj *self,
    const struct ow_symbol_obj *name, struct ow_object *val
) {
    ow_hashmap_set(&self->statics_map, &ow_symbol_obj_hashmap_funcs, name, val);
    ow_object_write_barrier(self, ow_object_from(name));
    ow_object_write_barrier(self, val);
}

OW_BICLS_DEF_CLASS_EX_1(
    class_,
    class,
    "Class",
    false,
    ow_class_obj_finalizer,
    ow_class_obj_gc_visitor,
)
