#include "natives.h"

#include <assert.h>
#include <stddef.h>

#include <owiz.h>

#define CHECK(EXPR) static_assert((EXPR), #EXPR)

#define FIELD_TYPE_ID(STRUCT, FIELD) _Generic((((struct STRUCT *)0)->FIELD), \
    bool: 0, \
    int: 1, \
    unsigned int: 2, \
    long int: 3, \
    unsigned long int: 4, \
    long long int: 5, \
    unsigned long long int: 6, \
    owiz_native_func_t: 7, \
    void (*)(void): 7, \
    const struct ow_native_func_def *: 8, \
    const struct owiz_native_func_def *: 8, \
    const char *: 9 )

#define PTR_TYPES_ARE_SAME(TYPE1, TYPE2) \
    _Generic((TYPE1)0, TYPE2 : 1, default: 0)
#define STRUCT_SIZES_ARE_SAME(STRUCT1, STRUCT2) \
    (sizeof(struct STRUCT1) == sizeof(struct STRUCT2))
#define FIELD_OFFSETS_ARE_SAME(STRUCT1, STRUCT2, FIELD) \
    (offsetof(struct STRUCT1, FIELD) == offsetof(struct STRUCT2, FIELD))
#define FIELD_TYPES_ARE_SAME(STRUCT1, STRUCT2, FIELD) \
    (FIELD_TYPE_ID(STRUCT1, FIELD) == FIELD_TYPE_ID(STRUCT2, FIELD))

CHECK(PTR_TYPES_ARE_SAME(ow_native_func_t, owiz_native_func_t));

CHECK(OW_NATIVE_FUNC_VARIADIC_ARGC(0) == OWIZ_NATIVE_FUNC_VARIADIC_ARGC(0));
CHECK(OW_NATIVE_FUNC_VARIADIC_ARGC(1) == OWIZ_NATIVE_FUNC_VARIADIC_ARGC(1));
CHECK(OW_NATIVE_FUNC_VARIADIC_ARGC(2) == OWIZ_NATIVE_FUNC_VARIADIC_ARGC(2));

CHECK(STRUCT_SIZES_ARE_SAME(ow_native_func_def, owiz_native_func_def));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_func_def, owiz_native_func_def, name));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_func_def, owiz_native_func_def, func));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_func_def, owiz_native_func_def, argc));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_func_def, owiz_native_func_def, oarg));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_func_def, owiz_native_func_def, name));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_func_def, owiz_native_func_def, func));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_func_def, owiz_native_func_def, argc));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_func_def, owiz_native_func_def, oarg));

CHECK(STRUCT_SIZES_ARE_SAME(ow_native_class_def, owiz_native_class_def));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_class_def, owiz_native_class_def, name));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_class_def, owiz_native_class_def, data_size));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_class_def, owiz_native_class_def, methods));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_class_def, owiz_native_class_def, finalizer));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_class_def, owiz_native_class_def, name));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_class_def, owiz_native_class_def, data_size));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_class_def, owiz_native_class_def, methods));

CHECK(STRUCT_SIZES_ARE_SAME(ow_native_module_def, owiz_native_module_def));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_module_def, owiz_native_module_def, name));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_module_def, owiz_native_module_def, functions));
CHECK(FIELD_OFFSETS_ARE_SAME(ow_native_module_def, owiz_native_module_def, finalizer));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_module_def, owiz_native_module_def, name));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_module_def, owiz_native_module_def, functions));
CHECK(FIELD_TYPES_ARE_SAME(ow_native_module_def, owiz_native_module_def, finalizer));

#undef CHECK
