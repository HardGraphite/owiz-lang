#include "stack.h"

#include <assert.h>

#include <objects/memory.h>
#include <utilities/attributes.h>
#include <utilities/malloc.h>

#define CALLSTACK_MIN 64

static void ow_callstack_frame_info_list_init(
		struct ow_callstack_frame_info_list *list) {
	list->current = NULL;
	list->_free_list = NULL;
}

static void ow_callstack_frame_info_list_fini(
		struct ow_callstack_frame_info_list *list) {
	struct ow_callstack_frame_info *const lists[2] = {list->current, list->_free_list};
	for (size_t i = 0; i < 2; i++) {
		for (struct ow_callstack_frame_info *fi = lists[i]; fi; ) {
			struct ow_callstack_frame_info *const next_fi = fi->_next;
			ow_free(fi);
			fi = next_fi;
		}
	}
}

void ow_callstack_frame_info_list_enter(
		struct ow_callstack_frame_info_list *list) {
	struct ow_callstack_frame_info *fi;
	if (ow_likely(list->_free_list)) {
		fi = list->_free_list;
		list->_free_list = fi->_next;
	} else {
		fi = ow_malloc(sizeof(struct ow_callstack_frame_info));
	}
	fi->_next = list->current;
	list->current = fi;
}

void ow_callstack_frame_info_list_leave(
		struct ow_callstack_frame_info_list *list) {
	assert(list->current);
	struct ow_callstack_frame_info *const fi = list->current;
	list->current = fi->_next;
	fi->_next = list->_free_list;
	list->_free_list = fi;
}

void ow_callstack_init(struct ow_callstack *stack, size_t n) {
	if (n < CALLSTACK_MIN)
		n = CALLSTACK_MIN;
	stack->_data = ow_malloc(sizeof(struct ow_object *) * n);
	stack->regs.sp = stack->_data - 1;
	stack->regs.fp = stack->_data;
	stack->data_end = stack->_data + n;
	ow_callstack_frame_info_list_init(&stack->frame_info_list);
}

void ow_callstack_fini(struct ow_callstack *stack) {
	ow_callstack_frame_info_list_fini(&stack->frame_info_list);
	ow_free(stack->_data);
}

void ow_callstack_clear(struct ow_callstack *stack) {
	stack->regs.sp = stack->_data - 1;
	stack->regs.fp = stack->_data;
}

void _ow_callstack_gc_marker(struct ow_machine *om, struct ow_callstack *stack) {
	assert(stack->regs.sp < stack->data_end);
	for (struct ow_object **p = stack->_data, **const p_end = stack->regs.sp;
			p < p_end; p++) {
		ow_objmem_object_gc_marker(om, *p);
	}
}
