#include "classes.h"

#include "classes_util.h"
#include "classobj.h"
#include "objmem.h"
#include "natives.h"
#include "object.h"
#include <machine/machine.h>
#include <utilities/attributes.h>
#include <utilities/memalloc.h>

#define ELEM(NAME) \
    extern OW_BICLS_CLASS_DEF_EX(NAME) ;
OW_BICLS_LIST0
OW_BICLS_LIST
OW_BICLS_STREAM_LIST
#undef ELEM

static void ow_builtin_classes_gc_visitor(void *_ptr, int op) {
    struct ow_builtin_classes * const bic = _ptr;
    for (size_t i = 0; i < sizeof *bic / sizeof(struct ow_class_obj *); i++)
        ow_objmem_visit_object(((struct ow_class_obj **)bic)[i], op);
}

struct ow_builtin_classes *_ow_builtin_classes_new(struct ow_machine *om) {
    struct ow_builtin_classes *const bic = ow_malloc(sizeof(struct ow_builtin_classes));

    assert(!om->builtin_classes);
    om->builtin_classes = bic; // Otherwise, `ow_class_obj_new()` will crash.

    // Function `ow_objmem_allocate()` reads `basic_field_count` field in
    // `struct ow_class_obj`. So, this field must be filled to avoid errors.

    struct {
        OW_OBJECT_HEAD
        struct ow_class_obj_pub_info pub_info;
    } fake_class_class;
    fake_class_class.pub_info.basic_field_count =
        OW_BICLS_CLASS_DEF_EX_NAME(class_).data_size / OW_OBJECT_FIELD_SIZE;
    fake_class_class.pub_info.has_extra_fields = false;
    bic->class_ = (struct ow_class_obj *)&fake_class_class;
    bic->class_ = ow_class_obj_new(om);
    ow_object_meta_store_(CLS, ow_object_from(bic->class_)->_meta, bic->class_); // Set class.
    ((struct ow_class_obj_pub_info *)ow_class_obj_pub_info(bic->class_))
        ->basic_field_count = fake_class_class.pub_info.basic_field_count;

    bic->object = ow_class_obj_new(om);
#define ELEM(NAME) { \
        struct ow_class_obj *const cls = ow_class_obj_new(om); \
        struct ow_class_obj_pub_info *const cls_pub_info = \
            (struct ow_class_obj_pub_info *)ow_class_obj_pub_info(cls); \
        const struct ow_native_class_def_ex *const cls_def = \
            &OW_BICLS_CLASS_DEF_EX_NAME(NAME); \
        cls_pub_info->basic_field_count = \
            cls_def->data_size / OW_OBJECT_FIELD_SIZE; \
        bic-> NAME = cls; \
    }
    OW_BICLS_LIST
    OW_BICLS_STREAM_LIST
#undef ELEM

#define ELEM(NAME) assert(ow_object_class(ow_object_from(bic-> NAME )) == bic->class_);
    OW_BICLS_LIST0
    OW_BICLS_LIST
    OW_BICLS_STREAM_LIST
#undef ELEM

    assert(om->builtin_classes == bic);
    om->builtin_classes = NULL;

    ow_objmem_add_gc_root(om, bic, ow_builtin_classes_gc_visitor);

    return bic;
}

void _ow_builtin_classes_setup(
    struct ow_machine *om, struct ow_builtin_classes *bic
) {
    ow_objmem_push_ngc(om);

#define MARK_EXTENDED(NAME) \
    do { \
        assert(OW_BICLS_CLASS_DEF_EX_NAME(NAME).extended); \
        ((struct ow_class_obj_pub_info *)ow_class_obj_pub_info(bic-> NAME)) \
            ->has_extra_fields = true; \
    } while (false) \
// ^^^ MARK_EXTENDED() ^^^
    MARK_EXTENDED(string);
    MARK_EXTENDED(symbol);
    MARK_EXTENDED(tuple);
#undef MARK_EXTENDED // MARK_EXTENDED

#define ELEM(NAME) \
    ow_class_obj_load_native_def_ex( \
        om, bic-> NAME, bic->object, &OW_BICLS_CLASS_DEF_EX_NAME(NAME), NULL);
    OW_BICLS_LIST0
    OW_BICLS_LIST
#undef ELEM
    ((struct ow_class_obj_pub_info *)ow_class_obj_pub_info(bic->object))
        ->super_class = NULL;

#define ELEM(NAME) \
    ow_class_obj_load_native_def_ex( \
        om, bic-> NAME, bic->stream, &OW_BICLS_CLASS_DEF_EX_NAME(NAME), NULL);
    OW_BICLS_STREAM_LIST
#undef ELEM

    ow_objmem_pop_ngc(om);
}

void _ow_builtin_classes_del(
    struct ow_machine *om, struct ow_builtin_classes * bic
) {
    ow_objmem_remove_gc_root(om, bic);
    ow_free(bic);
}
