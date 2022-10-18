#include "dumpcode.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include <utilities/stream.h>
#include <utilities/unreachable.h>

static void print_dis_line(
		size_t addr_off, bool mark, enum ow_opcode opcode, const char *op_name,
		int operand_info, const void *operand, struct ow_iostream *out) {
	char buffer[120];
	char *p = buffer, *const p_end = buffer + sizeof buffer;
	int n;

	n = snprintf(
		p, (size_t)(p_end - p), "%04zx%s%02x ",
		addr_off, (mark ? ">>" : ": "), (unsigned int)opcode);
	assert(n > 0);
	p += (size_t)n;

	for (size_t i = 0, operand_width = (size_t)abs(operand_info); i < 2; i++) {
		if (i < operand_width) {
			const unsigned char byte = ((const unsigned char *)operand)[i];
			n = snprintf(p, (size_t)(p_end - p), "%02x ", byte);
			assert(n > 0);
			p += (size_t)n;
		} else {
			assert(p_end - p > 3);
			memset(p, ' ', 3);
			p += 3;
		}
	}

	n = snprintf(p, (size_t)(p_end - p), " %-10s ", op_name);
	assert(n > 0);
	p += (size_t)n;

	int operand_val;
	switch (operand_info) {
	case -2:
		operand_val = *(const int16_t *)operand;
		break;
	case -1:
		operand_val = *(const int8_t *)operand;
		break;
	case 0:
		break;
	case 1:
		operand_val = *(const uint8_t *)operand;
		break;
	case 2:
		operand_val = *(const uint16_t *)operand;
		break;
	default:
		ow_unreachable();
	}
	if (operand_info) {
		n = snprintf(p, (size_t)(p_end - p), "%i", operand_val);
		assert(n > 0);
		p += (size_t)n;
	}

	assert(p < p_end);
	*p++ = '\n';
	ow_iostream_write(out, buffer, (size_t)(p - buffer));
}

void ow_bytecode_dump(
		const unsigned char *code, size_t offset, size_t length,
		size_t mark_pos, struct ow_iostream *out) {
	for (const unsigned char *p = code + offset, *const p_end = p + length; p < p_end; ) {
		const size_t add_off = (size_t)(p - code);
		const enum ow_opcode opcode = (enum ow_opcode)*p;
		const char *const op_name = ow_opcode_name(opcode);
		const int operand_info = ow_operand_info(opcode);
		print_dis_line(
			add_off, add_off == mark_pos, opcode, op_name ? op_name : "???",
			operand_info, p + 1, out);
		p += 1 + (operand_info < 0 ? -operand_info : operand_info);
	}
}
