/*************************************************************************\
**  source       * sslimits.h
**  directory    * ss
**  description  * limits.h & other architecture-dependent constants
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


#ifndef SSLIMITS_H
#define SSLIMITS_H

#include "ssenv.h"

#ifdef SS_MT
#define SS_MAXTHREADS 512
#else
#define SS_MAXTHREADS 1
#endif

/* machine architecture **************************************************/

#if (defined(SS_WIN) && !defined(SS_W32)) || (defined(SS_DOS) && !defined(SS_DOS4GW)) 
#define INTEL_16BIT
#endif

#if (defined(SS_W32) && !defined(SS_NT64)) || defined(SS_DOS4GW) || (defined(SS_FREEBSD) && !defined(SS_FEX_64BIT)) || defined(SS_BSI)
#define INTEL_32BIT
#endif

#if (defined(SS_NT) && !defined(SS_NT64)) || defined(SS_SCO)
#define INTEL_32BIT
#endif

#if defined(SS_LINUX) && !defined(SS_LINUX_64BIT)
#if defined(SS_PPC) 
#define PPC_32BIT
#else
#define INTEL_32BIT
#endif
#endif

# if defined(SS_PPC)
#  define PPC_32BIT
# endif

#if defined(INTEL_16BIT)

#  include <limits.h>

#  define INT_BIT 16                      /* number of bits in an int */
#  define LONG_BIT 32                     /* number of bits in a long */
#  define SS_POINTER_SIZE 4               /* pointer size in bytes */
#  define SS_ALIGN_T ss_uint4_t           /* 4 byte alignment (for speed) */

#  define SS_LSB1ST                       /* byte order: LSB first */
#  define UNALIGNED_LOAD                  /* can do unaligned memory accesses */
#  define WORD_SIZE 2                     /* size of efficient loads & stores */

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)60 * (size_t)1024)

#elif defined(INTEL_32BIT) 

#  include <limits.h>
#  define INT_BIT 32

#  ifndef LONG_BIT
     /* LONG_BIT is defined on some platforms. */
#    define LONG_BIT 32
#  endif

#  define SS_POINTER_SIZE 4
#  define SS_LSB1ST
#  define UNALIGNED_LOAD
#  define WORD_SIZE 4

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif (defined(UNIX) && defined(BANYAN))

#  define CHAR_BIT	(8)
#  define CHAR_MAX	UCHAR_MAX
#  define CHAR_MIN	(0)
#  define INT_MAX   (2147483647)        /* maximum signed int value    */
#  define INT_MIN   (-(INT_MAX + 1))    /* minimum signed int value    */
#  define LONG_MAX  INT_MAX             /* maximum signed long value   */
#  define LONG_MIN  INT_MIN             /* minimum signed long value   */
#  define SCHAR_MAX	(127)
#  define SCHAR_MIN	(-(SCHAR_MAX + 1))
#  define SHRT_MAX	(32767)
#  define SHRT_MIN	(-(SHRT_MAX + 1))
#  define UCHAR_MAX	(255)
#  define UINT_MAX  (4294967295)        /* maximum unsigned int value  */
#  define ULONG_MAX (UINT_MAX)          /* maximum unsigned long value */
#  define USHRT_MAX	(65535)

#  define INT_BIT 32
#  define LONG_BIT 32
#  define SS_POINTER_SIZE 4

#  define SS_LSB1ST
#  define UNALIGNED_LOAD
#  define WORD_SIZE 4

#  define SIGN_EXTEND_TO_INT(byte) ((int)(char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined(PPC_32BIT) 
#  include <limits.h>

#  define INT_BIT 32
#  define LONG_BIT 32
#  define SS_POINTER_SIZE 4

#  undef SS_LSB1ST
#if 0 /* Unaligned load does work, but is not fast */
#  define UNALIGNED_LOAD
#endif
#  define WORD_SIZE 4

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif ((defined(SS_SOLARIS) && !defined(SS_SOLARIS_64BIT)))
#  include <limits.h>

#  define INT_BIT 32
#  define LONG_BIT 32
#  define SS_POINTER_SIZE 4

