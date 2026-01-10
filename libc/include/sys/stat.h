/*
 * sys/stat.h - File status definitions
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <stddef.h>
#include <time.h>

/* Mode type */
typedef unsigned int mode_t;

/* Device type */
typedef unsigned long dev_t;

/* Inode number type */
typedef unsigned long ino_t;

/* Link count type */
typedef unsigned long nlink_t;

/* User ID type */
typedef unsigned int uid_t;

/* Group ID type */
typedef unsigned int gid_t;

/* Offset type */
typedef long off_t;

/* Block size type */
typedef long blksize_t;

/* Block count type */
typedef long blkcnt_t;

/* File mode bits */
#define S_IFMT   0170000    /* Type of file mask */
#define S_IFSOCK 0140000    /* Socket */
#define S_IFLNK  0120000    /* Symbolic link */
#define S_IFREG  0100000    /* Regular file */
#define S_IFBLK  0060000    /* Block device */
#define S_IFDIR  0040000    /* Directory */
#define S_IFCHR  0020000    /* Character device */
#define S_IFIFO  0010000    /* FIFO */

/* File type test macros */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Permission bits */
#define S_ISUID  04000      /* Set UID bit */
#define S_ISGID  02000      /* Set GID bit */
#define S_ISVTX  01000      /* Sticky bit */

#define S_IRWXU  00700      /* Owner: RWX */
#define S_IRUSR  00400      /* Owner: R */
#define S_IWUSR  00200      /* Owner: W */
#define S_IXUSR  00100      /* Owner: X */

#define S_IRWXG  00070      /* Group: RWX */
#define S_IRGRP  00040      /* Group: R */
#define S_IWGRP  00020      /* Group: W */
#define S_IXGRP  00010      /* Group: X */

#define S_IRWXO  00007      /* Other: RWX */
#define S_IROTH  00004      /* Other: R */
#define S_IWOTH  00002      /* Other: W */
#define S_IXOTH  00001      /* Other: X */

/* Access macros */
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#define ALLPERMS    (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

/* stat structure */
struct stat {
    dev_t     st_dev;       /* Device ID */
    ino_t     st_ino;       /* Inode number */
    mode_t    st_mode;      /* File mode */
    nlink_t   st_nlink;     /* Number of hard links */
    uid_t     st_uid;       /* User ID of owner */
    gid_t     st_gid;       /* Group ID of owner */
    dev_t     st_rdev;      /* Device ID (if special file) */
    off_t     st_size;      /* Total size, in bytes */
    blksize_t st_blksize;   /* Block size for filesystem I/O */
    blkcnt_t  st_blocks;    /* Number of 512B blocks allocated */
    time_t    st_atime;     /* Time of last access */
    time_t    st_mtime;     /* Time of last modification */
    time_t    st_ctime;     /* Time of last status change */
};

/* Function declarations */
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *path, struct stat *buf);
int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int mkdir(const char *path, mode_t mode);
int mkfifo(const char *path, mode_t mode);
int mknod(const char *path, mode_t mode, dev_t dev);
mode_t umask(mode_t mask);

#endif /* _SYS_STAT_H */
