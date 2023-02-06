#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <owiz.h>
#include <utilities/attributes.h>
#include <utilities/platform.h>
#include <utilities/unreachable.h>

#if _IS_WINDOWS_
#	include <io.h>
#	include <Windows.h>
#elif _IS_POSIX_
#	include <sys/ioctl.h>
#	include <unistd.h>
#endif

#ifdef __GNUC__
#	define static_cold_func __attribute__((cold)) __attribute__((noinline)) static
#else
#	define static_cold_func ow_noinline static
#endif

/**** **** ***** ***** ***** ***** Utilities ***** ****** ***** ***** **** ****/

#ifdef _WIN32

static char *win32_str_wide_to_utf8(const wchar_t *wide_str) {
	const int buf_sz =
		WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
	assert(buf_sz > 0);
	char *const buf = malloc((size_t)buf_sz);
	WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, buf, buf_sz, NULL, NULL);
	return buf;
}

static char **win32_strarr_wide_to_utf8(
	const wchar_t **wide_str_arr, size_t arr_len) {
	char **const new_arr = malloc(sizeof(char *) * arr_len);
	for (size_t i = 0; i < arr_len; i++) {
		const wchar_t *const wide_str = wide_str_arr[i];
		new_arr[i] = wide_str ? win32_str_wide_to_utf8(wide_str) : NULL;
	}
	return new_arr;
}

#endif // _WIN32

static bool stdin_is_tty(void) {
#if defined(_POSIX_VERSION)
	return isatty(STDIN_FILENO);
#elif defined(_WIN32)
	return _isatty(_fileno(stdin));
#else
	return true;
#endif
}

static size_t stdout_win_width(void) {
	const unsigned int default_value = 80;
#if defined(_POSIX_VERSION)
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
		return default_value;
	return w.ws_col;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
		return default_value;
	return (size_t)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#else
	return default_value;
#endif
}

/***** ***** ***** ***** ******  Argument Parser ****** ***** ***** ***** *****/

struct argparse_option;

/// Callback function for option handling.
/// Parameter `opt_arg` will be NULL if there is no argument for this option.
/// If this function returns a non-zero value, argparse_parse() will return.
typedef int (*argparse_optionhandler_t)
	(void *data, const struct argparse_option *opt, const char *opt_arg);

/// Description of an option.
typedef struct argparse_option {
	char short_name; ///< Short option name to be used like `-o [arg]` or `-o[arg]`. Assign 0 to disable.
	const char *long_name; ///< Long option name to be used like `--opt [arg]` or `--opt=[arg]`. Assign NULL to disable.
	const char *argument; ///< Name of argument which will be printed by argparse_help(). Assign NULL to disable.
	const char *help; ///< Help message which will be used by argparse_help(). Nullable.
	argparse_optionhandler_t handler; ///< The option handler.
} argparse_option_t;

/// An array of options. The last element must be filled with 0.
typedef const argparse_option_t *argparse_options_t;

/// Description of the program.
typedef struct argparse_program {
	const char *name; ///< Program name.
	const char *usage; ///< Arguments usage message. Nullable.
	const char *help; ///< Help message. Nullable.
	argparse_options_t opts; ///< Valid options.
} argparse_program_t;

#define ARGPARSE_OK          0 ///< No error.
#define ARGPARSE_ERR_BADARG  1 ///< Error: unexpected positional argument.
#define ARGPARSE_ERR_BADOPT  2 ///< Error: unrecognized option.
#define ARGPARSE_ERR_NOARG   3 ///< Error: unexpected argument for this option.
#define ARGPARSE_ERR_NEEDARG 4 ///< Error: this option takes an argument.
#define ARGPARSE_ERR_TERM    7 ///< Error: terminate.

///< Get error code (ARGPARSE_ERR_XXX or ARGPARSE_OK) from return value of argparse_parse().
#define ARGPARSE_GETERROR(STATUS) ((STATUS) & 0xf)
///< Get error index (argv[index]) from return value of argparse_parse().
#define ARGPARSE_GETINDEX(STATUS) ((STATUS) >> 4)

