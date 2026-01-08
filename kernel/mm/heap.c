/*
 * heap.c - Simple kernel heap allocator
 *
 * Uses a simple block-based allocator with first-fit strategy.
 * Each block has a header with size and free/used status.
 */

#include "heap.h"
#include "pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Heap configuration */
#define HEAP_INITIAL_PAGES  16          /* 64 KB initial heap */
#define HEAP_BLOCK_MAGIC    0xDEADBEEF

/* Block header */
struct heap_block {
    uint32_t magic;         /* Magic number for validation */
    uint32_t size;          /* Size of data area (excluding header) */
    uint32_t free;          /* 1 = free, 0 = used */
    uint32_t padding;       /* Alignment padding */
    struct heap_block *next;
    struct heap_block *prev;
};

#define HEADER_SIZE     sizeof(struct heap_block)
#define MIN_BLOCK_SIZE  16

/* Heap state */
static struct heap_block *heap_start = NULL;
static struct heap_block *heap_end = NULL;
static uint64_t heap_size = 0;

/* Statistics */
static uint64_t total_allocated = 0;
static uint64_t total_freed = 0;
static uint64_t alloc_count = 0;
static uint64_t free_count = 0;

/* Initialize kernel heap */
void heap_init(void) {
    uint64_t i;
    void *page;
    uint64_t heap_base = 0;

    /* Allocate initial heap pages */
    for (i = 0; i < HEAP_INITIAL_PAGES; i++) {
        page = pmm_alloc_page();
        if (page == NULL) {
            kpanic("heap_init: Failed to allocate heap pages\n");
        }
        if (i == 0) {
            heap_base = (uint64_t)page;
        }
    }

    heap_size = HEAP_INITIAL_PAGES * PAGE_SIZE;

    /* Initialize first block */
    heap_start = (struct heap_block *)heap_base;
    heap_start->magic = HEAP_BLOCK_MAGIC;
    heap_start->size = heap_size - HEADER_SIZE;
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;

    heap_end = heap_start;

    kprintf("[Heap] Initialized: %d KB at 0x%x\n",
            (int)(heap_size / 1024), heap_base);
}

/* Find a free block that fits the requested size */
static struct heap_block *find_free_block(size_t size) {
    struct heap_block *block = heap_start;

    while (block != NULL) {
        if (block->free && block->size >= size) {
            return block;
        }
        block = block->next;
    }

    return NULL;
}

/* Split a block if it's too large */
static void split_block(struct heap_block *block, size_t size) {
    size_t remaining;
    struct heap_block *new_block;

    remaining = block->size - size - HEADER_SIZE;

    /* Only split if remaining space is useful */
    if (remaining < MIN_BLOCK_SIZE) {
        return;
    }

    /* Create new free block after this one */
    new_block = (struct heap_block *)((uint8_t *)block + HEADER_SIZE + size);

    /* Safety check: don't overwrite an existing allocated block.
     * This can happen if the free block list is corrupted or
     * if block sizes are incorrect. */
    if (new_block->magic == HEAP_BLOCK_MAGIC && !new_block->free) {
        return;  /* Don't split - keep the larger block */
    }

    new_block->magic = HEAP_BLOCK_MAGIC;
    new_block->size = remaining;
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) {
        block->next->prev = new_block;
    } else {
        heap_end = new_block;
    }

    block->next = new_block;
    block->size = size;
}

/* Merge adjacent free blocks */
static void merge_free_blocks(struct heap_block *block) {
    uint64_t block_end;

    /* Merge with next block if free and physically adjacent */
    if (block->next && block->next->free && block->next->magic == HEAP_BLOCK_MAGIC) {
        block_end = (uint64_t)block + HEADER_SIZE + block->size;
        if (block_end == (uint64_t)block->next) {
            struct heap_block *next = block->next;
            block->size += HEADER_SIZE + next->size;
            block->next = next->next;
            if (next->next) {
                next->next->prev = block;
            } else {
                heap_end = block;
            }
            next->magic = 0;
        }
    }

    /* Merge with previous block if free and physically adjacent */
    if (block->prev && block->prev->free && block->prev->magic == HEAP_BLOCK_MAGIC) {
        uint64_t prev_end = (uint64_t)block->prev + HEADER_SIZE + block->prev->size;
        if (prev_end == (uint64_t)block) {
            struct heap_block *prev = block->prev;
            prev->size += HEADER_SIZE + block->size;
            prev->next = block->next;
            if (block->next) {
                block->next->prev = prev;
            } else {
                heap_end = prev;
            }
            block->magic = 0;
        }
    }
}

