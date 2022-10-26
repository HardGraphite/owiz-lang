#ifndef OW_H
#define OW_H

#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#	if OW_EXPORT_API
#		define OW_API __declspec(dllexport)
#	else
#		define OW_API __declspec(dllimport)
#	endif
#elif (__GNUC__ + 0 >= 4) || defined(__clang__)
#	if OW_EXPORT_API
#		define OW_API __attribute__((visibility ("default")))
#	else
#		define OW_API
#	endif
#else
#	define OW_API
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Context of an OW instance.
 */
typedef struct ow_machine ow_machine_t;

/**
 * @brief Native function that can be called by OW.
 */
typedef int (*ow_native_func_t)(ow_machine_t *);

/**
 * @brief Pair of name string and function pointer.
 */
typedef struct ow_native_name_func_pair {
	const char      *name; ///< Name of the function.
	ow_native_func_t func; ///< Pointer to the native function.
} ow_native_name_func_pair_t;

/**
 * @brief Definition of a native class.
 */
typedef struct ow_native_class_def {
	const char                       *name;      ///< Class name. Optional.
	size_t                            data_size; ///< Object data area size (number of bytes).
	const ow_native_name_func_pair_t *methods;   ///< Object methods. A NULL-terminated array.
	void (*finalizer)(ow_machine_t *, void *);   ///< Object finalizer. Optional.
} ow_native_class_def_t;

/**
 * @brief Definition of a native module.
 */
typedef struct ow_native_module_def {
	const char                       *name;      ///< Module name. Optional.
	const ow_native_name_func_pair_t *functions; ///< Functions in module. A NULL-terminated array.
	void (*finalizer)(ow_machine_t *);           ///< Module finalizer. Optional.
} ow_native_module_def_t;

/**
 * @brief Create an OW instance.
 *
 * @return Return the newly created instance.
 *
 * @warning To avoid a memory leak, the instance must be destroyed with `ow_destroy()`.
 */
OW_API ow_machine_t *ow_create(void);

/**
 * @brief Destroy an OW instance created using `ow_create()`.
 *
 * @param om pointer to the instance to destroy.
 */
OW_API void ow_destroy(ow_machine_t *om);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OW_H */
