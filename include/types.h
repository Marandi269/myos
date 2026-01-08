/*
 * types.h - Basic type definitions for MyOS
 */

#ifndef _TYPES_H
#define _TYPES_H

/* Unsigned integer types */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Signed integer types */
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* Size types */
typedef uint64_t size_t;
typedef int64_t  ssize_t;
typedef uint64_t uintptr_t;
typedef int64_t  intptr_t;

/* Boolean type */
typedef int bool;
#define true  1
#define false 0

/* NULL pointer */
#define NULL ((void*)0)

#endif /* _TYPES_H */
