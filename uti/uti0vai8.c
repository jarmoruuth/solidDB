/*************************************************************************\
**  source       * uti0vai.c
**  directory    * uti
**  description  * 8-byte integer v-attribute functions
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

See other va format related documentation, also source
file uti0vai.c.

Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstring.h>
#include <ssc.h>
#include <ssdebug.h>
#include <sslimits.h>

#include "uti0va.h"
#include "uti1val.h"

static ss_int8_t posbias;
static ss_int8_t negbias;
static ss_int8_t value_0x1111110000000000;
static ss_int8_t value_0x1111000000000000;
static ss_int8_t value_0x1100000000000000;
static ss_int8_t value_0x0000010000000000;
static ss_int8_t value_0x0001000000000000;
static ss_int8_t value_0x0100000000000000;

static bool constants_initialized = FALSE;

/*##**********************************************************************\
 * 
 *		va_setint8
 * 
 * Initializes some logically constant values which need to be initialized
 * using the 8-byte integer library routines
 * 
 * Parameters : 
 * 
 * Return value : 
 * 
 * Limitations  : 
 *                
 * Globals used : 
 */
static void init_constants(void)
{
        SsInt8SetInt4(&posbias, VA_POS_BIAS);
        SsInt8SetInt4(&negbias, VA_NEG_BIAS);
        SsInt8Set2Uint4s(&value_0x1111110000000000, 0x11111100UL, 0x00000000UL);
        SsInt8Set2Uint4s(&value_0x1111000000000000, 0x11110000UL, 0x00000000UL);
        SsInt8Set2Uint4s(&value_0x1100000000000000, 0x11000000UL, 0x00000000UL);
        SsInt8Set2Uint4s(&value_0x0000010000000000, 0x00000100UL, 0x00000000UL);
        SsInt8Set2Uint4s(&value_0x0001000000000000, 0x00010000UL, 0x00000000UL);
        SsInt8Set2Uint4s(&value_0x0100000000000000, 0x01000000UL, 0x00000000UL);
        constants_initialized = TRUE;
}

/* conditionally call the above init_constants() */
#define INIT_CONSTANTS_IF_NEEDED() \
        if (!constants_initialized) {\
            init_constants();\
        }

/*##**********************************************************************\
 * 
 *		va_setint8
 * 
 * Set an 8-byte integer value to a v-attribute
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to v-attribute to store in
 *
 *	value - in
 *		the 8-byte integer to store
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 *                
 * Globals used : 
 */
va_t* va_setint8(
        va_t* target_va,
        ss_int8_t parvalue)
{
        ss_int4_t i4;
        va_index_t long_len;
        
        if (SsInt8ConvertToInt4(&i4, parvalue)) {
            /* fits into 4-byte integer, use va_setlong() */
            return (va_setlong(target_va, (long)i4));
        }
        INIT_CONSTANTS_IF_NEEDED();
        if (SsInt8IsNegative(parvalue)) {
            SsInt8AddUint2(&parvalue, parvalue, VA_NEG_BIAS);
            if (SsInt8Cmp(parvalue, value_0x1111110000000000) >= 0) {
                long_len = 5;
            } else if (SsInt8Cmp(parvalue, value_0x1111000000000000) >= 0) {
                long_len = 6;
            } else if (SsInt8Cmp(parvalue, value_0x1100000000000000) >= 0) {
                long_len = 7;
            } else {
                long_len = 8;
            }
            TWO_BYTE_STORE(target_va, long_len + 1, VA_INT_MAX_BYTES - long_len);
        } else {
            /* non-negative value */
            SsInt8AddInt8(&parvalue, parvalue, posbias);
            if (SsInt8Cmp(parvalue, value_0x0000010000000000) < 0) {
                long_len = 5;
            } else if (SsInt8Cmp(parvalue, value_0x0001000000000000) < 0) {
                long_len = 6;
            } else if (SsInt8Cmp(parvalue, value_0x0100000000000000) < 0) {
                long_len = 7;
            } else {
                long_len = 8;
            }
            TWO_BYTE_STORE(target_va, long_len + 1, 0xFF - VA_INT_MAX_BYTES + long_len);
        }
        {
            /* set the value bytes */
            ss_uint4_t ls_uint4 = SsInt8GetLeastSignificantUint4(parvalue);
            ss_uint4_t ms_uint4 = SsInt8GetMostSignificantUint4(parvalue);
            va_index_t long_len_minus_5 = long_len - 5;
            ss_byte_t* targetptr;
            
            /* first set the occupied bytes containing the bits from range
             * 32 - 63, in left-to-right order. The looping count is in
             * the range 1 - 4.
             */
            for (targetptr = (ss_byte_t*)&(target_va->c[2]) + long_len_minus_5;
                 ;
                 targetptr--, ms_uint4 >>= SS_CHAR_BIT, long_len_minus_5--)
            {
                *targetptr = (ss_byte_t)ms_uint4;
                if (long_len_minus_5 == 0) {
                    break;
                }
            }
            /* Then set bytes containing the bits 0 - 31 using fastest
             * available store mechanism. (all of them are occupied,
             * because smaller values are set using va_setlong()
             */
            targetptr = (ss_byte_t*)&(target_va->c[2]) + long_len - 4;
            SS_UINT4_STORE_MSB1ST(targetptr, ls_uint4);
        }
        return (target_va);
}