#if defined(SS_SPARC)
#  undef  SS_LSB1ST
#  define SS_ALIGN_T double
#  undef  UNALIGNED_LOAD
#else 
#  define SS_LSB1ST
#  define UNALIGNED_LOAD      
#  define SS_ALIGN_T ss_uint4_t
#endif

#  define WORD_SIZE 4

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif (defined(SS_SOLARIS) && defined(SS_SOLARIS_64BIT))

#  include <limits.h>

#  define INT_BIT 32
#  define LONG_BIT 64
#  define SS_POINTER_SIZE 8

#if defined(SS_SPARC)
#  undef  SS_LSB1ST
#  undef  UNALIGNED_LOAD
#else 
#  define SS_LSB1ST
#  define UNALIGNED_LOAD      
#endif

#  define WORD_SIZE 8

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined(SS_FEX_64BIT)

#  include <limits.h>

#  define INT_BIT 32
# ifndef LONG_BIT
#  define LONG_BIT 64
# endif
#  define SS_POINTER_SIZE 8
#  define SS_LSB1ST
#  define UNALIGNED_LOAD
#  define WORD_SIZE 8
#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined (SS_NT) 

/* Same as Intel NT, except removed UNALIGNED_LOAD */
#  include <limits.h>

#  define INT_BIT 32
#  define LONG_BIT 32
#  define SS_POINTER_SIZE 4

#  define SS_LSB1ST                       /* byte order: LSB first */
/* #  define UNALIGNED_LOAD */
#  define WORD_SIZE 4

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined (SS_LINUX) 

#  include <limits.h>

#  define INT_BIT 32 
#  define LONG_BIT 64
#  define SS_POINTER_SIZE 8

#  define SS_LSB1ST     /* byte order: LSB first */
#  define WORD_SIZE 8

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined (SS_LINUX) && defined(SS_LINUX_64BIT) 

#  include <limits.h>

#  define INT_BIT 32 
#  define LONG_BIT 64
#  define SS_POINTER_SIZE 8

#  define SS_LSB1ST         /* byte order: LSB first */
#  define UNALIGNED_LOAD      
#  define WORD_SIZE 8

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#elif defined (SS_NT64) 

#  include <limits.h>

#  define INT_BIT 32 
#  define LONG_BIT 32
#  define SS_POINTER_SIZE 8

#  define SS_LSB1ST         /* byte order: LSB first */
#  define UNALIGNED_LOAD      
#  define WORD_SIZE 4
#  define SS_ALIGN_T double

#  define SIGN_EXTEND_TO_INT(byte) ((int)(signed char)(byte))
#  define ZERO_EXTEND_TO_INT(byte) ((int)(unsigned char)(byte))
#  define SIGN_EXTEND_TO_LONG(byte) ((long)(signed char)(byte))
#  define ZERO_EXTEND_TO_LONG(byte) ((long)(unsigned char)(byte))

#  define SS_MAXALLOCSIZE   ((size_t)(512L * 1024L * 1024))

#else

#error   Machine architecture unknown!

#endif

/* macros for memory access **********************************************/


/* In the following macros, bytes, words, etc. are all unsigned, and the
   values passed for them are assumed to be positive.
   'word' means two bytes. 'dword' is four bytes.

   These macros are in this file, because it is anticipated, that they
   might be written in a compiler-specific way for a non-standard or
   sophisticated (in-line assembler?) compiler.
*/


/*##**********************************************************************\
 * 
 *		TWO_BYTE_VALUE
 * 
 * Creates a little-endian two-byte value out of two bytes.
 * 
 * Parameters : 
 * 
 *	b1, b0 - in
 *          the bytes; b1 should end up lowest in memory
 * 
 * Return value : 
 * 
 *      a two-byte integer
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifdef SS_LSB1ST
#define TWO_BYTE_VALUE(b1, b0) ((TWO_BYTE_T)((b0) << 8 | (b1)))
#else
#define TWO_BYTE_VALUE(b1, b0) ((TWO_BYTE_T)((b1) << 8 | (b0)))
#endif


/*##**********************************************************************\
 * 
 *		ONE_WORD_VALUE
 * 
 * Creates a little-endian two-byte value out of one word.
 * 
 * Parameters : 
 * 
 *	w - in
 *		the word
 *
 * Return value : 
 * 
 *      a two-byte integer
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifdef SS_LSB1ST
#define ONE_WORD_VALUE(w) ((TWO_BYTE_T)((w) << 8 | (unsigned)(w) >> 8))
#else
#define ONE_WORD_VALUE(w) ((TWO_BYTE_T)(w))
#endif


/*##**********************************************************************\
 * 
 *		FOUR_BYTE_VALUE
 * 
 * Creates a little-endian four-byte value out of four bytes.
 * 
 * Parameters : 
 * 
 *	b3, b2, b1, b0 - in
 *          the bytes; b3 should end up lowest in memory
 * 
 * Return value : 
 * 
 *      a four-byte integer
 * 
 * Limitations  :
 * 
 * Globals used : 
 */
