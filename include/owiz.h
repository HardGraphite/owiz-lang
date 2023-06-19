/*
 * Owiz language runtime public APIs.
 */

#ifndef OWIZ_H
#define OWIZ_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#    if OWIZ_EXPORT_API
#        define OWIZ_API __declspec(dllexport)
#    else
#        define OWIZ_API __declspec(dllimport)
#    endif
#elif (__GNUC__ + 0 >= 4) || defined(__clang__)
#    if OWIZ_EXPORT_API
#        define OWIZ_API __attribute__((used, visibility("default")))
#    else
#        define OWIZ_API
#    endif
#else
#    define OWIZ_API
#endif

#if (defined(__cplusplus) && __cplusplus >= 201603L) /* C++17 */ \
        || (defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L) /* C23 */
#    define OWIZ_NODISCARD [[nodiscard]]
#elif defined(__GNUC__)
#    define OWIZ_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#    define OWIZ_NODISCARD _Check_return_
#else
#    define OWIZ_NODISCARD
#endif

#if !defined(__cplusplus) /* C */
#    define OWIZ_NOEXCEPT
#elif __cplusplus >= 201103L /* >= C++11 */
#    define OWIZ_NOEXCEPT noexcept
#else /* < C++11 */
#    define OWIZ_NOEXCEPT throw()
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define OWIZ_ERR_FAIL       (-1) ///< Error code: failed.
#define OWIZ_ERR_ARG        (-2) ///< Error code: invalid argument.
#define OWIZ_ERR_INDEX      (-3) ///< Error code: index out of range or name not found.
#define OWIZ_ERR_TYPE       (-4) ///< Error code: type mismatching.
#define OWIZ_ERR_NIMPL    (-128) ///< Error code: not implemented.

/**
 * @brief Context of an OWIZ instance.
 */
typedef struct ow_machine owiz_machine_t;

/**
 * @brief A type used in `owiz_setjmp()` to store execution context.
 */
typedef struct owiz_jmpbuf { void *_data[4]; } owiz_jmpbuf_t [1];

/**
 * @brief Native function that can be called by OWIZ.
 * @details To return object, push the object and return `1`; to return `nil`,
 * return `0`; to raise exception, push the exception object and return `-1`.
 */
typedef int (*owiz_native_func_t)(owiz_machine_t *) OWIZ_NOEXCEPT;

/**
 * @brief Pair of name string and function pointer.
 */
typedef struct owiz_native_func_def {
    const char        *name; ///< Name of the function.
    owiz_native_func_t func; ///< Pointer to the native function.
    int                argc; ///< Number of arguments. See `OWIZ_NATIVE_FUNC_VARIADIC_ARGC()` for variadic.
    unsigned int       oarg; ///< Number of optional arguments.
} owiz_native_func_def_t;

/**
 * @brief Represent number of arguments accepted by a variadic function.
 *
 * @param ARGC number of expected arguments except variadic ones
 * @return Return the value that can be used as field `argc` in `struct owiz_native_func_def`.
 */
#define OWIZ_NATIVE_FUNC_VARIADIC_ARGC(ARGC)  (-1 - (ARGC))

/**
 * @brief Definition of a native class.
 */
typedef struct owiz_native_class_def {
    const char                   *name;      ///< Class name. Optional.
    size_t                        data_size; ///< Object data area size (number of bytes).
    const owiz_native_func_def_t *methods;   ///< Object methods. A NULL-terminated array.
    void (*finalizer)(void *);               ///< Object finalizer. Optional.
} owiz_native_class_def_t;

/**
 * @brief Definition of a native module.
 */
typedef struct owiz_native_module_def {
    const char                   *name;      ///< Module name. Optional.
    const owiz_native_func_def_t *functions; ///< Functions in module. A NULL-terminated array.
    void (*finalizer)(void);                 ///< Module finalizer. Optional.
} owiz_native_module_def_t;

/**
 * @brief Return type of `owiz_sysconf()` function.
 */
typedef union owiz_sysconf_result {
    signed int   i;
    unsigned int u;
    const char  *s;
} owiz_sysconf_result_t;