/// Parse command-line arguments. On error, it will return a non-zero value
/// representing the error type and position, which can be interpreted using
/// ARGPARSE_GETERROR() and ARGPARSE_GETINDEX().
static_cold_func int argparse_parse(
		argparse_options_t opts, int argc, char *argv[], void *data) {
	int err_code = ARGPARSE_OK;
	int err_index = 0;

#define RETURN_ERR(ERR_CODE, ERR_INDEX) \
	do { \
		err_code = ERR_CODE; \
		err_index = ERR_INDEX; \
		goto return_err; \
	} while (0)

	for (int arg_index = 1; arg_index < argc; arg_index++) {
		char *const arg_str = argv[arg_index];
		const argparse_option_t *opt = NULL;

		if (arg_str[0] != '-' || !arg_str[1]) {
			for (const argparse_option_t *p = opts; ; p++) {
				if (!p->short_name && !p->long_name) {
					if (p->argument)
						opt = p;
					break;
				}
			}
			if (!opt)
				RETURN_ERR(ARGPARSE_ERR_BADARG, arg_index);
			if (opt->handler(data, opt, arg_str))
				RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
			continue;
		}

		if (arg_str[1] == '-') {
			char *const long_name = arg_str + 2;
			char *const equal_pos = strchr(long_name, '=');
			if (equal_pos)
				*equal_pos = '\0';
			for (const argparse_option_t *p = opts; ; p++) {
				const char *const p_long_name = p->long_name;
				if (p_long_name) {
					if (!strcmp(p_long_name, long_name)) {
						opt = p;
						break;
					}
				} else if (!p->short_name && !p->argument) {
					break;
				}
			}
			if (equal_pos)
				*equal_pos = '=';
			if (!opt)
				RETURN_ERR(ARGPARSE_ERR_BADOPT, arg_index);
			if (opt->argument) {
				if (equal_pos) {
					if (opt->handler(data, opt, equal_pos + 1))
						RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
				} else {
					const char *const opt_arg = argv[arg_index + 1];
					if (arg_index + 1 >= argc || opt_arg[0] == '-')
						RETURN_ERR(ARGPARSE_ERR_NEEDARG, arg_index);
					if (opt->handler(data, opt, opt_arg))
						RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
					arg_index++;
				}
			} else {
				if (equal_pos)
					RETURN_ERR(ARGPARSE_ERR_NOARG, arg_index);
				if (opt->handler(data, opt, NULL))
					RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
			}
		} else {
			const char short_name = arg_str[1];
			for (const argparse_option_t *p = opts; ; p++) {
				const char p_short_name = p->short_name;
				if (p_short_name) {
					if (p_short_name == short_name) {
						opt = p;
						break;
					}
				} else if (!p->long_name && !p->argument) {
					break;
				}
			}
			if (!opt)
				RETURN_ERR(ARGPARSE_ERR_BADOPT, arg_index);
			if (opt->argument) {
				if (arg_str[2]) {
					if (opt->handler(data, opt, arg_str + 2))
						RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
				} else {
					const char *const opt_arg = argv[arg_index + 1];
					if (arg_index + 1 >= argc || opt_arg[0] == '-')
						RETURN_ERR(ARGPARSE_ERR_NEEDARG, arg_index);
					if (opt->handler(data, opt, opt_arg))
						RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
					arg_index++;
				}
			} else {
				if (arg_str[2])
					RETURN_ERR(ARGPARSE_ERR_NOARG, arg_index);
				if (opt->handler(data, opt, NULL))
					RETURN_ERR(ARGPARSE_ERR_TERM, arg_index);
			}
		}
	}

	return 0;

#undef RETURN_ERR

return_err:
	assert(err_code != ARGPARSE_OK);
	const int status = (err_index << 4) | err_code;
	assert(ARGPARSE_GETERROR(status) == err_code);
	assert(ARGPARSE_GETINDEX(status) == err_index);
	return status;
}

