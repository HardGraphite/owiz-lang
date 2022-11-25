#pragma once

#include <objects/natives.h>

#define OW_BIMOD_MODULE_DEF_NAME(MODULE) \
	__wo_bim_##MODULE##_module_def

#define OW_BIMOD_MODULE_DEF(MODULE) \
	const struct ow_native_module_def OW_BIMOD_MODULE_DEF_NAME(MODULE)
