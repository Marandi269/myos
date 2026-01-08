/*
 * tcp.c - Transmission Control Protocol
 */

#include "net/tcp.h"
#include "net/ip.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"

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

/* TCP connections */
static tcp_conn_t tcp_connections[TCP_MAX_CONNECTIONS];
static uint16_t next_ephemeral_port = 49152;
static uint32_t tcp_isn = 0x12345678;  /* Initial sequence number */

/* State names */
static const char *state_names[] = {
    "CLOSED", "LISTEN", "SYN_SENT", "SYN_RECEIVED",
    "ESTABLISHED", "FIN_WAIT_1", "FIN_WAIT_2", "CLOSE_WAIT",
    "CLOSING", "LAST_ACK", "TIME_WAIT"
};

/* Get TCP state name */
const char *tcp_state_name(tcp_state_t state) {
    if (state <= TCP_TIME_WAIT) {
        return state_names[state];
    }
    return "UNKNOWN";
}

/* Calculate TCP checksum */
static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                             void *tcp_packet, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr;

    /* Add pseudo-header */
    ptr = (uint16_t *)&src_ip;
    sum += ptr[0];
    sum += ptr[1];

    ptr = (uint16_t *)&dst_ip;
    sum += ptr[0];
    sum += ptr[1];

    sum += htons(IP_PROTO_TCP);
    sum += htons(len);

    /* Add TCP packet */
    ptr = (uint16_t *)tcp_packet;
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

/* Initialize TCP */
void tcp_init(void) {
    memset(tcp_connections, 0, sizeof(tcp_connections));
    kprintf("[TCP] Initialized\n");
}

/* Find connection by tuple */
static tcp_conn_t *tcp_find_conn(uint32_t local_ip, uint16_t local_port,
                                  uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        tcp_conn_t *conn = &tcp_connections[i];
        if (conn->active && conn->state != TCP_CLOSED &&
            conn->local_port == local_port &&
            (conn->remote_ip == remote_ip || conn->remote_ip == 0) &&
            (conn->remote_port == remote_port || conn->remote_port == 0)) {
            return conn;
        }
    }
    return NULL;
}

/* Allocate new connection */
static tcp_conn_t *tcp_alloc_conn(void) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (!tcp_connections[i].active) {
            tcp_conn_t *conn = &tcp_connections[i];
            memset(conn, 0, sizeof(*conn));
            conn->active = true;
            conn->state = TCP_CLOSED;
            conn->rcv_wnd = TCP_RECV_BUF_SIZE;
            return conn;
        }
    }
    return NULL;
}

/* Send TCP segment */
static int tcp_send_segment(tcp_conn_t *conn, uint8_t flags,
                            void *data, size_t len) {
    uint8_t packet[1500];
    tcp_header_t *hdr = (tcp_header_t *)packet;

    size_t hdr_len = sizeof(tcp_header_t);
    size_t total_len = hdr_len + len;

    if (total_len > sizeof(packet)) {
        return -1;
    }

    memset(hdr, 0, hdr_len);
    hdr->src_port = htons(conn->local_port);
    hdr->dst_port = htons(conn->remote_port);
    hdr->seq_num = htonl(conn->snd_nxt);
    hdr->ack_num = htonl(conn->rcv_nxt);
    hdr->data_offset = (hdr_len / 4) << 4;
    hdr->flags = flags;
    hdr->window = htons(conn->rcv_wnd);
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;

    /* Copy data */
    if (len > 0 && data) {
        memcpy(packet + hdr_len, data, len);
    }

    /* Calculate checksum */
    hdr->checksum = tcp_checksum(conn->local_ip, conn->remote_ip,
                                  packet, total_len);

    /* Update sequence number */
    if (flags & TCP_FLAG_SYN) {
        conn->snd_nxt++;
    }
    if (flags & TCP_FLAG_FIN) {
        conn->snd_nxt++;
    }
    conn->snd_nxt += len;

    netdev_t *dev = netdev_get_default();
    return ip_send(dev, conn->remote_ip, IP_PROTO_TCP, packet, total_len);
}

/* Send RST */
static void tcp_send_rst(uint32_t src_ip, uint16_t src_port,
                         uint32_t dst_ip, uint16_t dst_port,
                         uint32_t seq, uint32_t ack) {
    uint8_t packet[sizeof(tcp_header_t)];
    tcp_header_t *hdr = (tcp_header_t *)packet;

    memset(hdr, 0, sizeof(*hdr));
    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->seq_num = htonl(seq);
    hdr->ack_num = htonl(ack);
    hdr->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    hdr->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
    hdr->window = 0;
    hdr->checksum = 0;

    hdr->checksum = tcp_checksum(src_ip, dst_ip, packet, sizeof(*hdr));

    netdev_t *dev = netdev_get_default();
    ip_send(dev, dst_ip, IP_PROTO_TCP, packet, sizeof(*hdr));
}

