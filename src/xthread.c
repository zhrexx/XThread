#include "xthread.h"
#include <stdlib.h>

#ifndef TIME_UTC
    #define TIME_UTC 1
#endif

typedef struct {
    thrd_start_t function;
    void *arg;
} _thread_start_info;

#ifdef TINYCTHREAD_PLATFORM_WINDOWS
static DWORD WINAPI _thrd_wrapper_function(LPVOID arg)
#else
static void* _thrd_wrapper_function(void *arg)
#endif
{
    _thread_start_info *info = (_thread_start_info*)arg;
    thrd_start_t func = info->function;
    void *thread_arg = info->arg;
    
    free(info);
    
    int result = func(thread_arg);
    
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return (DWORD)result;
    #else
    return (void*)(intptr_t)result;
    #endif
}

int mtx_init(mtx_t *mtx, int type)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    mtx->already_locked = false;
    mtx->is_recursive = type & MTX_RECURSIVE;
    mtx->is_timed = type & MTX_TIMED;
    
    if (!mtx->is_timed) {
        InitializeCriticalSection(&mtx->handle.cs);
    } else {
        mtx->handle.mut = CreateMutex(NULL, FALSE, NULL);
        if (mtx->handle.mut == NULL) {
            return THRD_ERROR;
        }
    }
    return THRD_SUCCESS;
    
    #else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    
    if (type & MTX_RECURSIVE) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    
    int result = pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    
    return result == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

void mtx_destroy(mtx_t *mtx)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    if (!mtx->is_timed) {
        DeleteCriticalSection(&mtx->handle.cs);
    } else {
        CloseHandle(mtx->handle.mut);
    }
    #else
    pthread_mutex_destroy(mtx);
    #endif
}

int mtx_lock(mtx_t *mtx)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    if (!mtx->is_timed) {
        EnterCriticalSection(&mtx->handle.cs);
    } else {
        switch (WaitForSingleObject(mtx->handle.mut, INFINITE)) {
            case WAIT_OBJECT_0:
                break;
            case WAIT_ABANDONED:
            default:
                return THRD_ERROR;
        }
    }

    if (!mtx->is_recursive) {
        while (mtx->already_locked) {
            Sleep(1);
        }
        mtx->already_locked = true;
    }
    return THRD_SUCCESS;
    
    #else
    return pthread_mutex_lock(mtx) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int mtx_timedlock(mtx_t *mtx, const struct timespec *ts)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    if (!mtx->is_timed) {
        return THRD_ERROR;
    }

    struct timespec current_ts;
    timespec_get(&current_ts, TIME_UTC);

    DWORD timeout_ms = (current_ts.tv_sec > ts->tv_sec || 
                        (current_ts.tv_sec == ts->tv_sec && current_ts.tv_nsec >= ts->tv_nsec)) 
                       ? 0 
                       : (DWORD)((ts->tv_sec - current_ts.tv_sec) * 1000 + 
                                  (ts->tv_nsec - current_ts.tv_nsec) / 1000000 + 1);

    switch (WaitForSingleObject(mtx->handle.mut, timeout_ms)) {
        case WAIT_OBJECT_0:
            break;
        case WAIT_TIMEOUT:
            return THRD_TIMEOUT;
        case WAIT_ABANDONED:
        default:
            return THRD_ERROR;
    }

    if (!mtx->is_recursive) {
        while (mtx->already_locked) {
            Sleep(1);
        }
        mtx->already_locked = true;
    }
    return THRD_SUCCESS;
    
    #elif defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)
    switch (pthread_mutex_timedlock(mtx, ts)) {
        case 0: return THRD_SUCCESS;
        case ETIMEDOUT: return THRD_TIMEOUT;
        default: return THRD_ERROR;
    }
    
    #else
    int rc;
    struct timespec cur, remaining;

    while ((rc = pthread_mutex_trylock(mtx)) == EBUSY) {
        timespec_get(&cur, TIME_UTC);

        if (cur.tv_sec > ts->tv_sec || 
            (cur.tv_sec == ts->tv_sec && cur.tv_nsec >= ts->tv_nsec)) {
            break;
        }

        remaining.tv_sec = ts->tv_sec - cur.tv_sec;
        remaining.tv_nsec = ts->tv_nsec - cur.tv_nsec;
        
        if (remaining.tv_nsec < 0) {
            remaining.tv_sec--;
            remaining.tv_nsec += 1000000000;
        }

        nanosleep(&remaining, NULL);
    }

    switch (rc) {
        case 0: return THRD_SUCCESS;
        case ETIMEDOUT:
        case EBUSY: return THRD_TIMEOUT;
        default: return THRD_ERROR;
    }
    #endif
}

