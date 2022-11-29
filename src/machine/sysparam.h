#pragma once

#include <stdbool.h>
#include <stddef.h>

/// Global parameters.
struct ow_sysparam {
	bool verbose_memory;
	bool verbose_lexer;
	bool verbose_parser;
	bool verbose_codegen;
	size_t stack_size; // Number of objects.
};

/// Global parameters.
extern volatile struct ow_sysparam ow_sysparam;
