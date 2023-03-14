/*************************************************************************\
**  source       * uti0vaf.c
**  directory    * uti
**  description  * 
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


/* include files *******************************************/

#include <ssc.h>
#include <ssdebug.h>
#include <sslimits.h>
#include <ssfloat.h>
#include "uti0va.h"


/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		va_setfloat
 * 
 * Set a float to a v-attribute.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to the v-attribute
 *
 *	value - in
 *		the float
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#ifdef NO_ANSI
va_t* va_setfloat(target_va, value_arg)
	register va_t* target_va;
        double value_arg;
#elif defined(NO_ANSI_FLOAT)
va_t* va_setfloat(register va_t* target_va, double value_arg)
#else
va_t* va_setfloat(register va_t* target_va, float value)
#endif
{
#ifdef NO_ANSI_FLOAT
        float value = (float)value_arg;
        /* NOTE: required in SCO MSC, where a typecast doesn't do a conversion! */
#endif

        /* NOTE: va_setdouble contains almost exactly the same code */
	ss_byte_t *const c = target_va->c;
	ss_byte_t *const val = (void*)&value;

        c[0] = 4;
        if (value >= (float)0.0) {
            /* positive, complement sign bit */
#           if defined(SS_LSB1ST)
                c[1] = val[3] ^ 0x80;
                c[2] = val[2];
                c[3] = val[1];
                c[4] = val[0];
#           else
#               ifdef UNALIGNED_LOAD
                    *(FOUR_BYTE_T*)(void*)(&c[1]) =
                        ((FOUR_BYTE_T*)(void*)&value)[0] ^ 0x80000000;
                    /* These complicated typecasts are necessary: (FOUR_BYTE_T)value
                       would do an arithmetic format conversion. */
#               else
                    c[1] = val[0] ^ 0x80;
                    c[2] = val[1];
                    c[3] = val[2];
                    c[4] = val[3];
#               endif
#           endif
        } else {
            /* negative, complement the whole thing */
#           if defined(SS_LSB1ST)
                c[1] = ~val[3];
                c[2] = ~val[2];
                c[3] = ~val[1];
                c[4] = ~val[0];
#           else
#               ifdef UNALIGNED_LOAD
                    *(FOUR_BYTE_T*)(void*)(&c[1]) =
                        ~((FOUR_BYTE_T*)(void*)&value)[0];
#               else
                    c[1] = ~val[0];
                    c[2] = ~val[1];
                    c[3] = ~val[2];
                    c[4] = ~val[3];
#               endif
#           endif
        }
        return(target_va);
}


