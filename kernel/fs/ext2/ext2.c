/*
 * ext2.c - ext2 filesystem implementation (read/write)
 */

#include "ext2.h"
#include "../../drivers/ide.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"
#include "../../mm/heap.h"

/* Global ext2 filesystem instance */
static ext2_fs_t *g_ext2 = NULL;

/* Dirty flag for sync */
static int g_ext2_dirty = 0;

/* Read blocks from disk */
static int ext2_read_blocks(uint32_t block, void *buf, uint32_t count) {
    if (!g_ext2 || !g_ext2->dev)
        return -1;

    /* Calculate LBA from block number */
    uint32_t sectors_per_block = g_ext2->block_size / 512;
    uint32_t lba = g_ext2->part_start_lba + (block * sectors_per_block);

    /* Read sectors */
    uint32_t sector_count = count * sectors_per_block;
    return g_ext2->dev->read(g_ext2->dev, lba, buf, sector_count);
}

/* Read a single block */
static int ext2_read_block(uint32_t block, void *buf) {
    return ext2_read_blocks(block, buf, 1);
}

/* Write blocks to disk */
static int ext2_write_blocks(uint32_t block, const void *buf, uint32_t count) {
    if (!g_ext2 || !g_ext2->dev)
        return -1;

    /* Calculate LBA from block number */
    uint32_t sectors_per_block = g_ext2->block_size / 512;
    uint32_t lba = g_ext2->part_start_lba + (block * sectors_per_block);

    /* Write sectors */
    uint32_t sector_count = count * sectors_per_block;
    return g_ext2->dev->write(g_ext2->dev, lba, buf, sector_count);
}

/* Write a single block */
static int ext2_write_block(uint32_t block, const void *buf) {
    return ext2_write_blocks(block, buf, 1);
}

/* Get block size */
uint32_t ext2_block_size(void) {
    return g_ext2 ? g_ext2->block_size : 0;
}

/* Read inode from disk */
int ext2_read_inode(uint32_t ino, ext2_inode_t *inode) {
    if (!g_ext2 || ino == 0)
        return -1;

    /* Inode numbers start at 1 */
    ino--;

    /* Calculate block group and index within group */
    uint32_t group = ino / g_ext2->sb.s_inodes_per_group;
    uint32_t index = ino % g_ext2->sb.s_inodes_per_group;

    if (group >= g_ext2->groups_count)
        return -2;

    /* Get block group descriptor */
    ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

    /* Calculate which block and offset within the inode table */
    uint32_t inode_size = g_ext2->sb.s_inode_size;
    if (inode_size == 0)
        inode_size = sizeof(ext2_inode_t);  /* Default for rev 0 */

    uint32_t inodes_per_block = g_ext2->block_size / inode_size;
    uint32_t block = bg->bg_inode_table + (index / inodes_per_block);
    uint32_t offset = (index % inodes_per_block) * inode_size;

    /* Use static buffer to avoid heap corruption bug */
    static uint8_t inode_buf[4096];  /* Max block size */
    if (g_ext2->block_size > sizeof(inode_buf))
        return -3;

    if (ext2_read_block(block, inode_buf) < 0)
        return -4;

    /* Copy inode data */
    memcpy(inode, inode_buf + offset, sizeof(ext2_inode_t));

    return 0;
}

