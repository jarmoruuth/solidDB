/*************************************************************************\
**  source       * dbe7trxb.c
**  directory    * dbe
**  description  * Buffer for transactions the status of which
**               * is not fully marked to the database. The marking is
**               * done during the merge process.
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

#define DBE7TRXB_C

#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>

#include <su0rbtr.h>

#include "dbe9type.h"
#include "dbe8trxl.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe0trx.h"
#include "dbe0type.h"

extern dbe_trxid_t dbe_bsea_disabletrxid;

/*#***********************************************************************\
 *
 *              TABLE_SPLITALLOC
 *
 * The split alloc of hash table means that the table is allocated
 * from separate, smaller pieces instead of single large allocated
 * hash table.
 */
#ifdef TABLE_SPLITALLOC

#define ELEMS_PER_BLOCK     (4096 / sizeof(void*) - 32)

#define TABLE_GET(t, i)     (t)[(i) / ELEMS_PER_BLOCK][(i) % ELEMS_PER_BLOCK]
#define TABLE_SET(t, i, v)  (t)[(i) / ELEMS_PER_BLOCK][(i) % ELEMS_PER_BLOCK] = (v)

static void* trxbuf_tablealloc(uint* p_tablesize)
{
        int i;
        uint nblocks;
        void** table;

        ss_dprintf_3(("trxbuf_tablealloc:tablesize = %u\n", *p_tablesize));
        ss_assert(sizeof(void*) * (ulong)*p_tablesize <= SS_MAXALLOCSIZE);

        nblocks = *p_tablesize / ELEMS_PER_BLOCK + 1;
        *p_tablesize = nblocks * ELEMS_PER_BLOCK;

        ss_dprintf_3(("trxbuf_tablealloc:nblocks = %u, new tablesize = %u\n", nblocks, *p_tablesize));

        table = SsMemAlloc(nblocks * sizeof(void*));
        for (i = 0; i < nblocks; i++) {
            table[i] = SsMemCalloc(ELEMS_PER_BLOCK, sizeof(void*));
        }
        return(table);
}

static void trxbuf_tablefree(void* tab, uint tablesize)
{
        uint i;
        uint nblocks;
        void** table = tab;

        nblocks = tablesize / ELEMS_PER_BLOCK;

        ss_dprintf_3(("trxbuf_tablefree:nblocks = %u, tablesize = %u\n", nblocks, tablesize));

        for (i = 0; i < nblocks; i++) {
            SsMemFree(table[i]);
        }
        SsMemFree(table);
}

#else /* TABLE_SPLITALLOC */

#define TABLE_GET(t, i)     (t)[i]
#define TABLE_SET(t, i, v)  (t)[i] = (v)

static void* trxbuf_tablealloc(uint* p_tablesize)
{
        ss_assert(sizeof(void*) * (ulong)*p_tablesize <= SS_MAXALLOCSIZE);
        return(SsMemCalloc(sizeof(void*), *p_tablesize));
}

static void trxbuf_tablefree(void* tab, uint tablesize __attribute__ ((unused)))
{
        SsMemFree(tab);
}

#endif /* TABLE_SPLITALLOC */

static dbe_trxbufslot_t* trxbuf_getbufslot(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid,
        uint* p_index)
{
        uint tbs_index;
        uint index;
        dbe_trxbufslot_t* tbs;

        index = (uint)((ulong)DBE_TRXID_GETLONG(trxid) % (ulong)tb->tb_tablesize);
        if (p_index != NULL) {
            *p_index = index;
        }

        tbs_index = index % tb->tb_nbufslots;

        tbs = tb->tb_tbslottable[tbs_index];

        CHK_TRXBUFSLOT(tbs);
        return(tbs);
}

static void trxbufslot_enterall(
        dbe_trxbuf_t* tb)
{
        dbe_trxbufslot_t* tbs;
        uint i;
        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            SsFlatMutexLock(tbs->tbs_sem);
        }
}

static void trxbufslot_exitall(
        dbe_trxbuf_t* tb)
{
        dbe_trxbufslot_t* tbs;
        int i;
        for (i=tb->tb_nbufslots-1;i>=0;i-- ) {
            tbs = tb->tb_tbslottable[i];
            SsFlatMutexUnlock(tbs->tbs_sem);
        }
}

/*#***********************************************************************\
 *
 *              trxbuf_listdeletefun
 *
 *
 *
 * Parameters :
 *
 *      data - in, take
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trxbuf_listdeletefun(
        void* data)
{
        dbe_trxstmt_t* ts = data;

        CHK_TRXSTMT(ts);
        CHK_TRXINFO(ts->ts_trxinfo);
        SS_MEMOBJ_DEC(SS_MEMOBJ_TRXBUFSTMT);
        dbe_trxinfo_done(ts->ts_trxinfo, NULL, dbe_trxbuf_getsembytrxid(ts->ts_tb, ts->ts_trxinfo->ti_usertrxid));
        SsMemFree(ts);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_init
 *
 *
 *
 * Parameters :
 *
 *      tablesize - in
 *              Hash table size.
 *
 *      aborttrxid - in
 *              Greatest active trx id that cannot uncertain. All
 *              transaction ids that are smaller than or equal to this
 *          and are not found from the buffer are considered as aborted.
 *
 * Return value - give :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trxbuf_t* dbe_trxbuf_init(
        uint tablesize,
        dbe_trxid_t aborttrxid,
        bool usevisiblealltrxid)
{
        dbe_trxbuf_t* tb;
        dbe_trxbufslot_t* tbs;
        uint i;

        ss_dprintf_1(("dbe_trxbuf_init:aborttrxid=%ld, usevisiblealltrxid=%d\n", DBE_TRXID_GETLONG(aborttrxid), usevisiblealltrxid));
        SS_PUSHNAME("dbe_trxbuf_init");

        tb = SSMEM_NEW(dbe_trxbuf_t);

        ss_debug(tb->tb_chk = DBE_CHK_TRXBUF);
        tb->tb_table = trxbuf_tablealloc(&tablesize);
        tb->tb_tablesize = tablesize;
        tb->tb_usevisiblealltrxid = usevisiblealltrxid;
        tb->tb_visiblealltrxid = aborttrxid;
        tb->tb_aborttrxid = aborttrxid;
        tb->tb_disabletrxid = DBE_TRXID_NULL;
        tb->tb_nbufslots = 16; /* This should be configurable */
        tb->tb_tbslottable = SsMemAlloc(tb->tb_nbufslots * sizeof(dbe_trxbufslot_t*));
        tb->tb_trxinfo_sem = SsMemAlloc(tb->tb_nbufslots * sizeof(tb->tb_trxinfo_sem[0]));
        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = SsMemAlloc(sizeof(dbe_trxbufslot_t)); 

            ss_debug(tbs->tbs_chk = DBE_CHK_TRXBUFSLOT);
            tbs->tbs_list = NULL;
            tbs->tbs_listlen = 0;
            SsFlatMutexInit(&tbs->tbs_sem, SS_SEMNUM_DBE_TRXB);

            CHK_TRXBUFSLOT(tbs);
            tb->tb_tbslottable[i] = tbs;

            tb->tb_trxinfo_sem[i] = SsQsemCreateLocal(SS_SEMNUM_DBE_TRXI);
        }

        dbe_bsea_disabletrxid = DBE_TRXID_NULL;

        SS_POPNAME;
        return(tb);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_done
 *
 *
 *
 * Parameters :
 *
 *      tb - in, take
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_trxbuf_done(dbe_trxbuf_t* tb)
{
        dbe_trxbufslot_t* tbs;
        uint i;

        CHK_TRXBUF(tb);

        trxbuf_tablefree(tb->tb_table, tb->tb_tablesize);

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);
            while (tbs->tbs_list != NULL) {
                dbe_trxstmt_t* del_ts;
                del_ts = tbs->tbs_list;
                CHK_TRXSTMT(del_ts);
                tbs->tbs_list = tbs->tbs_list->ts_listnext;
                trxbuf_listdeletefun(del_ts);
            }
            SsFlatMutexDone(tbs->tbs_sem);
            SsMemFree(tbs);
        }
        for (i=0;i<tb->tb_nbufslots;i++ ) {
            SsQsemFree(tb->tb_trxinfo_sem[i]);
        }
        SsMemFree(tb->tb_trxinfo_sem);
        SsMemFree(tb->tb_tbslottable);

        SsMemFree(tb);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_getaborttrxid
 *
 *
 *
 * Parameters :
 *
 *      tb -
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
dbe_trxid_t dbe_trxbuf_getaborttrxid(dbe_trxbuf_t* tb)
{
        dbe_trxid_t aborttrxid;

        CHK_TRXBUF(tb);
        ss_dprintf_1(("dbe_trxbuf_getaborttrxid\n"));

        /* MUTEX BEGIN */
        SsQsemEnter(tb->tb_trxinfo_sem[0]);

        if (tb->tb_usevisiblealltrxid) {
            aborttrxid = tb->tb_visiblealltrxid;    /* Just to return something. */
        } else {
            aborttrxid = tb->tb_aborttrxid;
        }

        /* MUTEX END */
        SsQsemExit(tb->tb_trxinfo_sem[0]);

        return(aborttrxid);
}

