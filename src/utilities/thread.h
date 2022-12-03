#pragma once

#include <utilities/platform.h>

struct timespec;

#if _IS_WINDOWS_
#	include <Windows.h> // !!
#	define OW_THRD_WINNT 1
typedef void *ow_thrd_t;
typedef CRITICAL_SECTION ow_mtx_t;
#elif _IS_POSIX_
#	include <pthread.h>
#	define OW_THRD_POSIX 1
typedef pthread_t ow_thrd_t;
typedef pthread_mutex_t ow_mtx_t;
#elif !defined(__STDC_NO_THREADS__)
#	include <threads.h>
#	define OW_THRD_STDC  1
typedef thrd_t ow_thrd_t;
typedef mtx_t ow_mtx_t;
#else
typedef long ow_thrd_t;
typedef long ow_mtx_t;
#endif

/// Identifiers for thread states and errors.
enum {
#if !OW_THRD_STDC
	ow_thrd_success = 0,
	ow_thrd_nomem,
	ow_thrd_timedout,
	ow_thrd_busy,
	ow_thrd_error,
#else // OW_THRD_STDC
	ow_thrd_success  = thrd_success,
	ow_thrd_nomem    = thrd_nomem,
	ow_thrd_timedout = thrd_timedout,
	ow_thrd_busy     = thrd_busy,
	ow_thrd_error    = thrd_error,
#endif // !OW_THRD_STDC
};

/// A typedef of the function pointer type `int(*)(void*)`, used by ow_thrd_create().
typedef int(*ow_thrd_start_t)(void*);

/// Creates a thread.
int ow_thrd_create(ow_thrd_t *thrd, ow_thrd_start_t func, void *arg);
/// Checks if two identifiers refer to the same thread.
int ow_thrd_equal(ow_thrd_t lhs, ow_thrd_t rhs);
/// Obtains the current thread identifier.
ow_thrd_t ow_thrd_current(void);
/// suspends execution of the calling thread for the given period of time
int ow_thrd_sleep(const struct timespec *duration, struct timespec *remaining);
/// Yields the current time slice.
void ow_thrd_yield(void);
/// Terminates the calling thread.
_Noreturn void ow_thrd_exit(int res);
/// Detaches a thread.
int ow_thrd_detach(ow_thrd_t thrd);
/// Blocks until a thread terminates.
int ow_thrd_join(ow_thrd_t thrd, int *res);

/// When passed to ow_mtx_init(), identifies the type of a mutex to create.
enum {
#if !OW_THRD_STDC
	ow_mtx_plain = 1,
	ow_mtx_timed = 2,
	ow_mtx_recursive = 4,
#else // OW_THRD_STDC
	ow_mtx_plain     = mtx_plain,
	ow_mtx_timed     = mtx_timed,
	ow_mtx_recursive = mtx_recursive,
#endif // !OW_THRD_STDC
};

/// Creates a mutex.
int ow_mtx_init(ow_mtx_t *mutex, int type);
/// Blocks until locks a mutex.
int ow_mtx_lock(ow_mtx_t *mutex);
/// Blocks until locks a mutex or times out.
int ow_mtx_timedlock(ow_mtx_t *mutex, const struct timespec *time_point);
/// Locks a mutex or returns without blocking if already locked.
int ow_mtx_trylock(ow_mtx_t *mutex);
/// Unlocks a mutex.
int ow_mtx_unlock(ow_mtx_t *mutex);
/// Destroys a mutex.
void ow_mtx_destroy(ow_mtx_t *mutex);
