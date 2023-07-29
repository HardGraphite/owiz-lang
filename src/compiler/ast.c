#include "ast.h"

#include <assert.h>
#include <stddef.h>

#include <utilities/attributes.h>
#include <utilities/memalloc.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

void ow_ast_init(struct ow_ast *ast) {
    ast->_file_name = NULL;
    ast->_module = NULL;
}

void ow_ast_fini(struct ow_ast *ast) {
    if (ast->_file_name)
        ow_sharedstr_unref(ast->_file_name);
    if (ast->_module)
        ow_ast_node_del((struct ow_ast_node *)ast->_module);
}

void ow_ast_set_filename(struct ow_ast *ast, struct ow_sharedstr *name) {
    if (ast->_file_name)
        ow_sharedstr_unref(ast->_file_name);
    ast->_file_name = ow_sharedstr_ref(name);
}

struct ow_sharedstr *ow_ast_get_filename(const struct ow_ast *ast) {
    return ast->_file_name;
}

void ow_ast_set_module(struct ow_ast *ast, struct ow_ast_Module *mod) {
    if (ast->_module)
        ow_ast_node_del((struct ow_ast_node *)ast->_module);
    ast->_module = mod;
}

struct ow_ast_Module *ow_ast_get_module(const struct ow_ast *ast) {
    return ast->_module;
}

#include "ast_node_dump.h"

void ow_ast_dump(const struct ow_ast *ast, struct ow_stream *out) {
    if (ast->_module)
        ow_ast_Module_dump(ast->_module, 0, out);
}

#define CHECK_NODE_STRUCT_LAYOUT(STRUCT) \
    (offsetof(STRUCT, location) == offsetof(struct ow_ast_node, location)) && \
    (offsetof(STRUCT, type) == offsetof(struct ow_ast_node, type)) &&         \
    (sizeof(STRUCT) >= sizeof(struct ow_ast_node))

#define ELEM(NAME) \
    static_assert(CHECK_NODE_STRUCT_LAYOUT(struct ow_ast_##NAME), \
    "bad layout: struct ow_ast_" #NAME);
OW_AST_NODE_LIST
#undef ELEM

#undef CHECK_NODE_STRUCT_LAYOUT

#pragma pack(push, 1)

static const char *const ow_ast_node_names[(size_t)_OW_AST_NODE_TYPE_COUNT] = {
#define ELEM(NAME) [ OW_AST_NODE_##NAME ] = #NAME ,
    OW_AST_NODE_LIST
#undef ELEM
};

#pragma pack(pop)

const char *ow_ast_node_name(const struct ow_ast_node *node) {
    const size_t type_index = (size_t)node->type;
    if (ow_unlikely(type_index >= (size_t)_OW_AST_NODE_TYPE_COUNT))
        return "";
    return ow_ast_node_names[type_index];
}

#include "ast_node_funcs.h"

#define ELEM(NAME) \
    struct ow_ast_##NAME *ow_ast_##NAME##_new(void) { \
        struct ow_ast_##NAME *const node = \
            ow_malloc(sizeof(struct ow_ast_##NAME)); \
        node->type = OW_AST_NODE_##NAME; \
        ow_ast_##NAME##_init(node); \
        return node; \
    }
OW_AST_NODE_LIST
#undef ELEM

void ow_ast_node_del(struct ow_ast_node *node) {
    switch (node->type) {
#define ELEM(NAME) case OW_AST_NODE_##NAME : \
    ow_ast_##NAME##_fini((struct ow_ast_##NAME *)node); break;
        OW_AST_NODE_LIST
#undef ELEM
    default:
        ow_unreachable();
    }

    ow_free(node);
}
