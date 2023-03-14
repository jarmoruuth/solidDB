/*************************************************************************\
**  source       * dbe7gtrs.c
**  directory    * dbe
**  description  * Global transaction state information.
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

#define DBE7GTRS_C

#include <ssc.h>
#include <ssmem.h>
#include <ssmemtrc.h>
#include <sssem.h>
#include <ssdebug.h>

#include <su0list.h>
#include <su0rbtr.h>

#include "dbe9type.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe7ctr.h"
#include "dbe7gtrs.h"
#include "dbe0db.h"

ss_debug(static long gtrs_nmergewrites_in;)
ss_debug(static long gtrs_nmergewrites_out;)

typedef struct {
        ss_debug(dbe_chk_t  ar_chk;)
        ulong               ar_relid;
        dbe_trxnum_t        ar_trxnum;
        su_list_node_t*     ar_node;
        su_rbt_node_t*      ar_rbtnode;
} gtrs_abortedrelh_t;

#define CHK_ABORTEDRELH(ar) ss_dassert(SS_CHKPTR(ar) && ar->ar_chk == DBE_CHK_ABORTEDRELH)

bool dbe_gtrs_mergecleanup_recovery;

static void abortedrelh_done(
        gtrs_abortedrelh_t* ar)
{
        CHK_ABORTEDRELH(ar);

        SsMemFree(ar);
}

static void gtrs_purge_abortedrelhs(
        dbe_gtrs_t*         gtrs,
        dbe_trxnum_t        trxnum)
{
        gtrs_abortedrelh_t* ar;
        su_list_node_t*     n;
        su_list_node_t*     nn;

        for (n = su_list_first(gtrs->gtrs_abortedrelhs_list);
             (n != NULL
              && ((nn = su_list_next(gtrs->gtrs_abortedrelhs_list, n))
                  || TRUE));
             n = nn) {
            ar = su_listnode_getdata(n);
            CHK_ABORTEDRELH(ar);
            if (DBE_TRXNUM_CMP_EX(ar->ar_trxnum, trxnum) < 0) {
                su_list_remove(gtrs->gtrs_abortedrelhs_list, n);
                su_rbt_delete(gtrs->gtrs_abortedrelhs_rbt, ar->ar_rbtnode);
                abortedrelh_done(ar);
            }
        }
}

/*#***********************************************************************\
 *
 *		gtrs_activetrxlistdeletefun
 *
 * Delete function for list containing gtrs_activetrx_t*.
 *
 * Parameters :
 *
 *	data - in, take
 *		Data element from the list, must be of type gtrs_activetrx_t*.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void gtrs_activetrxlistdeletefun(
        rs_sysi_t*          cd,
        dbe_gtrs_t*         gtrs,
        gtrs_activetrx_t*   at)
{
        ss_rc_dassert(at->at_nlink > 0, at->at_nlink);
        ss_rc_dassert(at->at_nlink <= 2, at->at_nlink);
        at->at_nlink--;
        if (at->at_nlink == 0) {
            ss_dassert(at->at_synclistnode == NULL);
            dbe_trxinfo_done(at->at_trxinfo, cd, at->at_trxbufsem);
            ss_debug(SsMemTrcFreeCallStk(at->at_callstack));

            if (gtrs->gtrs_ncachedats < GTRS_NCACHED_ATS) {
                at->at_trx = (dbe_trx_t *) gtrs->gtrs_atcache;
                gtrs->gtrs_atcache = at;
                gtrs->gtrs_ncachedats++;
            } else {
                rs_sysi_qmemctxfree(cd, at);
            }
        }
}

/*#***********************************************************************\
 *
 *		gtrs_synctrxlistdeletefun
 *
 * Delete function for list containing gtrs_synctrx_t*.
 *
 * Parameters :
 *
 *	data - in, take
 *		Data element from the list, must be of type gtrs_synctrx_t*.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void gtrs_synctrxlistdeletefun(void* data)
{
        gtrs_synctrx_t* st = data;

        SsMemFree(st);
}

/*#***********************************************************************\
 * 
 *		gtrs_abortedrelhs_rbt_compare
 * 
 * Compares two abortedrelh keys in rb-tree.
 * 
 * Parameters : 
 * 
 *		key1 - 
 *			
 *			
 *		key2 - 
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
static int gtrs_abortedrelhs_rbt_compare(void* key1, void* key2)
{
        gtrs_abortedrelh_t* ar1 = key1;
        gtrs_abortedrelh_t* ar2 = key2;

        CHK_ABORTEDRELH(ar1);
        CHK_ABORTEDRELH(ar2);

        return(su_rbt_long_compare(ar1->ar_relid, ar2->ar_relid));
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_init
 *
 *
 *
 * Parameters :
 *
 *	db - in, hold
 *
 *	trxbuf - in, hold
 *
 *	ctr - in, hold
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_gtrs_t* dbe_gtrs_init(
        dbe_db_t* db,
        dbe_trxbuf_t* trxbuf,
        dbe_counter_t* ctr)
{
        dbe_gtrs_t* gtrs;

        ss_dprintf_1(("dbe_gtrs_init\n"));

        gtrs = SSMEM_NEW(dbe_gtrs_t);

        gtrs->gtrs_db = db;
        gtrs->gtrs_trxbuf = trxbuf;
        gtrs->gtrs_ctr = ctr;
        gtrs->gtrs_validatetrxlist = su_list_init(NULL);
        su_list_startrecycle(gtrs->gtrs_validatetrxlist);
        gtrs->gtrs_activetrxlist = su_list_init(NULL);
        su_list_startrecycle(gtrs->gtrs_activetrxlist);
        gtrs->gtrs_readlevellist = su_list_init(NULL);
        su_list_startrecycle(gtrs->gtrs_readlevellist);
#ifdef SS_SYNC
        gtrs->gtrs_synctrxlist = su_list_init(gtrs_synctrxlistdeletefun);
        su_list_startrecycle(gtrs->gtrs_synctrxlist);
#endif
        gtrs->gtrs_abortedrelhs_list = su_list_init(NULL);
        ss_autotest(su_list_setmaxlen(gtrs->gtrs_abortedrelhs_list, 100000));
        su_list_startrecycle(gtrs->gtrs_abortedrelhs_list);
        gtrs->gtrs_abortedrelhs_rbt = su_rbt_init(gtrs_abortedrelhs_rbt_compare, NULL);
        ss_autotest(su_rbt_maxnodes(gtrs->gtrs_abortedrelhs_rbt, 100000));
        SsFlatMutexInit(&gtrs->gtrs_sem, SS_SEMNUM_DBE_GTRS);
        gtrs->gtrs_pendingmergewrites = 0;
        gtrs->gtrs_atcache = NULL;
        gtrs->gtrs_ncachedats = 0;
        ss_debug(gtrs->gtrs_insidemutex = FALSE);
        ss_debug(gtrs->gtrs_lastcommittrxnum = DBE_TRXNUM_NULL;)
        ss_debug(gtrs->gtrs_lastmaxtrxnum = DBE_TRXNUM_NULL;)
        ss_debug(gtrs->gtrs_lastaborttrxid = DBE_TRXID_NULL;)

        return(gtrs);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_done
 *
 *
 *
 * Parameters :
 *
 *	gtrs - in, take
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_gtrs_done(dbe_gtrs_t* gtrs)
{
        su_list_node_t* n;
        gtrs_activetrx_t* at;

#ifdef SS_DEBUG
        if (su_list_length(gtrs->gtrs_activetrxlist) != 0) {
            SsDbgPrintf("gtrs->gtrs_activetrxlist:\n");
            su_list_do_get(gtrs->gtrs_activetrxlist, n, at) {
                SsDbgPrintf("nmergewrites=%ld, at=%ld\n", at->at_nmergewrites, (long)at);
                SsMemTrcFprintCallStk(NULL, at->at_callstack, NULL);
            }
            ss_derror;
        }
#endif /* SS_DEBUG */
        ss_dassert(su_list_length(gtrs->gtrs_readlevellist) == 0);
        ss_dassert(su_list_length(gtrs->gtrs_validatetrxlist) == 0);
        ss_rc_dassert(gtrs->gtrs_pendingmergewrites == 0, gtrs->gtrs_pendingmergewrites);

        su_list_do_get(gtrs->gtrs_validatetrxlist, n, at) {
            gtrs_activetrxlistdeletefun(NULL, gtrs, at);
        }
        su_list_done(gtrs->gtrs_validatetrxlist);
        su_list_do_get(gtrs->gtrs_activetrxlist, n, at) {
            gtrs_activetrxlistdeletefun(NULL, gtrs, at);
        }
        su_list_done(gtrs->gtrs_activetrxlist);
        su_list_done(gtrs->gtrs_readlevellist);
