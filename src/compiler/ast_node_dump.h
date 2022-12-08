// For "ast.c" only.

#include "ast.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <utilities/attributes.h>
#include <utilities/stream.h>
#include <utilities/unreachable.h>

ow_noinline static void print_node_line(
		size_t level, struct ow_iostream *out, const char *fmt, ...) {
	char buffer[128];

	const size_t indent_width = ow_unlikely(level > 16) ? 16 * 2 : level * 2;
	memset(buffer, ' ', indent_width);
	char *p = buffer + indent_width;

	va_list ap;
	va_start(ap, fmt);
	const int n = vsnprintf(p, sizeof buffer - indent_width, fmt, ap);
	va_end(ap);
	assert(n > 0);
	p += (size_t)n;
	*p = '\n';

	ow_iostream_write(out, buffer, p - buffer + 1);
}

ow_noinline static void print_node_begin_tag(
		const char *name, const struct ow_source_range *loc,
		size_t level, struct ow_iostream *out) {
	if (loc)
		print_node_line(
			level, out, "<%s location=\"%u:%u-%u:%u\">",
			name, loc->begin.line, loc->begin.column, loc->end.line, loc->end.column);
	else
		print_node_line(level, out, "<%s>", name);
}

ow_noinline static void print_node_end_tag(
		const char *name, size_t level, struct ow_iostream *out) {
	print_node_line(level, out, "</%s>", name);
}

ow_noinline static void xml_escape_string(const char *s, char *buf, size_t buf_sz) {
	assert(buf_sz > 4);
	char *buf_p = buf, *buf_end = buf + buf_sz;
	while (1) {
		if (ow_unlikely(buf_p == buf_end)) {
		no_enough_space:
			buf_p--;
			buf_p[-3] = '.', buf_p[-2] = '.', buf_p[-1] = '.', buf_p[0] = '\0';
			return;
		}
		const char c = *s++;
		if (ow_unlikely(!c)) {
			*buf_p = '\0';
			return;
		}
		if (iscntrl(c) || c == ' ') {
			if (buf_end - buf_p < 6)
				goto no_enough_space;
			char escape[8];
			snprintf(escape, sizeof escape, "&#x%02x;", c);
			memcpy(buf_p, escape, 6);
			buf_p += 6;
		} else {
			const char *escape;
			switch (c) {
			case '"' : escape = "&quot;";break;
			case '\'': escape = "&apos;";break;
			case '<' : escape = "&lt;";  break;
			case '>' : escape = "&gt;";  break;
			case '&' : escape = "&amp;"; break;
			default  : escape = NULL;   break;
			}
			if (escape) {
				const size_t n = strlen(escape);
				if ((size_t)(buf_end - buf_p) < n)
					goto no_enough_space;
				memcpy(buf_p, escape, n);
				buf_p += n;
			} else {
				*buf_p++ = c;
			}
		}
	}
}

static void ow_ast_node_dump(const struct ow_ast_node *, size_t, struct ow_iostream *);

#define ELEM(NAME) \
	static void ow_ast_##NAME##_dump( \
		const struct ow_ast_##NAME *, size_t, struct ow_iostream *);
OW_AST_NODE_LIST
#undef ELEM

