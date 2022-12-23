#pragma once

#include <limits.h>
#include <stddef.h>

#include "opcode.h"

struct ow_func_obj;
struct ow_stream;

/// Callback function for `ow_bytecode_disassemble()`.
/// Return a non-zero value to stop disassembling.
typedef int (*ow_bytecode_disassemble_walker_t)(
	void *arg, size_t offset,
	const unsigned char *instruction, size_t instruction_width,
	enum ow_opcode opcode, int operand
);

/// A special value for param `operand` of `ow_bytecode_disassemble_walker_t`
/// to indicate that this instruction does not have an operand.
#define OW_BYTECODE_DISASSEMBLE_NO_OPERAND INT_MIN

/// Disassemble bytecode. Return the last result from param `walker`.
int ow_bytecode_disassemble(
	const unsigned char *code, size_t offset, size_t length,
	ow_bytecode_disassemble_walker_t walker, void *walker_arg);

/// Print disassemble result of bytecode. Param `func` is optional.
void ow_bytecode_dump(
	const unsigned char *code, size_t offset, size_t length,
	struct ow_func_obj *func, size_t offset_off,
	size_t mark_off, struct ow_stream *out);
