#include "object.h"

#include <assert.h>

#include "classes.h"
#include "classes_util.h"
#include "intobj.h"
#include "natives.h"
#include "object_util.h"
#include <machine/globals.h>
#include <machine/invoke.h>
#include <machine/machine.h>
#include <machine/symbols.h>
#include <utilities/hash.h>
#include <utilities/hashmap.h>

static_assert(
	sizeof(struct ow_object) ==
		(sizeof(struct ow_object_meta) + sizeof(struct ow_class_obj *)),
	"bad size of struct ow_object"
);

static_assert(OW_OBJECT_SIZE == sizeof(struct ow_object), "OW_OBJECT_SIZE is incorrect");

static bool _ow_object_hashmap_funcs_key_equal(
		void *ctx, const void *key_new, const void *key_stored) {
	if (ow_unlikely(key_new == key_stored))
		return true;
	struct ow_machine *const om = ctx;
	struct ow_object *const lhs = (void *)key_new, *const rhs = (void *)key_stored;
	if (ow_unlikely(ow_smallint_check(lhs) && ow_smallint_check(rhs)))
		return ow_smallint_from_ptr(lhs) == ow_smallint_from_ptr(rhs);
	struct ow_object *cmp_res;
	const int cmp_status = ow_machine_call_method(
		om, om->common_symbols->cmp, 2,
		(struct ow_object *[]){om->globals->value_nil, lhs, rhs}, &cmp_res);
	if (cmp_status != 0)
		return false; // TODO: Return an exception.
	if (ow_smallint_check(cmp_res))
		return ow_smallint_from_ptr(cmp_res) == 0;
	if (ow_object_class(cmp_res) == om->builtin_classes->int_)
		return ow_int_obj_value(ow_object_cast(cmp_res, struct ow_int_obj)) == 0;
	return false;
}

static ow_hash_t _ow_object_hashmap_funcs_key_hash(
		void *ctx, const void *key_new) {
	struct ow_machine *const om = ctx;
	struct ow_object *val = (void *)key_new;
	if (ow_unlikely(ow_smallint_check(val))) {
		return
#if OW_SMALLINT_MAX == INT64_MAX / 2
			ow_hash_int64
#else
			ow_hash_int
#endif
			(ow_smallint_from_ptr(val));
	}
	struct ow_object *hash_res;
	const int hash_status = ow_machine_call_method(
		om, om->common_symbols->hash, 1, &val, &hash_res);
	if (hash_status != 0)
		return 0; // TODO: Return an exception.
	if (ow_smallint_check(hash_res))
		return (ow_hash_t)ow_smallint_from_ptr(hash_res);
	if (ow_object_class(hash_res) == om->builtin_classes->int_)
		return (ow_hash_t)ow_int_obj_value(ow_object_cast(hash_res, struct ow_int_obj));
	return 0; // TODO: Return an exception.
}

const struct ow_hashmap_funcs _ow_object_hashmap_funcs_tmpl = {
	_ow_object_hashmap_funcs_key_equal,
	_ow_object_hashmap_funcs_key_hash,
	NULL,
};

static const struct ow_native_func_def object_methods[] = {
	{NULL, NULL, 0, 0},
};

OW_BICLS_CLASS_DEF_EX(object) = {
	.name      = "Object",
	.data_size = OW_OBJ_STRUCT_DATA_SIZE(struct ow_object),
	.methods   = object_methods,
	.finalizer = NULL,
	.gc_marker = NULL,
	.extended  = false,
};
