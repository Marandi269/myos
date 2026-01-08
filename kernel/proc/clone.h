/*
 * clone.h - clone() system call for thread creation
 */

#ifndef _CLONE_H
#define _CLONE_H

#include "types.h"

/* Clone flags (Linux compatible) */
#define CLONE_VM            0x00000100  /* Share virtual memory */
#define CLONE_FS            0x00000200  /* Share filesystem info */
#define CLONE_FILES         0x00000400  /* Share file descriptors */
#define CLONE_SIGHAND       0x00000800  /* Share signal handlers */
#define CLONE_PTRACE        0x00002000  /* Trace child too */
#define CLONE_VFORK         0x00004000  /* vfork() semantics */
#define CLONE_PARENT        0x00008000  /* Same parent as cloner */
#define CLONE_THREAD        0x00010000  /* Same thread group */
#define CLONE_NEWNS         0x00020000  /* New namespace */
#define CLONE_SYSVSEM       0x00040000  /* Share SysV semaphores */
#define CLONE_SETTLS        0x00080000  /* Set TLS for child */
#define CLONE_PARENT_SETTID 0x00100000  /* Store child TID in parent */
#define CLONE_CHILD_CLEARTID 0x00200000 /* Clear child TID on exit */
#define CLONE_DETACHED      0x00400000  /* Unused */
#define CLONE_CHILD_SETTID  0x01000000  /* Store child TID in child */

/* Typical thread flags */
#define CLONE_THREAD_FLAGS  (CLONE_VM | CLONE_FS | CLONE_FILES | \
                             CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM)

/* clone() system call
 *
 * flags: CLONE_* flags
 * child_stack: new stack pointer for child (must be set for CLONE_VM)
 * parent_tidptr: where to store child TID in parent
 * child_tidptr: where to store child TID in child
 * tls: TLS descriptor (if CLONE_SETTLS)
 */
int64_t sys_clone(uint64_t flags, void *child_stack,
                   int *parent_tidptr, int *child_tidptr, void *tls);

/* Set thread area (TLS) */
int64_t sys_set_tid_address(int *tidptr);

/* Architecture-specific: set FS base for TLS */
int64_t sys_arch_prctl(int code, uint64_t addr);

/* arch_prctl codes */
#define ARCH_SET_GS     0x1001
#define ARCH_SET_FS     0x1002
#define ARCH_GET_FS     0x1003
#define ARCH_GET_GS     0x1004

#endif /* _CLONE_H */
