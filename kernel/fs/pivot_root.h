/*
 * pivot_root.h - Root filesystem switch functionality
 */

#ifndef PIVOT_ROOT_H
#define PIVOT_ROOT_H

#include "types.h"

/*
 * Switch root filesystem from ramfs to ext2 on disk
 *
 * This function:
 * 1. Mounts ext2 from disk partition
 * 2. Switches VFS root to ext2
 * 3. Optionally unmounts old root
 *
 * Parameters:
 *   new_root: Path to new root (e.g., "/mnt")
 *   put_old:  Path to place old root (e.g., "/mnt/oldroot"), or NULL to discard
 *
 * Returns:
 *   0 on success, negative error code on failure
 */
int pivot_root(const char *new_root, const char *put_old);

/*
 * Mount ext2 and switch to it as root filesystem
 *
 * This is a convenience function that:
 * 1. Detects the disk and partition
 * 2. Mounts ext2
 * 3. Performs pivot_root
 *
 * Returns:
 *   0 on success, negative error code on failure
 */
int switch_to_disk_root(void);

/*
 * Check if running from disk root
 */
int is_disk_root(void);

#endif /* PIVOT_ROOT_H */
