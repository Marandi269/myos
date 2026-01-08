/*
 * pivot_root.c - Root filesystem switch implementation
 *
 * Allows switching from initial ramfs to persistent ext2 filesystem on disk.
 */

#include "pivot_root.h"
#include "vfs.h"
#include "ext2/ext2.h"
#include "../drivers/ide.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"
#include "../mm/heap.h"

/* Flag indicating we're running from disk root */
static int g_disk_root = 0;

/* Mounted ext2 filesystem info */
static struct {
    int mounted;
    uint32_t part_start;
    block_device_t *dev;
} g_ext2_mount = {0};

/*
 * Parse MBR partition table and find first partition
 */
static int find_first_partition(block_device_t *dev, uint32_t *start_lba) {
    uint8_t mbr[512];

    /* Read MBR */
    if (dev->read(dev, 0, mbr, 1) < 0) {
        kprintf("[pivot_root] Failed to read MBR\n");
        return -1;
    }

    /* Check MBR signature */
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        kprintf("[pivot_root] Invalid MBR signature\n");
        return -2;
    }

    /* Parse first partition entry at offset 0x1BE */
    uint8_t *part = &mbr[0x1BE];

    /* Check if partition is valid (type != 0) */
    uint8_t part_type = part[4];
    if (part_type == 0) {
        kprintf("[pivot_root] No partition found\n");
        return -3;
    }

    /* Get LBA start */
    *start_lba = part[8] | (part[9] << 8) | (part[10] << 16) | (part[11] << 24);

    kprintf("[pivot_root] Found partition type 0x%02x at LBA %d\n", part_type, *start_lba);
    return 0;
}

/*
 * Mount ext2 from disk
 */
static int mount_ext2_disk(void) {
    if (g_ext2_mount.mounted) {
        return 0;  /* Already mounted */
    }

    /* Check for IDE disk */
    if (ide_device_count() == 0) {
        kprintf("[pivot_root] No IDE disk found\n");
        return -1;
    }

    /* Get first disk */
    block_device_t *disk = ide_get_device(0);
    if (!disk) {
        kprintf("[pivot_root] Failed to get IDE device\n");
        return -2;
    }

    /* Find partition */
    uint32_t part_start;
    if (find_first_partition(disk, &part_start) < 0) {
        return -3;
    }

    /* Mount ext2 */
    if (ext2_mount(disk, part_start) < 0) {
        kprintf("[pivot_root] Failed to mount ext2\n");
        return -4;
    }

    g_ext2_mount.mounted = 1;
    g_ext2_mount.part_start = part_start;
    g_ext2_mount.dev = disk;

    kprintf("[pivot_root] ext2 mounted from disk\n");
    return 0;
}

/*
 * ext2 VFS wrapper structures
 */

/* Forward declarations */
static struct inode* ext2_vfs_lookup(struct inode *dir, const char *name);
static int ext2_vfs_create(struct inode *dir, const char *name, uint32_t mode);
static int ext2_vfs_mkdir(struct inode *dir, const char *name, uint32_t mode);
static int ext2_vfs_unlink(struct inode *dir, const char *name);

static ssize_t ext2_vfs_file_read(struct file *file, char *buf, size_t count);
static ssize_t ext2_vfs_file_write(struct file *file, const char *buf, size_t count);
static int ext2_vfs_file_readdir(struct file *file, struct dirent *dirent);

/* Inode operations for ext2 */
static struct inode_operations ext2_inode_ops = {
    .lookup = ext2_vfs_lookup,
    .create = ext2_vfs_create,
    .mkdir = ext2_vfs_mkdir,
    .unlink = ext2_vfs_unlink,
    .rmdir = ext2_vfs_unlink,  /* Same as unlink for now */
    .rename = NULL
};

/* File operations for ext2 regular files */
static struct file_operations ext2_file_ops = {
    .open = NULL,
    .close = NULL,
    .read = ext2_vfs_file_read,
    .write = ext2_vfs_file_write,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL
};

