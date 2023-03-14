/*************************************************************************\
**  source       * dbe1trx.h
**  directory    * dbe
**  description  * Transaction structure. This header is intended
**               * for internal use only. Only transaction modules
**               * can include this header.
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


#ifndef DBE1TRX_H
#define DBE1TRX_H

#include <ssstdio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssthread.h>

#include <su0prof.h>
#include <su0rbtr.h>
#include <su0list.h>
#include <su0bflag.h>

#include <uti0dyn.h>

#include <rs0types.h>
#include <rs0error.h>
#include <rs0sysi.h>
#include <rs0sqli.h>
#include <rs0rbuf.h>
#include <rs0key.h>
#include <rs0viewh.h>
#include <rs0relh.h>

#include "dbe0type.h"
#include "dbe9type.h"
#include "dbe7trxi.h"
#include "dbe5ivld.h"
#include "dbe1trdd.h"
#include "dbe0trx.h"

typedef enum {
        TRX_FLAG_DELAYEDBEGINVALIDATE = SU_BFLAG_BIT(0),
        TRX_FLAG_DTABLE               = SU_BFLAG_BIT(1),
        TRX_FLAG_MTABLE               = SU_BFLAG_BIT(2),
        TRX_FLAG_USEREADLEVEL         = SU_BFLAG_BIT(3),
        TRX_FLAG_OLDSTATEMENTERROR    = SU_BFLAG_BIT(4), /* Statement error that is already returned. */
        TRX_FLAG_DELAYEDSTMTERROR     = SU_BFLAG_BIT(5)  /* SS_MYSQL, delay statement errors to statement commit. */
} trx_flags_t;

#define CHK_TRX(t) ss_dassert(SS_CHKPTR(t) && (t)->trx_chk == DBE_CHK_TRX)
#define CHK_DBETRX(t) ss_dassert(SS_CHKPTR(t) && (t)->dtrx_chk == DBE_CHK_DBETRX)
#define CHK_TCI(t)    ss_dassert(SS_CHKPTR(t) && (t)->tci_chk == DBE_CHK_TCI)

typedef enum {
        TRX_COMMITST_INIT,
        TRX_COMMITST_VALIDATE,
        TRX_COMMITST_RUNDDOP,
        TRX_COMMITST_REPINIT,
        TRX_COMMITST_REPEXEC,
        TRX_COMMITST_REPFLUSH,
        TRX_COMMITST_REPREADY,
        TRX_COMMITST_WAITREADLEVEL,
        TRX_COMMITST_DONE
} trx_commitst_t;

typedef enum {
        TRX_TROPST_BEFORE,
        TRX_TROPST_MIDDLE,
        TRX_TROPST_AFTER,
        TRX_TROPST_DONE
} trx_tropst_t;

/* Transaction validation state during read write transaction
 * validation. The following state transition are possible:
 *      
 *      TRX_VLDST_READS  -> TRX_VLDST_KEYS
 *      TRX_VLDST_WRITES -> TRX_VLDST_KEYS
 */
typedef enum {
        TRX_VLDST_INIT,
        TRX_VLDST_READS,
        TRX_VLDST_WRITES,
        TRX_VLDST_KEYS,
        TRX_VLDST_END
} trx_vldst_t;

typedef union {
        struct {
            su_list_node_t* node;
        } vr_;
        struct {
            su_list_node_t* node;
        } vw_;
        struct {
            dbe_btrsea_timecons_t   tc;
            su_list_node_t*         node;
        } vk_;
} trx_vldinfo_t;

typedef enum {
        TRX_FLUSH_UNKNOWN,
        TRX_FLUSH_YES,
        TRX_FLUSH_NO
} trx_flush_policy_t;

typedef struct {
        ss_debug(dbe_chk_t tci_chk;)
        SsSemT*            tci_sem;
        rs_sysi_t*         tci_cd;
        int                tci_userid;
        su_rbt_t*          tci_cardinrbt;      /* Relation cardinality changes
                                                  done in trx. */
        su_rbt_t*          tci_stmtcardinrbt;  /* Relation cardinality changes
                                                  done in statement. */
        su_list_t*         tci_stmtcardingrouplist;
        ss_debug(dbe_trx_t* tci_trx;)
} trx_cardininfo_t;

/* Transaction object.
 */
