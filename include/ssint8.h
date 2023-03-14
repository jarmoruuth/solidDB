/*************************************************************************\
**  source       * ssint8.h
**  directory    * ss
**  description  * 8-byte integer support routines
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


#ifndef SSINT8_H
#define SSINT8_H

#include "ssenv.h"
#include "ssc.h"
#include "sslimits.h"
#include "ssint4.h"
#include "ssstdlib.h"

/*****************************************************
 I N T E R N A L   D E C L A R A T I O N S   B E G I N
 -----------------------------------------------------
 do not use directly outside ssint8.[ch]!!!!!
******************************************************/
 
/* Hide byte order differences with these constants */
#ifdef SS_LSB1ST

#define SS_INT8_MS_U4_IDX 1 /* index of most significant uint4 */
#define SS_INT8_LS_U4_IDX 0
#define SS_INT8_MS_U2_IDX 3
#define SS_INT8_LS_U2_IDX 0
#define SS_INT8_MS_U1_IDX 7
#define SS_INT8_LS_U1_IDX 0

#else /* SS_LSB1ST */

#define SS_INT8_MS_U4_IDX 0
#define SS_INT8_LS_U4_IDX 1
#define SS_INT8_MS_U2_IDX 0
#define SS_INT8_LS_U2_IDX 3
#define SS_INT8_MS_U1_IDX 0
#define SS_INT8_LS_U1_IDX 7

#endif /* SS_LSB1ST */


/* Never use members of this union directly,
   only through the routines defined below!
*/
typedef union {
#ifdef SS_NATIVE_UINT8_T
        SS_NATIVE_UINT8_T u8[1];
#endif /* SS_NATIVE_UINT8_T */
        ss_uint4_t u4[2];
        ss_uint2_t u2[4];
        ss_uint1_t u1[8];
} ss_int8_t;

/* local macros that should have pretty obvious semantics */
#define I8_ISNEG(i8) \
        I4_ISNEG((i8).u4[SS_INT8_MS_U4_IDX])

#define I2_ISNEG(i2) \
        ((bool)((((ss_uint2_t)(i2)) >>\
                 (sizeof(ss_uint2_t) * SS_CHAR_BIT - 1))))

#define I4_ISNEG(i4) \
        ((bool)((((ss_uint4_t)(i4)) >>\
                 (sizeof(ss_uint4_t) * SS_CHAR_BIT - 1))))

#define I8_LS_UINT4(i8) \
        ((i8).u4[SS_INT8_LS_U4_IDX])

#define I8_MS_UINT4(i8) \
        ((i8).u4[SS_INT8_MS_U4_IDX])

#define I8_IS_0(i8) \
        (((i8).u4[0] | (i8).u4[1]) == 0)

#define I8_SET_2_U4(p_i8, u4hi, u4lo) \
        do {\
            (p_i8)->u4[SS_INT8_LS_U4_IDX] = (u4lo);\
            (p_i8)->u4[SS_INT8_MS_U4_IDX] = (u4hi);\
        } while (FALSE)

#define I8_SET_U4(p_i8, u4) \
        I8_SET_2_U4(p_i8, (ss_uint4_t)0, u4)

#define I8_SET_I4(p_i8, i4) \
        I8_SET_2_U4((p_i8), (ss_uint4_t)(-((ss_int4_t)I4_ISNEG(i4))), (ss_uint4_t)(i4))

#define I8_SET_0(p_i8) \
        I8_SET_U4(p_i8, (ss_uint4_t)0)

#define I8_SETBIT(p_i8, bitno) \
        (p_i8)->u4[SS_INT8_LS_U4_IDX +\
                   (((bitno) / (sizeof(ss_uint4_t) * SS_CHAR_BIT)) *\
                    SS_SIGNIFICANCE_GROW_DIRECTION)] |=\
            (ss_uint4_t)1 << ((bitno) % (sizeof(ss_uint4_t) * SS_CHAR_BIT))

