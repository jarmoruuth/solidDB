/*************************************************************************\
**  source       * tab0tran.h
**  directory    * tab
**  description  * Transaction functions.
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


#ifndef TAB0TRAN_H
#define TAB0TRAN_H

#include <su0error.h>

#include <rs0types.h>

#include "tab1defs.h"

#include "dbe0trx.h"

#define CHK_TRANS(trans)    ss_dassert(SS_CHKPTR(trans) && (trans)->tr_chk == TBCHK_TRANS)

#define TRANS_ISACTIVE(trans)  ((trans)->tr_open)

/* For compatibility with old code. */
#define TB_TRANSOPT_NONREPEATABLEREAD TB_TRANSOPT_ISOLATION_READCOMMITTED

typedef enum {
        TB_TRANSOPT_CHECKWRITESET,
        TB_TRANSOPT_CHECKREADSET,
        TB_TRANSOPT_READONLY,
        TB_TRANSOPT_READWRITE,
        TB_TRANSOPT_NOCHECK,
        TB_TRANSOPT_ISOLATION_READCOMMITTED,
        TB_TRANSOPT_ISOLATION_REPEATABLEREAD,
        TB_TRANSOPT_NOINTEGRITY,
        TB_TRANSOPT_NOLOGGING,
        TB_TRANSOPT_HSBFLUSH_YES,
        TB_TRANSOPT_HSBFLUSH_NO,
        TB_TRANSOPT_USEMAXREADLEVEL, /* always see ALL committed changes! */
        TB_TRANSOPT_DURABILITY_DEFAULT,
        TB_TRANSOPT_DURABILITY_RELAXED,
        TB_TRANSOPT_DURABILITY_STRICT,
        TB_TRANSOPT_ISOLATION_SERIALIZABLE,
        TB_TRANSOPT_ISOLATION_DEFAULT,

        /* Ruka:Flow:do not check referential integrity:
         *   should be used only if ref-integrity check is disabled in replica subscribe 
         */ 
        TB_TRANSOPT_REFINTEGRITY,
        TB_TRANSOPT_NOREFINTEGRITY,

        TB_TRANSOPT_SAFENESS_DEFAULT,
        TB_TRANSOPT_SAFENESS_1SAFE,
        TB_TRANSOPT_SAFENESS_2SAFE,

        TB_TRANSOPT_MAX
} tb_transopt_t;

typedef enum {
        TB_TRANS_SYNCST_NONE,
        TB_TRANS_SYNCST_PROPAGATEWRITE,
        TB_TRANS_SYNCST_SUBSCRIBEWRITE,
        TB_TRANS_SYNCST_DISABLE_END_MESSAGE,
        TB_TRANS_SYNCST_ISSAVE,
        TB_TRANS_SYNCST_DISABLE_HISTORY,
        TB_TRANS_SYNCST_SUBSC_DELHISTORY,
        TB_TRANS_SYNCST_MASTEREXEC,
        TB_TRANS_SYNCST_ISPUBLTUPLE
} tb_trans_syncst_t;

struct tbtransstruct {
        ss_debug(tb_check_t tr_chk;)
        dbe_trx_t*          tr_trx;
        SsSemT*             tr_trxsem;
        bool                tr_open;
        tb_transopt_t       tr_checkmode;
        tb_transopt_t       tr_checkmode_once;
        bool                tr_readonly;
        bool                tr_readonly_once;
        bool                tr_nointegrity;
        bool                tr_nointegrity_once;
        bool                tr_norefintegrity;
        bool                tr_norefintegrity_once;
        bool                tr_stmt;
        long                tr_curid;
        uint                tr_trigcnt;
        uint                tr_funccnt;
        uint                tr_stmtgroupcnt;
        bool                tr_usestmtgroup;
        rs_sysi_t*          tr_initcd;
        long                tr_execcount;
        int                 tr_allowtrxend_once;

