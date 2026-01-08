/*
 * arp.h - Address Resolution Protocol
 */

#ifndef _ARP_H
#define _ARP_H

#include "types.h"
#include "net/netdev.h"

/* ARP constants */
#define ARP_HW_ETHER        1       /* Ethernet hardware type */
#define ARP_PROTO_IP        0x0800  /* IPv4 protocol type */

#define ARP_OP_REQUEST      1       /* ARP request */
#define ARP_OP_REPLY        2       /* ARP reply */

/* ARP table size */
#define ARP_TABLE_SIZE      64
#define ARP_TIMEOUT_MS      (60 * 1000)  /* 60 seconds */

/* ARP pending queue size */
#define ARP_PENDING_MAX     16
#define ARP_PENDING_PKT_SIZE 1500

/* ARP header for IPv4 over Ethernet */
typedef struct arp_header {
    uint16_t hw_type;       /* Hardware type (1 = Ethernet) */
    uint16_t proto_type;    /* Protocol type (0x0800 = IPv4) */
    uint8_t hw_len;         /* Hardware address length (6 for Ethernet) */
    uint8_t proto_len;      /* Protocol address length (4 for IPv4) */
    uint16_t opcode;        /* Operation (1=request, 2=reply) */
    uint8_t sender_mac[6];  /* Sender MAC address */
    uint32_t sender_ip;     /* Sender IP address */
    uint8_t target_mac[6];  /* Target MAC address */
    uint32_t target_ip;     /* Target IP address */
} __attribute__((packed)) arp_header_t;

/* ARP table entry */
typedef struct arp_entry {
    uint32_t ip;            /* IP address */
    uint8_t mac[6];         /* MAC address */
    uint64_t timestamp;     /* Time when entry was added */
    bool valid;             /* Entry is valid */
} arp_entry_t;

/* Initialize ARP */
void arp_init(void);

/* Receive ARP packet */
void arp_receive(netdev_t *dev, void *data, size_t len);

/* Resolve IP address to MAC address */
int arp_resolve(netdev_t *dev, uint32_t ip, uint8_t *mac_out);

/* Send ARP request */
int arp_request(netdev_t *dev, uint32_t target_ip);

/* Add/update ARP entry */
void arp_add_entry(uint32_t ip, const uint8_t *mac);

/* Lookup MAC by IP */
bool arp_lookup(uint32_t ip, uint8_t *mac_out);

/* Print ARP table */
void arp_print_table(void);

/* Queue packet waiting for ARP resolution */
int arp_queue_packet(netdev_t *dev, uint32_t ip, void *data, size_t len);

/* Process pending packets after ARP resolution */
void arp_process_pending(uint32_t ip);

#endif /* _ARP_H */
