/*
 * pthread.c - POSIX threads implementation
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

/* Clone flags for thread creation */
#define CLONE_VM            0x00000100
#define CLONE_FS            0x00000200
#define CLONE_FILES         0x00000400
#define CLONE_SIGHAND       0x00000800
#define CLONE_THREAD        0x00010000
#define CLONE_SYSVSEM       0x00040000
#define CLONE_SETTLS        0x00080000
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_CHILD_SETTID  0x01000000

/* Futex operations */
#define FUTEX_WAIT          0
#define FUTEX_WAKE          1
#define FUTEX_PRIVATE_FLAG  128

/* arch_prctl codes */
#define ARCH_SET_FS         0x1002

/* Default stack size */
#define PTHREAD_STACK_SIZE  (64 * 1024)

/* Thread structure (stored at bottom of stack) */
typedef struct {
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    volatile int tid;
    volatile int exited;
    pthread_t joinable;
} thread_info_t;

/* Use 6-arg syscall for clone and futex */
static inline long syscall6_impl(long num, long a1, long a2, long a3,
                            long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* Clone syscall */
static long sys_clone(unsigned long flags, void *stack, int *parent_tid,
                      int *child_tid, void *tls) {
    return syscall6_impl(SYS_clone, flags, (long)stack, (long)parent_tid,
                    (long)child_tid, (long)tls, 0);
}

/* Futex syscall */
static long sys_futex(int *uaddr, int op, int val) {
    return syscall6_impl(SYS_FUTEX, (long)uaddr, op, val, 0, 0, 0);
}

/* Exit syscall */
static void sys_exit_thread(int status) {
    syscall1(SYS_exit, status);
}

/* Get TID */
static long sys_gettid(void) {
    return syscall0(SYS_getpid);  /* In our simple impl, PID == TID */
}

/* Create a new thread */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
    size_t stacksize = PTHREAD_STACK_SIZE;
    void *stack;
    thread_info_t *info;
    long ret;
    int tid;

    /* Get stack size from attributes */
    if (attr && attr->stacksize > 0) {
        stacksize = attr->stacksize;
    }

    /* Allocate stack */
    stack = malloc(stacksize);
    if (!stack) {
        return -1;  /* ENOMEM */
    }

    /* Place thread info at the top of stack (growing down) */
    info = (thread_info_t *)((char *)stack + stacksize - sizeof(thread_info_t));
    info->start_routine = start_routine;
    info->arg = arg;
    info->retval = NULL;
    info->tid = 0;
    info->exited = 0;
    info->joinable = 0;

    /* Setup stack pointer (below thread_info) */
    void *child_stack = (void *)info;

    /* Clone flags for thread */
    unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                          CLONE_THREAD | CLONE_SYSVSEM |
                          CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;

    /* Create the thread */
    ret = sys_clone(flags, child_stack, &tid, &info->tid, info);

    if (ret < 0) {
        free(stack);
        return -1;
    }

    /* Return thread ID */
    *thread = (pthread_t)info;
    info->tid = (int)ret;

    return 0;
}

/* Wait for a thread to terminate */
int pthread_join(pthread_t thread, void **retval) {
    thread_info_t *info = (thread_info_t *)thread;

    if (!info) {
        return -1;  /* EINVAL */
    }

    /* Wait for thread to exit */
    while (!info->exited) {
        sys_futex((int *)&info->exited, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0);
    }

    /* Get return value */
    if (retval) {
        *retval = info->retval;
    }

    return 0;
}

/* Terminate calling thread */
void pthread_exit(void *retval) {
    thread_info_t *info;
    __asm__ volatile ("mov %%fs:0, %0" : "=r"(info));

    if (info) {
        info->retval = retval;
        info->exited = 1;
        sys_futex((int *)&info->exited, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1);
    }

    sys_exit_thread(0);
}

/* Get thread ID of calling thread */
pthread_t pthread_self(void) {
    thread_info_t *info;
    __asm__ volatile ("mov %%fs:0, %0" : "=r"(info));
    return (pthread_t)info;
}

/* Compare thread IDs */
int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

/* Detach a thread */
int pthread_detach(pthread_t thread) {
    (void)thread;
    /* Our simple implementation doesn't really support detached threads */
    return 0;
}

/* Initialize thread attributes */
int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr) return -1;
    attr->stacksize = PTHREAD_STACK_SIZE;
    attr->stackaddr = NULL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

/* Destroy thread attributes */
int pthread_attr_destroy(pthread_attr_t *attr) {
    (void)attr;
    return 0;
}

/* Set detach state in attributes */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (!attr) return -1;
    attr->detachstate = detachstate;
    return 0;
}

/* Get detach state from attributes */
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    if (!attr || !detachstate) return -1;
    *detachstate = attr->detachstate;
    return 0;
}

/* Set stack size in attributes */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (!attr) return -1;
    attr->stacksize = stacksize;
    return 0;
}

/* Get stack size from attributes */
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
    if (!attr || !stacksize) return -1;
    *stacksize = attr->stacksize;
    return 0;
}

