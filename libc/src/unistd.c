/*
 * unistd.c - POSIX system calls
 */

#include <unistd.h>
#include <syscall.h>
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

/* File I/O */
ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, count);
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    /* SYS_pread64 = 17 */
    return syscall4(17, fd, (long)buf, count, offset);
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    /* SYS_pwrite64 = 18 */
    return syscall4(18, fd, (long)buf, count, offset);
}

int close(int fd) {
    return syscall1(SYS_close, fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return syscall3(SYS_lseek, fd, offset, whence);
}

int dup(int oldfd) {
    return syscall1(SYS_dup, oldfd);
}

int dup2(int oldfd, int newfd) {
    return syscall2(SYS_dup2, oldfd, newfd);
}

int dup3(int oldfd, int newfd, int flags) {
    /* SYS_dup3 = 292 */
    return syscall3(292, oldfd, newfd, flags);
}

int pipe(int pipefd[2]) {
    return syscall1(SYS_pipe, (long)pipefd);
}

int pipe2(int pipefd[2], int flags) {
    return syscall2(SYS_pipe2, (long)pipefd, flags);
}

/* File operations */
int link(const char *oldpath, const char *newpath) {
    return syscall2(SYS_link, (long)oldpath, (long)newpath);
}

int unlink(const char *pathname) {
    return syscall1(SYS_unlink, (long)pathname);
}

int symlink(const char *target, const char *linkpath) {
    return syscall2(SYS_symlink, (long)target, (long)linkpath);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    return syscall3(SYS_readlink, (long)pathname, (long)buf, bufsiz);
}

int rmdir(const char *pathname) {
    return syscall1(SYS_rmdir, (long)pathname);
}

int truncate(const char *path, off_t length) {
    /* SYS_truncate = 76 */
    return syscall2(76, (long)path, length);
}

int ftruncate(int fd, off_t length) {
    return syscall2(SYS_ftruncate, fd, length);
}

/* Access checking */
int access(const char *pathname, int mode) {
    return syscall2(SYS_access, (long)pathname, mode);
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    /* SYS_faccessat = 269 */
    return syscall4(269, dirfd, (long)pathname, mode, flags);
}

/* Process control */
pid_t fork(void) {
    return syscall0(SYS_fork);
}

pid_t vfork(void) {
    return syscall0(SYS_vfork);
}

pid_t getpid(void) {
    return syscall0(SYS_getpid);
}

pid_t getppid(void) {
    return syscall0(SYS_getppid);
}

pid_t getpgrp(void) {
    return syscall0(SYS_getpgrp);
}

pid_t getpgid(pid_t pid) {
    /* SYS_getpgid = 121 */
    return syscall1(121, pid);
}

int setpgid(pid_t pid, pid_t pgid) {
    return syscall2(SYS_setpgid, pid, pgid);
}

pid_t setsid(void) {
    return syscall0(SYS_setsid);
}

pid_t getsid(pid_t pid) {
    /* SYS_getsid = 124 */
    return syscall1(124, pid);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    return syscall3(SYS_execve, (long)pathname, (long)argv, (long)envp);
}

int execv(const char *pathname, char *const argv[]) {
    return execve(pathname, argv, NULL);
}

int execvp(const char *file, char *const argv[]) {
    /* Simple implementation - just try the path directly */
    return execve(file, argv, NULL);
}

int execvpe(const char *file, char *const argv[], char *const envp[]) {
    return execve(file, argv, envp);
}

void _exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

/* User and group IDs */
uid_t getuid(void) {
    return syscall0(SYS_getuid);
}

uid_t geteuid(void) {
    return syscall0(SYS_geteuid);
}

gid_t getgid(void) {
    return syscall0(SYS_getgid);
}

gid_t getegid(void) {
    return syscall0(SYS_getegid);
}

int setuid(uid_t uid) {
    return syscall1(SYS_setuid, uid);
}

int seteuid(uid_t uid) {
    return syscall1(SYS_setuid, uid);  /* Simplified */
}

int setgid(gid_t gid) {
    return syscall1(SYS_setgid, gid);
}

int setegid(gid_t gid) {
    return syscall1(SYS_setgid, gid);  /* Simplified */
}

int setreuid(uid_t ruid, uid_t euid) {
    return syscall2(SYS_setreuid, ruid, euid);
}

int setregid(gid_t rgid, gid_t egid) {
    return syscall2(SYS_setregid, rgid, egid);
}

int getgroups(int size, gid_t list[]) {
    return syscall2(SYS_getgroups, size, (long)list);
}

int setgroups(size_t size, const gid_t *list) {
    return syscall2(SYS_setgroups, size, (long)list);
}

/* Working directory */
char *getcwd(char *buf, size_t size) {
    long ret = syscall2(SYS_getcwd, (long)buf, size);
    return (ret < 0) ? NULL : buf;
}

int chdir(const char *path) {
    return syscall1(SYS_chdir, (long)path);
}

int fchdir(int fd) {
    return syscall1(SYS_fchdir, fd);
}

int chroot(const char *path) {
    /* SYS_chroot = 161 */
    return syscall1(161, (long)path);
}

/* Hostname */
static char hostname[256] = "myos";
static char domainname[256] = "";

int gethostname(char *name, size_t len) {
    size_t hlen = strlen(hostname);
    if (hlen + 1 > len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strcpy(name, hostname);
    return 0;
}

int sethostname(const char *name, size_t len) {
    if (len >= sizeof(hostname)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(hostname, name, len);
    hostname[len] = '\0';
    return 0;
}

int getdomainname(char *name, size_t len) {
    size_t dlen = strlen(domainname);
    if (dlen + 1 > len) {
        errno = EINVAL;
        return -1;
    }
    strcpy(name, domainname);
    return 0;
}

int setdomainname(const char *name, size_t len) {
    if (len >= sizeof(domainname)) {
        errno = EINVAL;
        return -1;
    }
    memcpy(domainname, name, len);
    domainname[len] = '\0';
    return 0;
}

/* Sleeping */
unsigned int sleep(unsigned int seconds) {
    struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };
    struct timespec rem;
    if (nanosleep(&req, &rem) < 0) {
        return rem.tv_sec;
    }
    return 0;
}

int usleep(unsigned int usec) {
    struct timespec req = {
        .tv_sec = usec / 1000000,
        .tv_nsec = (usec % 1000000) * 1000
    };
    return nanosleep(&req, NULL);
}

unsigned int alarm(unsigned int seconds) {
    /* SYS_alarm = 37 */
    return syscall1(37, seconds);
}

int pause(void) {
    /* SYS_pause = 34 */
    return syscall0(34);
}

/* File sync */
int fsync(int fd) {
    /* SYS_fsync = 74 */
    return syscall1(74, fd);
}

int fdatasync(int fd) {
    /* SYS_fdatasync = 75 */
    return syscall1(75, fd);
}

void sync(void) {
    /* SYS_sync = 162 */
    syscall0(162);
}

/* Configuration */
long sysconf(int name) {
    switch (name) {
        case _SC_CLK_TCK: return 100;  /* 100 Hz clock */
        case _SC_PAGE_SIZE: return 4096;
        case _SC_OPEN_MAX: return 256;
        case _SC_HOST_NAME_MAX: return 255;
        case _SC_ARG_MAX: return 131072;
        case _SC_CHILD_MAX: return 32;
        case _SC_NGROUPS_MAX: return 32;
        default:
            errno = EINVAL;
            return -1;
    }
}

long pathconf(const char *path, int name) {
    (void)path;
    switch (name) {
        case _PC_NAME_MAX: return 255;
        case _PC_PATH_MAX: return 4096;
        case _PC_PIPE_BUF: return 4096;
        case _PC_LINK_MAX: return 127;
        default:
            errno = EINVAL;
            return -1;
    }
}

long fpathconf(int fd, int name) {
    (void)fd;
    return pathconf(NULL, name);
}

/* Terminal */
int isatty(int fd) {
    /* Simple check - try ioctl TCGETS */
    /* For now, assume fd 0, 1, 2 are ttys */
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

char *ttyname(int fd) {
    static char ttyname_buf[32];
    if (isatty(fd)) {
        strcpy(ttyname_buf, "/dev/console");
        return ttyname_buf;
    }
    return NULL;
}

int ttyname_r(int fd, char *buf, size_t buflen) {
    if (!isatty(fd)) {
        errno = ENOTTY;
        return ENOTTY;
    }
    if (buflen < 13) {  /* "/dev/console" + null */
        errno = ERANGE;
        return ERANGE;
    }
    strcpy(buf, "/dev/console");
    return 0;
}

/* Misc */
int nice(int inc) {
    /* SYS_nice doesn't exist on x86_64, use SYS_setpriority */
    (void)inc;
    return 0;  /* Stub */
}

void swab(const void *from, void *to, ssize_t n) {
    const unsigned char *f = from;
    unsigned char *t = to;
    while (n >= 2) {
        t[0] = f[1];
        t[1] = f[0];
        f += 2;
        t += 2;
        n -= 2;
    }
}

char *crypt(const char *key, const char *salt) {
    (void)key;
    (void)salt;
    return NULL;  /* Not implemented */
}

/* open() implementation */
int open(const char *pathname, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) {
        mode = 0644;
    }
    return syscall3(SYS_open, (long)pathname, flags, mode);
}

int mkdir(const char *pathname, unsigned int mode) {
    return syscall2(SYS_mkdir, (long)pathname, mode);
}

/* getopt implementation */
char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char *const argv[], const char *optstring) {
    static int sp = 1;

    if (sp == 1) {
        if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
    }

    char c = argv[optind][sp];
    const char *cp = strchr(optstring, c);

    if (c == ':' || cp == NULL) {
        optopt = c;
        if (opterr) {
            /* Could print error message */
        }
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return '?';
    }

    if (cp[1] == ':') {
        if (argv[optind][sp + 1] != '\0') {
            optarg = &argv[optind][sp + 1];
        } else if (++optind >= argc) {
            optopt = c;
            if (opterr) {
                /* Could print error message */
            }
            sp = 1;
            return optstring[0] == ':' ? ':' : '?';
        } else {
            optarg = argv[optind];
        }
        optind++;
        sp = 1;
    } else {
        if (argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }

    return c;
}
