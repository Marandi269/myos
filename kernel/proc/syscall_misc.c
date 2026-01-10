/*
 * syscall_misc.c - Miscellaneous system calls
 */

#include "syscall.h"
#include "process.h"
#include "../lib/string.h"
#include "../drivers/pit.h"

/* External access to current process */
extern process_t *current_proc;

static inline process_t* current_process(void) {
    return current_proc;
}

/* Error codes */
#ifndef EPERM
#define EPERM 1
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ESRCH
#define ESRCH 3
#endif

/* User/group IDs for the current process (simplified - always root) */
static uint32_t current_uid = 0;
static uint32_t current_euid = 0;
static uint32_t current_gid = 0;
static uint32_t current_egid = 0;

int64_t sys_getuid(void) {
    return current_uid;
}

int64_t sys_geteuid(void) {
    return current_euid;
}

int64_t sys_getgid(void) {
    return current_gid;
}

int64_t sys_getegid(void) {
    return current_egid;
}

int64_t sys_setuid(uint32_t uid) {
    /* Only root can change uid */
    if (current_euid != 0 && uid != current_uid) {
        return -EPERM;
    }
    current_uid = uid;
    current_euid = uid;
    return 0;
}

int64_t sys_setgid(uint32_t gid) {
    /* Only root can change gid */
    if (current_euid != 0 && gid != current_gid) {
        return -EPERM;
    }
    current_gid = gid;
    current_egid = gid;
    return 0;
}

int64_t sys_uname(struct utsname *buf) {
    if (!buf) {
        return -EINVAL;
    }

    memset(buf, 0, sizeof(*buf));
    strcpy(buf->sysname, "MyOS");
    strcpy(buf->nodename, "myos");
    strcpy(buf->release, "0.1.0");
    strcpy(buf->version, "#1 SMP");
    strcpy(buf->machine, "x86_64");
    strcpy(buf->domainname, "(none)");

    return 0;
}

/* Clock IDs */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

/* Get current time in ticks and convert to timespec */
static void ticks_to_timespec(uint64_t ticks, struct timespec *ts) {
    /* Assuming 100 Hz timer (10ms per tick) */
    ts->tv_sec = ticks / 100;
    ts->tv_nsec = (ticks % 100) * 10000000;  /* 10ms = 10,000,000 ns */
}

int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    if (!req) {
        return -EINVAL;
    }

    /* Convert to ticks */
    uint64_t ticks_to_sleep = req->tv_sec * 100 + req->tv_nsec / 10000000;
    uint64_t start_ticks = pit_get_ticks();
    uint64_t end_ticks = start_ticks + ticks_to_sleep;

    /* Simple busy wait - TODO: use proper sleep mechanism */
    while (pit_get_ticks() < end_ticks) {
        /* Could yield here */
        __asm__ volatile ("pause");
    }

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return 0;
}

int64_t sys_clock_gettime(int clk_id, struct timespec *tp) {
    if (!tp) {
        return -EINVAL;
    }

    uint64_t ticks = pit_get_ticks();

    switch (clk_id) {
        case CLOCK_REALTIME:
        case CLOCK_MONOTONIC:
        case CLOCK_MONOTONIC + 4:  /* CLOCK_BOOTTIME */
            ticks_to_timespec(ticks, tp);
            /* Add a base time for CLOCK_REALTIME (Jan 1, 2024) */
            if (clk_id == CLOCK_REALTIME) {
                tp->tv_sec += 1704067200;  /* 2024-01-01 00:00:00 UTC */
            }
            return 0;

        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_THREAD_CPUTIME_ID:
            ticks_to_timespec(ticks, tp);
            return 0;

        default:
            return -EINVAL;
    }
}

int64_t sys_gettimeofday(struct timeval *tv, void *tz) {
    if (!tv) {
        return -EINVAL;
    }

    struct timespec ts;
    int64_t ret = sys_clock_gettime(CLOCK_REALTIME, &ts);
    if (ret < 0) {
        return ret;
    }

    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;

    (void)tz;  /* Timezone ignored */
    return 0;
}

/* Resource limits */
#define RLIMIT_CPU        0
#define RLIMIT_FSIZE      1
#define RLIMIT_DATA       2
#define RLIMIT_STACK      3
#define RLIMIT_CORE       4
#define RLIMIT_RSS        5
#define RLIMIT_NPROC      6
#define RLIMIT_NOFILE     7
#define RLIMIT_MEMLOCK    8
#define RLIMIT_AS         9
#define RLIMIT_LOCKS      10
#define RLIMIT_SIGPENDING 11
#define RLIMIT_MSGQUEUE   12
#define RLIMIT_NICE       13
#define RLIMIT_RTPRIO     14
#define RLIMIT_RTTIME     15
#define RLIM_NLIMITS      16

#define RLIM_INFINITY     (~0UL)

/* Default resource limits */
static struct rlimit default_rlimits[RLIM_NLIMITS] = {
    [RLIMIT_CPU] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_FSIZE] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_DATA] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_STACK] = { 8 * 1024 * 1024, RLIM_INFINITY },
    [RLIMIT_CORE] = { 0, RLIM_INFINITY },
    [RLIMIT_RSS] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_NPROC] = { 32, 32 },
    [RLIMIT_NOFILE] = { 256, 256 },
    [RLIMIT_MEMLOCK] = { 64 * 1024, 64 * 1024 },
    [RLIMIT_AS] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_LOCKS] = { RLIM_INFINITY, RLIM_INFINITY },
    [RLIMIT_SIGPENDING] = { 128, 128 },
    [RLIMIT_MSGQUEUE] = { 819200, 819200 },
    [RLIMIT_NICE] = { 0, 0 },
    [RLIMIT_RTPRIO] = { 0, 0 },
    [RLIMIT_RTTIME] = { RLIM_INFINITY, RLIM_INFINITY },
};

int64_t sys_getrlimit(int resource, struct rlimit *rlim) {
    if (!rlim || resource < 0 || resource >= RLIM_NLIMITS) {
        return -EINVAL;
    }

    *rlim = default_rlimits[resource];
    return 0;
}

int64_t sys_setrlimit(int resource, const struct rlimit *rlim) {
    if (!rlim || resource < 0 || resource >= RLIM_NLIMITS) {
        return -EINVAL;
    }

    /* Only allow lowering limits (simplified check) */
    if (rlim->rlim_cur <= default_rlimits[resource].rlim_max) {
        default_rlimits[resource] = *rlim;
        return 0;
    }

    return -EPERM;
}

int64_t sys_times(struct tms *buf) {
    if (!buf) {
        return -EINVAL;
    }

    uint64_t ticks = pit_get_ticks();

    buf->tms_utime = ticks;
    buf->tms_stime = 0;
    buf->tms_cutime = 0;
    buf->tms_cstime = 0;

    return ticks;
}

int64_t sys_setsid(void) {
    process_t *proc = current_process();
    if (!proc) {
        return -ESRCH;
    }

    /* Create a new session */
    /* In a real implementation, this would set the process as session leader */
    return proc->pid;
}
