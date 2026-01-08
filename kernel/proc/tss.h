/*
 * tss.h - Task State Segment definitions
 */

#ifndef _TSS_H
#define _TSS_H

#include "types.h"

/* Task State Segment (64-bit) */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;          /* Kernel stack pointer (ring 0) */
    uint64_t rsp1;          /* Ring 1 stack (unused) */
    uint64_t rsp2;          /* Ring 2 stack (unused) */
    uint64_t reserved1;
    uint64_t ist[7];        /* Interrupt Stack Table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;   /* I/O Permission Bitmap offset */
} __attribute__((packed)) tss_t;

/* Initialize TSS */
void tss_init(void);

/* Set kernel stack for current process (called on context switch) */
void tss_set_rsp0(uint64_t rsp0);

/* Get TSS pointer */
tss_t *tss_get(void);

#endif /* _TSS_H */
