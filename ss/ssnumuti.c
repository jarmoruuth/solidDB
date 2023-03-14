/*************************************************************************\
**  source       * ssnumuti.c
**  directory    * ss
**  description  * Numeric utilities for tests
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

Random numnber and other numeric utilities.

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
 

#include "sstraph.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "sslimits.h"
#include "ssfloat.h"
#include "ssc.h"

#include "ssdebug.h"
#include "ssnumuti.h"

ss_define_float_constants


/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		ss_long_rand
 * 
 * Generates a random number in the range -LONG_MAX...LONG_MAX, evenly
 * distributed, provided rand() is.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 *      the random number
 * 
 * Limitations  : 
 * 
 *      doesn't work if RAND_MAX >= ULONG_MAX
 * 
 * Globals used : 
 * 
 *      the random seed
 */
long ss_long_rand()
{
        unsigned long r = 0; /* the random number */
        unsigned long range = 1; /* the range of r */
        unsigned long ri;
#define RAND_SCALE ((unsigned long)RAND_MAX + 1)
#define RANGE_LIMIT (((unsigned long)LONG_MAX + 1) / RAND_SCALE)

        do {
            do { /* generate a new candidate */
                r = r * RAND_SCALE + rand();
                range *= RAND_SCALE;
            /* loop until the next time would overflow the long range */
            } while (range < RANGE_LIMIT);
            r /= range / RANGE_LIMIT; /* scale to the right range */
            ri = (unsigned long)rand();
        /* if the candidate wouldn't be <= LONG_MAX, get a new one */
        } while (r > (LONG_MAX - ri) / RAND_SCALE);
        r = r * RAND_SCALE + ri;
        if (rand() < (long)(RAND_SCALE / 2)) {
            return((long)r);
        } else {
            return(-(long)r);
        }
}


/*##**********************************************************************\
 * 
 *		ss_print_float
 * 
 * Print the internals of a float in +mmmmmm^ee format:
 * sign, 6 hex digits of mantissa, '^', and 2 hex digits of exponent.
 * 
 * Parameters : 
 * 
 *	f - in
 *		a float
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 *      For IEEE-format floats only
 * 
 * Globals used : 
 */
#ifdef NO_ANSI
void ss_print_float(d)
        double d;
#elif defined(NO_ANSI_FLOAT)
void ss_print_float(double d)
#else
void ss_print_float(float f)
#endif
{
#ifdef NO_ANSI_FLOAT
        float f = (float)d;
#endif

        if (SS_RETYPE(ss_float_t, f).sign_bit) {
            SsPrintf("-");
        } else {
            SsPrintf("+");
        }
        SsPrintf("%02x%04x^%02x",
               SS_RETYPE(ss_float_t, f).mantissa_22_16,
               SS_RETYPE(ss_float_t, f).mantissa_15_0,
               SS_RETYPE(ss_float_t, f).exponent);
}


/*##**********************************************************************\
 * 
 *		ss_print_double
 * 
 * Print the internals of a double in +mmmmmmmmmmmmm^eee format:
 * sign, 13 hex digits of mantissa, '^', and 3 hex digits of exponent.
 * 
 * Parameters : 
 * 
 *	f - in
 *		a double
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 *      For IEEE-format floats only
 * 
 * Globals used : 
 */
void ss_print_double(f)
	double f;
{
        if (SS_RETYPE(ss_double_t, f).sign_bit) {
            SsPrintf("-");
        } else {
            SsPrintf("+");
        }
        SsPrintf("%x%04x%04x%04x^%03x",
               SS_RETYPE(ss_double_t, f).mantissa_51_48, SS_RETYPE(ss_double_t, f).mantissa_47_32,
               SS_RETYPE(ss_double_t, f).mantissa_31_16, SS_RETYPE(ss_double_t, f).mantissa_15_0,
               SS_RETYPE(ss_double_t, f).exponent);
}


/*##**********************************************************************\
 * 
 *		ss_float_rand
 * 
 * Generates a random float; every legal float will occur with equal
 * frequency, provided rand() is evenly distributed.
 * 
 * Parameters :
 * 
 * Return value : a random float
 * 
 * 
 * 
 * Limitations  : 
 *                
 *      For IEEE-format floats only, assumes denormals are either
 *      allowed or disallowed.  Uses long_rand.
 * 
 * Globals used : 
 * 
 *      the random seed
 */
