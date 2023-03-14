/*************************************************************************\
**  source       * ssltow.c
**  directory    * ss
**  description  * Long integer to Wide-character string
**               * conversion
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

#include "ssc.h"
#include "ssltow.h"
#include "ssdebug.h"
#include "sswcs.h"

static const ss_char2_t int2wc_xlat[36] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

/*##**********************************************************************\
 * 
 *		SsLongToWcs
 * 
 * Long integer to Wide-char string conversion function
 * 
 * Parameters : 
 * 
 *	l - in
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
size_t SsLongToWcs(
        long l,
        ss_char2_t* buf,
        uint radix,
        size_t width,
        ss_char2_t leftfillch,
        bool is_signed)
{
        ss_char2_t* p;
        int sign;
        size_t n;
        ulong ul;

        ss_dprintf_1((
        "SsLongToWcs(l=%ld,buf=0x%08lX,radix=%u,width=%u,leftfillch=0x%04X(='%c'),signed=%d\n",
                    l, buf, radix, (uint)width, (uint)leftfillch, leftfillch, is_signed));
        if (radix == 10 && is_signed && l < 0) {
            sign = 1;
            ul = (ulong)(-(long)l);
        } else {
            sign = 0;
            ul = (ulong)l;
        }
        ss_dassert((uint)(radix - 2) <=
            (uint)(sizeof(int2wc_xlat)/sizeof(int2wc_xlat[0]) - 2));
        if ((uint)(radix - 2) >
            (uint)(sizeof(int2wc_xlat)/sizeof(int2wc_xlat[0]) - 2))
        {
            return (0);
        }
        if (width == 0) {
            ss_char2_t* p1;

            p = buf + sign;
            p1 = p;
            if (sign) {
                buf[0] = (ss_char2_t)'-';
            }
            do {
                *p = int2wc_xlat[ul % radix];
                ul /= radix;
                p++;
            } while (ul);
            *p = (ss_char2_t)0;
            width = (p - buf);
            /* reverse the string */
            for (; --p > p1; p1++) {
                ss_char2_t ch;

                ch = *p;
                *p = *p1;
                *p1 = ch;
            }
        } else {
            p = buf + width;
            *p = (ss_char2_t)0;
            
            for (n = width; n;) {
                p--;
                n--;
                *p = int2wc_xlat[ul % radix];
                ul /= radix;
                if (!ul) {
                    break;
                }
            }
            if (n && leftfillch != (ss_char2_t)'0') {
                if (sign) {
                    p--;
                    sign = FALSE;
                    n--;
                    *p = (ss_char2_t)'-';
                }
            }
            while (n > (size_t)sign) {
                p--;
                n--;
                *p = leftfillch;
            }
            if (sign && n) {
                ss_dassert(n == 1);
                p[-1] = (ss_char2_t)'-';
            }
        }
        ss_debug(
            {
                ss_char1_t dbg_buf[128];
                SsWcs2Str(dbg_buf, buf);
                ss_dprintf_1(("SsLongToWcs: *buf = %s return (%u)\n",
                    dbg_buf, (uint)width));
            }
        );
        return (width);
}

/*##**********************************************************************\
 * 
 *		SsLtow
 * 
 * Replacement for library routine ltow(), that is not available
 * in every system
 * 
 * Parameters : 
 * 
 *	l - in
 *		value to convert
 *		
 *	buf - out
 *		string buffer
 *		
 *	radix - in
 *		print radix (2 .. 36)
 *		
 * Return value :
 *      buf
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsLtow(long l, ss_char2_t* buf, int radix)
{
        size_t len;

        len = SsLongToWcs(l, buf, radix, 0, (ss_char2_t)0, TRUE);
        ss_dassert(len != 0);
        return (buf);
}

/*##**********************************************************************\
 * 
 *		SsUltow
 * 
 * Replacement for library routine ultow(), that is not available
 * in every system
 * 
 * Parameters : 
 * 
 *	l - in
 *		value to convert
 *		
 *	buf - out
 *		string buffer
 *		
 *	radix - in
 *		print radix (2 .. 36)
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsUltow(ulong l, ss_char2_t* buf, int radix)
{
        size_t len;

        len = SsLongToWcs((long)l, buf, radix, 0, (ss_char2_t)0, FALSE);
        ss_dassert(len != 0);
        return (buf);
}
