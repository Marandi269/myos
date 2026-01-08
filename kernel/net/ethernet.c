/*
 * ethernet.c - Ethernet Frame Processing
 */

#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ip.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Broadcast MAC address */
const uint8_t ETH_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Byte swap for 16-bit values */
static inline uint16_t bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

/* Network to host byte order (16-bit) */
static inline uint16_t ntohs(uint16_t x) {
    return bswap16(x);
}

/* Host to network byte order (16-bit) */
static inline uint16_t htons(uint16_t x) {
    return bswap16(x);
}

/* Initialize ethernet layer */
void ethernet_init(void) {
    kprintf("[ethernet] Initialized\n");
}

/* Receive ethernet frame */
void ethernet_receive(netdev_t *dev, void *data, size_t len) {
    if (!data || len < ETH_HLEN) {
        return;
    }

    eth_header_t *hdr = (eth_header_t *)data;
    uint16_t type = ntohs(hdr->type);
    void *payload = (uint8_t *)data + ETH_HLEN;
    size_t payload_len = len - ETH_HLEN;

    /* Check if frame is for us */
    if (!eth_is_broadcast(hdr->dst) &&
        !eth_is_multicast(hdr->dst) &&
        !eth_addr_equal(hdr->dst, dev->mac)) {
        /* Not for us */
        return;
    }

    /* Dispatch based on EtherType */
    switch (type) {
        case ETH_TYPE_ARP:
            arp_receive(dev, payload, payload_len);
            break;

        case ETH_TYPE_IP:
            ip_receive(dev, payload, payload_len);
            break;

        case ETH_TYPE_IPV6:
            /* IPv6 not supported yet */
            break;

        default:
            /* Unknown protocol */
            break;
    }
}

/* Send ethernet frame */
int ethernet_send(netdev_t *dev, const uint8_t *dst_mac,
                  uint16_t ethertype, void *data, size_t len) {
    if (!dev || !dst_mac || !data) {
        return -1;
    }

    if (len > ETH_MTU) {
        kprintf("[ethernet] Payload too large: %d\n", (int)len);
        return -1;
    }

    /* Build frame buffer */
    uint8_t frame[ETH_FRAME_MAX];
    eth_header_t *hdr = (eth_header_t *)frame;

    /* Fill header */
    memcpy(hdr->dst, dst_mac, ETH_ALEN);
    memcpy(hdr->src, dev->mac, ETH_ALEN);
    hdr->type = htons(ethertype);

    /* Copy payload */
    memcpy(frame + ETH_HLEN, data, len);

    /* Pad to minimum frame size if needed */
    size_t frame_len = ETH_HLEN + len;
    if (frame_len < ETH_FRAME_MIN) {
        memset(frame + frame_len, 0, ETH_FRAME_MIN - frame_len);
        frame_len = ETH_FRAME_MIN;
    }

    /* Send via device */
    return netdev_send(dev, frame, frame_len);
}
