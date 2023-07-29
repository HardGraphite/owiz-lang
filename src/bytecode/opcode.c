#include "opcode.h"

#include <assert.h>
#include <stddef.h>

#include <utilities/attributes.h>

static_assert((size_t)_OW_OPC_COUNT <= 128, "too many opcodes");

#pragma pack(push, 1)

static const char *const _opcode_names[(size_t)_OW_OPC_COUNT] = {
#define ELEM(NAME, CODE, OPERAND_TYPE) [ (size_t) OW_OPC_##NAME ] = #NAME ,
    OW_OPCODE_LIST
#undef ELEM
};

#pragma pack(pop)

const char *ow_opcode_name(enum ow_opcode opcode) {
    const size_t index = (size_t)opcode;
    if (ow_unlikely(index >= (size_t)_OW_OPC_COUNT))
        return NULL;
    return _opcode_names[index];
}