/* Handle incoming SYN (passive open) */
static void tcp_handle_syn(tcp_conn_t *listen_conn,
                            uint32_t remote_ip, uint16_t remote_port,
                            uint32_t seq) {
    /* Allocate new connection for this client */
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) {
        kprintf("[TCP] No free connections\n");
        return;
    }

    conn->local_ip = ip_get_addr();
    conn->local_port = listen_conn->local_port;
    conn->remote_ip = remote_ip;
    conn->remote_port = remote_port;
    conn->irs = seq;
    conn->rcv_nxt = seq + 1;
    conn->iss = tcp_isn++;
    conn->snd_nxt = conn->iss;
    conn->snd_una = conn->iss;
    conn->state = TCP_SYN_RECEIVED;

    /* Allocate buffers */
    conn->recv_buf = kmalloc(TCP_RECV_BUF_SIZE);
    conn->recv_size = TCP_RECV_BUF_SIZE;
    conn->send_buf = kmalloc(TCP_SEND_BUF_SIZE);
    conn->send_size = TCP_SEND_BUF_SIZE;

    /* Copy callbacks from listening socket */
    conn->on_connect = listen_conn->on_connect;
    conn->on_data = listen_conn->on_data;
    conn->on_close = listen_conn->on_close;

    char ip_str[16];
    ip_format(remote_ip, ip_str);
    kprintf("[TCP] SYN from %s:%d\n", ip_str, remote_port);

    /* Send SYN-ACK */
    tcp_send_segment(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
}

/* Receive TCP packet */
void tcp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len) {
    (void)dev;

    if (len < sizeof(tcp_header_t)) {
        return;
    }

    tcp_header_t *hdr = (tcp_header_t *)data;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint32_t seq = ntohl(hdr->seq_num);
    uint32_t ack = ntohl(hdr->ack_num);
    uint8_t flags = hdr->flags;

    size_t hdr_len = ((hdr->data_offset >> 4) & 0x0F) * 4;
    if (hdr_len < sizeof(tcp_header_t) || hdr_len > len) {
        return;
    }

    /* Verify checksum */
    uint16_t saved_checksum = hdr->checksum;
    hdr->checksum = 0;
    uint16_t calc_checksum = tcp_checksum(src_ip, ip_get_addr(), data, len);
    hdr->checksum = saved_checksum;

    if (calc_checksum != saved_checksum) {
        kprintf("[TCP] Bad checksum\n");
        return;
    }

    void *payload = (uint8_t *)data + hdr_len;
    size_t payload_len = len - hdr_len;

    /* Find connection */
    tcp_conn_t *conn = tcp_find_conn(ip_get_addr(), dst_port, src_ip, src_port);

    if (!conn) {
        /* Check for listening socket */
        conn = tcp_find_conn(ip_get_addr(), dst_port, 0, 0);

        if (conn && conn->state == TCP_LISTEN && (flags & TCP_FLAG_SYN)) {
            tcp_handle_syn(conn, src_ip, src_port, seq);
            return;
        }

        /* No connection - send RST */
        if (!(flags & TCP_FLAG_RST)) {
            tcp_send_rst(ip_get_addr(), dst_port, src_ip, src_port, ack, seq + 1);
        }
        return;
    }

    /* State machine */
    switch (conn->state) {
        case TCP_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                /* Received SYN-ACK */
                conn->irs = seq;
                conn->rcv_nxt = seq + 1;
                conn->snd_una = ack;
                conn->state = TCP_ESTABLISHED;

                /* Send ACK */
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);

                kprintf("[TCP] Connection established\n");
                if (conn->on_connect) {
                    conn->on_connect(conn);
                }
            }
            break;

        case TCP_SYN_RECEIVED:
            if (flags & TCP_FLAG_ACK) {
                conn->snd_una = ack;
                conn->state = TCP_ESTABLISHED;

                kprintf("[TCP] Connection established (server)\n");
                if (conn->on_connect) {
                    conn->on_connect(conn);
                }
            }
            break;

        case TCP_ESTABLISHED:
            if (flags & TCP_FLAG_FIN) {
                conn->rcv_nxt = seq + 1;
                conn->state = TCP_CLOSE_WAIT;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);

                kprintf("[TCP] Received FIN, closing\n");
                if (conn->on_close) {
                    conn->on_close(conn);
                }
            } else if (payload_len > 0) {
                /* Data received */
                if (seq == conn->rcv_nxt) {
                    /* In-order data */
                    if (conn->recv_buf && conn->recv_len + payload_len <= conn->recv_size) {
                        memcpy(conn->recv_buf + conn->recv_len, payload, payload_len);
                        conn->recv_len += payload_len;
                    }
                    conn->rcv_nxt += payload_len;

                    /* Send ACK */
                    tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);

                    if (conn->on_data) {
                        conn->on_data(conn, payload, payload_len);
                    }
                }
            } else if (flags & TCP_FLAG_ACK) {
                /* ACK only */
                conn->snd_una = ack;
            }
            break;

        case TCP_FIN_WAIT_1:
            if (flags & TCP_FLAG_ACK) {
                conn->snd_una = ack;
                conn->state = TCP_FIN_WAIT_2;
            }
            if (flags & TCP_FLAG_FIN) {
                conn->rcv_nxt = seq + 1;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_TIME_WAIT;
            }
            break;

        case TCP_FIN_WAIT_2:
            if (flags & TCP_FLAG_FIN) {
                conn->rcv_nxt = seq + 1;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_TIME_WAIT;
            }
            break;

        case TCP_CLOSE_WAIT:
            /* Waiting for application to close */
            break;

        case TCP_LAST_ACK:
            if (flags & TCP_FLAG_ACK) {
                conn->state = TCP_CLOSED;
                conn->active = false;
                if (conn->recv_buf) kfree(conn->recv_buf);
                if (conn->send_buf) kfree(conn->send_buf);
            }
            break;

        case TCP_TIME_WAIT:
            /* TODO: Implement 2MSL timeout */
            break;

        default:
            break;
    }
}

