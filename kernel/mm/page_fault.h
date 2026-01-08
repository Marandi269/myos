/*
 * page_fault.h - Page fault handler interface
 */

#ifndef _PAGE_FAULT_H
#define _PAGE_FAULT_H

#include "types.h"

/* Page fault error code bits */
#define PF_PRESENT      (1 << 0)    /* Fault caused by protection violation */
#define PF_WRITE        (1 << 1)    /* Fault caused by write access */
#define PF_USER         (1 << 2)    /* Fault occurred in user mode */
#define PF_RESERVED     (1 << 3)    /* Reserved bit was set in page entry */
#define PF_FETCH        (1 << 4)    /* Fault caused by instruction fetch */

/* Page fault handler (called from exception_handler) */
void page_fault_handler(uint64_t error_code);

#endif /* _PAGE_FAULT_H */