#define OWIZ_SC_DEBUG           0 ///< Whether compiled with debug code.
#define OWIZ_SC_VERSION         1 ///< Get version number (`(MAJOR<<24)|(MINOR<<16)|(PATCH<<8)`).
#define OWIZ_SC_VERSION_STR     2 ///< Get version string.
#define OWIZ_SC_COMPILER        3 ///< Get the compiler that used to build this program.
#define OWIZ_SC_BUILDTIME       4 ///< Get the time and date when this program was built.
#define OWIZ_SC_PLATFORM        5 ///< Get the name of current platform.

/**
 * @brief Get configuration information at run time.
 *
 * @param name a `OWIZ_SC_XXX` macro
 * @return The result or `-1` if param `name` is not found.
 */
OWIZ_API union owiz_sysconf_result owiz_sysconf(int name) OWIZ_NOEXCEPT;

#define OWIZ_CTL_VERBOSE        0 ///< Enable verbose output. Value: `"[!]NAME"`.
#define OWIZ_CTL_STACKSIZE      1 ///< Set stack size (number of objects). Value: pointer to integer.
#define OWIZ_CTL_DEFAULTPATH    2 ///< Default module paths. Value: `"path_1\0path_2\0...path_n\0"`.

/**
 * @breif Write runtime parameters.
 *
 * @param name a `OWIZ_CTL_XXX` macro
 * @param val pointer to the value to set
 * @param val_sz size of the value in bytes
 * @return Returns `0` (success), `OWIZ_ERR_INDEX` (wrong `name`),
 * or `OWIZ_ERR_FAIL` (illegal value or not configurable).
 */
OWIZ_API int owiz_sysctl(int name, const void *val, size_t val_sz) OWIZ_NOEXCEPT;

/**
 * @brief Create an OWIZ instance.
 *
 * @return Return the newly created instance.
 *
 * @warning To avoid a memory leak, the instance must be destroyed with `owiz_destroy()`.
 */
OWIZ_API OWIZ_NODISCARD owiz_machine_t *owiz_create(void) OWIZ_NOEXCEPT;

/**
 * @brief Destroy an OWIZ instance created using `owiz_create()`.
 *
 * @param om pointer to the instance to destroy
 */
OWIZ_API void owiz_destroy(owiz_machine_t *om) OWIZ_NOEXCEPT;

/**
 * @brief Saves the current execution context.
 *
 * @param om the instance
 * @param env jmpbuf to save context to
 */
OWIZ_API void owiz_setjmp(owiz_machine_t *om, owiz_jmpbuf_t env) OWIZ_NOEXCEPT;

/**
 * @brief Restore execution context.
 *
 * @param om the instance
 * @param env jmpbuf in which execution context is saved by `owiz_setjmp()`
 * @return On success, return 0. If the `env` is invalid, return `OWIZ_ERR_FAIL`.
 *
 * @note Unlike `longjmp()` function from C standard library, this function
 * does not handle jumps in native environment.
 *
 * @warning DO NOT jump to another OWIZ instance or jump to a function call that has exited.
 */
OWIZ_API int owiz_longjmp(owiz_machine_t *om, owiz_jmpbuf_t env) OWIZ_NOEXCEPT;

/**
 * @brief Push a Nil object.
 *
 * @param om the instance
 */
OWIZ_API void owiz_push_nil(owiz_machine_t *om) OWIZ_NOEXCEPT;

/**
 * @brief Push a Bool object.
 *
 * @param om the instance
 * @param val the boolean value to push
 */
OWIZ_API void owiz_push_bool(owiz_machine_t *om, bool val) OWIZ_NOEXCEPT;

/**
 * @brief Push an Int object.
 *
 * @param om the instance
 * @param val the integer value to push
 */
OWIZ_API void owiz_push_int(owiz_machine_t *om, intmax_t val) OWIZ_NOEXCEPT;

/**
 * @brief Push a Float object.
 *
 * @param om the instance
 * @param val the floating-point value to push
 */
