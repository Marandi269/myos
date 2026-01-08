/*
 * shm.c - Shared memory implementation
 *
 * Simplified mmap implementation for anonymous shared memory.
 */

#include "shm.h"
#include "../proc/process.h"
#include "../proc/syscall.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../proc/user_space.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Simple memory region tracking */
#define MAX_MMAP_REGIONS 32

typedef struct {
    uint64_t vaddr;
    size_t length;
    int prot;
    int flags;
    int in_use;
} mmap_region_t;

static mmap_region_t mmap_regions[MAX_MMAP_REGIONS];

/* Find free virtual address range */
static uint64_t find_free_vaddr(size_t length) {
    /* Start searching from a reasonable user-space address */
    static uint64_t next_addr = 0x40000000;  /* 1GB */
    uint64_t addr = next_addr;

    /* Align to page boundary */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    next_addr += length;

    return addr;
}

/*
 * sys_mmap - Map memory
 *
 * Simplified implementation supporting:
 * - MAP_ANONYMOUS | MAP_PRIVATE (private anonymous memory)
 * - MAP_ANONYMOUS | MAP_SHARED (shared anonymous memory)
 */
int64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset) {
    uint64_t vaddr;
    uint64_t page_flags = PTE_USER;
    size_t i;
    mmap_region_t *region = NULL;

    (void)fd;
    (void)offset;

    if (length == 0) {
        return (int64_t)MAP_FAILED;
    }

    if (!current_proc || !current_proc->page_table) {
        kprintf("[MMAP] No current process or page table\n");
        return (int64_t)MAP_FAILED;
    }

    /* Only support anonymous mappings for now */
    if (!(flags & MAP_ANONYMOUS)) {
        kprintf("[MMAP] Only MAP_ANONYMOUS supported\n");
        return -EINVAL;
    }

    /* Find a free region slot */
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (!mmap_regions[i].in_use) {
            region = &mmap_regions[i];
            break;
        }
    }

    if (!region) {
        kprintf("[MMAP] No free region slots\n");
        return -ENOMEM;
    }

    /* Determine virtual address */
    if (addr && (flags & MAP_FIXED)) {
        vaddr = (uint64_t)addr;
    } else {
        vaddr = find_free_vaddr(length);
    }

    /* Align length to page size */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Set page flags based on protection */
    if (prot & PROT_WRITE) {
        page_flags |= PTE_WRITABLE;
    }

    kprintf("[MMAP] Mapping %d bytes at 0x%lx, prot=%d, flags=%d\n",
            (int)length, vaddr, prot, flags);

    /* Allocate and map pages */
    for (uint64_t page = vaddr; page < vaddr + length; page += PAGE_SIZE) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys) {
            /* TODO: Free already allocated pages on failure */
            kprintf("[MMAP] Failed to allocate physical page\n");
            return -ENOMEM;
        }

        /* Zero the page */
        memset((void *)phys, 0, PAGE_SIZE);

        /* Map the page */
        if (vmm_map_page_in(current_proc->page_table, page, phys, page_flags) != 0) {
            pmm_free_page((void *)phys);
            return -ENOMEM;
        }
    }

    /* Record the mapping */
    region->vaddr = vaddr;
    region->length = length;
    region->prot = prot;
    region->flags = flags;
    region->in_use = 1;

    kprintf("[MMAP] Successfully mapped at 0x%lx\n", vaddr);
    return (int64_t)vaddr;
}

/*
 * sys_munmap - Unmap memory
 */
int64_t sys_munmap(void *addr, size_t length) {
    uint64_t vaddr = (uint64_t)addr;
    size_t i;
    mmap_region_t *region = NULL;

    if (!current_proc || !current_proc->page_table) {
        return -EINVAL;
    }

    /* Find the region */
    for (i = 0; i < MAX_MMAP_REGIONS; i++) {
        if (mmap_regions[i].in_use && mmap_regions[i].vaddr == vaddr) {
            region = &mmap_regions[i];
            break;
        }
    }

    if (!region) {
        kprintf("[MUNMAP] Region not found at 0x%lx\n", vaddr);
        return -EINVAL;
    }

    /* Align length */
    length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (length == 0) {
        length = region->length;
    }

    kprintf("[MUNMAP] Unmapping %d bytes at 0x%lx\n", (int)length, vaddr);

    /* Unmap pages and free physical memory */
    for (uint64_t page = vaddr; page < vaddr + length; page += PAGE_SIZE) {
        uint64_t phys = vmm_get_phys(page);
        if (phys) {
            vmm_unmap_page(page);
            pmm_free_page((void *)phys);
        }
    }

    /* Mark region as free */
    region->in_use = 0;

    return 0;
}