void dbe_trxbuf_setusevisiblealltrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t aborttrxid,
        bool usevisiblealltrxid)
{
        CHK_TRXBUF(tb);
        ss_dprintf_1(("dbe_trxbuf_setusevisiblealltrxid:usevisiblealltrxid=%d\n", usevisiblealltrxid));

        tb->tb_usevisiblealltrxid = usevisiblealltrxid;
        tb->tb_visiblealltrxid = aborttrxid;
        tb->tb_aborttrxid = aborttrxid;
}

#ifdef SS_HSBG2
/*##**********************************************************************\
 *
 *              dbe_trxbuf_abortall
 *
 * Abort all transactions in the trx buffer.
 *
 * Parameters :
 *
 *      tb - use
 *
 * Return value :
 *      #-of aborted transactions
 * Limitations  :
 *
 * Globals used :
 */
int dbe_trxbuf_abortall(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum,
        dbe_log_t* log,
        bool* p_reload_rbuf)
{
        int naborts;
        dbe_trxstmt_t* ts;
        dbe_trxstmt_t* ts_chk;
        dbe_trxstmt_t* next_ts;
        bool committed;
        bool aborted;
        dbe_logi_commitinfo_t info;
        dbe_ret_t logrc;
        dbe_trxbufslot_t* tbs;
        uint i;

        CHK_TRXBUF(tb);
        ss_dprintf_1(("dbe_trxbuf_abortall\n"));
        ss_dassert(p_reload_rbuf != NULL);

        *p_reload_rbuf = FALSE;
        naborts = 0;

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);

            SsFlatMutexLock(tbs->tbs_sem);
            /*
             * scan through the whole tb->tb_list and remove statements that are
             * no longer needed in the buffer.
             *
             */
            next_ts = tbs->tbs_list;
            while (next_ts != NULL) {

                ts = next_ts;
                next_ts = ts->ts_listnext;

                CHK_TRXSTMT(ts);
                CHK_TRXINFO(ts->ts_trxinfo);

                committed = dbe_trxinfo_iscommitted(ts->ts_trxinfo);
                aborted = dbe_trxinfo_isaborted(ts->ts_trxinfo);

                if (!aborted && !committed) {
                    if (dbe_trxinfo_isunknown(ts->ts_trxinfo) || dbe_trxinfo_isopenddop(ts->ts_trxinfo)) {
                        *p_reload_rbuf = TRUE;
                    }
                    (void)dbe_trxinfo_setaborted(ts->ts_trxinfo);
                    ts->ts_trxinfo->ti_committrxnum = aborttrxnum;

                    info = DBE_LOGI_COMMIT_LOCAL;

                    /* check if this is last one */
                    while (next_ts != NULL) {
                        ts_chk = next_ts;
                        if (dbe_trxinfo_isunknown(ts_chk->ts_trxinfo) || dbe_trxinfo_isopenddop(ts_chk->ts_trxinfo)) {
                            *p_reload_rbuf = TRUE;
                        }
                        committed = dbe_trxinfo_iscommitted(ts_chk->ts_trxinfo);
                        aborted = dbe_trxinfo_isaborted(ts_chk->ts_trxinfo);
                        if (!aborted && !committed) {
                            /* not last */
                            break;
                        } else {
                            next_ts = ts_chk->ts_listnext;
                        }
                    }

                    if (next_ts == NULL) {
                        /* reload rbuf on last abort */
                        ss_dprintf_1(("dbe_trxbuf_abortall:RELOAD:info %d, trxid %ld\n", info, DBE_TRXID_GETLONG(ts->ts_trxinfo->ti_usertrxid)));
                        SU_BFLAG_SET(info, DBE_LOGI_COMMIT_DDRELOAD);
                    }
                    ss_dprintf_1(("dbe_trxbuf_abortall:info %d, trxid %ld\n", info, DBE_TRXID_GETLONG(ts->ts_trxinfo->ti_usertrxid)));

                    if (log != NULL) {
                        logrc = dbe_log_puttrxmark(
                                    log,
                                    NULL,       /* cd */
                                    DBE_LOGREC_ABORTTRX_INFO,
                                    info,
                                    ts->ts_trxinfo->ti_usertrxid,
                                    DBE_HSB_NOT_KNOWN);
                        ss_dassert(logrc == DBE_RC_SUCC);
                    }

                    if (ts->ts_trxinfo->ti_hsbcd != NULL) {
                        dbe_trx_cardintrans_mutexif(ts->ts_trxinfo->ti_hsbcd, FALSE, FALSE, TRUE);
                    }
                    naborts++;

                }
            }
            SsFlatMutexUnlock(tbs->tbs_sem);
        }
        ss_dprintf_1(("dbe_trxbuf_abortall:END:naborts %d\n", naborts));

        return(naborts);
}
#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_trxbuf_addstmt
 *
 * Adds new statement with statement id stmttrxid to the trx buffer.
 *
 * Parameters :
 *
 *      tb - use
 *
 *
 *      stmttrxid - in
 *
 *
 *      ti - in
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_trxbuf_addstmt(
        dbe_trxbuf_t* tb,
        dbe_trxid_t stmttrxid,
        dbe_trxinfo_t* ti)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxstmt_t* ts;
        dbe_trxbufslot_t* tbs;

        CHK_TRXBUF(tb);
        CHK_TRXINFO(ti);
        ss_dprintf_3(("dbe_trxbuf_addstmt:usertrxid = %ld, stmttrxid = %ld\n",
            DBE_TRXID_GETLONG(ti->ti_usertrxid), DBE_TRXID_GETLONG(stmttrxid)));

        ss_dassert(!DBE_TRXID_EQUAL(stmttrxid, DBE_TRXID_NULL));

        SS_MEMOBJ_INC(SS_MEMOBJ_TRXBUFSTMT, dbe_trxstmt_t);

        dbe_trxinfo_link(ti, dbe_trxbuf_getsembytrxid(tb, ti->ti_usertrxid));

        ts = SSMEM_NEW(dbe_trxstmt_t);

        ss_debug(ts->ts_chk = DBE_CHK_TRXSTMT);
        ts->ts_trxinfo = ti;
        ts->ts_stmttrxid = stmttrxid;
        ts->ts_tb = tb;

        tbs = trxbuf_getbufslot(tb, stmttrxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        ts->ts_listnext = tbs->tbs_list;
        tbs->tbs_list = ts;
        tbs->tbs_listlen++;
        ss_dassert(tbs->tbs_listlen > 0);
        SS_PMON_ADD(SS_PMON_TRANSBUFADDED);

        tsslot = TABLE_GET(tb->tb_table, index);
        ts->ts_bufnext = tsslot;
        TABLE_SET(tb->tb_table, index, ts);

#if defined(SS_DEBUG) || defined(SS_DEBUGGER)
        while (tsslot != NULL) {
            ss_rc_assert(!DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, ts->ts_stmttrxid), DBE_TRXID_GETLONG(ts->ts_stmttrxid));
            tsslot = tsslot->ts_bufnext;
        }
