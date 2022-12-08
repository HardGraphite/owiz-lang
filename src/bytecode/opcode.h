#pragma once

#define OW_OPCODE_LIST \
	/*   NAME       , CODE, OPERAND_INFO */ \
	ELEM(Nop        , 0x00,   0) \
	ELEM(_01        , 0x01,   0) \
	ELEM(Trap       , 0x02,   0) \
	ELEM(_03        , 0x03,   0) \
	ELEM(Swap       , 0x04,   0) \
	ELEM(SwapN      , 0x05,  u8) \
	ELEM(Drop       , 0x06,   0) \
	ELEM(DropN      , 0x07,  u8) \
	ELEM(Dup        , 0x08,   0) \
	ELEM(DupN       , 0x09,  u8) \
	ELEM(LdNil      , 0x0a,   0) \
	ELEM(LdBool     , 0x0b,  u8) \
	ELEM(LdInt      , 0x0c,  i8) \
	ELEM(LdIntW     , 0x0d, i16) \
	ELEM(LdFlt      , 0x0e,  i8) \
	ELEM(_0f        , 0x0f,   0) \
	ELEM(Add        , 0x10,   0) \
	ELEM(Sub        , 0x11,   0) \
	ELEM(Mul        , 0x12,   0) \
	ELEM(Div        , 0x13,   0) \
	ELEM(Rem        , 0x14,   0) \
	ELEM(Shl        , 0x15,   0) \
	ELEM(Shr        , 0x16,   0) \
	ELEM(And        , 0x17,   0) \
	ELEM(Or         , 0x18,   0) \
	ELEM(Xor        , 0x19,   0) \
	ELEM(Neg        , 0x1a,   0) \
	ELEM(Inv        , 0x1b,   0) \
	ELEM(Not        , 0x1c,   0) \
	ELEM(Test       , 0x1d,   0) \
	ELEM(_1e        , 0x1e,   0) \
	ELEM(_1f        , 0x1f,   0) \
	ELEM(Is         , 0x20,   0) \
	ELEM(Cmp        , 0x21,   0) \
	ELEM(CmpLt      , 0x22,   0) \
	ELEM(CmpLe      , 0x23,   0) \
	ELEM(CmpGt      , 0x24,   0) \
	ELEM(CmpGe      , 0x25,   0) \
	ELEM(CmpEq      , 0x26,   0) \
	ELEM(CmpNe      , 0x27,   0) \
	ELEM(LdCnst     , 0x28,  u8) \
	ELEM(LdCnstW    , 0x29, u16) \
	ELEM(LdSym      , 0x2a,  u8) \
	ELEM(LdSymW     , 0x2b, u16) \
	ELEM(LdArg      , 0x2c,  u8) \
	ELEM(StArg      , 0x2d,  u8) \
	ELEM(LdLoc      , 0x2e,  u8) \
	ELEM(LdLocW     , 0x2f, u16) \
	ELEM(StLoc      , 0x30,  u8) \
	ELEM(StLocW     , 0x31, u16) \
	ELEM(LdGlob     , 0x32,  u8) \
	ELEM(LdGlobW    , 0x33, u16) \
	ELEM(StGlob     , 0x34,  u8) \
	ELEM(StGlobW    , 0x35, u16) \
	ELEM(LdGlobY    , 0x36,  u8) \
	ELEM(LdGlobYW   , 0x37, u16) \
	ELEM(StGlobY    , 0x38,  u8) \
	ELEM(StGlobYW   , 0x39, u16) \
	ELEM(LdAttrY    , 0x3a,  u8) \
	ELEM(LdAttrYW   , 0x3b, u16) \
	ELEM(StAttrY    , 0x3c,  u8) \
	ELEM(StAttrYW   , 0x3d, u16) \
	ELEM(LdElem     , 0x3e,   0) \
	ELEM(StElem     , 0x3f,   0) \
	ELEM(Jmp        , 0x40,  i8) \
	ELEM(JmpW       , 0x41, i16) \
	ELEM(JmpWhen    , 0x42,  i8) \
	ELEM(JmpWhenW   , 0x43, i16) \
	ELEM(JmpUnls    , 0x44,  i8) \
	ELEM(JmpUnlsW   , 0x45, i16) \
	ELEM(LdMod      , 0x46, u16) \
	ELEM(_47        , 0x47,   0) \
	ELEM(Ret        , 0x48,   0) \
	ELEM(RetLoc     , 0x49,  u8) \
	ELEM(Call       , 0x4a,  u8) \
	ELEM(_4b        , 0x4b,   0) \
	ELEM(_4c        , 0x4c,   0) \
	ELEM(_4d        , 0x4d,   0) \
	ELEM(PrepMethY  , 0x4e,  u8) \
	ELEM(PrepMethYW , 0x4f, u16) \
// ^^^ OW_OPCODE_LIST ^^^

/// Opcodes.
enum ow_opcode {
#define ELEM(NAME, CODE, OPERAND_SIZE) OW_OPC_##NAME = CODE ,
	OW_OPCODE_LIST
#undef ELEM
	_OW_OPC_COUNT
};

/// Get name of an opcode. Return NULL if not valid.
const char *ow_opcode_name(enum ow_opcode opcode);