struct dbe_trx_st {
        ss_debug(dbe_chk_t trx_chk;)
        trx_mode_t      trx_mode;           /* Transaction mode. */
        trx_commitst_t  trx_commitst;       /* Commit state. */
        trx_tropst_t    trx_tropst;         /* Trop state. */
        trx_mode_t      trx_defaultwritemode; /* Default mode for a
                                                 read write transaction. */
        trx_vldst_t     trx_vldst;          /* Read write transaction
                                               validation state. */
        trx_vldinfo_t   trx_vldinfo;        /* Validate info buffer, used
                                               during validate. */
        dbe_trxid_t     trx_usertrxid;      /* User transaction id. */
        dbe_trxid_t     trx_stmttrxid;      /* Statement transaction id.
                                               There is an active statement,
                                               if value is different than
                                               trx_usertrxid. */
        dbe_trxid_t     trx_readtrxid;      /* Max trx id visible in reads. */
        dbe_trxinfo_t*  trx_info;           /* Info containing ids. */
        dbe_trxnum_t    trx_committrxnum;   /* Commit transaction number,
                                               stored also into trx_info. */
        dbe_trxnum_t    trx_searchtrxnum;   /* Search transaction number. */
        dbe_trxnum_t    trx_stmtsearchtrxnum; /* Search transaction number after last statement. */
        dbe_trxnum_t    trx_tmpmaxtrxnum;   /* If not DBE_TRXNUM_NULL, this
                                               is the read level used to
                                               validate write checks. Set by
                                               dbe_trx_inheritreadlevel(). */
        dbe_db_t*       trx_db;             /* Database object. */
        dbe_user_t*     trx_user;           /* User of the transaction. */
        dbe_index_t*    trx_index;          /* Index system used. */
        rs_sysi_t*      trx_cd;             /* User info object */
        dbe_gtrs_t*     trx_gtrs;           /* Global transaction state. */
        dbe_counter_t*  trx_counter;        /* Global counter object. */
        dbe_log_t*      trx_log;            /* Logical log  subsystem */
        bool            trx_stoplogging;    /* If TRUE, logging is stopped
                                               for this trx. */
        su_list_t       trx_keychklist;     /* List of trx_keycheck_t. */
        su_list_t       trx_readchklist;    /* List of trx_readcheck_t. */
        su_list_t       trx_writechklist;   /* List of trx_writecheck_t. */
        dbe_trdd_t*     trx_trdd;           /* Data dictionary trx object. */
        bool            trx_ddopact;        /* If TRUE, at end db object
                                               must be notified of an ended
                                               dd operation (because it was
                                               aborted abnormally). */
        bool            trx_isddop;         /* There is DD operation that
                                               is not listed in trx_trdd. */
        dbe_ret_t       trx_errcode;        /* If transaction is set as
                                               failed by dbe_trx_setfailed,
                                               the error code is set here. */
        rs_key_t*       trx_err_key;        /* Constraint that has cuased
                                             * constraint violation. */
        long            trx_nindexwrites;   /* Number of index writes. */
        long            trx_nmergewrites;   /* Number of index writes. */
        long            trx_nlogwrites;     /* Number of log writes. */
        bool            trx_rollbackdone;   /* If TRUE, trx has timed out. */
        bool            trx_earlyvld;       /* If TRUE, early validate is
                                               used. */
        dbe_trxbuf_t*   trx_trxbuf;
        bool            trx_puttolog;       /* If TRUE, transaction end mark
                                               is added to the log.
                                               Used during trx validate. */
        bool            trx_abort_no_validate; /* If TRUE, the transaction
                                                  has been aborted without
                                                  validating it.
                                                  Used during trx validate. */
        bool            trx_updatetrxinfo;
        ss_debug(trx_cardininfo_t* trx_cardininfo_in_cd;)
        long            trx_stmtcnt;
        bool            trx_stmtiswrites;
        bool            trx_nonrepeatableread;
        bool            trx_trxaddedtotrxbuf;
        bool            trx_stmtaddedtotrxbuf;
        bool            trx_nointegrity;
        bool            trx_norefintegrity;
        trx_flush_policy_t trx_flush_policy; /* flush/noflush/default*/
        dbe_indvld_t*   trx_chksearch;
        dbe_indvld_t    trx_chksearchbuf;
        dbe_escalatelimits_t* trx_escalatelimits;
        bool            trx_hsbflushallowed;
        void*           trx_hsbctx;
        void*           trx_hsbsqlctx;
        dbe_hsbctx_funs_t* trx_hsbctxfuns;

        bool            trx_forcecommit;
        bool            trx_usemaxreadlevel;
        bool            trx_delaystmtcommit;
#ifndef SS_NOLOCKING
        dbe_lockmgr_t*  trx_lockmgr;
        dbe_locktran_t* trx_locktran;
        bool            trx_locktran_localinitp;
        long            trx_pessimistic_lock_to;
        long            trx_optimistic_lock_to;
#endif /* SS_NOLOCKING */

#ifndef SS_NOSEQUENCE
        dbe_seq_t*      trx_seq;
        su_rbt_t*       trx_seqrbt;         /* Sequence changes. */
#endif /* SS_NOSEQUENCE */

