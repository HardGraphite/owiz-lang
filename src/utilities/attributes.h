#pragma once

#if defined __GNUC__
#	define ow_likely(EXPR)    __builtin_expect((_Bool)(EXPR), 1)
#	define ow_unlikely(EXPR)  __builtin_expect((_Bool)(EXPR), 0)
#	define ow_forceinline     __attribute__((always_inline)) inline
#	define ow_noinline        __attribute__((noinline))
#	define ow_nodiscard       __attribute__((warn_unused_result))
#elif defined _MSC_VER
#	define ow_likely(EXPR)    (EXPR)
#	define ow_unlikely(EXPR)  (EXPR)
#	define ow_inline          __forceinline
#	define ow_noinline        __declspec(noinline)
#	define ow_nodiscard       _Check_return_
#else
#	define ow_likely(EXPR)    (EXPR)
#	define ow_unlikely(EXPR)  (EXPR)
#	define ow_inline
#	define ow_noinline
#	define ow_nodiscard
#endif

#define ow_static_inline      static inline
#define ow_static_forceinline ow_forceinline static
#define ow_noreturn           _Noreturn

#define ow_unused_var(X)      ((void)(X))
