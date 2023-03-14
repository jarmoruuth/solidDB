/*************************************************************************\
**  source       * dt0dfloa.c
**  directory    * dt
**  description  * Decimal float implementation
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


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssfloat.h>
#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssdtoa.h>
#include <ssstring.h>
#include <sslimits.h>
#include <ssscan.h>

#include <uti0va.h>

#include "dt1dfl.h"
#include "dt0dfloa.h"


#define DFL_NOT_INITIALIZED '\22'

#define DFL_PRINTBUFSIZE 40

#define dfl_initvalue  { DFL_NOT_INITIALIZED } 

static dt_dfl_t dfl_int_max = dfl_initvalue;
static dt_dfl_t dfl_int_min = dfl_initvalue;

static dt_dfl_t dfl_long_max = dfl_initvalue;
static dt_dfl_t dfl_long_min = dfl_initvalue;

#ifndef SS_MYSQL
static dt_dfl_t dfl_double_max = dfl_initvalue;
static dt_dfl_t dfl_double_min = dfl_initvalue;
#endif

static bool dfl_initialized(dt_dfl_t* p_dfl);

static bool dfl_initialized(dt_dfl_t* p_dfl)
{
        char* ptr = (char *)p_dfl;
        return(ptr[0] != DFL_NOT_INITIALIZED);
}

/************ Calculation and comparison **************/

/*##**********************************************************************\
 * 
 *          dt_dfl_sum
 * 
 * Calculates the sum of two dfloats
 * 
 * Parameters : 
 * 
 *      p_res_dfl - out, use
 *          Pointer to dfloat variable where the result is to be stored 
 *          
 *      dfl1 - in, use             
 *          dfloat 1    
 *          
 *      dfl2 - in, use
 *          dfloat 2    
 *          
 * Return value : 
 *       
 *      TRUE, succeeded. *p_res_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_sum(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2)
{
        ss_dassert(p_res_dfl != NULL);
        *p_res_dfl = dfl_sum(dfl1, dfl2);
        if (dfl_overflow(*p_res_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_diff
 * 
 * Calculates the subtraction of two dfloats
 * 
 * Parameters : 
 * 
 *      p_res_dfl - out, use
 *          Pointer to dfloat variable where the result is to be stored 
 *          
 *      dfl1 - in, use             
 *          dfloat 1    
 *          
 *      dfl2 - in, use
 *          dfloat 2    
 *          
 * Return value : 
 *       
 *      TRUE, succeeded. *p_res_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_diff(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2)
{
        ss_dassert(p_res_dfl != NULL);
        *p_res_dfl = dfl_diff(dfl1, dfl2);
        if (dfl_overflow(*p_res_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_prod
 * 
 * Calculates the product of two dfloats
 * 
 * Parameters : 
 * 
 *      p_res_dfl - out, use
 *          Pointer to dfloat variable where the result is to be stored 
 *          
 *      dfl1 - in, use             
 *          dfloat 1    
 *          
 *      dfl2 - in, use
 *          dfloat 2    
 *          
 * Return value : 
 *       
 *      TRUE, succeeded. *p_res_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_prod(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2)
{
        ss_dassert(p_res_dfl != NULL);
        *p_res_dfl = dfl_prod(dfl1, dfl2);
        if (dfl_overflow(*p_res_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_quot
 * 
 * Calculates the division of two dfloats
 * 
 * Parameters : 
 * 
 *      p_res_dfl - out, use
 *          Pointer to dfloat variable where the result is to be stored 
 *          
 *      dfl1 - in, use             
 *          dfloat 1    
 *          
 *      dfl2 - in, use
 *          dfloat 2    
 *          
 * Return value : 
 *       
 *      TRUE, succeeded. *p_res_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_quot(dt_dfl_t* p_res_dfl, dt_dfl_t dfl1, dt_dfl_t dfl2)
{
        ss_dassert(p_res_dfl != NULL);
        *p_res_dfl = dfl_quot(dfl1, dfl2);
        if (dfl_overflow(*p_res_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_overflow
 * 
 * Checks if a dfloat variable represents the overflow/not-a-number value
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          Dfloat variable 
 *          
 * Return value : 
 * 
 *      TRUE, if overflow
 *      FALSE, if not.
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_overflow(dt_dfl_t dfl)
{
        return((bool)(dfl_overflow(dfl) == 1));
}

/*##**********************************************************************\
 * 
 *          dt_dfl_underflow
 * 
 * Checks if a dfloat variable represents the underflow/not-a-number value.
 * Underflow can sometimes be detected during calculations when the absolute
 * value of negative exponent grows too big.
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          Dfloat variable 
 *          
 * Return value : 
 * 
 *      TRUE, if underflow is detected
 *      FALSE, if not.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_underflow(dt_dfl_t dfl)
{
        SS_NOTUSED(dfl);
        return(FALSE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_compare
 * 
 * Compares too dfloat variables.
 * 
 * Parameters : 
 * 
 *      dfl1 - in, use
 *          dfloat 1    
 *          
 *      dfl2 - in, use
 *          dfloat 2    
 *          
 * Return value : 
 * 
 *      -1, if dfl1 < dfl2
 *       0, if dfl1 == dfl2
 *       1, if dfl1 > dfl2
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int dt_dfl_compare(dt_dfl_t dfl1, dt_dfl_t dfl2)
{
        return(dfl_compare(dfl1, dfl2));
}


/************ Conversions to dfloat, address of destination dfl used ! *****/


