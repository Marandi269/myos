/*
 * ramfs.c - RAM filesystem implementation
 */

#include "ramfs.h"
#include "../../mm/heap.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"

/* Initial data buffer size */
#define RAMFS_INITIAL_SIZE 1024

/* ramfs inode private data */
struct ramfs_inode {
    char *data;                      /* File data (for regular files) */
    size_t capacity;                 /* Allocated size */
    struct ramfs_inode *children;    /* First child (for directories) */
    struct ramfs_inode *sibling;     /* Next sibling */
    struct ramfs_inode *parent;      /* Parent directory */
    struct inode *vfs_inode;         /* Back pointer to VFS inode */
    char name[NAME_MAX + 1];         /* Entry name */
};

/* Forward declarations */
static struct inode* ramfs_lookup(struct inode *dir, const char *name);
static int ramfs_create(struct inode *dir, const char *name, uint32_t mode);
static int ramfs_mkdir(struct inode *dir, const char *name, uint32_t mode);
static int ramfs_rmdir(struct inode *dir, const char *name);
static int ramfs_unlink(struct inode *dir, const char *name);
static ssize_t ramfs_read(struct file *file, char *buf, size_t count);
static ssize_t ramfs_write(struct file *file, const char *buf, size_t count);
static int ramfs_readdir(struct file *file, struct dirent *dirent);
static struct super_block* ramfs_mount(struct file_system_type *fs, const char *source);
static void ramfs_umount(struct super_block *sb);

/* Inode operations */
static struct inode_operations ramfs_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
    .rmdir = ramfs_rmdir,
    .unlink = ramfs_unlink,
    .rename = NULL,
};

/* File operations */
static struct file_operations ramfs_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = ramfs_read,
    .write = ramfs_write,
    .lseek = NULL,  /* Use default lseek */
    .readdir = NULL,
    .ioctl = NULL,
};

/* Directory operations */
static struct file_operations ramfs_dir_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .lseek = NULL,
    .readdir = ramfs_readdir,
    .ioctl = NULL,
};

/* Filesystem type */
static struct file_system_type ramfs_type = {
    .name = "ramfs",
    .mount = ramfs_mount,
    .umount = ramfs_umount,
    .next = NULL,
};

/*
 * Create a ramfs inode with private data
 */
static struct ramfs_inode* ramfs_create_inode(struct super_block *sb,
                                               const char *name,
                                               uint32_t mode) {
    struct ramfs_inode *ri;
    struct inode *inode;

    ri = kzalloc(sizeof(struct ramfs_inode));
    if (!ri) {
        return NULL;
    }

    inode = inode_alloc(sb);
    if (!inode) {
        kfree(ri);
        return NULL;
    }

    /* Set up name */
    strncpy(ri->name, name, NAME_MAX);
    ri->name[NAME_MAX] = '\0';

    /* Link ramfs inode to VFS inode */
    ri->vfs_inode = inode;
    inode->i_private = ri;
    inode->i_mode = mode;
    inode->i_op = &ramfs_inode_ops;

    if (S_ISDIR(mode)) {
        inode->i_fop = &ramfs_dir_ops;
    } else {
        inode->i_fop = &ramfs_file_ops;
    }

    return ri;
}

/*
 * Free a ramfs inode
 */
static void ramfs_free_inode(struct ramfs_inode *ri) {
    if (!ri) {
        return;
    }

    /* Free file data */
    if (ri->data) {
        kfree(ri->data);
    }

    /* Free VFS inode */
    if (ri->vfs_inode) {
        inode_free(ri->vfs_inode);
    }

    kfree(ri);
}

/*
 * Look up a name in a directory
 */
static struct inode* ramfs_lookup(struct inode *dir, const char *name) {
    struct ramfs_inode *dir_ri;
    struct ramfs_inode *child;

    if (!dir || !name || !S_ISDIR(dir->i_mode)) {
        return NULL;
    }

    dir_ri = (struct ramfs_inode *)dir->i_private;
    if (!dir_ri) {
        return NULL;
    }

    /* Search children */
    child = dir_ri->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            inode_get(child->vfs_inode);
            return child->vfs_inode;
        }
        child = child->sibling;
    }

    return NULL;
}

/*
 * Create a file in a directory
 */
