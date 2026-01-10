/*
 * unistd.h - POSIX API
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

/* File descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Types */
typedef int pid_t;
typedef long off_t;
typedef int uid_t;
typedef int gid_t;

/* POSIX constants */
#define _POSIX_VERSION 200809L
#define _POSIX2_VERSION 200809L

/* Seek whence */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* access() mode flags */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

/* pathconf() and sysconf() names */
#define _SC_CLK_TCK         2
#define _SC_PAGE_SIZE       30
#define _SC_PAGESIZE        30
#define _SC_OPEN_MAX        4
#define _SC_HOST_NAME_MAX   180
#define _SC_ARG_MAX         0
#define _SC_CHILD_MAX       1
#define _SC_NGROUPS_MAX     3

#define _PC_NAME_MAX        3
#define _PC_PATH_MAX        4
#define _PC_PIPE_BUF        5
#define _PC_LINK_MAX        0

/* File I/O */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);

/* File operations */
int link(const char *oldpath, const char *newpath);
int unlink(const char *pathname);
int symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
int rmdir(const char *pathname);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);

/* Access checking */
int access(const char *pathname, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);

/* Process control */
pid_t fork(void);
pid_t vfork(void);
pid_t getpid(void);
pid_t getppid(void);
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
pid_t setsid(void);
pid_t getsid(pid_t pid);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);
int execl(const char *pathname, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execle(const char *pathname, const char *arg, ...);
void _exit(int status);

/* User and group IDs */
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int seteuid(uid_t uid);
int setgid(gid_t gid);
int setegid(gid_t gid);
int setreuid(uid_t ruid, uid_t euid);
int setregid(gid_t rgid, gid_t egid);
int getgroups(int size, gid_t list[]);
int setgroups(size_t size, const gid_t *list);

/* Working directory */
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int fchdir(int fd);
int chroot(const char *path);

/* Hostname */
int gethostname(char *name, size_t len);
int sethostname(const char *name, size_t len);
int getdomainname(char *name, size_t len);
int setdomainname(const char *name, size_t len);

/* Sleeping */
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
unsigned int alarm(unsigned int seconds);
int pause(void);

/* File sync */
int fsync(int fd);
int fdatasync(int fd);
void sync(void);

/* Configuration */
long sysconf(int name);
long pathconf(const char *path, int name);
long fpathconf(int fd, int name);

/* Terminal */
int isatty(int fd);
char *ttyname(int fd);
int ttyname_r(int fd, char *buf, size_t buflen);

/* Misc */
int nice(int inc);
void swab(const void *from, void *to, ssize_t n);
char *crypt(const char *key, const char *salt);
int brk(void *addr);
void *sbrk(intptr_t increment);

/* Optarg for getopt */
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int argc, char *const argv[], const char *optstring);

#endif /* _UNISTD_H */
