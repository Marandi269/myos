/*
 * syscall_fs.c - File system related system calls
 */

#include "syscall.h"
#include "process.h"
#include "../fs/vfs.h"
#include "../fs/fd.h"
#include "../lib/string.h"

/* Get current process's fd table */
static struct fd_table* get_fd_table(void) {
    extern process_t *current_proc;
    extern struct fd_table *global_fd_table;
    if (current_proc && current_proc->fd_table) {
        return current_proc->fd_table;
    }
    return global_fd_table;
}

/* Current umask */
static uint32_t current_umask = 022;

/* File mode bits */
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISUID  04000
#define S_ISGID  02000
#define S_ISVTX  01000

/* Error codes not in vfs.h */
#ifndef ENOSYS
#define ENOSYS 38
#endif
#ifndef ENOTTY
#define ENOTTY 25
#endif

/* Fill stat structure from inode */
static void fill_stat(struct stat *statbuf, struct inode *inode) {
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = 1;  /* Device ID */
    statbuf->st_ino = (uint64_t)inode;  /* Use pointer as inode number */
    statbuf->st_mode = inode->i_mode;
    statbuf->st_nlink = 1;
    statbuf->st_uid = inode->i_uid;
    statbuf->st_gid = inode->i_gid;
    statbuf->st_rdev = 0;
    statbuf->st_size = inode->i_size;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (inode->i_size + 511) / 512;
    statbuf->st_atime = inode->i_atime;
    statbuf->st_mtime = inode->i_mtime;
    statbuf->st_ctime = inode->i_ctime;
}

int64_t sys_stat(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) {
        return -EINVAL;
    }

    struct inode *inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }

    fill_stat(statbuf, inode);
    return 0;
}

int64_t sys_fstat(int fd, struct stat *statbuf) {
    if (!statbuf) {
        return -EINVAL;
    }

    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    if (file->f_inode) {
        fill_stat(statbuf, file->f_inode);
    } else {
        memset(statbuf, 0, sizeof(*statbuf));
        statbuf->st_mode = S_IFCHR;  /* Assume character device */
    }

    return 0;
}

int64_t sys_lstat(const char *pathname, struct stat *statbuf) {
    /* For now, same as stat (no symlink handling) */
    return sys_stat(pathname, statbuf);
}

int64_t sys_access(const char *pathname, int mode) {
    if (!pathname) {
        return -EINVAL;
    }

    struct inode *inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }

    /* F_OK (0) - just check existence */
    if (mode == 0) {
        return 0;
    }

    /* Simplified permission check - allow all for root */
    return 0;
}

int64_t sys_chmod(const char *pathname, uint32_t mode) {
    if (!pathname) {
        return -EINVAL;
    }

    struct inode *inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }

    inode->i_mode = (inode->i_mode & S_IFMT) | (mode & 07777);
    return 0;
}

int64_t sys_fchmod(int fd, uint32_t mode) {
    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    if (file->f_inode) {
        file->f_inode->i_mode = (file->f_inode->i_mode & S_IFMT) | (mode & 07777);
    }

    return 0;
}

int64_t sys_chown(const char *pathname, uint32_t owner, uint32_t group) {
    if (!pathname) {
        return -EINVAL;
    }

    struct inode *inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }

    if (owner != (uint32_t)-1) {
        inode->i_uid = owner;
    }
    if (group != (uint32_t)-1) {
        inode->i_gid = group;
    }

    return 0;
}

int64_t sys_fchown(int fd, uint32_t owner, uint32_t group) {
    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    if (file->f_inode) {
        if (owner != (uint32_t)-1) {
            file->f_inode->i_uid = owner;
        }
        if (group != (uint32_t)-1) {
            file->f_inode->i_gid = group;
        }
    }

    return 0;
}

int64_t sys_link(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    /* Not implemented for ramfs */
    return -ENOSYS;
}

int64_t sys_unlink(const char *pathname) {
    if (!pathname) {
        return -EINVAL;
    }
    return vfs_unlink(pathname);
}

int64_t sys_symlink(const char *target, const char *linkpath) {
    (void)target;
    (void)linkpath;
    /* Not implemented */
    return -ENOSYS;
}

int64_t sys_readlink(const char *pathname, char *buf, size_t bufsiz) {
    (void)pathname;
    (void)buf;
    (void)bufsiz;
    /* Not implemented */
    return -ENOSYS;
}

int64_t sys_rename(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    /* Not implemented yet */
    return -ENOSYS;
}

int64_t sys_rmdir(const char *pathname) {
    if (!pathname) {
        return -EINVAL;
    }
    return vfs_rmdir(pathname);
}

int64_t sys_umask(uint32_t mask) {
    uint32_t old = current_umask;
    current_umask = mask & 0777;
    return old;
}

int64_t sys_ftruncate(int fd, int64_t length) {
    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    if (file->f_inode) {
        file->f_inode->i_size = length;
    }

    return 0;
}

/* fcntl commands */
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_DUPFD_CLOEXEC 1030

int64_t sys_fcntl(int fd, int cmd, uint64_t arg) {
    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    switch (cmd) {
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
            return sys_dup(fd);

        case F_GETFD:
            return 0;  /* No close-on-exec flag implemented */

        case F_SETFD:
            return 0;  /* Ignored */

        case F_GETFL:
            return file->f_flags;

        case F_SETFL:
            file->f_flags = (file->f_flags & ~0x7FF) | (arg & 0x7FF);
            return 0;

        default:
            return -EINVAL;
    }
}

int64_t sys_ioctl_impl(int fd, unsigned long request, void *arg) {
    struct file *file = fd_get(get_fd_table(), fd);
    if (!file) {
        return -EBADF;
    }

    /* Handle some common ioctls */
    /* TIOCGWINSZ = 0x5413 - get window size */
    if (request == 0x5413) {
        struct winsize {
            unsigned short ws_row;
            unsigned short ws_col;
            unsigned short ws_xpixel;
            unsigned short ws_ypixel;
        } *ws = arg;
        if (ws) {
            ws->ws_row = 25;
            ws->ws_col = 80;
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
        }
        return 0;
    }

    /* TCGETS = 0x5401 - get terminal attributes */
    if (request == 0x5401) {
        /* Return success but don't fill - stub */
        return 0;
    }

    return -ENOTTY;
}
