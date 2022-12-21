#include "parser.h"

#include <assert.h>
#include <setjmp.h>

#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "token.h"
#include <utilities/array.h>
#include <utilities/attributes.h>
#include <utilities/malloc.h>
#include <utilities/stream.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

#include <config/options.h>

#if OW_DEBUG_PARSER
#	include <stdio.h>
#endif // OW_DEBUG_PARSER

/// Max number of tokens in `struct token_queue`.
#define TOKEN_QUEUE_CAP 2

/// Token queue.
struct token_queue {
	struct ow_token   _tokens[TOKEN_QUEUE_CAP];
	size_t            _token_cnt;
	size_t            _ign_eol_cnt;
	struct ow_lexer  *_lexer;
	struct ow_parser *_parser;
};

/// Initialize token queue.
static void token_queue_init(struct token_queue *tq, struct ow_parser *parser) {
	for (size_t i = 0; i < TOKEN_QUEUE_CAP; i++)
		ow_token_init(&tq->_tokens[i]);
	tq->_token_cnt = 0;
	tq->_ign_eol_cnt = 0;
	tq->_lexer = ow_parser_lexer(parser);
	tq->_parser = parser;
}

/// Finalize token queue.
static void token_queue_fini(struct token_queue *tq) {
	for (size_t i = 0; i < TOKEN_QUEUE_CAP; i++)
		ow_token_fini(&tq->_tokens[i]);
}

/// Reset the queue.
static void token_queue_clear(struct token_queue *tq) {
	for (size_t i = 0; i < TOKEN_QUEUE_CAP; i++)
		ow_token_assign_simple(&tq->_tokens[i], OW_TOK_END);
	tq->_token_cnt = 0;
	tq->_ign_eol_cnt = 0;
}

ow_noinline ow_noreturn static void ow_parser_error_throw_lex_err(
	struct ow_parser *, const struct ow_syntax_error *);

/// View current token.
static struct ow_token *token_queue_peek(struct token_queue *tq) {
	assert(tq->_token_cnt >= 1);
	return &tq->_tokens[0];
}

/// View next token.
static struct ow_token *token_queue_peek2(struct token_queue *tq) {
	assert(tq->_token_cnt >= 1);
	if (tq->_token_cnt == 1) {
		bool ok;
	next_token:
		ok = ow_lexer_next(tq->_lexer, &tq->_tokens[1]);
		if (ow_unlikely(!ok)) {
			const struct ow_syntax_error *const err = ow_lexer_error(tq->_lexer);
			ow_parser_error_throw_lex_err(tq->_parser, err);
		}
		if (ow_unlikely(ow_token_type(&tq->_tokens[1]) == OW_TOK_END_LINE
				&& tq->_ign_eol_cnt)) {
			goto next_token;
		}
		tq->_token_cnt = 2;
	}
	return &tq->_tokens[1];
}

/// Move to next token.
static void token_queue_advance(struct token_queue *tq) {
	bool ok;
	static_assert(TOKEN_QUEUE_CAP == 2, "");
	if (ow_unlikely(tq->_token_cnt == 2)) {
		ow_token_move(&tq->_tokens[0], &tq->_tokens[1]);
		tq->_token_cnt = 1;
		goto check_eol;
	}
	assert(tq->_token_cnt == 1);
next_token:
	ok = ow_lexer_next(tq->_lexer, &tq->_tokens[0]);
	if (ow_unlikely(!ok)) {
		const struct ow_syntax_error *const err = ow_lexer_error(tq->_lexer);
		ow_parser_error_throw_lex_err(tq->_parser, err);
	}
check_eol:
	if (ow_unlikely(ow_token_type(&tq->_tokens[0]) == OW_TOK_END_LINE
			&& tq->_ign_eol_cnt)) {
		goto next_token;
	}
}

/// Sync with lexer. Call this function after `token_queue_clear()`.
static void token_queue_sync(struct token_queue *tq) {
	assert(tq->_token_cnt == 0);
	tq->_token_cnt = 1;
	token_queue_advance(tq);
}

/// Enter an ignoring-end-of-line region.
static void token_queue_push_iel(struct token_queue *tq) {
	assert(tq->_ign_eol_cnt < SIZE_MAX);
	assert(ow_token_type(token_queue_peek(tq)) != OW_TOK_END_LINE);
	tq->_ign_eol_cnt++;
}

/// Leave an ignoring-end-of-line region.
static void token_queue_pop_iel(struct token_queue *tq) {
	assert(tq->_ign_eol_cnt > 0);
	tq->_ign_eol_cnt--;
}

/// Check whether in an ignoring-end-of-line region.
ow_unused_func static bool token_queue_test_iel(struct token_queue *tq) {
	return tq->_ign_eol_cnt;
}

/// Set ignoring-end-of-line region level and return the old level.
ow_unused_func static size_t token_queue_set_iel(
		struct token_queue *tq, size_t new_level) {
	const size_t old_level = tq->_ign_eol_cnt;
	tq->_ign_eol_cnt = new_level;
	return old_level;
}

/// A stack of AST nodes.
struct ast_node_stack {
	struct ow_array _data;
};

/// Initialize the stack.
static void ast_node_stack_init(struct ast_node_stack *stack) {
	ow_array_init(&stack->_data, 8);
}

/// Delete all nodes.
static void ast_node_stack_clear(struct ast_node_stack *stack) {
	struct ow_ast_node **nodes =
		(struct ow_ast_node **)ow_array_data(&stack->_data);
	for (size_t i = 0, n = ow_array_size(&stack->_data); i < n; i++)
		ow_ast_node_del(nodes[i]);
	ow_array_clear(&stack->_data);
}

/// Finalize the stack.
static void ast_node_stack_fini(struct ast_node_stack *stack) {
	ast_node_stack_clear(stack);
	ow_array_fini(&stack->_data);
}

/// Get number of nodes.
static size_t ast_node_stack_size(struct ast_node_stack *stack) {
	return ow_array_size(&stack->_data);
}

/// View top node.
static struct ow_ast_node *ast_node_stack_top(struct ast_node_stack *stack) {
	return ow_array_at(&stack->_data, ow_array_size(&stack->_data) - 1);
}

/// Push a node.
static void ast_node_stack_push(
		struct ast_node_stack *stack, struct ow_ast_node *node) {
	ow_array_append(&stack->_data, node);
}

/// Release last node.
static struct ow_ast_node *ast_node_stack_release(struct ast_node_stack *stack) {
	ow_unused_var(stack);
	assert(ow_array_size(&stack->_data));
	struct ow_ast_node *const node = ast_node_stack_top(stack);
	ow_array_drop(&stack->_data);
	return node;
}

/// Like ast_node_stack_release(), but check whether it is the same one with param `node`.
static void ast_node_stack_release_s(
		struct ast_node_stack *stack, struct ow_ast_node *node) {
	struct ow_ast_node *const n = ast_node_stack_release(stack);
	assert(n == node);
	ow_unused_var(n);
	ow_unused_var(node);
}

/// Operator info, used in `struct expr_parser`. Trivial type.
struct _expr_parser_op_info {
	struct ow_source_range location;
	enum ow_token_type type;
	int8_t precedence;
};

/// Operator expression parser. It takes operators and operands, and output expr node.
struct expr_parser {
	struct ow_xarray _operator_stack;
	struct ast_node_stack _operand_stack;
	struct ow_parser *_parser;
	struct expr_parser *_next; // Next node of a linked list.
};

/// Create an expr parser.
static struct expr_parser *expr_parser_new(struct ow_parser *parser) {
	struct expr_parser *const ep = ow_malloc(sizeof (struct expr_parser));
	ow_xarray_init(&ep->_operator_stack, struct _expr_parser_op_info, 8);
	ast_node_stack_init(&ep->_operand_stack);
	ep->_parser = parser;
	ep->_next = NULL;
	return ep;
}

