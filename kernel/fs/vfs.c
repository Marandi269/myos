/*
 * vfs.c - Virtual File System implementation
 */

#include "vfs.h"
#include "../mm/heap.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Registered file systems */
static struct file_system_type *fs_types = NULL;

/* Mount points */
static struct mount *mounts = NULL;

/* Global inode counter */
static uint32_t next_ino = 1;

/*
 * Initialize VFS
 */
void vfs_init(void) {
    fs_types = NULL;
    mounts = NULL;
    next_ino = 1;
    kprintf("[VFS] Initialized\n");
}

/*
 * Register a filesystem type
 */
int vfs_register_fs(struct file_system_type *fs) {
    if (!fs || !fs->name) {
        return -EINVAL;
    }

    /* Add to head of list */
    fs->next = fs_types;
    fs_types = fs;

    kprintf("[VFS] Registered filesystem: %s\n", fs->name);
    return 0;
}

/*
 * Unregister a filesystem type
 */
int vfs_unregister_fs(struct file_system_type *fs) {
    struct file_system_type **pp = &fs_types;

    while (*pp) {
        if (*pp == fs) {
            *pp = fs->next;
            return 0;
        }
        pp = &(*pp)->next;
    }

    return -ENOENT;
}

/*
 * Find filesystem type by name
 */
static struct file_system_type* find_fs_type(const char *name) {
    struct file_system_type *fs = fs_types;

    while (fs) {
        if (strcmp(fs->name, name) == 0) {
            return fs;
        }
        fs = fs->next;
    }

    return NULL;
}

/*
 * Find mount point for path
 */
static struct mount* find_mount(const char *path) {
    struct mount *best = NULL;
    size_t best_len = 0;
    struct mount *m = mounts;

    while (m) {
        size_t len = strlen(m->mnt_point);
        if (strncmp(path, m->mnt_point, len) == 0) {
            /* Check for exact match or path continues with / */
            if (path[len] == '\0' || path[len] == '/' || len == 1) {
                if (len > best_len) {
                    best = m;
                    best_len = len;
                }
            }
        }
        m = m->next;
    }

    return best;
}

/*
 * Mount a filesystem
 */
int vfs_mount(const char *source, const char *target, const char *fstype) {
    struct file_system_type *fs;
    struct super_block *sb;
    struct mount *m;

    /* Find filesystem type */
    fs = find_fs_type(fstype);
    if (!fs) {
        kprintf("[VFS] Unknown filesystem type: %s\n", fstype);
        return -EINVAL;
    }

    /* Mount the filesystem */
    if (!fs->mount) {
        return -EINVAL;
    }

    sb = fs->mount(fs, source);
    if (!sb) {
        kprintf("[VFS] Failed to mount %s\n", fstype);
        return -EIO;
    }

    /* Create mount point entry */
    m = kmalloc(sizeof(struct mount));
    if (!m) {
        if (fs->umount) {
            fs->umount(sb);
        }
        return -ENOMEM;
    }

    m->mnt_point = target;
    m->mnt_sb = sb;
    m->next = mounts;
    mounts = m;

    kprintf("[VFS] Mounted %s on %s\n", fstype, target);
    return 0;
}

/*
 * Unmount a filesystem
 */
int vfs_umount(const char *target) {
    struct mount **pp = &mounts;

    while (*pp) {
        if (strcmp((*pp)->mnt_point, target) == 0) {
            struct mount *m = *pp;
            *pp = m->next;

            if (m->mnt_sb->s_type->umount) {
                m->mnt_sb->s_type->umount(m->mnt_sb);
            }

            kfree(m);
            kprintf("[VFS] Unmounted %s\n", target);
            return 0;
        }
        pp = &(*pp)->next;
    }

    return -ENOENT;
}

/*
 * Skip leading slashes and get next path component
 */
static const char* path_next_component(const char *path, char *component, size_t max_len) {
    size_t i = 0;

    /* Skip leading slashes */
    while (*path == '/') {
        path++;
    }

    /* End of path */
    if (*path == '\0') {
        component[0] = '\0';
        return NULL;
    }

    /* Copy component */
    while (*path && *path != '/' && i < max_len - 1) {
        component[i++] = *path++;
    }
    component[i] = '\0';

    return path;
}

/*
 * Look up a path and return the inode
 */
struct inode* vfs_lookup(const char *path) {
    struct mount *m;
    struct inode *inode;
    char component[NAME_MAX + 1];
    const char *remaining;

    if (!path || path[0] != '/') {
        return NULL;
    }

    /* Find mount point */
    m = find_mount(path);
    if (!m || !m->mnt_sb || !m->mnt_sb->s_root) {
        return NULL;
    }

    /* Skip mount point prefix */
    remaining = path + strlen(m->mnt_point);
    if (*remaining == '/') {
        remaining++;
    }

