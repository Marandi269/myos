/*
 * net.h - Network Subsystem Main Header
 */

#ifndef _NET_H
#define _NET_H

#include "types.h"

/* Include all network headers */
#include "net/netdev.h"
#include "net/ethernet.h"
#include "net/arp.h"
#include "net/ip.h"
#include "net/icmp.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/socket.h"
#include "net/dhcp.h"

/* Initialize entire network stack */
void net_init(void);

/* Network polling (call from main loop or timer) */
void net_poll(void);

/* Network test function */
void net_test(void);

#endif /* _NET_H */