#endif
        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_add
 *
 * Adds new transaction to the trx buffer.
 *
 * Parameters :
 *
 *      tb -
 *
 *
 *      ti -
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
void dbe_trxbuf_add(
        dbe_trxbuf_t* tb,
        dbe_trxinfo_t* ti)
{
        ss_dprintf_3(("dbe_trxbuf_add\n"));
        CHK_TRXINFO(ti);

        dbe_trxbuf_addstmt(tb, ti->ti_usertrxid, ti);
}

/*#***********************************************************************\
 *
 *              trxbuf_tableremove
 *
 *
 *
 * Parameters :
 *
 *      tb - in out, use
 *
 *
 *      stmttrxid - in
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trxbuf_tableremove(
        dbe_trxbuf_t* tb,
        dbe_trxid_t stmttrxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxstmt_t* prevtsslot;
        dbe_trxbufslot_t* tbs;

        ss_dprintf_3(("trxbuf_tableremove:stmttrxid = %ld\n", DBE_TRXID_GETLONG(stmttrxid)));
        CHK_TRXBUF(tb);

        tbs = trxbuf_getbufslot(tb, stmttrxid, &index);
        ss_dassert(SsFlatMutexIsLockedByThread(tbs->tbs_sem));

        tsslot = TABLE_GET(tb->tb_table, index);

        ss_dassert(tsslot != NULL);

        if (DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, stmttrxid)) {
            TABLE_SET(tb->tb_table, index, tsslot->ts_bufnext);
        } else {
            do {
                prevtsslot = tsslot;
                tsslot = tsslot->ts_bufnext;
                ss_dassert(tsslot != NULL);
            } while (!DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, stmttrxid));
            prevtsslot->ts_bufnext = tsslot->ts_bufnext;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_abortstmt
 *
 * Marks statement identified by stmttrxid as aborted.
 *
 * Parameters :
 *
 *      tb - use
 *
 *
 *      aborttrxnum - in
 *          Transaction number for aborted statement. We need to use the 
 *          latest committrxnum in case of mergecleanup because otherwise statement
 *          may be removed too early.
 *
 *      stmttrxid - in
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
void dbe_trxbuf_abortstmt(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum,
        dbe_trxid_t stmttrxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxinfo_t* ti;
        dbe_trxbufslot_t* tbs;
        ss_debug(int nloop = 0;)

        CHK_TRXBUF(tb);
        ss_dprintf_3(("dbe_trxbuf_abortstmt:stmttrxid = %ld\n", DBE_TRXID_GETLONG(stmttrxid)));

        tbs = trxbuf_getbufslot(tb, stmttrxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);
        ss_dassert(tsslot != NULL);

        while (!DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, stmttrxid)) {
            CHK_TRXSTMT(tsslot);
            ss_dassert(nloop++ < 1000);
            tsslot = tsslot->ts_bufnext;
            if (tsslot == NULL) {
                /* Statement not found. */
                ss_derror;
                /* MUTEX END */
                SsFlatMutexUnlock(tbs->tbs_sem);
                return;
            }
        }

        ti = dbe_trxinfo_init(NULL);

        ti->ti_usertrxid = stmttrxid;
        ti->ti_committrxnum = aborttrxnum;
        (void)dbe_trxinfo_setaborted(ti);

#ifdef DBE_REPLICATION
        if (dbe_trxinfo_istobeaborted(tsslot->ts_trxinfo)) {
            (void)dbe_trxinfo_setbegin(tsslot->ts_trxinfo);
        }
#endif
        dbe_trxinfo_done(tsslot->ts_trxinfo, NULL, dbe_trxbuf_getsembytrxid(tb, tsslot->ts_trxinfo->ti_usertrxid));

        tsslot->ts_trxinfo = ti;

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);
}

void* dbe_trxbuf_disablestmt(
        dbe_trxbuf_t* tb,
        dbe_trxid_t stmttrxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxbufslot_t* tbs;
        ss_debug(int nloop = 0;)

        CHK_TRXBUF(tb);
        ss_dprintf_3(("dbe_trxbuf_disablestmt:stmttrxid = %ld\n", DBE_TRXID_GETLONG(stmttrxid)));

        tbs = trxbuf_getbufslot(tb, stmttrxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);
        ss_dassert(tsslot != NULL);

        while (!DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, stmttrxid)) {
            CHK_TRXSTMT(tsslot);
            ss_dassert(nloop++ < 1000);
            tsslot = tsslot->ts_bufnext;
            if (tsslot == NULL) {
                /* Statement not found. */
                ss_error;
            }
        }

        tsslot->ts_stmttrxid = DBE_TRXID_NULL;
        tb->tb_disabletrxid = stmttrxid;
        dbe_bsea_disabletrxid = stmttrxid;

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);

        return(tsslot);
}

void dbe_trxbuf_enablestmt(
        dbe_trxbuf_t* tb,
        void* ctx,
        dbe_trxid_t stmttrxid)
{
        dbe_trxstmt_t* tsslot = ctx;
        dbe_trxbufslot_t* tbs;
        uint index;

        CHK_TRXBUF(tb);
        ss_dprintf_3(("dbe_trxbuf_enablestmt:stmttrxid = %ld\n", DBE_TRXID_GETLONG(stmttrxid)));

        tbs = trxbuf_getbufslot(tb, stmttrxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        /* stmttrxid must be same when dbe_trxbuf_disablestmt was called.
         * We do not move ts from hash
         */
        tsslot->ts_stmttrxid = stmttrxid;
        tb->tb_disabletrxid = DBE_TRXID_NULL;
        dbe_bsea_disabletrxid = DBE_TRXID_NULL;

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);
}