/// Print help message to stdout.
static_cold_func void argparse_help(const argparse_program_t *prog) {
	printf("Usage: %s %s\n", prog->name, prog->usage ? prog->usage : "...");
	if (prog->help) {
		fwrite("  ", 1, 2, stdout);
		puts(prog->help);
		fputc('\n', stdout);
	}

	puts("Options:");
	const size_t left_width = 24, right_width = stdout_win_width() - 1 - left_width;
	for (const argparse_option_t *opt = prog->opts; ; opt++) {
		if (!opt->short_name && !opt->long_name) {
			if (opt->argument)
				continue;
			else
				break;
		}

		size_t char_count = 0;
		fwrite("  ", 1, 2, stdout);
		char_count += 2;
		if (opt->short_name) {
			fputc('-', stdout);
			fputc(opt->short_name, stdout);
			char_count += 2;
			if (opt->long_name) {
				fwrite(", ", 1, 2, stdout);
				char_count += 2;
			}
		}
		if (opt->long_name) {
			const size_t n = strlen(opt->long_name);
			fwrite("--", 1, 2, stdout);
			fwrite(opt->long_name, 1, n, stdout);
			char_count += 2 + n;
		}
		if (opt->argument) {
			const size_t n = strlen(opt->argument);
			fputc(' ', stdout);
			fwrite(opt->argument, 1, n, stdout);
			char_count += 1 + n;
		}
		fputc(' ', stdout);
		char_count += 1;

		if (opt->help) {
			size_t pad_n;
			if (char_count <= left_width) {
				pad_n = left_width - char_count;
			} else {
				fputc('\n', stdout);
				pad_n = left_width;
			}
			for (size_t i = 0; i < pad_n; i++)
				fputc(' ', stdout);

			const char *s = opt->help;
			while (1) {
				if (strlen(s) <= right_width) {
					fputs(s, stdout);
					break;
				}

				const char *end = s + right_width;
				while (1) {
					if (*end == ' ')
						break;
					if (end == s) {
						end = s + right_width;
						break;
					}
					end--;
				}

				fwrite(s, 1, (size_t)(end - s), stdout);
				s = *end == ' ' ? end + 1 : end;
				fputc('\n', stdout);
				for (size_t i = 0; i < left_width; i++)
					fputc(' ', stdout);
			}
		}

		fputc('\n', stdout);
	}
}

/***** ***** ***** ****** ******  Main Instance ****** ****** ***** ***** *****/

static ow_machine_t *main_om = NULL;

/// Make sure `main_om` has been initialized.
static void prepare_mom(void) {
	if (ow_likely(main_om))
		return;
	main_om = ow_create();
}

/// Make sure `main_om` has been finalized.
static_cold_func void cleanup_mom(void) {
	if (!main_om)
		return;
	ow_destroy(main_om);
	main_om = NULL;
}

/// Call `cleanup_mom()` and then `exit()`.
static_cold_func ow_noreturn void cleanup_mom_and_exit(int exit_status) {
	cleanup_mom();
	exit(exit_status);
}

/**** **** **** Commandline Arguments and Environment Variables ***** **** ****/

struct ow_args {
	const char *prog;
	bool        repl;
	const char *code;
	const char *file;
	size_t      argc;
	char      **argv;
};

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable: 4646)
#endif // _MSC_VER

static_cold_func void print_program_help(void);
static_cold_func void print_detailed_version(void);

static_cold_func ow_noreturn int opt_help(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(ctx), ow_unused_var(opt), ow_unused_var(arg);
	print_program_help();
	cleanup_mom_and_exit(EXIT_SUCCESS);
}

static_cold_func ow_noreturn int opt_version(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(ctx), ow_unused_var(opt), ow_unused_var(arg);
	puts(ow_sysconf(OW_SC_VERSION_STR).s);
	cleanup_mom_and_exit(EXIT_SUCCESS);
}

static_cold_func int opt_repl(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt), ow_unused_var(arg);
	struct ow_args *const args = ctx;
	if (args->code || args->file) {
		fprintf(stderr,
			"%s: mutually exclusive options: `%s', `%s' and `%s'\n",
			args->prog, "-i/--repl", "-e/--eval", "-r/--run");
		cleanup_mom_and_exit(EXIT_FAILURE);
	}
	args->repl = true;
	return 0;
}

static_cold_func int opt_eval(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt), ow_unused_var(arg);
	struct ow_args *const args = ctx;
	if (args->file || args->repl) {
		fprintf(stderr,
			"%s: mutually exclusive options: `%s', `%s' and `%s'\n",
			args->prog, "-e/--eval", "-i/--repl", "-r/--run");
		cleanup_mom_and_exit(EXIT_FAILURE);
	}
	args->code = arg;
	return 0;
}

static_cold_func int opt_run(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt), ow_unused_var(arg);
	struct ow_args *const args = ctx;
	if (args->code || args->repl) {
		fprintf(stderr,
			"%s: mutually exclusive options: `%s', `%s' and `%s'\n",
			args->prog, "-r/--run", "-e/--eval", "-i/--repl");
		cleanup_mom_and_exit(EXIT_FAILURE);
	}
	args->file = arg;
	return 0;
}

