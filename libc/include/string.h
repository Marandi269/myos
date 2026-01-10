/*
 * string.h - String operations
 */

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* String length */
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);

/* String copy */
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);

/* String concatenation */
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
size_t strlcat(char *dest, const char *src, size_t size);

/* String comparison */
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int strcoll(const char *s1, const char *s2);

/* String search */
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strcasestr(const char *haystack, const char *needle);
char *strpbrk(const char *s, const char *accept);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);

/* String tokenization */
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);
char *strsep(char **stringp, const char *delim);

/* String duplication */
char *strdup(const char *s);
char *strndup(const char *s, size_t n);

/* String transformation */
size_t strxfrm(char *dest, const char *src, size_t n);

/* Error string */
char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t buflen);

/* Memory functions */
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memchr(const void *s, int c, size_t n);
void *memrchr(const void *s, int c, size_t n);
void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen);

/* BSD extensions */
void bzero(void *s, size_t n);
void bcopy(const void *src, void *dest, size_t n);
int bcmp(const void *s1, const void *s2, size_t n);
char *index(const char *s, int c);
char *rindex(const char *s, int c);

/* GNU extensions */
void *mempcpy(void *dest, const void *src, size_t n);
char *stpcpy(char *dest, const char *src);
char *stpncpy(char *dest, const char *src, size_t n);

#endif /* _STRING_H */
