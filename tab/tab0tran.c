/*************************************************************************\
**  source       * tab0tran.c
**  directory    * tab
**  description  * Transaction functions
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

#define TAB0TRAN_C

#include <ssenv.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <sssem.h>
#include <sstimer.h>

#include <su0usrid.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0bull.h>
#include <rs0auth.h>
#include <rs0relh.h>

#define DBE_TRX_INTERNAL

#include "tab1dd.h"
#include "tab0cata.h"
#include "tab0relc.h"
#include "tab0tran.h"
#include "tab0type.h"
#include "tab0relh.h"

#ifndef SS_MYSQL
#include "tab0sync.h"
#endif /* !SS_MYSQL */

#include "dbe0user.h"

static long  snc_sysprogate_lock_ctr;
static void* snc_sysprogate_lock_name;

static void trans_syspropagate_initlock(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        trans->tb_syspropagatelockctr = snc_sysprogate_lock_ctr;
        trans->tb_issyspropagatelock = FALSE;
}

bool tb_trans_syspropagate_trylock(
        rs_sysi_t* cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        if (!trans->tb_issyspropagatelock) {
            /* We do not have loci yet.
             */
            rs_sysi_rslinksem_enter(cd);

            if (trans->tb_syspropagatelockctr == snc_sysprogate_lock_ctr &&
                snc_sysprogate_lock_name == NULL) {
                /* Lock is free and has not been locked since this
                 * transaction started.
                 */
                trans->tb_issyspropagatelock = TRUE;
                snc_sysprogate_lock_name = cd;
            }

            rs_sysi_rslinksem_exit(cd);
        }

        return(trans->tb_issyspropagatelock);
}

static void trans_syspropagate_freelock(
        rs_sysi_t* cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        if (trans->tb_issyspropagatelock) {

            trans->tb_issyspropagatelock = FALSE;
            trans->tb_syspropagatelockctr = -1L;

            rs_sysi_rslinksem_enter(cd);

            snc_sysprogate_lock_ctr++;
            snc_sysprogate_lock_name = NULL;

            rs_sysi_rslinksem_exit(cd);
        }
}

static void trans_clear_errorkey(
        rs_sysi_t* cd, 
        tb_trans_t* trans)
{
        if (trans->tr_uniquerrorkeyvalue != NULL) {
            dstr_free(&trans->tr_uniquerrorkeyvalue);
        }
        if (trans->tr_errkey != NULL) {
            rs_key_done(cd, trans->tr_errkey);
            trans->tr_errkey = NULL;
        }
}

static void trans_check_errorkey(
        rs_sysi_t* cd, 
        tb_trans_t* trans)
{
        dstr_t uniquerrorkeyvalue;
        rs_key_t* key;

        uniquerrorkeyvalue = dbe_trx_giveuniquerrorkeyvalue(trans->tr_trx);
        if (uniquerrorkeyvalue != NULL) {
            dstr_free(&trans->tr_uniquerrorkeyvalue);
            trans->tr_uniquerrorkeyvalue = uniquerrorkeyvalue;
        }

        key = dbe_trx_giveerrkey(trans->tr_trx);
        if (key != NULL) {
            if (trans->tr_errkey != NULL) {
                rs_key_done(cd, trans->tr_errkey);
            }
            trans->tr_errkey = key;
        }
}

/*#***********************************************************************\
 *
 *              trans_cleanup
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
 *      trop -
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
static void trans_cleanup(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_trop_t trop)
{
        /* rs_trend_transend handling is moved to dbe0trx
         * rs_trend_transend(rs_sysi_gettrend(cd), cd, trans, trop);
         */

        switch (trop) {
            case RS_TROP_AFTERCOMMIT:
            case RS_TROP_AFTERROLLBACK:
                trans_syspropagate_freelock(cd, trans);
            default:
                break;
        }
}

/*#***********************************************************************\
 *
 *              trans_init
 *
 * Inits transaction structure.
 *
 * Parameters :
 *
 *      cd -
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
static tb_trans_t* trans_init(rs_sysi_t* cd)
{
        tb_trans_t* trans;
        dbe_user_t* user;

        SS_NOTUSED(cd);

        user = rs_sysi_user(cd);

        trans = SSMEM_NEW(tb_trans_t);
        trans->tr_trx = NULL;
        trans->tr_trxsem = dbe_user_gettrxsem(user);
        trans->tr_open = FALSE;
        trans->tr_checkmode = TB_TRANSOPT_ISOLATION_DEFAULT;
        trans->tr_checkmode_once = TB_TRANSOPT_ISOLATION_DEFAULT;
        trans->tr_readonly = FALSE;
        trans->tr_readonly_once = FALSE;
        trans->tr_nointegrity = FALSE;
        trans->tr_nointegrity_once = FALSE;
        trans->tr_norefintegrity = FALSE;
        trans->tr_norefintegrity_once = FALSE;
        trans->tr_stmt = FALSE;
        trans->tr_curid = 0L;
        trans->tr_initcd = cd;
        trans->tr_trigcnt = 0;
        trans->tr_funccnt = 0;
        trans->tr_stmtgroupcnt = 0;
        trans->tr_execcount = 0;
        trans->tr_allowtrxend_once = 0;

        trans->tr_commitbeginp = FALSE;
        trans->tr_autocommit = FALSE; /* by default autocommit is off */
        trans->tr_forcecommit = FALSE;
#ifndef SS_NOLOGGING
        trans->tr_allowlogfailure = FALSE;
        trans->tr_nologging = FALSE;
        trans->tr_nologging_once = FALSE;
#endif /* SS_NOLOGGING */
        trans->tr_usemaxreadlevel = FALSE;
#ifndef SS_NOLOCKING
        trans->tr_locktimeout = -1L;
        trans->tr_optimisticlocktimeout = -1L;
#endif /* SS_NOLOCKING */
        trans->tr_idletimeout = -1L;
        trans->tr_idletimeoutfired = FALSE;
#ifdef SS_SYNC
        trans->tb_syncst = TB_TRANS_SYNCST_NONE;
        trans->tb_savedstmtflag = FALSE;
#endif
        trans->tr_durability = TB_TRANSOPT_DURABILITY_DEFAULT;
        trans->tr_durability_once = TB_TRANSOPT_DURABILITY_DEFAULT;
        trans->tr_safeness = TB_TRANSOPT_SAFENESS_DEFAULT;
        trans->tr_safeness_once = TB_TRANSOPT_SAFENESS_DEFAULT;

        trans->tr_lasttrxisolation = rs_sqli_getisolationlevel(rs_sysi_sqlinfo(cd));
        trans->tr_userelaxedreacommitted = rs_sqli_userelaxedreacommitted(rs_sysi_sqlinfo(cd));

        trans->tr_dbe_ret = DBE_RC_SUCC;
        trans->tr_usertrxcleanup = FALSE;
        trans->tr_uniquerrorkeyvalue = NULL;
        trans->tr_errkey = NULL;

        /* set callback function to sysi for function ISTRANSACTION_ACTIVE */
        rs_sysi_set_istransactive_fun(
                cd,
                trans,
                (long(*)(rs_sysi_t*,void*))tb_trans_getexeccount);
        ss_debug(trans->tr_chk = TBCHK_TRANS);

        switch (rs_sysi_getconnecttype(cd)) {
            case RS_SYSI_CONNECT_SYSTEM:
            case RS_SYSI_CONNECT_HSB:
                trans->tr_systrans = TRUE;
                break;
            default:
                trans->tr_systrans = FALSE;
                break;
        }

        dbe_trx_locktran_init(cd);

        dbe_trx_initbuf(&trans->tr_trxbuf, rs_sysi_user(cd));

        CHK_TRANS(trans);

        return(trans);
}

/*##**********************************************************************\
 *
 *              tb_trans_init
 *
 * Creates a new transaction handle. A new transaction is started by
 * the function tb_trans_beginif.
 *
 * Parameters :
 *
 *      cd - in, hold
 *              client data
 *
 * Return value - give :
 *
 *      Pointer to the transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_trans_t* tb_trans_init(cd)
    rs_sysi_t*       cd;
{
        return(trans_init(cd));
}

static void trans_trxdone(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans,
        bool        issoft)
{
        ss_pprintf_3(("trans_trxdone:%ld\n", (long)trans));
        ss_dassert(!trans->tr_open);
        if (trans->tr_trx == &trans->tr_trxbuf) {
            dbe_trx_donebuf(&trans->tr_trxbuf, issoft, TRUE);
        } else {
            dbe_trx_done(trans->tr_trx);
        }
        trans->tr_trx = NULL;
}

/*##**********************************************************************\
 *
 *              tb_trans_done
 *
 * Releases the transaction handle. If a transaction is active, it is
 * rolled back.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, take
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_trans_done(cd, trans)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
{
        dbe_ret_t   rc;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(!trans->tr_trigcnt);
        ss_dassert(!trans->tr_funccnt);

        dbe_trx_unlockall_long(cd);

        if (trans->tr_trx != NULL) {
            dbe_trx_restartif(trans->tr_trx);

            /* Dbe transaction is active, abort it. */
            trans_cleanup(cd, trans, RS_TROP_BEFOREROLLBACK);
            trans_check_errorkey(cd, trans);

            ss_pprintf_1(("tb_trans_done:%ld:rollback, trxid=%ld\n", (long)trans, DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx))));

            rc = dbe_trx_rollback(trans->tr_trx, TRUE, NULL);
            ss_dassert(rc == DBE_RC_SUCC);

            trans_trxdone(cd, trans, FALSE);

            trans_cleanup(cd, trans, RS_TROP_AFTERROLLBACK);
        }

        trans_clear_errorkey(cd, trans);

        dbe_trx_donebuf(&trans->tr_trxbuf, FALSE, FALSE);

        rs_sysi_set_istransactive_fun(cd, NULL, NULL);
        dbe_trx_locktran_done(cd);

        SsMemFree(trans);
}

