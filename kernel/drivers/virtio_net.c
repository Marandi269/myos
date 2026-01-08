/*
 * virtio_net.c - Virtio Network Device Driver
 */

#include "drivers/virtio_net.h"
#include "drivers/pci.h"
#include "drivers/virtio.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "net/ethernet.h"

/* I/O port access */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Global virtio-net device */
static virtio_net_dev_t vnet_dev;
static bool vnet_initialized = false;

/* Receive buffer tracking */
static uint8_t *rx_packet_bufs[VIRTIO_NET_QUEUE_SIZE];
static int rx_next_buf = 0;

/* Transmit buffer tracking */
static uint8_t *tx_packet_bufs[VIRTIO_NET_QUEUE_SIZE];
static int tx_next_buf = 0;

/* Read virtio-net config space */
static uint8_t vnet_read_config8(virtio_net_dev_t *dev, int offset) {
    return inb(dev->io_base + VIRTIO_PCI_CONFIG + offset);
}

/* Netdev send callback */
static int vnet_netdev_send(netdev_t *dev, void *data, size_t len) {
    (void)dev;
    return virtio_net_send(data, len);
}

/* Setup receive buffers */
static void vnet_setup_rx_bufs(virtio_net_dev_t *dev) {
    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE / 2; i++) {
        /* Allocate buffer for virtio header + ethernet frame */
        uint8_t *buf = (uint8_t *)pmm_alloc_page();
        if (!buf) {
            kprintf("[virtio-net] Failed to allocate RX buffer %d\n", i);
            break;
        }

        rx_packet_bufs[i] = buf;

        /* Add to receive queue */
        int desc = virtq_add_buf(&dev->rx_vq, buf, VIRTIO_NET_RX_BUF_SIZE, 1);
        if (desc < 0) {
            kprintf("[virtio-net] Failed to add RX buffer to queue\n");
            break;
        }
    }

    /* Notify device about available buffers */
    outw(dev->io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);  /* Queue 0 = RX */
}

