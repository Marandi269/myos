/*
 * arp.c - Address Resolution Protocol
 */

#include "net/arp.h"
#include "net/ethernet.h"
#include "net/ip.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/pit.h"

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

/* ARP table */
static arp_entry_t arp_table[ARP_TABLE_SIZE];

/* Pending packet queue - packets waiting for ARP resolution */
typedef struct arp_pending {
    netdev_t *dev;
    uint32_t ip;
    uint8_t data[ARP_PENDING_PKT_SIZE];
    size_t len;
    bool valid;
} arp_pending_t;

static arp_pending_t arp_pending_queue[ARP_PENDING_MAX];

/* Convert IP to string for debug output */
static void ip_to_str(uint32_t ip, char *buf) {
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

/* Initialize ARP */
void arp_init(void) {
    memset(arp_table, 0, sizeof(arp_table));
    memset(arp_pending_queue, 0, sizeof(arp_pending_queue));
    kprintf("[ARP] Initialized\n");
}

/* Add or update ARP entry */
void arp_add_entry(uint32_t ip, const uint8_t *mac) {
    int free_slot = -1;

    /* Look for existing entry or free slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            /* Update existing entry */
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].timestamp = pit_get_ticks();
            /* Process any pending packets for this IP */
            arp_process_pending(ip);
            return;
        }
        if (!arp_table[i].valid && free_slot < 0) {
            free_slot = i;
        }
    }

    /* Add new entry */
    if (free_slot >= 0) {
        arp_table[free_slot].ip = ip;
        memcpy(arp_table[free_slot].mac, mac, 6);
        arp_table[free_slot].timestamp = pit_get_ticks();
        arp_table[free_slot].valid = true;

        char ip_str[16];
        ip_to_str(ip, ip_str);
        kprintf("[ARP] Added: %s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                ip_str, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        /* Process any pending packets for this IP */
        arp_process_pending(ip);
    }
}

/* Lookup MAC by IP */
bool arp_lookup(uint32_t ip, uint8_t *mac_out) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(mac_out, arp_table[i].mac, 6);
            return true;
        }
    }
    return false;
}

/* Send ARP request */
int arp_request(netdev_t *dev, uint32_t target_ip) {
    if (!dev) {
        return -1;
    }

    arp_header_t arp;
    memset(&arp, 0, sizeof(arp));

    arp.hw_type = htons(ARP_HW_ETHER);
    arp.proto_type = htons(ARP_PROTO_IP);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REQUEST);

    /* Sender: our MAC and IP */
    memcpy(arp.sender_mac, dev->mac, 6);
    arp.sender_ip = ip_get_addr();

    /* Target: unknown MAC, target IP */
    memset(arp.target_mac, 0, 6);
    arp.target_ip = target_ip;

    char ip_str[16];
    ip_to_str(target_ip, ip_str);
    kprintf("[ARP] Request: Who has %s?\n", ip_str);

    /* Send as broadcast ethernet frame */
    return ethernet_send(dev, ETH_BROADCAST, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* Send ARP reply */
static int arp_reply(netdev_t *dev, uint32_t target_ip, const uint8_t *target_mac) {
    arp_header_t arp;
    memset(&arp, 0, sizeof(arp));

    arp.hw_type = htons(ARP_HW_ETHER);
    arp.proto_type = htons(ARP_PROTO_IP);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REPLY);

    /* Sender: our MAC and IP */
    memcpy(arp.sender_mac, dev->mac, 6);
    arp.sender_ip = ip_get_addr();

    /* Target: requester's MAC and IP */
    memcpy(arp.target_mac, target_mac, 6);
    arp.target_ip = target_ip;

    return ethernet_send(dev, target_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* Receive ARP packet */
void arp_receive(netdev_t *dev, void *data, size_t len) {
    if (len < sizeof(arp_header_t)) {
        return;
    }

    arp_header_t *arp = (arp_header_t *)data;

    /* Validate ARP packet */
    if (ntohs(arp->hw_type) != ARP_HW_ETHER ||
        ntohs(arp->proto_type) != ARP_PROTO_IP ||
        arp->hw_len != 6 || arp->proto_len != 4) {
        return;
    }

    uint16_t opcode = ntohs(arp->opcode);
    uint32_t our_ip = ip_get_addr();

    /* Always update ARP table with sender info */
    arp_add_entry(arp->sender_ip, arp->sender_mac);

    if (opcode == ARP_OP_REQUEST) {
        /* Check if request is for us */
        if (arp->target_ip == our_ip) {
            char ip_str[16];
            ip_to_str(arp->sender_ip, ip_str);
            kprintf("[ARP] Request from %s - replying\n", ip_str);
            arp_reply(dev, arp->sender_ip, arp->sender_mac);
        }
    } else if (opcode == ARP_OP_REPLY) {
        char ip_str[16];
        ip_to_str(arp->sender_ip, ip_str);
        kprintf("[ARP] Reply: %s is at %02x:%02x:%02x:%02x:%02x:%02x\n",
                ip_str,
                arp->sender_mac[0], arp->sender_mac[1], arp->sender_mac[2],
                arp->sender_mac[3], arp->sender_mac[4], arp->sender_mac[5]);
    }
}

/* Queue packet waiting for ARP resolution */
int arp_queue_packet(netdev_t *dev, uint32_t ip, void *data, size_t len) {
    if (len > ARP_PENDING_PKT_SIZE) {
        return -1;
    }

    /* Find free slot */
    for (int i = 0; i < ARP_PENDING_MAX; i++) {
        if (!arp_pending_queue[i].valid) {
            arp_pending_queue[i].dev = dev;
            arp_pending_queue[i].ip = ip;
            memcpy(arp_pending_queue[i].data, data, len);
            arp_pending_queue[i].len = len;
            arp_pending_queue[i].valid = true;
            return 0;
        }
    }

    return -1;  /* Queue full */
}

/* Process pending packets after ARP resolution */
void arp_process_pending(uint32_t ip) {
    uint8_t mac[6];

    if (!arp_lookup(ip, mac)) {
        return;  /* Still no MAC */
    }

    for (int i = 0; i < ARP_PENDING_MAX; i++) {
        if (arp_pending_queue[i].valid && arp_pending_queue[i].ip == ip) {
            /* Send the pending packet */
            ethernet_send(arp_pending_queue[i].dev, mac, ETH_TYPE_IP,
                          arp_pending_queue[i].data, arp_pending_queue[i].len);

            /* Mark as sent */
            arp_pending_queue[i].valid = false;

            kprintf("[ARP] Sent queued packet to resolved IP\n");
        }
    }
}

/* Resolve IP to MAC with ARP - returns immediately, queues packet if needed */
int arp_resolve(netdev_t *dev, uint32_t ip, uint8_t *mac_out) {
    /* Check cache first */
    if (arp_lookup(ip, mac_out)) {
        return 0;
    }

    /* Not in cache - need to send ARP request */
    /* Caller should queue the packet using arp_queue_packet() */
    arp_request(dev, ip);

    return -1;  /* Not resolved yet */
}

/* Print ARP table */
void arp_print_table(void) {
    kprintf("[ARP] Table:\n");
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid) {
            char ip_str[16];
            ip_to_str(arp_table[i].ip, ip_str);
            kprintf("  %s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                    ip_str,
                    arp_table[i].mac[0], arp_table[i].mac[1],
                    arp_table[i].mac[2], arp_table[i].mac[3],
                    arp_table[i].mac[4], arp_table[i].mac[5]);
        }
    }
}
