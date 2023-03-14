/*************************************************************************\
**  source       * rs0viewh.c
**  directory    * res
**  description  * View handle functions
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
A view handle contains a view definition string, view id and a view name.
Several methods exists to access these properties.

Limitations:
-----------
None

Error handling:
--------------
None

Objects used:
------------
None.

Preconditions:
-------------
None.

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same viewh object can not be used simultaneously from many threads.


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssmem.h>

#include "rs0types.h"
#include "rs0ttype.h"
#include "rs0viewh.h"
#include "rs0sysi.h"
#include "rs0sdefs.h"
#include "rs0auth.h"
#include "rs0entna.h"

#ifndef SS_NOVIEW

/* type for view handle */
struct rsviewhstruct {

        rs_check_t      vh_check;
        int             vh_nlink;   /* number of links to this relh,
                                       free is not done until the link count
                                       is zero */
        rs_entname_t*   vh_name;    /* name of the view */
        rs_ttype_t*     vh_ttype;   /* tuple type */
        ulong           vh_viewid;  /* id of the view */
        char*           vh_def;     /* definition of the view */
        SsSemT*         vh_sem;

}; /* rs_viewh_t */

#define CHECK_VIEWH(vh) {\
                            ss_dassert(SS_CHKPTR(vh));\
                            ss_dassert((vh)->vh_check == RSCHK_VIEWHTYPE);\
                        }



/*##**********************************************************************\
 * 
 *		rs_viewh_init
 * 
 * Creates a virtual view handle to a non-existent (new) view.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewname - in, use
 *		Name of the new view
 *		
 *	viewid - in, use
 *		Id of the view
 *		
 *	ttype - in, use
 *		tuple type of the view
 *
 * Return value - give : 
 * 
 *      Pointer to new view handle object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_viewh_t* rs_viewh_init(
	void* cd,
	rs_entname_t* viewname,
        ulong viewid,
	rs_ttype_t* ttype
){
        rs_viewh_t* viewh;

        SS_NOTUSED(cd);
        ss_dassert(viewname != NULL);
        viewh = SSMEM_NEW(rs_viewh_t);
        viewh->vh_check = RSCHK_VIEWHTYPE;
        viewh->vh_nlink = 1;
        viewh->vh_name = rs_entname_copy(viewname);
        if (ttype == NULL) {
            viewh->vh_ttype = NULL;
        } else {
            viewh->vh_ttype = rs_ttype_copy(cd, ttype);
        }
        viewh->vh_viewid = viewid;
        viewh->vh_def = NULL;
        viewh->vh_sem = rs_sysi_getrslinksem(cd);
        CHECK_VIEWH(viewh);
        return(viewh);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_done
 * 
 * Releases a view handle object
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, take
 *		view handle
 *	    	
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_viewh_done(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        ss_dassert(viewh->vh_name != NULL);

        SsSemEnter(viewh->vh_sem);

        ss_dassert(viewh->vh_nlink > 0);

        viewh->vh_nlink--;

        if (viewh->vh_nlink == 0) {

            /* Do the actual free.
             */
            SsSemExit(viewh->vh_sem);

            rs_entname_done(viewh->vh_name);
            if (viewh->vh_ttype != NULL) {
                rs_ttype_free(cd, viewh->vh_ttype);
            }
            if (viewh->vh_def) {
                SsMemFree(viewh->vh_def);
            }

            SsMemFree(viewh);

        } else {

            SsSemExit(viewh->vh_sem);
        }
}

