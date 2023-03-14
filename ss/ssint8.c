/*************************************************************************\
**  source       * ssint8.c
**  directory    * ss
**  description  * Portable 64 bit (= 8byte) integer routines
**               * (only the set needed by current odbc3 driver
**               * is currently supported).
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

Limitations:
-----------

Error handling:
--------------

None

Objects used:
------------

None

Preconditions:
-------------


Multithread considerations:
--------------------------

This code is re-entrant

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define SSINT8_C /* function instantiation if !SS_USE_INLINE */
#include "ssenv.h"
#include "sslimits.h"
#include "ssstring.h"
#include "ssc.h"
#include "ssctype.h"
#include "ssdebug.h"
#include "ssltoa.h"
#include "ssint8.h"




/*##**********************************************************************\
 *		
 *      SsInt8IsNegative
 * 
 * Checks whether an int8 value is negative
 * 
 * Parameters :
 *      i8 - in
 *          the value to test
 *
 * Return value :
 *      TRUE - i8 is negative
 *      FALSE - i8 is nonnegative
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8IsNegative
bool SsInt8IsNegative(ss_int8_t i8)
{
        bool isneg = I8_ISNEG(i8);

        ss_dassert(isneg == TRUE || isneg == FALSE);
        return (isneg);
}
#endif /* !defined(SsInt8IsNegative) */

/*#***********************************************************************\
 *		
 *      SsInt8AddUint2WithShift
 * 
 * adds a unsigned short integer (ss_uint2_t) to
 * an int8 value with shift position multiple of 16 bits
 * (0 means add to least significant position, 3 means to
 * most significant)
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value (1st operand)
 *
 *      u2 - in
 *          the uint2 value (2nd operand)
 *
 * Return value :
 *      TRUE - success
 *      FALSE - i8 if overflow occured (value of i8 changed from
 *             positive to negative.)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool SsInt8AddUint2WithShift(ss_int8_t* p_result, ss_int8_t i8, ss_uint2_t u2, int shift)
{
        uint i;
        ss_uint4_t u4;
        bool wasneg = I8_ISNEG(i8);
        int shift_with_direction = shift * SS_SIGNIFICANCE_GROW_DIRECTION;

        ss_rc_dassert(shift >= 0 && shift <= 3, shift);
        *p_result = i8;
        for (i = SS_INT8_LS_U2_IDX + shift_with_direction;
             u2 != 0;
             i += SS_SIGNIFICANCE_GROW_DIRECTION)
        {
            u4 = (ss_uint4_t)i8.u2[i] + (ss_uint4_t)u2;
            p_result->u2[i] = (ss_uint2_t)u4;
            u2 = (ss_uint2_t)(u4 >> (sizeof(ss_uint2_t) * SS_CHAR_BIT));
            ss_dassert(u2 == 0 || u2 == 1);
            if (i == SS_INT8_MS_U2_IDX) {
                break;
            }
        }
        if (!wasneg && I8_ISNEG(*p_result)) {
            return (FALSE);
        }
        return (TRUE);
}

/*##**********************************************************************\
 *		
 *      SsInt8AddUint2
 * 
 * adds a unsigned short integer (ss_uint2_t) to
 * an int8 value
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value (1st operand)
 *
 *      u2 - in
 *          the uint2 value (2nd operand)
 *
 * Return value :
 *      TRUE - success
 *      FALSE - i8 if overflow occured (value of i8 changed from
 *             positive to negative.)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8AddUint2(ss_int8_t* p_result, ss_int8_t i8, ss_uint2_t u2)
{
        bool succp = SsInt8AddUint2WithShift(p_result, i8, u2, 0);
        return (succp);
}


/*##**********************************************************************\
 *		
 *      SsInt8AddUint4
 * 
 * adds a unsigned integer (ss_uint4_t) to
 * an int8 value
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value (1st operand)
 *
 *      u4 - in
 *          the uint4 value (2nd operand)
 *
 * Return value :
 *      TRUE - success
 *      FALSE - i8 if overflow occured (value of i8 changed from
 *             positive to negative.)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8AddUint4(ss_int8_t* p_result, ss_int8_t i8, ss_uint4_t u4)
{
        bool succp;
        ss_int8_t tmp_result;
        ss_uint2_t ls_uint2 = (ss_uint2_t)u4;
        ss_uint2_t ms_uint2 = (ss_uint2_t)(u4 >> (SS_CHAR_BIT * sizeof(ss_uint2_t)));
        succp = SsInt8AddUint2WithShift(&tmp_result, i8, ls_uint2, 0);
        succp &= SsInt8AddUint2WithShift(p_result, tmp_result, ms_uint2, 1);
        return (succp);
}


/*##**********************************************************************\
 *		
 *      SsInt8Negate
 * 
 * Changes the sign of the int8 value
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value to negate
 *
 * Return value :
 *      TRUE - success
 *      FALSE - i8 if overflow occured (tried to change the smallest int8
 *              value to its positive equivalent which does not exist).
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8Negate(ss_int8_t* p_result, ss_int8_t i8)
{
        bool wasneg = I8_ISNEG(i8);
        
        i8.u4[0] = ~i8.u4[0];
        i8.u4[1] = ~i8.u4[1];
        SsInt8AddUint2(p_result, i8, 1);
        if (wasneg) {
            if (I8_ISNEG(*p_result)) {
                return (FALSE);
            }
        }
        return (TRUE);
}

/*##**********************************************************************\
 *		
 *      SsInt8MultiplyByInt2
 * 
 * Multiplies int8 value by int2 value
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      i8 - in
 *          the int8 value (1st operand)
 *
 *      i2 - in
 *          the int2 value (2nd operand)
 *
 * Return value :
 *      TRUE - success
 *      FALSE - if overflow occured 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8MultiplyByInt2(ss_int8_t* p_result, ss_int8_t i8, ss_int2_t i2)
{
        uint i;
        bool neg1;
        bool neg2;
        bool neg;
        ss_uint4_t multiplier;
        ss_uint4_t val;
        bool succp = TRUE;

        neg1 = I8_ISNEG(i8);
        if (neg1) {
            SsInt8Negate(&i8, i8);
        }
        neg2 = I2_ISNEG(i2);
        if (neg2) {
            i2 = -i2;
        }
        multiplier = (ss_uint4_t)(ss_uint2_t)i2;
        neg = neg1 ^ neg2;
        for (val = 0, i = SS_INT8_LS_U2_IDX;
             ;
             i += SS_SIGNIFICANCE_GROW_DIRECTION)
        {
            val = (ss_uint4_t)i8.u2[i] * multiplier + val;
            p_result->u2[i] = (ss_uint2_t)val;
            val >>= (sizeof(ss_uint2_t) * SS_CHAR_BIT);
            if (i == SS_INT8_MS_U2_IDX) {
                break;
            }
        }
        if (val || I8_ISNEG(*p_result)) {
            /* overflow! */
            succp = FALSE;
        }
        if (neg) {
            SsInt8Negate(p_result, *p_result);
        }
        return (succp);
}

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
void SsInt8ShrBy8Bits(ss_int8_t* p_result, ss_int8_t i8)
{
        int i;

        i = SS_INT8_LS_U1_IDX;
        do {
            p_result->u1[i] = i8.u1[i + SS_SIGNIFICANCE_GROW_DIRECTION];
            i += SS_SIGNIFICANCE_GROW_DIRECTION;
        } while (i != SS_INT8_MS_U1_IDX);
        p_result->u1[i] = (ss_uint1_t)0;
}

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
#ifndef SsInt8GetLeastSignificantByte
ss_byte_t SsInt8GetLeastSignificantByte(ss_int8_t i8)
{
        return (i8.u1[SS_INT8_LS_U1_IDX]);
}
#endif /* !defined(SsInt8GetLeastSignificantByte) */

