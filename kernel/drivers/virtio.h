/*
 * virtio.h - Virtio common definitions
 */

#ifndef _VIRTIO_H
#define _VIRTIO_H

#include "types.h"

/* Virtio PCI capability types */
#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

/* Virtio device status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

/* Virtio legacy I/O offsets (for legacy devices) */
#define VIRTIO_PCI_HOST_FEATURES    0   /* 32-bit */
#define VIRTIO_PCI_GUEST_FEATURES   4   /* 32-bit */
#define VIRTIO_PCI_QUEUE_PFN        8   /* 32-bit */
#define VIRTIO_PCI_QUEUE_SIZE       12  /* 16-bit */
#define VIRTIO_PCI_QUEUE_SEL        14  /* 16-bit */
#define VIRTIO_PCI_QUEUE_NOTIFY     16  /* 16-bit */
#define VIRTIO_PCI_STATUS           18  /* 8-bit */
#define VIRTIO_PCI_ISR              19  /* 8-bit */
#define VIRTIO_PCI_CONFIG           20  /* Device-specific config starts here */

/* Virtio ring flags */
#define VIRTQ_DESC_F_NEXT           1   /* Buffer continues via next field */
#define VIRTQ_DESC_F_WRITE          2   /* Buffer is write-only (for device) */
#define VIRTQ_DESC_F_INDIRECT       4   /* Buffer contains indirect desc table */

#define VIRTQ_AVAIL_F_NO_INTERRUPT  1

#define VIRTQ_USED_F_NO_NOTIFY      1

/* Virtio queue descriptor */
typedef struct virtq_desc {
    uint64_t addr;      /* Physical address of buffer */
    uint32_t len;       /* Length of buffer */
    uint16_t flags;     /* VIRTQ_DESC_F_* */
    uint16_t next;      /* Next descriptor if flags & NEXT */
} __attribute__((packed)) virtq_desc_t;

/* Virtio available ring */
typedef struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];    /* Queue size entries */
} __attribute__((packed)) virtq_avail_t;

/* Virtio used element */
typedef struct virtq_used_elem {
    uint32_t id;        /* Index of start of used descriptor chain */
    uint32_t len;       /* Total length written to descriptor chain */
} __attribute__((packed)) virtq_used_elem_t;

/* Virtio used ring */
typedef struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];   /* Queue size entries */
} __attribute__((packed)) virtq_used_t;

/* Virtio queue structure */
typedef struct virtq {
    uint16_t size;              /* Queue size (power of 2) */
    uint16_t free_head;         /* Head of free descriptor list */
    uint16_t num_free;          /* Number of free descriptors */
    uint16_t last_used_idx;     /* Last processed used ring idx */

    virtq_desc_t *desc;         /* Descriptor table */
    virtq_avail_t *avail;       /* Available ring */
    virtq_used_t *used;         /* Used ring */

    void **desc_virt;           /* Virtual addresses for descriptors */
} virtq_t;

/* Calculate size needed for virtqueue */
static inline uint32_t virtq_size(uint16_t qsz) {
    return ((sizeof(virtq_desc_t) * qsz + sizeof(uint16_t) * (3 + qsz)
             + 4096 - 1) & ~(4096 - 1))
           + ((sizeof(uint16_t) * 3 + sizeof(virtq_used_elem_t) * qsz
               + 4096 - 1) & ~(4096 - 1));
}

/* Initialize a virtqueue */
int virtq_init(virtq_t *vq, uint16_t size, void *mem);

/* Allocate descriptor chain */
int virtq_alloc_desc(virtq_t *vq);

/* Free descriptor chain */
void virtq_free_desc(virtq_t *vq, int head);

/* Add buffer to queue */
int virtq_add_buf(virtq_t *vq, void *buf, uint32_t len, int write);

/* Get used buffer */
int virtq_get_buf(virtq_t *vq, uint32_t *len);

/* Check if there are used buffers */
bool virtq_has_used(virtq_t *vq);

#endif /* _VIRTIO_H */