rs_sysi_t* tb_trans_getinitcd(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        return(trans->tr_initcd);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              tb_trans_rep_init
 *
 * Creates a new transaction handle for replication. The database
 * transaction is given as a parameter.
 *
 * Parameters :
 *
 *      cd - in, hold
 *              client data
 *
 *      trx - in, hold
 *              database transaction
 *
 * Return value - give :
 *
 *      Pointer to the transaction handle.
 *
 * Limitations  :
 *
 * Globals used :
 */
tb_trans_t* tb_trans_rep_init(cd, trx)
    rs_sysi_t*  cd;
        dbe_trx_t*  trx;
{
        tb_trans_t* trans;

        trans = trans_init(cd);

        trans->tr_trx = trx;
        trans->tr_open = TRUE;

        return(trans);
}

/*##**********************************************************************\
 *
 *              tb_trans_rep_done
 *
 * Releases the replication transaction handle. The database transaction
 * object is not rolled back.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, take
 *              handle to the current transaction
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_trans_rep_done(cd, trans)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
{
        SS_NOTUSED(cd);

        CHK_TRANS(trans);

        SsMemFree(trans);
}

/*##**********************************************************************\
 *
 *              tb_trans_setsystrans
 *
 * Sets the transaction as system transaction.
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
void tb_trans_setsystrans(
    rs_sysi_t*  cd,
    tb_trans_t* trans)
{
        SS_NOTUSED(cd);

        CHK_TRANS(trans);

        trans->tr_systrans = TRUE;
}

/*##**********************************************************************\
 *
 *              tb_trans_setforcecommit
 *
 * Sets transaction to committed mode. Used in some system transactions
 * with HSB G2 where default trx mode in secondary is read-only.
 *
 * This mode is similar to nocheck mode but index writes are done to
 * Bonsai-tree.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              trans -
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
void tb_trans_setforcecommit(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        dbe_trx_setforcecommit(trans->tr_trx);
        trans->tr_forcecommit = TRUE;
}

/*##**********************************************************************\
 *
 *              tb_trans_replicatesql
 *
 * Replicate a SQL string.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      sqlstr -
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
bool tb_trans_replicatesql(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* sqlstr,
        bool* p_finishedp,
        su_err_t** p_errh)
{
        dbe_ret_t rc;
        rs_auth_t *auth;
        char *sqlcatalog;
        char *sqlschema;

        CHK_TRANS(trans);

        if (trans->tr_trx == NULL) {
            /* Transaction not active for some SET commands. */
            *p_finishedp = TRUE;
            return(TRUE);
        }

        auth = rs_sysi_auth(cd);
        sqlcatalog = rs_auth_catalog(cd, auth);
        if (sqlcatalog == NULL) {
            sqlcatalog = (char *)"";
        }
        sqlschema = rs_auth_schema(cd, auth);
        if (sqlschema == NULL) {
            sqlschema = (char *)"";
        }
        rc = dbe_trx_replicatesql(trans->tr_trx, sqlcatalog, sqlschema, sqlstr);
        if (rc == DBE_RC_CONT) {
            *p_finishedp = FALSE;
            return(TRUE);
        } else {
            *p_finishedp = TRUE;
            if (rc == DBE_RC_SUCC) {
                return(TRUE);
            } else {
                su_err_init(p_errh, rc);
                return(FALSE);
            }
        }
}

#endif /* DBE_REPLICATION */

/*#***********************************************************************\
 *
 *              trans_sementer
 *
 * Enters table level transaction mutex.
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
static void trans_sementer(tb_trans_t* trans)
{
        if (!trans->tr_systrans) {
            SsSemEnter(trans->tr_trxsem);
        }
}

/*#***********************************************************************\
 *
 *              trans_semexit
 *
 * Exits table level transaction mutex.
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
static void trans_semexit(tb_trans_t* trans)
{
        if (!trans->tr_systrans) {
            SsSemExit(trans->tr_trxsem);
        }
}

/*#***********************************************************************\
 *
 *              trans_sementer_nocheck
 *
 * Unconfitionally enters table level transaction mutex.
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
static void trans_sementer_nocheck(tb_trans_t* trans)
{
        SsSemEnter(trans->tr_trxsem);
}

/*#***********************************************************************\
 *
 *              trans_semexit_nocheck
 *
 * Unconditionally exits table level transaction mutex.
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
static void trans_semexit_nocheck(tb_trans_t* trans)
{
        SsSemExit(trans->tr_trxsem);
}

/*##**********************************************************************\
 *
 *              tb_trans_beginwithtrxinfo
 *
 * Begins transaction using a given trxinfo.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      trxinfo -
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
bool tb_trans_beginwithtrxinfo(
        rs_sysi_t*           cd,
        tb_trans_t*     trans,
        dbe_trxinfo_t*  trxinfo,
        dbe_trxid_t     readtrxid)
{
        tb_transopt_t   checkmode;
        rs_sqli_isolation_t isolation;

        CHK_TRANS(trans);

        ss_dassert(!trans->tr_open);
        ss_dassert(trans->tr_trx == NULL);

        ss_bassert(!trans->tr_trigcnt);
        ss_bassert(!trans->tr_funccnt);
        ss_bassert(!trans->tr_stmtgroupcnt);

        ss_pprintf_1(("tb_trans_beginif:%ld:start new trx\n", (long)trans));

        trans->tr_starttime = ss_timer_curtime_sec;

        trans_syspropagate_initlock(cd, trans);

        if (trxinfo != NULL) {
            ss_dassert(!trans->tr_usemaxreadlevel);
            trans->tr_trx = dbe_trx_beginreplicarecovery(
                                rs_sysi_user(trans->tr_initcd),
                                trxinfo);
            if (!DBE_TRXID_ISNULL(readtrxid)) {
                dbe_trx_setreadtrxid(trans->tr_trx, readtrxid);
            }


        } else if (trans->tr_usemaxreadlevel) {
            trans->tr_trx =
                dbe_trx_beginwithmaxreadlevel(
                        rs_sysi_user(trans->tr_initcd));
        } else {
            trans->tr_trx = dbe_trx_beginbuf(&trans->tr_trxbuf, rs_sysi_user(trans->tr_initcd));
        }

        if (trans->tr_readonly || trans->tr_readonly_once) {
            dbe_trx_setreadonly(trans->tr_trx);
            trans->tr_readonly_once = FALSE;
        }
        if (trans->tr_forcecommit) {
            dbe_trx_setforcecommit(trans->tr_trx);
        }

        if (trans->tr_nointegrity || trans->tr_nointegrity_once) {
            dbe_trx_setnointegrity(trans->tr_trx);
            trans->tr_nointegrity_once = FALSE;
        }

        /* dont check referentian integrity:for nor Flow refrech uses this (Ruka).
         * Later this can be used in canned transaction and other (bulk) transactions.
         * Maybe this check feature should be escalated to statement level also...;)
         */
        if (trans->tr_norefintegrity || trans->tr_norefintegrity_once) {
            dbe_trx_setrefintegrity_check(trans->tr_trx, FALSE);
            trans->tr_norefintegrity_once = FALSE;
        }
        if (trans->tr_durability_once != TB_TRANSOPT_DURABILITY_DEFAULT) {
            dbe_trx_setflush(
                trans->tr_trx,
                trans->tr_durability_once == TB_TRANSOPT_DURABILITY_STRICT);
            trans->tr_durability_once = TB_TRANSOPT_DURABILITY_DEFAULT;
        } else if (trans->tr_durability != TB_TRANSOPT_DURABILITY_DEFAULT) {
            dbe_trx_setflush(
                trans->tr_trx,
                trans->tr_durability == TB_TRANSOPT_DURABILITY_STRICT);
        }

        if (trans->tr_safeness_once != TB_TRANSOPT_SAFENESS_DEFAULT) {
            dbe_trx_set2safe_user(
                trans->tr_trx,
                trans->tr_safeness_once == TB_TRANSOPT_SAFENESS_2SAFE);
            trans->tr_safeness_once = TB_TRANSOPT_SAFENESS_DEFAULT;
        } else if (trans->tr_safeness != TB_TRANSOPT_SAFENESS_DEFAULT) {
            dbe_trx_set2safe_user(
                trans->tr_trx,
                trans->tr_safeness == TB_TRANSOPT_SAFENESS_2SAFE);
        }

