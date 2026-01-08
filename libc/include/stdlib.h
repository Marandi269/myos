/*
 * stdlib.h - Standard library
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

/* Memory allocation */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

/* Process control */
void exit(int status);
void abort(void);

/* String conversion */
int atoi(const char *str);
long atol(const char *str);
long strtol(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);

/* Environment */
char *getenv(const char *name);

/* NULL */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Exit status */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif /* _STDLIB_H */
