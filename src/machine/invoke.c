#include "invoke.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "machine.h"
#include "symbols.h"
#include <bytecode/opcode.h>
#include <bytecode/operand.h>
#include <objects/cfuncobj.h>
#include <objects/classes.h>
#include <objects/classobj.h>
#include <objects/exceptionobj.h>
#include <objects/floatobj.h>
#include <objects/funcobj.h>
#include <objects/intobj.h>
#include <objects/memory.h>
#include <objects/moduleobj.h>
#include <objects/object.h>
#include <objects/smallint.h>
#include <objects/stringobj.h>
#include <objects/symbolobj.h>
#include <utilities/attributes.h>
#include <utilities/unreachable.h>

/// Format an error.
#ifdef __GNUC__
__attribute__((cold))
#endif // __GNUC__
ow_noinline ow_nodiscard static struct ow_exception_obj *invoke_impl_make_error(
		struct ow_machine *om, struct ow_class_obj *exc_type, const char *fmt, ...) {
	va_list ap;
	char buf[64];
	va_start(ap, fmt);
	const int n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	assert(n >= 0);

	ow_objmem_push_ngc(om);
	struct ow_object *const msg_o =
		ow_object_from(ow_string_obj_new(om, buf, (size_t)n));
	struct ow_exception_obj *const exc_o = ow_exception_new(om, exc_type, msg_o);
	ow_objmem_pop_ngc(om);

	return exc_o;
}

/// Check number of arguments. If there is no error, return NULL.
ow_nodiscard ow_forceinline static struct ow_exception_obj *invoke_impl_check_argc(
		struct ow_machine *om, const struct ow_func_spec *func_spec, size_t argc) {
	assert(argc <= INT_MAX);
	const int expected_argc = func_spec->arg_cnt;
	if (ow_likely(expected_argc >= 0)) {
		if (ow_likely((unsigned int)argc == (unsigned int)expected_argc))
			return NULL;
		return invoke_impl_make_error(om, NULL, "too %s arguments",
			(unsigned int)argc < (unsigned int)expected_argc ? "few" : "many");
	} else {
		const unsigned int argc_min = (unsigned int)OW_NATIVE_FUNC_VARIADIC_ARGC(argc);
		if (ow_likely((unsigned int)argc >= argc_min))
			return NULL;
		return invoke_impl_make_error(om, NULL, "too few arguments");
	}
}

/// Try to call `__find_attr__()` to get attribute.
ow_nodiscard ow_noinline static int invoke_impl_do_find_attribute(
		struct ow_machine *om, struct ow_object *obj, struct ow_class_obj *obj_class,
		struct ow_symbol_obj *name, struct ow_object **result) {
	const size_t find_attr_index =
		ow_class_obj_find_method(obj_class, om->common_symbols->find_attr);
	if (ow_likely(find_attr_index != (size_t)-1)) {
		struct ow_object *const find_attr =
			ow_class_obj_get_method(obj_class, find_attr_index);
		struct ow_object *argv[2] = {obj, ow_object_from(name)};
		return ow_machine_call(om, find_attr, 2, argv, result);
	}
	*result = ow_object_from(invoke_impl_make_error(
		om, NULL, "`%s' object has not attribute `%s'",
		ow_symbol_obj_data(_ow_class_obj_pub_info(obj_class)->class_name),
		ow_symbol_obj_data(name)));
	return -1;
}

/// Try to call `__find_meth__()` to get method.
ow_nodiscard ow_noinline static int invoke_impl_do_find_method(
		struct ow_machine *om, struct ow_object *obj, struct ow_class_obj *obj_class,
		struct ow_symbol_obj *name, struct ow_object **result) {
	const size_t find_meth_index =
		ow_class_obj_find_method(obj_class, om->common_symbols->find_meth);
	if (ow_likely(find_meth_index != (size_t)-1)) {
		struct ow_object *const find_meth =
			ow_class_obj_get_method(obj_class, find_meth_index);
		struct ow_object *argv[2] = {obj, ow_object_from(name)};
		return ow_machine_call(om, find_meth, 2, argv, result);
	}
	*result = ow_object_from(invoke_impl_make_error(
		om, NULL, "`%s' object has not method `%s'",
		ow_symbol_obj_data(_ow_class_obj_pub_info(obj_class)->class_name),
		ow_symbol_obj_data(name)));
	return -1;
}