/// Reset the parser.
static void expr_parser_clear(struct expr_parser *ep) {
	ow_xarray_clear(&ep->_operator_stack);
	ast_node_stack_clear(&ep->_operand_stack);
}

/// Destroy an expr parser.
static void expr_parser_del(struct expr_parser *ep) {
	expr_parser_clear(ep);
	ow_xarray_fini(&ep->_operator_stack);
	ast_node_stack_fini(&ep->_operand_stack);
	ow_free(ep);
}

static void _expr_parser_gen_op_expr(struct expr_parser *ep);
/// Input an operator token into the expr parser.
static void expr_parser_input_operator(
	struct expr_parser *ep, struct ow_source_range location, enum ow_token_type type);
/// Input an operand token (an AST expr node) into the expr parser.
static void expr_parser_input_operand(struct expr_parser *ep, struct ow_ast_Expr *expr_node);
/// Input a "(" into the expr parser.
static void expr_parser_input_left_paren(struct expr_parser *ep, struct ow_source_range location);
/// Try to input a ")" into the expr parser. Return whether accepted.
ow_nodiscard static bool expr_parser_try_input_right_paren(struct expr_parser *ep);
/// Generated the result AST expr node.
static struct ow_ast_Expr *expr_parser_output_expression(struct expr_parser *ep);

/// A linked list of `struct expr_parser *`.
struct expr_parser_list {
	struct expr_parser *_first;
};

/// Initialize the list.
static void expr_parser_list_init(struct expr_parser_list *list) {
	list->_first = NULL;
}

/// Finalize the list.
static void expr_parser_list_fini(struct expr_parser_list *list) {
	for (struct expr_parser *ep = list->_first; ep;) {
		struct expr_parser *const next_ep = ep->_next;
		expr_parser_del(ep);
		ep = next_ep;
	}
	list->_first = NULL;
}

/// Add a parser.
static void expr_parser_list_add(
		struct expr_parser_list *list, struct expr_parser *ep) {
	assert(!ep->_next);
	ep->_next = list->_first;
	list->_first = ep;
}

/// Pop the last parser. If empty, return NULL.
static struct expr_parser *expr_parser_list_pop(struct expr_parser_list *list) {
	if (ow_unlikely(!list->_first))
		return NULL;
	struct expr_parser *const res = list->_first;
	list->_first = res->_next;
	res->_next = NULL;
	return res;
}

/// Move all parsers from `other_list` to `list`.
static void expr_parser_list_merge(
		struct expr_parser_list *list, struct expr_parser_list *other_list, bool clear) {
	struct expr_parser *const list_data = other_list->_first;
	if (!list_data)
		return;
	other_list->_first = NULL;
	struct expr_parser *list_data_last;
	for (struct expr_parser *ep = list_data; ;) {
		if (clear)
			expr_parser_clear(ep);
		struct expr_parser *next_ep = ep->_next;
		if (next_ep) {
			ep = next_ep;
		} else {
			list_data_last = ep;
			break;
		}
	}
	list_data_last->_next = list->_first;
	list->_first = list_data;
}

struct ow_parser {
	struct ow_lexer *lexer;
	struct token_queue token_queue;
	struct ast_node_stack temp_nodes;
	struct expr_parser_list free_expr_parsers, inuse_expr_parsers;
	jmp_buf error_jmpbuf;
	struct ow_syntax_error error_info;
#if OW_DEBUG_PARSER
	bool verbose;
#endif // OW_DEBUG_PARSER
};

/// A wrapper of `setjmp()`.
#define ow_parser_error_setjmp(parser) \
	(setjmp((parser)->error_jmpbuf))

/// Throw error.
ow_noinline ow_noreturn static void ow_parser_error_throw(
		struct ow_parser *parser, const struct ow_source_range *location,
		const char *msg_fmt, ...) {
	va_list ap;
	va_start(ap, msg_fmt);
	ow_syntax_error_vformat(&parser->error_info, location, msg_fmt, ap);
	va_end(ap);
	longjmp(parser->error_jmpbuf, 1);
}

/// Check next token
static void ow_parser_check_next(
		struct ow_parser *parser, enum ow_token_type expected) {
	struct token_queue *const tq = &parser->token_queue;
	if (ow_unlikely(ow_token_type(token_queue_peek(tq)) != expected)) {
		struct ow_token *const tok = token_queue_peek(tq);
		ow_parser_error_throw(
			parser, &tok->location, "expected `%s', got `%s'",
			ow_token_type_represent(expected),
			ow_token_type_represent(ow_token_type(tok)));
	}
}

/// Check next token and ignore it.
static void ow_parser_check_and_ignore(
		struct ow_parser *parser, enum ow_token_type expected) {
	struct token_queue *const tq = &parser->token_queue;
	if (ow_unlikely(ow_token_type(token_queue_peek(tq)) != expected)) {
		struct ow_token *const tok = token_queue_peek(tq);
		ow_parser_error_throw(
			parser, &tok->location, "expected `%s', got `%s'",
			ow_token_type_represent(expected),
			ow_token_type_represent(ow_token_type(tok)));
	}
	token_queue_advance(tq);
}

/// Throw lexer error.
ow_noinline ow_noreturn static void ow_parser_error_throw_lex_err(
		struct ow_parser *parser, const struct ow_syntax_error *err) {
	ow_syntax_error_copy(&parser->error_info, err);
	longjmp(parser->error_jmpbuf, 1);
}

/// Borrow an expr parser.
static struct expr_parser *ow_parser_expr_parser_borrow(struct ow_parser *parser) {
	struct expr_parser *ep =
		expr_parser_list_pop(&parser->free_expr_parsers);
	if (ow_unlikely(!ep))
		ep = expr_parser_new(parser);
	expr_parser_list_add(&parser->inuse_expr_parsers, ep);
	return ep;
}

/// Return a borrowed expr parser.
static void ow_parser_expr_parser_return(
		struct ow_parser *parser, struct expr_parser *ep) {
	struct expr_parser *const expected_ep =
		expr_parser_list_pop(&parser->inuse_expr_parsers);
	assert(expected_ep == ep);
	ow_unused_var(expected_ep);
	expr_parser_clear(ep);
	expr_parser_list_add(&parser->free_expr_parsers, ep);
}

/// Protect temporary node.
#define ow_parser_protect_node(parser, node) \
	ast_node_stack_push(&(parser)->temp_nodes, (struct ow_ast_node *)(node))

/// Un-protect the last added temporary node.
#define ow_parser_unprotect_node(parser, node) \
	ast_node_stack_release_s(&(parser)->temp_nodes, (struct ow_ast_node *)(node))

ow_nodiscard struct ow_parser *ow_parser_new(void) {
	struct ow_parser *const parser = ow_malloc(sizeof(struct ow_parser));
	parser->lexer = ow_lexer_new();
	token_queue_init(&parser->token_queue, parser);
	ast_node_stack_init(&parser->temp_nodes);
	expr_parser_list_init(&parser->free_expr_parsers);
	expr_parser_list_init(&parser->inuse_expr_parsers);
	ow_syntax_error_init(&parser->error_info);
#if OW_DEBUG_PARSER
	parser->verbose = false;
#endif // OW_DEBUG_PARSER
	return parser;
}

void ow_parser_del(struct ow_parser *parser) {
	ow_parser_clear(parser);
	ow_lexer_del(parser->lexer);
	token_queue_fini(&parser->token_queue);
	ast_node_stack_fini(&parser->temp_nodes);
	expr_parser_list_fini(&parser->free_expr_parsers);
	expr_parser_list_fini(&parser->inuse_expr_parsers);
	ow_syntax_error_fini(&parser->error_info);
	ow_free(parser);
}

