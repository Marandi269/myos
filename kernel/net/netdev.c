/*
 * netdev.c - Network Device Abstraction
 */

#include "net/netdev.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Registered network devices */
static netdev_t *netdevs[NETDEV_MAX];
static int netdev_count = 0;

/* Register a network device */
int netdev_register(netdev_t *dev) {
    if (!dev || netdev_count >= NETDEV_MAX) {
        return -1;
    }

    netdevs[netdev_count] = dev;
    netdev_count++;

    kprintf("[netdev] Registered %s\n", dev->name);
    return 0;
}

/* Get device by name */
netdev_t *netdev_get_by_name(const char *name) {
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < netdev_count; i++) {
        if (strcmp(netdevs[i]->name, name) == 0) {
            return netdevs[i];
        }
    }
    return NULL;
}

/* Get default device */
netdev_t *netdev_get_default(void) {
    if (netdev_count > 0) {
        return netdevs[0];
    }
    return NULL;
}

/* Send packet via device */
int netdev_send(netdev_t *dev, void *data, size_t len) {
    if (!dev || !dev->send) {
        return -1;
    }
    return dev->send(dev, data, len);
}

/* Set receive callback */
void netdev_set_receive_callback(netdev_t *dev,
                                  void (*callback)(netdev_t *, void *, size_t)) {
    if (dev) {
        dev->receive = callback;
    }
}