static int ramfs_create(struct inode *dir, const char *name, uint32_t mode) {
    struct ramfs_inode *dir_ri;
    struct ramfs_inode *new_ri;

    if (!dir || !name || !S_ISDIR(dir->i_mode)) {
        return -EINVAL;
    }

    dir_ri = (struct ramfs_inode *)dir->i_private;
    if (!dir_ri) {
        return -EINVAL;
    }

    /* Check if name already exists */
    if (ramfs_lookup(dir, name) != NULL) {
        return -EEXIST;
    }

    /* Create new inode */
    new_ri = ramfs_create_inode(dir->i_sb, name, mode);
    if (!new_ri) {
        return -ENOMEM;
    }

    /* Add to directory */
    new_ri->parent = dir_ri;
    new_ri->sibling = dir_ri->children;
    dir_ri->children = new_ri;

    return 0;
}

/*
 * Create a directory
 */
static int ramfs_mkdir(struct inode *dir, const char *name, uint32_t mode) {
    return ramfs_create(dir, name, mode | S_IFDIR);
}

/*
 * Remove a child from directory's children list
 */
static int ramfs_unlink_child(struct ramfs_inode *dir_ri,
                               struct ramfs_inode *child_ri) {
    struct ramfs_inode **pp = &dir_ri->children;

    while (*pp) {
        if (*pp == child_ri) {
            *pp = child_ri->sibling;
            child_ri->sibling = NULL;
            child_ri->parent = NULL;
            return 0;
        }
        pp = &(*pp)->sibling;
    }

    return -ENOENT;
}

/*
 * Remove a directory
 */
static int ramfs_rmdir(struct inode *dir, const char *name) {
    struct ramfs_inode *dir_ri;
    struct ramfs_inode *child_ri;
    struct inode *child;

    if (!dir || !name) {
        return -EINVAL;
    }

    dir_ri = (struct ramfs_inode *)dir->i_private;
    if (!dir_ri) {
        return -EINVAL;
    }

    /* Find the child */
    child = ramfs_lookup(dir, name);
    if (!child) {
        return -ENOENT;
    }

    /* Must be a directory */
    if (!S_ISDIR(child->i_mode)) {
        inode_put(child);
        return -ENOTDIR;
    }

    child_ri = (struct ramfs_inode *)child->i_private;

    /* Directory must be empty */
    if (child_ri->children) {
        inode_put(child);
        return -ENOTEMPTY;
    }

    /* Remove from parent */
    ramfs_unlink_child(dir_ri, child_ri);

    /* Free the inode */
    inode_put(child);
    ramfs_free_inode(child_ri);

    return 0;
}

/*
 * Unlink (delete) a file
 */
static int ramfs_unlink(struct inode *dir, const char *name) {
    struct ramfs_inode *dir_ri;
    struct ramfs_inode *child_ri;
    struct inode *child;

    if (!dir || !name) {
        return -EINVAL;
    }

    dir_ri = (struct ramfs_inode *)dir->i_private;
    if (!dir_ri) {
        return -EINVAL;
    }

    /* Find the child */
    child = ramfs_lookup(dir, name);
    if (!child) {
        return -ENOENT;
    }

    /* Must not be a directory */
    if (S_ISDIR(child->i_mode)) {
        inode_put(child);
        return -EISDIR;
    }

    child_ri = (struct ramfs_inode *)child->i_private;

    /* Remove from parent */
    ramfs_unlink_child(dir_ri, child_ri);

    /* Free the inode */
    inode_put(child);
    ramfs_free_inode(child_ri);

    return 0;
}

/*
 * Read from a file
 */
static ssize_t ramfs_read(struct file *file, char *buf, size_t count) {
    struct ramfs_inode *ri;
    size_t available;
    size_t to_read;

    if (!file || !buf || !file->f_inode) {
        return -EINVAL;
    }

    ri = (struct ramfs_inode *)file->f_inode->i_private;
    if (!ri) {
        return -EINVAL;
    }

    /* Calculate how much we can read */
    if (file->f_pos >= file->f_inode->i_size) {
        return 0;  /* EOF */
    }

    available = file->f_inode->i_size - file->f_pos;
    to_read = (count < available) ? count : available;

    /* Copy data */
    if (ri->data) {
        memcpy(buf, ri->data + file->f_pos, to_read);
    }

    file->f_pos += to_read;
    return to_read;
}

