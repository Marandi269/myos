/*
 * vfs.h - Virtual File System interface
 */

#ifndef _VFS_H
#define _VFS_H

#include "types.h"

/* Forward declarations */
struct inode;
struct file;
struct super_block;
struct file_system_type;
struct dentry;

/* File types (mode bits) */
#define S_IFMT   0170000  /* Mask for file type */
#define S_IFREG  0100000  /* Regular file */
#define S_IFDIR  0040000  /* Directory */
#define S_IFCHR  0020000  /* Character device */
#define S_IFBLK  0060000  /* Block device */
#define S_IFIFO  0010000  /* FIFO */
#define S_IFLNK  0120000  /* Symbolic link */
#define S_IFSOCK 0140000  /* Socket */

/* File type test macros */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Permission bits */
#define S_IRWXU  0700  /* Owner RWX */
#define S_IRUSR  0400  /* Owner read */
#define S_IWUSR  0200  /* Owner write */
#define S_IXUSR  0100  /* Owner execute */
#define S_IRWXG  0070  /* Group RWX */
#define S_IRWXO  0007  /* Others RWX */

/* Open flags */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003  /* Mask for access mode */
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_DIRECTORY 0x10000

/* Seek whence */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Maximum path length */
#define PATH_MAX    4096
#define NAME_MAX    255

/* Error codes */
#define ENOENT      2   /* No such file or directory */
#define EIO         5   /* I/O error */
#define EBADF       9   /* Bad file descriptor */
#define ENOMEM      12  /* Out of memory */
#define EEXIST      17  /* File exists */
#define ENOTDIR     20  /* Not a directory */
#define EISDIR      21  /* Is a directory */
#define EINVAL      22  /* Invalid argument */
#define EMFILE      24  /* Too many open files */
#define ENOSPC      28  /* No space left on device */
#define EROFS       30  /* Read-only file system */
#define ENOTEMPTY   39  /* Directory not empty */

/* Directory entry for readdir */
struct dirent {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
};

/* Directory entry types */
#define DT_UNKNOWN  0
#define DT_REG      8
#define DT_DIR      4
#define DT_CHR      2
#define DT_BLK      6
#define DT_FIFO     1
#define DT_LNK      10
#define DT_SOCK     12

/* inode structure */
struct inode {
    uint32_t i_ino;         /* Inode number */
    uint32_t i_mode;        /* File type and permissions */
    uint32_t i_nlink;       /* Number of hard links */
    uint32_t i_uid;         /* Owner UID */
    uint32_t i_gid;         /* Owner GID */
    uint64_t i_size;        /* File size in bytes */
    uint64_t i_atime;       /* Access time */
    uint64_t i_mtime;       /* Modification time */
    uint64_t i_ctime;       /* Status change time */

    struct inode_operations *i_op;   /* Inode operations */
    struct file_operations *i_fop;   /* Default file operations */
    struct super_block *i_sb;        /* Superblock */
    void *i_private;                 /* FS-specific data */
    uint32_t i_count;                /* Reference count */
};

/* inode operations */
struct inode_operations {
    struct inode* (*lookup)(struct inode *dir, const char *name);
    int (*create)(struct inode *dir, const char *name, uint32_t mode);
    int (*mkdir)(struct inode *dir, const char *name, uint32_t mode);
    int (*rmdir)(struct inode *dir, const char *name);
    int (*unlink)(struct inode *dir, const char *name);
    int (*rename)(struct inode *old_dir, const char *old_name,
                  struct inode *new_dir, const char *new_name);
};

/* File operations */
struct file_operations {
    int (*open)(struct inode *inode, struct file *file);
    int (*close)(struct file *file);
    ssize_t (*read)(struct file *file, char *buf, size_t count);
    ssize_t (*write)(struct file *file, const char *buf, size_t count);
    int64_t (*lseek)(struct file *file, int64_t offset, int whence);
    int (*readdir)(struct file *file, struct dirent *dirent);
    int (*ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
};

/* Open file structure */
struct file {
    struct inode *f_inode;           /* Associated inode */
    struct file_operations *f_op;    /* File operations */
    uint64_t f_pos;                  /* Current file position */
    uint32_t f_flags;                /* Open flags */
    uint32_t f_count;                /* Reference count */
    void *f_private;                 /* File-specific data */
};

/* Superblock structure */
struct super_block {
    struct file_system_type *s_type;  /* Filesystem type */
    struct inode *s_root;             /* Root inode */
    void *s_fs_info;                  /* FS-specific info */
    uint64_t s_blocksize;             /* Block size */
    const char *s_id;                 /* Identifier string */
};

/* File system type */
struct file_system_type {
    const char *name;
    struct super_block* (*mount)(struct file_system_type *fs, const char *source);
    void (*umount)(struct super_block *sb);
    struct file_system_type *next;
};

/* Mount point */
struct mount {
    const char *mnt_point;            /* Mount point path */
    struct super_block *mnt_sb;       /* Mounted superblock */
    struct mount *next;
};

/* VFS functions */
void vfs_init(void);

/* Filesystem registration */
int vfs_register_fs(struct file_system_type *fs);
int vfs_unregister_fs(struct file_system_type *fs);

/* Mount operations */
int vfs_mount(const char *source, const char *target, const char *fstype);
int vfs_umount(const char *target);

/* Path resolution */
struct inode* vfs_lookup(const char *path);
struct inode* vfs_lookup_parent(const char *path, char *name);

/* File operations */
struct file* vfs_open(const char *path, int flags, uint32_t mode);
int vfs_close(struct file *file);
ssize_t vfs_read(struct file *file, void *buf, size_t count);
ssize_t vfs_write(struct file *file, const void *buf, size_t count);
int64_t vfs_lseek(struct file *file, int64_t offset, int whence);
int vfs_readdir(struct file *file, struct dirent *dirent);

/* Inode operations */
int vfs_create(const char *path, uint32_t mode);
int vfs_mkdir(const char *path, uint32_t mode);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);

/* Inode helpers */
struct inode* inode_alloc(struct super_block *sb);
void inode_free(struct inode *inode);
void inode_get(struct inode *inode);
void inode_put(struct inode *inode);

/* File helpers */
struct file* file_alloc(void);
void file_free(struct file *file);
void file_get(struct file *file);
void file_put(struct file *file);

#endif /* _VFS_H */
