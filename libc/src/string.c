/*
 * string.c - String and memory functions
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Forward declare snprintf */
int snprintf(char *str, size_t size, const char *format, ...);

/* String length */
size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) len++;
    return len;
}

/* String copy */
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

size_t strlcpy(char *dest, const char *src, size_t size) {
    size_t src_len = strlen(src);
    if (size > 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
    return src_len;
}

/* String concatenation */
char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest + strlen(dest);
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        d[i] = src[i];
    }
    d[i] = '\0';
    return dest;
}

size_t strlcat(char *dest, const char *src, size_t size) {
    size_t dest_len = strnlen(dest, size);
    size_t src_len = strlen(src);

    if (dest_len == size) {
        return size + src_len;
    }

    size_t copy_len = size - dest_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }

    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return dest_len + src_len;
}

/* String comparison */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return (unsigned char)*s1 - (unsigned char)*s2;
}

static inline int _tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && _tolower(*s1) == _tolower(*s2)) {
        s1++;
        s2++;
    }
    return _tolower((unsigned char)*s1) - _tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && _tolower(*s1) == _tolower(*s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return _tolower((unsigned char)*s1) - _tolower((unsigned char)*s2);
}

int strcoll(const char *s1, const char *s2) {
    /* Simple implementation - same as strcmp for C locale */
    return strcmp(s1, s2);
}

/* String search */
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char *)s;
        s++;
    }
    return c ? NULL : (char *)s;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == c) last = s;
        s++;
    }
    return c ? (char *)last : (char *)s;
}

char *strstr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    if (!needle_len) return (char *)haystack;

    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

char *strcasestr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    if (!needle_len) return (char *)haystack;

    while (*haystack) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return NULL;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        const char *a = accept;
        while (*a) {
            if (*s == *a) return (char *)s;
            a++;
        }
        s++;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    while (*s) {
        const char *a = accept;
        int found = 0;
        while (*a) {
            if (*s == *a) {
                found = 1;
                break;
            }
            a++;
        }
        if (!found) break;
        count++;
        s++;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    while (*s) {
        const char *r = reject;
        while (*r) {
            if (*s == *r) return count;
            r++;
        }
        count++;
        s++;
    }
    return count;
}

/* String tokenization */
static char *__strtok_state = NULL;

char *strtok(char *str, const char *delim) {
    return strtok_r(str, delim, &__strtok_state);
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;

    if (str) {
        *saveptr = str;
    }

    if (!*saveptr) {
        return NULL;
    }

    /* Skip leading delimiters */
    *saveptr += strspn(*saveptr, delim);

    if (!**saveptr) {
        *saveptr = NULL;
        return NULL;
    }

    token = *saveptr;

    /* Find end of token */
    *saveptr = strpbrk(token, delim);
    if (*saveptr) {
        **saveptr = '\0';
        (*saveptr)++;
    }

    return token;
}

char *strsep(char **stringp, const char *delim) {
    char *start = *stringp;
    char *p;

    if (!start) {
        return NULL;
    }

    p = strpbrk(start, delim);
    if (p) {
        *p = '\0';
        *stringp = p + 1;
    } else {
        *stringp = NULL;
    }

    return start;
}

/* String duplication */
char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *new = malloc(len);
    if (new) {
        memcpy(new, s, len);
    }
    return new;
}

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *new = malloc(len + 1);
    if (new) {
        memcpy(new, s, len);
        new[len] = '\0';
    }
    return new;
}

/* String transformation */
size_t strxfrm(char *dest, const char *src, size_t n) {
    /* Simple implementation - just copy for C locale */
    size_t len = strlen(src);
    if (n > 0) {
        size_t copy_len = (len < n) ? len : n - 1;
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
    return len;
}

/* Error string */
char *strerror(int errnum) {
    static char buf[64];

    switch (errnum) {
        case 0: return "Success";
        case EPERM: return "Operation not permitted";
        case ENOENT: return "No such file or directory";
        case ESRCH: return "No such process";
        case EINTR: return "Interrupted system call";
        case EIO: return "I/O error";
        case ENXIO: return "No such device or address";
        case E2BIG: return "Argument list too long";
        case ENOEXEC: return "Exec format error";
        case EBADF: return "Bad file descriptor";
        case ECHILD: return "No child processes";
        case EAGAIN: return "Resource temporarily unavailable";
        case ENOMEM: return "Cannot allocate memory";
        case EACCES: return "Permission denied";
        case EFAULT: return "Bad address";
        case EBUSY: return "Device or resource busy";
        case EEXIST: return "File exists";
        case EXDEV: return "Invalid cross-device link";
        case ENODEV: return "No such device";
        case ENOTDIR: return "Not a directory";
        case EISDIR: return "Is a directory";
        case EINVAL: return "Invalid argument";
        case ENFILE: return "Too many open files in system";
        case EMFILE: return "Too many open files";
        case ENOTTY: return "Inappropriate ioctl for device";
        case ETXTBSY: return "Text file busy";
        case EFBIG: return "File too large";
        case ENOSPC: return "No space left on device";
        case ESPIPE: return "Illegal seek";
        case EROFS: return "Read-only file system";
        case EMLINK: return "Too many links";
        case EPIPE: return "Broken pipe";
        case EDOM: return "Numerical argument out of domain";
        case ERANGE: return "Numerical result out of range";
        case ENOSYS: return "Function not implemented";
        case ENOTEMPTY: return "Directory not empty";
        case ELOOP: return "Too many levels of symbolic links";
        case ENAMETOOLONG: return "File name too long";
        default:
            /* Unknown error */
            snprintf(buf, sizeof(buf), "Unknown error %d", errnum);
            return buf;
    }
}

int strerror_r(int errnum, char *buf, size_t buflen) {
    const char *msg = strerror(errnum);
    size_t len = strlen(msg);

    if (len >= buflen) {
        if (buflen > 0) {
            memcpy(buf, msg, buflen - 1);
            buf[buflen - 1] = '\0';
        }
        return ERANGE;
    }

    memcpy(buf, msg, len + 1);
    return 0;
}

/* Memory functions */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c) {
            return (void *)p;
        }
        p++;
    }
    return NULL;
}

void *memrchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--) {
        --p;
        if (*p == (unsigned char)c) {
            return (void *)p;
        }
    }
    return NULL;
}

void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen) {
    if (needlelen == 0) {
        return (void *)haystack;
    }
    if (haystacklen < needlelen) {
        return NULL;
    }

    const unsigned char *h = haystack;
    const unsigned char *end = h + haystacklen - needlelen + 1;

    while (h < end) {
        if (memcmp(h, needle, needlelen) == 0) {
            return (void *)h;
        }
        h++;
    }
    return NULL;
}

/* BSD extensions */
void bzero(void *s, size_t n) {
    memset(s, 0, n);
}

void bcopy(const void *src, void *dest, size_t n) {
    memmove(dest, src, n);
}

int bcmp(const void *s1, const void *s2, size_t n) {
    return memcmp(s1, s2, n);
}

char *index(const char *s, int c) {
    return strchr(s, c);
}

char *rindex(const char *s, int c) {
    return strrchr(s, c);
}

/* GNU extensions */
void *mempcpy(void *dest, const void *src, size_t n) {
    return (char *)memcpy(dest, src, n) + n;
}

char *stpcpy(char *dest, const char *src) {
    while ((*dest = *src)) {
        dest++;
        src++;
    }
    return dest;
}

char *stpncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest + i;
}
