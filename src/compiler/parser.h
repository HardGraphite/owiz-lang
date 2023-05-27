#pragma once

#include <stdbool.h>

#include <utilities/attributes.h>

struct ow_ast;
struct ow_stream;
struct ow_lexer;
struct ow_sharedstr;
struct ow_syntax_error;

/// Syntax analyzer, converting source code to AST.
struct ow_parser;

/// Create a parser.
ow_nodiscard struct ow_parser *ow_parser_new(void);
/// Delete a parser created by `ow_parser_new()`.
void ow_parser_del(struct ow_parser *parser);
/// Get the lexer in a parser.
struct ow_lexer *ow_parser_lexer(struct ow_parser *parser);
/// Config whether the parser prints details.
void ow_parser_verbose(struct ow_parser *parser, bool status);
/// Delete data and reset.
void ow_parser_clear(struct ow_parser *parser);
/// Do parsing. On success, store result to param `ast` and return true;
/// on failure, record the error info and return false.
bool ow_parser_parse(struct ow_parser *parser,
    struct ow_stream *stream, struct ow_sharedstr *file_name, int flags,
    struct ow_ast *ast);
/// Get last error. If no error ever occurred, return NULL.
struct ow_syntax_error *ow_parser_error(struct ow_parser *parser);
