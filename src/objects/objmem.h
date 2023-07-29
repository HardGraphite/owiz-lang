#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "object.h"
#include "smallint.h"

#include <utilities/attributes.h>
#include <utilities/unreachable.h>

struct ow_machine;

/// Context of object memory management.
struct ow_objmem_context;

/// GC: object scanning functions. A Class whose instances have native fields
/// can provide such function to override the default scanning behaviour.
/// In this function, visit native fields with `ow_objmem_visit_object()`.
/// If the object has extra fields, they shall also be visited.
typedef void (*ow_objmem_obj_fields_visitor_t)(void *, int);

/// GC: weak reference container scanning functions. A registered weak reference container
/// shall provide such function to update moved references delete unused weak references.
/// In this function, visit each reference with `ow_objmem_object_alive()`.
typedef void (*ow_objmem_weak_refs_visitor_t)(void *, int);

/// Create a memory context.
struct ow_objmem_context *ow_objmem_context_new(void);
/// Destroy a memory context. (All objects will be finalized.)
void ow_objmem_context_del(struct ow_objmem_context *ctx);

/// Memory allocation options.
enum ow_objmem_alloc_type {
    OW_OBJMEM_ALLOC_AUTO, ///< Decide automatically.
    OW_OBJMEM_ALLOC_SURV, ///< Assume the object has survived from a few GCs.
    OW_OBJMEM_ALLOC_HUGE, ///< Treat object as a large object.
};

/// Allocate memory for an object. Only the head of the object is initialized.
/// This function may call `ow_objmem_gc()` if necessary.
struct ow_object *ow_objmem_allocate(
    struct ow_machine *om, struct ow_class_obj *obj_class);
/// Allocate object memory like `ow_objmem_allocate()`, but provides more options.
/// Class objects must be allocated with type `OW_OBJMEM_ALLOC_SURV`.
struct ow_object *ow_objmem_allocate_ex(
    struct ow_machine *om, enum ow_objmem_alloc_type alloc_type,
    struct ow_class_obj *obj_class, size_t extra_field_count);

/// Add GC root, along with its GC functions.
void ow_objmem_add_gc_root(
    struct ow_machine *om, void *root, ow_objmem_obj_fields_visitor_t fn);
/// Remove a GC root added with `ow_objmem_add_gc_root()`.
bool ow_objmem_remove_gc_root(struct ow_machine *om, void *root);

/// Record a weak reference.
void ow_objmem_register_weak_ref(
    struct ow_machine *om, void *ref_container, ow_objmem_weak_refs_visitor_t fn);
/// Remove a weak reference record.
bool ow_objmem_remove_weak_ref(struct ow_machine *om, void *ref_container);

/// GC options.
enum ow_objmem_gc_type {
    OW_OBJMEM_GC_NONE = -1,
    OW_OBJMEM_GC_AUTO,
    OW_OBJMEM_GC_FAST,
    OW_OBJMEM_GC_FULL,
};

/// Run garbage collection.
int ow_objmem_gc(struct ow_machine *om, enum ow_objmem_gc_type type);
/// Get current GC type. Returning `OW_OBJMEM_GC_NONE` means GC is not running.
enum ow_objmem_gc_type ow_objmem_current_gc(struct ow_machine *om);

/// Start a no-GC region.
ow_static_forceinline void ow_objmem_push_ngc(struct ow_machine *om);
/// End a no-GC region.
ow_static_forceinline void ow_objmem_pop_ngc(struct ow_machine *om);
/// Test no-GC flag.
ow_static_forceinline bool ow_objmem_test_ngc(struct ow_machine *om);

enum ow_objmem_obj_visit_op {
    OW_OBJMEM_OBJ_VISIT_MARK_REC, ///< mark reachable object and its fields recursively
    OW_OBJMEM_OBJ_VISIT_MARK_REC_Y, ///< mark reachable young object and its fields recursively
    OW_OBJMEM_OBJ_VISIT_MARK_REC_O2X, ///< mark reachable object referred by old and its fields recursively
    OW_OBJMEM_OBJ_VISIT_MARK_REC_O2Y, ///< mark reachable young object referred by old and its fields recursively
    OW_OBJMEM_OBJ_VISIT_MOVE, ///< update reference to moved object
};

enum ow_objmem_weak_ref_visit_op {
    OW_OBJMEM_WEAK_REF_VISIT_FINI, ///< finalize reference
    OW_OBJMEM_WEAK_REF_VISIT_FINI_Y, ///< finalize reference to young object
    OW_OBJMEM_WEAK_REF_VISIT_MOVE, ///< update reference to moved object
};

/// Record an old object that stores an young object.
/// `obj` must be in old generation and contains young object field.
void ow_objmem_record_o2y_object(struct ow_object *obj);

/// GC: visit an object. Parameter `obj` must be a field in an object, and be
/// the variable itself, so that it is assignable and can be updated correctly.
/// If `obj` is a small int, it will be ignored. Usually used in a
/// `ow_objmem_obj_fields_visitor_t` function.
#define ow_objmem_visit_object(obj, op) \
    do {                                \
        if (ow_unlikely(ow_smallint_check((struct ow_object *)(obj)))) \
            break;                      \
        const enum ow_objmem_obj_visit_op __op = (enum ow_objmem_obj_visit_op)(op); \
        if (__op != OW_OBJMEM_OBJ_VISIT_MOVE)                          \
            _ow_objmem_visit_object_do_mark((struct ow_object *)(obj), __op);       \
        else                            \
            _ow_objmem_visit_object_do_move((struct ow_object **)&(obj));           \
    } while (0)                         \
// ^^^ ow_objmem_visit_object() ^^^