#ifdef SS_LSB1ST
#define FOUR_BYTE_VALUE(b3, b2, b1, b0) \
        ((((FOUR_BYTE_T)(b0) << 8 | (b1)) << 8 | (b2)) << 8 | (b3))
#else
#define FOUR_BYTE_VALUE(b3, b2, b1, b0) \
        ((((FOUR_BYTE_T)(b3) << 8 | (b2)) << 8 | (b1)) << 8 | (b0))
#endif


/*##**********************************************************************\
 * 
 *		ONE_WORD_LOAD
 * 
 * Loads one little-endian word from memory, using as few
 * operations as possible.
 * 
 * Parameters : 
 * 
 *      ptr - in, use
 *		pointer to memory
 *
 * Return value : 
 * 
 *      word value
 * 
 * Limitations  :
 * 
 * Globals used : 
 */
#if defined(UNALIGNED_LOAD) && WORD_SIZE >= 2 && !defined(SS_LSB1ST)
#  define ONE_WORD_LOAD(ptr) (*(TWO_BYTE_T*)(ptr))
#else
#  define ONE_WORD_LOAD(ptr) ((TWO_BYTE_T)((ss_byte_t*)(ptr))[0] << 8 | ((ss_byte_t*)(ptr))[1])
#endif


/*##**********************************************************************\
 * 
 *		TWO_BYTE_STORE
 * 
 * Stores two bytes into memory in little-endian order, using as few
 * operations as possible.
 * 
 * Parameters : 
 * 
 *      ptr - out, use
 *		pointer to memory
 *
 *	b1, b0 - in
 *          the bytes; b1 should end up lowest in memory
 * 
 * Return value : 
 * 
 * Limitations  :
 * 
 * Globals used : 
 */
#if defined(UNALIGNED_LOAD) && WORD_SIZE >= 2
#  define TWO_BYTE_STORE(ptr, b1, b0) {\
            *(TWO_BYTE_T*)(ptr) = TWO_BYTE_VALUE(b1, b0); \
        }
#else
#  define TWO_BYTE_STORE(ptr, b1, b0) {\
            ((ss_byte_t*)(ptr))[0] = (b1); \
            ((ss_byte_t*)(ptr))[1] = (b0); \
        }
#endif


/*##**********************************************************************\
 * 
 *		FOUR_BYTE_STORE
 * 
 * Stores four bytes into memory in little-endian order, using as few
 * operations as possible.  Efficient when most of the bytes are constant.
 * 
 * Parameters : 
 * 
 *      ptr - out, use
 *		pointer to memory
 *
 *	b3, b2, b1, b0 - in
 *          the bytes; b3 should end up lowest in memory
 * 
 * Return value : 
 * 
 * Limitations  :
 * 
 * Globals used : 
 */
#if defined(UNALIGNED_LOAD) && WORD_SIZE >= 4
#  define FOUR_BYTE_STORE(ptr, b3, b2, b1, b0) \
        *(FOUR_BYTE_T*)(ptr) = FOUR_BYTE_VALUE(b3, b2, b1, b0);
#elif defined(UNALIGNED_LOAD) && WORD_SIZE == 2
#  define FOUR_BYTE_STORE(ptr, b3, b2, b1, b0) {\
            ((TWO_BYTE_T*)(ptr))[0] = TWO_BYTE_VALUE(b3, b2); \
            ((TWO_BYTE_T*)(ptr))[1] = TWO_BYTE_VALUE(b1, b0); \
        }
