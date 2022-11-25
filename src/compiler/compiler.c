#include "compiler.h"

#include "ast.h"
#include "codegen.h"
#include "parser.h"
#include <utilities/malloc.h>
#include <utilities/unreachable.h>

enum last_error_source {
	ERR_SRC_NONE,
	ERR_SRC_PARSER,
	ERR_SRC_CODEGEN,
};

struct ow_compiler {
	struct ow_parser *parser;
	struct ow_codegen *codegen;
	enum last_error_source last_error_source;
};

ow_nodiscard struct ow_compiler *ow_compiler_new(struct ow_machine *om) {
	struct ow_compiler *const compiler = ow_malloc(sizeof(struct ow_compiler));
	compiler->parser = ow_parser_new();
	compiler->codegen = ow_codegen_new(om);
	compiler->last_error_source = ERR_SRC_NONE;
	return compiler;
}

void ow_compiler_del(struct ow_compiler *compiler) {
	ow_parser_del(compiler->parser);
	ow_codegen_del(compiler->codegen);
	ow_free(compiler);
}

struct ow_parser *ow_compiler_parser(struct ow_compiler *compiler) {
	return compiler->parser;
}

struct ow_codegen *ow_compiler_codegen(struct ow_compiler *compiler) {
	return compiler->codegen;
}

void ow_compiler_clear(struct ow_compiler *compiler) {
	ow_parser_clear(compiler->parser);
	ow_codegen_clear(compiler->codegen);
	compiler->last_error_source = ERR_SRC_NONE;
}

bool ow_compiler_compile(
		struct ow_compiler *compiler,
		struct ow_istream *stream, struct ow_sharedstr *file_name,
		int flags, struct ow_module_obj *module) {
	ow_unused_var(flags);

	struct ow_ast ast;
	ow_ast_init(&ast);

	compiler->last_error_source = ERR_SRC_NONE;
	if (!ow_parser_parse(compiler->parser, stream, file_name, 0, &ast))
		compiler->last_error_source = ERR_SRC_PARSER;
	else if (!ow_codegen_generate(compiler->codegen, &ast, 0, module))
		compiler->last_error_source = ERR_SRC_CODEGEN;

	ow_ast_fini(&ast);
	return compiler->last_error_source == ERR_SRC_NONE;
}

struct ow_syntax_error *ow_compiler_error(struct ow_compiler *compiler) {
	switch (compiler->last_error_source) {
	case ERR_SRC_NONE:
		return NULL;
	case ERR_SRC_PARSER:
		return ow_parser_error(compiler->parser);
	case ERR_SRC_CODEGEN:
		return ow_codegen_error(compiler->codegen);
	default:
		ow_unreachable();
	}
}
