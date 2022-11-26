#include "lexer.h"

#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h> // EOF
#include <stdlib.h> // atexit()
#include <string.h>

#include "error.h"
#include "location.h"
#include "token.h"
#include <utilities/attributes.h>
#include <utilities/hash.h>
#include <utilities/hashmap.h>
#include <utilities/malloc.h>
#include <utilities/stream.h>
#include <utilities/strings.h>
#include <utilities/unicode.h>

#include <config/options.h>

static struct ow_hashmap _ow_lexer_keywords_map;

#define OW_LEXER_KEYWORD_MAXLEN 8

static bool _ow_lexer_keywords_map_key_equal(
		void *ctx, const void *key1, const void *key2) {
	ow_unused_var(ctx);
	return strcmp(key1, key2) == 0;
}

static ow_hash_t _ow_lexer_keywords_map_key_hash(void *ctx, const void *key) {
	ow_unused_var(ctx);
	return ow_hash_bytes(key, strlen(key));
}

static const struct ow_hashmap_funcs _ow_lexer_keywords_map_funcs = {
	.key_equal = _ow_lexer_keywords_map_key_equal,
	.key_hash = _ow_lexer_keywords_map_key_hash,
	.context = NULL,
};

ow_noinline static void _ow_lexer_keywords_map_fini(void) {
	assert(_ow_lexer_keywords_map._size != 0);

	ow_hashmap_fini(&_ow_lexer_keywords_map);
	_ow_lexer_keywords_map._size = 0;
}

ow_noinline static void _ow_lexer_keywords_map_init(void) {
	assert(_ow_lexer_keywords_map._size == 0);
	// TODO: Add atomic flag.

	const char *const kw_names[] = {
#define ELEM(NAME, STRING) STRING,
		OW_TOKEN_KEYWORDS_LIST
#undef ELEM
	};
	const enum ow_token_type kw_tokens[] = {
#define ELEM(NAME, STRING) OW_TOK_##NAME,
			OW_TOKEN_KEYWORDS_LIST
#undef ELEM
	};
	const size_t kw_count = sizeof kw_names / sizeof kw_names[0];

	ow_hashmap_init(&_ow_lexer_keywords_map, kw_count);
	for (size_t i = 0; i < kw_count; i++) {
		ow_hashmap_set(
			&_ow_lexer_keywords_map, &_ow_lexer_keywords_map_funcs,
			kw_names[i], (void *)(uintptr_t)kw_tokens[i]);
	}
	assert(ow_hashmap_size(&_ow_lexer_keywords_map) == kw_count);

	atexit(_ow_lexer_keywords_map_fini);
}

/// Call this function to make sure that the map has been initialized.
ow_forceinline static void ow_lexer_keywords_map_prepare(void) {
	if (ow_likely(_ow_lexer_keywords_map._size))
		return;
	_ow_lexer_keywords_map_init();
	assert(_ow_lexer_keywords_map._size);
}

/// Find keyword by string. If it is not a keyword, return 0.
static enum ow_token_type ow_lexer_keywords_map_query(const char *s) {
	assert(_ow_lexer_keywords_map._size);
	const void *res =
		ow_hashmap_get(&_ow_lexer_keywords_map, &_ow_lexer_keywords_map_funcs, s);
	return (enum ow_token_type)(uintptr_t)res;
}

#define OW_LEXER_CODE_BUFFER_SIZE 128
#define OW_LEXER_CODE_BUFFER_END(LEXER_P) \
	((LEXER_P)->code_buffer + OW_LEXER_CODE_BUFFER_SIZE)

struct ow_lexer {
	struct ow_istream *stream;
	struct ow_source_location location;
	const char *code_current;
	const char *code_end;
	char code_buffer[OW_LEXER_CODE_BUFFER_SIZE];
	struct ow_dynamicstr string_buffer;
	struct ow_sharedstr *file_name;
	jmp_buf error_jmpbuf;
	struct ow_syntax_error error_info;
	bool stream_end;
#if OW_DEBUG_LEXER
	bool verbose;
#endif // OW_DEBUG_LEXER
};