struct ow_lexer *ow_parser_lexer(struct ow_parser *parser) {
	return parser->lexer;
}

void ow_parser_verbose(struct ow_parser *parser, bool status) {
	ow_unused_var(parser);
	ow_unused_var(status);
#if OW_DEBUG_PARSER
	parser->verbose = status;
#endif // OW_DEBUG_PARSER
}

void ow_parser_clear(struct ow_parser *parser) {
	ow_lexer_clear(parser->lexer);
	token_queue_clear(&parser->token_queue);
	ast_node_stack_clear(&parser->temp_nodes);
	expr_parser_list_merge(
		&parser->free_expr_parsers, &parser->inuse_expr_parsers, true);
	ow_syntax_error_clear(&parser->error_info);
}

/*
Principle for the following ow_parser_parse_xxx() functions:

- Assume that the first token from queue is the expected one.
- On error, throw an error using ow_parser_error_throw(). Never return NULL, except specified.
- Keep temp nodes with `ow_parser_protect_node()/ow_parser_unprotect_node()` to avoid memory leak.
*/

static struct ow_ast_Identifier *ow_parser_parse_identifier(struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_token *const tok = token_queue_peek(tq);
	assert(ow_token_type(tok) == OW_TOK_IDENTIFIER);
	struct ow_ast_Identifier *const id = ow_ast_Identifier_new();
	assert(!id->value);
	id->value = ow_sharedstr_ref(ow_token_value_string(tok));
	id->location = tok->location;
	token_queue_advance(tq);
	return id;
}

