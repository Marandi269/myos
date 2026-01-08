/*
 * icmp.h - Internet Control Message Protocol
 */

#ifndef _ICMP_H
#define _ICMP_H

#include "types.h"
#include "net/netdev.h"

/* ICMP types */
#define ICMP_TYPE_ECHO_REPLY        0
#define ICMP_TYPE_DEST_UNREACHABLE  3
#define ICMP_TYPE_ECHO_REQUEST      8
#define ICMP_TYPE_TIME_EXCEEDED     11

/* ICMP codes for destination unreachable */
#define ICMP_CODE_NET_UNREACHABLE   0
#define ICMP_CODE_HOST_UNREACHABLE  1
#define ICMP_CODE_PROTO_UNREACHABLE 2
#define ICMP_CODE_PORT_UNREACHABLE  3

/* ICMP header */
typedef struct icmp_header {
    uint8_t type;           /* Message type */
    uint8_t code;           /* Message code */
    uint16_t checksum;      /* Checksum */
    union {
        struct {
            uint16_t id;        /* Identifier (for echo) */
            uint16_t sequence;  /* Sequence number (for echo) */
        } echo;
        uint32_t gateway;       /* Gateway address (for redirect) */
        struct {
            uint16_t unused;
            uint16_t mtu;       /* Next-hop MTU (for fragmentation needed) */
        } frag;
        uint32_t unused;
    } data;
} __attribute__((packed)) icmp_header_t;

/* Initialize ICMP */
void icmp_init(void);

/* Receive ICMP packet */
void icmp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len);

/* Send ICMP echo request (ping) */
int icmp_echo_request(netdev_t *dev, uint32_t dst_ip, uint16_t id, uint16_t seq);

/* Send ICMP destination unreachable */
int icmp_dest_unreachable(netdev_t *dev, uint32_t dst_ip, uint8_t code,
                          void *orig_packet, size_t orig_len);

#endif /* _ICMP_H */
