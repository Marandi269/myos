/*
 * ext2.h - ext2 filesystem structures and definitions
 */

#ifndef EXT2_H
#define EXT2_H

#include "types.h"

/* ext2 magic number */
#define EXT2_MAGIC          0xEF53

/* Superblock offset in the filesystem */
#define EXT2_SUPERBLOCK_OFFSET  1024

/* Inode types in i_mode */
#define EXT2_S_IFSOCK   0xC000  /* Socket */
#define EXT2_S_IFLNK    0xA000  /* Symbolic link */
#define EXT2_S_IFREG    0x8000  /* Regular file */
#define EXT2_S_IFBLK    0x6000  /* Block device */
#define EXT2_S_IFDIR    0x4000  /* Directory */
#define EXT2_S_IFCHR    0x2000  /* Character device */
#define EXT2_S_IFIFO    0x1000  /* FIFO */

#define EXT2_S_IFMT     0xF000  /* Type mask */

/* File type in directory entry */
#define EXT2_FT_UNKNOWN     0
#define EXT2_FT_REG_FILE    1
#define EXT2_FT_DIR         2
#define EXT2_FT_CHRDEV      3
#define EXT2_FT_BLKDEV      4
#define EXT2_FT_FIFO        5
#define EXT2_FT_SOCK        6
#define EXT2_FT_SYMLINK     7

/* Special inode numbers */
#define EXT2_BAD_INO        1
#define EXT2_ROOT_INO       2
#define EXT2_ACL_IDX_INO    3
#define EXT2_ACL_DATA_INO   4
#define EXT2_BOOT_LOADER_INO 5
#define EXT2_UNDEL_DIR_INO  6

/* Direct/indirect block counts */
#define EXT2_NDIR_BLOCKS    12
#define EXT2_IND_BLOCK      12
#define EXT2_DIND_BLOCK     13
#define EXT2_TIND_BLOCK     14
#define EXT2_N_BLOCKS       15

/* Superblock structure */
typedef struct ext2_superblock {
    uint32_t s_inodes_count;        /* Total inode count */
    uint32_t s_blocks_count;        /* Total block count */
    uint32_t s_r_blocks_count;      /* Reserved block count */
    uint32_t s_free_blocks_count;   /* Free block count */
    uint32_t s_free_inodes_count;   /* Free inode count */
    uint32_t s_first_data_block;    /* First data block (0 or 1) */
    uint32_t s_log_block_size;      /* Block size = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;       /* Fragment size */
    uint32_t s_blocks_per_group;    /* Blocks per group */
    uint32_t s_frags_per_group;     /* Fragments per group */
    uint32_t s_inodes_per_group;    /* Inodes per group */
    uint32_t s_mtime;               /* Last mount time */
    uint32_t s_wtime;               /* Last write time */
    uint16_t s_mnt_count;           /* Mount count */
    uint16_t s_max_mnt_count;       /* Maximum mount count */
    uint16_t s_magic;               /* Magic signature (0xEF53) */
    uint16_t s_state;               /* Filesystem state */
    uint16_t s_errors;              /* Error handling */
    uint16_t s_minor_rev_level;     /* Minor revision level */
    uint32_t s_lastcheck;           /* Last check time */
    uint32_t s_checkinterval;       /* Check interval */
    uint32_t s_creator_os;          /* Creator OS */
    uint32_t s_rev_level;           /* Revision level */
    uint16_t s_def_resuid;          /* Default reserved UID */
    uint16_t s_def_resgid;          /* Default reserved GID */

    /* EXT2_DYNAMIC_REV specific */
    uint32_t s_first_ino;           /* First non-reserved inode */
    uint16_t s_inode_size;          /* Inode structure size */
    uint16_t s_block_group_nr;      /* Block group number */
    uint32_t s_feature_compat;      /* Compatible features */
    uint32_t s_feature_incompat;    /* Incompatible features */
    uint32_t s_feature_ro_compat;   /* Read-only compatible features */
    uint8_t  s_uuid[16];            /* Volume UUID */
    char     s_volume_name[16];     /* Volume name */
    char     s_last_mounted[64];    /* Last mounted path */
    uint32_t s_algo_bitmap;         /* Compression algorithm */

    /* Performance hints */
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;

    /* Journaling support (ext3/ext4) */
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;

    /* Directory indexing support */
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint8_t  s_reserved[760];       /* Padding to 1024 bytes */
} __attribute__((packed)) ext2_superblock_t;