SS_INLINE bool dbe_trxbuf_isopentrx(dbe_opentrxinfo_t* oti, dbe_trxid_t trxid)
{
        int i;

        if (oti == NULL || oti->oti_len == 0) {
            return(FALSE);
        }
        if (dbe_trxid_cmp(trxid, oti->oti_tablemin) < 0
            || dbe_trxid_cmp(trxid, oti->oti_tablemax) > 0)
        {
            return(FALSE);
        }
        for (i = 0; i < oti->oti_len; i++) {
            if (dbe_trxid_equal(trxid, oti->oti_table[i])) {
                return(TRUE);
            }
        }
        return(FALSE);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_clean
 *
 *
 *
 * Parameters :
 *
 *      tb - in out, use
 *              Transcation info buffer.
 *
 *      cleantrxnum - in
 *              Merge transaction level. Committed transaction below or equal
 *              to this level are patched to the index and can be removed from
 *              the transaction buffer.
 *
 *      mergestart_aborttrxid - in
 *              Abort trxid when merge started. Aborted transaction below or
 *          equal to this level are removed form the index and can be removed
 *          also from the transaction buffer.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
long dbe_trxbuf_clean(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t cleantrxnum,
        dbe_trxid_t mergestart_aborttrxid,
        dbe_opentrxinfo_t* oti)
{
        dbe_trxstmt_t* ts;
        dbe_trxstmt_t* prev_ts;
        dbe_trxbufslot_t* tbs;
        uint i;
        dbe_trxid_t cleantrxid;
        long removecount = 0;

        CHK_TRXBUF(tb);
        ss_dprintf_1(("dbe_trxbuf_clean:cleantrxnum = %ld, mergestart_aborttrxid = %ld, usevisiblealltrxid=%d\n", DBE_TRXNUM_GETLONG(cleantrxnum), DBE_TRXID_GETLONG(mergestart_aborttrxid), tb->tb_usevisiblealltrxid));
        SS_PMON_ADD(SS_PMON_TRANSBUFCLEAN);
        SS_PMON_SET(SS_PMON_TRANSBUFCLEANLEVEL, DBE_TRXNUM_GETLONG(cleantrxnum));
        SS_PMON_SET(SS_PMON_TRANSBUFABORTLEVEL, DBE_TRXID_GETLONG(mergestart_aborttrxid));

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];

            CHK_TRXBUFSLOT(tbs);

            /* MUTEX BEGIN */
            SsFlatMutexLock(tbs->tbs_sem);

            /* Scan through the whole tb->tb_list and remove statements that are
             * no longer needed in the buffer.
             */
            ts = tbs->tbs_list;
            prev_ts = NULL;
            while (ts != NULL) {
                bool remove_trx;
                dbe_trxinfo_t* trxinfo;

                CHK_TRXSTMT(ts);
                CHK_TRXINFO(ts->ts_trxinfo);

                trxinfo = ts->ts_trxinfo;
                if (dbe_trxinfo_isended(trxinfo)) {
                    /* Remove only ended transactions. 
                     */
                    ss_dassert(dbe_trxinfo_iscommitted(trxinfo) || dbe_trxinfo_isaborted(trxinfo));
                    if (dbe_trxinfo_ismtabletrx(trxinfo)) {
                        /* Remove always all M-table transactions.
                         */
                        remove_trx = TRUE;
                    } else if (dbe_trxinfo_iscommitted(trxinfo) 
                                && dbe_trxnum_cmp(trxinfo->ti_committrxnum, cleantrxnum) <= 0)
                    {
                        /* Remove all committed transactions that are below current clean level
                         * and that were not open when merge started. 
                         */
                        if (dbe_trxbuf_isopentrx(oti, ts->ts_trxinfo->ti_usertrxid)) {
                            remove_trx = FALSE;
                        } else {
                            remove_trx = TRUE;
                        }
                    } else if (tb->tb_usevisiblealltrxid) {
                        if (DBE_TRXID_ISNULL(mergestart_aborttrxid)) {
                            /* In this call we should not remove any aborted transactions.
                             */
                            remove_trx = FALSE;
                        } else if (dbe_trxinfo_isaborted(trxinfo) 
                                   && dbe_trxid_cmp(ts->ts_stmttrxid, mergestart_aborttrxid) <= 0) 
                        {
                            /* Remove all aborted transaction that are already removed from
                             * Bonsai tree. Do not remove those transactions that were
                             * open when merge started or were aborted after merge started.
                             */
                            dbe_trxnum_t mergestart_aborttrxnum;
                            mergestart_aborttrxnum = dbe_trxnum_initfromtrxid(mergestart_aborttrxid);
                            if (dbe_trxnum_cmp(trxinfo->ti_committrxnum, mergestart_aborttrxnum) > 0
                                || dbe_trxbuf_isopentrx(oti, ts->ts_trxinfo->ti_usertrxid)) 
                            {
                                remove_trx = FALSE;
                            } else {
                                remove_trx = TRUE;
                            }
                        } else {
                            remove_trx = FALSE;
                        }
                    } else {
                        ss_dassert(!tb->tb_usevisiblealltrxid);
                        ss_dassert(oti == NULL);
                        if (dbe_trxinfo_isaborted(trxinfo) 
                            && dbe_trxid_cmp(ts->ts_stmttrxid, mergestart_aborttrxid) <= 0) 
                        {
                            /* Remove all aborted transactions that are below current
                             * abort level. Those are assumed to be aborted if
                             * tb->tb_usevisiblealltrxid is not set.
                             */
                            remove_trx = TRUE;
                        } else {
                            remove_trx = FALSE;
                        }
                    }
                } else {
                    /* Transaction state not yet known, we need to keep it.
                     */
                    remove_trx = FALSE;
                }

                if (remove_trx) {
                    /* This transaction is patched to the index during merge,
                     * remove the information from the trxbuf.
                     */
                    dbe_trxstmt_t* del_ts;
                    del_ts = ts;
                    CHK_TRXSTMT(del_ts);
                    ts = ts->ts_listnext;
                    if (prev_ts == NULL) {
                        /* Removing the first item of the list. */
                        ss_dassert(del_ts == tbs->tbs_list);
                        tbs->tbs_list = ts;
                    } else {
                        /* Removing from the middle of the list. */
                        ss_dassert(del_ts != tbs->tbs_list);
                        prev_ts->ts_listnext = ts;
                    }
                    trxbuf_tableremove(tb, del_ts->ts_stmttrxid);
                    trxbuf_listdeletefun(del_ts);
                    tbs->tbs_listlen--;
                    ss_dassert(tbs->tbs_listlen >= 0);
                    removecount++;
                    SS_PMON_ADD(SS_PMON_TRANSBUFREMOVED);
                } else {
                    prev_ts = ts;
                    ts = ts->ts_listnext;
                }
            }
            if (tb->tb_usevisiblealltrxid) {
                cleantrxid = dbe_trxid_initfromtrxnum(cleantrxnum);
                if (DBE_TRXID_CMP_EX(cleantrxid, tb->tb_visiblealltrxid) > 0) {
                    tb->tb_visiblealltrxid = cleantrxid;
                }
            }
            /* MUTEX END */
            SsFlatMutexUnlock(tbs->tbs_sem);
        }
        ss_dprintf_2(("dbe_trxbuf_clean:removecount = %ld\n", removecount));

        return(removecount);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_gettrxstate
 *
 *
 *
 * Parameters :
 *
 *      tb - in, use
 *
 *
 *      keytrxid - in
 *
 *
 *      p_trxnum - out
 *
 *
 *      p_usertrxid - out
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_trxstate_t dbe_trxbuf_gettrxstate(
        dbe_trxbuf_t* tb,
        dbe_trxid_t keytrxid,
        dbe_trxnum_t* p_trxnum,
        dbe_trxid_t* p_usertrxid)
{
        uint           index;
        dbe_trxstate_t trxresult;
        dbe_trxstmt_t* tsslot;
        dbe_trxnum_t   ret_trxnum = {0};
        dbe_trxid_t    ret_usertrxid = {0};
        dbe_trxbufslot_t* tbs;

        ss_dprintf_3(("dbe_trxbuf_gettrxstate:keytrxid=%ld, tb->tb_usevisiblealltrxid=%d, tb->tb_aborttrxid=%ld\n", 
            DBE_TRXID_GETLONG(keytrxid), tb->tb_usevisiblealltrxid, DBE_TRXID_GETLONG(tb->tb_aborttrxid), DBE_TRXID_GETLONG(tb->tb_visiblealltrxid)));
        CHK_TRXBUF(tb);

        tbs = trxbuf_getbufslot(tb, keytrxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);

        while (tsslot != NULL) {
            CHK_TRXSTMT(tsslot);
            CHK_TRXINFO(tsslot->ts_trxinfo);
            if (DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, keytrxid)) {
                trxresult = dbe_trxinfo_getstate(tsslot->ts_trxinfo);
                switch (trxresult) {

                    case DBE_TRXST_BEGIN:
                        ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_BEGIN, found\n"));
                        /* FALLTHROUGH */
#ifdef DBE_REPLICATION
                    case DBE_TRXST_TOBEABORTED:
                        ss_debug(
                            if (dbe_trxinfo_getstate(tsslot->ts_trxinfo)
                                == DBE_TRXST_TOBEABORTED)
                            {
                                ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_TOBEABORTED, found\n"));
                            }
                        )
#endif
                        ret_trxnum = DBE_TRXNUM_NULL;
                        ret_usertrxid = tsslot->ts_trxinfo->ti_usertrxid;
                        break;

                    case DBE_TRXST_VALIDATE:
                        ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_VALIDATE, found\n"));
                        ret_trxnum = tsslot->ts_trxinfo->ti_committrxnum;
                        ss_dassert(!DBE_TRXNUM_EQUAL(ret_trxnum, DBE_TRXNUM_NULL));
                        ret_usertrxid = tsslot->ts_trxinfo->ti_usertrxid;
                        ss_dassert(!DBE_TRXID_EQUAL(ret_usertrxid, DBE_TRXID_NULL));
                        break;

                    case DBE_TRXST_COMMIT:
                    case DBE_TRXST_ABORT:
                        ss_dassert(!dbe_trxinfo_iscommitted(tsslot->ts_trxinfo) ||
                                   !DBE_TRXNUM_EQUAL(tsslot->ts_trxinfo->ti_committrxnum, DBE_TRXNUM_NULL));
                        ret_trxnum = tsslot->ts_trxinfo->ti_committrxnum;
                        ret_usertrxid = tsslot->ts_trxinfo->ti_usertrxid;
                        ss_dprintf_3(("dbe_trxbuf_gettrxstate:%s\n",
                            dbe_trxinfo_iscommitted(tsslot->ts_trxinfo)
                                ? "DBE_TRXST_COMMIT"
                                : "DBE_TRXST_ABORT"));
                        ss_dassert(trxresult == DBE_TRXST_ABORT ||
                                   !DBE_TRXNUM_EQUAL(ret_trxnum, DBE_TRXNUM_NULL));
                        break;

                    default:
                        ss_rc_error(trxresult);
                }
                break;
            }
            tsslot = tsslot->ts_bufnext;
        }

        if (tsslot == NULL) {
            /* Slot not found for keytrxid.
             */
            if (dbe_trxid_equal(tb->tb_disabletrxid, keytrxid)) {
                ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_BEGIN, trxid is disabled\n"));
                ret_trxnum = DBE_TRXNUM_NULL;
                ret_usertrxid = keytrxid;
                trxresult = DBE_TRXST_BEGIN;
            } else if (!tb->tb_usevisiblealltrxid
                       && DBE_TRXID_CMP_EX(keytrxid, tb->tb_aborttrxid) <= 0) 
            {
                /* Statement is aborted.
                 */
                ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_ABORT, not found\n"));
                ret_trxnum = DBE_TRXNUM_NULL;
                ret_usertrxid = DBE_TRXID_NULL;
                trxresult = DBE_TRXST_ABORT;
            } else if (tb->tb_usevisiblealltrxid
                       && DBE_TRXID_CMP_EX(keytrxid, tb->tb_visiblealltrxid) <= 0) 
            {
                /* Statement is committed and visible to all.
                 */
                ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_COMMIT, visibleall\n"));
                ret_trxnum = dbe_trxnum_initfromtrxid(tb->tb_visiblealltrxid);
                ret_usertrxid = tb->tb_visiblealltrxid;
                trxresult = DBE_TRXST_COMMIT;
            } else {
                /* The statement info is not yet added to the trxbuf.
                 */
                ss_dprintf_3(("dbe_trxbuf_gettrxstate:DBE_TRXST_BEGIN, not found\n"));
                ret_trxnum = DBE_TRXNUM_NULL;
                ret_usertrxid = keytrxid;
                trxresult = DBE_TRXST_BEGIN;
            }
        }

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);

        if (p_trxnum != NULL) {
            *p_trxnum = ret_trxnum;
        }
        if (p_usertrxid != NULL) {
            *p_usertrxid = ret_usertrxid;
        }
        return(trxresult);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_save
 *
 * Saves the transaction info list to disk
 *
 * Parameters :
 *
 *      tb - in out, use
 *              transaction buffer
 *
 *      cache - in out, use
 *              cache
 *
 *      freelist - in out, use
 *              free list
 *
 *      cpnum - in
 *          checkpoint # used for storing the disk blocks
 *
 *
 *      p_trxlistdaddr - out
 *              pointer to variable where the disk address of the
 *          saved trx list start will be stored
 *
 *      p_stmttrxlistdaddr - out
 *              pointer to variable where the disk address of the
 *          saved statement trx list start will be stored
 *
 * Return value :
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trxbuf_save(
        dbe_trxbuf_t* tb,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_trxlistdaddr,
        su_daddr_t* p_stmttrxlistdaddr)
{
        dbe_trxstmt_t* ts;
        dbe_trxlist_t* trxlist;
        dbe_trxlist_t* stmttrxlist;
        su_ret_t rc;
        dbe_trxbufslot_t* tbs;
        uint i;

        ss_dprintf_1(("dbe_trxbuf_save\n"));
        CHK_TRXBUF(tb);

        ss_output_3(dbe_trxbuf_print(tb));

        /* save list of committed yet unpatched transactions */
        trxlist = dbe_trxl_init(
                    cache,
                    freelist,
                    cpnum,
                    (dbe_blocktype_t)DBE_BLOCK_TRXLIST);
        stmttrxlist =
            dbe_trxl_init(
                    cache,
                    freelist,
                    cpnum,
                    (dbe_blocktype_t)DBE_BLOCK_STMTTRXLIST);

        /* MUTEX BEGIN */
        trxbufslot_enterall(tb);

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);

            /* Scan the tb->tb_list and store the records
             */
            for (ts = tbs->tbs_list; ts != NULL; ts = ts->ts_listnext) {
                CHK_TRXSTMT(ts);
                if (dbe_trxinfo_iscommitted(ts->ts_trxinfo)) {
                    ss_dassert(!DBE_TRXNUM_EQUAL(ts->ts_trxinfo->ti_committrxnum, DBE_TRXNUM_MAX));
                    rc = dbe_trxl_add(
                            trxlist,
                            ts->ts_trxinfo->ti_usertrxid,
                            ts->ts_trxinfo->ti_committrxnum);
                    su_rc_assert(rc == SU_SUCCESS, rc);
                    /* Set the committed status to saved status, so we don't save
                     * the same transaction more than once. The status is later
                     * restored back to committed.
                     */
                    (void)dbe_trxinfo_setsaved(ts->ts_trxinfo);
                }
                if (!dbe_trxinfo_isaborted(ts->ts_trxinfo) || tb->tb_usevisiblealltrxid) {
                    FAKE_CODE_BLOCK(
                        FAKE_DBE_TRXB_CFGEXCEEDED,
                        {
                            SsDbgPrintf("FAKE_DBE_TRXB_CFGEXCEEDED\n");
                            FAKE_SET(FAKE_DBE_TRXL_CFGEXCEEDED, 1);
                        }
                    )
                    rc = dbe_trxl_addstmttrx(
                            stmttrxlist,
                            ts->ts_trxinfo->ti_usertrxid,
                            ts->ts_stmttrxid);
                    dbe_fileio_error((char *)__FILE__, __LINE__, rc);
                    su_rc_assert(rc == SU_SUCCESS, rc);
                }
            }
        }

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);
            /* Scan the tb->tb_list and restore saved status back to committed
             */
            for (ts = tbs->tbs_list; ts != NULL; ts = ts->ts_listnext) {
                if (dbe_trxinfo_issaved(ts->ts_trxinfo)) {
                    ss_dassert(!DBE_TRXNUM_EQUAL(ts->ts_trxinfo->ti_committrxnum, DBE_TRXNUM_MAX));
                    (void)dbe_trxinfo_setcommitted(ts->ts_trxinfo);
                }
            }
        }

        /* MUTEX END */
        trxbufslot_exitall(tb);

        rc = dbe_trxl_save(
                trxlist,
                p_trxlistdaddr);
        su_rc_assert(rc == SU_SUCCESS, rc);

        dbe_trxl_done(trxlist);

        rc = dbe_trxl_save(
                stmttrxlist,
                p_stmttrxlistdaddr);
        su_rc_assert(rc == SU_SUCCESS, rc);

        dbe_trxl_done(stmttrxlist);

        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_restore
 *
 * Restores the transaction info lists from disk
 *
 * Parameters :
 *
 *      tb - in out, use
 *              trx buffer
 *
 *      cache - in out, use
 *              cache
 *
 *      trxlistdaddr - in
 *              start disk address of saved trx info list
 *
 *      stmttrxlistdaddr - in
 *              start disk address of saved stmttrx info list
 *
 * Return value :
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trxbuf_restore(
        dbe_trxbuf_t* tb,
        dbe_cache_t* cache,
        su_daddr_t trxlistdaddr,
        su_daddr_t stmttrxlistdaddr)
{
        dbe_trxinfo_t* trxinfo;
        dbe_trxlist_iter_t* trxli;
        dbe_trxlist_iter_t* stmttrxli;
        dbe_trxid_t     usertrxid;
        dbe_trxid_t     stmttrxid;
        dbe_trxnum_t    committrxnum;

        ss_dprintf_1(("dbe_trxbuf_restore\n"));
        CHK_TRXBUF(tb);

        trxli = dbe_trxli_init(
                        cache,
                        trxlistdaddr,
                        (dbe_blocktype_t)DBE_BLOCK_TRXLIST);

        /* scan the trxlist iterator and restore all trxinfo nodes */
        while (dbe_trxli_getnext(trxli, &usertrxid, &committrxnum)) {
            trxinfo = dbe_trxinfo_init(NULL);
            (void)dbe_trxinfo_setcommitted(trxinfo);
            trxinfo->ti_usertrxid = usertrxid;
            trxinfo->ti_maxtrxnum = DBE_TRXNUM_NULL;
            trxinfo->ti_committrxnum = committrxnum;
            ss_dassert(!DBE_TRXNUM_EQUAL(committrxnum, DBE_TRXNUM_MAX));
            dbe_trxbuf_add(tb, trxinfo);
            dbe_trxinfo_done(trxinfo, NULL, dbe_trxbuf_getsembytrxid(tb, trxinfo->ti_usertrxid));
        }
        dbe_trxli_done(trxli);

        stmttrxli = dbe_trxli_init(
                        cache,
                        stmttrxlistdaddr,
                        (dbe_blocktype_t)DBE_BLOCK_STMTTRXLIST);
        while (dbe_trxli_getnextstmttrx(stmttrxli, &usertrxid, &stmttrxid)) {
            trxinfo = dbe_trxbuf_gettrxinfo(tb, usertrxid);
            if (trxinfo == NULL) {
                trxinfo = dbe_trxinfo_init(NULL);
                trxinfo->ti_usertrxid = usertrxid;
                trxinfo->ti_maxtrxnum = DBE_TRXNUM_NULL;
#ifdef SS_HSBG2
                dbe_trxinfo_setunknown(trxinfo);
#endif /* SS_HSBG2 */
                dbe_trxbuf_add(tb, trxinfo);
                if (!DBE_TRXID_EQUAL(usertrxid, stmttrxid)) {
                    dbe_trxbuf_addstmt(tb, stmttrxid, trxinfo);
                }
                dbe_trxinfo_done(trxinfo, NULL, dbe_trxbuf_getsembytrxid(tb, trxinfo->ti_usertrxid));
            } else {

#ifdef SS_HSBG2
                dbe_trxinfo_setunknown(trxinfo);
#endif /* SS_HSBG2 */
                if (!DBE_TRXID_EQUAL(usertrxid, stmttrxid)) {
                    dbe_trxbuf_addstmt(tb, stmttrxid, trxinfo);
                }
            }
        }
        dbe_trxli_done(stmttrxli);
        ss_output_3(dbe_trxbuf_print(tb));
        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_gettrxinfo
 *
 * Gets reference to trxinfo buffer
 *
 * Parameters :
 *
 *      tb - in, use
 *              transaction buffer
 *
 *      trxid - in
 *              trx ID
 *
 * Return value - ref :
 *      pointer to trx info structure or NULL if the trx ID was
 *      not found.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_trxinfo_t* dbe_trxbuf_gettrxinfo(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxinfo_t* trxinfo;
        dbe_trxbufslot_t* tbs;

        ss_dprintf_3(("dbe_trxbuf_gettrxinfo: trxid %ld\n", DBE_TRXID_GETLONG(trxid)));
        CHK_TRXBUF(tb);

        tbs = trxbuf_getbufslot(tb, trxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);

        while (tsslot != NULL) {
            CHK_TRXSTMT(tsslot);
            CHK_TRXINFO(tsslot->ts_trxinfo);
            if (DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, trxid)) {
                break;
            }
            tsslot = tsslot->ts_bufnext;
        }

        if (tsslot == NULL) {
            trxinfo = NULL;
        } else {
            trxinfo = tsslot->ts_trxinfo;
        }

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);

        return (trxinfo);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_gettrxinfo_linked
 *
 * Same as dbe_trxbuf_gettrxinfo but dbe_trxinfo_link is called for
 * returned dbe_trxinfo_t object.
 *
 * Parameters :
 *
 *              tb -
 *
 *
 *              trxid -
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
dbe_trxinfo_t* dbe_trxbuf_gettrxinfo_linked(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxinfo_t* trxinfo;
        dbe_trxbufslot_t* tbs;

        ss_dprintf_3(("dbe_trxbuf_gettrxinfo_linked: trxid %ld\n", DBE_TRXID_GETLONG(trxid)));
        CHK_TRXBUF(tb);

        tbs = trxbuf_getbufslot(tb, trxid, &index);

        /* MUTEX BEGIN */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);

        while (tsslot != NULL) {
            CHK_TRXSTMT(tsslot);
            CHK_TRXINFO(tsslot->ts_trxinfo);
            if (DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, trxid)) {
                break;
            }
            tsslot = tsslot->ts_bufnext;
        }

        if (tsslot == NULL) {
            trxinfo = NULL;
        } else {
            trxinfo = tsslot->ts_trxinfo;
            dbe_trxinfo_link(trxinfo, dbe_trxbuf_getsembytrxid(tb, trxinfo->ti_usertrxid));
        }

        /* MUTEX END */
        SsFlatMutexUnlock(tbs->tbs_sem);

        return (trxinfo);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_getcount
 *
 * Returns the number of transaction info elements in the buffer.
 *
 * Parameters :
 *
 *      tb -
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
ulong dbe_trxbuf_getcount(dbe_trxbuf_t* tb)
{
        ulong count = 0L;
        dbe_trxbufslot_t* tbs;
        int i;

        for (i=0;i< (int)tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);
            SsFlatMutexLock(tbs->tbs_sem);
        }

        for (i=tb->tb_nbufslots-1;i>=0;i-- ) {
            tbs = tb->tb_tbslottable[i];
            count = count + (ulong)tbs->tbs_listlen;
            SsFlatMutexUnlock(tbs->tbs_sem);
        }

        return(count);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_printinfo
 *
 *
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *      tb -
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
void dbe_trxbuf_printinfo(
        void* fp,
        dbe_trxbuf_t* tb)
{
        uint i;
        uint nused = 0;
        dbe_trxbufslot_t* tbs;
        long listlen = 0L;

        /* MUTEX BEGIN */
        trxbufslot_enterall(tb);

        for (i = 0; i < tb->tb_tablesize; i++) {
            if (TABLE_GET(tb->tb_table, i) != NULL) {
                nused++;
            }
        }

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);
            listlen = listlen + tbs->tbs_listlen;
        }
        SsFprintf(fp, "    Table size %d nused %d List length %d\n",
            tb->tb_tablesize,
            nused,
            listlen);

        /* MUTEX END */
        trxbufslot_exitall(tb);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_cleanuncommitted
 *
 * Cleans uncommitted transactions from trx buffer in the end of
 * roll-forward recovery.
 *
 * Parameters :
 *
 *      tb - in out, use
 *          pointer to transaction buffer object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_trxbuf_cleanuncommitted(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum)
{
        dbe_trxstmt_t* ts;
        dbe_trxstmt_t* prev_ts;
        dbe_trxbufslot_t* tbs;
        uint i;

        ss_dprintf_1(("dbe_trxbuf_cleanuncommitted\n"));

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);

            /* MUTEX BEGIN */
            SsFlatMutexLock(tbs->tbs_sem);

            if (tb->tb_usevisiblealltrxid) {
                ts = tbs->tbs_list;
                while (ts != NULL) {
                    CHK_TRXSTMT(ts);
                    if (!dbe_trxinfo_iscommitted(ts->ts_trxinfo)) {
                        dbe_trxinfo_setaborted(ts->ts_trxinfo);
                        ts->ts_trxinfo->ti_committrxnum = aborttrxnum;
                    }
                    ts = ts->ts_listnext;
                }
            } else {
                ts = tbs->tbs_list;
                prev_ts = NULL;
                while (ts != NULL) {
                    CHK_TRXSTMT(ts);
                    if (!dbe_trxinfo_iscommitted(ts->ts_trxinfo)) {
                        dbe_trxstmt_t* del_ts;
                        del_ts = ts;
                        CHK_TRXSTMT(del_ts);
                        ts = ts->ts_listnext;
                        if (prev_ts == NULL) {
                            /* Removing the first item of the list. */
                            ss_dassert(del_ts == tbs->tbs_list);
                            tbs->tbs_list = ts;
                        } else {
                            /* Removing from the middle of the list. */
                            ss_dassert(del_ts != tbs->tbs_list);
                            prev_ts->ts_listnext = ts;
                        }
                        trxbuf_tableremove(tb, del_ts->ts_stmttrxid);
                        trxbuf_listdeletefun(del_ts);
                        tbs->tbs_listlen--;
                        ss_dassert(tbs->tbs_listlen >= 0);
                        SS_PMON_ADD(SS_PMON_TRANSBUFREMOVED);
                    } else {
                        prev_ts = ts;
                        ts = ts->ts_listnext;
                    }
                }
            }

            /* MUTEX END */
            SsFlatMutexUnlock(tbs->tbs_sem);
        }
}

