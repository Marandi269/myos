/*
 * ip.c - Internet Protocol (IPv4)
 */

#include "net/ip.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Byte swap functions */
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

/* IP configuration */
static ip_config_t ip_config = {
    .addr = 0,
    .netmask = 0,
    .gateway = 0,
    .dns = 0,
    .configured = false
};

/* Packet ID counter */
static uint16_t ip_id = 0;

/* Initialize IP layer */
void ip_init(void) {
    /* Default configuration for QEMU user mode networking */
    ip_config.addr = IP_MAKE(10, 0, 2, 15);
    ip_config.netmask = IP_MAKE(255, 255, 255, 0);
    ip_config.gateway = IP_MAKE(10, 0, 2, 2);
    ip_config.dns = IP_MAKE(10, 0, 2, 3);
    ip_config.configured = true;

    char ip_str[16], mask_str[16], gw_str[16];
    ip_format(ip_config.addr, ip_str);
    ip_format(ip_config.netmask, mask_str);
    ip_format(ip_config.gateway, gw_str);

    kprintf("[IP] Configured: %s/%s gateway %s\n", ip_str, mask_str, gw_str);
}

/* Configure IP address */
void ip_configure(uint32_t addr, uint32_t netmask, uint32_t gateway) {
    ip_config.addr = addr;
    ip_config.netmask = netmask;
    ip_config.gateway = gateway;
    ip_config.configured = true;

    char ip_str[16], mask_str[16], gw_str[16];
    ip_format(addr, ip_str);
    ip_format(netmask, mask_str);
    ip_format(gateway, gw_str);

    kprintf("[IP] Configured: %s/%s gateway %s\n", ip_str, mask_str, gw_str);
}

/* Get our IP address */
uint32_t ip_get_addr(void) {
    return ip_config.addr;
}

/* Get subnet mask */
uint32_t ip_get_netmask(void) {
    return ip_config.netmask;
}

/* Get gateway */
uint32_t ip_get_gateway(void) {
    return ip_config.gateway;
}

/* Calculate IP checksum */
uint16_t ip_checksum(void *data, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    /* Add odd byte if present */
    if (len == 1) {
        sum += *(uint8_t *)ptr;
    }

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

/* Format IP address to string */
void ip_format(uint32_t ip, char *buf) {
    uint8_t *bytes = (uint8_t *)&ip;
    int pos = 0;

    for (int i = 0; i < 4; i++) {
        int val = bytes[i];
        if (val >= 100) {
            buf[pos++] = '0' + val / 100;
            val %= 100;
            buf[pos++] = '0' + val / 10;
            val %= 10;
        } else if (val >= 10) {
            buf[pos++] = '0' + val / 10;
            val %= 10;
        }
        buf[pos++] = '0' + val;
        if (i < 3) {
            buf[pos++] = '.';
        }
    }
    buf[pos] = '\0';
}

/* Parse IP address from string */
uint32_t ip_parse(const char *str) {
    uint8_t bytes[4] = {0};
    int idx = 0;

    while (*str && idx < 4) {
        if (*str >= '0' && *str <= '9') {
            bytes[idx] = bytes[idx] * 10 + (*str - '0');
        } else if (*str == '.') {
            idx++;
        }
        str++;
    }

    return IP_MAKE(bytes[0], bytes[1], bytes[2], bytes[3]);
}

/* Receive IP packet */
void ip_receive(netdev_t *dev, void *data, size_t len) {
    if (len < sizeof(ip_header_t)) {
        return;
    }

    ip_header_t *hdr = (ip_header_t *)data;

    /* Check version */
    uint8_t version = (hdr->version_ihl >> 4) & 0x0F;
    if (version != 4) {
        return;
    }

    /* Get header length */
    uint8_t ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < 20 || ihl > len) {
        return;
    }

    /* Verify checksum */
    uint16_t saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    uint16_t calc_checksum = ip_checksum(hdr, ihl);
    hdr->checksum = saved_checksum;

    if (calc_checksum != saved_checksum) {
        kprintf("[IP] Bad checksum\n");
        return;
    }

    /* Check if packet is for us */
    uint32_t dst = hdr->dst_ip;
    if (dst != ip_config.addr &&
        dst != 0xFFFFFFFF &&  /* Broadcast */
        (dst & ip_config.netmask) != (0xFFFFFFFF & ip_config.netmask)) {
        return;  /* Not for us */
    }

    /* Get payload */
    uint16_t total_len = ntohs(hdr->total_length);
    if (total_len > len) {
        return;
    }

    void *payload = (uint8_t *)data + ihl;
    size_t payload_len = total_len - ihl;

    /* Dispatch based on protocol */
    switch (hdr->protocol) {
        case IP_PROTO_ICMP:
            icmp_receive(dev, hdr->src_ip, payload, payload_len);
            break;

        case IP_PROTO_UDP:
            udp_receive(dev, hdr->src_ip, payload, payload_len);
            break;

        case IP_PROTO_TCP:
            tcp_receive(dev, hdr->src_ip, payload, payload_len);
            break;

        default:
            /* Unknown protocol */
            break;
    }
}

/* Send IP packet */
int ip_send(netdev_t *dev, uint32_t dst_ip, uint8_t protocol,
            void *data, size_t len) {
    if (!dev || !data || !ip_config.configured) {
        return -1;
    }

    /* Build IP packet */
    uint8_t packet[1500];
    ip_header_t *hdr = (ip_header_t *)packet;

    /* Fill header */
    hdr->version_ihl = 0x45;  /* IPv4, 5 DWORDS (20 bytes) */
    hdr->tos = 0;
    hdr->total_length = htons(sizeof(ip_header_t) + len);
    hdr->id = htons(ip_id++);
    hdr->flags_fragment = htons(IP_FLAG_DF);  /* Don't fragment */
    hdr->ttl = IP_DEFAULT_TTL;
    hdr->protocol = protocol;
    hdr->checksum = 0;
    hdr->src_ip = ip_config.addr;
    hdr->dst_ip = dst_ip;

    /* Calculate checksum */
    hdr->checksum = ip_checksum(hdr, sizeof(ip_header_t));

    /* Copy payload */
    memcpy(packet + sizeof(ip_header_t), data, len);

    /* Determine next hop */
    uint32_t next_hop;
    if (ip_is_local(dst_ip, ip_config.addr, ip_config.netmask)) {
        next_hop = dst_ip;
    } else {
        next_hop = ip_config.gateway;
    }

    /* Resolve MAC address via ARP */
    uint8_t dst_mac[6];
    size_t total_pkt_len = sizeof(ip_header_t) + len;

    if (arp_resolve(dev, next_hop, dst_mac) < 0) {
        /* MAC not in cache - queue packet and wait for ARP reply */
        char ip_str[16];
        ip_format(next_hop, ip_str);
        kprintf("[IP] Queuing packet, waiting for ARP resolution of %s\n", ip_str);

        /* Queue the IP packet for later transmission */
        if (arp_queue_packet(dev, next_hop, packet, total_pkt_len) < 0) {
            kprintf("[IP] Failed to queue packet\n");
            return -1;
        }
        return 0;  /* Packet queued, will be sent when ARP resolves */
    }

    /* Send via ethernet */
    return ethernet_send(dev, dst_mac, ETH_TYPE_IP, packet, total_pkt_len);
}
