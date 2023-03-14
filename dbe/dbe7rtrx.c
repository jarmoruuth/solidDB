/*************************************************************************\
**  source       * dbe7rtrx.c
**  directory    * dbe
**  description  * Replication Transaction translation
**               * routines
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
#include <su0rbtr.h>

#include "dbe9type.h"
#include "dbe8trxl.h"

#include "dbe7rtrx.h"

typedef struct {
        ss_debug(int rt_chk;)
        dbe_trxid_t rt_remotetrxid;
        dbe_trxid_t rt_localtrxid;
        void*       rt_trxdata;
        bool        rt_isdummy; /* is it a dummy mapping? */
} dbe_rtrx_t;

struct dbe_rtrxbuf_st {
        ss_debug(int rtb_chk;)
        dbe_rtrxsearchby_t rtb_searchby;
        su_rbt_t* rtb_buf;
};


#define CHK_RTRX(rtrx) \
        ss_dassert(SS_CHKPTR(rtrx));\
        ss_dassert((rtrx)->rt_chk == DBE_CHK_RTRX);

#define CHK_RTRTXBUF(rtrxbuf) \
        ss_dassert(SS_CHKPTR(rtrxbuf));\
        ss_dassert((rtrxbuf)->rtb_chk == DBE_CHK_RTRXBUF);

#ifdef SS_DEBUG
# define RTRX_RBTNODE_DPRINT(n) {\
            dbe_rtrx_t* rtrx = su_rbtnode_getkey(n);\
            ss_pprintf_2(("remotetrxid=%ld, localtrxid=%ld\n", DBE_TRXID_GETLONG(rtrx->rt_remotetrxid), DBE_TRXID_GETLONG(rtrx->rt_localtrxid)));\
        }
#else /* SS_DEBUG */
# define RTRX_RBTNODE_DPRINT(n)
#endif /* SS_DEBUG */

#ifdef DBE_REPLICATION

static dbe_rtrx_t* rtrx_init(
        dbe_trxid_t remotetrxid,
        dbe_trxid_t localtrxid,
        void* trxdata,
        bool  isdummy)
{
        dbe_rtrx_t* rtrx;

        rtrx = SSMEM_NEW(dbe_rtrx_t);
        ss_debug(rtrx->rt_chk = DBE_CHK_RTRX);
        rtrx->rt_remotetrxid = remotetrxid;
        rtrx->rt_localtrxid  = localtrxid;
        rtrx->rt_trxdata = trxdata;
        rtrx->rt_isdummy = isdummy;
        return (rtrx);
}

static void rtrx_done(dbe_rtrx_t* rtrx)
{
        CHK_RTRX(rtrx);
        SsMemFree(rtrx);
}

static int rtrx_inscmpbylocal(
        dbe_rtrx_t* rtrx1,
        dbe_rtrx_t* rtrx2)
{
        CHK_RTRX(rtrx1);
        CHK_RTRX(rtrx2);
        
        return (DBE_TRXID_CMP_EX(rtrx1->rt_localtrxid, rtrx2->rt_localtrxid));
}

static int rtrx_searchcmpbylocal(
        void* localtrxid,
        dbe_rtrx_t* rtrx2)
{
        dbe_trxid_t trxid;

        CHK_RTRX(rtrx2);

        trxid = DBE_TRXID_INIT((long)localtrxid);
        return(DBE_TRXID_CMP_EX(trxid, rtrx2->rt_localtrxid));
}

static int rtrx_inscmpbyremote(
        dbe_rtrx_t* rtrx1,
        dbe_rtrx_t* rtrx2)
{
        CHK_RTRX(rtrx1);
        CHK_RTRX(rtrx2);

        return(DBE_TRXID_CMP_EX(rtrx1->rt_remotetrxid, rtrx2->rt_remotetrxid));
}

static int rtrx_searchcmpbyremote(
        void* remotetrxid,
        dbe_rtrx_t* rtrx2)
{
        dbe_trxid_t trxid;

        CHK_RTRX(rtrx2);
        trxid = DBE_TRXID_INIT((long)remotetrxid);
        
        return(DBE_TRXID_CMP_EX(trxid, rtrx2->rt_remotetrxid));
}