static void _expr_parser_gen_op_expr(struct expr_parser *ep) {
	assert(ow_xarray_size(&ep->_operator_stack));
	const enum ow_token_type type =
		ow_xarray_last(&ep->_operator_stack, struct _expr_parser_op_info).type;

	switch (type) {
		union {
			struct ow_ast_BinOpExpr *bin_op;
			struct ow_ast_UnOpExpr *un_op;
			struct ow_ast_CallExpr *call;
			struct ow_ast_SubscriptExpr *subscript;
			struct ow_ast_node *node;
		} op_expr;

	case OW_TOK_OP_ADD:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_AddExpr_new();
		goto bin_op;
	case OW_TOK_OP_SUB:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_SubExpr_new();
		goto bin_op;
	case OW_TOK_OP_MUL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_MulExpr_new();
		goto bin_op;
	case OW_TOK_OP_DIV:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_DivExpr_new();
		goto bin_op;
	case OW_TOK_OP_REM:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_RemExpr_new();
		goto bin_op;
	case OW_TOK_OP_SHL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_ShlExpr_new();
		goto bin_op;
	case OW_TOK_OP_SHR:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_ShrExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_AND:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitAndExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_OR:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitOrExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_XOR:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitXorExpr_new();
		goto bin_op;
	case OW_TOK_OP_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_EqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_ADD_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_AddEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_SUB_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_SubEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_MUL_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_MulEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_DIV_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_DivEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_REM_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_RemEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_SHL_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_ShlEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_SHR_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_ShrEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_AND_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitAndEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_OR_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitOrEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_BIT_XOR_EQL:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_BitXorEqlExpr_new();
		goto bin_op;
	case OW_TOK_OP_EQ:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_EqExpr_new();
		goto bin_op;
	case OW_TOK_OP_NE:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_NeExpr_new();
		goto bin_op;
	case OW_TOK_OP_LT:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_LtExpr_new();
		goto bin_op;
	case OW_TOK_OP_LE:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_LeExpr_new();
		goto bin_op;
	case OW_TOK_OP_GT:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_GtExpr_new();
		goto bin_op;
	case OW_TOK_OP_GE:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_GeExpr_new();
		goto bin_op;
	case OW_TOK_OP_AND:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_AndExpr_new();
		goto bin_op;
	case OW_TOK_OP_OR:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_OrExpr_new();
		goto bin_op;
	case OW_TOK_OP_PERIOD:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_AttrAccessExpr_new();
		goto bin_op;
	case OW_TOK_OP_COLON:
		op_expr.bin_op = (struct ow_ast_BinOpExpr *)ow_ast_MethodUseExpr_new();
		goto bin_op;
	bin_op:
		if (ow_unlikely(ast_node_stack_size(&ep->_operand_stack) < 2))
			goto no_enough_operands;
		assert(!op_expr.bin_op->lhs && !op_expr.bin_op->rhs);
		op_expr.bin_op->rhs =
			(struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
		op_expr.bin_op->lhs =
			(struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
		op_expr.bin_op->location.begin = op_expr.bin_op->lhs->location.begin;
		op_expr.bin_op->location.end = op_expr.bin_op->rhs->location.end;
		ast_node_stack_push(&ep->_operand_stack, op_expr.node);
		break;

	case OW_TOK_OP_POS:
		op_expr.un_op = (struct ow_ast_UnOpExpr *)ow_ast_PosExpr_new();
		goto un_op;
	case OW_TOK_OP_NEG:
		op_expr.un_op = (struct ow_ast_UnOpExpr *)ow_ast_NegExpr_new();
		goto un_op;
	case OW_TOK_OP_BIT_NOT:
		op_expr.un_op = (struct ow_ast_UnOpExpr *)ow_ast_BitNotExpr_new();
		goto un_op;
	case OW_TOK_OP_NOT:
		op_expr.un_op = (struct ow_ast_UnOpExpr *)ow_ast_NotExpr_new();
		goto un_op;
	un_op:
		if (ow_unlikely(ast_node_stack_size(&ep->_operand_stack) < 1))
			goto no_enough_operands;
		assert(!op_expr.un_op->val);
		op_expr.un_op->val =
				(struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
		op_expr.un_op->location = op_expr.un_op->val->location;
		ast_node_stack_push(&ep->_operand_stack, op_expr.node);
		break;

	case OW_TOK_OP_CALL:
		if (ow_unlikely(ast_node_stack_size(&ep->_operand_stack) < 2))
			goto no_enough_operands;
		assert(ast_node_stack_top(&ep->_operand_stack)->type == OW_AST_NODE_CallExpr);
		op_expr.call =
			(struct ow_ast_CallExpr *)ast_node_stack_release(&ep->_operand_stack);
		assert(!op_expr.call->obj);
		op_expr.call->obj =
			(struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
		op_expr.call->location.begin = op_expr.call->obj->location.begin;
		op_expr.call->location.end = ow_ast_node_array_size(&op_expr.call->args) ?
			ow_ast_node_array_last(&op_expr.call->args)->location.end :
			op_expr.call->obj->location.end;
		ast_node_stack_push(&ep->_operand_stack, op_expr.node);
		break;

	case OW_TOK_OP_SUBSCRIPT:
		if (ow_unlikely(ast_node_stack_size(&ep->_operand_stack) < 2))
			goto no_enough_operands;
		assert(ast_node_stack_top(&ep->_operand_stack)->type == OW_AST_NODE_SubscriptExpr);
		op_expr.subscript =
			(struct ow_ast_SubscriptExpr *)ast_node_stack_release(&ep->_operand_stack);
		assert(!op_expr.subscript->obj);
		op_expr.subscript->obj =
			(struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
		op_expr.subscript->location.begin = op_expr.subscript->obj->location.begin;
		op_expr.subscript->location.end = ow_ast_node_array_size(&op_expr.subscript->args) ?
			ow_ast_node_array_last(&op_expr.subscript->args)->location.end :
			op_expr.subscript->obj->location.end;
		ast_node_stack_push(&ep->_operand_stack, op_expr.node);
		break;

	no_enough_operands:
		ow_ast_node_del(op_expr.node);
		ow_parser_error_throw(
			ep->_parser,
			&ow_xarray_last(&ep->_operator_stack, struct _expr_parser_op_info).location,
			"too few operands for operator %s", ow_token_type_represent(type));

	default:
		ow_unreachable();
	}

	ow_xarray_drop(&ep->_operator_stack);
}

static void expr_parser_input_operator(struct expr_parser *ep,
		struct ow_source_range location, enum ow_token_type type) {
	int8_t precedence = ow_token_type_precedence(type);
	assert(precedence);
	if (ow_likely(precedence > 0)) {
		while (ow_xarray_size(&ep->_operator_stack) &&
				ow_xarray_last(&ep->_operator_stack, struct _expr_parser_op_info)
					.precedence <= precedence)
			_expr_parser_gen_op_expr(ep);
	} else {
		precedence = (int8_t)-precedence;
		while (ow_xarray_size(&ep->_operator_stack) &&
			ow_xarray_last(&ep->_operator_stack, struct _expr_parser_op_info)
				.precedence <= precedence)
			_expr_parser_gen_op_expr(ep);
	}

	const struct _expr_parser_op_info oi = {location, type, precedence};
	ow_xarray_append(&ep->_operator_stack, struct _expr_parser_op_info, oi);
}

static void expr_parser_input_operand(
		struct expr_parser *ep, struct ow_ast_Expr *expr_node) {
	ast_node_stack_push(&ep->_operand_stack, (struct ow_ast_node *)expr_node);
}

ow_unused_func static void expr_parser_input_left_paren(
		struct expr_parser *ep, struct ow_source_range location) {
	const struct _expr_parser_op_info oi = {location, OW_TOK_L_PAREN, 100};
	ow_xarray_append(&ep->_operator_stack, struct _expr_parser_op_info, oi);
}

ow_unused_func ow_nodiscard static bool expr_parser_try_input_right_paren(
		struct expr_parser *ep) {
	while (true) {
		if (ow_xarray_size(&ep->_operator_stack) == 0)
			return false;

		if (ow_xarray_last(&ep->_operator_stack, struct _expr_parser_op_info)
				.type == OW_TOK_L_PAREN) {
			ow_xarray_drop(&ep->_operator_stack);
			return true;
		}

		_expr_parser_gen_op_expr(ep);
	}
}

static struct ow_ast_Expr *expr_parser_output_expression(struct expr_parser *ep) {
	while (ow_xarray_size(&ep->_operator_stack))
		_expr_parser_gen_op_expr(ep);

	const size_t rest_operands = ast_node_stack_size(&ep->_operand_stack);
	if (ow_unlikely(rest_operands != 1)) {
		const char *err_msg;
		const struct ow_source_range *err_loc;
		if (rest_operands == 0) {
			err_msg = "empty expression";
			err_loc = &token_queue_peek(&ep->_parser->token_queue)->location;
		} else {
			err_msg = "too many operands";
			err_loc = &ast_node_stack_top(&ep->_operand_stack)->location;
		}
		ow_parser_error_throw(ep->_parser, err_loc, err_msg);
	}

	return (struct ow_ast_Expr *)ast_node_stack_release(&ep->_operand_stack);
}

static struct ow_ast_Expr *ow_parser_parse_expr(struct ow_parser *);

/// Parse call-expr argument list, i.e. `(a1, a2, ...)`.
static void ow_parser_parse_CallExpr_args(
		struct ow_parser *parser, struct ow_ast_CallExpr *call_expr) {
	struct token_queue *const tq = &parser->token_queue;

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_L_PAREN);
	token_queue_push_iel(tq);
	token_queue_advance(tq);

	while (1) {
		if (ow_unlikely(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_PAREN))
			break;

		struct ow_ast_Expr *const arg_expr = ow_parser_parse_expr(parser);
		ow_ast_node_array_append(&call_expr->args, (struct ow_ast_node *)arg_expr);

		const enum ow_token_type trailing_tok_tp = ow_token_type(token_queue_peek(tq));
		if (ow_likely(trailing_tok_tp == OW_TOK_COMMA)) {
			token_queue_advance(tq);
		} else if (trailing_tok_tp == OW_TOK_R_PAREN) {
			break;
		} else {
			struct ow_token *const tok = token_queue_peek(tq);
			ow_parser_error_throw(
				parser, &tok->location, "expected `,' or `%c', got `%s'",
				')', ow_token_type_represent(trailing_tok_tp));
		}
	}

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_PAREN);
	token_queue_pop_iel(tq);
	token_queue_advance(tq);
}

/// Parse subscript-expr index list, i.e. `[i1, i2, ...]`.
static void ow_parser_parse_SubscriptExpr_indices(
		struct ow_parser *parser, struct ow_ast_SubscriptExpr *subscript_expr) {
	struct token_queue *const tq = &parser->token_queue;

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_L_BRACKET);
	token_queue_push_iel(tq);
	token_queue_advance(tq);

	while (1) {
		if (ow_unlikely(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_BRACKET))
			break;

		struct ow_ast_Expr *const arg_expr = ow_parser_parse_expr(parser);
		ow_ast_node_array_append(&subscript_expr->args, (struct ow_ast_node *)arg_expr);

		const enum ow_token_type trailing_tok_tp = ow_token_type(token_queue_peek(tq));
		if (ow_likely(trailing_tok_tp == OW_TOK_COMMA)) {
			token_queue_advance(tq);
		} else if (trailing_tok_tp == OW_TOK_R_BRACKET) {
			break;
		} else {
			struct ow_token *const tok = token_queue_peek(tq);
			ow_parser_error_throw(
				parser, &tok->location, "expected `,' or `%c', got `%s'",
				']', ow_token_type_represent(trailing_tok_tp));
		}
	}

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_BRACKET);
	token_queue_pop_iel(tq);
	token_queue_advance(tq);
}

/// Parse elements for an array-like expr and fill `ow_ast_ArrayLikeExpr`.
/// The end-token will be left in the token queue.
static void ow_parser_parse_ArrayLikeExpr_elems(
		struct ow_parser *parser, struct ow_ast_ArrayLikeExpr *expr,
		enum ow_token_type end_tok) {
	struct token_queue *const tq = &parser->token_queue;
	assert(token_queue_test_iel(tq));

	while (1) {
		if (ow_unlikely(ow_token_type(token_queue_peek(tq)) == end_tok))
			break;

		struct ow_ast_Expr *const elem_expr = ow_parser_parse_expr(parser);
		ow_ast_node_array_append(&expr->elems, (struct ow_ast_node *)elem_expr);

		const enum ow_token_type trailing_tok_tp = ow_token_type(token_queue_peek(tq));
		if (ow_likely(trailing_tok_tp == OW_TOK_COMMA)) {
			token_queue_advance(tq);
		} else if (trailing_tok_tp == end_tok) {
			break;
		} else {
			struct ow_token *const tok = token_queue_peek(tq);
			ow_parser_error_throw(
				parser, &tok->location, "expected `,' or `%s', got `%s'",
				ow_token_type_represent(end_tok),
				ow_token_type_represent(trailing_tok_tp));
		}
	}

	assert(ow_token_type(token_queue_peek(tq)) == end_tok);
	expr->location.end = token_queue_peek(tq)->location.end;
}

/// Parse a tuple expr or an expr starts with `(`. The next token must be `(`.
static struct ow_ast_Expr *ow_parser_parse_TupleExpr_or_expr(struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_Expr *expr;
	enum ow_token_type tok_tp;

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_L_PAREN);
	const struct ow_source_location loc_begin = token_queue_peek(tq)->location.begin;
	token_queue_push_iel(tq);
	token_queue_advance(tq);

	if (ow_unlikely(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_PAREN)) {
		struct ow_ast_TupleExpr *const tuple_expr = ow_ast_TupleExpr_new();
		tuple_expr->location.begin = loc_begin;
		tuple_expr->location.end = token_queue_peek(tq)->location.end;
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return (struct ow_ast_Expr *)tuple_expr;
	}

	expr = ow_parser_parse_expr(parser);
	tok_tp = ow_token_type(token_queue_peek(tq));
	if (ow_unlikely(tok_tp == OW_TOK_R_PAREN)) {
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return expr;
	}

	struct ow_ast_TupleExpr *const tuple_expr = ow_ast_TupleExpr_new();
	ow_parser_protect_node(parser, tuple_expr);
	tuple_expr->location.begin = loc_begin;
	if (ow_likely(tok_tp == OW_TOK_COMMA)) {
		ow_ast_node_array_append(&tuple_expr->elems, (struct ow_ast_node *)expr);
		token_queue_advance(tq);
	} else {
		ow_parser_error_throw(
			parser, &token_queue_peek(tq)->location, "expected `,' or `%c', got `%s'",
			')', ow_token_type_represent(tok_tp));
	}

	ow_parser_parse_ArrayLikeExpr_elems(
		parser, (struct ow_ast_ArrayLikeExpr *)tuple_expr, OW_TOK_R_PAREN);
	token_queue_pop_iel(tq);
	token_queue_advance(tq);
	ow_parser_unprotect_node(parser, tuple_expr);
	return (struct ow_ast_Expr *)tuple_expr;
}

