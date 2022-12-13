#include "codegen.h"

#include <assert.h>
#include <setjmp.h>

#include "assembler.h"
#include "ast.h"
#include "error.h"
#include <machine/globals.h>
#include <machine/machine.h>
#include <machine/symbols.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <objects/symbolobj.h>
#include <utilities/array.h>
#include <utilities/hash.h>
#include <utilities/hashmap.h>
#include <utilities/malloc.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

#include <config/options.h>

#if OW_DEBUG_CODEGEN

#include <stdio.h>

#include <bytecode/dumpcode.h>
#include <objects/funcobj.h>
#include <utilities/stream.h>

static void verbose_dump_func(
		unsigned int line, const char *name, struct ow_func_obj *func) {
	fprintf(stderr, "[CODEGEN] %s (line %u) vvv\n", name, line);
	ow_bytecode_dump(func->code, 0, func->code_size, (size_t)-1, ow_iostream_stderr());
	fputs("[CODEGEN] ^^^\n", stderr);
}

#endif // OW_DEBUG_CODEGEN

/// A stack of assemblers.
struct code_stack {
	struct ow_array assemblers;
	struct ow_array free_assemblers;
};

/// Initialize the stack.
static void code_stack_init(struct code_stack *stack) {
	ow_array_init(&stack->assemblers, 4);
	ow_array_init(&stack->free_assemblers, 4);
}

/// Clear the stack.
static void code_stack_clear(struct code_stack *stack) {
	for (size_t i = 0, n = ow_array_size(&stack->assemblers); i < n; i++) {
		struct ow_assembler *const as = ow_array_at(&stack->assemblers, i);
		ow_assembler_clear(as);
		ow_array_append(&stack->free_assemblers, as);
	}
	ow_array_clear(&stack->assemblers);
}

/// Finalize the stack.
static void code_stack_fini(struct code_stack *stack) {
	code_stack_clear(stack);
	assert(!ow_array_size(&stack->assemblers));
	for (size_t i = 0, n = ow_array_size(&stack->free_assemblers); i < n; i++)
		ow_assembler_del(ow_array_at(&stack->free_assemblers, i));
	ow_array_fini(&stack->assemblers);
	ow_array_fini(&stack->free_assemblers);
}

/// Push a new assembler and return it.
static struct ow_assembler *code_stack_push(
		struct code_stack *stack, struct ow_machine *om) {
	struct ow_assembler *as;
	if (ow_array_size(&stack->free_assemblers)) {
		as = ow_array_last(&stack->free_assemblers);
		ow_array_drop(&stack->free_assemblers);
	} else {
		as = ow_assembler_new(om);
	}
	ow_array_append(&stack->assemblers, as);
	return as;
}

/// Make a function from current assembler and pop it.
static struct ow_func_obj *code_stack_make_func_and_pop(
		struct code_stack *stack, const struct ow_assembler_output_spec *spec) {
	assert(ow_array_size(&stack->assemblers));
	struct ow_assembler *const as = ow_array_last(&stack->assemblers);
	struct ow_func_obj *const res = ow_assembler_output(as, spec);
	ow_array_drop(&stack->assemblers);
	ow_assembler_clear(as);
	ow_array_append(&stack->free_assemblers, as);
	return  res;
}

/// Get current assembler.
static struct ow_assembler *code_stack_top(struct code_stack *stack) {
	assert(ow_array_size(&stack->assemblers));
	return ow_array_last(&stack->assemblers);
}

/// Type of a lexical scope.
enum scope_type {
	SCOPE_MODULE,
	SCOPE_ARGS,
	SCOPE_FUNC,
};

/// Lexical scope.
struct scope {
	enum scope_type type;
	struct scope *parent_scope;
	struct ow_hashmap _variables_map; // {name, index}
};

/// Create a scope object.
static struct scope *scope_new(void) {
	struct scope *const scope = ow_malloc(sizeof(struct scope));
	ow_hashmap_init(&scope->_variables_map, 0);
	return scope;
}

int _scope_clear_walker(void *arg, const void *key, void *val) {
	ow_unused_var(arg), ow_unused_var(val);
	struct ow_sharedstr *const str = (void *)key;
	ow_sharedstr_unref(str);
	return 0;
}

/// Clear data in a scope.
static void scope_clear(struct scope *scope) {
	ow_hashmap_foreach(&scope->_variables_map, _scope_clear_walker, NULL);
	ow_hashmap_clear(&scope->_variables_map);
}

/// Destroy a scope object.
static void scope_del(struct scope *scope) {
	scope_clear(scope);
	ow_hashmap_fini(&scope->_variables_map);
	ow_free(scope);
}

/// Try to find variable. If found, return the index; otherwise, return -1.
static size_t scope_find_variable(
		struct scope *scope, const struct ow_sharedstr *name) {
	void *const res = ow_hashmap_get(
		&scope->_variables_map, &ow_sharedstr_hashmap_funcs, name);
	return (size_t)(uintptr_t)res - 1;
}

/// Register a variable and return its index.
static size_t scope_register_variable(
		struct scope *scope, struct ow_sharedstr *name) {
	assert(scope_find_variable(scope, name) == (size_t)-1);
	const size_t index = ow_hashmap_size(&scope->_variables_map);
	ow_hashmap_set(
		&scope->_variables_map, &ow_sharedstr_hashmap_funcs,
		ow_sharedstr_ref(name), (void *)(uintptr_t)(index + 1));
	return index;
}

struct _scope_foreach_variable_context {
	int(*walker)(void *, struct ow_sharedstr *, size_t);
	void *arg;
};

static int _scope_foreach_variable_walker(void *_ctx, const void *key, void *val) {
	struct _scope_foreach_variable_context *const ctx = _ctx;
	return ctx->walker(ctx->arg, (struct ow_sharedstr *)key, (uintptr_t)val - 1);
}

