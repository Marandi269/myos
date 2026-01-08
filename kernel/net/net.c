/*
 * net.c - Network Subsystem Initialization
 */

#include "net/net.h"
#include "drivers/pci.h"
#include "drivers/virtio_net.h"
#include "lib/kprintf.h"

/* Initialize entire network stack */
void net_init(void) {
    kprintf("\n[NET] Initializing network stack...\n");

    /* Initialize PCI bus */
    pci_init();

    /* Initialize network device abstraction */
    /* netdev is statically initialized */

    /* Initialize ethernet layer */
    ethernet_init();

    /* Initialize ARP */
    arp_init();

    /* Initialize IP */
    ip_init();

    /* Initialize ICMP */
    icmp_init();

    /* Initialize UDP */
    udp_init();

    /* Initialize TCP */
    tcp_init();

    /* Initialize socket layer */
    socket_init();

    /* Initialize DHCP client */
    dhcp_init();

    /* Initialize virtio-net driver */
    if (virtio_net_init() == 0) {
        virtio_net_dev_t *vnet = virtio_net_get_dev();
        if (vnet) {
            /* Register network device */
            netdev_register(&vnet->netdev);
        }
    }

    kprintf("[NET] Network stack initialized\n\n");
}

/* Network polling */
void net_poll(void) {
    /* Poll virtio-net for received packets */
    virtio_net_poll();
}

/* Network test function */
void net_test(void) {
    netdev_t *dev = netdev_get_default();

    if (!dev) {
        kprintf("[NET] No network device available\n");
        return;
    }

    kprintf("\n[NET] Running network tests...\n");

    /* Print network configuration */
    char ip_str[16], mask_str[16], gw_str[16];
    ip_format(ip_get_addr(), ip_str);
    ip_format(ip_get_netmask(), mask_str);
    ip_format(ip_get_gateway(), gw_str);

    kprintf("[NET] Device: %s\n", dev->name);
    kprintf("[NET] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            dev->mac[0], dev->mac[1], dev->mac[2],
            dev->mac[3], dev->mac[4], dev->mac[5]);
    kprintf("[NET] IP: %s\n", ip_str);
    kprintf("[NET] Netmask: %s\n", mask_str);
    kprintf("[NET] Gateway: %s\n", gw_str);

    /* Try to ping the gateway */
    kprintf("\n[NET] Pinging gateway...\n");
    icmp_echo_request(dev, ip_get_gateway(), 1, 1);

    /* Poll for response */
    for (int i = 0; i < 50; i++) {
        net_poll();
        /* Small delay */
        for (volatile int j = 0; j < 100000; j++);
    }

    kprintf("[NET] Network tests completed\n\n");
}
