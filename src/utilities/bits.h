#pragma once

#if defined __GNUC__ || defined __clang__

/// Count trailing zero bits. `X` must not be 0.
#define ow_bits_count_tz(X) \
    (unsigned int) _Generic((X), \
        unsigned long long : __builtin_ctzll, \
        unsigned long      : __builtin_ctzl , \
        unsigned int       : __builtin_ctz    \
    ) ((X)) \
// ^^^ ow_bits_count_tz() ^^^

#elif defined _MSC_VER

#define ow_bits_count_tz(X) \
    _Generic((X),             \
        unsigned long long : _ow_bits_count_tz_u64_msvc, \
        unsigned long      : _ow_bits_count_tz_u32_msvc, \
        unsigned int       : _ow_bits_count_tz_u32_msvc  \
    ) ((X)) \
// ^^^ ow_bits_count_tz() ^^^

static __forceinline unsigned int _ow_bits_count_tz_u32_msvc(unsigned long mask) {
    unsigned long index;
    _BitScanForward(&index, mask);
    return (unsigned int)index;
}

static __forceinline unsigned int _ow_bits_count_tz_u64_msvc(unsigned __int64 mask) {
    unsigned long index;
    _BitScanForward64(&index, mask);
    return (unsigned int)index;
}

#else

#error "Not implemented on this platform."

#endif
