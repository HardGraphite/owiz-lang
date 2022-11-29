#include "sysparam.h"

volatile struct ow_sysparam ow_sysparam = {
	.verbose_memory   = false,
	.verbose_lexer    = false,
	.verbose_parser   = false,
	.verbose_codegen  = false,
	.stack_size       = 4000 / sizeof(void *),
};