/*##**********************************************************************\
 * 
 *		va_setdouble
 * 
 * Set a double to a v-attribute.
 * 
 * Parameters : 
 * 
 *	target_va - out, use
 *		pointer to the v-attribute
 *
 *	value - in
 *		the double
 *
 * Return value - ref : 
 * 
 *      target_va
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
va_t* va_setdouble(target_va, value)
	register va_t* target_va;
	double value;
{
        /* NOTE: va_setfloat is a subset of this code */
	ss_byte_t *const c = target_va->c;
	ss_byte_t *const val = (void*)&value;

        c[0] = 8;
	/* printf ("VAs %e\n", value); */
        if (value >= 0.0) {
            /* positive, complement sign bit */
#           if defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_setdouble positive\n"));

                c[1] = val[3] ^ 0x80;
                c[2] = val[2];
                c[3] = val[1];
                c[4] = val[0];
                c[5] = val[7];
                c[6] = val[6];
                c[7] = val[5];
                c[8] = val[4];

#           elif defined(SS_LSB1ST) && !defined(SS_CROSSENDIAN)
                c[1] = val[7] ^ 0x80;
                c[2] = val[6];
                c[3] = val[5];
                c[4] = val[4];
                c[5] = val[3];
                c[6] = val[2];
                c[7] = val[1];
                c[8] = val[0];
#           else
#               ifdef UNALIGNED_LOAD
                    *(FOUR_BYTE_T*)(void*)(&c[1]) =
                        ((FOUR_BYTE_T*)(void*)&value)[0] ^ 0x80000000;
                    *(FOUR_BYTE_T*)(void*)(&c[5]) =
                        ((FOUR_BYTE_T*)(void*)&value)[1];
                    /* These complicated typecasts are necessary: (FOUR_BYTE_T)value
                    would do an arithmetic format conversion. */
#               else
                    c[1] = val[0] ^ 0x80;
                    c[2] = val[1];
                    c[3] = val[2];
                    c[4] = val[3];
                    c[5] = val[4];
                    c[6] = val[5];
                    c[7] = val[6];
                    c[8] = val[7];
#               endif                                            
#           endif
        } else {
            /* negative, complement the whole thing */
#           if defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_setdouble negative\n"));

                c[1] = ~val[3];
                c[2] = ~val[2];
                c[3] = ~val[1];
                c[4] = ~val[0];
                c[5] = ~val[7];
                c[6] = ~val[6];
                c[7] = ~val[5];
                c[8] = ~val[4];

#           elif defined(SS_LSB1ST) && !defined(SS_CROSSENDIAN)

                c[1] = ~val[7];
                c[2] = ~val[6];
                c[3] = ~val[5];
                c[4] = ~val[4];
                c[5] = ~val[3];
                c[6] = ~val[2];
                c[7] = ~val[1];
                c[8] = ~val[0];
#           else
#               ifdef UNALIGNED_LOAD
                    *(FOUR_BYTE_T*)(void*)(&c[1]) =
                        ~((FOUR_BYTE_T*)(void*)&value)[0];
                    *(FOUR_BYTE_T*)(void*)(&c[5]) =
                        ~((FOUR_BYTE_T*)(void*)&value)[1];
#               else
                    c[1] = ~val[0];
                    c[2] = ~val[1];
                    c[3] = ~val[2];
                    c[4] = ~val[3];
                    c[5] = ~val[4];
                    c[6] = ~val[5];
                    c[7] = ~val[6];
                    c[8] = ~val[7];
#               endif
#           endif
        }
        return(target_va);
}


/*##**********************************************************************\
 * 
 *		va_getfloat
 * 
 * Get a float from a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the float
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
float va_getfloat(va)
        register va_t* va;
{
        /* NOTE: va_getdouble contains almost exactly the same code */

	union {
            float result;
	    FOUR_BYTE_T a4b;     /* as one four-byte value */
	} r;

	ss_byte_t *const c = va->c;

        ss_assert(c[0] == 4);
        if (c[1] & 0x80) { /* positive, complement sign bit */
#           if defined(UNALIGNED_LOAD) && !defined(SS_LSB1ST)
                r.a4b = (*(FOUR_BYTE_T*)(void*)(&c[1]) ^ 0x80000000);
                /* These complicated typecasts are necessary: (float)value
                   would do an arithmetic format conversion. */
#           else
#           if defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_getfloat positive\n"));
#           endif /* SS_CROSSENDIAN */
                r.a4b =  (((FOUR_BYTE_T)(c[1] ^ (ss_byte_t)0x80) << 8 |
                      c[2]) << 8 | c[3]) << 8 | c[4];
#           endif
        } else { /* negative, complement the whole thing */
#           if defined(UNALIGNED_LOAD) && !defined(SS_LSB1ST)
                r.a4b = ~*(FOUR_BYTE_T*)(void*)(&c[1]);
#           else
#           if defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_getfloat negative\n"));
#           endif /* SS_CROSSENDIAN */
                r.a4b = ~((((FOUR_BYTE_T)c[1] << 8 | c[2]) << 8 |
                       c[3]) << 8 | c[4]);
#           endif
        }
        return(r.result);
}


