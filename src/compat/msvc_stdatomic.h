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

#endif // !__STDC_NO_ATOMICS__