#ifdef SS_SYNC
        su_list_done(gtrs->gtrs_synctrxlist);
#endif

        ss_dassert(gtrs_nmergewrites_in == gtrs_nmergewrites_out);

        while (gtrs->gtrs_atcache != NULL) {
            gtrs_activetrx_t*   at;

            at = gtrs->gtrs_atcache;
            gtrs->gtrs_atcache = (gtrs_activetrx_t *) at->at_trx;
            SsMemFree(at);
            gtrs->gtrs_ncachedats--;
        }
        ss_dassert(gtrs->gtrs_ncachedats == 0);
        
        gtrs_purge_abortedrelhs(gtrs, DBE_TRXNUM_MAX);
        su_list_done(gtrs->gtrs_abortedrelhs_list);
        su_rbt_done(gtrs->gtrs_abortedrelhs_rbt);

        SsFlatMutexDone(gtrs->gtrs_sem);

        SsMemFree(gtrs);
}

#ifdef SS_SYNC
/*#***********************************************************************\
 *
 *		gtrs_setminsynctuplevers
 *
 * Gets the history read level for the current trx.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	at -
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
static void gtrs_setminsynctuplevers(dbe_gtrs_t* gtrs, gtrs_activetrx_t* at)
{
        gtrs_synctrx_t* first_st;

        first_st = su_list_getfirst(gtrs->gtrs_synctrxlist);
        if (first_st == NULL) {
            /* No active trx, use current counter value. */
            at->at_minsynctuplevers = dbe_counter_getsynctupleversion(gtrs->gtrs_ctr);
            ss_poutput_2(rs_tuplenum_print_binary(2, "gtrs_setminsynctuplevers:from counter, minsynctuplevers %s\n", &at->at_minsynctuplevers));
        } else {
            /* Get minimum value from first list entry. */
            at->at_minsynctuplevers = first_st->st_startsynctuplevers;
            ss_poutput_2(rs_tuplenum_print_binary(2, "gtrs_setminsynctuplevers:from st_startsynctuplevers, minsynctuplevers %s\n", &at->at_minsynctuplevers));
        }

        at->at_synclistnode = NULL;
}

/*#***********************************************************************\
 *
 *		gtrs_beginsynctrx
 *
 * Marks a new transaction started for sync. Transactions are kept in
 * a list for maintaining the current histry read level.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	at -
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
static void gtrs_beginsynctrx(dbe_gtrs_t* gtrs, gtrs_activetrx_t* at)
{
        gtrs_synctrx_t* st;

        ss_dassert(at->at_synclistnode == NULL);

        st = SSMEM_NEW(gtrs_synctrx_t);

        st->st_startsynctuplevers = dbe_counter_getsynctupleversion(gtrs->gtrs_ctr);
        st->st_endp = FALSE;

        ss_poutput_2(rs_tuplenum_print_binary(2, "gtrs_beginsynctrx:st_startsynctuplevers %s\n", &st->st_startsynctuplevers));

        at->at_synclistnode = su_list_insertlast(gtrs->gtrs_synctrxlist, st);
}

/*#***********************************************************************\
 *
 *		gtrs_endsynctrx
 *
 * Ends sync transaction. Also updates history read level by removing
 * all ended transactions from the head of the queue. Note that this
 * read level mechanism is compatible with the normal transaction read
 * level, so both level go up at the same time.
 *
 * One problematic schedule that requires separate list and use of
 * activetrxlist or validatetrxlist would fail:
 *
 *      T1      T2      T3
 *      begin
 *      INS
 *              begin
 *              validate
 *      validate
 *      commit
 *                      begin
 *                      commit
 *              commit
 *
 * In above schedule T3 would get a wrong history read level if
 * activetrxlist would be used to give history read levels.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	at -
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
static void gtrs_endsynctrx(dbe_gtrs_t* gtrs, gtrs_activetrx_t* at)
{
        gtrs_synctrx_t* st;

        if (at->at_synclistnode != NULL) {
            st = su_listnode_getdata(at->at_synclistnode);
            st->st_endp = TRUE;
            at->at_synclistnode = NULL;

            while ((st = su_list_getfirst(gtrs->gtrs_synctrxlist)) != NULL) {
                if (st->st_endp) {
                    ss_poutput_2(rs_tuplenum_print_binary(2, "gtrs_endsynctrx:remove %s\n", &st->st_startsynctuplevers));
                    su_list_removefirst(gtrs->gtrs_synctrxlist);
                } else {
                    break;
                }
            }
        }
}

#endif /* SS_SYNC */

