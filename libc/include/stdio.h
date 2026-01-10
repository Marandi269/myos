/*
 * stdio.h - Standard I/O
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* Constants */
#define EOF (-1)
#define BUFSIZ 1024
#define FILENAME_MAX 4096
#define FOPEN_MAX 256
#define _IOFBF 0    /* Full buffering */
#define _IOLBF 1    /* Line buffering */
#define _IONBF 2    /* No buffering */

/* Seek constants */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* FILE structure flags */
#define _FILE_READ   0x01
#define _FILE_WRITE  0x02
#define _FILE_APPEND 0x04
#define _FILE_EOF    0x08
#define _FILE_ERROR  0x10
#define _FILE_UNBUF  0x20

/* FILE type */
typedef struct _FILE {
    int fd;
    int flags;
    int ungetc_buf;     /* For ungetc(), -1 if empty */
    unsigned char *buf;
    size_t buf_size;
    size_t buf_pos;
    size_t buf_len;
} FILE;

/* Standard streams (defined in stdio.c) */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Output functions */
int putchar(int c);
int putc(int c, FILE *stream);
int fputc(int c, FILE *stream);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/* Input functions */
int getchar(void);
int getc(FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
char *gets(char *s);
char *fgets(char *s, int size, FILE *stream);
int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *str, const char *format, ...);
int vscanf(const char *format, va_list ap);
int vfscanf(FILE *stream, const char *format, va_list ap);
int vsscanf(const char *str, const char *format, va_list ap);

/* File operations */
FILE *fopen(const char *pathname, const char *mode);
FILE *fdopen(int fd, const char *mode);
FILE *freopen(const char *pathname, const char *mode, FILE *stream);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fflush(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);

/* File positioning */
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, long *pos);
int fsetpos(FILE *stream, const long *pos);

/* Buffer control */
void setbuf(FILE *stream, char *buf);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

/* Temporary files */
FILE *tmpfile(void);
char *tmpnam(char *s);

/* Remove/rename */
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

/* Error output */
void perror(const char *s);

#endif /* _STDIO_H */