#ifndef SS_NOLOCKING
        if (trans->tr_locktimeout != -1L) {
            dbe_trx_setlocktimeout(trans->tr_trx, trans->tr_locktimeout);
        }
        if (trans->tr_optimisticlocktimeout != -1L) {
            dbe_trx_setoptimisticlocktimeout(trans->tr_trx, trans->tr_optimisticlocktimeout);
        }
#endif /* SS_NOLOCKING */

        trans->tr_commitbeginp = FALSE;
        trans->tr_execcount = 0;

        trans->tr_allowtrxend_once = 0;

        if (trans->tr_checkmode_once != TB_TRANSOPT_ISOLATION_DEFAULT) {
            checkmode = trans->tr_checkmode_once;
            trans->tr_checkmode_once = TB_TRANSOPT_ISOLATION_DEFAULT;
        } else {
            checkmode = trans->tr_checkmode;
        }

        switch (checkmode) {
            case TB_TRANSOPT_ISOLATION_DEFAULT:
                break;
            case TB_TRANSOPT_CHECKWRITESET:
                dbe_trx_setcheckwriteset(trans->tr_trx);
                break;
            case TB_TRANSOPT_CHECKREADSET:
                dbe_trx_setcheckreadset(trans->tr_trx);
                break;
            case TB_TRANSOPT_ISOLATION_READCOMMITTED:
                dbe_trx_setisolation(
                    trans->tr_trx,
                    RS_SQLI_ISOLATION_READCOMMITTED);
                break;
            case TB_TRANSOPT_ISOLATION_REPEATABLEREAD:
                dbe_trx_setisolation(
                    trans->tr_trx,
                    RS_SQLI_ISOLATION_REPEATABLEREAD);
                break;
            case TB_TRANSOPT_ISOLATION_SERIALIZABLE:
                dbe_trx_setisolation(
                    trans->tr_trx,
                    RS_SQLI_ISOLATION_SERIALIZABLE);
                break;
            case TB_TRANSOPT_NOCHECK:
                dbe_trx_setnocheck(trans->tr_trx);
                break;
            default:
                ss_error;
        }
#ifndef SS_NOLOGGING
        if (trans->tr_allowlogfailure || trans->tr_nologging || trans->tr_nologging_once) {
            dbe_trx_stoplogging(trans->tr_trx);
            trans->tr_nologging_once = FALSE;
        }
#endif /* SS_NOLOGGING */

#ifdef SS_SYNC
#ifdef SS_MYSQL
        trans->tb_savedstmtflag = FALSE;
        trans->tr_update_synchistory = FALSE;
#else
        trans->tb_savedstmtflag = FALSE;
        trans->tr_update_synchistory = (tb_sync_replica_count > 0);
#endif /* SS_MYSQL */
#endif /* SS_SYNC */

        isolation = dbe_trx_getisolation(trans->tr_trx);
        if (isolation != trans->tr_lasttrxisolation) {
            if (trans->tr_userelaxedreacommitted) {
                dbe_trx_signalisolationchange(trans->tr_trx);
            }
            trans->tr_lasttrxisolation = isolation;
        }

        trans->tr_dbe_ret = DBE_RC_SUCC;

        trans->tr_open = TRUE;

        dbe_trx_setopenflag(trans->tr_trx, &trans->tr_open);

        trans_clear_errorkey(cd, trans);

        if (su_usrid_traceflags != 0) {
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_SQL, 1, (char *)"trans begin");
        }

        return(TRUE);
}

#ifdef SS_SYNC
bool tb_trans_update_synchistory(cd, trans)
        rs_sysi_t*  cd;
        tb_trans_t* trans;
{
        CHK_TRANS(trans);
        SS_NOTUSED(cd);

        return trans->tr_update_synchistory;
}
#endif

