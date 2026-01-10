/*
 * stddef.h - Standard definitions
 */

#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;

/* wchar_t - wide character type */
#ifndef __WCHAR_TYPE__
typedef int wchar_t;
#else
typedef __WCHAR_TYPE__ wchar_t;
#endif

#define NULL ((void *)0)

#define offsetof(type, member) ((size_t)&((type *)0)->member)

#endif /* _STDDEF_H */
