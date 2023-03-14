/*************************************************************************\
**  source       * dbe7trxb.h
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


#ifndef DBE7TRXB_H
#define DBE7TRXB_H

#include <ssc.h>

#include "dbe0type.h"
#include "dbe9type.h"
#include "dbe0erro.h"
#include "dbe8cach.h"
#include "dbe8flst.h"
#include "dbe7trxi.h"
#include "dbe7rtrx.h"
#include "dbe6log.h"

#define CHK_TRXBUF(tb)       ss_dassert(SS_CHKPTR(tb) && (tb)->tb_chk == DBE_CHK_TRXBUF)
#define CHK_TRXBUFSLOT(tbs)  ss_dassert(SS_CHKPTR(tb) && (tbs)->tbs_chk == DBE_CHK_TRXBUFSLOT)
#define CHK_TRXSTMT(ts)      ss_dassert(SS_CHKPTR(ts) && (ts)->ts_chk == DBE_CHK_TRXSTMT)

typedef struct dbe_trxstmt_st dbe_trxstmt_t;
typedef struct dbe_trxbufslot_st dbe_trxbufslot_t;

/* ---------------------------------------------------------------
 *
 */
struct dbe_trxstmt_st {
        ss_debug(dbe_chk_t ts_chk;)
        dbe_trxid_t       ts_stmttrxid;   /* Statement transaction id. */
        dbe_trxinfo_t*    ts_trxinfo;     /* Trx info pointer. */
        dbe_trxstmt_t*    ts_bufnext;     /* Next pointer used in trxbuf hash. */
        dbe_trxstmt_t*    ts_listnext;    /* Next pointer used in stmt list. */
        dbe_trxbuf_t*     ts_tb;
};

/* ---------------------------------------------------------------
 *
 */
struct dbe_trxbufslot_st {
        ss_debug(dbe_chk_t tbs_chk;)
        dbe_trxstmt_t*   tbs_list;       /* List containing dbe_trxstmt_t*
                                           entries. */
        long             tbs_listlen;
        SsFlatMutexT     tbs_sem;
};

/* ---------------------------------------------------------------
 *
 */
struct dbe_trxbuf_st {
        ss_debug(dbe_chk_t tb_chk;)
#ifdef TABLE_SPLITALLOC
        dbe_trxstmt_t*** tb_table;    /* Hash table containing dbe_trxstmt_t*
                                         entries hashed by ti_usertrxid. */
#else
        dbe_trxstmt_t**  tb_table;
#endif
        uint             tb_tablesize;  /* Size of tb_table. */
        dbe_trxid_t      tb_aborttrxid; /* Trx ids smaller than or equal
                                           to this value and not found from
                                           the buffer are aborted. */
        dbe_trxid_t      tb_visiblealltrxid; /* Trx id below which all
                                                transactions are visible. */
        bool             tb_usevisiblealltrxid;
        dbe_trxbufslot_t** tb_tbslottable;
        uint             tb_nbufslots;

        SsQsemT**        tb_trxinfo_sem;
        dbe_trxid_t      tb_disabletrxid; /* Used during recover to mask out a trxid. */
};

dbe_trxbuf_t* dbe_trxbuf_init(
        uint bufsize,
        dbe_trxid_t aborttrxid,
        bool usevisiblealltrxid);

void dbe_trxbuf_done(
        dbe_trxbuf_t* tb);

void dbe_trxbuf_setusevisiblealltrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t aborttrxid,
        bool usevisiblealltrxid);

SS_INLINE bool dbe_trxbuf_usevisiblealltrxid(
        dbe_trxbuf_t* tb);

SS_INLINE void dbe_trxbuf_setaborttrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t aborttrxid);

dbe_trxid_t dbe_trxbuf_getaborttrxid(
        dbe_trxbuf_t* tb);

void dbe_trxbuf_add(
        dbe_trxbuf_t* tb,
        dbe_trxinfo_t* ti);

void dbe_trxbuf_addstmt(
        dbe_trxbuf_t* tb,
        dbe_trxid_t stmttrxid,
        dbe_trxinfo_t* ti);

void dbe_trxbuf_abortstmt(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum,
        dbe_trxid_t stmttrxid);

void* dbe_trxbuf_disablestmt(
        dbe_trxbuf_t* tb,
        dbe_trxid_t stmttrxid);

