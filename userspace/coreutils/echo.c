/*
 * echo.c - Print arguments
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    int i;
    int newline = 1;

    for (i = 1; i < argc; i++) {
        /* Handle -n flag */
        if (i == 1 && argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0') {
            newline = 0;
            continue;
        }

        if (i > 1 && (newline || i > 2)) {
            putchar(' ');
        }
        printf("%s", argv[i]);
    }

    if (newline) {
        putchar('\n');
    }

    return 0;
}