#else
#  define FOUR_BYTE_STORE(ptr, b3, b2, b1, b0) {\
            ((ss_byte_t*)(ptr))[0] = (b3); \
            ((ss_byte_t*)(ptr))[1] = (b2); \
            ((ss_byte_t*)(ptr))[2] = (b1); \
            ((ss_byte_t*)(ptr))[3] = (b0); \
        }
#endif

#ifndef SS_STACK_GROW_DIRECTION
#  define SS_STACK_GROW_DIRECTION (-1)
#endif

#ifndef SS_ALIGN_T
/* assume long int is proper alignment for all objs */
#define SS_ALIGN_T unsigned long
#endif
#define SS_ALIGNMENT (sizeof(SS_ALIGN_T))

#if defined(UNALIGNED_LOAD) && defined(SS_LSB1ST) /* a'la Intel */

#define SS_UINT2_LOADFROMDISK(ptr) \
        (*(ss_uint2_t*)(ptr))
#define SS_UINT4_LOADFROMDISK(ptr) \
        (*(ss_uint4_t*)(ptr))
#define SS_UINT2_STORETODISK(ptr, val) \
        {*(ss_uint2_t*)(ptr) = (ss_uint2_t)(val);}
#define SS_UINT4_STORETODISK(ptr, val) \
        {*(ss_uint4_t*)(ptr) = (ss_uint4_t)(val);}

#else /* a'la Intel */

#define SS_UINT2_LOADFROMDISK(ptr) \
        ( (((ss_byte_t*)(ptr))[0]) | \
         ((((ss_byte_t*)(ptr))[1]) << (SS_CHAR_BIT*1)))
#define SS_UINT4_LOADFROMDISK(ptr) \
        ( (ss_uint4_t)(((ss_byte_t*)(ptr))[0]) | \
         ((ss_uint4_t)(((ss_byte_t*)(ptr))[1]) << (SS_CHAR_BIT*1)) | \
         ((ss_uint4_t)(((ss_byte_t*)(ptr))[2]) << (SS_CHAR_BIT*2)) | \
         ((ss_uint4_t)(((ss_byte_t*)(ptr))[3]) << (SS_CHAR_BIT*3)))
#define SS_UINT2_STORETODISK(p, v) \
{\
        uint val = (v);\
        void* ptr = (p);\
        (((ss_byte_t*)(ptr))[0]) = (ss_byte_t)(val);\
        (((ss_byte_t*)(ptr))[1]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*1));\
}
#define SS_UINT4_STORETODISK(p, v) \
{\
        ss_uint4_t val = (v);\
        void* ptr = (p);\
        (((ss_byte_t*)(ptr))[0]) = (ss_byte_t)(val);\
        (((ss_byte_t*)(ptr))[1]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*1));\
        (((ss_byte_t*)(ptr))[2]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*2));\
        (((ss_byte_t*)(ptr))[3]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*3));\
}

#endif /* a'la Intel */

#if !defined(SS_LSB1ST) && defined(UNALIGNED_LOAD) /* a'la MC68k */

#define SS_CHAR2_STORE(ptr, val) \
        {*(ss_char2_t*)(ptr) = (ss_char2_t)(val);}

#define SS_CHAR2_LOAD(ptr) \
        (*(ss_char2_t*)(ptr))


#define SS_UINT2_LOAD_MSB1ST(ptr) \
        (*(ss_uint2_t*)(ptr))

#define SS_UINT4_LOAD_MSB1ST(ptr) \
        (*(ss_uint4_t*)(ptr))

#define SS_UINT2_STORE_MSB1ST(ptr, val) \
        {*(ss_uint2_t*)(ptr) = (ss_uint2_t)(val);}

#define SS_UINT4_STORE_MSB1ST(ptr, val) \
        {*(ss_uint4_t*)(ptr) = (ss_uint4_t)(val);}

#else /* a'la MC68k */

#define SS_CHAR2_STORE(p, v) \
{\
        uint val = (v);\
        void* ptr = (p);\
        (((ss_byte_t*)(ptr))[0]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*1));\
        (((ss_byte_t*)(ptr))[1]) = (ss_byte_t)(val);\
}

#define SS_CHAR2_LOAD(ptr) \
        (((((ss_byte_t*)(ptr))[0]) << (SS_CHAR_BIT*1))  | \
         (((ss_byte_t*)(ptr))[1]))