/// A wrapper of `setjmp()`.
#define ow_lexer_error_setjmp(lexer) \
	(setjmp((lexer)->error_jmpbuf))

/// Throw error.
ow_noinline ow_noreturn static void ow_lexer_error_throw(
		struct ow_lexer *lexer, const char *msg_fmt, ...) {
	va_list ap;
	va_start(ap, msg_fmt);
	ow_syntax_error_vformat(
		&lexer->error_info,
		&(struct ow_source_range){lexer->location, lexer->location},
		msg_fmt, ap);
	va_end(ap);
	longjmp(lexer->error_jmpbuf, 1);
}

ow_noinline static int ow_lexer_code_refill_and_peek(struct ow_lexer *lexer) {
	assert(lexer->code_current == lexer->code_end);
	const size_t n = ow_istream_read(
		lexer->stream, lexer->code_buffer, OW_LEXER_CODE_BUFFER_SIZE);
	if (ow_unlikely(!n))
		return EOF;
	lexer->code_current = lexer->code_buffer;
	lexer->code_end = lexer->code_current + n;
	return *lexer->code_current;
}

/// Peek next byte.
ow_forceinline static int ow_lexer_code_peek(struct ow_lexer *lexer) {
	assert(lexer->code_current <= lexer->code_end);
	if (ow_unlikely(lexer->code_current == lexer->code_end))
		return ow_lexer_code_refill_and_peek(lexer);
	return *lexer->code_current;
}

/// Move to next byte.
static void ow_lexer_code_advance(struct ow_lexer *lexer) {
	assert(lexer->code_current < lexer->code_end);
	const char c = *lexer->code_current;
	const size_t n = ow_u8_charlen((const ow_char8_t)c);
	if (ow_likely(n == 1)) {
		if (ow_likely(*lexer->code_current == '\n'))
			lexer->location.line++, lexer->location.column = 1;
		else
			lexer->location.column++;
	} else if (ow_likely(n)) {
		lexer->location.column++;
	}
	lexer->code_current++;
}

/// Ignore chars until `c`. Return false if reaches EOF before `c`.
static bool ow_lexer_code_ignore_until(
		struct ow_lexer *lexer, char c, bool ignore_c) {
	assert(lexer->code_current <= lexer->code_end);
	const char *const pos =
		memchr(lexer->code_current, c, lexer->code_end - lexer->code_current);
	if (pos) {
		while (lexer->code_current < pos)
			ow_lexer_code_advance(lexer);
		if (ignore_c)
			ow_lexer_code_advance(lexer);
		return true;
	}
	while (lexer->code_current != lexer->code_end)
		ow_lexer_code_advance(lexer); // TODO: Need optimization.
	if (ow_unlikely(ow_lexer_code_peek(lexer) == EOF))
		return false;
	return ow_lexer_code_ignore_until(lexer, c, ignore_c);
}

ow_nodiscard struct ow_lexer *ow_lexer_new(void) {
	struct ow_lexer *const lexer = ow_malloc(sizeof(struct ow_lexer));
	lexer->stream = NULL;
	lexer->location.line = 0;
	lexer->location.column = 0;
	lexer->code_current = NULL;
	lexer->code_end = NULL;
	lexer->file_name = NULL;
	ow_syntax_error_init(&lexer->error_info);
	lexer->stream_end = true;
	ow_dynamicstr_init(&lexer->string_buffer, 63);
#if OW_DEBUG_LEXER
	lexer->verbose = false;
#endif // OW_DEBUG_LEXER

	ow_lexer_keywords_map_prepare();

	return lexer;
}

void ow_lexer_del(struct ow_lexer *lexer) {
	ow_lexer_clear(lexer);
	ow_syntax_error_fini(&lexer->error_info);
	ow_dynamicstr_fini(&lexer->string_buffer);
	ow_free(lexer);
}

void ow_lexer_verbose(struct ow_lexer *lexer, bool status) {
	ow_unused_var(lexer);
	ow_unused_var(status);
#if OW_DEBUG_LEXER
	lexer->verbose = status;
#endif // OW_DEBUG_LEXER
}

