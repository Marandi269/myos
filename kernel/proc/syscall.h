/*
 * syscall.h - System call interface
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

/* MSR addresses for SYSCALL/SYSRET */
#define MSR_EFER    0xC0000080  /* Extended Feature Enable Register */
#define MSR_STAR    0xC0000081  /* Segment selectors */
#define MSR_LSTAR   0xC0000082  /* SYSCALL entry point (64-bit) */
#define MSR_CSTAR   0xC0000083  /* SYSCALL entry point (compat, unused) */
#define MSR_SFMASK  0xC0000084  /* RFLAGS mask for SYSCALL */

/* EFER bits */
#define EFER_SCE    (1 << 0)    /* SYSCALL/SYSRET Enable */

/*
 * System call numbers (Linux x86_64 compatible)
 */
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_STAT        4
#define SYS_FSTAT       5
#define SYS_LSTAT       6
#define SYS_POLL        7
#define SYS_LSEEK       8
#define SYS_MMAP        9
#define SYS_MPROTECT    10
#define SYS_MUNMAP      11
#define SYS_BRK         12
#define SYS_SIGACTION   13
#define SYS_SIGPROCMASK 14
#define SYS_SIGRETURN   15
#define SYS_IOCTL       16
#define SYS_READV       19
#define SYS_WRITEV      20
#define SYS_ACCESS      21
#define SYS_PIPE        22
#define SYS_SELECT      23
#define SYS_DUP         32
#define SYS_DUP2        33
#define SYS_NANOSLEEP   35
#define SYS_GETPID      39
#define SYS_CLONE       56
#define SYS_FORK        57
#define SYS_EXECVE      59
#define SYS_EXIT        60
#define SYS_WAIT4       61
#define SYS_KILL        62
#define SYS_UNAME       63
#define SYS_FCNTL       72
#define SYS_FTRUNCATE   77
#define SYS_GETCWD      79
#define SYS_CHDIR       80
#define SYS_RENAME      82
#define SYS_MKDIR       83
#define SYS_RMDIR       84
#define SYS_LINK        86
#define SYS_UNLINK      87
#define SYS_SYMLINK     88
#define SYS_READLINK    89
#define SYS_CHMOD       90
#define SYS_FCHMOD      91
#define SYS_CHOWN       92
#define SYS_FCHOWN      93
#define SYS_UMASK       95
#define SYS_GETTIMEOFDAY 96
#define SYS_GETRLIMIT   97
#define SYS_TIMES       100
#define SYS_GETUID      102
#define SYS_GETGID      104
#define SYS_SETUID      105
#define SYS_SETGID      106
#define SYS_GETEUID     107
#define SYS_GETEGID     108
#define SYS_GETPPID     110
#define SYS_SETSID      112
#define SYS_ARCH_PRCTL  158
#define SYS_SETRLIMIT   160
#define SYS_FUTEX       202
#define SYS_GETDENTS64  217
#define SYS_SET_TID_ADDRESS 218
#define SYS_CLOCK_GETTIME 228
#define SYS_EXIT_GROUP  231
#define SYS_PSELECT6    270
#define SYS_PPOLL       271
#define SYS_PIPE2       293

#define MAX_SYSCALL     512

/* Error codes */
#define ENOSYS      38  /* Function not implemented */
#define EBADF       9   /* Bad file descriptor */
#define EINVAL      22  /* Invalid argument */
#define ENOMEM      12  /* Out of memory */
#define EFAULT      14  /* Bad address */
#define ENOENT      2   /* No such file or directory */
#define ECHILD      10  /* No child processes */
#define EAGAIN      11  /* Try again */
#define EACCES      13  /* Permission denied */
#define ENOTDIR     20  /* Not a directory */
#define EISDIR      21  /* Is a directory */
#define ERANGE      34  /* Math result not representable */
#define ENOEXEC     8   /* Exec format error */
#define ESRCH       3   /* No such process */
#define EIO         5   /* I/O error */
#define EPIPE       32  /* Broken pipe */
#define EMFILE      24  /* Too many open files */

/* System call handler function type */
typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

/* Initialize SYSCALL/SYSRET */
void syscall_init(void);

/* Initialize syscall stdio (call after fs_init) */
void syscall_init_stdio(void);

/* System call handler (called from assembly) */
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Register a system call handler */
void syscall_register(int num, syscall_fn_t handler);

/*
 * Individual system call declarations
 */
int64_t sys_read(int fd, char *buf, size_t count);
int64_t sys_write(int fd, const char *buf, size_t count);
struct iovec;  /* Forward declaration */
int64_t sys_readv(int fd, const struct iovec *iov, int iovcnt);
int64_t sys_writev(int fd, const struct iovec *iov, int iovcnt);
int64_t sys_open(const char *pathname, int flags, int mode);
int64_t sys_close(int fd);
int64_t sys_lseek(int fd, int64_t offset, int whence);
int64_t sys_brk(uint64_t addr);
int64_t sys_getpid(void);
int64_t sys_getppid(void);
int64_t sys_exit(int status);
int64_t sys_exit_group(int status);
int64_t sys_dup(int oldfd);
int64_t sys_dup2(int oldfd, int newfd);

