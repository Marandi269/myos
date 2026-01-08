/*
 * pmm.c - Physical Memory Manager implementation
 *
 * Uses a bitmap to track page allocation.
 * Each bit represents one 4KB page.
 */

#include "pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Bitmap for page allocation */
#define MAX_PAGES       (256 * 1024)    /* Support up to 1GB RAM */
#define BITMAP_SIZE     (MAX_PAGES / 8)

static uint8_t page_bitmap[BITMAP_SIZE];

/* Memory statistics */
static uint64_t total_memory;
static uint64_t total_pages;
static uint64_t used_pages;

/* Kernel end address (defined in linker script) */
extern char _end[];

/* First usable page after kernel */
static uint64_t first_free_page;

/* Bitmap operations */
static inline void bitmap_set(uint64_t page) {
    page_bitmap[page / 8] |= (1 << (page % 8));
}

static inline void bitmap_clear(uint64_t page) {
    page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int bitmap_test(uint64_t page) {
    return page_bitmap[page / 8] & (1 << (page % 8));
}

/* Initialize physical memory manager */
void pmm_init(uint64_t mem_size) {
    uint64_t kernel_end;
    uint64_t i;

    total_memory = mem_size;
    total_pages = mem_size / PAGE_SIZE;

    if (total_pages > MAX_PAGES) {
        total_pages = MAX_PAGES;
        total_memory = MAX_PAGES * PAGE_SIZE;
    }

    /* Clear bitmap - all pages initially free */
    memset(page_bitmap, 0, BITMAP_SIZE);

    /* Calculate first free page after kernel */
    kernel_end = (uint64_t)_end;
    first_free_page = page_align_up(kernel_end) / PAGE_SIZE;

    /* Reserve pages 0 to first_free_page (includes kernel) */
    for (i = 0; i < first_free_page && i < total_pages; i++) {
        bitmap_set(i);
    }
    used_pages = first_free_page;

    kprintf("[PMM] Total memory: %d MB (%d pages)\n",
            (int)(total_memory / 1024 / 1024), (int)total_pages);
    kprintf("[PMM] Kernel end: 0x%x\n", (uint64_t)kernel_end);
    kprintf("[PMM] First free page: %d (0x%x)\n",
            (int)first_free_page, first_free_page * PAGE_SIZE);
    kprintf("[PMM] Free memory: %d MB\n",
            (int)((total_pages - used_pages) * PAGE_SIZE / 1024 / 1024));
}

/* Allocate a physical page */
void *pmm_alloc_page(void) {
    uint64_t i;

    for (i = first_free_page; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;

            /* Zero the page */
            void *page = (void *)(i * PAGE_SIZE);
            memset(page, 0, PAGE_SIZE);

            return page;
        }
    }

    /* Out of memory */
    kprintf("[PMM] ERROR: Out of physical memory!\n");
    return NULL;
}

/* Free a physical page */
void pmm_free_page(void *addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;

    if (page >= total_pages) {
        kprintf("[PMM] ERROR: Invalid page address 0x%x\n", (uint64_t)addr);
        return;
    }

    if (page < first_free_page) {
        kprintf("[PMM] ERROR: Cannot free kernel page 0x%x\n", (uint64_t)addr);
        return;
    }

    if (!bitmap_test(page)) {
        kprintf("[PMM] WARNING: Double free of page 0x%x\n", (uint64_t)addr);
        return;
    }

    bitmap_clear(page);
    used_pages--;
}

/* Get total memory */
uint64_t pmm_get_total_memory(void) {
    return total_memory;
}

/* Get free memory */
uint64_t pmm_get_free_memory(void) {
    return (total_pages - used_pages) * PAGE_SIZE;
}

/* Get used memory */
uint64_t pmm_get_used_memory(void) {
    return used_pages * PAGE_SIZE;
}

/* Print memory statistics */
void pmm_print_stats(void) {
    kprintf("[PMM] Memory Stats:\n");
    kprintf("  Total: %d MB\n", (int)(total_memory / 1024 / 1024));
    kprintf("  Used:  %d KB (%d pages)\n",
            (int)(used_pages * PAGE_SIZE / 1024), (int)used_pages);
    kprintf("  Free:  %d MB (%d pages)\n",
            (int)((total_pages - used_pages) * PAGE_SIZE / 1024 / 1024),
            (int)(total_pages - used_pages));
}
