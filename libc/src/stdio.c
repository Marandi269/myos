/*
 * stdio.c - Standard I/O functions
 */

#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

/* Write a character to stdout */
int putchar(int c) {
    char ch = c;
    if (write(STDOUT_FILENO, &ch, 1) != 1) {
        return EOF;
    }
    return (unsigned char)ch;
}

/* Write a string to stdout */
int puts(const char *s) {
    size_t len = strlen(s);
    if (write(STDOUT_FILENO, s, len) != (ssize_t)len) {
        return EOF;
    }
    if (write(STDOUT_FILENO, "\n", 1) != 1) {
        return EOF;
    }
    return 0;
}

/* Read a character from stdin */
int getchar(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return EOF;
    }
    return (unsigned char)c;
}

/* Internal: number to string conversion */
static int itoa(char *buf, long long value, int base, int is_signed, int width, char pad) {
    char tmp[32];
    char *p = tmp + sizeof(tmp) - 1;
    int neg = 0;
    unsigned long long uval;
    int len = 0;

    *p = '\0';

    if (is_signed && value < 0) {
        neg = 1;
        uval = -value;
    } else {
        uval = value;
    }

    if (uval == 0) {
        *--p = '0';
        len = 1;
    } else {
        while (uval) {
            int digit = uval % base;
            *--p = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
            uval /= base;
            len++;
        }
    }

    if (neg) {
        *--p = '-';
        len++;
    }

    /* Padding */
    while (len < width) {
        *--p = pad;
        len++;
    }

    memcpy(buf, p, len + 1);
    return len;
}

/* vsnprintf implementation */
int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    char *out = str;
    char *end = str + size - 1;
    const char *fmt = format;

    if (size == 0) return 0;

    while (*fmt && out < end) {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }

        fmt++;  /* Skip '%' */

        /* Parse flags */
        char pad = ' ';
        int width = 0;
        int long_flag = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        /* Parse width */
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        if (*fmt == 'l') {
            long_flag = 1;
            fmt++;
            if (*fmt == 'l') {
                long_flag = 2;
                fmt++;
            }
        }

        /* Parse conversion specifier */
        char buf[32];
        const char *s;
        int len;

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long val;
                if (long_flag == 2) val = va_arg(ap, long long);
                else if (long_flag) val = va_arg(ap, long);
                else val = va_arg(ap, int);
                len = itoa(buf, val, 10, 1, width, pad);
                s = buf;
                break;
            }
            case 'u': {
                unsigned long long val;
                if (long_flag == 2) val = va_arg(ap, unsigned long long);
                else if (long_flag) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                len = itoa(buf, val, 10, 0, width, pad);
                s = buf;
                break;
            }
            case 'x':
            case 'X': {
                unsigned long long val;
                if (long_flag == 2) val = va_arg(ap, unsigned long long);
                else if (long_flag) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                len = itoa(buf, val, 16, 0, width, pad);
                s = buf;
                break;
            }
            case 'p': {
                unsigned long long val = (unsigned long long)va_arg(ap, void *);
                buf[0] = '0';
                buf[1] = 'x';
                itoa(buf + 2, val, 16, 0, 0, '0');
                s = buf;
                len = strlen(buf);
                break;
            }
            case 's':
                s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                len = strlen(s);
                break;
            case 'c':
                buf[0] = (char)va_arg(ap, int);
                buf[1] = '\0';
                s = buf;
                len = 1;
                break;
            case '%':
                buf[0] = '%';
                buf[1] = '\0';
                s = buf;
                len = 1;
                break;
            default:
                buf[0] = '%';
                buf[1] = *fmt;
                buf[2] = '\0';
                s = buf;
                len = 2;
                break;
        }

        /* Copy to output */
        while (len > 0 && out < end) {
            *out++ = *s++;
            len--;
        }

        fmt++;
    }

    *out = '\0';
    return out - str;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, (size_t)-1, format, ap);
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    int fd = stream ? (int)(long)stream : STDOUT_FILENO;
    if (fd == 0) fd = STDIN_FILENO;
    else if (fd == 1) fd = STDOUT_FILENO;
    else if (fd == 2) fd = STDERR_FILENO;
    write(fd, buf, len);
    return len;
}

int vprintf(const char *format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

char *fgets(char *s, int size, FILE *stream) {
    int fd = stream ? (int)(long)stream : STDIN_FILENO;
    if (fd == 0) fd = STDIN_FILENO;

    int i = 0;
    char c;

    while (i < size - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = c;
        if (c == '\n') break;
    }

    s[i] = '\0';
    return s;
}

void perror(const char *s) {
    if (s && *s) {
        write(STDERR_FILENO, s, strlen(s));
        write(STDERR_FILENO, ": ", 2);
    }
    write(STDERR_FILENO, "error\n", 6);
}
