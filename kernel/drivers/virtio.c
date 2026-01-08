/*
 * virtio.c - Virtio common layer
 */

#include "drivers/virtio.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Initialize a virtqueue */
int virtq_init(virtq_t *vq, uint16_t size, void *mem) {
    if (!vq || !mem || size == 0) {
        return -1;
    }

    memset(vq, 0, sizeof(*vq));
    vq->size = size;
    vq->num_free = size;
    vq->free_head = 0;
    vq->last_used_idx = 0;

    /* Setup descriptor table at start of memory */
    vq->desc = (virtq_desc_t *)mem;

    /* Available ring follows descriptors */
    vq->avail = (virtq_avail_t *)((uint8_t *)mem +
                                   sizeof(virtq_desc_t) * size);

    /* Used ring at page-aligned offset */
    uint64_t used_offset = (sizeof(virtq_desc_t) * size +
                           sizeof(uint16_t) * (3 + size) + 4095) & ~4095UL;
    vq->used = (virtq_used_t *)((uint8_t *)mem + used_offset);

    /* Allocate virtual address tracking array */
    vq->desc_virt = (void **)kmalloc(sizeof(void *) * size);
    if (!vq->desc_virt) {
        return -1;
    }
    memset(vq->desc_virt, 0, sizeof(void *) * size);

    /* Initialize descriptor chain - each descriptor points to next */
    for (uint16_t i = 0; i < size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[size - 1].next = 0xFFFF;  /* End of free list */

    /* Initialize available ring */
    vq->avail->flags = 0;
    vq->avail->idx = 0;

    /* Initialize used ring */
    vq->used->flags = 0;
    vq->used->idx = 0;

    return 0;
}

/* Allocate a single descriptor */
int virtq_alloc_desc(virtq_t *vq) {
    if (vq->num_free == 0) {
        return -1;
    }

    int idx = vq->free_head;
    vq->free_head = vq->desc[idx].next;
    vq->num_free--;

    return idx;
}

/* Free a descriptor chain */
void virtq_free_desc(virtq_t *vq, int head) {
    if (head < 0 || head >= vq->size) {
        return;
    }

    int i = head;
    while (1) {
        vq->desc_virt[i] = NULL;

        if (!(vq->desc[i].flags & VIRTQ_DESC_F_NEXT)) {
            break;
        }
        i = vq->desc[i].next;
    }

    /* Add back to free list */
    vq->desc[i].next = vq->free_head;
    vq->free_head = head;
    vq->num_free++;
}

/* Add buffer to queue - returns descriptor index or -1 on error */
int virtq_add_buf(virtq_t *vq, void *buf, uint32_t len, int write) {
    int desc_idx = virtq_alloc_desc(vq);
    if (desc_idx < 0) {
        return -1;
    }

    /* Setup descriptor */
    vq->desc[desc_idx].addr = (uint64_t)buf;  /* Physical address */
    vq->desc[desc_idx].len = len;
    vq->desc[desc_idx].flags = write ? VIRTQ_DESC_F_WRITE : 0;
    vq->desc[desc_idx].next = 0;

    /* Track virtual address for later retrieval */
    vq->desc_virt[desc_idx] = buf;

    /* Add to available ring */
    uint16_t avail_idx = vq->avail->idx % vq->size;
    vq->avail->ring[avail_idx] = desc_idx;

    /* Memory barrier */
    __asm__ volatile ("mfence" ::: "memory");

    /* Update available index */
    vq->avail->idx++;

    return desc_idx;
}

/* Get completed buffer from used ring */
int virtq_get_buf(virtq_t *vq, uint32_t *len) {
    /* Check if there are used buffers */
    if (vq->last_used_idx == vq->used->idx) {
        return -1;
    }

    /* Memory barrier */
    __asm__ volatile ("lfence" ::: "memory");

    /* Get used element */
    uint16_t used_idx = vq->last_used_idx % vq->size;
    int desc_idx = vq->used->ring[used_idx].id;

    if (len) {
        *len = vq->used->ring[used_idx].len;
    }

    vq->last_used_idx++;

    return desc_idx;
}

/* Check if there are used buffers available */
bool virtq_has_used(virtq_t *vq) {
    return vq->last_used_idx != vq->used->idx;
}