/// Traverse through the scope variables.
static void scope_foreach_variable(
		struct scope *scope,
		int(*walker)(void *, struct ow_sharedstr *, size_t), void *arg) {
	ow_hashmap_foreach(
		&scope->_variables_map, _scope_foreach_variable_walker,
		&(struct _scope_foreach_variable_context){walker, arg});
}

/// A stack of scopes.
struct scope_stack {
	struct scope *scope_list;
	struct scope *free_scope_list;
};

/// Initialize a scope stack.
static void scope_stack_init(struct scope_stack *stack) {
	stack->scope_list = NULL;
	stack->free_scope_list = NULL;
}

/// Pop all scopes in a scope stack.
static void scope_stack_clear(struct scope_stack *stack) {
	for (struct scope *p = stack->scope_list; p; ) {
		struct scope *next = p->parent_scope;
		p->parent_scope = stack->free_scope_list;
		scope_clear(p);
		stack->free_scope_list = p;
		p = next;
	}

	stack->scope_list = NULL;
}

/// Finalize a scope stack.
static void scope_stack_fini(struct scope_stack *stack) {
	scope_stack_clear(stack);
	assert(!stack->scope_list);

	for (struct scope *scope = stack->free_scope_list; scope; ) {
		struct scope *next = scope->parent_scope;
		scope_del(scope);
		scope = next;
	}
}

/// Push a new scope.
static struct scope *scope_stack_push(
		struct scope_stack *stack, enum scope_type type) {
	struct scope *scope;

	if (stack->free_scope_list) {
		scope = stack->free_scope_list;
		stack->free_scope_list = scope->parent_scope;
	} else {
		scope = scope_new();
	}

	scope->type = type;
	assert(!ow_hashmap_size(&scope->_variables_map));
	if (ow_likely(stack->scope_list))
		scope->parent_scope = stack->scope_list;
	else
		scope->parent_scope = NULL;
	stack->scope_list = scope;
	return scope;
}

/// Pop current scope.
static void scope_stack_pop(struct scope_stack *stack) {
	assert(stack->scope_list);
	struct scope *const scope = stack->scope_list;
	scope_clear(scope);
	stack->scope_list = scope->parent_scope;
	scope->parent_scope = stack->free_scope_list;
	stack->free_scope_list = scope;
}

/// Get current scope.
static struct scope *scope_stack_top(struct scope_stack *stack) {
	assert(stack->scope_list);
	return stack->scope_list;
}

struct ow_codegen {
	struct code_stack code_stack;
	struct scope_stack scope_stack;
	struct ow_module_obj *module;
	struct ow_machine *machine;
	jmp_buf error_jmpbuf;
	struct ow_syntax_error error_info;
#if OW_DEBUG_CODEGEN
	bool verbose;
#endif // OW_DEBUG_CODEGEN
};

/// A wrapper of `setjmp()`.
#define ow_codegen_error_setjmp(codegen) \
	(setjmp((codegen)->error_jmpbuf))

/// Throw error.
ow_noinline ow_noreturn static void ow_codegen_error_throw(
		struct ow_codegen *codegen, const struct ow_source_range *location,
		const char *msg_fmt, ...) {
	va_list ap;
	va_start(ap, msg_fmt);
	ow_syntax_error_vformat(&codegen->error_info, location, msg_fmt, ap);
	va_end(ap);
	longjmp(codegen->error_jmpbuf, 1);
}

ow_noinline ow_noreturn static void ow_codegen_error_throw_not_implemented(
	struct ow_codegen *codegen, const struct ow_source_range *location) {
	ow_codegen_error_throw(codegen, location, "not implemented");
}

static void _codegen_gc_marker(struct ow_machine *om, void *_codegen) {
	struct ow_codegen *const codegen = _codegen;
	assert(om == codegen->machine);
	if (codegen->module)
		ow_objmem_object_gc_marker(om, ow_object_from(codegen->module));
}

ow_nodiscard struct ow_codegen *ow_codegen_new(struct ow_machine *om) {
	struct ow_codegen *const codegen = ow_malloc(sizeof(struct ow_codegen));

	code_stack_init(&codegen->code_stack);
	scope_stack_init(&codegen->scope_stack);
	codegen->module = NULL;
	codegen->machine = om;
	ow_syntax_error_init(&codegen->error_info);
#if OW_DEBUG_CODEGEN
	codegen->verbose = false;
#endif // OW_DEBUG_CODEGEN

	ow_objmem_add_gc_root(om, codegen, _codegen_gc_marker);
	return codegen;
}

void ow_codegen_del(struct ow_codegen *codegen) {
	ow_objmem_remove_gc_root(codegen->machine, codegen);

	ow_codegen_clear(codegen);
	ow_syntax_error_fini(&codegen->error_info);
	scope_stack_fini(&codegen->scope_stack);
	code_stack_fini(&codegen->code_stack);

	ow_free(codegen);
}

void ow_codegen_verbose(struct ow_codegen *codegen, bool status) {
	ow_unused_var(codegen);
	ow_unused_var(status);
#if OW_DEBUG_CODEGEN
	codegen->verbose = status;
#endif // OW_DEBUG_CODEGEN
}

void ow_codegen_clear(struct ow_codegen *codegen) {
	code_stack_clear(&codegen->code_stack);
	scope_stack_clear(&codegen->scope_stack);
	codegen->module = NULL;
	ow_syntax_error_clear(&codegen->error_info);
}

/// Action of a generating function.
enum codegen_action {
	ACT_PUSH, // Evaluate and push result.
	ACT_EVAL, // Evaluate only.
	ACT_RECV, // Pop and assign.
};

static void ow_codegen_emit_node(
	struct ow_codegen *, enum codegen_action action, const struct ow_ast_node *);

#define ELEM(NAME) \
	static void ow_codegen_emit_##NAME( \
		struct ow_codegen *, enum codegen_action action, const struct ow_ast_##NAME *);
OW_AST_NODE_LIST
#undef ELEM