/// Get method by name. If cannot find the method, make an error and return false.
ow_nodiscard ow_noinline static bool invoke_impl_get_method_y(
		struct ow_machine *om, struct ow_object *obj,
		struct ow_symbol_obj *name, struct ow_object **result) {
	struct ow_class_obj *obj_class;
	if (ow_unlikely(ow_smallint_check(obj)))
		obj_class = om->builtin_classes->int_;
	else
		obj_class = ow_object_class(obj);
	const size_t index = ow_class_obj_find_method(obj_class, name);
	if (ow_likely(index != (size_t)-1)) {
		*result = ow_class_obj_get_method(obj_class, index);
		return true;
	}
	return invoke_impl_do_find_method(om, obj, obj_class, name, result) == 0;
}

#ifdef __GNUC__
__attribute__((hot))
#endif // __GNUC__
static int invoke_impl(
		struct ow_machine *machine, int _argc, struct ow_object **_res_out) {
	register const unsigned char *ip;
	register struct ow_callstack_regs stack;
	struct ow_func_obj *current_func_obj;
	struct ow_callstack_frame_info *current_frame;
	struct ow_module_obj *current_module;
	struct ow_machine_globals *const machine_globals = machine->globals;
	struct ow_builtin_classes *const builtin_classes = machine->builtin_classes;
	struct ow_common_symbols *const common_symbols = machine->common_symbols;

#define STACK_COMMIT()     (machine->callstack.regs = stack)
#define STACK_UPDATE()     (stack = machine->callstack.regs)
#define STACK_ASSERT_NC()  \
	assert(stack.sp == machine->callstack.regs.sp && stack.fp == machine->callstack.regs.fp)

	ip = NULL;
	STACK_UPDATE();
	current_func_obj = NULL;
	current_module = NULL;
	current_frame = machine->callstack.frame_info_list.current;

	goto start;

	while (1) {
		switch ((enum ow_opcode)*ip++) {
			typedef int8_t   _operand_type_i8;
			typedef uint8_t  _operand_type_u8;
			typedef int16_t  _operand_type_i16;
			typedef uint16_t _operand_type_u16;

			register union {
				int_fast8_t   i8;
				uint_fast8_t  u8;
				int_fast16_t  i16;
				uint_fast16_t u16;
				size_t        index;
				size_t        count;
				ptrdiff_t     ptrdiff;
				void         *pointer;
			} operand;

#define OP_BEGIN(NAME)     case OW_OPC_##NAME : {
#define OP_END             } continue;
#define OPERAND_(TYPE, TO) { (TO) = *(_operand_type_##TYPE *)ip; }
#define OPERAND(TYPE, TO)  { OPERAND_(TYPE, TO); ip += sizeof(_operand_type_##TYPE); }
#define NO_OPERAND()       { }

#define DO_CALL(ARGC)      do { operand.u8 = (uint8_t)(ARGC); goto op_Call_1; } while (0)

		start:
			assert(_argc < (UINT8_MAX >> 1));
			DO_CALL(_argc);

		OP_BEGIN(Nop)
			NO_OPERAND()
		OP_END

		OP_BEGIN(Trap)
			NO_OPERAND()
			goto err_not_implemented;
		OP_END

		OP_BEGIN(Swap)
			NO_OPERAND()
			struct ow_object *const top = stack.sp[0];
			stack.sp[0] = stack.sp[-1];
			stack.sp[-1] = top;
		OP_END

		OP_BEGIN(SwapN)
			OPERAND(u8, operand.count)
			if (ow_unlikely(!operand.count))
				break;
			struct ow_object *const top = stack.sp[0];
			struct ow_object **const p_end = stack.sp - operand.count + 1;
			for (struct ow_object **p = stack.sp; p > p_end; p--)
				p[0] = p[-1];
			*p_end = top;
		OP_END

		OP_BEGIN(Drop)
			NO_OPERAND()
			stack.sp--;
		OP_END

		OP_BEGIN(DropN)
			OPERAND(u8, operand.count)
			stack.sp -= operand.count;
		OP_END

		OP_BEGIN(Dup)
			NO_OPERAND()
			struct ow_object *const top = stack.sp[0];
			*++stack.sp = top;
		OP_END

		OP_BEGIN(DupN)
			OPERAND(u8, operand.count)
			struct ow_object *const top = stack.sp[0];
			while (operand.count--)
				*++stack.sp = top;
		OP_END

		OP_BEGIN(LdNil)
			NO_OPERAND()
			*++stack.sp = machine_globals->value_nil;
		OP_END

		OP_BEGIN(LdBool)
			OPERAND(u8, operand.u8)
			*++stack.sp = operand.u8 ?
				machine_globals->value_true : machine_globals->value_false;
		OP_END

		OP_BEGIN(LdInt)
			OPERAND(i8, operand.i8)
			*++stack.sp = ow_smallint_to_ptr(operand.i8);
		OP_END

		OP_BEGIN(LdIntW)
			OPERAND(i16, operand.i16)
			*++stack.sp = ow_smallint_to_ptr(operand.i16);
		OP_END

		OP_BEGIN(LdFlt)
			OPERAND(i8, operand.i8)
			STACK_COMMIT();
			*++stack.sp =
				ow_object_from(ow_float_obj_new(machine, (double)operand.i8));
			STACK_ASSERT_NC();
		OP_END

#define IMPL_BIN_OP(OPERATOR, METH_NAME) \
	struct ow_object *const lhs = stack.sp[-1]; \
	struct ow_object *const rhs = stack.sp[0]; \
	if (ow_smallint_check(lhs) && ow_smallint_check(rhs)) { \
		const ow_smallint_t res = \
			ow_smallint_from_ptr(lhs) OPERATOR ow_smallint_from_ptr(rhs); \
		*--stack.sp = ow_int_obj_or_smallint(machine, res); \
	} else { \
		*stack.sp = lhs; \
		*++stack.sp = rhs; \
		STACK_COMMIT(); \
		const bool ok = invoke_impl_get_method_y( \
			machine, lhs, common_symbols-> METH_NAME , stack.sp - 2); \
		STACK_ASSERT_NC(); \
		if (ow_unlikely(!ok)) { \
			stack.sp -= 2; \
			goto raise_exc; \
		} \
		DO_CALL(2); \
	} \
// ^^^ IMPL_BIN_OP() ^^^

		OP_BEGIN(Add)
			NO_OPERAND()
			IMPL_BIN_OP(+, add)
		OP_END

		OP_BEGIN(Sub)
			NO_OPERAND()
			IMPL_BIN_OP(-, sub)
		OP_END

		OP_BEGIN(Mul)
			NO_OPERAND()
			IMPL_BIN_OP(*, mul)
		OP_END

		OP_BEGIN(Div)
			NO_OPERAND()
			IMPL_BIN_OP(/, div)
		OP_END

		OP_BEGIN(Rem)
			NO_OPERAND()
			IMPL_BIN_OP(%, rem)
		OP_END

		OP_BEGIN(Shl)
			NO_OPERAND()
			IMPL_BIN_OP(<<, shl)
		OP_END

		OP_BEGIN(Shr)
			NO_OPERAND()
			IMPL_BIN_OP(>>, shr)
		OP_END

		OP_BEGIN(And)
			NO_OPERAND()
			IMPL_BIN_OP(&, and_)
		OP_END

		OP_BEGIN(Or)
			NO_OPERAND()
			IMPL_BIN_OP(|, or_)
		OP_END

		OP_BEGIN(Xor)
			NO_OPERAND()
			IMPL_BIN_OP(^, xor_)
		OP_END

#undef IMPL_BIN_OP

#define IMPL_UN_OP(OPERATOR, METH_NAME) \
	struct ow_object *const val = stack.sp[0]; \
	if (ow_smallint_check(val)) { \
		const ow_smallint_t res = OPERATOR ow_smallint_from_ptr(val); \
		*--stack.sp = ow_int_obj_or_smallint(machine, res); \
	} else { \
		*++stack.sp = val; \
		STACK_COMMIT(); \
		const bool ok = invoke_impl_get_method_y( \
			machine, val, common_symbols-> METH_NAME , stack.sp - 1); \
		STACK_ASSERT_NC(); \
		if (ow_unlikely(!ok)) { \
			stack.sp--; \
			goto raise_exc; \
		} \
		DO_CALL(1); \
	} \
// ^^^ IMPL_UN_OP() ^^^

		OP_BEGIN(Neg)
			NO_OPERAND()
			IMPL_UN_OP(-, neg)
		OP_END

		OP_BEGIN(Inv)
			NO_OPERAND()
			IMPL_UN_OP(~, inv)
		OP_END

		OP_BEGIN(Not)
			NO_OPERAND()
			struct ow_object *obj= *stack.sp;
			if (obj == machine_globals->value_true)
				obj = machine_globals->value_false;
			else if (obj != machine_globals->value_false)
				obj = machine_globals->value_true;
			else
				goto err_cond_is_not_bool;
			*stack.sp = obj;
		OP_END

		OP_BEGIN(Test)
			goto err_not_implemented;
		OP_END

		OP_BEGIN(Is)
			NO_OPERAND()
			const bool is_same = stack.sp[0] == stack.sp[-1];
			stack.sp--;
			stack.sp[0] = is_same ?
				machine_globals->value_true : machine_globals->value_false;
		OP_END

		OP_BEGIN(Cmp)
			NO_OPERAND()
			struct ow_object *const lhs = stack.sp[-1];
			struct ow_object *const rhs = stack.sp[0];
			if (ow_smallint_check(lhs) && ow_smallint_check(rhs)) {
				const ow_smallint_t lhs_v = ow_smallint_from_ptr(lhs);
				const ow_smallint_t rhs_v = ow_smallint_from_ptr(rhs);
				const ow_smallint_t res = lhs_v == rhs_v ? 0 : lhs_v < rhs_v ? -1 : 1;
				*--stack.sp = ow_smallint_to_ptr(res);
			} else {
				*stack.sp = lhs;
				*++stack.sp = rhs;
				STACK_COMMIT();
				const bool ok = invoke_impl_get_method_y(
					machine, lhs, common_symbols->cmp, stack.sp - 2);
				STACK_ASSERT_NC();
				if (ow_unlikely(!ok)) {
					stack.sp -= 2;
					goto raise_exc;
				}
				DO_CALL(2);
			}
		OP_END

#define IMPL_CMP_OP(OPERATOR) \
	struct ow_object *const lhs = stack.sp[-1]; \
	struct ow_object *const rhs = stack.sp[0]; \
	if (ow_smallint_check(lhs) && ow_smallint_check(rhs)) { \
		const ow_smallint_t lhs_v = ow_smallint_from_ptr(lhs); \
		const ow_smallint_t rhs_v = ow_smallint_from_ptr(rhs); \
		*--stack.sp = lhs_v OPERATOR rhs_v ? \
			machine_globals->value_true : machine_globals->value_false; \
	} else { \
		*stack.sp = lhs; \
		*++stack.sp = rhs; \
		STACK_COMMIT(); \
		const bool ok = invoke_impl_get_method_y( \
			machine, lhs, common_symbols->cmp, stack.sp - 2); \
		STACK_ASSERT_NC(); \
		if (ow_unlikely(!ok)) { \
			stack.sp -= 2; \
			goto raise_exc; \
		} \
		const int status = ow_machine_invoke(machine, 2, stack.sp - 2); \
		stack.sp -= 2; \
		if (ow_unlikely(status)) \
			goto raise_exc; \
		struct ow_object *const cmp_res_o = *stack.sp; \
		if (ow_unlikely(!ow_smallint_check(cmp_res_o))) { \
			*stack.sp = ow_object_from(invoke_impl_make_error( \
				machine, NULL, "wrong type of comparison result")); \
			goto raise_exc; \
		} \
		*stack.sp = (ow_smallint_from_ptr(cmp_res_o) OPERATOR 0) ? \
			machine_globals->value_true : machine_globals->value_false; \
	} \
// ^^^ IMPL_CMP_OP^^^

		OP_BEGIN(CmpLt)
			NO_OPERAND()
			IMPL_CMP_OP(<)
		OP_END

		OP_BEGIN(CmpLe)
			NO_OPERAND()
			IMPL_CMP_OP(<=)
		OP_END

		OP_BEGIN(CmpGt)
			NO_OPERAND()
			IMPL_CMP_OP(>)
		OP_END

		OP_BEGIN(CmpGe)
			NO_OPERAND()
			IMPL_CMP_OP(>=)
		OP_END

		OP_BEGIN(CmpEq)
			NO_OPERAND()
			IMPL_CMP_OP(==)
		OP_END

		OP_BEGIN(CmpNe)
			NO_OPERAND()
			IMPL_CMP_OP(!=)
		OP_END

#undef IMPL_CMP_OP

		OP_BEGIN(LdCnst)
			OPERAND(u8, operand.index)
		op_LdCnst_1:;
			struct ow_object *obj =
				ow_func_obj_get_constant(current_func_obj, operand.index);
			if (ow_unlikely(!obj))
				goto err_bad_operand;
			*++stack.sp = obj;
		OP_END

		OP_BEGIN(LdCnstW)
			OPERAND(u16, operand.index)
			goto op_LdCnst_1;
		OP_END

		OP_BEGIN(LdSym)
			OPERAND(u8, operand.index)
		op_LdSym_1:;
			struct ow_symbol_obj *sym =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!sym))
				goto err_bad_operand;
			*++stack.sp = ow_object_from(sym);
		OP_END

		OP_BEGIN(LdSymW)
			OPERAND(u16, operand.index)
			goto op_LdSym_1;
		OP_END

		OP_BEGIN(LdArg)
			OPERAND(u8, operand.index)
			struct ow_object **const p = current_frame->arg_list + operand.index;
			if (ow_unlikely(p >= stack.fp))
				goto err_bad_operand;
			*++stack.sp = *p;
		OP_END

		OP_BEGIN(StArg)
			OPERAND(u8, operand.index)
			struct ow_object **const p = current_frame->arg_list + operand.index;
			if (ow_unlikely(p >= stack.fp))
				goto err_bad_operand;
			*p = *stack.sp--;
		OP_END

		OP_BEGIN(LdLoc)
			OPERAND(u8, operand.index)
			struct ow_object **const p = stack.fp + operand.index;
			if (ow_unlikely(p <= stack.sp))
				goto err_bad_operand;
			*++stack.sp = *p;
		OP_END

		OP_BEGIN(LdLocW)
			OPERAND(u16, operand.index)
			struct ow_object **const p = stack.fp + operand.index;
			if (ow_unlikely(p <= stack.sp))
				goto err_bad_operand;
			*++stack.sp = *p;
		OP_END

		OP_BEGIN(StLoc)
			OPERAND(u8, operand.index)
			struct ow_object **const p = stack.fp + operand.index;
			if (ow_unlikely(p < stack.sp))
				goto err_bad_operand;
			*p = *stack.sp--;
		OP_END

		OP_BEGIN(StLocW)
			OPERAND(u16, operand.index)
			struct ow_object **const p = stack.fp + operand.index;
			if (ow_unlikely(p < stack.sp))
				goto err_bad_operand;
			*p = *stack.sp--;
		OP_END

		OP_BEGIN(LdGlob)
			OPERAND(u8, operand.index)
		op_LdGlob_1:;
			struct ow_object *obj =
				ow_module_obj_get_global(current_module, operand.index);
			if (ow_unlikely(!obj))
				obj = machine_globals->value_nil;
			*++stack.sp = obj;
		OP_END

		OP_BEGIN(LdGlobW)
			OPERAND(u16, operand.index)
			goto op_LdGlob_1;
		OP_END

		OP_BEGIN(StGlob)
			OPERAND(u8, operand.index)
		op_StGlob_1:;
			struct ow_object *const obj = *stack.sp--;
			ow_module_obj_set_global(current_module, operand.index, obj);
		OP_END

		OP_BEGIN(StGlobW)
			OPERAND(u16, operand.index)
			goto op_StGlob_1;
		OP_END

		OP_BEGIN(LdGlobY)
			OPERAND(u8, operand.index)
		op_LdGlobY_1:;
			struct ow_symbol_obj *name =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!name))
				goto err_bad_operand;
			struct ow_object *obj =
				ow_module_obj_get_global_y(current_module, name);
			if (!obj) {
				obj = ow_module_obj_get_global_y(machine_globals->module_base, name);
				if (ow_unlikely(!obj))
					obj = machine_globals->value_nil;
			}
			*++stack.sp = obj;
		OP_END

		OP_BEGIN(LdGlobYW)
			OPERAND(u16, operand.index)
			goto op_LdGlobY_1;
		OP_END

		OP_BEGIN(StGlobY)
			OPERAND(u8, operand.index)
		op_StGlobY_1:;
			struct ow_symbol_obj *name =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!name))
				goto err_bad_operand;
			struct ow_object *const obj = *stack.sp--;
			ow_module_obj_set_global_y(current_module, name, obj);
		OP_END

		OP_BEGIN(StGlobYW)
			OPERAND(u16, operand.index)
			goto op_StGlobY_1;
		OP_END

		OP_BEGIN(LdAttrY)
			OPERAND(u8, operand.index)
		op_LdAttrY_1:;
			struct ow_symbol_obj *name =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!name))
				goto err_bad_operand;
			struct ow_object *const obj = *stack.sp;
			struct ow_class_obj *obj_class;
			if (ow_unlikely(ow_smallint_check(obj)))
				obj_class = builtin_classes->int_;
			else
				obj_class = ow_object_class(obj);
			struct ow_object *attr;
			if (obj_class == builtin_classes->module) {
				attr = ow_module_obj_get_global_y(
					ow_object_cast(obj, struct ow_module_obj), name);
				if (ow_unlikely(!attr))
					attr = machine_globals->value_nil;
			} else {
				operand.index = ow_class_obj_find_attribute(obj_class, name);
				if (ow_likely(operand.index != (size_t)-1)) {
					attr = ow_object_get_field(obj, operand.index);
				} else {
					STACK_COMMIT();
					const int status = invoke_impl_do_find_attribute(
						machine, obj, obj_class, name, &attr);
					STACK_ASSERT_NC();
					if (ow_unlikely(status)) {
						*stack.sp = attr;
						goto raise_exc;
					}
				}
			}
			*stack.sp = attr;
		OP_END

		OP_BEGIN(LdAttrYW)
			OPERAND(u16, operand.index)
			goto op_LdAttrY_1;
		OP_END

		OP_BEGIN(StAttrY)
			OPERAND(u8, operand.index)
		op_StAttrY_1:;
			struct ow_symbol_obj *name =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!name))
				goto err_bad_operand;
			struct ow_object *const obj = *stack.sp--;
			struct ow_object *const attr = *stack.sp;
			struct ow_class_obj *obj_class;
			if (ow_unlikely(ow_smallint_check(obj)))
				obj_class = builtin_classes->int_;
			else
				obj_class = ow_object_class(obj);
			if (obj_class == builtin_classes->module) {
				ow_module_obj_set_global_y(
					ow_object_cast(obj, struct ow_module_obj), name, attr);
			} else {
				goto err_not_implemented;
			}
		OP_END

		OP_BEGIN(StAttrYW)
			OPERAND(u16, operand.index)
			goto op_StAttrY_1;
		OP_END

		OP_BEGIN(LdElem)
			goto err_not_implemented;
		OP_END

		OP_BEGIN(StElem)
			goto err_not_implemented;
		OP_END

		OP_BEGIN(Jmp)
			OPERAND_(i8, operand.ptrdiff)
			ip = ip - 1 + operand.ptrdiff;
		OP_END

		OP_BEGIN(JmpW)
			OPERAND_(i16, operand.ptrdiff)
			ip = ip - 1 + operand.ptrdiff;
		OP_END

		OP_BEGIN(JmpWhen)
			OPERAND_(i8, operand.ptrdiff)
			struct ow_object *const cond = *stack.sp--;
			if (cond == machine_globals->value_true)
				ip = ip - 1 + operand.ptrdiff;
			else if (ow_likely(cond == machine_globals->value_false))
				ip += 1;
			else
				goto err_cond_is_not_bool;
		OP_END

		OP_BEGIN(JmpWhenW)
			OPERAND_(i16, operand.ptrdiff)
			struct ow_object *const cond = *stack.sp--;
			if (cond == machine_globals->value_true)
				ip = ip - 1 + operand.ptrdiff;
			else if (ow_likely(cond == machine_globals->value_false))
				ip += 2;
			else
				goto err_cond_is_not_bool;
		OP_END

		OP_BEGIN(JmpUnls)
			OPERAND_(i8, operand.ptrdiff)
			struct ow_object *const cond = *stack.sp--;
			if (cond == machine_globals->value_false)
				ip = ip - 1 + operand.ptrdiff;
			else if (ow_likely(cond == machine_globals->value_true))
				ip += 1;
			else
				goto err_cond_is_not_bool;
		OP_END

		OP_BEGIN(JmpUnlsW)
			OPERAND_(i16, operand.ptrdiff)
			struct ow_object *const cond = *stack.sp--;
			if (cond == machine_globals->value_false)
				ip = ip - 1 + operand.ptrdiff;
			else if (ow_likely(cond == machine_globals->value_true))
				ip += 2;
			else
				goto err_cond_is_not_bool;
		OP_END

		OP_BEGIN(Ret)
			NO_OPERAND()
			operand.pointer = machine_globals->value_nil; // Return value.
		op_Ret_2:;
			ip = current_frame->prev_ip;
			stack.fp = current_frame->prev_fp;
			if (current_frame->not_ret_val || ow_unlikely(!ip)) {
				stack.sp = current_frame->arg_list - 2;
			} else {
				stack.sp = current_frame->arg_list - 1;
				*stack.sp = operand.pointer;
			}

			struct ow_callstack_frame_info_list *const frame_info_list =
				&machine->callstack.frame_info_list;
			ow_callstack_frame_info_list_leave(frame_info_list);

			if (ow_unlikely(!ip)) {
				STACK_COMMIT();
				*_res_out = operand.pointer;
				return 0;
			}

			current_frame = frame_info_list->current;
			assert(ow_object_class(*(current_frame->arg_list - 1))
				== builtin_classes->func);
			current_func_obj = ow_object_cast(
				(*(current_frame->arg_list - 1)), struct ow_func_obj);
			current_module = current_func_obj->module;
		OP_END

		OP_BEGIN(RetLoc)
			OPERAND(u8, operand.index)
			if (operand.index == UINT8_MAX)
				operand.pointer = machine_globals->value_nil;
			else if (ow_likely(stack.fp + operand.index < stack.sp))
				operand.pointer = stack.fp[operand.index];
			else
				goto err_bad_operand;
			goto op_Ret_2;
		OP_END

		OP_BEGIN(Call)
			OPERAND(u8, operand.u8)
		op_Call_1:;
			const size_t arg_count = operand.u8 & 0x7f;
			const bool no_ret_val  = operand.u8 & 0x80;

			struct ow_callstack_frame_info_list *const frame_info_list =
				&machine->callstack.frame_info_list;
			ow_callstack_frame_info_list_enter(frame_info_list);
			current_frame = frame_info_list->current;
			current_frame->not_ret_val = no_ret_val;
			current_frame->arg_list = stack.sp - arg_count + 1;
			current_frame->prev_fp = stack.fp;
			current_frame->prev_ip = ip;
			stack.fp = stack.sp + 1;

			struct ow_object *const callable_obj = *(stack.sp - arg_count);
			struct ow_class_obj *callable_obj_class;
			if (ow_unlikely(ow_smallint_check(callable_obj))) {
				callable_obj_class = builtin_classes->int_;
				goto other_func_obj_type;
			}
			callable_obj_class = ow_object_class(callable_obj);
			if (callable_obj_class == builtin_classes->func) {
				struct ow_func_obj *const func_obj =
					ow_object_cast(callable_obj, struct ow_func_obj);
				operand.pointer = invoke_impl_check_argc(
					machine, &func_obj->func_spec, arg_count);
				if (ow_unlikely(operand.pointer)) {
					*++stack.sp = operand.pointer;
					goto raise_exc;
				}
				ip = func_obj->code;
				current_func_obj = func_obj;
				current_module = func_obj->module;
			} else if (callable_obj_class == builtin_classes->cfunc) {
				struct ow_cfunc_obj *const cfunc_obj =
					ow_object_cast(callable_obj, struct ow_cfunc_obj);
				operand.pointer = invoke_impl_check_argc(
					machine, &cfunc_obj->func_spec, arg_count);
				if (ow_unlikely(operand.pointer)) {
					*++stack.sp = operand.pointer;
					goto raise_exc;
				}
				stack.sp++; // Return value.
				STACK_COMMIT();
				const int status = cfunc_obj->code(machine);
				STACK_UPDATE();
				struct ow_object *const ret_val = *stack.fp;
				assert(current_frame->prev_ip == ip);
				stack.fp = current_frame->prev_fp;
				if (current_frame->not_ret_val || ow_unlikely(!ip)) {
					stack.sp = current_frame->arg_list - 2;
				} else {
					stack.sp = current_frame->arg_list - 1;
					*stack.sp = ret_val;
				}
				ow_callstack_frame_info_list_leave(frame_info_list);
				if (ow_unlikely(!ip)) {
					STACK_COMMIT();
					*_res_out = ret_val;
					return status;
				}
				current_frame = frame_info_list->current;
				if (ow_unlikely(status)) {
					*++stack.sp = ret_val; // Exception.
					goto raise_exc;
				}
			} else {
			other_func_obj_type:;
				STACK_COMMIT();
				const bool ok = invoke_impl_get_method_y(
					machine, callable_obj, common_symbols->call, stack.sp - arg_count);
				STACK_ASSERT_NC();
				if (ow_unlikely(!ok)) {
					stack.sp -= arg_count;
					goto raise_exc;
				}
				goto op_Call_1;
			}
		OP_END

		OP_BEGIN(PrepMethY)
			OPERAND(u8, operand.index)
		op_PrepMethY_1:;
			struct ow_symbol_obj *name =
				ow_func_obj_get_symbol(current_func_obj, operand.index);
			if (ow_unlikely(!name))
				goto err_bad_operand;
			struct ow_object *const obj = *stack.sp;
			struct ow_class_obj *obj_class;
			if (ow_unlikely(ow_smallint_check(obj)))
				obj_class = builtin_classes->int_;
			else
				obj_class = ow_object_class(obj);
			operand.index = ow_class_obj_find_method(obj_class, name);
			if (ow_likely(operand.index != (size_t)-1)) {
				*stack.sp = ow_class_obj_get_method(obj_class, operand.index);
				*++stack.sp = obj;
			} else {
				*++stack.sp = obj;
				STACK_COMMIT();
				const bool ok = invoke_impl_do_find_method(
					machine, obj, obj_class, name, stack.sp - 1);
				STACK_ASSERT_NC();
				if (ow_unlikely(!ok)) {
					stack.sp--;
					goto raise_exc;
				}
			}
		OP_END

		OP_BEGIN(PrepMethYW)
			OPERAND(u16, operand.index)
			goto op_PrepMethY_1;
		OP_END

