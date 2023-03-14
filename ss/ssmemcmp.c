/*************************************************************************\
**  source       * ssmemcmp.c
**  directory    * ss
**  description  * a replacement for memcmp() for systems
**               * where memcmp() of C lib is signed
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

#include "ssstring.h"

#ifndef SsMemcmp

#include "ssc.h"

/*##**********************************************************************\
 * 
 *		SsMemcmp
 * 
 * Exactly as C lib memcmp() but this always makes an unsigned char
 * comparison
 * 
 * Parameters : 
 * 
 *	p1 - in, use
 *		pointer to 1st memory block
 *		
 *	p2 - in, use 
 *		pointer to 2nd memory block
 *		
 *	n - in
 *		size of blocks to compare
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsMemcmp(const void* p1, const void* p2, size_t n)
{
        const uchar* pc1 = p1;
        const uchar* pc2 = p2;

        int diff;
        for (diff = 0; n; n--, pc1++, pc2++) {
            diff = (int)*pc1 - (int)*pc2;
            if (diff) {
                break;
            }
        }
        return (diff);
}

#endif /* SsMemcmp */
