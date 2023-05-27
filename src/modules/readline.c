#include "modules_util.h"

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <owiz.h>
#include <config/options.h>

#if OW_LIB_READLINE_USE_LIBEDIT
#    include <editline/readline.h>
#else
#    include <readline/readline.h>
#    include <readline/history.h>
#endif

#include <utilities/attributes.h>
#include <utilities/thread.h> // thread_local

#ifndef sigsetjmp
#    define sigjmp_buf                jmp_buf
#    define sigsetjmp(env, savemask)  setjmp(env)
#    define siglongjmp(env, status)   longjmp(env)
#endif

thread_local static struct ow_machine *completion_om;

static char *completion_entry_function(const char *text, int state) {
    struct ow_machine *const om = completion_om;
    assert(om);
    if (owiz_load_global(om, "rl_completion_entry_function") != 0)
        return NULL;
    if (owiz_read_nil(om, 0) == 0)
        return NULL;
    owiz_push_string(om, text, (size_t)-1);
    owiz_push_int(om, state);
    if (owiz_invoke(om, 2, 0) != 0)
        return NULL;
    size_t str_sz;
    if (owiz_read_string(om, -1, NULL, &str_sz) != 0)
        return NULL;
    char *str = malloc(str_sz + 1);
    if (owiz_read_string_to(om, -1, str, str_sz + 1) != 0) {
        free(str);
        return NULL;
    }
    return str;
}

thread_local static sigjmp_buf sig_jmp_buf;

ow_noreturn static void signal_handler(int sig) {
    ow_unused_var(sig);
    assert(sig == SIGINT);
    siglongjmp(sig_jmp_buf, 0xff);
}

//# readline(prompt :: String|Nil) :: String|Nil
static int func_readline(struct ow_machine *om) {
    const char *prompt = NULL;
    if (owiz_read_args(om, OWIZ_RDARG_IGNIL | OWIZ_RDARG_MKEXC,
            "s", &prompt, NULL) == OWIZ_ERR_FAIL)
        return -1;

    owiz_jmpbuf_t om_jmpbuf;
    owiz_setjmp(om, om_jmpbuf);
    rl_compentry_func_t *const orig_completion_entry_function =
        rl_completion_entry_function;
    rl_completion_func_t *const orig_attempted_completion_function =
        rl_attempted_completion_function;
    void (*const orig_sig_handler)(int) = signal(SIGINT, signal_handler);
    rl_completion_entry_function = completion_entry_function;
    rl_attempted_completion_function = NULL;
    completion_om = om;

    int status;
    if (sigsetjmp(sig_jmp_buf, 1) == 0) {
        char *const str = readline(prompt);
        if (str) {
            owiz_push_string(om, str, (size_t)-1);
            free(str);
            status = 1;
        } else {
            status = 0;
        }
    } else {
        rl_free_line_state();
        rl_cleanup_after_signal();
        rl_point = 0, rl_end = 0,/* rl_mark = 0,*/ rl_line_buffer[0] = 0;
        fputc('\n', rl_outstream);
        rl_on_new_line();
        rl_redisplay();
        status = owiz_longjmp(om, om_jmpbuf);
        assert(status == 0);
        status = owiz_make_exception(om, 0, "SIGINT");
        assert(status == 0);
        status = -1;
    }

    signal(SIGINT, orig_sig_handler != SIG_ERR ? orig_sig_handler : SIG_DFL);
    rl_completion_entry_function = orig_completion_entry_function;
    rl_attempted_completion_function = orig_attempted_completion_function;
    completion_om = NULL;

    return status;
}

//# add_history(line :: String)
static int func_add_history(struct ow_machine *om) {
    const char *line;
    if (owiz_read_args(om, OWIZ_RDARG_MKEXC, "s", &line, NULL) == OWIZ_ERR_FAIL)
        return -1;
    add_history(line);
    return 0;
}

//# set_completion_entry(func :: Callable[[String, Int], String|Nil] | Nil)
//# Set or remove completion entry function.
static int func_set_completion_entry(struct ow_machine *om) {
    owiz_load_local(om, -1);
    owiz_store_global(om, "rl_completion_entry_function");
    return 0;
}

static int initializer(struct ow_machine *om) {
    owiz_push_string(om, rl_readline_name, (size_t)-1);
    owiz_store_global(om, "name");
    return 0;
}

static const struct ow_native_func_def functions[] = {
    {"readline", func_readline, 1, 0},
    {"add_history", func_add_history, 1, 0},
    {"set_completion_entry", func_set_completion_entry, 1, 0},
    {"", initializer, 0, 0},
    {NULL, NULL, 0, 0},
};

OW_BIMOD_MODULE_DEF(readline) = {
    .name = "readline",
    .functions = functions,
    .finalizer = NULL,
};
