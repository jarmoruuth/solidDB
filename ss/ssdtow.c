/*************************************************************************\
**  source       * ssdtow.c
**  directory    * ss
**  description  * double to wide-char string conversion
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

#include "ssdtow.h"
#include "ssdebug.h"
#include "sswcs.h"

/*##**********************************************************************\
 * 
 *		SsDoubleToWcs
 * 
 * See SsDoubleToAscii
 * 
 * Parameters : 
 * 
 *	value - 
 *		
 *		
 *	wcs - 
 *		
 *		
 *	count - 
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
ss_char2_t* SsDoubleToWcs(
        double value,
        ss_char2_t* wcs,
        int count)
{
        (void)SsDoubleToAscii(value, (ss_char1_t*)wcs, count);
        SsStr2WcsInPlace(wcs);
        return (wcs);
}

/*##**********************************************************************\
 * 
 *		SsDoubleToWcsE
 * 
 * See SsDoubleToAsciiE
 * 
 * Parameters : 
 * 
 *	value - 
 *		
 *		
 *	wcs - 
 *		
 *		
 *	count - 
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
ss_char2_t* SsDoubleToWcsE(
        double value,
        ss_char2_t* wcs,
        int count)
{
        (void)SsDoubleToAsciiE(value, (ss_char1_t*)wcs, count);
        SsStr2WcsInPlace(wcs);
        return (wcs);
}

/*##**********************************************************************\
 * 
 *		SsTruncateWcsDoubleValue
 * 
 * SsTruncateAsciiDoubleValue
 * 
 * Parameters : 
 * 
 *	buffer - 
 *		
 *		
 *	maxsize - 
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
SsDoubleTruncateRetT SsTruncateWcsDoubleValue(
        ss_char2_t* buffer,
        size_t maxsize)
{
        bool succp;
        SsDoubleTruncateRetT rc;

        succp = SsWcs2StrInPlace(buffer);
        ss_assert(succp);
        rc = SsTruncateAsciiDoubleValue((ss_char1_t*)buffer, maxsize);
        if (rc == SS_DBLTRUNC_VALUELOST) {
            *buffer = (ss_char2_t)0;
        } else {
            ss_rc_dassert(rc == SS_DBLTRUNC_OK
                    ||    rc == SS_DBLTRUNC_TRUNCATED, rc);
            SsStr2WcsInPlace(buffer);
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsDoubleToWcsDecimals
 * 
 * see SsDoubleToAsciiDecimals
 * 
 * Parameters : 
 * 
 *	d - 
 *		
 *		
 *	wcs - 
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
ss_char2_t* SsDoubleToWcsDecimals(
        double d,
        ss_char2_t* wcs,
        int maxlen,
        int decimals)
{
        (void)SsDoubleToAsciiDecimals(d, (ss_char1_t*)wcs, maxlen, decimals);
        SsStr2WcsInPlace(wcs);
        return (wcs);
}



