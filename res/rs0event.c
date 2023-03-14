/*************************************************************************\
**  source       * rs0event.c
**  directory    * res
**  description  * Event handle functions
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
A event handle contains a event id and a event full name and paraminfo.
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
#include "rs0event.h"
#include "rs0sysi.h"
#include "rs0sdefs.h"
#include "rs0auth.h"
#include "rs0entna.h"

/* type for event handle */
struct rs_event_st {
        rs_check_t  ev_check;
        int         ev_nlink;   /* number of links to this relh,
                                 * free is not done until the link count
                                 * is zero */
        rs_entname_t* ev_name; /* name of the event */
        long        ev_id;
        int         ev_paramcount;
        int*        ev_paramtypes;
        SsSemT*     ev_sem;
};


#define CHECK_EVENT(ev) {\
                            ss_dassert(SS_CHKPTR(ev));\
                            ss_dassert((ev)->ev_check == RSCHK_EVENTTYPE);\
                        }


#ifndef SS_MYSQL
static char* ev_checknullstr(char* s)
{
        if (s == NULL) {
            return((char *)"");
        }
        return(s);
}

static char* ev_allocstr(char* s)
{
        return(SsMemStrdup(ev_checknullstr(s)));
}
#endif /* !SS_MYSQL */


/*##**********************************************************************\
 * 
 *              rs_event_init
 * 
 * Creates a virtual event handle to a non-existent (new) event.
 * 
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      name - 
 *              
 *              
 *      schema - 
 *              
 *              
 *      catalog - 
 *              
 *              
 *      eventid - 
 *              
 *              
 *      paramcount - 
 *              
 *              
 *      paramtypes - 
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
rs_event_t* rs_event_init(
        void* cd,
        rs_entname_t* entname, /* name of the event */
        long eventid,
        int paramcount,
        int* paramtypes)
{
        rs_event_t* ev;
        SS_NOTUSED(cd);

        ss_dassert(entname != NULL);

        ev = SSMEM_NEW(rs_event_t);
        ev->ev_check = RSCHK_EVENTTYPE;
        ev->ev_nlink = 1;
        ev->ev_name = rs_entname_copy(entname);
        ev->ev_id = eventid;
        ev->ev_paramcount = paramcount;
        ev->ev_sem = rs_sysi_getrslinksem(cd);

        if (paramcount > 0) {
            ss_dassert(paramtypes != NULL);
            ev->ev_paramtypes = SsMemAlloc(ev->ev_paramcount * sizeof(int));
            memcpy(ev->ev_paramtypes, paramtypes, ev->ev_paramcount * sizeof(int));
        } else {
            ev->ev_paramtypes = NULL;
        }

        CHECK_EVENT(ev);
        return(ev);
}


/*##**********************************************************************\
 * 
 *              rs_event_done
 * 
 * Releases a event handle object
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      ev - in, take
 *              event handle
 *              
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_event_done(
        void*       cd __attribute__ ((unused)),
        rs_event_t* ev)
{
        CHECK_EVENT(ev);

        ss_dassert(ev->ev_name != NULL);

        SsSemEnter(ev->ev_sem);

        ss_dassert(ev->ev_nlink > 0);

        ev->ev_nlink--;

        if (ev->ev_nlink == 0) {

            /* Do the actual free.
             */
            SsSemExit(ev->ev_sem);

            rs_entname_done(ev->ev_name);
            if (ev->ev_paramtypes != NULL) {
                SsMemFree(ev->ev_paramtypes);
            }
            SsMemFree(ev);

        } else {
            SsSemExit(ev->ev_sem);
        }
}

/*##**********************************************************************\
 * 
 *              rs_event_link
 * 
 * Links the current user to the event handle. The handle is not
 * released until the link count is zero, i.e. everyone has called
 * rs_event_done.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      event - in out, use
 *              event handle
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_event_link(
        void*      cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);

        ss_dprintf_1(("%s: rs_event_done\n", __FILE__));
        CHECK_EVENT(ev);

        SsSemEnter(ev->ev_sem);

        ss_dassert(ev->ev_nlink > 0);

        ev->ev_nlink++;

        SsSemExit(ev->ev_sem);
}

/*##**********************************************************************\
 * 
 *              rs_event_eventid
 * 
 * Returns the id of the event
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      ev - in, use
 *              event handle
 *              
 * Return value : 
 * 
 *      View id
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ulong rs_event_eventid(
        void*       cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);
        return(ev->ev_id);
}

/*##**********************************************************************\
 * 
 *              rs_event_paramcount
 * 
 * Returns the paramcount of the event
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      ev - in, use
 *              event handle
 *              
 * Return value - int : 
 * 
 *      paramcount
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int rs_event_paramcount(
        void*       cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);
        return(ev->ev_paramcount);
}

int* rs_event_paramtypes(
        void*       cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);
        return(ev->ev_paramtypes);
}

/*##**********************************************************************\
 * 
 *              rs_event_name
 * 
 * Returns the name of the event handle
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *
 *      event - in, use
 *              event handle
 *              
 * Return value - ref : 
 * 
 *      Name of the event.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* rs_event_name(
        void*       cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);
        ss_dassert(ev->ev_name != NULL);
        return(rs_entname_getname(ev->ev_name));
}

/*##**********************************************************************\
 * 
 *              rs_event_entname
 * 
 * Returns entity name object of event.
 * 
 * Parameters : 
 * 
 *      cd - 
 *              
 *              
 *      event - 
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
rs_entname_t* rs_event_entname(
        void*       cd,
        rs_event_t* ev)
{
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);
        ss_dassert(ev->ev_name != NULL);
        return(ev->ev_name);
}

/*##**********************************************************************\
 * 
 *              rs_event_schema
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      event - in
 *              
 *              
 * Return value  - ref : 
 * 
 *      Schema of event, or NULL if no schema.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_event_schema(
        void*      cd,
        rs_event_t* ev)
{
        char* schema;
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);

        schema = rs_entname_getschema(ev->ev_name);
        return (schema);
}

/*##**********************************************************************\
 * 
 *              rs_event_catalog
 * 
 * 
 * 
 * Parameters : 
 * 
 *      cd - in
 *              
 *              
 *      event - in
 *              
 *              
 * Return value  - ref : 
 * 
 *      Catalog of event, or NULL if no catalog.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_event_catalog(
        void*      cd,
        rs_event_t* ev)
{
        char* catalog;
        SS_NOTUSED(cd);
        CHECK_EVENT(ev);

        catalog = rs_entname_getcatalog(ev->ev_name);
        return (catalog);
}