#define I8_UNSIGNEDCMP(i8_1,i8_2) \
        ((i8_1).u4[SS_INT8_MS_U4_IDX] < (i8_2).u4[SS_INT8_MS_U4_IDX] ? -1 :\
         ((i8_1).u4[SS_INT8_MS_U4_IDX] > (i8_2).u4[SS_INT8_MS_U4_IDX] ? 1 :\
          ((i8_1).u4[SS_INT8_LS_U4_IDX] < (i8_2).u4[SS_INT8_LS_U4_IDX] ? -1 :\
           ((i8_1).u4[SS_INT8_LS_U4_IDX] > (i8_2).u4[SS_INT8_LS_U4_IDX] ? 1 : 0))))

#define I8_UNSIGNEDSHIFTRIGHT1(p_result_in_out) \
do {\
        (p_result_in_out)->u4[SS_INT8_LS_U4_IDX] = \
            ((p_result_in_out)->u4[SS_INT8_LS_U4_IDX] >> 1) | \
            /* the ORed value below is "carry" bit */ \
            ((p_result_in_out)->u4[SS_INT8_MS_U4_IDX] <<\
             (sizeof(ss_uint4_t) * SS_CHAR_BIT - 1));\
        (p_result_in_out)->u4[SS_INT8_MS_U4_IDX] >>= 1;\
} while (FALSE)

#define I8_UNSIGNEDSHIFTLEFT1(p_result_in_out) \
do {\
        (p_result_in_out)->u4[SS_INT8_MS_U4_IDX] = \
            ((p_result_in_out)->u4[SS_INT8_MS_U4_IDX] << 1) | \
            /* the ORed value below is "carry" bit */ \
            (((p_result_in_out)->u4[SS_INT8_LS_U4_IDX] >>\
             (sizeof(ss_uint4_t) * SS_CHAR_BIT - 1)) & 1);\
        (p_result_in_out)->u4[SS_INT8_LS_U4_IDX] <<= 1;\
} while (FALSE)

#define U4_BITS (sizeof(ss_uint4_t) * SS_CHAR_BIT)

#define I8_SHR_N(p_i8, n) \
do {\
        int n_div_32 = (n) / U4_BITS;\
        int n_minus_32 = (int)(n) - U4_BITS;\
        int one_minus_n_div_32 = 1 - n_div_32;\
        int n_noteq_0 = ((n) != 0);\
        (p_i8)->u4[SS_INT8_LS_U4_IDX] =\
            (((p_i8)->u4[SS_INT8_LS_U4_IDX] >> (n)) * one_minus_n_div_32) |\
            (n_noteq_0 * (((p_i8)->u4[SS_INT8_MS_U4_IDX] <<\
              (n_minus_32 * -one_minus_n_div_32)) >>\
             (n_minus_32 * n_div_32)));\
        (p_i8)->u4[SS_INT8_MS_U4_IDX] =\
            ((p_i8)->u4[SS_INT8_MS_U4_IDX] >> (n)) * one_minus_n_div_32;\
} while (FALSE)

#define I8_SHL_N(p_i8, n)\
do {\
        int n_div_32 = (n) / U4_BITS;\
        int n_minus_32 = (int)(n) - U4_BITS;\
        int one_minus_n_div_32 = 1 - n_div_32;\
        int n_noteq_0 = ((n) != 0);\
        (p_i8)->u4[SS_INT8_MS_U4_IDX] =\
            (((p_i8)->u4[SS_INT8_MS_U4_IDX] << (n)) * one_minus_n_div_32) |\
            (n_noteq_0 * (((p_i8)->u4[SS_INT8_LS_U4_IDX] >>\
              (n_minus_32 * -one_minus_n_div_32)) <<\
             (n_minus_32 * n_div_32)));\
        (p_i8)->u4[SS_INT8_LS_U4_IDX] =\
            ((p_i8)->u4[SS_INT8_LS_U4_IDX] << (n)) * one_minus_n_div_32;\
} while (FALSE)
    
#define I8_AND_I8(p_i8a, i8b) \
do {\
    (p_i8a)->u4[SS_INT8_MS_U4_IDX] &= (i8b).u4[SS_INT8_MS_U4_IDX];\
    (p_i8a)->u4[SS_INT8_LS_U4_IDX] &= (i8b).u4[SS_INT8_LS_U4_IDX];\
} while (FALSE)

