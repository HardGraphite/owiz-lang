#include "operand.h"

#include <stddef.h>

static const enum ow_operand_type _operand_types[(size_t)_OW_OPC_COUNT] = {
#define ELEM(NAME, CODE, OPERAND_TYPE) \
        [ (size_t) OW_OPC_##NAME ] = OW_OPERAND_##OPERAND_TYPE ,
    OW_OPCODE_LIST
#undef ELEM
};

enum ow_operand_type ow_operand_type(enum ow_opcode opcode) {
    const size_t index = (size_t)opcode;
    if (ow_unlikely(index >= (size_t)_OW_OPC_COUNT))
        return 0;
    return _operand_types[index];
}