static void ow_codegen_asm_push_constant(
		struct ow_codegen *codegen, const struct ow_ast_node *node,
		struct ow_assembler *as, size_t index) {
	if (index <= UINT8_MAX)
		ow_assembler_append(as, OW_OPC_LdCnst, (union ow_operand){.u8 = (uint8_t)index});
	else if (index <= UINT16_MAX)
		ow_assembler_append(as, OW_OPC_LdCnstW, (union ow_operand){.u16 = (uint16_t)index});
	else
		ow_codegen_error_throw(codegen, &node->location, "too many constants");
}

static void ow_codegen_asm_push_symbol(
		struct ow_codegen *codegen, const struct ow_ast_node *node,
		struct ow_assembler *as, size_t index) {
	if (index <= UINT8_MAX)
		ow_assembler_append(as, OW_OPC_LdSym, (union ow_operand){.u8 = (uint8_t)index});
	else if (index <= UINT16_MAX)
		ow_assembler_append(as, OW_OPC_LdSymW, (union ow_operand){.u16 = (uint16_t)index});
	else
		ow_codegen_error_throw(codegen, &node->location, "too many symbols");
}

static void ow_codegen_emit_NilLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_NilLiteral *node) {
	ow_unused_var(node);
	assert(action == ACT_EVAL || action == ACT_PUSH);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	ow_assembler_append(as, OW_OPC_LdNil, (union ow_operand){.u8 = 0});
}

static void ow_codegen_emit_BoolLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BoolLiteral *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	ow_assembler_append(as, OW_OPC_LdBool, (union ow_operand){.u8 = node->value});
}

static void ow_codegen_emit_IntLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_IntLiteral *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const int64_t value = node->value;
	if (INT8_MIN <= value && value <= INT8_MAX) {
		ow_assembler_append(as, OW_OPC_LdInt, (union ow_operand){.i8 = (int8_t)value});
	} else if (INT16_MIN <= value && value <= INT16_MAX) {
		ow_assembler_append(as, OW_OPC_LdIntW, (union ow_operand){.i16 = (int16_t)value});
	} else {
		const size_t index = ow_assembler_constant(
			as, (struct ow_assembler_constant){.type = OW_AS_CONST_INT, .i = value});
		ow_codegen_asm_push_constant(
			codegen, (const struct ow_ast_node *)node, as, index);
	}
}

static void ow_codegen_emit_FloatLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_FloatLiteral *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const double value = node->value;
	const int8_t value_as_i8 = (int8_t)value;
	if ((double)value_as_i8 == value) {
		ow_assembler_append(as, OW_OPC_LdFlt, (union ow_operand){.i8 = value_as_i8});
	} else {
		const size_t index = ow_assembler_constant(
			as, (struct ow_assembler_constant){.type = OW_AS_CONST_FLT, .f = value});
		ow_codegen_asm_push_constant(
			codegen, (const struct ow_ast_node *)node, as, index);
	}
}

static void ow_codegen_emit_SymbolLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_SymbolLiteral *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const size_t index = ow_assembler_symbol(
		as, ow_sharedstr_data(node->value), ow_sharedstr_size(node->value));
	ow_codegen_asm_push_symbol(
		codegen, (const struct ow_ast_node *)node, as, index);
}

static void ow_codegen_emit_StringLiteral(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_StringLiteral *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	if (ow_unlikely(action == ACT_EVAL))
		return;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const size_t index = ow_assembler_constant(
		as, (struct ow_assembler_constant){
			.type = OW_AS_CONST_STR,
			.s = {
				.p = ow_sharedstr_data(node->value),
				.n = ow_sharedstr_size(node->value),
			},
		}
	);
	ow_codegen_asm_push_constant(
		codegen, (const struct ow_ast_node *)node, as, index);
}

static void ow_codegen_emit_Identifier(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_Identifier *node) {
	if (ow_unlikely(action == ACT_EVAL))
		return;

	struct ow_sharedstr *const name = node->value;
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);

	enum ow_opcode opcode;
	union ow_operand operand;
	bool first_module = true, first_args = true, first_func = true;
	for (struct scope *scope = scope_stack_top(&codegen->scope_stack);
			scope; scope = scope->parent_scope) {
		const size_t var_index = scope_find_variable(scope, name);
		switch (scope->type) {
		case SCOPE_MODULE:
			if (!first_module)
				continue;
			first_module = false;
			if (var_index == (size_t)-1)
				continue;
			if (action == ACT_PUSH) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_LdGlob, operand.u8 = (uint8_t)var_index;
				else if (var_index <= UINT16_MAX)
					opcode = OW_OPC_LdGlobW, operand.u16 = (uint16_t)var_index;
				else
					ow_unreachable();
			} else if (action == ACT_RECV) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_StGlob, operand.u8 = (uint8_t)var_index;
				else if (var_index <= UINT16_MAX)
					opcode = OW_OPC_StGlobW, operand.u16 = (uint16_t)var_index;
				else
					ow_unreachable();
			} else {
				ow_unreachable();
			}
			goto write_instruction;

		case SCOPE_ARGS:
			if (!first_args)
				continue;
			first_args = false;
			if (var_index == (size_t)-1)
				continue;
			if (action == ACT_PUSH) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_LdArg, operand.u8 = (uint8_t)var_index;
				else
					ow_unreachable();
			} else if (action == ACT_RECV) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_StArg, operand.u8 = (uint8_t)var_index;
				else
					ow_unreachable();
			} else {
				ow_unreachable();
			}
			goto write_instruction;

		case SCOPE_FUNC:
			if (!first_func)
				continue;
			first_func = false;
			if (var_index == (size_t)-1)
				continue;
			if (action == ACT_PUSH) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_LdLoc, operand.u8 = (uint8_t)var_index;
				else if (var_index <= UINT16_MAX)
					opcode = OW_OPC_LdLocW, operand.u16 = (uint16_t)var_index;
				else
					ow_unreachable();
			} else if (action == ACT_RECV) {
				if (var_index <= UINT8_MAX)
					opcode = OW_OPC_StLoc, operand.u8 = (uint8_t)var_index;
				else if (var_index <= UINT16_MAX)
					opcode = OW_OPC_StLocW, operand.u16 = (uint16_t)var_index;
				else
					ow_unreachable();
			} else {
				ow_unreachable();
			}
			goto write_instruction;

		default:
			ow_unreachable();
		}
	}
	if (action == ACT_PUSH) {
		const size_t sym_index =
			ow_assembler_symbol(as, ow_sharedstr_data(name), ow_sharedstr_size(name));
		if (sym_index <= UINT8_MAX)
			opcode = OW_OPC_LdGlobY, operand.u8 = (uint8_t)sym_index;
		else if (sym_index <= UINT16_MAX)
			opcode = OW_OPC_LdGlobYW, operand.u16 = (uint16_t)sym_index;
		else
			ow_codegen_error_throw(codegen, &node->location, "too many symbols");
	} else if (action == ACT_RECV) {
		struct scope *const current_scope =
			scope_stack_top(&codegen->scope_stack);
		const size_t var_index = scope_register_variable(current_scope, name);
		if (ow_unlikely(var_index > UINT16_MAX))
			ow_codegen_error_throw(codegen, &node->location, "too many variables");
		if (current_scope->type == SCOPE_FUNC) {
			if (var_index <= UINT8_MAX)
				opcode = OW_OPC_StLoc, operand.u8 = (uint8_t)var_index;
			else if (var_index <= UINT16_MAX)
				opcode = OW_OPC_StLocW, operand.u16 = (uint16_t)var_index;
			else
				ow_unreachable();
		} else if (current_scope->type == SCOPE_MODULE) {
			if (var_index <= UINT8_MAX)
				opcode = OW_OPC_StGlob, operand.u8 = (uint8_t)var_index;
			else if (var_index <= UINT16_MAX)
				opcode = OW_OPC_StGlobW, operand.u16 = (uint16_t)var_index;
			else
				ow_unreachable();
		} else {
			ow_codegen_error_throw_not_implemented(codegen, &node->location);
		}
	} else {
		ow_unreachable();
	}

