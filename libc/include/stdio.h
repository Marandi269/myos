/*
 * stdio.h - Standard I/O
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* Standard streams */
#define stdin  ((FILE *)0)
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)

/* FILE type (simplified - just fd for now) */
typedef struct {
    int fd;
} FILE;

/* Constants */
#define EOF (-1)
#define BUFSIZ 1024

/* Output functions */
int putchar(int c);
int puts(const char *s);
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
char *gets(char *s);
char *fgets(char *s, int size, FILE *stream);

/* File operations (simplified) */
FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fflush(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);

/* Error output */
void perror(const char *s);

#endif /* _STDIO_H */
