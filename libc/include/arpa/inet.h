/*
 * arpa/inet.h - Address conversion functions
 */

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <stdint.h>
#include <netinet/in.h>

/* Byte order conversion */
uint16_t htons(uint16_t hostshort);
uint16_t ntohs(uint16_t netshort);
uint32_t htonl(uint32_t hostlong);
uint32_t ntohl(uint32_t netlong);

/* Address conversion */
in_addr_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);
int inet_aton(const char *cp, struct in_addr *inp);

/* Convert IP address between text and binary */
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, size_t size);

#endif /* _ARPA_INET_H */