int mtx_trylock(mtx_t *mtx)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    int result;
    if (!mtx->is_timed) {
        result = TryEnterCriticalSection(&mtx->handle.cs) ? THRD_SUCCESS : THRD_BUSY;
    } else {
        result = (WaitForSingleObject(mtx->handle.mut, 0) == WAIT_OBJECT_0) ? THRD_SUCCESS : THRD_BUSY;
    }

    if (!mtx->is_recursive && result == THRD_SUCCESS) {
        if (mtx->already_locked) {
            LeaveCriticalSection(&mtx->handle.cs);
            result = THRD_BUSY;
        } else {
            mtx->already_locked = true;
        }
    }
    return result;
    
    #else
    return (pthread_mutex_trylock(mtx) == 0) ? THRD_SUCCESS : THRD_BUSY;
    #endif
}

int mtx_unlock(mtx_t *mtx)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    mtx->already_locked = false;
    if (!mtx->is_timed) {
        LeaveCriticalSection(&mtx->handle.cs);
    } else {
        if (!ReleaseMutex(mtx->handle.mut)) {
            return THRD_ERROR;
        }
    }
    return THRD_SUCCESS;
    
    #else
    return pthread_mutex_unlock(mtx) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int cnd_init(cnd_t *cond)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    cond->waiters_count = 0;
    InitializeCriticalSection(&cond->waiters_lock);

    cond->events[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (cond->events[0] == NULL) {
        cond->events[1] = NULL;
        return THRD_ERROR;
    }

    cond->events[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (cond->events[1] == NULL) {
        CloseHandle(cond->events[0]);
        cond->events[0] = NULL;
        return THRD_ERROR;
    }

    return THRD_SUCCESS;
    
    #else
    return pthread_cond_init(cond, NULL) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

void cnd_destroy(cnd_t *cond)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    if (cond->events[0] != NULL) {
        CloseHandle(cond->events[0]);
    }
    if (cond->events[1] != NULL) {
        CloseHandle(cond->events[1]);
    }
    DeleteCriticalSection(&cond->waiters_lock);
    
    #else
    pthread_cond_destroy(cond);
    #endif
}

int cnd_signal(cnd_t *cond)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    bool have_waiters;

    EnterCriticalSection(&cond->waiters_lock);
    have_waiters = cond->waiters_count > 0;
    LeaveCriticalSection(&cond->waiters_lock);

    if (have_waiters && SetEvent(cond->events[0]) == 0) {
        return THRD_ERROR;
    }

    return THRD_SUCCESS;
    
    #else
    return pthread_cond_signal(cond) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int cnd_broadcast(cnd_t *cond)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    bool have_waiters;

    EnterCriticalSection(&cond->waiters_lock);
    have_waiters = cond->waiters_count > 0;
    LeaveCriticalSection(&cond->waiters_lock);

    if (have_waiters && SetEvent(cond->events[1]) == 0) {
        return THRD_ERROR;
    }

    return THRD_SUCCESS;
    
    #else
    return pthread_cond_broadcast(cond) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int cnd_wait(cnd_t *cond, mtx_t *mtx)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    static const DWORD timeout = INFINITE;
    DWORD result;
    bool last_waiter;

    EnterCriticalSection(&cond->waiters_lock);
    cond->waiters_count++;
    LeaveCriticalSection(&cond->waiters_lock);

    mtx_unlock(mtx);

    result = WaitForMultipleObjects(2, cond->events, FALSE, timeout);
    if (result == WAIT_TIMEOUT) {
        mtx_lock(mtx);
        return THRD_ERROR;
    } else if (result == WAIT_FAILED) {
        mtx_lock(mtx);
        return THRD_ERROR;
    }

    EnterCriticalSection(&cond->waiters_lock);
    cond->waiters_count--;
    last_waiter = (result == (WAIT_OBJECT_0 + 1)) && (cond->waiters_count == 0);
    LeaveCriticalSection(&cond->waiters_lock);

    if (last_waiter) {
        if (ResetEvent(cond->events[1]) == 0) {
            mtx_lock(mtx);
            return THRD_ERROR;
        }
    }

    mtx_lock(mtx);
    return THRD_SUCCESS;
    
    #else
    return pthread_cond_wait(cond, mtx) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) == TIME_UTC) {
        unsigned long long now_ms = now.tv_sec * 1000 + now.tv_nsec / 1000000;
        unsigned long long ts_ms = ts->tv_sec * 1000 + ts->tv_nsec / 1000000;
        DWORD delta = (ts_ms > now_ms) ? (DWORD)(ts_ms - now_ms) : 0;
        
        DWORD result = WaitForMultipleObjects(2, cond->events, FALSE, delta);
        if (result == WAIT_TIMEOUT) {
            return THRD_TIMEOUT;
        } else if (result == WAIT_FAILED) {
            return THRD_ERROR;
        }
        return THRD_SUCCESS;
    }
    return THRD_ERROR;
    
    #else
    int ret = pthread_cond_timedwait(cond, mtx, ts);
    if (ret == ETIMEDOUT) {
        return THRD_TIMEOUT;
    }
    return ret == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
    _thread_start_info *info = malloc(sizeof(_thread_start_info));
    if (!info) {
        return THRD_NO_MEM;
    }
    
    info->function = func;
    info->arg = arg;

    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    *thr = CreateThread(NULL, 0, _thrd_wrapper_function, info, 0, NULL);
    if (!*thr) {
        free(info);
        return THRD_ERROR;
    }
    
    #else
    if (pthread_create(thr, NULL, _thrd_wrapper_function, info) != 0) {
        free(info);
        return THRD_ERROR;
    }
    #endif

    return THRD_SUCCESS;
}

