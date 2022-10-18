#include "token.h"

#include <inttypes.h>
#include <stdio.h>

#include <utilities/attributes.h>

const int8_t _ow_token_type_precedences[(size_t)OW_TOK_OP_NOT + 1] = {
#define ELEM(NAME, STRING, PRECEDENCE) [ (size_t)OW_TOK_##NAME ] = PRECEDENCE,
		OW_TOKEN_BIN_OP_LIST
		OW_TOKEN_UN_OP_LIST
#undef ELEM
};

#pragma pack(push, 1)

static const char *const _ow_token_type_name[(size_t)_OW_TOK_COUNT] = {
#define ELEM(NAME, STRING, PRECEDENCE) [ (size_t)OW_TOK_##NAME ] = STRING,
		OW_TOKEN_BIN_OP_LIST
		OW_TOKEN_UN_OP_LIST
#undef ELEM
#define ELEM(NAME, STRING) [ (size_t)OW_TOK_##NAME ] = STRING,
		OW_TOKEN_OTHER_PUNCT_LIST
		OW_TOKEN_KEYWORDS_LIST
#undef ELEM
#define ELEM(NAME) [ (size_t)OW_TOK_##NAME ] = NULL,
		OW_TOKEN_LITERALS_LIST
#undef ELEM
	[(size_t)OW_TOK_IDENTIFIER] = NULL,
	[(size_t)OW_TOK_END_LINE  ] = "<END_LINE>",
	[(size_t)OW_TOK_END       ] = "<EOF>",
};

#pragma pack(pop)

const char *ow_token_type_represent(enum ow_token_type type) {
	const size_t type_index = (size_t)type;
	if (ow_unlikely(type_index >= (size_t)_OW_TOK_COUNT))
		return "";
	const char *const name = _ow_token_type_name[type_index];
	if (name)
		return name;
	else if (type == OW_TOK_INT)
		return "Int";
	else if (type == OW_TOK_FLOAT)
		return "Float";
	else if (type == OW_TOK_STRING)
		return "String";
	else if (type == OW_TOK_IDENTIFIER)
		return "Identifier";
	else
		return "";
}

const char *ow_token_represent(const struct ow_token *tok, char *buf, size_t buf_sz) {
	const enum ow_token_type type = ow_token_type(tok);
	const size_t type_index = (size_t)type;
	if (ow_unlikely(type_index >= (size_t)_OW_TOK_COUNT))
		return "";
	const char *const name = _ow_token_type_name[type_index];
	if (name)
		return name;
	if (type == OW_TOK_INT)
		snprintf(buf, buf_sz, "INT %" PRIi64, ow_token_value_int(tok));
	else if (type == OW_TOK_FLOAT)
		snprintf(buf, buf_sz, "FLT %f", ow_token_value_float(tok));
	else if (OW_TOK_SYMBOL <= type && type <= OW_TOK_IDENTIFIER)
		snprintf(buf, buf_sz, "%s %s",
			type == OW_TOK_STRING ? "STR" : type == OW_TOK_IDENTIFIER ? "ID" : "SYM",
			ow_sharedstr_data(ow_token_value_string(tok)));
	else
		return "";
	return buf;
}