/*##**********************************************************************\
 *
 *		dbe_gtrs_begintrx
 *
 * Informs that a new transaction has been started.
 *
 * This function must called between calls dbe_gtrs_entertrxgate and
 * dbe_gtrs_exittrxgate.
 *
 * Parameters :
 *
 *	gtrs - in, use
 *		Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_gtrs_begintrx(dbe_gtrs_t* gtrs, dbe_trxinfo_t* trxinfo, rs_sysi_t* cd)
{
        gtrs_activetrx_t* at;
        SsSemT* trxbufsem;

        ss_dprintf_1(("dbe_gtrs_begintrx:maxtrxnum=%ld, usertrxid=%ld\n",
            DBE_TRXNUM_GETLONG(trxinfo->ti_maxtrxnum), DBE_TRXID_GETLONG(trxinfo->ti_usertrxid)));
        CHK_TRXINFO(trxinfo);

        SsFlatMutexLock(gtrs->gtrs_sem);

        ss_debug(gtrs->gtrs_insidemutex = TRUE);
        ss_dassert(trxinfo->ti_actlistnode == NULL);

        trxbufsem = dbe_trxbuf_getsembytrxid(gtrs->gtrs_trxbuf, trxinfo->ti_usertrxid);

        dbe_trxinfo_link(trxinfo, trxbufsem);

        if (gtrs->gtrs_atcache != NULL) {
            ss_dassert(gtrs->gtrs_ncachedats > 0);
            at = gtrs->gtrs_atcache;
            gtrs->gtrs_atcache = (gtrs_activetrx_t *) at->at_trx;
            gtrs->gtrs_ncachedats--;
        } else {
            at = rs_sysi_qmemctxalloc(cd, sizeof(gtrs_activetrx_t));
        }

        at->at_trxinfo = trxinfo;
        at->at_trxbufsem = trxbufsem;
        at->at_nmergewrites = 0L;
        at->at_iswrites = FALSE;
        at->at_isvalidate = FALSE;
        at->at_trx = NULL;
        at->at_trxuncertain = FALSE;
        at->at_nlink = 1;
        at->at_minsynctupleversused = FALSE;
        at->at_readlevelnode = NULL;
        ss_debug(at->at_callstack = SsMemTrcCopyCallStk());

#ifdef SS_SYNC
        gtrs_setminsynctuplevers(gtrs, at);
#endif /* SS_SYNC */

        trxinfo->ti_actlistnode = su_list_insertlast(
                                    gtrs->gtrs_activetrxlist,
                                    at);

        if (!DBE_TRXNUM_ISNULL(trxinfo->ti_maxtrxnum)) {
            dbe_gtrs_addtrxreadlevel_nomutex(gtrs, trxinfo);
        }

        ss_debug(gtrs->gtrs_insidemutex = FALSE);

        SsFlatMutexUnlock(gtrs->gtrs_sem);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_begintrxvalidate
 *
 * Informs that a transaction has entered to the validation phase.
 *
 * A list is maintained for transactions that are under validation.
 * When the status of all transactions below some transaction is known,
 * the max transaction number is updated.
 *
 * This function must called between calls dbe_gtrs_entertrxgate and
 * dbe_gtrs_exittrxgate.
 *
 * Parameters :
 *
 *	gtrs - in, use
 *		Database object.
 *
 *	trxinfo - in, hold
 *		Transaction info.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_gtrs_begintrxvalidate(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_begintrxvalidate:committrxnum=%ld\n", DBE_TRXNUM_GETLONG(trxinfo->ti_committrxnum)));
        CHK_TRXINFO(trxinfo);
        ss_dassert(gtrs->gtrs_insidemutex);
        ss_dassert(!DBE_TRXNUM_EQUAL(trxinfo->ti_committrxnum, DBE_TRXNUM_NULL));

#ifdef SS_DEBUG
        ss_dassert(DBE_TRXNUM_CMP_EX(gtrs->gtrs_lastcommittrxnum, trxinfo->ti_committrxnum) < 0);
        gtrs->gtrs_lastcommittrxnum = trxinfo->ti_committrxnum;
        if (su_list_length(gtrs->gtrs_validatetrxlist) > 0) {
            dbe_trxinfo_t* last_trxinfo;
            at = su_listnode_getdata(su_list_last(gtrs->gtrs_validatetrxlist));
            last_trxinfo = at->at_trxinfo;
            ss_dassert(DBE_TRXNUM_CMP_EX(last_trxinfo->ti_committrxnum, trxinfo->ti_committrxnum) < 0);
        }
#endif /* SS_DEBUG */

        at = su_listnode_getdata(trxinfo->ti_actlistnode);
        ss_rc_dassert(at->at_nlink > 0, at->at_nlink);
        ss_rc_dassert(at->at_nlink <= 2, at->at_nlink);
        at->at_nlink++;
        at->at_isvalidate = TRUE;

        su_list_insertlast(gtrs->gtrs_validatetrxlist, at);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_abortnovalidate
 *
 * Called for every transaction that is aborted without validating it.
 * Such transaction are added to the trxbuf, so merge can remove them
 * as soon as possible. They need not to be added to any orher list.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	trxinfo -
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
void dbe_gtrs_abortnovalidate(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        SS_NOTUSED(gtrs);
        CHK_TRXINFO(trxinfo);
        ss_dassert(!dbe_trxinfo_iscommitted(trxinfo));
}

/*#***********************************************************************\
 * 
 *		gtrs_removetrxreadlevel
 * 
 * Removes current active trx from the list.
 * 
 * Parameters : 
 * 
 *		gtrs - in, use
 *			
 *			
 *		ended_trxinfo - in, use
 *			
 *			
 *		commitp - in
 *			
 *			
 *		p_nmergewrites - in, out
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
static void gtrs_removetrxreadlevel(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* ended_trxinfo,
        bool commitp,
        long* p_nmergewrites)
{
        gtrs_activetrx_t* at;
        ss_debug(gtrs_activetrx_t* orig_at;)
        su_list_node_t* node;

        /* Update merge writes value.
         */
        at = su_listnode_getdata(ended_trxinfo->ti_actlistnode);
        ss_debug(orig_at = at);

        ss_dassert(at->at_readlevelnode != NULL);
        ss_dassert(at->at_trx == NULL || SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        node = su_list_prev(
                    gtrs->gtrs_readlevellist,
                    at->at_readlevelnode);
        if (node != NULL) {
            /* There is older transaction. Update the current transaction's
             * merge writes to the previously started transaction.
             */
            gtrs_activetrx_t* prev_at;
            prev_at = su_listnode_getdata(node);
            prev_at->at_nmergewrites += at->at_nmergewrites;
            at->at_nmergewrites = 0;
            if (commitp) {
                prev_at->at_nmergewrites += *p_nmergewrites;
                *p_nmergewrites = 0;
            }
            ss_bprintf_2(("gtrs_removetrxreadlevel:at->at_trxinfo->ti_maxtrxnum=%ld, prev_at->at_trxinfo->ti_maxtrxnum=%ld\n",
                at->at_trxinfo->ti_maxtrxnum, prev_at->at_trxinfo->ti_maxtrxnum));
        } else {
            gtrs_activetrx_t* first_at;
            su_list_node_t* first_node;

            first_node = su_list_first(gtrs->gtrs_readlevellist);
            if (first_node != NULL) {
                first_at = su_listnode_getdata(first_node);
            } else {
                first_at = NULL;
            }
            ss_bassert(first_at == at);
            if (first_at != NULL && first_at == at) {
                first_node = su_list_next(gtrs->gtrs_readlevellist, first_node);
                if (first_node != NULL) {
                    first_at = su_listnode_getdata(first_node);
                } else {
                    first_at = NULL;
                }
            }
            ss_bassert(first_at != at);
            if (first_at == NULL ||
                DBE_TRXNUM_CMP_EX(first_at->at_trxinfo->ti_maxtrxnum, at->at_trxinfo->ti_maxtrxnum) > 0)
            {
                /* This is the oldest active transaction. Update the current
                 * transaction's cumulated merge writes to the db object.
                 * The actual db object update is done when transaction state
                 * is correctly set.
                 */
                *p_nmergewrites += at->at_nmergewrites;
            } else {
                ss_bprintf_2(("gtrs_removetrxreadlevel:at->at_trxinfo->ti_maxtrxnum=%ld, first_at->at_trxinfo->ti_maxtrxnum=%ld\n",
                    at->at_trxinfo->ti_maxtrxnum, first_at->at_trxinfo->ti_maxtrxnum));
                /* ss_bassert(DBE_TRXNUM_EQUAL(first_at->at_trxinfo->ti_maxtrxnum, at->at_trxinfo->ti_maxtrxnum)); */
                first_at->at_nmergewrites += at->at_nmergewrites;
                if (commitp) {
                    first_at->at_nmergewrites += *p_nmergewrites;
                    *p_nmergewrites = 0;
                }
            }
            at->at_nmergewrites = 0;
        }

        ss_dassert(orig_at == at);

        /* Remove the current trx read level.
         */
        su_list_remove(gtrs->gtrs_readlevellist, at->at_readlevelnode);
        at->at_readlevelnode = NULL;
        at->at_trxinfo->ti_maxtrxnum = DBE_TRXNUM_NULL;
}

/*#***********************************************************************\
 * 
 *		gtrs_removeactivetrx
 * 
 * Removes current active trx from the list.
 * 
 * Parameters : 
 * 
 *		gtrs - in, use
 *			
 *			
 *		ended_trxinfo - in, use
 *			
 *			
 *		commitp - in
 *			
 *			
 *		iswrites - in
 *			
 *			
 *		p_nmergewrites - in, out
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
static void gtrs_removeactivetrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* ended_trxinfo,
        rs_sysi_t* cd,
        bool commitp,
        bool iswrites,
        long* p_nmergewrites)
{
        gtrs_activetrx_t* at;

        ss_dprintf_3(("gtrs_removeactivetrx\n"));

        at = su_listnode_getdata(ended_trxinfo->ti_actlistnode);

        ss_dassert(at->at_trx == NULL || SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        ss_dassert(!at->at_iswrites);
        at->at_iswrites = iswrites;

        if (at->at_readlevelnode != NULL) {
            gtrs_removetrxreadlevel(gtrs, ended_trxinfo, commitp, p_nmergewrites);
        }

        if (!at->at_isvalidate) {
            ss_dassert(dbe_trxinfo_canremovereadlevel(ended_trxinfo) ? at->at_synclistnode == NULL : TRUE);
            gtrs_endsynctrx(gtrs, at);
        }

        /* Remove the current trx.
         */
        su_list_remove(gtrs->gtrs_activetrxlist, ended_trxinfo->ti_actlistnode);
        gtrs_activetrxlistdeletefun(cd, gtrs, at);
        ended_trxinfo->ti_actlistnode = NULL;
}