typedef struct {
        dbe_trxid_t tk_trxid;
        dbe_trxid_t tk_stmttrxid;
} trxrbt_key_t;

static int trxrbt_compare(void* key1, void* key2)
{
        trxrbt_key_t* tk1 = key1;
        trxrbt_key_t* tk2 = key2;

        return(DBE_TRXID_CMP_EX(tk1->tk_trxid, tk2->tk_trxid));
}

static void trxrbt_delete(void* key)
{
        SsMemFree(key);
}

static void trxrbt_insert(su_rbt_t* rbt, dbe_trxid_t trxid)
{
        bool succp;
        trxrbt_key_t* tk;

        tk = SSMEM_NEW(trxrbt_key_t);
        tk->tk_trxid = trxid;
        tk->tk_stmttrxid = trxid;
        succp = su_rbt_insert(rbt, tk);
        ss_dassert(succp);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_addopentrx
 *
 * Add uncommitted transactions from trx buffer to the remote trx buffer
 * This is done when a primary restart. Only last open statement for each
 * transaction is added to the remote trx buffer. A new statement and
 * transction is started in dbe_hsb_init.
 *
 * We do not really know if the last statement is committed or open.
 * It is added to the buffer and a new statement is created for it in
 * dbe_hsb_init. In case it is already committed we must be able to
 * handle the situation by committing the statement locally before
 * starting a new one.
 *
 * This kind of dummy mapping is needed for two different cases.
 *
 * CASE 1: After hsb copy or netcopy
 *
 *    PRI: commit statement 1
 *    PRI: netcopy
 *    SEC: restart statement 1
 *    PRI: start statement 2
 *    SEC: receive start statement 2
 *    -> error if we assume statement can not be active
 *
 * CASE 2: Secondary has committed but primary crash before ack
 *
 *    N1:pri: send commit t1 to n2
 *    N2:sec: Receive commit T1
 *    N2:sec: Do commit T1
 *    N1:pri: Crash
 *    N2:sec: Switch to primary
 *    N2:pri: Write T1 to hsb log
 *    N1:sec: Restart as secondary
 *    N2:pri: Send hsb log and T1 back to N1
 *    N1:sec: Must be able to map T1 and commit it
 *
 * Parameters :
 *
 *      tb - in
 *              pointer to transaction buffer object
 *
 *      rtrxbuf - in out, use
 *              pointer to remote transaction buffer object where
 *              dummy mapping is added
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_trxbuf_addopentrx(
        dbe_trxbuf_t* tb,
        struct dbe_rtrxbuf_st* rtrxbuf,
        bool isdummy)
{
        dbe_trxstmt_t* ts;
        su_rbt_t* rbt;
        dbe_trxbufslot_t* tbs;
        uint i;

        dbe_rtrxbuf_setsearchby(
            rtrxbuf,
            DBE_RTRX_SEARCHBYREMOTE);

        rbt = su_rbt_init(trxrbt_compare, trxrbt_delete);

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);

            ts = tbs->tbs_list;
            while (ts != NULL) {
                /* Transactions that have not ended yet will be added to remote trx buffer */
                CHK_TRXSTMT(ts);
                if (!dbe_trxinfo_isended(ts->ts_trxinfo)){
                    trxrbt_key_t* tk;
                    trxrbt_key_t search_tk;
                    su_rbt_node_t* n;
                    int cmp;
                    dbe_ret_t rc;

                    ss_dprintf_1(("dbe_trxbuf_addopentrx:ti_usertrxid=%ld, ti_trxstate=%ld\n",
                        DBE_TRXID_GETLONG(ts->ts_trxinfo->ti_usertrxid), dbe_trxinfo_getstate(ts->ts_trxinfo)));

                    if (DBE_TRXID_EQUAL(DBE_TRXID_NULL,
                                         dbe_rtrxbuf_localbyremotetrxid(
                                             rtrxbuf,
                                             ts->ts_trxinfo->ti_usertrxid))) {
                        /* Add a new transaction.
                         */
                        ss_dprintf_2(("dbe_trxbuf_addopentrx:add a new transaction\n"));
                        rc = dbe_rtrxbuf_add(rtrxbuf,
                                        ts->ts_trxinfo->ti_usertrxid,
                                        ts->ts_trxinfo->ti_usertrxid,
                                        NULL,
                                        isdummy);
                        ss_rc_dassert(rc == DBE_RC_SUCC, rc);
                        trxrbt_insert(rbt, ts->ts_trxinfo->ti_usertrxid);
                    }
                    /* Find transaction from the tree.
                     */
                    search_tk.tk_trxid = ts->ts_trxinfo->ti_usertrxid;
                    n = su_rbt_search(rbt, &search_tk);
                    if (n == NULL) {
                        /* This transaction is in rtrxbuf from previous run.
                         * Just add the info to the rbt to keep it updated.
                         */
                        ss_dprintf_2(("dbe_trxbuf_addopentrx:old transaction in rtrxbuf, add ts->ts_trxinfo->ti_usertrxid=%ld\n",
                            DBE_TRXID_GETLONG(ts->ts_trxinfo->ti_usertrxid)));
                        trxrbt_insert(rbt, ts->ts_trxinfo->ti_usertrxid);
                    } else {
                        tk = su_rbtnode_getkey(n);
                        cmp = DBE_TRXID_CMP_EX(ts->ts_stmttrxid, tk->tk_stmttrxid);
                        if (cmp > 0) {
                            /* This statement id is bigger than the one added to the
                            * rtrxbuf. Remove the smaller id and add this id. This
                            * is because we want to keep only the last statement id
                            * in the rtrxbuf.
                            */
                            ss_dassert(DBE_TRXID_ISNULL(
                                                dbe_rtrxbuf_localbyremotetrxid(
                                                    rtrxbuf,
                                                    ts->ts_stmttrxid))
                                    || dbe_rtrxbuf_isdummybyremotetrxid(
                                                    rtrxbuf,
                                                    ts->ts_stmttrxid));
                            ss_dassert(!DBE_TRXID_EQUAL(DBE_TRXID_NULL,
                                                dbe_rtrxbuf_localbyremotetrxid(
                                                    rtrxbuf,
                                                    tk->tk_stmttrxid)));
                            if (!DBE_TRXID_EQUAL(tk->tk_stmttrxid, tk->tk_trxid)) {
                                /* Delete if real statement and not transaction id. */
                                rc = dbe_rtrxbuf_deletebylocaltrxid(
                                        rtrxbuf,
                                        tk->tk_stmttrxid);
                                ss_rc_dassert(rc == DBE_RC_SUCC, rc);
                            }
                            if (DBE_TRXID_ISNULL(dbe_rtrxbuf_localbyremotetrxid(
                                                 rtrxbuf,
                                                 ts->ts_stmttrxid))) {
                                rc = dbe_rtrxbuf_add(rtrxbuf,
                                            ts->ts_stmttrxid,
                                            ts->ts_stmttrxid,
                                            NULL,
                                            isdummy);
                                ss_rc_dassert(rc == DBE_RC_SUCC, rc);
                            }

                            ss_dprintf_2(("dbe_trxbuf_addopentrx:set new tk_stmttrxid=%ld\n",
                                DBE_TRXID_GETLONG(ts->ts_stmttrxid)));
        
                            tk->tk_stmttrxid = ts->ts_stmttrxid;
                        } else {
                            ss_dprintf_2(("dbe_trxbuf_addopentrx:old tk_stmttrxid=%ld\n",
                                DBE_TRXID_GETLONG(tk->tk_stmttrxid)));
                        }
                    }
                }
                ts = ts->ts_listnext;
            }
        }
        su_rbt_done(rbt);

        dbe_rtrxbuf_setsearchby(
            rtrxbuf,
            DBE_RTRX_SEARCHBYLOCAL);

}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_marktoberemoved
 *
 *
 *
 * Parameters :
 *
 *      tb -
 *
 *
 *      trxid -
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
void dbe_trxbuf_marktoberemoved(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid)
{
        uint index;
        dbe_trxstmt_t* tsslot;
        dbe_trxstmt_t* prevtsslot;
        dbe_trxbufslot_t* tbs;

        ss_dprintf_1(("dbe_trxbuf_marktoberemoved:trxid = %ld\n", DBE_TRXID_GETLONG(trxid)));
        CHK_TRXBUF(tb);

        tbs = trxbuf_getbufslot(tb, trxid, &index);

        /* ToDo:TBD:mutexing ??? */
        SsFlatMutexLock(tbs->tbs_sem);

        tsslot = TABLE_GET(tb->tb_table, index);

        /* ss_dassert(tsslot != NULL); Assert changed to if, see ss_derror
           explanation below. JarmoR Jun 10, 1996 */
        if (tsslot == NULL) {
            SsFlatMutexUnlock(tbs->tbs_sem);
            return;
        }

        CHK_TRXSTMT(tsslot);

        if (DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, trxid)) {
            TABLE_SET(tb->tb_table, index, tsslot->ts_bufnext);
        } else {
            do {
                prevtsslot = tsslot;
                tsslot = tsslot->ts_bufnext;
                if (tsslot == NULL) {
                    /* ss_derror; If hot stantby replication recovery this dassert
                       may fire if transaction is aborted and log contains
                       switchtoprimary record which removes all uncommitted
                       transactions. JarmoR Jun 8, 1996 */
                    break;
                }
            } while (!DBE_TRXID_EQUAL(tsslot->ts_stmttrxid, trxid));
            if (tsslot != NULL) {
                prevtsslot->ts_bufnext = tsslot->ts_bufnext;
            }
        }
        if (tsslot != NULL) {
            tsslot->ts_bufnext = tsslot;
        }
        SsFlatMutexUnlock(tbs->tbs_sem);
}