/// Parse an array expr. Then next token must be `[`.
static struct ow_ast_Expr *ow_parser_parse_ArrayExpr(struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_ArrayExpr *const array_expr = ow_ast_ArrayExpr_new();
	ow_parser_protect_node(parser, array_expr);
	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_L_BRACKET);
	array_expr->location.begin = token_queue_peek(tq)->location.begin;
	token_queue_push_iel(tq);
	token_queue_advance(tq);
	ow_parser_parse_ArrayLikeExpr_elems(
		parser, (struct ow_ast_ArrayLikeExpr *)array_expr, OW_TOK_R_BRACKET);
	token_queue_pop_iel(tq);
	token_queue_advance(tq);
	ow_parser_unprotect_node(parser, array_expr);
	return (struct ow_ast_Expr *)array_expr;
}

/// Parse a set expr or a map expr. The next token must `{`.
static struct ow_ast_Expr *ow_parser_parse_SetExpr_or_MapExpr(struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_Expr *expr;
	enum ow_token_type tok_tp;

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_L_BRACE);
	const struct ow_source_location loc_begin = token_queue_peek(tq)->location.begin;
	token_queue_push_iel(tq);
	token_queue_advance(tq);

	tok_tp = ow_token_type(token_queue_peek(tq));
	if (ow_unlikely(tok_tp == OW_TOK_R_BRACE)) {
		struct ow_ast_MapExpr *const map_expr = ow_ast_MapExpr_new();
		map_expr->location.begin = loc_begin;
		map_expr->location.end = token_queue_peek(tq)->location.end;
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return (struct ow_ast_Expr *)map_expr;
	} else if (ow_unlikely(tok_tp == OW_TOK_COMMA)) {
		token_queue_advance(tq);
		if (ow_unlikely(ow_token_type(token_queue_peek(tq)) != OW_TOK_R_BRACE)) {
			ow_parser_error_throw(
				parser, &token_queue_peek(tq)->location, "expected `%c', got `%s'",
				'}', ow_token_type_represent(ow_token_type(token_queue_peek(tq))));
		}
		struct ow_ast_SetExpr *const set_expr = ow_ast_SetExpr_new();
		set_expr->location.begin = loc_begin;
		set_expr->location.end = token_queue_peek(tq)->location.end;
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return (struct ow_ast_Expr *)set_expr;
	}

	expr = ow_parser_parse_expr(parser);
	tok_tp = ow_token_type(token_queue_peek(tq));
	if (ow_unlikely(tok_tp == OW_TOK_R_BRACE)) {
		struct ow_ast_SetExpr *const set_expr = ow_ast_SetExpr_new();
		ow_ast_node_array_append(&set_expr->elems, (struct ow_ast_node *)expr);
		set_expr->location.begin = loc_begin;
		set_expr->location.end = token_queue_peek(tq)->location.end;
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return (struct ow_ast_Expr *)set_expr;
	} else if (ow_unlikely(tok_tp == OW_TOK_COMMA)) {
		struct ow_ast_SetExpr *const set_expr = ow_ast_SetExpr_new();
		ow_ast_node_array_append(&set_expr->elems, (struct ow_ast_node *)expr);
		set_expr->location.begin = loc_begin;
		token_queue_advance(tq);
		ow_parser_parse_ArrayLikeExpr_elems(
			parser, (struct ow_ast_ArrayLikeExpr *)set_expr, OW_TOK_R_BRACE);
		token_queue_pop_iel(tq);
		token_queue_advance(tq);
		return (struct ow_ast_Expr *)set_expr;
	}

	struct ow_ast_MapExpr *const map_expr = ow_ast_MapExpr_new();
	ow_parser_protect_node(parser, map_expr);
	map_expr->location.begin = loc_begin;

	struct ow_ast_nodepair_array_elem pair;
	pair.first = (struct ow_ast_node *)expr;
	ow_parser_protect_node(parser, pair.first);
	goto enter_loop;

	while (1) {
		if (ow_unlikely(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_BRACE))
			break;

		pair.first = (struct ow_ast_node *)ow_parser_parse_expr(parser);
		ow_parser_protect_node(parser, pair.first);

	enter_loop:
		tok_tp = ow_token_type(token_queue_peek(tq));
		if (ow_likely(tok_tp == OW_TOK_FATARROW)) {
			token_queue_advance(tq);
		} else {
			ow_parser_error_throw(
				parser, &token_queue_peek(tq)->location, "expected `%s', got `%s'",
				ow_token_type_represent(OW_TOK_FATARROW),
				ow_token_type_represent(tok_tp));
		}

		pair.second = (struct ow_ast_node *)ow_parser_parse_expr(parser);
		ow_parser_unprotect_node(parser, pair.first);
		ow_ast_nodepair_array_append(&map_expr->pairs, pair);

		tok_tp = ow_token_type(token_queue_peek(tq));
		if (ow_likely(tok_tp == OW_TOK_COMMA)) {
			token_queue_advance(tq);
		} else if (tok_tp == OW_TOK_R_BRACE) {
			break;
		} else {
			ow_parser_error_throw(
				parser, &token_queue_peek(tq)->location, "expected `,' or `%c', got `%s'",
				'}', ow_token_type_represent(tok_tp));
		}
	}

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_R_BRACE);
	map_expr->location.end = token_queue_peek(tq)->location.end;
	token_queue_pop_iel(tq);
	token_queue_advance(tq);

	ow_parser_unprotect_node(parser, map_expr);
	return (struct ow_ast_Expr *)map_expr;
}

static void _parse_func_def_args(struct ow_parser *, struct ow_ast_FuncStmt *);
static void ow_parser_parse_block_to(struct ow_parser *, struct ow_ast_BlockStmt *);
static struct ow_ast_ExprStmt *ow_parser_make_expr_stmt(struct ow_ast_Expr *);