/* Get block number from inode block pointers */
static uint32_t ext2_get_block(ext2_inode_t *inode, uint32_t block_idx) {
    uint32_t ptrs_per_block = g_ext2->block_size / sizeof(uint32_t);
    uint32_t *buf = NULL;
    uint32_t result = 0;

    /* Direct blocks (0-11) */
    if (block_idx < EXT2_NDIR_BLOCKS) {
        return inode->i_block[block_idx];
    }

    block_idx -= EXT2_NDIR_BLOCKS;

    /* Single indirect (12) */
    if (block_idx < ptrs_per_block) {
        if (inode->i_block[EXT2_IND_BLOCK] == 0)
            return 0;

        buf = kmalloc(g_ext2->block_size);
        if (!buf)
            return 0;

        if (ext2_read_block(inode->i_block[EXT2_IND_BLOCK], buf) < 0) {
            kfree(buf);
            return 0;
        }

        result = buf[block_idx];
        kfree(buf);
        return result;
    }

    block_idx -= ptrs_per_block;

    /* Double indirect (13) */
    if (block_idx < ptrs_per_block * ptrs_per_block) {
        if (inode->i_block[EXT2_DIND_BLOCK] == 0)
            return 0;

        buf = kmalloc(g_ext2->block_size);
        if (!buf)
            return 0;

        /* First level */
        if (ext2_read_block(inode->i_block[EXT2_DIND_BLOCK], buf) < 0) {
            kfree(buf);
            return 0;
        }

        uint32_t ind_block = buf[block_idx / ptrs_per_block];
        if (ind_block == 0) {
            kfree(buf);
            return 0;
        }

        /* Second level */
        if (ext2_read_block(ind_block, buf) < 0) {
            kfree(buf);
            return 0;
        }

        result = buf[block_idx % ptrs_per_block];
        kfree(buf);
        return result;
    }

    /* Triple indirect not implemented for simplicity */
    return 0;
}

/* Read file data */
int ext2_read_file(uint32_t ino, void *buf, size_t offset, size_t count) {
    ext2_inode_t inode;

    if (ext2_read_inode(ino, &inode) < 0)
        return -1;

    /* Check bounds */
    if (offset >= inode.i_size)
        return 0;

    if (offset + count > inode.i_size)
        count = inode.i_size - offset;

    if (count == 0)
        return 0;

    /* Use static buffer to avoid heap corruption bug */
    static uint8_t read_block_buf[4096];
    if (g_ext2->block_size > sizeof(read_block_buf))
        return -2;

    size_t bytes_read = 0;
    uint8_t *dst = (uint8_t *)buf;

    while (bytes_read < count) {
        /* Calculate block index and offset within block */
        uint32_t block_idx = offset / g_ext2->block_size;
        uint32_t block_offset = offset % g_ext2->block_size;
        uint32_t to_read = g_ext2->block_size - block_offset;

        if (to_read > count - bytes_read)
            to_read = count - bytes_read;

        /* Get block number */
        uint32_t block_num = ext2_get_block(&inode, block_idx);
        if (block_num == 0) {
            /* Sparse file - fill with zeros */
            memset(dst, 0, to_read);
        } else {
            /* Read block and copy data */
            if (ext2_read_block(block_num, read_block_buf) < 0) {
                return -3;
            }
            memcpy(dst, read_block_buf + block_offset, to_read);
        }

        bytes_read += to_read;
        offset += to_read;
        dst += to_read;
    }

    return bytes_read;
}

/* Lookup file in directory */
int ext2_lookup(uint32_t dir_ino, const char *name, uint32_t *result_ino) {
    ext2_inode_t inode;

    if (ext2_read_inode(dir_ino, &inode) < 0)
        return -1;

    /* Must be a directory */
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return -2;

    /* Allocate buffer for directory data */
    uint8_t *dir_buf = kmalloc(inode.i_size);
    if (!dir_buf)
        return -3;

    /* Read entire directory */
    if (ext2_read_file(dir_ino, dir_buf, 0, inode.i_size) < 0) {
        kfree(dir_buf);
        return -4;
    }

    /* Search for entry */
    size_t name_len = strlen(name);
    uint32_t offset = 0;

    while (offset < inode.i_size) {
        ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(dir_buf + offset);

        if (entry->rec_len == 0)
            break;

        if (entry->inode != 0 &&
            entry->name_len == name_len &&
            memcmp(entry->name, name, name_len) == 0) {
            *result_ino = entry->inode;
            kfree(dir_buf);
            return 0;
        }

        offset += entry->rec_len;
    }

    kfree(dir_buf);
    return -5;  /* Not found */
}