/*#***********************************************************************\
 * 
 *		gtrs_releasemergewrites
 * 
 * Tries to release merge writes by ending transaction that are marked
 * as stoppable.
 * 
 * Parameters : 
 * 
 *		gtrs - 
 *			
 *			
 *		mergewrites - 
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
static bool gtrs_releasemergewrites(dbe_gtrs_t* gtrs, long* p_nmergewrites)
{
        long curmergewrites;
        long nmergewrites;
        long mergelimit;
        bool releasep = FALSE;

        curmergewrites = dbe_db_getmergewrites(gtrs->gtrs_db) +
                         *p_nmergewrites +
                         gtrs->gtrs_pendingmergewrites;

        ss_dprintf_2(("gtrs_releasemergewrites:curmergewrites=%ld\n", curmergewrites));

        mergelimit = dbe_db_getmergelimit(gtrs->gtrs_db);

        FAKE_CODE_BLOCK_GT(FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT, 0,
        {
            mergelimit = FAKE_GETVAL(FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT);
            ss_dprintf_1(("gtrs_releasemergewrites:FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT, mergelimit=%ld\n", mergelimit));
        });

        if (curmergewrites >= mergelimit) {
            gtrs_activetrx_t* at;
            gtrs_activetrx_t* prev_at = NULL;

            for (;;) {
                at = su_list_getfirst(gtrs->gtrs_readlevellist);
                if (at == NULL || at == prev_at) {
                    /* Empty list or gtrs_removetrxreadlevel did not remove anything.
                     */
                    break;
                }
                if (!dbe_trxinfo_canremovereadlevel(at->at_trxinfo)
                    || at->at_iswrites 
                    || at->at_isvalidate)
                {
                    break;
                }

                ss_dprintf_2(("gtrs_releasemergewrites:release current merge level\n"));

                /* Now we should have active trx (at) which is read only and we can
                 * release the read level.
                 */
                nmergewrites = 0;

                gtrs_removetrxreadlevel(
                    gtrs,
                    at->at_trxinfo,
                    FALSE,  /* commitp */
                    &nmergewrites);

                *p_nmergewrites += nmergewrites;
                releasep = TRUE;

                prev_at = at;
            }
        }
        return(releasep);
}

