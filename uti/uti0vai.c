/*************************************************************************\
**  source       * uti0vai.c
**  directory    * uti
**  description  * Integer v-attribute functions
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

See nstdyn.doc.

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


/* macros **************************************************/


/*#**********************************************************************\
 * 
 *		STORE_VA_INT3
 * 
 * Stores a three-byte v-attribute integer representation, using a single
 * operation, if possible.  Takes care to truncate the value, if necessary.
 * 
 * Parameters : 
 * 
 *      va - out, use
 *		pointer to the v-attribute
 *
 *	va_len - in
 *		length of the v-attribute
 *
 *      int1 - in
 *		the first byte of the integer representation
 *
 *      value - in
 *		the variable that holds the rest of the representation
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 *      There must be four bytes of space at va.
 * 
 * Globals used : 
 */
#if defined(UNALIGNED_LOAD) && WORD_SIZE >= 4
#    define STORE_VA_INT3(va, va_len, int1, value) \
        *(FOUR_BYTE_T*)(va) = FOUR_BYTE_VALUE((va_len), (int1), (ss_byte_t)(value & 0xFF), 0);
#elif defined(UNALIGNED_LOAD) && WORD_SIZE == 2
#    define STORE_VA_INT3(va, va_len, int1, value) { \
            *(TWO_BYTE_T*)(va) = TWO_BYTE_VALUE((va_len), (int1)); \
            *((ss_byte_t*)(va)+2) = (ss_byte_t)value; \
        }
#else
#    define STORE_VA_INT3(va, va_len, int1, value) { \
            ((ss_byte_t*)(va))[0] = (va_len); \
            ((ss_byte_t*)(va))[1] = (int1); \
            ((ss_byte_t*)(va))[2] = (ss_byte_t)value; \
        }
#endif


/*#**********************************************************************\
 * 
 *		STORE_VA_INT4
 * 
 * Stores a four-byte v-attribute integer representation, using a single
 * operation, if possible.  Takes care to truncate the value, if necessary.
 * 
 * Parameters : 
 * 
 *      va - out, use
 *		pointer to the v-attribute
 *
 *	va_len - in
 *		length of the v-attribute
 *
 *      int1 - in
 *		the first byte of the integer representation
 *
 *      value - in
 *		the variable that holds the rest of the representation
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#if defined(UNALIGNED_LOAD) && WORD_SIZE >= 4 && defined(SS_LSB1ST)
#  define STORE_VA_INT4(va, va_len, int1, value) \
     *(FOUR_BYTE_T*)(va) = (FOUR_BYTE_T)ONE_WORD_VALUE(value) << 16 | \
                           TWO_BYTE_VALUE((va_len), (int1));
#elif defined(UNALIGNED_LOAD) && WORD_SIZE >= 4 && !defined(SS_LSB1ST)
#  define STORE_VA_INT4(va, va_len, int1, value) \
     *(FOUR_BYTE_T*)(va) = (FOUR_BYTE_T)TWO_BYTE_VALUE((va_len), (int1)) << 16 | \
                           ONE_WORD_VALUE(value);
#elif defined(UNALIGNED_LOAD) && WORD_SIZE == 2
#  define STORE_VA_INT4(va, va_len, int1, value) { \
        ((TWO_BYTE_T*)(va))[0] = TWO_BYTE_VALUE((va_len), (int1)); \
        ((TWO_BYTE_T*)(va))[1] = ONE_WORD_VALUE(value); \
     }
#else
#  define STORE_VA_INT4(va, va_len, int1, value) { \
        ((ss_byte_t*)(va))[0] = (va_len); \
        ((ss_byte_t*)(va))[1] = (int1); \
        ((ss_byte_t*)(va))[2] = (ss_byte_t)(value >> 8); \
        ((ss_byte_t*)(va))[3] = (ss_byte_t)value; \
     }
#endif


/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		va_setint
 * 
 * Set an integer value to a v-attribute
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to v-attribute to store in
 *
 *	value - in
 *		the integer to store
 *
 * 
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 *                
 *      In some cases where the representation fits into three
 *      bytes, the target must still have four bytes space.
 * 
 * Globals used : 
 */
