/*
 * tcp.h - Transmission Control Protocol
 */

#ifndef _TCP_H
#define _TCP_H

#include "types.h"
#include "net/netdev.h"

/* TCP flags */
#define TCP_FLAG_FIN    0x01
#define TCP_FLAG_SYN    0x02
#define TCP_FLAG_RST    0x04
#define TCP_FLAG_PSH    0x08
#define TCP_FLAG_ACK    0x10
#define TCP_FLAG_URG    0x20

/* TCP states */
typedef enum tcp_state {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

/* TCP header */
typedef struct tcp_header {
    uint16_t src_port;      /* Source port */
    uint16_t dst_port;      /* Destination port */
    uint32_t seq_num;       /* Sequence number */
    uint32_t ack_num;       /* Acknowledgment number */
    uint8_t data_offset;    /* Data offset (4 bits) + reserved (4 bits) */
    uint8_t flags;          /* Flags */
    uint16_t window;        /* Window size */
    uint16_t checksum;      /* Checksum */
    uint16_t urgent_ptr;    /* Urgent pointer */
} __attribute__((packed)) tcp_header_t;

/* TCP connection block */
typedef struct tcp_conn {
    /* Connection tuple */
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    /* State */
    tcp_state_t state;

    /* Sequence numbers */
    uint32_t snd_una;       /* Send unacknowledged */
    uint32_t snd_nxt;       /* Send next */
    uint32_t snd_wnd;       /* Send window */
    uint32_t rcv_nxt;       /* Receive next */
    uint32_t rcv_wnd;       /* Receive window */
    uint32_t iss;           /* Initial send sequence */
    uint32_t irs;           /* Initial receive sequence */

    /* Buffers */
    uint8_t *recv_buf;
    size_t recv_len;
    size_t recv_size;

    uint8_t *send_buf;
    size_t send_len;
    size_t send_size;

    /* Callbacks */
    void (*on_connect)(struct tcp_conn *conn);
    void (*on_data)(struct tcp_conn *conn, void *data, size_t len);
    void (*on_close)(struct tcp_conn *conn);

    /* Internal */
    bool active;
    void *priv;
} tcp_conn_t;

/* Maximum TCP connections */
#define TCP_MAX_CONNECTIONS     32
#define TCP_RECV_BUF_SIZE       8192
#define TCP_SEND_BUF_SIZE       8192
#define TCP_MSS                 1460

/* Initialize TCP */
void tcp_init(void);

/* Receive TCP packet */
void tcp_receive(netdev_t *dev, uint32_t src_ip, void *data, size_t len);

/* Create a listening socket */
tcp_conn_t *tcp_listen(uint16_t port);

/* Connect to remote host */
tcp_conn_t *tcp_connect(uint32_t remote_ip, uint16_t remote_port);

/* Send data on connection */
int tcp_write(tcp_conn_t *conn, void *data, size_t len);

/* Read data from connection */
int tcp_read(tcp_conn_t *conn, void *buf, size_t len);

/* Close connection */
void tcp_close(tcp_conn_t *conn);

/* Get TCP state name */
const char *tcp_state_name(tcp_state_t state);

#endif /* _TCP_H */
