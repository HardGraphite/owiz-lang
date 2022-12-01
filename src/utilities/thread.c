#include "thread.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>

#include <utilities/attributes.h>
#include <utilities/pragmas.h>

#if OW_THRD_WINNT
#	include <Windows.h>
static_assert(sizeof(ow_thrd_t) == sizeof(HANDLE), "");
#elif OW_THRD_POSIX
#	include <sched.h> // sched_yield()
#	include <unistd.h>
#elif OW_THRD_STDC
#	include <threads.h>
#else
ow_pragma_message("thread utilities are not implemeted")
#	include <stdlib.h>
#endif

#if OW_THRD_POSIX

struct posix_thrd_create_func_wrapper_arg {
	ow_thrd_start_t func;
	void *func_arg;
};

static void *posix_thrd_create_func_wrapper(void *arg) {
	struct posix_thrd_create_func_wrapper_arg *const wrapper_arg = arg;
	const int res = wrapper_arg->func(wrapper_arg->func_arg);
	return (void *)(uintptr_t)(unsigned int)res;
}

#endif

int ow_thrd_create(ow_thrd_t *thrd, ow_thrd_start_t func, void *arg) {
#if OW_THRD_WINNT
	const uintptr_t handle = _beginthreadex(
		NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
	if (!handle)
		return ow_thrd_error;
	*thrd = h;
	return ow_thrd_success;
#elif OW_THRD_POSIX
	const int state = pthread_create(
		thrd, NULL, posix_thrd_create_func_wrapper,
		&(struct posix_thrd_create_func_wrapper_arg){func, arg}
	);
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return thrd_create(thrd, func, arg);
#else
	ow_unused_var(thrd), ow_unused_var(func), ow_unused_var(arg);
	return ow_thrd_error;
#endif
}

int ow_thrd_equal(ow_thrd_t lhs, ow_thrd_t rhs) {
#if OW_THRD_WINNT
	return CompareObjectHandles(lhs, rhs);
#elif OW_THRD_POSIX
	return lhs == rhs;
#elif OW_THRD_STDC
	return thrd_equal(lhs, rhs);
#else
	return lhs == rhs;
#endif
}

ow_thrd_t ow_thrd_current(void) {
#if OW_THRD_WINNT
	return GetCurrentThread();
#elif OW_THRD_POSIX
	return pthread_self();
#elif OW_THRD_STDC
	return thrd_current();
#else
	return -1;
#endif
}

int ow_thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
#if OW_THRD_WINNT
	const struct timespec duration_ = duration;
	while (duration_.tv_sec) {
		DWORD time_sec;
		if (duration_.tv_sec >= UINT_MAX / 1000)
			time_sec = UINT_MAX / 1000;
		else
			time_sec = (DWORD)duration_.tv_sec;
		Sleep(time_ms * 1000);
		duration_.tv_sec -= time_sec;
	}
	if (duration_.tv_nsec) {
		const DWORD time_ms = duration_.tv_nsec / 1000000;
		if (time_ms)
			Sleep(time_ms);
	}
	if (remaining) {
		remaining->tv_sec = 0;
		remaining->tv_nsec = 0;
	}
	return 0;
#elif OW_THRD_POSIX
	return nanosleep(duration, remaining);
#elif OW_THRD_STDC
	return thrd_sleep(duration, remaining);
#else
	if (remaining)
		*remaining = duration;
	return -2;
#endif
}

void ow_thrd_yield(void) {
#if OW_THRD_WINNT
	SwitchToThread();
#elif OW_THRD_POSIX
#	ifdef _GNU_SOURCE
	pthread_yield();
#	else
	sched_yield();
#	endif
#elif OW_THRD_STDC
	thrd_yield();
#else
	;
#endif
}

_Noreturn void ow_thrd_exit(int res) {
#if OW_THRD_WINNT
	_endthreadex((DWORD)res);
#elif OW_THRD_POSIX
	pthread_exit((void *)(uintptr_t)(unsigned int)res);
#elif OW_THRD_STDC
	thrd_exit(res);
#else
	exit(res);
#endif
}

int ow_thrd_detach(ow_thrd_t thrd) {
#if OW_THRD_WINNT
	return CloseHandle(thrd) ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_POSIX
	const int state = pthread_detach(thrd);
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return thrd_detach(thrd);
#else
	ow_unused_var(thrd);
	return ow_thrd_error;
#endif
}