/*##**********************************************************************\
 *
 *      SsInt8GetNthOrderByte
 *
 * Gets Nth least significant byte of 8-byte integer
 *
 * Parameters:
 *      i8 - in
 *          8-byte integer value
 *
 *      n - in
 *          value 0 .. 7 (0=Least significant, 7=Most significant)
 *
 * Return value:
 *      Value of the byte
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8GetNthOrderByte
ss_byte_t SsInt8GetNthOrderByte(ss_int8_t i8, uint n)
{
        ss_rc_dassert(n < sizeof(ss_int8_t), n);
        return (i8.u1[SS_INT8_LS_U1_IDX + (int)n * SS_SIGNIFICANCE_GROW_DIRECTION]);
}
#endif /* !defined(SsInt8GetNthOrderByte) */

/*##**********************************************************************\
 *		
 *      SsInt8Set2Uint4s
 * 
 * Sets the 2 32-bit unsigned integer values to int8 (=constructor)
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      u4high - in
 *          Highest 32 bits of value to set
 *
 *      u4low - in
 *          Lowest 32 bits of value to set
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8Set2Uint4s
void SsInt8Set2Uint4s(ss_int8_t* p_result, ss_uint4_t u4high, ss_uint4_t u4low)
{
        I8_SET_2_U4(p_result, u4high, u4low);
}
#endif /* !defined(SsInt8Set2Uint4s) */

/*##**********************************************************************\
 *		
 *      SsInt8SetUint4
 * 
 * Sets the 32-bit unsigned integer value to int8
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 *      u4 - in
 *          value to set
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8SetUint4
void SsInt8SetUint4(ss_int8_t* p_result, ss_uint4_t u4)
{
        I8_SET_U4(p_result, u4);
}
#endif /* !defined(SsInt8SetUint4) */

/*##**********************************************************************\
 *		
 *      SsInt8Set0
 * 
 * Sets the value 0 to int8
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to ss_uint8_t variable where to store result
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8Set0
void SsInt8Set0(ss_int8_t* p_result)
{
        I8_SET_0(p_result);
}
#endif /* !defined(SsInt8Set0) */

/*##**********************************************************************\
 *		
 *      SsInt8Equal
 * 
 * Checks whether two int8's have the same value
 * 
 * Parameters :
 *
 *      i8_1 - in
 *          1st value
 *
 *      i8_2 - in
 *          2nd value
 *
 * Return value :
 *      TRUE - values are equal
 *      FALSE - otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8Equal
bool SsInt8Equal(ss_int8_t i8_1, ss_int8_t i8_2)
{
#ifdef SS_NATIVE_UINT8_T
        return (i8_1.u8[0] == i8_2.u8[0]);
#else
        return (i8_1.u4[0] == i8_2.u4[0] && i8_1.u4[1] == i8_2.u4[1]);
#endif
}
#endif /* !defined(SsInt8Equal) */

/*##**********************************************************************\
 *		
 *      SsInt8SetInt4
 * 
 * sets 32-bit signed integer value to int8
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      i4 - in
 *          32-bit signed integer value
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8SetInt4
void SsInt8SetInt4(ss_int8_t* p_result, ss_int4_t i4)
{
        I8_SET_I4(p_result, i4);
}
#endif /* !defined(SsInt8SetInt4) */