#define I8_OR_I8(p_i8a, i8b) \
do {\
    (p_i8a)->u4[SS_INT8_MS_U4_IDX] |= (i8b).u4[SS_INT8_MS_U4_IDX];\
    (p_i8a)->u4[SS_INT8_LS_U4_IDX] |= (i8b).u4[SS_INT8_LS_U4_IDX];\
} while (FALSE)

#define I8_XOR_I8(p_i8a, i8b) \
do {\
    (p_i8a)->u4[SS_INT8_MS_U4_IDX] ^= (i8b).u4[SS_INT8_MS_U4_IDX];\
    (p_i8a)->u4[SS_INT8_LS_U4_IDX] ^= (i8b).u4[SS_INT8_LS_U4_IDX];\
} while (FALSE)

#define I8_INV_SIGNBIT(p_i8) \
do {\
    (p_i8)->u4[SS_INT8_MS_U4_IDX] ^= \
        (ss_uint4_t)1 << (sizeof(ss_uint4_t) * SS_CHAR_BIT - 1);\
} while (FALSE)

/*****************************************************
 I N T E R N A L   D E C L A R A T I O N S   E N D
 -----------------------------------------------------
 E X T E R N A L S   B E G I N
******************************************************/

void SsInt8Set2Uint4s(ss_int8_t* p_result, ss_uint4_t u4high, ss_uint4_t u4low);
ss_int8_t SsInt8InitFrom2Uint4s(ss_uint4_t u4high, ss_uint4_t u4low);
void SsInt8SetUint4(ss_int8_t* p_result, ss_uint4_t u4);
void SsInt8Set0(ss_int8_t* p_result);

void SsInt8SetInt4(ss_int8_t* p_result, ss_int4_t i4);
bool SsInt8ConvertToInt4(ss_int4_t* p_result, ss_int8_t i8);
bool SsInt8ConvertToLong(long* p_result, ss_int8_t i8);
bool SsInt8ConvertToSizeT(size_t* p_result, ss_int8_t i8);
bool SsInt8ConvertToDouble(double* p_result, ss_int8_t i8);
bool SsInt8SetDouble(ss_int8_t* p_result, double d);
bool SsInt8AddInt8(ss_int8_t* p_result, ss_int8_t i8_1, ss_int8_t i8_2);
bool SsInt8SubtractInt8(ss_int8_t* p_result, ss_int8_t i8_1, ss_int8_t i8_2);
bool SsInt8MultiplyByInt8(ss_int8_t* p_result, ss_int8_t i8_1, ss_int8_t i8_2);
bool SsInt8DivideByInt8(ss_int8_t* p_result, ss_int8_t dividend, ss_int8_t divisor);
bool SsInt8RemainderByInt8(ss_int8_t* p_result, ss_int8_t dividend, ss_int8_t divisor);
bool SsInt8DivWithRemByInt8(ss_int8_t* p_result, ss_int8_t* p_remainder,
                            ss_int8_t dividend, ss_int8_t divisor);
void SsInt8AndInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b);
void SsInt8AndInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b);
void SsInt8OrInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b);
void SsInt8OrInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b);
void SsInt8XorInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b);
void SsInt8XorInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b);
uint SsInt8GetHighestBitIndex(ss_int8_t i8);
void SsInt8InvertSignBitInPlace(ss_int8_t* p_i8);

size_t SsInt8ToAscii(ss_int8_t i8, char* buf, uint radix, size_t width,
                     char leftfillch, bool is_signed);

bool SsInt8Is0(ss_int8_t i8);
int SsInt8Cmp(ss_int8_t i8_1, ss_int8_t i8_2);
int SsInt8UnsignedCmp(ss_int8_t i8_1, ss_int8_t i8_2);

