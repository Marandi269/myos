/*
 * stdio.c - Standard I/O functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/* Standard streams */
static FILE _stdin_file = { .fd = STDIN_FILENO, .flags = _FILE_READ, .ungetc_buf = -1 };
static FILE _stdout_file = { .fd = STDOUT_FILENO, .flags = _FILE_WRITE | _FILE_UNBUF, .ungetc_buf = -1 };
static FILE _stderr_file = { .fd = STDERR_FILENO, .flags = _FILE_WRITE | _FILE_UNBUF, .ungetc_buf = -1 };

FILE *stdin = &_stdin_file;
FILE *stdout = &_stdout_file;
FILE *stderr = &_stderr_file;

/* Internal: get fd from FILE */
static int _get_fd(FILE *stream) {
    if (!stream) return -1;
    return stream->fd;
}

/* Write a character to a stream */
int fputc(int c, FILE *stream) {
    unsigned char ch = (unsigned char)c;
    int fd = _get_fd(stream);
    if (fd < 0) {
        errno = EBADF;
        return EOF;
    }
    if (write(fd, &ch, 1) != 1) {
        if (stream) stream->flags |= _FILE_ERROR;
        return EOF;
    }
    return ch;
}

int putc(int c, FILE *stream) {
    return fputc(c, stream);
}

/* Write a character to stdout */
int putchar(int c) {
    return fputc(c, stdout);
}

/* Write a string to a stream (without newline) */
int fputs(const char *s, FILE *stream) {
    int fd = _get_fd(stream);
    if (fd < 0) {
        errno = EBADF;
        return EOF;
    }
    size_t len = strlen(s);
    if (write(fd, s, len) != (ssize_t)len) {
        if (stream) stream->flags |= _FILE_ERROR;
        return EOF;
    }
    return 0;
}

/* Write a string to stdout (with newline) */
int puts(const char *s) {
    if (fputs(s, stdout) == EOF) return EOF;
    if (fputc('\n', stdout) == EOF) return EOF;
    return 0;
}

/* Read a character from a stream */
int fgetc(FILE *stream) {
    int fd;
    unsigned char c;

    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    /* Check for pushed-back character */
    if (stream->ungetc_buf != -1) {
        int ch = stream->ungetc_buf;
        stream->ungetc_buf = -1;
        return ch;
    }

    fd = stream->fd;
    if (fd < 0) {
        errno = EBADF;
        return EOF;
    }

    ssize_t n = read(fd, &c, 1);
    if (n <= 0) {
        if (n == 0) stream->flags |= _FILE_EOF;
        else stream->flags |= _FILE_ERROR;
        return EOF;
    }
    return c;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

/* Read a character from stdin */
int getchar(void) {
    return fgetc(stdin);
}

/* Push a character back to stream */
int ungetc(int c, FILE *stream) {
    if (!stream || c == EOF) {
        return EOF;
    }
    if (stream->ungetc_buf != -1) {
        /* Already have a pushed-back character */
        return EOF;
    }
    stream->ungetc_buf = (unsigned char)c;
    stream->flags &= ~_FILE_EOF;  /* Clear EOF flag */
    return c;
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
        int left_justify = 0;
        int precision = -1;

        /* Parse flags */
        while (*fmt == '-' || *fmt == '0' || *fmt == '+' || *fmt == ' ' || *fmt == '#') {
            if (*fmt == '-') left_justify = 1;
            else if (*fmt == '0' && !left_justify) pad = '0';
            fmt++;
        }

        /* Parse width */
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left_justify = 1;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Parse precision */
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        /* Parse length modifier */
        if (*fmt == 'l') {
            long_flag = 1;
            fmt++;
            if (*fmt == 'l') {
                long_flag = 2;
                fmt++;
            }
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') fmt++;
        } else if (*fmt == 'z' || *fmt == 't') {
            long_flag = 1;
            fmt++;
        }

        /* Parse conversion specifier */
        char buf[64];
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
            case 'o': {
                unsigned long long val;
                if (long_flag == 2) val = va_arg(ap, unsigned long long);
                else if (long_flag) val = va_arg(ap, unsigned long);
                else val = va_arg(ap, unsigned int);
                len = itoa(buf, val, 8, 0, width, pad);
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
                if (precision >= 0 && len > precision) len = precision;
                break;
            case 'c':
                buf[0] = (char)va_arg(ap, int);
                buf[1] = '\0';
                s = buf;
                len = 1;
                break;
            case 'n': {
                int *np = va_arg(ap, int *);
                if (np) *np = out - str;
                s = "";
                len = 0;
                break;
            }
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

        /* Handle width and justification */
        int padding = width - len;
        if (!left_justify && padding > 0) {
            while (padding-- > 0 && out < end) {
                *out++ = ' ';
            }
        }

        /* Copy to output */
        while (len > 0 && out < end) {
            *out++ = *s++;
            len--;
        }

        if (left_justify && padding > 0) {
            while (padding-- > 0 && out < end) {
                *out++ = ' ';
            }
        }

        fmt++;
    }

    *out = '\0';
    return out - str;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 4096, format, ap);
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
    int fd = _get_fd(stream);
    if (fd < 0) fd = STDOUT_FILENO;
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
    int i = 0;
    int c;

    if (!s || size <= 0 || !stream) return NULL;

    while (i < size - 1) {
        c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }

    s[i] = '\0';
    return s;
}