OWIZ_API void owiz_push_float(owiz_machine_t *om, double val) OWIZ_NOEXCEPT;

/**
 * @brief Push a Symbol object.
 *
 * @param om the instance
 * @param str pointer to the string of name
 * @param len length of the string, or `-1` to use length of NUL-terminated string
 */
OWIZ_API void owiz_push_symbol(owiz_machine_t *om, const char *str, size_t len) OWIZ_NOEXCEPT;

/**
 * @brief Push a String object.
 *
 * @param om the instance
 * @param str pointer to the string
 * @param len length of the string, or `-1` to use length of NUL-terminated string
 */
OWIZ_API void owiz_push_string(owiz_machine_t *om, const char *str, size_t len) OWIZ_NOEXCEPT;

/**
 * @brief Pop elements and create an Array object.
 *
 * @param om the instance
 * @param count number of elements
 */
OWIZ_API void owiz_make_array(owiz_machine_t *om, size_t count) OWIZ_NOEXCEPT;

/**
 * @brief Pop elements and create a Tuple object.
 *
 * @param om the instance
 * @param count number of elements
 */
OWIZ_API void owiz_make_tuple(owiz_machine_t *om, size_t count) OWIZ_NOEXCEPT;

/**
 * @brief Pop elements and create a Set object.
 *
 * @param om the instance
 * @param count number of objects to pop
 */
OWIZ_API void owiz_make_set(owiz_machine_t *om, size_t count) OWIZ_NOEXCEPT;

/**
 * @brief Pop keys and values and create a Map object.
 *
 * @param om the instance
 * @param count number of key-value pairs to pop
 */
OWIZ_API void owiz_make_map(owiz_machine_t *om, size_t count) OWIZ_NOEXCEPT;

/**
 * @brief Create and push an exception.
 *
 * @param om the instance
 * @param type `0`
 * @param fmt format string like `printf()`
 * @param ... data to be formatted
 * @return On success, returns `0`.
 */
OWIZ_API int owiz_make_exception(owiz_machine_t *om, int type, const char *fmt, ...) OWIZ_NOEXCEPT;

#define OWIZ_MKMOD_EMPTY    0x00 ///< Create an empty module.
#define OWIZ_MKMOD_NATIVE   0x01 ///< Create a module from native definition.
#define OWIZ_MKMOD_FILE     0x02 ///< Compile a module from file.
#define OWIZ_MKMOD_STRING   0x03 ///< Compile a module from string.
#define OWIZ_MKMOD_STDIN    0x04 ///< Compile a module from stdin.
#define OWIZ_MKMOD_LOAD     0x05 ///< Load a module like import statement.
#define OWIZ_MKMOD_INCR     (1 << 4) ///< Incremental, add new code to an existing module on stack top.
#define OWIZ_MKMOD_RETLAST  (1 << 5) ///< Try to return value of last expression.

/**
 * @brief Compile or load a module and push it.
 *
 * @param om the instance
 * @param name module name or `NULL` (`OWIZ_MKMOD_NATIVE`); see the note below for more details
 * @param src `NULL` (`OWIZ_MKMOD_EMPTY`/`OWIZ_MKMOD_STDIN`/`OWIZ_MKMOD_LOAD`);
 * path to source file (`OWIZ_MKMOD_FILE`); source string (`OWIZ_MKMOD_STRING`);
 * pointer to a `struct owiz_native_module_def` object (`OWIZ_MKMOD_NATIVE`).
 * @param flags `0` or `OWIZ_MKMOD_XXX` macros
 * @return On success, push the module and return `0`.
 * On failure, push an exception object instead of the generated module object,
 * and return `OWIZ_ERR_FAIL`.
 *
 * @note In `OWIZ_MKMOD_LOAD` mode, param `name` specifies the name of the module
 * to load; while in other modes, param `name` tells what the name of the newly
 * created module should be. Be aware of the difference between these two cases.
 * @note If `OWIZ_MKMOD_INCR` is specified, there shall be a module object pushed,
 * and the module will not be pushed again in this function.
 */