/*##**********************************************************************\
 * 
 *          dt_dfl_setint
 * 
 * Set an integer value to a dfl
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      i - in, use
 *          the integer to store
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setint(dt_dfl_t* p_dest_dfl, int i)
{
        ss_dassert(p_dest_dfl != NULL);
        *p_dest_dfl = dfl_inttodfl(i);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setinte
 * 
 * Set an integer value to a dfl. Value to use is (i * 10 ^ exp)
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      i - in, use
 *          the integer to store
 *          
 *      exp - in, use
 *          the exponent of 10 to multiply i
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setinte(dt_dfl_t* p_dest_dfl, int i, int exp)
{
        ss_dassert(p_dest_dfl != NULL);
        *p_dest_dfl = dfl_intetodfl(i, exp);
        if (dfl_overflow(*p_dest_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setlong
 * 
 * Set a long value to a dfl
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      l - in, use
 *          the long value to store
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setlong(dt_dfl_t* p_dest_dfl, long l)
{
        ss_dassert(p_dest_dfl != NULL);
        *p_dest_dfl = dfl_longtodfl(l);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setlonge
 * 
 * Set a long value to a dfl. Value to use is (l * 10 ^ exp)
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      l - in, use
 *          the long value to store
 *          
 *      exp - in, use
 *          the exponent of 10 to multiply i
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setlonge(dt_dfl_t* p_dest_dfl, long l, int exp)
{
        ss_dassert(p_dest_dfl != NULL);
        *p_dest_dfl = dfl_longetodfl(l, exp);
        if (dfl_overflow(*p_dest_dfl)) {
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setdouble
 * 
 * Sets a double value to dfl
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      d - in, use
 *          Double value to store
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setdouble(dt_dfl_t* p_dest_dfl, double d)
{
        char buf[DFL_PRINTBUFSIZE];
        ss_dassert(p_dest_dfl != NULL);
        SsDoubleToAscii(d, buf, 16);
        return(dt_dfl_setasciiz(p_dest_dfl, buf));
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setasciiz
 * 
 * Sets an asciiz string to dfl
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      extdfl - in, use
 *          Asciiz string containing a sequence of digits with an       
 *          optional decimal point and sign.
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 *                     Probably string was not recognized as a legal dfl.
 * Limitations  : 
 * 
 *      Might be so that should return FALSE if there is some
 *      garbage immediately following the numeric value ("1.234somechars")
 *      RES-level may assume so ??
 * 
 * Globals used : 
 */