char *gets(char *s) {
    /* Deprecated and unsafe, but implement for compatibility */
    return fgets(s, 4096, stdin);
}

/* File open */
FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;
    int file_flags = 0;

    if (!pathname || !mode) {
        errno = EINVAL;
        return NULL;
    }

    /* Parse mode */
    switch (mode[0]) {
        case 'r':
            flags = O_RDONLY;
            file_flags = _FILE_READ;
            if (mode[1] == '+' || (mode[1] && mode[2] == '+')) {
                flags = O_RDWR;
                file_flags |= _FILE_WRITE;
            }
            break;
        case 'w':
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            file_flags = _FILE_WRITE;
            if (mode[1] == '+' || (mode[1] && mode[2] == '+')) {
                flags = O_RDWR | O_CREAT | O_TRUNC;
                file_flags |= _FILE_READ;
            }
            break;
        case 'a':
            flags = O_WRONLY | O_CREAT | O_APPEND;
            file_flags = _FILE_WRITE | _FILE_APPEND;
            if (mode[1] == '+' || (mode[1] && mode[2] == '+')) {
                flags = O_RDWR | O_CREAT | O_APPEND;
                file_flags |= _FILE_READ;
            }
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    int fd = open(pathname, flags, 0666);
    if (fd < 0) {
        return NULL;
    }

    FILE *fp = malloc(sizeof(FILE));
    if (!fp) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    fp->fd = fd;
    fp->flags = file_flags;
    fp->ungetc_buf = -1;
    fp->buf = NULL;
    fp->buf_size = 0;
    fp->buf_pos = 0;
    fp->buf_len = 0;

    return fp;
}

/* Create FILE from existing fd */
FILE *fdopen(int fd, const char *mode) {
    if (fd < 0 || !mode) {
        errno = EINVAL;
        return NULL;
    }

    int file_flags = 0;
    switch (mode[0]) {
        case 'r':
            file_flags = _FILE_READ;
            if (mode[1] == '+') file_flags |= _FILE_WRITE;
            break;
        case 'w':
            file_flags = _FILE_WRITE;
            if (mode[1] == '+') file_flags |= _FILE_READ;
            break;
        case 'a':
            file_flags = _FILE_WRITE | _FILE_APPEND;
            if (mode[1] == '+') file_flags |= _FILE_READ;
            break;
        default:
            errno = EINVAL;
            return NULL;
    }

    FILE *fp = malloc(sizeof(FILE));
    if (!fp) {
        errno = ENOMEM;
        return NULL;
    }

    fp->fd = fd;
    fp->flags = file_flags;
    fp->ungetc_buf = -1;
    fp->buf = NULL;
    fp->buf_size = 0;
    fp->buf_pos = 0;
    fp->buf_len = 0;

    return fp;
}

/* Reopen a file */
FILE *freopen(const char *pathname, const char *mode, FILE *stream) {
    if (!stream) {
        return fopen(pathname, mode);
    }

    fclose(stream);

    FILE *new_fp = fopen(pathname, mode);
    if (new_fp) {
        /* Copy contents to original stream pointer location */
        *stream = *new_fp;
        free(new_fp);
        return stream;
    }
    return NULL;
}

/* Close file */
int fclose(FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return EOF;
    }

    /* Don't close standard streams */
    if (stream == stdin || stream == stdout || stream == stderr) {
        return 0;
    }

    int ret = 0;
    if (stream->fd >= 0) {
        ret = close(stream->fd);
    }

    if (stream->buf) {
        free(stream->buf);
    }
    free(stream);

    return ret < 0 ? EOF : 0;
}

/* Read from file */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        return 0;
    }

    size_t total = size * nmemb;
    size_t done = 0;
    char *p = (char *)ptr;

    /* Handle ungetc first */
    if (stream->ungetc_buf != -1 && done < total) {
        *p++ = (char)stream->ungetc_buf;
        stream->ungetc_buf = -1;
        done++;
    }

    while (done < total) {
        ssize_t n = read(stream->fd, p, total - done);
        if (n <= 0) {
            if (n == 0) stream->flags |= _FILE_EOF;
            else stream->flags |= _FILE_ERROR;
            break;
        }
        done += n;
        p += n;
    }

    return done / size;
}