/* Block group descriptor */
typedef struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;       /* Block bitmap block */
    uint32_t bg_inode_bitmap;       /* Inode bitmap block */
    uint32_t bg_inode_table;        /* Inode table block */
    uint16_t bg_free_blocks_count;  /* Free blocks count */
    uint16_t bg_free_inodes_count;  /* Free inodes count */
    uint16_t bg_used_dirs_count;    /* Directory count */
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed)) ext2_block_group_desc_t;

/* Inode structure */
typedef struct ext2_inode {
    uint16_t i_mode;            /* File mode */
    uint16_t i_uid;             /* Owner UID */
    uint32_t i_size;            /* Size in bytes */
    uint32_t i_atime;           /* Access time */
    uint32_t i_ctime;           /* Creation time */
    uint32_t i_mtime;           /* Modification time */
    uint32_t i_dtime;           /* Deletion time */
    uint16_t i_gid;             /* Group ID */
    uint16_t i_links_count;     /* Link count */
    uint32_t i_blocks;          /* Block count (512-byte units) */
    uint32_t i_flags;           /* File flags */
    uint32_t i_osd1;            /* OS-dependent value 1 */
    uint32_t i_block[15];       /* Block pointers */
    uint32_t i_generation;      /* File version (for NFS) */
    uint32_t i_file_acl;        /* File ACL */
    uint32_t i_dir_acl;         /* Directory ACL / size high */
    uint32_t i_faddr;           /* Fragment address */
    uint8_t  i_osd2[12];        /* OS-dependent value 2 */
} __attribute__((packed)) ext2_inode_t;

/* Directory entry */
typedef struct ext2_dir_entry {
    uint32_t inode;             /* Inode number */
    uint16_t rec_len;           /* Record length */
    uint8_t  name_len;          /* Name length */
    uint8_t  file_type;         /* File type */
    char     name[];            /* File name (variable length) */
} __attribute__((packed)) ext2_dir_entry_t;

/* ext2 filesystem instance */
typedef struct ext2_fs {
    struct block_device *dev;       /* Block device */
    ext2_superblock_t sb;           /* Superblock */
    ext2_block_group_desc_t *bgdt;  /* Block group descriptor table */
    uint32_t block_size;            /* Block size in bytes */
    uint32_t inodes_per_block;      /* Inodes per block */
    uint32_t groups_count;          /* Number of block groups */
    uint32_t part_start_lba;        /* Partition start LBA (for partition offset) */
} ext2_fs_t;

/* Functions */
int ext2_mount(struct block_device *dev, uint32_t part_start_lba);
void ext2_unmount(void);

/* File operations - read */
int ext2_read_inode(uint32_t ino, ext2_inode_t *inode);
int ext2_read_file(uint32_t ino, void *buf, size_t offset, size_t count);
int ext2_lookup(uint32_t dir_ino, const char *name, uint32_t *ino);
int ext2_readdir(uint32_t dir_ino, void (*callback)(const char *name, uint32_t ino, uint8_t type));

/* File operations - write */
int ext2_write_inode(uint32_t ino, ext2_inode_t *inode);
int ext2_write_file(uint32_t ino, const void *buf, size_t offset, size_t count);
int ext2_create(uint32_t dir_ino, const char *name, uint16_t mode, uint32_t *new_ino);
int ext2_mkdir(uint32_t dir_ino, const char *name, uint16_t mode, uint32_t *new_ino);
int ext2_unlink(uint32_t dir_ino, const char *name);
int ext2_truncate(uint32_t ino, uint32_t new_size);
int ext2_sync(void);

/* Block/inode allocation */
int ext2_alloc_block(uint32_t *block);
void ext2_free_block(uint32_t block);
int ext2_alloc_inode(uint32_t *ino);
void ext2_free_inode(uint32_t ino);

/* Helper to get block size */
uint32_t ext2_block_size(void);

/* VFS integration */
void ext2_register(void);

#endif /* EXT2_H */
