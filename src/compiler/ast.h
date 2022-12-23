#pragma once

#include <stdint.h>

#include "location.h"

struct ow_ast_Module;
struct ow_stream;
struct ow_sharedstr;

/// Abstract syntax tree.
struct ow_ast {
	struct ow_sharedstr *_file_name;
	struct ow_ast_Module *_module;
};

/// Initialize an AST.
void ow_ast_init(struct ow_ast *ast);
/// Finalize an AST.
void ow_ast_fini(struct ow_ast *ast);
/// Set file name.
void ow_ast_set_filename(struct ow_ast *ast, struct ow_sharedstr *name);
/// Get file name. Return NULL if not set.
struct ow_sharedstr *ow_ast_get_filename(const struct ow_ast *ast);
/// Set module node.
void ow_ast_set_module(struct ow_ast *ast, struct ow_ast_Module *mod);
/// Get module node. Return NULL if not set.
struct ow_ast_Module *ow_ast_get_module(const struct ow_ast *ast);
/// Print the AST.
void ow_ast_dump(const struct ow_ast *ast, struct ow_stream *out);

#include "ast_node_list.h"

#define OW_AST_NODE_HEAD \
	struct ow_source_range location; \
	enum ow_ast_node_type type;      \
// ^^^ OW_AST_NODE_HEAD ^^^

/// Type of AST node.
enum ow_ast_node_type {
#define ELEM(NAME) OW_AST_NODE_##NAME ,
	OW_AST_NODE_LIST
#undef ELEM
	_OW_AST_NODE_TYPE_COUNT
};

/// Abstract AST node.
struct ow_ast_node {
	OW_AST_NODE_HEAD
};

/// Get name of an AST node.
const char *ow_ast_node_name(const struct ow_ast_node *node);

#define ELEM(NAME) \
	struct ow_ast_##NAME *ow_ast_##NAME##_new(void);
OW_AST_NODE_LIST
#undef ELEM

/// Delete an AST node.
void ow_ast_node_del(struct ow_ast_node *node);

/// Abstract expression node.
struct ow_ast_Expr {
	OW_AST_NODE_HEAD
};

/// Abstract statement node.
struct ow_ast_Stmt {
	OW_AST_NODE_HEAD
};

#include "ast_node_collections.h"

#include "ast_node_structs.h"

#undef OW_AST_NODE_HEAD
