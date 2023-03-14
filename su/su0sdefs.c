/*************************************************************************\
**  source       * su0sdefs.c
**  directory    * su
**  description  * System definitions.
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

#include <ssenv.h>

#include <ssstring.h>
#include <ssctype.h>
#include <sschcvt.h>

#include <ssc.h>
#include <ssutf.h>

#include "su0sdefs.h"

/*##**********************************************************************\
 * 
 *		su_sdefs_isvalidusername
 * 
 * Checks that user name is valid.
 * 
 * Parameters : 
 * 
 *	name - 
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
bool su_sdefs_isvalidusername(char* name)
{
        uint len;

        len = strlen(name);
        len = SsUTF8CharLen((ss_byte_t*)name, len);
        if (len < SU_MINUSERNAMELEN || len > SU_MAXUSERNAMELEN) {
            return(FALSE);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		su_sdefs_isvalidpassword
 * 
 * Checks that password is valid.
 * 
 * Parameters : 
 * 
 *	name - 
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
bool su_sdefs_isvalidpassword(char* name)
{
        uint len;

        len = strlen(name);
        len = SsUTF8CharLen((ss_byte_t*)name, len);

        if (len < SU_MINPASSWORDLEN || len > SU_MAXPASSWORDLEN) {
            return(FALSE);
        }
        while (*name != '\0') {
            if (*name == '\"') {
                return(FALSE);
            }
            name++;
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		su_sdefs_isvalidcatalog
 * 
 * 
 * 
 * Parameters : 
 * 
 *	name - 
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
bool su_sdefs_isvalidcatalog(char* name)
{
        uint len;

        len = strlen(name);
        len = SsUTF8CharLen((ss_byte_t*)name, len);
        if (len < SU_MINCATALOGLEN || len > SU_MAXDEFCATALOGLEN) {
            return(FALSE);
        }
        return(TRUE);
}

