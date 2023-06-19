#pragma once

#include <utilities/round.h>

/// Size of object head.
#define OW_OBJECT_HEAD_SIZE (sizeof(void *) * 2)

/// Size of an object field.
#define OW_OBJECT_FIELD_SIZE sizeof(void *)

/// Size of an object, calculated according to number of fields.
#define OW_OBJECT_SIZE(OBJ_FIELD_CNT) \
    (OW_OBJECT_HEAD_SIZE + OW_OBJECT_FIELD_SIZE * (OBJ_FIELD_CNT))

/// Size of object fields, calculated according to the struct size.
#define OW_OBJ_STRUCT_DATA_SIZE(STRUCT) \
    sizeof(STRUCT) - OW_OBJECT_HEAD_SIZE

/// Number of object fields, calculated according to the struct size.
#define OW_OBJ_STRUCT_FIELD_COUNT(STRUCT) \
    (ow_round_up_to(OW_OBJECT_FIELD_SIZE, OW_OBJ_STRUCT_DATA_SIZE(STRUCT)) / OW_OBJECT_FIELD_SIZE)