/* Initialize virtio-net driver */
int virtio_net_init(void) {
    pci_device_t *pci_dev;
    uint32_t features;

    kprintf("[virtio-net] Initializing...\n");

    /* Find virtio-net PCI device */
    pci_dev = pci_find_device(PCI_VENDOR_REDHAT, PCI_DEVICE_VIRTIO_NET);
    if (!pci_dev) {
        kprintf("[virtio-net] No device found\n");
        return -1;
    }

    kprintf("[virtio-net] Found device at %02x:%02x.%d\n",
            pci_dev->bus, pci_dev->slot, pci_dev->func);

    /* Enable bus mastering */
    pci_enable_bus_master(pci_dev);

    /* Get I/O base from BAR0 */
    vnet_dev.io_base = pci_get_bar(pci_dev, 0);
    vnet_dev.irq = pci_dev->irq;

    kprintf("[virtio-net] I/O base: 0x%x, IRQ: %d\n",
            vnet_dev.io_base, vnet_dev.irq);

    /* Reset device */
    outb(vnet_dev.io_base + VIRTIO_PCI_STATUS, 0);

    /* Acknowledge device */
    outb(vnet_dev.io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    /* Driver loaded */
    outb(vnet_dev.io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Read offered features */
    features = inl(vnet_dev.io_base + VIRTIO_PCI_HOST_FEATURES);
    kprintf("[virtio-net] Host features: 0x%x\n", features);

    /* Accept features we support */
    vnet_dev.features = features & VIRTIO_NET_F_MAC;
    outl(vnet_dev.io_base + VIRTIO_PCI_GUEST_FEATURES, vnet_dev.features);

    /* Read MAC address */
    for (int i = 0; i < 6; i++) {
        vnet_dev.mac[i] = vnet_read_config8(&vnet_dev, i);
    }

    kprintf("[virtio-net] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            vnet_dev.mac[0], vnet_dev.mac[1], vnet_dev.mac[2],
            vnet_dev.mac[3], vnet_dev.mac[4], vnet_dev.mac[5]);

    /* Setup receive queue (queue 0) */
    outw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_SEL, 0);
    uint16_t rx_queue_size = inw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_SIZE);
    kprintf("[virtio-net] RX queue size: %d\n", rx_queue_size);

    if (rx_queue_size == 0 || rx_queue_size > VIRTIO_NET_QUEUE_SIZE) {
        rx_queue_size = VIRTIO_NET_QUEUE_SIZE;
    }

    /* Allocate memory for RX virtqueue */
    uint32_t rx_vq_size = virtq_size(rx_queue_size);
    void *rx_vq_mem = pmm_alloc_page();
    if (!rx_vq_mem) {
        kprintf("[virtio-net] Failed to allocate RX queue memory\n");
        return -1;
    }
    memset(rx_vq_mem, 0, PAGE_SIZE);

    /* Allocate more pages if needed */
    if (rx_vq_size > PAGE_SIZE) {
        for (uint32_t i = PAGE_SIZE; i < rx_vq_size; i += PAGE_SIZE) {
            void *page = pmm_alloc_page();
            if (!page) {
                kprintf("[virtio-net] Failed to allocate RX queue memory\n");
                return -1;
            }
            memset(page, 0, PAGE_SIZE);
        }
    }

    if (virtq_init(&vnet_dev.rx_vq, rx_queue_size, rx_vq_mem) < 0) {
        kprintf("[virtio-net] Failed to init RX queue\n");
        return -1;
    }

    /* Tell device the queue address */
    uint32_t rx_pfn = (uint64_t)rx_vq_mem / PAGE_SIZE;
    outl(vnet_dev.io_base + VIRTIO_PCI_QUEUE_PFN, rx_pfn);

    /* Setup transmit queue (queue 1) */
    outw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_SEL, 1);
    uint16_t tx_queue_size = inw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_SIZE);
    kprintf("[virtio-net] TX queue size: %d\n", tx_queue_size);

    if (tx_queue_size == 0 || tx_queue_size > VIRTIO_NET_QUEUE_SIZE) {
        tx_queue_size = VIRTIO_NET_QUEUE_SIZE;
    }

    /* Allocate memory for TX virtqueue */
    uint32_t tx_vq_size = virtq_size(tx_queue_size);
    void *tx_vq_mem = pmm_alloc_page();
    if (!tx_vq_mem) {
        kprintf("[virtio-net] Failed to allocate TX queue memory\n");
        return -1;
    }
    memset(tx_vq_mem, 0, PAGE_SIZE);

    /* Allocate more pages if needed */
    if (tx_vq_size > PAGE_SIZE) {
        for (uint32_t i = PAGE_SIZE; i < tx_vq_size; i += PAGE_SIZE) {
            void *page = pmm_alloc_page();
            if (!page) {
                kprintf("[virtio-net] Failed to allocate TX queue memory\n");
                return -1;
            }
            memset(page, 0, PAGE_SIZE);
        }
    }

    if (virtq_init(&vnet_dev.tx_vq, tx_queue_size, tx_vq_mem) < 0) {
        kprintf("[virtio-net] Failed to init TX queue\n");
        return -1;
    }

    /* Tell device the queue address */
    uint32_t tx_pfn = (uint64_t)tx_vq_mem / PAGE_SIZE;
    outl(vnet_dev.io_base + VIRTIO_PCI_QUEUE_PFN, tx_pfn);

    /* Allocate TX buffers */
    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE / 2; i++) {
        tx_packet_bufs[i] = (uint8_t *)pmm_alloc_page();
        if (!tx_packet_bufs[i]) {
            kprintf("[virtio-net] Failed to allocate TX buffer\n");
            break;
        }
    }

    /* Device ready */
    outb(vnet_dev.io_base + VIRTIO_PCI_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    /* Setup netdev interface */
    memcpy(vnet_dev.netdev.name, "eth0", 5);
    memcpy(vnet_dev.netdev.mac, vnet_dev.mac, 6);
    vnet_dev.netdev.send = vnet_netdev_send;
    vnet_dev.netdev.receive = NULL;  /* Set by ethernet layer */
    vnet_dev.netdev.priv = &vnet_dev;
    vnet_dev.netdev.mtu = 1500;

    /* Setup receive buffers */
    vnet_setup_rx_bufs(&vnet_dev);

    vnet_initialized = true;

    kprintf("[virtio-net] Initialized\n");
    return 0;
}