write_instruction:
	ow_assembler_append(as, opcode, operand);
}

ow_noinline static void ow_codegen_emit_BinOpExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BinOpExpr *node, enum ow_opcode opcode) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->lhs);
	ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->rhs);
	ow_assembler_append(as, opcode, (union ow_operand){.u8 = 0});
	if (ow_unlikely(action == ACT_EVAL))
		ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
}

static void ow_codegen_emit_AddExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_AddExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Add);
}

static void ow_codegen_emit_SubExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_SubExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Sub);
}

static void ow_codegen_emit_MulExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_MulExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Mul);
}

static void ow_codegen_emit_DivExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_DivExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Div);
}

static void ow_codegen_emit_RemExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_RemExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Rem);
}

static void ow_codegen_emit_ShlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ShlExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Shl);
}

static void ow_codegen_emit_ShrExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ShrExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Shr);
}

static void ow_codegen_emit_BitAndExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitAndExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_And);
}

static void ow_codegen_emit_BitOrExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitOrExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Or);
}

static void ow_codegen_emit_BitXorExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitXorExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Xor);
}

ow_noinline static void ow_codegen_emit_EqlOpExpr(
	struct ow_codegen *codegen, enum codegen_action action,
	const struct ow_ast_BinOpExpr *node, enum ow_opcode opcode) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const enum ow_ast_node_type lhs_type = node->lhs->type;
	if (ow_unlikely(lhs_type != OW_AST_NODE_Identifier
					&& lhs_type != OW_AST_NODE_AttrAccessExpr))
		ow_codegen_error_throw(codegen, &node->location, "illegal assignment");
	if (opcode == (enum ow_opcode)0)
		ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->rhs);
	else
		ow_codegen_emit_BinOpExpr(codegen, ACT_PUSH, node, opcode);
	if (ow_unlikely(action == ACT_PUSH))
		ow_assembler_append(as, OW_OPC_Dup, (union ow_operand){.u8 = 0});
	ow_codegen_emit_node(codegen, ACT_RECV, (struct ow_ast_node *)node->lhs);
}

static void ow_codegen_emit_EqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_EqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, (enum ow_opcode)0);
}

static void ow_codegen_emit_AddEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_AddEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Add);
}

static void ow_codegen_emit_SubEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_SubEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Sub);
}

static void ow_codegen_emit_MulEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_MulEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Mul);
}

static void ow_codegen_emit_DivEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_DivEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Div);
}

static void ow_codegen_emit_RemEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_RemEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Rem);
}

static void ow_codegen_emit_ShlEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ShlEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Shl);
}

static void ow_codegen_emit_ShrEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ShrEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Shr);
}

static void ow_codegen_emit_BitAndEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitAndEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_And);
}

static void ow_codegen_emit_BitOrEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitOrEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Or);
}

static void ow_codegen_emit_BitXorEqlExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitXorEqlExpr *node) {
	ow_codegen_emit_EqlOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_Xor);
}

static void ow_codegen_emit_EqExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_EqExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpEq);
}

static void ow_codegen_emit_NeExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_NeExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpNe);
}

static void ow_codegen_emit_LtExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_LtExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpLt);
}

static void ow_codegen_emit_LeExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_LeExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpLe);
}

static void ow_codegen_emit_GtExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_GtExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpGt);
}

static void ow_codegen_emit_GeExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_GeExpr *node) {
	ow_codegen_emit_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, OW_OPC_CmpGe);
}

ow_noinline static void ow_codegen_emit_logical_BinOpExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BinOpExpr *node, bool which /* true=Or, false = And */) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const int lbl_end = ow_assembler_prepare_label(as);
	ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->lhs);
	ow_assembler_append(as, OW_OPC_Dup, (union ow_operand){.u8 = 0});
	// TODO: Add new instructions to simplify this.
	ow_assembler_append_jump(as, which ? OW_OPC_JmpWhen : OW_OPC_JmpUnls, lbl_end);
	ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
	ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->rhs);
	ow_assembler_place_label(as, lbl_end);
	if (ow_unlikely(action == ACT_EVAL))
		ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
}