/* Atomic compare and swap */
static inline int atomic_cmpxchg(volatile int *ptr, int old, int new) {
    int prev;
    __asm__ volatile (
        "lock cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(new), "0"(old)
        : "memory"
    );
    return prev;
}

/* Atomic exchange */
static inline int atomic_xchg(volatile int *ptr, int val) {
    __asm__ volatile (
        "xchgl %0, %1"
        : "=r"(val), "+m"(*ptr)
        : "0"(val)
        : "memory"
    );
    return val;
}

/* Initialize mutex */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    if (!mutex) return -1;
    mutex->lock = 0;
    mutex->owner = 0;
    mutex->count = 0;
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_DEFAULT;
    return 0;
}

/* Destroy mutex */
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

/* Lock mutex */
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!mutex) return -1;

    int tid = (int)sys_gettid();

    /* Handle recursive mutex */
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner == tid) {
            mutex->count++;
            return 0;
        }
    }

    /* Try to acquire lock */
    while (atomic_cmpxchg(&mutex->lock, 0, 1) != 0) {
        /* Spin or wait */
        sys_futex(&mutex->lock, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 1);
    }

    mutex->owner = tid;
    mutex->count = 1;
    return 0;
}

/* Try to lock mutex (non-blocking) */
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex) return -1;

    int tid = (int)sys_gettid();

    /* Handle recursive mutex */
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == tid) {
        mutex->count++;
        return 0;
    }

    if (atomic_cmpxchg(&mutex->lock, 0, 1) == 0) {
        mutex->owner = tid;
        mutex->count = 1;
        return 0;
    }

    return -1;  /* EBUSY */
}

/* Unlock mutex */
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex) return -1;

    /* Handle recursive mutex */
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (--mutex->count > 0) {
            return 0;
        }
    }

    mutex->owner = 0;
    mutex->count = 0;
    atomic_xchg(&mutex->lock, 0);

    /* Wake one waiter */
    sys_futex(&mutex->lock, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1);

    return 0;
}

/* Initialize mutex attributes */
int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (!attr) return -1;
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

/* Destroy mutex attributes */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    (void)attr;
    return 0;
}

/* Set mutex type */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (!attr) return -1;
    attr->type = type;
    return 0;
}

/* Get mutex type */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type) {
    if (!attr || !type) return -1;
    *type = attr->type;
    return 0;
}

/* Initialize condition variable */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void)attr;
    if (!cond) return -1;
    cond->seq = 0;
    cond->waiters = 0;
    return 0;
}

/* Destroy condition variable */
int pthread_cond_destroy(pthread_cond_t *cond) {
    (void)cond;
    return 0;
}

/* Wait on condition variable */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (!cond || !mutex) return -1;

    int seq = cond->seq;
    cond->waiters++;

    /* Release mutex */
    pthread_mutex_unlock(mutex);

    /* Wait for signal */
    sys_futex(&cond->seq, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, seq);

    cond->waiters--;

    /* Re-acquire mutex */
    pthread_mutex_lock(mutex);

    return 0;
}

/* Signal one waiter */
int pthread_cond_signal(pthread_cond_t *cond) {
    if (!cond) return -1;

    if (cond->waiters > 0) {
        cond->seq++;
        sys_futex(&cond->seq, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1);
    }

    return 0;
}

/* Signal all waiters */
int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (!cond) return -1;

    if (cond->waiters > 0) {
        cond->seq++;
        sys_futex(&cond->seq, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, cond->waiters);
    }

    return 0;
}

/* Initialize condition variable attributes */
int pthread_condattr_init(pthread_condattr_t *attr) {
    if (!attr) return -1;
    attr->dummy = 0;
    return 0;
}

/* Destroy condition variable attributes */
int pthread_condattr_destroy(pthread_condattr_t *attr) {
    (void)attr;
    return 0;
}

/* Thread-local storage - simple implementation */
#define PTHREAD_KEYS_MAX 64
static void *tls_values[PTHREAD_KEYS_MAX];
static void (*tls_destructors[PTHREAD_KEYS_MAX])(void *);
static int tls_used[PTHREAD_KEYS_MAX];

/* Create TLS key */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if (!key) return -1;

    for (int i = 0; i < PTHREAD_KEYS_MAX; i++) {
        if (!tls_used[i]) {
            tls_used[i] = 1;
            tls_destructors[i] = destructor;
            *key = (pthread_key_t)i;
            return 0;
        }
    }

    return -1;  /* EAGAIN */
}

/* Delete TLS key */
int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !tls_used[key]) {
        return -1;  /* EINVAL */
    }

    tls_used[key] = 0;
    tls_destructors[key] = NULL;
    tls_values[key] = NULL;
    return 0;
}

/* Get TLS value */
void *pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !tls_used[key]) {
        return NULL;
    }
    return tls_values[key];
}

/* Set TLS value */
int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= PTHREAD_KEYS_MAX || !tls_used[key]) {
        return -1;  /* EINVAL */
    }
    tls_values[key] = (void *)value;
    return 0;
}

/* Once control */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) {
        return -1;
    }

    if (atomic_cmpxchg(once_control, 0, 1) == 0) {
        init_routine();
    }

    return 0;
}
