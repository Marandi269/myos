/*
 * dhcp.h - Dynamic Host Configuration Protocol Client
 */

#ifndef _DHCP_H
#define _DHCP_H

#include "types.h"
#include "net/netdev.h"

/* DHCP ports */
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

/* DHCP message types */
#define DHCP_DISCOVER       1
#define DHCP_OFFER          2
#define DHCP_REQUEST        3
#define DHCP_DECLINE        4
#define DHCP_ACK            5
#define DHCP_NAK            6
#define DHCP_RELEASE        7
#define DHCP_INFORM         8

/* DHCP options */
#define DHCP_OPT_PAD            0
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_DNS            6
#define DHCP_OPT_HOSTNAME       12
#define DHCP_OPT_DOMAIN         15
#define DHCP_OPT_BROADCAST      28
#define DHCP_OPT_REQUESTED_IP   50
#define DHCP_OPT_LEASE_TIME     51
#define DHCP_OPT_MSG_TYPE       53
#define DHCP_OPT_SERVER_ID      54
#define DHCP_OPT_PARAM_LIST     55
#define DHCP_OPT_MAX_SIZE       57
#define DHCP_OPT_CLIENT_ID      61
#define DHCP_OPT_END            255

/* DHCP magic cookie */
#define DHCP_MAGIC_COOKIE   0x63825363

/* DHCP header */
typedef struct dhcp_header {
    uint8_t op;             /* Message op code (1=request, 2=reply) */
    uint8_t htype;          /* Hardware address type (1=Ethernet) */
    uint8_t hlen;           /* Hardware address length (6 for Ethernet) */
    uint8_t hops;           /* Hops */
    uint32_t xid;           /* Transaction ID */
    uint16_t secs;          /* Seconds elapsed */
    uint16_t flags;         /* Flags */
    uint32_t ciaddr;        /* Client IP address */
    uint32_t yiaddr;        /* Your (client) IP address */
    uint32_t siaddr;        /* Server IP address */
    uint32_t giaddr;        /* Gateway IP address */
    uint8_t chaddr[16];     /* Client hardware address */
    uint8_t sname[64];      /* Server host name */
    uint8_t file[128];      /* Boot file name */
    uint32_t magic;         /* Magic cookie */
    uint8_t options[308];   /* Options (variable length) */
} __attribute__((packed)) dhcp_header_t;

/* DHCP lease information */
typedef struct dhcp_lease {
    uint32_t client_ip;
    uint32_t server_ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t lease_time;
    bool valid;
} dhcp_lease_t;

/* Initialize DHCP client */
void dhcp_init(void);

/* Start DHCP discovery */
int dhcp_discover(netdev_t *dev);

/* Get current lease */
dhcp_lease_t *dhcp_get_lease(void);

/* Release current lease */
void dhcp_release(void);

/* Check if DHCP is configured */
bool dhcp_is_configured(void);

#endif /* _DHCP_H */
