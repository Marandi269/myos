/*
 * netdev.h - Network Device Abstraction
 */

#ifndef _NETDEV_H
#define _NETDEV_H

#include "types.h"

/* Maximum network devices */
#define NETDEV_MAX  4

/* Forward declaration */
struct netdev;

/* Network device structure */
typedef struct netdev {
    char name[16];              /* Device name (e.g., "eth0") */
    uint8_t mac[6];             /* MAC address */
    uint16_t mtu;               /* Maximum transmission unit */

    /* Device callbacks */
    int (*send)(struct netdev *dev, void *data, size_t len);
    void (*receive)(struct netdev *dev, void *data, size_t len);

    /* Private device data */
    void *priv;
} netdev_t;

/* Register a network device */
int netdev_register(netdev_t *dev);

/* Get device by name */
netdev_t *netdev_get_by_name(const char *name);

/* Get default device (first registered) */
netdev_t *netdev_get_default(void);

/* Send packet via device */
int netdev_send(netdev_t *dev, void *data, size_t len);

/* Set receive callback */
void netdev_set_receive_callback(netdev_t *dev,
                                  void (*callback)(netdev_t *, void *, size_t));

#endif /* _NETDEV_H */