    /* Start from root inode */
    inode = m->mnt_sb->s_root;
    inode_get(inode);

    /* Handle case where remaining path is empty (root of mount) */
    if (!remaining || !*remaining) {
        return inode;
    }

    /* Traverse path components */
    while (1) {
        remaining = path_next_component(remaining, component, sizeof(component));

        /* No more components */
        if (component[0] == '\0') {
            break;
        }

        if (!S_ISDIR(inode->i_mode)) {
            inode_put(inode);
            return NULL;
        }

        if (!inode->i_op || !inode->i_op->lookup) {
            inode_put(inode);
            return NULL;
        }

        struct inode *next = inode->i_op->lookup(inode, component);
        inode_put(inode);

        if (!next) {
            return NULL;
        }

        inode = next;

        /* Exit if no more path to process */
        if (!remaining || !*remaining) {
            break;
        }
    }

    return inode;
}

/*
 * Look up parent directory and return last component name
 */
struct inode* vfs_lookup_parent(const char *path, char *name) {
    char parent_path[PATH_MAX];
    const char *last_slash;
    size_t parent_len;

    if (!path || path[0] != '/' || !name) {
        return NULL;
    }

    /* Find last slash */
    last_slash = strrchr(path, '/');
    if (!last_slash) {
        return NULL;
    }

    /* Extract parent path */
    parent_len = last_slash - path;
    if (parent_len == 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    } else {
        if (parent_len >= PATH_MAX) {
            return NULL;
        }
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }

    /* Copy name */
    strncpy(name, last_slash + 1, NAME_MAX);
    name[NAME_MAX] = '\0';

    return vfs_lookup(parent_path);
}

/*
 * Open a file
 */
struct file* vfs_open(const char *path, int flags, uint32_t mode) {
    struct inode *inode;
    struct file *file;
    int ret;

    /* Try to look up the file */
    inode = vfs_lookup(path);

    /* Handle O_CREAT */
    if (!inode && (flags & O_CREAT)) {
        ret = vfs_create(path, mode | S_IFREG);
        if (ret < 0) {
            return NULL;
        }
        inode = vfs_lookup(path);
    }

    if (!inode) {
        return NULL;
    }

    /* Check for directory open */
    if (S_ISDIR(inode->i_mode) && !(flags & O_DIRECTORY)) {
        if ((flags & O_ACCMODE) != O_RDONLY) {
            inode_put(inode);
            return NULL;
        }
    }

    /* Allocate file structure */
    file = file_alloc();
    if (!file) {
        inode_put(inode);
        return NULL;
    }

    file->f_inode = inode;
    file->f_op = inode->i_fop;
    file->f_pos = 0;
    file->f_flags = flags;
    file->f_count = 1;
    file->f_private = NULL;

    /* Handle O_TRUNC */
    if ((flags & O_TRUNC) && S_ISREG(inode->i_mode)) {
        inode->i_size = 0;
    }

    /* Handle O_APPEND */
    if (flags & O_APPEND) {
        file->f_pos = inode->i_size;
    }

    /* Call file open operation */
    if (file->f_op && file->f_op->open) {
        ret = file->f_op->open(inode, file);
        if (ret < 0) {
            file_free(file);
            inode_put(inode);
            return NULL;
        }
    }

    return file;
}

/*
 * Close a file - decrements ref count, frees when zero
 */
int vfs_close(struct file *file) {
    int ret = 0;

    if (!file) {
        return -EBADF;
    }

    /* Decrement reference count */
    if (file->f_count > 1) {
        file->f_count--;
        return 0;
    }

    /* Last reference, actually close */
    if (file->f_op && file->f_op->close) {
        ret = file->f_op->close(file);
    }

    if (file->f_inode) {
        inode_put(file->f_inode);
        file->f_inode = NULL;
    }

    file_free(file);
    return ret;
}

/*
 * Read from a file
 */
ssize_t vfs_read(struct file *file, void *buf, size_t count) {
    if (!file || !buf) {
        return -EINVAL;
    }

    if (!file->f_op || !file->f_op->read) {
        return -EIO;
    }

    /* Check read permission */
    if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
        return -EBADF;
    }

    return file->f_op->read(file, buf, count);
}

/*
 * Write to a file
 */
ssize_t vfs_write(struct file *file, const void *buf, size_t count) {
    if (!file || !buf) {
        return -EINVAL;
    }

    if (!file->f_op || !file->f_op->write) {
        return -EIO;
    }

    /* Check write permission */
    if ((file->f_flags & O_ACCMODE) == O_RDONLY) {
        return -EBADF;
    }

    return file->f_op->write(file, buf, count);
}

/*
 * Seek in a file
 */
