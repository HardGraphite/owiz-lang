#include "memory.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#if OW_DEBUG_MEMORY
#	include <stdio.h>
#	include <time.h>
#endif // OW_DEBUG_MEMORY

#include "classobj.h"
#include "natives.h"
#include "object.h"
#include "object_util.h"
#include "smallint.h"
#include <machine/machine.h>
#include <utilities/malloc.h>

#define DEFAULT_GC_THRESHOLD (sizeof(void *) * 1024 * 1024 / 4)
#define DEFAULT_ALLOCATE_MAX (sizeof(void *) * 8 * 1024 * 1024)

struct object_linked_list {
	OW_OBJECT_HEAD
};

ow_forceinline static void object_linked_list_init(struct object_linked_list *list) {
	ow_object_meta_set_next(&list->_meta, NULL);
}

ow_forceinline static bool object_linked_list_empty(struct object_linked_list *list) {
	return ow_object_meta_get_next(&list->_meta);
}

ow_forceinline static struct ow_object *object_linked_list_first(
		struct object_linked_list *list) {
	return ow_object_meta_get_next(&list->_meta);
}

ow_forceinline static struct ow_object *object_linked_list_pre_first(
		struct object_linked_list *list) {
	return (struct ow_object *)list;
}

ow_forceinline static void object_linked_list_add(
		struct object_linked_list *list, struct ow_object *obj) {
	ow_object_meta_set_next(&obj->_meta, ow_object_meta_get_next(&list->_meta));
	ow_object_meta_set_next(&list->_meta, obj);
}

struct ow_objmem_context {
	size_t no_gc_count;
	size_t gc_threshold;
	size_t allocate_max;
	size_t allocated_size;
	struct object_linked_list allocated_objects;
#if OW_DEBUG_MEMORY
	bool verbose;
#endif // OW_DEBUG_MEMORY
};

static_assert(
	offsetof(struct ow_machine, objmem_context) == 0 &&
	offsetof(struct ow_objmem_context, no_gc_count) == 0,
	"_ow_objmem_ngc_count()");

struct ow_objmem_context *ow_objmem_context_new(void) {
	struct ow_objmem_context *const ctx = ow_malloc(sizeof(struct ow_objmem_context));
	ctx->no_gc_count = 0;
	ctx->gc_threshold = DEFAULT_GC_THRESHOLD;
	ctx->allocate_max = DEFAULT_ALLOCATE_MAX;
	ctx->allocated_size = 0;
	object_linked_list_init(&ctx->allocated_objects);
#if OW_DEBUG_MEMORY
	ctx->verbose = false;
#endif // OW_DEBUG_MEMORY
	return ctx;
}

void ow_objmem_context_del(struct ow_objmem_context *ctx) {
	for (struct ow_object *obj = object_linked_list_first(&ctx->allocated_objects); obj; ) {
		struct ow_object *const next_obj = ow_object_meta_get_next(&obj->_meta);
		ow_free(obj); // Not safe!!!
		obj = next_obj;
	}

	ow_free(ctx);
}

void ow_objmem_context_verbose(struct ow_objmem_context *ctx, bool status) {
	ow_unused_var(ctx);
	ow_unused_var(status);
#if OW_DEBUG_PARSER
	ctx->verbose = status;
#endif // OW_DEBUG_PARSER
}

struct ow_object *ow_objmem_allocate(
		struct ow_machine *om, struct ow_class_obj *obj_class, size_t extra_field_count) {
	struct ow_objmem_context *const ctx = om->objmem_context;

	if (ow_unlikely(ctx->allocated_size >= ctx->gc_threshold)) {
		ow_objmem_gc(om, 0);
		if (ow_unlikely(ctx->allocated_size >= ctx->allocate_max))
			abort(); // Out of memory.
	}

	const size_t obj_field_count =
		_ow_class_obj_pub_info(obj_class)->basic_field_count + extra_field_count;
	const size_t obj_size = OW_OBJECT_SIZE + obj_field_count * sizeof(void *);
	struct ow_object *const obj = ow_malloc(obj_size);
	ctx->allocated_size += obj_size;

	static_assert(sizeof obj->_meta == sizeof(uint64_t), "");
	*(uint64_t *)&obj->_meta = 0;
	if (ow_unlikely(extra_field_count)) {
		ow_object_meta_set_flag(&obj->_meta, OW_OBJMETA_FLAG_EXTENDED);
		ow_object_set_field(obj, 0, (void *)(uintptr_t)obj_field_count);
	}
	obj->_class = obj_class;

	object_linked_list_add(&ctx->allocated_objects, obj);

	assert(!ow_smallint_check(obj));
	return obj;
}