#define SS_UINT2_LOAD_MSB1ST(ptr) \
        (((((ss_byte_t*)(ptr))[0]) << (SS_CHAR_BIT*1)) | \
         (((ss_byte_t*)(ptr))[1]))

#define SS_UINT4_LOAD_MSB1ST(ptr) \
        (((ss_uint4_t)(((ss_byte_t*)(ptr))[0]) << (SS_CHAR_BIT*3)) | \
         ((ss_uint4_t)(((ss_byte_t*)(ptr))[1]) << (SS_CHAR_BIT*2)) | \
         ((ss_uint4_t)(((ss_byte_t*)(ptr))[2]) << (SS_CHAR_BIT*1)) | \
         (ss_uint4_t)(((ss_byte_t*)(ptr))[3]))

#define SS_UINT2_STORE_MSB1ST(p, v) \
{\
        uint val = (v);\
        void* ptr = (p);\
        (((ss_byte_t*)(ptr))[0]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*1));\
        (((ss_byte_t*)(ptr))[1]) = (ss_byte_t)(val);\
}

#define SS_UINT4_STORE_MSB1ST(p, v) \
{\
        ss_uint4_t val = (v);\
        void* ptr = (p);\
        (((ss_byte_t*)(ptr))[0]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*3));\
        (((ss_byte_t*)(ptr))[1]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*2));\
        (((ss_byte_t*)(ptr))[2]) = (ss_byte_t)((val) >> (SS_CHAR_BIT*1));\
        (((ss_byte_t*)(ptr))[3]) = (ss_byte_t)(val);\
}

#endif /* a'la MC68k */

#ifdef UNALIGNED_LOAD
#  define SS_UINT4_LOAD(p) (*(ss_uint4_t*)(ss_byte_t*)(p))
#  define SS_UINT2_LOAD(p) (*(ss_uint2_t*)(ss_byte_t*)(p))
#  define SS_PTR_LOAD(p)   (*(void**)(ss_byte_t*)(p))
#  define SS_UINT4_STORE(p, u4) \
        do { *(ss_uint4_t*)(ss_byte_t*)(p) = (u4); } while (0)
#  define SS_UINT2_STORE(p, u2) \
        do { *(ss_uint2_t*)(ss_byte_t*)(p) = (u2); } while (0)
#  define SS_PTR_STORE(p, ptr) \
        do { *(void**)(ss_byte_t*)(p) = (ptr); } while (0)

#else /* UNALIGNED_LOAD */
#  ifdef SS_LSB1ST
#    define SS_UINT4_LOAD(p) SS_UINT4_LOADFROMDISK(p)
#    define SS_UINT2_LOAD(p) SS_UINT2_LOADFROMDISK(p)
#    define SS_UINT4_STORE(p, u4) SS_UINT4_STORETODISK(p, u4)
#    define SS_UINT2_STORE(p, u2) SS_UINT2_STORETODISK(p, u2)

#    if SS_POINTER_SIZE == 8
#      define SS_PTR_LOAD(p) ((void*)\
                              ((ss_ptr_as_scalar_t)SS_UINT4_LOAD(p) |\
                               ((ss_ptr_as_scalar_t)SS_UINT4_LOAD(\
                                       (((ss_byte_t*)p) + sizeof(ss_uint4_t)))\
                                << (sizeof(ss_uint4_t) * SS_CHAR_BIT))))
                             
#      define SS_PTR_STORE(p, ptr) \
        do {\
            SS_UINT4_STORE(p, (ss_uint4_t)(ss_ptr_as_scalar_t)(ptr));\
            SS_UINT4_STORE((((ss_byte_t*)(p)) + sizeof(ss_uint4_t)),\
                           (ss_uint4_t)((ss_ptr_as_scalar_t)(ptr)\
                                        >> (sizeof(ss_uint4_t)\
                                            * SS_CHAR_BIT)));\
        } while (0)