/* Process management syscalls */
int64_t sys_fork(void);
int64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]);
int64_t sys_wait4(int pid, int *status, int options, void *rusage);

/* Directory syscalls */
int64_t sys_getcwd(char *buf, size_t size);
int64_t sys_chdir(const char *path);
int64_t sys_mkdir(const char *pathname, uint32_t mode);
int64_t sys_getdents64(int fd, void *dirp, size_t count);

/* Linux dirent64 structure */
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

/*
 * I/O Multiplexing data structures
 */

/* fd_set for select() */
#define FD_SETSIZE 256

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

/* fd_set manipulation macros */
#define __FDELT(fd)     ((fd) / (8 * sizeof(unsigned long)))
#define __FDMASK(fd)    (1UL << ((fd) % (8 * sizeof(unsigned long))))

#define FD_ZERO(set)    do { \
    unsigned int __i; \
    for (__i = 0; __i < sizeof((set)->fds_bits)/sizeof((set)->fds_bits[0]); __i++) \
        (set)->fds_bits[__i] = 0; \
} while (0)

#define FD_SET(fd, set)   ((set)->fds_bits[__FDELT(fd)] |= __FDMASK(fd))
#define FD_CLR(fd, set)   ((set)->fds_bits[__FDELT(fd)] &= ~__FDMASK(fd))
#define FD_ISSET(fd, set) (((set)->fds_bits[__FDELT(fd)] & __FDMASK(fd)) != 0)

/* timeval structure for select() */
struct timeval {
    int64_t tv_sec;     /* Seconds */
    int64_t tv_usec;    /* Microseconds */
};

/* timespec structure for pselect()/ppoll() */
struct timespec {
    int64_t tv_sec;     /* Seconds */
    int64_t tv_nsec;    /* Nanoseconds */
};

/* pollfd structure for poll() */
struct pollfd {
    int   fd;           /* File descriptor */
    short events;       /* Requested events */
    short revents;      /* Returned events */
};

/* Poll event flags */
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020
#define POLLRDNORM  0x0040
#define POLLRDBAND  0x0080
#define POLLWRNORM  0x0100
#define POLLWRBAND  0x0200

/* I/O Multiplexing syscall declarations */
int64_t sys_poll(struct pollfd *fds, uint64_t nfds, int timeout);
int64_t sys_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, struct timeval *timeout);

/*
 * New system calls for Phase 12
 */

/* File stat syscalls */
struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

int64_t sys_stat(const char *pathname, struct stat *statbuf);
int64_t sys_fstat(int fd, struct stat *statbuf);
int64_t sys_lstat(const char *pathname, struct stat *statbuf);

/* File operations */
int64_t sys_access(const char *pathname, int mode);
int64_t sys_chmod(const char *pathname, uint32_t mode);
int64_t sys_fchmod(int fd, uint32_t mode);
int64_t sys_chown(const char *pathname, uint32_t owner, uint32_t group);
int64_t sys_fchown(int fd, uint32_t owner, uint32_t group);
int64_t sys_link(const char *oldpath, const char *newpath);
int64_t sys_unlink(const char *pathname);
int64_t sys_symlink(const char *target, const char *linkpath);
int64_t sys_readlink(const char *pathname, char *buf, size_t bufsiz);
int64_t sys_rename(const char *oldpath, const char *newpath);
int64_t sys_rmdir(const char *pathname);
int64_t sys_umask(uint32_t mask);
int64_t sys_ftruncate(int fd, int64_t length);
int64_t sys_fcntl(int fd, int cmd, uint64_t arg);
int64_t sys_ioctl_impl(int fd, unsigned long request, void *arg);

/* User/group IDs */
int64_t sys_getuid(void);
int64_t sys_geteuid(void);
int64_t sys_getgid(void);
int64_t sys_getegid(void);
int64_t sys_setuid(uint32_t uid);
int64_t sys_setgid(uint32_t gid);

/* System information */
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

int64_t sys_uname(struct utsname *buf);

/* Time */
int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem);
int64_t sys_clock_gettime(int clk_id, struct timespec *tp);
int64_t sys_gettimeofday(struct timeval *tv, void *tz);

/* Resource limits */
struct rlimit {
    uint64_t rlim_cur;  /* Soft limit */
    uint64_t rlim_max;  /* Hard limit */
};

int64_t sys_getrlimit(int resource, struct rlimit *rlim);
int64_t sys_setrlimit(int resource, const struct rlimit *rlim);

/* Process times */
struct tms {
    int64_t tms_utime;   /* User CPU time */
    int64_t tms_stime;   /* System CPU time */
    int64_t tms_cutime;  /* User CPU time of children */
    int64_t tms_cstime;  /* System CPU time of children */
};

int64_t sys_times(struct tms *buf);

/* Session ID */
int64_t sys_setsid(void);

#endif /* _SYSCALL_H */