/*##**********************************************************************\
 *		
 *      SsInt8ConvertToInt4
 * 
 * converts an int8 value to signed 32 bit integer
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int4 where result is to be stored.
 *          The value will be stored in case of overflow also
 *
 *      i8 - in
 *          int8 value to convert
 *
 * Return value :
 *      TRUE - success
 *      FALSE - result lost significant bits.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8ConvertToInt4(ss_int4_t* p_result, ss_int8_t i8)
{
        bool succp = TRUE;
        ss_int4_t result = i8.u4[SS_INT8_LS_U4_IDX];

        *p_result = result;
        if (I8_ISNEG(i8)) {
            if (i8.u4[SS_INT8_MS_U4_IDX] != (ss_uint4_t)~0UL
            ||  !I4_ISNEG(result)) {
                succp = FALSE;
            }
        } else {
            if (i8.u4[SS_INT8_MS_U4_IDX] != 0
            ||  I4_ISNEG(result))
            {
                succp = FALSE;
            }
        }
        return (succp);
}

/*##**********************************************************************\
 *
 *      SsInt8ConvertToLong
 *
 * Converts 8-byte integer into native long
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to variable where to store the result
 *
 *      i8 - in
 *          8-byte integer value to convert
 *
 * Return value:
 *      TRUE - conversion was successful
 *      FALSE - value lost in conversion
 *
 * Limitations:
 *
 * Globals used:
 */
bool SsInt8ConvertToLong(long* p_result, ss_int8_t i8)
{
        bool succp;
        if (sizeof(long) == sizeof(i8)) {
#ifdef SS_NATIVE_UINT8_T
            *p_result = (long)i8.u8[0];
#else /* SS_NATIVE_UINT8_T */
            memcpy(p_result, &i8, sizeof(long));
#endif /* SS_NATIVE_UINT8_T */
            succp = TRUE;
        } else {
            ss_dassert(sizeof(long) == sizeof(ss_int4_t));
            succp = SsInt8ConvertToInt4((ss_int4_t*)p_result, i8);
        }
        return (succp);
}

/*##**********************************************************************\
 *		
 *      SsInt8ConvertToSizeT
 * 
 * converts an int8 value to native size_t
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to size_t where result is to be stored.
 *          The value will be stored in case of overflow also
 *
 *      i8 - in
 *          int8 value to convert
 *
 * Return value :
 *      TRUE - success
 *      FALSE - result lost significant bits.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8ConvertToSizeT(size_t* p_result, ss_int8_t i8)
{
        if (sizeof(size_t) == sizeof(i8)) {
            memcpy(p_result, &i8, sizeof(i8));
            return (TRUE);
        } else {
            *p_result = i8.u4[SS_INT8_LS_U4_IDX];

            return (i8.u4[SS_INT8_MS_U4_IDX] == 0);
        }
}

/*##**********************************************************************\
 *		
 *      SsInt8ConvertToDouble
 * 
 * converts an int8 value to 64 bit double
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to double where result is to be stored.
 *
 *      i8 - in
 *          int8 value to convert
 *
 * Return value :
 *      TRUE - success
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8ConvertToDouble(double* p_result, ss_int8_t i8)
{
        double addbit;
        double result;
        ss_uint4_t u4_bitmask;
        uint u4_index;
        bool wasneg = I8_ISNEG(i8);

        if (wasneg) {
            SsInt8Negate(&i8, i8);
        }
        addbit = 1;
        result = 0;
        for (u4_index = SS_INT8_LS_U4_IDX;
             ;
             u4_index += SS_SIGNIFICANCE_GROW_DIRECTION)
        {
            for (u4_bitmask = 1; u4_bitmask != 0; u4_bitmask <<= 1U, addbit *= 2) {
                if ((i8.u4[u4_index] & u4_bitmask) != 0) {
                    result += addbit;
                }
            }   
            if (u4_index == SS_INT8_MS_U4_IDX) {
                break;
            }
        }
        if (wasneg) {
            *p_result = -result;
        } else {
            *p_result = result;
        }
        return (TRUE);
}

 
/*##**********************************************************************\
 *		
 *      SsInt8SetDouble
 * 
 * sets double value to int8 (truncating digits after decimal point)
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      d - in
 *          double value
 *
 * Return value :
 *      TRUE - success
 *      FALSE - absolute value of d is so large that it causes overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8SetDouble(
        ss_int8_t* p_result,
        double d)
{
        ss_int8_t result;
        double dbitmask;
        int bitno;
        bool wasneg = FALSE;

        I8_SET_0(&result);
        if (d < 0) {
            wasneg = TRUE;
            d = -d;
        }
        /* scan for smallest power of 2 that is greater than d */
        dbitmask = 1;
        for (bitno = 0;
             ;
             bitno++, dbitmask *= 2)
        {
            if (dbitmask > d) {
                break;
            }
            if ((size_t)bitno >= sizeof(ss_int8_t) * SS_CHAR_BIT) {
                return (FALSE);
            }
        }
        /* now scan bits from left to right and set accordingly */
        for (;;) {
            bitno--;
            dbitmask /= 2;
            if (bitno < 0) {
                break;
            }
            if (dbitmask <= d) {
                d -= dbitmask;
                I8_SETBIT(&result, (uint)bitno);
            }
        }
        if (wasneg) {
            SsInt8Negate(&result, result);
        }
        *p_result = result;
        return (TRUE);
}

            
/*##**********************************************************************\
 *		
 *      SsInt8AddInt8
 * 
 * Adds two 8-byte integers
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      i8_1 - in
 *          1st operand
 *
 *      i8_2 - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8AddInt8(
        ss_int8_t* p_result,
        ss_int8_t i8_1,
        ss_int8_t i8_2)
{
        bool isneg1 = I8_ISNEG(i8_1);
        bool isneg2 = I8_ISNEG(i8_2);
        uint i;
        int shift;

        for (i = SS_INT8_LS_U2_IDX, shift = 0;
             ;
             i += SS_SIGNIFICANCE_GROW_DIRECTION, shift++)
        {
            SsInt8AddUint2WithShift(&i8_1, i8_1, i8_2.u2[i], shift);
            if (i == SS_INT8_MS_U2_IDX) {
                break;
            }
        }
        *p_result = i8_1;
        if (isneg1 == isneg2 && isneg1 != I8_ISNEG(i8_1)) {
            /* both operands had same sign and result has different sign, over/underflow! */
            return (FALSE);
        }
        return (TRUE);
}

