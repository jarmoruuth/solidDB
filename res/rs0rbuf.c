/*************************************************************************\
**  source       * rs0rbuf.c
**  directory    * res
**  description  * Relation information buffering
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

This module implements a relation and view information buffer (rbuf).
The intention is that when database is openend (or created) the relation
and view information is read from data dictionary and stored into rbuf.

There are two ways to store the information in rbuf.
1. Insert only the name and id of the object
2. Insert the complete handle of the object

The information is always asked by object name. It is possible to check if
the object with given name is present in rbuf. A holding ownership to an
existing handle in rbuf can be achieved by "link" method.

The rbuf is implemented using Red-Black Tree utility.
rbdata_t objects, called nodes in documentation, are stored in RBTree.
Each node contains object name, id and handle. Each node is either view
or relation (= kind of the node).

Limitations:
-----------

Error handling:
--------------


Objects used:
------------

Red-Black Tree      <su0rbtr.h>
Relation handle     <rs0relh.h>
View handle         <rs0viewh.h>


Preconditions:
-------------


Multithread considerations:
--------------------------

The rbuf object is not protected against simultaneous operations.
However, there is already a mutex semaphore in rbuf structure but it
is never used.

Example:
-------

See trbuf.c

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssstring.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>

#include <su0rbtr.h>
#include <su0list.h>

#include "rs0types.h"
#include "rs0sdefs.h"
#include "rs0sdefs.h"
#include "rs0ttype.h"
#include "rs0relh.h"
#include "rs0viewh.h"
#include "rs0event.h"
#include "rs0rbuf.h"
#include "rs0sysi.h"
#include "rs0auth.h"
#include "rs0cardi.h"

typedef enum {
        RS_RBUF_ANY = 100,
        RS_RBUF_RELATION,
        RS_RBUF_RELATION_DROPCARDIN,
        RS_RBUF_VIEW,
        RS_RBUF_NAME,
        RS_RBUF_EVENT
} rs_rbuf_objtype_t;

struct relbufstruct {
        ss_debug(rs_check_t rb_check;) /* check field, RSCHK_RBUF */
        su_rbt_t*   rb_name_rbt;
        su_rbt_t*   rb_id_rbt;
        SsSemT*     rb_sem;
        rs_auth_t*  rb_auth;
        su_list_t*  rb_lru;         /* LRU of rbdata elements. */
        uint        rb_maxlru;      /* Max length of LRU list. */
        ulong       rb_searchcnt;   /* Counters to calculate ... */
        ulong       rb_foundcnt;    /* ... hit rate. */
        void        (*rb_resetcallback)(rs_sysi_t* cd, rs_rbuf_t* rbuf, bool unicode_enabled);
        bool        rb_unicode_enabled;
        bool        rb_recovery;
}; /* rs_rbuf_t; */

typedef struct {

        ss_debug(rs_check_t rd_check;)      /* check field, RSCHK_RBUFDATA */
        rs_rbuf_objtype_t   rd_objtype;     /* relation or view */
        rs_rbuf_nametype_t  rd_nametype;
        rs_entname_t*       rd_nodename;    /* relation or view name */
        long                rd_nodeid;      /* relation or view id */
        su_rbt_node_t*      rd_idrbtnode;
        su_list_node_t*     rd_lrunode;
        su_list_node_t      rd_lrunodebuf;
        bool                rd_infovalid;   /* TRUE, if the relh* or view
                                               definition is valid (==buffered) */
        rs_cardin_t*        rd_cardin;      /* Relation cardinality object,
                                               shared with rs_relh_t.*/
        rs_rbuf_t*          rd_rbuf;
        union {
            rs_relh_t*  relh;           /* relation handle */
            rs_viewh_t* viewh;          /* view handle */
            rs_event_t* event;          /* event handle */
            void*       nameh;          /* dummy name handle, always NULL */
        } rd_;
} rbdata_t;

typedef struct {
        rs_rbuf_objtype_t   ii_nodetype;
        long                ii_nodeid;
        rs_cardin_t*        ii_cardin;
        union {
            rs_relh_t*  relh;
            rs_viewh_t* viewh;
        } ii_;
} rbuf_iterinfo_t;

#define CHK_RBUF(rb)    ss_dassert(SS_CHKPTR(rb) && (rb)->rb_check == RSCHK_RBUF)

#define RBDATA_ISKIND(d, k) ((d != NULL) && (d)->rd_objtype == k)
#define RBDATA_ISREL(rd)    RBDATA_ISKIND(rd, RS_RBUF_RELATION)
#define RBDATA_ISVIEW(rd)   RBDATA_ISKIND(rd, RS_RBUF_VIEW)
#define RBDATA_ISNAME(rd)   RBDATA_ISKIND(rd, RS_RBUF_NAME)
#define RBDATA_ISEVENT(rd)  RBDATA_ISKIND(rd, RS_RBUF_EVENT)

#define RBDATA_USELRU(rd)   ((rd)->rd_nodeid >= RS_USER_ID_START && \
                             (rd)->rd_objtype != RS_RBUF_EVENT)

#define RBDATA_GETH(rd) (RBDATA_ISREL(rd) ? \
                            (void *)(rd)->rd_.relh :\
                            (void *)(rd)->rd_.viewh\
                        )

#define CHECK_RBDATA(rd) {\
                            ss_dassert(SS_CHKPTR(rd));\
                            ss_dassert((rd)->rd_check == RSCHK_RBUFDATA);\
                            ss_dassert(RBDATA_ISREL(rd) || RBDATA_ISVIEW(rd) ||\
                                       RBDATA_ISNAME(rd) || RBDATA_ISEVENT(rd));\
                         }

static rbdata_t* rbdata_init_kind(
        rs_sysinfo_t*       cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        long                nodeid,
        void*               handle,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type
);

static rs_rbuf_present_t rbuf_present_kind(
        void*         cd,
        rs_rbuf_t*    rbuf,
        rs_entname_t* nodename,
        void**        p_h,
        ulong*        p_id,
        rs_rbuf_objtype_t kind
);

static bool rbuf_removenode_kind_nomutex(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type
);

static bool rbuf_removenode_kind(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type
);

/* used by su0rbtr utilities */
static int rbdata_compare(
        void* rbdata1,
        void* rbdata2);

static int rbdata_idcompare(
        void* rbdata1,
        void* rbdata2);

static void rbdata_done(
        void* rbdata);

static bool rbuf_rbdata_insert(
        rs_rbuf_t* rbuf,
        rbdata_t* rbdata);

static rs_rbuf_present_t rbuf_rbdata_find(
        void* cd,
        rs_rbuf_t* rbuf,
        rs_entname_t* name,
        rs_rbuf_objtype_t nametype,
        su_rbt_node_t** p_node,
        rbdata_t** p_rbdata);

static bool rbuf_deletenode(
        rs_rbuf_t* rbuf,
        su_rbt_node_t* node);

/*#***********************************************************************\
 *
 *              rbdata_unbuffer
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      rbdata -
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
static void rbdata_unbuffer(
        void* cd,
        rs_rbuf_t* rbuf,
        rbdata_t* rbdata,
        bool dropcardin)
{
        ss_dprintf_3(("rbdata_unbuffer:nodename = '%s.%s.%s', dropcardin = %d\n",
            rs_entname_getprintcatalog(rbdata->rd_nodename),
            rs_entname_getprintschema(rbdata->rd_nodename),
            rs_entname_getname(rbdata->rd_nodename),
            dropcardin));
        ss_dassert(rbdata->rd_infovalid);

        if (rbdata->rd_lrunode != NULL) {
            su_list_remove_nodebuf(rbuf->rb_lru, &rbdata->rd_lrunodebuf);
            rbdata->rd_lrunode = NULL;
        } else {
            ss_dassert(!RBDATA_USELRU(rbdata));
        }

        rbdata->rd_infovalid = FALSE;
        if (RBDATA_ISREL(rbdata)) {
            SS_MEM_SETUNLINK(rbdata->rd_.relh);
            rs_relh_done(cd, rbdata->rd_.relh);
            rbdata->rd_.relh = NULL;
            if (dropcardin) {
                if (rbdata->rd_cardin != NULL) {
                    rs_cardin_done(cd, rbdata->rd_cardin);
                }
                rbdata->rd_cardin = rs_cardin_init(cd, rbdata->rd_nodeid);
                rs_cardin_setchanged(cd, rbdata->rd_cardin);
                ss_debug(rs_cardin_setcheck(rbdata->rd_cardin, rbdata->rd_nodename));
            }
        } else if (RBDATA_ISVIEW(rbdata)) {
#ifndef SS_NOVIEW
            rs_viewh_done(cd, rbdata->rd_.viewh);
#endif /* SS_NOVIEW */
            rbdata->rd_.viewh = NULL;
        } else if (RBDATA_ISEVENT(rbdata)) {
            rs_event_done(cd, rbdata->rd_.event);
            rbdata->rd_.event = NULL;
        }
}

/*#***********************************************************************\
 *
 *              rbuf_lruinsert
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      rbdata -
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
static void rbuf_lruinsert(
        void* cd,
        rs_rbuf_t* rbuf,
        rbdata_t* rbdata)
{
        ss_dprintf_3(("rbuf_lruinsert:nodename = '%s.%s.%s'\n",
            rs_entname_getprintcatalog(rbdata->rd_nodename),
            rs_entname_getprintschema(rbdata->rd_nodename),
            rs_entname_getname(rbdata->rd_nodename)));
        ss_dassert(rbdata->rd_lrunode == NULL);

        if (su_list_length(rbuf->rb_lru) >= rbuf->rb_maxlru) {
            rbdata_t* del_rbdata;
            su_list_node_t* node;

            node = su_list_last(rbuf->rb_lru);
            ss_dassert(node != NULL);
            del_rbdata = su_listnode_getdata(node);
            rbdata_unbuffer(cd, rbuf, del_rbdata, FALSE);
            ss_dprintf_4(("rbuf_lruinsert:too many nodes, remove node, nodename = '%s.%s.%s'\n",
                rs_entname_getprintcatalog(del_rbdata->rd_nodename),
                rs_entname_getprintschema(del_rbdata->rd_nodename),
                rs_entname_getname(del_rbdata->rd_nodename)));
        }

        if (RBDATA_USELRU(rbdata)) {
            rbdata->rd_lrunode = su_list_insertfirst_nodebuf(rbuf->rb_lru, &rbdata->rd_lrunodebuf, rbdata);
        }
}

/*#***********************************************************************\
 *
 *              rbuf_lruaccess
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      rbdata -
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
static void rbuf_lruaccess(
        void* cd,
        rs_rbuf_t* rbuf,
        rbdata_t* rbdata)
{
        ss_dprintf_3(("rbuf_lruaccess:nodename = '%s.%s.%s'\n",
            rs_entname_getprintcatalog(rbdata->rd_nodename),
            rs_entname_getprintschema(rbdata->rd_nodename),
            rs_entname_getname(rbdata->rd_nodename)));
        SS_NOTUSED(cd);
        ss_dassert(rbdata->rd_infovalid);

        if (RBDATA_USELRU(rbdata)) {
            ss_dassert(rbdata->rd_lrunode != NULL);
            su_list_remove_nodebuf(rbuf->rb_lru, &rbdata->rd_lrunodebuf);
            rbdata->rd_lrunode = su_list_insertfirst_nodebuf(rbuf->rb_lru, &rbdata->rd_lrunodebuf, rbdata);
        } else {
            ss_dassert(rbdata->rd_lrunode == NULL);
        }
}

/*#***********************************************************************\
 *
 *              rbdata_init_kind
 *
 * Allocates a new node of type kind
 *
 * Parameters :
 *
 *      cd - in
 *          not used
 *
 *      rbuf - in, use
 *              Pointer to rbuf
 *
 *      nodename - in
 *              relation/view name
 *
 *      nodeid - in
 *              relation/view id
 *
 *      handle - in, take
 *              if non-NULL, a valid rel/view handle
 *          if NULL, only nodename and id are stored in rbdata node
 *
 *      kind - in
 *              RS_RBUF_RELATION or RS_RBUF_VIEW or RS_RBUF_EVENT
 *
 * Return value - out, give:
 *
 *      Pointer into a newly allocated rbdata_t object
 *
 * Limitations  :
 *
 * Globals used :
 */