static void ow_codegen_emit_AndExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_AndExpr *node) {
	ow_codegen_emit_logical_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, false);
}

static void ow_codegen_emit_OrExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_OrExpr *node) {
	ow_codegen_emit_logical_BinOpExpr(
		codegen, action, (const struct ow_ast_BinOpExpr *)node, true);
}

static void ow_codegen_emit_AttrAccessExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_AttrAccessExpr *node) {
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);

	ow_codegen_emit_node(
		codegen, ACT_PUSH, (const struct ow_ast_node *)node->lhs);

	if (ow_unlikely(node->rhs->type != OW_AST_NODE_Identifier))
		ow_codegen_error_throw(codegen, &node->location, "illegal attribute accessing");
	struct ow_sharedstr *const attr_name =
		((const struct ow_ast_Identifier *)node->rhs)->value;
	const size_t sym_idx = ow_assembler_symbol(
		as, ow_sharedstr_data(attr_name), ow_sharedstr_size(attr_name));

	enum ow_opcode opcode;
	union ow_operand operand;
	if (action == ACT_PUSH || ow_unlikely(action == ACT_EVAL)) {
		if (sym_idx <= UINT8_MAX)
			opcode = OW_OPC_LdAttrY, operand.u8 = (uint8_t)sym_idx;
		else if (sym_idx <= UINT16_MAX)
			opcode = OW_OPC_LdAttrYW, operand.u16 = (uint16_t)sym_idx;
		else
			ow_unreachable();
		if (ow_unlikely(action == ACT_EVAL))
			ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
	} else if (action == ACT_RECV) {
		if (sym_idx <= UINT8_MAX)
			opcode = OW_OPC_StAttrY, operand.u8 = (uint8_t)sym_idx;
		else if (sym_idx <= UINT16_MAX)
			opcode = OW_OPC_StAttrYW, operand.u16 = (uint16_t)sym_idx;
		else
			ow_unreachable();
	} else {
		ow_unreachable();
	}
	ow_assembler_append(as, opcode, operand);
}

static void ow_codegen_emit_MethodUseExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_MethodUseExpr *node) {
	ow_unused_var(action);
	assert(action == ACT_PUSH);

	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);

	ow_codegen_emit_node(
		codegen, ACT_PUSH, (const struct ow_ast_node *)node->lhs);

	if (ow_unlikely(node->rhs->type != OW_AST_NODE_Identifier))
		ow_codegen_error_throw(codegen, &node->location, "illegal method accessing");
	struct ow_sharedstr *const name =
		((const struct ow_ast_Identifier *)node->rhs)->value;
	const size_t sym_idx = ow_assembler_symbol(
		as, ow_sharedstr_data(name), ow_sharedstr_size(name));

	enum ow_opcode opcode;
	union ow_operand operand;
	if (sym_idx <= UINT8_MAX)
		opcode = OW_OPC_PrepMethY, operand.u8 = (uint8_t)sym_idx;
	else if (sym_idx <= UINT16_MAX)
		opcode = OW_OPC_PrepMethYW, operand.u16 = (uint16_t)sym_idx;
	else
		ow_unreachable();
	ow_assembler_append(as, opcode, operand);
}

ow_noinline static void ow_codegen_emit_UnOpExpr(
	struct ow_codegen *codegen, enum codegen_action action,
	const struct ow_ast_UnOpExpr *node, enum ow_opcode opcode) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->val);
	ow_assembler_append(as, opcode, (union ow_operand){.u8 = 0});
	if (ow_unlikely(action == ACT_EVAL))
		ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
}

static void ow_codegen_emit_PosExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_PosExpr *node) {
	ow_unused_var(action);
	ow_codegen_error_throw_not_implemented(codegen, &node->location);
}

static void ow_codegen_emit_NegExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_NegExpr *node) {
	ow_codegen_emit_UnOpExpr(
		codegen, action, (const struct ow_ast_UnOpExpr *)node, OW_OPC_Neg);
}

static void ow_codegen_emit_BitNotExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BitNotExpr *node) {
	ow_codegen_emit_UnOpExpr(
		codegen, action, (const struct ow_ast_UnOpExpr *)node, OW_OPC_Inv);
}

static void ow_codegen_emit_NotExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_NotExpr *node) {
	ow_codegen_emit_UnOpExpr(
		codegen, action, (const struct ow_ast_UnOpExpr *)node, OW_OPC_Not);
}

ow_noinline static void ow_codegen_emit_MakeContainerExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_node_array *elems, const struct ow_source_range *location,
		enum ow_opcode opcode, enum ow_opcode opcode_w) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	const size_t elem_count = ow_ast_node_array_size(elems);
	for (size_t i = 0; i < elem_count; i++)
		ow_codegen_emit_node(codegen, ACT_PUSH, ow_ast_node_array_at(elems, i));
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	if (elem_count <= UINT8_MAX)
		ow_assembler_append(as, opcode, (union ow_operand){.u8 = (uint8_t)elem_count});
	else if (elem_count <= UINT16_MAX)
		ow_assembler_append(as, opcode_w, (union ow_operand){.u16 = (uint16_t)elem_count});
	else
		ow_codegen_error_throw(codegen, location, "too many elements");
	if (action != ACT_PUSH)
		ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
}