bool SsInt8IsNegative(ss_int8_t i8);
bool SsInt8Equal(ss_int8_t i8_1, ss_int8_t i8_2);
bool SsInt8AddUint2(ss_int8_t* p_result, ss_int8_t i8, ss_uint2_t u2);
bool SsInt8AddUint4(ss_int8_t* p_result, ss_int8_t i8, ss_uint4_t u4);
bool SsInt8Negate(ss_int8_t* p_result, ss_int8_t i8);
bool SsInt8MultiplyByInt2(ss_int8_t* p_result, ss_int8_t i8, ss_int2_t i2);
void SsInt8ShrN(ss_int8_t* p_result, ss_int8_t i8, uint n);
void SsInt8ShrNInPlace(ss_int8_t* p_i8, uint n); 
void SsInt8ShlN(ss_int8_t* p_result, ss_int8_t i8, uint n);
void SsInt8ShlNInPlace(ss_int8_t* p_i8, uint n);
ss_uint4_t SsInt8GetLeastSignificantUint4(ss_int8_t i8);
ss_uint4_t SsInt8GetMostSignificantUint4(ss_int8_t i8);
ss_byte_t SsInt8GetNthOrderByte(ss_int8_t i8, uint n);
SS_INLINE size_t SsInt8MinBytes(ss_int8_t i8);
SS_INLINE void SsInt8StorePackedMSB1st(
        ss_int8_t i8, ss_byte_t* buf, size_t size);
SS_INLINE void SsInt8LoadPackedMSB1st(
        ss_int8_t* p_result, ss_byte_t* buf, size_t size);

/*##**********************************************************************\
 *		
 *      SsInt8ShrBy8Bits
 * 
 * Shifts int8 value to right by 8 bits
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SsInt8ShrBy8Bits(ss_int8_t* p_result, ss_int8_t i8);

/*##**********************************************************************\
 *		
 *      SsInt8GetLeastSignificantByte
 * 
 * Gets the value of the least significant byte
 * 
 * Parameters :
 *
 *      i8 - in
 *          the int8 value
 *
 * Return value :
 *      unsigned byte value of the least significant
 *      byte of the integer value.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_byte_t SsInt8GetLeastSignificantByte(ss_int8_t i8);

/*##**********************************************************************\
 * 
 *		SsStrScanInt8
 * 
 * Converts a string to 8-byte integer. 
 * Leading whitespaces are skipped.
 * 
 *  [whitespace][{+|-}][digits]
 * 
 * Parameters : 
 * 
 *      s - in, use
 *          source string
 *
 *      p_result - out
 *          pointer to resulting 8-byte integer
 *
 *      p_mismatch - out
 *          pointer to the first position in src that
 *          does not belong to the long integer returned
 *
 * Return value : 
 * 
 *      TRUE, if succesful
 *      FALSE, if failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsStrScanInt8(
        char* s,
        ss_int8_t* p_result,
        char** p_mismatch);

#ifdef SS_NATIVE_UINT8_T
#define SsInt8GetNativeUint8(i8) ((i8).u8[0])
#define SsInt8SetNativeUint8(p_i8, _u8) \
        do { (p_i8)->u8[0] = (_u8); } while (0)

#ifdef SS_NATIVE_INT8_T
#  define SsInt8GetNativeInt8(i8) \
        ((SS_NATIVE_INT8_T)SsInt8GetNativeUint8(i8))
#  define SsInt8SetNativeInt8(p_i8, i8) \
        do { (p_i8)->u8[0] = (SS_NATIVE_UINT8_T)(i8); } while (0)
#endif /* SS_NATIVE_INT8_T */

#endif /* SS_NATIVE_UINT8_T */

#ifndef SS_DEBUG

/* Macros to remove call overhead of the most trivial functions */

#define SsInt8IsNegative(i8) \
        I8_ISNEG(i8)

#define SsInt8Set2Uint4s(p_result,u4high,u4low) \
        do { I8_SET_2_U4((ss_int8_t*)(p_result), (ss_uint4_t)(u4high), (ss_uint4_t)(u4low)); } while (FALSE)

#define SsInt8SetUint4(p_result, u4) \
        do { I8_SET_U4((ss_int8_t*)(p_result), (ss_uint4_t)(u4)); } while (FALSE)

#define SsInt8Set0(p_result) \
        do { I8_SET_0((ss_int8_t*)(p_result)); } while (FALSE)

#ifdef SS_NATIVE_UINT8_T
#define SsInt8Equal(i8_1, i8_2) \
        ((i8_1).u8[0] == (i8_2).u8[0])
#else
#define SsInt8Equal(i8_1, i8_2) \
        ((i8_1).u4[0] == (i8_2).u4[0] && (i8_1).u4[1] == (i8_2).u4[1])