void ow_lexer_source(struct ow_lexer *lexer,
		struct ow_istream *stream, struct ow_sharedstr *file_name) {
	ow_lexer_clear(lexer);
	lexer->stream = stream;
	lexer->location.line = 1;
	lexer->location.column = 1;
	lexer->code_current = OW_LEXER_CODE_BUFFER_END(lexer);
	lexer->code_end = lexer->code_current;
	lexer->file_name = ow_sharedstr_ref(file_name);
	lexer->stream_end = false;
}

void ow_lexer_clear(struct ow_lexer *lexer) {
	if (lexer->file_name) {
		ow_sharedstr_unref(lexer->file_name);
		lexer->file_name = NULL;
	}
	ow_syntax_error_clear(&lexer->error_info);
	ow_dynamicstr_clear(&lexer->string_buffer);
}

/// Read a int or float literal.
static void ow_lexer_scan_number(
		struct ow_lexer *lexer, struct ow_token *result) {
	int64_t int_val;
	double  float_val;
	int64_t int_base;
	int c;

	c = ow_lexer_code_peek(lexer);
	assert(isdigit(c));
	if (ow_unlikely(c == '0')) {
		ow_lexer_code_advance(lexer);
		c = ow_lexer_code_peek(lexer);
		if (isalpha(c)) {
			c = tolower(c);
			if (c == 'x')
				int_base = 16;
			else if (c == 'b')
				int_base = 2;
			else if (c == 'o')
				int_base = 8;
			else
				goto only_0;
			ow_lexer_code_advance(lexer);
		} else if (isdigit(c)) {
			int_base = 8;
		} else {
		only_0:
			ow_token_assign_int(result, OW_TOK_INT, 0);
			return;
		}
	} else {
		int_base = 10;
	}

	int_val = 0;
	for (size_t i = 0; ; i++) {
		int64_t digit;
		c = ow_lexer_code_peek(lexer);
		if (ow_likely(isdigit(c))) {
			digit = c - '0';
		} else if (islower(c)) {
			digit = c - 'a' + 0xa;
		} else if (isupper(c)) {
			digit = c - 'A' + 0xa;
		} else if (c == '_' || c == '\'') {
			ow_lexer_code_advance(lexer);
			continue;
		} else {
			if (!i)
				ow_lexer_error_throw(lexer, "invalid number literal");
			break;
		}
		ow_lexer_code_advance(lexer);
		const int64_t new_int_val = int_val * int_base + digit;
		if (ow_unlikely(new_int_val < int_val))
			ow_lexer_error_throw(lexer, "number literal overflow");
		int_val = new_int_val;
	}

	c = ow_lexer_code_peek(lexer);
	if (c == '.' && int_base == 10) {
		ow_lexer_code_advance(lexer);
		float_val = (double)int_val;
		for (unsigned int ratio = 10; ; ratio *= 10) {
			int digit;
			c = ow_lexer_code_peek(lexer);
			if (ow_likely(isdigit(c))) {
				digit = c - '0';
			} else if (c == '_' || c == '\'') {
				ow_lexer_code_advance(lexer);
				continue;
			} else {
				if (ratio == 10)
					ow_lexer_error_throw(lexer, "invalid floating-point literal");
				break;
			}
			ow_lexer_code_advance(lexer);
			float_val += (double)digit / (double)ratio;
		}
		ow_token_assign_float(result, OW_TOK_FLOAT, float_val);
	} else {
		ow_token_assign_int(result, OW_TOK_INT, int_val);
	}
}

static ow_wchar_t ow_lexer_scan_string_esc_hex_seq(
		struct ow_lexer *lexer, size_t max_len) {
	ow_wchar_t res = 0;
	for (size_t i = 0; i < max_len; i++) {
		const int c = ow_lexer_code_peek(lexer);
		if (ow_unlikely(c == EOF))
			break;
		ow_lexer_code_advance(lexer);
		ow_wchar_t digit;
		if (isdigit(c))
			digit = c - '0';
		else if (isalpha(c))
			digit = tolower(c) - 'a' + 0xa;
		else
			break;
		res = res * 16 + digit;
	}
	return res;
}