static rbdata_t* rbdata_init_kind(
        rs_sysinfo_t*       cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        long                nodeid,
        void*               handle,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type)
{
        rbdata_t* rbdata;

        ss_dassert(nodename != NULL);
        ss_dassert(nodeid != 0L);
        ss_dassert(rs_entname_getschema(nodename) != NULL);

        rbdata = SSMEM_NEW(rbdata_t);
        ss_debug(rbdata->rd_check = RSCHK_RBUFDATA;)
        rbdata->rd_objtype = kind;
        rbdata->rd_nametype = type;
        rbdata->rd_nodename = rs_entname_copy(nodename);
        rbdata->rd_nodeid = nodeid;
        rbdata->rd_lrunode = NULL;
        rbdata->rd_rbuf = rbuf;
        if (handle != NULL) {
            rbdata->rd_infovalid = TRUE;
            rbuf_lruinsert(cd, rbuf, rbdata);
        } else {
            rbdata->rd_infovalid = FALSE;
        }
        switch (kind) {
            case RS_RBUF_RELATION:
                if (handle != NULL) {
                    /* Adding a new relation and name to rbuf.
                     */
                    rbdata->rd_cardin = rs_relh_cardin(cd, handle);
                    ss_dassert(rbdata->rd_cardin != NULL);
                    rs_cardin_link(cd, rbdata->rd_cardin);
                } else {
                    rbdata->rd_cardin = NULL;
                }
                rbdata->rd_.relh = (rs_relh_t *)handle;
                break;
#ifndef SS_NOVIEW
            case RS_RBUF_VIEW:
                rbdata->rd_cardin = NULL;
                rbdata->rd_.viewh = (rs_viewh_t *)handle;
                break;
#endif /* SS_NOVIEW */
            case RS_RBUF_NAME:
                rbdata->rd_cardin = NULL;
                rbdata->rd_.nameh = NULL;
                break;
            case RS_RBUF_EVENT:
                rbdata->rd_cardin = NULL;
                rbdata->rd_.event = (rs_event_t *)handle;
                break;
            default:
                ss_error;
        }
        CHECK_RBDATA(rbdata);
        return(rbdata);
}

static void rbuf_rbdata_free(
        rs_rbuf_t*          rbuf,
        rbdata_t*           rbdata)
{
        CHECK_RBDATA(rbdata);

        if (rbdata->rd_lrunode != NULL) {
            su_list_remove_nodebuf(rbuf->rb_lru, &rbdata->rd_lrunodebuf);
            rbdata->rd_lrunode = NULL;
        }

        rbdata_done(rbdata);
}

void rbdata_replacenullcatalog(
        void* cd,
        rbdata_t* rbdata,
        char* newcatalog)
{
        char* old_catalog;
        char* schema;
        char* name;
        rs_entname_t* new_en;

        CHECK_RBDATA(rbdata);
        old_catalog = rs_entname_getcatalog(rbdata->rd_nodename);
        if (old_catalog != NULL) {
            return;
        }
        schema = rs_entname_getschema(rbdata->rd_nodename);
        name = rs_entname_getname(rbdata->rd_nodename);
        new_en = rs_entname_init(newcatalog,
                                 schema,
                                 name);
        rs_entname_done(rbdata->rd_nodename);
        rbdata->rd_nodename = new_en;
        switch (rbdata->rd_objtype) {
            case RS_RBUF_RELATION:
                if (rbdata->rd_.relh != NULL) {
                    rs_relh_setcatalog(cd,
                                       rbdata->rd_.relh,
                                       newcatalog);
                }
                break;
#ifndef SS_NOVIEW
            case RS_RBUF_VIEW:
                if (rbdata->rd_.viewh != NULL) {
                    rs_viewh_setcatalog(cd,
                                        rbdata->rd_.viewh,
                                        newcatalog);
                }
                break;
#endif /* SS_NOVIEW */
            case RS_RBUF_NAME:
                break;
            case RS_RBUF_EVENT:
                break;
            default:
                ss_error;
        }
}

void rs_rbuf_replacenullcatalogs(
        void* cd,
        rs_rbuf_t* rbuf,
        char* newcatalog)
{
        su_rbt_node_t* node;

        for (node = su_rbt_min(rbuf->rb_name_rbt, NULL);
             node != NULL;
             node = su_rbt_succ(rbuf->rb_name_rbt, node))
        {
            rbdata_t* rd = su_rbtnode_getkey(node);
            CHECK_RBDATA(rd);
            rbdata_replacenullcatalog(cd, rd, newcatalog);
        }
}

/*#***********************************************************************\
 *
 *              rbuf_present_kind
 *
 * Checks if a node with given name is present in rbuf.
 *
 * Parameters :
 *
 *      cd - in
 *          not used
 *
 *      rbuf - in, use
 *              Pointer to rbuf
 *
 *      nodename - in, use
 *              Name of the node to be searched
 *
 *      p_h - out, ref
 *              If non-NULL, a pointer to found object handle is stored
 *          in (*p_h) if the object is BUFFERED.
 *
 *      p_id - out
 *              If non-NULL, a pointer to found object id is stored
 *          in (*p_id) if the object EXISTS or is BUFFERED.
 *
 *
 *      kind - in, use
 *              One of RS_RBUF_RELATION or RS_RBUF_VIEW to specify the type of
 *          the object of interest.
 *
 * Return value :
 *       RSRBUF_NOTEXIST, Relation/view does not exist in rbuf
 *       RSRBUF_EXISTS,   Relation/view exists in rbuf but is not buffered.
 *                        *p_id is updated.
 *       RSRBUF_BUFFERED  Relation/view exists and is already buffered.
 *                        *p_h and *p_id are updated.
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_rbuf_present_t rbuf_present_kind(
        void*         cd,
        rs_rbuf_t*    rbuf,
        rs_entname_t* nodename,
        void**        p_h,
        ulong*        p_id,
        rs_rbuf_objtype_t kind
) {
        rbdata_t*           rbdata;
        su_rbt_node_t*      node;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(cd, rbuf, nodename, kind, &node, &rbdata);
        if (rp != RSRBUF_EXISTS && rp != RSRBUF_BUFFERED) {
            SsSemExit(rbuf->rb_sem);
            return(rp);
        }
        ss_dassert(rbdata != NULL);

        if (!RBDATA_ISKIND(rbdata, kind)) {

            SsSemExit(rbuf->rb_sem);
            return(RSRBUF_NOTEXIST);

        } else {

            CHECK_RBDATA(rbdata);
            rbuf->rb_searchcnt++;
            if (p_id) {
                *p_id = rbdata->rd_nodeid;
            }
            if (rbdata->rd_infovalid) {
                if (p_h) {
                    *p_h = RBDATA_GETH(rbdata);
                    if (RBDATA_ISKIND(rbdata, RS_RBUF_RELATION)) {
                        rs_relh_link(cd, *p_h);
                        SS_MEM_SETLINK(*p_h);
                    } else if (RBDATA_ISKIND(rbdata, RS_RBUF_VIEW)) {
#ifndef SS_NOVIEW
                        rs_viewh_link(cd, *p_h);
#endif /* SS_NOVIEW */
                    } else {
                        ss_dassert(RBDATA_ISKIND(rbdata, RS_RBUF_EVENT));
                        rs_event_link(cd, *p_h);
                    }
                }
                rbuf->rb_foundcnt++;
                rbuf_lruaccess(cd, rbuf, rbdata);
                SsSemExit(rbuf->rb_sem);
                return(RSRBUF_BUFFERED);
            } else {
                SsSemExit(rbuf->rb_sem);
                return(RSRBUF_EXISTS);
            }
        }
}