float ss_float_rand()
{
        bool is_portable;
        union {
                float f;
                ss_uint4_t i4;
        } result;

        do {
            SS_TRAP_HANDLERSECTION
                case SS_TRAP_FPE_ANY:
                default:
                    is_portable = FALSE;
                    break;
            SS_TRAP_RUNSECTION
                result.i4 = (ss_uint4_t)ss_long_rand();
                if (result.f == -0.0) {
                    result.f = 0.0;
                }
                is_portable = SS_FLOAT_IS_PORTABLE(result.f);
            SS_TRAP_END
        } while (!is_portable);
        return(result.f);
}


/*##**********************************************************************\
 * 
 *		ss_double_rand
 * 
 * Generates a random double; every legal double will occur with equal
 * frequency, provided rand() is evenly distributed.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 *      a random double
 * 
 * Limitations  : 
 *                
 *      For IEEE-format floats only, assumes denormals are either
 *      allowed or disallowed.  Uses long_rand.
 * 
 * Globals used : 
 * 
 *      the random seed
 */
double ss_double_rand()
{
        bool is_portable;
        union {
                double d;
                ss_uint4_t i4[2];
        } result;

        do {
            SS_TRAP_HANDLERSECTION
                case SS_TRAP_FPE_ANY:
                default:
                    is_portable = FALSE;
                    break;
            SS_TRAP_RUNSECTION
                result.i4[0] = (ss_uint4_t)ss_long_rand();
                result.i4[1] = (ss_uint4_t)ss_long_rand();
                if (result.d == -0.0) {
                    result.d = 0.0;
                }
                is_portable = SS_DOUBLE_IS_PORTABLE(result.d);
            SS_TRAP_END
        } while (!is_portable);
        return(result.d);
}


/*##**********************************************************************\
 * 
 *		ss_sign
 * 
 * Returns the sign of the argument.
 * 
 * Parameters : 
 * 
 *	i - in
 *		an integer
 *
 * Return value : 
 *                
 *      -1, if i < 0               
 *       0, if i = 0
 *      +1, if i > 0
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int ss_sign(i)
        int i;
{
        return((i < 0) ? -1 : (i == 0) ? 0 : 1);
}


/*##**********************************************************************\
 * 
 *		ss_double_near
 * 
 * Determines whether two floats are equal to within the specified number
 * of bits
 * 
 * Parameters : 
 * 
 *	d1, d2	- in
 *          the float
 * 
 *	nbits - in
 *		number of unequal bits allowed, must be < 16
 *
 * Return value : 
 * 
 *      boolean
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool ss_double_near(d1, d2, nbits)
	double d1;
	double d2;
	int nbits;
{
        ss_double_t sd1, sd2;
        long diff; /* assuming long >= 32 bits */
        int ediff;

        sd1 = SS_RETYPE(ss_double_t, d1);
        sd2 = SS_RETYPE(ss_double_t, d2);
        if (sd1.sign_bit != sd2.sign_bit) return(FALSE);
        ediff = sd1.exponent - sd2.exponent;
        if (ediff > 1 || ediff < -1) return(FALSE);
        if (ediff != 0 && sd1.exponent != 0 && sd2.exponent != 0) {
            /* different exponents, neither denormalized */
            if (ediff == 1) { /* swap */
                sd1 = SS_RETYPE(ss_double_t, d2);
                sd2 = SS_RETYPE(ss_double_t, d1);
            }
            /* Now we know sd1.exponent == sd2.exponent + 1, and compensate */
            diff = ((long)(0x20 - (sd2.exponent ? 0x10 : 0) /* hidden bits */
                           + (sd1.mantissa_51_48 << 1) - sd2.mantissa_51_48)
                    << 16)
                   + ((long)sd1.mantissa_47_32 << 1) - sd2.mantissa_47_32;
            /* Check that diff doesn't overflow */
            if (diff > 1) return(FALSE);
            diff = (diff << 16) + ((long)sd1.mantissa_31_16 << 1) - sd2.mantissa_31_16;
            if (diff > 1) return(FALSE);
            return((((diff << 16) + ((long)sd1.mantissa_15_0 << 1) - sd2.mantissa_15_0)
                     >> nbits) == 0);
        } else {
            /* equal exponents, or 1 & 0 */
            diff = (((long)(sd1.exponent ? 0x10 : 0) - (sd2.exponent ? 0x10 : 0)
                            + sd1.mantissa_51_48 - sd2.mantissa_51_48)
                    << 16)
                   + sd1.mantissa_47_32 - sd2.mantissa_47_32;
            if (diff > 1 || diff < -1) return(FALSE);
            diff = (diff << 16) + sd1.mantissa_31_16 - sd2.mantissa_31_16;
            if (diff > 1 || diff < -1) return(FALSE);
            return((labs((diff << 16) + sd1.mantissa_15_0 - sd2.mantissa_15_0)
                    >> nbits)
                   == 0);
        }
}