#endif

#define SsInt8SetInt4(p_result, i4) \
        I8_SET_I4((ss_int8_t*)(p_result), i4)

#define SsInt8GetLeastSignificantUint4(i8) \
        (I8_LS_UINT4(i8))

#define SsInt8GetMostSignificantUint4(i8) \
        (I8_MS_UINT4(i8))

#define SsInt8GetLeastSignificantByte(i8) \
        ((i8).u1[SS_INT8_LS_U1_IDX])

#define SsInt8GetNthOrderByte(i8,n) \
        ((i8).u1[SS_INT8_LS_U1_IDX + (int)(n) * SS_SIGNIFICANCE_GROW_DIRECTION])

#define SsInt8Is0(i8) \
        (I8_IS_0(i8))

#define SsInt8ShrNInPlace(p_i8, n) \
        I8_SHR_N((ss_int8_t*)(p_i8), n)

#define SsInt8ShrN(p_result, i8, n) \
        do { *(ss_int8_t*)(p_result) = (i8); SsInt8ShrNInPlace(p_result, n); } while (FALSE)

#define SsInt8ShlNInPlace(p_i8, n) \
        I8_SHL_N((ss_int8_t*)(p_i8), n)

#define SsInt8ShlN(p_result, i8, n) \
        do { *(ss_int8_t*)(p_result) = (i8); SsInt8ShlNInPlace(p_result, n); } while (FALSE)

#define SsInt8AndInt8InPlace(p_i8a, i8b) \
        I8_AND_I8((ss_int8_t*)(p_i8a), i8b)

#define SsInt8AndInt8(p_result, i8a, i8b) \
        do {\
            *(ss_int8_t*)(p_result) = (i8a);\
            SsInt8AndInt8InPlace(p_result, i8b);\
        } while (FALSE)

#define SsInt8OrInt8InPlace(p_i8a, i8b) \
        I8_OR_I8((ss_int8_t*)(p_i8a), i8b)

#define SsInt8OrInt8(p_result, i8a, i8b) \
        do {\
            *(ss_int8_t*)(p_result) = (i8a);\
            SsInt8OrInt8InPlace(p_result, i8b);\
        } while (FALSE)

#define SsInt8XorInt8InPlace(p_i8a, i8b) \
        I8_XOR_I8((ss_int8_t*)(p_i8a), i8b)

#define SsInt8XorInt8(p_result, i8a, i8b) \
        do {\
            *(ss_int8_t*)(p_result) = (i8a);\
            SsInt8XorInt8InPlace(p_result, i8b);\
        } while (FALSE)

#define SsInt8InvertSignBitInPlace(p_i8) \
        do { I8_INV_SIGNBIT(p_i8); } while (FALSE)
    
#define SsInt8UnsignedCmp(i8a, i8b) \
        I8_UNSIGNEDCMP(i8a, i8b)

#endif /* !defined(SS_DEBUG) */

#if defined(SS_USE_INLINE) || defined(SSINT8_C)

SS_INLINE size_t SsInt8MinBytes(ss_int8_t i8)
{
        if (i8.u4[SS_INT8_MS_U4_IDX] == 0) {
            if (!I4_ISNEG(i8.u4[SS_INT8_LS_U4_IDX])) {
                goto fits_to_4_or_less_bytes;
            }
        } else if (i8.u4[SS_INT8_MS_U4_IDX] == ~(ss_uint4_t)0) {
            if (I4_ISNEG(i8.u4[SS_INT8_LS_U4_IDX])) {
                goto fits_to_4_or_less_bytes;
            }
        }
        return (4 + SsInt4MinBytes((ss_int4_t)i8.u4[SS_INT8_MS_U4_IDX]));
 fits_to_4_or_less_bytes:;
        return (SsInt4MinBytes((ss_int4_t)i8.u4[SS_INT8_LS_U4_IDX]));
}


