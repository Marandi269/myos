/*
 * ethernet.h - Ethernet Frame Processing
 */

#ifndef _ETHERNET_H
#define _ETHERNET_H

#include "types.h"
#include "net/netdev.h"

/* Ethernet frame constants */
#define ETH_ALEN        6       /* Ethernet address length */
#define ETH_HLEN        14      /* Ethernet header length */
#define ETH_MTU         1500    /* Maximum payload */
#define ETH_FRAME_MIN   60      /* Minimum frame size (without CRC) */
#define ETH_FRAME_MAX   1514    /* Maximum frame size (without CRC) */

/* EtherType values */
#define ETH_TYPE_IP     0x0800
#define ETH_TYPE_ARP    0x0806
#define ETH_TYPE_IPV6   0x86DD

/* Broadcast MAC address */
extern const uint8_t ETH_BROADCAST[6];

/* Ethernet header */
typedef struct eth_header {
    uint8_t dst[ETH_ALEN];      /* Destination MAC */
    uint8_t src[ETH_ALEN];      /* Source MAC */
    uint16_t type;              /* EtherType (big-endian) */
} __attribute__((packed)) eth_header_t;

/* Initialize ethernet layer */
void ethernet_init(void);

/* Receive ethernet frame (called by driver) */
void ethernet_receive(netdev_t *dev, void *data, size_t len);

/* Send ethernet frame */
int ethernet_send(netdev_t *dev, const uint8_t *dst_mac,
                  uint16_t ethertype, void *data, size_t len);

/* Get source MAC from a received frame */
static inline uint8_t *eth_src(eth_header_t *hdr) {
    return hdr->src;
}

/* Get destination MAC from a received frame */
static inline uint8_t *eth_dst(eth_header_t *hdr) {
    return hdr->dst;
}

/* Compare MAC addresses */
static inline bool eth_addr_equal(const uint8_t *a, const uint8_t *b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] &&
           a[3] == b[3] && a[4] == b[4] && a[5] == b[5];
}

/* Check if MAC is broadcast */
static inline bool eth_is_broadcast(const uint8_t *addr) {
    return addr[0] == 0xFF && addr[1] == 0xFF && addr[2] == 0xFF &&
           addr[3] == 0xFF && addr[4] == 0xFF && addr[5] == 0xFF;
}

/* Check if MAC is multicast */
static inline bool eth_is_multicast(const uint8_t *addr) {
    return (addr[0] & 0x01) != 0;
}

#endif /* _ETHERNET_H */
