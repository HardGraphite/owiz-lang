#pragma once

#include <stdbool.h>

#include <utilities/attributes.h>

struct ow_codegen;
struct ow_istream;
struct ow_machine;
struct ow_module_obj;
struct ow_parser;
struct ow_sharedstr;
struct ow_syntax_error;

/// The compiler.
struct ow_compiler;

/// Create a compiler.
ow_nodiscard struct ow_compiler *ow_compiler_new(struct ow_machine *om);
/// Destroy a compiler.
void ow_compiler_del(struct ow_compiler *compiler);
/// Get the parser of a compiler.
struct ow_parser *ow_compiler_parser(struct ow_compiler *compiler);
/// Get the code generator of a compiler.
struct ow_codegen *ow_compiler_codegen(struct ow_compiler *compiler);
/// Clear a compiler.
void ow_compiler_clear(struct ow_compiler *compiler);
/// Do compilation. Return whether successful.
bool ow_compiler_compile(
	struct ow_compiler *compiler,
	struct ow_istream *stream, struct ow_sharedstr *file_name,
	int flags, struct ow_module_obj *module);
/// Get last error.
struct ow_syntax_error *ow_compiler_error(struct ow_compiler *compiler);
