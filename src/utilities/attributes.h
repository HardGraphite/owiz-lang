#pragma once

#if defined __GNUC__
#    define ow_likely(EXPR)    __builtin_expect((_Bool)(EXPR), 1)
#    define ow_unlikely(EXPR)  __builtin_expect((_Bool)(EXPR), 0)
#    define ow_forceinline     __attribute__((always_inline)) inline
#    define ow_noinline        __attribute__((noinline))
#    define ow_nodiscard       __attribute__((warn_unused_result))
#    define ow_fallthrough     __attribute__((fallthrough))
#    define ow_unused_func     __attribute__((unused))
#elif defined _MSC_VER
#    define ow_likely(EXPR)    (EXPR)
#    define ow_unlikely(EXPR)  (EXPR)
#    define ow_forceinline     __forceinline
#    define ow_noinline        __declspec(noinline)
#    define ow_nodiscard       _Check_return_
#    define ow_fallthrough
#    define ow_unused_func
#else
#    define ow_likely(EXPR)    (EXPR)
#    define ow_unlikely(EXPR)  (EXPR)
#    define ow_forceinline
#    define ow_noinline
#    define ow_nodiscard
#    define ow_fallthrough
#    define ow_unused_func
#endif

#define ow_static_inline      static inline
#define ow_static_forceinline ow_forceinline static
#define ow_noreturn           _Noreturn
#define ow_unused_var(X)      ((void)(X))

#if defined __GNUC__
#    define ow_printf_fn_attrs(fmtstr_idx, data_idx) \
        __attribute__((format(__printf__, fmtstr_idx, data_idx)))
#    define ow_printf_fn_arg_fmtstr
#    define ow_malloc_fn_attrs(sz_idx, sz_name) \
        __attribute__((malloc)) __attribute__((alloc_size(sz_idx)))
#    define ow_realloc_fn_attrs(sz_idx, sz_name) \
        __attribute__((warn_unused_result)) __attribute__((alloc_size(sz_idx)))
#elif defined _MSC_VER
#    define ow_printf_fn_attrs(fmtstr_idx, data_idx)
#    define ow_printf_fn_arg_fmtstr _Printf_format_string_
#    define ow_malloc_fn_attrs(sz_idx, sz_name) \
        _Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(sz_name)
#    define ow_realloc_fn_attrs(sz_idx, sz_name) \
        _Success_(return != 0) _Check_return_ \
        _Ret_maybenull_ _Post_writable_byte_size_(sz_name)
#else
#    define ow_printf_fn_attrs(fmtstr_idx, data_idx)
#    define ow_printf_fn_arg_fmtstr
#    define ow_malloc_attrs(sz_idx, sz_name)
#    define ow_realloc_attrs(sz_idx, sz_name)
#endif