/*##**********************************************************************\
 *
 *              dbe_trxbuf_removemarked
 *
 *
 *
 * Parameters :
 *
 *      tb -
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
void dbe_trxbuf_removemarked(
        dbe_trxbuf_t* tb)
{
        dbe_trxstmt_t* ts;
        dbe_trxstmt_t* prev_ts;
        dbe_trxbufslot_t* tbs;
        uint i;

        ss_dprintf_1(("dbe_trxbuf_removemarked\n"));
        CHK_TRXBUF(tb);

        /* ToDo:TBD:mutexing ??? */
        trxbufslot_enterall(tb);

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);

            ts = tbs->tbs_list;
            prev_ts = NULL;
            while (ts != NULL) {
                CHK_TRXSTMT(ts);
                if (ts->ts_bufnext == ts) {
                    dbe_trxstmt_t* del_ts;
                    del_ts = ts;
                    ts = ts->ts_listnext;
                    if (prev_ts == NULL) {
                        /* Removing the first item of the list. */
                        ss_dassert(del_ts == tbs->tbs_list);
                        tbs->tbs_list = ts;
                    } else {
                        /* Removing from the middle of the list. */
                        ss_dassert(del_ts != tbs->tbs_list);
                        prev_ts->ts_listnext = ts;
                    }
                    trxbuf_listdeletefun(del_ts);
                    tbs->tbs_listlen--;
                    ss_dassert(tbs->tbs_listlen >= 0);
                    SS_PMON_ADD(SS_PMON_TRANSBUFREMOVED);
                } else {
                    prev_ts = ts;
                    ts = ts->ts_listnext;
                }
            }
        }
        trxbufslot_exitall(tb);
}

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
/*##**********************************************************************\
 *
 *              dbe_trxbuf_print
 *
 * Prints the contetents of txrbuf.
 *
 * Parameters :
 *
 *      tb -
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
void dbe_trxbuf_print(
        dbe_trxbuf_t* tb)
{
        dbe_trxstmt_t* ts;
        uint i;
        uint nused = 0;
        dbe_trxbufslot_t* tbs;
        long listlen = 0L;

        /* MUTEX BEGIN */
        trxbufslot_enterall(tb);

        for (i = 0; i < tb->tb_tablesize; i++) {
            if (TABLE_GET(tb->tb_table, i) != NULL) {
                nused++;
            }
        }

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];
            CHK_TRXBUFSLOT(tbs);
            listlen = listlen + tbs->tbs_listlen;
        }

        SsDbgPrintf("TRXBUF: Table size %d nused %d List length %d\n",
            tb->tb_tablesize,
            nused,
            listlen);

        SsDbgPrintf("  stmttrxid usertrxid committrxnum maxtrxnum mode isolation state\n");

        for (i=0;i<tb->tb_nbufslots;i++ ) {
            tbs = tb->tb_tbslottable[i];

            for (ts = tbs->tbs_list; ts != NULL; ts = ts->ts_listnext) {
                CHK_TRXSTMT(ts);
                SsDbgPrintf("  %9ld %9ld %12ld %9ld %4d %9d ",
                    DBE_TRXID_GETLONG(ts->ts_stmttrxid),
                    DBE_TRXID_GETLONG(ts->ts_trxinfo->ti_usertrxid),
                    DBE_TRXNUM_GETLONG(ts->ts_trxinfo->ti_committrxnum),
                    DBE_TRXNUM_GETLONG(ts->ts_trxinfo->ti_maxtrxnum),
                    ts->ts_trxinfo->ti_trxmode,
                    ts->ts_trxinfo->ti_isolation);
                switch (dbe_trxinfo_getstate(ts->ts_trxinfo)) {
                    case DBE_TRXST_BEGIN:
                        SsDbgPrintf("BEGIN");
                        break;
                    case DBE_TRXST_VALIDATE:
                        SsDbgPrintf("VALIDATE");
                        break;
                    case DBE_TRXST_COMMIT:
                        SsDbgPrintf("COMMIT");
                        break;
                    case DBE_TRXST_ABORT:
                        SsDbgPrintf("ABORT");
                        break;
                    case DBE_TRXST_TOBEABORTED:
                        SsDbgPrintf("TO BE ABORTED");
                        break;
                    default:
                        ss_error;
                }
                SsDbgPrintf("\n");
            }
        }

        /* MUTEX END */
        trxbufslot_exitall(tb);
}
#endif /* defined(AUTOTEST_RUN) || defined(SS_DEBUG) */