ow_noinline static void ow_codegen_emit_MakePairContainerExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_nodepair_array *elems, const struct ow_source_range *location,
		enum ow_opcode opcode, enum ow_opcode opcode_w) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	const size_t elem_count = ow_ast_nodepair_array_size(elems);
	for (size_t i = 0; i < elem_count; i++) {
		const struct ow_ast_nodepair_array_elem pair = ow_ast_nodepair_array_at(elems, i);
		ow_codegen_emit_node(codegen, ACT_PUSH, pair.first);
		ow_codegen_emit_node(codegen, ACT_PUSH, pair.second);
	}
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	if (elem_count <= UINT8_MAX)
		ow_assembler_append(as, opcode, (union ow_operand){.u8 = (uint8_t)elem_count});
	else if (elem_count <= UINT16_MAX)
		ow_assembler_append(as, opcode_w, (union ow_operand){.u16 = (uint16_t)elem_count});
	else
		ow_codegen_error_throw(codegen, location, "too many elements");
	if (action != ACT_PUSH)
		ow_assembler_append(as, OW_OPC_Drop, (union ow_operand){.u8 = 0});
}

static void ow_codegen_emit_TupleExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_TupleExpr *node) {
	if (action != ACT_RECV) {
		ow_codegen_emit_MakeContainerExpr(
			codegen, action, &node->elems, &node->location,
			OW_OPC_MkTup, OW_OPC_MkTupW);
		return;
	}
	// TODO: Unpacking.
	ow_codegen_error_throw_not_implemented(codegen, &node->location);

}

static void ow_codegen_emit_ArrayExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ArrayExpr *node) {
	ow_codegen_emit_MakeContainerExpr(
		codegen, action, &node->elems, &node->location, OW_OPC_MkArr, OW_OPC_MkArrW);
}

static void ow_codegen_emit_SetExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_SetExpr *node) {
	ow_codegen_emit_MakeContainerExpr(
		codegen, action, &node->elems, &node->location, OW_OPC_MkSet, OW_OPC_MkSetW);
}

static void ow_codegen_emit_MapExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_MapExpr *node) {
	ow_codegen_emit_MakePairContainerExpr(
		codegen, action, &node->pairs, &node->location, OW_OPC_MkMap, OW_OPC_MkMapW);
}

static void ow_codegen_emit_CallExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_CallExpr *node) {
	assert(action == ACT_PUSH || action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	size_t arg_cnt = ow_ast_node_array_size(&node->args);
	if (node->obj->type == OW_AST_NODE_MethodUseExpr) {
		ow_codegen_emit_MethodUseExpr(
			codegen, ACT_PUSH, (const struct ow_ast_MethodUseExpr *)node->obj);
		arg_cnt++;
	} else {
		ow_codegen_emit_node(
			codegen, ACT_PUSH, (const struct ow_ast_node *)node->obj);
	}
	for (size_t i = 0, n = ow_ast_node_array_size(&node->args); i < n; i++) {
		ow_codegen_emit_node(
			codegen, ACT_PUSH, ow_ast_node_array_at(
				(struct ow_ast_node_array *)&node->args, i));
	}
	if (ow_unlikely(arg_cnt > (UINT8_MAX >> 1)))
		ow_codegen_error_throw(codegen, &node->location, "too many arguments");
	const uint8_t operand = (uint8_t)arg_cnt | (action == ACT_EVAL ? (1 << 7) : 0);
	ow_assembler_append(as, OW_OPC_Call, (union ow_operand){.u8 = operand});
}

static void ow_codegen_emit_SubscriptExpr(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_SubscriptExpr *node) {
	ow_unused_var(action);
	ow_codegen_error_throw_not_implemented(codegen, &node->location);
}

static void ow_codegen_emit_ExprStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ExprStmt *node) {
	assert(action == ACT_EVAL);
	ow_codegen_emit_node(
		codegen, action, (const struct ow_ast_node *)node->expr);
}

static void ow_codegen_emit_BlockStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_BlockStmt *node) {
	ow_unused_var(action);
	assert(action == ACT_EVAL);
	for (size_t i = 0, n = ow_ast_node_array_size(&node->stmts); i < n; i++) {
		struct ow_ast_node *stmt =
			ow_ast_node_array_at((struct ow_ast_node_array *)&node->stmts, i);
		ow_codegen_emit_node(codegen, ACT_EVAL, stmt);
	}
}

static void ow_codegen_emit_ReturnStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ReturnStmt *node) {
	if (ow_unlikely(scope_stack_top(&codegen->scope_stack)->type != SCOPE_FUNC
			&& node->type != OW_AST_NODE_MagicReturnStmt))
		ow_codegen_error_throw(codegen, &node->location, "unexpected return statement");

	ow_unused_var(action);
	assert(action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	if (node->ret_val) {
		do {
			if (node->ret_val->type != OW_AST_NODE_Identifier)
				break;
			struct ow_sharedstr *const name =
				((struct ow_ast_Identifier *)node->ret_val)->value;
			struct scope *const scope = scope_stack_top(&codegen->scope_stack);
			if (scope->type != SCOPE_FUNC)
				break;
			const size_t index = scope_find_variable(scope, name);
			if (index >= UINT8_MAX)
				break;
			ow_assembler_append(as, OW_OPC_RetLoc, (union ow_operand){.u8 = (uint8_t)index});
			return;
		} while (0);
		ow_codegen_emit_node(codegen, ACT_PUSH, (struct ow_ast_node *)node->ret_val);
		ow_assembler_append(as, OW_OPC_RetLoc, (union ow_operand){.u8 = UINT8_MAX});
	} else {
		ow_assembler_append(as, OW_OPC_Ret, (union ow_operand){.u8 = 0});
	}
}

static void ow_codegen_emit_MagicReturnStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_MagicReturnStmt *node) {
	ow_codegen_emit_ReturnStmt(
		codegen, action, (const struct ow_ast_ReturnStmt *)node);
}

static void ow_codegen_emit_ImportStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ImportStmt *node) {
	assert(action == ACT_EVAL);
	ow_unused_var(action);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	const size_t index = ow_assembler_symbol(
		as, ow_sharedstr_data(node->mod_name->value),
		ow_sharedstr_size(node->mod_name->value));
	if (index > UINT16_MAX)
		ow_codegen_error_throw(codegen, &node->location, "too many symbols");
	ow_assembler_append(as, OW_OPC_LdMod, (union ow_operand){.u16 = (uint16_t)index});
	ow_codegen_emit_Identifier(codegen, ACT_RECV, node->mod_name);
}

