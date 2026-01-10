/*
 * ctype.h - Character classification and conversion
 */

#ifndef _CTYPE_H
#define _CTYPE_H

/* Character classification functions */
int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

/* Character conversion functions */
int tolower(int c);
int toupper(int c);
int toascii(int c);

/* Inline implementations for performance */
static inline int __isascii(int c) { return (c >= 0 && c <= 127); }
static inline int __isdigit(int c) { return (c >= '0' && c <= '9'); }
static inline int __islower(int c) { return (c >= 'a' && c <= 'z'); }
static inline int __isupper(int c) { return (c >= 'A' && c <= 'Z'); }
static inline int __isalpha(int c) { return __islower(c) || __isupper(c); }
static inline int __isalnum(int c) { return __isalpha(c) || __isdigit(c); }
static inline int __isspace(int c) { return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'); }
static inline int __isblank(int c) { return (c == ' ' || c == '\t'); }
static inline int __iscntrl(int c) { return ((c >= 0 && c < 32) || c == 127); }
static inline int __isgraph(int c) { return (c > 32 && c < 127); }
static inline int __isprint(int c) { return (c >= 32 && c < 127); }
static inline int __ispunct(int c) { return __isgraph(c) && !__isalnum(c); }
static inline int __isxdigit(int c) { return __isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline int __tolower(int c) { return __isupper(c) ? (c + ('a' - 'A')) : c; }
static inline int __toupper(int c) { return __islower(c) ? (c - ('a' - 'A')) : c; }
static inline int __toascii(int c) { return (c & 0x7F); }

/* Macro versions (can be redefined for locale support) */
#define isascii(c)  __isascii(c)
#define isdigit(c)  __isdigit(c)
#define islower(c)  __islower(c)
#define isupper(c)  __isupper(c)
#define isalpha(c)  __isalpha(c)
#define isalnum(c)  __isalnum(c)
#define isspace(c)  __isspace(c)
#define isblank(c)  __isblank(c)
#define iscntrl(c)  __iscntrl(c)
#define isgraph(c)  __isgraph(c)
#define isprint(c)  __isprint(c)
#define ispunct(c)  __ispunct(c)
#define isxdigit(c) __isxdigit(c)
#define tolower(c)  __tolower(c)
#define toupper(c)  __toupper(c)
#define toascii(c)  __toascii(c)

#endif /* _CTYPE_H */