static ow_wchar_t ow_lexer_scan_string_esc(struct ow_lexer *lexer) {
	assert(ow_lexer_code_peek(lexer) == '\\');
	ow_lexer_code_advance(lexer);

	const int c1 = ow_lexer_code_peek(lexer);
	if (ow_unlikely(c1 == EOF))
		ow_lexer_error_throw(lexer, "missing terminating quotation mark");
	ow_lexer_code_advance(lexer);

	ow_wchar_t res;
	switch ((char)c1) {
	case 'a':
		res = '\a';
		break;

	case 'b':
		res = '\b';
		break;

	case 'e':
		res = 0x1b;
		break;

	case 'f':
		res = '\f';
		break;

	case 'n':
		res = '\n';
		break;

	case 'r':
		res = '\r';
		break;

	case 't':
		res = '\t';
		break;

	case 'v':
		res = '\v';
		break;

	case 'x':
		res = ow_lexer_scan_string_esc_hex_seq(lexer, 2);
		break;

	case 'u':
		res = ow_lexer_scan_string_esc_hex_seq(lexer, 4);
		break;

	case 'U':
		res = ow_lexer_scan_string_esc_hex_seq(lexer, 8);
		break;

	default:
		res = c1 == '0' ? 0 : c1;
		break;
	}

	return res;
}

static void ow_lexer_scan_string_u8c_to(
		struct ow_lexer *lexer, struct ow_dynamicstr *buffer) {
	const int c = ow_lexer_code_peek(lexer);
	assert(c != EOF); // Check before calling this function.
	ow_lexer_code_advance(lexer);
	const size_t len = ow_u8_charlen((ow_char8_t)c);
	if (len == 1) {
		ow_dynamicstr_append_char(buffer, (char)c);
	} else if (len > 1) {
		assert(len <= 4);
		char chars[4];
		chars[0] = (char)c;
		for (size_t i = 1; i < len; i++) {
			const int ci = ow_lexer_code_peek(lexer);
			if (ow_unlikely(ci == EOF))
				ow_lexer_error_throw(lexer, "incomplete UTF-8 character");
			ow_lexer_code_advance(lexer);
			chars[i] = (char)ci;
		}
		const int n = ow_u8_charlen_s((ow_char8_t *)chars, len);
		if (ow_unlikely(n != (int)len))
			ow_lexer_error_throw(lexer, "illegal UTF-8 character");
		ow_dynamicstr_append(buffer, chars, len);
	} else {
		ow_lexer_error_throw(
			lexer, "`\\x%02x' is not a valid UTF-8 starting byte", c);
	}
}

/// Read a string literal.
static void ow_lexer_scan_string(
		struct ow_lexer *lexer, struct ow_token *result) {
	struct ow_dynamicstr *buffer = &lexer->string_buffer;
	ow_dynamicstr_clear(buffer);

	const int q_mark = ow_lexer_code_peek(lexer);
	ow_lexer_code_advance(lexer);
	assert(q_mark == '"' || q_mark == '\'');

	while (1) {
		const int c = ow_lexer_code_peek(lexer);
		if (ow_unlikely(c == EOF)) {
			ow_lexer_error_throw(lexer, "missing terminating quotation mark");
		} else if (ow_unlikely(c == q_mark)) {
			ow_lexer_code_advance(lexer);
			break;
		} else if (ow_unlikely(c == '\\')) {
			char chars[4];
			const size_t n = ow_u8_from_unicode(
				ow_lexer_scan_string_esc(lexer), (ow_char8_t *)chars);
			ow_dynamicstr_append(buffer, chars, n);
		} else {
			ow_lexer_scan_string_u8c_to(lexer, buffer);
		}
	}

	struct ow_sharedstr *const res_str =
			ow_sharedstr_new(ow_dynamicstr_data(buffer), ow_dynamicstr_size(buffer));
	ow_token_assign_string(result, OW_TOK_STRING, res_str);
	ow_sharedstr_unref(res_str);
}

