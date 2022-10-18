#pragma once

#include <stddef.h>

struct ow_iostream;

/// Print disassemble result of bytecode.
void ow_bytecode_dump(
	const unsigned char *code, size_t offset, size_t length,
	size_t mark_pos, struct ow_iostream *out);