/*##**********************************************************************\
 *		
 *      SsInt8SubtractInt8
 * 
 * Subtracts two 8-byte integers
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      i8_1 - in
 *          1st operand
 *
 *      i8_2 - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8SubtractInt8(
        ss_int8_t* p_result,
        ss_int8_t i8_1,
        ss_int8_t i8_2)
{
        bool succp;

        succp = SsInt8Negate(&i8_2, i8_2);
        succp &= SsInt8AddInt8(p_result, i8_1, i8_2);
        return (succp);
}

/*#***********************************************************************\
 *		
 *      SsInt8MultiplyByUint2WithShift
 * 
 * Multiplies int8 with uint2 shifted left a multiple of 16 bits
 * (0 means no shift, 3 means shift 48 bits)
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      i8 - in
 *          1st operand
 *
 *      u2 - in
 *          2nd operand
 *
 *      shift - in,
 *          number of 16-bits to shift (legal range 0 - 3)
 *
 * Return value :
 *      TRUE - success
 *      FALSE - overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool SsInt8MultiplyByUint2WithShift(
        ss_int8_t* p_result_in_out, /* note in/out mode !!! */
        ss_int8_t i8,
        ss_uint2_t u2,
        int shift)
{
        bool succp = TRUE;
        ss_int8_t result;
        int i;
        int i_limit = SS_INT8_MS_U2_IDX - (shift * SS_SIGNIFICANCE_GROW_DIRECTION);
        ss_uint4_t u4;
        ss_rc_dassert(shift >= 0 && shift <= 3, shift);
        result = *p_result_in_out;

        for (i = SS_INT8_LS_U2_IDX; ; i += SS_SIGNIFICANCE_GROW_DIRECTION) {
            u4 = i8.u2[i] * u2;
            succp &= SsInt8AddUint2WithShift(
                    &result, result, (ss_uint2_t)u4,
                    shift + ((i - SS_INT8_LS_U2_IDX) * SS_SIGNIFICANCE_GROW_DIRECTION));
            u4 >>= (sizeof(ss_uint2_t) * SS_CHAR_BIT);
            if (i == i_limit) {
                break;
            }
            succp &= SsInt8AddUint2WithShift(
                    &result, result, (ss_uint2_t)u4,
                    1 + shift + ((i - SS_INT8_LS_U2_IDX) * SS_SIGNIFICANCE_GROW_DIRECTION));
        }
        if (u4 != 0) {
            succp = FALSE;
        }
        *p_result_in_out = result;
        return (succp);
}

