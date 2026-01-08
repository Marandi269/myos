/*
 * heap.h - Kernel heap allocator interface
 */

#ifndef _HEAP_H
#define _HEAP_H

#include "types.h"

/* Initialize kernel heap */
void heap_init(void);

/* Allocate memory */
void *kmalloc(size_t size);

/* Free memory */
void kfree(void *ptr);

/* Reallocate memory */
void *krealloc(void *ptr, size_t new_size);

/* Allocate zeroed memory */
void *kzalloc(size_t size);

/* Print heap statistics */
void heap_print_stats(void);

#endif /* _HEAP_H */
