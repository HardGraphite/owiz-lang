#pragma once

#include <stdbool.h>

#include <utilities/attributes.h>

struct ow_ast;
struct ow_machine;
struct ow_module_obj;
struct ow_syntax_error;

/// Code generator, converting AST to byte code.
struct ow_codegen;

/// Create a code generator.
ow_nodiscard struct ow_codegen *ow_codegen_new(struct ow_machine *om);
/// Destroy a code generator.
void ow_codegen_del(struct ow_codegen *codegen);
/// Config whether prints details.
void ow_codegen_verbose(struct ow_codegen *codegen, bool status);
/// Delete data and reset.
void ow_codegen_clear(struct ow_codegen *codegen);
/// Do code generation.
bool ow_codegen_generate(
    struct ow_codegen *codegen,
    const struct ow_ast *ast, int flags,
    struct ow_module_obj *module);
/// Get last error. If no error ever occurred, return NULL.
struct ow_syntax_error *ow_codegen_error(struct ow_codegen *codegen);
