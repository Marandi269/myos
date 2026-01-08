/*
 * stdlib.c - Standard library functions
 */

#include <stdlib.h>
#include <string.h>
#include <syscall.h>

/* Simple heap implementation using brk */
static void *heap_start = NULL;
static void *heap_end = NULL;

/* Block header */
struct block {
    size_t size;
    int free;
    struct block *next;
};

#define BLOCK_SIZE sizeof(struct block)

static struct block *free_list = NULL;

/* Extend heap using brk */
static void *sbrk(size_t increment) {
    long current = syscall1(SYS_brk, 0);

    if (!heap_start) {
        heap_start = (void *)current;
        heap_end = heap_start;
    }

    if (increment == 0) {
        return heap_end;
    }

    long new_brk = syscall1(SYS_brk, current + increment);
    if (new_brk == current) {
        return (void *)-1;  /* Failed */
    }

    void *old_end = heap_end;
    heap_end = (void *)new_brk;
    return old_end;
}

/* Find free block */
static struct block *find_free_block(struct block **last, size_t size) {
    struct block *current = free_list;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

/* Request space from OS */
static struct block *request_space(struct block *last, size_t size) {
    struct block *block = sbrk(0);
    void *request = sbrk(BLOCK_SIZE + size);

    if (request == (void *)-1) {
        return NULL;
    }

    if (last) {
        last->next = block;
    }

    block->size = size;
    block->free = 0;
    block->next = NULL;
    return block;
}

void *malloc(size_t size) {
    struct block *block;

    if (size == 0) {
        return NULL;
    }

    /* Align to 16 bytes */
    size = (size + 15) & ~15;

    if (!free_list) {
        block = request_space(NULL, size);
        if (!block) return NULL;
        free_list = block;
    } else {
        struct block *last = free_list;
        block = find_free_block(&last, size);

        if (!block) {
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            block->free = 0;
        }
    }

    return (void *)(block + 1);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }

    struct block *block = (struct block *)ptr - 1;

    if (block->size >= size) {
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }

    struct block *block = (struct block *)ptr - 1;
    block->free = 1;
}

void exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

void abort(void) {
    exit(134);  /* 128 + SIGABRT (6) */
}

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}

long atol(const char *str) {
    return strtol(str, NULL, 10);
}

long strtol(const char *str, char **endptr, int base) {
    long result = 0;
    int sign = 1;

    /* Skip whitespace */
    while (*str == ' ' || *str == '\t') str++;

    /* Handle sign */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str += 2;
        } else if (base == 0) {
            base = (str[0] == '0') ? 8 : 10;
        }
    }

    /* Convert digits */
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'z') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'Z') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        result = result * base + digit;
        str++;
    }

    if (endptr) {
        *endptr = (char *)str;
    }

    return result * sign;
}

unsigned long strtoul(const char *str, char **endptr, int base) {
    return (unsigned long)strtol(str, endptr, base);
}

char *getenv(const char *name) {
    (void)name;
    return NULL;  /* Not implemented */
}
