#include "disassemble.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "operand.h"
#include <objects/funcobj.h>
#include <objects/moduleobj.h>
#include <objects/symbolobj.h>
#include <utilities/stream.h>
#include <utilities/unreachable.h>

#include <config/options.h>

static int get_operand_value(
		const unsigned char *operand_addr, enum ow_operand_type operand_info) {
	int operand_val;
	switch (operand_info) {
	case -2:
		operand_val = (int)*(const int16_t *)operand_addr;
		break;
	case -1:
		operand_val = (int)*(const int8_t *)operand_addr;
		break;
	case 0:
		operand_val = OW_BYTECODE_DISASSEMBLE_NO_OPERAND;
		static_assert(OW_BYTECODE_DISASSEMBLE_NO_OPERAND < INT16_MIN, "");
		break;
	case 1:
		operand_val = (int)*(const uint8_t *)operand_addr;
		break;
	case 2:
		operand_val = (int)*(const uint16_t *)operand_addr;
		static_assert(INT_MAX >= UINT16_MAX, "");
		break;
	default:
		ow_unreachable();
	}
	return operand_val;
}

int ow_bytecode_disassemble(
		const unsigned char *code, size_t offset, size_t length,
		ow_bytecode_disassemble_walker_t walker, void *walker_arg) {
	int walker_ret = 0;
	for (const unsigned char *p = code + offset, *const p_end = p + length; p < p_end; ) {
		const size_t addr_off = (size_t)(p - code);
		const enum ow_opcode opcode = (enum ow_opcode)*p;
		const enum ow_operand_type operand_info = ow_operand_type(opcode);
		const size_t operand_width = ow_operand_type_width(operand_info);
		const int operand = get_operand_value(p + 1, operand_info);
		walker_ret =
			walker(walker_arg, addr_off, p, operand_width + 1, opcode, operand);
		if (walker_ret != 0)
			break;
		p += 1 + ow_operand_type_width(operand_info);
	}
	return walker_ret;
}

struct dump_walker_arg {
	struct ow_stream *out;
	struct ow_func_obj *func;
	size_t offset_off;
	size_t mark_off;
};

static const char *write_operand_comment(
	size_t offset, enum ow_opcode opcode, int operand,
	struct ow_func_obj *func, char *buf, size_t buf_sz);

static int dump_walker(
		void *_arg, size_t offset,
		const unsigned char *instruction, size_t instruction_width,
		enum ow_opcode opcode, int operand) {
	struct dump_walker_arg *const arg = _arg;
	char buffer[120];
	char *p = buffer, *const p_end = buffer + sizeof buffer;
	int n;

	size_t disp_offset = offset + arg->offset_off;
	n = snprintf(
		p, (size_t)(p_end - p), "%04zx%s%02x ",
		disp_offset, (arg->mark_off == offset ? ">>" : ": "), (unsigned int)opcode);
	assert(n > 0);
	p += (size_t)n;

	assert(instruction_width >= 1);
	for (size_t i = 0, operand_width = instruction_width - 1; i < 2; i++) {
		if (i < operand_width) {
			const unsigned char byte = instruction[i + 1];
			n = snprintf(p, (size_t)(p_end - p), "%02x ", byte);
			assert(n > 0);
			p += (size_t)n;
		} else {
			assert(p_end - p > 3);
			memset(p, ' ', 3);
			p += 3;
		}
	}

	n = snprintf(p, (size_t)(p_end - p), " %-10s ", ow_opcode_name(opcode));
	assert(n > 0);
	p += (size_t)n;

	if (operand != OW_BYTECODE_DISASSEMBLE_NO_OPERAND) {
		n = snprintf(p, (size_t)(p_end - p), "%-3i", operand);
		assert(n > 0);
		p += (size_t)n;
	}

	if (instruction_width > 1) {
		assert(p < p_end);
		char comment_buffer[48];
		const char *const comment = write_operand_comment(
			disp_offset, opcode, operand,
			arg->func, comment_buffer, sizeof comment_buffer);
		if (comment) {
			n = snprintf(p, (size_t)(p_end - p - 1), " ; %s", comment);
			assert(n > 0);
			p += (size_t)n;
		}
	}

	assert(p < p_end);
	*p++ = '\n';
	ow_stream_write(arg->out, buffer, (size_t)(p - buffer));

	return 0;
}

