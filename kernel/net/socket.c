/*
 * socket.c - BSD Socket API Implementation
 */

#include "net/socket.h"
#include "net/netdev.h"
#include "net/ip.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "fs/poll.h"

/* Socket table */
static socket_t sockets[SOCKET_MAX];
static bool socket_initialized = false;

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

uint16_t socket_htons(uint16_t hostshort) { return bswap16(hostshort); }
uint16_t socket_ntohs(uint16_t netshort) { return bswap16(netshort); }
uint32_t socket_htonl(uint32_t hostlong) { return bswap32(hostlong); }
uint32_t socket_ntohl(uint32_t netlong) { return bswap32(netlong); }

#define SOCKET_RECV_BUF_SIZE 4096

/* UDP receive callback for sockets */
static void socket_udp_callback(uint32_t src_ip, uint16_t src_port,
                                 uint16_t dst_port,
                                 void *data, size_t len) {
    /* Find socket bound to this port */
    for (int i = 0; i < SOCKET_MAX; i++) {
        socket_t *sock = &sockets[i];
        if (sock->active && sock->type == SOCK_DGRAM &&
            sock->local_port == dst_port) {

            /* Store in receive buffer */
            if (!sock->recv_buf) {
                sock->recv_buf = kmalloc(SOCKET_RECV_BUF_SIZE);
                sock->recv_size = SOCKET_RECV_BUF_SIZE;
                sock->recv_head = 0;
                sock->recv_tail = 0;
            }

            /* Copy data (simple ring buffer) */
            size_t space = sock->recv_size - sock->recv_tail;
            size_t to_copy = len < space ? len : space;

            if (to_copy > 0) {
                memcpy(sock->recv_buf + sock->recv_tail, data, to_copy);
                sock->recv_tail += to_copy;
            }

            sock->last_src_addr = src_ip;
            sock->last_src_port = src_port;

            /* Wake up any processes waiting to receive */
            wake_up_all(&sock->recv_wait);
            return;
        }
    }
}

/* Initialize socket subsystem */
void socket_init(void) {
    memset(sockets, 0, sizeof(sockets));
    socket_initialized = true;
    kprintf("[Socket] Initialized\n");
}

/* Find socket by fd */
static socket_t *socket_get(int fd) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) {
        return NULL;
    }
    return &sockets[fd];
}

/* Public function to get socket by fd (for poll support) */
socket_t *socket_get_by_fd(int fd) {
    return socket_get(fd);
}

/* Allocate socket */
static int socket_alloc(void) {
    for (int i = 0; i < SOCKET_MAX; i++) {
        if (!sockets[i].active) {
            memset(&sockets[i], 0, sizeof(socket_t));
            sockets[i].active = true;
            sockets[i].blocking = true;
            /* Initialize wait queues */
            init_waitqueue_head(&sockets[i].recv_wait);
            init_waitqueue_head(&sockets[i].send_wait);
            init_waitqueue_head(&sockets[i].accept_wait);
            return i;
        }
    }
    return -1;
}

/* Create socket */
int sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        return -1;  /* Only IPv4 supported */
    }

    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        return -1;
    }

    int fd = socket_alloc();
    if (fd < 0) {
        return -1;
    }

    socket_t *sock = &sockets[fd];
    sock->type = type;

    if (type == SOCK_STREAM) {
        sock->protocol = IPPROTO_TCP;
    } else {
        sock->protocol = IPPROTO_UDP;
    }

    if (protocol != 0) {
        sock->protocol = protocol;
    }

    sock->state = SOCKET_UNCONNECTED;

    return fd;
}