/*#***********************************************************************\
 *
 *              rbuf_removenode_kind_nomutex
 *
 * Same as rbuf_removenode_kind but not protected by rbuf mutex.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      nodename -
 *
 *
 *      kind -
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
static bool rbuf_removenode_kind_nomutex(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type
) {
        rbdata_t*           rbdata;
        su_rbt_node_t*      node = NULL;
        rs_rbuf_present_t   rp;

        ss_dprintf_3(("rbuf_removenode_kind_nomutex:name='%s.%s.%s',kind=%d\n",
            rs_entname_getprintcatalog(nodename),
            rs_entname_getprintschema(nodename),
            rs_entname_getname(nodename),
            (int)kind));
        SS_NOTUSED(cd);
        ss_dassert(rs_entname_getschema(nodename) != NULL);

        rp = rbuf_rbdata_find(cd, rbuf, nodename, kind, &node, &rbdata);
        if (rp != RSRBUF_EXISTS && rp != RSRBUF_BUFFERED) {
            return(FALSE);
        }

        if (RBDATA_ISKIND(rbdata, kind) &&
            (type == RSRBUF_NAME_GENERIC ||
             type == rbdata->rd_nametype)) {
            bool b;
            ss_dprintf_4(("rbuf_removenode_kind_nomutex:found\n"));
            CHECK_RBDATA(rbdata);
            if (rbdata->rd_nodeid  > 0 && rbdata->rd_nodeid < RS_SYS_SQL_ID_START) {
                ss_dprintf_4(("rbuf_removenode_kind_nomutex:NOP, fixed system table\n"));
                return(TRUE);
            }
            if (rbdata->rd_infovalid) {
                rbdata_unbuffer(cd, rbuf, rbdata, FALSE);
            }
            if (rbdata->rd_cardin != NULL) {
                rs_cardin_done(cd, rbdata->rd_cardin);
            }
            b = rbuf_deletenode(rbuf, node);
            ss_dassert(b);
            return(TRUE);
        } else {
            ss_dprintf_4(("rbuf_removenode_kind_nomutex:not found\n"));
            return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              rbuf_namebyid_kind
 *
 * Returns the name with id, if found from the buffer.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      id - in
 *              id that is searched from the rbuf
 *
 *      p_name - out, give
 *              name is allocated and returned here
 *
 *      kind -
 *
 *
 * Return value :
 *
 *      TRUE    - id found and name stored to *p_name
 *      FALSE   - id not found
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool rbuf_namebyid_kind(
        void*               cd,
        rs_rbuf_t*          rbuf,
        long                id,
        rs_entname_t**      p_name,
        rs_relh_t**         p_relh,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type)
{
        su_rbt_node_t*  node;
        rbdata_t search_rd;

        ss_dprintf_1(("rbuf_namebyid_kind:id=%ld, objtype=%d, nametype=%d\n", id, kind, type));
        SS_NOTUSED(cd);
        SS_PUSHNAME("rbuf_namebyid_kind");

        search_rd.rd_objtype = kind;
        search_rd.rd_nodeid = id;
        search_rd.rd_rbuf = rbuf;

        SsSemEnter(rbuf->rb_sem);

        node = su_rbt_search(rbuf->rb_id_rbt, &search_rd);

        if (node != NULL) {
            rbdata_t* rd;

            rd = su_rbtnode_getkey(node);
            CHECK_RBDATA(rd);
            ss_dassert(rd->rd_nodeid == id);
            ss_dassert(rd->rd_objtype == kind);

            ss_dprintf_2(("rbuf_namebyid_kind:found nametype=%d\n", rd->rd_nametype));

            if (type == RSRBUF_NAME_GENERIC || type == rd->rd_nametype) {
                /* Found. */
                ss_dprintf_2(("rbuf_namebyid_kind:return TRUE\n"));
                if (p_relh != NULL) {
                    ss_dassert(kind == RS_RBUF_RELATION);
                    ss_dassert(type == RSRBUF_NAME_RELATION);
                    ss_dassert(RBDATA_ISKIND(rd, RS_RBUF_RELATION));
                    if (rd->rd_infovalid) {
                        *p_relh = RBDATA_GETH(rd);
                        *p_name = NULL;
                        rs_relh_link(cd, *p_relh);
                        SS_MEM_SETLINK(*p_relh);
                        rbuf->rb_foundcnt++;
                        rbuf_lruaccess(cd, rbuf, rd);
                    } else {
                        *p_relh = NULL;
                        *p_name = rs_entname_copy(rd->rd_nodename);
                    }
                } else {
                    *p_name = rs_entname_copy(rd->rd_nodename);
                }
                SsSemExit(rbuf->rb_sem);
                SS_POPNAME;
                return(TRUE);
            }
        }
        ss_dprintf_2(("rbuf_namebyid_kind:return FALSE\n"));
        SsSemExit(rbuf->rb_sem);
        SS_POPNAME;
        return(FALSE);
}

/*#***********************************************************************\
 *
 *              rbuf_removenode_kind
 *
 * Removes a node from rbuf.
 * Called by rs_rbuf_remove[viewh|relh].
 *
 * Parameters :
 *
 *      cd - in, use
 *          not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf
 *
 *      nodename - in, use
 *              Name of the node to be removed
 *
 *      kind - in, use
 *              One of RS_RBUF_RELATION or RS_RBUF_VIEW to specify the type of
 *          the node of interest.
 *
 * Return value:
 *      TRUE    - handle found and removed
 *      FALSE   - handle not found
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool rbuf_removenode_kind(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       nodename,
        rs_rbuf_objtype_t   kind,
        rs_rbuf_nametype_t  type
) {
        bool succp;

        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        succp = rbuf_removenode_kind_nomutex(cd, rbuf, nodename, kind, type);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*#***********************************************************************\
 *
 *              rbuf_unbuffer_kind
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      nodename -
 *
 *
 *      kind -
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
static bool rbuf_unbuffer_kind(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   nodename,
        rs_rbuf_objtype_t   kind,
        bool                dropcardin
) {
        bool                foundp;
        rbdata_t*           rbdata;
        su_rbt_node_t*      node = NULL;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);
        ss_dassert(rs_entname_getschema(nodename) != NULL);

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(cd, rbuf, nodename, kind, &node, &rbdata);
        if (rp != RSRBUF_EXISTS && rp != RSRBUF_BUFFERED) {
            SsSemExit(rbuf->rb_sem);
            return(FALSE);
        }

        if (RBDATA_ISKIND(rbdata, kind)) {
            CHECK_RBDATA(rbdata);
            if (rbdata->rd_infovalid) {
                rbdata_unbuffer(cd, rbuf, rbdata, dropcardin);
            }
            foundp = TRUE;
        } else {
            foundp = FALSE;
        }
        SsSemExit(rbuf->rb_sem);
        return(foundp);
}

/*#***********************************************************************\
 *
 *              rbdata_done
 *
 * Deletes a rbdata_t object (= rbuf node).
 * Does not touch the relh or viewdef inside.
 * Called by su0rbtree
 *
 * Parameters :
 *
 *      rbdata - in, take
 *              pointer to rbdata_t object to be freed
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void rbdata_done(void* rbdata)
{
        rbdata_t* rbd = rbdata;
        CHECK_RBDATA(rbd);
        ss_dassert(rbd->rd_nodename != NULL);
        rs_entname_done(rbd->rd_nodename);
        SsMemFree(rbd);
}

/*#***********************************************************************\
 *
 *              rbdata_compare
 *
 * Compares nodename fields of two rbdata_t objects.
 * Called by su0rbtree
 *
 * Parameters :
 *
 *      searched_rbdata - in, use
 *              node contaning searched name in nodename field
 *
 *      rbdata - in, use
 *              node in rbtree
 *
 * Return value :
 *          Return code of strcmp
 *
 * Limitations  :
 *
 * Globals used :
 */
static int rbdata_compare(void* searched_rbdata, void* rbdata)
{
        int cmp;
        rbdata_t* searched_rd = searched_rbdata;
        rbdata_t* rd = rbdata;

        ss_dassert(searched_rd->rd_nodename != NULL);
        ss_dassert(rd->rd_nodename != NULL);
        CHK_RBUF(rd->rd_rbuf);
        ss_dassert(searched_rd->rd_objtype >= RS_RBUF_ANY && searched_rd->rd_objtype <= RS_RBUF_EVENT);
        ss_dassert(rd->rd_objtype >= RS_RBUF_ANY && rd->rd_objtype <= RS_RBUF_EVENT);

        cmp = rs_entname_compare(searched_rd->rd_nodename, rd->rd_nodename);
        if (cmp == 0 && rd->rd_rbuf->rb_recovery) {
            ss_dassert(searched_rd->rd_objtype > RS_RBUF_ANY);
            ss_dassert(rd->rd_objtype > RS_RBUF_ANY);
            cmp = su_rbt_long_compare((long)rd->rd_objtype, (long)searched_rd->rd_objtype);
        }
        return(cmp);
}

/*#***********************************************************************\
 *
 *              rbdata_idcompare
 *
 * Compares id fields of two rbdata_t objects.
 * Called by su0rbtree
 *
 * Parameters :
 *
 *      searched_rbdata - in, use
 *              node contaning searched id in nodeid field
 *
 *      rbdata - in, use
 *              node in rbtree
 *
 * Return value :
 *          Return code of id compare
 *
 * Limitations  :
 *
 * Globals used :
 */
static int rbdata_idcompare(void* searched_rbdata, void* rbdata)
{
        rbdata_t* searched_rd = searched_rbdata;
        rbdata_t* rd = rbdata;
        int cmp;

        cmp = su_rbt_long_compare(searched_rd->rd_nodeid, rd->rd_nodeid);
        if (cmp == 0) {
            cmp = su_rbt_long_compare(searched_rd->rd_objtype, rd->rd_objtype);
        }
        return(cmp);
}

/*#***********************************************************************\
 *
 *              rbuf_rbdata_insert
 *
 * Inserts a new node to rbuf.
 *
 * Parameters :
 *
 *      rbuf - in, use
 *          Pointer to rbuf object
 *
 *      rbdata - in, take
 *              Pointer to rbdata object
 *
 * Return value :
 *      TRUE, succeeded
 *      FALSE, failed
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool rbuf_rbdata_insert(rs_rbuf_t* rbuf, rbdata_t* rbdata)
{
        su_rbt_node_t* node;

        CHECK_RBDATA(rbdata);
        SS_PUSHNAME("rbuf_rbdata_insert");
        ss_dprintf_1(("rs_rbuf_addviewname:%s.%s.%s objtype=%d\n",
            rs_entname_getprintcatalog(rbdata->rd_nodename),
            rs_entname_getprintschema(rbdata->rd_nodename),
            rs_entname_getname(rbdata->rd_nodename),
            rbdata->rd_objtype));

        node = su_rbt_insert2(rbuf->rb_name_rbt, rbdata);
        ss_dassert(node != NULL);
        if (node == NULL) {
            SS_POPNAME;
            return(FALSE);
        }
        if (rbdata->rd_nodeid > 0) {
            rbdata->rd_idrbtnode = su_rbt_insert2(rbuf->rb_id_rbt, rbdata);
#if !(defined(SS_MYSQL) || defined(SS_MYSQL_AC))
            ss_dassert(rbdata->rd_idrbtnode != NULL);
#endif
            if (rbdata->rd_idrbtnode == NULL) {
                su_rbt_delete_nodatadel(rbuf->rb_name_rbt, node);
                SS_POPNAME;
                return(FALSE);
            }
        } else {
            rbdata->rd_idrbtnode = NULL;
        }
        SS_POPNAME;
        return(TRUE);
}

/*#***********************************************************************\
 *
 *              rbuf_rbdata_find
 *
 * Finds a data node from rbuf using name as a key.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data contaxt
 *
 *      rbuf - in, use
 *              rel info buffer
 *
 *      name - in, use
 *              name to be used as a search key
 *
 *      p_node - out, ref
 *              if !NULL, a pointer to a tree node in rbtree
 *
 * Return value :
 *
 *      If not NULL, Pointer to rbdata_t object
 *      NULL, if not found
 *
 * Limitations  :
 *
 * Globals used :
 */
