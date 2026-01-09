/*
 * devfs.c - Device filesystem implementation
 */

#include "devfs.h"
#include "../../mm/heap.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"
#include "../../serial.h"
#include "../poll.h"
#include "../../proc/wait_queue.h"

/* Device entry in devfs */
struct dev_entry {
    char name[NAME_MAX + 1];
    struct file_operations *ops;
    void *private;
    struct inode *inode;
    struct dev_entry *next;
};

/* devfs superblock private data */
struct devfs_info {
    struct dev_entry *devices;
    struct inode *root;
};

/* Forward declarations */
static struct inode* devfs_lookup(struct inode *dir, const char *name);
static int devfs_readdir(struct file *file, struct dirent *dirent);
static struct super_block* devfs_mount(struct file_system_type *fs, const char *source);
static void devfs_umount(struct super_block *sb);

/* /dev/null operations */
static ssize_t null_read(struct file *file, char *buf, size_t count);
static ssize_t null_write(struct file *file, const char *buf, size_t count);
static unsigned int null_poll(struct file *file, struct poll_table *pt);

/* /dev/zero operations */
static ssize_t zero_read(struct file *file, char *buf, size_t count);
static ssize_t zero_write(struct file *file, const char *buf, size_t count);
static unsigned int zero_poll(struct file *file, struct poll_table *pt);

/* /dev/console operations */
static ssize_t console_read(struct file *file, char *buf, size_t count);
static ssize_t console_write(struct file *file, const char *buf, size_t count);
static unsigned int console_poll(struct file *file, struct poll_table *pt);

/* Console wait queue for poll support */
static wait_queue_head_t console_read_wq = WAIT_QUEUE_HEAD_INIT;

/* Inode operations for devfs root */
static struct inode_operations devfs_root_inode_ops = {
    .lookup = devfs_lookup,
    .create = NULL,
    .mkdir = NULL,
    .rmdir = NULL,
    .unlink = NULL,
    .rename = NULL,
};

/* Directory operations for devfs root */
static struct file_operations devfs_root_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .lseek = NULL,
    .readdir = devfs_readdir,
    .ioctl = NULL,
};

/* /dev/null operations */
static struct file_operations null_ops = {
    .open = NULL,
    .close = NULL,
    .read = null_read,
    .write = null_write,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL,
    .poll = null_poll,
};

/* /dev/zero operations */
static struct file_operations zero_ops = {
    .open = NULL,
    .close = NULL,
    .read = zero_read,
    .write = zero_write,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL,
    .poll = zero_poll,
};

/* /dev/console operations */
static struct file_operations console_ops = {
    .open = NULL,
    .close = NULL,
    .read = console_read,
    .write = console_write,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL,
    .poll = console_poll,
};

/* Filesystem type */
static struct file_system_type devfs_type = {
    .name = "devfs",
    .mount = devfs_mount,
    .umount = devfs_umount,
    .next = NULL,
};

/* Global devfs info */
static struct devfs_info *devfs_info = NULL;

/*
 * /dev/null - read returns EOF
 */
static ssize_t null_read(struct file *file, char *buf, size_t count) {
    (void)file;
    (void)buf;
    (void)count;
    return 0;  /* EOF */
}

/*
 * /dev/null - write discards data
 */
static ssize_t null_write(struct file *file, const char *buf, size_t count) {
    (void)file;
    (void)buf;
    return count;  /* Pretend we wrote it all */
}

/*
 * /dev/zero - read returns zeros
 */
static ssize_t zero_read(struct file *file, char *buf, size_t count) {
    (void)file;
    memset(buf, 0, count);
    return count;
}

/*
 * /dev/zero - write discards data
 */
static ssize_t zero_write(struct file *file, const char *buf, size_t count) {
    (void)file;
    (void)buf;
    return count;
}

/*
 * /dev/null - poll always ready for read (EOF) and write
 */
static unsigned int null_poll(struct file *file, struct poll_table *pt) {
    (void)file;
    (void)pt;
    return POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
}

/*
 * /dev/zero - poll always ready for read and write
 */
static unsigned int zero_poll(struct file *file, struct poll_table *pt) {
    (void)file;
    (void)pt;
    return POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
}

/*
 * /dev/console - read from serial (blocking read not implemented yet)
 */
static ssize_t console_read(struct file *file, char *buf, size_t count) {
    (void)file;
    (void)buf;
    (void)count;
    /* TODO: Implement keyboard input buffer */
    return 0;
}

/*
 * /dev/console - write to serial
 */
static ssize_t console_write(struct file *file, const char *buf, size_t count) {
    size_t i;
    (void)file;

    for (i = 0; i < count; i++) {
        if (buf[i] == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(buf[i]);
    }

    return count;
}

/*
 * /dev/console - poll for events
 * Console is always writable, but reading depends on keyboard input
 */
static unsigned int console_poll(struct file *file, struct poll_table *pt) {
    unsigned int mask = POLLOUT | POLLWRNORM;  /* Always writable */

    (void)file;

    /* Register with console read wait queue */
    if (pt) {
        poll_wait(file, &console_read_wq, (poll_table_t *)pt);
    }

    /* TODO: Check if keyboard input is available
     * For now, just return writable only since we don't have keyboard buffer */

    return mask;
}

/*
 * Wake up processes waiting to read from console
 * Call this when keyboard input is available
 */
void console_input_available(void) {
    wake_up_all(&console_read_wq);
}

/*
 * Look up a device by name
 */
static struct inode* devfs_lookup(struct inode *dir, const char *name) {
    struct dev_entry *dev;

    if (!dir || !name || !devfs_info) {
        return NULL;
    }

    dev = devfs_info->devices;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            inode_get(dev->inode);
            return dev->inode;
        }
        dev = dev->next;
    }

    return NULL;
}

