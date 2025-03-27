#ifndef TINYCTHREAD_H
#define TINYCTHREAD_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    #define TINYCTHREAD_PLATFORM_WINDOWS
    #include <windows.h>
#else
    #define TINYCTHREAD_PLATFORM_POSIX
    #include <pthread.h>
    #include <sched.h>
    #include <unistd.h>
    #include <errno.h>
#endif

#define TINYCTHREAD_VERSION_MAJOR 2
#define TINYCTHREAD_VERSION_MINOR 0
#define TINYCTHREAD_VERSION (TINYCTHREAD_VERSION_MAJOR * 100 + TINYCTHREAD_VERSION_MINOR)

typedef enum {
    THRD_SUCCESS = 0,
    THRD_ERROR,
    THRD_TIMEOUT,
    THRD_BUSY,
    THRD_NO_MEM
} thrd_result;

typedef enum {
    MTX_PLAIN = 0,
    MTX_TIMED = 1,
    MTX_RECURSIVE = 2
} mtx_type;

typedef int (*thrd_start_t)(void *arg);
typedef void (*tss_dtor_t)(void *val);

#ifdef TINYCTHREAD_PLATFORM_WINDOWS
typedef HANDLE thrd_t;
typedef DWORD tss_t;
typedef struct {
    union {
        CRITICAL_SECTION cs;
        HANDLE mut;
    } handle;
    atomic_bool already_locked;
    bool is_recursive;
    bool is_timed;
} mtx_t;

typedef struct {
    HANDLE events[2];
    atomic_uint waiters_count;
    CRITICAL_SECTION waiters_lock;
} cnd_t;

typedef struct {
    LONG volatile status;
    CRITICAL_SECTION lock;
} once_flag;

#define ONCE_FLAG_INIT {0}

#else
typedef pthread_t thrd_t;
typedef pthread_key_t tss_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_once_t once_flag;
#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#endif

int mtx_init(mtx_t *mtx, int type);
void mtx_destroy(mtx_t *mtx);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *mtx, const struct timespec *ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);

int cnd_init(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mtx);
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts);

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t thr0, thrd_t thr1);
_Noreturn void thrd_exit(int res);
int thrd_join(thrd_t thr, int *res);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);

int tss_create(tss_t *key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);

#ifdef TINYCTHREAD_PLATFORM_WINDOWS
void call_once(once_flag *flag, void (*func)(void));
#else
    #define call_once(flag, func) pthread_once(flag, func)
#endif

#ifdef __cplusplus
}
#endif

#endif
