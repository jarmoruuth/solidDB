/*************************************************************************\
**  source       * ssc.h
**  directory    * ss
**  description  * Basic C language definitions for system services.
**               * 
**               * 
**               * Copyright (C) 2006 Solid Information Technology Ltd
\*************************************************************************/
/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; only under version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA
*/


#include "sswindow.h"

#if !defined(SSC_H) && defined(SS_UNIX)
#  include "ssstdio.h"
#  include <sys/types.h>
#endif /* !SSC_H && SS_UNIX */

#ifndef bool_defined
#ifdef bool
# undef bool
#endif
#endif

#ifdef uchar
# undef uchar
#endif

#ifdef ulong
# undef ulong
#endif

#ifdef ushort
# undef ushort
#endif

#ifdef uint
# undef uint
#endif

#ifndef bool_defined
#define bool    int
#endif
#define uchar   unsigned char
#define ulong   unsigned long
#define ushort  unsigned short
#define uint    unsigned int

#ifndef SSC_H
#define SSC_H

/* sslimits.h includes ssenv.h! */
#include "sslimits.h"

#ifndef SS_INCLUDED_FROM_SSMAIN
# ifdef main
#   undef main
# endif
# define main ss_main
#endif /* SS_INCLUDED_FROM_SSMAIN */

/* add ss_ct_assert(sizeof(ss_ptr_as_scalar_t) == sizeof(void*)) ! */
#define ss_ptr_as_scalar_t size_t

/* portable integer types:
 * unsigned & signed 1, 2 and 4 byte
 */
#if (defined(SS_SOLARIS) && defined(SS_SOLARIS_64BIT)) || \
   (defined(SS_LINUX) && defined(SS_LINUX_64BIT)) || \
   (defined(SS_FREEBSD) && defined(SS_FEX_64BIT))
#define ss_byte_t  unsigned char
#define ss_uint1_t ss_byte_t
#define ss_uint2_t unsigned short
#define ss_uint4_t unsigned int 
#define ss_int1_t  signed char
#define ss_int2_t  short int
#define ss_int4_t  int          
#else /* 64bit support? */
#define ss_byte_t  unsigned char
#define ss_uint1_t ss_byte_t
#define ss_uint2_t unsigned short
#define ss_uint4_t unsigned long
#define ss_int1_t  signed char
#define ss_int2_t  short int
#define ss_int4_t  long int
#endif /* 64bit support? */

#if defined(SS_NT)
#  if defined(SS_NT64)
#    define ss_ssize_t __int64
#  else /* SS_NT64 */
#    define ss_ssize_t long
#  endif /* SS_NT64 */
#elif defined(SS_SOLARIS)
#  define ss_ssize_t long
#else /* SS_NT */
#  define ss_ssize_t ssize_t
#endif /* SS_NT */

/* define own size_t for the sake of completeness,
 * "native" size_t should be portable also
 */
#define ss_size_t size_t 

#define TWO_BYTE_T  ss_uint2_t  /* data type for two-byte access */
#define FOUR_BYTE_T ss_uint4_t  /* data type for four-byte access */

/* character types
*/
#define ss_char1_t char       /* normal 8-bit char */
#define ss_char2_t ss_uint2_t /* Unicode */
#define ss_lchar_t wchar_t    /* either 2 or 4 bytes (depending on system) */

#if defined(SS_UNICODE) 
#define ss_char_t ss_char2_t
#define SS_STR(s) L ## s
#else
#define ss_char_t ss_char1_t
#define SS_STR(s) s
#endif

/* Native 64-bit unsigned integer, if it exists. */
#if SS_LONG_BIT == 64
#define SS_NATIVE_UINT8_T  unsigned long
#define SS_NATIVE_INT8_T  long
#elif defined(__GNUC__)
#define SS_NATIVE_UINT8_T  unsigned long long
#define SS_NATIVE_INT8_T  long long
#elif defined(SS_NT) 
#define SS_NATIVE_UINT8_T  unsigned __int64
#define SS_NATIVE_INT8_T  __int64
#elif defined(SS_SOLARIS)
#define SS_NATIVE_UINT8_T u_longlong_t
#define SS_NATIVE_INT8_T __longlong_t
#endif

#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#ifdef SS_DOSX
#  define SS_FAR  /* far  */
#else /* SS_DOSX */
#  define SS_FAR 
#endif /* SS_DOSX */

/* ------------------------------------------------------------------ */
#ifndef MSC

# define __near

#else /* MSC */

# if MSC >= 70

#  pragma warning(disable:4131)
#  pragma warning(disable:4018) /* warning C4018: '<=' : signed/unsigned mismatch */

# else /* MSC 5.1 or 6.0 */

#define __near    near
#define __far     far
#define __cdecl   cdecl
#define __pascal  pascal
#define __export  _export

# endif /* MSC >= 70 */

#endif /* MSC */

/* ------------------------------------------------------------------ */
#if defined(SS_W32) && defined(WCC)     /* Watcom Windows 386 version */

