#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "location.h"
#include <utilities/attributes.h>
#include <utilities/strings.h>

#define OW_TOKEN_BIN_OP_LIST \
    ELEM(OP_ADD        , "+"  ,  5) \
    ELEM(OP_SUB        , "-"  ,  5) \
    ELEM(OP_MUL        , "*"  ,  4) \
    ELEM(OP_DIV        , "/"  ,  4) \
    ELEM(OP_REM        , "%"  ,  4) \
    ELEM(OP_SHL        , "<<" ,  6) \
    ELEM(OP_SHR        , ">>" ,  6) \
    ELEM(OP_BIT_AND    , "&"  , 10) \
    ELEM(OP_BIT_OR     , "|"  , 12) \
    ELEM(OP_BIT_XOR    , "^"  , 11) \
    ELEM(OP_EQL        , "="  ,-15) \
    ELEM(OP_ADD_EQL    , "+=" ,-15) \
    ELEM(OP_SUB_EQL    , "-=" ,-15) \
    ELEM(OP_MUL_EQL    , "*=" ,-15) \
    ELEM(OP_DIV_EQL    , "/=" ,-15) \
    ELEM(OP_REM_EQL    , "%=" ,-15) \
    ELEM(OP_SHL_EQL    , "<<=",-15) \
    ELEM(OP_SHR_EQL    , ">>=",-15) \
    ELEM(OP_BIT_AND_EQL, "&=" ,-15) \
    ELEM(OP_BIT_OR_EQL , "|=" ,-15) \
    ELEM(OP_BIT_XOR_EQL, "^=" ,-15) \
    ELEM(OP_EQ         , "==" ,  9) \
    ELEM(OP_NE         , "!=" ,  9) \
    ELEM(OP_LT         , "<"  ,  8) \
    ELEM(OP_LE         , "<=" ,  8) \
    ELEM(OP_GT         , ">"  ,  8) \
    ELEM(OP_GE         , ">=" ,  8) \
    ELEM(OP_AND        , "&&" , 13) \
    ELEM(OP_OR         , "||" , 14) \
    ELEM(OP_PERIOD     , "."  ,  1) \
    ELEM(OP_COLON      , ":"  ,  1) \
    ELEM(OP_CALL       , "()" ,  2) \
    ELEM(OP_SUBSCRIPT  , "[]" ,  2) \
// ^^^ OW_TOKEN_BIN_OP_LIST ^^^

#define OW_TOKEN_UN_OP_LIST \
    ELEM(OP_POS    , "+"  , -3) \
    ELEM(OP_NEG    , "-"  , -3) \
    ELEM(OP_BIT_NOT, "~"  , -3) \
    ELEM(OP_NOT    , "!"  , -3) \
// ^^^ OW_TOKEN_UN_OP_LIST ^^^

#define OW_TOKEN_OTHER_PUNCT_LIST \
    ELEM(COMMA     , ","  ) \
    ELEM(HASHTAG   , "#"  ) \
    ELEM(AT        , "@"  ) \
    ELEM(QUESTION  , "?"  ) \
    ELEM(DOLLAR    , "$"  ) \
    ELEM(DOTDOT    , ".." ) \
    ELEM(ELLIPSIS  , "...") \
    ELEM(L_ARROW   , "<-" ) \
    ELEM(FATARROW  , "=>" ) \
    ELEM(L_PAREN   , "("  ) \
    ELEM(R_PAREN   , ")"  ) \
    ELEM(L_BRACKET , "["  ) \
    ELEM(R_BRACKET , "]"  ) \
    ELEM(L_BRACE   , "{"  ) \
    ELEM(R_BRACE   , "}"  ) \
// ^^^ OW_TOKEN_OTHER_PUNCT_LIST ^^^

#define OW_TOKEN_KEYWORDS_LIST \
    ELEM(KW_NIL     , "nil"     ) \
    ELEM(KW_TRUE    , "true"    ) \
    ELEM(KW_FALSE   , "false"   ) \
    ELEM(KW_SELF    , "self"    ) \
    ELEM(KW_END     , "end"     ) \
    ELEM(KW_RETURN  , "return"  ) \
    ELEM(KW_IMPORT  , "import"  ) \
    ELEM(KW_IF      , "if"      ) \
    ELEM(KW_ELIF    , "elif"    ) \
    ELEM(KW_ELSE    , "else"    ) \
    ELEM(KW_FOR     , "for"     ) \
    ELEM(KW_WHILE   , "while"   ) \
    ELEM(KW_FUNC    , "func"    ) \
// ^^^ OW_TOKEN_KEYWORDS_LIST ^^^

#define OW_TOKEN_LITERALS_LIST \
    ELEM(INT     ) \
    ELEM(FLOAT   ) \
    ELEM(SYMBOL  ) \
    ELEM(STRING  ) \
// ^^^ OW_TOKEN_LITERALS_LIST ^^^

#define OW_TOKEN_OTHERS_LIST \
    ELEM(IDENTIFIER ) \
    ELEM(END_LINE   ) \
    ELEM(END        ) \