/* Create listening socket */
tcp_conn_t *tcp_listen(uint16_t port) {
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) {
        return NULL;
    }

    conn->local_ip = ip_get_addr();
    conn->local_port = port;
    conn->remote_ip = 0;
    conn->remote_port = 0;
    conn->state = TCP_LISTEN;

    kprintf("[TCP] Listening on port %d\n", port);
    return conn;
}

/* Connect to remote host */
tcp_conn_t *tcp_connect(uint32_t remote_ip, uint16_t remote_port) {
    tcp_conn_t *conn = tcp_alloc_conn();
    if (!conn) {
        return NULL;
    }

    conn->local_ip = ip_get_addr();
    conn->local_port = next_ephemeral_port++;
    if (next_ephemeral_port == 0) {
        next_ephemeral_port = 49152;
    }
    conn->remote_ip = remote_ip;
    conn->remote_port = remote_port;

    conn->iss = tcp_isn++;
    conn->snd_nxt = conn->iss;
    conn->snd_una = conn->iss;
    conn->state = TCP_SYN_SENT;

    /* Allocate buffers */
    conn->recv_buf = kmalloc(TCP_RECV_BUF_SIZE);
    conn->recv_size = TCP_RECV_BUF_SIZE;
    conn->send_buf = kmalloc(TCP_SEND_BUF_SIZE);
    conn->send_size = TCP_SEND_BUF_SIZE;

    char ip_str[16];
    ip_format(remote_ip, ip_str);
    kprintf("[TCP] Connecting to %s:%d\n", ip_str, remote_port);

    /* Send SYN */
    tcp_send_segment(conn, TCP_FLAG_SYN, NULL, 0);

    return conn;
}

/* Send data */
int tcp_write(tcp_conn_t *conn, void *data, size_t len) {
    if (!conn || conn->state != TCP_ESTABLISHED) {
        return -1;
    }

    /* Send data in segments */
    size_t sent = 0;
    while (sent < len) {
        size_t chunk = len - sent;
        if (chunk > TCP_MSS) {
            chunk = TCP_MSS;
        }

        tcp_send_segment(conn, TCP_FLAG_ACK | TCP_FLAG_PSH,
                         (uint8_t *)data + sent, chunk);
        sent += chunk;
    }

    return sent;
}

/* Read data */
int tcp_read(tcp_conn_t *conn, void *buf, size_t len) {
    if (!conn || !conn->recv_buf) {
        return -1;
    }

    size_t to_read = conn->recv_len < len ? conn->recv_len : len;
    if (to_read > 0) {
        memcpy(buf, conn->recv_buf, to_read);
        /* Move remaining data */
        if (to_read < conn->recv_len) {
            memmove(conn->recv_buf, conn->recv_buf + to_read,
                    conn->recv_len - to_read);
        }
        conn->recv_len -= to_read;
    }

    return to_read;
}

/* Close connection */
void tcp_close(tcp_conn_t *conn) {
    if (!conn) {
        return;
    }

    switch (conn->state) {
        case TCP_ESTABLISHED:
            tcp_send_segment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            conn->state = TCP_FIN_WAIT_1;
            break;

        case TCP_CLOSE_WAIT:
            tcp_send_segment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            conn->state = TCP_LAST_ACK;
            break;

        case TCP_LISTEN:
        case TCP_SYN_SENT:
            conn->state = TCP_CLOSED;
            conn->active = false;
            if (conn->recv_buf) kfree(conn->recv_buf);
            if (conn->send_buf) kfree(conn->send_buf);
            break;

        default:
            break;
    }
}
