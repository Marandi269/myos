/*
 * ls.c - List directory contents
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

static void print_type(unsigned char type) {
    switch (type) {
        case DT_DIR:  putchar('d'); break;
        case DT_REG:  putchar('-'); break;
        case DT_LNK:  putchar('l'); break;
        case DT_CHR:  putchar('c'); break;
        case DT_BLK:  putchar('b'); break;
        case DT_FIFO: putchar('p'); break;
        case DT_SOCK: putchar('s'); break;
        default:      putchar('?'); break;
    }
}

static int list_dir(const char *path, int show_all, int long_format) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);
    if (!dir) {
        printf("ls: cannot access '%s': No such file or directory\n", path);
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files unless -a */
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }

        if (long_format) {
            print_type(entry->d_type);
            printf("  %s\n", entry->d_name);
        } else {
            printf("%s  ", entry->d_name);
        }
    }

    if (!long_format) {
        putchar('\n');
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    int show_all = 0;
    int long_format = 0;
    const char *path = ".";
    int i;
    int ret = 0;

    /* Parse options */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                    case 'a': show_all = 1; break;
                    case 'l': long_format = 1; break;
                    default:
                        printf("ls: invalid option -- '%c'\n", argv[i][j]);
                        return 1;
                }
            }
        } else {
            path = argv[i];
        }
    }

    return list_dir(path, show_all, long_format);
}