/// Parse an lambda function expression. Similar to `ow_parser_parse_func_stmt()`.
static struct ow_ast_LambdaExpr *ow_parser_parse_lambda_expr(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_LambdaExpr *const lambda_expr = ow_ast_LambdaExpr_new();
	ow_parser_protect_node(parser, lambda_expr);
	assert(!lambda_expr->func);
	lambda_expr->func = ow_ast_FuncStmt_new();
	struct ow_ast_FuncStmt *const func_stmt = lambda_expr->func;

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_FUNC);
	lambda_expr->location.begin = token_queue_peek(tq)->location.begin;
	token_queue_advance(tq);

	assert(!func_stmt->name); // No name.
	_parse_func_def_args(parser, func_stmt);

	if (ow_token_type(token_queue_peek(tq)) == OW_TOK_FATARROW) {
		token_queue_advance(tq);
		struct ow_ast_Expr *const expr = ow_parser_parse_expr(parser);
		struct ow_ast_ReturnStmt *const ret_stmt = ow_ast_ReturnStmt_new();
		ret_stmt->ret_val = expr;
		ret_stmt->location = expr->location;
		func_stmt->location.end = expr->location.end;
		ow_ast_node_array_append(&func_stmt->stmts, (struct ow_ast_node *)ret_stmt);
	} else {
		const size_t iel_lv = token_queue_set_iel(tq, 0);
		ow_parser_parse_block_to(parser, (struct ow_ast_BlockStmt *)func_stmt);
		const size_t iel_lv_1 = token_queue_set_iel(tq, iel_lv);
		ow_unused_var(iel_lv_1);
		assert(iel_lv_1 == 0);
		func_stmt->location.end = token_queue_peek(tq)->location.end;
		ow_parser_check_and_ignore(parser, OW_TOK_KW_END);
	}

	ow_parser_unprotect_node(parser, lambda_expr);
	return lambda_expr;
}

/// Parser an expression.
static struct ow_ast_Expr *ow_parser_parse_expr(struct ow_parser *parser) {
	struct expr_parser *const ep = ow_parser_expr_parser_borrow(parser);
	struct token_queue *const tq = &parser->token_queue;
	bool last_tok_is_operand = false;

	while (true) {
		struct ow_token *const tok = token_queue_peek(tq);
		enum ow_token_type tok_tp = ow_token_type(tok);
		if (ow_token_type_has_value(tok_tp)) {
		tok_is_value:;
			union {
				struct ow_ast_BoolLiteral *bool_literal;
				struct ow_ast_IntLiteral *int_literal;
				struct ow_ast_FloatLiteral *float_literal;
				struct ow_ast_SymbolLiteral *symbol_literal;
				struct ow_ast_StringLiteral *string_literal;
				struct ow_ast_Identifier *identifier;
				struct ow_ast_Expr *expr;
			} operand;
			switch (tok_tp) {
			case OW_TOK_KW_NIL:
				operand.expr = (struct ow_ast_Expr *)ow_ast_NilLiteral_new();
				break;
			case OW_TOK_KW_TRUE:
				operand.bool_literal = ow_ast_BoolLiteral_new();
				operand.bool_literal->value = true;
				break;
			case OW_TOK_KW_FALSE:
				operand.bool_literal = ow_ast_BoolLiteral_new();
				operand.bool_literal->value = false;
				break;
			case OW_TOK_INT:
				operand.int_literal = ow_ast_IntLiteral_new();
				operand.int_literal->value = ow_token_value_int(tok);
				break;
			case OW_TOK_FLOAT:
				operand.float_literal = ow_ast_FloatLiteral_new();
				operand.float_literal->value = ow_token_value_float(tok);
				break;
			case OW_TOK_SYMBOL:
				operand.symbol_literal = ow_ast_SymbolLiteral_new();
				operand.symbol_literal->value =
					ow_sharedstr_ref(ow_token_value_string(tok));
				break;
			case OW_TOK_STRING:
				operand.string_literal = ow_ast_StringLiteral_new();
				operand.string_literal->value =
					ow_sharedstr_ref(ow_token_value_string(tok));
				break;
			case OW_TOK_IDENTIFIER:
				operand.identifier = ow_ast_Identifier_new();
				operand.identifier->value =
					ow_sharedstr_ref(ow_token_value_string(tok));
				break;
			default:
				ow_unreachable();
			}
			operand.expr->location = tok->location;
			expr_parser_input_operand(ep, operand.expr);
			token_queue_advance(tq);
			last_tok_is_operand = true;
		} else if (ow_token_type_is_operator(tok_tp)) {
			// Scanner cannot distinguish between ADD and POS, SUB and NEG.
			if (ow_unlikely(tok_tp == OW_TOK_OP_ADD)) {
				if (ow_unlikely(!last_tok_is_operand))
					tok_tp = OW_TOK_OP_POS;
			} else if (ow_unlikely(tok_tp == OW_TOK_OP_SUB)) {
				if (ow_unlikely(!last_tok_is_operand))
					tok_tp = OW_TOK_OP_NEG;
			}
			expr_parser_input_operator(ep, tok->location, tok_tp);
			token_queue_advance(tq);
			last_tok_is_operand = false;
		} else if (tok_tp == OW_TOK_L_PAREN) {
			if (last_tok_is_operand) {
				expr_parser_input_operator(ep, tok->location, OW_TOK_OP_CALL);
				struct ow_ast_CallExpr *const call_expr = ow_ast_CallExpr_new();
				expr_parser_input_operand(ep, (struct ow_ast_Expr *)call_expr);
				ow_parser_parse_CallExpr_args(parser, call_expr);
				last_tok_is_operand = true;
			} else {
				struct ow_ast_Expr *const expr =
					ow_parser_parse_TupleExpr_or_expr(parser);
				expr_parser_input_operand(ep, expr);
				last_tok_is_operand = true;
			}
		} else if (tok_tp == OW_TOK_L_BRACKET) {
			if (last_tok_is_operand) {
				expr_parser_input_operator(ep, tok->location, OW_TOK_OP_SUBSCRIPT);
				struct ow_ast_SubscriptExpr *const subscript_expr =
					ow_ast_SubscriptExpr_new();
				expr_parser_input_operand(ep, (struct ow_ast_Expr *)subscript_expr);
				ow_parser_parse_SubscriptExpr_indices(parser, subscript_expr);
				last_tok_is_operand = true;
			} else {
				struct ow_ast_Expr *const expr = ow_parser_parse_ArrayExpr(parser);
				expr_parser_input_operand(ep, expr);
				last_tok_is_operand = true;
			}
		} else if (tok_tp == OW_TOK_L_BRACE) {
			if (last_tok_is_operand) {
				ow_parser_error_throw(
					parser, &tok->location,
					"unexpected `%s'",
					ow_token_type_represent(ow_token_type(tok))
				);
			} else {
				struct ow_ast_Expr *const expr =
					ow_parser_parse_SetExpr_or_MapExpr(parser);
				expr_parser_input_operand(ep, expr);
				last_tok_is_operand = true;
			}
		} else if (OW_TOK_KW_NIL <= tok_tp && tok_tp <= OW_TOK_KW_FALSE) {
			goto tok_is_value;
		} else if (tok_tp == OW_TOK_KW_FUNC) {
			expr_parser_input_operand(
				ep, (struct ow_ast_Expr *)ow_parser_parse_lambda_expr(parser));
			last_tok_is_operand = true;
		} else {
			break;
		}
	}

	struct ow_ast_Expr *const res = expr_parser_output_expression(ep);
	ow_parser_expr_parser_return(parser, ep);
	return res;
}

/// Convert expr to expr-stmt.
static struct ow_ast_ExprStmt *ow_parser_make_expr_stmt(struct ow_ast_Expr *expr) {
	struct ow_ast_ExprStmt *const expr_stmt = ow_ast_ExprStmt_new();
	assert(!expr_stmt->expr);
	expr_stmt->expr = expr;
	expr_stmt->location = expr_stmt->expr->location;
	return expr_stmt;
}

