#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "object_util.h"
#include <utilities/attributes.h>

struct ow_exception_obj;
struct ow_machine;
struct ow_module_obj;
struct ow_native_class_def;
struct ow_native_class_def_ex;
struct ow_native_class_def_nn;
struct ow_native_class_def_nn_ex;
struct ow_object;
struct ow_symbol_obj;

typedef void (*ow_objmem_obj_fields_visitor_t)(void *, int); // Declared in "objmem.h"

/// Class object.
struct ow_class_obj;

/*
 * ## Native class
 *
 * A class whose instances have native fields is a native class. In other words,
 * the `native_field_count` of a native class shall be grater than 0.
 *
 * Data in native fields have no name associated. They can only be access by
 * offset in native code.
 *
 * A native class shall provide a `gc_visitor` function to specify how the GC
 * system should visit native fields. The `gc_visitor` shall cover both native
 * fields and extra fields (if there are) with `ow_objmem_visit_object()`.
 * By omitting the `gc_visitor` function, related fields are considered to
 * contain no reference to objects.
 *
 * A native class may provide a `finalizer`, which will be called before the
 * object is deleted. It is not recommended.
 */

/*
 * ## Non-native class
 *
 * A class whose instances have no native field is a non-native class. In other
 * words, the `native_field_count` of a native class shall be 0.
 *
 * A non-native class cannot provide `gc_visitor` and `finalizer`.
 */

/// Create an empty class.
struct ow_class_obj *ow_class_obj_new(struct ow_machine *om);
/// Initialize class object from a native definition. The class obj must be empty.
void ow_class_obj_load_native_def(
    struct ow_machine *om, struct ow_class_obj *self,
    const struct ow_native_class_def *def, struct ow_module_obj *func_mod);
/// Initialize class object from a native definition. The class obj must be empty.
void ow_class_obj_load_native_def_ex(
    struct ow_machine *om, struct ow_class_obj *self,
    const struct ow_native_class_def_ex *def, struct ow_module_obj *func_mod);
/// Initialize class object from a native definition for a non-native class.
/// The class object object must be empty. Parameter `super` is the super class.
/// `super` being NULL means inherit from the Object class. When an error occurs,
/// return an exception; otherwise return NULL.
struct ow_exception_obj *ow_class_obj_load_native_def_nn(
    struct ow_machine *om, struct ow_class_obj *self,
    struct ow_class_obj *super, const struct ow_native_class_def_nn *def,
    struct ow_module_obj *func_mod);
/// Get class name.
ow_static_forceinline struct ow_symbol_obj *ow_class_obj_name(const struct ow_class_obj *self);
/// Get super class. Return NULL if there is no super class.
ow_static_forceinline struct ow_class_obj *ow_class_obj_super(const struct ow_class_obj *self);
/// Get size (in bytes) of an object.
ow_static_forceinline size_t ow_class_obj_object_size(
    const struct ow_class_obj *self, const struct ow_object *obj);
/// Get number of object fields, including basic fields and extra fields.
ow_static_forceinline size_t ow_class_obj_object_field_count(
    const struct ow_class_obj *self, const struct ow_object *obj);
/// Get number of object fields, not including extended fields.
ow_static_forceinline size_t ow_class_obj_attribute_count(const struct ow_class_obj *self);
/// Get attribute index. If not exists, return -1.
size_t ow_class_obj_find_attribute(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name);
/// Get method index by name. If not exists, return -1.
size_t ow_class_obj_find_method(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name);
/// Get method by index. If not exists, return NULL.
struct ow_object *ow_class_obj_get_method(
    const struct ow_class_obj *self, size_t index);
/// Get static attribute by name. If not exists, return NULL.
struct ow_object *ow_class_obj_get_static(
    const struct ow_class_obj *self, const struct ow_symbol_obj *name);
/// Set or add static attribute by name.
void ow_class_obj_set_static(
    struct ow_class_obj *self, const struct ow_symbol_obj *name, struct ow_object *val);
/// Test if `derived_class` is derived from `self` or if both are the same.
ow_static_inline bool ow_class_obj_is_base(
    struct ow_class_obj *self, struct ow_class_obj *derived_class);

////////////////////////////////////////////////////////////////////////////////

struct ow_class_obj_pub_info {
    size_t super_field_count; // super class field count
    size_t basic_field_count;
    size_t native_field_count;
    bool has_extra_fields;
    struct ow_class_obj *super_class; // optional
    struct ow_symbol_obj *class_name; // optional
    void (*finalizer)(struct ow_object *); // optional
    ow_objmem_obj_fields_visitor_t gc_visitor; // optional
};

ow_static_forceinline const struct ow_class_obj_pub_info *
ow_class_obj_pub_info(const struct ow_class_obj *self) {
    return (const struct ow_class_obj_pub_info *)
        ((const unsigned char *)self + OW_OBJECT_HEAD_SIZE);
}

ow_static_forceinline struct ow_symbol_obj *
ow_class_obj_name(const struct ow_class_obj *self) {
    return ow_class_obj_pub_info(self)->class_name;
}

ow_static_forceinline struct ow_class_obj *
ow_class_obj_super(const struct ow_class_obj *self) {
    return ow_class_obj_pub_info(self)->super_class;
}

ow_static_forceinline size_t ow_class_obj_object_size(
    const struct ow_class_obj *self, const struct ow_object *obj
) {
    return OW_OBJECT_SIZE(ow_class_obj_object_field_count(self, obj));
}

ow_static_forceinline size_t ow_class_obj_object_field_count(
    const struct ow_class_obj *self, const struct ow_object *obj
) {
    const struct ow_class_obj_pub_info *const info = ow_class_obj_pub_info(self);
    return ow_likely(!info->has_extra_fields) ?
        info->basic_field_count :
        *(size_t *)((char *)obj + OW_OBJECT_HEAD_SIZE);
}

ow_static_forceinline size_t
ow_class_obj_attribute_count(const struct ow_class_obj *self) {
    return ow_class_obj_pub_info(self)->basic_field_count;
}

ow_static_inline bool ow_class_obj_is_base(
    struct ow_class_obj *self, struct ow_class_obj *derived_class
) {
    while (1) {
        if (!derived_class)
            return false;
        if (derived_class == self)
            return true;
        derived_class = ow_class_obj_super(derived_class);
    }
}