static void ow_codegen_emit_IfElseStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_IfElseStmt *node) {
	ow_unused_var(action);
	assert(action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);

	int lbl_next_branch;
	const int lbl_end = ow_assembler_prepare_label(as);
	for (size_t i = 0, i_max = ow_ast_nodepair_array_size(&node->branches) - 1;
			i <= i_max; i++) {
		if (i) // Exclude the first branch.
			ow_assembler_place_label(as, lbl_next_branch);
		lbl_next_branch = ow_assembler_prepare_label(as);
		const struct ow_ast_nodepair_array_elem branch =
			ow_ast_nodepair_array_at((struct ow_ast_nodepair_array *)&node->branches, i);
		ow_codegen_emit_node(codegen, ACT_PUSH, branch.first);
		ow_assembler_append_jump(as, OW_OPC_JmpUnls, lbl_next_branch);
		assert(branch.second->type == OW_AST_NODE_BlockStmt);
		ow_codegen_emit_BlockStmt(
			codegen, ACT_EVAL, (struct ow_ast_BlockStmt *)branch.second);
		if (i < i_max || node->else_branch) // Exclude the last branch.
			ow_assembler_append_jump(as, OW_OPC_Jmp, lbl_end);
	}
	assert(ow_ast_nodepair_array_size(&node->branches));
	ow_assembler_place_label(as, lbl_next_branch);
	if (node->else_branch) {
		ow_codegen_emit_BlockStmt(codegen, ACT_EVAL, node->else_branch);
	}
	ow_assembler_place_label(as, lbl_end);
}

static void ow_codegen_emit_ForStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_ForStmt *node) {
	ow_unused_var(action);
	ow_codegen_error_throw_not_implemented(codegen, &node->location);
}

static void ow_codegen_emit_WhileStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_WhileStmt *node) {
	ow_unused_var(action);
	assert(action == ACT_EVAL);
	struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
	bool infinite_loop;

	if (node->cond->type == OW_AST_NODE_BoolLiteral) {
		if (((struct ow_ast_BoolLiteral *)node->cond)->value)
			infinite_loop = true;
		else
			return;
	} else {
		infinite_loop = false;
	}

	const int lbl_begin = ow_assembler_place_label(as, -1);
	const int lbl_end = ow_assembler_prepare_label(as);
	if (!infinite_loop) {
		ow_codegen_emit_node(
			codegen, ACT_PUSH, (const struct ow_ast_node *)node->cond);
		ow_assembler_append_jump(as, OW_OPC_JmpUnls, lbl_end);
	}
	ow_codegen_emit_BlockStmt(
		codegen, ACT_EVAL, (const struct ow_ast_BlockStmt *)node);
	ow_assembler_append_jump(as, OW_OPC_Jmp, lbl_begin);
	if (!infinite_loop)
		ow_assembler_place_label(as, lbl_end);
}

static void _scope_load_func_args(
		struct scope *scope, struct ow_codegen *codegen,
		struct ow_ast_node_array *args) {
	assert(scope->type == SCOPE_ARGS);
	for (size_t i = 0, n = ow_ast_node_array_size(args); i < n; i++) {
		struct ow_ast_node *const elem = ow_ast_node_array_at(args, i);
		if (elem->type != OW_AST_NODE_Identifier) {
			ow_codegen_error_throw(
				codegen, &elem->location, "illegal formal parameter");
		}
		struct ow_ast_Identifier *const arg_name = (struct ow_ast_Identifier *)elem;
		scope_register_variable(scope, arg_name->value);
	}
}

static void ow_codegen_emit_FuncStmt(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_FuncStmt *node) {
	ow_unused_var(action);
	assert(action == ACT_EVAL);
	if (ow_unlikely(ow_ast_node_array_size(&node->args->elems) >= INT8_MAX))
		ow_codegen_error_throw(codegen, &node->args->location, "too many arguments");
	struct ow_func_obj *func;
	{
		struct ow_assembler *const as =
			code_stack_push(&codegen->code_stack, codegen->machine);
		struct scope *const args_scope =
			scope_stack_push(&codegen->scope_stack, SCOPE_ARGS);
		struct scope *const func_scope =
			scope_stack_push(&codegen->scope_stack, SCOPE_FUNC);
		_scope_load_func_args(args_scope, codegen, &node->args->elems);
		ow_unused_var(func_scope);

		ow_codegen_emit_BlockStmt(
			codegen, ACT_EVAL, (const struct ow_ast_BlockStmt *)node);
		const enum ow_opcode last_opcode = ow_assembler_last(as);
		if (!(last_opcode == OW_OPC_Ret || last_opcode == OW_OPC_RetLoc))
			ow_assembler_append(as, OW_OPC_Ret, (union ow_operand){.u8 = 0});

		const struct ow_assembler_output_spec as_output_spec = {
			codegen->module,
			(struct ow_func_spec){
				.arg_cnt = (int)ow_ast_node_array_size(&node->args->elems),
				.local_cnt = (unsigned int)ow_hashmap_size(&func_scope->_variables_map),
			},
		};

		assert(code_stack_top(&codegen->code_stack) == as);
		func = code_stack_make_func_and_pop(&codegen->code_stack, &as_output_spec);
		assert(scope_stack_top(&codegen->scope_stack) == func_scope);
		scope_stack_pop(&codegen->scope_stack);
		assert(scope_stack_top(&codegen->scope_stack) == args_scope);
		scope_stack_pop(&codegen->scope_stack);
	}

	{
		struct ow_assembler *const as = code_stack_top(&codegen->code_stack);
		const size_t const_index = ow_assembler_constant(
			as, (struct ow_assembler_constant){
				.type = OW_AS_CONST_OBJ, .o = ow_object_from(func)});
		ow_codegen_asm_push_constant(
			codegen, (const struct ow_ast_node *)node, as, const_index);
		ow_codegen_emit_Identifier(codegen, ACT_RECV, node->name);
	}

#if OW_DEBUG_CODEGEN
	if (ow_unlikely(codegen->verbose)) {
		const char *const name = ow_sharedstr_data(node->name->value);
		verbose_dump_func(node->location.begin.line, name, func);
	}
#endif // OW_DEBUG_CODEGEN
}

