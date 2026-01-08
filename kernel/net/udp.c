/*
 * udp.c - User Datagram Protocol
 */

#include "net/udp.h"
#include "net/ip.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Byte swap */
static inline uint16_t bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

#define ntohs(x) bswap16(x)
#define htons(x) bswap16(x)

/* UDP socket binding */
typedef struct udp_binding {
    uint16_t port;
    udp_recv_callback_t callback;
    bool active;
} udp_binding_t;

static udp_binding_t udp_bindings[UDP_MAX_SOCKETS];
static uint16_t next_ephemeral_port = 49152;

/* Initialize UDP */
void udp_init(void) {
    memset(udp_bindings, 0, sizeof(udp_bindings));
    kprintf("[UDP] Initialized\n");
}

/* Calculate UDP checksum */
static uint16_t udp_checksum(uint32_t src_ip, uint32_t dst_ip,
                             void *udp_packet, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr;

    /* Add pseudo-header */
    ptr = (uint16_t *)&src_ip;
    sum += ptr[0];
    sum += ptr[1];

    ptr = (uint16_t *)&dst_ip;
    sum += ptr[0];
    sum += ptr[1];

    sum += htons(IP_PROTO_UDP);
    sum += htons(len);

    /* Add UDP packet */
    ptr = (uint16_t *)udp_packet;
    size_t remaining = len;

    while (remaining > 1) {
        sum += *ptr++;
        remaining -= 2;
    }

    if (remaining == 1) {
        sum += *(uint8_t *)ptr;
    }

    /* Fold to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

/* Bind to a port */
int udp_bind(uint16_t port, udp_recv_callback_t callback) {
    /* Check if already bound */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_bindings[i].active && udp_bindings[i].port == port) {
            return -1;  /* Already bound */
        }
    }

    /* Find free slot */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!udp_bindings[i].active) {
            udp_bindings[i].port = port;
            udp_bindings[i].callback = callback;
            udp_bindings[i].active = true;
            kprintf("[UDP] Bound to port %d\n", port);
            return 0;
        }
    }

    return -1;  /* No free slots */
}

/* Unbind from a port */
void udp_unbind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_bindings[i].active && udp_bindings[i].port == port) {
            udp_bindings[i].active = false;
            kprintf("[UDP] Unbound port %d\n", port);
            return;
        }
    }
}

/* Allocate ephemeral port */
uint16_t udp_alloc_port(void) {
    uint16_t port = next_ephemeral_port++;
    if (next_ephemeral_port == 0) {
        next_ephemeral_port = 49152;
    }
    return port;
}

/* Receive UDP packet */
void udp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len) {
    (void)dev;

    if (len < sizeof(udp_header_t)) {
        return;
    }

    udp_header_t *hdr = (udp_header_t *)data;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t udp_len = ntohs(hdr->length);

    if (udp_len > len || udp_len < sizeof(udp_header_t)) {
        return;
    }

    /* Verify checksum if present */
    if (hdr->checksum != 0) {
        uint16_t saved_checksum = hdr->checksum;
        hdr->checksum = 0;
        uint16_t calc_checksum = udp_checksum(src_ip, ip_get_addr(), data, udp_len);
        hdr->checksum = saved_checksum;

        if (calc_checksum != saved_checksum && saved_checksum != 0xFFFF) {
            kprintf("[UDP] Bad checksum\n");
            return;
        }
    }

    void *payload = (uint8_t *)data + sizeof(udp_header_t);
    size_t payload_len = udp_len - sizeof(udp_header_t);

    /* Find bound socket */
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (udp_bindings[i].active && udp_bindings[i].port == dst_port) {
            if (udp_bindings[i].callback) {
                udp_bindings[i].callback(src_ip, src_port, dst_port,
                                         payload, payload_len);
            }
            return;
        }
    }

    /* No handler - silently drop (or send ICMP port unreachable) */
    char ip_str[16];
    ip_format(src_ip, ip_str);
    kprintf("[UDP] No handler for port %d (from %s:%d)\n",
            dst_port, ip_str, src_port);
}

/* Send UDP packet */
int udp_send(netdev_t *dev, uint32_t dst_ip, uint16_t src_port,
             uint16_t dst_port, void *data, size_t len) {
    if (!dev || !data) {
        return -1;
    }

    /* Build UDP packet */
    uint8_t packet[1472];  /* Max UDP payload in IPv4 without fragmentation */
    udp_header_t *hdr = (udp_header_t *)packet;

    size_t total_len = sizeof(udp_header_t) + len;
    if (total_len > sizeof(packet)) {
        return -1;
    }

    /* Fill header */
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length = htons(total_len);
    hdr->checksum = 0;

    /* Copy payload */
    memcpy(packet + sizeof(udp_header_t), data, len);

    /* Calculate checksum */
    hdr->checksum = udp_checksum(ip_get_addr(), dst_ip, packet, total_len);
    if (hdr->checksum == 0) {
        hdr->checksum = 0xFFFF;  /* 0 means no checksum, use 0xFFFF instead */
    }

    /* Send via IP */
    return ip_send(dev, dst_ip, IP_PROTO_UDP, packet, total_len);
}