OWIZ_API int owiz_make_module(
    owiz_machine_t *om, const char *name, const void *src, int flags) OWIZ_NOEXCEPT;

/**
 * @brief Push a local variable or argument.
 *
 * @param om the instance
 * @param index 1-based index of the local variable to load;
 * if `index` is negative, load the `(-index)`-th argument
 * @return On success, returns `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`.
 */
OWIZ_API int owiz_load_local(owiz_machine_t *om, int index) OWIZ_NOEXCEPT;

/**
 * @brief Push a global variable from current module.
 *
 * @param om the instance
 * @param name name of the variable to load
 * @return On success, returns `0`. If variable is not found, return `OWIZ_ERR_INDEX`.
 */
OWIZ_API int owiz_load_global(owiz_machine_t *om, const char *name) OWIZ_NOEXCEPT;

/**
 * @brief Push object attribute or module content.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param name name of the attribute to get
 * @return On success, returns `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if attribute is not found, return `OWIZ_ERR_INDEX`.
 */
OWIZ_API int owiz_load_attribute(owiz_machine_t *om, int index, const char *name) OWIZ_NOEXCEPT;

/**
 * @brief Duplicate top object.
 *
 * @param om the instance
 * @param count number of duplications
 */
OWIZ_API void owiz_dup(owiz_machine_t *om, size_t count) OWIZ_NOEXCEPT;

/**
 * @brief Swap top two objects.
 *
 * @param om the instance
 */
OWIZ_API void owiz_swap(owiz_machine_t *om) OWIZ_NOEXCEPT;

/**
 * @brief Check whether an object is of Nil type.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_nil(owiz_machine_t *om, int index) OWIZ_NOEXCEPT;

/**
 * @brief Read the value of a Bool object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param val_p pointer to a buffer to store the value
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_bool(owiz_machine_t *om, int index, bool *val_p) OWIZ_NOEXCEPT;

/**
 * @brief Read the value of an Int object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param val_p pointer to a buffer to store the value
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_int(owiz_machine_t *om, int index, intmax_t *val_p) OWIZ_NOEXCEPT;

/**
 * @brief Read the value of a Float object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param val_p pointer to a buffer to store the value
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_float(owiz_machine_t *om, int index, double *val_p) OWIZ_NOEXCEPT;

/**
 * @brief Read the value of a Symbol object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param str_p pointer to a pointer to store the string
 * @param len_p pointer to a size_t value to store string length or `NULL`
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_symbol(
    owiz_machine_t *om, int index, const char **str_p, size_t *len_p) OWIZ_NOEXCEPT;

/**
 * @brief Read the value of a String object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param str_p pointer to a pointer to store the string
 * @param len_p pointer to a size_t value to store string length or `NULL`
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 *
 * @warning Calling this function may cause extra operation.
 */
OWIZ_API int owiz_read_string(
    owiz_machine_t *om, int index, const char **str_p, size_t *len_p) OWIZ_NOEXCEPT;

/**
 * @brief Copy contents of a String object to a buffer.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param buf pointer to a buffer to copy the string to
 * @param buf_sz size of the buffer `buf`
 * @return On success, return number of copied bytes. If `index` is out of range,
 * return `OWIZ_ERR_INDEX`; if the object class mismatches, return `OWIZ_ERR_TYPE`;
 * if the buffer `buf` is not big enough, return `OWIZ_ERR_FAIL`.
 *
 * @note If the buffer is big enough, a '\0' will be appended to the end of string,
 * but the return value will not change.
 */
OWIZ_API int owiz_read_string_to(
    owiz_machine_t *om, int index, char *buf, size_t buf_sz) OWIZ_NOEXCEPT;

#define OWIZ_RDEXC_MSG    0x01 ///< Get exception data as string.
#define OWIZ_RDEXC_BT     0x02 ///< Get backtrace.
#define OWIZ_RDEXC_PRINT  0x10 ///< Print to stdout or stderr.
#define OWIZ_RDEXC_TOBUF  0x20 ///< Write to a buffer.
#define OWIZ_RDEXC_PUSH   0x30 ///< Push the exception data.

