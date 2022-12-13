#pragma once

struct ow_machine;
struct ow_symbol_obj;

#define OW_COMSYM_LIST \
	ELEM(add , "+"  ) \
	ELEM(sub , "-"  ) \
	ELEM(mul , "*"  ) \
	ELEM(div , "/"  ) \
	ELEM(rem , "%"  ) \
	ELEM(shl , "<<" ) \
	ELEM(shr , ">>" ) \
	ELEM(and_, "&"  ) \
	ELEM(or_ , "|"  ) \
	ELEM(xor_, "^"  ) \
	ELEM(neg , "-." ) \
	ELEM(inv , "~"  ) \
	ELEM(cmp , "<=>") \
	ELEM(call, "()" ) \
	ELEM(anon, ""   ) \
	ELEM(main, "main") \
	ELEM(hash, "__hash__") \
	ELEM(find_attr, "__find_attr__") \
	ELEM(find_meth, "__find_meth__") \
// ^^^ OW_COMSYM_LIST ^^^

/// Commonly used symbols.
struct ow_common_symbols {
#define ELEM(NAME, STRING) struct ow_symbol_obj * NAME ;
	OW_COMSYM_LIST
#undef ELEM
};

/// Create symbols.
struct ow_common_symbols *ow_common_symbols_new(struct ow_machine *om);
/// Destroy symbols.
void ow_common_symbols_del(struct ow_common_symbols *cs);

void _ow_common_symbols_gc_marker(struct ow_machine *om, struct ow_common_symbols *cs);