void dbe_trxbuf_enablestmt(
        dbe_trxbuf_t* tb,
        void* ctx,
        dbe_trxid_t stmttrxid);

long dbe_trxbuf_clean(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t cleantrxnum,
        dbe_trxid_t mergestart_aborttrxid,
        dbe_opentrxinfo_t* oti);

dbe_trxstate_t dbe_trxbuf_gettrxstate(
        dbe_trxbuf_t* tb,
        dbe_trxid_t keytrxid,
        dbe_trxnum_t* p_trxnum,
        dbe_trxid_t* p_usertrxid);

dbe_trxinfo_t* dbe_trxbuf_gettrxinfo(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid);

dbe_trxinfo_t* dbe_trxbuf_gettrxinfo_linked(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid);

dbe_ret_t dbe_trxbuf_save(
        dbe_trxbuf_t* tb,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_trxlistdaddr,
        su_daddr_t* p_stmttrxlistdaddr);

dbe_ret_t dbe_trxbuf_restore(
        dbe_trxbuf_t* tb,
        dbe_cache_t* cache,
        su_daddr_t trxlistdaddr,
        su_daddr_t stmttrxlistdaddr);

ulong dbe_trxbuf_getcount(
        dbe_trxbuf_t* tb);
                         
void dbe_trxbuf_printinfo(
        void* fp,
        dbe_trxbuf_t* tb);

void dbe_trxbuf_cleanuncommitted(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum);

void dbe_trxbuf_addopentrx(
        dbe_trxbuf_t* tb,
        struct dbe_rtrxbuf_st* rtrxbuf,
        bool isdummy);

void dbe_trxbuf_marktoberemoved(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid);

void dbe_trxbuf_removemarked(
        dbe_trxbuf_t* tb);

SS_INLINE SsSemT* dbe_trxbuf_getsembytrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid);

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)

void dbe_trxbuf_print(
        dbe_trxbuf_t* tb);

#endif /* defined(AUTOTEST_RUN) || defined(SS_DEBUG) */

#ifdef SS_HSBG2

int dbe_trxbuf_abortall(
        dbe_trxbuf_t* tb,
        dbe_trxnum_t aborttrxnum,
        dbe_log_t* log,
        bool* p_reload_rbuf);

#endif /* SS_HSBG2 */

#if defined(DBE7TRXB_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              dbe_trxbuf_setaborttrxid
 *
 * Sets the level below or equal which all transactions must be aborted
 * or they must be found from the buffer.
 *
 * Parameters :
 *
 *      tb -
 *
 *
 *      aborttrxid -
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
SS_INLINE void dbe_trxbuf_setaborttrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t aborttrxid)
{
        CHK_TRXBUF(tb);

        if (!tb->tb_usevisiblealltrxid) {
            /* MUTEX BEGIN */
            SsQsemEnter(tb->tb_trxinfo_sem[0]);

            /* ss_dassert(tb->tb_aborttrxid <= aborttrxid); Not true after hot
                                                            standby recovery. */
            if (DBE_TRXID_CMP_EX(aborttrxid, tb->tb_aborttrxid) > 0) {
                ss_dprintf_1(("dbe_trxbuf_setaborttrxid:aborttrxid = %ld\n", DBE_TRXID_GETLONG(aborttrxid)));
                tb->tb_aborttrxid = aborttrxid;
            }

            /* MUTEX END */
            SsQsemExit(tb->tb_trxinfo_sem[0]);
        }
}

SS_INLINE bool dbe_trxbuf_usevisiblealltrxid(dbe_trxbuf_t* tb)
{
        CHK_TRXBUF(tb);
        ss_dprintf_1(("dbe_trxbuf_usevisiblealltrxid:usevisiblealltrxid=%d\n", tb->tb_usevisiblealltrxid));

        return(tb->tb_usevisiblealltrxid);
}

SS_INLINE SsSemT* dbe_trxbuf_getsembytrxid(
        dbe_trxbuf_t* tb,
        dbe_trxid_t trxid)
{
        uint index;

        CHK_TRXBUF(tb);

        index = (ulong)DBE_TRXID_GETLONG(trxid) % tb->tb_nbufslots;

        return(tb->tb_trxinfo_sem[index]);
}

#endif /* defined(DBE7TRXB_C) || defined(SS_USE_INLINE) */

#endif /* DBE7TRXB_H */