/*##**********************************************************************\
 *
 *              tb_trans_iswrites
 *
 * Checks if there are any write operations in a transaction.
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
bool tb_trans_iswrites(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        bool iswrites;

        CHK_TRANS(trans);
        SS_NOTUSED(cd);

        trans_sementer(trans);

        if (TRANS_ISACTIVE(trans)) {
            iswrites = dbe_trx_iswrites(trans->tr_trx);
        } else {
            iswrites = FALSE;
        }

        trans_semexit(trans);

        return(iswrites);
}

/*##**********************************************************************\
 *
 *              tb_trans_isstmtactive
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
bool tb_trans_isstmtactive(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);
        SS_NOTUSED(cd);

        return(trans->tr_stmt);
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_begin
 *
 * Begins a new statement in a transaction.
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
void tb_trans_stmt_begin(cd, trans)
        rs_sysi_t*  cd __attribute__ ((unused));
        tb_trans_t* trans;
{
        CHK_TRANS(trans);
        ss_dprintf_1(("tb_trans_stmt_begin:%ld\n", (long)trans));

        if (trans->tr_trx == NULL) {
            ss_pprintf_2(("tb_trans_stmt_begin:%ld:No dbe transaction is active\n", (long)trans));
            return;
        }

        if (!trans->tr_stmt) {
            dbe_trx_stmt_begin(trans->tr_trx);
            trans->tr_stmt = TRUE;
            ss_pprintf_2(("tb_trans_stmt_begin:%ld:begin a new statement, trxid=%ld, stmttrxid=%ld\n",
                (long)trans,
                DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx)),
                DBE_TRXID_GETLONG(dbe_trx_getstmttrxid(trans->tr_trx))));
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_commit
 *
 * Commits a statement in a transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      p_finished - out, give
 *          TRUE is stored in *p_finished if the operation
 *          terminated (either successfully or in error),
 *          FALSE is stored in *p_finished if the operation did
 *          not complete
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_stmt_commit(cd, trans, p_finished, p_errh)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
    bool*       p_finished;
    rs_err_t**  p_errh;
{
        dbe_ret_t rc;
        ss_beta(char buf[80];)

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(p_finished != NULL);

        if (trans->tr_trx == NULL) {
            ss_pprintf_2(("tb_trans_stmt_commit:%ld:No dbe transaction is active\n", (long)trans));
            *p_finished = TRUE;
            return(TRUE);
        }

        if (!trans->tr_stmt) {
            ss_pprintf_2(("tb_trans_stmt_commit:%ld:No statement is active\n", (long)trans));
            *p_finished = TRUE;
            return(TRUE);
        }

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld, stmttrxid=%ld",
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx)),
            DBE_TRXID_GETLONG(dbe_trx_getstmttrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_stmt_commit:%ld%s\n", (long)trans, buf));

        do {
            rc = dbe_trx_stmt_commit(trans->tr_trx, trans->tr_stmtgroupcnt, p_errh);
        } while (rc == DBE_RC_CONT && rs_sysi_decstepctr(cd) > 0);

        if (rc != DBE_RC_CONT) {
            ss_pprintf_2(("tb_trans_stmt_commit:%ld:rc=%d\n", (long)trans, rc));
            ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[stmt commit, code %d%s]\n", rs_sysi_userid(cd), rc, buf));
            if (su_usrid_traceflags != 0) {
                su_usrid_trace(
                    rs_sysi_userid(cd),
                    SU_USRID_TRACE_SQL,
                    1,
                    (char *)"stmt commit (%d) %s",
                    rc,
                    (rc == DBE_RC_SUCC || p_errh == NULL) ? "" : su_err_geterrstr(*p_errh));
            }
            ss_dassert(rc == DBE_RC_SUCC || p_errh == NULL || *p_errh != NULL);
            trans_check_errorkey(cd, trans);
            trans->tr_stmt = FALSE;
            *p_finished = TRUE;
            return(rc == DBE_RC_SUCC);
        } else {
            ss_dprintf_2(("tb_trans_stmt_commit:%ld:continue", (long)trans));
            *p_finished = FALSE;
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_rollback
 *
 * Rolls back a statement in a transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      p_finished - out, give
 *          TRUE is stored in *p_finished if the operation
 *          terminated (either successfully or in error),
 *          FALSE is stored in *p_finished if the operation did
 *          not complete
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_stmt_rollback(cd, trans, p_finished, p_errh)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
    bool*       p_finished;
    rs_err_t**  p_errh;
{
        dbe_ret_t rc;
        ss_beta(char buf[80];)

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(p_finished != NULL);

        if (trans->tr_trx == NULL) {
            ss_pprintf_2(("tb_trans_stmt_rollback:%ld:No dbe transaction is active\n", (long)trans));
            *p_finished = TRUE;
            return(TRUE);
        }

        if (!trans->tr_stmt) {
            ss_pprintf_2(("tb_trans_stmt_rollback:%ld:No statement is active\n", (long)trans));
            /* Must be called even if there is no statement, because
               the whole idea of this is to abort ("rollback") searches
               without a statement. */
            dbe_trx_rollback_searches(trans->tr_trx);
        
            *p_finished = TRUE;
            return(TRUE);
        }

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld, stmttrxid=%ld",
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx)),
            DBE_TRXID_GETLONG(dbe_trx_getstmttrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_stmt_rollback:%ld%s\n", (long)trans, buf));

        do {
            rc = dbe_trx_stmt_rollback(trans->tr_trx, trans->tr_stmtgroupcnt, p_errh);
        } while (rc == DBE_RC_CONT && rs_sysi_decstepctr(cd) > 0);

        if (rc != DBE_RC_CONT) {
            ss_pprintf_2(("tb_trans_stmt_rollback:%ld:rc=%d\n", (long)trans, rc));
            ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[stmt rollback%s]\n", rs_sysi_userid(cd), buf));
            if (su_usrid_traceflags != 0) {
                su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_SQL, 1, (char *)"stmt rollback");
            }
            trans_check_errorkey(cd, trans);
            trans->tr_stmt = FALSE;
            *p_finished = TRUE;
            return(rc == DBE_RC_SUCC);
        } else {
            ss_dprintf_2(("tb_trans_stmt_rollback:%ld:continue\n", (long)trans));
            *p_finished = FALSE;
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_commitandbegin
 *
 * Commits current statement and starts a new statement.
 * NOTE! Statement commit is executed in one step.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      p_errh -
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
bool tb_trans_stmt_commitandbegin(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        su_err_t** p_errh)
{
        bool succp;

        ss_pprintf_1(("tb_trans_stmt_commitandbegin:%ld\n", (long)trans));

        succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);

        tb_trans_stmt_begin(cd, trans);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_rollbackandbegin
 *
 * Rolls back current statement and starts a new statement.
 * NOTE! Statement rollback is executed in one step.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      p_errh -
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
bool tb_trans_stmt_rollbackandbegin(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        su_err_t** p_errh)
{
        bool succp;

        ss_pprintf_1(("tb_trans_stmt_rollbackandbegin:%ld\n", (long)trans));

        succp = tb_trans_stmt_rollback_onestep(cd, trans, p_errh);

        tb_trans_stmt_begin(cd, trans);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_commit_onestep
 *
 * Runs transaction statement commit in one step.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      p_errh -
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
bool tb_trans_stmt_commit_onestep(
    rs_sysi_t*  cd,
    tb_trans_t* trans,
    rs_err_t**  p_errh)
{
    bool succp;
    bool finishedp = FALSE;

        do {
            succp = tb_trans_stmt_commit(cd, trans, &finishedp, p_errh);
        } while (!finishedp);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_stmt_rollback_onestep
 *
 * Runs transaction statement rollback in one step.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      p_errh -
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
bool tb_trans_stmt_rollback_onestep(
    rs_sysi_t*  cd,
    tb_trans_t* trans,
    rs_err_t**  p_errh)
{
    bool succp;
    bool finishedp = FALSE;

        do {
            succp = tb_trans_stmt_rollback(cd, trans, &finishedp, p_errh);
        } while (!finishedp);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_savepoint
 *
 * Creates a savepoint with a specified name to a transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      spname - in, use
 *              name for the savepoint
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_savepoint(cd, trans, spname, p_errh)
    rs_sysi_t*  cd;
    tb_trans_t* trans __attribute__ ((unused));
    char*       spname;
    rs_err_t**  p_errh;
{
        SS_NOTUSED(cd);
        SS_NOTUSED(spname);
        CHK_TRANS(trans);
        ss_dassert(!trans->tr_trigcnt);
        ss_dassert(!trans->tr_funccnt);
        ss_dassert(!trans->tr_stmtgroupcnt);

        rs_error_create(p_errh, E_TRXSPNOSUP);

        return(FALSE);
}

/*##**********************************************************************\
 *
 *              tb_trans_savepoint
 *
 * Member of the SQL function block.
 * Creates a savepoint with a specified name to a transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      spname - in, use
 *              name for the savepoint
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_savepoint_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        void**      cont,
        rs_err_t**  p_errh )
{
        *cont = NULL;
        return ( tb_trans_savepoint(cd, trans, spname, p_errh) );
}

/*#***********************************************************************\
 *
 *              trans_resetflags
 *
 * Resets flags after transaction has ended.
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
static void trans_resetflags(tb_trans_t* trans)
{
        trans->tr_readonly_once = FALSE;
        trans->tr_nologging_once = FALSE;
        trans->tr_allowtrxend_once = 0;
        trans->tr_execcount = 0;
        trans->tr_nointegrity_once = FALSE;
        trans->tr_norefintegrity_once = FALSE;
}

/*##**********************************************************************\
 *
 *              tb_trans_commit
 *
 * Member of the SQL function block.
 * Commits a transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      p_finished - out, give
 *          TRUE is stored in *p_finished if the operation
 *          terminated (either successfully or in error),
 *          FALSE is stored in *p_finished if the operation did
 *          not complete
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_commit(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        bool*       p_finished,
        rs_err_t**  p_errh)
{
        dbe_ret_t rc;
        ss_beta(char buf[80];)

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(p_finished != NULL);

        if (trans->tr_trx == NULL) {
            /* No dbe transaction is active. */
            trans_resetflags(trans);
            *p_finished = TRUE;
            return(TRUE);
        }

        if (trans->tr_trigcnt || trans->tr_funccnt) {
            *p_finished = TRUE;
            return(TRUE);
        }

        if (trans->tr_allowtrxend_once == 0) {
            if (trans->tb_syncst == TB_TRANS_SYNCST_MASTEREXEC) {
                su_err_init(p_errh, SNC_ERR_ILLCOMMITROLLBACK);
                *p_finished = TRUE;
                return(FALSE);
            }
        }

        if (!trans->tr_commitbeginp) {
            dbe_trx_restartif(trans->tr_trx);
            trans->tr_commitbeginp = TRUE;
            trans_cleanup(cd, trans, RS_TROP_BEFORECOMMIT);

        }

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld", DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_commit:%ld%s\n", (long)trans, buf));

        do {
            rc = dbe_trx_commit(trans->tr_trx, FALSE, p_errh);
        } while (rc == DBE_RC_CONT && rs_sysi_decstepctr(cd) > 0);


        tb_trans_setdberet(cd, trans, rc);
        
        if (rc != DBE_RC_CONT && !tb_trans_isusertrxcleanup(cd, trans)) {
            tb_trans_commit_cleanup(cd, trans, rc, p_errh);
            *p_finished = TRUE;
            return(rc == DBE_RC_SUCC);
        } else if (rc != DBE_RC_CONT && tb_trans_isusertrxcleanup(cd, trans)) {
            *p_finished = TRUE;
            return(rc == DBE_RC_SUCC);
        } else {
            *p_finished = FALSE;
            
            return(TRUE);
        }

}

