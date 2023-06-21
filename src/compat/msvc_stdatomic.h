#pragma once

#if !__STDC_NO_ATOMICS__

#pragma message("__STDC_NO_ATOMICS__ is not true")
#include <stdatomic.h>

#else // __STDC_NO_ATOMICS__

#include <Windows.h>

typedef LONG  volatile __declspec(align(32)) atomic_int;
typedef ULONG volatile __declspec(align(32)) atomic_uint;

#define atomic_store(obj_ptr, desired) \
    InterlockedExchange((LONG *)obj_ptr, (LONG)desired)
#define atomic_load(obj_ptr) \
    InterlockedCompareExchange((LONG *)obj_ptr, 0, 0)

#define atomic_fetch_add(obj_ptr, value) \
    InterlockedExchangeAdd((LONG *)obj_ptr, value)
#define atomic_fetch_sub(obj_ptr, value) \
    InterlockedExchangeAdd((LONG *)obj_ptr, -(LONG)(value))

#define atomic_compare_exchange_strong(obj_ptr, expected, desired) \
    _atomic_compare_exchange_strong_int((LONG *)obj_ptr, (LONG *)expected, (LONG)desired)

static inline _Bool _atomic_compare_exchange_strong_int(
    LONG *obj, LONG *expected, LONG desired
) {
    LONG orig = *expected;
    *expected = InterlockedCompareExchange(obj, desired, orig);
    return *expected == orig;
}

#endif // !__STDC_NO_ATOMICS__
