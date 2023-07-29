#pragma once

#include <stdint.h>

#include <utilities/wordsize.h>

struct ow_object;

/// GC data, class pointer, and other info.
struct ow_object_meta {

    /*
     * Flags:
     * - `OLD`: GC generation; 0 = young, 1 = old.
     * - `MID`: valid when OLD = 0; 0 = new object, 1 = young object survived once
     * - `BIG`: valid when OLD = 1; 0 = small object, 1 = large object
     * - `MRK`: GC reachable mark; 0 = unreachable or not marking, 1 = reachable
     * - `XXX`: (reserved)
     *
     * Values:
     * - `PTR`: pointer used by GC system
     * - `CLS`: pointer to the class object
     *
     * The lower 2 bits of the values must be zero.
     */

#if OW_WORDSIZE == 64 || OW_WORDSIZE == 32

    uintptr_t _1;
    uintptr_t _2;

#define OW_OBJMETA_OLD_MVAR    _1
#define OW_OBJMETA_OLD_MASK    ((uintptr_t)1U)

#define OW_OBJMETA_MID_MVAR    _1
#define OW_OBJMETA_MID_MASK    ((uintptr_t)2U)

#define OW_OBJMETA_BIG_MVAR    OW_OBJMETA_MID_MVAR
#define OW_OBJMETA_BIG_MASK    OW_OBJMETA_MID_MASK

#define OW_OBJMETA_PTR_MVAR    _1
#define OW_OBJMETA_PTR_MASK    (~(uintptr_t)3U)

#define OW_OBJMETA_XXX_MVAR    _2
#define OW_OBJMETA_XXX_MASK    ((uintptr_t)1U)

#define OW_OBJMETA_MRK_MVAR    _2
#define OW_OBJMETA_MRK_MASK    ((uintptr_t)2U)

#define OW_OBJMETA_CLS_MVAR    _2
#define OW_OBJMETA_CLS_MASK    (~(uintptr_t)3U)

/// Initial value.
#define ow_object_meta_init(META, OLD, MID_OR_BIG, PTR, CLS) \
    do {                                                     \
        (META)._1                                            \
            = (uintptr_t)(PTR)                               \
            | ((OLD) ? OW_OBJMETA_OLD_MASK : 0U)             \
            | ((MID_OR_BIG) ? OW_OBJMETA_MID_MASK : 0U)      \
            ;                                                \
        (META)._2                                            \
            = (uintptr_t)(CLS)                               \
            ;                                                \
    } while (0)                                              \
// ^^^ ow_object_meta_init() ^^^

#else

#    error "unexpected OW_WORDSIZE value"

#endif

};

/// Get value.
#define ow_object_meta_load_(NAME, type, obj_meta) \
    ((type)((obj_meta). OW_OBJMETA_##NAME##_MVAR & OW_OBJMETA_##NAME##_MASK))

/// Set value.
#define ow_object_meta_store_(NAME, obj_meta, value) \
    ((obj_meta). OW_OBJMETA_##NAME##_MVAR = \
        ((obj_meta). OW_OBJMETA_##NAME##_MVAR & ~ OW_OBJMETA_##NAME##_MASK) \
        | (uintptr_t)(value))

/// Test flag (return 0 for false and non-0 for true).
#define ow_object_meta_test_(NAME, obj_meta) \
    ((obj_meta). OW_OBJMETA_##NAME##_MVAR & OW_OBJMETA_##NAME##_MASK)

/// Set flag to true.
#define ow_object_meta_set_(NAME, obj_meta) \
    ((obj_meta). OW_OBJMETA_##NAME##_MVAR |= OW_OBJMETA_##NAME##_MASK)

/// Set flag to false.
#define ow_object_meta_reset_(NAME, obj_meta) \
    ((obj_meta). OW_OBJMETA_##NAME##_MVAR &= ~ OW_OBJMETA_##NAME##_MASK)

/// Check whether a value can be stored into the meta.
#define ow_object_meta_check_value(val) \
    ((sizeof(val) <= sizeof(uintptr_t)) && !((uintptr_t)val & (uintptr_t)3U))