/* Bind socket to address */
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen) {
    socket_t *sock = socket_get(sockfd);
    if (!sock || !addr || addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
    if (sin->sin_family != AF_INET) {
        return -1;
    }

    sock->local_addr = socket_ntohl(sin->sin_addr.s_addr);
    sock->local_port = socket_ntohs(sin->sin_port);
    sock->bound = true;
    sock->state = SOCKET_BOUND;

    if (sock->type == SOCK_DGRAM) {
        /* Bind UDP callback */
        udp_bind(sock->local_port, socket_udp_callback);
    }

    kprintf("[Socket] Bound to port %d\n", sock->local_port);
    return 0;
}

/* Listen for connections (TCP only) */
int sys_listen(int sockfd, int backlog) {
    (void)backlog;

    socket_t *sock = socket_get(sockfd);
    if (!sock || sock->type != SOCK_STREAM) {
        return -1;
    }

    if (!sock->bound) {
        return -1;
    }

    sock->tcp_conn = tcp_listen(sock->local_port);
    if (!sock->tcp_conn) {
        return -1;
    }

    sock->listening = true;
    sock->state = SOCKET_LISTENING;

    return 0;
}

/* Accept connection (TCP only) */
int sys_accept(int sockfd, struct sockaddr *addr, size_t *addrlen) {
    socket_t *sock = socket_get(sockfd);
    if (!sock || !sock->listening) {
        return -1;
    }

    /* TODO: Wait for incoming connection and create new socket */
    /* For now, return error */
    (void)addr;
    (void)addrlen;

    return -1;
}

/* Connect to remote host */
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen) {
    socket_t *sock = socket_get(sockfd);
    if (!sock || !addr || addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
    if (sin->sin_family != AF_INET) {
        return -1;
    }

    sock->remote_addr = socket_ntohl(sin->sin_addr.s_addr);
    sock->remote_port = socket_ntohs(sin->sin_port);

    if (sock->type == SOCK_STREAM) {
        /* TCP connect */
        sock->tcp_conn = tcp_connect(sock->remote_addr, sock->remote_port);
        if (!sock->tcp_conn) {
            return -1;
        }

        /* TODO: Wait for connection to establish */
        sock->state = SOCKET_CONNECTING;
    } else {
        /* UDP "connect" just sets the remote address */
        sock->connected = true;
        sock->state = SOCKET_CONNECTED;
    }

    return 0;
}

/* Send data (connected socket) */
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags) {
    (void)flags;

    socket_t *sock = socket_get(sockfd);
    if (!sock || !buf) {
        return -1;
    }

    netdev_t *dev = netdev_get_default();
    if (!dev) {
        return -1;
    }

    if (sock->type == SOCK_STREAM) {
        if (!sock->tcp_conn) {
            return -1;
        }
        return tcp_write((tcp_conn_t *)sock->tcp_conn, (void *)buf, len);
    } else {
        /* UDP */
        if (!sock->connected) {
            return -1;
        }
        return udp_send(dev, sock->remote_addr, sock->local_port,
                        sock->remote_port, (void *)buf, len);
    }
}

/* Receive data (connected socket) */
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags) {
    (void)flags;

    socket_t *sock = socket_get(sockfd);
    if (!sock || !buf) {
        return -1;
    }

    if (sock->type == SOCK_STREAM) {
        if (!sock->tcp_conn) {
            return -1;
        }
        return tcp_read((tcp_conn_t *)sock->tcp_conn, buf, len);
    } else {
        /* UDP */
        if (!sock->recv_buf || sock->recv_head >= sock->recv_tail) {
            return 0;  /* No data */
        }

        size_t available = sock->recv_tail - sock->recv_head;
        size_t to_copy = len < available ? len : available;

        memcpy(buf, sock->recv_buf + sock->recv_head, to_copy);
        sock->recv_head += to_copy;

        /* Reset buffer if empty */
        if (sock->recv_head >= sock->recv_tail) {
            sock->recv_head = 0;
            sock->recv_tail = 0;
        }

        return to_copy;
    }
}

/* Send to specific address (UDP) */
ssize_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                    const struct sockaddr *dest_addr, size_t addrlen) {
    (void)flags;

    socket_t *sock = socket_get(sockfd);
    if (!sock || !buf || sock->type != SOCK_DGRAM) {
        return -1;
    }

    if (!dest_addr || addrlen < sizeof(struct sockaddr_in)) {
        return -1;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)dest_addr;
    uint32_t dst_ip = socket_ntohl(sin->sin_addr.s_addr);
    uint16_t dst_port = socket_ntohs(sin->sin_port);

    /* Allocate ephemeral port if not bound */
    if (!sock->bound) {
        sock->local_port = udp_alloc_port();
        udp_bind(sock->local_port, socket_udp_callback);
        sock->bound = true;
    }

    netdev_t *dev = netdev_get_default();
    if (!dev) {
        return -1;
    }

    return udp_send(dev, dst_ip, sock->local_port, dst_port, (void *)buf, len);
}