/*##**********************************************************************\
 *
 *              tb_trans_commit_cleanup
 *
 * Cleanup transaction object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      rc - in out, use,
 *              DBE return code
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
void tb_trans_commit_cleanup(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        dbe_ret_t   rc,
        rs_err_t**  p_errh)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans_check_errorkey(cd, trans);
        
        trans->tr_allowtrxend_once = 0;

        trans_trxdone(cd, trans, TRUE);

        ss_pprintf_1(("tb_trans_commit:%ld:rc=%d\n", (long)trans, rc));
        ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[trans commit, code %d]\n", rs_sysi_userid(cd), rc));
        if (su_usrid_traceflags != 0) {
                su_usrid_trace(
                    rs_sysi_userid(cd),
                    SU_USRID_TRACE_SQL,
                    1,
                    (char *)"trans commit (%d) %s",
                    rc,
                    (rc == DBE_RC_SUCC || p_errh == NULL) ? "" : su_err_geterrstr(*p_errh));
        }
        
        trans_resetflags(trans);
        trans->tr_stmt = FALSE;
        trans_cleanup(cd, trans, rc == DBE_RC_SUCC ? RS_TROP_AFTERCOMMIT : RS_TROP_AFTERROLLBACK);
}

/*##**********************************************************************\
 *
 *              tb_trans_commit_user
 *
 * Commits a user transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      p_finished - out, give
 *          TRUE is stored in *p_finished if the operation
 *          terminated (either successfully or in error),
 *          FALSE is stored in *p_finished if the operation did
 *          not complete
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_commit_user(cd, trans, p_finished, p_errh)
    rs_sysi_t*  cd;
    tb_trans_t* trans;
        bool*       p_finished;
    rs_err_t**  p_errh;
{
        dbe_ret_t rc;
        ss_beta(char buf[80];)

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(p_finished != NULL);

        if (trans->tr_trx == NULL) {
            /* No dbe transaction is active. */
            trans_resetflags(trans);
            *p_finished = TRUE;
            return(TRUE);
        }

        if (trans->tr_trigcnt) {
            su_err_init(p_errh, E_TRIGILLCOMMITROLLBACK);
            *p_finished = TRUE;
            return(FALSE);
        }

        if (trans->tr_funccnt) {
            su_err_init(p_errh, E_FUNCILLCOMMITROLLBACK);
            *p_finished = TRUE;
            return(FALSE);
        }

        if (trans->tr_allowtrxend_once == 0) {
            if (trans->tb_syncst == TB_TRANS_SYNCST_MASTEREXEC) {
                su_err_init(p_errh, SNC_ERR_ILLCOMMITROLLBACK);
                *p_finished = TRUE;
                return(FALSE);
            }
        }

        if (!trans->tr_commitbeginp) {
            dbe_trx_restartif(trans->tr_trx);
            trans->tr_commitbeginp = TRUE;
            trans_cleanup(cd, trans, RS_TROP_BEFORECOMMIT);
        }

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld", DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_commit_user:%ld%s\n", (long)trans, buf));

        do {
            rc = dbe_trx_commit(trans->tr_trx, TRUE, p_errh);
            FAKE_CODE_BLOCK_GT(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 0, { break; });
        } while (rc == DBE_RC_CONT && rs_sysi_decstepctr(cd) > 0);

        tb_trans_setdberet(cd, trans, rc);

        if (rc != DBE_RC_CONT) {

            trans->tr_allowtrxend_once = 0;

            trans_trxdone(cd, trans, TRUE);

            ss_pprintf_1(("tb_trans_commit_user:%ld:rc=%d\n", (long)trans, rc));
            ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[trans commit user, code %d%s]\n", rs_sysi_userid(cd), rc, buf));
            if (su_usrid_traceflags != 0) {
                su_usrid_trace(
                    rs_sysi_userid(cd),
                    SU_USRID_TRACE_SQL,
                    1,
                    (char *)"trans commit (%d) %s",
                    rc,
                    (rc == DBE_RC_SUCC || p_errh == NULL) ? "" : su_err_geterrstr(*p_errh));
            }
            trans_resetflags(trans);
            trans->tr_stmt = FALSE;
            trans_cleanup(cd, trans, rc == DBE_RC_SUCC ? RS_TROP_AFTERCOMMIT : RS_TROP_AFTERROLLBACK);
            *p_finished = TRUE;
            return(rc == DBE_RC_SUCC);
        } else {
            FAKE_CODE_BLOCK_GT(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 2,
            {
                su_ret_t rc;
                tb_connect_t* tc;
                SsPrintf("FAKE_DBE_SEQUENCE_RECOVERY_BUG:tb_trans_commit_user\n");
                FAKE_SET(FAKE_DBE_SEQUENCE_RECOVERY_BUG, 0);
                tc = tb_sysconnect_init(rs_sysi_tabdb(cd));
                rc = tb_createcheckpoint(tc, FALSE);
                tb_sysconnect_done(tc);
                ss_assert(rc == SU_SUCCESS);
            }
            );
            *p_finished = FALSE;
            return(TRUE);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_commit_user_sql
 *
 * Member of the SQL function block.
 * Commits a user transaction.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_commit_user_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        void**      cont,
        rs_err_t**  p_errh )
{
        bool succ;
        bool finished;

        SS_PUSHNAME("tb_trans_commit_user_sql");
        succ = tb_trans_commit_user(cd, trans, &finished, p_errh);

        if (finished){
            *cont = NULL;
        } else {
            *cont = trans;
        }
        SS_POPNAME;
        return(succ);
}

/*##**********************************************************************\
 *
 *              tb_trans_rollback
 *
 * Rolls a transaction back
 *
 * New database transaction is started when the function tb_trans_dbtrx
 * is called for the first time after begin, commit or rollback.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      spname - in, use
 *              if non-NULL, name for the savepoint to which
 *          the rollback is done. If NULL, the transaction
 *          is completely rolled back
 *
 *      p_finished - out, give
 *          TRUE is stored in *p_finished if the operation
 *          terminated (either successfully or in error),
 *          FALSE is stored in *p_finished if the operation did
 *          not complete
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_rollback(cd, trans, spname, p_finished, count_trx, p_errh)
        rs_sysi_t*  cd;
        tb_trans_t* trans;
        char*       spname;
        bool*       p_finished;
        bool        count_trx;
        rs_err_t**  p_errh;
{
        dbe_ret_t rc;
        ss_beta(char buf[80];)

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(p_finished != NULL);

        *p_finished = TRUE; /* for now */

        if (trans->tr_trigcnt) {
            su_err_init(p_errh, E_TRIGILLCOMMITROLLBACK);
            *p_finished = TRUE;
            return(FALSE);
        }

        if (trans->tr_funccnt) {
            su_err_init(p_errh, E_FUNCILLCOMMITROLLBACK);
            *p_finished = TRUE;
            return(FALSE);
        }

        if (trans->tr_allowtrxend_once == 0) {
            if (trans->tb_syncst == TB_TRANS_SYNCST_MASTEREXEC) {
                su_err_init(p_errh, SNC_ERR_ILLCOMMITROLLBACK);
                *p_finished = TRUE;
                return(FALSE);
            }
        }
        trans->tr_allowtrxend_once = 0;

        trans_resetflags(trans);

        if (spname != NULL) {
            rs_error_create(p_errh, E_TRXSPNOSUP);
            return(FALSE);
        }

        if (trans->tr_trx == NULL) {
            /* No dbe transaction is active. */
            return(TRUE);
        }

        dbe_trx_restartif(trans->tr_trx);

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld", DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_rollback:%ld%s\n", (long)trans, buf));

        tb_trans_setdberet(cd, trans, dbe_trx_geterrcode(trans->tr_trx));
        trans_check_errorkey(cd, trans);

        trans_cleanup(cd, trans, RS_TROP_BEFOREROLLBACK);

        rc = dbe_trx_rollback(trans->tr_trx, count_trx, p_errh);
        ss_rc_dassert(rc == DBE_RC_SUCC, rc);

        trans_trxdone(cd, trans, TRUE);

        ss_pprintf_1(("tb_trans_rollback:%ld:rc=%d\n", (long)trans, rc));
        ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[trans rollback%s]\n", rs_sysi_userid(cd), buf));
        if (su_usrid_traceflags != 0) {
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_SQL, 1, (char *)"trans rollback");
        }

        trans->tr_stmt = FALSE;

        trans_cleanup(cd, trans, RS_TROP_AFTERROLLBACK);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              tb_trans_rollback_sql
 *
 * Member of the SQL function block.
 * Rolls a transaction back
 *
 * New database transaction is started when the function tb_trans_dbtrx
 * is called for the first time after begin, commit or rollback.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in out, use
 *              handle to the current transaction
 *
 *      spname - in, use
 *              if non-NULL, name for the savepoint to which
 *          the rollback is done. If NULL, the transaction
 *          is completely rolled back
 *
 *      cont - in/out, give
 *          *cont contains NULL pointer in the first
 *          call. If the operation does not finish, a
 *          non-NULL "continuation" handle is stored into
 *          *cont, and pointer to this value should be
 *          passed with the subsequent calls. NULL is
 *          stored into *cont when the operation eventually
 *          finishes
 *
 *      p_errh - in out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value :
 *
 *      TRUE if the operation is successful
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_rollback_sql(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       spname,
        void**      cont,
        rs_err_t**  p_errh )
{
        bool succ;
        bool finished;

        SS_PUSHNAME("tb_trans_rollback_sql");
        succ = tb_trans_rollback( cd, trans, spname, &finished, TRUE, p_errh );

        if (finished){
            *cont = NULL;
        } else {
            *cont = trans;
        }
        SS_POPNAME;
        return(succ);
}

/*##**********************************************************************\
 *
 *              tb_trans_rollback_onestep
 *
 * Runs transaction rollback in one step.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      p_errh -
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
bool tb_trans_rollback_onestep(
    rs_sysi_t*  cd,
    tb_trans_t* trans,
    bool        count_trx,
    rs_err_t**  p_errh)
{
    bool succp;
    bool finishedp;

        do {
            succp = tb_trans_rollback(cd, trans, NULL, &finishedp,
                                      count_trx, p_errh);
        } while (!finishedp);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_enduncertain
 *
 * Ends uncertain transaction. Transaction is nether committed or rolled
 * back. Trans object should not be used after this call.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              trans -
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
void tb_trans_enduncertain(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        ss_beta(char buf[80];)

        CHK_TRANS(trans);

        trans_resetflags(trans);

        if (trans->tr_trx == NULL) {
            /* No dbe transaction is active. */
            return;
        }

        ss_beta(buf[0] = '\0';)
        ss_debug(SsSprintf(buf, ", trxid=%ld", DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trans->tr_trx))));
        ss_dprintf_1(("tb_trans_enduncertain:%ld%s\n", (long)trans, buf));

        trans_sementer(trans);

        trans->tr_open = FALSE;

        trans_semexit(trans);

        trans_trxdone(cd, trans, FALSE);

        ss_beta(if (ss_debug_sqloutput) SsDbgPrintf("SQL:%d:[trans enduncertain%s]\n", rs_sysi_userid(cd), buf));
        if (su_usrid_traceflags != 0) {
            su_usrid_trace(rs_sysi_userid(cd), SU_USRID_TRACE_SQL, 1, (char *)"trans rollback");
        }
        trans->tr_stmt = FALSE;
}

