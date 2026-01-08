/*
 * dhcp.c - DHCP Client Implementation
 */

#include "net/dhcp.h"
#include "net/ethernet.h"
#include "net/ip.h"
#include "net/udp.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/pit.h"

/* offsetof macro */
#define offsetof(type, member) ((size_t)(&((type *)0)->member))

/* Byte swap */
static inline uint16_t bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static inline uint32_t bswap32(uint32_t x) {
    return ((x >> 24) & 0x000000FF) |
           ((x >> 8)  & 0x0000FF00) |
           ((x << 8)  & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

#define ntohs(x) bswap16(x)
#define htons(x) bswap16(x)
#define ntohl(x) bswap32(x)
#define htonl(x) bswap32(x)

/* DHCP state */
typedef enum {
    DHCP_STATE_INIT,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING
} dhcp_state_t;

/* Current state */
static dhcp_state_t dhcp_state = DHCP_STATE_INIT;
static dhcp_lease_t current_lease;
static uint32_t dhcp_xid = 0;
static netdev_t *dhcp_dev = NULL;

/* Add DHCP option */
static uint8_t *dhcp_add_option(uint8_t *options, uint8_t code,
                                 uint8_t len, void *data) {
    *options++ = code;
    *options++ = len;
    memcpy(options, data, len);
    return options + len;
}

/* Parse DHCP options */
static void dhcp_parse_options(uint8_t *options, size_t len,
                                uint8_t *msg_type, dhcp_lease_t *lease) {
    size_t i = 0;

    while (i < len) {
        uint8_t code = options[i++];

        if (code == DHCP_OPT_PAD) {
            continue;
        }
        if (code == DHCP_OPT_END) {
            break;
        }

        if (i >= len) break;
        uint8_t opt_len = options[i++];

        if (i + opt_len > len) break;

        switch (code) {
            case DHCP_OPT_MSG_TYPE:
                if (opt_len >= 1) {
                    *msg_type = options[i];
                }
                break;

            case DHCP_OPT_SUBNET_MASK:
                if (opt_len >= 4) {
                    memcpy(&lease->netmask, &options[i], 4);
                }
                break;

            case DHCP_OPT_ROUTER:
                if (opt_len >= 4) {
                    memcpy(&lease->gateway, &options[i], 4);
                }
                break;

            case DHCP_OPT_DNS:
                if (opt_len >= 4) {
                    memcpy(&lease->dns, &options[i], 4);
                }
                break;

            case DHCP_OPT_LEASE_TIME:
                if (opt_len >= 4) {
                    memcpy(&lease->lease_time, &options[i], 4);
                    lease->lease_time = ntohl(lease->lease_time);
                }
                break;

            case DHCP_OPT_SERVER_ID:
                if (opt_len >= 4) {
                    memcpy(&lease->server_ip, &options[i], 4);
                }
                break;
        }

        i += opt_len;
    }
}

/* Send DHCP packet */
static int dhcp_send_packet(netdev_t *dev, dhcp_header_t *dhcp, size_t len) {
    /* Build UDP packet */
    uint8_t packet[sizeof(dhcp_header_t) + 64];

    /* UDP header (we need raw packet since we have no IP yet) */
    typedef struct {
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t length;
        uint16_t checksum;
    } __attribute__((packed)) udp_hdr_t;

    /* IP header */
    typedef struct {
        uint8_t version_ihl;
        uint8_t tos;
        uint16_t total_length;
        uint16_t id;
        uint16_t flags_fragment;
        uint8_t ttl;
        uint8_t protocol;
        uint16_t checksum;
        uint32_t src_ip;
        uint32_t dst_ip;
    } __attribute__((packed)) ip_hdr_t;

    size_t udp_len = sizeof(udp_hdr_t) + len;
    size_t ip_len = sizeof(ip_hdr_t) + udp_len;

    ip_hdr_t *ip = (ip_hdr_t *)packet;
    udp_hdr_t *udp = (udp_hdr_t *)(packet + sizeof(ip_hdr_t));
    void *payload = packet + sizeof(ip_hdr_t) + sizeof(udp_hdr_t);

    /* Fill IP header */
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(ip_len);
    ip->id = htons(1);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->checksum = 0;
    ip->src_ip = 0;  /* 0.0.0.0 */
    ip->dst_ip = 0xFFFFFFFF;  /* 255.255.255.255 */

    /* Calculate IP checksum */
    ip->checksum = ip_checksum(ip, sizeof(ip_hdr_t));

    /* Fill UDP header */
    udp->src_port = htons(DHCP_CLIENT_PORT);
    udp->dst_port = htons(DHCP_SERVER_PORT);
    udp->length = htons(udp_len);
    udp->checksum = 0;  /* Optional for IPv4 */

    /* Copy DHCP payload */
    memcpy(payload, dhcp, len);

    /* Send as broadcast ethernet frame */
    return ethernet_send(dev, ETH_BROADCAST, ETH_TYPE_IP, packet, ip_len);
}

/* Build and send DHCP Discover */
static int dhcp_send_discover(netdev_t *dev) {
    dhcp_header_t dhcp;
    memset(&dhcp, 0, sizeof(dhcp));

    dhcp.op = 1;  /* Request */
    dhcp.htype = 1;  /* Ethernet */
    dhcp.hlen = 6;
    dhcp.hops = 0;
    dhcp.xid = htonl(dhcp_xid);
    dhcp.secs = 0;
    dhcp.flags = htons(0x8000);  /* Broadcast flag */
    dhcp.ciaddr = 0;
    dhcp.yiaddr = 0;
    dhcp.siaddr = 0;
    dhcp.giaddr = 0;
    memcpy(dhcp.chaddr, dev->mac, 6);
    dhcp.magic = htonl(DHCP_MAGIC_COOKIE);

    /* Add options */
    uint8_t *opt = dhcp.options;
    uint8_t msg_type = DHCP_DISCOVER;
    opt = dhcp_add_option(opt, DHCP_OPT_MSG_TYPE, 1, &msg_type);

    /* Parameter request list */
    uint8_t param_list[] = {
        DHCP_OPT_SUBNET_MASK,
        DHCP_OPT_ROUTER,
        DHCP_OPT_DNS
    };
    opt = dhcp_add_option(opt, DHCP_OPT_PARAM_LIST, sizeof(param_list), param_list);

    *opt++ = DHCP_OPT_END;

    size_t len = (opt - (uint8_t *)&dhcp);

    kprintf("[DHCP] Sending DISCOVER\n");
    dhcp_state = DHCP_STATE_SELECTING;

    return dhcp_send_packet(dev, &dhcp, len);
}

/* Build and send DHCP Request */
static int dhcp_send_request(netdev_t *dev, uint32_t offered_ip,
                              uint32_t server_ip) {
    dhcp_header_t dhcp;
    memset(&dhcp, 0, sizeof(dhcp));

    dhcp.op = 1;
    dhcp.htype = 1;
    dhcp.hlen = 6;
    dhcp.hops = 0;
    dhcp.xid = htonl(dhcp_xid);
    dhcp.secs = 0;
    dhcp.flags = htons(0x8000);
    dhcp.ciaddr = 0;
    dhcp.yiaddr = 0;
    dhcp.siaddr = 0;
    dhcp.giaddr = 0;
    memcpy(dhcp.chaddr, dev->mac, 6);
    dhcp.magic = htonl(DHCP_MAGIC_COOKIE);

    /* Add options */
    uint8_t *opt = dhcp.options;
    uint8_t msg_type = DHCP_REQUEST;
    opt = dhcp_add_option(opt, DHCP_OPT_MSG_TYPE, 1, &msg_type);

    /* Requested IP */
    opt = dhcp_add_option(opt, DHCP_OPT_REQUESTED_IP, 4, &offered_ip);

    /* Server identifier */
    opt = dhcp_add_option(opt, DHCP_OPT_SERVER_ID, 4, &server_ip);

    *opt++ = DHCP_OPT_END;

    size_t len = (opt - (uint8_t *)&dhcp);

    char ip_str[16];
    ip_format(offered_ip, ip_str);
    kprintf("[DHCP] Sending REQUEST for %s\n", ip_str);
    dhcp_state = DHCP_STATE_REQUESTING;

    return dhcp_send_packet(dev, &dhcp, len);
}

/* Handle received DHCP packet */
static void dhcp_receive_callback(uint32_t src_ip, uint16_t src_port,
                                   uint16_t dst_port,
                                   void *data, size_t len) {
    (void)src_ip;
    (void)src_port;
    (void)dst_port;

    if (len < sizeof(dhcp_header_t) - 308) {  /* Minimum DHCP size */
        return;
    }

    dhcp_header_t *dhcp = (dhcp_header_t *)data;

    /* Verify this is a reply for us */
    if (dhcp->op != 2) {  /* Not a reply */
        return;
    }

    if (ntohl(dhcp->xid) != dhcp_xid) {  /* Wrong transaction */
        return;
    }

    if (ntohl(dhcp->magic) != DHCP_MAGIC_COOKIE) {
        return;
    }

    /* Parse options */
    uint8_t msg_type = 0;
    dhcp_lease_t lease;
    memset(&lease, 0, sizeof(lease));
    lease.client_ip = dhcp->yiaddr;

    size_t opt_len = len - (offsetof(dhcp_header_t, options));
    dhcp_parse_options(dhcp->options, opt_len, &msg_type, &lease);

    char ip_str[16];

    switch (msg_type) {
        case DHCP_OFFER:
            if (dhcp_state == DHCP_STATE_SELECTING) {
                ip_format(lease.client_ip, ip_str);
                kprintf("[DHCP] Received OFFER: %s\n", ip_str);

                /* Send request */
                dhcp_send_request(dhcp_dev, lease.client_ip, lease.server_ip);
            }
            break;

        case DHCP_ACK:
            if (dhcp_state == DHCP_STATE_REQUESTING) {
                memcpy(&current_lease, &lease, sizeof(lease));
                current_lease.valid = true;
                dhcp_state = DHCP_STATE_BOUND;

                ip_format(lease.client_ip, ip_str);
                kprintf("[DHCP] Received ACK: %s\n", ip_str);

                /* Configure IP stack */
                ip_configure(lease.client_ip, lease.netmask, lease.gateway);
            }
            break;

        case DHCP_NAK:
            kprintf("[DHCP] Received NAK, restarting\n");
            dhcp_state = DHCP_STATE_INIT;
            /* Retry after delay */
            break;
    }
}

/* Initialize DHCP client */
void dhcp_init(void) {
    memset(&current_lease, 0, sizeof(current_lease));
    dhcp_state = DHCP_STATE_INIT;
    dhcp_xid = 0x12345678;  /* TODO: Use random number */

    /* Bind to DHCP client port */
    udp_bind(DHCP_CLIENT_PORT, dhcp_receive_callback);

    kprintf("[DHCP] Initialized\n");
}

/* Start DHCP discovery */
int dhcp_discover(netdev_t *dev) {
    if (!dev) {
        return -1;
    }

    dhcp_dev = dev;
    dhcp_xid++;

    return dhcp_send_discover(dev);
}

/* Get current lease */
dhcp_lease_t *dhcp_get_lease(void) {
    if (current_lease.valid) {
        return &current_lease;
    }
    return NULL;
}

/* Release current lease */
void dhcp_release(void) {
    if (!current_lease.valid) {
        return;
    }

    /* TODO: Send DHCP RELEASE */
    current_lease.valid = false;
    dhcp_state = DHCP_STATE_INIT;
}

/* Check if DHCP is configured */
bool dhcp_is_configured(void) {
    return dhcp_state == DHCP_STATE_BOUND;
}