/*
 * Write to a file
 */
static ssize_t ramfs_write(struct file *file, const char *buf, size_t count) {
    struct ramfs_inode *ri;
    size_t new_size;
    size_t new_capacity;
    char *new_data;

    if (!file || !buf || !file->f_inode) {
        return -EINVAL;
    }

    ri = (struct ramfs_inode *)file->f_inode->i_private;
    if (!ri) {
        return -EINVAL;
    }

    /* Calculate new size */
    new_size = file->f_pos + count;

    /* Grow buffer if needed */
    if (new_size > ri->capacity) {
        new_capacity = ri->capacity;
        if (new_capacity == 0) {
            new_capacity = RAMFS_INITIAL_SIZE;
        }
        while (new_capacity < new_size) {
            new_capacity *= 2;
        }

        new_data = kmalloc(new_capacity);
        if (!new_data) {
            return -ENOMEM;
        }

        /* Copy old data */
        if (ri->data && ri->capacity > 0) {
            memcpy(new_data, ri->data, ri->capacity);
            kfree(ri->data);
        }

        /* Zero new area */
        memset(new_data + ri->capacity, 0, new_capacity - ri->capacity);

        ri->data = new_data;
        ri->capacity = new_capacity;
    }

    /* Write data */
    memcpy(ri->data + file->f_pos, buf, count);
    file->f_pos += count;

    /* Update size */
    if (file->f_pos > file->f_inode->i_size) {
        file->f_inode->i_size = file->f_pos;
    }

    return count;
}

/*
 * Read directory entries
 */
static int ramfs_readdir(struct file *file, struct dirent *dirent) {
    struct ramfs_inode *dir_ri;
    struct ramfs_inode *child;
    uint64_t pos;
    uint64_t current = 0;

    if (!file || !dirent || !file->f_inode) {
        return -EINVAL;
    }

    dir_ri = (struct ramfs_inode *)file->f_inode->i_private;
    if (!dir_ri) {
        return -EINVAL;
    }

    pos = file->f_pos;

    /* Iterate through children */
    child = dir_ri->children;
    while (child) {
        if (current == pos) {
            /* Found entry at current position */
            dirent->d_ino = child->vfs_inode->i_ino;
            dirent->d_off = current + 1;
            dirent->d_reclen = sizeof(struct dirent);

            if (S_ISDIR(child->vfs_inode->i_mode)) {
                dirent->d_type = DT_DIR;
            } else if (S_ISCHR(child->vfs_inode->i_mode)) {
                dirent->d_type = DT_CHR;
            } else {
                dirent->d_type = DT_REG;
            }

            strncpy(dirent->d_name, child->name, 255);
            dirent->d_name[255] = '\0';

            file->f_pos = current + 1;
            return 1;  /* One entry read */
        }
        current++;
        child = child->sibling;
    }

    return 0;  /* No more entries */
}

/*
 * Mount ramfs
 */
static struct super_block* ramfs_mount(struct file_system_type *fs,
                                        const char *source) {
    struct super_block *sb;
    struct ramfs_inode *root_ri;

    (void)source;  /* Unused */

    sb = kzalloc(sizeof(struct super_block));
    if (!sb) {
        return NULL;
    }

    sb->s_type = fs;
    sb->s_blocksize = 4096;
    sb->s_id = "ramfs";

    /* Create root directory */
    root_ri = ramfs_create_inode(sb, "", S_IFDIR | 0755);
    if (!root_ri) {
        kfree(sb);
        return NULL;
    }

    sb->s_root = root_ri->vfs_inode;
    sb->s_fs_info = root_ri;

    return sb;
}

/*
 * Unmount ramfs
 */
static void ramfs_umount(struct super_block *sb) {
    /* TODO: Free all inodes recursively */
    if (sb) {
        kfree(sb);
    }
}

/*
 * Initialize ramfs
 */
void ramfs_init(void) {
    vfs_register_fs(&ramfs_type);
}

/*
 * Get ramfs filesystem type
 */
struct file_system_type* ramfs_get_type(void) {
    return &ramfs_type;
}
