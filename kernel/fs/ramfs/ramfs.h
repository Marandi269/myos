/*
 * ramfs.h - RAM filesystem interface
 */

#ifndef _RAMFS_H
#define _RAMFS_H

#include "../vfs.h"

/* Initialize ramfs */
void ramfs_init(void);

/* Get ramfs filesystem type */
struct file_system_type* ramfs_get_type(void);

#endif /* _RAMFS_H */
