/*
 * ping.c - Simple ping utility
 *
 * Note: This is a placeholder that prints usage info.
 * Actual ping functionality is handled by the kernel.
 * Use kernel shell command: ping <ip>
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ping <ip_address>\n");
        printf("Example: ping 10.0.2.2\n");
        printf("\nNote: Ping is implemented in the kernel.\n");
        printf("Use the kernel test function for now.\n");
        return 1;
    }

    printf("ping %s\n", argv[1]);
    printf("Note: User-space ping requires socket syscalls.\n");
    printf("Kernel network test will ping the gateway.\n");

    return 0;
}