static_cold_func int opt_path(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(ctx), ow_unused_var(opt);
	prepare_mom();
	ow_syscmd(main_om, OW_CMD_ADDPATH, arg);
	return 0;
}

static_cold_func int opt_verbose(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt);
	if (ow_sysctl(OW_CTL_VERBOSE, arg, (size_t)-1) == OW_ERR_FAIL) {
		if (toupper(arg[0]) == 'V' && arg[1] == '\0') {
			print_detailed_version();
			cleanup_mom_and_exit(EXIT_SUCCESS);
		}
		struct ow_args *const args = ctx;
		fprintf(stderr, "%s: invalid verbose target: `%s'\n", args->prog, arg);
		cleanup_mom_and_exit(EXIT_FAILURE);
	}
	return 0;
}

static_cold_func int opt_stack_size(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt);
	const long long n = atoll(arg);
	if (n <= 0) {
		struct ow_args *const args = ctx;
		fprintf(stderr, "%s: invalid stack size: `%s'\n", args->prog, arg);
		cleanup_mom_and_exit(EXIT_FAILURE);
	}
	ow_sysctl(OW_CTL_STACKSIZE, &n, sizeof n);
	return 0;
}

static_cold_func int opt_file_or_arg(
		void *ctx, const argparse_option_t *opt, const char *arg) {
	ow_unused_var(opt), ow_unused_var(arg);
	struct ow_args *const args = ctx;
	ow_unused_var(arg), ow_unused_var(args);
	assert(!args->argc && !args->argv);
	return 1;
}

#ifdef _MSC_VER
#	pragma warning(pop) // 4646
#endif // _MSC_VER

#pragma pack(push, 1)

static const char opt_version_help[] =
	"Print version information and exit. Use `-vV' to print more details.";
static const char opt_repl_help[] =
	"Run REPL (interactive mode). "
	"This option will be automatically enabled if neither `-e' nor "
	"MODULE/FILE/- is given and stdin is connected to a terminal.";
static const char opt_run_help[] =
	"Run the module or file. "
	"`:MODULE' is a module name with prefix `:'; `FILE' is the path to a file; "
	"`-' represents stdin. "
	"This option will be automatically used if neither `-i' nor `-e' is specified "
	"and there are reset command-line arguments.";
static const char opt_verbose_help[] =
	"Enable or disable verbose output for memory (M), lexer (L), parser (P), "
	"code generator (C) or detailed version information (V).";

static const argparse_option_t options[] = {
	{'h', "help"   , NULL   , "Print help message and exit.", opt_help        },
	{'V', "version", NULL   , opt_version_help              , opt_version     },
	{'i', "repl"   , NULL   , opt_repl_help                 , opt_repl        },
	{'e', "eval"   , "CODE" , "Evaluate the given CODE."    , opt_eval        },
	{'r', "run"    , ":MODULE|FILE|-", opt_run_help         , opt_run         },
	{'P', "path"   , "PATH" , "Add a module search path."   , opt_path        },
	{'v', "verbose", "[!]M|L|P|C|V", opt_verbose_help       , opt_verbose     },
	{0  , "stack-size", "N" , "Set stack size (object count).", opt_stack_size},
	{0  , NULL     , "..."  , NULL                          , opt_file_or_arg },
	{0  , NULL     , NULL   , NULL                          , NULL            },
};

static const argparse_program_t program = {
	.name = "owiz",
	.usage= "[OPTION...] [:MODULE|FILE|-] [ARG...]",
	.help = "the OWIZ programming language",
	.opts = options,
};

#pragma pack(pop)

ow_noreturn static_cold_func void print_arg_err_and_exit(char *argv[], int status) {
	const int error = ARGPARSE_GETERROR(status);
	const int error_idx = ARGPARSE_GETINDEX(status);
	const char *error_msg;
	switch (error) {
	case ARGPARSE_ERR_BADARG:
		error_msg = "unexpected positional argument";
		break;
	case ARGPARSE_ERR_BADOPT:
		error_msg = "unrecognized option";
		break;
	case ARGPARSE_ERR_NOARG:
		error_msg = "unexpected argument";
		break;
	case ARGPARSE_ERR_NEEDARG:
		error_msg = "option takes an argument";
		break;
	case ARGPARSE_ERR_TERM:
	default:
		error_msg = NULL;
		break;
	}
	if (error_msg)
		fprintf(stderr, "%s: %s: `%s'\n", argv[0], error_msg, argv[error_idx]);
	cleanup_mom_and_exit(EXIT_FAILURE);
}

