// Generated from "ast_node_def.ini". DO NOT edit.

static void ow_ast_NilLiteral_init(struct ow_ast_NilLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_NilLiteral_fini(struct ow_ast_NilLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_BoolLiteral_init(struct ow_ast_BoolLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_BoolLiteral_fini(struct ow_ast_BoolLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_IntLiteral_init(struct ow_ast_IntLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_IntLiteral_fini(struct ow_ast_IntLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_FloatLiteral_init(struct ow_ast_FloatLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_FloatLiteral_fini(struct ow_ast_FloatLiteral *node) {
    ow_unused_var(node);
}

static void ow_ast_StringLikeLiteral_init(struct ow_ast_StringLikeLiteral *node) {
    node->value = NULL;
}

static void ow_ast_StringLikeLiteral_fini(struct ow_ast_StringLikeLiteral *node) {
    if (ow_likely(node->value)) ow_sharedstr_unref(node->value);
}

static void ow_ast_SymbolLiteral_init(struct ow_ast_SymbolLiteral *node) {
    ow_ast_StringLikeLiteral_init((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_SymbolLiteral_fini(struct ow_ast_SymbolLiteral *node) {
    ow_ast_StringLikeLiteral_fini((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_StringLiteral_init(struct ow_ast_StringLiteral *node) {
    ow_ast_StringLikeLiteral_init((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_StringLiteral_fini(struct ow_ast_StringLiteral *node) {
    ow_ast_StringLikeLiteral_fini((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_Identifier_init(struct ow_ast_Identifier *node) {
    ow_ast_StringLikeLiteral_init((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_Identifier_fini(struct ow_ast_Identifier *node) {
    ow_ast_StringLikeLiteral_fini((struct ow_ast_StringLikeLiteral *)node);
}

static void ow_ast_BinOpExpr_init(struct ow_ast_BinOpExpr *node) {
    node->lhs = NULL, node->rhs = NULL;
}

static void ow_ast_BinOpExpr_fini(struct ow_ast_BinOpExpr *node) {
    if (ow_likely(node->lhs)) ow_ast_node_del((struct ow_ast_node *)node->lhs);
    if (ow_likely(node->rhs)) ow_ast_node_del((struct ow_ast_node *)node->rhs);
}

static void ow_ast_AddExpr_init(struct ow_ast_AddExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AddExpr_fini(struct ow_ast_AddExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_SubExpr_init(struct ow_ast_SubExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_SubExpr_fini(struct ow_ast_SubExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MulExpr_init(struct ow_ast_MulExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MulExpr_fini(struct ow_ast_MulExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_DivExpr_init(struct ow_ast_DivExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_DivExpr_fini(struct ow_ast_DivExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_RemExpr_init(struct ow_ast_RemExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_RemExpr_fini(struct ow_ast_RemExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShlExpr_init(struct ow_ast_ShlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShlExpr_fini(struct ow_ast_ShlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShrExpr_init(struct ow_ast_ShrExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShrExpr_fini(struct ow_ast_ShrExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitAndExpr_init(struct ow_ast_BitAndExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitAndExpr_fini(struct ow_ast_BitAndExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitOrExpr_init(struct ow_ast_BitOrExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitOrExpr_fini(struct ow_ast_BitOrExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitXorExpr_init(struct ow_ast_BitXorExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitXorExpr_fini(struct ow_ast_BitXorExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_EqlExpr_init(struct ow_ast_EqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_EqlExpr_fini(struct ow_ast_EqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AddEqlExpr_init(struct ow_ast_AddEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AddEqlExpr_fini(struct ow_ast_AddEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_SubEqlExpr_init(struct ow_ast_SubEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_SubEqlExpr_fini(struct ow_ast_SubEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MulEqlExpr_init(struct ow_ast_MulEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MulEqlExpr_fini(struct ow_ast_MulEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_DivEqlExpr_init(struct ow_ast_DivEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_DivEqlExpr_fini(struct ow_ast_DivEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_RemEqlExpr_init(struct ow_ast_RemEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_RemEqlExpr_fini(struct ow_ast_RemEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShlEqlExpr_init(struct ow_ast_ShlEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShlEqlExpr_fini(struct ow_ast_ShlEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShrEqlExpr_init(struct ow_ast_ShrEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_ShrEqlExpr_fini(struct ow_ast_ShrEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitAndEqlExpr_init(struct ow_ast_BitAndEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitAndEqlExpr_fini(struct ow_ast_BitAndEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitOrEqlExpr_init(struct ow_ast_BitOrEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitOrEqlExpr_fini(struct ow_ast_BitOrEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitXorEqlExpr_init(struct ow_ast_BitXorEqlExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_BitXorEqlExpr_fini(struct ow_ast_BitXorEqlExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_EqExpr_init(struct ow_ast_EqExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_EqExpr_fini(struct ow_ast_EqExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_NeExpr_init(struct ow_ast_NeExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_NeExpr_fini(struct ow_ast_NeExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_LtExpr_init(struct ow_ast_LtExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_LtExpr_fini(struct ow_ast_LtExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_LeExpr_init(struct ow_ast_LeExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_LeExpr_fini(struct ow_ast_LeExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_GtExpr_init(struct ow_ast_GtExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_GtExpr_fini(struct ow_ast_GtExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_GeExpr_init(struct ow_ast_GeExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_GeExpr_fini(struct ow_ast_GeExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AndExpr_init(struct ow_ast_AndExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AndExpr_fini(struct ow_ast_AndExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_OrExpr_init(struct ow_ast_OrExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_OrExpr_fini(struct ow_ast_OrExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AttrAccessExpr_init(struct ow_ast_AttrAccessExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_AttrAccessExpr_fini(struct ow_ast_AttrAccessExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MethodUseExpr_init(struct ow_ast_MethodUseExpr *node) {
    ow_ast_BinOpExpr_init((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_MethodUseExpr_fini(struct ow_ast_MethodUseExpr *node) {
    ow_ast_BinOpExpr_fini((struct ow_ast_BinOpExpr *)node);
}

static void ow_ast_UnOpExpr_init(struct ow_ast_UnOpExpr *node) {
    node->val = NULL;
}

static void ow_ast_UnOpExpr_fini(struct ow_ast_UnOpExpr *node) {
    if (ow_likely(node->val)) ow_ast_node_del((struct ow_ast_node *)node->val);
}

static void ow_ast_PosExpr_init(struct ow_ast_PosExpr *node) {
    ow_ast_UnOpExpr_init((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_PosExpr_fini(struct ow_ast_PosExpr *node) {
    ow_ast_UnOpExpr_fini((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_NegExpr_init(struct ow_ast_NegExpr *node) {
    ow_ast_UnOpExpr_init((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_NegExpr_fini(struct ow_ast_NegExpr *node) {
    ow_ast_UnOpExpr_fini((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_BitNotExpr_init(struct ow_ast_BitNotExpr *node) {
    ow_ast_UnOpExpr_init((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_BitNotExpr_fini(struct ow_ast_BitNotExpr *node) {
    ow_ast_UnOpExpr_fini((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_NotExpr_init(struct ow_ast_NotExpr *node) {
    ow_ast_UnOpExpr_init((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_NotExpr_fini(struct ow_ast_NotExpr *node) {
    ow_ast_UnOpExpr_fini((struct ow_ast_UnOpExpr *)node);
}

static void ow_ast_ArrayLikeExpr_init(struct ow_ast_ArrayLikeExpr *node) {
    ow_ast_node_array_init(&node->elems);
}

static void ow_ast_ArrayLikeExpr_fini(struct ow_ast_ArrayLikeExpr *node) {
    ow_ast_node_array_fini(&node->elems);
}

static void ow_ast_TupleExpr_init(struct ow_ast_TupleExpr *node) {
    ow_ast_ArrayLikeExpr_init((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_TupleExpr_fini(struct ow_ast_TupleExpr *node) {
    ow_ast_ArrayLikeExpr_fini((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_ArrayExpr_init(struct ow_ast_ArrayExpr *node) {
    ow_ast_ArrayLikeExpr_init((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_ArrayExpr_fini(struct ow_ast_ArrayExpr *node) {
    ow_ast_ArrayLikeExpr_fini((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_SetExpr_init(struct ow_ast_SetExpr *node) {
    ow_ast_ArrayLikeExpr_init((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_SetExpr_fini(struct ow_ast_SetExpr *node) {
    ow_ast_ArrayLikeExpr_fini((struct ow_ast_ArrayLikeExpr *)node);
}

static void ow_ast_MapExpr_init(struct ow_ast_MapExpr *node) {
    ow_ast_nodepair_array_init(&node->pairs);
}

static void ow_ast_MapExpr_fini(struct ow_ast_MapExpr *node) {
    ow_ast_nodepair_array_fini(&node->pairs);
}

static void ow_ast_CallLikeExpr_init(struct ow_ast_CallLikeExpr *node) {
    node->obj = NULL;
    ow_ast_node_array_init(&node->args);
}

static void ow_ast_CallLikeExpr_fini(struct ow_ast_CallLikeExpr *node) {
    if (ow_likely(node->obj)) ow_ast_node_del((struct ow_ast_node *)node->obj);
    ow_ast_node_array_fini(&node->args);
}

static void ow_ast_CallExpr_init(struct ow_ast_CallExpr *node) {
    ow_ast_CallLikeExpr_init((struct ow_ast_CallLikeExpr *)node);
}

static void ow_ast_CallExpr_fini(struct ow_ast_CallExpr *node) {
    ow_ast_CallLikeExpr_fini((struct ow_ast_CallLikeExpr *)node);
}

static void ow_ast_SubscriptExpr_init(struct ow_ast_SubscriptExpr *node) {
    ow_ast_CallLikeExpr_init((struct ow_ast_CallLikeExpr *)node);
}

static void ow_ast_SubscriptExpr_fini(struct ow_ast_SubscriptExpr *node) {
    ow_ast_CallLikeExpr_fini((struct ow_ast_CallLikeExpr *)node);
}

static void ow_ast_LambdaExpr_init(struct ow_ast_LambdaExpr *node) {
    node->func = NULL;
}

static void ow_ast_LambdaExpr_fini(struct ow_ast_LambdaExpr *node) {
    if (ow_likely(node->func)) ow_ast_node_del((struct ow_ast_node *)node->func);
}

static void ow_ast_ExprStmt_init(struct ow_ast_ExprStmt *node) {
    node->expr = NULL;
}

static void ow_ast_ExprStmt_fini(struct ow_ast_ExprStmt *node) {
    if (ow_likely(node->expr)) ow_ast_node_del((struct ow_ast_node *)node->expr);
}

static void ow_ast_BlockStmt_init(struct ow_ast_BlockStmt *node) {
    ow_ast_node_array_init(&node->stmts);
}

static void ow_ast_BlockStmt_fini(struct ow_ast_BlockStmt *node) {
    ow_ast_node_array_fini(&node->stmts);
}

static void ow_ast_ReturnStmt_init(struct ow_ast_ReturnStmt *node) {
    node->ret_val = NULL;
}

static void ow_ast_ReturnStmt_fini(struct ow_ast_ReturnStmt *node) {
    if (ow_likely(node->ret_val)) ow_ast_node_del((struct ow_ast_node *)node->ret_val);
}

static void ow_ast_MagicReturnStmt_init(struct ow_ast_MagicReturnStmt *node) {
    ow_ast_ReturnStmt_init((struct ow_ast_ReturnStmt *)node);
}

static void ow_ast_MagicReturnStmt_fini(struct ow_ast_MagicReturnStmt *node) {
    ow_ast_ReturnStmt_fini((struct ow_ast_ReturnStmt *)node);
}

static void ow_ast_ImportStmt_init(struct ow_ast_ImportStmt *node) {
    node->mod_name = NULL;
}

static void ow_ast_ImportStmt_fini(struct ow_ast_ImportStmt *node) {
    if (ow_likely(node->mod_name)) ow_ast_node_del((struct ow_ast_node *)node->mod_name);
}

static void ow_ast_IfElseStmt_init(struct ow_ast_IfElseStmt *node) {
    ow_ast_nodepair_array_init(&node->branches);
    node->else_branch = NULL;
}

static void ow_ast_IfElseStmt_fini(struct ow_ast_IfElseStmt *node) {
    ow_ast_nodepair_array_fini(&node->branches);
    if (node->else_branch) ow_ast_node_del((struct ow_ast_node *)node->else_branch);
}

static void ow_ast_ForStmt_init(struct ow_ast_ForStmt *node) {
    ow_ast_BlockStmt_init((struct ow_ast_BlockStmt *)node);
    node->var = NULL;
    node->iter = NULL;
}

static void ow_ast_ForStmt_fini(struct ow_ast_ForStmt *node) {
    if (ow_likely(node->var)) ow_ast_node_del((struct ow_ast_node *)node->var);
    if (ow_likely(node->iter)) ow_ast_node_del((struct ow_ast_node *)node->iter);
    ow_ast_BlockStmt_fini((struct ow_ast_BlockStmt *)node);
}

static void ow_ast_WhileStmt_init(struct ow_ast_WhileStmt *node) {
    ow_ast_BlockStmt_init((struct ow_ast_BlockStmt *)node);
    node->cond = NULL;
}

static void ow_ast_WhileStmt_fini(struct ow_ast_WhileStmt *node) {
    if (ow_likely(node->cond)) ow_ast_node_del((struct ow_ast_node *)node->cond);
    ow_ast_BlockStmt_fini((struct ow_ast_BlockStmt *)node);
}

static void ow_ast_FuncStmt_init(struct ow_ast_FuncStmt *node) {
    ow_ast_BlockStmt_init((struct ow_ast_BlockStmt *)node);
    node->name = NULL;
    node->args = NULL;
}

static void ow_ast_FuncStmt_fini(struct ow_ast_FuncStmt *node) {
    if (ow_likely(node->name)) ow_ast_node_del((struct ow_ast_node *)node->name);
    if (ow_likely(node->args)) ow_ast_node_del((struct ow_ast_node *)node->args);
    ow_ast_BlockStmt_fini((struct ow_ast_BlockStmt *)node);
}

static void ow_ast_Module_init(struct ow_ast_Module *node) {
    node->code = NULL;
}

static void ow_ast_Module_fini(struct ow_ast_Module *node) {
    if (ow_likely(node->code)) ow_ast_node_del((struct ow_ast_node *)node->code);
}
