/*
 * fnmatch.c - Filename pattern matching
 */

#include <fnmatch.h>
#include <string.h>

static int tolower_char(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

static int match_char(char p, char s, int flags) {
    if (flags & FNM_CASEFOLD) {
        return tolower_char(p) == tolower_char(s);
    }
    return p == s;
}

int fnmatch(const char *pattern, const char *string, int flags) {
    const char *p = pattern;
    const char *s = string;

    while (*p) {
        switch (*p) {
            case '?':
                /* Match any single character */
                if (!*s) {
                    return FNM_NOMATCH;
                }
                if ((flags & FNM_PATHNAME) && *s == '/') {
                    return FNM_NOMATCH;
                }
                if ((flags & FNM_PERIOD) && *s == '.' &&
                    (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                    return FNM_NOMATCH;
                }
                p++;
                s++;
                break;

            case '*':
                /* Match any sequence of characters */
                /* Skip multiple asterisks */
                while (*p == '*') p++;

                /* Trailing * matches everything (except pathname constraints) */
                if (!*p) {
                    if (flags & FNM_PATHNAME) {
                        return strchr(s, '/') ? FNM_NOMATCH : 0;
                    }
                    return 0;
                }

                /* Try to match the rest of the pattern */
                while (*s) {
                    if ((flags & FNM_PATHNAME) && *s == '/') {
                        break;
                    }
                    if ((flags & FNM_PERIOD) && *s == '.' &&
                        (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                        break;
                    }
                    if (fnmatch(p, s, flags & ~FNM_PERIOD) == 0) {
                        return 0;
                    }
                    s++;
                }
                return fnmatch(p, s, flags & ~FNM_PERIOD);

            case '[':
                /* Character class */
                if (!*s) {
                    return FNM_NOMATCH;
                }
                if ((flags & FNM_PATHNAME) && *s == '/') {
                    return FNM_NOMATCH;
                }
                if ((flags & FNM_PERIOD) && *s == '.' &&
                    (s == string || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                    return FNM_NOMATCH;
                }

                p++;
                int negate = 0;
                if (*p == '!' || *p == '^') {
                    negate = 1;
                    p++;
                }

                int match = 0;
                char c = *s;
                if (flags & FNM_CASEFOLD) {
                    c = tolower_char(c);
                }

                while (*p && *p != ']') {
                    char start = *p++;
                    if (flags & FNM_CASEFOLD) {
                        start = tolower_char(start);
                    }

                    if (*p == '-' && p[1] && p[1] != ']') {
                        /* Range */
                        p++;
                        char end = *p++;
                        if (flags & FNM_CASEFOLD) {
                            end = tolower_char(end);
                        }
                        if (c >= start && c <= end) {
                            match = 1;
                        }
                    } else {
                        if (c == start) {
                            match = 1;
                        }
                    }
                }

                if (*p == ']') p++;

                if (negate) match = !match;
                if (!match) {
                    return FNM_NOMATCH;
                }
                s++;
                break;

            case '\\':
                /* Escape character */
                if (!(flags & FNM_NOESCAPE) && p[1]) {
                    p++;
                }
                /* Fall through to default case */
                /* FALLTHROUGH */

            default:
                /* Literal character match */
                if (!*s) {
                    return FNM_NOMATCH;
                }
                if (!match_char(*p, *s, flags)) {
                    return FNM_NOMATCH;
                }
                p++;
                s++;
                break;
        }
    }

    /* Pattern exhausted */
    if (*s) {
        if (flags & FNM_LEADING_DIR) {
            return (*s == '/') ? 0 : FNM_NOMATCH;
        }
        return FNM_NOMATCH;
    }

    return 0;
}
