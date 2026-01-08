/*
 * cat.c - Concatenate and display files
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 1024

static int cat_fd(int fd) {
    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }

    return (n < 0) ? 1 : 0;
}

static int cat_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("cat: %s: No such file or directory\n", path);
        return 1;
    }

    int ret = cat_fd(fd);
    close(fd);
    return ret;
}

int main(int argc, char *argv[]) {
    int i;
    int ret = 0;

    if (argc == 1) {
        /* No arguments - read from stdin */
        return cat_fd(STDIN_FILENO);
    }

    for (i = 1; i < argc; i++) {
        if (cat_file(argv[i]) != 0) {
            ret = 1;
        }
    }

    return ret;
}
