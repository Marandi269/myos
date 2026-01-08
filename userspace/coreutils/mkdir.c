/*
 * mkdir.c - Make directories
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int i;
    int ret = 0;

    if (argc < 2) {
        printf("mkdir: missing operand\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) != 0) {
            printf("mkdir: cannot create directory '%s'\n", argv[i]);
            ret = 1;
        }
    }

    return ret;
}