static void ow_ast_NilLiteral_dump(
		const struct ow_ast_NilLiteral *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_BoolLiteral_dump(
		const struct ow_ast_BoolLiteral *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_line(level + 1, out, node->value ? "true" : "false");
	print_node_end_tag(name, level, out);
}

static void ow_ast_IntLiteral_dump(
		const struct ow_ast_IntLiteral *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_line(level + 1, out, "%" PRIi64, node->value);
	print_node_end_tag(name, level, out);
}

static void ow_ast_FloatLiteral_dump(
		const struct ow_ast_FloatLiteral *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_line(level + 1, out, "%f", node->value);
	print_node_end_tag(name, level, out);
}

static void ow_ast_StringLikeLiteral_dump(
		const struct ow_ast_StringLikeLiteral *node, size_t level, struct ow_iostream *out) {
	char string[64];
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	xml_escape_string(ow_sharedstr_data(node->value), string, sizeof string);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_line(level + 1, out, "%s", string);
	print_node_end_tag(name, level, out);
}

static void ow_ast_SymbolLiteral_dump(
		const struct ow_ast_SymbolLiteral *node, size_t level, struct ow_iostream *out) {
	ow_ast_StringLikeLiteral_dump((const struct ow_ast_StringLikeLiteral *)node, level, out);
}

static void ow_ast_StringLiteral_dump(
		const struct ow_ast_StringLiteral *node, size_t level, struct ow_iostream *out) {
	ow_ast_StringLikeLiteral_dump((const struct ow_ast_StringLikeLiteral *)node, level, out);
}

static void ow_ast_Identifier_dump(
		const struct ow_ast_Identifier *node, size_t level, struct ow_iostream *out) {
	ow_ast_StringLikeLiteral_dump((const struct ow_ast_StringLikeLiteral *)node, level, out);
}

static void ow_ast_BinOpExpr_dump(
		const struct ow_ast_BinOpExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("lhs", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->lhs, level + 2, out);
	print_node_end_tag("lhs", level + 1, out);
	print_node_begin_tag("rhs", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->rhs, level + 2, out);
	print_node_end_tag("rhs", level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_AddExpr_dump(
		const struct ow_ast_AddExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_SubExpr_dump(
		const struct ow_ast_SubExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_MulExpr_dump(
		const struct ow_ast_MulExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_DivExpr_dump(
		const struct ow_ast_DivExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_RemExpr_dump(
		const struct ow_ast_RemExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_ShlExpr_dump(
		const struct ow_ast_ShlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_ShrExpr_dump(
		const struct ow_ast_ShrExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitAndExpr_dump(
		const struct ow_ast_BitAndExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitOrExpr_dump(
		const struct ow_ast_BitOrExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitXorExpr_dump(
		const struct ow_ast_BitXorExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_EqlExpr_dump(
		const struct ow_ast_EqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_AddEqlExpr_dump(
		const struct ow_ast_AddEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_SubEqlExpr_dump(
		const struct ow_ast_SubEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_MulEqlExpr_dump(
		const struct ow_ast_MulEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_DivEqlExpr_dump(
		const struct ow_ast_DivEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_RemEqlExpr_dump(
		const struct ow_ast_RemEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_ShlEqlExpr_dump(
		const struct ow_ast_ShlEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_ShrEqlExpr_dump(
		const struct ow_ast_ShrEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitAndEqlExpr_dump(
		const struct ow_ast_BitAndEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitOrEqlExpr_dump(
		const struct ow_ast_BitOrEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_BitXorEqlExpr_dump(
		const struct ow_ast_BitXorEqlExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_EqExpr_dump(
		const struct ow_ast_EqExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_NeExpr_dump(
		const struct ow_ast_NeExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_LtExpr_dump(
		const struct ow_ast_LtExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_LeExpr_dump(
		const struct ow_ast_LeExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_GtExpr_dump(
		const struct ow_ast_GtExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_GeExpr_dump(
		const struct ow_ast_GeExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_AndExpr_dump(
		const struct ow_ast_AndExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_OrExpr_dump(
		const struct ow_ast_OrExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_AttrAccessExpr_dump(
		const struct ow_ast_AttrAccessExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_MethodUseExpr_dump(
		const struct ow_ast_MethodUseExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_BinOpExpr_dump((const struct ow_ast_BinOpExpr *)node, level, out);
}

static void ow_ast_UnOpExpr_dump(
		const struct ow_ast_UnOpExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->val, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_PosExpr_dump(
		const struct ow_ast_PosExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_UnOpExpr_dump((const struct ow_ast_UnOpExpr *)node, level, out);
}

static void ow_ast_NegExpr_dump(
		const struct ow_ast_NegExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_UnOpExpr_dump((const struct ow_ast_UnOpExpr *)node, level, out);
}

static void ow_ast_BitNotExpr_dump(
		const struct ow_ast_BitNotExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_UnOpExpr_dump((const struct ow_ast_UnOpExpr *)node, level, out);
}

static void ow_ast_NotExpr_dump(
		const struct ow_ast_NotExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_UnOpExpr_dump((const struct ow_ast_UnOpExpr *)node, level, out);
}

static void ow_ast_CallExpr_dump(
		const struct ow_ast_CallExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("func", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->obj, level + 2, out);
	print_node_end_tag("func", level + 1, out);
	print_node_begin_tag("args", NULL, level + 1, out);
	for (size_t i = 0, n = ow_ast_node_array_size(&node->args); i < n; i++) {
		ow_ast_node_dump(ow_ast_node_array_at(
			(struct ow_ast_node_array *)&node->args, i), level + 2, out);
	}
	print_node_end_tag("args", level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_SubscriptExpr_dump(
		const struct ow_ast_SubscriptExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("obj", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->obj, level + 2, out);
	print_node_end_tag("obj", level + 1, out);
	print_node_begin_tag("indices", NULL, level + 1, out);
	for (size_t i = 0, n = ow_ast_node_array_size(&node->args); i < n; i++) {
		ow_ast_node_dump(ow_ast_node_array_at(
				(struct ow_ast_node_array *)&node->args, i), level + 2, out);
	}
	print_node_end_tag("indices", level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_ArrayLikeExpr_dump(
		const struct ow_ast_ArrayLikeExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	for (size_t i = 0, n = ow_ast_node_array_size(&node->elems); i < n; i++) {
		ow_ast_node_dump(ow_ast_node_array_at(
			(struct ow_ast_node_array *)&node->elems, i), level + 1, out);
	}
	print_node_end_tag(name, level, out);
}

static void ow_ast_TupleExpr_dump(
		const struct ow_ast_TupleExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_ArrayLikeExpr_dump((struct ow_ast_ArrayLikeExpr *)node, level, out);
}

static void ow_ast_ArrayExpr_dump(
		const struct ow_ast_ArrayExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_ArrayLikeExpr_dump((struct ow_ast_ArrayLikeExpr *)node, level, out);
}

static void ow_ast_SetExpr_dump(
		const struct ow_ast_SetExpr *node, size_t level, struct ow_iostream *out) {
	ow_ast_ArrayLikeExpr_dump((struct ow_ast_ArrayLikeExpr *)node, level, out);
}

static void ow_ast_MapExpr_dump(
		const struct ow_ast_MapExpr *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	for (size_t i = 0, n = ow_ast_nodepair_array_size(&node->pairs); i < n; i++) {
		const struct ow_ast_nodepair_array_elem pair =
			ow_ast_nodepair_array_at((struct ow_ast_nodepair_array *)&node->pairs, i);
		print_node_begin_tag("key", NULL, level + 1, out);
		ow_ast_node_dump(pair.first, level + 2, out);
		print_node_end_tag("key", level + 1, out);
		print_node_begin_tag("val", NULL, level + 1, out);
		ow_ast_node_dump(pair.second, level + 2, out);
		print_node_end_tag("val", level + 1, out);
	}
	print_node_end_tag(name, level, out);
}

static void ow_ast_ExprStmt_dump(
		const struct ow_ast_ExprStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->expr, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_BlockStmt_dump(
		const struct ow_ast_BlockStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	for (size_t i = 0, n = ow_ast_node_array_size(&node->stmts); i < n; i++) {
		ow_ast_node_dump(ow_ast_node_array_at(
			(struct ow_ast_node_array *)&node->stmts, i), level + 1, out);
	}
	print_node_end_tag(name, level, out);
}

static void ow_ast_ReturnStmt_dump(
		const struct ow_ast_ReturnStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->ret_val, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_MagicReturnStmt_dump(
		const struct ow_ast_MagicReturnStmt *node, size_t level, struct ow_iostream *out) {
	ow_ast_ReturnStmt_dump((const struct ow_ast_ReturnStmt *)node, level, out);
}

static void ow_ast_ImportStmt_dump(
		const struct ow_ast_ImportStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->mod_name, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_IfElseStmt_dump(
		const struct ow_ast_IfElseStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	for (size_t i = 0, n = ow_ast_nodepair_array_size(&node->branches); i < n; i++) {
		const struct ow_ast_nodepair_array_elem branch =
			ow_ast_nodepair_array_at((struct ow_ast_nodepair_array *)&node->branches, i);
		print_node_begin_tag("cond", NULL, level + 1, out);
		ow_ast_node_dump(branch.first, level + 2, out);
		print_node_end_tag("cond", level + 1, out);
		print_node_begin_tag("body", NULL, level + 1, out);
		ow_ast_node_dump(branch.second, level + 2, out);
		print_node_end_tag("body", level + 1, out);
	}
	if (node->else_branch) {
		print_node_begin_tag("cond", NULL, level + 1, out);
		print_node_end_tag("cond", level + 1, out);
		print_node_begin_tag("body", NULL, level + 1, out);
		ow_ast_BlockStmt_dump(node->else_branch, level + 2, out);
		print_node_end_tag("body", level + 1, out);
	}
	print_node_end_tag(name, level, out);
}

static void ow_ast_ForStmt_dump(
		const struct ow_ast_ForStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("var", NULL, level + 1, out);
	ow_ast_Identifier_dump(node->var, level + 2, out);
	print_node_end_tag("var", level + 1, out);
	print_node_begin_tag("iter", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->iter, level + 2, out);
	print_node_end_tag("iter", level + 1, out);
	ow_ast_BlockStmt_dump((const struct ow_ast_BlockStmt *)node, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_WhileStmt_dump(
		const struct ow_ast_WhileStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("cond", NULL, level + 1, out);
	ow_ast_node_dump((const struct ow_ast_node *)node->cond, level + 2, out);
	print_node_end_tag("cond", level + 1, out);
	ow_ast_BlockStmt_dump((const struct ow_ast_BlockStmt *)node, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_FuncStmt_dump(
		const struct ow_ast_FuncStmt *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	print_node_begin_tag("name", NULL, level + 1, out);
	ow_ast_Identifier_dump(node->name, level + 2, out);
	print_node_end_tag("name", level + 1, out);
	print_node_begin_tag("args", NULL, level + 1, out);
	ow_ast_ArrayExpr_dump(node->args, level + 2, out);
	print_node_end_tag("args", level + 1, out);
	ow_ast_BlockStmt_dump((const struct ow_ast_BlockStmt *)node, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_Module_dump(
		const struct ow_ast_Module *node, size_t level, struct ow_iostream *out) {
	const char *const name = ow_ast_node_name((const struct ow_ast_node *)node);
	print_node_begin_tag(name, &node->location, level, out);
	ow_ast_BlockStmt_dump(node->code, level + 1, out);
	print_node_end_tag(name, level, out);
}

static void ow_ast_node_dump(
			const struct ow_ast_node *node, size_t level, struct ow_iostream *out) {
	switch (node->type) {
#define ELEM(NAME) case OW_AST_NODE_##NAME : \
	ow_ast_##NAME##_dump((struct ow_ast_##NAME *)node, level, out); break;
	OW_AST_NODE_LIST
#undef ELEM
	default:
		ow_unreachable();
	}
}