ow_forceinline static size_t delete_obj(struct ow_machine *om, struct ow_object *obj) {
	size_t obj_field_count;
	if (ow_unlikely(ow_object_meta_get_flag(&obj->_meta, OW_OBJMETA_FLAG_EXTENDED)))
		obj_field_count = (uintptr_t)ow_object_get_field(obj, 0);
	else
		obj_field_count = _ow_class_obj_pub_info(obj->_class)->basic_field_count;
	const size_t obj_size = OW_OBJECT_SIZE + obj_field_count * sizeof(void *);

	void (*const finalizer)(struct ow_machine *,struct ow_object *) =
		_ow_class_obj_pub_info(obj->_class)->finalizer;
	if (ow_unlikely(finalizer))
		finalizer(om, obj);
	ow_free(obj);

	return obj_size;
}

int ow_objmem_gc(struct ow_machine *om, int flags) {
	ow_unused_var(flags);
	struct ow_objmem_context *const ctx = om->objmem_context;

	if (ow_unlikely(ctx->no_gc_count
			|| !object_linked_list_empty(&ctx->allocated_objects)))
		return -1;
	ctx->no_gc_count = 1;

#if OW_DEBUG_MEMORY
	struct timespec ts0;
	timespec_get(&ts0, TIME_UTC);
#endif // OW_DEBUG_MEMORY

	_om_machine_gc_marker(om);

	size_t freed_size = 0;

	for (struct ow_object *prev_obj = object_linked_list_pre_first(&ctx->allocated_objects); ; ) {
		struct ow_object *obj = ow_object_meta_get_next(&prev_obj->_meta);
	re_process_obj:
		if (ow_unlikely(!obj))
			break;

		if (ow_object_meta_get_flag(&obj->_meta, OW_OBJMETA_FLAG_MARKED)) {
			ow_object_meta_clear_flag(&obj->_meta, OW_OBJMETA_FLAG_MARKED);
			prev_obj = obj;
		} else {
			struct ow_object *const next_obj = ow_object_meta_get_next(&obj->_meta);
			freed_size += delete_obj(om, obj);
			ow_object_meta_set_next(&prev_obj->_meta, next_obj);
			obj = next_obj;
			goto re_process_obj;
		}
	}

	assert(ctx->no_gc_count == 1);
	ctx->no_gc_count = 0;

	assert(freed_size <= ctx->allocated_size);
	const size_t alive_size = ctx->allocated_size - freed_size;
	ctx->allocated_size = alive_size;
	if (ow_unlikely(freed_size < alive_size / 4)) {
		const size_t new_th = alive_size + alive_size / 2;
		ctx->gc_threshold = new_th;
	} else if (ow_unlikely(freed_size > alive_size * 3)) {
		const size_t new_th = alive_size * 2;
		ctx->gc_threshold = new_th > DEFAULT_GC_THRESHOLD ? new_th : DEFAULT_GC_THRESHOLD;
	}

#if OW_DEBUG_MEMORY
	if (ow_unlikely(ctx->verbose)) {
		struct timespec ts1;
		timespec_get(&ts1, TIME_UTC);
		double dt_ms =
			(double)(ts1.tv_sec - ts0.tv_sec) * 1e3 +
			(double)(ts1.tv_nsec - ts0.tv_nsec) / 1e6;

		fprintf(
			stderr, "[GC] %zu B freed, %zu B alive, next-threshold = %zu B; %.1lf ms\n",
			freed_size, alive_size, ctx->gc_threshold, dt_ms);
	}
#endif // OW_DEBUG_MEMORY

	return 0;
}

void _ow_objmem_object_gc_mark_children_obj(
		struct ow_machine *om, struct ow_object *obj) {
	struct ow_class_obj *const obj_class = ow_object_class(obj);
	ow_objmem_object_gc_marker(om, ow_object_from(obj_class));

	const struct _ow_class_obj_pub_info *const info =
		_ow_class_obj_pub_info(obj_class);

	size_t field_index;
	if (ow_unlikely(info->native_field_count)) {
		void (*const marker)(struct ow_machine *,struct ow_object *) = info->gc_marker;
		if (ow_likely(marker))
			marker(om, obj);
		field_index = info->native_field_count;
	} else {
		field_index = 0;
	}

	const size_t field_count = info->basic_field_count; // Ignore extra (extended) fields.
	assert(field_index <= field_count);
	for (; field_index < field_count; field_index++)
		ow_objmem_object_gc_marker(om, ow_object_get_field(obj, field_index));
}