void ow_bytecode_dump(
		const unsigned char *code, size_t offset, size_t length,
		struct ow_func_obj *func, size_t offset_off,
		size_t mark_off, struct ow_stream *out) {
	ow_bytecode_disassemble(
		code, offset, length, dump_walker,
		&(struct dump_walker_arg){out, func, offset_off, mark_off});
}

#if OW_BUILD_BYTECODE_DUMP_COMMENT

#if defined __GNUC__ && !defined __clang__
#	pragma GCC push_options
#	pragma GCC optimize ("Oz")
#elif defined _MSC_VER
#	pragma optimize("s", on)
#endif

struct find_global_name_by_index_walker_arg {
	size_t index;
	const char *name;
};

static int find_global_name_by_index_walker(
	void *_arg, struct ow_symbol_obj *name, size_t index, struct ow_object *val) {
	ow_unused_var(val);
	struct find_global_name_by_index_walker_arg *const arg = _arg;
	if (index != arg->index)
		return 0;
	arg->name = ow_symbol_obj_data(name);
	return 1;
}

static const char *find_global_name_by_index(
	struct ow_module_obj *module, size_t index) {
	struct find_global_name_by_index_walker_arg arg = {index, NULL};
	if (ow_module_obj_foreach_global(module, find_global_name_by_index_walker, &arg))
		return arg.name;
	return "?";
}

static const char *write_operand_comment(
	size_t offset, enum ow_opcode opcode, int operand,
	struct ow_func_obj *func, char *buf, size_t buf_sz) {
	if (opcode == OW_OPC_Call) {
		assert(operand >= 0 && operand <= 0xff);
		const bool no_ret_val = operand & 0x80;
		const int argc = operand & 0x7f;
		snprintf(buf, buf_sz, "argc=%i%c nrv", argc, no_ret_val ? ',' : '\0');
		return buf;
	}
	if (opcode == OW_OPC_Jmp     || opcode == OW_OPC_JmpW ||
		opcode == OW_OPC_JmpWhen || opcode == OW_OPC_JmpWhenW ||
		opcode == OW_OPC_JmpUnls || opcode == OW_OPC_JmpUnlsW) {
		snprintf(buf, buf_sz, "target=%04zx", (offset + (ptrdiff_t)operand));
		return buf;
	}
	if (opcode == OW_OPC_LdSym   || opcode == OW_OPC_LdSymW   ||
		opcode == OW_OPC_LdGlobY || opcode == OW_OPC_LdGlobYW ||
		opcode == OW_OPC_StGlobY || opcode == OW_OPC_StGlobYW ||
		opcode == OW_OPC_LdAttrY || opcode == OW_OPC_LdAttrYW ||
		opcode == OW_OPC_StAttrY || opcode == OW_OPC_StAttrYW ||
		opcode == OW_OPC_LdMod) {
		if (func) {
			struct ow_symbol_obj *const v =
				ow_func_obj_get_symbol(func, (size_t)operand);
			if (v)
				return ow_symbol_obj_data(v);
		}
		return NULL;
	}
	if (opcode == OW_OPC_LdGlob || opcode == OW_OPC_LdGlobW ||
		opcode == OW_OPC_StGlob || opcode == OW_OPC_StGlobW) {
		if (func) {
			assert(func->module);
			return find_global_name_by_index(func->module, (size_t)operand);
		}
		return NULL;
	}

	return NULL;
}

#if defined __GNUC__ && !defined __clang__
#	pragma GCC pop_options
#elif defined _MSC_VER
#	pragma optimize("s", on)
#endif

#else // !OW_BUILD_BYTECODE_DUMP_COMMENT

static const char *write_operand_comment(
		size_t offset, enum ow_opcode opcode, int operand,
		struct ow_func_obj *func, char *buf, size_t buf_sz) {
	ow_unused_var(offset), ow_unused_var(opcode), ow_unused_var(operand),
	ow_unused_var(func), ow_unused_var(buf), ow_unused_var(buf_sz);
	return NULL;
}

#endif // OW_BUILD_BYTECODE_DUMP_COMMENT
