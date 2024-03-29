#include "modules_util.h"

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <owiz.h>
#include <utilities/thread.h> // thread_local

#ifndef sigsetjmp
#    define sigjmp_buf                jmp_buf
#    define sigsetjmp(env, savemask)  setjmp(env)
#    define siglongjmp(env, status)   longjmp(env, (status))
#endif

#define DEFAULT_PS1 ">> "
#define DEFAULT_PS2 " > "
#define DEFAULT_PSR ":= "

//# prompt(type :: Int) :: String
//# Get prompt string. `type`: `0` = result, `1` = input, `2` = continued input.
static int func_prompt(struct ow_machine *om) {
    intmax_t type;
    if (owiz_read_int(om, -1, &type) != 0)
        return 0;
    if (type == 0) {
        if (owiz_load_global(om, "__PSR") != 0) {
            owiz_push_string(om, DEFAULT_PSR, (size_t)-1);
            owiz_dup(om, 1);
            owiz_store_global(om, "__PSR");
        }
    } else if (type == 1) {
        intmax_t count;
        if (owiz_load_global(om, "count") != 0 || owiz_read_int(om, 0, &count) != 0)
            count = 0;
        owiz_push_int(om, count + 1);
        owiz_store_global(om, "count");

        if (owiz_load_global(om, "__PS1") != 0) {
            owiz_push_string(om, DEFAULT_PS1, (size_t)-1);
            owiz_dup(om, 1);
            owiz_store_global(om, "__PS1");
        }
    } else if (type == 2) {
        if (owiz_load_global(om, "__PS2") != 0) {
            owiz_push_string(om, DEFAULT_PS2, (size_t)-1);
            owiz_dup(om, 1);
            owiz_store_global(om, "__PS2");
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

enum readline_status {
    RL_OK, // Succeeded.
    RL_EOF, // Reached end of file.
    RL_ABORT, // Abort, SIGINT.
    RL_NA, // Not available.
};

thread_local static sigjmp_buf simple_readline_sig_jmp_buf;

_Noreturn static void simple_readline_sig_handler(int sig) {
    assert(sig);
    assert(sig == SIGINT);
    siglongjmp(simple_readline_sig_jmp_buf, sig);
}

/// Pop top value as prompt string and read line.
static enum readline_status simple_readline(
    struct ow_machine *om, struct code_buffer *code
) {
    enum readline_status status = RL_OK;

    const char *prompt_string;
    if (owiz_read_string(om, 0, &prompt_string, NULL) < 0)
        prompt_string = NULL;
    owiz_drop(om, 1);
    if (prompt_string) {
        fputs(prompt_string, stdout);
        fflush(stdout);
    }

    void (*const orig_sig_handler)(int) =
        signal(SIGINT, simple_readline_sig_handler);
    if (sigsetjmp(simple_readline_sig_jmp_buf, 1)) {
        status = RL_ABORT;
        goto cleanup;
    }

    while (true) {
        char buffer[80];
        if (!fgets(buffer, sizeof buffer, stdin)) {
            status = RL_EOF;
            goto cleanup;
        }
        const size_t len = strlen(buffer);
        assert(len > 0);
        code_buffer_append(code, buffer, len);
        if (buffer[len - 1] == '\n')
            break;
    }

cleanup:
    signal(SIGINT, orig_sig_handler != SIG_ERR ? orig_sig_handler : SIG_DFL);
    return status;
}

/// Pop top value as prompt string and read line with readline module.
static enum readline_status librl_readline(
    struct ow_machine *om, struct code_buffer *code
) {
    const int prompt_index = owiz_drop(om, 0);
    if (owiz_load_global(om, "readline") != 0)
        return RL_NA;
    if (owiz_load_attribute(om, 0, "readline") != 0) {
        owiz_drop(om, 1);
        return RL_NA;
    }
    owiz_load_local(om, prompt_index);
    const int status = owiz_invoke(om, 1, 0);
    if (status != 0) {
        if (status == OWIZ_ERR_FAIL) {
            const char *msg;
            owiz_read_exception(om, 0, OWIZ_RDEXC_MSG | OWIZ_RDEXC_TOBUF, &msg, NULL);
            if (strstr(msg, "SIGINT"))
                return RL_ABORT;
        }
        owiz_drop(om, 1);
        return RL_NA;
    }

    const char *line;
    if (owiz_read_string(om, 0, &line, NULL) != 0)
        line = NULL;
    if (!line)
        return RL_EOF;
    if (owiz_load_attribute(om, prompt_index + 1, "add_history") == 0) {
        owiz_swap(om);
        owiz_invoke(om, 1, OWIZ_IVK_NORETVAL);
    }
    code_buffer_append(code, line, strlen(line));
    return RL_OK;
}

//# read() :: String | Nil
//# Read input code.
static int func_read(struct ow_machine *om) {
    struct code_buffer code;
    code_buffer_init(&code);

    for (int line_num = 1; ; line_num++) {
        do {
            if (owiz_load_global(om, "prompt") == 0) {
                owiz_push_int(om, line_num > 1 ? 2 : 1);
                if (owiz_invoke(om, 1, 0) == 0)
                    break;
                owiz_drop(om, 1);
            }
            owiz_push_nil(om);
        } while (false);

        enum readline_status rl_status;
        rl_status = librl_readline(om, &code);
        if (rl_status == RL_NA)
            rl_status = simple_readline(om, &code);
        assert(rl_status != RL_NA);

        if (rl_status != RL_OK) {
            if (rl_status == RL_ABORT) {
                fputc('\n', stdout);
                line_num = 0;
                code.length = 0;
                continue;
            }
            assert(rl_status == RL_EOF);
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

    owiz_push_string(om, code.string, code.length);
    code_buffer_fini(&code);
    return 1;
}

//# eval(code :: String) :: Object
//# Compile and run code. Return the return-value.
static int func_eval(struct ow_machine *om) {
    const char *code;
    if (owiz_read_string(om, -1, &code, NULL) != 0 || code[0] == '\0')
        return 0;
    if (owiz_load_global(om, "repl_top") != 0) {
        owiz_make_module(om, "repl_top", NULL, OWIZ_MKMOD_EMPTY);
        owiz_dup(om, 1);
        owiz_store_global(om, "repl_top");
    }
    if (owiz_make_module(
            om, NULL, code,
            OWIZ_MKMOD_STRING | OWIZ_MKMOD_INCR | OWIZ_MKMOD_RETLAST) != 0) {
        owiz_read_exception(om, 0, OWIZ_RDEXC_MSG | OWIZ_RDEXC_PRINT);
        return 0;
    }
    if (owiz_invoke(om, 0, OWIZ_IVK_MODULE) != 0) {
        owiz_read_exception(om, 0, (OWIZ_RDEXC_MSG | OWIZ_RDEXC_BT) | OWIZ_RDEXC_PRINT);
        return 0;
    }
    return 1;
}

//# print_result(res :: Object)
//# Print eval result.
static int func_print_result(struct ow_machine *om) {
    if (owiz_read_nil(om, 0) == 0)
        return 0;

    if (owiz_load_global(om, "prompt") == 0) {
        const char *prompt;
        owiz_push_int(om, 0);
        if (owiz_invoke(om, 1, 0) != 0)
            return -1;
        if (owiz_read_string(om, 0, &prompt, NULL) == 0) {
            fputs(prompt, stdout);
            owiz_drop(om, 1);
        }
    }
    if (owiz_load_global(om, "print") == 0) {
        owiz_load_local(om, -1);
        if (owiz_invoke(om, 1, 0) != 0)
            return -1;
    }
    fputc('\n', stdout);
    return 0;
}

//# loop()
//# Start read-eval-print loop.
static int func_loop(struct ow_machine *om) {
    while (true) {
        assert(owiz_drop(om, 0) == 0);

        if (owiz_load_global(om, "read") != 0)
            goto func_not_found;
        if (owiz_invoke(om, 0, 0) != 0)
            goto raise_exc;
        if (owiz_read_nil(om, 0) == 0)
            goto quit_loop;

        if (owiz_load_global(om, "eval") != 0)
            goto func_not_found;
        owiz_swap(om);
        if (owiz_invoke(om, 1, 0) != 0) {
            owiz_read_exception(om, 0, (OWIZ_RDEXC_MSG | OWIZ_RDEXC_BT) | OWIZ_RDEXC_PRINT);
            owiz_drop(om, 1);
            continue;
        }

        if (owiz_load_global(om, "print_result") != 0)
            goto func_not_found;
        owiz_swap(om);
        if (owiz_invoke(om, 1, OWIZ_IVK_NORETVAL) != 0)
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
    owiz_load_global(om, "loop");
    return owiz_invoke(om, 0, OWIZ_IVK_NORETVAL);
}

static int initializer(struct ow_machine *om) {
    owiz_push_int(om, 0);
    owiz_store_global(om, "count");
    if (owiz_make_module(om, "readline", NULL, OWIZ_MKMOD_LOAD) == 0) {
        owiz_dup(om, 1);
        owiz_store_global(om, "readline");
        owiz_invoke(om, 0, OWIZ_IVK_MODULE | OWIZ_IVK_NORETVAL);
    }
    // TODO: Set completion entry function.
    return 0;
}

static const struct ow_native_func_def functions[] = {
    {"prompt"      , func_prompt      , 1, 0},
    {"read"        , func_read        , 0, 0},
    {"eval"        , func_eval        , 1, 0},
    {"print_result", func_print_result, 1, 0},
    {"loop"        , func_loop        , 0, 0},
    {"main"        , func_main        , 0, 0},
    {""            , initializer      , 0, 0},
    {NULL          , NULL             , 0, 0},
};

OW_BIMOD_MODULE_DEF(repl) = {
    .name = "repl",
    .functions = functions,
    .finalizer = NULL,
};
