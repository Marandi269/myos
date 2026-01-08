/*
 * pwd.c - Print working directory
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    char cwd[256];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printf("pwd: error getting current directory\n");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}