SS_INLINE void SsInt8StorePackedMSB1st(
        ss_int8_t i8,
        ss_byte_t* buf,
        size_t size)
{
        ss_uint4_t u4;
        
        switch (size) {
            case 8:
                u4 = i8.u4[SS_INT8_MS_U4_IDX];
                goto bytes_8;
            case 7:
                u4 = i8.u4[SS_INT8_MS_U4_IDX];
                goto bytes_7;
            case 6:
                u4 = i8.u4[SS_INT8_MS_U4_IDX];
                goto bytes_6;
            case 5:
                u4 = i8.u4[SS_INT8_MS_U4_IDX];
                goto bytes_5;
            case 4:
                u4 = i8.u4[SS_INT8_LS_U4_IDX];
                goto bytes_4;
            case 3:
                u4 = i8.u4[SS_INT8_LS_U4_IDX];
                goto bytes_3;
            case 2:
                u4 = i8.u4[SS_INT8_LS_U4_IDX];
                goto bytes_2;
            default: /* 1 */
                u4 = i8.u4[SS_INT8_LS_U4_IDX];
                goto bytes_1;
        }
 bytes_8:;
        *buf++ = (ss_byte_t)(u4 >> (3 * SS_CHAR_BIT));
 bytes_7:;
        *buf++ = (ss_byte_t)(u4 >> (2 * SS_CHAR_BIT));
 bytes_6:;
        *buf++ = (ss_byte_t)(u4 >> (1 * SS_CHAR_BIT));
 bytes_5:;
        *buf++ = (ss_byte_t)u4;
        u4 = i8.u4[SS_INT8_LS_U4_IDX];
 bytes_4:
        *buf++ = (ss_byte_t)(u4 >> (3 * SS_CHAR_BIT));
 bytes_3:
        *buf++ = (ss_byte_t)(u4 >> (2 * SS_CHAR_BIT));
 bytes_2:
        *buf++ = (ss_byte_t)(u4 >> (1 * SS_CHAR_BIT));
 bytes_1:
        *buf = (ss_byte_t)u4;
}

SS_INLINE void SsInt8LoadPackedMSB1st(
        ss_int8_t* p_result,
        ss_byte_t* buf,
        size_t size)
{
        ss_uint4_t u4_hi;
        ss_uint4_t u4;
        
        /* note: the below does sign extension to the expanded 24 bits
         * due to cast to signed 1-byte type! (ss_int1_t)
         */
        u4 = (ss_uint4_t)(ss_int4_t)(ss_int1_t)*buf++;
        switch (size) {
            case 8:
                goto bytes_8;
            case 7:
                goto bytes_7;
            case 6:
                goto bytes_6;
            case 5:
                goto bytes_5;
            case 4:
                /* do sign extension to most significant 32 bits */
                u4_hi = (ss_uint4_t)-(ss_int4_t)I4_ISNEG(u4);
                goto bytes_4;
            case 3:
                /* do sign extension to most significant 32 bits */
                u4_hi = (ss_uint4_t)-(ss_int4_t)I4_ISNEG(u4);
                goto bytes_3;
            case 2:
                /* do sign extension to most significant 32 bits */
                u4_hi = (ss_uint4_t)-(ss_int4_t)I4_ISNEG(u4);
                goto bytes_2;
            default: /* 1 */
                /* do sign extension to most significant 32 bits */
                u4_hi = (ss_uint4_t)-(ss_int4_t)I4_ISNEG(u4);
                goto bytes_1;
        }
 bytes_8:;
        u4 = (u4 << SS_CHAR_BIT) | *buf++;
 bytes_7:;
        u4 = (u4 << SS_CHAR_BIT) | *buf++;
 bytes_6:;
        u4 = (u4 << SS_CHAR_BIT) | *buf++;
 bytes_5:;
        u4_hi = u4;
        u4 = *buf++;
 bytes_4:;
        u4 = (u4 << SS_CHAR_BIT) | *buf++;
 bytes_3:;
        u4 = (u4 << SS_CHAR_BIT) | *buf++;
 bytes_2:;
        u4 = (u4 << SS_CHAR_BIT) | *buf;
 bytes_1:;
        p_result->u4[SS_INT8_MS_U4_IDX] = u4_hi;
        p_result->u4[SS_INT8_LS_U4_IDX] = u4;
}

#endif /* SS_USE_INLINE || SSINT8_C */

#endif /* SSINT8_H */