static rs_rbuf_present_t rbuf_rbdata_find(
        void* cd,
        rs_rbuf_t* rbuf,
        rs_entname_t* name,
        rs_rbuf_objtype_t nametype,
        su_rbt_node_t** p_node,
        rbdata_t** p_rbdata)
{
        rs_rbuf_present_t rc;
        int count = 0;
        int cmp;
        rbdata_t rd;
        rs_entname_t* tmp_name = NULL;

        ss_dassert(name != NULL);
        ss_dassert(p_node != NULL);

        rd.rd_nodename = name;
        rd.rd_objtype = nametype;
        rd.rd_rbuf = rbuf;

        if (rs_entname_getcatalog(name) == NULL) {
            char* defcat;
            rs_auth_t* auth;

            if (cd != NULL &&
                (auth = rs_sysi_auth(cd)) != NULL)
            {
                ss_dassert(auth != NULL);
                defcat = rs_auth_catalog(cd, auth);
            } else {
                defcat = RS_AVAL_DEFCATALOG;
            }
            if (defcat != NULL) {
                tmp_name = rs_entname_init(defcat,
                                           rs_entname_getschema(name),
                                           rs_entname_getname(name));
                name = tmp_name;
            }
        }
        if (rs_entname_getschema(name) != NULL) {
            *p_node = su_rbt_search(rbuf->rb_name_rbt, &rd);
        } else {
            su_rbt_node_t* node2;
            rbdata_t* rd2;

            node2 = su_rbt_search_atleast(rbuf->rb_name_rbt, &rd);
            if (node2 == NULL) {
                /* Found nothing. */
                goto notexist;
            }
            rd2 = su_rbtnode_getkey(node2);
            cmp = rs_entname_comparenames(name, rd2->rd_nodename);
            if (cmp != 0) {
                /* Different name. */
                goto notexist;
            }
            if (nametype == RS_RBUF_ANY || RBDATA_ISKIND(rd2, nametype)) {
                count++;
            }
            *p_node = node2;
            while ((node2 = su_rbt_succ(rbuf->rb_name_rbt, node2)) != NULL) {
                rd2 = su_rbtnode_getkey(node2);
                cmp = rs_entname_comparenames(name, rd2->rd_nodename);
                if (cmp != 0) {
                    /* Different name, end of search. */
                    break;
                }
                if (nametype != RS_RBUF_ANY && !RBDATA_ISKIND(rd2, nametype)) {
                    /* Skip this name. */
                    continue;
                }
                if (count > 0) {
                    /* Duplicate name, return failure. */
                    goto ambiguous;
                } else {
                    /* Same name and type for the first time. */
                    *p_node = node2;
                    count++;
                }
            }
        }

        if (*p_node != NULL) {
            *p_rbdata = su_rbtnode_getkey(*p_node);
            if (nametype == RS_RBUF_ANY || RBDATA_ISKIND(*p_rbdata, nametype)) {
                goto exist;
            } else {
                goto notexist;
            }
        } else {
            goto notexist;
        }
 exist:;
        rc = RSRBUF_EXISTS;
        goto return_rc;
 ambiguous:;
        rc = RSRBUF_AMBIGUOUS;
        goto return_rc;
 notexist:;
        rc = RSRBUF_NOTEXIST;
        goto return_rc;
 return_rc:;
        if (tmp_name != NULL) {
            rs_entname_done(tmp_name);
        }
        return (rc);

}

