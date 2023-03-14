/*************************************************************************\
**  source       * sslscan.c
**  directory    * ss
**  description  * Portable sscan utilities for wide char strings
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

#include "ssenv.h"
#include "sslimits.h"
#include "ssc.h"
#include "ssdebug.h"
#include "sswctype.h"
#include "sslscan.h"

#define SS_NATIVE_LONG_MAX (((ulong)~0UL) >> 1U)

/*##**********************************************************************\
 * 
 *		SsLcsScanLong
 * 
 * Converts an wide-char string to long integer. 
 * Leading whitespaces are skipped.
 * 
 *  [whitespace][{+|-}][digits]
 * 
 * Parameters : 
 * 
 *      s - in, use
 *          source string
 *
 *      p_l - out
 *          pointer to long integer
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
bool SsLcsScanLong(ss_lchar_t* s, long* p_l, ss_lchar_t** p_mismatch)
{
        bool valid = FALSE;
        bool negative = FALSE;
        ulong v = 0UL;
        ulong t;

        ss_dassert(s != NULL);
        ss_dassert(p_l != NULL);
        ss_dassert(p_mismatch != NULL);

        while (ss_iswspace(*s)) {
            s++;
        }
        switch (*s) {
            case '-':
                negative = TRUE;
                /* FALLTHROUGH */
            case '+':
                s++;
                break;
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
            if (v > (SS_NATIVE_LONG_MAX - t) / 10UL) {
                valid = FALSE;
                break;
            }
            v = v * (ulong)10 + t;
            s++;
            valid = TRUE;
        }
 out:;
        if (negative) {
            *p_l = -(long)v;
        } else {
            *p_l = (long)v;
        }
        *p_mismatch = s;
        return (valid);
}
