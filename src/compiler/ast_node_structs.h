// Generated from "ast_node_def.ini". DO NOT edit.

struct ow_ast_NilLiteral {
    OW_AST_NODE_HEAD
};

struct ow_ast_BoolLiteral {
    OW_AST_NODE_HEAD
    _Bool value;
};

struct ow_ast_IntLiteral {
    OW_AST_NODE_HEAD
    int64_t value;
};

struct ow_ast_FloatLiteral {
    OW_AST_NODE_HEAD
    double value;
};

struct ow_ast_StringLikeLiteral {
    OW_AST_NODE_HEAD
    struct ow_sharedstr *value;
};

struct ow_ast_SymbolLiteral {
    OW_AST_NODE_HEAD
    struct ow_sharedstr *value;
};

struct ow_ast_StringLiteral {
    OW_AST_NODE_HEAD
    struct ow_sharedstr *value;
};

struct ow_ast_Identifier {
    OW_AST_NODE_HEAD
    struct ow_sharedstr *value;
};

struct ow_ast_BinOpExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_AddExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_SubExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_MulExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_DivExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_RemExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_ShlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_ShrExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitAndExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitOrExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitXorExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_EqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_AddEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_SubEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_MulEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_DivEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_RemEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_ShlEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_ShrEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitAndEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitOrEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_BitXorEqlExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_EqExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_NeExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_LtExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_LeExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_GtExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_GeExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_AndExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_OrExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_AttrAccessExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_MethodUseExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *lhs;
    struct ow_ast_Expr *rhs;
};

struct ow_ast_UnOpExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *val;
};

struct ow_ast_PosExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *val;
};

struct ow_ast_NegExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *val;
};

struct ow_ast_BitNotExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *val;
};

struct ow_ast_NotExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *val;
};

struct ow_ast_ArrayLikeExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array elems;
};

struct ow_ast_TupleExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array elems;
};

struct ow_ast_ArrayExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array elems;
};

struct ow_ast_SetExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array elems;
};

struct ow_ast_MapExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_nodepair_array pairs;
};

struct ow_ast_CallLikeExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *obj;
    struct ow_ast_node_array args;
};

struct ow_ast_CallExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *obj;
    struct ow_ast_node_array args;
};

struct ow_ast_SubscriptExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *obj;
    struct ow_ast_node_array args;
};

struct ow_ast_LambdaExpr {
    OW_AST_NODE_HEAD
    struct ow_ast_FuncStmt *func;
};

struct ow_ast_ExprStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *expr;
};

struct ow_ast_BlockStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array stmts;
};

struct ow_ast_ReturnStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *ret_val /*optional*/;
};

struct ow_ast_MagicReturnStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_Expr *ret_val /*optional*/;
};

struct ow_ast_ImportStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_Identifier *mod_name;
};

struct ow_ast_IfElseStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_nodepair_array branches /* {(Expr)cond;
    (BlockStmt)body} */;
    struct ow_ast_BlockStmt *else_branch;
};

struct ow_ast_ForStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array stmts;
    struct ow_ast_Identifier *var;
    struct ow_ast_Expr *iter;
};

struct ow_ast_WhileStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array stmts;
    struct ow_ast_Expr *cond;
};

struct ow_ast_FuncStmt {
    OW_AST_NODE_HEAD
    struct ow_ast_node_array stmts;
    struct ow_ast_Identifier *name;
    struct ow_ast_ArrayExpr *args /* (Identifier) */;
};

struct ow_ast_Module {
    OW_AST_NODE_HEAD
    struct ow_ast_BlockStmt *code;
};