/* Read directory entries */
int ext2_readdir(uint32_t dir_ino, void (*callback)(const char *name, uint32_t ino, uint8_t type)) {
    ext2_inode_t inode;

    if (ext2_read_inode(dir_ino, &inode) < 0)
        return -1;

    /* Must be a directory */
    if ((inode.i_mode & EXT2_S_IFMT) != EXT2_S_IFDIR)
        return -2;

    /* Allocate buffer for directory data */
    uint8_t *dir_buf = kmalloc(inode.i_size);
    if (!dir_buf)
        return -3;

    /* Read entire directory */
    if (ext2_read_file(dir_ino, dir_buf, 0, inode.i_size) < 0) {
        kfree(dir_buf);
        return -4;
    }

    /* Iterate through entries */
    uint32_t offset = 0;
    char name_buf[256];

    while (offset < inode.i_size) {
        ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(dir_buf + offset);

        if (entry->rec_len == 0)
            break;

        if (entry->inode != 0 && entry->name_len > 0) {
            /* Copy name and null-terminate */
            memcpy(name_buf, entry->name, entry->name_len);
            name_buf[entry->name_len] = '\0';

            callback(name_buf, entry->inode, entry->file_type);
        }

        offset += entry->rec_len;
    }

    kfree(dir_buf);
    return 0;
}

/* Mount ext2 filesystem */
int ext2_mount(block_device_t *dev, uint32_t part_start_lba) {
    if (g_ext2) {
        kprintf("[ext2] Already mounted\n");
        return -1;
    }

    /* Allocate filesystem structure */
    g_ext2 = kmalloc(sizeof(ext2_fs_t));
    if (!g_ext2)
        return -2;

    memset(g_ext2, 0, sizeof(ext2_fs_t));
    g_ext2->dev = dev;
    g_ext2->part_start_lba = part_start_lba;

    /* Read superblock (at 1024 bytes offset) */
    uint8_t sb_buf[1024];
    uint32_t sb_lba = part_start_lba + (EXT2_SUPERBLOCK_OFFSET / 512);

    if (dev->read(dev, sb_lba, sb_buf, 2) < 0) {
        kfree(g_ext2);
        g_ext2 = NULL;
        return -3;
    }

    memcpy(&g_ext2->sb, sb_buf, sizeof(ext2_superblock_t));

    /* Verify magic number */
    if (g_ext2->sb.s_magic != EXT2_MAGIC) {
        kprintf("[ext2] Invalid magic: 0x%x (expected 0xEF53)\n", g_ext2->sb.s_magic);
        kfree(g_ext2);
        g_ext2 = NULL;
        return -4;
    }

    /* Calculate block size */
    g_ext2->block_size = 1024 << g_ext2->sb.s_log_block_size;

    /* Calculate number of block groups */
    g_ext2->groups_count = (g_ext2->sb.s_blocks_count + g_ext2->sb.s_blocks_per_group - 1)
                           / g_ext2->sb.s_blocks_per_group;

    kprintf("[ext2] Block size: %d bytes\n", g_ext2->block_size);
    kprintf("[ext2] Blocks: %d, Inodes: %d\n",
            g_ext2->sb.s_blocks_count, g_ext2->sb.s_inodes_count);
    kprintf("[ext2] Block groups: %d\n", g_ext2->groups_count);

    /* Read block group descriptor table */
    /* It starts at block 2 for 1K blocks, or block 1 for larger blocks */
    uint32_t bgdt_block = (g_ext2->block_size == 1024) ? 2 : 1;
    uint32_t bgdt_size = g_ext2->groups_count * sizeof(ext2_block_group_desc_t);
    uint32_t bgdt_blocks = (bgdt_size + g_ext2->block_size - 1) / g_ext2->block_size;

    g_ext2->bgdt = kmalloc(bgdt_blocks * g_ext2->block_size);
    if (!g_ext2->bgdt) {
        kfree(g_ext2);
        g_ext2 = NULL;
        return -5;
    }

    if (ext2_read_blocks(bgdt_block, g_ext2->bgdt, bgdt_blocks) < 0) {
        kfree(g_ext2->bgdt);
        kfree(g_ext2);
        g_ext2 = NULL;
        return -6;
    }

    kprintf("[ext2] Mounted successfully\n");
    return 0;
}

/* Write superblock to disk */
static int ext2_write_superblock(void) {
    if (!g_ext2)
        return -1;

    uint8_t sb_buf[1024];
    memset(sb_buf, 0, sizeof(sb_buf));
    memcpy(sb_buf, &g_ext2->sb, sizeof(ext2_superblock_t));

    uint32_t sb_lba = g_ext2->part_start_lba + (EXT2_SUPERBLOCK_OFFSET / 512);
    return g_ext2->dev->write(g_ext2->dev, sb_lba, sb_buf, 2);
}