#  define SS_EXPORT         far __pascal
#  define SS_EXPORT_CDECL   far __cdecl
#  define SS_CALLBACK       far __pascal
#  define SS_CDECL          __cdecl
#  define SS_PASCAL16       far __pascal
#  define SS_FAR16PTR       *
#  define SS_CLIBCALLBACK
#  if defined(SS_DLL) 
#    define SS_EXPORT_C     SS_EXPORT
#    define SS_EXPORT_H     SS_EXPORT
#    define SS_DLLCALL      far __pascal
#    define SS_DLLCALL16    far __pascal
#  else /* SS_DLL */
#    define SS_EXPORT_H
#    define SS_EXPORT_C
#    define SS_DLLCALL
#    define SS_DLLCALL16
#  endif /* SS_DLL */

/* ------------------------------------------------------------------ */
#elif defined(SS_WIN)                  /* 16-bit Windows */

#  define SS_EXPORT         __far __pascal __export
#  define SS_EXPORT_CDECL   __far __cdecl  __export
#  define SS_CALLBACK       __far __pascal __export
#  define SS_CDECL          __cdecl
#  define SS_PASCAL16       __far __pascal
#  define SS_FAR16PTR       *
#  ifdef WCC
#    define SS_CLIBCALLBACK
#  else
#    define SS_CLIBCALLBACK SS_CDECL
#  endif
#  if defined(SS_DLL) 
#    define SS_EXPORT_C     SS_EXPORT
#    define SS_EXPORT_H     SS_EXPORT
#    define SS_DLLCALL      __far __pascal
#    define SS_DLLCALL16    __far __pascal
#  else /* SS_DLL */
#    define SS_EXPORT_H
#    define SS_EXPORT_C
#    define SS_DLLCALL
#    define SS_DLLCALL16
#  endif /* SS_DLL */

/* ------------------------------------------------------------------ */
#elif defined(SS_NT)

#  define SS_EXPORT         __stdcall   /* WINAPI */
#  define SS_EXPORT_CDECL   _cdecl      /* WINAPIV */
#  define SS_CALLBACK       __stdcall   /* */
#  define SS_CDECL          _cdecl
#  define SS_EXPORT_C       SS_EXPORT  
#  define SS_EXPORT_H       SS_EXPORT  
#  define SS_CLIBCALLBACK   SS_CDECL
#  define SS_DLLCALL        __stdcall   /* WINAPI */
#  define SS_FAR16PTR       *
#  define SS_DLLCALL16
#  define SS_PASCAL16 

/* ------------------------------------------------------------------ */
#else 

#  define SS_CLIBCALLBACK
#  define SS_EXPORT  
#  define SS_PASCAL16
#  define SS_FAR16PTR *
#  define SS_EXPORT_C SS_EXPORT
#  define SS_EXPORT_H SS_EXPORT
#  define SS_CDECL
#  define SS_DLLCALL
#  define SS_DLLCALL16
#  define SS_EXPORT_CDECL
#  define SS_CALLBACK

#endif

/* ------------------------------------------------------------------ */
#if defined(WCC)
#  define SS_NOTUSED(var) (var = var)
#elif defined(CSET2) || (defined(MSC) && MSC >= 70) || defined(SS_NT)
#  define SS_NOTUSED(var) (var = var)
#else
#  define SS_NOTUSED(var) (void)var
#endif

/* ------------------------------------------------------------------ */

#define SS_CLIBCALL SS_CLIBCALLBACK

#define SS_RETYPE(type, expr) (*(type*)(char*)&(expr))


#define SS_MIN(a, b)   ((a) < (b) ? (a) : (b))
#define SS_MAX(a, b)   ((a) < (b) ? (b) : (a))
#define SS_ABS(a)      ((a) < 0 ? -(a) : (a))


/* 32-bit recursive reduction using SWAR...
   but first step is mapping 2-bit values
   into sum of 2 1-bit values in sneaky way
*/
#define SS_UINT4_COUNT_BITS(bc, u4) \
do {\
        (bc) = (u4);\
        (bc) -= (((bc) >> 1) & 0x55555555);\
        (bc) = ((((bc) >> 2) & 0x33333333) + ((bc) & 0x33333333));\
        (bc) = ((((bc) >> 4) + (bc)) & 0x0f0f0f0f);\
        (bc) += ((bc) >> 8);\
        (bc) += ((bc) >> 16);\
        (bc) &= 0x0000003f;\
} while (0)

#define SS_CONCAT(p1, p2)   p1##p2
#define SS_CONCAT3(p1,p2,p3) p1##p2##p3

#define SS_STRINGIZE(s) #s
#define SS_STRINGIZE_EXPAND_ARG(arg) SS_STRINGIZE(arg)

/* An universal way to calculate needed allocation size for a
 * variable length structure of the form:
 *      typedef struct {
 *          int fld1;
 *          size_t arr_length;
 *          array[1];
 *      } vlenstruct_t;
 * Array length 1 is to keep compiler happy, and the real array
 * size is determined at run time.
 * example:
 * s = SsMemAlloc(SS_SIZEOF_VARLEN_STRUCT(vlenstruct_t, array, length));
 */
#define SS_SIZEOF_VARLEN_STRUCT(type_name,array_name,array_length) \
    ((ss_byte_t*)&((type_name*)NULL)->array_name[array_length] -\
     (ss_byte_t*)NULL)

#endif /* SSC_H */

