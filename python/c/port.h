// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_UTILS_H__
#define IBELIE_UTILS_H__

#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"

#define SINGLE_ARG(...) __VA_ARGS__

#undef NULL
#ifdef __cplusplus
#	define NULL 0
#else
#	define NULL ((void *)0)
#endif

#ifndef IblAPI
#	define IblAPI(RTYPE) extern RTYPE
#endif

#ifdef _MSC_VER
#	define IBL_ALIGN(X, FIELD) __declspec(align(X)) FIELD
#else
#	define IBL_ALIGN(X, FIELD) FIELD __attribute__((aligned(X)))
#endif

#define IBL_ALIGNED_SIZE(SIZE) ((size_t)(((SIZE) + (sizeof(size_t) - 1)) & ~(sizeof(size_t) - 1)))

#ifdef _MSC_VER

typedef unsigned __int8 byte;

typedef __int8  int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

#else /* _MSC_VER */

#include <stdint.h>

typedef uint8_t  byte;

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#endif /* _MSC_VER */

#ifndef __cplusplus
#	ifndef true
#		define true  1
#	endif
#	ifndef false
#		define false 0
#	endif
#	ifndef bool
#		define bool uint8
#	endif
#endif


// long long macros to be used because gcc and vc++ use different suffixes,
// and different size specifiers in format strings
#ifdef _MSC_VER
#	define Ibl_LONGLONG(x)  x##I64
#	define Ibl_ULONGLONG(x) x##UI64
#	define Ibl_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#else
#	if __WORDSIZE == 64
#		define Ibl_LONGLONG(x)  x##L
#		define Ibl_ULONGLONG(x) x##UL
#		define Ibl_LL_FORMAT "l"
#	else
#		define Ibl_LONGLONG(x)  x##LL
#		define Ibl_ULONGLONG(x) x##ULL
#		define Ibl_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#	endif
#endif

#ifdef _MSC_VER
#	define Ibl_LITTLE_ENDIAN 1
#else
#	include <sys/param.h> // __BYTE_ORDER
#	if ((defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)) || (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN))
#		define Ibl_LITTLE_ENDIAN 1
#	endif
#endif

#ifndef INT32_MAX
#	define INT32_MAX 0x7FFFFFFF
#endif
#ifndef INT32_MIN
#	define INT32_MIN (-INT32_MAX - 1)
#endif
#ifndef INT64_MAX
#	define INT64_MAX Ibl_LONGLONG(0x7FFFFFFFFFFFFFFF)
#endif
#ifndef INT64_MIN
#	define INT64_MIN (-INT64_MAX - 1)
#endif
#ifndef UINT32_MAX
#	define UINT32_MAX Ibl_ULONGLONG(0xFFFFFFFF)
#endif
#ifndef UINT64_MAX
#	define UINT64_MAX Ibl_ULONGLONG(0xFFFFFFFFFFFFFFFF)
#endif

#define Ibl_Max(a, b) ((a) > (b) ? (a) : (b))
#define Ibl_Min(a, b) ((a) < (b) ? (a) : (b))

#ifndef IblPrint_Info
#	define IblPrint_Info printf
#endif
#ifndef IblPrint_Warn
#	define IblPrint_Warn printf
#endif
#ifndef IblPrint_Err
#	define IblPrint_Err  printf
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _bytes {
	byte*  data;
	size_t length;
} *bytes;

IblAPI(size_t) IblBytesHash    (bytes);
IblAPI(int)    IblBytesCompare (bytes, bytes);

typedef char* string;

IblAPI(size_t) IblStringHash    (string*);
IblAPI(int)    IblStringCompare (string*, string*);

IblAPI(uint64) IblUint64    (byte*);
IblAPI(void)   IblPutUint64 (byte*, uint64);

#define MaxVarintLen 10

IblAPI(int)    IblUvarint    (byte*, size_t, uint64*);
IblAPI(size_t) IblPutUvarint (byte*, uint64);
IblAPI(size_t) IblSizeVarint (uint64);

IblAPI(FILE*)  IblFileOpen          (const string);
IblAPI(bool)   IblFileWriteUint64At (FILE*, uint64, size_t);
IblAPI(bool)   IblFileTruncate      (FILE*, size_t);

typedef size_t* IblBitmap;

#define IblBitmap_DATAMASK   ((size_t)(-1) >> 1)
#define IblBitmap_HEADMASK   (~((size_t)(-1) >> 1))
#define IblBitmap_BITCOUNT   (sizeof(size_t) * 8 - 1)
#define IblBitmap_HASNEXT(p) (~(IblBitmap_HEADMASK & (*((IblBitmap)(p)))))
#define IblBitmap_GET(p, b)  ((((size_t)(1) << (b)) & (*((IblBitmap)(p)))) ? true : false)
#define IblBitmap_SET0(p, b) (*((IblBitmap)(p)) &= ~((size_t)(1) << (b)))
#define IblBitmap_SET1(p, b) (*((IblBitmap)(p)) |= ((size_t)(1) << (b)))

IblAPI(IblBitmap) IblBitmap_New(size_t);
IblAPI(bool)      IblBitmap_Get(IblBitmap, size_t);
IblAPI(bool)      IblBitmap_Set(IblBitmap, size_t, bool);

#ifdef __cplusplus
}
#endif

#endif /* IBELIE_UTILS_H__ */
