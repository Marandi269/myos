/*
 * socket.h - BSD Socket API
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include "types.h"
#include "proc/wait_queue.h"

/* Forward declaration for poll_table */
struct poll_table;

/* Address families */
#define AF_UNSPEC   0
#define AF_INET     2

/* Socket types */
#define SOCK_STREAM 1   /* TCP */
#define SOCK_DGRAM  2   /* UDP */
#define SOCK_RAW    3   /* Raw IP */

/* Protocols */
#define IPPROTO_IP      0
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/* Socket options levels */
#define SOL_SOCKET      1

/* Socket options */
#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9
#define SO_RCVBUF       8
#define SO_SNDBUF       7

/* Shutdown modes */
#define SHUT_RD     0
#define SHUT_WR     1
#define SHUT_RDWR   2

/* Maximum sockets */
#define SOCKET_MAX  32

/* Socket address structures */
typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;         /* Network byte order */
    struct in_addr sin_addr;
    char sin_zero[8];
};

/* Socket structure */
typedef struct socket {
    int type;               /* SOCK_STREAM or SOCK_DGRAM */
    int protocol;           /* TCP, UDP, etc. */
    int state;              /* Socket state */

    uint32_t local_addr;    /* Local IP address */
    uint16_t local_port;    /* Local port */
    uint32_t remote_addr;   /* Remote IP address */
    uint16_t remote_port;   /* Remote port */

    /* Receive buffer */
    uint8_t *recv_buf;
    size_t recv_head;
    size_t recv_tail;
    size_t recv_size;

    /* For UDP: store sender info */
    uint32_t last_src_addr;
    uint16_t last_src_port;

    /* TCP connection */
    void *tcp_conn;

    /* Flags */
    bool bound;
    bool connected;
    bool listening;
    bool active;

    /* Blocking mode */
    bool blocking;

    /* Wait queues for poll/select support */
    wait_queue_head_t recv_wait;    /* Processes waiting to receive */
    wait_queue_head_t send_wait;    /* Processes waiting to send */
    wait_queue_head_t accept_wait;  /* Processes waiting to accept */
} socket_t;

/* Socket states */
#define SOCKET_UNCONNECTED  0
#define SOCKET_BOUND        1
#define SOCKET_LISTENING    2
#define SOCKET_CONNECTING   3
#define SOCKET_CONNECTED    4
#define SOCKET_CLOSING      5

/* Initialize socket subsystem */
void socket_init(void);

/* BSD Socket API */
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen);
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                    const struct sockaddr *dest_addr, size_t addrlen);
ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                      struct sockaddr *src_addr, size_t *addrlen);
int sys_shutdown(int sockfd, int how);
int sys_closesocket(int sockfd);

/* Helper functions */
uint16_t socket_htons(uint16_t hostshort);
uint16_t socket_ntohs(uint16_t netshort);
uint32_t socket_htonl(uint32_t hostlong);
uint32_t socket_ntohl(uint32_t netlong);

/* Poll support */
unsigned int socket_poll(int sockfd, struct poll_table *pt);

/* Get socket by fd (for poll support) */
socket_t *socket_get_by_fd(int fd);

/* Wake up waiters on socket (called when data arrives) */
void socket_wakeup_recv(socket_t *sock);
void socket_wakeup_send(socket_t *sock);

#endif /* _SOCKET_H */
