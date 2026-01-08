/*
 * syscall.h - System call interface
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

/* MSR addresses for SYSCALL/SYSRET */
#define MSR_EFER    0xC0000080  /* Extended Feature Enable Register */
#define MSR_STAR    0xC0000081  /* Segment selectors */
#define MSR_LSTAR   0xC0000082  /* SYSCALL entry point (64-bit) */
#define MSR_CSTAR   0xC0000083  /* SYSCALL entry point (compat, unused) */
#define MSR_SFMASK  0xC0000084  /* RFLAGS mask for SYSCALL */

/* EFER bits */
#define EFER_SCE    (1 << 0)    /* SYSCALL/SYSRET Enable */

/* Initialize SYSCALL/SYSRET */
void syscall_init(void);

/* System call handler (called from assembly) */
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif /* _SYSCALL_H */