#    else /* SS_POINTER_SIZE == 8 */
#      define SS_PTR_LOAD(p) ((void*)SS_UINT4_LOAD(p))
#      define SS_PTR_STORE(p, ptr) SS_UINT4_STORE(p, (ss_uint4_t)(ptr))
#    endif /* SS_POINTER_SIZE == 8 */
#  else /* SS_LSB1ST */
#    define SS_UINT4_LOAD(p) SS_UINT4_LOAD_MSB1ST(p)
#    define SS_UINT2_LOAD(p) SS_UINT2_LOAD_MSB1ST(p)
#    define SS_UINT4_STORE(p, u4) SS_UINT4_STORE_MSB1ST(p, u4)
#    define SS_UINT2_STORE(p, u2) SS_UINT2_STORE_MSB1ST(p, u2)

#    if SS_POINTER_SIZE == 8
#      define SS_PTR_LOAD(p) ((void*)\
                              (((ss_ptr_as_scalar_t)SS_UINT4_LOAD(p)\
                                << (sizeof(ss_uint4_t) * SS_CHAR_BIT)) |\
                               (ss_ptr_as_scalar_t)SS_UINT4_LOAD(\
                                   (((ss_byte_t*)p) + sizeof(ss_uint4_t)))))

#      define SS_PTR_STORE(p, ptr) \
        do {\
            SS_UINT4_STORE(p,\
                           (ss_uint4_t)((ss_ptr_as_scalar_t)(ptr)\
                                        >> (sizeof(ss_uint4_t)*SS_CHAR_BIT)));\
            SS_UINT4_STORE((((ss_byte_t*)(p)) + sizeof(ss_uint4_t)),\
                           (ss_uint4_t)(ss_ptr_as_scalar_t)(ptr));\
        } while (0)
#    else /* SS_POINTER_SIZE == 8 */
#      define SS_PTR_LOAD(p) ((void*)SS_UINT4_LOAD(p))
#      define SS_PTR_STORE(p, ptr) SS_UINT4_STORE(p, (ss_uint4_t)(ptr))
#    endif /* SS_POINTER_SIZE == 8 */

#  endif /* SS_LSB1ST */

#endif /* UNALIGNED_LOAD */
/* significance grow direction is sometimes useful constant
 * when byte order independent code is wanted
 */
#ifdef SS_LSB1ST
#define SS_SIGNIFICANCE_GROW_DIRECTION 1
#else /* SS_LSB1ST */
#define SS_SIGNIFICANCE_GROW_DIRECTION (-1)
#endif /* SS_LSB1ST */

#define SS_CHAR2_LOAD_LSB1ST(ptr) SS_UINT2_LOADFROMDISK(ptr)
#define SS_CHAR2_LOAD_MSB1ST(ptr) SS_CHAR2_LOAD(ptr)
#define SS_CHAR2_STORE_LSB1ST(ptr, val) SS_UINT2_STORETODISK(ptr, val)
#define SS_CHAR2_STORE_MSB1ST(ptr, val) SS_CHAR2_STORE(ptr, val)

#define SS_CHAR_BIT  8
#define SS_LONG_BIT  LONG_BIT
#define SS_INT_BIT   INT_BIT
#define SS_UINT4_MAX ((ss_uint4_t)(~(ss_uint4_t)0))
#define SS_UINT4_MIN ((ss_uint4_t)((ss_uint4_t)0))
#define SS_INT4_MAX  ((ss_int4_t)(~SS_INT4_MIN))
#define SS_INT4_MIN  ((ss_int4_t)(1UL << (sizeof(ss_int4_t)*SS_CHAR_BIT - 1)))
#define SS_UINT2_MAX ((ss_uint2_t)(~(ss_uint2_t)0))
#define SS_UINT2_MIN ((ss_uint2_t)0)
#define SS_INT2_MAX  ((ss_int2_t)(~SS_INT2_MIN))
#define SS_INT2_MIN  ((ss_int2_t)(1U << (sizeof(ss_int2_t)*SS_CHAR_BIT - 1)))
#define SS_UINT1_MAX ((ss_uint1_t)~0)
#define SS_UINT1_MIN ((ss_uint1_t)0)
#define SS_INT1_MAX  ((ss_int1_t)~SS_INT1_MIN))
#define SS_INT1_MIN  ((ss_int1_t)(1U << (sizeof(ss_int1_t)*SS_CHAR_BIT - 1)))

#endif /* SSLIMITS_H */
