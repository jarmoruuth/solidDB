/*************************************************************************\
**  source       * rs0defno.c
**  directory    * res
**  description  * Default node definition
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
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>

#include "rs0types.h"
#include "rs0sdefs.h"
#include "rs0defno.h"

#define CHK_DEFNODE(defno)  ss_dassert(defno != NULL && defno->dn_chk == RSCHK_DEFNODE)

struct rs_defnode_st {
        rs_check_t          dn_chk;
        rs_defnodetype_t    dn_type;
        char*               dn_name;
        char*               dn_connectstr;
};

/*##**********************************************************************\
 * 
 *		rs_defnode_init
 * 
 *  Create a default node object.
 * 
 * Parameters : 
 * 
 *	type - in, use
 *              (master, replica, ...)
 *      name - in, take
 *              node name (master or replica name)
 *
 *      connectstr - in, take (can be NULL)       
 *
 * Return value : 
 * 
 *      defnode - out, give
 *		
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_defnode_t* rs_defnode_init(
        rs_defnodetype_t    type,
        char*               name,
        char*               connectstr)
{
        rs_defnode_t* defnode;

        ss_dassert(name != NULL);

        defnode = SSMEM_NEW(rs_defnode_t);

        ss_debug(defnode->dn_chk = RSCHK_DEFNODE);
        defnode->dn_type = type;
        defnode->dn_name = name;
        defnode->dn_connectstr = connectstr;

        CHK_DEFNODE(defnode);

        return defnode;
}

/*##**********************************************************************\
 * 
 *		rs_defnode_done
 * 
 *  Frees a default node object.
 * 
 * Parameters : 
 * 
 *      defnode - in take
 *
 * Return value : 
 *		
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_defnode_done(
        rs_defnode_t* defnode)
{
        CHK_DEFNODE(defnode);

        SsMemFree(defnode->dn_name);
        if (defnode->dn_connectstr != NULL) {
            SsMemFree(defnode->dn_connectstr);
        }
        SsMemFree(defnode);

        return;
}

/*##**********************************************************************\
 * 
 *		rs_defnode_type
 * 
 *  Returns the type of the default node object.
 * 
 * Parameters : 
 * 
 *      defnode - in, ref
 *
 * Return value : 
 *		
 *      type - out
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_defnodetype_t rs_defnode_type(
        rs_defnode_t* defnode)
{
        CHK_DEFNODE(defnode);

        return defnode->dn_type;
}

/*##**********************************************************************\
 * 
 *		rs_defnode_name
 * 
 *  Returns the name of the default node object.
 * 
 * Parameters : 
 * 
 *      defnode - in, ref
 *
 * Return value : 
 *		
 *      name - out, ref
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_defnode_name(
        rs_defnode_t* defnode)
{
        CHK_DEFNODE(defnode);

        return defnode->dn_name;
}

/*##**********************************************************************\
 * 
 *		rs_defnode_connectstr
 * 
 *  Returns the connect string of the default node object.
 * 
 * Parameters : 
 * 
 *      defnode - in, ref
 *
 * Return value : 
 *		
 *      connectstr - out, ref
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_defnode_connectstr(
        rs_defnode_t* defnode)
{
        CHK_DEFNODE(defnode);

        return defnode->dn_connectstr;
}