/* File operations for ext2 directories */
static struct file_operations ext2_dir_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .lseek = NULL,
    .readdir = ext2_vfs_file_readdir,
    .ioctl = NULL
};

/* Superblock for ext2 */
static struct super_block ext2_sb;

/*
 * Create VFS inode from ext2 inode number
 */
static struct inode* ext2_inode_to_vfs(uint32_t ino) {
    ext2_inode_t ext2_ino;

    if (ext2_read_inode(ino, &ext2_ino) < 0)
        return NULL;

    struct inode *inode = inode_alloc(&ext2_sb);
    if (!inode)
        return NULL;

    inode->i_ino = ino;
    inode->i_mode = ext2_ino.i_mode;
    inode->i_size = ext2_ino.i_size;
    inode->i_nlink = ext2_ino.i_links_count;
    inode->i_uid = ext2_ino.i_uid;
    inode->i_gid = ext2_ino.i_gid;

    inode->i_op = &ext2_inode_ops;
    inode->i_private = (void *)(uintptr_t)ino;  /* Store ext2 inode number */

    /* Set file operations based on type */
    if (S_ISDIR(ext2_ino.i_mode)) {
        inode->i_fop = &ext2_dir_ops;
    } else {
        inode->i_fop = &ext2_file_ops;
    }

    return inode;
}

/*
 * VFS inode operations
 */
static struct inode* ext2_vfs_lookup(struct inode *dir, const char *name) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)dir->i_private;
    uint32_t found_ino;

    if (ext2_lookup(dir_ino, name, &found_ino) < 0)
        return NULL;

    return ext2_inode_to_vfs(found_ino);
}

static int ext2_vfs_create(struct inode *dir, const char *name, uint32_t mode) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)dir->i_private;
    uint32_t new_ino;

    return ext2_create(dir_ino, name, mode | EXT2_S_IFREG, &new_ino);
}

static int ext2_vfs_mkdir(struct inode *dir, const char *name, uint32_t mode) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)dir->i_private;
    uint32_t new_ino;

    return ext2_mkdir(dir_ino, name, mode, &new_ino);
}

static int ext2_vfs_unlink(struct inode *dir, const char *name) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)dir->i_private;

    return ext2_unlink(dir_ino, name);
}

/*
 * VFS file operations
 */
static ssize_t ext2_vfs_file_read(struct file *file, char *buf, size_t count) {
    uint32_t ino = (uint32_t)(uintptr_t)file->f_inode->i_private;

    ssize_t ret = ext2_read_file(ino, buf, file->f_pos, count);
    if (ret > 0)
        file->f_pos += ret;

    return ret;
}

static ssize_t ext2_vfs_file_write(struct file *file, const char *buf, size_t count) {
    uint32_t ino = (uint32_t)(uintptr_t)file->f_inode->i_private;

    ssize_t ret = ext2_write_file(ino, buf, file->f_pos, count);
    if (ret > 0) {
        file->f_pos += ret;
        /* Update inode size */
        ext2_inode_t ext2_ino;
        if (ext2_read_inode(ino, &ext2_ino) == 0) {
            file->f_inode->i_size = ext2_ino.i_size;
        }
    }

    return ret;
}

/* Readdir context */
static struct {
    struct dirent *dirent;
    int index;
    int current;
    int found;
} g_readdir_ctx;

static void readdir_callback(const char *name, uint32_t ino, uint8_t type) {
    if (g_readdir_ctx.current == g_readdir_ctx.index) {
        g_readdir_ctx.dirent->d_ino = ino;
        g_readdir_ctx.dirent->d_off = g_readdir_ctx.index;
        g_readdir_ctx.dirent->d_reclen = sizeof(struct dirent);

        /* Convert ext2 type to VFS type */
        switch (type) {
            case EXT2_FT_REG_FILE: g_readdir_ctx.dirent->d_type = DT_REG; break;
            case EXT2_FT_DIR:      g_readdir_ctx.dirent->d_type = DT_DIR; break;
            case EXT2_FT_SYMLINK:  g_readdir_ctx.dirent->d_type = DT_LNK; break;
            case EXT2_FT_CHRDEV:   g_readdir_ctx.dirent->d_type = DT_CHR; break;
            case EXT2_FT_BLKDEV:   g_readdir_ctx.dirent->d_type = DT_BLK; break;
            default:               g_readdir_ctx.dirent->d_type = DT_UNKNOWN; break;
        }

        /* Copy name */
        size_t name_len = strlen(name);
        if (name_len > 255) name_len = 255;
        memcpy(g_readdir_ctx.dirent->d_name, name, name_len);
        g_readdir_ctx.dirent->d_name[name_len] = '\0';

        g_readdir_ctx.found = 1;
    }
    g_readdir_ctx.current++;
}