thrd_t thrd_current(void)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return GetCurrentThread();
    #else
    return pthread_self();
    #endif
}

int thrd_detach(thrd_t thr)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return CloseHandle(thr) != 0 ? THRD_SUCCESS : THRD_ERROR;
    #else
    return pthread_detach(thr) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

int thrd_equal(thrd_t thr0, thrd_t thr1)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return GetThreadId(thr0) == GetThreadId(thr1);
    #else
    return pthread_equal(thr0, thr1);
    #endif
}

_Noreturn void thrd_exit(int res)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    ExitThread((DWORD)res);
    #else
    pthread_exit((void*)(intptr_t)res);
    #endif
}

int thrd_join(thrd_t thr, int *res)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    DWORD dwRes;

    if (WaitForSingleObject(thr, INFINITE) == WAIT_FAILED) {
        return THRD_ERROR;
    }

    if (res != NULL) {
        if (GetExitCodeThread(thr, &dwRes) != 0) {
            *res = (int)dwRes;
        } else {
            return THRD_ERROR;
        }
    }
    
    CloseHandle(thr);
    
    #else
    void *pres;
    if (pthread_join(thr, &pres) != 0) {
        return THRD_ERROR;
    }
    
    if (res != NULL) {
        *res = (int)(intptr_t)pres;
    }
    #endif
    
    return THRD_SUCCESS;
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    struct timespec start;
    timespec_get(&start, TIME_UTC);

    DWORD sleep_result = SleepEx(
        (DWORD)(duration->tv_sec * 1000 + duration->tv_nsec / 1000000 + 
                (duration->tv_nsec % 1000000 != 0)),
        TRUE
    );

    if (sleep_result == 0) {
        return 0;
    } else {
        if (remaining != NULL) {
            timespec_get(remaining, TIME_UTC);
            remaining->tv_sec -= start.tv_sec;
            remaining->tv_nsec -= start.tv_nsec;
            
            if (remaining->tv_nsec < 0) {
                remaining->tv_nsec += 1000000000;
                remaining->tv_sec -= 1;
            }
        }
        return sleep_result == WAIT_IO_COMPLETION ? -1 : -2;
    }
    
    #else
    int res = nanosleep(duration, remaining);
    if (res == 0) {
        return 0;
    } else if (errno == EINTR) {
        return -1;
    } else {
        return -2;
    }
    #endif
}

void thrd_yield(void)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    Sleep(0);
    #else
    sched_yield();
    #endif
}

#ifdef TINYCTHREAD_PLATFORM_WINDOWS
struct TinyCThreadTSSData {
    void *value;
    tss_t key;
    struct TinyCThreadTSSData *next;
};

static tss_dtor_t _tinycthread_tss_dtors[1088] = { NULL };
static _Thread_local struct TinyCThreadTSSData *_tinycthread_tss_head = NULL;
static _Thread_local struct TinyCThreadTSSData *_tinycthread_tss_tail = NULL;

#define TSS_DTOR_ITERATIONS 4
#endif

int tss_create(tss_t *key, tss_dtor_t dtor)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    *key = TlsAlloc();
    if (*key == TLS_OUT_OF_INDEXES) {
        return THRD_ERROR;
    }
    _tinycthread_tss_dtors[*key] = dtor;
    #else
    if (pthread_key_create(key, dtor) != 0) {
        return THRD_ERROR;
    }
    #endif
    
    return THRD_SUCCESS;
}

void tss_delete(tss_t key)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    TlsFree(key);
    _tinycthread_tss_dtors[key] = NULL;
    #else
    pthread_key_delete(key);
    #endif
}

void *tss_get(tss_t key)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return TlsGetValue(key);
    #else
    return pthread_getspecific(key);
    #endif
}

int tss_set(tss_t key, void *val)
{
    #ifdef TINYCTHREAD_PLATFORM_WINDOWS
    return TlsSetValue(key, val) ? THRD_SUCCESS : THRD_ERROR;
    #else
    return pthread_setspecific(key, val) == 0 ? THRD_SUCCESS : THRD_ERROR;
    #endif
}

#ifdef TINYCTHREAD_PLATFORM_WINDOWS
void call_once(once_flag *flag, void (*func)(void))
{
    while (flag->status < 3) {
        switch (flag->status) {
            case 0:
                if (InterlockedCompareExchange(&flag->status, 1, 0) == 0) {
                    InitializeCriticalSection(&flag->lock);
                    EnterCriticalSection(&flag->lock);
                    flag->status = 2;
                    func();
                    flag->status = 3;
                    LeaveCriticalSection(&flag->lock);
                    return;
                }
                break;
            case 1:
                break;
            case 2:
                EnterCriticalSection(&flag->lock);
                LeaveCriticalSection(&flag->lock);
                break;
        }
    }
}
#endif