/// Read an identifier or keyword.
static void ow_lexer_scan_identifier_or_keyword(
		struct ow_lexer *lexer, struct ow_token *result, bool is_symbol) {
	struct ow_dynamicstr *buffer = &lexer->string_buffer;
	ow_dynamicstr_clear(buffer);

	while (1) {
		const int c0 = ow_lexer_code_peek(lexer);
		if (ow_unlikely(c0 == EOF))
			break;
		if (ow_unlikely(!(isalnum(c0) || c0 == '_')))
			break;
		ow_lexer_scan_string_u8c_to(lexer, buffer);
	}

	const char *const buffer_data = ow_dynamicstr_data(buffer);
	const size_t buffer_size = ow_dynamicstr_size(buffer);

	if (ow_unlikely(is_symbol)) {
		if (ow_unlikely(!buffer_size))
			ow_lexer_error_throw(lexer, "empty symbol literal");
		struct ow_sharedstr *const res_str =
			ow_sharedstr_new(ow_dynamicstr_data(buffer), ow_dynamicstr_size(buffer));
		ow_token_assign_string(result, OW_TOK_SYMBOL, res_str);
		ow_sharedstr_unref(res_str);
		return;
	}

	assert(buffer_size);

	if (buffer_size <= OW_LEXER_KEYWORD_MAXLEN) {
		assert(buffer_data[buffer_size] == '\0');
		const enum ow_token_type tok_tp = ow_lexer_keywords_map_query(buffer_data);
		if (ow_unlikely((int)tok_tp)) {
			ow_token_assign_simple(result, tok_tp);
			return;
		}
	}

	struct ow_sharedstr *const res_str =
		ow_sharedstr_new(ow_dynamicstr_data(buffer), ow_dynamicstr_size(buffer));
	ow_token_assign_string(result, OW_TOK_IDENTIFIER, res_str);
	ow_sharedstr_unref(res_str);
}