/* Write block group descriptor table to disk */
static int ext2_write_bgdt(void) {
    if (!g_ext2 || !g_ext2->bgdt)
        return -1;

    uint32_t bgdt_block = (g_ext2->block_size == 1024) ? 2 : 1;
    uint32_t bgdt_size = g_ext2->groups_count * sizeof(ext2_block_group_desc_t);
    uint32_t bgdt_blocks = (bgdt_size + g_ext2->block_size - 1) / g_ext2->block_size;

    return ext2_write_blocks(bgdt_block, g_ext2->bgdt, bgdt_blocks);
}

/* Write inode to disk */
int ext2_write_inode(uint32_t ino, ext2_inode_t *inode) {
    if (!g_ext2 || ino == 0)
        return -1;

    /* Inode numbers start at 1 */
    uint32_t adj_ino = ino - 1;

    /* Calculate block group and index within group */
    uint32_t group = adj_ino / g_ext2->sb.s_inodes_per_group;
    uint32_t index = adj_ino % g_ext2->sb.s_inodes_per_group;

    if (group >= g_ext2->groups_count)
        return -2;

    /* Get block group descriptor */
    ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

    /* Calculate which block and offset within the inode table */
    uint32_t inode_size = g_ext2->sb.s_inode_size;
    if (inode_size == 0)
        inode_size = sizeof(ext2_inode_t);

    uint32_t inodes_per_block = g_ext2->block_size / inode_size;
    uint32_t block = bg->bg_inode_table + (index / inodes_per_block);
    uint32_t offset = (index % inodes_per_block) * inode_size;

    /* Read the block containing this inode */
    uint8_t *buf = kmalloc(g_ext2->block_size);
    if (!buf)
        return -3;

    if (ext2_read_block(block, buf) < 0) {
        kfree(buf);
        return -4;
    }

    /* Update inode data */
    memcpy(buf + offset, inode, sizeof(ext2_inode_t));

    /* Write block back */
    if (ext2_write_block(block, buf) < 0) {
        kfree(buf);
        return -5;
    }

    kfree(buf);
    g_ext2_dirty = 1;
    return 0;
}

/* Allocate a block from the filesystem */
int ext2_alloc_block(uint32_t *block) {
    if (!g_ext2 || g_ext2->sb.s_free_blocks_count == 0)
        return -1;

    uint8_t *bitmap = kmalloc(g_ext2->block_size);
    if (!bitmap)
        return -2;

    /* Search through block groups */
    for (uint32_t group = 0; group < g_ext2->groups_count; group++) {
        ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

        if (bg->bg_free_blocks_count == 0)
            continue;

        /* Read block bitmap */
        if (ext2_read_block(bg->bg_block_bitmap, bitmap) < 0) {
            kfree(bitmap);
            return -3;
        }

        /* Find free block in bitmap */
        for (uint32_t i = 0; i < g_ext2->sb.s_blocks_per_group / 8; i++) {
            if (bitmap[i] == 0xFF)
                continue;

            for (int bit = 0; bit < 8; bit++) {
                if (!(bitmap[i] & (1 << bit))) {
                    /* Found free block */
                    bitmap[i] |= (1 << bit);

                    /* Write bitmap back */
                    if (ext2_write_block(bg->bg_block_bitmap, bitmap) < 0) {
                        kfree(bitmap);
                        return -4;
                    }

                    /* Update counts */
                    bg->bg_free_blocks_count--;
                    g_ext2->sb.s_free_blocks_count--;

                    /* Calculate actual block number */
                    *block = g_ext2->sb.s_first_data_block +
                             (group * g_ext2->sb.s_blocks_per_group) +
                             (i * 8 + bit);

                    kfree(bitmap);
                    g_ext2_dirty = 1;
                    return 0;
                }
            }
        }
    }

    kfree(bitmap);
    return -5;  /* No free blocks */
}