/* Allocate memory */
void *kmalloc(size_t size) {
    struct heap_block *block;

    if (size == 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = (size + 7) & ~7;

    /* Find a free block */
    block = find_free_block(size);
    if (block == NULL) {
        kprintf("[Heap] ERROR: Out of heap memory (requested %d bytes)\n", (int)size);
        return NULL;
    }

    /* Split if necessary */
    split_block(block, size);

    /* Mark as used */
    block->free = 0;

    /* Update statistics */
    total_allocated += block->size;
    alloc_count++;

    /* Return pointer to data area */
    return (void *)((uint8_t *)block + HEADER_SIZE);
}

/* Free memory */
void kfree(void *ptr) {
    struct heap_block *block;

    if (ptr == NULL) {
        return;
    }

    /* Get block header */
    block = (struct heap_block *)((uint8_t *)ptr - HEADER_SIZE);

    /* Validate block */
    if (block->magic != HEAP_BLOCK_MAGIC) {
        kprintf("[Heap] ERROR: Invalid free (bad magic at 0x%x)\n", (uint64_t)ptr);
        return;
    }

    if (block->free) {
        kprintf("[Heap] WARNING: Double free at 0x%x\n", (uint64_t)ptr);
        return;
    }

    /* Update statistics */
    total_freed += block->size;
    free_count++;

    /* Mark as free */
    block->free = 1;

    /* Merge adjacent free blocks */
    merge_free_blocks(block);
}

/* Reallocate memory */
void *krealloc(void *ptr, size_t new_size) {
    struct heap_block *block;
    void *new_ptr;
    size_t copy_size;

    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    block = (struct heap_block *)((uint8_t *)ptr - HEADER_SIZE);

    if (block->magic != HEAP_BLOCK_MAGIC) {
        kprintf("[Heap] ERROR: Invalid realloc (bad magic)\n");
        return NULL;
    }

    /* If current block is big enough, just return */
    if (block->size >= new_size) {
        return ptr;
    }

    /* Allocate new block */
    new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    /* Copy data */
    copy_size = block->size < new_size ? block->size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    /* Free old block */
    kfree(ptr);

    return new_ptr;
}

/* Allocate zeroed memory */
void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/* Print heap statistics */
void heap_print_stats(void) {
    struct heap_block *block = heap_start;
    int total_blocks = 0;
    int free_blocks = 0;
    uint64_t free_bytes = 0;

    while (block != NULL) {
        total_blocks++;
        if (block->free) {
            free_blocks++;
            free_bytes += block->size;
        }
        block = block->next;
    }

    kprintf("[Heap] Statistics:\n");
    kprintf("  Total size: %d KB\n", (int)(heap_size / 1024));
    kprintf("  Blocks: %d total, %d free\n", total_blocks, free_blocks);
    kprintf("  Free space: %d bytes\n", (int)free_bytes);
    kprintf("  Allocated: %d bytes (%d calls)\n", (int)total_allocated, (int)alloc_count);
    kprintf("  Freed: %d bytes (%d calls)\n", (int)total_freed, (int)free_count);
}

/* Debug: dump heap blocks near address 0x183000-0x185000 */
void heap_dump_blocks(void) {
    struct heap_block *block = heap_start;
    int i = 0;
    kprintf("[Heap] Blocks near 0x183000:\n");
    while (block != NULL && i < 80) {
        uintptr_t addr = (uintptr_t)block;
        if (addr >= 0x183000 && addr < 0x185000) {
            kprintf("  [%d] %p: size=%d free=%d data=[%p-%p]\n",
                    i, block, block->size, block->free,
                    (uint8_t*)block + HEADER_SIZE,
                    (uint8_t*)block + HEADER_SIZE + block->size);
        }
        block = block->next;
        i++;
    }
}