/* Write to file */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || !stream || size == 0 || nmemb == 0) {
        return 0;
    }

    size_t total = size * nmemb;
    size_t done = 0;
    const char *p = (const char *)ptr;

    while (done < total) {
        ssize_t n = write(stream->fd, p, total - done);
        if (n <= 0) {
            stream->flags |= _FILE_ERROR;
            break;
        }
        done += n;
        p += n;
    }

    return done / size;
}

/* Flush stream */
int fflush(FILE *stream) {
    if (!stream) {
        /* Flush all streams - just return success for now */
        return 0;
    }
    /* No buffering implemented, so nothing to flush */
    return 0;
}

/* Check EOF */
int feof(FILE *stream) {
    if (!stream) return 0;
    return (stream->flags & _FILE_EOF) ? 1 : 0;
}

/* Check error */
int ferror(FILE *stream) {
    if (!stream) return 0;
    return (stream->flags & _FILE_ERROR) ? 1 : 0;
}

/* Clear error and EOF flags */
void clearerr(FILE *stream) {
    if (stream) {
        stream->flags &= ~(_FILE_EOF | _FILE_ERROR);
    }
}

/* Get file descriptor */
int fileno(FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return -1;
    }
    return stream->fd;
}

/* File positioning */
int fseek(FILE *stream, long offset, int whence) {
    if (!stream) {
        errno = EBADF;
        return -1;
    }

    /* Clear ungetc buffer */
    stream->ungetc_buf = -1;

    /* Clear EOF flag */
    stream->flags &= ~_FILE_EOF;

    long result = lseek(stream->fd, offset, whence);
    if (result < 0) {
        stream->flags |= _FILE_ERROR;
        return -1;
    }
    return 0;
}

long ftell(FILE *stream) {
    if (!stream) {
        errno = EBADF;
        return -1;
    }
    return lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream) {
    if (stream) {
        fseek(stream, 0, SEEK_SET);
        stream->flags &= ~_FILE_ERROR;
    }
}

int fgetpos(FILE *stream, long *pos) {
    if (!stream || !pos) {
        errno = EINVAL;
        return -1;
    }
    *pos = ftell(stream);
    return (*pos < 0) ? -1 : 0;
}

int fsetpos(FILE *stream, const long *pos) {
    if (!stream || !pos) {
        errno = EINVAL;
        return -1;
    }
    return fseek(stream, *pos, SEEK_SET);
}

/* Buffer control */
void setbuf(FILE *stream, char *buf) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    (void)buf;
    (void)size;
    if (!stream) {
        errno = EBADF;
        return -1;
    }
    if (mode == _IONBF) {
        stream->flags |= _FILE_UNBUF;
    } else {
        stream->flags &= ~_FILE_UNBUF;
    }
    return 0;
}

/* Temporary files - stub implementations */
FILE *tmpfile(void) {
    /* Not implemented */
    errno = ENOSYS;
    return NULL;
}

char *tmpnam(char *s) {
    static char buf[20];
    static int counter = 0;
    char *p = s ? s : buf;
    snprintf(p, 20, "/tmp/tmp%d", counter++);
    return p;
}

/* Remove/rename */
int remove(const char *pathname) {
    return syscall1(SYS_unlink, (long)pathname);
}

int rename(const char *oldpath, const char *newpath) {
    /* SYS_rename = 82 on Linux x86_64 */
    return syscall2(82, (long)oldpath, (long)newpath);
}

/* Error output */
void perror(const char *s) {
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    /* Simple error message based on errno */
    const char *msg;
    switch (errno) {
        case 0: msg = "Success"; break;
        case EPERM: msg = "Operation not permitted"; break;
        case ENOENT: msg = "No such file or directory"; break;
        case ESRCH: msg = "No such process"; break;
        case EINTR: msg = "Interrupted system call"; break;
        case EIO: msg = "I/O error"; break;
        case ENXIO: msg = "No such device or address"; break;
        case E2BIG: msg = "Argument list too long"; break;
        case ENOEXEC: msg = "Exec format error"; break;
        case EBADF: msg = "Bad file descriptor"; break;
        case ECHILD: msg = "No child processes"; break;
        case EAGAIN: msg = "Resource temporarily unavailable"; break;
        case ENOMEM: msg = "Cannot allocate memory"; break;
        case EACCES: msg = "Permission denied"; break;
        case EFAULT: msg = "Bad address"; break;
        case EBUSY: msg = "Device or resource busy"; break;
        case EEXIST: msg = "File exists"; break;
        case ENODEV: msg = "No such device"; break;
        case ENOTDIR: msg = "Not a directory"; break;
        case EISDIR: msg = "Is a directory"; break;
        case EINVAL: msg = "Invalid argument"; break;
        case EMFILE: msg = "Too many open files"; break;
        case ENOSPC: msg = "No space left on device"; break;
        case ESPIPE: msg = "Illegal seek"; break;
        case EPIPE: msg = "Broken pipe"; break;
        case ENOSYS: msg = "Function not implemented"; break;
        default: msg = "Unknown error"; break;
    }
    fputs(msg, stderr);
    fputc('\n', stderr);
}