        dbe_mmlocklst_t* trx_mmll;

#ifdef DBE_REPLICATION
        bool            trx_replication;
        dbe_ret_t       trx_stmterrcode;
        bool            trx_replicaslave;
        su_list_t*      trx_repsqllist;
        dbe_tuplestate_t*trx_tuplestate;
        rep_params_t    trx_rp;
        bool            trx_committrx_hsb;
        bool            trx_migratehsbg2;
        bool            trx_hsbcommitsent;
        bool            trx_replicarecovery;
        bool            trx_hsbtrxmarkwrittentolog;
        bool            trx_hsbstmtmarkwrittentolog;
        bool            trx_is2safe;
        bool            trx_usersafeness;
        ss_debug(dbe_trxid_t trx_hsbstmtlastwrittentolog;)
#endif /* DBE_REPLICATION */

        dbe_hsbmode_t   trx_hsbg2mode;

#ifdef SS_SYNC
        long            trx_savestmtctr;
#endif /* SS_SYNC */

        long            trx_table_lock_to;  /* for SYNC conflict resolution */

        FAKE_CODE(bool  trx_fakewaitp;)
        FAKE_CODE(bool  trx_fakepausep;)
            
        su_list_t*      trx_groupstmtlist;
        SsSemT*         trx_sem;
        ss_debug(long   trx_waitreadlevelstarttime;)
        ss_debug(bool   trx_opactivep;)
        int             trx_waitreadlevelloopcnt;
        bool*           trx_openflag;
        trx_flags_t     trx_flags;
        dbe_trxinfo_t*  trx_infocache;
        ss_debug(int    trx_thrid;)

        dstr_t          trx_uniquerrorkeyvalue;
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        bool            trx_foreign_key_checks;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
        
};

trx_mode_t dbe_trx_mapisolation(
        dbe_trx_t* trx,
        rs_sqli_isolation_t isolation);

void dbe_trx_addstmttotrxbuf(
        dbe_trx_t* trx);

dbe_ret_t dbe_trx_localrollback(
        dbe_trx_t* trx,
        bool enteractionp,
        bool count_trx,
        rs_err_t** p_errh);

void dbe_trx_freecardininfo(
        rs_sysi_t* cd);

#ifdef SS_DEBUG
bool dbe_trx_checkcardininfo(
        dbe_trx_t* trx);

#endif

dbe_ret_t dbe_trx_stmtrollbackrepsql(
        dbe_trx_t* trx,
        dbe_trxid_t stmttrxid);

dbe_ret_t dbe_trx_endrepsql(
        dbe_trx_t* trx,
        bool commitp);

dbe_ret_t dbe_trx_seqcommit_nomutex(
        dbe_trx_t* trx);

void dbe_trx_seqtransend_nomutex(
        dbe_trx_t* trx,
        bool commitp);

bool dbe_trx_initrepparams(
        dbe_trx_t* trx,
        rep_type_t type);

su_ret_t dbe_trx_replicate(
        dbe_trx_t* trx,
        rep_type_t type);

bool dbe_trx_setfailurecode(
        dbe_trx_t* trx,
        dbe_ret_t rc);

bool dbe_trx_setfailurecode_nomutex(
        dbe_trx_t* trx,
        dbe_ret_t rc);

SS_INLINE void dbe_trx_abortif_nomutex(
        dbe_trx_t* trx);

#ifdef SS_MYSQL

#define dbe_trx_sementer(trx)   ss_dassert((trx)->trx_thrid == SsThrGetid())
#define dbe_trx_semexit(trx)

#else /* SS_MYSQL */

SS_INLINE void dbe_trx_sementer(
        dbe_trx_t* trx);

SS_INLINE void dbe_trx_semexit(
        dbe_trx_t* trx);

#endif /* SS_MYSQL */

#ifdef SS_DEBUG
bool dbe_trx_semisentered(
        dbe_trx_t* trx);

bool dbe_trx_semisnotentered(
        dbe_trx_t* trx);

#endif /* SS_DEBUG */

#ifdef DBE_HSB_REPLICATION

SS_INLINE dbe_tuplestate_t * dbe_trx_gettuplestate(
        dbe_trx_t* trx);

SS_INLINE void dbe_trx_settuplestate(
        dbe_trx_t* trx,
        dbe_tuplestate_t *ts);

#endif /* DBE_HSB_REPLICATION */

#define dbe_trx_ensureusertrxid(trx) ss_dassert(!DBE_TRXID_ISNULL((trx)->trx_usertrxid))

SS_INLINE void dbe_trx_ensurereadlevel(
        dbe_trx_t* trx, 
        bool entertrxgate);

void dbe_trx_getnewreadlevel(
        dbe_trx_t* trx, 
        bool entertrxgate);

SS_INLINE bool dbe_trx_needtoaddreadcheck(
        dbe_trx_t* trx);

SS_INLINE void dbe_trx_setflag(
        dbe_trx_t* trx,
        trx_flags_t flag);