/*##**********************************************************************\
 * 
 *		va_getint8
 * 
 * Read an 8-byte integer from a v-attribute.
 * 
 * Parameters : 
 * 
 *	p_va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the 8-byte integer
 * 
 * Limitations  : 
 * 
 *      no overflow checks
 * 
 * Globals used : 
 */
ss_int8_t va_getint8(va_t* p_va)
{
        unsigned first_byte;
        ss_int8_t result;

        first_byte = p_va->c[1];

        ss_assert(!va_testnull(p_va));
        first_byte = p_va->c[1];
        if (VA_INT_MAX_BYTES <= first_byte &&
            first_byte <= 0xFF - VA_INT_MAX_BYTES)
        {
            ss_int4_t result_i4;
            /* value is in 1 byte! */
            ss_dassert(p_va->c[0] == 1);
            result_i4 = SIGN_EXTEND_TO_LONG(first_byte - VA_INT_BIAS);
            SsInt8SetInt4(&result, result_i4);
        } else {
            ss_byte_t* ptr;
            ss_uint4_t result_lo; /* lowest 32 bits */
            ss_uint4_t result_hi; /* highest 32 bits */
            ss_int8_t* p_bias; /* see below how this is used */

            INIT_CONSTANTS_IF_NEEDED();
            if (first_byte < VA_INT_MAX_BYTES) { /* negative */
                /* calculate length into first_byte */
                first_byte = VA_INT_MAX_BYTES - first_byte;

                result_hi = result_lo = ~(ss_uint4_t)0;
                ss_dassert(first_byte <= sizeof(ss_int8_t));
                ss_dassert((unsigned)p_va->c[0] == first_byte + 1);
                p_bias = &negbias;
            } else { /* positive, calculate length into first_byte */
                first_byte -= 0xFF - VA_INT_MAX_BYTES;
                ss_dassert(first_byte <= sizeof(ss_int8_t));
                ss_dassert((unsigned)p_va->c[0] == first_byte + 1)
                result_hi = result_lo = 0;
                p_bias = &posbias;
            }
            /* get bytes */
            for (ptr = &p_va->c[2]; first_byte > 4; first_byte--, ptr++) {
                result_hi = (result_hi << SS_CHAR_BIT) | *ptr;
            }
            for (; first_byte > 0; first_byte--, ptr++) {
                result_lo = (result_lo << SS_CHAR_BIT) | *ptr;
            }
            SsInt8Set2Uint4s(&result, result_hi, result_lo);
            SsInt8SubtractInt8(&result, result, *p_bias);
        }
        return (result);
}

