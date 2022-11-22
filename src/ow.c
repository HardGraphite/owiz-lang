#include <stdio.h>

#include <ow.h>

#include <compiler/codegen.h>
#include <compiler/compiler.h>
#include <compiler/error.h>
#include <compiler/lexer.h>
#include <compiler/parser.h>
#include <machine/invoke.h>
#include <machine/machine.h>
#include <objects/cfuncobj.h>
#include <objects/classes.h>
#include <objects/exceptionobj.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <objects/stringobj.h>
#include <utilities/strings.h>
#include <utilities/stream.h>

static void test(ow_machine_t *const ow) {
	struct ow_compiler *compiler = ow_compiler_new(ow);
	struct ow_sharedstr *file_name = ow_sharedstr_new("<stdin>", (size_t)-1);
	struct ow_module_obj *module = ow_module_obj_new(ow);
	struct ow_object *result;
	ow_objmem_add_gc_root(ow, module, NULL);
#if 0
	ow_objmem_context_verbose(ow->objmem_context, true);
	ow_parser_verbose(ow_compiler_parser(compiler), true);
	ow_lexer_verbose(ow_parser_lexer(ow_compiler_parser(compiler)), true);
	ow_codegen_verbose(ow_compiler_codegen(compiler), true);
#endif
	if (!ow_compiler_compile(compiler, ow_istream_stdin(), file_name, 0, module)) {
		struct ow_syntax_error *err = ow_compiler_error(compiler);
		printf("%s : %u:%u-%u:%u : %s\n",
			ow_sharedstr_data(file_name),
			err->location.begin.line, err->location.begin.column,
			err->location.end.line, err->location.end.column,
			ow_sharedstr_data(err->message));
	}
	if (ow_machine_run(ow, module, &result)) {
		struct ow_exception_obj *const exc = ow_object_cast(result, struct ow_exception_obj);
		struct ow_object *const exc_data = ow_exception_obj_data(exc);
		printf("exception : %s\n",
			ow_object_class(exc_data) == ow->builtin_classes->string ?
				ow_string_obj_flatten(
					ow, ow_object_cast(exc_data, struct ow_string_obj), NULL) :
				"...");
		puts("stack trace:");
		size_t bt_count;
		const struct ow_exception_obj_frame_info *const bt =
			ow_exception_obj_backtrace(exc, &bt_count);
		for (size_t i = 0; i < bt_count; i++) {
			struct ow_object *const func = bt[i].function;
			const char *func_name;
			assert(!ow_smallint_check(func));
			struct ow_class_obj *const func_class = ow_object_class(func);
			if (func_class == ow->builtin_classes->cfunc)
				func_name = ow_object_cast(func, struct ow_cfunc_obj)->name;
			else
				func_name = "???";
			printf(" [%zu] %s @%p\n", i, func_name, (void *)bt[i].ip);
		}
	}
	ow_objmem_remove_gc_root(ow, module);
	ow_sharedstr_unref(file_name);
	ow_compiler_del(compiler);
}

int main(void) {
	ow_machine_t *const ow = ow_create();
	test(ow);
	ow_destroy(ow);
}
