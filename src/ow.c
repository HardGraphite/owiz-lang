#include <stdio.h>

#include <compiler/ast.h>
#include <compiler/lexer.h>
#include <compiler/parser.h>
#include <utilities/strings.h>
#include <utilities/stream.h>

static void test(void) {
	struct ow_parser *parser = ow_parser_new();
	struct ow_sharedstr *file_name = ow_sharedstr_new("<stdin>", (size_t)-1);
	struct ow_ast ast;
	ow_ast_init(&ast);
	ow_parser_verbose(parser, true);
	ow_lexer_verbose(ow_parser_lexer(parser), true);
	if (!ow_parser_parse(parser, ow_istream_stdin(), file_name, 0, &ast)) {
		struct ow_parser_error *err = ow_parser_error(parser);
		printf("%s : %u:%u-%u:%u : %s\n",
			ow_sharedstr_data(file_name),
			err->location.begin.line, err->location.begin.column,
			err->location.end.line, err->location.end.column,
			ow_sharedstr_data(err->message));
	}
	ow_ast_fini(&ast);
	ow_sharedstr_unref(file_name);
	ow_parser_del(parser);
}

int main(void) {
	test();
}
