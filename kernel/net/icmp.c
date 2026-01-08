/*
 * icmp.c - Internet Control Message Protocol
 */

#include "net/icmp.h"
#include "net/ip.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Byte swap */
static inline uint16_t bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

#define ntohs(x) bswap16(x)
#define htons(x) bswap16(x)

/* Ping response callback */
static void (*ping_callback)(uint32_t src_ip, uint16_t id, uint16_t seq, uint32_t rtt) = NULL;

/* Ping request tracking */
static struct {
    uint16_t id;
    uint16_t seq;
    uint64_t send_time;
    bool waiting;
} ping_state = {0};

/* Initialize ICMP */
void icmp_init(void) {
    kprintf("[ICMP] Initialized\n");
}

/* Calculate ICMP checksum */
static uint16_t icmp_checksum(void *data, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

/* Send ICMP echo reply */
static int icmp_echo_reply(netdev_t *dev, uint32_t dst_ip,
                            uint16_t id, uint16_t seq,
                            void *data, size_t len) {
    uint8_t packet[1500];
    icmp_header_t *hdr = (icmp_header_t *)packet;

    size_t total_len = sizeof(icmp_header_t) + len;
    if (total_len > sizeof(packet)) {
        return -1;
    }

    /* Build ICMP header */
    hdr->type = ICMP_TYPE_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->data.echo.id = id;        /* Keep original ID */
    hdr->data.echo.sequence = seq; /* Keep original sequence */

    /* Copy echo data */
    if (len > 0 && data) {
        memcpy(packet + sizeof(icmp_header_t), data, len);
    }

    /* Calculate checksum */
    hdr->checksum = icmp_checksum(packet, total_len);

    /* Send via IP */
    return ip_send(dev, dst_ip, IP_PROTO_ICMP, packet, total_len);
}

/* Receive ICMP packet */
void icmp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len) {
    if (len < sizeof(icmp_header_t)) {
        return;
    }

    icmp_header_t *hdr = (icmp_header_t *)data;

    /* Verify checksum */
    uint16_t saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    uint16_t calc_checksum = icmp_checksum(data, len);
    hdr->checksum = saved_checksum;

    if (calc_checksum != saved_checksum) {
        kprintf("[ICMP] Bad checksum\n");
        return;
    }

    char ip_str[16];
    ip_format(src_ip, ip_str);

    switch (hdr->type) {
        case ICMP_TYPE_ECHO_REQUEST: {
            kprintf("[ICMP] Echo request from %s\n", ip_str);

            /* Send reply */
            void *echo_data = (uint8_t *)data + sizeof(icmp_header_t);
            size_t echo_len = len - sizeof(icmp_header_t);

            icmp_echo_reply(dev, src_ip,
                           hdr->data.echo.id,
                           hdr->data.echo.sequence,
                           echo_data, echo_len);
            break;
        }

        case ICMP_TYPE_ECHO_REPLY: {
            uint16_t id = ntohs(hdr->data.echo.id);
            uint16_t seq = ntohs(hdr->data.echo.sequence);

            kprintf("[ICMP] Echo reply from %s: id=%d seq=%d\n",
                    ip_str, id, seq);

            /* Call ping callback if set */
            if (ping_callback && ping_state.waiting &&
                id == ping_state.id && seq == ping_state.seq) {
                ping_state.waiting = false;
                ping_callback(src_ip, id, seq, 0);  /* TODO: calculate RTT */
            }
            break;
        }

        case ICMP_TYPE_DEST_UNREACHABLE:
            kprintf("[ICMP] Destination unreachable from %s, code=%d\n",
                    ip_str, hdr->code);
            break;

        case ICMP_TYPE_TIME_EXCEEDED:
            kprintf("[ICMP] Time exceeded from %s\n", ip_str);
            break;

        default:
            kprintf("[ICMP] Unknown type %d from %s\n", hdr->type, ip_str);
            break;
    }
}

/* Send ICMP echo request */
int icmp_echo_request(netdev_t *dev, uint32_t dst_ip, uint16_t id, uint16_t seq) {
    uint8_t packet[64];
    icmp_header_t *hdr = (icmp_header_t *)packet;

    /* Build ICMP header */
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->data.echo.id = htons(id);
    hdr->data.echo.sequence = htons(seq);

    /* Add some payload data */
    size_t payload_len = 32;
    for (size_t i = 0; i < payload_len; i++) {
        packet[sizeof(icmp_header_t) + i] = 'a' + (i % 26);
    }

    size_t total_len = sizeof(icmp_header_t) + payload_len;

    /* Calculate checksum */
    hdr->checksum = icmp_checksum(packet, total_len);

    /* Track ping state */
    ping_state.id = id;
    ping_state.seq = seq;
    ping_state.waiting = true;

    char ip_str[16];
    ip_format(dst_ip, ip_str);
    kprintf("[ICMP] Sending echo request to %s: id=%d seq=%d\n",
            ip_str, id, seq);

    /* Send via IP */
    return ip_send(dev, dst_ip, IP_PROTO_ICMP, packet, total_len);
}

/* Send ICMP destination unreachable */
int icmp_dest_unreachable(netdev_t *dev, uint32_t dst_ip, uint8_t code,
                          void *orig_packet, size_t orig_len) {
    uint8_t packet[576];  /* Minimum IP MTU */
    icmp_header_t *hdr = (icmp_header_t *)packet;

    /* ICMP error includes original IP header + 8 bytes of data */
    size_t copy_len = orig_len > 28 ? 28 : orig_len;

    hdr->type = ICMP_TYPE_DEST_UNREACHABLE;
    hdr->code = code;
    hdr->checksum = 0;
    hdr->data.unused = 0;

    /* Copy original packet data */
    memcpy(packet + sizeof(icmp_header_t), orig_packet, copy_len);

    size_t total_len = sizeof(icmp_header_t) + copy_len;

    /* Calculate checksum */
    hdr->checksum = icmp_checksum(packet, total_len);

    return ip_send(dev, dst_ip, IP_PROTO_ICMP, packet, total_len);
}