static void ow_lexer_next_impl(
		struct ow_lexer *lexer, struct ow_token *result) {
	struct ow_source_location loc_begin;

	while (1) {
		loc_begin = lexer->location;
		const int c = ow_lexer_code_peek(lexer);

		if (ow_unlikely(c == EOF)) {
			enum ow_token_type tok_type;
			if (lexer->stream_end) {
				tok_type = OW_TOK_END;
			} else {
				lexer->stream_end = true;
				tok_type = OW_TOK_END_LINE;
			}
			ow_token_assign_simple(result, tok_type);
			result->location.begin = lexer->location;
			result->location.end = lexer->location;
			return;
		}

		switch ((char) c) {
			int c1;

		case '\0':
		case '\x01':
		case '\x02':
		case '\x03':
		case '\x04':
		case '\x05':
		case '\x06':
		case '\a':
		case '\b':
		case '\v':
		case '\f':
		case '\r':
		case '\x0e':
		case '\x0f':
		case '\x10':
		case '\x11':
		case '\x12':
		case '\x13':
		case '\x14':
		case '\x15':
		case '\x16':
		case '\x17':
		case '\x18':
		case '\x19':
		case '\x1a':
		case '\x1b':
		case '\x1c':
		case '\x1d':
		case '\x1e':
		case '\x1f':
		case '\x7f':
			ow_lexer_error_throw(lexer, "unexpected byte `\\x%02x'", c);

		case '\t':
		case ' ':
			ow_lexer_code_advance(lexer);
			continue;

		case '\n':
		case ';':
			ow_token_assign_simple(result, OW_TOK_END_LINE);
			goto advance_and_set_loc_and_return;

		case '!':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_NE);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_NOT);
				goto set_loc_and_return;
			}

		case '"':
		case '\'':
			ow_lexer_scan_string(lexer, result);
			goto set_loc_and_return;

		case '#':
			ow_lexer_code_advance(lexer);
			if (ow_likely(ow_lexer_code_peek(lexer) != '=')) {
				ow_lexer_code_ignore_until(lexer, '\n', false);
				ow_token_assign_simple(result, OW_TOK_END_LINE);
				goto advance_and_set_loc_and_return;
			} else {
				ow_lexer_code_advance(lexer);
				while (ow_lexer_code_ignore_until(lexer, '=', true)) {
					if (ow_lexer_code_peek(lexer) == '#') {
						ow_lexer_code_advance(lexer);
						break;
					}
				}
				continue;
			}

		case '$':
			ow_token_assign_simple(result, OW_TOK_DOLLAR);
			goto advance_and_set_loc_and_return;

		case '%':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_REM_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_REM);
				goto set_loc_and_return;
			}

		case '&':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '&') {
				ow_token_assign_simple(result, OW_TOK_OP_AND);
				goto advance_and_set_loc_and_return;
			} else if (c1 == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_BIT_AND_EQL);
				goto advance_and_set_loc_and_return;
			}
			ow_token_assign_simple(result, OW_TOK_OP_BIT_AND);
			goto set_loc_and_return;

		case '(':
			ow_token_assign_simple(result, OW_TOK_L_PAREN);
			goto advance_and_set_loc_and_return;

		case ')':
			ow_token_assign_simple(result, OW_TOK_R_PAREN);
			goto advance_and_set_loc_and_return;

		case '*':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_MUL_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_MUL);
				goto set_loc_and_return;
			}

		case '+':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_ADD_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_ADD);
				goto set_loc_and_return;
			}

		case ',':
			ow_token_assign_simple(result, OW_TOK_COMMA);
			goto advance_and_set_loc_and_return;

		case '-':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_SUB_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_SUB);
				goto set_loc_and_return;
			}

		case '.':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '.') {
				ow_lexer_code_advance(lexer);
				c1 = ow_lexer_code_peek(lexer);
				if (c1 == '.') {
					ow_token_assign_simple(result, OW_TOK_ELLIPSIS);
					goto advance_and_set_loc_and_return;
				} else {
					ow_token_assign_simple(result, OW_TOK_DOTDOT);
					goto set_loc_and_return;
				}
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_PERIOD);
				goto set_loc_and_return;
			}

		case '/':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_DIV_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_DIV);
				goto set_loc_and_return;
			}

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			ow_lexer_scan_number(lexer, result);
			goto set_loc_and_return;

		case ':':
			ow_token_assign_simple(result, OW_TOK_OP_COLON);
			goto advance_and_set_loc_and_return;

		case '<':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '<') {
				ow_lexer_code_advance(lexer);
				c1 = ow_lexer_code_peek(lexer);
				if (c1 == '=') {
					ow_token_assign_simple(result, OW_TOK_OP_SHL_EQL);
					goto advance_and_set_loc_and_return;
				} else {
					ow_token_assign_simple(result, OW_TOK_OP_SHL);
					goto set_loc_and_return;
				}
			} else if (c1 == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_LE);
				goto advance_and_set_loc_and_return;
			} else if (c1 == '-') {
				ow_token_assign_simple(result, OW_TOK_L_ARROW);
				goto advance_and_set_loc_and_return;
			}
			ow_token_assign_simple(result, OW_TOK_OP_LT);
			goto set_loc_and_return;

		case '=':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_EQ);
				goto advance_and_set_loc_and_return;
			} else if (c1 == '>') {
				ow_token_assign_simple(result, OW_TOK_FATARROW);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_EQL);
				goto set_loc_and_return;
			}

		case '>':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '>') {
				ow_lexer_code_advance(lexer);
				c1 = ow_lexer_code_peek(lexer);
				if (c1 == '=') {
					ow_token_assign_simple(result, OW_TOK_OP_SHR_EQL);
					goto advance_and_set_loc_and_return;
				} else {
					ow_token_assign_simple(result, OW_TOK_OP_SHR);
					goto set_loc_and_return;
				}
			} else if (c1 == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_GE);
				goto advance_and_set_loc_and_return;
			}
			ow_token_assign_simple(result, OW_TOK_OP_GT);
			goto set_loc_and_return;

		case '?':
			ow_token_assign_simple(result, OW_TOK_QUESTION);
			goto advance_and_set_loc_and_return;

		case '@':
			ow_token_assign_simple(result, OW_TOK_AT);
			goto advance_and_set_loc_and_return;

		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		case '_':
			ow_lexer_scan_identifier_or_keyword(lexer, result, false);
			goto set_loc_and_return;

		case '[':
			ow_token_assign_simple(result, OW_TOK_L_BRACKET);
			goto advance_and_set_loc_and_return;

		case '\\':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '\n') {
				ow_lexer_code_advance(lexer);
				continue;
			} else {
				ow_lexer_error_throw(lexer, "unexpected character after `\\'");
			}

		case ']':
			ow_token_assign_simple(result, OW_TOK_R_BRACKET);
			goto advance_and_set_loc_and_return;

		case '^':
			ow_lexer_code_advance(lexer);
			if (ow_lexer_code_peek(lexer) == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_BIT_XOR_EQL);
				goto advance_and_set_loc_and_return;
			} else {
				ow_token_assign_simple(result, OW_TOK_OP_BIT_XOR);
				goto set_loc_and_return;
			}

		case '`':
			ow_lexer_code_advance(lexer);
			ow_lexer_scan_identifier_or_keyword(lexer, result, true);
			goto set_loc_and_return;

		case '{':
			ow_token_assign_simple(result, OW_TOK_L_BRACE);
			goto advance_and_set_loc_and_return;

		case '|':
			ow_lexer_code_advance(lexer);
			c1 = ow_lexer_code_peek(lexer);
			if (c1 == '|') {
				ow_token_assign_simple(result, OW_TOK_OP_OR);
				goto advance_and_set_loc_and_return;
			} else if (c1 == '=') {
				ow_token_assign_simple(result, OW_TOK_OP_BIT_OR_EQL);
				goto advance_and_set_loc_and_return;
			}
			ow_token_assign_simple(result, OW_TOK_OP_BIT_OR);
			goto set_loc_and_return;

		case '}':
			ow_token_assign_simple(result, OW_TOK_R_BRACE);
			goto advance_and_set_loc_and_return;

		case '~':
			ow_token_assign_simple(result, OW_TOK_OP_BIT_NOT);
			goto advance_and_set_loc_and_return;

		default:
			assert((char) c & 0x80);
			ow_lexer_scan_identifier_or_keyword(lexer, result, false);
			goto set_loc_and_return;
		}
	}