/// GC: visit a weak reference. Parameter `obj` must be a reference, and be
/// the variable itself, so that it is assignable and can be updated correctly.
/// To use this macro, a function-like macro called `WEAK_REF_FINI()` must be defined,
/// which takes an argument that is the object (reference) to finalize.
/// Usually used in a Usually used in a `ow_objmem_weak_refs_visitor_t` function.
#define ow_objmem_visit_weak_ref(obj, op) \
    do {                                  \
        const enum ow_objmem_weak_ref_visit_op __op = (enum ow_objmem_weak_ref_visit_op)(op); \
        if (__op != OW_OBJMEM_WEAK_REF_VISIT_MOVE) {                                          \
            assert(__op == OW_OBJMEM_WEAK_REF_VISIT_FINI || __op == OW_OBJMEM_WEAK_REF_VISIT_FINI_Y); \
            if (__op == OW_OBJMEM_WEAK_REF_VISIT_FINI_Y &&                                    \
                ow_object_meta_test_(OLD, ((struct ow_object *)(obj))->_meta)                 \
            ) {                           \
                break;                    \
            }                             \
            if (!ow_object_meta_test_(MRK, ((struct ow_object *)(obj))->_meta)) {             \
                WEAK_REF_FINI( (obj) );   \
            }                             \
        } else {                          \
            _ow_objmem_visit_object_do_move((struct ow_object **)&(obj));                     \
        }                                 \
    } while (0)                           \
// ^^^ ow_objmem_visit_weak_ref() ^^^

/// Print object memory usage to a file or to stderr (`NULL`).
/// Only available when compile with `OW_DEBUG_MEMORY` being true.
void ow_objmem_print_usage(struct ow_objmem_context *ctx, void *FILE_ptr);

////////////////////////////////////////////////////////////////////////////////

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

void _ow_objmem_mark_object_fields_rec(struct ow_object *obj);
void _ow_objmem_mark_object_young_fields_rec(struct ow_object *obj);
void _ow_objmem_mark_old_referred_object_fields_rec(struct ow_object *obj);
void _ow_objmem_mark_old_referred_object_young_fields_rec(struct ow_object *obj);
void _ow_objmem_move_object_fields(struct ow_object *obj);

ow_static_forceinline void _ow_objmem_visit_object_do_mark(
    struct ow_object *obj, enum ow_objmem_obj_visit_op op
) {
    assert(!ow_smallint_check(obj));

    switch (op) {
    case OW_OBJMEM_OBJ_VISIT_MARK_REC:
        // Ignore if marked.
        if (ow_object_meta_test_(MRK, obj->_meta))
            return;
        // Mark itself.
        ow_object_meta_set_(MRK, obj->_meta);
        // Mark its fields. If old or newly-old (`YOUNG-MID`), use op `OW_OBJMEM_OBJ_VISIT_MARK_REC_O2X`.
        if (ow_object_meta_test_(OLD, obj->_meta) || ow_object_meta_test_(MID, obj->_meta))
            _ow_objmem_mark_old_referred_object_fields_rec(obj);
        else
            _ow_objmem_mark_object_fields_rec(obj);
        return;

    case OW_OBJMEM_OBJ_VISIT_MARK_REC_Y:
        // Ignore if not young or marked.
        if (ow_object_meta_test_(OLD, obj->_meta) || ow_object_meta_test_(MRK, obj->_meta))
            return;
        // Mark itself.
        ow_object_meta_set_(MRK, obj->_meta);
        // Mark its fields. If newly-old (`YOUNG-MID`), use op `OW_OBJMEM_OBJ_VISIT_MARK_REC_O2Y`.
        if (ow_object_meta_test_(MID, obj->_meta))
            _ow_objmem_mark_old_referred_object_young_fields_rec(obj);
        else
            _ow_objmem_mark_object_young_fields_rec(obj);
        return;

    case OW_OBJMEM_OBJ_VISIT_MARK_REC_O2X:
        // Ignore if marked.
        if (ow_object_meta_test_(MRK, obj->_meta))
            return;
        // Mark itself.
        ow_object_meta_set_(MRK, obj->_meta);
        // Make it `YOUNG/MID` if it is `YOUNG` and not `MID`, so that it will become `OLD` after GC.
        if (!ow_object_meta_test_(OLD, obj->_meta) && !ow_object_meta_test_(MID, obj->_meta))
            ow_object_meta_set_(MID, obj->_meta);
        // Mark its fields.
        _ow_objmem_mark_old_referred_object_fields_rec(obj);
        return;

    case OW_OBJMEM_OBJ_VISIT_MARK_REC_O2Y:
        // Ignore if not young or marked.
        if (ow_object_meta_test_(OLD, obj->_meta) || ow_object_meta_test_(MRK, obj->_meta))
            return;
        // Mark itself.
        ow_object_meta_set_(MRK, obj->_meta);
        // Make it `YOUNG/MID`, so that it will become `OLD` after GC.
        if (!ow_object_meta_test_(MID, obj->_meta))
            ow_object_meta_set_(MID, obj->_meta);
        // Mark its fields.
        _ow_objmem_mark_old_referred_object_young_fields_rec(obj);
        return;

    default:
        ow_unreachable();
    }
}

ow_static_forceinline bool _ow_objmem_visit_object_do_move(struct ow_object **obj_ref) {
    struct ow_object *obj = *obj_ref;
    assert(!ow_smallint_check(obj));

    if (!ow_object_meta_test_(MRK, obj->_meta))
        return false;

    // Pointer to the new storage shall have been stored in the PTR field in object meta.
    *obj_ref = ow_object_meta_load_(PTR, struct ow_object *, obj->_meta);

    // This operation is not recursive, so `_ow_objmem_move_object_fields()`
    // is not going to be called.

    return true;
}
