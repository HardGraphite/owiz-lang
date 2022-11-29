#include "modules_util.h"

#include <inttypes.h>
#include <stdio.h>

#include <machine/machine.h>
#include <objects/classes.h>
#include <objects/classobj.h>
#include <objects/floatobj.h>
#include <objects/intobj.h>
#include <objects/object.h>
#include <objects/stringobj.h>
#include <objects/symbolobj.h>

static int func_print(struct ow_machine *om) {
	FILE *const fp = stdout;
	struct ow_object *const obj = om->callstack.regs.fp[-1];
	if (ow_smallint_check(obj)) {
		fprintf(fp, "%ji", (intmax_t)ow_smallint_from_ptr(obj));
	} else {
		struct ow_class_obj *const obj_cls = ow_object_class(obj);
		if (obj_cls == om->builtin_classes->int_) {
			struct ow_int_obj *const int_o =
				ow_object_cast(obj, struct ow_int_obj);
			fprintf(fp, "%" PRIi64, ow_int_obj_value(int_o));
		} else if (obj_cls == om->builtin_classes->float_) {
			struct ow_float_obj *const flt_o =
				ow_object_cast(obj, struct ow_float_obj);
			fprintf(fp, "%f", ow_float_obj_value(flt_o));
		} else if (obj_cls == om->builtin_classes->string) {
			struct ow_string_obj *const str_o =
				ow_object_cast(obj, struct ow_string_obj);
			fputs(ow_string_obj_flatten(om, str_o, NULL), fp);
		} else {
			const char *cls_name =
				ow_symbol_obj_data(_ow_class_obj_pub_info(obj_cls)->class_name);
			fprintf(fp, "<%s>", cls_name);
		}
	}
	return 0;
}

static const struct ow_native_func_def functions[] = {
	{"print", func_print, 1},
	{NULL, NULL, 0},
};

OW_BIMOD_MODULE_DEF(base) = {
	.name = "__base__",
	.functions = functions,
	.finalizer = NULL,
};
