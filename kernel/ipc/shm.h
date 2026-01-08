/*
 * shm.h - Shared memory interface
 *
 * Simplified implementation using mmap with MAP_SHARED
 */

#ifndef _SHM_H
#define _SHM_H

#include "types.h"

/* mmap protection flags */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

/* mmap flags */
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

/* mmap failed return value */
#define MAP_FAILED ((void *)-1)

/* mmap system call */
int64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset);

/* munmap system call */
int64_t sys_munmap(void *addr, size_t length);

#endif /* _SHM_H */
