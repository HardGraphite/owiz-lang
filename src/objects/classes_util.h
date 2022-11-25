#pragma once

#define OW_BICLS_CLASS_DEF_NAME(CLASS) \
	__wo_bic_##CLASS##_class_def

#define OW_BICLS_CLASS_DEF_EX_NAME(CLASS) \
	__wo_bic_##CLASS##_class_def_ex

#define OW_BICLS_CLASS_DEF(CLASS) \
	const struct ow_native_class_def OW_BICLS_CLASS_DEF_NAME(CLASS)

#define OW_BICLS_CLASS_DEF_EX(CLASS) \
	const struct ow_native_class_def_ex OW_BICLS_CLASS_DEF_EX_NAME(CLASS)
