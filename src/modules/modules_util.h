#pragma once

#include <objects/natives.h>

#if !OW_EXPORT_MOD
#	define OW_BIMOD_MODULE_ATTR
#elif defined(_WIN32) || defined(__CYGWIN__)
#	define OW_BIMOD_MODULE_ATTR __declspec(dllexport)
#elif (__GNUC__ + 0 >= 4) || defined(__clang__)
#	define OW_BIMOD_MODULE_ATTR __attribute__((used, visibility("default")))
#else
#	define OW_BIMOD_MODULE_ATTR
#endif

#define OW_BIMOD_MODULE_DEF_NAME_PREFIX_STR "__wo_mod_"
#define OW_BIMOD_MODULE_DEF_NAME_SUFFIX_STR "_def"

#define OW_BIMOD_MODULE_DEF_NAME(MODULE) \
	__wo_mod_##MODULE##_def

#define OW_BIMOD_MODULE_DEF(MODULE) \
	OW_BIMOD_MODULE_ATTR \
	const struct ow_native_module_def OW_BIMOD_MODULE_DEF_NAME(MODULE)
