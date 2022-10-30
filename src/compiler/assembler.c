#include "assembler.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <machine/machine.h>
#include <objects/classes.h>
#include <objects/floatobj.h>
#include <objects/funcobj.h>
#include <objects/intobj.h>
#include <objects/memory.h>
#include <objects/stringobj.h>
#include <objects/symbolobj.h>
#include <utilities/array.h>
#include <utilities/malloc.h>
#include <utilities/strings.h>
#include <utilities/unreachable.h>

/// Representation of an instruction.
struct instr_data {
	uint8_t opcode;
	bool operand_is_label;
	union ow_operand operand;
};

/// An array of `struct instr_data`.
struct instr_array {
	struct ow_xarray _data;
};

/// Initialize the array.
static void instr_array_init(struct instr_array *arr) {
	ow_xarray_init(&arr->_data, struct instr_data, 0);
}

/// Finalize the array.
static void instr_array_fini(struct instr_array *arr) {
	ow_xarray_fini(&arr->_data);
}

/// Clear the array.
static void instr_array_clear(struct instr_array *arr) {
	ow_xarray_clear(&arr->_data);
}

/// Get number of instructions.
static size_t instr_array_size(const struct instr_array *arr) {
	return ow_xarray_size(&arr->_data);
}

/// Append an instruction.
static void instr_array_append(struct instr_array *arr, struct instr_data instr) {
	ow_xarray_append(&arr->_data, struct instr_data, instr);
}

/// Drop last instruction.
static void instr_array_drop(struct instr_array *arr, size_t n) {
	while (n--)
		ow_xarray_drop(&arr->_data);
}

/// Drop last instruction.
static struct instr_data *instr_array_ref(struct instr_array *arr, size_t i) {
	return &ow_xarray_at(&arr->_data, struct instr_data, i);
}

struct ow_assembler {
	struct instr_array instr_seq;
	struct ow_array constants; // {ow_object *}
	struct ow_array symbols; // {ow_symbol_obj *}
	struct ow_array labels; // {unsigned index}
	struct ow_machine *machine;
};

static void _as_gc_marker(struct ow_machine *om, void *_as) {
	struct ow_assembler *const as = _as;
	assert(om == as->machine);
	for (size_t i = 0, n = ow_array_size(&as->constants); i < n; i++)
		ow_objmem_object_gc_marker(om, ow_array_at(&as->constants, i));
	for (size_t i = 0, n = ow_array_size(&as->symbols); i < n; i++)
		ow_objmem_object_gc_marker(om, ow_array_at(&as->symbols, i));
}

struct ow_assembler *ow_assembler_new(struct ow_machine *om) {
	struct ow_assembler *const as = ow_malloc(sizeof(struct ow_assembler));
	instr_array_init(&as->instr_seq);
	ow_array_init(&as->constants, 0);
	ow_array_init(&as->symbols, 0);
	ow_array_init(&as->labels, 0);
	as->machine = om;
	ow_objmem_add_gc_root(om, as, _as_gc_marker);
	return as;
}

void ow_assembler_del(struct ow_assembler *as) {
	ow_objmem_remove_gc_root(as->machine, as);
	ow_array_fini(&as->labels);
	ow_array_fini(&as->symbols);
	ow_array_fini(&as->constants);
	instr_array_fini(&as->instr_seq);
	ow_free(as);
}

void ow_assembler_clear(struct ow_assembler *as) {
	instr_array_clear(&as->instr_seq);
	ow_array_clear(&as->constants);
	ow_array_clear(&as->symbols);
	ow_array_clear(&as->labels);
}