/// Parse return statement.
static struct ow_ast_ReturnStmt *ow_parser_parse_return_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_ReturnStmt *const ret_stmt = ow_ast_ReturnStmt_new();
	ow_parser_protect_node(parser, ret_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_RETURN);
	ret_stmt->location = token_queue_peek(tq)->location;
	token_queue_advance(tq);

	assert(!ret_stmt->ret_val);
	if (ow_token_type(token_queue_peek(tq)) != OW_TOK_END_LINE) {
		ret_stmt->ret_val = ow_parser_parse_expr(parser);
		ret_stmt->location.end = ret_stmt->ret_val->location.end;
	}

	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_unprotect_node(parser, ret_stmt);
	return ret_stmt;
}

/// Parse import statement.
static struct ow_ast_ImportStmt *ow_parser_parse_import_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_ImportStmt *const import_stmt = ow_ast_ImportStmt_new();
	ow_parser_protect_node(parser, import_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_IMPORT);
	import_stmt->location = token_queue_peek(tq)->location;
	token_queue_advance(tq);

	assert(!import_stmt->mod_name);
	ow_parser_check_next(parser, OW_TOK_IDENTIFIER);
	import_stmt->mod_name = ow_parser_parse_identifier(parser);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_unprotect_node(parser, import_stmt);
	return import_stmt;
}

static void ow_parser_parse_block_to(struct ow_parser *, struct ow_ast_BlockStmt *);
static struct ow_ast_BlockStmt *ow_parser_parse_block_stmt(struct ow_parser *);

/// Parse if-else statement.
static struct ow_ast_IfElseStmt *ow_parser_parse_if_else_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_IfElseStmt *const if_else_stmt = ow_ast_IfElseStmt_new();
	ow_parser_protect_node(parser, if_else_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_IF);
	if_else_stmt->location.begin = token_queue_peek(tq)->location.begin;
	goto enter_loop;

	while (1) {
		struct ow_ast_nodepair_array_elem branch;
		const enum ow_token_type tok_tp = ow_token_type(token_queue_peek(tq));

		if (tok_tp == OW_TOK_KW_ELIF) {
		enter_loop:
			token_queue_advance(tq);
			branch.first = (struct ow_ast_node *)ow_parser_parse_expr(parser);
			ow_parser_protect_node(parser, branch.first);
			ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);
			branch.second = (struct ow_ast_node *)ow_parser_parse_block_stmt(parser);
			ow_parser_unprotect_node(parser, branch.first);
			ow_ast_nodepair_array_append(&if_else_stmt->branches, branch);
		} else if (tok_tp == OW_TOK_KW_ELSE) {
			token_queue_advance(tq);
			ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);
			assert(!if_else_stmt->else_branch);
			if_else_stmt->else_branch = ow_parser_parse_block_stmt(parser);
			ow_parser_check_next(parser, OW_TOK_KW_END);
			goto end_stmt;
		} else if (tok_tp == OW_TOK_KW_END) {
		end_stmt:
			if_else_stmt->location.end = token_queue_peek(tq)->location.end;
			token_queue_advance(tq);
			ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);
			break;
		} else {
			struct ow_token *const tok = token_queue_peek(tq);
			ow_parser_error_throw(
				parser, &tok->location,
				"unexpected `%s'",
				ow_token_type_represent(ow_token_type(tok))
			);
		}
	}

	ow_parser_unprotect_node(parser, if_else_stmt);
	return if_else_stmt;
}

/// Parse for statement.
static struct ow_ast_ForStmt *ow_parser_parse_for_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_ForStmt *const for_stmt = ow_ast_ForStmt_new();
	ow_parser_protect_node(parser, for_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_FOR);
	for_stmt->location.begin = token_queue_peek(tq)->location.begin;
	token_queue_advance(tq);

	assert(!for_stmt->var);
	ow_parser_check_next(parser, OW_TOK_IDENTIFIER);
	for_stmt->var = ow_parser_parse_identifier(parser);
	ow_parser_check_and_ignore(parser, OW_TOK_L_ARROW);

	assert(!for_stmt->iter);
	for_stmt->iter = ow_parser_parse_expr(parser);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_parse_block_to(parser, (struct ow_ast_BlockStmt *)for_stmt);
	for_stmt->location.end = token_queue_peek(tq)->location.end;
	ow_parser_check_and_ignore(parser, OW_TOK_KW_END);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_unprotect_node(parser, for_stmt);
	return for_stmt;
}

/// Parse while statement.
static struct ow_ast_WhileStmt *ow_parser_parse_while_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_WhileStmt *const while_stmt = ow_ast_WhileStmt_new();
	ow_parser_protect_node(parser, while_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_WHILE);
	while_stmt->location.begin = token_queue_peek(tq)->location.begin;
	token_queue_advance(tq);

	assert(!while_stmt->cond);
	while_stmt->cond = ow_parser_parse_expr(parser);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_parse_block_to(parser, (struct ow_ast_BlockStmt *)while_stmt);
	while_stmt->location.end = token_queue_peek(tq)->location.end;
	ow_parser_check_and_ignore(parser, OW_TOK_KW_END);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_unprotect_node(parser, while_stmt);
	return while_stmt;
}

static void _parse_func_def_args(
		struct ow_parser *parser, struct ow_ast_FuncStmt *func_stmt) {
	struct token_queue *const tq = &parser->token_queue;
	token_queue_push_iel(tq);
	ow_parser_check_and_ignore(parser, OW_TOK_L_PAREN);
	assert(!func_stmt->args);
	func_stmt->args = ow_ast_ArrayExpr_new();
	ow_parser_parse_ArrayLikeExpr_elems(
		parser, (struct ow_ast_ArrayLikeExpr *)func_stmt->args, OW_TOK_R_PAREN);
	token_queue_pop_iel(tq);
	token_queue_advance(tq);
	for (size_t i = 0, n = ow_ast_node_array_size(&func_stmt->args->elems); i < n; i++) {
		struct ow_ast_node *const param = ow_ast_node_array_at(&func_stmt->args->elems, i);
		if (ow_unlikely(param->type != OW_AST_NODE_Identifier)) {
			ow_parser_error_throw(
				parser, &param->location,
				"param-%zu is not a legal formal parameter", i + 1);
		}
	}
}

/// Parse func statement.
static struct ow_ast_FuncStmt *ow_parser_parse_func_stmt(
		struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_ast_FuncStmt *const func_stmt = ow_ast_FuncStmt_new();
	ow_parser_protect_node(parser, func_stmt);

	assert(ow_token_type(token_queue_peek(tq)) == OW_TOK_KW_FUNC);
	func_stmt->location.begin = token_queue_peek(tq)->location.begin;
	token_queue_advance(tq);

	assert(!func_stmt->name);
	ow_parser_check_next(parser, OW_TOK_IDENTIFIER);
	func_stmt->name = ow_parser_parse_identifier(parser);

	_parse_func_def_args(parser, func_stmt);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_parse_block_to(parser, (struct ow_ast_BlockStmt *)func_stmt);
	func_stmt->location.end = token_queue_peek(tq)->location.end;
	ow_parser_check_and_ignore(parser, OW_TOK_KW_END);
	ow_parser_check_and_ignore(parser, OW_TOK_END_LINE);

	ow_parser_unprotect_node(parser, func_stmt);
	return func_stmt;
}

/// Parse next statement. If reaches unexpected token, return NULL.
static struct ow_ast_Stmt *ow_parser_parse_stmt(struct ow_parser *parser) {
	struct token_queue *const tq = &parser->token_queue;
	struct ow_token *const tok = token_queue_peek(tq);