dbe_rtrxbuf_t* dbe_rtrxbuf_init(void)
{
        dbe_rtrxbuf_t* rtrxbuf;

        rtrxbuf = SSMEM_NEW(dbe_rtrxbuf_t);
        ss_debug(rtrxbuf->rtb_chk = DBE_CHK_RTRXBUF);
        rtrxbuf->rtb_searchby = DBE_RTRX_SEARCHBYNONE;
        rtrxbuf->rtb_buf = NULL;
        return (rtrxbuf);
}

void dbe_rtrxbuf_done(dbe_rtrxbuf_t* rtrxbuf)
{
        CHK_RTRTXBUF(rtrxbuf);
        if (rtrxbuf->rtb_buf != NULL) {
            su_rbt_done(rtrxbuf->rtb_buf);
        }
        SsMemFree(rtrxbuf);
}

void dbe_rtrxbuf_setsearchby(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_rtrxsearchby_t searchby)
{
        su_rbt_t* tmpbuf = NULL;
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        if (searchby == rtrxbuf->rtb_searchby) {
            ss_dassert(searchby != DBE_RTRX_SEARCHBYNONE);
            return;
        }
        switch (searchby) {
            case DBE_RTRX_SEARCHBYLOCAL:
                tmpbuf = su_rbt_inittwocmp(
                            (int(*)(void*,void*))rtrx_inscmpbylocal,
                            (int(*)(void*,void*))rtrx_searchcmpbylocal,
                            (void(*)(void*))rtrx_done);
                break;
            case DBE_RTRX_SEARCHBYREMOTE:
                tmpbuf = su_rbt_inittwocmp(
                            (int(*)(void*,void*))rtrx_inscmpbyremote,
                            (int(*)(void*,void*))rtrx_searchcmpbyremote,
                            (void(*)(void*))rtrx_done);
                break;
            case DBE_RTRX_SEARCHBYNONE:
                ss_derror;
            default:
                ss_error;

        }
        switch (rtrxbuf->rtb_searchby) {
            case DBE_RTRX_SEARCHBYNONE:
                rtrxbuf->rtb_buf = tmpbuf;
                break;
            case DBE_RTRX_SEARCHBYLOCAL:
            case DBE_RTRX_SEARCHBYREMOTE:
                for (rbtnode = su_rbt_min(rtrxbuf->rtb_buf, NULL);
                     rbtnode != NULL;
                     rbtnode = su_rbt_succ(rtrxbuf->rtb_buf, rbtnode))
                {
                    rtrx = su_rbtnode_getkey(rbtnode);
                    rtrx = rtrx_init(rtrx->rt_remotetrxid,
                                     rtrx->rt_localtrxid,
                                     rtrx->rt_trxdata,
                                     rtrx->rt_isdummy);
                    su_rbt_insert(tmpbuf, rtrx);
                }
                su_rbt_done(rtrxbuf->rtb_buf);
                rtrxbuf->rtb_buf = tmpbuf;
                break;
            default:
                ss_error;
        }
        rtrxbuf->rtb_searchby = searchby;
}


dbe_ret_t dbe_rtrxbuf_add(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid,
        dbe_trxid_t localtrxid,
        void* trxdata,
        bool  isdummy)
{
        dbe_rtrx_t* rtrx;

        ss_pprintf_1(("dbe_rtrxbuf_add:remotetrxid=%ld, localtrxid=%ld, isdummy=%s\n", DBE_TRXID_GETLONG(remotetrxid), DBE_TRXID_GETLONG(localtrxid), isdummy ? "TRUE" : "FALSE"));
        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby != DBE_RTRX_SEARCHBYNONE);
        rtrx = rtrx_init(remotetrxid, localtrxid, trxdata, isdummy);

#ifdef SS_DEBUG
        switch (rtrxbuf->rtb_searchby) {
            case DBE_RTRX_SEARCHBYLOCAL:
                ss_assert(DBE_TRXID_EQUAL(dbe_rtrxbuf_remotebylocaltrxid(rtrxbuf, localtrxid), DBE_TRXID_NULL));
                break;
            case DBE_RTRX_SEARCHBYREMOTE:
                ss_assert(DBE_TRXID_EQUAL(dbe_rtrxbuf_localbyremotetrxid(rtrxbuf, remotetrxid), DBE_TRXID_NULL));
                break;
            default:
                ss_error;
        }
#endif /* SS_DEBUG */

        if (!su_rbt_insert(rtrxbuf->rtb_buf, rtrx)) {
            return(DBE_ERR_REDEFINITION);
        } else {
            return (DBE_RC_SUCC);
        }
}

