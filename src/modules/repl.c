#include "modules_util.h"

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#	include <compat/msvc_stdatomic.h>
#else
#	include <stdatomic.h>
#endif

#include <ow.h>

#define DEFAULT_PS1 ">> "
#define DEFAULT_PS2 " > "
#define DEFAULT_PSR ":= "

//# prompt(type :: Int) :: String
//# Get prompt string. `type`: `0` = result, `1` = input, `2` = continued input.
static int func_prompt(struct ow_machine *om) {
	intmax_t type;
	if (ow_read_int(om, -1, &type) != 0)
		return 0;
	if (type == 0) {
		if (ow_load_global(om, "__PSR") != 0) {
			ow_push_string(om, DEFAULT_PSR, (size_t)-1);
			ow_dup(om, 1);
			ow_store_global(om, "__PSR");
		}
	} else if (type == 1) {
		intmax_t count;
		if (ow_load_global(om, "count") != 0 || ow_read_int(om, 0, &count) != 0)
			count = 0;
		ow_push_int(om, count + 1);
		ow_store_global(om, "count");

		if (ow_load_global(om, "__PS1") != 0) {
			ow_push_string(om, DEFAULT_PS1, (size_t)-1);
			ow_dup(om, 1);
			ow_store_global(om, "__PS1");
		}
	} else if (type == 2) {
		if (ow_load_global(om, "__PS2") != 0) {
			ow_push_string(om, DEFAULT_PS2, (size_t)-1);
			ow_dup(om, 1);
			ow_store_global(om, "__PS2");
		}
	} else {
		return 0;
	}
	return 1;
}

struct code_buffer {
	char  *string;
	size_t length;
	size_t capacity;
};

static void code_buffer_init(struct code_buffer *cb) {
	cb->string = NULL;
	cb->length = 0;
	cb->capacity = 0;
}

static void code_buffer_fini(struct code_buffer *cb) {
	if (cb->string)
		free(cb->string);
}

static void code_buffer_append(struct code_buffer *cb, const char *s, size_t n) {
	const size_t new_length = n + cb->length;
	if (new_length > cb->capacity) {
		cb->string = realloc(cb->string, new_length);
		cb->capacity = new_length;
	}
	memcpy(cb->string + cb->length, s, n);
	cb->length = new_length;
}

static jmp_buf sig_jmp_buf;

_Noreturn static void sig_handler(int sig) {
	assert(sig);
	longjmp(sig_jmp_buf, sig);
}

static atomic_int read_line_calling;

static bool do_read_line(struct code_buffer *code) {
	while (true) {
		if (atomic_fetch_add(&read_line_calling, 1) == 0)
			break;
		atomic_fetch_sub(&read_line_calling, 1);
	}

	signal(SIGINT, sig_handler);
	setjmp(sig_jmp_buf);

	while (true) {
		char buffer[80];
		if (!fgets(buffer, sizeof buffer, stdin))
			return false;
		const size_t len = strlen(buffer);
		assert(len > 0);
		code_buffer_append(code, buffer, len);
		if (buffer[len - 1] == '\n')
			break;
	}

	signal(SIGINT, SIG_DFL);
	atomic_fetch_sub(&read_line_calling, 1);
	return true;
}

//# read() :: String | Nil
//# Read input code.
static int func_read(struct ow_machine *om) {
	struct code_buffer code;
	code_buffer_init(&code);

	for (int line_num = 1; ; line_num++) {
		char prompt_string[96];
		do {
			if (ow_load_global(om, "prompt") == 0) {
				ow_push_int(om, line_num);
				if (ow_invoke(om, 1, 0) == 0)
					break;
				ow_drop(om, 1);
			}
			ow_push_nil(om);
		} while (false);
		if (ow_read_string_to(om, 0, prompt_string, sizeof prompt_string) < 0)
			strcpy(prompt_string, DEFAULT_PS1);
		ow_drop(om, 1);

		fputs(prompt_string, stdout);
		fflush(stdout);

		if (!do_read_line(&code)) {
			if (!code.length) {
				fputs("(EOF)\n", stdout);
				code_buffer_fini(&code);
				return 0;
			}
			break;
		}

		// TODO: Support multi-line input.
		break;
	}

	ow_push_string(om, code.string, code.length);
	code_buffer_fini(&code);
	return 1;
}