/*#***********************************************************************\
 *
 *              rbuf_deletenode
 *
 * Deletes a node from rbuf. The input parameter is returned by
 * rbuf_rbdata_find().
 *
 * Parameters :
 *
 *      rbuf - in out, use
 *          Pointer to rbuf
 *
 *      node -
 *              Node in rbtree
 *
 * Return value :
 *
 *      TRUE, if successfull
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool rbuf_deletenode(rs_rbuf_t* rbuf, su_rbt_node_t* node)
{
        rbdata_t* rd;

        rd = su_rbtnode_getkey(node);
        CHECK_RBDATA(rd);

        if (rd->rd_idrbtnode != NULL) {
            rbdata_t* rd2;

            rd2 = su_rbt_delete(rbuf->rb_id_rbt, rd->rd_idrbtnode);
            ss_dassert(rd == rd2);
            CHECK_RBDATA(rd);
        }

        rd = su_rbt_delete(rbuf->rb_name_rbt, node);
        if (rd == NULL) {
            /* The su_rbt_delete calls rbdata_done when removing a node from
               rbtree.  So, a NULL is returned if everything was OK. */
            return(TRUE);
        }  else {
            ss_derror;
            return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              rbuf_init_buf
 *
 * Inits rbuf partially using pointer to allocated rbuf.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
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
static void rbuf_init_buf(
        void*      cd __attribute__ ((unused)),
        rs_rbuf_t* rbuf
) {
        CHK_RBUF(rbuf);

        rbuf->rb_name_rbt = su_rbt_init(rbdata_compare, rbdata_done);
        rbuf->rb_id_rbt = su_rbt_init(rbdata_idcompare, NULL);
        rbuf->rb_lru = su_list_init(NULL);
}

/*#***********************************************************************\
 *
 *              rbuf_done_buf
 *
 * Clears rbuf content but does not release the buffer or reset other
 * variables.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
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
static void rbuf_done_buf(
        void*      cd,
        rs_rbuf_t* rbuf
) {
        su_rbt_t*       rbt;
        su_rbt_node_t*  node;
        rbdata_t*       rbdata;

        CHK_RBUF(rbuf);

        rbt = rbuf->rb_name_rbt;

        su_list_done_nodebuf(rbuf->rb_lru);

        node = su_rbt_min(rbt, NULL);
        while (node != NULL) {
            rbdata = su_rbtnode_getkey(node);
            CHECK_RBDATA(rbdata);
#ifdef SS_DEBUG
            {
                su_rbt_node_t* idnode;

                idnode = su_rbt_search(rbuf->rb_id_rbt, rbdata);
                if (idnode == NULL) {
                    ss_dassert(rbdata->rd_idrbtnode == NULL);
                    ss_dassert(rbdata->rd_nodeid <= 0);
                } else {
                    ss_dassert(rbdata->rd_idrbtnode != NULL);
                    ss_dassert(idnode == rbdata->rd_idrbtnode);
                }
            }
#endif /* SS_DEBUG */
            if (rbdata->rd_cardin != NULL) {
                rs_cardin_done(cd, rbdata->rd_cardin);
            }
            if (rbdata->rd_infovalid) {
                if (RBDATA_ISREL(rbdata)) {
                    SS_MEM_SETUNLINK(RBDATA_GETH(rbdata));
                    rs_relh_done(cd, RBDATA_GETH(rbdata));
                } else if (RBDATA_ISVIEW(rbdata)) {
#ifndef SS_NOVIEW
                    rs_viewh_done(cd, RBDATA_GETH(rbdata));
#endif /* SS_NOVIEW */
                } else if (RBDATA_ISEVENT(rbdata)) {
                    rs_event_done(cd, RBDATA_GETH(rbdata));
                } else {
                    ss_error;
                }
            }
            node = su_rbt_succ(rbt, node);
        }
        su_rbt_done(rbuf->rb_name_rbt);
        su_rbt_done(rbuf->rb_id_rbt);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_init
 *
 * Initializes a rbuf object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      auth - in, use
 *              authorization, not used
 *
 * Return value - out, give:
 *      Pointer to a new rbuf object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_t* rs_rbuf_init(
        void*      cd,
        rs_auth_t* auth
) {
        rs_rbuf_t* rbuf;

        SS_NOTUSED(cd);
        rbuf = SSMEM_NEW(rs_rbuf_t);
        ss_debug(rbuf->rb_check = RSCHK_RBUF;)

        rbuf_init_buf(cd, rbuf);

        rbuf->rb_sem = SsSemCreateLocal(SS_SEMNUM_RES_RBUF);
        rbuf->rb_auth = auth;
        rbuf->rb_maxlru = UINT_MAX;
        rbuf->rb_searchcnt = 0;
        rbuf->rb_foundcnt = 0;
        rbuf->rb_resetcallback = 0;
        rbuf->rb_unicode_enabled = TRUE;
        rbuf->rb_recovery = FALSE;

        CHK_RBUF(rbuf);

        return(rbuf);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_done
 *
 * Frees resources allocated for rbuf object.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data, not used
 *
 *      rbuf - in, take
 *              Pointer to rbuf object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_rbuf_done(
        void*      cd,
        rs_rbuf_t* rbuf
) {
        CHK_RBUF(rbuf);

        SsSemFree(rbuf->rb_sem);

        rbuf_done_buf(cd, rbuf);

        SsMemFree(rbuf);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_init_replace
 *
 * Inits a new rbuf object for rs_rbuf_replace call. Before rs_rbuf_replace
 * the rbuf should be loaded with new content.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      old_rbuf -
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
rs_rbuf_t* rs_rbuf_init_replace(
        void*      cd,
        rs_rbuf_t* old_rbuf
) {
        rs_rbuf_t* new_rbuf;

        ss_dprintf_1(("rs_rbuf_init_replace\n"));


        new_rbuf = SSMEM_NEW(rs_rbuf_t);
        ss_debug(new_rbuf->rb_check = RSCHK_RBUF;)

        *new_rbuf = *old_rbuf;

        rbuf_init_buf(cd, new_rbuf);
        new_rbuf->rb_sem = SsSemCreateLocal(SS_SEMNUM_RES_RBUF);

        CHK_RBUF(new_rbuf);

        if (new_rbuf->rb_resetcallback != 0) {
            (*new_rbuf->rb_resetcallback)(cd, new_rbuf, new_rbuf->rb_unicode_enabled);
        }

        return(new_rbuf);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_replace
 *
 * Replaces rbuf content from source_rbuf. Replace means info in source_rbuf
 * is put to target_rbuf and old content of target_rbuf is released.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      target_rbuf - in, use
 *
 *
 *      source_rbuf - in, take
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
void rs_rbuf_replace(
        void*      cd,
        rs_rbuf_t* target_rbuf,
        rs_rbuf_t* source_rbuf
) {
        rs_rbuf_t tmp_rbuf;
        su_rbt_node_t* node;

        ss_dprintf_1(("rs_rbuf_replace\n"));
        CHK_RBUF(target_rbuf);
        CHK_RBUF(source_rbuf);

        SsSemEnter(target_rbuf->rb_sem);

        /* Switch content of target and source.
         */
        tmp_rbuf.rb_name_rbt = target_rbuf->rb_name_rbt;
        tmp_rbuf.rb_id_rbt = target_rbuf->rb_id_rbt;
        tmp_rbuf.rb_lru = target_rbuf->rb_lru;

        target_rbuf->rb_name_rbt = source_rbuf->rb_name_rbt;
        target_rbuf->rb_id_rbt = source_rbuf->rb_id_rbt;
        target_rbuf->rb_lru = source_rbuf->rb_lru;

        source_rbuf->rb_name_rbt = tmp_rbuf.rb_name_rbt;
        source_rbuf->rb_id_rbt = tmp_rbuf.rb_id_rbt;
        source_rbuf->rb_lru = tmp_rbuf.rb_lru;

        node = su_rbt_min(target_rbuf->rb_name_rbt, NULL);
        while (node != NULL) {
            rbdata_t* rd;
            rd = su_rbtnode_getkey(node);
            rd->rd_rbuf = target_rbuf;
            node = su_rbt_succ(target_rbuf->rb_name_rbt, node);
        }

        rs_rbuf_done(cd, source_rbuf);

        CHK_RBUF(target_rbuf);

        SsSemExit(target_rbuf->rb_sem);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_setresetcallback
 *
 * Sets callback function that should be called when rbuf reset is done.
 * Callback function should store initial values (like system table info)
 * to rbuf.
 *
 * Parameters :
 *
 *      cd - in
 *          Client data.
 *
 *      rbuf - in, use
 *          Pointer to rbuf.
 *
 *      resetcallback - in, hold
 *          Callback function.
 *
 *      unicode_enabled - in
 *          If TRUE, unicode is enabled in system tables.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_rbuf_setresetcallback(
        void*      cd __attribute__ ((unused)),
        rs_rbuf_t* rbuf,
        void       (*resetcallback)(rs_sysi_t* cd, rs_rbuf_t* rbuf, bool unicode_enabled),
        bool       unicode_enabled
) {
        ss_dprintf_1(("rs_rbuf_setresetcallback\n"));
        CHK_RBUF(rbuf);

        rbuf->rb_resetcallback = resetcallback;
        rbuf->rb_unicode_enabled = unicode_enabled;
}

/*##**********************************************************************\
 *
 *              rs_rbuf_setmaxbuffered
 *
 * Sets maximum number of buffered relation and view handles. If more
 * than maxbuffered handles are added to the rbuf, Other handles are
 * removed using LRU replacing algorithm. By default all handles are
 * buffered.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      rbuf - use
 *
 *
 *      maxbuffered - in
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
void rs_rbuf_setmaxbuffered(
        void*      cd __attribute__ ((unused)),
        rs_rbuf_t* rbuf,
        uint       maxbuffered)
{
        ss_dprintf_1(("rs_rbuf_setmaxbuffered:maxbuffered=%u\n", maxbuffered));
        CHK_RBUF(rbuf);

        rbuf->rb_maxlru = maxbuffered;
}

/*##**********************************************************************\
 * 
 *              rs_rbuf_setrecovery
 * 
 * Sets recovery mode on or off. This setting affect on name 
 * resolution scope. With recovery mode enabled the name resolution
 * is more strict and we expect that all changes are already validated.
 * 
 * Parameters : 
 * 
 *              cd - 
 *                      
 *                      
 *              rbuf - 
 *                      
 *                      
 *              isrecovery - 
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
void rs_rbuf_setrecovery(
        void*      cd __attribute__ ((unused)),
        rs_rbuf_t* rbuf,
        bool       isrecovery)
{
        ss_dprintf_1(("rs_rbuf_setrecovery:isrecovery=%d\n", isrecovery));
        CHK_RBUF(rbuf);

        SsSemEnter(rbuf->rb_sem);
        rbuf->rb_recovery = isrecovery;
        SsSemExit(rbuf->rb_sem);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_addname
 *
 * Adds a new, unique name to relation buffer.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      name -
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
bool rs_rbuf_addname(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       name,
        rs_rbuf_nametype_t  type,
        long                id
) {
        su_rbt_node_t*      node;
        rbdata_t*           rbdata;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_addname:%s.%s.%s\n",
            rs_entname_getprintcatalog(name),
            rs_entname_getprintschema(name),
            rs_entname_getname(name)));
        ss_dassert(rs_entname_getschema(name) != NULL);

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                name, 
                rbuf->rb_recovery ? RS_RBUF_NAME : RS_RBUF_ANY, 
                &node, 
                &rbdata);
        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {
            SsSemExit(rbuf->rb_sem);
            return(FALSE);

        } else {
            bool b;
            rbdata_t* rbdata = rbdata_init_kind(
                                    cd,
                                    rbuf,
                                    name,
                                    id,
                                    NULL,
                                    RS_RBUF_NAME,
                                    type
                               );
            CHECK_RBDATA(rbdata);
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (!b) {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            SsSemExit(rbuf->rb_sem);
            return(b);
        }
}

/*##**********************************************************************\
 *
 *              rs_rbuf_removename
 *
 * Removes a name object from relation buffer.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      name -
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
bool rs_rbuf_removename(
        void*               cd,
        rs_rbuf_t*          rbuf,
        rs_entname_t*       name,
        rs_rbuf_nametype_t  type
) {
        ss_dprintf_1(("rs_rbuf_removename:%s.%s.%s\n",
            rs_entname_getprintcatalog(name),
            rs_entname_getprintschema(name),
            rs_entname_getname(name)));
        return(rbuf_removenode_kind(cd, rbuf, name, RS_RBUF_NAME, type));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_namebyid
 *
 * Finds a name object of given type based on name id.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      id -
 *
 *
 *      type -
 *
 *
 *      p_name -
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
bool rs_rbuf_namebyid(
        void*               cd,
        rs_rbuf_t*          rbuf,
        ulong               id,
        rs_rbuf_nametype_t  type,
        rs_entname_t**      p_name)
{
        return(rbuf_namebyid_kind(cd, rbuf, id, p_name, NULL, RS_RBUF_NAME, type));
}
/*##**********************************************************************\
 *
 *              rs_rbuf_nameinuse
 *
 * Finds any kind of name from rbuf and returns whether it is in use or
 * not
 *
 * Parameters :
 *
 *      cd - in, use
 *                  client data
 *
 *      rbuf - in, use
 *                  rbuf object
 *
 *      name - in, use
 *              entity name
 *
 * Return value :
 *      TRUE - name in use
 *      FALSE - name not in use
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_rbuf_nameinuse(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   entname)
{
        rbdata_t*           rbdata;
        su_rbt_node_t*      node;
        rs_rbuf_present_t   rp;
        bool foundp;

        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(cd, rbuf, entname, RS_RBUF_ANY, &node, &rbdata);
        if (rp == RSRBUF_NOTEXIST) {
            foundp = FALSE;
        } else {
            ss_dassert(rbdata != NULL);
            foundp = TRUE;
        }
        SsSemExit(rbuf->rb_sem);
        return (foundp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_removeallnames
 *
 * Removes all NAME type objects from rbuf. Note that relation and view
 * names are not removed.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
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
void rs_rbuf_removeallnames(
        void*       cd,
        rs_rbuf_t*  rbuf
) {
        bool succp;
        su_rbt_node_t* node;
        su_list_t* list;
        su_list_node_t* n;
        rs_entname_t* nodename;

        ss_dprintf_1(("rs_rbuf_removeallnames\n"));
        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        list = su_list_init(NULL);

        /* Find all NAME objects and add them to list.
         */
        node = su_rbt_min(rbuf->rb_name_rbt, NULL);
        while (node != NULL) {
            rbdata_t* rd;

            rd = su_rbtnode_getkey(node);
            CHECK_RBDATA(rd);

            switch (rd->rd_objtype) {
                case RS_RBUF_RELATION:
                case RS_RBUF_VIEW:
                case RS_RBUF_EVENT:
                    break;
                case RS_RBUF_NAME:
                    su_list_insertlast(list, rd->rd_nodename);
                    break;
                default:
                    ss_error;
            }
            node = su_rbt_succ(rbuf->rb_name_rbt, node);
        }

        /* Delete all NAME objects found from list.
         */
        su_list_do_get(list, n, nodename) {
            succp = rbuf_removenode_kind_nomutex(
                        cd,
                        rbuf,
                        nodename,
                        RS_RBUF_NAME,
                        RSRBUF_NAME_GENERIC);
            ss_dassert(succp);
        }

        su_list_done(list);

        SsSemExit(rbuf->rb_sem);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_relpresent
 *
 * Checks if the relation with given name is present in rbuf
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in, use
 *              rbuf object
 *
 *      relname - in, use
 *              relation name
 *
 *      p_relh - out, ref
 *              if p_relh is non-NULL and return value is RSRBUF_BUFFERED,
 *          a pointer to relh in rbuf is stored in *p_relh
 *
 *      p_relid - out
 *              if p_relid is non-NULL and return value is RSRBUF_BUFFERED,
 *          or RSRBUF_EXISTS the relation id is stored in *p_relid
 *
 * Return value :
 *
 *      RSRBUF_EXISTS    Relation exists, but relh is not buffered
 *      RSRBUF_NOTEXIST, Relation does not exist in rbuf
 *      RSRBUF_BUFFERED  Rel exists and relh is buffered
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_present_t rs_rbuf_relpresent(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        rs_relh_t**     p_relh,
        ulong*          p_relid
) {
        ss_dprintf_1(("rs_rbuf_relpresent:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));
        return(rbuf_present_kind(
                    cd,
                    rbuf,
                    relname,
                    (void**)p_relh,
                    p_relid,
                    RS_RBUF_RELATION)
              );
}

/*##**********************************************************************\
 *
 *              rs_rbuf_relnamebyid
 *
 * Returns the relname with id relid, if found from the buffer.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relid - in
 *              relation id that is searched from the rbuf
 *
 *      p_relname - out, give
 *              relation name is allocated and returned here
 *
 * Return value :
 *
 *      TRUE    - relid found and relname stored to *p_relname
 *      FALSE   - relid not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_relnamebyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           relid,
        rs_entname_t**  p_relname)
{
        ss_dprintf_1(("rs_rbuf_relnamebyid:relid=%ld\n", relid));
        return(rbuf_namebyid_kind(cd, rbuf, relid, p_relname, NULL, RS_RBUF_RELATION, RSRBUF_NAME_RELATION));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_relhbyid
 *
 * Returns the relation handle or relation name name with id relid, 
 * if found from the buffer.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relid - in
 *              relation id that is searched from the rbuf
 *
 *      p_relh - out, give
 *              relation handle is allocated and returned here. if relh
 *      is found then *p_relh is not NULL and *p_relname is set
 *      to NULL
 *
 *      p_relname - out, give
 *              relation name is allocated and returned here if id is found
 *      but relh is not valid. then *p_relhis set to NULL
 *
 * Return value :
 *
 *      TRUE    - relid found and relname stored to *p_relname
 *      FALSE   - relid not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_relhbyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           relid,
        rs_relh_t**     p_relh,
        rs_entname_t**  p_relname)
{
        return(rbuf_namebyid_kind(cd, rbuf, relid, p_relname, p_relh, RS_RBUF_RELATION, RSRBUF_NAME_RELATION));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_insertrelh_ex_nomutex
 *
 * Same as rs_rbuf_insertrelh_ex but does not enter mutex.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relh - in, take
 *              relation handle
 *
 * Return value :
 *
 *      RSRBUF_SUCCESS, succeeded
 *      RSRBUF_ERR_EXISTS, if relation (or view) with same name existed in rbuf
 *      RSRBUF_ERR_INVALID_ARG, if relation id does not match
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_ret_t rs_rbuf_insertrelh_ex_nomutex(
        void*      cd,
        rs_rbuf_t* rbuf,
        rs_relh_t* relh
) {
        su_rbt_node_t*      node;
        rs_entname_t*       relname;
        rbdata_t*           rbdata;
        rs_rbuf_present_t   rp;

        ss_dassert(relh != NULL);

        relname = rs_relh_entname(cd, relh);

        ss_dprintf_1(("rs_rbuf_insertrelh_ex_nomutex:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                relname, 
                rbuf->rb_recovery ? RS_RBUF_RELATION : RS_RBUF_ANY, 
                &node, 
                &rbdata);

        ss_dassert(rs_entname_getschema(relname) != NULL);

        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {

            /* Something with the same name already exists */
            CHECK_RBDATA(rbdata);

            if (RBDATA_ISREL(rbdata) && !rbdata->rd_infovalid) {
                /* Relation contains only name info, ensure that
                 * the existing information matches
                 */
                ss_dassert(0 == rs_entname_compare(rs_relh_entname(cd, relh), rbdata->rd_nodename));
                if (rs_relh_relid(cd, relh) != (ulong)rbdata->rd_nodeid) {
                    return(RSRBUF_ERR_INVALID_ARG);
                }
                if (rbdata->rd_cardin == NULL) {
                    /* First time this relation is in the rbuf, get cardinal
                     * object from the relh. Cardinal object is kept here
                     * until server is shut down. This is needed e.g. to
                     * be able to update cardinal info during checkpoint
                     * even if relh is removed from the rbuf.
                     */
                    rbdata->rd_cardin = rs_relh_cardin(cd, relh);
                } else {
                    /* Relh has previously been buffered here and we have
                     * the cardinal info already available. It is also more
                     * correct that the values read from the system table
                     * which are from the last checkpoint level.
                     * Put the cardinal
                     * values in rbuf to relh cardinal object and start
                     * using relh cardinal object also here.
                     */
                    rs_cardin_t* cr;
                    cr = rbdata->rd_cardin;
                    rbdata->rd_cardin = rs_relh_replacecardin(cd, relh, cr);
                    rs_cardin_done(cd, cr);
                }
                rs_cardin_link(cd, rbdata->rd_cardin);
                rbdata->rd_.relh = relh;
                rbdata->rd_infovalid = TRUE;
                rbuf_lruinsert(cd, rbuf, rbdata);
                return(RSRBUF_SUCCESS);
            } else {
                /* Node found was a view or the relh was already present */
                return(RSRBUF_ERR_EXISTS);
            }

        } else {

            ulong relid = rs_relh_relid(cd, relh);
            bool b;

            /* rs_ttype_setsysttype(cd, rs_relh_ttype(cd, relh)); */

            rbdata = rbdata_init_kind(
                          cd,
                          rbuf,
                          relname,
                          relid,
                          relh,
                          RS_RBUF_RELATION,
                          RSRBUF_NAME_RELATION
                     );
            CHECK_RBDATA(rbdata);
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (!b) {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            return(b ? RSRBUF_SUCCESS : RSRBUF_ERR_EXISTS);
        }
}

/*##**********************************************************************\
 *
 *              rs_rbuf_insertrelh_ex
 *
 * Inserts a relh into rbuf.
 * Relh is "moved" into the rbuf. This means that the structure is not
 * copied, only ownership of it is transferred.
 *
 * If the same relname already exists in rbuf, the insert is performed only
 * if the relh is not previously present in that node.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relh - in, take
 *              relation handle
 *
 * Return value :
 *
 *      RSRBUF_SUCCESS, succeeded
 *      RSRBUF_ERR_EXISTS, if relation (or view) with same name existed in rbuf
 *      RSRBUF_ERR_INVALID_ARG, if relation id does not match
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_ret_t rs_rbuf_insertrelh_ex(
        void*      cd,
        rs_rbuf_t* rbuf,
        rs_relh_t* relh
) {
        rs_rbuf_ret_t ret;

        SsSemEnter(rbuf->rb_sem);

        ss_dprintf_1(("rs_rbuf_insertrelh_ex\n"));

        ret = rs_rbuf_insertrelh_ex_nomutex(cd, rbuf, relh);

        SsSemExit(rbuf->rb_sem);

        return(ret);
}

/*#***********************************************************************\
 *
 *              rbuf_addrelname_nomutex
 *
 * Same as rs_rbuf_addrelname but not protected by rbuf mutex.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
 *
 *
 *      relid -
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
static bool rbuf_addrelname_nomutex(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid,
        rs_cardin_t*    cardin,
        bool            set_new_cardin
) {
        su_rbt_node_t*      node;
        rbdata_t*           rbdata;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);
        ss_dassert(rs_entname_getschema(relname) != NULL);

        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                relname, 
                rbuf->rb_recovery ? RS_RBUF_RELATION : RS_RBUF_ANY, 
                &node, 
                &rbdata);
        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {
            ss_dprintf_3(("rbuf_addrelname_nomutex:already in rbuf, return FALSE\n"));
            if (set_new_cardin && rbdata->rd_cardin != cardin && cardin != NULL) {
                ss_dprintf_4(("rbuf_addrelname_nomutex:set new cardin\n"));
                ss_dassert(RBDATA_ISKIND(rbdata, RS_RBUF_RELATION));
                if (rbdata->rd_cardin != NULL) {
                    rs_cardin_done(cd, rbdata->rd_cardin);
                }
                rs_cardin_link(cd, cardin);
                rbdata->rd_cardin = cardin;
                if (rbdata->rd_infovalid) {
                    rs_relh_setcardin(cd, rbdata->rd_.relh, rbdata->rd_cardin);
                }
            }
            return(FALSE);
        } else {

            bool b;
            rbdata_t* rbdata = rbdata_init_kind(
                                    cd,
                                    rbuf,
                                    relname,
                                    relid,
                                    NULL,
                                    RS_RBUF_RELATION,
                                    RSRBUF_NAME_RELATION
                               );
            CHECK_RBDATA(rbdata);
            if (cardin != NULL) {
                ss_dassert(rbdata->rd_cardin == NULL);
                rbdata->rd_cardin = cardin;
                rs_cardin_link(cd, rbdata->rd_cardin);
            }
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (!b) {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            ss_dprintf_3(("rbuf_addrelname_nomutex:return %d\n", b));
            return(b);
        }
}

/*##**********************************************************************\
 *
 *              rs_rbuf_addrelname
 *
 * Inserts a relation name and id into rbuf.
 *
 * If the same relname already exists in rbuf, the insert is not performed.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relname - in, use
 *              relation name
 *
 *      relid - in, use
 *              relation id
 *
 * Return value :
 *
 *      TRUE, succeeded
 *      FALSE, if relation (or view) with same name existed in rbuf
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_addrelname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid
) {
        bool succp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_addrelname:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        SsSemEnter(rbuf->rb_sem);

        succp = rbuf_addrelname_nomutex(cd, rbuf, relname, relid, NULL, FALSE);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*##**********************************************************************\
 *
 *		rs_rbuf_addrelnameandcardin
 *
 * Inserts a relation name, id and cardin info into rbuf.
 *
 * If the same relname already exists in rbuf, the insert is not performed.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data, not used
 *
 *	rbuf - in out, use
 *		Pointer to rbuf object where the insert is done to.
 *
 *	relname - in, use
 *		relation name
 *
 *	relid - in, use
 *		relation id
 *
 *	cardin - in, use
 *		cardinality info
 *
 * Return value :
 *
 *      TRUE, succeeded
 *      FALSE, if relation (or view) with same name existed in rbuf
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_addrelnameandcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid,
        rs_cardin_t*    cardin
) {
        bool succp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_addrelnameandcardin:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        SsSemEnter(rbuf->rb_sem);

        succp = rbuf_addrelname_nomutex(cd, rbuf, relname, relid, cardin, TRUE);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*##**********************************************************************\
 * 
 *		rs_rbuf_getcardin
 * 
 * Returns cardin object for a relname.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		rbuf - 
 *			
 *			
 *		relname - 
 *			
 *			
 * Return value - give : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_cardin_t* rs_rbuf_getcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname)
{
        rbdata_t*           rbdata;
        su_rbt_node_t*      node;
        rs_rbuf_present_t   rp;
        rs_cardin_t*        cardin = NULL;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_getcardin:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(cd, rbuf, relname, RS_RBUF_RELATION, &node, &rbdata);
        if (rp != RSRBUF_NOTEXIST) {
            ss_dassert(rbdata != NULL);
            cardin = rbdata->rd_cardin;
            if (cardin != NULL) {
                rs_cardin_link(cd, cardin);
            }
            ss_dprintf_2(("rs_rbuf_getcardin:cardin ptr=%ld\n", (long)cardin));
        }
        SsSemExit(rbuf->rb_sem);

        return (cardin);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_updaterelname
 *
 * Updates relation name from NAME object to a REL object.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
 *
 *
 *      relid -
 *
 *
 *      cardin -
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
bool rs_rbuf_updaterelname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname,
        ulong           relid,
        rs_cardin_t*    cardin
) {
        bool succp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_updaterelname:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));

        SsSemEnter(rbuf->rb_sem);

        rbuf_removenode_kind_nomutex(cd, rbuf, relname, RS_RBUF_NAME, RSRBUF_NAME_RELATION);

        succp = rbuf_addrelname_nomutex(cd, rbuf, relname, relid, cardin, FALSE);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_removerelh
 *
 * Removes a relh from rbuf. Caller becomes the owner of the relh and is
 * responsible for freeing it.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      relname - in, use
 *              relation name
 *
 * Return value - give:
 *
 *      TRUE    - handle found and removed
 *      FALSE   - handle not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_removerelh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname
) {
        ss_dprintf_1(("rs_rbuf_removerelh:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));
        return(rbuf_removenode_kind(cd, rbuf, relname, RS_RBUF_RELATION, RSRBUF_NAME_RELATION));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_relhunbuffer
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
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
bool rs_rbuf_relhunbuffer(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname)
{
        bool foundp;

        ss_dprintf_1(("rs_rbuf_relhunbuffer:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));
        foundp = rbuf_unbuffer_kind(cd, rbuf, relname, RS_RBUF_RELATION, FALSE);
        return(foundp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_relhunbuffer_dropcardin
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      relname -
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
bool rs_rbuf_relhunbuffer_dropcardin(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   relname)
{
        bool foundp;

        ss_dprintf_1(("rs_rbuf_relhunbuffer_dropcardin:%s.%s.%s\n",
            rs_entname_getprintcatalog(relname),
            rs_entname_getprintschema(relname),
            rs_entname_getname(relname)));
        foundp = rbuf_unbuffer_kind(cd, rbuf, relname, RS_RBUF_RELATION, TRUE);
        return(foundp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_renamerel
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      oldname -
 *
 *
 *      newname -
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
bool rs_rbuf_renamerel(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   oldname,
        rs_entname_t*   newname)
{
        bool succp;
        rbdata_t* rbdata;
        su_rbt_node_t* node;
        rs_rbuf_present_t rp;
        long relid;
        rs_cardin_t* cardin;

        ss_dprintf_1(("rs_rbuf_renamerel:%s.%s.%s to %s.%s.%s\n",
            rs_entname_getprintcatalog(oldname),
            rs_entname_getprintschema(oldname),
            rs_entname_getname(oldname),
            rs_entname_getprintcatalog(newname),
            rs_entname_getprintschema(newname),
            rs_entname_getname(newname)));

        SsSemEnter(rbuf->rb_sem);

        rp = rbuf_rbdata_find(cd, rbuf, oldname, RS_RBUF_RELATION, &node, &rbdata);
        if ((rp != RSRBUF_EXISTS && rp != RSRBUF_BUFFERED) ||
            !RBDATA_ISKIND(rbdata, RS_RBUF_RELATION)) {
            SsSemExit(rbuf->rb_sem);
            return(FALSE);
        }

        relid = rbdata->rd_nodeid;
        if (rbdata->rd_cardin != NULL) {
            cardin = rbdata->rd_cardin;
            rs_cardin_link(cd, cardin);
        } else {
            cardin = NULL;
        }

        rbuf_removenode_kind_nomutex(cd, rbuf, newname, RS_RBUF_RELATION, RSRBUF_NAME_RELATION);
        rbuf_removenode_kind_nomutex(cd, rbuf, newname, RS_RBUF_NAME, RSRBUF_NAME_RELATION);
        rbuf_removenode_kind_nomutex(cd, rbuf, oldname, RS_RBUF_RELATION, RSRBUF_NAME_RELATION);

        succp = rbuf_addrelname_nomutex(cd, rbuf, newname, relid, NULL, FALSE);
        if (succp) {
            /* Restore cardin value. */
            rp = rbuf_rbdata_find(cd, rbuf, newname, RS_RBUF_RELATION, &node, &rbdata);
            rbdata->rd_cardin = cardin;
        }

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

#ifndef SS_NOVIEW

/*##**********************************************************************\
 *
 *              rs_rbuf_viewpresent
 *
 * Checks if the view with given name is present in db and/or rbuf
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in, use
 *              rbuf object
 *
 *      relname - in, use
 *              relation name
 *
 *      p_viewh - out, ref
 *              if p_viewh is non-NULL and return value is RSRBUF_BUFFERED
 *          a pointer to viewh in rbuf is stored in *p_viewh
 *
 *      p_viewid - out
 *              if p_viewid is non-NULL and return value is RSRBUF_BUFFERED,
 *          or RSRBUF_EXISTS the view id is stored in *p_viewid
 *
 * Return value :
 *
 *      RSRBUF_EXISTS    View exists in rbuf, but relh is not buffered
 *      RSRBUF_NOTEXIST, View does not exist in rbuf
 *      RSRBUF_BUFFERED  View exists and viewh is buffered
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_present_t rs_rbuf_viewpresent(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        rs_viewh_t**    p_viewh,
        ulong*          p_viewid
) {
        ss_dprintf_1(("rs_rbuf_viewpresent:%s.%s.%s\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));
        return(rbuf_present_kind(
                    cd,
                    rbuf,
                    viewname,
                    (void**)p_viewh,
                    p_viewid,
                    RS_RBUF_VIEW)
              );
}

/*##**********************************************************************\
 *
 *              rs_rbuf_viewnamebyid
 *
 * Returns the relname with id relid, if found from the buffer.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      viewid - in
 *              view id that is searched from the rbuf
 *
 *      p_viewname - out, give
 *              view name is allocated and returned here
 *
 * Return value :
 *
 *      TRUE    - viewid found and viewname stored to *p_viewname
 *      FALSE   - viewid not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_viewnamebyid(
        void*           cd,
        rs_rbuf_t*      rbuf,
        ulong           viewid,
        rs_entname_t**  p_viewname)
{
        return(rbuf_namebyid_kind(cd, rbuf, viewid, p_viewname, NULL, RS_RBUF_VIEW, RSRBUF_NAME_VIEW));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_insertviewh
 *
 * Inserts a viewh into rbuf.
 * Viewh is "moved" into the rbuf. This means that the structure is not
 * copied, only ownership of it is transferred.
 *
 * If the same viewname already exists in rbuf, the insert is performed only
 * if the viewh is not previously present in that node.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      viewh - in, take
 *              view handle
 *
 * Return value :
 *
 *      TRUE, succeeded
 *      FALSE, if view (or relation) with same name existed in rbuf
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_insertviewh(
        void*       cd,
        rs_rbuf_t*  rbuf,
        rs_viewh_t* viewh
) {
        su_rbt_node_t*      node;
        rbdata_t*           rbdata;
        rs_entname_t*       viewname;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);
        ss_dassert(viewh != NULL);
        ss_dprintf_1(("rs_rbuf_insertviewh:%s\n", rs_viewh_name(cd, viewh)));

        SsSemEnter(rbuf->rb_sem);

        viewname = rs_viewh_entname(cd, viewh);
        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                viewname, 
                rbuf->rb_recovery ? RS_RBUF_VIEW : RS_RBUF_ANY, 
                &node, 
                &rbdata);

        ss_dassert(rs_entname_getschema(viewname) != NULL);

        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {

            /* Something with the same name already exists */
            CHECK_RBDATA(rbdata);

            if (RBDATA_ISVIEW(rbdata) && !rbdata->rd_infovalid) {
                /* View contains only name info, ensure that
                   the existing information matches
                */
                ss_dassert(
                    0 == rs_entname_compare(
                            rs_viewh_entname(cd, viewh),
                            rbdata->rd_nodename
                         )
                );
                /* The following condition may be temporarily violated
                 * in HSB secondary server.
                 *   ss_dassert(rs_viewh_viewid(cd, viewh) == rbdata->rd_nodeid);
                 */
                rbdata->rd_.viewh = viewh;
                rbdata->rd_infovalid = TRUE;
                rbuf_lruinsert(cd, rbuf, rbdata);
                SsSemExit(rbuf->rb_sem);
                return(TRUE);
            } else {
                /* Node found was a rel or the viewh was already present */
                SsSemExit(rbuf->rb_sem);
                return(FALSE);
            }

        } else {

            ulong viewid   = rs_viewh_viewid(cd, viewh);
            bool b;
            rbdata_t* rbdata = rbdata_init_kind(
                                    cd,
                                    rbuf,
                                    viewname,
                                    viewid,
                                    viewh,
                                    RS_RBUF_VIEW,
                                    RSRBUF_NAME_VIEW
                               );
            CHECK_RBDATA(rbdata);
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (!b) {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            SsSemExit(rbuf->rb_sem);
            return(b);
        }
}

/*#***********************************************************************\
 *
 *              rbuf_addviewname_nomutex
 *
 * Same as rs_rbuf_addviewname but not protected by rbuf mutex.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      viewname -
 *
 *
 *      viewid -
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
static bool rbuf_addviewname_nomutex(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        ulong           viewid
) {
        su_rbt_node_t*  node;
        rbdata_t*           rbdata;
        rs_rbuf_present_t   rp;

        SS_NOTUSED(cd);
        ss_dassert(rs_entname_getschema(viewname) != NULL);

        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                viewname, 
                rbuf->rb_recovery ? RS_RBUF_VIEW : RS_RBUF_ANY, 
                &node, 
                &rbdata);
        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {
            return(FALSE);
        } else {

            bool b;
            rbdata_t* rbdata = rbdata_init_kind(
                                    cd,
                                    rbuf,
                                    viewname,
                                    viewid,
                                    NULL,
                                    RS_RBUF_VIEW,
                                    RSRBUF_NAME_VIEW
                               );
            CHECK_RBDATA(rbdata);
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (!b) {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            return(b);
        }
}

/*##**********************************************************************\
 *
 *              rs_rbuf_addviewname
 *
 * Inserts a view name and id into rbuf.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object where the insert is done to.
 *
 *      viewname - in, use
 *              view name
 *
 *      viewid - in, use
 *              view id
 *
 * Return value :
 *
 *      TRUE, succeeded
 *      FALSE, if view (or relation) with same name existed in rbuf
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_addviewname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        ulong           viewid
) {
        bool succp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_addviewname:%s.%s.%s\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        SsSemEnter(rbuf->rb_sem);

        succp = rbuf_addviewname_nomutex(cd, rbuf, viewname, viewid);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_updateviewname
 *
 * Updates view name is rbuf from NAME object to a VIEW object.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      viewname -
 *
 *
 *      viewid -
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
bool rs_rbuf_updateviewname(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname,
        ulong           viewid
) {
        bool succp;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_updateviewname:%s.%s.%s\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));

        SsSemEnter(rbuf->rb_sem);

        rbuf_removenode_kind_nomutex(cd, rbuf, viewname, RS_RBUF_NAME, RSRBUF_NAME_VIEW);

        succp = rbuf_addviewname_nomutex(cd, rbuf, viewname, viewid);

        SsSemExit(rbuf->rb_sem);

        return(succp);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_removeviewh
 *
 * Removes a viewh from rbuf. Caller becomes the owner of the viewh and is
 * responsible for freeing it.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      rbuf - in out, use
 *              relinfo buf object
 *
 *      viewname - in, use
 *              view name
 *
 * Return value - give :
 *
 *      TRUE    - handle found and removed
 *      FALSE   - handle not found
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_rbuf_removeviewh(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname
) {
        ss_dprintf_1(("rs_rbuf_removeviewh:%s.%s.%s\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));
        return(rbuf_removenode_kind(cd, rbuf, viewname, RS_RBUF_VIEW, RSRBUF_NAME_VIEW));
}

/*##**********************************************************************\
 *
 *              rs_rbuf_viewhunbuffer
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      viewname -
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
bool rs_rbuf_viewhunbuffer(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   viewname)
{
        bool foundp;

        ss_dprintf_1(("rs_rbuf_viewhunbuffer:%s.%s.%s\n",
            rs_entname_getprintcatalog(viewname),
            rs_entname_getprintschema(viewname),
            rs_entname_getname(viewname)));
        foundp = rbuf_unbuffer_kind(cd, rbuf, viewname, RS_RBUF_VIEW, FALSE);
        return(foundp);
}

#endif /* SS_NOVIEW */


bool rs_rbuf_event_add(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_event_t*     event)
{
        su_rbt_node_t*  node;
        rbdata_t*       rbdata;
        rs_rbuf_present_t rp;
        rs_entname_t*   name;

        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        name = rs_event_entname(cd, event);
        ss_dassert(rs_entname_getschema(name) != NULL);

        rp = rbuf_rbdata_find(
                cd, 
                rbuf, 
                name, 
                rbuf->rb_recovery ? RS_RBUF_EVENT : RS_RBUF_ANY, 
                &node, 
                &rbdata);
        if (rp == RSRBUF_EXISTS || rp == RSRBUF_BUFFERED) {
            SsSemExit(rbuf->rb_sem);
            return(FALSE);
        } else {

            bool b;
            rbdata_t* rbdata = rbdata_init_kind(
                                    cd,
                                    rbuf,
                                    name,
                                    rs_event_eventid(cd, event),
                                    event,
                                    RS_RBUF_EVENT,
                                    RSRBUF_NAME_EVENT
                               );
            CHECK_RBDATA(rbdata);
            b = rbuf_rbdata_insert(rbuf, rbdata);
            if (b) {
                rs_event_link(cd, event);
            } else {
                rbuf_rbdata_free(rbuf, rbdata);
            }
            SsSemExit(rbuf->rb_sem);
            return(b);
        }
}

bool rs_rbuf_event_remove(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   name)
{
        ss_dprintf_1(("rs_rbuf_event_remove:%s.%s.%s\n",
            rs_entname_getprintcatalog(name),
            rs_entname_getprintschema(name),
            rs_entname_getname(name)));
        return(rbuf_removenode_kind(cd, rbuf, name, RS_RBUF_EVENT, RSRBUF_NAME_EVENT));
}

bool rs_rbuf_event_findref(
        void*           cd,
        rs_rbuf_t*      rbuf,
        rs_entname_t*   name,
        rs_event_t**    p_event)
{
        ulong id;
        rs_rbuf_present_t r;

        ss_dprintf_1(("rs_rbuf_event_findref:%s.%s.%s\n",
            rs_entname_getprintcatalog(name),
            rs_entname_getprintschema(name),
            rs_entname_getname(name)));
        r = rbuf_present_kind(
                    cd,
                    rbuf,
                    name,
                    (void**)p_event,
                    &id,
                    RS_RBUF_EVENT);

        ss_assert(r != RSRBUF_EXISTS);

        if (r == RSRBUF_BUFFERED) {
            ss_assert(*p_event != NULL);
            return(TRUE);
        }
        return(FALSE);
}


/*##**********************************************************************\
 *
 *              rs_rbuf_iterate
 *
 * Iterates through all relations and views in rbuf and calls user
 * defined function iterfun for every element that is buffered.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in out, use
 *              Pointer to rbuf object.
 *
 *      userinfo -
 *              user info passed to iterfun
 *
 *      iterfun -
 *              function called for every buffered element in rbuf
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Not fully reentrant, fails if current iteration node is
 *      removed from the rbuf.
 *
 * Globals used :
 */
void rs_rbuf_iterate(
        void*      cd,
        rs_rbuf_t* rbuf,
        void*      userinfo,
        void       (*iterfun)(
                        void* ui,
                        bool is_rel,
                        void* relh_or_viewh,
                        long id,
                        void* cardin))
{
        rbuf_iterinfo_t* ii;
        su_rbt_node_t* node;
        su_list_t* list;
        su_list_node_t* n;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_iterate\n"));

        list = su_list_init(NULL);

        SsSemEnter(rbuf->rb_sem);

        node = su_rbt_min(rbuf->rb_name_rbt, NULL);
        while (node != NULL) {
            rbdata_t* rd;

            rd = su_rbtnode_getkey(node);
            CHECK_RBDATA(rd);

            switch (rd->rd_objtype) {
                case RS_RBUF_RELATION:
                case RS_RBUF_VIEW:
                    ii = SSMEM_NEW(rbuf_iterinfo_t);
                    ii->ii_nodetype = rd->rd_objtype;
                    ii->ii_nodeid = rd->rd_nodeid;
                    if (rd->rd_objtype == RS_RBUF_RELATION) {
                        ii->ii_cardin = rd->rd_cardin;
                        if (ii->ii_cardin != NULL) {
                            rs_cardin_link(cd, ii->ii_cardin);
                        }
                        ii->ii_.relh = rd->rd_.relh;
                        if (ii->ii_.relh != NULL) {
                            rs_relh_link(cd, ii->ii_.relh);
                            SS_MEM_SETLINK(ii->ii_.relh);
                        }
                    } else {
                        ss_dassert(rd->rd_objtype == RS_RBUF_VIEW);
                        ii->ii_cardin = NULL;
                        ii->ii_.viewh = rd->rd_.viewh;
                        if (ii->ii_.viewh != NULL) {
                            rs_viewh_link(cd, ii->ii_.viewh);
                        }
                    }
                    su_list_insertlast(list, ii);
                    break;
                case RS_RBUF_NAME:
                case RS_RBUF_EVENT:
                    break;
                default:
                    ss_error;
            }
            node = su_rbt_succ(rbuf->rb_name_rbt, node);
        }

        SsSemExit(rbuf->rb_sem);

        su_list_do_get(list, n, ii) {
            switch (ii->ii_nodetype) {
                case RS_RBUF_RELATION:
                    (*iterfun)(
                        userinfo,
                        TRUE,
                        ii->ii_.relh,
                        ii->ii_nodeid,
                        ii->ii_cardin);
                    if (ii->ii_.relh != NULL) {
                        SS_MEM_SETUNLINK(ii->ii_.relh);
                        rs_relh_done(cd, ii->ii_.relh);
                    }
                    if (ii->ii_cardin != NULL) {
                        rs_cardin_done(cd, ii->ii_cardin);
                    }
                    break;
                case RS_RBUF_VIEW:
                    (*iterfun)(
                        userinfo,
                        FALSE,
                        ii->ii_.viewh,
                        ii->ii_nodeid,
                        ii->ii_cardin);
                    if (ii->ii_.viewh != NULL) {
                        rs_viewh_done(cd, ii->ii_.viewh);
                    }
                    break;
                default:
                    ss_error;
            }
            SsMemFree(ii);
        }

        su_list_done(list);
}

/*##**********************************************************************\
 *
 *              rs_rbuf_ischemaobjects
 *
 * Checks if there are any objects for a schema.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      rbuf -
 *
 *
 *      schemaname -
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
bool rs_rbuf_ischemaobjects(
        void*      cd,
        rs_rbuf_t* rbuf,
        char*      schemaname)
{
        su_rbt_node_t*  node;

        SS_NOTUSED(cd);
        ss_dprintf_1(("rs_rbuf_ischemaobjects:schemaname='%s'\n", schemaname));

        SsSemEnter(rbuf->rb_sem);

        node = su_rbt_min(rbuf->rb_name_rbt, NULL);

        while (node != NULL) {
            rbdata_t* rd;

            rd = su_rbtnode_getkey(node);
            CHECK_RBDATA(rd);

            if (SsStricmp(schemaname, rs_entname_getschema(rd->rd_nodename)) == 0) {
                SsSemExit(rbuf->rb_sem);
                return(TRUE);
            }
            node = su_rbt_succ(rbuf->rb_name_rbt, node);
        }
        SsSemExit(rbuf->rb_sem);

        return(FALSE);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *              rs_rbuf_printinfo
 *
 *
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *      rbuf -
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
void rs_rbuf_printinfo(
        void*      fp,
        rs_rbuf_t* rbuf)
{
        long nrel = 0;
        long nrel_buffered = 0;
        long nview = 0;
        long nview_buffered = 0;
        su_rbt_node_t* node;
        double hitrate;

        SsSemEnter(rbuf->rb_sem);

        node = su_rbt_min(rbuf->rb_name_rbt, NULL);

        while (node != NULL) {
            rbdata_t* rd;

            rd = su_rbtnode_getkey(node);

            if (rd->rd_objtype == RS_RBUF_RELATION) {
                nrel++;
                if (rd->rd_infovalid) {
                    nrel_buffered++;
                }
            } else if (rd->rd_objtype == RS_RBUF_VIEW) {
                nview++;
                if (rd->rd_infovalid) {
                    nview_buffered++;
                }
            }
            node = su_rbt_succ(rbuf->rb_name_rbt, node);
        }

        if (rbuf->rb_searchcnt == 0) {
            hitrate = 0.0;
        } else {
            hitrate = 100.0 * (double)rbuf->rb_foundcnt / (double)rbuf->rb_searchcnt;
        }

        SsFprintf(fp, "  Relations : Buffered %4ld Total %4ld\n", nrel_buffered, nrel);
        SsFprintf(fp, "  Views     : Buffered %4ld Total %4ld\n", nview_buffered, nview);
        SsFprintf(fp, "  Hit rate %.1lf%% (%ld/%ld) MaxLRU %ld\n",
            hitrate,
            rbuf->rb_foundcnt,
            rbuf->rb_searchcnt,
            rbuf->rb_maxlru);

        SsSemExit(rbuf->rb_sem);
}

#endif /* SS_LIGHT */


/*#***********************************************************************\
 *
 *              rbuf_modify1systablechartypes
 *
 * Iterator callback to modify system table char column types from rbuf
 *
 * Parameters :
 *
 *      ui - use
 *              same as cd
 *
 *      is_rel - in
 *              TRUE - relation, FALSE - view
 *
 *      relh_or_viewh - use
 *              relation or view handle
 *
 *      id - in
 *              not used
 *
 *      cardin - in, use
 *              not used
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rbuf_modify1systablechartypes(
        void* ui,
        bool is_rel,
        void* relh_or_viewh,
        long id __attribute__ ((unused)),
        void* cardin __attribute__ ((unused)))
{
        if (is_rel && relh_or_viewh != NULL) {
            rs_relh_modifyttypeifsystable(
                ui,
                relh_or_viewh);
        }
}

/*##**********************************************************************\
 *
 *              rs_rbuf_modifysystablechartypes
 *
 * Modifies char columns in system tables to double the previous max
 * length
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      rbuf - use
 *              relbuf
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_rbuf_modifysystablechartypes(void* cd, rs_rbuf_t* rbuf)
{
        rs_rbuf_iterate(
            cd,
            rbuf,
            cd,
            rbuf_modify1systablechartypes);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *              rs_rbuf_print
 *
 * Printd relation buffer using SsDbgPrintf.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data, not used
 *
 *      rbuf - in, use
 *              Pointer to rbuf object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_rbuf_print(
        void*      cd,
        rs_rbuf_t* rbuf)
{
        su_rbt_node_t*  node;

        SS_NOTUSED(cd);

        SsSemEnter(rbuf->rb_sem);

        SsDbgPrintf("RELATION BUFFER:\n");
        SsDbgPrintf("----------------\n");
        SsDbgPrintf("%-4s %-20s %-20s %-20s %-5s %-8s %-3s\n",
            "TYPE", "CATALOG", "SCHEMA", "NAME", "ID", "NAMETYPE", "BUFFERED");

        node = su_rbt_min(rbuf->rb_name_rbt, NULL);

        while (node != NULL) {
            rbdata_t* rd;
            const char* type;

            rd = su_rbtnode_getkey(node);

            switch (rd->rd_objtype) {
                case RS_RBUF_RELATION:
                    type = "REL";
                    break;
                case RS_RBUF_RELATION_DROPCARDIN:
                    type = "RELATION_DROPCARDIN";
                    break;
                case RS_RBUF_VIEW:
                    type = "VIEW";
                    break;
                case RS_RBUF_NAME:
                    type = "NAME";
                    break;
                case RS_RBUF_EVENT:
                    type = "EVENT";
                    break;
                default:
                    ss_error;
            }

            SsDbgPrintf("%-4s(%d) %-20s %-20s %-20s %5ld %8d %-3s\n",
                type,
                rd->rd_objtype,
                rs_entname_getprintcatalog(rd->rd_nodename),
                rs_entname_getprintschema(rd->rd_nodename),
                rs_entname_getname(rd->rd_nodename),
                rd->rd_nodeid,
                rd->rd_nametype,
                rd->rd_infovalid ? "YES" : "NO");

            node = su_rbt_succ(rbuf->rb_name_rbt, node);
        }
        SsSemExit(rbuf->rb_sem);
}
#endif /* SS_DEBUG */