	switch (ow_token_type(tok)) {
	case OW_TOK_OP_ADD:
	case OW_TOK_OP_SUB:
	case OW_TOK_OP_MUL:
	case OW_TOK_OP_DIV:
	case OW_TOK_OP_REM:
	case OW_TOK_OP_SHL:
	case OW_TOK_OP_SHR:
	case OW_TOK_OP_BIT_AND:
	case OW_TOK_OP_BIT_OR:
	case OW_TOK_OP_BIT_XOR:
	case OW_TOK_OP_EQL:
	case OW_TOK_OP_ADD_EQL:
	case OW_TOK_OP_SUB_EQL:
	case OW_TOK_OP_MUL_EQL:
	case OW_TOK_OP_DIV_EQL:
	case OW_TOK_OP_REM_EQL:
	case OW_TOK_OP_SHL_EQL:
	case OW_TOK_OP_SHR_EQL:
	case OW_TOK_OP_BIT_AND_EQL:
	case OW_TOK_OP_BIT_OR_EQL:
	case OW_TOK_OP_BIT_XOR_EQL:
	case OW_TOK_OP_EQ:
	case OW_TOK_OP_NE:
	case OW_TOK_OP_LT:
	case OW_TOK_OP_LE:
	case OW_TOK_OP_GT:
	case OW_TOK_OP_GE:
	case OW_TOK_OP_AND:
	case OW_TOK_OP_OR:
	case OW_TOK_OP_PERIOD:
	case OW_TOK_OP_COLON:
	case OW_TOK_OP_CALL:
	case OW_TOK_OP_SUBSCRIPT:
	case OW_TOK_OP_POS:
	case OW_TOK_OP_NEG:
	case OW_TOK_OP_BIT_NOT:
	case OW_TOK_OP_NOT:
	case OW_TOK_KW_NIL:
	case OW_TOK_KW_TRUE:
	case OW_TOK_KW_FALSE:
	case OW_TOK_INT:
	case OW_TOK_FLOAT:
	case OW_TOK_SYMBOL:
	case OW_TOK_STRING:
	case OW_TOK_IDENTIFIER:
	case OW_TOK_L_PAREN:
	case OW_TOK_L_BRACKET:
	case OW_TOK_L_BRACE:
		return (struct ow_ast_Stmt *)
			ow_parser_make_expr_stmt(ow_parser_parse_expr(parser));

	case OW_TOK_COMMA:
	case OW_TOK_HASHTAG:
	case OW_TOK_AT:
	case OW_TOK_QUESTION:
	case OW_TOK_DOLLAR:
	case OW_TOK_DOTDOT:
	case OW_TOK_ELLIPSIS:
	case OW_TOK_L_ARROW:
	case OW_TOK_FATARROW:
	case OW_TOK_R_PAREN:
	case OW_TOK_R_BRACKET:
	case OW_TOK_R_BRACE:
		ow_parser_error_throw(
			parser, &tok->location,
			"unexpected `%s'",
			ow_token_type_represent(ow_token_type(tok))
		);

	case OW_TOK_KW_RETURN:
		return (struct ow_ast_Stmt *)ow_parser_parse_return_stmt(parser);

	case OW_TOK_KW_IMPORT:
		return (struct ow_ast_Stmt *)ow_parser_parse_import_stmt(parser);

	case OW_TOK_KW_IF:
		return (struct ow_ast_Stmt *)ow_parser_parse_if_else_stmt(parser);

	case OW_TOK_KW_FOR:
		return (struct ow_ast_Stmt *)ow_parser_parse_for_stmt(parser);

	case OW_TOK_KW_WHILE:
		return (struct ow_ast_Stmt *)ow_parser_parse_while_stmt(parser);

	case OW_TOK_KW_FUNC:
		return ow_likely(ow_token_type(token_queue_peek2(tq)) == OW_TOK_IDENTIFIER) ?
			(struct ow_ast_Stmt *)ow_parser_parse_func_stmt(parser):
			(struct ow_ast_Stmt *)
				ow_parser_make_expr_stmt(ow_parser_parse_expr(parser))/*lambda*/;

	case OW_TOK_KW_END:
	case OW_TOK_KW_SELF:
	case OW_TOK_KW_ELIF:
	case OW_TOK_KW_ELSE:
		return NULL;

	case OW_TOK_END_LINE:
		token_queue_advance(tq); // Ignore the empty statement.
		return ow_parser_parse_stmt(parser);

	case OW_TOK_END:
		return NULL;

	default:
		ow_unreachable();
	}
}

/// Parse a code block.
static void ow_parser_parse_block_to(
		struct ow_parser *parser, struct ow_ast_BlockStmt *block) {
	struct ow_ast_node_array *const stmts = &block->stmts;
	assert(!ow_ast_node_array_size(stmts));
	while (1) {
		struct ow_ast_Stmt *const stmt_node = ow_parser_parse_stmt(parser);
		if (ow_unlikely(!stmt_node))
			break;
		ow_ast_node_array_append(stmts, (struct ow_ast_node *)stmt_node);
	}
	if (ow_likely(ow_ast_node_array_size(stmts))) {
		block->location.begin = ow_ast_node_array_at(stmts, 0)->location.begin;
		block->location.end = ow_ast_node_array_last(stmts)->location.end;
	}
}

/// Parse a block statement.
static struct ow_ast_BlockStmt *ow_parser_parse_block_stmt(struct ow_parser *parser) {
	struct ow_ast_BlockStmt *const block = ow_ast_BlockStmt_new();
	ow_parser_protect_node(parser, block);
	ow_parser_parse_block_to(parser, block);
	ow_parser_unprotect_node(parser, block);
	return block;
}

static struct ow_ast_Module *ow_parser_parse_impl(struct ow_parser *parser) {
	assert(!ast_node_stack_size(&parser->temp_nodes));
	struct ow_ast_Module *const module = ow_ast_Module_new();
	ow_parser_protect_node(parser, module);

	assert(!module->code);
	module->code = ow_ast_BlockStmt_new();
	ow_parser_parse_block_to(parser, module->code);
	ow_parser_check_and_ignore(parser, OW_TOK_END);

	module->location = module->code->location;

	ow_parser_unprotect_node(parser, module);
	assert(!ast_node_stack_size(&parser->temp_nodes));
	return module;
}

#if OW_DEBUG_PARSER

ow_noinline static void ow_parser_dump_ast(const struct ow_ast *ast) {
	fputs("[AST] vvv\n", stderr);
	ow_ast_dump(ast, ow_iostream_stderr());
	fputs("[AST] ^^^\n", stderr);
}

#endif // OW_DEBUG_PARSER

bool ow_parser_parse(struct ow_parser *parser,
		struct ow_istream *stream, struct ow_sharedstr *file_name, int flags,
		struct ow_ast *ast) {
	ow_unused_var(flags);

	ow_parser_clear(parser);
	token_queue_clear(&parser->token_queue);
	ow_lexer_source(parser->lexer, stream, file_name);
	token_queue_sync(&parser->token_queue);

	if (!ow_parser_error_setjmp(parser)) {
		struct ow_ast_Module *const mod = ow_parser_parse_impl(parser);
		ow_ast_set_module(ast, mod);
		ow_ast_set_filename(ast, file_name);
#if OW_DEBUG_PARSER
		if (ow_unlikely(parser->verbose))
			ow_parser_dump_ast(ast);
#endif // OW_DEBUG_PARSER
		return true;
	} else {
		assert(ow_parser_error(parser));
		return false;
	}
}

struct ow_syntax_error *ow_parser_error(struct ow_parser *parser) {
	if (!parser->error_info.message)
		return NULL;
	return &parser->error_info;
}
