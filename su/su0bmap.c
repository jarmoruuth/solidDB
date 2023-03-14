/*************************************************************************\
**  source       * su0bmap.c
**  directory    * su
**  description  * Functions for bit map handling
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


#include "su0bmap.h"

#define ELEM_BIT (sizeof(su_bmelem_t)*CHAR_BIT)

/*##**********************************************************************\
 * 
 *		su_bmap_find1st
 * 
 * Finds first occurence position of bool value in bit map
 * 
 * Parameters : 
 * 
 *	bmap - in, use
 *		pointer to bitmap storage
 *		
 *	bmsize - in
 *		bitmap size in bits
 *		
 *	val - in
 *		TRUE when searching for TRUE value or
 *          FALSE when searching for FALSE
 *		
 * Return value : 
 *      bit # of first occurence of val in bmap or
 *      -1 when val is not found from bmap
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
long su_bmap_find1st(su_bmap_t* bmap, ulong bmsize, bool val)
{
        su_bmelem_t e;
        su_bmelem_t* ep;
        size_t i;
        size_t i_max;
        long rv;

        if (val) {
            val = TRUE;
            e = (su_bmelem_t)0L;
        } else {
            e = (su_bmelem_t)~0L;
        }
        i_max = (size_t)((bmsize + ELEM_BIT - 1) / ELEM_BIT);
        for (ep = bmap, i = 0; i < i_max; i++, ep++) {
            if (*ep != e) {
                break;
            }
        }
        for (rv = i * ELEM_BIT; rv < (long)bmsize; rv++) {
            if (SU_BMAP_GET(bmap, (ulong)rv) == val) {
                return (rv);
            }
        }
        ss_dassert(rv >= (long)bmsize);
        return (SU_BMAPRC_NOTFOUND);
}

/*##**********************************************************************\
 * 
 *		su_bmap_findlast
 * 
 * Finds last occurence of bool value in bit map
 * 
 * Parameters : 
 * 
 *	bmap - in, use
 *		pointer to bit map storage
 *		
 *	bmsize - in
 *		bit map size in bits
 *		
 *	val - in
 *		TRUE when searching for TRUE value or
 *          FALSE when searching for FALSE
 *		
 * Return value : 
 *      bit # of last occurence of val in bmap or
 *      -1 when val is not found from bmap
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
long su_bmap_findlast(su_bmap_t* bmap, ulong bmsize, bool val)
{
        su_bmelem_t e;
        size_t i;
        long rv;

        if (val) {
            val = TRUE;
            e = (su_bmelem_t)0;
        } else {
            e = (su_bmelem_t)~0;
        }
        i = (size_t)(bmsize % ELEM_BIT);
        rv = bmsize;
        if (i > 0) {
            do {
                i--;
                rv--;
                if (SU_BMAP_GET(bmap, (ulong)rv) == val) {
                    return (rv);
                }
            } while (i > 0);
        }
        bmsize = rv;
        ss_dassert(bmsize % ELEM_BIT == 0);
        i = (size_t)(bmsize / ELEM_BIT);
        for (; i; i--) {
            if ((bmap-1)[i] != e) {
                break;
            }
        }
        if (i == 0) {
            return (SU_BMAPRC_NOTFOUND);
        }
        i--;
        rv = i * ELEM_BIT + (ELEM_BIT - 1);
        ss_dassert(rv < (long)bmsize);
        for (; ; rv--) {
            ss_dassert(rv >= 0L);
            if (SU_BMAP_GET(bmap, (ulong)rv) == val) {
                break;
            }
        }
        return (rv);
}

long su_bmap_findnext(su_bmap_t* bmap, ulong bmsize, bool val, ulong startpos)
{
        size_t same_bits;
        su_bmelem_t same_byte;

        ss_dassert(startpos<=bmsize);

        if (startpos==bmsize) {
            return SU_BMAPRC_NOTFOUND;
        }

        same_bits = startpos%ELEM_BIT;
        same_byte = bmap[startpos/ELEM_BIT];

        val = val ? TRUE : FALSE;

        if (startpos+ELEM_BIT<bmsize &&
            ((val && (same_byte>>same_bits)==0) ||
             (!val && (same_byte|((1<<same_bits)-1))==(su_bmelem_t)~0)))
        {
            long ret;
            startpos -= same_bits;
            startpos += ELEM_BIT;
            ret = su_bmap_find1st(bmap+startpos/ELEM_BIT,bmsize-startpos,val);
            if (ret==SU_BMAPRC_NOTFOUND) {
                return ret;
            } else {
                return ret+startpos;
            }
        } else {
            while (startpos<bmsize) {
                if (SU_BMAP_GET(bmap, startpos)==val) {
                    return startpos;
                }
                startpos++;
            }
            return SU_BMAPRC_NOTFOUND;
        }
}