/* Receive from with address (UDP) */
ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                      struct sockaddr *src_addr, size_t *addrlen) {
    (void)flags;

    socket_t *sock = socket_get(sockfd);
    if (!sock || !buf || sock->type != SOCK_DGRAM) {
        return -1;
    }

    /* Receive data */
    ssize_t received = sys_recv(sockfd, buf, len, flags);

    /* Fill in source address if requested */
    if (received > 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *sin = (struct sockaddr_in *)src_addr;
        sin->sin_family = AF_INET;
        sin->sin_port = socket_htons(sock->last_src_port);
        sin->sin_addr.s_addr = socket_htonl(sock->last_src_addr);
        *addrlen = sizeof(struct sockaddr_in);
    }

    return received;
}

/* Shutdown socket */
int sys_shutdown(int sockfd, int how) {
    socket_t *sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (sock->type == SOCK_STREAM && sock->tcp_conn) {
        if (how == SHUT_WR || how == SHUT_RDWR) {
            tcp_close((tcp_conn_t *)sock->tcp_conn);
        }
    }

    return 0;
}

/* Close socket */
int sys_closesocket(int sockfd) {
    socket_t *sock = socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (sock->type == SOCK_STREAM && sock->tcp_conn) {
        tcp_close((tcp_conn_t *)sock->tcp_conn);
    }

    if (sock->type == SOCK_DGRAM && sock->bound) {
        udp_unbind(sock->local_port);
    }

    if (sock->recv_buf) {
        kfree(sock->recv_buf);
    }

    /* Wake up any waiters before closing */
    wake_up_all(&sock->recv_wait);
    wake_up_all(&sock->send_wait);
    wake_up_all(&sock->accept_wait);

    sock->active = false;
    return 0;
}

/*
 * Poll a socket for events
 * Returns events that are ready
 */
unsigned int socket_poll(int sockfd, struct poll_table *pt) {
    socket_t *sock = socket_get(sockfd);
    unsigned int mask = 0;

    if (!sock) {
        return POLLNVAL;
    }

    /* Register with wait queues */
    if (pt) {
        poll_wait(NULL, &sock->recv_wait, (poll_table_t *)pt);
        poll_wait(NULL, &sock->send_wait, (poll_table_t *)pt);
    }

    /* Check for readable data */
    if (sock->type == SOCK_DGRAM) {
        /* UDP: check receive buffer */
        if (sock->recv_buf && sock->recv_tail > sock->recv_head) {
            mask |= POLLIN | POLLRDNORM;
        }
    } else if (sock->type == SOCK_STREAM) {
        /* TCP: check connection state and receive buffer */
        if (sock->tcp_conn) {
            /* Simplified: assume readable if connected */
            if (sock->state == SOCKET_CONNECTED) {
                mask |= POLLIN | POLLRDNORM;
            }
        }
        /* Check for connection accepted (listening socket) */
        if (sock->listening) {
            /* TODO: check for pending connections */
        }
    }

    /* Check for writable (can send) */
    if (sock->state == SOCKET_CONNECTED || sock->type == SOCK_DGRAM) {
        mask |= POLLOUT | POLLWRNORM;  /* Simplified: assume always writable */
    }

    /* Check for errors/hangup */
    if (!sock->active) {
        mask |= POLLHUP;
    }

    return mask;
}

/*
 * Wake up processes waiting to receive on a socket
 */
void socket_wakeup_recv(socket_t *sock) {
    if (sock) {
        wake_up_all(&sock->recv_wait);
    }
}

/*
 * Wake up processes waiting to send on a socket
 */
void socket_wakeup_send(socket_t *sock) {
    if (sock) {
        wake_up_all(&sock->send_wait);
    }
}
