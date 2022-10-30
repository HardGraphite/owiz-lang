#include <stdio.h>

#include <ow.h>

#include <compiler/codegen.h>
#include <compiler/compiler.h>
#include <compiler/error.h>
#include <compiler/lexer.h>
#include <compiler/parser.h>
#include <machine/machine.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <utilities/strings.h>
#include <utilities/stream.h>

static void test(ow_machine_t *const ow) {
	ow_objmem_context_verbose(ow->objmem_context, true);
	struct ow_compiler *compiler = ow_compiler_new(ow);
	struct ow_sharedstr *file_name = ow_sharedstr_new("<stdin>", (size_t)-1);
	struct ow_module_obj *module = ow_module_obj_new(ow);
	ow_objmem_add_gc_root(ow, module, NULL);
	ow_parser_verbose(ow_compiler_parser(compiler), true);
	ow_lexer_verbose(ow_parser_lexer(ow_compiler_parser(compiler)), true);
	ow_codegen_verbose(ow_compiler_codegen(compiler), true);
	if (!ow_compiler_compile(compiler, ow_istream_stdin(), file_name, 0, module)) {
		struct ow_syntax_error *err = ow_compiler_error(compiler);
		printf("%s : %u:%u-%u:%u : %s\n",
			ow_sharedstr_data(file_name),
			err->location.begin.line, err->location.begin.column,
			err->location.end.line, err->location.end.column,
			ow_sharedstr_data(err->message));
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
