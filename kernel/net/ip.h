/*
 * ip.h - Internet Protocol (IPv4)
 */

#ifndef _IP_H
#define _IP_H

#include "types.h"
#include "net/netdev.h"

/* IP protocol numbers */
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

/* IP header flags */
#define IP_FLAG_DF      0x4000  /* Don't Fragment */
#define IP_FLAG_MF      0x2000  /* More Fragments */

/* Default TTL */
#define IP_DEFAULT_TTL  64

/* IP header */
typedef struct ip_header {
    uint8_t  version_ihl;   /* Version (4 bits) + IHL (4 bits) */
    uint8_t  tos;           /* Type of service */
    uint16_t total_length;  /* Total length (header + data) */
    uint16_t id;            /* Identification */
    uint16_t flags_fragment; /* Flags (3 bits) + Fragment offset (13 bits) */
    uint8_t  ttl;           /* Time to live */
    uint8_t  protocol;      /* Protocol (ICMP=1, TCP=6, UDP=17) */
    uint16_t checksum;      /* Header checksum */
    uint32_t src_ip;        /* Source IP address */
    uint32_t dst_ip;        /* Destination IP address */
} __attribute__((packed)) ip_header_t;

/* IP configuration */
typedef struct ip_config {
    uint32_t addr;          /* Our IP address */
    uint32_t netmask;       /* Subnet mask */
    uint32_t gateway;       /* Default gateway */
    uint32_t dns;           /* DNS server */
    bool configured;        /* Is IP configured? */
} ip_config_t;

/* Initialize IP layer */
void ip_init(void);

/* Configure IP address */
void ip_configure(uint32_t addr, uint32_t netmask, uint32_t gateway);

/* Get our IP address */
uint32_t ip_get_addr(void);

/* Get subnet mask */
uint32_t ip_get_netmask(void);

/* Get gateway */
uint32_t ip_get_gateway(void);

/* Receive IP packet */
void ip_receive(netdev_t *dev, void *data, size_t len);

/* Send IP packet */
int ip_send(netdev_t *dev, uint32_t dst_ip, uint8_t protocol,
            void *data, size_t len);

/* Calculate IP checksum */
uint16_t ip_checksum(void *data, size_t len);

/* Parse IP address from string */
uint32_t ip_parse(const char *str);

/* Format IP address to string */
void ip_format(uint32_t ip, char *buf);

/* Byte order conversion macros */
#define IP_MAKE(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
     ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* Check if IP is in same subnet */
static inline bool ip_is_local(uint32_t ip, uint32_t our_ip, uint32_t netmask) {
    return (ip & netmask) == (our_ip & netmask);
}

#endif /* _IP_H */