size_t ow_assembler_constant(struct ow_assembler *as, struct ow_assembler_constant v) {
	struct ow_machine *const om = as->machine;

	if (v.type == OW_AS_CONST_OBJ) {
		const size_t index = ow_array_size(&as->constants);
		ow_array_append(&as->constants, v.o);
		return index;
	}

	struct ow_class_obj *const_class;
	switch (v.type) {
	case OW_AS_CONST_INT:
		const_class = om->builtin_classes->int_;
		break;
	case OW_AS_CONST_FLT:
		const_class = om->builtin_classes->float_;
		break;
	case OW_AS_CONST_STR:
		const_class = om->builtin_classes->string;
		break;
	default:
		ow_unreachable();
	}

	for (size_t i = 0, cnt = ow_array_size(&as->constants); i < cnt; i++) {
		struct ow_object *const obj = ow_array_at(&as->constants, i);
		if (ow_object_class(obj) != const_class)
			continue;
		switch (v.type) {
		case OW_AS_CONST_INT:
			if (ow_int_obj_value(ow_object_cast(obj, struct ow_int_obj)) != v.i)
				continue;
			break;
		case OW_AS_CONST_FLT:
			if (ow_float_obj_value(ow_object_cast(obj, struct ow_float_obj)) != v.f)
				continue;
			break;
		case OW_AS_CONST_STR: {
			size_t size;
			const char *const str = ow_string_obj_flatten(
				om, ow_object_cast(obj, struct ow_string_obj), &size);
			if (!(size == v.s.n && !memcmp(str, v.s.p, size)))
				continue;
		}
			break;
		default:
			ow_unreachable();
		}
		return i;
	}

	const size_t index = ow_array_size(&as->constants);
	struct ow_object *obj;
	switch (v.type) {
	case OW_AS_CONST_INT:
		obj = ow_object_from(ow_int_obj_or_smallint(om, v.i));
		break;
	case OW_AS_CONST_FLT:
		obj = ow_object_from(ow_float_obj_new(om, v.i));
		break;
	case OW_AS_CONST_STR:
		obj = ow_object_from(ow_string_obj_new(om, v.s.p, v.s.n));
		break;
	default:
		ow_unreachable();
	}
	ow_array_append(&as->constants, obj);
	return index;
}

size_t ow_assembler_symbol(struct ow_assembler *as, const char *s, size_t n) {
	for (size_t i = 0, i_max = ow_array_size(&as->symbols); i < i_max; i++) {
		struct ow_symbol_obj *const obj = ow_array_at(&as->symbols, i);
		if (n != ow_symbol_obj_size(obj))
			continue;
		if (memcmp(s, ow_symbol_obj_data(obj), n) != 0)
			continue;
		return i;
	}

	const size_t index = ow_array_size(&as->symbols);
	ow_array_append(&as->symbols, ow_symbol_obj_new(as->machine, s, n));
	return index;
}

int ow_assembler_prepare_label(struct ow_assembler *as) {
	const size_t label_id = ow_array_size(&as->labels);
	ow_array_append(&as->labels, (void *)UINTPTR_MAX);
	assert(label_id <= INT_MAX);
	return (int)label_id;
}

int ow_assembler_place_label(struct ow_assembler *as, int label) {
	if (label < 0)
		label = ow_assembler_prepare_label(as);
	const size_t label_id = (size_t)label;
	const size_t instr_idx = instr_array_size(&as->instr_seq);
	assert(ow_array_at(&as->labels, label_id) == (void *)UINTPTR_MAX);
	ow_array_at(&as->labels, label_id) = (void *)(uintptr_t)instr_idx;
	return label;
}

void ow_assembler_append(
		struct ow_assembler *as, enum ow_opcode opcode, union ow_operand operand) {
	instr_array_append(&as->instr_seq, (struct instr_data){
		.opcode = (uint8_t)opcode,
		.operand_is_label = false,
		.operand = operand,
	});
}

void ow_assembler_append_jump(
		struct ow_assembler *as, enum ow_opcode opcode, int target_label) {
	assert(target_label >= 0 && target_label <= UINT16_MAX);
	assert(ow_operand_type_width(ow_operand_type(opcode)) == 1);
	instr_array_append(&as->instr_seq, (struct instr_data){
		.opcode = (uint8_t)opcode,
		.operand_is_label = true,
		.operand.u16 = (uint16_t)target_label,
	});
}

enum ow_opcode ow_assembler_last(struct ow_assembler *as) {
	const size_t n = instr_array_size(&as->instr_seq);
	if (ow_unlikely(!n))
		return OW_OPC_Nop;
	return instr_array_ref(&as->instr_seq, n - 1)->opcode;
}

void ow_assembler_discard(struct ow_assembler *as, size_t n) {
	assert(instr_array_size(&as->instr_seq) >= n);
	instr_array_drop(&as->instr_seq, n);
}

#ifndef NDEBUG

static bool _opcode_to_wide_check(enum ow_opcode opc, enum ow_opcode opc_w) {
	if (ow_operand_type_width(ow_operand_type(opc)) != 1)
		return false;
	if (ow_operand_type_width(ow_operand_type(opc_w)) != 2)
		return false;
	const char *const name = ow_opcode_name(opc);
	const char *const name_w = ow_opcode_name(opc_w);
	const size_t name_len = strlen(name);
	if (strlen(name_w) != name_len + 1)
		return false;
	if (name_w[name_len] != 'W')
		return false;
	if (memcmp(name, name_w, name_len) != 0)
		return false;
	return true;
}

