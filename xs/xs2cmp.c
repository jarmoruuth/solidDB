/*************************************************************************\
**  source       * xs2cmp.c
**  directory    * xs
**  description  * compare function for solid sort utility
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

#include <uti0vcmp.h>
#include "xs0acnd.h"
#include "xs2cmp.h"


/*##**********************************************************************\
 * 
 *		xs_qsort_cmp
 * 
 * Compares two vtuples used by xs_qsort(). This function is used
 * merely for making an extra dereference to 2 first parameters
 * 
 * Parameters : 
 * 
 *	vtpl1 - in, use
 *		pointer^2 to 1st vtuple
 *		
 *	vtpl2 - in, use
 *		pointer^2 to 2nd vtuple
 *		
 *	cmpcondarr - in, use
 *		array of orderby conditions (begins with cond count)
 *		
 * Return value :
 *      == 0 when vtuples compare equal up to length of orderby_list
 *      <  0 when vtpl1 < vtpl2 (logically)
 *      >  0 otherwise
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int xs_qsort_cmp(
        vtpl_t** vtpl1,
        vtpl_t** vtpl2,
        uint* cmpcondarr)
{
        register int cmp;

        cmp = vtpl_condcompare(*vtpl1, *vtpl2, cmpcondarr);
        return (cmp);
}
