/*
 * hello.c - Simple hello world test program
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Hello from user space!\n");
    printf("My PID is %d\n", getpid());

    return 0;
}