bool dt_dfl_setasciiz(dt_dfl_t* p_dest_dfl, char* extdfl)
{
        ss_dassert(p_dest_dfl != NULL);
        ss_purify(memset(p_dest_dfl, '\0', sizeof(*p_dest_dfl)));
        return((bool)(dfl_asciiztodfl(extdfl, p_dest_dfl) == 1));
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setdflprecision
 * 
 * Sets a dfl to another dfl using given precision (length and scale)
 * for the result.
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in.     
 *          
 *      src_dfl - in, use
 *          Dfl value to use    
 *          
 *      len - in, use
 *          Number of significant decimal digits    
 *          
 *      scale - in, use
 *          Number of digits after decimal point.
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. *p_dest_dfl can be further used for calculations
 *      FALSE, failed. Check for overflow, underflow, not-a-number etc.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setdflprecision(
        dt_dfl_t* p_dest_dfl,
        dt_dfl_t  src_dfl,
        int       len,
        int       scale
) {
        char  buf[DFL_PRINTBUFSIZE];
        char* ptr;
        int   n_special_chars = 0;
        int   n_dec;

        /* Set the correct number of digits after decimal point, scale */
        ss_dassert(scale >= 0);
        *p_dest_dfl = dfl_rounded(src_dfl, -scale);


        /* Check that the total number of digits does not exceed the len */
        if (!dt_dfl_dfltoasciiz_maxlen(*p_dest_dfl, buf, DFL_PRINTBUFSIZE)) {
            *p_dest_dfl = dfl_nan;
            return(FALSE);
        }

        ptr = strstr(buf, "-");
        if (ptr != NULL) {
            n_special_chars++;
        }
        ptr = strstr(buf, ".");
        if (ptr != NULL) {
            n_special_chars++;
        }
        
        n_dec = strlen(buf) - n_special_chars;

        if (n_dec > len) {
            /* Number does not fit into required len, set overflow */
            *p_dest_dfl = dfl_nan;
            return(FALSE);
        } else {
            return(TRUE);
        }
}

/*##**********************************************************************\
 * 
 *          dt_dfl_change_sign
 * 
 * Changes the sign of a dfl number
 * 
 * Parameters : 
 * 
 *      p_dfl - out, use
 *          Pointer to dfl.     
 *          
 * Return value : 
 * 
 * Limitations  : 
 * 
 * 
 * Globals used : 
 */
void dt_dfl_change_sign(dt_dfl_t* p_dfl)
{
        ss_dassert(p_dfl != NULL);
        dfl_change_sign(p_dfl);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_setva
 * 
 * Sets a v-attribute value as a dfl value.
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          Pointer to dfl to store in  
 *          
 *      va - in, use
 *          v-attribute that contains the source value
 *          
 * Return value : 
 * 
 *      TRUE, succeeded. 
 *      FALSE, failed.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_setva(dt_dfl_t* p_dest_dfl, va_t* va)
{
        ss_dassert(p_dest_dfl != NULL);
        dfl_nsttodfl((char *)va, p_dest_dfl);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_round
 * 
 * Assigns a rounded value of src_dfl to *p_dest_dfl rounding
 * position is indicated with parameter scale.
 * eg. round 11.51 1 -> 11.5
 *     round 11.51 0 -> 12
 *     round 11.51 -1 -> 10
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          pointer to destination dfl
 *          
 *      src_dfl - in
 *          source dfl
 *          
 *      scale - in
 *          rounding positions to right of decimal point
 *          
 * Return value :
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_dfl_round(dt_dfl_t* p_dest_dfl, dt_dfl_t src_dfl, int scale)
{
        *p_dest_dfl = dfl_rounded(src_dfl, -scale);
        if (dfl_overflow(*p_dest_dfl)) {
            return (FALSE);
        }
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_truncate
 * 
 * Assigns a truncateed value of src_dfl to *p_dest_dfl truncateing
 * position is indicated with parameter scale.
 * eg. truncate 11.51 1 -> 11.5
 *     truncate 11.51 0 -> 11
 *     truncate 11.51 -1 -> 10
 * 
 * Parameters : 
 * 
 *      p_dest_dfl - out, use
 *          pointer to destination dfl
 *          
 *      src_dfl - in
 *          source dfl
 *          
 *      scale - in
 *          truncating positions to right of decimal point
 *          
 * Return value :
 *      TRUE when successful
 *      FALSE when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_dfl_truncate(dt_dfl_t* p_dest_dfl, dt_dfl_t src_dfl, int scale)
{
        *p_dest_dfl = dfl_truncated(src_dfl, -scale);
        if (dfl_overflow(*p_dest_dfl)) {
            return (FALSE);
        }
        return (TRUE);
}

/************ Conversions from dfloat to other types ************/

/*##**********************************************************************\
 * 
 *          dt_dfl_dfltoint
 * 
 * Converts a dfl value to integer.
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          Dfl containing the original value   
 *          
 *      p_i - out, use
 *          Pointer to an integer where to store the result
 *          
 * Return value : 
 * 
 *      TRUE, if succeeded.
 *      FALSE, if overflow. *p_i contains INT_MAX or INT_MIN.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltoint(dt_dfl_t dfl, int* p_i)
{

        if (!dfl_initialized(&dfl_int_max)) {
            /* Initialize at first time */
            dt_dfl_setint(&dfl_int_max, INT_MAX);
            dt_dfl_setint(&dfl_int_min, INT_MIN);
            ss_output_4(
                char* ptr;
                ss_dprintf_4(("Initialized limits :\n"));
                ptr = dt_dfl_print(dfl_int_max);
                ss_dprintf_4(("dfl_int_max = %s\n", ptr));
                SsMemFree(ptr);
                ptr = dt_dfl_print(dfl_int_min);
                ss_dprintf_4(("dfl_int_min = %s\n", ptr));
                SsMemFree(ptr);
            )
        }
        if (dt_dfl_gt(dfl, dfl_int_max)) {
            *p_i = INT_MAX;
            return(FALSE);
        }
        if (dt_dfl_lt(dfl, dfl_int_min)) {
            *p_i = INT_MIN;
            return(FALSE);
        }
        *p_i = dfl_dfltoint(dfl);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *          dt_dfl_dfltolong
 * 
 * Converts a dfl value to long integer.
 * 
 *      dfl - in, use
 *          Dfl containing the original value   
 *          
 *      p_l - out, use
 *          Pointer to a long integer where to store the result
 *          
 * Return value : 
 * 
 *      TRUE, if succeeded.
 *      FALSE, if overflow. *p_i contains LONG_MAX or LONG_MIN.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltolong(dt_dfl_t dfl, long* p_l)
{
        ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
        if (!dfl_initialized(&dfl_long_max)) {
            /* Initialize at first time */
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            dt_dfl_setlong(&dfl_long_max, DF_LONGMAX);
            dt_dfl_setlong(&dfl_long_min, DF_LONGMIN);
        }
        ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
        if (dt_dfl_gt(dfl, dfl_long_max)) {
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            *p_l = LONG_MAX;
            return(FALSE);
        }
        ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
        if (dt_dfl_lt(dfl, dfl_long_min)) {
            *p_l = LONG_MIN;
            ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
            return(FALSE);
        }
        ss_dprintf_1(("%s, %d.\n", __FILE__, __LINE__));
        *p_l = dfl_dfltolong(dfl);
        return(TRUE);

}

/*##**********************************************************************\
 * 
 *          dt_dfl_dfltodouble
 * 
 * Converts a dfl value to double.
 * 
 *      dfl - in, use
 *          Dfl containing the original value   
 *          
 *      p_d - out, use
 *          Pointer to a double where to store the result
 *          
 * Return value : 
 * 
 *      TRUE, if succeeded.
 *      FALSE, if overflow
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltodouble(dt_dfl_t dfl, double* p_d)
{
        if (dfl_overflow(dfl)) {
            return(FALSE);
        }
        {
            char  buf[DFL_PRINTBUFSIZE];
            bool  b;
            char* ptr;

            b = dt_dfl_dfltoasciiz_maxlen(dfl, buf, DFL_PRINTBUFSIZE);
            ss_dassert(b);
            if (!b) {
                *p_d = 0.0;
                return(FALSE);
            }
            b = SsStrScanDouble(buf, p_d, &ptr);
            ss_dassert(b);
            return(TRUE);
        }
}
                 
/*##**********************************************************************\
 * 
 *          dt_dfl_dfltoasciiz_maxlen
 * 
 * Prints the dfl value into an external asciiz string with a maximum
 * buffer length limit.
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          Dfl to be converted 
 *          
 *      res - in out, use
 *          Pointer to the buffer where to print the dfl
 *          
 *      maxlen - in
 *          Max res buffer length
 *          
 * Return value : 
 * 
 *      TRUE, if succeeded.
 *      FALSE, if failed. 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltoasciiz_maxlen(dt_dfl_t dfl, char* res, int maxlen)
{
        ss_dassert(res != NULL);
        if (dfl_overflow(dfl)) {
            return(FALSE);
        }
        return(dfl_dfltoasciiz_maxlen(dfl, res, maxlen));
}

/*##**********************************************************************\
 * 
 *          dt_dfl_dfltoasciiz_dec_maxlen
 * 
 * Prints the dfl value into an external asciiz string using given number
 * of digits after decimal point.
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          dfl to be converted 
 *          
 *      n_decimals - in, use
 *          number of digits after the decimal point in the result
 *          
 *      res - in out, use
 *          Pointer to the buffer where to print the dfl
 *          
 *      maxlen - in
 *          Max res buffer length
 *          
 * Return value : 
 * 
 *      TRUE, if succeeded.
 *      FALSE, if failed. 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltoasciiz_dec_maxlen(dt_dfl_t dfl, int n_decimals, char* res, int maxlen)
{
        ss_dassert(res != NULL);
        if (dfl_overflow(dfl)) {
            return(FALSE);
        }
        return(dfl_dfltoasciiz_dec_maxlen(dfl, n_decimals, res, maxlen));
}

/*##**********************************************************************\
 * 
 *          dt_dfl_dfltova
 * 
 * Converts 
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          
 *          
 *      va - out, use
 *          
 *          
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dt_dfl_dfltova(dt_dfl_t dfl, va_t* va)
{
        dfl_dfltonst(&dfl, (char *)va);
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		dt_dfl_eqzero
 * 
 * Checks if a dloaf is zero.
 * 
 * Parameters : 
 * 
 *	p_dfl - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_dfl_eqzero(dt_dfl_t* p_dfl)
{
        return(df_eqzero(p_dfl));
}

/*##**********************************************************************\
 * 
 *		dt_dfl_negative
 * 
 * Checks whether a dfloat is negative
 * 
 * Parameters : 
 * 
 *	p_dfl - in, use
 *		pointer to dfloat
 *		
 * Return value :
 *      TRUE -> *p_dfl < 0
 *      FALSE -> *p_dfl >= 0
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dt_dfl_negative(dt_dfl_t* p_dfl)
{
        return (dfl_negative(p_dfl));
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *          dt_dfl_print
 * 
 * Prints the dfl variable
 * 
 * Parameters : 
 * 
 *      dfl - in, use
 *          dfl variable    
 *      
 * Return value - ref, give : 
 * 
 *      Pointer into the newly allocated string containing the dfl value
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* dt_dfl_print(dt_dfl_t dfl)
{
        bool b;
        char* buf = SsMemAlloc(30);
        b = dt_dfl_dfltoasciiz_maxlen(dfl, buf, 30);
        ss_dassert(b);
        return(buf);
}
#endif /* SS_DEBUG */
