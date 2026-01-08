/*
 * udp.h - User Datagram Protocol
 */

#ifndef _UDP_H
#define _UDP_H

#include "types.h"
#include "net/netdev.h"

/* UDP header */
typedef struct udp_header {
    uint16_t src_port;      /* Source port */
    uint16_t dst_port;      /* Destination port */
    uint16_t length;        /* Length (header + data) */
    uint16_t checksum;      /* Checksum (optional in IPv4) */
} __attribute__((packed)) udp_header_t;

/* UDP pseudo-header for checksum calculation */
typedef struct udp_pseudo_header {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_length;
} __attribute__((packed)) udp_pseudo_header_t;

/* Maximum UDP sockets */
#define UDP_MAX_SOCKETS     16

/* UDP receive callback type */
typedef void (*udp_recv_callback_t)(uint32_t src_ip, uint16_t src_port,
                                     uint16_t dst_port,
                                     void *data, size_t len);

/* Initialize UDP */
void udp_init(void);

/* Receive UDP packet */
void udp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len);

/* Send UDP packet */
int udp_send(netdev_t *dev, uint32_t dst_ip, uint16_t src_port,
             uint16_t dst_port, void *data, size_t len);

/* Bind to a port */
int udp_bind(uint16_t port, udp_recv_callback_t callback);

/* Unbind from a port */
void udp_unbind(uint16_t port);

/* Allocate ephemeral port */
uint16_t udp_alloc_port(void);

#endif /* _UDP_H */
