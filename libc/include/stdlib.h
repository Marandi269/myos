/*
 * stdlib.h - Standard library
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

/* NULL */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Exit status */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* Random number generation */
#define RAND_MAX 2147483647

/* Memory allocation */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void *aligned_alloc(size_t alignment, size_t size);

/* Process control */
void exit(int status);
void _Exit(int status);
void abort(void);
int atexit(void (*func)(void));
int on_exit(void (*func)(int, void *), void *arg);

/* String conversion */
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);
long strtol(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);
long long strtoll(const char *str, char **endptr, int base);
unsigned long long strtoull(const char *str, char **endptr, int base);
/* Floating point functions - stub implementations (return 0) */
/* Real floating point requires SSE which is disabled */
long strtod(const char *str, char **endptr);
long strtof(const char *str, char **endptr);
long strtold(const char *str, char **endptr);

/* Environment */
char *getenv(const char *name);
int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int clearenv(void);

/* Random number generation */
int rand(void);
void srand(unsigned int seed);
int rand_r(unsigned int *seedp);

/* Integer arithmetic */
int abs(int j);
long labs(long j);
long long llabs(long long j);

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

/* Searching and sorting */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

/* Multibyte/wide character conversion (stubs) */
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *pwc, const char *s, size_t n);
int wctomb(char *s, wchar_t wchar);
size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
size_t wcstombs(char *dest, const wchar_t *src, size_t n);

/* Temporary file generation */
char *mktemp(char *template);
int mkstemp(char *template);
char *mkdtemp(char *template);

/* Pseudo-terminal */
int posix_openpt(int flags);
int grantpt(int fd);
int unlockpt(int fd);
char *ptsname(int fd);

/* Realpath */
char *realpath(const char *path, char *resolved_path);

/* System */
int system(const char *command);

#endif /* _STDLIB_H */