/* Free a block */
void ext2_free_block(uint32_t block) {
    if (!g_ext2 || block < g_ext2->sb.s_first_data_block)
        return;

    /* Calculate which group and bit */
    uint32_t rel_block = block - g_ext2->sb.s_first_data_block;
    uint32_t group = rel_block / g_ext2->sb.s_blocks_per_group;
    uint32_t index = rel_block % g_ext2->sb.s_blocks_per_group;

    if (group >= g_ext2->groups_count)
        return;

    ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

    /* Read bitmap */
    uint8_t *bitmap = kmalloc(g_ext2->block_size);
    if (!bitmap)
        return;

    if (ext2_read_block(bg->bg_block_bitmap, bitmap) < 0) {
        kfree(bitmap);
        return;
    }

    /* Clear bit */
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    bitmap[byte] &= ~(1 << bit);

    /* Write bitmap back */
    ext2_write_block(bg->bg_block_bitmap, bitmap);

    /* Update counts */
    bg->bg_free_blocks_count++;
    g_ext2->sb.s_free_blocks_count++;

    kfree(bitmap);
    g_ext2_dirty = 1;
}

/* Allocate an inode */
int ext2_alloc_inode(uint32_t *ino) {
    if (!g_ext2 || g_ext2->sb.s_free_inodes_count == 0)
        return -1;

    uint8_t *bitmap = kmalloc(g_ext2->block_size);
    if (!bitmap)
        return -2;

    /* Search through block groups */
    for (uint32_t group = 0; group < g_ext2->groups_count; group++) {
        ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

        if (bg->bg_free_inodes_count == 0)
            continue;

        /* Read inode bitmap */
        if (ext2_read_block(bg->bg_inode_bitmap, bitmap) < 0) {
            kfree(bitmap);
            return -3;
        }

        /* Find free inode in bitmap */
        for (uint32_t i = 0; i < g_ext2->sb.s_inodes_per_group / 8; i++) {
            if (bitmap[i] == 0xFF)
                continue;

            for (int bit = 0; bit < 8; bit++) {
                if (!(bitmap[i] & (1 << bit))) {
                    /* Found free inode */
                    bitmap[i] |= (1 << bit);

                    /* Write bitmap back */
                    if (ext2_write_block(bg->bg_inode_bitmap, bitmap) < 0) {
                        kfree(bitmap);
                        return -4;
                    }

                    /* Update counts */
                    bg->bg_free_inodes_count--;
                    g_ext2->sb.s_free_inodes_count--;

                    /* Calculate actual inode number (1-based) */
                    *ino = (group * g_ext2->sb.s_inodes_per_group) + (i * 8 + bit) + 1;

                    kfree(bitmap);
                    g_ext2_dirty = 1;
                    return 0;
                }
            }
        }
    }

    kfree(bitmap);
    return -5;  /* No free inodes */
}

/* Free an inode */
void ext2_free_inode(uint32_t ino) {
    if (!g_ext2 || ino == 0)
        return;

    uint32_t adj_ino = ino - 1;
    uint32_t group = adj_ino / g_ext2->sb.s_inodes_per_group;
    uint32_t index = adj_ino % g_ext2->sb.s_inodes_per_group;

    if (group >= g_ext2->groups_count)
        return;

    ext2_block_group_desc_t *bg = &g_ext2->bgdt[group];

    /* Read bitmap */
    uint8_t *bitmap = kmalloc(g_ext2->block_size);
    if (!bitmap)
        return;

    if (ext2_read_block(bg->bg_inode_bitmap, bitmap) < 0) {
        kfree(bitmap);
        return;
    }

    /* Clear bit */
    uint32_t byte = index / 8;
    uint32_t bit = index % 8;
    bitmap[byte] &= ~(1 << bit);

    /* Write bitmap back */
    ext2_write_block(bg->bg_inode_bitmap, bitmap);

    /* Update counts */
    bg->bg_free_inodes_count++;
    g_ext2->sb.s_free_inodes_count++;

    kfree(bitmap);
    g_ext2_dirty = 1;
}

