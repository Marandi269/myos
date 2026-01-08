/*
 * initramfs.c - Initial RAM filesystem loader (CPIO newc format)
 *
 * Unpacks a CPIO archive into ramfs.
 */

#include "initramfs.h"
#include "vfs.h"
#include "ramfs/ramfs.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Embedded initramfs data (linked from initramfs.S) */
extern char _binary_initramfs_cpio_start[];
extern char _binary_initramfs_cpio_end[];

static int initramfs_loaded = 0;

/* Parse hex number from CPIO header field */
static uint32_t parse_hex(const char *s, int len) {
    uint32_t val = 0;
    while (len--) {
        val <<= 4;
        if (*s >= '0' && *s <= '9') val |= *s - '0';
        else if (*s >= 'a' && *s <= 'f') val |= *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') val |= *s - 'A' + 10;
        s++;
    }
    return val;
}

/* Align to 4 bytes */
static inline size_t align4(size_t n) {
    return (n + 3) & ~3;
}

/*
 * Load and unpack CPIO archive into ramfs
 */
int initramfs_load(void *data, size_t size) {
    char *ptr = data;
    char *end = ptr + size;
    int file_count = 0;
    char path[256];

    kprintf("[initramfs] Loading archive (%d bytes)...\n", (int)size);

    while (ptr + sizeof(struct cpio_header) < end) {
        struct cpio_header *hdr = (struct cpio_header *)ptr;

        /* Check magic */
        if (memcmp(hdr->magic, CPIO_MAGIC, 6) != 0) {
            kprintf("[initramfs] Invalid magic at offset %d\n", (int)(ptr - (char*)data));
            break;
        }

        /* Parse header fields */
        uint32_t mode = parse_hex(hdr->mode, 8);
        uint32_t filesize = parse_hex(hdr->filesize, 8);
        uint32_t namesize = parse_hex(hdr->namesize, 8);

        /* Get filename (follows header) */
        char *name = ptr + sizeof(struct cpio_header);

        /* Check for end marker */
        if (strcmp(name, CPIO_TRAILER) == 0) {
            kprintf("[initramfs] End of archive\n");
            break;
        }

        /* Calculate data position */
        size_t header_plus_name = sizeof(struct cpio_header) + namesize;
        size_t name_padded = align4(header_plus_name);
        char *file_data = ptr + name_padded;
        size_t data_padded = align4(filesize);

        /* Skip "." entry */
        if (strcmp(name, ".") == 0) {
            ptr = file_data + data_padded;
            continue;
        }

        /* Build full path */
        if (name[0] == '/') {
            strncpy(path, name, sizeof(path) - 1);
        } else {
            path[0] = '/';
            strncpy(path + 1, name, sizeof(path) - 2);
        }
        path[sizeof(path) - 1] = '\0';

        /* Create entry based on type */
        if (S_ISDIR(mode)) {
            /* Directory */
            kprintf("[initramfs] mkdir %s\n", path);
            vfs_mkdir(path, mode & 0777);
        } else if (S_ISREG(mode)) {
            /* Regular file */
            kprintf("[initramfs] file %s (%d bytes)\n", path, filesize);

            /* Create parent directories if needed */
            char parent[256];
            strncpy(parent, path, sizeof(parent));
            char *last_slash = strrchr(parent, '/');
            if (last_slash && last_slash != parent) {
                *last_slash = '\0';
                /* Try to create parent (may already exist) */
                vfs_mkdir(parent, 0755);
            }

            /* Create and write file */
            {
                struct file *f = vfs_open(path, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
                if (f) {
                    if (filesize > 0) {
                        vfs_write(f, file_data, filesize);
                    }
                    vfs_close(f);
                    file_count++;
                } else {
                    kprintf("[initramfs] ERROR: Failed to open %s for writing\n", path);
                }
            }
        }

        /* Move to next entry */
        ptr = file_data + data_padded;
    }

    initramfs_loaded = 1;
    kprintf("[initramfs] Loaded %d files\n", file_count);
    return file_count;
}

/*
 * Initialize initramfs - try to load embedded archive
 */
void initramfs_init(void) {
    /* Check if we have embedded initramfs */
    size_t size = _binary_initramfs_cpio_end - _binary_initramfs_cpio_start;

    if (size > 0) {
        kprintf("[initramfs] Found embedded archive (%d bytes)\n", (int)size);
        initramfs_load(_binary_initramfs_cpio_start, size);
    } else {
        kprintf("[initramfs] No embedded archive\n");
    }
}

/*
 * Check if initramfs was successfully loaded
 */
int initramfs_available(void) {
    return initramfs_loaded;
}