/*##***********************************************************************\
 *
 *		dbe_gtrs_endtrx
 *
 * Notifies that a transaction has ended. Maximum transaction number
 * and base transaction number are updated if possible.
 *
 * Parameters :
 *
 *	gtrs - in out, use
 *
 *
 *	ended_trxinfo - in
 *		Trx info of the ended transaction.
 *
 *	commitp - in
 *
 *
 *	nmergewrites - in
 *
 *
 *	updatetrxinfo - in
 *		If TRUE updates trxinfo with commit or abort status.
 *		In HSB G2 secondary in read only transactions we do
 *		not want to update the status because the transaction
 *		may be shared with some active HSB transaction.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_gtrs_endtrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* ended_trxinfo,
        rs_sysi_t* cd,
        bool commitp,
        bool iswrites,
        long nmergewrites,
        bool updatetrxinfo,
        bool entergate)
{
        su_list_node_t* node;
        dbe_trxinfo_t* trxinfo;
        dbe_trxnum_t mergetrxnum = DBE_TRXNUM_NULL;
        dbe_trxnum_t maxtrxnum = DBE_TRXNUM_NULL;
        dbe_trxid_t aborttrxid = DBE_TRXID_NULL;
        gtrs_activetrx_t* at;
        bool setaborttrxid = FALSE;

        ss_bprintf_1(("dbe_gtrs_endtrx:commitp=%d, trxid=%ld, trxnum=%ld, nmergewrites=%ld, updatetrxinfo=%d\n",
            commitp, DBE_TRXID_GETLONG(ended_trxinfo->ti_usertrxid), DBE_TRXNUM_GETLONG(ended_trxinfo->ti_committrxnum), nmergewrites,
            updatetrxinfo));
        CHK_TRXINFO(ended_trxinfo);
        ss_dassert(nmergewrites >= 0);

        if (entergate) {
            SsFlatMutexLock(gtrs->gtrs_sem);
            ss_debug(gtrs->gtrs_insidemutex = TRUE);
        } else {
            ss_dassert(gtrs->gtrs_insidemutex);
        }

        if (ended_trxinfo->ti_actlistnode == NULL) {
            ss_bprintf_2(("dbe_gtrs_endtrx:ended_trxinfo->ti_actlistnode == NULL, nmergewrites=%ld\n", nmergewrites));
            /* MUTEX END */
            ss_debug(gtrs->gtrs_insidemutex = FALSE);
            SsFlatMutexUnlock(gtrs->gtrs_sem);
            return;
        }

        ss_debug(gtrs_nmergewrites_in += nmergewrites;)
        gtrs->gtrs_pendingmergewrites += nmergewrites;

        if (updatetrxinfo) {
            if (commitp) {
                ss_dprintf_4(("dbe_gtrs_endtrx:dbe_trxinfo_setcommitted\n"));
                dbe_trxinfo_setcommitted(ended_trxinfo);
            } else {
                ss_dprintf_4(("dbe_gtrs_endtrx:dbe_trxinfo_setaborted\n"));
                if (iswrites && dbe_trxnum_isnull(ended_trxinfo->ti_committrxnum)) {
                    ended_trxinfo->ti_committrxnum = dbe_counter_getnewcommittrxnum(gtrs->gtrs_ctr);
                }
                dbe_trxinfo_setaborted(ended_trxinfo);
            }
        }

        /* Update max trx number if there are ended transactions at the
         * head of the list.
         */
        while ((node = su_list_first(gtrs->gtrs_validatetrxlist)) != NULL) {
            at = su_listnode_getdata(node);
            trxinfo = at->at_trxinfo;
            CHK_TRXINFO(trxinfo);
            if (dbe_trxinfo_isended(trxinfo)) {
                ss_dassert(!DBE_TRXNUM_EQUAL(trxinfo->ti_committrxnum, DBE_TRXNUM_NULL));
                maxtrxnum = trxinfo->ti_committrxnum;
                gtrs_endsynctrx(gtrs, at);
                su_list_remove(gtrs->gtrs_validatetrxlist, node);
                gtrs_activetrxlistdeletefun(cd, gtrs, at);
            } else if (ended_trxinfo == trxinfo) {
                /* Always remove current trxinfo. */
                ss_dassert(!updatetrxinfo);
                gtrs_endsynctrx(gtrs, at);
                su_list_remove(gtrs->gtrs_validatetrxlist, node);
                gtrs_activetrxlistdeletefun(cd, gtrs, at);
            } else {
                break;
            }
        }
        if (DBE_TRXNUM_EQUAL(maxtrxnum, DBE_TRXNUM_NULL)) {
            if (commitp
                && !DBE_TRXNUM_EQUAL(ended_trxinfo->ti_committrxnum, DBE_TRXNUM_NULL)
                && su_list_length(gtrs->gtrs_validatetrxlist) == 0) 
            {
                /* No transactions at validate phase. We may have MME only transactions,
                 * update read level to current commit level.
                 */
                dbe_counter_setmaxtrxnum(gtrs->gtrs_ctr, ended_trxinfo->ti_committrxnum);

                ss_bprintf_2(("dbe_gtrs_endtrx:new maxtrxnum=%ld (empty validate list)\n", DBE_TRXNUM_GETLONG(ended_trxinfo->ti_committrxnum)));
            }
        } else {
            /* Set the new read level.
             */
            dbe_counter_setmaxtrxnum(gtrs->gtrs_ctr, maxtrxnum);

            ss_bprintf_2(("dbe_gtrs_endtrx:new maxtrxnum=%ld\n", DBE_TRXNUM_GETLONG(maxtrxnum)));
        }

        gtrs_removeactivetrx(
            gtrs,
            ended_trxinfo,
            cd,
            commitp,
            iswrites,
            &nmergewrites);

        if (commitp && iswrites) {
            gtrs_releasemergewrites(gtrs, &nmergewrites);
        }

        /* Update merge trx number. First read off all ended transactions
         * and remember the last trx number that not ended.
         */
        node = su_list_first(gtrs->gtrs_readlevellist);
        while (node != NULL) {
            at = su_listnode_getdata(node);
            CHK_TRXINFO(at->at_trxinfo);

            ss_dassert(!DBE_TRXNUM_EQUAL(at->at_trxinfo->ti_maxtrxnum, DBE_TRXNUM_NULL));
            mergetrxnum = at->at_trxinfo->ti_maxtrxnum;
            if (dbe_trxinfo_isended(at->at_trxinfo)) {
                node = su_list_next(gtrs->gtrs_readlevellist, node);
            } else {
                break;
            }
        }
        if (node == NULL) {
            /* There are no unfinished transactions. All transactions are ended.
             */
            mergetrxnum = dbe_counter_getmaxtrxnum(gtrs->gtrs_ctr);
        }
        if (!DBE_TRXNUM_EQUAL(mergetrxnum, DBE_TRXNUM_NULL)) {
            /* Set the new merge level.
             */
            ss_dprintf_2(("dbe_gtrs_endtrx:new mergetrxnum=%ld\n", DBE_TRXNUM_GETLONG(mergetrxnum)));
            if (!dbe_gtrs_mergecleanup_recovery) {
                dbe_counter_setmergetrxnum(gtrs->gtrs_ctr, mergetrxnum);
            }
            gtrs_purge_abortedrelhs(gtrs, mergetrxnum);

            /* Update abort trx id. First read off all ended transactions
             * and remember the last trx number that was smaller than the
             * last trx number.
             */
            node = su_list_first(gtrs->gtrs_activetrxlist);
            while (node != NULL) {
                at = su_listnode_getdata(node);
                CHK_TRXINFO(at->at_trxinfo);
                if (dbe_trxinfo_isended(at->at_trxinfo)) {
                    node = su_list_next(gtrs->gtrs_activetrxlist, node);
                } else {
                    if (at->at_iswrites) {
                        aborttrxid = at->at_trxinfo->ti_usertrxid;
                        aborttrxid = DBE_TRXID_SUM(aborttrxid, -1);
                    }
                    break;
                }
            }
            if (node == NULL || DBE_TRXID_EQUAL(aborttrxid, DBE_TRXID_NULL)) {
                /* There are no unfinished transactions. All transactions are ended.
                 */
                aborttrxid = dbe_counter_gettrxid(gtrs->gtrs_ctr);
            }
            if (!DBE_TRXID_EQUAL(aborttrxid, DBE_TRXID_NULL)) {
                /* Update also aborted transaction id in trxbuf.
                 */
                ss_dprintf_2(("dbe_gtrs_endtrx:new aborttrxid=%ld\n", DBE_TRXID_GETLONG(aborttrxid)));
                ss_dassert(DBE_TRXID_CMP_EX(dbe_counter_gettrxid(gtrs->gtrs_ctr), aborttrxid) >= 0);
                ss_dassert(DBE_TRXID_CMP_EX(gtrs->gtrs_lastaborttrxid, aborttrxid) <= 0);
                ss_debug(gtrs->gtrs_lastaborttrxid = aborttrxid);
                setaborttrxid = TRUE;
            }
        }
        
        gtrs->gtrs_pendingmergewrites -= nmergewrites;
        ss_rc_dassert(gtrs->gtrs_pendingmergewrites >= 0, gtrs->gtrs_pendingmergewrites);

        ss_debug(gtrs_nmergewrites_out += nmergewrites;)
        ss_dprintf_4(("dbe_gtrs_endtrx:gtrs_nmergewrites_in=%ld, gtrs_nmergewrites_out=%ld\n",
            gtrs_nmergewrites_in, gtrs_nmergewrites_out));
        ss_dassert(su_list_length(gtrs->gtrs_readlevellist) != 0 ||
                   gtrs_nmergewrites_in == gtrs_nmergewrites_out);

        /* MUTEX END */
        ss_debug(gtrs->gtrs_insidemutex = FALSE);
        SsFlatMutexUnlock(gtrs->gtrs_sem);

        if (setaborttrxid) {
            dbe_trxbuf_setaborttrxid(gtrs->gtrs_trxbuf, aborttrxid);
        }

        ss_dassert(nmergewrites >= 0);

        if (nmergewrites > 0) {
            /* Update merge writes to the database object.
             */
            ss_bprintf_2(("dbe_gtrs_endtrx:nmergewrites=%ld\n", nmergewrites));
            dbe_db_addmergewrites(gtrs->gtrs_db, nmergewrites);
        }
}