/* Set block in inode (allocate indirect blocks as needed) */
static int ext2_set_block(ext2_inode_t *inode, uint32_t block_idx, uint32_t block_num) {
    uint32_t ptrs_per_block = g_ext2->block_size / sizeof(uint32_t);

    /* Direct blocks (0-11) */
    if (block_idx < EXT2_NDIR_BLOCKS) {
        inode->i_block[block_idx] = block_num;
        return 0;
    }

    block_idx -= EXT2_NDIR_BLOCKS;

    /* Single indirect */
    if (block_idx < ptrs_per_block) {
        /* Allocate indirect block if needed */
        if (inode->i_block[EXT2_IND_BLOCK] == 0) {
            uint32_t ind_block;
            if (ext2_alloc_block(&ind_block) < 0)
                return -1;

            /* Zero out the indirect block */
            uint8_t *zero_buf = kmalloc(g_ext2->block_size);
            if (!zero_buf)
                return -2;
            memset(zero_buf, 0, g_ext2->block_size);
            ext2_write_block(ind_block, zero_buf);
            kfree(zero_buf);

            inode->i_block[EXT2_IND_BLOCK] = ind_block;
            inode->i_blocks += g_ext2->block_size / 512;
        }

        /* Read indirect block, update, write back */
        uint32_t *ind_buf = kmalloc(g_ext2->block_size);
        if (!ind_buf)
            return -3;

        ext2_read_block(inode->i_block[EXT2_IND_BLOCK], ind_buf);
        ind_buf[block_idx] = block_num;
        ext2_write_block(inode->i_block[EXT2_IND_BLOCK], ind_buf);
        kfree(ind_buf);
        return 0;
    }

    /* Double/triple indirect not implemented for simplicity */
    return -4;
}

/* Write file data */
int ext2_write_file(uint32_t ino, const void *buf, size_t offset, size_t count) {
    ext2_inode_t inode;

    if (ext2_read_inode(ino, &inode) < 0)
        return -1;

    /* Allocate temp buffer for block operations */
    uint8_t *block_buf = kmalloc(g_ext2->block_size);
    if (!block_buf)
        return -2;

    size_t bytes_written = 0;
    const uint8_t *src = (const uint8_t *)buf;

    while (bytes_written < count) {
        /* Calculate block index and offset within block */
        uint32_t block_idx = offset / g_ext2->block_size;
        uint32_t block_offset = offset % g_ext2->block_size;
        uint32_t to_write = g_ext2->block_size - block_offset;

        if (to_write > count - bytes_written)
            to_write = count - bytes_written;

        /* Get or allocate block */
        uint32_t block_num = ext2_get_block(&inode, block_idx);
        if (block_num == 0) {
            /* Need to allocate a new block */
            if (ext2_alloc_block(&block_num) < 0) {
                kfree(block_buf);
                return -3;
            }

            /* Set block in inode */
            if (ext2_set_block(&inode, block_idx, block_num) < 0) {
                ext2_free_block(block_num);
                kfree(block_buf);
                return -4;
            }

            inode.i_blocks += g_ext2->block_size / 512;

            /* Zero the new block if partial write */
            if (block_offset > 0 || to_write < g_ext2->block_size)
                memset(block_buf, 0, g_ext2->block_size);
        } else if (block_offset > 0 || to_write < g_ext2->block_size) {
            /* Partial write - read existing block first */
            if (ext2_read_block(block_num, block_buf) < 0) {
                kfree(block_buf);
                return -5;
            }
        }

        /* Copy data to block buffer */
        memcpy(block_buf + block_offset, src, to_write);

        /* Write block */
        if (ext2_write_block(block_num, block_buf) < 0) {
            kfree(block_buf);
            return -6;
        }

        bytes_written += to_write;
        offset += to_write;
        src += to_write;
    }

    /* Update inode size if necessary */
    if (offset > inode.i_size)
        inode.i_size = offset;

    /* Write inode back */
    ext2_write_inode(ino, &inode);

    kfree(block_buf);
    g_ext2_dirty = 1;
    return bytes_written;
}

