#pragma once

#include <utilities/round.h>

#define OW_OBJECT_SIZE (8 + sizeof(void *))

#define OW_OBJECT_FIELD_SIZE sizeof(void *)

#define OW_OBJ_STRUCT_DATA_SIZE(STRUCT) \
	sizeof(STRUCT) - OW_OBJECT_SIZE

#define OW_OBJ_STRUCT_FIELD_COUNT(STRUCT) \
	(ow_round_up_to(OW_OBJECT_FIELD_SIZE, OW_OBJ_STRUCT_DATA_SIZE(STRUCT)) / OW_OBJECT_FIELD_SIZE)
