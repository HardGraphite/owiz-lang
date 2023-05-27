#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "opcode.h" // enum ow_opcode
#include <utilities/attributes.h>

#define OW_OPERAND_INFO_LIST \
    /*   TYPE, WIDTH, SIGNED */ \
    ELEM(0  , 0, 0) \
    ELEM(i8 , 1, 1) \
    ELEM(u8 , 1, 0) \
    ELEM(i16, 2, 1) \
    ELEM(u16, 2, 0) \
// ^^^ OW_OPERAND_INFO_LIST ^^^

enum ow_operand_type {
#define ELEM(TYPE, WIDTH, SIGNED) OW_OPERAND_##TYPE = (SIGNED ? -WIDTH : WIDTH),
    OW_OPERAND_INFO_LIST
#undef ELEM
};

/// Operand value.
union ow_operand {
    int8_t   i8;
    uint8_t  u8;
    int16_t  i16;
    uint16_t u16;
};

/// Get operand info from opcode.
enum ow_operand_type ow_operand_type(enum ow_opcode opcode);

/// Check the operand is a signed integer.
ow_static_forceinline bool ow_operand_type_signed(enum ow_operand_type type) {
    return (int)type < 0;
}

/// Check the width operand integer (0, 1 or 2).
ow_static_forceinline unsigned int ow_operand_type_width(enum ow_operand_type type) {
    return (unsigned int)((int)type < 0 ? -(int)type : (int)type);
}
