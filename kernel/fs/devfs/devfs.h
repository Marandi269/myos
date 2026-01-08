/*
 * devfs.h - Device filesystem interface
 */

#ifndef _DEVFS_H
#define _DEVFS_H

#include "../vfs.h"

/* Initialize devfs */
void devfs_init(void);

/* Register a device */
int devfs_register(const char *name, struct file_operations *ops, void *private);

/* Unregister a device */
int devfs_unregister(const char *name);

/* Get devfs filesystem type */
struct file_system_type* devfs_get_type(void);

#endif /* _DEVFS_H */
