/*
 * ifconfig.c - Network interface configuration utility
 *
 * Note: This is a placeholder that prints usage info.
 * Actual network config is shown by kernel output.
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("ifconfig - Network Interface Configuration\n\n");
    printf("eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>\n");
    printf("      Note: Actual network info is printed by kernel.\n");
    printf("\n");
    printf("Network interface information:\n");
    printf("  - Device: virtio-net (eth0)\n");
    printf("  - IP: 10.0.2.15 (QEMU user mode default)\n");
    printf("  - Netmask: 255.255.255.0\n");
    printf("  - Gateway: 10.0.2.2\n");
    printf("\n");
    printf("Use kernel net_test() for actual network status.\n");

    return 0;
}
