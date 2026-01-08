/*
 * initramfs.h - Initial RAM filesystem (CPIO format)
 */

#ifndef _INITRAMFS_H
#define _INITRAMFS_H

#include "types.h"

/* CPIO newc format header */
#define CPIO_MAGIC "070701"
#define CPIO_TRAILER "TRAILER!!!"

/* CPIO header structure (newc format - ASCII) */
struct cpio_header {
    char magic[6];      /* "070701" */
    char ino[8];        /* Inode number */
    char mode[8];       /* File mode */
    char uid[8];        /* User ID */
    char gid[8];        /* Group ID */
    char nlink[8];      /* Number of links */
    char mtime[8];      /* Modification time */
    char filesize[8];   /* File size */
    char devmajor[8];   /* Device major number */
    char devminor[8];   /* Device minor number */
    char rdevmajor[8];  /* Rdev major number */
    char rdevminor[8];  /* Rdev minor number */
    char namesize[8];   /* Filename length */
    char check[8];      /* Checksum (always 0 for newc) */
};

/* Initialize initramfs module */
void initramfs_init(void);

/* Load initramfs from memory location */
int initramfs_load(void *data, size_t size);

/* Check if initramfs is available */
int initramfs_available(void);

#endif /* _INITRAMFS_H */