dbe_trxid_t dbe_rtrxbuf_localbyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid)
{
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYREMOTE);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(remotetrxid));
        if (rbtnode == NULL) {
            ss_pprintf_1(("dbe_rtrxbuf_localbyremotetrxid:remotetrxid=%ld, localtrxid=DBE_TRXID_NULL\n",
                DBE_TRXID_GETLONG(remotetrxid)));
            return (DBE_TRXID_NULL);
        }
        rtrx = su_rbtnode_getkey(rbtnode);
        ss_pprintf_1(("dbe_rtrxbuf_localbyremotetrxid:remotetrxid=%ld, localtrxid=%ld, isdummy=%s\n",
            DBE_TRXID_GETLONG(remotetrxid), 
            DBE_TRXID_GETLONG(rtrx->rt_localtrxid),
            rtrx->rt_isdummy ? "TRUE" : "FALSE"));
        return (rtrx->rt_localtrxid);
}

void* dbe_rtrxbuf_localtrxbyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid)
{
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYREMOTE);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(remotetrxid));
        if (rbtnode == NULL) {
            return (NULL);
        }
        rtrx = su_rbtnode_getkey(rbtnode);
        return (rtrx->rt_trxdata);
}

bool dbe_rtrxbuf_isdummybyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid)
{
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYREMOTE);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(remotetrxid));
        if (rbtnode == NULL) {
            return (FALSE);
        }
        rtrx = su_rbtnode_getkey(rbtnode);
        return (rtrx->rt_isdummy);
}

void* dbe_rtrxbuf_localtrxbylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid)
{
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYLOCAL);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(localtrxid));
        if (rbtnode == NULL) {
            return (NULL);
        }
        rtrx = su_rbtnode_getkey(rbtnode);
        return (rtrx->rt_trxdata);
}

dbe_trxid_t dbe_rtrxbuf_remotebylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid)
{
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYLOCAL);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(localtrxid));
        if (rbtnode == NULL) {
            ss_pprintf_1(("dbe_rtrxbuf_remotebylocaltrxid:localtrxid=%ld, remotetrxid=DBE_TRXID_NULL\n",
                DBE_TRXID_GETLONG(localtrxid)));
            return (DBE_TRXID_NULL);
        }
        rtrx = su_rbtnode_getkey(rbtnode);
        ss_pprintf_1(("dbe_rtrxbuf_remotebylocaltrxid:localtrxid=%ld, remotetrxid=%ld, isdummy=%s\n",
            DBE_TRXID_GETLONG(localtrxid), 
            DBE_TRXID_GETLONG(rtrx->rt_remotetrxid),
            rtrx->rt_isdummy ? "TRUE" : "FALSE"));
        return (rtrx->rt_remotetrxid);
}

dbe_ret_t dbe_rtrxbuf_deletebylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid)
{
        su_rbt_node_t* rbtnode;

        ss_pprintf_1(("dbe_rtrxbuf_deletebylocaltrxid:localtrxid=%ld\n", DBE_TRXID_GETLONG(localtrxid)));
        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYLOCAL);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(localtrxid));
        if (rbtnode == NULL) {
            return (DBE_ERR_NOTFOUND);
        }
        RTRX_RBTNODE_DPRINT(rbtnode);
        su_rbt_delete(rtrxbuf->rtb_buf, rbtnode);
        return (DBE_RC_SUCC);
}

dbe_ret_t dbe_rtrxbuf_deletebyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid)
{
        su_rbt_node_t* rbtnode;

        ss_pprintf_1(("dbe_rtrxbuf_deletebyremotetrxid:remotetrxid=%ld\n", DBE_TRXID_GETLONG(remotetrxid)));
        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYREMOTE);
        rbtnode = su_rbt_search(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(remotetrxid));
        if (rbtnode == NULL) {
            return (DBE_ERR_NOTFOUND);
        }
        RTRX_RBTNODE_DPRINT(rbtnode);
        su_rbt_delete(rtrxbuf->rtb_buf, rbtnode);
        return (DBE_RC_SUCC);
}

