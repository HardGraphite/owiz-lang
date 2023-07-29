#pragma once

#include <stddef.h>

struct ow_machine;
struct ow_object;

/// Map object.
struct ow_map_obj;

/// Create an empty map object.
struct ow_map_obj *ow_map_obj_new(struct ow_machine *om);
/// Get number of elements.
size_t ow_map_obj_length(const struct ow_map_obj *self);
/// Insert or assign.
void ow_map_obj_set(
    struct ow_machine *om, struct ow_map_obj *self,
    struct ow_object *key, struct ow_object *val);
/// Get value by key. Return `NULL` if the key does not exist.
struct ow_object *ow_map_obj_get(
    struct ow_machine *om, struct ow_map_obj *self, struct ow_object *key);
/// Visit each key-value pair.
int ow_map_obj_foreach(
    const struct ow_map_obj *self,
    int (*walker)(void *arg, struct ow_object *key, struct ow_object *val), void *arg);
