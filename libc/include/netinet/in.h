/*
 * netinet/in.h - Internet Protocol addresses
 */

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

/* Port numbers */
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

/* IPv4 address structure */
struct in_addr {
    in_addr_t s_addr;
};

/* IPv4 socket address */
struct sockaddr_in {
    sa_family_t sin_family;     /* AF_INET */
    in_port_t sin_port;         /* Port number (network byte order) */
    struct in_addr sin_addr;    /* IPv4 address */
    char sin_zero[8];           /* Padding */
};

/* Special addresses */
#define INADDR_ANY          ((in_addr_t)0x00000000)
#define INADDR_BROADCAST    ((in_addr_t)0xFFFFFFFF)
#define INADDR_LOOPBACK     ((in_addr_t)0x7F000001)
#define INADDR_NONE         ((in_addr_t)0xFFFFFFFF)

#endif /* _NETINET_IN_H */