/*##**********************************************************************\
 * 
 *		va_getdouble
 * 
 * Get a double from a v-attribute.
 * 
 * Parameters : 
 * 
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value : 
 * 
 *      the double
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
double va_getdouble(va)
        register va_t* va;
{
        /* NOTE: va_getfloat is almost a subset of this code */

	union {
	    double result;
	    ss_byte_t   a1b[8];  /* as an array of 8 one-byte values */
	    FOUR_BYTE_T a4b[2];  /* as an array of 2 four-byte values */
	} r;

	ss_byte_t *const c=va->c;
        ss_assert(c[0] == 8);
        if (c[1] & 0x80) { /* positive, complement sign bit */
#           if defined(SS_LSB1ST) && !defined(SS_CROSSENDIAN)

                r.a4b[1] = FOUR_BYTE_VALUE(c[4], c[3],
                                           c[2], c[1] ^ (ss_byte_t)0x80);
                r.a4b[0] = FOUR_BYTE_VALUE(c[8], c[7], c[6], c[5]);
#           elif defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_getdouble positive\n"));

                r.a1b[0] = c[4];
                r.a1b[1] = c[3];
                r.a1b[2] = c[2];
                r.a1b[3] = c[1] ^ 0x80;
                r.a1b[4] = c[8];
                r.a1b[5] = c[7];
                r.a1b[6] = c[6];
                r.a1b[7] = c[5];
                ss_pprintf_1(("E32 va_getdouble positive result: "
                         "%02X %02X %02X %02X %02X %02X %02X %02X\n", 
                         r.a1b[7], r.a1b[6], r.a1b[5], r.a1b[4],
                         r.a1b[3], r.a1b[2], r.a1b[1], r.a1b[0]));

#           else
#               ifdef UNALIGNED_LOAD
                    r.a4b[0] = *(FOUR_BYTE_T*)(void*)(&c[1]) ^ 0x80000000;
                    r.a4b[1] = *(FOUR_BYTE_T*)(void*)(&c[5]);
                    /* These complicated typecasts are necessary: (double)value
                    would do an arithmetic format conversion. */
#               else
                    r.a4b[0] = FOUR_BYTE_VALUE(c[1] ^ (ss_byte_t)0x80, c[2],
                                               c[3], c[4]);
                    r.a4b[1] = FOUR_BYTE_VALUE(c[5], c[6], c[7], c[8]);
#               endif
#           endif
        } else { /* negative, complement the whole thing */
#           if defined(SS_LSB1ST) && !defined(SS_CROSSENDIAN)
                r.a4b[1] = ~FOUR_BYTE_VALUE(c[4], c[3], c[2], c[1]);
                r.a4b[0] = ~FOUR_BYTE_VALUE(c[8], c[7], c[6], c[5]);
#           elif defined(SS_CROSSENDIAN)
                ss_pprintf_1(("E32 va_getdouble negative\n"));

                r.a1b[0] = ~(c[4]);
                r.a1b[1] = ~(c[3]);
                r.a1b[2] = ~(c[2]);
                r.a1b[3] = ~(c[1]);
                r.a1b[4] = ~(c[8]);
                r.a1b[5] = ~(c[7]);
                r.a1b[6] = ~(c[6]);
                r.a1b[7] = ~(c[5]);
                ss_pprintf_1(("E32 va_getdouble negative result: "
                         "%02X %02X %02X %02X %02X %02X %02X %02X\n", 
		         r.a1b[7], r.a1b[6], r.a1b[5], r.a1b[4],
                         r.a1b[3], r.a1b[2], r.a1b[1], r.a1b[0]));
#           else
#               ifdef UNALIGNED_LOAD
                    r.a4b[0] = ~*(FOUR_BYTE_T*)(&c[1]);
                    r.a4b[1] = ~*(FOUR_BYTE_T*)(&c[5]);
#               else
                    r.a4b[0] = ~FOUR_BYTE_VALUE(c[1], c[2], c[3], c[4]);
                    r.a4b[1] = ~FOUR_BYTE_VALUE(c[5], c[6], c[7], c[8]);
#               endif
#           endif
        }
        return(r.result);
}