/*##**********************************************************************\
 *		
 *      SsInt8MultiblyByInt8
 * 
 * Multiplies two 8-byte integers
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      i8_1 - in
 *          1st operand
 *
 *      i8_2 - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8MultiplyByInt8(
        ss_int8_t* p_result,
        ss_int8_t i8_1,
        ss_int8_t i8_2)
{
        bool succp = TRUE;
        bool negate_result = FALSE;
        int i;
        int shift;
        ss_int8_t result;
        ss_uint2_t u2;

        I8_SET_0(&result);
        if (I8_ISNEG(i8_1)) {
            succp &= SsInt8Negate(&i8_1, i8_1);
            negate_result ^= TRUE;
        }
        if (I8_ISNEG(i8_2)) {
            succp &= SsInt8Negate(&i8_2, i8_2);
            negate_result ^= TRUE;
        }
        for (i = SS_INT8_LS_U2_IDX, shift = 0;
             ;
             i += SS_SIGNIFICANCE_GROW_DIRECTION, shift++)
        {
            u2 = i8_2.u2[i];
            if (u2 != 0) {
                succp &= SsInt8MultiplyByUint2WithShift(&result, i8_1, u2, shift);
            }
            if (i == SS_INT8_MS_U2_IDX) {
                break;
            }
        }
        if (negate_result) {
            succp &= SsInt8Negate(p_result, result);
        } else {
            *p_result = result;
        }
        return (succp);
}

/*#***********************************************************************\
 *		
 *      SsInt8UnsignedDivide
 * 
 * Divides two 8-byte integers using unsigned semantics
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *          (NULL is also legal and implies result is not needed)
 *
 *      p_remainder - out, use
 *          pointer to int8 where remainder is to be stored
 *          (NULL is also legal and implies remainder is not needed)
 *
 *      dividend - in
 *          1st operand
 *
 *      divisor - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - division by zero (assertion failure in DEBUG compile,
 *              should be checked before calling this routine)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static bool SsInt8UnsignedDivide(
        ss_int8_t* p_result,
        ss_int8_t* p_remainder,
        ss_int8_t dividend,
        ss_int8_t divisor)
{
        int bitshift;
        ss_int8_t resbit;
        ss_int8_t result;

        if (I8_IS_0(divisor)) {
            ss_derror;
            return (FALSE);
        }
        I8_SET_0(&result);
        I8_SET_U4(&resbit, 1);
        /* Shift divisor bits left till it is bigger or equal to the dividend
         * or else its most significant bit becomes 1.
         * Simultaneously shift a 1-bit mask left as many positions.
         */
        for (bitshift = 0;
             !I8_ISNEG(divisor) && I8_UNSIGNEDCMP(dividend, divisor) > 0;
             bitshift++)
        {
            I8_UNSIGNEDSHIFTLEFT1(&divisor);
            I8_UNSIGNEDSHIFTLEFT1(&resbit);
        }
        /* Now do the actual divide algorithm by shifting bit mask
         * to right till the shift becomes zero
         */
        for (;;) {
            while (I8_UNSIGNEDCMP(dividend, divisor) < 0) {
                if (bitshift == 0) {
                    /* we do not calculate fractions, this an integer! */
                    goto completed;
                }
                I8_UNSIGNEDSHIFTRIGHT1(&divisor);
                I8_UNSIGNEDSHIFTRIGHT1(&resbit);
                bitshift--;
            }
            SsInt8AddInt8(&result, result, resbit);
            SsInt8SubtractInt8(&dividend, dividend, divisor);
        }
 completed:;
        if (p_remainder != NULL) {
            *p_remainder = dividend;
        }
        if (p_result != NULL) {
            *p_result = result;
        }
        return (TRUE);
}

/*##**********************************************************************\
 *		
 *      SsInt8DivideByInt8
 * 
 * Divides two 8-byte integers using signed semantics
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      dividend - in
 *          1st operand
 *
 *      divisor - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - division by zero 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8DivideByInt8(
        ss_int8_t* p_result,
        ss_int8_t dividend,
        ss_int8_t divisor)
{
        bool succp;
        bool dividend_wasneg = I8_ISNEG(dividend);
        bool divisor_wasneg = I8_ISNEG(divisor);
        bool result_isneg = dividend_wasneg ^ divisor_wasneg;
        ss_int8_t result;

        if (I8_IS_0(divisor)) {
            return (FALSE);
        }
        if (dividend_wasneg) {
            SsInt8Negate(&dividend, dividend);
        }
        if (divisor_wasneg) {
            SsInt8Negate(&divisor, divisor);
        }
        succp = SsInt8UnsignedDivide(&result, NULL, dividend, divisor);
        if (result_isneg) {
            SsInt8Negate(&result, result);
        }
        *p_result = result;
        return (succp);
}

/*##**********************************************************************\
 *		
 *      SsInt8RemainderByInt8
 * 
 * Calculates division remainder of two 8-byte integers using signed semantics
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      dividend - in
 *          1st operand
 *
 *      divisor - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - division by zero
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8RemainderByInt8(
        ss_int8_t* p_result,
        ss_int8_t dividend,
        ss_int8_t divisor)
{
        bool succp;
        bool dividend_wasneg = I8_ISNEG(dividend);
        bool divisor_wasneg = I8_ISNEG(divisor);
        bool result_isneg = dividend_wasneg;
        ss_int8_t result;

        if (I8_IS_0(divisor)) {
            return (FALSE);
        }
        if (dividend_wasneg) {
            SsInt8Negate(&dividend, dividend);
        }
        if (divisor_wasneg) {
            SsInt8Negate(&divisor, divisor);
        }
        succp = SsInt8UnsignedDivide(NULL, &result, dividend, divisor);
        if (result_isneg) {
            SsInt8Negate(&result, result);
        }
        *p_result = result;
        return (succp);
}

/*##**********************************************************************\
 *		
 *      SsInt8DivWithRemByInt8
 * 
 * Divides two 8-byte integers using signed semantics and returns
 * the ANSI ldiv-compatible remainder also
 * 
 * Parameters :
 *
 *      p_result - out, use
 *          pointer to int8 where result is to be stored
 *
 *      p_remainder - out, use
 *          pointer to int8 where remainder is to be stored
 *
 *      dividend - in
 *          1st operand
 *
 *      divisor - in
 *          2nd operand
 *
 * Return value :
 *      TRUE - success
 *      FALSE - division by zero
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool SsInt8DivWithRemByInt8(
        ss_int8_t* p_result,
        ss_int8_t* p_remainder,
        ss_int8_t dividend,
        ss_int8_t divisor)
{
        bool succp;
        bool dividend_wasneg = I8_ISNEG(dividend);
        bool divisor_wasneg = I8_ISNEG(divisor);
        bool result_isneg = dividend_wasneg ^ divisor_wasneg;
        ss_int8_t result;
        ss_int8_t remainder;

        if (I8_IS_0(divisor)) {
            return (FALSE);
        }
        if (dividend_wasneg) {
            SsInt8Negate(&dividend, dividend);
        }
        if (divisor_wasneg) {
            SsInt8Negate(&divisor, divisor);
        }
        succp = SsInt8UnsignedDivide(&result, &remainder, dividend, divisor);
        if (result_isneg) {
            SsInt8Negate(&result, result);
        }
        if (dividend_wasneg) {
            SsInt8Negate(&remainder, remainder);
        }
        *p_result = result;
        *p_remainder = remainder;
        return (succp);
}

/*##**********************************************************************\
 *		
 *      SsInt8Cmp
 * 
 * Compares two int8's using signed semantics, return value has strcmp
 * semantics
 * 
 * Parameters :
 *
 *      i8_1 - in
 *          1st value
 *
 *      i8_2 - in
 *          2nd value
 *
 * Return value :
 *      < 0 if i8_1 < i8_2
 *      = 0 if i8_1 == i8_2
 *      > 0 if i8_1 > i8_2
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int SsInt8Cmp(ss_int8_t i8_1, ss_int8_t i8_2)
{
        int i;
        ss_int4_t cmp;

        cmp = (ss_int4_t)I8_ISNEG(i8_2) - (ss_int4_t)I8_ISNEG(i8_1);
        for (i = SS_INT8_MS_U2_IDX;
             cmp == 0;
             i -= SS_SIGNIFICANCE_GROW_DIRECTION)
        {
            cmp = (ss_int4_t)i8_1.u2[i] - (ss_int4_t)i8_2.u2[i];
            if (i == SS_INT8_LS_U2_IDX) {
                break;
            }
        }
        if (sizeof(int) < sizeof(ss_int4_t)) {
            /* a proper compiler will optimize this branch away if sizeof(int) >= 4 */
            if (cmp < 0) {
                return (-1);
            } else if (cmp > 0) {
                return (1);
            } else {
                return (0);
            }
        }
        return ((int)cmp);
}