        bool                tr_commitbeginp;
        bool                tr_autocommit;
        bool                tr_systrans;
        bool                tr_forcecommit;
#ifndef SS_NOLOGGING
        bool                tr_allowlogfailure;
        bool                tr_nologging;
        bool                tr_nologging_once;
#endif /* SS_NOLOGGING */
        bool                tr_usemaxreadlevel;
#ifndef SS_NOLOCKING
        long                tr_locktimeout;     /* Pessimistic lock timeout,
                                                   -1 = use default. */
        long                tr_optimisticlocktimeout; /* Optimistic lock
                                                    timeout, -1 = use default. */
#endif /* SS_NOLOCKING */
        long                tr_idletimeout;
        bool                tr_idletimeoutfired;
        SsTimeT             tr_starttime;
#ifdef SS_SYNC

        tb_trans_syncst_t   tb_syncst;
        bool                tb_savedstmtflag;
        long                tb_syspropagatelockctr;
        bool                tb_issyspropagatelock;
        bool                tr_update_synchistory;
#endif /* SS_SYNC */

        tb_transopt_t       tr_durability;
        tb_transopt_t       tr_durability_once;
        tb_transopt_t       tr_safeness;
        tb_transopt_t       tr_safeness_once;
        rs_sqli_isolation_t tr_lasttrxisolation;
        bool                tr_userelaxedreacommitted;
        dbe_trx_t           tr_trxbuf;
        dbe_ret_t           tr_dbe_ret;
        bool                tr_usertrxcleanup;
        dstr_t              tr_uniquerrorkeyvalue;
        rs_key_t*           tr_errkey;
};

tb_trans_t* tb_trans_init(
        rs_sysi_t* cd
);