#undef OP_BEGIN
#undef OP_END
#undef OPERAND
#undef NO_OPERAND

		default:
			ip--;
			*++stack.sp = ow_object_from(invoke_impl_make_error(
				machine, NULL, "unrecognized opcode `%02x' at %p", *ip, ip));
			goto raise_exc;

		err_not_implemented:
			ip--;
			*++stack.sp = ow_object_from(invoke_impl_make_error(
				machine, NULL,
				"instruction `%s' has not been implemented",
				ow_opcode_name((enum ow_opcode)*ip)));
			goto raise_exc;

		err_bad_operand:
			ip--;
			*++stack.sp = ow_object_from(invoke_impl_make_error(
				machine, NULL,
				"illegal operand for instruction `%s' at %p",
				ow_opcode_name((enum ow_opcode)*ip), ip));
			goto raise_exc;

		err_cond_is_not_bool:
			ip--;
			*++stack.sp = ow_object_from(invoke_impl_make_error(
				machine, NULL, "condition value is not a boolean object"));
			goto raise_exc;

		raise_exc:
			operand.pointer = *stack.sp; // The exception to raise.
			if (ow_unlikely(ow_smallint_check(operand.pointer) ||
					!ow_class_obj_is_base(builtin_classes->exception,
						ow_object_class(operand.pointer)))) {
				operand.pointer = ow_object_from(invoke_impl_make_error(
					machine, NULL, "raise a non-exception object"));
				*stack.sp = operand.pointer;
			}
			while (1) {
				ow_exception_obj_backtrace_append(
					operand.pointer,
					&(const struct ow_exception_obj_frame_info){
						.ip = ip,
						.function = ow_object_from(current_func_obj),
					}
				);

				ip = current_frame->prev_ip;
				stack.fp = current_frame->prev_fp;
				stack.sp = current_frame->arg_list - 2;

				struct ow_callstack_frame_info_list *const frame_info_list =
					&machine->callstack.frame_info_list;
				ow_callstack_frame_info_list_leave(frame_info_list);

				if (ow_unlikely(!ip)) {
					*_res_out = operand.pointer;
					STACK_COMMIT();
					return -1;
				}

				current_frame = frame_info_list->current;
				current_func_obj = ow_object_cast(
					(*(current_frame->arg_list - 1)), struct ow_func_obj);
				current_module = current_func_obj->module;
			}
		}

		ow_unreachable();
	}