static int _scope_load_module_globals_walker(
		void *_ctx, struct ow_symbol_obj *name, size_t index, struct ow_object *value) {
	ow_unused_var(value);
	struct ow_symbol_obj **names = _ctx;
	names[index] = name;
	return 0;
}

static void _scope_load_module_globals(
		struct scope *scope, struct ow_module_obj *module) {
	assert(scope->type == SCOPE_MODULE);
	const size_t count = ow_module_obj_global_count(module);
	if (!count)
		return;
	struct ow_symbol_obj **names = ow_malloc(count * sizeof(void *));
	ow_module_obj_foreach_global(module, _scope_load_module_globals_walker, names);
	for (size_t i = 0; i < count; i++) {
		struct ow_symbol_obj *const sym = names[i];
		const char *const str = ow_symbol_obj_data(sym);
		const size_t len = ow_symbol_obj_size(sym);
		struct ow_sharedstr *const ss = ow_sharedstr_new(str, len);
		const size_t idx = scope_register_variable(scope, ss);
		ow_unused_var(idx);
		assert(idx == i);
		ow_sharedstr_unref(ss);
	}
	ow_free(names);
}

struct _scope_update_module_globals_context {
	size_t orig_count;
	struct ow_sharedstr *names[];
};

static int _scope_update_module_globals_walker(
	void *_ctx, struct ow_sharedstr *name, size_t index) {
	struct _scope_update_module_globals_context *const ctx = _ctx;
	if (index >= ctx->orig_count)
		ctx->names[index] = name;
	return 0;
}

static void _scope_update_module_globals(
		struct scope *scope, struct ow_machine *om, struct ow_module_obj *module) {
	assert(scope->type == SCOPE_MODULE);
	const size_t orig_count = ow_module_obj_global_count(module);
	const size_t total_count = ow_hashmap_size(&scope->_variables_map);
	assert(orig_count <= total_count);
	struct _scope_update_module_globals_context *const ctx =
		ow_malloc(sizeof(struct _scope_update_module_globals_context)
			+ (total_count - orig_count) * sizeof(void *));
	ctx->orig_count = orig_count;
	scope_foreach_variable(scope, _scope_update_module_globals_walker, ctx);
	struct ow_object *const nil = om->globals->value_nil;
	for (size_t i = orig_count; i < total_count; i++) {
		struct ow_sharedstr *const ss = ctx->names[i - orig_count];
		struct ow_symbol_obj *const sym =
			ow_symbol_obj_new(om, ow_sharedstr_data(ss), ow_sharedstr_size(ss));
		const size_t idx = ow_module_obj_set_global_y(module, sym, nil);
		ow_unused_var(idx);
		assert(idx == i);
	}
	ow_free(ctx);
}

static void ow_codegen_emit_Module(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_Module *node) {
	ow_unused_var(action);
	assert(action == ACT_EVAL);

	struct ow_assembler *const as =
		code_stack_push(&codegen->code_stack, codegen->machine);
	struct scope *const scope =
		scope_stack_push(&codegen->scope_stack, SCOPE_MODULE);
	_scope_load_module_globals(scope, codegen->module);

	ow_codegen_emit_BlockStmt(codegen, ACT_EVAL, node->code);
	ow_assembler_append(as, OW_OPC_Ret, (union ow_operand){.u8 = 0});

	ow_unused_var(scope);
	assert(code_stack_top(&codegen->code_stack) == as);
	assert(scope_stack_top(&codegen->scope_stack) == scope);

	struct ow_func_obj *const func = code_stack_make_func_and_pop(
		&codegen->code_stack, &(struct ow_assembler_output_spec){
			codegen->module, (struct ow_func_spec){0, 0}});
	_scope_update_module_globals(scope, codegen->machine, codegen->module);
	scope_stack_pop(&codegen->scope_stack);

	ow_objmem_push_ngc(codegen->machine);
	struct ow_symbol_obj *const func_name = codegen->machine->common_symbols->anon;
	ow_module_obj_set_global_y(codegen->module, func_name, ow_object_from(func));
	ow_objmem_pop_ngc(codegen->machine);

#if OW_DEBUG_CODEGEN
	if (ow_unlikely(codegen->verbose))
		verbose_dump_func(node->location.begin.line, "(module)", func);
#endif // OW_DEBUG_CODEGEN
}

static void ow_codegen_emit_node(
		struct ow_codegen *codegen, enum codegen_action action,
		const struct ow_ast_node *node) {
	switch (node->type) {
#define ELEM(NAME) case OW_AST_NODE_##NAME : \
		ow_codegen_emit_##NAME( \
			codegen, action, (struct ow_ast_##NAME *)node); \
		break;
	OW_AST_NODE_LIST
#undef ELEM
	default:
		ow_unreachable();
	}
}

bool ow_codegen_generate(
		struct ow_codegen *codegen,
		const struct ow_ast *ast, int flags,
		struct ow_module_obj *module) {
	ow_unused_var(flags);

	ow_codegen_clear(codegen);
	codegen->module = module;

	if (!ow_codegen_error_setjmp(codegen)) {
		ow_codegen_emit_Module(codegen, ACT_EVAL, ow_ast_get_module(ast));
		codegen->module = NULL;
		return true;
	} else {
		assert(ow_codegen_error(codegen));
		codegen->module = NULL;
		return false;
	}
}

struct ow_syntax_error *ow_codegen_error(struct ow_codegen *codegen) {
	if (!codegen->error_info.message)
		return NULL;
	return &codegen->error_info;
}
