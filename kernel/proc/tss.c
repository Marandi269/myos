/*
 * tss.c - Task State Segment implementation
 */

#include "tss.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* TSS structure (global) */
static tss_t tss __attribute__((aligned(16)));

/* Initialize TSS */
void tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    /* Set I/O permission bitmap offset to beyond TSS (disable I/O) */
    tss.iopb_offset = sizeof(tss);

    kprintf("[TSS] Initialized\n");
}

/* Set kernel stack for current process */
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

/* Get TSS pointer */
tss_t *tss_get(void) {
    return &tss;
}