int ow_thrd_join(ow_thrd_t thrd, int *res) {
#if OW_THRD_WINNT
	if (WaitForSingleObject(thrd, INFINITE) != WAIT_OBJECT_0)
		return ow_thrd_error;
	if (res) {
		DWORD unsigned_res;
		if (!GetExitCodeThread(thrd, &unsigned_res)) {
			CloseHandle(thrd);
			return ow_thrd_error;
		}
		*res = (int)unsigned_res;
	}
	CloseHandle(thrd);
	return ow_thrd_success;
#elif OW_THRD_POSIX
	void *pthread_res;
	const int state = pthread_join(thrd, &pthread_res);
	if (res)
		*res = (int)(unsigned int)(uintptr_t)pthread_res;
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return thrd_join(thrd, res);
#else
	ow_unused_var(thrd), ow_unused_var(res);
	return ow_thrd_error;
#endif
}

int ow_mtx_init(ow_mtx_t *mutex, int type) {
#if OW_THRD_WINNT
	ow_unused_var(type);
	InitializeCriticalSection(mutex);
	return ow_thrd_success;
#elif OW_THRD_POSIX
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if (type & ow_mtx_recursive) {
		pthread_mutexattr_settype(
			&attr,
#if defined(__linux__) || defined(__linux)
			PTHREAD_MUTEX_RECURSIVE_NP
#else
			PTHREAD_MUTEX_RECURSIVE
#endif
		);
	}
	const int state = pthread_mutex_init(mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return mtx_init(mutex, type);
#else
	ow_unused_var(mutex), ow_unused_var(type);
	return ow_thrd_error;
#endif
}

int ow_mtx_lock(ow_mtx_t *mutex) {
#if OW_THRD_WINNT
	EnterCriticalSection(mutex);
	return ow_thrd_success;
#elif OW_THRD_POSIX
	const int state = pthread_mutex_lock(mutex);
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return mtx_lock(mutex);
#else
	ow_unused_var(mutex);
	return ow_thrd_error;
#endif
}

int ow_mtx_timedlock(ow_mtx_t *mutex, const struct timespec *time_point) {
#if OW_THRD_WINNT
	const struct timespec time_point_ = *time_point;
	while (ow_mtx_trylock(mtx) != thrd_success) {
		struct timespec current_time;
		timespec_get(current_time, TIME_UTC);
		if (current_time.tv_sec > time_point_.tv_sec ||
				(current_time.tv_sec == time_point_.tv_sec &&
				current_time.tv_nsec > time_point_.tv_nsec))
			return thrd_busy;
		SwitchToThread();
	}
	return thrd_success;
#elif OW_THRD_POSIX
	const int state = pthread_mutex_timedlock(mutex, time_point);
	return state == 0 ? ow_thrd_success : ow_thrd_busy;
#elif OW_THRD_STDC
	return mtx_timedlock(mutex, time_point);
#else
	ow_unused_var(mutex), ow_unused_var(time_point);
	return ow_thrd_error;
#endif
}

int ow_mtx_trylock(ow_mtx_t *mutex) {
#if OW_THRD_WINNT
	const BOOL ok = TryEnterCriticalSection(mutex);
	return ok ? ow_thrd_success : ow_thrd_busy;
#elif OW_THRD_POSIX
	const int state = pthread_mutex_trylock(mutex);
	return state == 0 ? ow_thrd_success : ow_thrd_busy;
#elif OW_THRD_STDC
	return mtx_trylock(mutex);
#else
	ow_unused_var(mutex);
	return ow_thrd_error;
#endif
}

int ow_mtx_unlock(ow_mtx_t *mutex) {
#if OW_THRD_WINNT
	LeaveCriticalSection(mutex);
	return ow_thrd_success;
#elif OW_THRD_POSIX
	const int state = pthread_mutex_unlock(mutex);
	return state == 0 ? ow_thrd_success : ow_thrd_error;
#elif OW_THRD_STDC
	return mtx_unlock(mutex);
#else
	ow_unused_var(mutex);
	return ow_thrd_error;
#endif
}

void ow_mtx_destroy(ow_mtx_t *mutex) {
#if OW_THRD_WINNT
	DeleteCriticalSection(mutex);
#elif OW_THRD_POSIX
	pthread_mutex_destroy(mutex);
#elif OW_THRD_STDC
	mtx_destroy(mutex);
#else
	ow_unused_var(mutex);
#endif
}