void tb_trans_done(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

rs_sysi_t* tb_trans_getinitcd(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

tb_trans_t* tb_trans_rep_init(
        rs_sysi_t*  cd,
        dbe_trx_t*  trx
);

void tb_trans_rep_done(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

void tb_trans_setsystrans(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);   

void tb_trans_setforcecommit(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);   

SS_INLINE bool tb_trans_beginif(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

bool tb_trans_beginwithtrxinfo(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        dbe_trxinfo_t*  trxinfo,
        dbe_trxid_t     readtrxid
);

SS_INLINE bool tb_trans_begintransandstmt(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

bool tb_trans_replicatesql(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* sqlstr,
        bool* p_finishedp,
        su_err_t** p_errh);

SS_INLINE bool tb_trans_isactive(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE bool tb_trans_isusertrxcleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE void tb_trans_setusertrxcleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool usertrxcleanup
);

SS_INLINE dstr_t tb_trans_getuniquerrorkeyvalue(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE rs_key_t* tb_trans_geterrkey(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE dbe_ret_t tb_trans_getdberet(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE void tb_trans_setdberet(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        dbe_ret_t   dbe_rc
);

bool tb_trans_update_synchistory(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);


bool tb_trans_isstmtactive(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

bool tb_trans_iswrites(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

void tb_trans_stmt_begin(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

bool tb_trans_stmt_commit(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool*       p_finished,
        rs_err_t**  errhandle
);

bool tb_trans_stmt_rollback(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool*       p_finished,
        rs_err_t**  errhandle
);

bool tb_trans_stmt_commitandbegin(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        su_err_t** p_errh
);

bool tb_trans_stmt_rollbackandbegin(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        su_err_t** p_errh
);

bool tb_trans_stmt_commit_onestep(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_err_t**  p_errh);

bool tb_trans_stmt_rollback_onestep(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_err_t**  p_errh);

bool tb_trans_savepoint(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        rs_err_t**  errhandle
);

bool tb_trans_savepoint_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        void**      cont,
        rs_err_t**  errhandle
);

bool tb_trans_commit(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool*       p_finished,
        rs_err_t**  errhandle
);

void tb_trans_commit_cleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        dbe_ret_t   rc,
        rs_err_t**  p_errh
);

bool tb_trans_commit_user(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool*       p_finished,
        rs_err_t**  errhandle
);

bool tb_trans_commit_user_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        void**      cont,
        rs_err_t**  errhandle
);

bool tb_trans_rollback(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        bool*       p_finished,
        bool        count_trx,
        rs_err_t**  errhandle
);

bool tb_trans_rollback_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        void**      cont,
        rs_err_t**  errhandle
);

bool tb_trans_rollback_onestep(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool        count_trx,
        rs_err_t**  p_errh);

void tb_trans_enduncertain(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

void tb_trans_setautocommit(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool        autocommitp);

SS_INLINE bool tb_trans_isautocommit(
        tb_trans_t* trans);

void tb_trans_inheritreadlevel(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_trans_t* readtrans);

long tb_trans_getreadlevel(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

void tb_trans_settransoption(
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        tb_transopt_t option
);

SS_INLINE tb_transopt_t tb_trans_getisolation(
        rs_sysi_t*      cd,
        tb_trans_t*     trans
);

char* tb_trans_getisolationname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans
);

char* tb_trans_getdurabilityname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans);

char* tb_trans_getsafenessname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans);

SS_INLINE bool tb_trans_isrelaxedreacommitted(
        rs_sysi_t*      cd,
        tb_trans_t*     trans
);

SS_INLINE bool tb_trans_setcanremovereadlevel_withwrites(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        bool            release_with_writes);

SS_INLINE bool tb_trans_setcanremovereadlevel(
        rs_sysi_t*      cd,
        tb_trans_t*     trans
);

void tb_trans_setdelayedstmterror(
        rs_sysi_t*      cd,
        tb_trans_t*     trans
);

bool tb_trans_settransopt_once(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        tb_transopt_t option,
        su_err_t**    p_errh);

bool tb_trans_setfailed(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        int           errcode
);

bool tb_trans_settimeout(
        rs_sysi_t*         cd,
        tb_trans_t*   trans
);

SS_INLINE void tb_trans_startnewsearch(
        rs_sysi_t*         cd,
        tb_trans_t*   trans
);

bool tb_trans_setrelhchanged(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        rs_relh_t*    relh,
        su_err_t**    p_errh
);

void tb_trans_setddop(
        rs_sysi_t*    cd,
        tb_trans_t*   trans
);

SS_INLINE void tb_trans_logauditinfo(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        char*         info
);

bool tb_trans_isfailed(
        rs_sysi_t*         cd,
        tb_trans_t*   trans
);

su_ret_t tb_trans_geterrcode(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        su_err_t**    p_errh
);

char* tb_trans_authid(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

SS_INLINE dbe_trx_t* tb_trans_dbtrx(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

long tb_trans_newcurid(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

void tb_trans_allowlogfailure(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

long tb_trans_getexeccount(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

SS_INLINE void tb_trans_addexeccount(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

SS_INLINE void tb_trans_allowtrxend_once(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool        allowp);

long tb_trans_getlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

void tb_trans_setlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout);

void tb_trans_setoptimisticlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout);

long tb_trans_getoptimisticlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

void tb_trans_setidletimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout);

long tb_trans_getidletimeout(
        tb_trans_t* trans);

bool tb_trans_isidletimedout(
        tb_trans_t* trans);

void tb_trans_markidletimedout(
        tb_trans_t* trans);

void tb_trans_clearidletimedout(
        tb_trans_t* trans);

void tb_trans_markidletimedout(
        tb_trans_t* trans);

long tb_trans_getduration_sec(
        tb_trans_t* trans);

bool tb_trans_lockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        long timeout,
        bool* p_finished,
        su_err_t** p_errh);

bool tb_trans_lockrelh_long(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        long timeout,
        bool* p_finished,
        su_err_t** p_errh);

void tb_trans_unlockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh);

bool tb_trans_getlockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool* p_isexclusive,
        bool* p_isverylong_duration);

bool tb_trans_lockconvert(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        bool verylong_duration);

bool tb_trans_settriggerp(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_entname_t*   trigname);

void tb_trans_setfunctionp(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_entname_t*   funcname);

void tb_trans_setstmtgroup(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        bool            setp);

void tb_trans_usestmtgroup(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        bool            usestmtgroup);

#ifdef SS_SYNC

void tb_trans_setsyncstate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_trans_syncst_t state);

SS_INLINE tb_trans_syncst_t tb_trans_getsyncstate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_trans_syncst_t state);

void tb_trans_setsavedstmtflag(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool        value);

bool tb_trans_getsavedstmtflag(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

bool tb_trans_syspropagate_trylock(
        rs_sysi_t* cd,
        tb_trans_t* trans);

#endif /* SS_SYNC */

bool tb_trans_iscommitactive(
        rs_sysi_t* cd,
        tb_trans_t* trans);

bool tb_trans_hsbopactive(
        rs_sysi_t* cd,
        tb_trans_t* trans);

bool tb_trans_hsbcommitsent(
        rs_sysi_t* cd,
        tb_trans_t* trans);

bool tb_trans_isreadonly(
        tb_trans_t*   trans);

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
SS_INLINE void tb_trans_set_foreign_key_checks(
        tb_trans_t* trans,
        bool fkey_checks
);

SS_INLINE bool tb_trans_is_foreign_key_checks(
        tb_trans_t* trans
);
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#if defined(TAB0TRAN_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              tb_trans_startnewsearch
 *
 * Informs the transaction object that a new search is started, used e.g.
 * to select a new read level in non-repeatable read mode.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void tb_trans_startnewsearch(
        rs_sysi_t*         cd,
        tb_trans_t*   trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        if (TRANS_ISACTIVE(trans) && !trans->tr_trigcnt && !trans->tr_funccnt) {
            dbe_trx_startnewsearch(trans->tr_trx);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_logauditinfo
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      info -
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
SS_INLINE void tb_trans_logauditinfo(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        char*         info)
{
        CHK_TRANS(trans);
        ss_dassert(info != NULL);

        if (rs_sysi_logauditinfo(cd) && TRANS_ISACTIVE(trans)) {
            /* Note that tr_trx could be NULL if SQL statement
             * is COMMIT or ROLLBACK.
             */
            dbe_trx_logauditinfo(trans->tr_trx, rs_sysi_dbuserid(cd), info);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_dbtrx
 *
 * Returns the database transaction pointer from a table transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value - ref :
 *
 *      database transaction pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_trx_t* tb_trans_dbtrx(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        ss_dassert(trans != NULL);

        return(trans->tr_trx);
}

/*##**********************************************************************\
 *
 *              tb_trans_addexeccount
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
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
SS_INLINE void tb_trans_addexeccount(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans->tr_execcount++;
}

SS_INLINE void tb_trans_allowtrxend_once(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool        allowp)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        if (allowp) {
            /* Here count 2 is because the enabling statement after execution
             * decs it (blind in sp-level) by one.
             * Increment calls come from sp0smsg.
             */
            trans->tr_allowtrxend_once = 2;
        } else {
            if (trans->tr_allowtrxend_once > 0) {
                trans->tr_allowtrxend_once--;
            }
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_getsyncstate
 *
 * Returns sync state of transaction.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      state -
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
SS_INLINE tb_trans_syncst_t tb_trans_getsyncstate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_trans_syncst_t state __attribute__ ((unused)))
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tb_syncst);
}

/*##**********************************************************************\
 *
 *              tb_trans_beginif
 *
 * Begins a new transaction, if a transaction is not yet active.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 *      TRUE    - a new transaction was started
 *      FALSE   - transaction was already active
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool tb_trans_beginif(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        /* If trans is already started we should not have any thread
         * conflicts here.
         */
        if (trans->tr_trx != NULL) {
            ss_dprintf_1(("tb_trans_beginif:%ld, already active\n", (long)trans));
            dbe_trx_restartif(trans->tr_trx);
            return(FALSE);
        } else {
            ss_dprintf_1(("tb_trans_beginif:%ld, start new trans\n", (long)trans));
            return(tb_trans_beginwithtrxinfo(cd, trans, NULL, DBE_TRXID_NULL));
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_begintransandstmt
 *
 * Starts a new transaction and new statement, if one is not active.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 * Return value :
 *
 *      TRUE    - new transaction was started.
 *      FALSE   - transaction was already active.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE bool tb_trans_begintransandstmt(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        bool succp;

        succp = tb_trans_beginif(cd, trans);

        tb_trans_stmt_begin(cd, trans);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_isactive
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
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
SS_INLINE bool tb_trans_isactive(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);
        SS_NOTUSED(cd);
        ss_pprintf_1(("tb_trans_isactive:%ld:%s\n", (long)trans, trans->tr_trx != NULL ? "yes" : "no"));

        return trans->tr_trx != NULL;
}

/*##**********************************************************************\
 *
 *              tb_trans_isautocommit
 *
 * return if transaction is autocommit state
 *
 * Parameters :
 *
 *      trans -
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
SS_INLINE bool tb_trans_isautocommit(
    tb_trans_t* trans)
{
        CHK_TRANS(trans);
        return(trans->tr_autocommit);
}

/*##**********************************************************************\
 *
 *              tb_trans_getisolation
 *
 * Returns current default isolation level. One of TB_TRANSOPT_ISOLATION_*
 * but not TB_TRANSOPT_ISOLATION_DEFAULT. Default is always mapped to
 * some real value.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE tb_transopt_t tb_trans_getisolation(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        tb_transopt_t isolation = TB_TRANSOPT_ISOLATION_READCOMMITTED;

        CHK_TRANS(trans);

        switch (trans->tr_checkmode) {
            case TB_TRANSOPT_ISOLATION_DEFAULT:
                switch (rs_sqli_getisolationlevel(rs_sysi_sqlinfo(cd))) {
                    case RS_SQLI_ISOLATION_READCOMMITTED:
                        isolation = TB_TRANSOPT_ISOLATION_READCOMMITTED;
                        break;
                    case RS_SQLI_ISOLATION_REPEATABLEREAD:
                        isolation = TB_TRANSOPT_ISOLATION_REPEATABLEREAD;
                        break;
                    case RS_SQLI_ISOLATION_SERIALIZABLE:
                        isolation = TB_TRANSOPT_ISOLATION_SERIALIZABLE;
                        break;
                    default:
                        ss_error;
                }
                break;
            case TB_TRANSOPT_NOCHECK:
            case TB_TRANSOPT_ISOLATION_READCOMMITTED:
                isolation = TB_TRANSOPT_ISOLATION_READCOMMITTED;
                break;
            case TB_TRANSOPT_CHECKWRITESET:
            case TB_TRANSOPT_ISOLATION_REPEATABLEREAD:
                isolation = TB_TRANSOPT_ISOLATION_REPEATABLEREAD;
                break;
            case TB_TRANSOPT_CHECKREADSET:
            case TB_TRANSOPT_ISOLATION_SERIALIZABLE:
                isolation = TB_TRANSOPT_ISOLATION_SERIALIZABLE;
                break;
            default:
                ss_error;
        }
        return(isolation);
}

/*##**********************************************************************\
 *
 *              tb_trans_isrelaxedreacommitted
 *
 * Returns TRUE if new READ COMMITTED behaviour is currently used for this
 * transaction. For other than READ COMMITTED isolation always returns FALSE.
 * some real value.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool tb_trans_isrelaxedreacommitted(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        tb_transopt_t isolation;

        CHK_TRANS(trans);

        isolation = tb_trans_getisolation(cd, trans);

        return(trans->tr_userelaxedreacommitted && 
               isolation == TB_TRANSOPT_ISOLATION_READCOMMITTED);
}

/*##**********************************************************************\
 * 
 *      tb_trans_setcanremovereadlevel_withwrites
 * 
 * Sets a flag in READ COMMITTED transaction that allows removing a read 
 * level. Removing the read level helps merge to proceed even if a 
 * transaction if open and idle for a long time.
 *
 * Parameter release_with_writes is used to control if transaction with
 * writes can release the read level or not.
 * 
 * Parameters : 
 * 
 *      cd - 
 *          
 *          
 *      trans - 
 *          
 *          
 *      release_with_writes - 
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
SS_INLINE bool tb_trans_setcanremovereadlevel_withwrites(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        bool            release_with_writes)
{
        bool canremove = FALSE;

        if (TRANS_ISACTIVE(trans) && tb_trans_isrelaxedreacommitted(cd, trans)) {
            canremove = dbe_trx_setcanremovereadlevel(trans->tr_trx, release_with_writes);
        }
        return(canremove);
}

/*##**********************************************************************\
 * 
 *      tb_trans_setcanremovereadlevel
 * 
 * Same as tb_trans_setcanremovereadlevel_withwrites but parameter
 * release_with_writes is always FALSE.
 * 
 * Parameters : 
 * 
 *      cd - 
 *          
 *          
 *      trans - 
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
SS_INLINE bool tb_trans_setcanremovereadlevel(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        return(tb_trans_setcanremovereadlevel_withwrites(cd, trans, FALSE));
}

/*##**********************************************************************\
 *
 *              tb_trans_isusertrxcleanup
 *
 * Returns TRUE if transaction user should issue a transaction cleanup
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value : TRUE if user should do a transaction cleanup
 *                FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */

SS_INLINE bool tb_trans_isusertrxcleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans
)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tr_usertrxcleanup);
}

/*##**********************************************************************\
 *
 *              tb_trans_setusertrxcleanup
 *
 * Set value wheather user should call transaction cleanup
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 *      usertrxcleanup - in, use
 *              value to be set
 *
 * Return value : 
 *                
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void tb_trans_setusertrxcleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool usertrxcleanup
)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans->tr_usertrxcleanup = usertrxcleanup;
}

/*##**********************************************************************\
 *
 *              tb_trans_getuniquerrorkeyvalue
 *
 * Returns dbe unique error key value or NULL if not set.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dstr_t tb_trans_getuniquerrorkeyvalue(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tr_uniquerrorkeyvalue);
}

/*##**********************************************************************\
 *
 *              tb_trans_geterrkey
 *
 * Returns dbe error key or NULL if not set.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_key_t* tb_trans_geterrkey(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tr_errkey);
}

/*##**********************************************************************\
 *
 *              tb_trans_setdberet
 *
 * Set dbe ret value to a transaction object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 *      dbe_rc - in, use
 *              value to be set
 *
 * Return value : 
 *                
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void tb_trans_setdberet(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        dbe_ret_t   dbe_rc
)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans->tr_dbe_ret = dbe_rc;
}

/*##**********************************************************************\
 *
 *              tb_trans_getdberet
 *
 * Returns dbe return code from the transaction object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value : DBE return code
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_ret_t tb_trans_getdberet(
        rs_sysi_t*  cd,
        tb_trans_t* trans
)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tr_dbe_ret);
}

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
/*##**********************************************************************\
 *
 *              tb_trans_set_foreign_key_checks
 *
 * Sets tr_foreign_key_checks member
 *
 * Parameters :
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void tb_trans_set_foreign_key_checks(
        tb_trans_t* trans,
        bool fkey_checks)
{
        CHK_TRANS(trans);

        dbe_trx_set_foreign_key_check(trans->tr_trx, fkey_checks);
}

/*##**********************************************************************\
 *
 *              tb_trans_get_foreign_key_checks
 *
 * Gets tr_foreign_key_checks value
 *
 * Parameters :
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool tb_trans_get_foreign_key_checks(
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        return dbe_trx_get_foreign_key_check(trans->tr_trx);
}
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

#endif /* defined(TAB0TRAN_C) || defined(SS_USE_INLINE) */

#endif /* TAB0TRAN_H */