/**
 * @brief Push element(s) of an Array object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param elem_idx 1-based index of element to get; `-1` to push all elements;
 * `0` to return array length only
 * @return On success, if param `elem_idx` is `0` or `-1`, return array length,
 * otherwise, return `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`;
 * if `elem_idx` is out of range, return `OWIZ_ERR_FAIL`.
 */
OWIZ_API size_t owiz_read_array(owiz_machine_t *om, int index, size_t elem_idx) OWIZ_NOEXCEPT;

/**
 * @brief Push element(s) of a Tuple object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param elem_idx 1-based index of element to get; `-1` to push all elements;
 * `0` to return tuple length only
 * @return On success, if param `elem_idx` is `0` or `-1`, return array length,
 * otherwise, return `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`;
 * if `elem_idx` is out of range, return `OWIZ_ERR_FAIL`.
 */
OWIZ_API size_t owiz_read_tuple(owiz_machine_t *om, int index, size_t elem_idx) OWIZ_NOEXCEPT;

/**
 * @brief Push element(s) of a set object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param op `-1` to push all elements; `0` to return number of elements only
 * @return On success, return array length. If `index` is out of range,
 * return `OWIZ_ERR_INDEX`; if the object class mismatches, return `OWIZ_ERR_TYPE`;
 * if `op` is invalid, return `OWIZ_ERR_ARG`.
 */
OWIZ_API size_t owiz_read_set(owiz_machine_t *om, int index, int op) OWIZ_NOEXCEPT;

#define OWIZ_RDMAP_GETLEN      (INT_MIN + 0) ///< Get number of elements only.
#define OWIZ_RDMAP_EXPAND      (INT_MIN + 1) ///< Push all keys and values.

/**
 * @brief Push element(s) of a map object.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param key_idx index of a local variable like param `index`, which is the key;
 * or a `OWIZ_RDMAP_GETLEN` or `OWIZ_RDMAP_EXPAND` macro
 * @return On success, if param `key_idx` is `OWIZ_RDMAP_XXX`, return element count,
 * otherwise, return `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`;
 * if `key_idx` is out of range or key does not exist, return `OWIZ_ERR_FAIL`.
 */
OWIZ_API size_t owiz_read_map(owiz_machine_t *om, int index, int key_idx) OWIZ_NOEXCEPT;

/**
 * @brief Get or print contents of an exception.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`,
 * or `0` to represent the top object on stack
 * @param flags `OWIZ_RDEXC_XXX` macros
 * @param ... `stdout` or `stderr` (when `OWIZ_RDEXC_PRINT`);
 * `char *buf, size_t buf_sz` (when `OWIZ_RDEXC_TOBUF`)
 * @return On success, return 0. If `index` is out of range, return `OWIZ_ERR_INDEX`;
 * if the object class mismatches, return `OWIZ_ERR_TYPE`.
 */
OWIZ_API int owiz_read_exception(owiz_machine_t *om, int index, int flags, ...) OWIZ_NOEXCEPT;

#define OWIZ_RDARG_IGNIL (1 << 1) ///< Ignore type error when the actual object is nil.
#define OWIZ_RDARG_MKEXC (1 << 2) ///< Create and push an exception when type error occurs.

/**
 * @brief Parse function arguments.
 *
 * @param om the instance
 * @param flags `OWIZ_RDARG_XXX` macros or `0`
 * @param fmt a string specifying the expected argument types or `NULL`
 * @param ... arguments depending on param `fmt`, or `int *argc` if `param` is `NULL`
 * @return On success, return 0. If an exception is raised, return `OWIZ_ERR_FAIL`;
 * if the `fmt` string is too long, return `OWIZ_ERR_INDEX`; if one object class
 * does not match the specified type, return `OWIZ_ERR_TYPE`; if unexpected character
 * is found in para `fmt`, return `OWIZ_ERR_ARG`.
 *
 * @details The `fmt` string consists of specifiers listed below, and the arguments
 * for the specifiers is similar to the corresponding owiz_read_xxx functions:
 * | Specifier |   Corresponding function   |
 * |:---------:|:--------------------------:|
 * | `x`       | `owiz_read_bool()`         |
 * | `i`       | `owiz_read_int()`          |
 * | `f`       | `owiz_read_float()`        |
 * | `y`       | `owiz_read_symbol()`       |
 * | `s`       | `owiz_read_string()`       |
 * | `s*`      | `owiz_read_string_to()`    |
 */