dbe_ret_t dbe_rtrxbuf_save(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_rtrxlistdaddr)
{
        dbe_trxlist_t* rtrxlist;
        su_rbt_node_t* rbtnode;
        dbe_rtrx_t* rtrx;
        dbe_ret_t rc;

        ss_pprintf_1(("dbe_rtrxbuf_save\n"));
        CHK_RTRTXBUF(rtrxbuf);
        if (rtrxbuf->rtb_buf == NULL) {
            ss_pprintf_2(("dbe_rtrxbuf_save:no tree created\n"));
            *p_rtrxlistdaddr = SU_DADDR_NULL;
            return (DBE_RC_SUCC);
        }
        rbtnode = su_rbt_min(rtrxbuf->rtb_buf, NULL);
        if (rbtnode != NULL) {
            rtrxlist =
                dbe_trxl_init(
                        cache,
                        freelist,
                        cpnum,
                        (dbe_blocktype_t)DBE_BLOCK_RTRXLIST);
            do {
                rtrx = su_rbtnode_getkey(rbtnode);
                if (rtrx->rt_isdummy) {
                    rc = dbe_trxl_addrtrx(
                        rtrxlist,
                        rtrx->rt_remotetrxid,
                        DBE_TRXID_NULL);
                } else {
                    rc = dbe_trxl_addrtrx(
                        rtrxlist,
                        rtrx->rt_remotetrxid,
                        rtrx->rt_localtrxid);
                }
                su_rc_assert(rc == DBE_RC_SUCC, rc);
                ss_pprintf_2(("dbe_rtrxbuf_save:remotetrxid=%ld, localtrxid=%ld\n", DBE_TRXID_GETLONG(rtrx->rt_remotetrxid), 
    DBE_TRXID_GETLONG(rtrx->rt_localtrxid)));
                rbtnode = su_rbt_succ(rtrxbuf->rtb_buf, rbtnode);
            } while (rbtnode != NULL);
            rc = dbe_trxl_save(rtrxlist, p_rtrxlistdaddr);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            dbe_trxl_done(rtrxlist);
            ss_pprintf_2(("dbe_rtrxbuf_save:saved to addr %ld\n", *p_rtrxlistdaddr));
        } else {
            ss_pprintf_2(("dbe_rtrxbuf_save:empty tree\n"));
            *p_rtrxlistdaddr = SU_DADDR_NULL;
        }
        
        return (DBE_RC_SUCC);
}

dbe_ret_t dbe_rtrxbuf_restore(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_cache_t* cache,
        su_daddr_t rtrxlistdaddr)
{
        dbe_trxlist_iter_t* rtrxli;
        dbe_trxid_t     remotetrxid;
        dbe_trxid_t     localtrxid;
        dbe_rtrx_t* rtrx;
        bool isdummy;

        ss_pprintf_1(("dbe_rtrxbuf_restore:addr=%ld\n", rtrxlistdaddr));
        CHK_RTRTXBUF(rtrxbuf);
        if (rtrxlistdaddr == SU_DADDR_NULL) {
            return (DBE_RC_SUCC);
        }
        rtrxli = dbe_trxli_init(
                        cache,
                        rtrxlistdaddr,
                        (dbe_blocktype_t)DBE_BLOCK_RTRXLIST);
        ss_dassert(rtrxbuf->rtb_buf != NULL);
        while (dbe_trxli_getnextrtrx(rtrxli, &remotetrxid, &localtrxid)) {
            if (DBE_TRXID_ISNULL(localtrxid)) {
                localtrxid = remotetrxid;
                isdummy = TRUE;
            } else {
                isdummy = FALSE;
            } 
            ss_pprintf_2(("dbe_rtrxbuf_restore:remotetrxid=%ld, localtrxid=%ld\n", DBE_TRXID_GETLONG(remotetrxid), DBE_TRXID_GETLONG(localtrxid)));
            rtrx = rtrx_init(remotetrxid, localtrxid, NULL, isdummy);
            su_rbt_insert(rtrxbuf->rtb_buf, rtrx);
        }
        dbe_trxli_done(rtrxli);
        ss_pprintf_2(("dbe_rtrxbuf_restore:restored\n"));
        return (DBE_RC_SUCC);
}

bool dbe_rtrxbuf_iterate(
        dbe_rtrxbuf_t* rtrxbuf,
        void** p_iter)
{
        CHK_RTRTXBUF(rtrxbuf);
        if (rtrxbuf->rtb_buf == NULL) {
            return (FALSE);
        }
        if (*p_iter == NULL) {
            *p_iter = su_rbt_min(rtrxbuf->rtb_buf, NULL);
        } else {
            *p_iter = su_rbt_succ(rtrxbuf->rtb_buf, *p_iter);
        }
        if (*p_iter == NULL) {
            return(FALSE);
        }
        return(TRUE);
}