/*##**********************************************************************\
 *		
 *      SsInt8GetLeastSignificantUint4
 * 
 * Gets the value of the least significant 32 bits
 * 
 * Parameters :
 *
 *      i8 - in
 *          the int8 value
 *
 * Return value :
 *      unsigned 32-bit value of the least significant
 *      32 bits of the integer value.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8GetLeastSignificantUint4
ss_uint4_t SsInt8GetLeastSignificantUint4(ss_int8_t i8)
{
        return (I8_LS_UINT4(i8));
}
#endif /* !defined(SsInt8GetLeastSignificantUint4) */

/*##**********************************************************************\
 *		
 *      SsInt8GetMostSignificantUint4
 * 
 * Gets the value of the most significant 32 bits
 * 
 * Parameters :
 *
 *      i8 - in
 *          the int8 value
 *
 * Return value :
 *      unsigned 32-bit value of the most significant
 *      32 bits of the integer value.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8GetMostSignificantUint4
ss_uint4_t SsInt8GetMostSignificantUint4(ss_int8_t i8)
{
        return (I8_MS_UINT4(i8));
}
#endif /* !defined(SsInt8GetMostSignificantUint4) */

/*##**********************************************************************\
 * 
 *		SsInt8ToAscii
 * 
 * 8-byte integer to ascii conversion function
 * 
 * Parameters : 
 * 
 *	i8 - in
 *		value to convert
 *		
 *	buf - out
 *		string buffer
 *		
 *	radix - in
 *		radix of string value, legal range: 2 .. 36
 *		
 *	width - in
 *		print-width, or zero for dynamic width
 *		
 *	leftfillch - in
 *		character that is used to left fill the string.
 *          Used only when width >= 2
 *		
 *	is_signed - in
 *		if radix == 10 and is_signed and l < 0, the value is
 *          prepended with '-'
 *		
 * Return value :
 *      strlen(buf)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsInt8ToAscii(
        ss_int8_t i8,
        char* buf,
        uint radix,
        size_t width,
        char leftfillch,
        bool is_signed)
{
        char* p;
        int sign;
        size_t n;
        ss_int8_t rem;
        ss_int8_t radix8;
        ss_uint4_t digitvalue;
        ss_debug(bool succp;)

        if (radix == 10 && is_signed && I8_ISNEG(i8)) {
            sign = 1;
            SsInt8Negate(&i8, i8);
        } else {
            sign = 0;
        }
        ss_dassert((uint)(radix - 2) <=
            (uint)(sizeof(ss_ltoa_int2ascii_xlat)/sizeof(ss_ltoa_int2ascii_xlat[0]) - 2));
        if ((uint)(radix - 2) >
            (uint)(sizeof(ss_ltoa_int2ascii_xlat)/sizeof(ss_ltoa_int2ascii_xlat[0]) - 2))
        {
            return (0);
        }
        I8_SET_U4(&radix8, (ss_uint4_t)radix);
        if (width == 0) {
            char* p1;

            p = buf + sign;
            p1 = p;
            if (sign) {
                buf[0] = '-';
            }
            do {
                ss_debug(succp = )
                    SsInt8UnsignedDivide(&i8, &rem, i8, radix8);
                ss_dassert(succp == TRUE);
                ss_dassert(SsInt8GetMostSignificantUint4(rem) == 0);
                digitvalue = I8_LS_UINT4(rem);
                ss_dassert(digitvalue < radix);
                *p = SS_LTOA_DIGITXLATE(digitvalue);
                p++;
            } while (!I8_IS_0(i8));
            *p = '\0';
            width = (p - buf);
            /* reverse the string */
            for (; --p > p1; p1++) {
                char ch;

                ch = *p;
                *p = *p1;
                *p1 = ch;
            }
        } else {
            p = buf + width;
            *p = '\0';
            
            for (n = width; n;) {
                p--;
                n--;
                ss_debug(succp = )
                    SsInt8UnsignedDivide(&i8, &rem, i8, radix8);
                ss_dassert(succp == TRUE);
                ss_dassert(SsInt8GetMostSignificantUint4(rem) == 0);
                digitvalue = I8_LS_UINT4(rem);
                ss_dassert(digitvalue < radix);
                *p = SS_LTOA_DIGITXLATE(digitvalue);
                if (I8_IS_0(i8)) {
                    break;
                }
            }
            if (n && leftfillch != '0') {
                if (sign) {
                    p--;
                    sign = 0;
                    n--;
                    *p = '-';
                }
            }
            while (n > (size_t)sign) {
                p--;
                n--;
                *p = leftfillch;
            }
            if (sign && n) {
                ss_dassert(n == 1);
                p[-1] = '-';
            }
        }
        return (width);
}

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
        char** p_mismatch)
{
        bool valid = FALSE;
        bool succp = TRUE;
        bool neg = FALSE;
        ss_int8_t v;
        ss_uint2_t t;
        
        ss_dassert(s != NULL);
        ss_dassert(p_result != NULL);
        ss_dassert(p_mismatch != NULL);

        I8_SET_0(&v);
        while (ss_isspace((ss_byte_t)*s)) {
            s++;
        }
        switch (*s) {
            case '-':
                neg = TRUE;
                /* FALLTHROUGH */
            case '+':
                s++;
                /* FALLTHROUGH */
            default:
                break;
        }
        for (;;) {
            switch (*s) {
                case '9': t = 9; break;
                case '8': t = 8; break;
                case '7': t = 7; break;
                case '6': t = 6; break;
                case '5': t = 5; break;
                case '4': t = 4; break;
                case '3': t = 3; break;
                case '2': t = 2; break;
                case '1': t = 1; break;
                case '0': t = 0; break;
                default: goto out;
            }
            valid = TRUE;
            if (!succp) {
                valid = FALSE;
                break;
            }
            succp = SsInt8MultiplyByInt2(&v, v, 10);
            if (!succp) {
                valid = FALSE;
                break;
            }
            succp = SsInt8AddUint2(&v, v, t);
            if (!succp) {
                /* special case for SS_UINT8_MIN */
                if (I8_LS_UINT4(v) != 0
                ||  I8_MS_UINT4(v) != (ss_uint4_t)0x80000000UL
                ||  !neg)
                {
                    valid = FALSE;
                    break;
                }
            }
            s++;
        }
 out:;
        if (neg) {
            SsInt8Negate(&v, v);
        }
        if (!valid) {
            I8_SET_0(p_result);
        } else {
            *p_result = v;
        }
        *p_mismatch = s;
        return (valid);
}