/*##**********************************************************************\
 * 
 *		rs_viewh_link
 * 
 * Links the current user to the view handle. The handle is not
 * released until the link count is zero, i.e. everyone has called
 * rs_viewh_done.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in out, use
 *		view handle
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_viewh_link(cd, viewh)
	void*      cd;
	rs_viewh_t* viewh;
{
        SS_NOTUSED(cd);

        ss_dprintf_1(("%s: rs_viewh_done\n", __FILE__));
        CHECK_VIEWH(viewh);

        SsSemEnter(viewh->vh_sem);

        ss_dassert(viewh->vh_nlink > 0);

        viewh->vh_nlink++;

        SsSemExit(viewh->vh_sem);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_def
 * 
 * Gets the definition string for a view handle.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, use
 *		view handle
 * 
 * Return value - ref : 
 * 
 *      pointer into the view definition string
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* rs_viewh_def(cd, viewh)
	void*       cd;
	rs_viewh_t* viewh;
{
        SS_NOTUSED(cd);
        SS_NOTUSED(viewh);
        CHECK_VIEWH(viewh);
        return(viewh->vh_def);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_setdef
 * 
 * Sets the definition string of a view handle
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in out, use
 *		view handle
 *	    	
 *	viewdef - in, use
 *		Definition string for the view
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_viewh_setdef(
        void*       cd,
        rs_viewh_t* viewh,
        char*       viewdef 
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        if (viewh->vh_def) {
            SsMemFree(viewh->vh_def);
        }
        viewh->vh_def = SsMemStrdup(viewdef);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_setviewid
 * 
 * Sets the id of a view handle
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in out, use
 *		view handle
 *	    	
 *	viewid - in
 *		Identification for the view handle
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_viewh_setviewid(
        void*       cd,
        rs_viewh_t* viewh,
        ulong       viewid
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        viewh->vh_viewid = viewid;
}

/*##**********************************************************************\
 * 
 *		rs_viewh_viewid
 * 
 * Returns the id of the view
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, use
 *		view handle
 *	    	
 * Return value : 
 * 
 *      View id
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ulong rs_viewh_viewid(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        return(viewh->vh_viewid);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_ttype
 * 
 * Returns the ttype of the view
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, use
 *		view handle
 *	    	
 * Return value - ref : 
 * 
 *      tuple type of the view
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ttype_t* rs_viewh_ttype(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        return(viewh->vh_ttype);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_name
 * 
 * Returns the name of the view handle
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, use
 *		view handle
 *		
 * Return value - ref : 
 * 
 *      Name of the view.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* rs_viewh_name(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        ss_dassert(viewh->vh_name != NULL);
        return(rs_entname_getname(viewh->vh_name));
}

/*##**********************************************************************\
 * 
 *		rs_viewh_entname
 * 
 * Returns entity name object of view.
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	viewh - 
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
rs_entname_t* rs_viewh_entname(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        ss_dassert(viewh->vh_name != NULL);
        return(viewh->vh_name);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_schema
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	viewh - in
 *		
 *		
 * Return value  - ref : 
 * 
 *      Schema of viewh, or NULL if no schema.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_viewh_schema(
        void*      cd,
        rs_viewh_t* viewh)
{
        char* schema;
        
        ss_dprintf_1(("%s: rs_viewh_schema\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        schema = rs_entname_getschema(viewh->vh_name);
        return (schema);
}

/*##**********************************************************************\
 * 
 *		rs_viewh_catalog
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	viewh - in
 *		
 *		
 * Return value  - ref : 
 * 
 *      Catalog of viewh, or NULL if no catalog.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_viewh_catalog(
        void*      cd,
        rs_viewh_t* viewh)
{
        char* catalog;
        ss_dprintf_1(("%s: rs_viewh_catalog\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);
        catalog = rs_entname_getcatalog(viewh->vh_name);
        return (catalog);
}

void rs_viewh_setcatalog(
        void*      cd __attribute__ ((unused)),
        rs_viewh_t* viewh,
        char* newcatalog)
{
        rs_entname_t* new_en;
        
        CHECK_VIEWH(viewh);
        new_en = rs_entname_init(newcatalog,
                                 rs_entname_getschema(viewh->vh_name),
                                 rs_entname_getname(viewh->vh_name));
        rs_entname_done(viewh->vh_name);
        viewh->vh_name = new_en;
}
        
/*##**********************************************************************\
 * 
 *		rs_viewh_issysview
 * 
 * Checks if the viewation handle is a system viewation handle.
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	viewh - 
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
bool rs_viewh_issysview(
	void*      cd,
	rs_viewh_t* viewh)
{
        ss_dprintf_1(("%s: rs_viewh_issysview\n", __FILE__));
        SS_NOTUSED(cd);
        CHECK_VIEWH(viewh);

        if (viewh->vh_viewid < RS_USER_ID_START) {
            return(TRUE);
        }
        if (strcmp(rs_entname_getschema(viewh->vh_name), RS_AVAL_SYSNAME) == 0) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *		rs_viewh_print
 * 
 * Prints view handle using SsDbgPrintf.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	viewh - in, use
 *		view handle
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_viewh_print(
        void*       cd,
        rs_viewh_t* viewh
){
        SS_NOTUSED(cd);
        SsDbgPrintf("VIEW:\n");
        SsDbgPrintf("-----\n");
        SsDbgPrintf("%-20s %-20s %-5s %s\n", "SCHEMA", "NAME", "ID", "DEF");
        SsDbgPrintf("%-20s %-20s %5ld %s\n",
            rs_entname_getschema(viewh->vh_name),
            rs_entname_getname(viewh->vh_name),
            viewh->vh_viewid,
            viewh->vh_def != NULL ? viewh->vh_def : "NULL");

}
#endif /* SS_DEBUG */

#endif /* SS_NOVIEW */
