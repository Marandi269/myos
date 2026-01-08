/*
 * mman.c - Memory mapping functions
 */

#include <sys/mman.h>
#include <syscall.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    long ret = syscall5(SYS_mmap, (long)addr, length, prot, flags, fd);
    (void)offset;  /* Offset not fully supported */
    return (void *)ret;
}

int munmap(void *addr, size_t length) {
    return syscall2(SYS_munmap, (long)addr, length);
}