#undef STACK_COMMIT
#undef STACK_UPDATE
#undef STACK_ASSERT_NC
}

int ow_machine_invoke(
		struct ow_machine *om, int argc, struct ow_object **res_out) {
	return invoke_impl(om, argc, res_out);
}

int ow_machine_call(
		struct ow_machine *om, struct ow_object *func,
		int argc, struct ow_object *argv[], struct ow_object **res_out) {
	ow_callstack_push(om->callstack, func);
	for (int i = 0; i < argc; i++)
		ow_callstack_push(om->callstack, argv[i]);
	return ow_machine_invoke(om, argc, res_out);
}

int ow_machine_call_native(
		struct ow_machine *om,
		struct ow_module_obj *mod, int (*func)(struct ow_machine *),
		int argc, struct ow_object *argv[], struct ow_object **res_out) {
	if (argc > (UINT8_MAX >> 1))
		argc = UINT8_MAX >> 1;
	struct ow_cfunc_obj *const func_obj =
		ow_cfunc_obj_new(om, mod, "", func, (struct ow_func_spec){0, (uint8_t)argc});
	ow_callstack_push(om->callstack, ow_object_from(func_obj));
	for (int i = 0; i < argc; i++)
		ow_callstack_push(om->callstack, argv[i]);
	return ow_machine_invoke(om, argc, res_out);
}

int ow_machine_run(
		struct ow_machine *om,
		struct ow_module_obj *module, struct ow_object **res_out) {
	struct ow_object *const init_func =
		ow_module_obj_get_global_y(module, om->common_symbols->anon);

	struct ow_object *const nil = om->globals->value_nil;
	if (!init_func || init_func == nil) {
		*res_out = nil;
		return 0;
	}

	ow_callstack_push(om->callstack, init_func);
	return ow_machine_invoke(om, 0, res_out);
}