/* Scanf family - minimal implementations */
int vsscanf(const char *str, const char *format, va_list ap) {
    const char *s = str;
    const char *fmt = format;
    int count = 0;

    while (*fmt && *s) {
        /* Skip whitespace in format */
        if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
            while (*s == ' ' || *s == '\t' || *s == '\n') s++;
            fmt++;
            continue;
        }

        if (*fmt != '%') {
            if (*fmt != *s) break;
            fmt++;
            s++;
            continue;
        }

        fmt++;  /* Skip '%' */

        /* Check for assignment suppression */
        int suppress = 0;
        if (*fmt == '*') {
            suppress = 1;
            fmt++;
        }

        /* Parse width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (width == 0) width = -1;  /* No limit */

        /* Parse length modifier */
        int long_flag = 0;
        if (*fmt == 'l') {
            long_flag = 1;
            fmt++;
            if (*fmt == 'l') {
                long_flag = 2;
                fmt++;
            }
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') fmt++;
        }

        /* Skip leading whitespace for most conversions */
        if (*fmt != 'c' && *fmt != '[' && *fmt != 'n') {
            while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long val = 0;
                int neg = 0;
                if (*s == '-') { neg = 1; s++; }
                else if (*s == '+') s++;
                while (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    s++;
                }
                if (neg) val = -val;
                if (!suppress) {
                    if (long_flag == 2) *va_arg(ap, long long *) = val;
                    else if (long_flag) *va_arg(ap, long *) = val;
                    else *va_arg(ap, int *) = (int)val;
                    count++;
                }
                break;
            }
            case 'u': {
                unsigned long long val = 0;
                while (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    s++;
                }
                if (!suppress) {
                    if (long_flag == 2) *va_arg(ap, unsigned long long *) = val;
                    else if (long_flag) *va_arg(ap, unsigned long *) = val;
                    else *va_arg(ap, unsigned int *) = (unsigned int)val;
                    count++;
                }
                break;
            }
            case 'x':
            case 'X': {
                unsigned long long val = 0;
                if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
                while (1) {
                    if (*s >= '0' && *s <= '9') val = val * 16 + (*s - '0');
                    else if (*s >= 'a' && *s <= 'f') val = val * 16 + (*s - 'a' + 10);
                    else if (*s >= 'A' && *s <= 'F') val = val * 16 + (*s - 'A' + 10);
                    else break;
                    s++;
                }
                if (!suppress) {
                    if (long_flag == 2) *va_arg(ap, unsigned long long *) = val;
                    else if (long_flag) *va_arg(ap, unsigned long *) = val;
                    else *va_arg(ap, unsigned int *) = (unsigned int)val;
                    count++;
                }
                break;
            }
            case 's': {
                char *dest = suppress ? NULL : va_arg(ap, char *);
                int i = 0;
                while (*s && *s != ' ' && *s != '\t' && *s != '\n' && (width < 0 || i < width)) {
                    if (dest) dest[i] = *s;
                    i++;
                    s++;
                }
                if (dest) {
                    dest[i] = '\0';
                    count++;
                }
                break;
            }
            case 'c': {
                if (!suppress) {
                    char *dest = va_arg(ap, char *);
                    *dest = *s;
                    count++;
                }
                s++;
                break;
            }
            case 'n': {
                if (!suppress) {
                    int *dest = va_arg(ap, int *);
                    *dest = s - str;
                }
                break;
            }
            case '%':
                if (*s != '%') return count;
                s++;
                break;
        }
        fmt++;
    }

    return count;
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsscanf(str, format, ap);
    va_end(ap);
    return ret;
}

int vfscanf(FILE *stream, const char *format, va_list ap) {
    char buf[1024];
    if (!fgets(buf, sizeof(buf), stream)) return EOF;
    return vsscanf(buf, format, ap);
}

int fscanf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfscanf(stream, format, ap);
    va_end(ap);
    return ret;
}

int vscanf(const char *format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

int scanf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vscanf(format, ap);
    va_end(ap);
    return ret;
}