/*#***********************************************************************\
 * 
 *		gtrs_releasereadlevels_nomutex
 * 
 * Nomutex version for releasing the read level.
 * 
 * Parameters : 
 * 
 *		gtrs - 
 *			
 *			
 *		p_nmergewrites - 
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
static bool gtrs_releasereadlevels_nomutex(dbe_gtrs_t* gtrs, long* p_nmergewrites)
{
        long nmergewrites = 0;
        bool releasep;

        ss_bprintf_1(("gtrs_releasereadlevels_nomutex\n"));

        ss_dassert(gtrs->gtrs_insidemutex);
        
        releasep = gtrs_releasemergewrites(gtrs, &nmergewrites);

        gtrs->gtrs_pendingmergewrites -= nmergewrites;
        ss_rc_dassert(gtrs->gtrs_pendingmergewrites >= 0, gtrs->gtrs_pendingmergewrites);

        ss_debug(gtrs_nmergewrites_out += nmergewrites;)
        ss_dprintf_4(("gtrs_releasereadlevels_nomutex:gtrs_nmergewrites_in=%ld, gtrs_nmergewrites_out=%ld\n",
            gtrs_nmergewrites_in, gtrs_nmergewrites_out));
        ss_dassert(su_list_length(gtrs->gtrs_readlevellist) != 0 ||
                   gtrs_nmergewrites_in == gtrs_nmergewrites_out);

        *p_nmergewrites = nmergewrites;

        return(releasep);
}

/*##**********************************************************************\
 * 
 *		dbe_gtrs_releasereadlevels
 * 
 * Tries to release read levels and thus release writes to the merge 
 * process.
 * 
 * Parameters : 
 * 
 *		gtrs - 
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
bool dbe_gtrs_releasereadlevels(dbe_gtrs_t* gtrs)
{
        long nmergewrites = 0;
        bool merge_level_changed;

        ss_bprintf_1(("gtrs_releasereadlevels\n"));

        /* MUTEX BEGIN */
        SsFlatMutexLock(gtrs->gtrs_sem);
        ss_debug(gtrs->gtrs_insidemutex = TRUE);

        merge_level_changed = gtrs_releasereadlevels_nomutex(gtrs, &nmergewrites);

        /* MUTEX END */
        ss_debug(gtrs->gtrs_insidemutex = FALSE);
        SsFlatMutexUnlock(gtrs->gtrs_sem);

        if (nmergewrites > 0) {
            /* Update merge writes to the database object.
             */
            ss_bprintf_2(("dbe_gtrs_releasereadlevels:nmergewrites=%ld\n", nmergewrites));
            dbe_db_addmergewrites(gtrs->gtrs_db, nmergewrites);
        }
        return(merge_level_changed);
}

