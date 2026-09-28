#include <sys/iosupport.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/reent.h>
#include <string.h>

#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/env.h>
#include <3ds/synchronization.h>
#include "../internal.h"

void __ctru_exit(int rc);
int __libctru_gtod(struct _reent *ptr, struct timeval *tp, struct timezone *tz);

extern const u8 __tdata_lma[];
extern const u8 __tdata_lma_end[];
extern u8 __tls_start[];

struct _reent* __SYSCALL(getreent)(void)
{
    ThreadVars* tv = getThreadVars();
    if (tv->magic != THREADVARS_MAGIC)
    {
        svcBreak(USERBREAK_PANIC);
        for (;;);
    }
    return tv->reent;
}

int __SYSCALL(gettod_r)(struct _reent *ptr, struct timeval *tp, struct timezone *tz)
{
    return __libctru_gtod(ptr, tp, tz);
}

void __SYSCALL(lock_init)(_LOCK_T *lock) { LightLock_Init(lock); }
void __SYSCALL(lock_acquire)(_LOCK_T *lock) { LightLock_Lock(lock); }
int __SYSCALL(lock_try_acquire)(_LOCK_T *lock) { return LightLock_TryLock(lock); }
void __SYSCALL(lock_release)(_LOCK_T *lock) { LightLock_Unlock(lock); }

void __SYSCALL(lock_init_recursive)(_LOCK_RECURSIVE_T *lock) { RecursiveLock_Init(lock); }
void __SYSCALL(lock_acquire_recursive)(_LOCK_RECURSIVE_T *lock) { RecursiveLock_Lock(lock); }
int __SYSCALL(lock_try_acquire_recursive)(_LOCK_RECURSIVE_T *lock) { return RecursiveLock_TryLock(lock); }
void __SYSCALL(lock_release_recursive)(_LOCK_RECURSIVE_T *lock) { RecursiveLock_Unlock(lock); }

void __SYSCALL(exit)(int rc)
{
    __ctru_exit(rc);
}

void __system_initSyscalls(void)
{
    ThreadVars* tv = getThreadVars();
    tv->magic = THREADVARS_MAGIC;
    tv->reent = _impure_ptr;
    tv->thread_ptr = NULL;
    tv->tls_tp = __tls_start - 8;

    u32 tls_size = __tdata_lma_end - __tdata_lma;
    if (tls_size)
        memcpy(__tls_start, __tdata_lma, tls_size);
}