/// Parse command line arguments.
static void parse_args(int argc, char *argv[], struct ow_args *args) {
	const int status = argparse_parse(options, argc, argv, args);
	const int error = ARGPARSE_GETERROR(status);
	if (error != ARGPARSE_OK) {
		if (!(error == ARGPARSE_ERR_TERM && !args->argv))
			print_arg_err_and_exit(argv, status);

		// See opt_file_or_arg().
		assert(!args->argc && !args->argv);
		int index = ARGPARSE_GETINDEX(status);
		if (!(args->repl || args->code || args->file))
			args->file = argv[index++];
		assert(index <= argc);
		args->argc = (size_t)(argc - index);
		args->argv = argv + index;
	}
	if (!args->file && !args->code && !args->repl) {
		if (stdin_is_tty())
			args->repl = true;
		else
			args->file = "-"; // stdin
	}
}

static_cold_func void print_program_help(void) {
	argparse_help(&program);
}

static_cold_func void print_detailed_version(void) {
	printf("%s version %s\n\n", "ow", ow_sysconf(OW_SC_VERSION_STR).s);
	printf("%-16s: %s\n", "Platform", ow_sysconf(OW_SC_PLATFORM).s);
	printf("%-16s: %s\n", "Compiler", ow_sysconf(OW_SC_COMPILER).s);
	printf("%-16s: %s\n", "Build time", ow_sysconf(OW_SC_BUILDTIME).s);
	printf("%-16s: %s\n", "Build type", ow_sysconf(OW_SC_DEBUG).i ? "debug" : "release");
}

/// Parse environment variables.
static void parse_environ(struct ow_args *args) {
	ow_unused_var(args);
}

/***** ***** ***** ***** ***** the Main Function ****** ***** ***** ***** *****/

static_cold_func ow_noreturn void print_top_exception_and_exit(void) {
	const int flags = (OW_RDEXC_MSG | OW_RDEXC_BT) | OW_RDEXC_PRINT;
	const int status = ow_read_exception(main_om, 0, flags);
	ow_unused_var(status);
	assert(status == 0);
	cleanup_mom_and_exit(EXIT_FAILURE);
}

static int ow_main(int argc, char *argv[]) {
	int status;
	struct ow_args args;

	memset(&args, 0, sizeof args);
	args.prog = argv[0];
	parse_environ(&args);
	parse_args(argc, argv, &args);

	prepare_mom();

	ow_syscmd(main_om, OW_CMD_ADDPATH, ".");

	if (args.repl) {
		status = ow_make_module(main_om, "repl", NULL, OW_MKMOD_LOAD);
	} else {
		const char *name = "__main__";
		const char *src;
		int flags;
		if (args.file) {
			if (args.file[0] == '-' && args.file[1] == '\0') // "-", stdin
				src = NULL, flags = OW_MKMOD_STDIN;
			else if (args.file[0] == ':' && args.file[1] != '\0') // ":MODULE"
				name = args.file + 1, src = NULL, flags = OW_MKMOD_LOAD;
			else
				src = args.file, flags = OW_MKMOD_FILE;
		} else if (args.code) {
			src = args.code, flags = OW_MKMOD_STRING;
		} else {
			ow_unreachable();
		}
		status = ow_make_module(main_om, name, src, flags);
	}
	if (status != 0) {
		assert(status == OW_ERR_FAIL);
		print_top_exception_and_exit();
	}

	status = ow_invoke(main_om, 0, OW_IVK_MODULE | (OW_IVK_NORETVAL | OW_IVK_MODMAIN));
	if (status != 0) {
		assert(status == OW_ERR_FAIL);
		print_top_exception_and_exit();
	}

	cleanup_mom();
	return EXIT_SUCCESS;
}

#ifdef _WIN32

static char **utf8_argv;

int wmain(int argc, wchar_t *argv[]) {
	utf8_argv = win32_strarr_wide_to_utf8(
		(const wchar_t **)argv, (size_t)argc + 1);
	return ow_main(argc, utf8_argv);
}

#else // !_WIN32

int main(int argc, char *argv[]) {
	return ow_main(argc, argv);
}

#endif // _WIN32