//# eval(code :: String) :: Object
//# Compile and run code. Return the return-value.
static int func_eval(struct ow_machine *om) {
	const char *code;
	if (ow_read_string(om, -1, &code, NULL) != 0 || code[0] == '\0')
		return 0;
	if (ow_load_global(om, "repl_top") != 0) {
		ow_make_module(om, "repl_top", NULL, OW_MKMOD_EMPTY);
		ow_dup(om, 1);
		ow_store_global(om, "repl_top");
	}
	if (ow_make_module(
			om, NULL, code,
			OW_MKMOD_STRING | OW_MKMOD_INCR | OW_MKMOD_RETLAST) != 0) {
		ow_read_exception(om, 0, OW_RDEXC_MSG | OW_RDEXC_PRINT);
		return 0;
	}
	if (ow_invoke(om, 0, OW_IVK_MODULE) != 0) {
		ow_read_exception(om, 0, (OW_RDEXC_MSG | OW_RDEXC_BT) | OW_RDEXC_PRINT);
		return 0;
	}
	return 1;
}

//# print_result(res :: Object)
//# Print eval result.
static int func_print_result(struct ow_machine *om) {
	if (ow_read_nil(om, 0) == 0)
		return 0;

	if (ow_load_global(om, "prompt") == 0) {
		const char *prompt;
		ow_push_int(om, 0);
		if (ow_invoke(om, 1, 0) != 0)
			return -1;
		if (ow_read_string(om, 0, &prompt, NULL) == 0) {
			fputs(prompt, stdout);
			ow_drop(om, 1);
		}
	}
	if (ow_load_global(om, "print") == 0) {
		ow_load_local(om, -1);
		if (ow_invoke(om, 1, 0) != 0)
			return -1;
	}
	fputc('\n', stdout);
	return 0;
}

//# loop()
//# Start read-eval-print loop.
static int func_loop(struct ow_machine *om) {
	while (true) {
		assert(ow_drop(om, 0) == 0);

		if (ow_load_global(om, "read") != 0)
			goto func_not_found;
		if (ow_invoke(om, 0, 0) != 0)
			goto raise_exc;
		if (ow_read_nil(om, 0) == 0)
			goto quit_loop;

		if (ow_load_global(om, "eval") != 0)
			goto func_not_found;
		ow_swap(om);
		if (ow_invoke(om, 1, 0) != 0) {
			ow_read_exception(om, 0, (OW_RDEXC_MSG | OW_RDEXC_BT) | OW_RDEXC_PRINT);
			ow_drop(om, 1);
			continue;
		}

		if (ow_load_global(om, "print_result") != 0)
			goto func_not_found;
		ow_swap(om);
		if (ow_invoke(om, 1, OW_IVK_NORETVAL) != 0)
			goto raise_exc;
	}

func_not_found:
	return 0;
raise_exc:
	return -1;
quit_loop:
	return 0;
}

static int func_main(struct ow_machine *om) {
	ow_load_global(om, "loop");
	return ow_invoke(om, 0, OW_IVK_NORETVAL);
}

static int initializer(struct ow_machine *om) {
	ow_push_int(om, 0);
	ow_store_global(om, "count");
	return 0;
}

static const struct ow_native_func_def functions[] = {
	{"prompt"      , func_prompt      , 1},
	{"read"        , func_read        , 0},
	{"eval"        , func_eval        , 1},
	{"print_result", func_print_result, 1},
	{"loop"        , func_loop        , 0},
	{"main"        , func_main        , 0},
	{""            , initializer      , 0},
	{NULL, NULL, 0},
};

OW_BIMOD_MODULE_DEF(repl) = {
	.name = "repl",
	.functions = functions,
	.finalizer = NULL,
};