/*##**********************************************************************\
 *
 *              tb_trans_setautocommit
 *
 * sets autocommit state to transaction. (called only from sse)
 *
 * Parameters :
 *
 *      trans -
 *
 *
 *      autocommitp -
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
void tb_trans_setautocommit(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        bool autocommitp)
{
        CHK_TRANS(trans);
        trans->tr_autocommit = autocommitp;
        rs_sysi_tc_setautocommit(cd, autocommitp);
}

/*##**********************************************************************\
 *
 *              tb_trans_inheritreadlevel
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
 *      readtrans -
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
void tb_trans_inheritreadlevel(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans,
        tb_trans_t* readtrans)
{
        CHK_TRANS(trans);
        ss_dassert(!trans->tr_trigcnt);
        ss_dassert(!trans->tr_funccnt);

        dbe_trx_inheritreadlevel(
            trans->tr_trx,
            readtrans->tr_trx);
}

/*##**********************************************************************\
 *
 *              tb_trans_getreadlevel
 *
 * Returns the read level of current transaction.
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
long tb_trans_getreadlevel(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        long readlevel;

        CHK_TRANS(trans);

        if (!TRANS_ISACTIVE(trans)) {
            readlevel = 0L;
        } else {
            readlevel = dbe_trx_getreadlevel_long(trans->tr_trx);
        }

        return(readlevel);
}

/*##**********************************************************************\
 *
 *              tb_trans_settransoption
 *
 * Sets the transaction option.
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
void tb_trans_settransoption(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        tb_transopt_t option)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        switch (option) {
            case TB_TRANSOPT_CHECKWRITESET:
            case TB_TRANSOPT_CHECKREADSET:
            case TB_TRANSOPT_ISOLATION_DEFAULT:
            case TB_TRANSOPT_ISOLATION_READCOMMITTED:
            case TB_TRANSOPT_ISOLATION_REPEATABLEREAD:
            case TB_TRANSOPT_ISOLATION_SERIALIZABLE:
            case TB_TRANSOPT_NOCHECK:
                rs_sysi_tc_setisolation(cd, option);
                trans->tr_checkmode = option;
                break;
            case TB_TRANSOPT_READONLY:
                rs_sysi_tc_setreadonly(cd, option);
                trans->tr_readonly = TRUE;
                break;
            case TB_TRANSOPT_READWRITE:
                rs_sysi_tc_setreadonly(cd, option);
                trans->tr_readonly = FALSE;
                break;
            case TB_TRANSOPT_NOLOGGING:
                trans->tr_nologging = TRUE;
                break;
            case TB_TRANSOPT_NOINTEGRITY:
                trans->tr_nointegrity = TRUE;
                break;
            case TB_TRANSOPT_NOREFINTEGRITY:
            case TB_TRANSOPT_REFINTEGRITY:
                /* dont check referentian integrity:for nor Flow refrech uses this (Ruka).
                 * Later this can be used in canned transaction and other (bulk) transactions.
                 * Maybe this check feature should be escalated to statement level also...;)
                 */
                trans->tr_norefintegrity = (option == TB_TRANSOPT_NOREFINTEGRITY);
                trans_sementer(trans);
                if (TRANS_ISACTIVE(trans)) {
                    dbe_trx_setrefintegrity_check(trans->tr_trx, !trans->tr_norefintegrity);
                }
                trans_semexit(trans);

                break;
            case TB_TRANSOPT_HSBFLUSH_YES:
            case TB_TRANSOPT_HSBFLUSH_NO:
                trans_sementer(trans);
                if (TRANS_ISACTIVE(trans)) {
                    dbe_trx_sethsbflushallowed(
                        trans->tr_trx,
                        option == TB_TRANSOPT_HSBFLUSH_YES);
                }
                trans_semexit(trans);
                break;

            case TB_TRANSOPT_USEMAXREADLEVEL:
                trans->tr_usemaxreadlevel = TRUE;
                ss_dassert(!TRANS_ISACTIVE(trans));
                break;

            case TB_TRANSOPT_DURABILITY_DEFAULT:
            case TB_TRANSOPT_DURABILITY_RELAXED:
            case TB_TRANSOPT_DURABILITY_STRICT:
                rs_sysi_tc_setdurability(cd, option);
                trans->tr_durability = option;
                break;

            case TB_TRANSOPT_SAFENESS_DEFAULT:
            case TB_TRANSOPT_SAFENESS_1SAFE:
            case TB_TRANSOPT_SAFENESS_2SAFE:
                rs_sysi_tc_setsafeness(cd, option);
                trans->tr_safeness = option;
                break;

            default:
                su_rc_error(option);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_isreadonly
 *
 * Returns TRUE if transaction is read-only.
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
bool tb_trans_isreadonly(tb_trans_t*   trans)
{
        bool succp = FALSE;
        CHK_TRANS(trans);

        if (TRANS_ISACTIVE(trans)) {
            succp = dbe_trx_isreadonly(trans->tr_trx);
        }
        return(trans->tr_readonly || succp);
}

