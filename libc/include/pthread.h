/*
 * pthread.h - POSIX threads interface
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stddef.h>
#include <stdint.h>

/* Thread identifier */
typedef unsigned long pthread_t;

/* Thread attributes */
typedef struct {
    size_t stacksize;
    void *stackaddr;
    int detachstate;
} pthread_attr_t;

/* Mutex types */
#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

/* Mutex structure */
typedef struct {
    volatile int lock;
    volatile int owner;
    volatile int count;  /* For recursive mutex */
    int type;
} pthread_mutex_t;

/* Mutex initializers */
#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, PTHREAD_MUTEX_DEFAULT }

/* Mutex attributes */
typedef struct {
    int type;
} pthread_mutexattr_t;

/* Condition variable structure */
typedef struct {
    volatile int seq;
    volatile int waiters;
} pthread_cond_t;

/* Condition variable initializer */
#define PTHREAD_COND_INITIALIZER { 0, 0 }

/* Condition variable attributes */
typedef struct {
    int dummy;
} pthread_condattr_t;

/* Detach state */
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* Thread functions */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
void pthread_exit(void *retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_detach(pthread_t thread);

/* Thread attributes */
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);

/* Mutex functions */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Mutex attribute functions */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);

/* Condition variable functions */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

/* Condition variable attribute functions */
int pthread_condattr_init(pthread_condattr_t *attr);
int pthread_condattr_destroy(pthread_condattr_t *attr);

/* Thread-local storage key */
typedef unsigned int pthread_key_t;

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

/* Once control */
typedef int pthread_once_t;
#define PTHREAD_ONCE_INIT 0

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

#endif /* _PTHREAD_H */