#endif // NDEBUG

static enum ow_opcode _opcode_to_wide(enum ow_opcode opcode) {
	const enum ow_opcode opcode_w = (enum ow_opcode)((unsigned int)opcode + 1);
	assert(_opcode_to_wide_check(opcode, opcode_w));
	return opcode_w;
}

struct ow_func_obj *ow_assembler_output(
		struct ow_assembler *as, const struct ow_assembler_output_spec *spec) {
	const size_t instr_seq_len = instr_array_size(&as->instr_seq);
	size_t *const addr_map = ow_malloc(sizeof(size_t) * (instr_seq_len + 1));

	size_t code_seq_len = 0;
	for (size_t i = 0; i < instr_seq_len; i++) {
		struct instr_data *const instr = instr_array_ref(&as->instr_seq, i);
		const size_t current_code_addr = code_seq_len;
		addr_map[i] = current_code_addr;

		if (instr->operand_is_label) {
			const size_t label_id = instr->operand.u16;
			assert(label_id < ow_array_size(&as->labels));
			const size_t lbl_instr_idx =
				(uintptr_t)ow_array_at(&as->labels, label_id);
			assert(lbl_instr_idx != UINTPTR_MAX); // Has not been placed.

			if (lbl_instr_idx <= i) {
				const size_t code_addr = addr_map[lbl_instr_idx];
				const ptrdiff_t offset = (ptrdiff_t)code_addr - (ptrdiff_t)current_code_addr;
				assert(offset <= 0);
				if (offset >= INT8_MIN) {
					instr->operand.i8 = (int8_t)offset;
					code_seq_len += 1 + 1;
				} else if (offset >= INT16_MIN) {
					instr->operand.i16 = (int16_t)offset;
					code_seq_len += 1 + 2;
				} else {
					abort(); // Too far.
				}
				instr->operand_is_label = false;
			} else {
				const size_t idx_diff = (size_t)(lbl_instr_idx - i);
				const bool use_width_operand = idx_diff > (INT8_MAX / 3);
				if (use_width_operand) {
					instr->opcode = (uint8_t)_opcode_to_wide(
						(enum ow_opcode)instr->opcode);
					code_seq_len += 1 + 2;
				} else {
					code_seq_len += 1 + 1;
				}
			}
		} else {
			code_seq_len += 1 + ow_operand_type_width(
				ow_operand_type((enum ow_opcode)instr->opcode));
		}
	}
	addr_map[instr_seq_len] = code_seq_len;

	uint8_t *const code_seq = ow_malloc(code_seq_len);
	uint8_t *code_seq_ptr = code_seq;

	for (size_t i = 0, n = instr_array_size(&as->instr_seq); i < n; i++) {
		struct instr_data instr = *instr_array_ref(&as->instr_seq, i);

		if (instr.operand_is_label) {
			const size_t label_id = instr.operand.u16;
			const size_t lbl_instr_idx =
				(uintptr_t)ow_array_at(&as->labels, label_id);
			assert(lbl_instr_idx > i);
			const bool use_width_operand = (lbl_instr_idx - i) > (INT8_MAX / 3);
			const size_t offset = addr_map[lbl_instr_idx] - addr_map[i];
			if (use_width_operand)
				instr.operand.i16 = (int16_t)offset;
			else
				instr.operand.i8 = (int8_t)offset;
		}

		*code_seq_ptr++ = instr.opcode;

		switch (ow_operand_type((enum ow_opcode)instr.opcode)) {
		case OW_OPERAND_0:
			break;

		case OW_OPERAND_u8:
		case OW_OPERAND_i8:
			*code_seq_ptr = instr.operand.u8;
			code_seq_ptr += 1;
			break;

		case OW_OPERAND_u16:
		case OW_OPERAND_i16:
			*(uint16_t *)code_seq_ptr = instr.operand.u16;
			code_seq_ptr += 2;
			break;

		default:
			ow_unreachable();
		}
	}

	assert((size_t)(code_seq_ptr - code_seq) <= code_seq_len);
	code_seq_len = (size_t)(code_seq_ptr - code_seq);

	struct ow_func_obj *const func = ow_func_obj_new(
		as->machine, spec->module,
		(struct ow_object **)ow_array_data(&as->constants), ow_array_size(&as->constants),
		(struct ow_symbol_obj **)ow_array_data(&as->symbols), ow_array_size(&as->symbols),
		code_seq, code_seq_len, spec->func_spec
	);

	ow_free(code_seq);
	ow_free(addr_map);

	return func;
}