int64_t vfs_lseek(struct file *file, int64_t offset, int whence) {
    int64_t new_pos;

    if (!file) {
        return -EBADF;
    }

    if (file->f_op && file->f_op->lseek) {
        return file->f_op->lseek(file, offset, whence);
    }

    /* Default lseek implementation */
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->f_pos + offset;
            break;
        case SEEK_END:
            if (!file->f_inode) {
                return -EINVAL;
            }
            new_pos = file->f_inode->i_size + offset;
            break;
        default:
            return -EINVAL;
    }

    if (new_pos < 0) {
        return -EINVAL;
    }

    file->f_pos = new_pos;
    return new_pos;
}

/*
 * Read directory entry
 */
int vfs_readdir(struct file *file, struct dirent *dirent) {
    if (!file || !dirent) {
        return -EINVAL;
    }

    if (!file->f_inode || !S_ISDIR(file->f_inode->i_mode)) {
        return -ENOTDIR;
    }

    if (!file->f_op || !file->f_op->readdir) {
        return -EIO;
    }

    return file->f_op->readdir(file, dirent);
}

/*
 * Create a file
 */
int vfs_create(const char *path, uint32_t mode) {
    struct inode *parent;
    char name[NAME_MAX + 1];
    int ret;

    parent = vfs_lookup_parent(path, name);
    if (!parent) {
        return -ENOENT;
    }

    if (!S_ISDIR(parent->i_mode)) {
        inode_put(parent);
        return -ENOTDIR;
    }

    if (!parent->i_op || !parent->i_op->create) {
        inode_put(parent);
        return -EIO;
    }

    ret = parent->i_op->create(parent, name, mode);
    inode_put(parent);
    return ret;
}

/*
 * Create a directory
 */
int vfs_mkdir(const char *path, uint32_t mode) {
    struct inode *parent;
    char name[NAME_MAX + 1];
    int ret;

    parent = vfs_lookup_parent(path, name);
    if (!parent) {
        return -ENOENT;
    }

    if (!S_ISDIR(parent->i_mode)) {
        inode_put(parent);
        return -ENOTDIR;
    }

    if (!parent->i_op || !parent->i_op->mkdir) {
        inode_put(parent);
        return -EIO;
    }

    ret = parent->i_op->mkdir(parent, name, mode | S_IFDIR);
    inode_put(parent);
    return ret;
}

/*
 * Remove a directory
 */
int vfs_rmdir(const char *path) {
    struct inode *parent;
    char name[NAME_MAX + 1];
    int ret;

    parent = vfs_lookup_parent(path, name);
    if (!parent) {
        return -ENOENT;
    }

    if (!parent->i_op || !parent->i_op->rmdir) {
        inode_put(parent);
        return -EIO;
    }

    ret = parent->i_op->rmdir(parent, name);
    inode_put(parent);
    return ret;
}

/*
 * Unlink (delete) a file
 */
int vfs_unlink(const char *path) {
    struct inode *parent;
    char name[NAME_MAX + 1];
    int ret;

    parent = vfs_lookup_parent(path, name);
    if (!parent) {
        return -ENOENT;
    }

    if (!parent->i_op || !parent->i_op->unlink) {
        inode_put(parent);
        return -EIO;
    }

    ret = parent->i_op->unlink(parent, name);
    inode_put(parent);
    return ret;
}

/*
 * Allocate a new inode
 */
struct inode* inode_alloc(struct super_block *sb) {
    struct inode *inode = kzalloc(sizeof(struct inode));
    if (!inode) {
        return NULL;
    }

    inode->i_ino = next_ino++;
    inode->i_sb = sb;
    inode->i_count = 1;
    inode->i_nlink = 1;

    return inode;
}

/*
 * Free an inode
 */
void inode_free(struct inode *inode) {
    if (inode) {
        kfree(inode);
    }
}

/*
 * Increment inode reference count
 */
void inode_get(struct inode *inode) {
    if (inode) {
        inode->i_count++;
    }
}

/*
 * Decrement inode reference count
 */
void inode_put(struct inode *inode) {
    if (inode) {
        if (inode->i_count > 0) {
            inode->i_count--;
        }
        /* Note: actual freeing is handled by filesystem */
    }
}

/*
 * Allocate a file structure
 */
struct file* file_alloc(void) {
    struct file *file = kzalloc(sizeof(struct file));
    if (file) {
        file->f_count = 1;
    }
    return file;
}

/*
 * Free a file structure
 */
void file_free(struct file *file) {
    if (file) {
        kfree(file);
    }
}

/*
 * Increment file reference count
 */
void file_get(struct file *file) {
    if (file) {
        file->f_count++;
    }
}

/*
 * Decrement file reference count (alias for vfs_close)
 */
void file_put(struct file *file) {
    vfs_close(file);
}