OWIZ_API int owiz_read_args(owiz_machine_t *om, int flags, const char *fmt, ...) OWIZ_NOEXCEPT;

/**
 * @brief Pop object and copy to a local variable or argument.
 *
 * @param om the instance
 * @param index index of local variable like param `index` in `owiz_load_local()`
 * @return On success, returns `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`.
 */
OWIZ_API int owiz_store_local(owiz_machine_t *om, int index) OWIZ_NOEXCEPT;

/**
 * @brief Pop object and copy to a global variable in current module.
 *
 * @param om the instance
 * @param name name of the variable to store to
 * @return Returns `0`.
 */
OWIZ_API int owiz_store_global(owiz_machine_t *om, const char *name) OWIZ_NOEXCEPT;

/**
 * @brief Pop object and assign as an attribute or a module variable.
 *
 * @param om the instance
 * @param index index of local variable to store to, like param `index` in
 * `owiz_load_local()`, or `0` to represent the top object on stack
 * @param name name of the attribute to store to
 * @return On success, returns `0`. If `index` is out of range, return `OWIZ_ERR_INDEX`.
 */
OWIZ_API int owiz_store_attribute(owiz_machine_t *om, int index, const char *name) OWIZ_NOEXCEPT;

/**
 * @brief Pop objects on stack.
 *
 * @param om the instance
 * @param count number of objects to drop; if it is greater than the number
 * of local variables, the frame will be empty
 * @return Returns number of rest objects.
 */
OWIZ_API int owiz_drop(owiz_machine_t *om, int count) OWIZ_NOEXCEPT;

#define OWIZ_IVK_METHOD         0x01 ///< The object at the function position is a method name.
#define OWIZ_IVK_MODULE         0x02 ///< Run a module instead of invoking it.
#define OWIZ_IVK_NORETVAL   (1 << 4) ///< Discard return value.
#define OWIZ_IVK_MODMAIN    (1 << 5) ///< Use with `OWIZ_IVK_MODULE` to call main function.

/**
 * @brief Invoke a function or method, or run a module.
 * @details To invoke a function (or an invocable object), the function (or
 * a method name, if `OWIZ_IVK_METHOD` is specified; or a module, if `OWIZ_IVK_MODULE`
 * is specified) and arguments shall have been pushed in order. After invoked,
 * the function and arguments will be automatically popped, and the return value
 * will be pushed (if flag `OWIZ_IVK_NORETVAL` is not specified). If an exception
 * is thrown, the exception object will be pushed instead (even if with
 * `OWIZ_IVK_NORETVAL` flag).
 *
 * @param om the instance
 * @param argc number of arguments
 * @param flags `0` or `OWIZ_IVK_XXX` macros
 * @return On success, return `0`. On failure (exception thrown) return `OWIZ_ERR_FAIL`.
 *
 * @warning If `OWIZ_IVK_METHOD` is specified, there must be at least 1 argument.
 */
OWIZ_API int owiz_invoke(owiz_machine_t *om, int argc, int flags) OWIZ_NOEXCEPT;

#define OWIZ_CMD_ADDPATH      0x0001 ///< Append a module search path. `const char *path`.

/**
 * @brief Execute a command.
 *
 * @param om the instance
 * @param name a `OWIZ_CMD_XXX` macro
 * @param ... arguments
 * @return On success, return `0`. If `name` is illegal, return `OWIZ_ERR_INDEX`;
 * if error occurs, return `OWIZ_ERR_FAIL`.
 */
OWIZ_API int owiz_syscmd(owiz_machine_t *om, int name, ...) OWIZ_NOEXCEPT;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#undef  OWIZ_NOEXCEPT

#endif /* OWIZ_H */
