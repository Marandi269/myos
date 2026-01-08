/*
 * sys/socket.h - BSD Socket API
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>

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

/* Type definitions */
typedef uint16_t sa_family_t;
typedef size_t socklen_t;

/* Generic socket address */
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

/* Socket API */
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
int shutdown(int sockfd, int how);

#endif /* _SYS_SOCKET_H */
