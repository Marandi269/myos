/*
 * dirent.c - Directory operations
 */

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syscall.h>

DIR *opendir(const char *name) {
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return NULL;
    }

    DIR *dir = malloc(sizeof(DIR));
    if (!dir) {
        close(fd);
        return NULL;
    }

    dir->fd = fd;
    dir->buf_pos = 0;
    dir->buf_end = 0;

    return dir;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent entry;

    if (!dirp) {
        return NULL;
    }

    /* Need more data? */
    if (dirp->buf_pos >= dirp->buf_end) {
        long nread = syscall3(SYS_getdents64, dirp->fd, (long)dirp->buf, sizeof(dirp->buf));
        if (nread <= 0) {
            return NULL;
        }
        dirp->buf_pos = 0;
        dirp->buf_end = nread;
    }

    /* Parse linux_dirent64 */
    struct {
        uint64_t d_ino;
        int64_t  d_off;
        uint16_t d_reclen;
        uint8_t  d_type;
        char     d_name[];
    } *de = (void *)(dirp->buf + dirp->buf_pos);

    entry.d_ino = de->d_ino;
    entry.d_off = de->d_off;
    entry.d_reclen = de->d_reclen;
    entry.d_type = de->d_type;
    strncpy(entry.d_name, de->d_name, sizeof(entry.d_name) - 1);
    entry.d_name[sizeof(entry.d_name) - 1] = '\0';

    dirp->buf_pos += de->d_reclen;

    return &entry;
}

int closedir(DIR *dirp) {
    if (!dirp) {
        return -1;
    }

    int ret = close(dirp->fd);
    free(dirp);
    return ret;
}
