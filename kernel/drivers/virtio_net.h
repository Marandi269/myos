/*
 * virtio_net.h - Virtio Network Device Driver
 */

#ifndef _VIRTIO_NET_H
#define _VIRTIO_NET_H

#include "types.h"
#include "drivers/virtio.h"
#include "net/netdev.h"

/* Virtio-net feature bits */
#define VIRTIO_NET_F_CSUM           (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM     (1 << 1)
#define VIRTIO_NET_F_MAC            (1 << 5)
#define VIRTIO_NET_F_GSO            (1 << 6)
#define VIRTIO_NET_F_GUEST_TSO4     (1 << 7)
#define VIRTIO_NET_F_GUEST_TSO6     (1 << 8)
#define VIRTIO_NET_F_GUEST_ECN      (1 << 9)
#define VIRTIO_NET_F_GUEST_UFO      (1 << 10)
#define VIRTIO_NET_F_HOST_TSO4      (1 << 11)
#define VIRTIO_NET_F_HOST_TSO6      (1 << 12)
#define VIRTIO_NET_F_HOST_ECN       (1 << 13)
#define VIRTIO_NET_F_HOST_UFO       (1 << 14)
#define VIRTIO_NET_F_MRG_RXBUF      (1 << 15)
#define VIRTIO_NET_F_STATUS         (1 << 16)
#define VIRTIO_NET_F_CTRL_VQ        (1 << 17)
#define VIRTIO_NET_F_CTRL_RX        (1 << 18)
#define VIRTIO_NET_F_CTRL_VLAN      (1 << 19)
#define VIRTIO_NET_F_GUEST_ANNOUNCE (1 << 21)

/* Virtio-net header (prepended to packets) */
typedef struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed)) virtio_net_hdr_t;

/* GSO types */
#define VIRTIO_NET_HDR_GSO_NONE     0
#define VIRTIO_NET_HDR_GSO_TCPV4    1
#define VIRTIO_NET_HDR_GSO_UDP      3
#define VIRTIO_NET_HDR_GSO_TCPV6    4
#define VIRTIO_NET_HDR_GSO_ECN      0x80

/* Virtio-net device config (at VIRTIO_PCI_CONFIG offset) */
typedef struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
} __attribute__((packed)) virtio_net_config_t;

/* Virtio-net device structure */
typedef struct virtio_net_dev {
    uint16_t io_base;       /* I/O base address */
    uint8_t irq;            /* IRQ number */
    uint8_t mac[6];         /* MAC address */
    uint32_t features;      /* Negotiated features */

    virtq_t rx_vq;          /* Receive virtqueue */
    virtq_t tx_vq;          /* Transmit virtqueue */

    void *rx_bufs;          /* Receive buffers */
    void *tx_bufs;          /* Transmit buffers */

    netdev_t netdev;        /* Network device interface */
} virtio_net_dev_t;

/* Buffer sizes */
#define VIRTIO_NET_RX_BUF_SIZE  2048
#define VIRTIO_NET_TX_BUF_SIZE  2048
#define VIRTIO_NET_QUEUE_SIZE   256

/* Initialize virtio-net driver */
int virtio_net_init(void);

/* Get the virtio-net device */
virtio_net_dev_t *virtio_net_get_dev(void);

/* Send packet */
int virtio_net_send(void *data, size_t len);

/* Receive packet (called from interrupt or poll) */
int virtio_net_receive(void *buf, size_t buf_size);

/* IRQ handler */
void virtio_net_irq_handler(void);

/* Poll for received packets */
void virtio_net_poll(void);

#endif /* _VIRTIO_NET_H */