/* Create a new file in a directory */
int ext2_create(uint32_t dir_ino, const char *name, uint16_t mode, uint32_t *new_ino) {
    /* Allocate new inode */
    uint32_t ino;
    if (ext2_alloc_inode(&ino) < 0)
        return -1;

    /* Initialize inode */
    ext2_inode_t inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = mode;
    inode.i_links_count = 1;

    /* Write inode */
    if (ext2_write_inode(ino, &inode) < 0) {
        ext2_free_inode(ino);
        return -2;
    }

    /* Add directory entry */
    ext2_inode_t dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0) {
        ext2_free_inode(ino);
        return -3;
    }

    /* Read directory content */
    uint8_t *dir_buf = kmalloc(dir_inode.i_size + g_ext2->block_size);
    if (!dir_buf) {
        ext2_free_inode(ino);
        return -4;
    }

    if (ext2_read_file(dir_ino, dir_buf, 0, dir_inode.i_size) < 0) {
        kfree(dir_buf);
        ext2_free_inode(ino);
        return -5;
    }

    /* Find space for new entry or add at end */
    size_t name_len = strlen(name);
    size_t entry_size = sizeof(ext2_dir_entry_t) + name_len;
    entry_size = (entry_size + 3) & ~3;  /* 4-byte alignment */

    uint32_t offset = 0;
    while (offset < dir_inode.i_size) {
        ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(dir_buf + offset);

        if (entry->rec_len == 0)
            break;

        /* Check if this entry has enough slack space */
        size_t actual_size = sizeof(ext2_dir_entry_t) + entry->name_len;
        actual_size = (actual_size + 3) & ~3;

        if (entry->rec_len >= actual_size + entry_size) {
            /* Split this entry */
            size_t new_rec_len = entry->rec_len - actual_size;
            entry->rec_len = actual_size;

            /* Create new entry */
            ext2_dir_entry_t *new_entry = (ext2_dir_entry_t *)(dir_buf + offset + actual_size);
            new_entry->inode = ino;
            new_entry->rec_len = new_rec_len;
            new_entry->name_len = name_len;
            new_entry->file_type = (mode & EXT2_S_IFDIR) ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
            memcpy(new_entry->name, name, name_len);

            /* Write directory back */
            ext2_write_file(dir_ino, dir_buf, 0, dir_inode.i_size);

            kfree(dir_buf);
            *new_ino = ino;
            return 0;
        }

        offset += entry->rec_len;
    }

    /* Need to extend directory - add a new block */
    uint32_t new_offset = dir_inode.i_size;
    uint32_t new_block_offset = new_offset % g_ext2->block_size;

    if (new_block_offset == 0) {
        /* Allocate new block for directory */
        memset(dir_buf + new_offset, 0, g_ext2->block_size);
    }

    ext2_dir_entry_t *new_entry = (ext2_dir_entry_t *)(dir_buf + new_offset);
    new_entry->inode = ino;
    new_entry->rec_len = g_ext2->block_size - new_block_offset;
    new_entry->name_len = name_len;
    new_entry->file_type = (mode & EXT2_S_IFDIR) ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    memcpy(new_entry->name, name, name_len);

    /* Write extended directory */
    ext2_write_file(dir_ino, dir_buf, 0, new_offset + g_ext2->block_size);

    kfree(dir_buf);
    *new_ino = ino;
    return 0;
}

/* Create a directory */
int ext2_mkdir(uint32_t dir_ino, const char *name, uint16_t mode, uint32_t *new_ino) {
    /* Create the directory inode */
    uint32_t ino;
    if (ext2_create(dir_ino, name, (mode & 0xFFF) | EXT2_S_IFDIR, &ino) < 0)
        return -1;

    /* Create . and .. entries */
    uint8_t *dir_buf = kmalloc(g_ext2->block_size);
    if (!dir_buf) {
        /* TODO: cleanup */
        return -2;
    }
    memset(dir_buf, 0, g_ext2->block_size);

    /* . entry */
    ext2_dir_entry_t *dot = (ext2_dir_entry_t *)dir_buf;
    dot->inode = ino;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';

    /* .. entry */
    ext2_dir_entry_t *dotdot = (ext2_dir_entry_t *)(dir_buf + 12);
    dotdot->inode = dir_ino;
    dotdot->rec_len = g_ext2->block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    /* Write directory content */
    ext2_write_file(ino, dir_buf, 0, g_ext2->block_size);
    kfree(dir_buf);

    /* Update link counts */
    ext2_inode_t inode;
    ext2_read_inode(ino, &inode);
    inode.i_links_count = 2;  /* . and parent link */
    ext2_write_inode(ino, &inode);

    ext2_read_inode(dir_ino, &inode);
    inode.i_links_count++;  /* .. link from child */
    ext2_write_inode(dir_ino, &inode);

    /* Update used_dirs_count in block group */
    uint32_t group = (ino - 1) / g_ext2->sb.s_inodes_per_group;
    g_ext2->bgdt[group].bg_used_dirs_count++;

    *new_ino = ino;
    g_ext2_dirty = 1;
    return 0;
}

