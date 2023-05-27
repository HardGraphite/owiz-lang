#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "object.h"
#include "smallint.h"

#include <utilities/attributes.h>

struct ow_machine;

/// Context of object memory management.
struct ow_objmem_context;

/// Create a memory context.
struct ow_objmem_context *ow_objmem_context_new(void);
/// Destroy a memory context.
void ow_objmem_context_del(struct ow_objmem_context *ctx);
/// Set verbose flag.
void ow_objmem_context_verbose(struct ow_objmem_context *ctx, bool status);

/// Add GC root, along with its GC marker function.
/// If the root is an object, the marker function can be NULL.
void ow_objmem_add_gc_root(
    struct ow_machine *om, void *root, void (*gc_marker)(struct ow_machine *, void *));
/// Remove a GC root added with `ow_objmem_add_gc_root()`.
bool ow_objmem_remove_gc_root(struct ow_machine *om, void *root);

/// Allocate memory for an object. Only the head of the object is initialized.
/// This function may call `ow_objmem_gc()` if necessary.
struct ow_object *ow_objmem_allocate(
    struct ow_machine *om, struct ow_class_obj *obj_class, size_t extra_field_count);

/// Run garbage collection.
int ow_objmem_gc(struct ow_machine *om, int flags);
/// Start a no-GC region.
ow_static_forceinline void ow_objmem_push_ngc(struct ow_machine *om);
/// End a no-GC region.
ow_static_forceinline void ow_objmem_pop_ngc(struct ow_machine *om);
/// Test no-GC flag.
ow_static_forceinline bool ow_objmem_test_ngc(struct ow_machine *om);

/// GC marker for any object.
/// If the object pointer is a small int in fact, it will be ignored.
/// Native fields and extra fields of an object will not be scanned by this function,
/// while others will be automatically marked.
/// A native gc_marker function will be called if exists in its class, which shall
/// mark native fields and extra fields manually if needed.
ow_static_forceinline void ow_objmem_object_gc_marker(
    struct ow_machine *om, struct ow_object *obj);

ow_static_forceinline size_t *_ow_objmem_ngc_count(struct ow_machine *om) {
    return (size_t *)*(struct ow_objmem_context **)om;
}

ow_static_forceinline void ow_objmem_push_ngc(struct ow_machine *om) {
    (*_ow_objmem_ngc_count(om))++;
    assert(*_ow_objmem_ngc_count(om) > 0);
}

ow_static_forceinline void ow_objmem_pop_ngc(struct ow_machine *om) {
    assert(*_ow_objmem_ngc_count(om) > 0);
    (*_ow_objmem_ngc_count(om))--;
}

ow_static_forceinline bool ow_objmem_test_ngc(struct ow_machine *om) {
    return *_ow_objmem_ngc_count(om) > 0;
}

void _ow_objmem_object_gc_mark_children_obj(struct ow_machine *om, struct ow_object *obj);

ow_static_forceinline void ow_objmem_object_gc_marker(
    struct ow_machine *om, struct ow_object *obj
) {
    if (ow_unlikely(ow_smallint_check(obj)))
        return;
    if (ow_object_meta_get_flag(&obj->_meta, OW_OBJMETA_FLAG_MARKED))
        return;
    ow_object_meta_set_flag(&obj->_meta, OW_OBJMETA_FLAG_MARKED);
    _ow_objmem_object_gc_mark_children_obj(om, obj);
}
