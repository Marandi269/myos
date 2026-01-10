/*
 * stdlib.c - Standard library functions
 */

#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <errno.h>

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

void *aligned_alloc(size_t alignment, size_t size) {
    /* Simple implementation - just use malloc with extra space */
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    void *ptr = malloc(size + alignment);
    if (!ptr) return NULL;
    void *aligned = (void *)(((size_t)ptr + alignment - 1) & ~(alignment - 1));
    return aligned;
}

void exit(int status) {
    /* TODO: call atexit handlers */
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

void _Exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

void abort(void) {
    /* Send SIGABRT to self */
    syscall2(SYS_kill, syscall0(SYS_getpid), 6);  /* SIGABRT = 6 */
    _Exit(134);  /* 128 + SIGABRT */
}

/* atexit handlers */
#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= MAX_ATEXIT) {
        return -1;
    }
    atexit_funcs[atexit_count++] = func;
    return 0;
}

int on_exit(void (*func)(int, void *), void *arg) {
    (void)func;
    (void)arg;
    /* Not fully implemented */
    return -1;
}

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}

long atol(const char *str) {
    return strtol(str, NULL, 10);
}

long long atoll(const char *str) {
    return strtoll(str, NULL, 10);
}

long strtol(const char *str, char **endptr, int base) {
    long result = 0;
    int sign = 1;
    const char *s = str;

    /* Skip whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    /* Handle sign */
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    /* Handle base prefix */
    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = (s[0] == '0') ? 8 : 10;
        }
    }

    /* Convert digits */
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        result = result * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = (char *)s;
    }

    return result * sign;
}

unsigned long strtoul(const char *str, char **endptr, int base) {
    return (unsigned long)strtol(str, endptr, base);
}

long long strtoll(const char *str, char **endptr, int base) {
    long long result = 0;
    int sign = 1;
    const char *s = str;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = (s[0] == '0') ? 8 : 10;
        }
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        result = result * base + digit;
        s++;
    }

    if (endptr) {
        *endptr = (char *)s;
    }

    return result * sign;
}

unsigned long long strtoull(const char *str, char **endptr, int base) {
    return (unsigned long long)strtoll(str, endptr, base);
}

/* Floating point stubs - return 0 as long (no SSE) */
long strtod_stub(const char *str, char **endptr) {
    (void)str;
    if (endptr) *endptr = (char *)str;
    return 0;  /* Stub */
}

/* Use integer-only implementations - real floating point needs SSE */
__attribute__((weak, alias("strtod_stub"))) long strtod(const char *, char **);
__attribute__((weak, alias("strtod_stub"))) long strtof(const char *, char **);
__attribute__((weak, alias("strtod_stub"))) long strtold(const char *, char **);

char *getenv(const char *name) {
    (void)name;
    return NULL;  /* Not implemented */
}

int putenv(char *string) {
    (void)string;
    return -1;  /* Not implemented */
}

int setenv(const char *name, const char *value, int overwrite) {
    (void)name;
    (void)value;
    (void)overwrite;
    return -1;  /* Not implemented */
}

int unsetenv(const char *name) {
    (void)name;
    return -1;  /* Not implemented */
}

int clearenv(void) {
    return -1;  /* Not implemented */
}

/* Random number generation */
static unsigned int rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (unsigned int)(rand_seed / 65536) % 32768;
}

void srand(unsigned int seed) {
    rand_seed = seed;
}

int rand_r(unsigned int *seedp) {
    *seedp = *seedp * 1103515245 + 12345;
    return (unsigned int)(*seedp / 65536) % 32768;
}

/* Integer arithmetic */
int abs(int j) {
    return (j < 0) ? -j : j;
}

long labs(long j) {
    return (j < 0) ? -j : j;
}

long long llabs(long long j) {
    return (j < 0) ? -j : j;
}

div_t div(int numer, int denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

/* qsort implementation */
static void swap(char *a, char *b, size_t size) {
    while (size--) {
        char tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    if (nmemb <= 1) return;

    char *arr = (char *)base;
    char *pivot = arr + (nmemb - 1) * size;
    size_t i = 0;

    for (size_t j = 0; j < nmemb - 1; j++) {
        if (compar(arr + j * size, pivot) < 0) {
            swap(arr + i * size, arr + j * size, size);
            i++;
        }
    }
    swap(arr + i * size, pivot, size);

    if (i > 0) qsort(arr, i, size, compar);
    if (i + 1 < nmemb) qsort(arr + (i + 1) * size, nmemb - i - 1, size, compar);
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
    const char *arr = (const char *)base;
    size_t low = 0, high = nmemb;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int cmp = compar(key, arr + mid * size);

        if (cmp < 0) {
            high = mid;
        } else if (cmp > 0) {
            low = mid + 1;
        } else {
            return (void *)(arr + mid * size);
        }
    }
    return NULL;
}

/* Multibyte stubs */
int mblen(const char *s, size_t n) {
    (void)n;
    if (!s || !*s) return 0;
    return 1;  /* Assume single-byte encoding */
}

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    (void)n;
    if (!s) return 0;
    if (!*s) {
        if (pwc) *pwc = 0;
        return 0;
    }
    if (pwc) *pwc = (unsigned char)*s;
    return 1;
}

int wctomb(char *s, wchar_t wchar) {
    if (!s) return 0;
    *s = (char)wchar;
    return 1;
}

size_t mbstowcs(wchar_t *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        if (dest) dest[i] = (unsigned char)src[i];
    }
    return i;
}

size_t wcstombs(char *dest, const wchar_t *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        if (dest) dest[i] = (char)src[i];
    }
    return i;
}

/* Temporary file stubs */
char *mktemp(char *template) {
    (void)template;
    return NULL;  /* Not implemented */
}

int mkstemp(char *template) {
    (void)template;
    errno = ENOSYS;
    return -1;  /* Not implemented */
}

char *mkdtemp(char *template) {
    (void)template;
    return NULL;  /* Not implemented */
}

/* Pseudo-terminal stubs */
int posix_openpt(int flags) {
    (void)flags;
    errno = ENOSYS;
    return -1;
}

int grantpt(int fd) {
    (void)fd;
    return 0;  /* Always succeed */
}

int unlockpt(int fd) {
    (void)fd;
    return 0;  /* Always succeed */
}

char *ptsname(int fd) {
    (void)fd;
    return NULL;
}

char *realpath(const char *path, char *resolved_path) {
    /* Simple implementation - just copy for now */
    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    if (!resolved_path) {
        resolved_path = malloc(4096);
        if (!resolved_path) {
            errno = ENOMEM;
            return NULL;
        }
    }
    /* TODO: resolve symlinks and . and .. */
    strcpy(resolved_path, path);
    return resolved_path;
}

int system(const char *command) {
    (void)command;
    return -1;  /* Not implemented */
}