/*##**********************************************************************\
 *		
 *      SsInt8InitFrom2Uint4s
 * 
 * Sets the 2 32-bit unsigned integer values to int8 and returns that value
 * (=constructor)
 * 
 * Parameters :
 *
 *      u4high - in
 *          Highest 32 bits of value to set
 *
 *      u4low - in
 *          Lowest 32 bits of value to set
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_int8_t SsInt8InitFrom2Uint4s(ss_uint4_t u4high, ss_uint4_t u4low)
{
        ss_int8_t result;

        I8_SET_2_U4(&result, u4high, u4low);
        return (result);
}


/*##**********************************************************************\
 *		
 *      SsInt8Is0
 * 
 * Checks whether int8 has value 0
 * 
 * Parameters :
 *
 *      i8 - in
 *          value
 *
 * Return value :
 *      TRUE - value is 0
 *      FALSE - value != 0
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifndef SsInt8Is0
bool SsInt8Is0(ss_int8_t i8)
{
        return (I8_IS_0(i8));
}
#endif /* !defined(SsInt8Is0) */

/*##**********************************************************************\
 *
 *      SsInt8ShrNInPlace
 *
 * Shifts right n bits 8-byte integer value (zero filled from left!)
 *
 * Parameters:
 *      p_i8 - in out, use
 *          pointer to 8-byte integer variable to modify in place
 *
 *      n - in
 *          number of bits to shift
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8ShrNInPlace
void SsInt8ShrNInPlace(ss_int8_t* p_i8, uint n)
{
        ss_rc_dassert(n < sizeof(ss_int8_t) * SS_CHAR_BIT, n);
        I8_SHR_N(p_i8, n);
}
#endif /* !defined(SsInt8ShrNInPlace) */

/*##**********************************************************************\
 *
 *      SsInt8ShrN
 *
 * Shifts right n bits 8-byte integer value (zero filled from left!)
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to variable where to put result
 *
 *      i8 - in
 *          value to shift right
 *
 *      n - in
 *          number of bits to shift
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8ShrN
void SsInt8ShrN(ss_int8_t* p_result, ss_int8_t i8, uint n)
{
        *p_result = i8;
        SsInt8ShrNInPlace(p_result, n);
}
#endif /* !defined(SsInt8ShrN) */

/*##**********************************************************************\
 *
 *      SsInt8ShlNInPlace
 *
 * Shifts left n bits 8-byte integer value (zero filled from left!)
 *
 * Parameters:
 *      p_i8 - in out, use
 *          pointer to 8-byte integer variable to modify in place
 *
 *      n - in
 *          number of bits to shift
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8ShlNInPlace
void SsInt8ShlNInPlace(ss_int8_t* p_i8, uint n)
{
        ss_rc_dassert(n < sizeof(ss_int8_t) * SS_CHAR_BIT, n);
        I8_SHL_N(p_i8, n);
}
#endif /* !defined(SsInt8ShlNInPlace) */