/*##**********************************************************************\
 * 
 *		dbe_gtrs_releasemyreadlevelif_nomutex
 * 
 * Releases the read level of current transaction if it holds more
 * transaction writes than specified by mergelimit.
 * 
 * Parameters : 
 * 
 *		gtrs - 
 *			
 *			
 *		p_nmergewrites - 
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
bool dbe_gtrs_releasemyreadlevelif_nomutex(
        dbe_gtrs_t* gtrs, 
        dbe_trxinfo_t* trxinfo, 
        long* p_nmergewrites)
{
        bool released = FALSE;
        gtrs_activetrx_t* at;
        long mergelimit;

        ss_bprintf_1(("dbe_gtrs_releasemyreadlevelif_nomutex\n"));
        ss_dassert(gtrs->gtrs_insidemutex);
        ss_bassert(trxinfo->ti_actlistnode != NULL);

        mergelimit = dbe_db_getmergelimit(gtrs->gtrs_db);

        FAKE_CODE_BLOCK_GT(FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT, 0,
        {
            mergelimit = FAKE_GETVAL(FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT);
            ss_dprintf_1(("dbe_gtrs_releasemyreadlevelif_nomutex:FAKE_DBE_REMOVEREADLEVEL_MERGELIMIT, mergelimit=%ld\n", mergelimit));
        });


        at = su_listnode_getdata(trxinfo->ti_actlistnode);

        if (at->at_nmergewrites >= mergelimit) {
            ss_bprintf_2(("dbe_gtrs_releasemyreadlevelif_nomutex:release, at_nmergewrites=%ld, dbe_db_getmergelimit()=%ld\n",
                (long)at->at_nmergewrites, mergelimit));
            released = gtrs_releasereadlevels_nomutex(gtrs, p_nmergewrites);
            ss_dassert(*p_nmergewrites > 0 ? released : TRUE);
            if (released) {
                SS_RTCOVERAGE_INC(SS_RTCOV_DBE_REMOVEREADLEVEL);
            }
        }

        return(released);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_getcount
 *
 * Returns counts of active transactions and transactions under validation.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	p_activecount - out
 *
 *
 *	p_validatecount - out
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
void dbe_gtrs_getcount(
        dbe_gtrs_t* gtrs,
        ulong* p_activecount,
        ulong* p_validatecount)
{
        SsFlatMutexLock(gtrs->gtrs_sem);

        *p_activecount = su_list_length(gtrs->gtrs_activetrxlist);
        *p_validatecount = su_list_length(gtrs->gtrs_validatetrxlist);

        SsFlatMutexUnlock(gtrs->gtrs_sem);
}

#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *		dbe_gtrs_printinfo
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	gtrs -
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
void dbe_gtrs_printinfo(void* fp, dbe_gtrs_t* gtrs)
{
        int len;
        su_list_node_t* node;
        dbe_trxinfo_t* trxinfo;
        dbe_trxnum_t first_tn;
        dbe_trxnum_t last_tn;
        dbe_trxid_t first_tid;
        dbe_trxid_t last_tid;
        gtrs_activetrx_t* at;

        SsFlatMutexLock(gtrs->gtrs_sem);

        len = su_list_length(gtrs->gtrs_activetrxlist);
        SsFprintf(fp, "  Activelist   length %d ", len);
        if (len == 0) {
            SsFprintf(fp, "\n");
        } else {
            long mergewrites;

            node = su_list_first(gtrs->gtrs_activetrxlist);
            at = su_listnode_getdata(node);
            first_tid = at->at_trxinfo->ti_usertrxid;

            node = su_list_last(gtrs->gtrs_activetrxlist);
            at = su_listnode_getdata(node);
            last_tid = at->at_trxinfo->ti_usertrxid;

            node = su_list_first(gtrs->gtrs_readlevellist);
            if (node != NULL) {
                at = su_listnode_getdata(node);
                first_tn = at->at_trxinfo->ti_maxtrxnum;
            } else {
                first_tn = DBE_TRXNUM_NULL;
            }

            node = su_list_last(gtrs->gtrs_readlevellist);
            if (node != NULL) {
                at = su_listnode_getdata(node);
                last_tn = at->at_trxinfo->ti_maxtrxnum;
            } else {
                last_tn = DBE_TRXNUM_NULL;
            }

            mergewrites = 0;
            su_list_do_get(gtrs->gtrs_activetrxlist, node, at) {
                mergewrites += at->at_nmergewrites;
            }

            SsFprintf(fp, "  Maxtn first %ld last %ld MrgWrites %ld Usertrxid first %ld last %ld \n",
                DBE_TRXNUM_GETLONG(first_tn), DBE_TRXNUM_GETLONG(last_tn), mergewrites, DBE_TRXID_GETLONG(first_tid), DBE_TRXID_GETLONG(last_tid));
        }

        len = su_list_length(gtrs->gtrs_validatetrxlist);
        SsFprintf(fp, "  Validatelist length %d\n", len);
        if (len == 0) {
            SsFprintf(fp, "\n");
        } else {
            gtrs_activetrx_t* at;
            node = su_list_first(gtrs->gtrs_validatetrxlist);
            at = su_listnode_getdata(node);
            trxinfo = at->at_trxinfo;
            first_tn = trxinfo->ti_committrxnum;
            first_tid = trxinfo->ti_usertrxid;

            node = su_list_last(gtrs->gtrs_validatetrxlist);
            at = su_listnode_getdata(node);
            trxinfo = at->at_trxinfo;
            last_tn = trxinfo->ti_committrxnum;
            last_tid = trxinfo->ti_usertrxid;

            SsFprintf(fp, "  Committn first %ld last %ld Usertrxid first %ld last %ld\n",
                DBE_TRXNUM_GETLONG(first_tn), DBE_TRXNUM_GETLONG(last_tn), DBE_TRXID_GETLONG(first_tid), DBE_TRXID_GETLONG(last_tid));
        }

        SsFlatMutexUnlock(gtrs->gtrs_sem);
}
#endif /* SS_LIGHT */

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *		dbe_gtrs_getminsynctuplevers
 *
 * Returns minimum sync tuple version that is visible to calling
 * transaction.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	trxinfo -
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
rs_tuplenum_t dbe_gtrs_getminsynctuplevers(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_getminsynctuplevers\n"));
        ss_bassert(trxinfo->ti_actlistnode != NULL);

        at = su_listnode_getdata(trxinfo->ti_actlistnode);

        at->at_minsynctupleversused = TRUE;

        ss_poutput_2(rs_tuplenum_print_binary(2, "dbe_gtrs_getminsynctuplevers:tnum %s\n", &at->at_minsynctuplevers));

        return(at->at_minsynctuplevers);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_getfirstsynctuplevers
 *
 * Returns the first sync tuple version that is used by some transaction.
 * This is the history cleanup level.
 *
 * Parameters :
 *
 *	gtrs -
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
rs_tuplenum_t dbe_gtrs_getfirstsynctuplevers(
        dbe_gtrs_t* gtrs)
{
        rs_tuplenum_t tuplevers;
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_getfirstsynctuplevers\n"));

        /* MUTEX BEGIN */
        SsFlatMutexLock(gtrs->gtrs_sem);

        at = su_list_getfirst(gtrs->gtrs_activetrxlist);
        ss_bassert(at != NULL);

        at->at_minsynctupleversused = TRUE;

        tuplevers = at->at_minsynctuplevers;

        /* MUTEX END */
        SsFlatMutexUnlock(gtrs->gtrs_sem);

        ss_poutput_2(rs_tuplenum_print_binary(2, "dbe_gtrs_getfirstsynctuplevers:tnum %s\n", &tuplevers));

        return(tuplevers);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_getnewsynctuplevers
 *
 * Returns a new sync tuple version. This function also maintains a
 * list of active sync transactions for history read levels. If the
 * current transaction is not already in the list, it is added to the
 * list before a new sync tuple version is allocated.
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 *	trxinfo -
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
rs_tuplenum_t dbe_gtrs_getnewsynctuplevers(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        gtrs_activetrx_t* at;
        rs_tuplenum_t synctuplevers;

        ss_dassert(trxinfo->ti_actlistnode != NULL);

        at = su_listnode_getdata(trxinfo->ti_actlistnode);

        /* MUTEX BEGIN */
        SsFlatMutexLock(gtrs->gtrs_sem);

        if (at->at_synclistnode == NULL) {

            gtrs_beginsynctrx(gtrs, at);
            ss_dassert(at->at_synclistnode != NULL);

        }

        synctuplevers = dbe_counter_getnewsynctupleversion(gtrs->gtrs_ctr);

        ss_poutput_2(rs_tuplenum_print_binary(2, "dbe_gtrs_getnewsynctuplevers:tnum %s\n", &synctuplevers));

        /* MUTEX END */
        SsFlatMutexUnlock(gtrs->gtrs_sem);

        return(synctuplevers);
}

void dbe_gtrs_settrxuncertain(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_settrxuncertain\n"));
        ss_dassert(trxinfo->ti_actlistnode != NULL);

        at = su_listnode_getdata(trxinfo->ti_actlistnode);

        /* MUTEX BEGIN */
        SsFlatMutexLock(gtrs->gtrs_sem);

        at->at_trxuncertain = TRUE;

        /* MUTEX END */
        SsFlatMutexUnlock(gtrs->gtrs_sem);
}

void dbe_gtrs_setactivetrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo,
        dbe_trx_t* trx)
{
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_setactivetrx\n"));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        if (trxinfo->ti_actlistnode != NULL) {

            ss_dprintf_2(("dbe_gtrs_setactivetrx:adding\n"));

            at = su_listnode_getdata(trxinfo->ti_actlistnode);

            /* MUTEX BEGIN */
            SsFlatMutexLock(gtrs->gtrs_sem);

            at->at_trx = trx;

            /* MUTEX END */
            SsFlatMutexUnlock(gtrs->gtrs_sem);
        }
}

