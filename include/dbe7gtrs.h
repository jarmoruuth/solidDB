/*************************************************************************\
**  source       * dbe7gtrs.h
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


#ifndef DBE7GTRS_H
#define DBE7GTRS_H

#include <ssc.h>
#include <sssem.h>

#include <su0list.h>

#include <rs0tnum.h>

#include "dbe0type.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe7ctr.h"

#define GTRS_NCACHED_ATS  1000

extern bool dbe_gtrs_mergecleanup_recovery;

typedef struct {
        int             at_nlink;
        dbe_trxinfo_t*  at_trxinfo;
        SsSemT*         at_trxbufsem;
        long            at_nmergewrites;
#ifdef SS_SYNC
        su_list_node_t* at_synclistnode;
        rs_tuplenum_t   at_minsynctuplevers;    /* Minimum active value at start */
        bool            at_minsynctupleversused; /* Value used, can not release read level. */
#endif /* SS_SYNC */
        bool            at_iswrites;
        bool            at_isvalidate;
        dbe_trx_t*      at_trx;
        bool            at_trxuncertain;
        su_list_node_t* at_readlevelnode;       /* Node pointer in gtrs_readlevellist
                                                   if we hace a read level. */
        ss_debug(char** at_callstack;)
} gtrs_activetrx_t;

#ifdef SS_SYNC
typedef struct {
        rs_tuplenum_t   st_startsynctuplevers;  /* Counter value at start */
        bool            st_endp;
} gtrs_synctrx_t;
#endif /* SS_SYNC */

struct dbe_gtrs_st {
        dbe_trxbuf_t*   gtrs_trxbuf;          /* Transaction buffer. */
        dbe_counter_t*  gtrs_ctr;             /* Database counter object. */
        su_list_t*      gtrs_validatetrxlist; /* List of gtrs_activetrx_t* of
                                                 transactions under
                                                 validation. Entires are stored
                                                 in the order they entered to
                                                 the validation phase which
                                                 is the transaction
                                                 serialization order. */
        su_list_t*      gtrs_activetrxlist;   /* List of currently active
                                                 transactions. */
        su_list_t*      gtrs_readlevellist;   /* List of currently active
                                                 read levels. Used to
                                                 specify transaction level
                                                 below which all transactions
                                                 are ended. It is the merge
                                                 level. Entries are stored
                                                 in the transaction starting
                                                 order, oldest first. */
#ifdef SS_SYNC
        su_list_t*      gtrs_synctrxlist;   /* List of active transactions.
                                               This list maintains the sync
                                               history read level. Committed
                                               transaction are removed from
                                               the head of the list, never
                                               from the middle. */
#endif
        SsFlatMutexT    gtrs_sem;
        dbe_db_t*       gtrs_db;
        long            gtrs_pendingmergewrites;
        su_list_t*      gtrs_abortedrelhs_list;
        su_rbt_t*       gtrs_abortedrelhs_rbt;
        gtrs_activetrx_t*   gtrs_atcache;
        ulong           gtrs_ncachedats;
        ss_debug(dbe_trxnum_t gtrs_lastcommittrxnum;)
        ss_debug(dbe_trxnum_t gtrs_lastmaxtrxnum;)
        ss_debug(dbe_trxid_t  gtrs_lastaborttrxid;)
        ss_debug(bool  gtrs_insidemutex;)
};

dbe_gtrs_t* dbe_gtrs_init(
        dbe_db_t* db,
        dbe_trxbuf_t* trxbuf,
        dbe_counter_t* ctr);

void dbe_gtrs_done(
        dbe_gtrs_t* gtrs);

void dbe_gtrs_begintrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo,
        rs_sysi_t* cd);

