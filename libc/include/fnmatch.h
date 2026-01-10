/*
 * fnmatch.h - Filename pattern matching
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

/* Flags for fnmatch */
#define FNM_NOESCAPE    (1 << 0)    /* Disable backslash escaping */
#define FNM_PATHNAME    (1 << 1)    /* Slash must be matched by slash */
#define FNM_PERIOD      (1 << 2)    /* Leading period must be matched */
#define FNM_LEADING_DIR (1 << 3)    /* Ignore trailing characters after match */
#define FNM_CASEFOLD    (1 << 4)    /* Case-insensitive matching */

/* Return value for no match */
#define FNM_NOMATCH     1

/* Match a filename against a pattern */
int fnmatch(const char *pattern, const char *string, int flags);

#endif /* _FNMATCH_H */