va_t* va_setint(target_va, value)
	register va_t* target_va;
	register int value;
{
        register va_index_t int_len;

        if (VA_INT_MAX_BYTES - VA_INT_BIAS <= value
              && value <= 0xFF - VA_INT_BIAS - VA_INT_MAX_BYTES) {
            target_va->c[0] = 1;
            target_va->c[1] = (ss_byte_t)(value + VA_INT_BIAS);
            return(target_va);
        } else if (value < 0) {
            value += VA_NEG_BIAS;
            if (value >= -0x100) { /* fits into a single byte? */
                STORE_VA_INT3(target_va, 2, VA_INT_MAX_BYTES - 1, value)
                return(target_va);
            } else { /* doesn't fit into a single byte */
#             if !(INT_BIT > 16) /* int type no longer than 16 bits? */
                  STORE_VA_INT4(target_va, 3, VA_INT_MAX_BYTES - 2, value)
                  SS_NOTUSED(int_len);
#             else /* !(INT_BIT > 16) */

                /* NOTE: the rest of the function is copied in va_setlong */

                /* determine length */
                if (value >= -0x10000) int_len = 2;
                else if (value >= -0x1000000) int_len = 3;
                else
#                 if INT_BIT > 32 /* int type longer than 32 bits? */
                    if (value >= -0x100000000)
#                 endif
                        int_len = 4;
#             if INT_BIT > 32
                else if (value >= -0x10000000000) int_len = 5;
                else if (value >= -0x1000000000000) int_len = 6;
                else if (value >= -0x100000000000000) int_len = 7;
                else int_len = 8;
#             endif /* INT_BIT > 32 */

                /* store length */
                TWO_BYTE_STORE(target_va, int_len + 1, VA_INT_MAX_BYTES - int_len)
#             endif /* !(INT_BIT > 16) */
            }
        } else { /* value > 0 */
            value += VA_POS_BIAS;
            if (value < 0x100) {
                STORE_VA_INT3(target_va, 2, 0x100 - VA_INT_MAX_BYTES, value)
                return(target_va);
            } else {
#             if !(INT_BIT > 16) /* int type no longer than 16 bits? */
                STORE_VA_INT4(target_va, 3, 0x101 - VA_INT_MAX_BYTES, value)
#             else /* !(INT_BIT > 16) */

                /* determine length */
                if (value < 0x10000) int_len = 2;
                else if (value < 0x1000000) int_len = 3;
                else
#                 if INT_BIT > 32 /* int type longer than 32 bits? */
                    if (value < 0x100000000)
#                 endif
                        int_len = 4;
#             if INT_BIT > 32
                else if (value < 0x10000000000) int_len = 5;
                else if (value < 0x1000000000000) int_len = 6;
                else if (value < 0x100000000000000) int_len = 7;
                else int_len = 8;
#             endif /* INT_BIT > 32 */

                /* store length */
                TWO_BYTE_STORE(target_va, int_len + 1, 0xFF - VA_INT_MAX_BYTES + int_len)
#endif /* !(INT_BIT > 16) */
            }
        }
#if INT_BIT > 16

        { /* move the value */
            int mem_value = value;
            ss_byte_t* p_mem_value;
            ss_byte_t* p_va = &(target_va->c[2]);

#           ifdef SS_LSB1ST
              p_mem_value = (ss_byte_t*)&mem_value + int_len - 1;
#           else
              p_mem_value = (ss_byte_t*)&mem_value + sizeof(int) - int_len;
#           endif
            for (; int_len > 0; int_len--) {
                *(p_va++) = *p_mem_value;
#               ifdef SS_LSB1ST
                  p_mem_value--;
#               else
                  p_mem_value++;
#               endif
            }
        }
#endif /* (INT_BIT > 16) */
        return(target_va);
}


/*##**********************************************************************\
 * 
 *		va_setlong
 * 
 * Set a long value to a v-attribute
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to v-attribute to store in
 *
 *	value - in
 *		the long to store
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 *                
 *      In some cases where the representation fits into three
 *      bytes, the target must still have four bytes space.
 * 
 * Globals used : 
 */