/*
 * Read directory entries
 */
static int devfs_readdir(struct file *file, struct dirent *dirent) {
    struct dev_entry *dev;
    uint64_t pos;
    uint64_t current = 0;

    if (!file || !dirent || !devfs_info) {
        return -EINVAL;
    }

    pos = file->f_pos;

    dev = devfs_info->devices;
    while (dev) {
        if (current == pos) {
            dirent->d_ino = dev->inode->i_ino;
            dirent->d_off = current + 1;
            dirent->d_reclen = sizeof(struct dirent);
            dirent->d_type = DT_CHR;
            strncpy(dirent->d_name, dev->name, 255);
            dirent->d_name[255] = '\0';

            file->f_pos = current + 1;
            return 1;
        }
        current++;
        dev = dev->next;
    }

    return 0;  /* No more entries */
}

/*
 * Create a device inode
 */
static struct inode* devfs_create_device_inode(struct super_block *sb,
                                                struct file_operations *ops) {
    struct inode *inode = inode_alloc(sb);
    if (!inode) {
        return NULL;
    }

    inode->i_mode = S_IFCHR | 0666;
    inode->i_op = NULL;
    inode->i_fop = ops;

    return inode;
}

/*
 * Register a device
 */
int devfs_register(const char *name, struct file_operations *ops, void *private) {
    struct dev_entry *dev;
    struct super_block *sb;

    if (!name || !ops || !devfs_info) {
        return -EINVAL;
    }

    /* Check if already exists */
    if (devfs_lookup(devfs_info->root, name) != NULL) {
        return -EEXIST;
    }

    dev = kzalloc(sizeof(struct dev_entry));
    if (!dev) {
        return -ENOMEM;
    }

    strncpy(dev->name, name, NAME_MAX);
    dev->name[NAME_MAX] = '\0';
    dev->ops = ops;
    dev->private = private;

    /* Get superblock from root inode */
    sb = devfs_info->root->i_sb;

    dev->inode = devfs_create_device_inode(sb, ops);
    if (!dev->inode) {
        kfree(dev);
        return -ENOMEM;
    }

    /* Add to device list */
    dev->next = devfs_info->devices;
    devfs_info->devices = dev;

    return 0;
}

/*
 * Unregister a device
 */
int devfs_unregister(const char *name) {
    struct dev_entry **pp;

    if (!name || !devfs_info) {
        return -EINVAL;
    }

    pp = &devfs_info->devices;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            struct dev_entry *dev = *pp;
            *pp = dev->next;
            inode_free(dev->inode);
            kfree(dev);
            return 0;
        }
        pp = &(*pp)->next;
    }

    return -ENOENT;
}

/*
 * Mount devfs
 */
static struct super_block* devfs_mount(struct file_system_type *fs,
                                        const char *source) {
    struct super_block *sb;
    struct inode *root;

    (void)source;

    sb = kzalloc(sizeof(struct super_block));
    if (!sb) {
        return NULL;
    }

    sb->s_type = fs;
    sb->s_blocksize = 4096;
    sb->s_id = "devfs";

    /* Create root directory inode */
    root = inode_alloc(sb);
    if (!root) {
        kfree(sb);
        return NULL;
    }

    root->i_mode = S_IFDIR | 0755;
    root->i_op = &devfs_root_inode_ops;
    root->i_fop = &devfs_root_file_ops;

    sb->s_root = root;

    /* Allocate devfs info */
    devfs_info = kzalloc(sizeof(struct devfs_info));
    if (!devfs_info) {
        inode_free(root);
        kfree(sb);
        return NULL;
    }

    devfs_info->root = root;
    devfs_info->devices = NULL;
    sb->s_fs_info = devfs_info;

    /* Register built-in devices */
    devfs_register("null", &null_ops, NULL);
    devfs_register("zero", &zero_ops, NULL);
    devfs_register("console", &console_ops, NULL);

    return sb;
}

/*
 * Unmount devfs
 */
static void devfs_umount(struct super_block *sb) {
    struct dev_entry *dev, *next;

    if (!sb || !sb->s_fs_info) {
        return;
    }

    /* Free all devices */
    dev = devfs_info->devices;
    while (dev) {
        next = dev->next;
        inode_free(dev->inode);
        kfree(dev);
        dev = next;
    }

    kfree(devfs_info);
    devfs_info = NULL;

    if (sb->s_root) {
        inode_free(sb->s_root);
    }

    kfree(sb);
}

/*
 * Initialize devfs
 */
void devfs_init(void) {
    vfs_register_fs(&devfs_type);
}

/*
 * Get devfs filesystem type
 */
struct file_system_type* devfs_get_type(void) {
    return &devfs_type;
}