/*##**********************************************************************\
 *
 *      SsInt8ShlN
 *
 * Shifts left n bits 8-byte integer value (zero filled from left!)
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to variable where to put result
 *
 *      i8 - in
 *          value to shift right
 *
 *      n - in
 *          number of bits to shift
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8ShlN
void SsInt8ShlN(ss_int8_t* p_result, ss_int8_t i8, uint n)
{
        *p_result = i8;
        SsInt8ShlNInPlace(p_result, n);
}
#endif /* !defined(SsInt8ShlN) */

/*##**********************************************************************\
 *
 *      SsInt8AndInt8InPlace
 *
 * Bitwise AND of 2 8-byte integer values
 *
 * Parameters:
 *      p_i8a - in out, use
 *          left operand, updated to contain result in-place
 *
 *      i8b - in
 *          right operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8AndInt8InPlace
void SsInt8AndInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b)
{
        I8_AND_I8(p_i8a, i8b);
}
#endif /* !defined(SsInt8AndInt8InPlace) */

/*##**********************************************************************\
 *
 *      SsInt8AndInt8
 *
 * Bitwise AND of 2 8-byte integers
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to 8-byte integer where the result will be stored
 *
 *      i8a - in
 *          1st operand
 *
 *      i8b - in
 *          2nd operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8AndInt8
void SsInt8AndInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b)
{
        *p_result = i8a;
        SsInt8AndInt8InPlace(p_result, i8b);
}
#endif /* !defined(SsInt8AndInt8) */

/*##**********************************************************************\
 *
 *      SsInt8OrInt8InPlace
 *
 * Bitwise OR of 2 8-byte integer values
 *
 * Parameters:
 *      p_i8a - in out, use
 *          left operand, updated to contain result in-place
 *
 *      i8b - in
 *          right operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8OrInt8InPlace
void SsInt8OrInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b)
{
        I8_OR_I8(p_i8a, i8b);
}
#endif /* !defined(SsInt8OrInt8InPlace) */

/*##**********************************************************************\
 *
 *      SsInt8OrInt8
 *
 * Bitwise OR of 2 8-byte integers
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to 8-byte integer where the result will be stored
 *
 *      i8a - in
 *          1st operand
 *
 *      i8b - in
 *          2nd operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8OrInt8
void SsInt8OrInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b)
{
        *p_result = i8a;
        SsInt8OrInt8InPlace(p_result, i8b);
}
#endif /* !defined(SsInt8OrInt8) */

/*##**********************************************************************\
 *
 *      SsInt8XorInt8InPlace
 *
 * Bitwise XOR of 2 8-byte integer values
 *
 * Parameters:
 *      p_i8a - in out, use
 *          left operand, updated to contain result in-place
 *
 *      i8b - in
 *          right operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8XorInt8InPlace
void SsInt8XorInt8InPlace(ss_int8_t* p_i8a, ss_int8_t i8b)
{
        I8_XOR_I8(p_i8a, i8b);
}
#endif /* !defined(SsInt8XorInt8InPlace) */

/*##**********************************************************************\
 *
 *      SsInt8XorInt8
 *
 * Bitwise XOR of 2 8-byte integers
 *
 * Parameters:
 *      p_result - out, use
 *          pointer to 8-byte integer where the result will be stored
 *
 *      i8a - in
 *          1st operand
 *
 *      i8b - in
 *          2nd operand
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
#ifndef SsInt8XorInt8
void SsInt8XorInt8(ss_int8_t* p_result, ss_int8_t i8a, ss_int8_t i8b)
{
        *p_result = i8a;
        SsInt8XorInt8InPlace(p_result, i8b);
}
#endif /* !defined(SsInt8XorInt8) */

#ifndef SsInt8GetHighestBitIndex
uint SsInt8GetHighestBitIndex(ss_int8_t i8)
{
        uint bit;
        ss_uint4_t u4;
        ss_uint4_t t;

        if ((u4 = i8.u4[SS_INT8_MS_U4_IDX]) != 0) {
            bit = 32;
        } else {
            u4 = i8.u4[SS_INT8_LS_U4_IDX];
            bit = 0;
        }
        t = u4 >> 16;
        if (t != 0) {
            u4 = t;
            bit += 16;
        }
        t = u4 >> 8;
        if (t != 0) {
            u4 = t;
            bit += 8;
        }
        t = u4 >> 4;
        if (t != 0) {
            u4 = t;
            bit += 4;
        }
        t = u4 >> 2;
        if (t != 0) {
            u4 = t;
            bit += 2;
        }
        if ((u4 >> 1) != 0) {
            bit += 1;
        }
        return (bit);
}
#endif /* !defined(SsInt8GetHighestBitIndex) */

#ifndef SsInt8UnsignedCmp
int SsInt8UnsignedCmp(ss_int8_t i8a, ss_int8_t i8b)
{
        return (I8_UNSIGNEDCMP(i8a, i8b));
}
#endif /* !defined(SsInt8UnsignedCmp) */

#ifndef SsInt8InvertSignBitInPlace
void SsInt8InvertSignBitInPlace(ss_int8_t* p_i8)
{
        I8_INV_SIGNBIT(p_i8);
}
#endif /* !defined(SsInt8InvertSignBitInPlace) */