/* Truncate a file */
int ext2_truncate(uint32_t ino, uint32_t new_size) {
    ext2_inode_t inode;
    if (ext2_read_inode(ino, &inode) < 0)
        return -1;

    if (new_size >= inode.i_size)
        return 0;  /* Nothing to do */

    /* Free blocks beyond new size */
    uint32_t new_blocks = (new_size + g_ext2->block_size - 1) / g_ext2->block_size;
    uint32_t old_blocks = (inode.i_size + g_ext2->block_size - 1) / g_ext2->block_size;

    for (uint32_t i = new_blocks; i < old_blocks; i++) {
        uint32_t block = ext2_get_block(&inode, i);
        if (block != 0) {
            ext2_free_block(block);
        }
    }

    /* Update inode */
    inode.i_size = new_size;
    inode.i_blocks = new_blocks * (g_ext2->block_size / 512);
    ext2_write_inode(ino, &inode);

    return 0;
}

/* Unlink (delete) a file */
int ext2_unlink(uint32_t dir_ino, const char *name) {
    /* Find the entry */
    uint32_t ino;
    if (ext2_lookup(dir_ino, name, &ino) < 0)
        return -1;

    /* Read directory */
    ext2_inode_t dir_inode;
    if (ext2_read_inode(dir_ino, &dir_inode) < 0)
        return -2;

    uint8_t *dir_buf = kmalloc(dir_inode.i_size);
    if (!dir_buf)
        return -3;

    if (ext2_read_file(dir_ino, dir_buf, 0, dir_inode.i_size) < 0) {
        kfree(dir_buf);
        return -4;
    }

    /* Find and remove entry */
    size_t name_len = strlen(name);
    uint32_t offset = 0;
    uint32_t prev_offset = 0;

    while (offset < dir_inode.i_size) {
        ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(dir_buf + offset);

        if (entry->rec_len == 0)
            break;

        if (entry->inode == ino &&
            entry->name_len == name_len &&
            memcmp(entry->name, name, name_len) == 0) {

            if (offset == 0) {
                /* First entry - just zero the inode */
                entry->inode = 0;
            } else {
                /* Merge with previous entry */
                ext2_dir_entry_t *prev = (ext2_dir_entry_t *)(dir_buf + prev_offset);
                prev->rec_len += entry->rec_len;
            }

            /* Write directory back */
            ext2_write_file(dir_ino, dir_buf, 0, dir_inode.i_size);

            /* Decrease link count and possibly free inode */
            ext2_inode_t inode;
            ext2_read_inode(ino, &inode);
            inode.i_links_count--;

            if (inode.i_links_count == 0) {
                /* Free all blocks */
                ext2_truncate(ino, 0);
                ext2_free_inode(ino);
            } else {
                ext2_write_inode(ino, &inode);
            }

            kfree(dir_buf);
            return 0;
        }

        prev_offset = offset;
        offset += entry->rec_len;
    }

    kfree(dir_buf);
    return -5;  /* Not found */
}

/* Sync filesystem to disk */
int ext2_sync(void) {
    if (!g_ext2 || !g_ext2_dirty)
        return 0;

    /* Write superblock */
    if (ext2_write_superblock() < 0)
        return -1;

    /* Write block group descriptor table */
    if (ext2_write_bgdt() < 0)
        return -2;

    g_ext2_dirty = 0;
    kprintf("[ext2] Synced to disk\n");
    return 0;
}

/* Unmount ext2 filesystem */
void ext2_unmount(void) {
    if (g_ext2) {
        /* Sync before unmount */
        ext2_sync();

        if (g_ext2->bgdt)
            kfree(g_ext2->bgdt);
        kfree(g_ext2);
        g_ext2 = NULL;
    }
}

/* Register ext2 with VFS (placeholder for future VFS integration) */
void ext2_register(void) {
    kprintf("[ext2] Filesystem registered (standalone mode)\n");
}