// ^^^ OW_TOKEN_LITERALS_LIST ^^^

/// Token type.
enum ow_token_type {
#define ELEM(NAME, STRING, PRECEDENCE) OW_TOK_##NAME,
    OW_TOKEN_BIN_OP_LIST
    OW_TOKEN_UN_OP_LIST
#undef ELEM
#define ELEM(NAME, STRING) OW_TOK_##NAME,
    OW_TOKEN_OTHER_PUNCT_LIST
    OW_TOKEN_KEYWORDS_LIST
#undef ELEM
#define ELEM(NAME) OW_TOK_##NAME,
    OW_TOKEN_LITERALS_LIST
    OW_TOKEN_OTHERS_LIST
#undef ELEM
    _OW_TOK_COUNT
};

extern const int8_t _ow_token_type_precedences[(size_t)OW_TOK_OP_NOT + 1];

/// Token is an operator.
ow_static_inline bool ow_token_type_is_operator(enum ow_token_type tp) {
    static_assert((int)OW_TOK_OP_ADD == 0, ""); // OW_TOK_OP_ADD <= tp
    return (int)tp <= (int)OW_TOK_OP_NOT;
}

/// Token contains a value.
ow_static_inline bool ow_token_type_has_value(enum ow_token_type tp) {
    return (int)OW_TOK_INT <= (int)tp && (int)tp <= (int)OW_TOK_IDENTIFIER;
}

/// Get operator precedence. Return 0 for non-operator token types.
ow_static_inline int8_t ow_token_type_precedence(enum ow_token_type tp) {
    if (ow_unlikely((size_t)tp >= (size_t)OW_TOK_OP_NOT + 1))
        return 0;
    return _ow_token_type_precedences[(size_t)tp];
}

/// Represent token type as text.
const char *ow_token_type_represent(enum ow_token_type tp);

/// Lexical token. DO NOT access members directly except `location`.
struct ow_token {
    enum ow_token_type _type;
    union {
        int64_t i;
        double  f;
        struct ow_sharedstr *s;
    } _data;
    struct ow_source_range location;
};

/// Initialize a token.
ow_static_inline void ow_token_init(struct ow_token *tok) {
    tok->_type = (enum ow_token_type)0;
    tok->_data.s = NULL;
    tok->location.begin.line = 0;
    tok->location.begin.column = 0;
    tok->location.end.line = 0;
    tok->location.end.column = 0;
}

/// Finalize a token.
ow_static_inline void ow_token_fini(struct ow_token *tok) {
    if (OW_TOK_SYMBOL <= tok->_type && tok->_type <= OW_TOK_IDENTIFIER)
        ow_sharedstr_unref(tok->_data.s);
}

/// Get token type.
ow_static_forceinline enum ow_token_type ow_token_type(
    const struct ow_token *tok
) {
    return tok->_type;
}

/// Get token int value.
ow_static_forceinline int64_t ow_token_value_int(const struct ow_token *tok) {
    assert(tok->_type == OW_TOK_INT);
    return tok->_data.i;
}

/// Get token float value.
ow_static_forceinline double ow_token_value_float(const struct ow_token *tok) {
    assert(tok->_type == OW_TOK_FLOAT);
    return tok->_data.f;
}

/// Get token string value.
ow_static_forceinline struct ow_sharedstr *
ow_token_value_string(const struct ow_token *tok) {
    assert(OW_TOK_SYMBOL <= tok->_type && tok->_type <= OW_TOK_IDENTIFIER);
    return tok->_data.s;
}

/// Represent type and value of the token as text.
const char *ow_token_represent(const struct ow_token *tok, char *buf, size_t buf_sz);

/// Assign a simple type, without value.
ow_static_inline void ow_token_assign_simple(
    struct ow_token *tok, enum ow_token_type tp
) {
    assert(!ow_token_type_has_value(tp));
    ow_token_fini(tok);
    tok->_type = tp;
}

/// Assign type with int value.
ow_static_inline void ow_token_assign_int(
    struct ow_token *tok, enum ow_token_type tp, int64_t val
) {
    assert(tp == OW_TOK_INT);
    ow_token_fini(tok);
    tok->_type = tp;
    tok->_data.i = val;
}

/// Assign type with float value.
ow_static_inline void ow_token_assign_float(
    struct ow_token *tok, enum ow_token_type tp, double val
) {
    assert(tp == OW_TOK_FLOAT);
    ow_token_fini(tok);
    tok->_type = tp;
    tok->_data.f = val;
}

/// Assign type with string value.
ow_static_inline void ow_token_assign_string(
    struct ow_token *tok, enum ow_token_type tp, struct ow_sharedstr *val
) {
    assert(OW_TOK_SYMBOL <= tp && tp <= OW_TOK_IDENTIFIER);
    ow_token_fini(tok);
    tok->_type = tp;
    tok->_data.s = ow_sharedstr_ref(val);
}

/// Move token value to another.
ow_static_inline void ow_token_move(
    struct ow_token *dest, struct ow_token *src
) {
    *dest = *src;
    src->_type = OW_TOK_END;
}