va_t* va_setlong(target_va, parvalue)
	register va_t* target_va;
	register long parvalue;
{
#define DB_LONG_BIT 32
        ss_int4_t value = (ss_int4_t)parvalue;
        if ((long)INT_MIN <= value && value <= (long)INT_MAX) {
            /* typecasts above needed for MSC 5.1 */
            return(va_setint(target_va, (int)value));
        } else {
#         if (DB_LONG_BIT > INT_BIT)
            register va_index_t long_len;
            
            /* determine length */
            if (value < 0) {
                value += VA_NEG_BIAS;
#             if !(INT_BIT > 16) /* int type no longer than 16 bits? */
                if (value >= -0x10000) long_len = 2;
                else if (value >= -0x1000000) long_len = 3;
                else
#             endif
#             if DB_LONG_BIT > 32 /* long type longer than 32 bits? */
                    if (value >= -0x100000000)
#             endif
                        long_len = 4;
#             if DB_LONG_BIT > 32
                else if (value >= -0x10000000000) long_len = 5;
                else if (value >= -0x1000000000000) long_len = 6;
                else if (value >= -0x100000000000000) long_len = 7;
                else long_len = 8;
#             endif /* DB_LONG_BIT > 32 */

                /* store length */
                TWO_BYTE_STORE(target_va, long_len + 1, VA_INT_MAX_BYTES - long_len)
            } else { /* value > 0 */
                value += VA_POS_BIAS;
#             if !(INT_BIT > 16) /* int type no longer than 16 bits? */
                if (value < 0x10000) long_len = 2;
                else if (value < 0x1000000) long_len = 3;
                else
#             endif
#             if DB_LONG_BIT > 32 /* long type longer than 32 bits? */
                    if (value < 0x100000000)
#             endif
                        long_len = 4;
#             if DB_LONG_BIT > 32
                else if (value < 0x10000000000) long_len = 5;
                else if (value < 0x1000000000000) long_len = 6;
                else if (value < 0x100000000000000) long_len = 7;
                else long_len = 8;
#             endif /* DB_LONG_BIT > 32 */

                /* store length */
                TWO_BYTE_STORE(target_va, long_len + 1, 0xFF - VA_INT_MAX_BYTES + long_len)
            }
            { /* move the value */
                long mem_value = value;
                ss_byte_t* p_mem_value;
                ss_byte_t* p_va = &(target_va->c[2]);

#               ifdef SS_LSB1ST
                  p_mem_value = (ss_byte_t*)&mem_value + long_len - 1;
#               else
                  p_mem_value = (ss_byte_t*)&mem_value + sizeof(long) - long_len;
#               endif
                for (; long_len > 0; long_len--) {
                    *(p_va++) = *p_mem_value;
#                   ifdef SS_LSB1ST
                      p_mem_value--;
#                   else
                      p_mem_value++;
#                   endif
                }
            }
            return(target_va);
#         else /* (DB_LONG_BIT > INT_BIT) */
            ss_debug(SsPrintf("value=%ld (min=%ld, max=%ld).\n",
                        (long)value, (long)INT_MIN, (long)INT_MAX);)
            ss_error;
            return(NULL);
#         endif /* (DB_LONG_BIT > INT_BIT) */
        }        
}


/*##**********************************************************************\
 * 
 *		va_getint
 * 
 * Read an integer from a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the integer
 * 
 * Limitations  : 
 * 
 *      no overflow checks
 * 
 * Globals used : 
 */
int va_getint(va)
	va_t* va;
{
        register unsigned first_byte;

        /* NOTE: the following six lines are copied in va_getlong */
        ss_assert(!va_testnull(va));
        first_byte = va->c[1];
        if (VA_INT_MAX_BYTES <= first_byte &&
              first_byte <= 0xFF - VA_INT_MAX_BYTES) {
            ss_dassert(va->c[0] == 1);
            return(SIGN_EXTEND_TO_INT(first_byte - VA_INT_BIAS));
        } else {
#         if !(INT_BIT > 16) /* int type no longer than 16 bits? */
            switch (first_byte) {
                case (0x100 - VA_INT_MAX_BYTES): /* one byte, positive */
                    ss_dassert(va->c[0] == 2);
                    return(va->c[2] - VA_POS_BIAS);
                case (VA_INT_MAX_BYTES - 1): /* one byte, negative */
                    ss_dassert(va->c[0] == 2);
                    return(va->c[2] - 0x100 - VA_NEG_BIAS);
                case (0x101 - VA_INT_MAX_BYTES): /* two bytes, positive */
                    ss_dassert(va->c[0] == 3);
                    ss_dassert(ONE_WORD_LOAD(&va->c[2]) <= INT_MAX + VA_POS_BIAS);
                    return(ONE_WORD_LOAD(&va->c[2]) - VA_POS_BIAS);
                default: /* two bytes, negative */
                    ss_dassert(first_byte == VA_INT_MAX_BYTES - 2);
                    ss_dassert(va->c[0] == 3);
                    ss_dassert(ONE_WORD_LOAD(&va->c[2]) >= INT_MIN + VA_NEG_BIAS);
                    return(ONE_WORD_LOAD(&va->c[2]) - VA_NEG_BIAS);
            }
#         else
            /* NOTE: the rest of the function is copied in va_getlong */
            ss_byte_t* ptr;
            register int result;

            if (first_byte < VA_INT_MAX_BYTES) { /* negative */
                /* calculate length into first_byte */
                first_byte = VA_INT_MAX_BYTES - first_byte;
                ss_dassert(first_byte <= sizeof(int));
                ss_dassert((unsigned)va->c[0] == first_byte + 1);
                /* get bytes */
                result = -1;
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                ss_dassert((unsigned int)result >= (unsigned int)(INT_MIN + VA_NEG_BIAS));
                return(result - VA_NEG_BIAS);
            } else { /* positive, calculate length into first_byte */
                first_byte -= 0xFF - VA_INT_MAX_BYTES;
                ss_dassert(first_byte <= sizeof(int));
                ss_dassert((unsigned)va->c[0] == first_byte + 1);
                result = 0;
                /* get bytes */
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                ss_dassert((unsigned int)result <= INT_MAX + VA_POS_BIAS);
                return(result - VA_POS_BIAS);
            }
#         endif
        }
}