advance_and_set_loc_and_return:
	ow_lexer_code_advance(lexer);
set_loc_and_return:
	result->location.begin = loc_begin;
	result->location.end = lexer->location;
	result->location.end.column--;
}

#if OW_DEBUG_LEXER

ow_noinline static void ow_lexer_dump_tok(const struct ow_token *tok) {
	char tok_rep_buf[128];
	const char *const tok_rep =
		ow_token_represent(tok, tok_rep_buf, sizeof tok_rep_buf);
	fprintf(stderr, "[Token] (%u:%u-%u:%u) %s\n",
		tok->location.begin.line, tok->location.begin.column,
		tok->location.end.line, tok->location.end.column, tok_rep);
}

#endif // OW_DEBUG_LEXER

bool ow_lexer_next(struct ow_lexer *lexer, struct ow_token *result) {
	if (!ow_lexer_error_setjmp(lexer)) {
		ow_lexer_next_impl(lexer, result);
#if OW_DEBUG_LEXER
		if (ow_unlikely(lexer->verbose))
			ow_lexer_dump_tok(result);
#endif // OW_DEBUG_LEXER
		return true;
	} else {
		assert(ow_lexer_error(lexer));
		return false;
	}
}

struct ow_syntax_error *ow_lexer_error(struct ow_lexer *lexer) {
	if (!lexer->error_info.message)
		return NULL;
	return &lexer->error_info;
}
