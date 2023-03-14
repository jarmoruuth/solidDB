/*************************************************************************\
**  source       * ssdtoa.c
**  directory    * ss
**  description  * Double to ascii conversion 
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

Portable double to ascii conversions.

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

#include "ssenv.h"

#include "sswindow.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstring.h"
#include "ssctype.h"
#include "sschcvt.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssltoa.h"
#include "ssdtoa.h"

/*##**********************************************************************\
 * 
 *		SsDoubleToAscii
 * 
 * Converts a double into a string (cf. itoa).  This is needed because
 * MSC Windows DLL libraries don't have sprintf %f or _gcvt, and _ecvt
 * doesn't work in the 16-bit OS/2 libraries.  The output format is
 * unspecified, because it would be too much trouble to make the two
 * implementations return the same format.
 * 
 * Parameters : 
 * 
 *	value - in
 *		the double to convert
 *
 *	str - out, use
 *		buffer to put the string
 *
 *	count - in
 *		number of significant digits
 *
 * Return value : 
 * 
 *      str
 * 
 * Limitations  : 
 * 
 *      no buffer overflow check
 *                           
 *      Seems like the size of buffer str should be at least count+5
 *      digits(count) + sign(1) + decimal point(1) + exp info(2) + '\0'(1)
 *             -1.33e-205         
 * 
 * Globals used : 
 * 
 *      ecvt buffer
 */
char* SsDoubleToAscii(double value, char* str, int count)
{
#if defined(WINDOWS) && !defined(WCC)
#if (MSC==60)
#       define _gcvt gcvt
#endif
        /* Very strange results, if count > 15 and
         * value happens to be in E-format.  Don't know why. Ari
         */
        /* ss_dassert(count <= 15); */
        _gcvt(value, count, str);
        return (str);
#else /* WINDOWS */
        char format[4 + (sizeof(int) * 12 + 3) / 5];
        /* The buffer is large enough for the sign and the exponent (4 chars),
           plus as many digits as can be specified with an int (one byte
           can specify lg 256 ~= 2.4 = 12/5 digits, multiply by the number of
           bytes, add 3/5 to round up. */

        sprintf(format, "%%.%dg", count);
        ss_dprintf_2(("SsDoubleToAscii: format = %s\n", format));
        sprintf(str, format, value);
        ss_dprintf_1(("SsDoubleToAscii: str = %s\n", str));

#endif /* WINDOWS */

        return(str);
}



char* SsDoubleToAsciiE(double value, char* str, int count)
{
#ifdef WINDOWS
        char* buffer;
        int dec, sign;
        size_t i_str, i_buffer, i_nonzero;
        char ch;

        /* The output format for the ecvt version is [-]0.d*E[-]e*, where
           d are the digits of the mantissa, at most count of them,
           trailing zeroes removed, and e are the digits of the exponent,
           leading zeroes removed. */

        buffer = ecvt(value, count, &dec, &sign);
        if (sign) {
            str[0] = '-';
            i_str = 1;
        } else {
            i_str = 0;
        }
        
        str[i_str++] = '0';
        str[i_str++] = '.';

        for (i_buffer = 0, i_nonzero = i_str; (int)i_buffer < count;) {
            str[i_str++] = ch = buffer[i_buffer++];
            if (ch != '0') i_nonzero = i_str;
        }
        i_str = i_nonzero;

        str[i_str++] = 'E';
        
        itoa(dec, str + i_str, 10);
        return(str);
#else   /* WINDOWS */
        char format[4 + (sizeof(int) * 12 + 3) / 5]; /* lg 256 = 2.4 */
        sprintf(format, "%%.%dE", count); 
        sprintf(str, format, value);
        return(str);
#endif
}


/*##**********************************************************************\
 * 
 *		SsTruncateAsciiDoubleValue
 * 
 * Truncates a double (or float) value that has been printed to string
 * to fit into certain buffer size.
 * 
 * Parameters : 
 * 
 *	buffer - in out, use
 *		character buffer containing valid floating-point value
 *		
 *	maxsize - in
 *		maximum size (including null byte) of the resulting
 *          string
 *		
 * Return value :
 *      SS_DBLTRUNC_OK when no non-null digits were truncated
 *      SS_DBLTRUNC_TRUNCATED when significant digits were lost
 *      SS_DBLTRUNC_VALUELOST when the significant value was spoiled
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsDoubleTruncateRetT SsTruncateAsciiDoubleValue(char* buffer, size_t maxsize)
{
        int buflen;
        int i;
        int e_pos;
        int ntoremove;
        bool decpointpassed;
        bool hasdecpoint;
        bool nonnulltruncated;
        
        buflen = strlen(buffer);
        ntoremove = buflen - (int)maxsize + 1;
        if (ntoremove <= 0) {
            return (SS_DBLTRUNC_OK);
        }
        for (hasdecpoint = FALSE, i = 0; i < buflen; i++) {
            switch (buffer[i]) {
                case 'E':
                case 'e':
                    break;
                case '.':
                    hasdecpoint = TRUE;
                    continue;
                default:
                    continue;
            }
            break;
        }
        if (!hasdecpoint) {
            return (SS_DBLTRUNC_VALUELOST);
        }
        e_pos = i;
        nonnulltruncated = FALSE;
        for (i--, decpointpassed = FALSE; i > 0; i--) {
            if (decpointpassed) {
                return (SS_DBLTRUNC_VALUELOST);
            }
            if (buffer[i] == '.') {
                decpointpassed = TRUE;
            } else if (buffer[i] != '0') {
                if (!ss_isdigit((ss_byte_t)buffer[i])) {
                    return (SS_DBLTRUNC_VALUELOST);
                }
                nonnulltruncated = TRUE;
            }
            if (e_pos - i >= ntoremove) {
                ss_dassert(e_pos - i == ntoremove);
                break;
            }
        }
        if (i <= 0) {
            ss_dassert(i == 0);
            return (SS_DBLTRUNC_VALUELOST);
        }
        if (buffer[e_pos] == '\0') {
            /* Not E format */
            buffer[i] = '\0';
        } else {
            /* E format */
            memmove(buffer + i, buffer + e_pos, buflen - e_pos + 1);
        }
        if (nonnulltruncated) {
            return (SS_DBLTRUNC_TRUNCATED);
        }
        return (SS_DBLTRUNC_OK);
}

/*##**********************************************************************\
 * 
 *		SsDoubleToAsciiDecimals
 * 
 * Same as SsDoubleToAscii but with a fixed number of decimals.
 * 
 * Note that numbers with E format a not modified by this routine,
 * they are returned in the same format as SsDoubleToAscii.
 * 
 * No rounding is done by this routine.
 * 
 * Parameters : 
 * 
 *	d - 
 *		
 *		
 *	str - 
 *		
 *		
 *	maxlen - 
 *		
 *		
 *	decimals - 
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
char* SsDoubleToAsciiDecimals(double d, char* str, int maxlen, int decimals)
{
        char* dot;

        SsDoubleToAscii(d, str, maxlen);

        if (strchr(str, 'e') != NULL || strchr(str, 'E') != NULL) {
            /* In E format, dont know what to do */
            return(str);
        }
        dot = strchr(str, '.');
        if (dot == NULL) {
            strcat(str, ".");
            dot = strchr(str, '.');
            ss_dassert(dot != NULL);
        }
        /* Skip dot */
        dot++;
        while (decimals-- > 0) {
            if (*dot == '\0') {
                strcpy(dot, "0");
            }
            dot++;
        }
        *dot = '\0';
        return(str);
}
