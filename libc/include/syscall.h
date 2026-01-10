/*
 * syscall.h - System call definitions for user space
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

/* System call numbers (Linux x86_64 compatible) */
#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_lstat       6
#define SYS_poll        7
#define SYS_lseek       8
#define SYS_mmap        9
#define SYS_mprotect    10
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_sigaction   13
#define SYS_sigprocmask 14
#define SYS_sigreturn   15
#define SYS_ioctl       16
#define SYS_pread64     17
#define SYS_pwrite64    18
#define SYS_readv       19
#define SYS_writev      20
#define SYS_access      21
#define SYS_pipe        22
#define SYS_select      23
#define SYS_sched_yield 24
#define SYS_dup         32
#define SYS_dup2        33
#define SYS_nanosleep   35
#define SYS_getpid      39
#define SYS_socket      41
#define SYS_connect     42
#define SYS_accept      43
#define SYS_sendto      44
#define SYS_recvfrom    45
#define SYS_bind        49
#define SYS_listen      50
#define SYS_clone       56
#define SYS_fork        57
#define SYS_vfork       58
#define SYS_execve      59
#define SYS_exit        60
#define SYS_wait4       61
#define SYS_kill        62
#define SYS_uname       63
#define SYS_fcntl       72
#define SYS_ftruncate   77
#define SYS_getcwd      79
#define SYS_chdir       80
#define SYS_fchdir      81
#define SYS_rename      82
#define SYS_mkdir       83
#define SYS_rmdir       84
#define SYS_link        86
#define SYS_unlink      87
#define SYS_symlink     88
#define SYS_readlink    89
#define SYS_chmod       90
#define SYS_fchmod      91
#define SYS_chown       92
#define SYS_fchown      93
#define SYS_lchown      94
#define SYS_umask       95
#define SYS_gettimeofday 96
#define SYS_getrlimit   97
#define SYS_getrusage   98
#define SYS_times       100
#define SYS_getuid      102
#define SYS_getgid      104
#define SYS_setuid      105
#define SYS_setgid      106
#define SYS_geteuid     107
#define SYS_getegid     108
#define SYS_setpgid     109
#define SYS_getppid     110
#define SYS_getpgrp     111
#define SYS_setsid      112
#define SYS_setreuid    113
#define SYS_setregid    114
#define SYS_getgroups   115
#define SYS_setgroups   116
#define SYS_setresuid   117
#define SYS_getresuid   118
#define SYS_setresgid   119
#define SYS_getresgid   120
#define SYS_setrlimit   160
#define SYS_ARCH_PRCTL  158
#define SYS_FUTEX       202
#define SYS_getdents64  217
#define SYS_SET_TID_ADDRESS 218
#define SYS_clock_gettime 228
#define SYS_exit_group  231
#define SYS_pselect6    270
#define SYS_ppoll       271
#define SYS_pipe2       293

/* Inline syscall wrappers */
static inline long syscall0(long num) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall1(long num, long arg1) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall2(long num, long arg1, long arg2) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long ret;
    register long r10 __asm__("r10") = arg4;
    register long r8 __asm__("r8") = arg5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif /* _SYSCALL_H */
