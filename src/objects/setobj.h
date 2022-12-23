#pragma once

#include <stddef.h>

struct ow_machine;
struct ow_object;

/// Set object.
struct ow_set_obj;

/// Create an empty set object.
struct ow_set_obj *ow_set_obj_new(struct ow_machine *om);
/// Insert an element.
void ow_set_obj_insert(
	struct ow_machine *om, struct ow_set_obj *self, struct ow_object *val);
/// Get number of elements.
size_t ow_set_obj_length(const struct ow_set_obj *self);
/// Visit each element.
int ow_set_obj_foreach(
	const struct ow_set_obj *self,
	int (*walker)(void *arg, struct ow_object *elem), void *arg);