void* dbe_rtrxbuf_getitertrxdata(
        dbe_rtrxbuf_t* rtrxbuf __attribute__ ((unused)),
        void* iter)
{
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(iter != NULL);
        
        rtrx = su_rbtnode_getkey(iter);
        return (rtrx->rt_trxdata);
}

dbe_trxid_t dbe_rtrxbuf_getiterlocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf __attribute__ ((unused)),
        void* iter)
{
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(iter != NULL);
        
        rtrx = su_rbtnode_getkey(iter);
        return (rtrx->rt_localtrxid);
}

dbe_trxid_t dbe_rtrxbuf_getiterremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf __attribute__ ((unused)),
        void* iter)
{
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(iter != NULL);
        
        rtrx = su_rbtnode_getkey(iter);
        return (rtrx->rt_remotetrxid);
}

bool dbe_rtrxbuf_getiterisdummymapping(
        dbe_rtrxbuf_t* rtrxbuf __attribute__ ((unused)),
        void* iter)
{
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(iter != NULL);
        
        rtrx = su_rbtnode_getkey(iter);
        return (rtrx->rt_isdummy);
}

void dbe_rtrxbuf_setitertrxdata(
        dbe_rtrxbuf_t* rtrxbuf __attribute__ ((unused)),
        void* iter,
        void* trxdata)
{
        dbe_rtrx_t* rtrx;

        CHK_RTRTXBUF(rtrxbuf);
        ss_dassert(iter != NULL);
        
        rtrx = su_rbtnode_getkey(iter);
        ss_dassert(rtrx->rt_trxdata == NULL);
        rtrx->rt_trxdata = trxdata;
}

void dbe_rtrxbuf_deleteall(
        dbe_rtrxbuf_t* rtrxbuf)
{
        su_rbt_node_t* rbtnode;

        ss_pprintf_1(("dbe_rtrxbuf_deleteall\n"));
        CHK_RTRTXBUF(rtrxbuf);

        while ((rbtnode = su_rbt_min(rtrxbuf->rtb_buf, NULL)) != NULL) {
            RTRX_RBTNODE_DPRINT(rbtnode);
            su_rbt_delete(rtrxbuf->rtb_buf, rbtnode);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_rtrxbuf_removeaborted
 * 
 * Removes aborted transaction from rtrxbuf
 * 
 * Parameters : 
 * 
 *	rtrxbuf - use
 *		
 *		
 *	trxbuf - in, use
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
void dbe_rtrxbuf_removeaborted(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxbuf_t* trxbuf)
{
        su_rbt_node_t* rbtnode;
        dbe_trxid_t prev_trxid;
        dbe_rtrx_t* rtrx;
        dbe_trxinfo_t* ti;

        ss_pprintf_1(("dbe_rtrxbuf_removeaborted\n"));

        for (rbtnode = su_rbt_min(rtrxbuf->rtb_buf, NULL);
             rbtnode != NULL;)
        {
            rtrx = su_rbtnode_getkey(rbtnode);
            ti = dbe_trxbuf_gettrxinfo(trxbuf, rtrx->rt_localtrxid);
            if (ti == NULL || dbe_trxinfo_isended(ti)) {
                ss_pprintf_2(("dbe_rtrxbuf_removeaborted:remotetrxid=%ld, localtrxid=%ld\n",
                    DBE_TRXID_GETLONG(rtrx->rt_remotetrxid), 
                    DBE_TRXID_GETLONG(rtrx->rt_localtrxid)));
                if (rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYLOCAL) {
                    prev_trxid = rtrx->rt_localtrxid;
                    dbe_rtrxbuf_deletebylocaltrxid(rtrxbuf, prev_trxid);
                } else {
                    ss_dassert(rtrxbuf->rtb_searchby == DBE_RTRX_SEARCHBYREMOTE);
                    prev_trxid = rtrx->rt_remotetrxid;
                    dbe_rtrxbuf_deletebyremotetrxid(rtrxbuf, prev_trxid);
                }
                rbtnode = su_rbt_search_atleast(rtrxbuf->rtb_buf, (void*)DBE_TRXID_GETLONG(prev_trxid));
            } else {
                rbtnode = su_rbt_succ(rtrxbuf->rtb_buf, rbtnode);
            }
        }
}
#endif /* DBE_REPLICATION */