SS_INLINE void dbe_gtrs_addtrxreadlevel_nomutex(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

SS_INLINE void dbe_gtrs_entertrxgate(
        dbe_gtrs_t* gtrs);

SS_INLINE void dbe_gtrs_exittrxgate(
        dbe_gtrs_t* gtrs);

void dbe_gtrs_begintrxvalidate(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

void dbe_gtrs_abortnovalidate(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

void dbe_gtrs_endtrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* ended_trxinfo,
        rs_sysi_t* cd,
        bool commitp,
        bool iswrites,
        long nmergewrites,
        bool updatetrxinfo,
        bool entergate);

bool dbe_gtrs_releasereadlevels(
        dbe_gtrs_t* gtrs);

void dbe_gtrs_getcount(
        dbe_gtrs_t* gtrs,
        ulong* p_activecount,
        ulong* p_validatecount);

void dbe_gtrs_printinfo(
        void* fp,
        dbe_gtrs_t* gtrs);

#ifdef SS_SYNC

rs_tuplenum_t dbe_gtrs_getminsynctuplevers(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

rs_tuplenum_t dbe_gtrs_getfirstsynctuplevers(
        dbe_gtrs_t* gtrs);

rs_tuplenum_t dbe_gtrs_getnewsynctuplevers(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

#endif /* SS_SYNC */

void dbe_gtrs_setactivetrx(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo,
        dbe_trx_t* trx);

void dbe_gtrs_settrxuncertain(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo);

su_list_t* dbe_gtrs_getactivetrxlist(
        dbe_gtrs_t* gtrs);

su_list_t* dbe_gtrs_getuncertaintrxlist(
        dbe_gtrs_t* gtrs);

dbe_opentrxinfo_t* dbe_gtrs_getopentrxinfo_nomutex(
        dbe_gtrs_t* gtrs);

void dbe_gtrs_freeopentrxinfo(
        dbe_opentrxinfo_t* oti);

dbe_ret_t dbe_gtrs_readrelh(
        dbe_gtrs_t*     gtrs,
        ulong           relid,
        dbe_trxnum_t    trxnum);

void dbe_gtrs_abortrelh(
        dbe_gtrs_t*     gtrs,
        ulong           relid,
        dbe_trxnum_t    trxnum);

#if defined(DBE7GTRS_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_gtrs_entertrxgate
 *
 *
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_gtrs_entertrxgate(dbe_gtrs_t* gtrs)
{
        SS_NOTUSED(gtrs);

        /* MUTEX BEGIN */
        SsFlatMutexLock(gtrs->gtrs_sem);

        ss_dassert(!gtrs->gtrs_insidemutex);
        ss_debug(gtrs->gtrs_insidemutex = TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_gtrs_exittrxgate
 *
 *
 *
 * Parameters :
 *
 *	gtrs -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_gtrs_exittrxgate(dbe_gtrs_t* gtrs)
{
        SS_NOTUSED(gtrs);

        ss_dassert(gtrs->gtrs_insidemutex);
        ss_debug(gtrs->gtrs_insidemutex = FALSE);

        /* MUTEX END */
        SsFlatMutexUnlock(gtrs->gtrs_sem);
}

/*##**********************************************************************\
 * 
 *		dbe_gtrs_addtrxreadlevel_nomutex
 * 
 * Adds a new read level for a transaction.
 * 
 * Parameters : 
 * 
 *		gtrs - 
 *			
 *			
 *		trxinfo - 
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
SS_INLINE void dbe_gtrs_addtrxreadlevel_nomutex(
        dbe_gtrs_t* gtrs,
        dbe_trxinfo_t* trxinfo)
{
        gtrs_activetrx_t* at;

        ss_dprintf_1(("gtrs_addreadlevel_nomutex:trxnum=%ld\n", DBE_TRXNUM_GETLONG(trxinfo->ti_maxtrxnum)));
        ss_dassert(!DBE_TRXNUM_ISNULL(trxinfo->ti_maxtrxnum));
        ss_dassert(!DBE_TRXNUM_EQUAL(trxinfo->ti_maxtrxnum, DBE_TRXNUM_MAX));
        ss_dassert(gtrs->gtrs_insidemutex);
        ss_dassert(trxinfo->ti_actlistnode != NULL);

        at = (gtrs_activetrx_t*)su_listnode_getdata(trxinfo->ti_actlistnode);

#ifdef SS_DEBUG
        ss_dassert(at->at_readlevelnode == NULL);
        ss_dassert(DBE_TRXNUM_CMP_EX(gtrs->gtrs_lastmaxtrxnum, at->at_trxinfo->ti_maxtrxnum) <= 0);
        gtrs->gtrs_lastmaxtrxnum = at->at_trxinfo->ti_maxtrxnum;
        if (su_list_length(gtrs->gtrs_readlevellist) > 0) {
            gtrs_activetrx_t* last_at;
            last_at = (gtrs_activetrx_t*)su_listnode_getdata(su_list_last(gtrs->gtrs_readlevellist));
            ss_dassert(!DBE_TRXNUM_ISNULL(last_at->at_trxinfo->ti_maxtrxnum));
            ss_dassert(DBE_TRXNUM_CMP_EX(last_at->at_trxinfo->ti_maxtrxnum, at->at_trxinfo->ti_maxtrxnum) <= 0);
        }
#endif /* SS_DEBUG */

        at->at_readlevelnode = su_list_insertlast(gtrs->gtrs_readlevellist, at);
}

#endif /* defined(DBE7GTRS_C) || defined(SS_USE_INLINE) */

#endif /* DBE7GTRS_H */