/* Get virtio-net device */
virtio_net_dev_t *virtio_net_get_dev(void) {
    if (!vnet_initialized) {
        return NULL;
    }
    return &vnet_dev;
}

/* Send packet */
int virtio_net_send(void *data, size_t len) {
    if (!vnet_initialized || !data || len == 0) {
        return -1;
    }

    if (len > VIRTIO_NET_TX_BUF_SIZE - sizeof(virtio_net_hdr_t)) {
        kprintf("[virtio-net] Packet too large: %d\n", (int)len);
        return -1;
    }

    /* Get a TX buffer */
    uint8_t *buf = tx_packet_bufs[tx_next_buf % (VIRTIO_NET_QUEUE_SIZE / 2)];
    if (!buf) {
        return -1;
    }
    tx_next_buf++;

    /* Prepare virtio-net header */
    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)buf;
    memset(hdr, 0, sizeof(*hdr));

    /* Copy packet data after header */
    memcpy(buf + sizeof(virtio_net_hdr_t), data, len);

    /* Add to TX queue */
    int desc = virtq_add_buf(&vnet_dev.tx_vq, buf,
                             sizeof(virtio_net_hdr_t) + len, 0);
    if (desc < 0) {
        kprintf("[virtio-net] TX queue full\n");
        return -1;
    }

    /* Notify device */
    outw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_NOTIFY, 1);  /* Queue 1 = TX */

    return len;
}

/* Poll for received packets */
void virtio_net_poll(void) {
    if (!vnet_initialized) {
        return;
    }

    uint32_t len;

    /* Process received packets */
    while (virtq_has_used(&vnet_dev.rx_vq)) {
        int desc = virtq_get_buf(&vnet_dev.rx_vq, &len);
        if (desc < 0) {
            break;
        }

        /* Get buffer virtual address */
        uint8_t *buf = vnet_dev.rx_vq.desc_virt[desc];
        if (!buf) {
            virtq_free_desc(&vnet_dev.rx_vq, desc);
            continue;
        }

        /* Skip virtio header */
        virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)buf;
        uint8_t *packet = buf + sizeof(virtio_net_hdr_t);
        size_t pkt_len = len - sizeof(virtio_net_hdr_t);

        (void)hdr;

        /* Pass to ethernet layer */
        if (pkt_len > 0) {
            ethernet_receive(&vnet_dev.netdev, packet, pkt_len);
        }

        /* Requeue buffer */
        vnet_dev.rx_vq.desc[desc].addr = (uint64_t)buf;
        vnet_dev.rx_vq.desc[desc].len = VIRTIO_NET_RX_BUF_SIZE;
        vnet_dev.rx_vq.desc[desc].flags = VIRTQ_DESC_F_WRITE;

        uint16_t avail_idx = vnet_dev.rx_vq.avail->idx % vnet_dev.rx_vq.size;
        vnet_dev.rx_vq.avail->ring[avail_idx] = desc;
        __asm__ volatile ("mfence" ::: "memory");
        vnet_dev.rx_vq.avail->idx++;

        /* Notify device */
        outw(vnet_dev.io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);
    }

    /* Clean up completed TX buffers */
    while (virtq_has_used(&vnet_dev.tx_vq)) {
        int desc = virtq_get_buf(&vnet_dev.tx_vq, &len);
        if (desc >= 0) {
            virtq_free_desc(&vnet_dev.tx_vq, desc);
        }
    }
}

/* IRQ handler */
void virtio_net_irq_handler(void) {
    if (!vnet_initialized) {
        return;
    }

    /* Read ISR to acknowledge interrupt */
    inb(vnet_dev.io_base + VIRTIO_PCI_ISR);

    /* Poll for packets */
    virtio_net_poll();
}
