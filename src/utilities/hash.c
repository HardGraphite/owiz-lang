#include "hash.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

ow_hash_t ow_hash_double(double val) {
    if (ow_unlikely(!isnormal(val))) {
        if (val == 0.0)
            return 0;
        return 0x55555555;
    }

    const int32_t as_int = (int32_t)val;
    static_assert(sizeof as_int == sizeof(ow_hash_t), "");
    if ((double)as_int == val)
        return ow_hash_int(as_int);

    static_assert(sizeof(float) == sizeof(ow_hash_t), "");
    union { float f; int32_t i; } converter;
    converter.f = (float)val;
    return converter.i >> 2;
}

#ifdef _MSC_VER
extern unsigned int _rotl(unsigned int value, int shift);
#endif // _MSC_VER

ow_static_forceinline uint32_t rotl32(uint32_t x, int8_t r) {
#if defined(_MSC_VER)
    return _rotl(x, r);
#else // !_MSC_VER
    return (x << r) | (x >> (32 - r));
#endif // _MSC_VER
}

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
static uint32_t MurmurHash3_x86_32(const void *key, int len, uint32_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    const int nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t *blocks = (const uint32_t *)(data + (ptrdiff_t)nblocks * 4);

    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t *tail = (const uint8_t *)(data + (ptrdiff_t)nblocks * 4);

    uint32_t k1 = 0;

    switch (len & 3) {
    case 3:
        k1 ^= tail[2] << 16;
        ow_fallthrough;
    case 2:
        k1 ^= tail[1] << 8;
        ow_fallthrough;
    case 1:
        k1 ^= tail[0];
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
    default:
        break;
    };

    h1 ^= len;

    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

ow_hash_t ow_hash_bytes(const void *data, size_t size) {
    assert(size <= INT32_MAX);
    const uint32_t seed = 0x5d9ee90;
    const ow_hash_t result = MurmurHash3_x86_32(data, (int)size, seed);
    return result;
}