su_list_t* dbe_gtrs_getactivetrxlist(
        dbe_gtrs_t* gtrs)
{
        su_list_t* list;
        su_list_node_t* n;
        gtrs_activetrx_t* at;

        ss_dprintf_1(("dbe_gtrs_getactivetrxlist\n"));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        list = su_list_init(NULL);

        SsFlatMutexLock(gtrs->gtrs_sem);

        su_list_do_get(gtrs->gtrs_activetrxlist, n, at) {
            if (at->at_trx != NULL) {
                su_list_insertlast(list, at->at_trx);
            }
        }

        SsFlatMutexUnlock(gtrs->gtrs_sem);

        return(list);
}

su_list_t* dbe_gtrs_getuncertaintrxlist(
        dbe_gtrs_t* gtrs)
{
        su_list_t* list;
        su_list_node_t* n;
        gtrs_activetrx_t* at;

        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        list = su_list_init(NULL);

        SsFlatMutexLock(gtrs->gtrs_sem);

        su_list_do_get(gtrs->gtrs_activetrxlist, n, at) {
            if (at->at_trx != NULL) {
                su_list_insertlast(list, at->at_trx);
            }
        }

        SsFlatMutexUnlock(gtrs->gtrs_sem);

        return(list);
}

#endif /* SS_SYNC */

dbe_opentrxinfo_t* dbe_gtrs_getopentrxinfo_nomutex(
        dbe_gtrs_t* gtrs)
{
        su_list_node_t* n;
        gtrs_activetrx_t* at;
        dbe_opentrxinfo_t* oti;
        int i;
        int len;

        ss_dprintf_1(("dbe_gtrs_getopentrxinfo\n"));
        ss_dassert(gtrs->gtrs_insidemutex);

        if (gtrs == NULL) {
            /* some low level tests do not have gtrs */
            return(NULL);
        }

        len = su_list_length(gtrs->gtrs_activetrxlist);
        
        oti = SsMemAlloc(sizeof(dbe_opentrxinfo_t) + len * sizeof(oti->oti_table[0]));
        oti->oti_len = len;

        i = 0;
        su_list_do_get(gtrs->gtrs_activetrxlist, n, at) {
            oti->oti_table[i++] = at->at_trxinfo->ti_usertrxid;
        }

        if (len > 0) {
            oti->oti_tablemin = oti->oti_table[0];
            oti->oti_tablemax = oti->oti_table[0];
            for (i = 1; i < len; i++) {
                if (dbe_trxid_cmp(oti->oti_table[i], oti->oti_tablemin) < 0) {
                    oti->oti_tablemin = oti->oti_table[i];
                }
                if (dbe_trxid_cmp(oti->oti_table[i], oti->oti_tablemax) > 0) {
                    oti->oti_tablemax = oti->oti_table[i];
                }
            }
        }

        return(oti);
}

void dbe_gtrs_freeopentrxinfo(
        dbe_opentrxinfo_t* oti)
{
        SsMemFree(oti);
}

dbe_ret_t dbe_gtrs_readrelh(
        dbe_gtrs_t*             gtrs,
        ulong                   relid,
        dbe_trxnum_t            trxnum)
{
        gtrs_abortedrelh_t*     ar;
        gtrs_abortedrelh_t      search_ar;
        su_rbt_node_t*          n;

        SsFlatMutexLock(gtrs->gtrs_sem);

        ss_dprintf_1(("dbe_gtrs_readrelh:relid=%ld, trxnum=%ld\n", relid, DBE_TRXNUM_GETLONG(trxnum)));

        ss_debug(search_ar.ar_chk = DBE_CHK_ABORTEDRELH);
        search_ar.ar_relid = relid;

        n = su_rbt_search(gtrs->gtrs_abortedrelhs_rbt, &search_ar);

        if (n == NULL) {
            ss_dprintf_2(("dbe_gtrs_readrelh:SU_TRIE_KEY_NOT_FOUND, SU_SUCCESS\n"));
            SsFlatMutexUnlock(gtrs->gtrs_sem);

            return SU_SUCCESS;
        }

        ar = su_rbtnode_getkey(n);
        CHK_ABORTEDRELH(ar);

        if (DBE_TRXNUM_ISNULL(trxnum)
            || DBE_TRXNUM_CMP_EX(ar->ar_trxnum, trxnum) <= 0) 
        {
            ss_dprintf_2(("dbe_gtrs_readrelh:found, trxnum comparison, SU_SUCCESS\n"));
            SsFlatMutexUnlock(gtrs->gtrs_sem);

            return SU_SUCCESS;
        }

        ss_dprintf_2(("dbe_gtrs_readrelh:found, trxnum comparison, DBE_ERR_DDOPNEWERTRX\n"));

        SsFlatMutexUnlock(gtrs->gtrs_sem);

        return DBE_ERR_DDOPNEWERTRX;
}

void dbe_gtrs_abortrelh(
        dbe_gtrs_t*             gtrs,
        ulong                   relid,
        dbe_trxnum_t            trxnum)
{
        gtrs_abortedrelh_t*     ar;
        gtrs_abortedrelh_t      search_ar;
        su_rbt_node_t*          n;

        SsFlatMutexLock(gtrs->gtrs_sem);

        ss_dprintf_1(("dbe_gtrs_abortrelh:relid=%ld, trxnum=%ld\n", relid, DBE_TRXNUM_GETLONG(trxnum)));

        ss_debug(search_ar.ar_chk = DBE_CHK_ABORTEDRELH);
        search_ar.ar_relid = relid;

        n = su_rbt_search(gtrs->gtrs_abortedrelhs_rbt, &search_ar);

        if (n != NULL) {
            ss_dprintf_2(("dbe_gtrs_abortrelh:found\n"));
            ar = su_rbtnode_getkey(n);
            CHK_ABORTEDRELH(ar);
            if (DBE_TRXNUM_CMP_EX(ar->ar_trxnum, trxnum) < 0) {
                ss_dprintf_2(("dbe_gtrs_abortrelh:update trxnum\n"));
                ar->ar_trxnum = trxnum;
            }
            ss_dassert(DBE_TRXNUM_CMP_EX(ar->ar_trxnum, trxnum) <= 0);
        } else {
            ss_dprintf_2(("dbe_gtrs_abortrelh:add new\n"));
            ar = SSMEM_NEW(gtrs_abortedrelh_t);

            ar->ar_relid = relid;
            ar->ar_trxnum = trxnum;
            ar->ar_node = su_list_insertfirst(
                    gtrs->gtrs_abortedrelhs_list,
                    ar);
            ss_debug(ar->ar_chk = DBE_CHK_ABORTEDRELH);
            CHK_ABORTEDRELH(ar);

            ar->ar_rbtnode = su_rbt_insert2(
                                gtrs->gtrs_abortedrelhs_rbt,
                                ar);
            ss_dassert(ar->ar_rbtnode != NULL);
        }

        SsFlatMutexUnlock(gtrs->gtrs_sem);
}