SS_INLINE bool dbe_trx_isflag(
        dbe_trx_t* trx,
        trx_flags_t flag);

void dbe_trx_getrelhcardin_nomutex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes);

void dbe_trx_cardintrans_mutexif(
        rs_sysi_t* cd,
        bool commitp,
        bool entermutex,
        bool freetrxcardininfo);

void dbe_trx_cardinstmttrans_mutexif(
        rs_sysi_t* cd,
        bool commitp,
        bool groupstmtp,
        bool entermutex);

#ifdef SS_FAKE
void dbe_trx_semexit_fake(
        dbe_trx_t* trx, 
        bool dofake);
#endif

/* internal */
void dbe_trx_restart(
        dbe_trx_t* trx);


dbe_ret_t dbe_trx_markwrite_local(
        dbe_trx_t* trx, 
        bool stmtoper, 
        bool puttolog);

void dbe_trx_abort_local(
        dbe_trx_t* trx,
        bool enteractionp);

#if defined(DBE0TRUT_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *      dbe_trx_ensurereadlevel
 * 
 * Ensures that transaction has a read level.
 * 
 * Parameters : 
 * 
 *  trx - in, use
 *      
 *      
 *  entertrxgate - in 
 *      If TRUE enters gtrs mutex gate.
 *          
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE void dbe_trx_ensurereadlevel(dbe_trx_t* trx, bool entertrxgate)
{
        CHK_TRX(trx);

        if (DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum)) {
            dbe_trx_getnewreadlevel(trx, entertrxgate);
        }
}

SS_INLINE void dbe_trx_setflag(
        dbe_trx_t* trx,
        trx_flags_t flag)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setflag:flag=%d\n", flag));

        if ((flag & TRX_FLAG_DTABLE) != 0) {
            dbe_trx_ensurereadlevel(trx, TRUE);
        }

        trx->trx_flags = (trx_flags_t)((int)(trx->trx_flags) | (int)flag);
}

SS_INLINE bool dbe_trx_isflag(
        dbe_trx_t* trx,
        trx_flags_t flag)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_isflag:flag=%d, %s\n", flag, (trx->trx_flags & flag) ? "YES" : "NO"));

        return (trx->trx_flags & flag) != 0;
}

#ifndef SS_MYSQL

/*##**********************************************************************\
 *
 *              dbe_trx_sementer
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE void dbe_trx_sementer(dbe_trx_t* trx)
{
        CHK_TRX(trx);

        if (!rs_sysi_isinsidedbeatomicsection(trx->trx_cd)) {
            SsSemEnter(trx->trx_sem);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_semexit
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE void dbe_trx_semexit(dbe_trx_t* trx)
{
        CHK_TRX(trx);

        if (!rs_sysi_isinsidedbeatomicsection(trx->trx_cd)) {
            SsSemExit(trx->trx_sem);
        }

#ifdef SS_FAKE
        dbe_trx_semexit_fake(trx, TRUE);
#endif /* SS_FAKE */
}

#endif /* SS_MYSQL */

/*##**********************************************************************\
 *
 *              dbe_trx_needtoaddreadcheck
 *
 * Returns TRUE if this transaction wants to do read set validation. Read 
 * check info should be then added using function dbe_trx_addreadcheck.
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE bool dbe_trx_needtoaddreadcheck(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return (trx->trx_errcode == DBE_RC_SUCC && \
                (trx->trx_mode == TRX_CHECKREADS || \
                 (trx->trx_mode == TRX_NOWRITES && \
                  trx->trx_defaultwritemode == TRX_CHECKREADS)));
}

/*##**********************************************************************\
 *
 *              dbe_trx_gettuplestate
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE dbe_tuplestate_t * dbe_trx_gettuplestate(dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_tuplestate);
}

/*##**********************************************************************\
 *
 *              dbe_trx_settuplestate
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      ts -
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
SS_INLINE void dbe_trx_settuplestate(dbe_trx_t* trx, dbe_tuplestate_t * ts)
{
        CHK_TRX(trx);

        trx->trx_tuplestate = ts;
}

/*##**********************************************************************\
 *
 *              dbe_trx_abortif_nomutex
 *
 *
 *
 * Parameters :
 *
 *      trx -
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
SS_INLINE void dbe_trx_abortif_nomutex(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_3(("dbe_trx_abortif\n"));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        switch (trx->trx_errcode) {
            case DBE_ERR_HSBABORTED:
            case DBE_ERR_HSBSECONDARY:
                if (!trx->trx_rollbackdone) {
                    dbe_trx_abort_local(trx, FALSE);
                }
                break;
            default:
                break;
        }
}

#endif /* defined(DBE0TRUT_C) || defined(SS_USE_INLINE) */

#endif /* DBE1TRX_H */