/*##**********************************************************************\
 * 
 *		va_getlong
 * 
 * Read a long from a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the long
 * 
 * Limitations  : 
 * 
 *      no overflow checks
 * 
 * Globals used : 
 */
long va_getlong(va)
	va_t* va;
{
        register unsigned first_byte;

        /* NOTE: this function is basically a copy of va_getint */
        ss_assert(!va_testnull(va));
        first_byte = va->c[1];
        if (VA_INT_MAX_BYTES <= first_byte &&
              first_byte <= 0xFF - VA_INT_MAX_BYTES) {
            ss_dassert(va->c[0] == 1);
            return(SIGN_EXTEND_TO_LONG(first_byte - VA_INT_BIAS));
        } else {
            ss_byte_t* ptr;
            register long result;

            if (first_byte < VA_INT_MAX_BYTES) { /* negative */
                /* calculate length into first_byte */
                first_byte = VA_INT_MAX_BYTES - first_byte;
                ss_dassert(first_byte <= sizeof(long));
                ss_dassert((unsigned)va->c[0] == first_byte + 1);
                result = -1;
                /* get bytes */
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                ss_dassert((unsigned long)result >= (unsigned long)(LONG_MIN + VA_NEG_BIAS));
                return(result - VA_NEG_BIAS);
            } else { /* positive, calculate length into first_byte */
                first_byte -= 0xFF - VA_INT_MAX_BYTES;
                ss_dassert(first_byte <= sizeof(long));
                ss_dassert((unsigned)va->c[0] == first_byte + 1);
                result = 0;
                /* get bytes */
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                ss_dassert((unsigned long)result <= LONG_MAX + VA_POS_BIAS);
                return(result - VA_POS_BIAS);
            }
        }
}

/*##**********************************************************************\
 * 
 *		va_getlong_check
 * 
 * Read a long from a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the long
 * 
 * Limitations  : 
 * 
 *      no overflow checks
 * 
 * Globals used : 
 */
long va_getlong_check(va)
	va_t* va;
{
        register unsigned first_byte;

        /* NOTE: this function is a copy of va_getlong */
        if (va_testnull(va)) {
            return(0L);
        }
        first_byte = va->c[1];
        if (VA_INT_MAX_BYTES <= first_byte &&
              first_byte <= 0xFF - VA_INT_MAX_BYTES) {
            if (!(va->c[0] == 1)) {
                return(0L);
            }
            return(SIGN_EXTEND_TO_LONG(first_byte - VA_INT_BIAS));
        } else {
            ss_byte_t* ptr;
            register long result;

            if (first_byte < VA_INT_MAX_BYTES) { /* negative */
                /* calculate length into first_byte */
                first_byte = VA_INT_MAX_BYTES - first_byte;
                if (!(first_byte <= sizeof(long))) {
                    return(0L);
                }
                if (!((unsigned)va->c[0] == first_byte + 1)) {
                    return(0L);
                }
                result = -1;
                /* get bytes */
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                if (!((unsigned long)result >= (unsigned long)(LONG_MIN + VA_NEG_BIAS))) {
                    return(0L);
                }
                return(result - VA_NEG_BIAS);
            } else { /* positive, calculate length into first_byte */
                first_byte -= 0xFF - VA_INT_MAX_BYTES;
                if (!(first_byte <= sizeof(long))) {
                    return(0L);
                }
                if (!((unsigned)va->c[0] == first_byte + 1)) {
                    return(0L);
                }
                result = 0;
                /* get bytes */
                for (ptr = &va->c[2]; first_byte > 0; first_byte--) {
                    result = result << 8 | *ptr++;
                }
                if (!((unsigned long)result <= LONG_MAX + VA_POS_BIAS)) {
                    return(0L);
                }
                return(result - VA_POS_BIAS);
            }
        }
}