static int ext2_vfs_file_readdir(struct file *file, struct dirent *dirent) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)file->f_inode->i_private;

    /* Set up context */
    g_readdir_ctx.dirent = dirent;
    g_readdir_ctx.index = (int)file->f_pos;
    g_readdir_ctx.current = 0;
    g_readdir_ctx.found = 0;

    /* Read directory */
    ext2_readdir(dir_ino, readdir_callback);

    if (g_readdir_ctx.found) {
        file->f_pos++;
        return 0;
    }

    return -1;  /* No more entries */
}

/*
 * ext2 VFS mount function
 */
static struct super_block* ext2_vfs_mount(struct file_system_type *fs, const char *source) {
    (void)fs;
    (void)source;

    /* Mount ext2 from disk if not already mounted */
    if (mount_ext2_disk() < 0)
        return NULL;

    /* Create superblock */
    ext2_sb.s_type = fs;
    ext2_sb.s_blocksize = ext2_block_size();
    ext2_sb.s_id = "ext2";
    ext2_sb.s_fs_info = NULL;

    /* Create root inode */
    ext2_sb.s_root = ext2_inode_to_vfs(EXT2_ROOT_INO);
    if (!ext2_sb.s_root) {
        kprintf("[pivot_root] Failed to create root inode\n");
        return NULL;
    }

    return &ext2_sb;
}

static void ext2_vfs_umount(struct super_block *sb) {
    (void)sb;

    /* Sync and unmount ext2 */
    ext2_sync();
    ext2_unmount();
    g_ext2_mount.mounted = 0;
}

/* ext2 filesystem type for VFS */
static struct file_system_type ext2_fs_vfs = {
    .name = "ext2",
    .mount = ext2_vfs_mount,
    .umount = ext2_vfs_umount,
    .next = NULL
};

/*
 * Register ext2 with VFS
 */
static int register_ext2_vfs(void) {
    return vfs_register_fs(&ext2_fs_vfs);
}

/*
 * Switch to disk root filesystem
 */
int switch_to_disk_root(void) {
    kprintf("[pivot_root] Switching to disk root filesystem...\n");

    /* Mount ext2 from disk directly (not through VFS for now) */
    if (mount_ext2_disk() < 0) {
        kprintf("[pivot_root] Failed to mount ext2 from disk\n");
        return -1;
    }

    /* Register ext2 with VFS */
    if (register_ext2_vfs() < 0) {
        kprintf("[pivot_root] Failed to register ext2 with VFS\n");
        return -2;
    }

    /* Mount ext2 at /mnt */
    if (vfs_mount(NULL, "/mnt", "ext2") < 0) {
        kprintf("[pivot_root] Failed to mount ext2 at /mnt\n");
        return -3;
    }

    g_disk_root = 1;
    kprintf("[pivot_root] Successfully switched to disk root\n");
    kprintf("[pivot_root] ext2 filesystem available at /mnt\n");

    return 0;
}

/*
 * Simplified pivot_root - currently just mounts ext2 at a path
 */
int pivot_root(const char *new_root, const char *put_old) {
    (void)put_old;  /* Not implemented yet */

    kprintf("[pivot_root] pivot_root(%s, %s)\n", new_root, put_old ? put_old : "NULL");

    /* For now, just ensure ext2 is mounted */
    return switch_to_disk_root();
}

/*
 * Check if running from disk root
 */
int is_disk_root(void) {
    return g_disk_root;
}