/*##**********************************************************************\
 * 
 *		tb_trans_getisolationname
 * 
 * Returns current transaction isolation level as a string.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		trans - 
 *			
 *			
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* tb_trans_getisolationname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        switch (tb_trans_getisolation(cd, trans)) {
            case TB_TRANSOPT_ISOLATION_READCOMMITTED:
                return((char *)"READ COMMITTED");
            case TB_TRANSOPT_ISOLATION_REPEATABLEREAD:
                return((char *)"REPEATABLE READ");
            case TB_TRANSOPT_ISOLATION_SERIALIZABLE:
                return((char *)"SERIALIZABLE");
            default:
                ss_error;
                return(NULL);
        }
}

char* tb_trans_getdurabilityname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        switch (trans->tr_durability) {
        case TB_TRANSOPT_DURABILITY_DEFAULT:
            return (char *)"DEFAULT";
        case TB_TRANSOPT_DURABILITY_RELAXED:
            return (char *)"RELAXED";
        case TB_TRANSOPT_DURABILITY_STRICT:
            return (char *)"STRICT";
        default:
            ss_error;
        }
        return(NULL);
}

char* tb_trans_getsafenessname(
        rs_sysi_t*      cd,
        tb_trans_t*     trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        switch (trans->tr_safeness) {
        case TB_TRANSOPT_SAFENESS_DEFAULT:
            return (char *)"DEFAULT";
        case TB_TRANSOPT_SAFENESS_1SAFE:
            return (char *)"1SAFE";
        case TB_TRANSOPT_SAFENESS_2SAFE:
            return (char *)"2SAFE";
        default:
            ss_error;
        }
        return(NULL);
}

void tb_trans_setdelayedstmterror(
        rs_sysi_t*      cd __attribute__ ((unused)),
        tb_trans_t*     trans)
{
        ss_dassert(TRANS_ISACTIVE(trans));

        dbe_trx_setdelayedstmterror(trans->tr_trx);
}

/*##**********************************************************************\
 *
 *              tb_trans_settransopt_once
 *
 * Sets a non-permanent transaction option that
 * remains in effect until next commit/rollback
 *
 * Parameters :
 *
 *      cd - notused
 *
 *
 *      trans - use
 *              transaction
 *
 *      option - in
 *              option code
 *
 *      p_errh - out, give
 *              error information
 *
 * Return value :
 *      TRUE - success
 *      FALSE - failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool tb_trans_settransopt_once(
        rs_sysi_t*    cd __attribute__ ((unused)),
        tb_trans_t*   trans,
        tb_transopt_t option,
        su_err_t**    p_errh)
{
        bool succp;

        CHK_TRANS(trans);

        trans_sementer(trans);
        if (TRANS_ISACTIVE(trans)) {
            succp = FALSE;
        } else {
            succp = TRUE;
        }
        trans_semexit(trans);

        if (!succp) {
            su_err_init(p_errh, E_TRANSACTIVE);
            return(FALSE);
        }

        switch (option) {
            case TB_TRANSOPT_CHECKWRITESET:
            case TB_TRANSOPT_CHECKREADSET:
            case TB_TRANSOPT_ISOLATION_DEFAULT:
            case TB_TRANSOPT_ISOLATION_READCOMMITTED:
            case TB_TRANSOPT_ISOLATION_REPEATABLEREAD:
            case TB_TRANSOPT_ISOLATION_SERIALIZABLE:
            case TB_TRANSOPT_NOCHECK:
                trans->tr_checkmode_once = option;
                break;
            case TB_TRANSOPT_USEMAXREADLEVEL:
                ss_rc_derror(option);
                break;
            case TB_TRANSOPT_DURABILITY_DEFAULT:
            case TB_TRANSOPT_DURABILITY_RELAXED:
            case TB_TRANSOPT_DURABILITY_STRICT:
                trans->tr_durability_once = option;
                break;
            case TB_TRANSOPT_NOINTEGRITY:
                trans->tr_nointegrity_once = TRUE;
                break;
            case TB_TRANSOPT_REFINTEGRITY:
            case TB_TRANSOPT_NOREFINTEGRITY:
                /* dont check referentian integrity:for nor Flow refrech uses this (Ruka).
                 * Later this can be used in canned transaction and other (bulk) transactions.
                 * Maybe this check feature should be escalated to statement level also...;)
                 */
                trans->tr_norefintegrity_once = (option == TB_TRANSOPT_NOREFINTEGRITY);
                break;
            case TB_TRANSOPT_READONLY:
                trans->tr_readonly_once = TRUE;
                break;
            case TB_TRANSOPT_READWRITE:
                trans->tr_readonly_once = FALSE;
                break;
            case TB_TRANSOPT_NOLOGGING:
                trans->tr_nologging_once = TRUE;
                break;

            case TB_TRANSOPT_SAFENESS_1SAFE:
            case TB_TRANSOPT_SAFENESS_2SAFE:
                trans->tr_safeness_once = option;
                break;
            default:
                su_rc_error(option);
        }
        return (succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_setfailed
 *
 * Sets the transaction as failed with errcode.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      trans - in, use
 *              handle to the current transaction
 *
 *      errcode - in
 *              error code
 *
 * Return value :
 *
 *      TRUE    - transaction set as failed
 *      FALSE   - transaction is already set as failed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool tb_trans_setfailed(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        int           errcode)
{
        bool succp;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        dbe_db_enteraction(rs_sysi_db(cd), cd);
        trans_sementer_nocheck(trans);
        if (TRANS_ISACTIVE(trans)) {
            succp = dbe_trx_setfailed_nomutex(trans->tr_trx, errcode);
        } else {
            succp = FALSE;
        }
        trans_semexit_nocheck(trans);
        dbe_db_exitaction(rs_sysi_db(cd), cd);

        return(succp);
}

/*##**********************************************************************\
 *
 *              tb_trans_settimeout
 *
 * Sets the transaction as failed for time out reason.
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
bool tb_trans_settimeout(
        rs_sysi_t*         cd,
        tb_trans_t*   trans)
{
        bool succp;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        dbe_db_enteraction(rs_sysi_db(cd), cd);
        trans_sementer_nocheck(trans);
        if (TRANS_ISACTIVE(trans) && !dbe_trx_isfailed(trans->tr_trx)) {
            dbe_trx_settimeout_nomutex(trans->tr_trx);
            ss_dassert(dbe_trx_isfailed(trans->tr_trx));
            succp = TRUE;
        } else {
            succp = FALSE;
        }
        trans_semexit_nocheck(trans);
        dbe_db_exitaction(rs_sysi_db(cd), cd);
        return(succp);
}

#ifndef SS_NODDUPDATE
/*##**********************************************************************\
 *
 *              tb_trans_setrelhchanged
 *
 * Marks the relation handle as changed. When transaction commits, the
 * relation handle should be discarded from the memory.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
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
bool tb_trans_setrelhchanged(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        rs_relh_t*    relh,
        su_err_t**    p_errh)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(TRANS_ISACTIVE(trans));

        if (TRANS_ISACTIVE(trans)) {
            dbe_ret_t rc;
            rc = dbe_trx_setrelhchanged(trans->tr_trx, relh);

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            if (rc == DBE_RC_SUCC) {
                rc = dbe_trx_readrelh(
                        trans->tr_trx, 
                        rs_relh_relid(cd, relh));
            }
#endif
            if (rc != DBE_RC_SUCC) {
                su_err_init(p_errh, E_DDOP);
            }
            return(rc == DBE_RC_SUCC);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_setddop
 *
 * Sets DD operation flag for the current trx. Needed to do in-memory buffer
 * reloads in HSB secondary.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              trans -
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
void tb_trans_setddop(
        rs_sysi_t*    cd,
        tb_trans_t*   trans)
{
        dbe_ret_t rc;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);
        ss_dassert(TRANS_ISACTIVE(trans));

        rc = dbe_trx_setddop(trans->tr_trx);
        ss_dassert(rc == DBE_RC_SUCC);
}

#endif /* SS_NODDUPDATE */

/*##**********************************************************************\
 *
 *              tb_trans_isfailed
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
bool tb_trans_isfailed(
        rs_sysi_t*         cd,
        tb_trans_t*   trans)
{
        bool failedp;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans_sementer(trans);
        if (TRANS_ISACTIVE(trans)) {
            failedp = dbe_trx_isfailed(trans->tr_trx);
        } else {
            failedp = FALSE;
        }
        trans_semexit(trans);
        return(failedp);
}

/*##**********************************************************************\
 *
 *              tb_trans_geterrcode
 *
 *
 *
 * Parameters :
 *
 *      FALSE -
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
su_ret_t tb_trans_geterrcode(
        rs_sysi_t*         cd,
        tb_trans_t*   trans,
        su_err_t**    p_errh)
{
        su_ret_t rc;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans_sementer(trans);
        if (TRANS_ISACTIVE(trans)) {
            rc = dbe_trx_geterrcode(trans->tr_trx);
            if (rc != SU_SUCCESS) {
                su_err_init(p_errh, rc);
            }
        } else {
            rc = SU_SUCCESS;
        }
        trans_semexit(trans);
        return(rc);
}

/*##**********************************************************************\
 *
 *              tb_trans_newcurid
 *
 * Returns an id that is unique inside this transaction. Can be used to
 * generate unique cursor names.
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
long tb_trans_newcurid(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        long curid;

        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        curid = trans->tr_curid++;

        return(curid);
}

/*##**********************************************************************\
 *
 *              tb_trans_allowlogfailure
 *
 * Makes transaction object to allow failure in log writing. This is
 * because certain system information needs to be saved after log write
 * failure.
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
void tb_trans_allowlogfailure(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

#ifndef SS_NOLOGGING
        SS_NOTUSED(cd);
        ss_dassert(trans != NULL);
        trans->tr_allowlogfailure = TRUE;
#endif /* SS_NOLOGGING */
}

/*##**********************************************************************\
 *
 *              tb_trans_getexeccount
 *
 * Returns number of statements started in current transaction. Each
 * statement begin increments the counter.
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
long tb_trans_getexeccount(
        rs_sysi_t* cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        return(trans->tr_execcount);
}

#ifndef SS_NOLOCKING

/*##**********************************************************************\
 *
 *              tb_trans_setlocktimeout
 *
 * Sets pessimistic lock timeout, value -1 uses default timeout.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      timeout -
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
void tb_trans_setlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout)
{
        CHK_TRANS(trans);

        rs_sysi_tc_setlocktimeout(cd, timeout);

        trans_sementer(trans);
        trans->tr_locktimeout = timeout;
        if (TRANS_ISACTIVE(trans)) {
            dbe_trx_setlocktimeout(trans->tr_trx, timeout);
        }
        trans_semexit(trans);
}

/*##**********************************************************************\
 *
 *              tb_trans_getlocktimeout
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
long tb_trans_getlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        long timeout;
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans_sementer(trans);
        if (TRANS_ISACTIVE(trans)) {
            timeout = dbe_trx_getlocktimeout(trans->tr_trx);
        } else {
            timeout = trans->tr_locktimeout;
        }
        trans_semexit(trans);

        return(timeout);
}


/*##**********************************************************************\
 *
 *              tb_trans_lockrelh
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
 *      relh -
 *
 *
 *      exclusive -
 *
 *
 *      timeout -
 *
 *
 *      p_finished -
 *
 *
 *      p_errh -
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
bool tb_trans_lockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        long timeout,
        bool* p_finished,
        su_err_t** p_errh)
{
        dbe_ret_t rc;

        CHK_TRANS(trans);
        /* ss_bassert(trans->tr_trx != NULL); */

        rc = dbe_trx_lockrelh_cd(cd, trans->tr_trx, relh, exclusive, timeout);
        switch (rc) {
            case DBE_RC_SUCC:
                *p_finished = TRUE;
                return(TRUE);
            case DBE_RC_WAITLOCK:
                *p_finished = FALSE;
                return(TRUE);
            default:
                su_err_init(p_errh, rc);
                *p_finished = TRUE;
                return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_lockrelh_long
 *
 * Requests WERY LONG DURATION lock. E.g. lock can exists over a transaction
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      exclusive -
 *
 *
 *      timeout -
 *
 *
 *      p_finished -
 *
 *
 *      p_errh -
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
bool tb_trans_lockrelh_long(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        long timeout,
        bool* p_finished,
        su_err_t** p_errh)
{
        dbe_ret_t rc;

        CHK_TRANS(trans);

        rc = dbe_trx_lockrelh_long(cd, trans->tr_trx, relh, exclusive, timeout);
        switch (rc) {
            case DBE_RC_SUCC:
                *p_finished = TRUE;
                return(TRUE);
            case DBE_RC_WAITLOCK:
                *p_finished = FALSE;
                return(TRUE);
            default:
                su_err_init(p_errh, rc);
                *p_finished = TRUE;
                return(FALSE);
        }
}


/*##**********************************************************************\
 *
 *              tb_trans_unlockrelh
 *
 * Explicit unlock. Used in explicit locking to return back to
 * original state.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
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
void tb_trans_unlockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh)
{
        CHK_TRANS(trans);
        dbe_trx_unlockrelh(cd, trans->tr_trx, relh);
}

/*##**********************************************************************\
 *
 *              tb_trans_getlockrelh
 *
 * Returns table lock information
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      p_isexclusive -
 *
 *
 *      p_isverylong_duration -
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
bool tb_trans_getlockrelh(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool* p_isexclusive,
        bool* p_isverylong_duration)
{
        CHK_TRANS(trans);
        return(dbe_trx_getlockrelh(cd, trans->tr_trx,relh,p_isexclusive, p_isverylong_duration));
}

/*##**********************************************************************\
 *
 *              tb_trans_lockconvert
 *
 * Used in explicit locking to restore / rollback to original state
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      relh -
 *
 *
 *      exclusive -
 *
 *
 *      verylong_duration -
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
bool tb_trans_lockconvert(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        bool exclusive,
        bool verylong_duration)
{
        dbe_ret_t rc;

        CHK_TRANS(trans);
        /* ss_dassert(trans->tr_trx != NULL); */

        rc = dbe_trx_lockrelh_convert(cd, trans->tr_trx, relh, exclusive, verylong_duration);

        ss_rc_dassert(rc == DBE_RC_SUCC, rc);
        ss_dassert(rc != DBE_RC_WAITLOCK);

        return(rc == DBE_RC_SUCC);
}


