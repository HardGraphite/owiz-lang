#pragma once

#include <stdbool.h>
#include <stddef.h>

struct ow_machine;
struct ow_object;

/// Information of a call stack frame.
struct ow_callstack_frame_info {
	bool not_ret_val;
	struct ow_object **arg_list;
	struct ow_object **prev_fp;
	const unsigned char *prev_ip;
	struct ow_callstack_frame_info *_next;
};

/// A linked list of frame info.
struct ow_callstack_frame_info_list {
	struct ow_callstack_frame_info *current;
	struct ow_callstack_frame_info *_free_list;
};

/// Enter a new frame. All members of `list->current` must be filled after returns.
void ow_callstack_frame_info_list_enter(struct ow_callstack_frame_info_list *list);
/// Leave current frame.
void ow_callstack_frame_info_list_leave(struct ow_callstack_frame_info_list *list);

/// Registers used in call stack.
struct ow_callstack_regs {
	struct ow_object **sp; ///< Stack top.
	struct ow_object **fp; ///< Frame base.
};

/// The runtime call stack.
struct ow_callstack {
	struct ow_callstack_regs regs;
	struct ow_callstack_frame_info_list frame_info_list;
	struct ow_object **data_end;
	struct ow_object **_data;
};

/// Create a call stack.
void ow_callstack_init(struct ow_callstack *stack, size_t n);
/// Destroy a call stack.
void ow_callstack_fini(struct ow_callstack *stack);
/// Clear stack.
void ow_callstack_clear(struct ow_callstack *stack);
/// Push object to stack.
#define ow_callstack_push(stack, object)  (*++(stack).regs.sp = (object))
/// Pop top object on stack.
#define ow_callstack_pop(stack)  ((stack).regs.sp--)
/// Get top object on stack.
#define ow_callstack_top(stack)  (*(stack).regs.sp)

void _ow_callstack_gc_marker(struct ow_machine *om, struct ow_callstack *stack);
