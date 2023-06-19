#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <utilities/attributes.h>
#include <utilities/bits.h>
#include <utilities/round.h>

typedef size_t ow_bitset_cell_t;

/// Bit set, an array of bits. Size is not stored inside for more usable space.
struct ow_bitset {
    ow_bitset_cell_t _cells[1];
};

/// The minimum size in bytes required for an `n_bits` bit map.
#define ow_bitset_required_size(n_bits) \
    (ow_round_up_to(sizeof(ow_bitset_cell_t) * 8, n_bits) / 8)

/// Convert bit index to internal indices.
#define ow_bitset_extract_index( \
    bit_index, cell_index_var, bit_offset_var, bit_mask_var \
) \
    do {                                                                 \
        (cell_index_var) = (bit_index) / (sizeof(ow_bitset_cell_t) * 8); \
        (bit_offset_var) = (bit_index) % (sizeof(ow_bitset_cell_t) * 8); \
        (bit_mask_var)   = (ow_bitset_cell_t)1u << (bit_offset_var);     \
    } while (0)                                                          \
// ^^^ ow_bitset_extract_index() ^^^

/// Test if a bit is set.
ow_static_forceinline bool
ow_bitset_test_bit(const struct ow_bitset *bs, size_t bit_index) {
    size_t cell_index, bit_offset;
    ow_bitset_cell_t bit_mask;
    ow_bitset_extract_index(bit_index, cell_index, bit_offset, bit_mask);
    return bs->_cells[cell_index] & bit_mask;
}

/// Set a bit to true.
ow_static_forceinline void
ow_bitset_set_bit(struct ow_bitset *bs, size_t bit_index) {
    size_t cell_index, bit_offset;
    ow_bitset_cell_t bit_mask;
    ow_bitset_extract_index(bit_index, cell_index, bit_offset, bit_mask);
    bs->_cells[cell_index] |= bit_mask;
}

/// Set a bit to false.
ow_static_forceinline void
ow_bitset_reset_bit(struct ow_bitset *bs, size_t bit_index) {
    size_t cell_index, bit_offset;
    ow_bitset_cell_t bit_mask;
    ow_bitset_extract_index(bit_index, cell_index, bit_offset, bit_mask);
    bs->_cells[cell_index] &= ~bit_mask;
}

/// Set a bit to true if it is false.
ow_static_forceinline void
ow_bitset_try_set_bit(struct ow_bitset *bs, size_t bit_index) {
    size_t cell_index, bit_offset;
    ow_bitset_cell_t bit_mask;
    ow_bitset_extract_index(bit_index, cell_index, bit_offset, bit_mask);
    ow_bitset_cell_t *const cell_ptr = bs->_cells + cell_index;
    const ow_bitset_cell_t cell_data = *cell_ptr;
    if (!(cell_data & bit_mask))
        *cell_ptr = cell_data | bit_mask;
}

/// Set a bit to false if it is true.
ow_static_forceinline void
ow_bitset_try_reset_bit(struct ow_bitset *bs, size_t bit_index) {
    size_t cell_index, bit_offset;
    ow_bitset_cell_t bit_mask;
    ow_bitset_extract_index(bit_index, cell_index, bit_offset, bit_mask);
    ow_bitset_cell_t *const cell_ptr = bs->_cells + cell_index;
    const ow_bitset_cell_t cell_data = *cell_ptr;
    if (cell_data & bit_mask)
        *cell_ptr = cell_data & ~bit_mask;
}

/// Set all bits to false.
ow_static_inline void ow_bitset_clear(struct ow_bitset *bs, size_t size) {
    for (size_t i = 0, n = size / sizeof(ow_bitset_cell_t); i < n; i++)
        bs->_cells[i] = 0;
}

/// Iterate over set bits.
#define ow_bitset_foreach_set(bitset, bitset_size, BIT_INDEX_VAR, STATEMENT) \
    do {                                                                     \
        ow_bitset_cell_t *const __cells = (bitset)->_cells;                  \
        const size_t __cells_count = bitset_size / sizeof(ow_bitset_cell_t); \
        for (size_t __cell_i = 0; __cell_i < __cells_count; __cell_i++) {    \
            ow_bitset_cell_t __cell_data = __cells[__cell_i];                \
            for (ow_bitset_cell_t __t; __cell_data; __cell_data ^= __t) {    \
                __t = __cell_data & -__cell_data;                            \
                unsigned int __n = ow_bits_count_tz(__cell_data);            \
                {                                                            \
                    const size_t BIT_INDEX_VAR =                             \
                        __cell_i * sizeof(ow_bitset_cell_t) * 8 + __n;       \
                    { STATEMENT }                                            \
                }                                                            \
            }                                                                \
        }                                                                    \
    } while (0)                                                              \
// ^^^ ow_bitset_foreach_set() ^^^