/*##**********************************************************************\
 *
 *              tb_trans_setoptimisticlocktimeout
 *
 * Sets optimistic lock timeout, value -1 uses default timeout.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      timeout -
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
void tb_trans_setoptimisticlocktimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout)
{
        CHK_TRANS(trans);

        rs_sysi_tc_setoptimisticlocktimeout(cd, timeout);

        trans_sementer(trans);
        trans->tr_optimisticlocktimeout = timeout;
        if (TRANS_ISACTIVE(trans)) {
            dbe_trx_setoptimisticlocktimeout(trans->tr_trx, timeout);
        }
        trans_semexit(trans);
}

long tb_trans_getoptimisticlocktimeout(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        CHK_TRANS(trans);
        return trans->tr_optimisticlocktimeout;
}

#endif /* SS_NOLOCKING */

/*##**********************************************************************\
 *
 *              tb_trans_setidletimeout
 *
 * Sets idle timeout, value 0 uses default timeout.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      timeout -
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
void tb_trans_setidletimeout(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        long        timeout)
{
        CHK_TRANS(trans);

        ss_dprintf_1(("tb_trans_setidletimeout trans:%ld, timeout:%ld\n", (long)trans, (long)timeout));

        rs_sysi_tc_setidletimeout(cd, timeout);
        trans->tr_idletimeout = timeout;
}

/*##**********************************************************************\
 *
 *              tb_trans_getidletimeout
 *
 * gets idle timeout
 *
 * Parameters :
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
long tb_trans_getidletimeout(tb_trans_t* trans)
{
        CHK_TRANS(trans);

        ss_dprintf_1(("tb_trans_getidletimeout trans:%ld, timeout:%ld\n", (long)trans, (long)trans->tr_idletimeout));
        return(trans->tr_idletimeout);
}

/*##**********************************************************************\
 *
 *              tb_trans_isidletimedout
 *
 * checks id transaction idle timeout has fired
 *
 * Parameters :
 *
 *
 *      trans -
 *
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
bool tb_trans_isidletimedout(
        tb_trans_t* trans)
{
        bool isidletimedout = FALSE;

        CHK_TRANS(trans);

        isidletimedout = trans->tr_idletimeoutfired;

        ss_dprintf_1(("tb_trans_isidletimedout trans:%ld\n", (long)trans));
        return(isidletimedout);
}

/*##**********************************************************************\
 *
 *              tb_trans_markidletimedout
 *
 * marks that the transaction idle timeout has fired (called from sse only)
 *
 * Parameters :
 *
 *      trans -
 *
 *
 *      timeout -
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
void tb_trans_markidletimedout(
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        ss_dassert(!trans->tr_idletimeoutfired);
        trans->tr_idletimeoutfired = TRUE;
        ss_dprintf_1(("tb_trans_markidletimeout trans:%ld\n", (long)trans));
}

/*##**********************************************************************\
 *
 *              tb_trans_markidletimedout
 *
 * marks that the transaction idle timeout has fired (called from sse only)
 *
 * Parameters :
 *
 *      trans -
 *
 *
 *      timeout -
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
void tb_trans_clearidletimedout(
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        ss_dassert(trans->tr_idletimeoutfired);
        trans->tr_idletimeoutfired = FALSE;
        ss_dprintf_1(("tb_trans_clearidletimeout trans:%ld\n", (long)trans));
}

/*##**********************************************************************\
 *
 *              tb_trans_getduration_sec
 *
 * Returns duration of current active transaction in seconds. If transaction
 * is not acive returns zero.
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
long tb_trans_getduration_sec(tb_trans_t* trans)
{
        CHK_TRANS(trans);

        if (trans->tr_trx != NULL) {
            return(SsTime(NULL) - trans->tr_starttime);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_settriggerp
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
 *      trigname - in, hold
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
bool tb_trans_settriggerp(
        rs_sysi_t*           cd,
        tb_trans_t*     trans,
        rs_entname_t*   trigname)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        if (trigname != NULL) {
            /* Enter trigger. */
            if (!rs_sysi_trigpush(cd, trigname)) {
                return(FALSE);
            }
            trans->tr_trigcnt++;
            tb_trans_setstmtgroup(cd, trans, TRUE);
        } else {
            /* Exit trigger. */
            ss_bassert(trans->tr_trigcnt > 0);
            trans->tr_trigcnt--;
            rs_sysi_trigpop(cd);
            tb_trans_setstmtgroup(cd, trans, FALSE);
        }
        return(TRUE);
}

void tb_trans_setfunctionp(
        rs_sysi_t*      cd,
        tb_trans_t*     trans,
        rs_entname_t*   funcname)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        if (funcname != NULL) {
            /* Enter function. */
            trans->tr_funccnt++;
            if (trans->tr_usestmtgroup) {
                tb_trans_setstmtgroup(cd, trans, TRUE);
            }
        } else {
            /* Exit function. */
            ss_dassert(trans->tr_funccnt > 0);
            trans->tr_funccnt--;
            if (trans->tr_usestmtgroup) {
                tb_trans_setstmtgroup(cd, trans, FALSE);
            }
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_setstmtgroup
 *
 * Sets statemet grouping on of off. Reference counting is used for nested
 * settings.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trans -
 *
 *
 *      setp -
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
void tb_trans_setstmtgroup(
        rs_sysi_t*      cd __attribute__ ((unused)),
        tb_trans_t*     trans,
        bool            setp)
{
        CHK_TRANS(trans);

        if (setp) {
            trans->tr_stmtgroupcnt++;
        } else {
            ss_dassert(trans->tr_stmtgroupcnt > 0);
            trans->tr_stmtgroupcnt--;
        }
        ss_dprintf_1(("tb_trans_setstmtgroup:%ld:stmtgroupcnt=%d\n", (long)trans, trans->tr_stmtgroupcnt));
}

void tb_trans_usestmtgroup(
        rs_sysi_t*      cd __attribute__ ((unused)),
        tb_trans_t*     trans,
        bool            usestmtgroup)
{
        CHK_TRANS(trans);

        if (usestmtgroup) {
            trans->tr_usestmtgroup++;
        } else {
            trans->tr_usestmtgroup--;
        }
}

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              tb_trans_setsyncstate
 *
 * Sets transaction sync state.
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
void tb_trans_setsyncstate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        tb_trans_syncst_t state)
{
        SS_NOTUSED(cd);
        CHK_TRANS(trans);

        trans->tb_syncst = state;
}

void tb_trans_setsavedstmtflag(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans,
        bool        value)
{
        CHK_TRANS(trans);

        trans->tb_savedstmtflag = value;
}

bool tb_trans_getsavedstmtflag(
        rs_sysi_t*  cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        return(trans->tb_savedstmtflag);
}

#endif /* SS_SYNC */

/*##**********************************************************************\
 *
 *              tb_trans_iscommitactive
 *
 * Checks if commit is active.
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
bool tb_trans_iscommitactive(
        rs_sysi_t* cd,
        tb_trans_t* trans)
{
        CHK_TRANS(trans);

        return (tb_trans_isactive(cd, trans) && trans->tr_commitbeginp);
}

/*##**********************************************************************\
 *
 *              tb_trans_hsbopactive
 *
 * Checks if HSB operation is actyive.
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
bool tb_trans_hsbopactive(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        bool hsbopactivep = FALSE;

        if (trans == NULL) {
            return(FALSE);
        } else {
            CHK_TRANS(trans);

            if (TRANS_ISACTIVE(trans)) {
                hsbopactivep = dbe_trx_ishsbopactive(trans->tr_trx);
            }

            return(hsbopactivep);
        }
}

/*##**********************************************************************\
 *
 *              tb_trans_hsbcommitsent
 *
 * Checks if HSB commit is sent to secondary.
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
bool tb_trans_hsbcommitsent(
        rs_sysi_t* cd __attribute__ ((unused)),
        tb_trans_t* trans)
{
        bool hsbcommitsent = FALSE;

        if (trans == NULL) {
            return(FALSE);
        } else {
            CHK_TRANS(trans);

            if (TRANS_ISACTIVE(trans)) {
                hsbcommitsent = dbe_trx_ishsbcommitsent(trans->tr_trx);
            }

            return(hsbcommitsent);
        }
}
