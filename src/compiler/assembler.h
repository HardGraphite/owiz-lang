#pragma once

#include <stddef.h>

#include <bytecode/opcode.h>
#include <bytecode/operand.h>
#include <objects/funcspec.h>

struct ow_func_obj;
struct ow_machine;
struct ow_module_obj;
struct ow_object;

/// The assembler, converting opcodes and operands to byte code.
struct ow_assembler;

/// Value of a constant. Used in assembler functions.
struct ow_assembler_constant {
    enum {
        OW_AS_CONST_INT,
        OW_AS_CONST_FLT,
        OW_AS_CONST_STR,
        OW_AS_CONST_OBJ,
    } type;
    union {
        int64_t i;
        double  f;
        struct { const char *p; size_t n; } s;
        struct ow_object *o;
    };
};

struct ow_assembler_output_spec {
    struct ow_module_obj *module;
    struct ow_func_spec func_spec;
};

/// Create an assembler.
struct ow_assembler *ow_assembler_new(struct ow_machine *om);
/// Destroy an assembler.
void ow_assembler_del(struct ow_assembler *as);
/// Reset the assembler
void ow_assembler_clear(struct ow_assembler *as);
/// Get constant index in the table.
size_t ow_assembler_constant(
    struct ow_assembler *as, struct ow_assembler_constant v);
/// Get symbol index in the table.
size_t ow_assembler_symbol(struct ow_assembler *as, const char *s, size_t n);
/// Allocate a new label.
int ow_assembler_prepare_label(struct ow_assembler *as);
/// Place a label. If param `label` is -1, allocate a new one. Then return the label.
/// A label must be placed for only once.
int ow_assembler_place_label(struct ow_assembler *as, int label);
/// Append a simple instruction.
void ow_assembler_append(
    struct ow_assembler *as, enum ow_opcode opcode, union ow_operand operand);
/// Append a jump instruction. The operand shall be a jump offset.
void ow_assembler_append_jump(
    struct ow_assembler *as, enum ow_opcode opcode, int target_label);
/// Get opcode of last instruction. If there is no instruction, return OW_OPC_Nop.
enum ow_opcode ow_assembler_last(struct ow_assembler *as);
/// Delete last `n` instruction(s).
void ow_assembler_discard(struct ow_assembler *as, size_t n);
/// Generate result.
struct ow_func_obj *ow_assembler_output(
    struct ow_assembler *as, const struct ow_assembler_output_spec *spec);
