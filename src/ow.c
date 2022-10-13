#include <stdio.h>

#include <compiler/lexer.h>
#include <compiler/token.h>
#include <utilities/strings.h>
#include <utilities/stream.h>

static void test(void) {
	struct ow_lexer *lexer = ow_lexer_new();
	struct ow_sharedstr *file_name = ow_sharedstr_new("<stdin>", (size_t)-1);
	ow_lexer_source(lexer, ow_istream_stdin(), file_name);
	ow_sharedstr_unref(file_name);
	struct ow_token token;
	ow_token_init(&token);
	ow_lexer_verbose(lexer, true);
	while (true) {
		if (!ow_lexer_next(lexer, &token)) {
			struct ow_lexer_error *err = ow_lexer_error(lexer);
			printf("%u:%u: %s\n",
				err->location.line, err->location.column,
				ow_sharedstr_data(err->message));
			break;
		}
		if (ow_token_type(&token) == OW_TOK_END)
			break;
	}
	ow_token_fini(&token);
	ow_lexer_del(lexer);
}

int main(void) {
	test();
}
