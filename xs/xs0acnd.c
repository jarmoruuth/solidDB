/*************************************************************************\
**  source       * xs0acnd.c
**  directory    * xs
**  description  * Attributewise sort condition interface
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


#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include "xs0acnd.h"

/*##**********************************************************************\
 * 
 *		xs_acond_init
 * 
 * Creates an attributewise sort condition object
 * 
 * Parameters : 
 * 
 *	ascending - in
 *		TRUE when ascending, FALSE when descending
 *		
 *	ano - in
 *		SQL attribute number in tval
 *		
 * Return value - give :
 *      pointer to created object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_acond_t* xs_acond_init(
        bool ascending,
        rs_ano_t ano)
{
        xs_acond_t* acond;

        acond = SSMEM_NEW(xs_acond_t);
        acond->sac_asc =  ascending;
        acond->sac_ano = ano;
        return (acond);
}

/*##**********************************************************************\
 * 
 *		xs_acond_done
 * 
 * Deletes an attributewise sort condition object
 * 
 * Parameters : 
 * 
 *	acond - in, take
 *		pointer to object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_acond_done(xs_acond_t* acond)
{
        ss_dassert(acond != NULL);
        SsMemFree(acond);
}
