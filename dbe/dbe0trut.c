/*************************************************************************\
**  source       * dbe0trut.c
**  directory    * dbe
**  description  * Transaction object "utility" routines.
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

#define DBE0TRUT_C

#include <rs0auth.h>

#include "dbe7gtrs.h"
#include "dbe6gobj.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe6log.h"
#include "dbe6bmgr.h"
#include "dbe6lmgr.h"
#include "dbe5ivld.h"
#include "dbe4tupl.h"
#include "dbe4srch.h"
#include "dbe4svld.h"
#include "dbe1trdd.h"
#include "dbe0erro.h"
#include "dbe1seq.h"
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0user.h"

/* Structure for cardinality changes inside the transaction.
 */
typedef struct {
        long        tcr_relid;
        rs_relh_t*  tcr_relh;
        long        tcr_ntuples;
        long        tcr_nbytes;
        long        tcr_nchanges;
} trx_cardin_t;

typedef struct {
        char*       rs_sqlcatalog;
        char*       rs_sqlschema;
        char*       rs_sqlstr;
        dbe_trxid_t rs_stmttrxid;
} trx_repsql_t;

/*##**********************************************************************\
 *
 *              dbe_trx_addstmttotrxbuf
 *
 * Adds statement to trxbuf if not yet added and not read-only trx.
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
void dbe_trx_addstmttotrxbuf(dbe_trx_t* trx)
{
        if (!trx->trx_stmtaddedtotrxbuf
        &&  trx->trx_mode != TRX_NOWRITES
        &&  trx->trx_mode != TRX_READONLY
        &&  !DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid))
        {
            if (!trx->trx_trxaddedtotrxbuf) {
                trx->trx_trxaddedtotrxbuf = TRUE;
                dbe_trxbuf_add(trx->trx_trxbuf, trx->trx_info);
            }
            trx->trx_stmtaddedtotrxbuf = TRUE;
#ifdef SS_HSBG2
            if (trx->trx_mode != TRX_NOCHECK)
#endif
            {
                dbe_trxbuf_addstmt(
                    trx->trx_trxbuf,
                    trx->trx_stmttrxid,
                    trx->trx_info);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_puthsbmarkstolog
 *
 * Put trx and stmt begin marks to log file.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      remotetrxid -
 *
 *
 *      remotestmttrxid -
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
dbe_ret_t dbe_trx_puthsbmarkstolog(
        dbe_trx_t* trx,
        dbe_trxid_t remotetrxid __attribute__ ((unused)),
        dbe_trxid_t remotestmttrxid __attribute__ ((unused)),
        bool isdummy __attribute__ ((unused)))
{
#ifdef SS_HSBG2
        dbe_ret_t rc;

        CHK_TRX(trx);

        if (trx->trx_log == NULL) {
            return(DBE_RC_SUCC);
        }

        if (!trx->trx_hsbtrxmarkwrittentolog) {
            rc = dbe_log_putreplicatrxstart(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_REPLICATRXSTART,
                        trx->trx_usertrxid,
                        DBE_TRXID_NULL);
            if (rc != SU_SUCCESS) {
                return(rc);
            }
            trx->trx_hsbtrxmarkwrittentolog = TRUE;
            trx->trx_nlogwrites++;
        }
        if (!trx->trx_hsbstmtmarkwrittentolog
        &&  !DBE_TRXID_EQUAL(trx->trx_usertrxid, trx->trx_stmttrxid))
        {
            /* Put hsb stmt mark to log. Note that usertrxid and stmttrxid
             * can be equal in some sequence operations.
             */
            rc = dbe_log_putreplicastmtstart(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_REPLICASTMTSTART,
                        trx->trx_usertrxid,
                        DBE_TRXID_NULL,
                        trx->trx_stmttrxid);
            if (rc != SU_SUCCESS) {
                return(rc);
            }
            trx->trx_hsbstmtmarkwrittentolog = TRUE;
            trx->trx_stmtiswrites = TRUE;
            trx->trx_nlogwrites++;
        }
        return(DBE_RC_SUCC);
#else /* SS_HSBG2 */
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(dbe_db_gethsbmode(trx->trx_db) == DBE_HSB_PRIMARY ||
                   trx->trx_replicaslave);

        if (trx->trx_log == NULL || !dbe_db_ishsb(trx->trx_db)) {
            return(DBE_RC_SUCC);
        }

        if (!trx->trx_hsbtrxmarkwrittentolog) {
            rc = dbe_log_putreplicatrxstart(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_REPLICATRXSTART,
                        trx->trx_usertrxid,
                        remotetrxid);
            if (rc != SU_SUCCESS) {
                return(rc);
            }
            trx->trx_hsbtrxmarkwrittentolog = TRUE;
        }
        ss_dassert(trx->trx_hsbstmtmarkwrittentolog == FALSE
                || DBE_TRXID_EQUAL(trx->trx_hsbstmtlastwrittentolog,
                                   trx->trx_stmttrxid));
        if (!trx->trx_hsbstmtmarkwrittentolog
        &&  (isdummy || !DBE_TRXID_EQUAL(remotestmttrxid, DBE_TRXID_NULL)))
        {
            rc = dbe_log_putreplicastmtstart(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_REPLICASTMTSTART,
                        trx->trx_usertrxid,
                        remotestmttrxid,
                        trx->trx_stmttrxid);
            if (rc != SU_SUCCESS) {
                return(rc);
            }
            trx->trx_hsbstmtmarkwrittentolog = TRUE;
            trx->trx_stmtiswrites = TRUE;
            ss_debug(trx->trx_hsbstmtlastwrittentolog = trx->trx_stmttrxid);
        }
        return(DBE_RC_SUCC);
#endif  /* SS_HSBG2 */
}

/*##**********************************************************************\
 *
 *              dbe_trx_markwrite_nolog
 *
 *
 *
 * Parameters :
 *
 *              trx -
 *
 *
 *              stmtoper -
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
dbe_ret_t dbe_trx_markwrite_nolog(dbe_trx_t* trx, bool stmtoper)
{
        return dbe_trx_markwrite_local(trx, stmtoper, FALSE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_markwrite_enteraction
 *
 * Marks the transaction as a write transaction. The current mode is
 * changed. Also enters and exits db action gate so the caller does
 * not have to do it.
 *
 * Parameters :
 *
 *              trx -
 *
 *
 *              stmtoper -
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
dbe_ret_t dbe_trx_markwrite_enteraction(dbe_trx_t* trx, bool stmtoper)
{
        dbe_ret_t rc;

        ss_dassert(SsSemStkNotFound(SS_SEMNUM_DBE_DB_ACTIONGATE));

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite_local(trx, stmtoper, TRUE);

        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_markwrite_local
 *
 *
 *
 * Parameters :
 *
 *              trx -
 *
 *
 *              stmtoper -
 *
 *
 *              puttolog -
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
dbe_ret_t dbe_trx_markwrite_local(dbe_trx_t* trx, bool stmtoper, bool puttolog)
{
        dbe_ret_t rc;
        bool addactivetrx = FALSE;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_markwrite_local, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        dbe_trx_ensureusertrxid(trx);

#ifdef SS_HSBG2

        FAKE_CODE_RESET(FAKE_HSBG2_DBOPFAIL_D,
        {
            dbe_trx_sementer(trx);
            ss_dprintf_1(("FAKE_HSBG2_DBOPFAIL_D\n"));

            dbe_trx_setfailurecode_nomutex(trx, DBE_ERR_HSBABORTED);
            dbe_trx_semexit(trx);
        });

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

#endif

        dbe_trx_sementer(trx);

        if (trx->trx_mode == TRX_NOWRITES) {
            if (!dbe_db_setchanged(trx->trx_db, NULL)) {
                trx->trx_errcode = DBE_ERR_DBREADONLY;
                dbe_trx_semexit(trx);
                return(DBE_ERR_DBREADONLY);
            }
#ifdef SS_HSBG2
            if (stmtoper)
#endif
            {
                trx->trx_mode = trx->trx_defaultwritemode;
                ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);
            }
        }
        if (trx->trx_mode == TRX_READONLY) {
            if (dbe_db_isreadonly(trx->trx_db)) {
                trx->trx_errcode = DBE_ERR_DBREADONLY;
                dbe_trx_semexit(trx);
                return(DBE_ERR_DBREADONLY);
            } else {
                trx->trx_errcode = DBE_ERR_TRXREADONLY;
                dbe_trx_semexit(trx);
                return(DBE_ERR_TRXREADONLY);
            }
        }
        if (trx->trx_log == NULL && !trx->trx_stoplogging) {
            trx->trx_log = dbe_db_getlog(trx->trx_db);
            if (trx->trx_trdd != NULL) {
                dbe_trdd_setlog(trx->trx_trdd, trx->trx_log);
            }
        }

        /* trx can not start writing in secondary role */
        if (trx->trx_hsbg2mode == DBE_HSB_PRIMARY) {
            dbe_hsbmode_t curhsbg2mode;

            curhsbg2mode = dbe_db_gethsbg2mode(trx->trx_db);

            if ((curhsbg2mode == DBE_HSB_SECONDARY ||
                 curhsbg2mode == DBE_HSB_PRIMARY_UNCERTAIN)
            &&  puttolog
            &&  trx->trx_log != NULL
            &&  stmtoper)
            {
                trx->trx_errcode = DBE_ERR_TRXREADONLY;
                dbe_trx_semexit(trx);
                return(DBE_ERR_TRXREADONLY);
            }
        }

        rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);

        /* Add statement to trxbuf. Otherwise it is not written
         * to disk in checkpoint and HSB secondary can not restart
         * such statements after netcopy if the statement is not yet
         * committed.
         * TPR #450565
         * JarmoR Oct 8, 2001
         */
        if (!trx->trx_stmtaddedtotrxbuf) {
            dbe_trx_addstmttotrxbuf(trx);
        }

#ifdef SS_HSBG2
        if (puttolog && !trx->trx_hsbstmtmarkwrittentolog && stmtoper) {
            if (!trx->trx_hsbtrxmarkwrittentolog && trx->trx_hsbg2mode != DBE_HSB_STANDALONE) {
                addactivetrx = TRUE;
            }
            rc = dbe_trx_puthsbmarkstolog(
                    trx,
                    DBE_TRXID_NULL,
                    DBE_TRXID_NULL,
                    TRUE);

            /*
             * dbe_trx_puthsbmarkstolog can fail when we are in uncertain
             * mode => set the transaction as failed
             *
             */
            if (rc != DBE_RC_SUCC) {
                dbe_trx_setfailed_nomutex(trx, rc);
            }
        } else {
            rc = DBE_RC_SUCC;
        }
#else /* SS_HSBG2 */
        if (puttolog
            && !trx->trx_hsbstmtmarkwrittentolog
            && dbe_db_gethsbmode(trx->trx_db) == DBE_HSB_PRIMARY) {
            rc = dbe_trx_puthsbmarkstolog(
                    trx,
                    DBE_TRXID_NULL,
                    DBE_TRXID_NULL,
                    TRUE);

        } else {
            rc = DBE_RC_SUCC;
        }
#endif /* SS_HSBG2 */

        if (puttolog) {
#ifdef SS_HSBG2
            if (rc == DBE_RC_SUCC && stmtoper) {
#else /* SS_HSBG2 */
            if (rc == DBE_RC_SUCC) {
#endif /* SS_HSBG2 */
                trx->trx_stmtiswrites = TRUE;
            }
        }

        if (rc == DBE_RC_SUCC && addactivetrx) {
            dbe_gtrs_setactivetrx(trx->trx_gtrs, trx->trx_info, trx);
        }

        dbe_trx_semexit(trx);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setddop
 *
 * Sets DD operation flag for the current trx. Needed to do in-memory buffer
 * reloads in HSB secondary.
 *
 * Parameters :
 *
 *              trx -
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
dbe_ret_t dbe_trx_setddop(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        trx->trx_isddop = TRUE;
        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_trx_markreplicate
 *
 * Marks the transaction as a replicated transaction.
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
dbe_ret_t dbe_trx_markreplicate(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        trx->trx_replication = TRUE;
        return(DBE_RC_SUCC);
}

/*#***********************************************************************\
 *
 *              trx_resetnonrepeatableread
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
static void trx_resetnonrepeatableread(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_nonrepeatableread) {
            trx->trx_nonrepeatableread = FALSE;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setcheckwriteset
 *
 * Sets the read-write validation strategy for the transaction as a
 * write set validation. Only write operations are checked in the
 * transaction. The default validation stategy is to validate both
 * read and write operations. The default validation strategy
 * produces serializable transactions. The write set validation
 * notices lost update conflicts, but the transactions may not
 * be serializable. The write set validation is more efficient
 * especially for interactive transactions where there is a lot
 * of read operations that are not meaningful for the transaction
 * serialization.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      TRUE    - transaction is set to check only write set
 *      FALSE   - transaction is set to check only write set but there
 *                already is changes in the transaction, so part of the
 *                read set is also checked at commit
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_setcheckwriteset(
        dbe_trx_t* trx)
{
        bool succp;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setcheckwriteset, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        succp = (trx->trx_mode != TRX_CHECKREADS);

        if (succp) {
            trx->trx_defaultwritemode = TRX_CHECKWRITES;
            trx_resetnonrepeatableread(trx);
            ss_autotest_or_debug(trx->trx_info->ti_isolation = RS_SQLI_ISOLATION_REPEATABLEREAD);
        }

        dbe_trx_semexit(trx);

        return(succp);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setcheckreadset
 *
 * Sets read set validation a default. Early validate must used with read
 * set validate.
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
bool dbe_trx_setcheckreadset(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setcheckreadset, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (dbe_db_isreadonly(trx->trx_db) || !trx->trx_earlyvld) {
            return(FALSE);
        }

        dbe_trx_sementer(trx);

        trx->trx_defaultwritemode = TRX_CHECKREADS;
        trx_resetnonrepeatableread(trx);

        ss_autotest_or_debug(trx->trx_info->ti_isolation = RS_SQLI_ISOLATION_SERIALIZABLE);

        dbe_trx_semexit(trx);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_mapisolation
 *
 * Maps isolation level to internal mode.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      isolation -
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
trx_mode_t dbe_trx_mapisolation(
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_sqli_isolation_t isolation)
{
        switch (isolation) {

            case RS_SQLI_ISOLATION_READCOMMITTED:
                return(TRX_NONREPEATABLEREAD);

            case RS_SQLI_ISOLATION_REPEATABLEREAD:
                return(TRX_CHECKWRITES);

            case RS_SQLI_ISOLATION_SERIALIZABLE:
                return(TRX_CHECKREADS);

            default:
                ss_error;
                return(TRX_CHECKWRITES);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getwritemode
 *
 * Returns transaction write mode. One of trx_mode_t.
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
int dbe_trx_getwritemode(
        dbe_trx_t* trx)
{
        if (trx->trx_nonrepeatableread) {
            return(TRX_NONREPEATABLEREAD);
        }
        if (trx->trx_mode == TRX_NOWRITES) {
            return(trx->trx_defaultwritemode);
        } else {
            return(trx->trx_mode);
        }
}

/*#***********************************************************************\
 *
 *              trx_setnonrepeatableread
 *
 * Sets the transaction to a non-repeatable read mode. Each new search
 * gets the latest read level.
 *
 * This mode implies write check mode.
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
static bool trx_setnonrepeatableread(
        dbe_trx_t* trx)
{
        bool succp;

        CHK_TRX(trx);
        ss_dprintf_3(("trx_setnonrepeatableread, userid = %d\n", dbe_user_getid(trx->trx_user)));

        succp = dbe_trx_setcheckwriteset(trx);
        dbe_trx_sementer(trx);
        if (succp) {
            trx->trx_nonrepeatableread = TRUE;
            ss_autotest_or_debug(trx->trx_info->ti_isolation = RS_SQLI_ISOLATION_READCOMMITTED);
        }
        dbe_trx_semexit(trx);
        return(succp);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setisolation
 *
 * Sets transaction isolation level. In practise maps it to internal
 * modes.
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
bool dbe_trx_setisolation(
        dbe_trx_t* trx,
        rs_sqli_isolation_t isolation)
{
        bool succp;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setisolation, userid = %d, isolation = %d\n", dbe_user_getid(trx->trx_user), isolation));

        switch (dbe_trx_mapisolation(trx, isolation)) {
            case TRX_NONREPEATABLEREAD:
                succp = trx_setnonrepeatableread(trx);
                break;
            case TRX_CHECKWRITES:
                succp = dbe_trx_setcheckwriteset(trx);
                break;
            case TRX_CHECKREADS:
                succp = dbe_trx_setcheckreadset(trx);
                break;
            default:
                ss_error;
                succp = FALSE;
        }
        return(succp);
}

/*##**********************************************************************\
 * 
 *      dbe_trx_getisolation
 * 
 * Returns current isolation level.
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
rs_sqli_isolation_t dbe_trx_getisolation(
        dbe_trx_t* trx)
{
        rs_sqli_isolation_t isolation;

        CHK_TRX(trx);

        if (trx->trx_nonrepeatableread) {
            isolation = RS_SQLI_ISOLATION_READCOMMITTED;
        } else {
            switch (trx->trx_defaultwritemode) {
                case TRX_CHECKWRITES:
                    isolation = RS_SQLI_ISOLATION_REPEATABLEREAD;
                    break;
                case TRX_CHECKREADS:
                    isolation = RS_SQLI_ISOLATION_SERIALIZABLE;
                    break;
                default:
                    ss_derror;
                    isolation = RS_SQLI_ISOLATION_REPEATABLEREAD;
                    break;
            }

        }
        ss_dprintf_1(("dbe_trx_getisolation, userid = %d, isolation = %d\n", dbe_user_getid(trx->trx_user), isolation));

        return(isolation);
}

/*##**********************************************************************\
 * 
 *      dbe_trx_signalisolationchange
 * 
 * Signals isolation change to a transaction. Current behaviour is to
 * invalidate all open cursors.
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
void dbe_trx_signalisolationchange(dbe_trx_t* trx)
{
        ss_dprintf_1(("dbe_trx_signalisolationchange, userid = %d\n", dbe_user_getid(trx->trx_user)));
        
        dbe_user_invalidatesearches(trx->trx_user, trx->trx_usertrxid, DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_ishsbopactive
 *
 * Checks is HSB operation is active.
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
bool dbe_trx_ishsbopactive(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_rp.rp_activep || trx->trx_hsbcommitsent);
}

/*##**********************************************************************\
 *
 *              dbe_trx_ishsbopactive
 *
 * Checks if HSB commit is sent to the secondary.
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
bool dbe_trx_ishsbcommitsent(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_hsbcommitsent);
}

bool dbe_trx_hsbenduncertain(
        dbe_trx_t* trx,
        dbe_trxid_t trxid,
        su_ret_t rc)
{
        CHK_TRX(trx);

        dbe_trx_sementer(trx);

        if (!trx->trx_hsbcommitsent || trx->trx_commitst == TRX_COMMITST_DONE) {
            dbe_trx_semexit(trx);
            return(FALSE);
        }


        if (!DBE_TRXID_ISNULL(trxid)
        &&  !DBE_TRXID_EQUAL(trxid, trx->trx_usertrxid))
        {
            dbe_trx_semexit(trx);
            return(FALSE);
        }

        trx->trx_hsbcommitsent = FALSE;
        trx->trx_log = NULL;
        if (rc != SU_SUCCESS) {
            dbe_trx_setfailed_nomutex(trx, rc);
        }

        dbe_trx_semexit(trx);

        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_geterrcode
 *
 * Returns the possible current error code in transaction.
 *
 * Parameters :
 *
 *      trx - in
 *              Transaction handle.
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or error code
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trx_geterrcode(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_geterrcode, userid = %d\n", dbe_user_getid(trx->trx_user)));

        return(trx->trx_errcode);
}

/*##**********************************************************************\
 *
 *              dbe_trx_abort_local
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      enteractionp -
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
void dbe_trx_abort_local(
        dbe_trx_t* trx,
        bool enteractionp)
{
        CHK_TRX(trx);
        ss_dprintf_3(("dbe_trx_abort_local\n"));
        ss_dassert(!trx->trx_rollbackdone);
        ss_dassert(trx->trx_errcode != DBE_RC_SUCC);
        ss_dassert(enteractionp || dbe_trx_semisentered(trx));

        dbe_trx_localrollback(trx, enteractionp, TRUE, NULL);
}

/*#***********************************************************************\
 *
 *              trx_setaborted
 *
 * Sets the transaction as aborted. The transaction is rolled back and
 * marked as failed, but the transaction handle is still valid.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void trx_setaborted(
        dbe_trx_t* trx,
        dbe_ret_t rc,
        bool enteractionp)
{
        CHK_TRX(trx);
        ss_dprintf_3(("trx_setaborted, rc = %s (%d)\n", su_rc_nameof(rc), rc));
        ss_dassert(enteractionp || dbe_trx_semisentered(trx));

        if (trx->trx_errcode == DBE_RC_SUCC && !trx->trx_hsbcommitsent) {
            /* Not yet failed, set the failure code. */
            ss_dassert(!trx->trx_rollbackdone);
            trx->trx_errcode = rc;
            dbe_trx_abort_local(trx, enteractionp);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setfailed_nomutex
 *
 * Sets the transaction as failed. The reason for the failure is
 * given in parameter rc. Does not enter mutex.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 *      rc - in
 *              Failure code.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_setfailed_nomutex(
        dbe_trx_t* trx,
        dbe_ret_t rc)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setfailed_nomutex:rc=%s (%d), userid = %d (errcode %d, rp_active %d, hsbcommitsent %d, commitstate %d)\n", 
                       su_rc_nameof(rc), rc, 
                       dbe_user_getid(trx->trx_user),
                       trx->trx_errcode,
                       trx->trx_rp.rp_activep,
                       trx->trx_hsbcommitsent,
                       trx->trx_commitst));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_errcode == DBE_RC_SUCC
        &&  !trx->trx_rp.rp_activep
        &&  !trx->trx_hsbcommitsent
        &&  trx->trx_commitst != TRX_COMMITST_DONE)
        {
            /* Not yet failed, set the failure code. */
            ss_dassert(!trx->trx_rollbackdone);
            switch (rc) {
                case DBE_ERR_HSBABORTED:
                case DBE_ERR_HSBSECONDARY:
                    /* We need to do physical abort with some HSB errors
                       because otherwise some resources like new table name
                       or dropped table remain locked.
                       See e.g. TPR #30472.
                       JarmoR Aug 8, 2001.
                    */
                    trx_setaborted(trx, rc, FALSE);
                    break;
                default:
                    trx->trx_errcode = rc;
                    break;
            }
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setfailed
 *
 * Sets the transaction as failed. The reason for the failure is
 * given in parameter rc.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 *      rc - in
 *              Failure code.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_setfailed(
        dbe_trx_t* trx,
        dbe_ret_t rc,
        bool entercaction)
{
        bool retp;
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setfailed:rc=%s (%d), userid = %d\n", su_rc_nameof(rc), rc, dbe_user_getid(trx->trx_user)));

        if (entercaction) {
            dbe_db_enteraction(trx->trx_db, trx->trx_cd);
        }

        dbe_trx_sementer(trx);

        retp = dbe_trx_setfailed_nomutex(trx, rc);

        dbe_trx_semexit(trx);
        if (entercaction) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        }

        return (retp);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setfailurecode_nomutex
 *
 * Sets transcation failed with error code but do not do physical rollback
 * in any case like dbe_trx_setfailed does.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      rc -
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
bool dbe_trx_setfailurecode_nomutex(
        dbe_trx_t* trx,
        dbe_ret_t rc)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setfailurecode_nomutex:rc=%s (%d), userid = %d\n", su_rc_nameof(rc), rc, dbe_user_getid(trx->trx_user)));
        ss_dassert(dbe_trx_semisentered(trx));

        if (trx->trx_errcode == DBE_RC_SUCC) {
            /* Not yet failed, set the failure code. */
            ss_dassert(!trx->trx_rollbackdone);
            trx->trx_errcode = rc;
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setfailurecode
 *
 * Sets transcation failed with error code but do not do physical rollback
 * in any case like dbe_trx_setfailed does.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      rc -
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
bool dbe_trx_setfailurecode(
        dbe_trx_t* trx,
        dbe_ret_t rc)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setfailurecode:rc=%s (%d), userid = %d\n", su_rc_nameof(rc), rc, dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        b = dbe_trx_setfailurecode_nomutex(trx, rc);

        dbe_trx_semexit(trx);

        return(b);
}

/*##**********************************************************************\
 *
 *              dbe_trx_settimeout_nomutex
 *
 * Sets the transaction as failed for time out reason. The transaction
 * is rolled back and mark as failed, but the transaction handle is
 * still valid.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_trx_settimeout_nomutex(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_settimeout_nomutex, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));
        ss_dassert(dbe_trx_semisentered(trx));

        trx_setaborted(trx, DBE_ERR_TRXTIMEOUT, FALSE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setdeadlock_noenteraction
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
void dbe_trx_setdeadlock_noenteraction(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setdeadlock_noenteraction, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));

        dbe_trx_sementer(trx);
        trx_setaborted(trx, DBE_ERR_DEADLOCK, FALSE);
        dbe_trx_semexit(trx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setdeadlock
 *
 * Sets the transaction as failed for deadlock reason. The transaction
 * is rolled back and mark as failed, but the transaction handle is
 * still valid.
 *
 * Parameters :
 *
 *      trx - use
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
void dbe_trx_setdeadlock(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setdeadlock, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        dbe_trx_setdeadlock_noenteraction(trx);

        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setreadonly
 *
 * Sets the transaction a read-only.
 *
 * Parameters :
 *
 *      trx - in out, use
 *              Transaction handle.
 *
 * Return value :
 *
 *      TRUE    - transaction is set as read-only
 *      FALSE   - failed to set the transaction as read-only, there
 *                already is changes in the transaction
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_setreadonly(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setreadonly, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        if (trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY) {
            trx->trx_mode = TRX_READONLY;
            ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);
            dbe_trx_semexit(trx);
            return(TRUE);
        } else {
            dbe_trx_semexit(trx);
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setforcecommit
 *
 * Sets transaction to committed mode. Used in some system transactions
 * with HSB G2 where default trx mode in secondary is read-only.
 *
 * This mode is similar to nocheck mode but index writes are done to
 * Bonsai-tree.
 *
 * Parameters :
 *
 *              trx - in, use
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
void dbe_trx_setforcecommit(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setforcecommit, userid = %d\n", dbe_user_getid(trx->trx_user)));

        trx->trx_forcecommit = TRUE;
        trx->trx_info->ti_committrxnum = DBE_TRXNUM_SUM(dbe_counter_getmergetrxnum(trx->trx_counter), -1);
}

/*##**********************************************************************\
 *
 *              dbe_trx_forcecommit
 *
 * Returns forcecommit flag value.
 *
 * Parameters :
 *
 *              trx - in
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
bool dbe_trx_forcecommit(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_forcecommit);
}

/*##**********************************************************************\
 *
 *              dbe_trx_iswrites
 *
 * Checks if there are any write operations in a transaction.
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
bool dbe_trx_iswrites(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_mode != TRX_NOWRITES && trx->trx_mode != TRX_READONLY);
}

/*##**********************************************************************\
 *
 *              dbe_trx_isstmtgroupactive
 *
 * Checks if statement group is active.
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
bool dbe_trx_isstmtgroupactive(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_groupstmtlist != NULL);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setnocheck
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
bool dbe_trx_setnocheck(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setnocheck, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(!rs_sysi_testflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY));

#ifdef SS_HSBG2
        trx->trx_trxaddedtotrxbuf = TRUE;
#endif
        trx->trx_mode = TRX_NOCHECK;
        ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setflush
 *
 * Sets transaction's flush mode. Commit does not cause log file flush in
 * no flush mode.
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
void dbe_trx_setflush(
        dbe_trx_t* trx,
        bool flushp)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setflush:userid = %d, flushp = %d\n", dbe_user_getid(trx->trx_user), flushp));

        if (flushp) {
            trx->trx_flush_policy = TRX_FLUSH_YES;
            /*
             * If flushp == TRUE and safeness is adaptive --> 2-safe
             */
            if (!trx->trx_usersafeness && dbe_db_hsbg2safenesslevel_adaptive(trx->trx_db)) {
                trx->trx_is2safe = TRUE;
            }
        } else {
            trx->trx_flush_policy = TRX_FLUSH_NO;

            /*
             * If flushp == FALSE and safeness is adaptive --> 1-safe
             */
            if (!trx->trx_usersafeness && dbe_db_hsbg2safenesslevel_adaptive(trx->trx_db)) {
                trx->trx_is2safe = FALSE;
            }

        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setflush
 *
 * Sets transaction's safeness level for HSB.
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
void dbe_trx_set2safe_user(
        dbe_trx_t* trx,
        bool is2safe)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_set2safe_user:userid = %d, is2safe = %d\n", dbe_user_getid(trx->trx_user), is2safe));

        trx->trx_usersafeness = TRUE;
        trx->trx_is2safe = is2safe;
}


/*##**********************************************************************\
 *
 *              dbe_trx_setnointegrity
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
bool dbe_trx_setnointegrity(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setnointegrity, userid = %d\n", dbe_user_getid(trx->trx_user)));

        trx->trx_nointegrity = TRUE;
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setnorefintegrity
 *
 * Does this transaction check referential integrity or not.
 * All other integrity checks are not affected.
 * f.x. unique errors.
 *
 * When doing trx-object optimisation: this and some other 'flags' can be
 * in one 'flags' bitmap integer.
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
bool dbe_trx_setrefintegrity_check(
        dbe_trx_t* trx,
        bool refintegrity)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setnorefintegrity, userid = %d, integrity %d\n",
                        dbe_user_getid(trx->trx_user), refintegrity));

        trx->trx_norefintegrity = !refintegrity;
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_trx_check_refintegrity
 *
 * Returns TRUE if foreignkey checks should be done.
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
bool dbe_trx_check_refintegrity(
        dbe_trx_t* trx)
{
        bool bres;

        CHK_TRX(trx);
        
        bres = !trx->trx_norefintegrity && !trx->trx_nointegrity;
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        bres = bres && trx->trx_foreign_key_checks;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        
        
        return bres;
}



/*##**********************************************************************\
 *
 *              dbe_trx_inheritreadlevel
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      readtrx -
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
void dbe_trx_inheritreadlevel(
        dbe_trx_t* trx,
        dbe_trx_t* readtrx)
{
        CHK_TRX(trx);
        CHK_TRX(readtrx);
        ss_dprintf_1(("dbe_trx_inheritreadlevel, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_ensurereadlevel(readtrx, TRUE);

        if (readtrx->trx_errcode != DBE_RC_SUCC) {
            return;
        }

        dbe_trx_sementer(trx);

        ss_dassert(!DBE_TRXNUM_ISNULL(readtrx->trx_info->ti_maxtrxnum));

        if (DBE_TRXNUM_CMP_EX(trx->trx_info->ti_maxtrxnum, readtrx->trx_info->ti_maxtrxnum) > 0) {
            trx->trx_info->ti_maxtrxnum = readtrx->trx_info->ti_maxtrxnum;
        }

        trx->trx_tmpmaxtrxnum = readtrx->trx_info->ti_maxtrxnum;

        dbe_trx_semexit(trx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getusertrxid_aval
 *
 * Returns the user transaction id in aval.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *
 *      atype -
 *
 *
 *      aval -
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
void dbe_trx_getusertrxid_aval(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        ss_uint4_t low4bytes;
        ss_uint4_t high4bytes;
        rs_tuplenum_t trxid8byte;

        ss_dassert(cd == trx->trx_cd);

        dbe_counter_get8bytetrxid(
                trx->trx_counter,
                trx->trx_usertrxid,
                &low4bytes,
                &high4bytes);

        rs_tuplenum_ulonginit(&trxid8byte, high4bytes, low4bytes);

        if (!rs_tuplenum_setintoaval(&trxid8byte, cd, atype, aval)) {
            ss_derror;
        }
}

void dbe_trx_setreadtrxid(
        dbe_trx_t* trx,
        dbe_trxid_t readtrxid)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setreadtrxid:readtrxid=%ld\n", DBE_TRXID_GETLONG(readtrxid)));

        trx->trx_readtrxid = readtrxid;

        ss_dassert(DBE_TRXID_CMP_EX(trx->trx_usertrxid, trx->trx_readtrxid) <= 0);
}

/*##**********************************************************************\
 * 
 *      dbe_trx_getreadlevel_long
 * 
 * Returns current read level as long integer.
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
long dbe_trx_getreadlevel_long(
        dbe_trx_t* trx)
{
        long readlevel;
        dbe_trxnum_t trxnum;

        if (trx == NULL) {
            return(0);
        }

        CHK_TRX(trx);

        if (SU_BFLAG_TEST(trx->trx_flags, TRX_FLAG_USEREADLEVEL)) {
            if (!DBE_TRXNUM_EQUAL(trx->trx_tmpmaxtrxnum, DBE_TRXNUM_NULL)) {
                trxnum = trx->trx_tmpmaxtrxnum;
            } else {
                dbe_trxinfo_t* trxinfo;
                trxinfo = trx->trx_info;
                if (trxinfo == NULL) {
                    trxnum = dbe_trxnum_init(0);
                } else {
                    trxnum = trxinfo->ti_maxtrxnum;
                }
            }
            readlevel = DBE_TRXNUM_GETLONG(trxnum);
        } else {
            ss_dassert(DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum) || trx->trx_usemaxreadlevel);
            readlevel = 0;
        }

        return(readlevel);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getsearchtrxnum
 *
 * Returns the transaction number for a search. This is the search
 * read level.
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
dbe_trxnum_t dbe_trx_getsearchtrxnum(
        dbe_trx_t* trx)
{
        dbe_trxnum_t searchtrxnum;

        CHK_TRX(trx);

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(DBE_TRXNUM_NULL);
        }

        if (trx->trx_nonrepeatableread) {
            if (DBE_TRXNUM_ISNULL(trx->trx_searchtrxnum)) {
                trx->trx_searchtrxnum = dbe_counter_getmaxtrxnum(trx->trx_counter);
                trx->trx_stmtsearchtrxnum = trx->trx_searchtrxnum;
                {
                    dbe_trxnum_t storagetrxnum;
                    storagetrxnum = dbe_counter_getstoragetrxnum(trx->trx_counter);
                    if (DBE_TRXNUM_CMP_EX(storagetrxnum, trx->trx_searchtrxnum) < 0) {
                        ss_dprintf_3(("dbe_trx_getsearchtrxnum:clear RS_SYSI_FLAG_STORAGETREEONLY\n"));
                        rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
                    }
                }
            }
            searchtrxnum = trx->trx_searchtrxnum;
        } else {
            searchtrxnum = dbe_trx_getmaxtrxnum(trx);
        }

        return(searchtrxnum);
}

dbe_trxnum_t dbe_trx_getstmtsearchtrxnum(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        if (DBE_TRXNUM_ISNULL(trx->trx_stmtsearchtrxnum)) {
            dbe_trxnum_t storagetrxnum;
            trx->trx_stmtsearchtrxnum = dbe_counter_getmaxtrxnum(trx->trx_counter);
            storagetrxnum = dbe_counter_getstoragetrxnum(trx->trx_counter);
            if (DBE_TRXNUM_CMP_EX(storagetrxnum, trx->trx_stmtsearchtrxnum) < 0) {
                ss_dprintf_3(("dbe_trx_getstmtsearchtrxnum:clear RS_SYSI_FLAG_STORAGETREEONLY\n"));
                rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
            }
        }
        return(trx->trx_stmtsearchtrxnum);
}

void dbe_trx_resetstmtsearchtrxnum(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        trx->trx_stmtsearchtrxnum = DBE_TRXNUM_NULL;
}

/*##**********************************************************************\
 *
 *              dbe_trx_setdb
 *
 * Sets the db object of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *  db - in, take
 *              Pointer to the database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_setdb(
        dbe_trx_t* trx,
        dbe_db_t *db)
{
        CHK_TRX(trx);

        trx->trx_db = db;
        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setcd
 *
 * Sets the cd object of the transaction.
 *
 * Parameters :
 *
 *      trx - in, use
 *              Transaction handle.
 *
 *  cd - in, use
 *      Pointer to the cd object.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_setcd(
        dbe_trx_t* trx,
        void *cd)
{
        CHK_TRX(trx);

        trx->trx_cd = cd;
        return DBE_RC_SUCC;
}

#ifdef DBE_REPLICATION

void dbe_trx_setreplicaslave(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setreplicaslave, userid = %d\n", dbe_user_getid(trx->trx_user)));

        trx->trx_replicaslave = TRUE;
        trx->trx_mode = TRX_REPLICASLAVE;
        ss_autotest_or_debug(trx->trx_info->ti_trxmode = trx->trx_mode);
}

void dbe_trx_setstmtfailed(
        dbe_trx_t* trx,
        dbe_ret_t rc)
{
        CHK_TRX(trx);
        ss_dassert(rc != DBE_RC_SUCC);

        trx->trx_stmterrcode = rc;
}

dbe_ret_t dbe_trx_getstmterrcode(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_stmterrcode);
}

bool dbe_trx_stmtactive(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(!DBE_TRXID_EQUAL(trx->trx_stmttrxid, trx->trx_usertrxid));
}

void dbe_trx_sethsbflushallowed(
        dbe_trx_t* trx,
        bool hsbflushallowed)
{
        CHK_TRX(trx);

        trx->trx_hsbflushallowed = hsbflushallowed;
}

static void repsql_listdone(void* data)
{
        trx_repsql_t* repsql = data;

        SsMemFree(repsql->rs_sqlcatalog);
        SsMemFree(repsql->rs_sqlschema);
        SsMemFree(repsql->rs_sqlstr);
        SsMemFree(repsql);
}

dbe_ret_t dbe_trx_replicatesql(
        dbe_trx_t* trx,
        char* sqlcatalog,
        char* sqlschema,
        char* sqlstr)
{
#ifdef DBE_HSB_REPLICATION
        dbe_ret_t rc;
        trx_repsql_t* repsql;

        ss_dprintf_1(("dbe_trx_replicatesql\n"));

        if (trx->trx_replicaslave) {
            ss_dprintf_2(("dbe_trx_replicatesql:!trx->trx_replicaslave, rc=DBE_RC_SUCC\n"));
            return(DBE_RC_SUCC);
        }

        dbe_trx_initrepparams(trx, REP_SQLINIT);

        rc = dbe_trx_replicate(trx, REP_SQLINIT);
        switch (rc) {
            case DBE_RC_SUCC:
                trx->trx_replication = TRUE;
                repsql = SSMEM_NEW(trx_repsql_t);
                repsql->rs_sqlcatalog = SsMemStrdup(sqlcatalog);
                repsql->rs_sqlschema = SsMemStrdup(sqlschema);
                repsql->rs_sqlstr = SsMemStrdup(sqlstr);
                repsql->rs_stmttrxid = trx->trx_stmttrxid;
                if (trx->trx_repsqllist == NULL) {
                    trx->trx_repsqllist = su_list_init(repsql_listdone);
                }
                su_list_insertlast(trx->trx_repsqllist, repsql);
                break;
            case DBE_RC_CONT:
                break;
            case DB_RC_NOHSB:
                rc = DBE_RC_SUCC;
                break;
            default:
                break;
        }
        ss_dprintf_2(("dbe_trx_replicatesql:rc=%s\n", su_rc_nameof(rc)));
        return(rc);

#else /* DBE_HSB_REPLICATION */
        return(DBE_RC_SUCC);
#endif /* DBE_HSB_REPLICATION */
}

dbe_ret_t dbe_trx_stmtrollbackrepsql(
        dbe_trx_t* trx,
        dbe_trxid_t stmttrxid)
{
        trx_repsql_t* repsql;
        su_list_node_t* n;

        if (trx->trx_replicaslave) {
            ss_dassert(trx->trx_repsqllist == NULL);
            return(DBE_RC_SUCC);
        }
        if (trx->trx_repsqllist == NULL) {
            return(DBE_RC_SUCC);
        }

        ss_dassert(trx->trx_replication);

        n = su_list_first(trx->trx_repsqllist);
        while (n != NULL) {
            repsql = su_listnode_getdata(n);
            if (DBE_TRXID_EQUAL(repsql->rs_stmttrxid,stmttrxid)) {
                su_list_node_t* next_n;
                next_n = su_list_next(trx->trx_repsqllist, n);
                su_list_remove(trx->trx_repsqllist, n);
                n = next_n;
            } else {
                n = su_list_next(trx->trx_repsqllist, n);
            }
        }

        return(DBE_RC_SUCC);
}

dbe_ret_t dbe_trx_endrepsql(
        dbe_trx_t* trx,
        bool commitp)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        trx_repsql_t* repsql;
        su_list_node_t* n;

        ss_dprintf_1(("dbe_trx_endrepsql\n"));

        if (trx->trx_replicaslave) {
            ss_dprintf_2(("dbe_trx_endrepsql:!trx->trx_replicaslave, rc=DBE_RC_SUCC\n"));
            ss_dassert(trx->trx_repsqllist == NULL);
            return(DBE_RC_SUCC);
        }
        if (trx->trx_repsqllist == NULL) {
            return(DBE_RC_SUCC);
        }

        ss_dassert(trx->trx_replication);

        if (commitp) {
            rs_auth_t* auth;
            auth = rs_sysi_auth(trx->trx_cd);
            n = su_list_first(trx->trx_repsqllist);
            while (n != NULL) {
                repsql = su_listnode_getdata(n);

                if (dbe_trx_initrepparams(trx, REP_SQL)) {
                    trx->trx_rp.rp_stmttrxid = repsql->rs_stmttrxid;
                    trx->trx_rp.rp_sqlschema = repsql->rs_sqlschema;
                    trx->trx_rp.rp_sqlcatalog = repsql->rs_sqlcatalog;
                    trx->trx_rp.rp_sqlauthid = rs_auth_username(trx->trx_cd, auth);
                    trx->trx_rp.rp_sqlstr = repsql->rs_sqlstr;
                }

                rc = dbe_trx_replicate(trx, REP_SQL);
                switch (rc) {
                    case DBE_RC_SUCC:
                        break;
                    case DBE_RC_CONT:
                        ss_error;
                        return(DBE_RC_CONT);
                    case DB_RC_NOHSB:
                        rc = DBE_RC_SUCC;
                        break;
                    default:
                        dbe_trx_setfailed(trx, rc, TRUE);
                        break;
                }
                if (rc != DBE_RC_SUCC) {
                    break;
                }
                n = su_list_removeandnext(trx->trx_repsqllist, n);
            }
        }

        su_list_done(trx->trx_repsqllist);
        trx->trx_repsqllist = NULL;

        ss_dprintf_2(("dbe_trx_endrepsql:rc=%s\n", su_rc_nameof(rc)));

        return(rc);
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 *
 *              dbe_trx_stoplogging
 *
 * Stops logging in this transaction. This is necessary when
 * log file write fails, but certain system transactions need to
 * be done before creating the checkpoint.
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
void dbe_trx_stoplogging(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_stoplogging, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);

        trx->trx_log = NULL;
        trx->trx_stoplogging = TRUE;

        dbe_trx_semexit(trx);
}

#ifndef SS_NOLOCKING

/*##**********************************************************************\
 *
 *              dbe_trx_setlocktimeout
 *
 * Sets pessimistic lock timeout, value -1 uses the default timeout.
 *
 * Parameters :
 *
 *      trx -
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
void dbe_trx_setlocktimeout(
        dbe_trx_t* trx,
        long timeout)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setlocktimeout, userid = %d, timeout = %ld\n",
            dbe_user_getid(trx->trx_user), timeout));

        if (timeout == -1L) {
            dbe_db_getlocktimeout(trx->trx_db, &trx->trx_pessimistic_lock_to, NULL);
        } else {
            trx->trx_pessimistic_lock_to = timeout;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_setoptimisticlocktimeout
 *
 * Sets optimistic lock timeout, value -1 uses the default timeout.
 *
 * Parameters :
 *
 *      trx -
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
void dbe_trx_setoptimisticlocktimeout(
        dbe_trx_t* trx,
        long timeout)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_setoptimisticlocktimeout, userid = %d, timeout = %ld\n",
            dbe_user_getid(trx->trx_user), timeout));

        if (timeout == -1L) {
            dbe_db_getlocktimeout(trx->trx_db, NULL, &trx->trx_optimistic_lock_to);
        } else {
            trx->trx_optimistic_lock_to = timeout;
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_lockbyname
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relid -
 *
 *
 *      name -
 *
 *
 *      mode -
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
dbe_lock_reply_t dbe_trx_lockbyname(
        dbe_trx_t* trx,
        ulong relid,
        long lock_name,
        dbe_lock_mode_t mode,
        long timeout)
{
        dbe_lock_reply_t reply;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_lockbyname, userid = %d\n", dbe_user_getid(trx->trx_user)));

        SS_PUSHNAME("dbe_trx_lockbyname");

        if (trx->trx_mode == TRX_READONLY
        ||  trx->trx_errcode != DBE_RC_SUCC)
        {
            SS_POPNAME;
            return(LOCK_OK);
        }

        dbe_trx_sementer(trx);

        reply = dbe_lockmgr_lock(
                    trx->trx_lockmgr,
                    trx->trx_locktran,
                    relid,
                    lock_name,
                    mode,
                    timeout,
                    FALSE,
                    NULL);

        dbe_trx_semexit(trx);

        SS_POPNAME;
        return(reply);
}

/*##**********************************************************************\
 *
 *              dbe_trx_unlockbyname
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relid -
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
void dbe_trx_unlockbyname(
        dbe_trx_t* trx,
        ulong relid,
        long lock_name)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_unlockbyname, userid = %d\n", dbe_user_getid(trx->trx_user)));

        SS_PUSHNAME("dbe_trx_unlockbyname");

        if (trx->trx_mode == TRX_READONLY
        ||  trx->trx_errcode != DBE_RC_SUCC)
        {
            SS_POPNAME;
            return;
        }

        dbe_trx_sementer(trx);

        dbe_lockmgr_unlock(
            trx->trx_lockmgr,
            trx->trx_locktran,
            relid,
            lock_name);

        dbe_trx_semexit(trx);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *              dbe_trx_lockbyname_cd
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
 *
 *
 *      relid -
 *
 *
 *      lock_name -
 *
 *
 *      mode -
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
dbe_lock_reply_t dbe_trx_lockbyname_cd(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        ulong relid,
        long lock_name,
        dbe_lock_mode_t mode,
        long timeout)
{
        dbe_lock_reply_t reply;
        dbe_locktran_t* locktran;
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;

        if (trx != NULL) {
            return(dbe_trx_lockbyname(trx,relid,lock_name,mode,timeout));
        }

        locktran = rs_sysi_getlocktran(cd);
        ss_dassert(locktran != NULL);

        db = rs_sysi_db(cd);
        lockmgr = dbe_db_getlockmgr(db);
        ss_dassert(lockmgr != NULL);

        reply = dbe_lockmgr_lock(
                    lockmgr,
                    locktran,
                    relid,
                    lock_name,
                    mode,
                    timeout,
                    FALSE,
                    NULL);

        return(reply);
}

/*##**********************************************************************\
 *
 *              dbe_trx_lock
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relid -
 *
 *
 *      tref_vtpl -
 *
 *
 *      mode -
 *
 *
 *      timeout -
 *
 *
 *      bouncep - in
 *              If TRUE, nothing is locked but a check is made if the locking
 *              is possible.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_lock_reply_t dbe_trx_lock(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref,
        dbe_lock_mode_t mode,
        long timeout,
        bool bouncep,
        bool* p_newlock)
{
        dbe_lockname_t name;
        dbe_lock_reply_t reply;
        rs_sysi_t* cd;

        SS_PUSHNAME("dbe_trx_lock");
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_lock, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_mode == TRX_READONLY) {
            SS_POPNAME;
            return(LOCK_OK);
        }
        if (trx->trx_errcode != DBE_RC_SUCC) {
            SS_POPNAME;
            return(LOCK_OK);
        }

        if (rs_relh_ishistorytable(trx->trx_cd, relh)) {
            ss_dprintf_1(("dbe_trx_lock:force timeout to 0 in case of historytable: relid %d\n", rs_relh_relid(trx->trx_cd, relh)));
            timeout = 0L;
        }

        dbe_trx_sementer(trx);

        if (!trx->trx_escalatelimits->esclim_allowlockbounce) {
            bouncep = FALSE;
        }

        cd = trx->trx_cd;

        name = dbe_tref_getlockname(
                    cd,
                    tref,
                    rs_relh_clusterkey(cd, relh));

        reply = dbe_lockmgr_lock(
                    trx->trx_lockmgr,
                    trx->trx_locktran,
                    rs_relh_relid(cd, relh),
                    name,
                    mode,
                    timeout,
                    bouncep,
                    p_newlock);
        ss_debug(
                if (reply != LOCK_OK) {
                    ss_dprintf_1(("dbe_trx_lock returns %d\n",
                        (int)reply));
                }
            );

        dbe_trx_semexit(trx);

        SS_POPNAME;
        return(reply);
}

/*##**********************************************************************\
 * 
 *      dbe_trx_unlock
 * 
 * Unlocks a lock locked by dbe_trx_lock. Used e.g. if statement fails
 * for some other reason.
 * 
 * Parameters : 
 * 
 *      trx - 
 *          
 *          
 *      relh - 
 *          
 *          
 *      tref - 
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
void dbe_trx_unlock(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref)
{
        dbe_lockname_t name;
        rs_sysi_t* cd;

        SS_PUSHNAME("dbe_trx_unlock");
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_unlock, userid = %d\n", dbe_user_getid(trx->trx_user)));

        ss_dassert(trx->trx_mode != TRX_READONLY);
        ss_dassert(trx->trx_errcode == DBE_RC_SUCC);

        dbe_trx_sementer(trx);

        cd = trx->trx_cd;

        name = dbe_tref_getlockname(
                    cd,
                    tref,
                    rs_relh_clusterkey(cd, relh));

        dbe_lockmgr_unlock(
            trx->trx_lockmgr,
            trx->trx_locktran,
            rs_relh_relid(cd, relh),
            name);

        dbe_trx_semexit(trx);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *              dbe_trx_lockrelh
 *
 * Lock relation to avoid problems with concurrect updates/deletes
 * and drop table.
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_lockrelh(
        dbe_trx_t*  trx,
        rs_relh_t*  relh,
        bool        exclusive,
        long        timeout)
{
        dbe_lock_reply_t    lockreply;
        dbe_lock_mode_t     lockmode;
        long                relid;

        SS_PUSHNAME("dbe_trx_lockrelh");

        CHK_TRX(trx);

        ss_dprintf_1(("dbe_trx_lockrelh, userid = %d relid = %d relh = %08x\n",
                      dbe_user_getid(trx->trx_user),
                      rs_relh_relid(trx->trx_cd, relh),
                      relh));

        if (rs_relh_isaborted(trx->trx_cd, relh)) {
            SS_POPNAME;
            return(DBE_ERR_DDOPACT);
        }
        if (rs_relh_isreadonly(trx->trx_cd, relh)) {
            SS_POPNAME;
            return(DBE_ERR_RELREADONLY);
        }

        relid = rs_relh_relid(trx->trx_cd, relh);
        lockmode = exclusive ? LOCK_X : LOCK_S;

        if (rs_relh_ishistorytable(trx->trx_cd, relh)) {
            ss_dprintf_1(("dbe_trx_lockrelh:force timeout to 0 in case of historytable: relid %d\n", relid));
            timeout = 0L;
        }

        if (timeout == -1L) {
            /* Use system default. */
            timeout = trx->trx_table_lock_to;
        }

        lockreply = dbe_trx_lockbyname(trx, relid, relid, lockmode, timeout);
        switch (lockreply) {
            case LOCK_OK:
                if(rs_relh_isaborted(trx->trx_cd, relh)) {
                    SS_POPNAME;
                    return(DBE_ERR_DDOPACT);
                } else {
                    SS_POPNAME;
                    return(DBE_RC_SUCC);
                }
            case LOCK_WAIT:
                SS_POPNAME;
                return(DBE_RC_WAITLOCK);
            default:
                SS_POPNAME;
                return(DBE_ERR_LOCKED);

        }
}

/*##**********************************************************************\
 *
 *      dbe_trx_lockrelid
 *
 * Like dbe_trx_lockrelh, but locks based on relid instead of relh.
 *
 * Parameters:
 *      trx - <usage>
 *          <description>
 *
 *      relid - <usage>
 *          <description>
 *
 *      exclusive - <usage>
 *          <description>
 *
 *      timeout - <usage>
 *          <description>
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_ret_t dbe_trx_lockrelid(
        dbe_trx_t*  trx,
        long        relid,
        bool        exclusive,
        long        timeout)
{
        dbe_lock_reply_t    lockreply;
        dbe_lock_mode_t     lockmode;

        SS_PUSHNAME("dbe_trx_lockrelid");

        CHK_TRX(trx);

        ss_dprintf_1(("dbe_trx_lockrelid, userid = %d relid = %d\n",
                      dbe_user_getid(trx->trx_user),
                      relid));

        lockmode = exclusive ? LOCK_X : LOCK_S;

        if (timeout == -1L) {
            /* Use system default. */
            timeout = trx->trx_table_lock_to;
        }

        lockreply = dbe_trx_lockbyname(trx, relid, relid, lockmode, timeout);
        switch (lockreply) {
            case LOCK_OK:
                SS_POPNAME;
                return(DBE_RC_SUCC);
            case LOCK_WAIT:
                SS_POPNAME;
                return(DBE_RC_WAITLOCK);
            default:
                SS_POPNAME;
                return(DBE_ERR_LOCKED);

        }
}

void dbe_trx_unlockrelid(
        dbe_trx_t*  trx,
        long        relid)
{
        CHK_TRX(trx);

        ss_dprintf_1(("dbe_trx_unlockrelid, userid = %d relid = %d\n",
                      dbe_user_getid(trx->trx_user),
                      relid));

        dbe_trx_sementer(trx);

        dbe_lockmgr_unlock_shared(
            trx->trx_lockmgr,
            trx->trx_locktran,
            relid,
            relid);

        dbe_trx_semexit(trx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_lockrelh_cd
 *
 * Called from LOCK TABLE statement.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
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
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trx_lockrelh_cd(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool exclusive,
        long timeout)
{
        dbe_lock_reply_t lockreply;
        dbe_lock_mode_t lockmode;
        long relid;
        dbe_db_t* db;

        if (rs_relh_isaborted(cd, relh)) {
            return(DBE_ERR_DDOPACT);
        }
        if (rs_relh_isreadonly(cd, relh)) {
            return(DBE_ERR_RELREADONLY);
        }

        relid = rs_relh_relid(cd, relh);
        lockmode = exclusive ? LOCK_X : LOCK_S;

        if (trx != NULL && rs_relh_ishistorytable(trx->trx_cd, relh)) {
            ss_dprintf_1(("dbe_trx_lockrelh_cd:force timeout to 0 in case of historytable: relid %d\n", relid));
            timeout = 0L;
        }

        if (timeout == -1L) {
            /* Use system default. */
            db = rs_sysi_db(cd);
            timeout = dbe_db_gettablelocktimeout(db);
        }

        lockreply = dbe_trx_lockbyname_cd(cd, trx, relid, relid, lockmode, timeout);
        switch (lockreply) {
            case LOCK_OK:
                return(DBE_RC_SUCC);
            case LOCK_WAIT:
                return(DBE_RC_WAITLOCK);
            default:
                return(DBE_ERR_LOCKED);
        }
}


/*##**********************************************************************\
 *
 *              dbe_trx_lockrelh_long
 *
 * Called from LOCK TABLE -statement
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
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
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trx_lockrelh_long(
        rs_sysi_t* cd,
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh,
        bool exclusive,
        long timeout)
{
        dbe_lock_reply_t lockreply;
        dbe_lock_mode_t lockmode;
        long relid;
        dbe_locktran_t* locktran;
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;

        if (rs_relh_isaborted(cd, relh)) {
            return(DBE_ERR_DDOPACT);
        }
        if (rs_relh_isreadonly(cd, relh)) {
            return(DBE_ERR_RELREADONLY);
        }

        relid = rs_relh_relid(cd, relh);
        lockmode = exclusive ? LOCK_X : LOCK_S;
        db = rs_sysi_db(cd);

        if (timeout == -1L) {
            /* Use system default. */
            timeout = dbe_db_gettablelocktimeout(db);
        }

        locktran = rs_sysi_getlocktran(cd);
        ss_dassert(locktran != NULL);

        lockmgr = dbe_db_getlockmgr(db);
        ss_dassert(lockmgr != NULL);

        lockreply = dbe_lockmgr_lock_long(
                        lockmgr,
                        locktran,
                        relid,
                        relid,
                        lockmode,
                        timeout,
                        FALSE);

        switch (lockreply) {
            case LOCK_OK:
                return(DBE_RC_SUCC);
            case LOCK_WAIT:
                return(DBE_RC_WAITLOCK);
            default:
                return(DBE_ERR_LOCKED);
        }
}


/*##**********************************************************************\
 *
 *              dbe_trx_unlockall_long
 *
 * Called from UNLOCK -statement (after commit) and from disconnect
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
void dbe_trx_unlockall_long(
        rs_sysi_t* cd)
{
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;
        dbe_locktran_t* locktran;

        db = rs_sysi_db(cd);

        lockmgr = dbe_db_getlockmgr(db);
        locktran = rs_sysi_getlocktran(cd);

        if (locktran != NULL) {
            dbe_lockmgr_unlockall_long(lockmgr, locktran);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_unlockrelh
 *
 * Unlock relation. This is explisity called from LOCK/UNLOCK statements.
 *
 * Parameters :
 *
 *      trx -
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
void dbe_trx_unlockrelh(
        rs_sysi_t* cd,
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh)
{
        long relid;
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;
        dbe_locktran_t* locktran;

        db = rs_sysi_db(cd);

        lockmgr = dbe_db_getlockmgr(db);
        ss_dassert(lockmgr != NULL);
        locktran = rs_sysi_getlocktran(cd);
        ss_dassert(locktran != NULL);

        relid = rs_relh_relid(cd, relh);

        dbe_lockmgr_unlock(
                    lockmgr,
                    locktran,
                    relid,
                    relid);
}


/*##**********************************************************************\
 *
 *              dbe_trx_getlockrelh
 *
 * Gets lock data if locked by caller.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
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
bool dbe_trx_getlockrelh(
        rs_sysi_t* cd,
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh,
        bool* p_isexclusive,
        bool* p_isverylong_duration)
{
        bool locked;
        long relid;
        dbe_lock_mode_t mode;
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;
        dbe_locktran_t* locktran;

        db = rs_sysi_db(cd);

        lockmgr = dbe_db_getlockmgr(db);
        ss_dassert(lockmgr != NULL);
        locktran = rs_sysi_getlocktran(cd);
        ss_dassert(locktran != NULL);

        relid = rs_relh_relid(cd, relh);

        locked = dbe_lockmgr_getlock(
                    lockmgr,
                    locktran,
                    relid,
                    relid,
                    &mode,
                    p_isverylong_duration);

        if (locked && p_isexclusive != NULL) {
            *p_isexclusive = (mode == LOCK_X);
        }

        return(locked);
}


/*##**********************************************************************\
 *
 *              dbe_trx_lockrelh_convert
 *
 * Converts called lock. It is possible to convert very long
 * duration locks to normal. Also exclusive lock can be converted to shared
 * This function is called from LOCK/UNLOCK -statements
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      trx -
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
dbe_ret_t dbe_trx_lockrelh_convert(
        rs_sysi_t* cd,
        dbe_trx_t* trx __attribute__ ((unused)),
        rs_relh_t* relh,
        bool exclusive,
        bool verylong_duration)
{
        dbe_lock_reply_t lockreply;
        long relid;
        dbe_lock_mode_t lockmode;
        dbe_db_t* db;
        dbe_lockmgr_t* lockmgr;
        dbe_locktran_t* locktran;

        db = rs_sysi_db(cd);

        lockmgr = dbe_db_getlockmgr(db);
        ss_dassert(lockmgr != NULL);
        locktran = rs_sysi_getlocktran(cd);
        ss_dassert(locktran != NULL);

        lockmode = exclusive ? LOCK_X : LOCK_S;
        if (verylong_duration) {
            ss_dassert(lockmode == LOCK_X);
        }
        relid = rs_relh_relid(cd, relh);

        lockreply = dbe_lockmgr_lock_convert(
                        lockmgr,
                        locktran,
                        relid,
                        relid,
                        lockmode,
                        verylong_duration);

        switch (lockreply) {
            case LOCK_OK:
                return(DBE_RC_SUCC);
            case LOCK_WAIT:
                SS_POPNAME;
                return(DBE_RC_WAITLOCK);
            default:
                SS_POPNAME;
                return(DBE_ERR_LOCKED);

        }
}


/*##**********************************************************************\
 *
 *              dbe_trx_uselocking
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh -
 *
 *
 *      mode -
 *
 *
 *      p_timeout - out
 *              Lock timeout.
 *
 *      p_optimistic_lock - out
 *              If non-NULL, TRUE is set into *p_optimistic_lock if
 *          optimistic locking (or no locking) should be used.
 *              Otherwise FALSE is stored into *p_optlock.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_trx_uselocking(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_lock_mode_t mode,
        long* p_timeout,
        bool* p_optimistic_lock)
{
        rs_reltype_t reltype;
        rs_sysi_t* cd;

        if (p_optimistic_lock != NULL) {
            *p_optimistic_lock = TRUE;
        }

        if (trx == DBE_TRX_NOTRX || trx == DBE_TRX_HSBTRX) {
            /* Trx pointer can have value DBE_TRX_NOTRX during recovery,
             * do not use locking.
             */
            return(FALSE);
        }

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_uselocking, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_mode == TRX_NOCHECK || trx->trx_mode == TRX_REPLICASLAVE) {
            return(FALSE);
        }
        if (trx->trx_mode == TRX_READONLY) {
            /* Read only transactions never lock.
             */
            ss_dprintf_2(("dbe_trx_uselocking: NO (read only)\n"));
            return(FALSE);
        }

        cd = trx->trx_cd;

        if (rs_relh_issysrel(cd, relh)) {
            /* System tables never lock.
             */
            ss_dprintf_2(("dbe_trx_uselocking: NO (system table)\n"));
            return(FALSE);
        }

        reltype = rs_relh_reltype(cd, relh);

        if (reltype == RS_RELTYPE_MAINMEMORY) {
            ss_dprintf_2(("dbe_trx_uselocking: mainmemory\n"));
            return(FALSE);
        }
        if (reltype == RS_RELTYPE_PESSIMISTIC) {
            /* Full pessimistic locking is used in pessimistic tables.
             */
            rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
            if (p_optimistic_lock != NULL) {
                *p_optimistic_lock = FALSE;
            }
            *p_timeout = trx->trx_pessimistic_lock_to;
            if (((dbe_cfg_versionedpessimisticreadcommitted 
                      && trx->trx_nonrepeatableread)
                  || (dbe_cfg_versionedpessimisticrepeatableread 
                      && dbe_trx_getisolation(trx) != RS_SQLI_ISOLATION_SERIALIZABLE))
                && mode == LOCK_S) 
            {
                ss_dprintf_2(("dbe_trx_uselocking: NO (pessimistic table, LOCK_S, READ COMMITTED or REPEATABLE READ)\n"));
                return(FALSE);
            } else {
                ss_dprintf_2(("dbe_trx_uselocking: YES (pessimistic table)\n"));
                return(TRUE);
            }
        }
        if (trx->trx_earlyvld) {
            /* During early validate, shared locks are not set (read does
             * not lock). Other lock modes set the locks.
             */
            if (mode > LOCK_S) {
                /* Use locks.
                 */
                rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
                *p_timeout = trx->trx_optimistic_lock_to;
                ss_dprintf_2(("dbe_trx_uselocking: YES (early validate, optimistical table)\n"));
                return(TRUE);
            } else {
                ss_dprintf_2(("dbe_trx_uselocking: NO (early validate, optimistical table)\n"));
                return(FALSE);
            }

        }
        /* True optimistic transactions never lock.
         */
        ss_dprintf_2(("dbe_trx_uselocking: NO (late validate, optimistic table)\n"));
        return(FALSE);
}

#endif /* SS_NOLOCKING */

/*##**********************************************************************\
 *
 *      dbe_trx_rollback_searches
 *
 * Abort searches of this transaction.  Essentially a statement abort for
 * searches that don't have a statement open.
 *
 * Parameters:
 *      trx - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_trx_rollback_searches(
        dbe_trx_t*  trx)
{
        CHK_TRX(trx);

        if (trx->trx_mmll != NULL) {
            dbe_mme_locklist_rollback_searches(trx->trx_mmll);
        }
        /* XXX - need to do something for D-tables? */
}

#ifndef SS_NOSEQUENCE

/*##**********************************************************************\
 *
 *              dbe_trx_seqcommit_nomutex
 *
 * Does sequence commit. This is done only for dense sequences,
 * sparse sequences are done without transaction control. The commit
 * information is signaled to the sequence object.
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
dbe_ret_t dbe_trx_seqcommit_nomutex(
        dbe_trx_t* trx)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        su_rbt_node_t* rn;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_seqcommit_nomutex, userid = %d\n", dbe_user_getid(trx->trx_user)));
        SS_PUSHNAME("dbe_trx_seqcommit_nomutex");

        if (trx->trx_seqrbt == NULL) {
            ss_dprintf_1(("dbe_trx_seqtrans, no sequence writes\n"));
            SS_POPNAME;
            return(DBE_RC_SUCC);
        }

        rn = su_rbt_min(trx->trx_seqrbt, NULL);
        while (rn != NULL) {
            dbe_seqvalue_t* sv;
            sv = su_rbtnode_getkey(rn);
            rc = dbe_seq_commit(trx->trx_seq, trx, sv);
            if (rc != DBE_RC_SUCC) {
                break;
            }
            rn = su_rbt_succ(trx->trx_seqrbt, rn);
        }

        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_seqtransend_nomutex
 *
 * Marks transaction as ended for dense sequence writes.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
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
void dbe_trx_seqtransend_nomutex(
        dbe_trx_t* trx,
        bool commitp)
{
        su_rbt_node_t* rn;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_seqtransend_nomutex, userid = %d, commitp = %d\n", dbe_user_getid(trx->trx_user), commitp));
        SS_PUSHNAME("dbe_trx_seqtransend_nomutex");

        if (trx->trx_seqrbt == NULL) {
            ss_dprintf_1(("dbe_trx_seqtransend_nomutex, no sequence writes\n"));
            SS_POPNAME;
            return;
        }

        rn = su_rbt_min(trx->trx_seqrbt, NULL);
        while (rn != NULL) {
            dbe_seqvalue_t* sv;
            sv = su_rbtnode_getkey(rn);
            dbe_seq_transend(trx->trx_seq, sv, commitp);
            rn = su_rbt_succ(trx->trx_seqrbt, rn);
        }
        su_rbt_done(trx->trx_seqrbt);
        trx->trx_seqrbt = NULL;

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              seqrbt_insert_compare
 *
 * Sequence search tree compare routine.
 *
 * Parameters :
 *
 *      key1 -
 *
 *
 *      key2 -
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
static int seqrbt_insert_compare(void* key1, void* key2)
{
        dbe_seqvalue_t* sv1 = key1;
        dbe_seqvalue_t* sv2 = key2;

        return(su_rbt_long_compare(dbe_seqvalue_getid(sv1), dbe_seqvalue_getid(sv2)));
}

/*##**********************************************************************\
 *
 *              dbe_trx_markseqwrite
 *
 * Marks a sequence write operation to the transaction. The marked
 * sequence write is later signaled back to the sequence object
 * in transaction commit or rollback. This function should be called only
 * for dence sequences.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      sv -
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
dbe_ret_t dbe_trx_markseqwrite(
        dbe_trx_t* trx,
        void* sv)
{
        su_rbt_node_t* rn;
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_markseqwrite, userid = %d, seq_id = %ld\n", dbe_user_getid(trx->trx_user), dbe_seqvalue_getid(sv)));
        SS_PUSHNAME("dbe_trx_markseqwrite");
        ss_dassert(dbe_trx_iswrites(trx));

#ifdef DBE_REPLICATION
        if (!trx->trx_replicaslave) {

            dbe_trx_initrepparams(trx, REP_COMMIT_CHECK);

            rc = dbe_trx_replicate(trx, REP_COMMIT_CHECK);
            if (rc != DBE_RC_SUCC && rc != DB_RC_NOHSB) {
                SS_POPNAME;
                return(rc);
            }
            rc = DBE_RC_SUCC;
        }
#endif /* DBE_REPLICATION */

        dbe_trx_sementer(trx);

        if (trx->trx_seqrbt == NULL) {
            trx->trx_seqrbt = su_rbt_init(seqrbt_insert_compare, NULL);
        }

        rn = su_rbt_search(trx->trx_seqrbt, sv);
        if (rn == NULL) {
            /* New sequence id in this transaction. */
            bool succp;
            succp = su_rbt_insert(trx->trx_seqrbt, sv);
            ss_dassert(succp);
            if (!succp) {
                rc = DBE_ERR_FAILED;
            }
        } else {
            rc = DBE_RC_FOUND;
        }

        dbe_trx_semexit(trx);

        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_logseqvalue
 *
 * Logs sequence value to the transaction log. The whole equence values
 * need to be logged only for dense sequences, sparse sequences can be
 * logged with function dbe_trx_logseqinc.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      seq_id -
 *
 *
 *      densep -
 *
 *
 *      seq_value -
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
dbe_ret_t dbe_trx_logseqvalue(
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        void* seq_value)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_logseqvalue, userid = %d, seq_id = %ld\n", dbe_user_getid(trx->trx_user), seq_id));
        SS_PUSHNAME("dbe_trx_logseqvalue");
        ss_dassert(!densep || dbe_trx_iswrites(trx));

#ifdef DBE_REPLICATION
        if (!trx->trx_replicaslave) {
            dbe_ret_t rep_rc;

            if (dbe_trx_initrepparams(trx, densep ? REP_SEQDENSE : REP_SEQSPARSE)) {
                va_setdata(&trx->trx_rp.rp_vabuf, seq_value, sizeof(rs_tuplenum_t));
                trx->trx_rp.rp_vtpl = (vtpl_t*)&trx->trx_rp.rp_vabuf;
                trx->trx_rp.rp_seqid = seq_id;
            }

            rep_rc = dbe_trx_replicate(
                        trx,
                        densep ? REP_SEQDENSE : REP_SEQSPARSE);
            switch (rep_rc) {
                case DBE_RC_CONT:
                    ss_rc_error(rep_rc);
                case DBE_RC_SUCC:
                    trx->trx_replication = TRUE;
                    break;
                case DB_RC_NOHSB:
                    break;
                default:
                    SS_POPNAME;
                    return(rep_rc);
            }
        }
#endif /* DBE_REPLICATION */


        trx->trx_nlogwrites++;

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL) {
            if (densep) {
                rc = dbe_log_putsetseq(
                        trx->trx_log,
                        trx->trx_cd,
                        DBE_LOGREC_SETSEQ,
                        trx->trx_usertrxid,
                        (FOUR_BYTE_T)seq_id,
                        seq_value);
            } else {
                rc = dbe_log_putsetctr(
                        trx->trx_log,
                        DBE_LOGREC_SETCTR,
                        (FOUR_BYTE_T)seq_id,
                        seq_value);
            }
        }
#endif /* SS_NOLOGGING */
        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_logseqinc
 *
 * Logs sequence increment info to the transaction log. This function
 * should be called only for sparse sequences, dense sequences should
 * be logged by calling function dbe_trx_logseqvalue.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      seq_id -
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
dbe_ret_t dbe_trx_logseqinc(
        dbe_trx_t* trx,
        long seq_id,
        void* seq_value)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_logseqinc, userid = %d, seq_id = %ld\n", dbe_user_getid(trx->trx_user), seq_id));
        SS_PUSHNAME("dbe_trx_logseqinc");
#ifdef SS_HSBG2
#else
        ss_dassert(dbe_trx_iswrites(trx));
#endif

#ifdef DBE_REPLICATION
        if (!trx->trx_replicaslave) {
            dbe_ret_t rep_rc;

            if (dbe_trx_initrepparams(trx, REP_SEQSPARSE)) {
                va_setdata(&trx->trx_rp.rp_vabuf, seq_value, sizeof(rs_tuplenum_t));
                trx->trx_rp.rp_vtpl = (vtpl_t*)&trx->trx_rp.rp_vabuf;  /* Replicate values, not increments. */
                trx->trx_rp.rp_seqid = seq_id;
            }

            rep_rc = dbe_trx_replicate(trx, REP_SEQSPARSE);
            switch (rep_rc) {
                case DBE_RC_CONT:
                    ss_rc_error(rep_rc);
                case DBE_RC_SUCC:
                    trx->trx_replication = TRUE;
                    break;
                case DB_RC_NOHSB:
                    break;
                default:
                    SS_POPNAME;
                    return(rep_rc);
            }
        }
#endif /* DBE_REPLICATION */

        trx->trx_nlogwrites++;

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL) {
            rc = dbe_log_putincctr(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_INCCTR,
                    (FOUR_BYTE_T)seq_id);
        }
#endif /* SS_NOLOGGING */

        SS_POPNAME;
        return(rc);
}

#endif /* SS_NOSEQUENCE */

static trx_cardininfo_t* trx_getcardininfo(rs_sysi_t* cd)
{
        trx_cardininfo_t* tci;

        tci = rs_sysi_gettrxcardininfo(cd);
        if (tci == NULL) {
            dbe_user_t* user;
            
            tci = SSMEM_NEW(trx_cardininfo_t);

            user = rs_sysi_user(cd);
            
            ss_debug(tci->tci_chk = DBE_CHK_TCI);
            tci->tci_sem = dbe_user_gettrxsem(user);
            tci->tci_cd = cd;
            tci->tci_userid = rs_sysi_userid(cd);
            tci->tci_cardinrbt = NULL;
            tci->tci_stmtcardinrbt = NULL;
            tci->tci_stmtcardingrouplist = NULL;
            ss_debug(tci->tci_trx = NULL);

            ss_dprintf_3(("trx_getcardininfo:create new trxcardininfo, userid = %d, tci = %ld\n", rs_sysi_userid(cd), (long)tci));

            rs_sysi_settrxcardininfo(cd, tci);
        }
        return(tci);
}

static void trx_freecardininforbt(rs_sysi_t* cd, su_rbt_t* rbt)
{
        su_rbt_node_t* rn;
        trx_cardin_t* tcr;

        rn = su_rbt_min(rbt, NULL);
        while (rn != NULL) {
            tcr = su_rbtnode_getkey(rn);
            SS_MEM_SETUNLINK(tcr->tcr_relh);
            rs_relh_done(cd, tcr->tcr_relh);
            SsMemFree(tcr);
            rn = su_rbt_succ(rbt, rn);
        }
        su_rbt_done(rbt);
}

void dbe_trx_freecardininfo(rs_sysi_t* cd)
{
        trx_cardininfo_t* tci;

        tci = rs_sysi_gettrxcardininfo(cd);
        if (tci != NULL) {
            CHK_TCI(tci);
            if (tci->tci_stmtcardinrbt != NULL) {
                trx_freecardininforbt(cd, tci->tci_stmtcardinrbt);
            }
            if (tci->tci_cardinrbt != NULL) {
                trx_freecardininforbt(cd, tci->tci_cardinrbt);
            }
            SsMemFree(tci);
            rs_sysi_settrxcardininfo(cd, NULL);
        }
}

#ifdef SS_DEBUG
bool dbe_trx_checkcardininfo(dbe_trx_t* trx)
{
        trx_cardininfo_t* tci;

        tci = rs_sysi_gettrxcardininfo(trx->trx_cd);

        if (tci == NULL) {
            ss_dassert(trx->trx_cardininfo_in_cd == NULL);
            return(TRUE);
        }

        if (trx->trx_cardininfo_in_cd == NULL) {
            ss_dassert(tci->tci_trx == NULL);
            trx->trx_cardininfo_in_cd = tci; 
            tci->tci_trx = trx;
            return(TRUE);
        }
        ss_dassert(trx->trx_cardininfo_in_cd == tci);
        ss_dassert(tci->tci_trx == trx);
        return(TRUE);
}
#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *              trxcardin_insertcmp
 *
 *
 *
 * Parameters :
 *
 *      key1 -
 *
 *
 *      key2 -
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
static int trxcardin_insertcmp(void* key1, void* key2)
{
        trx_cardin_t* tcr1 = key1;
        trx_cardin_t* tcr2 = key2;

        return(su_rbt_long_compare(tcr1->tcr_relid, tcr2->tcr_relid));
}

/*#***********************************************************************\
 *
 *              trxcardin_searchcmp
 *
 *
 *
 * Parameters :
 *
 *      search_key -
 *
 *
 *      rbt_key -
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
static int trxcardin_searchcmp(void* search_key, void* rbt_key)
{
        trx_cardin_t* tcr = rbt_key;

        return(su_rbt_long_compare((long)search_key, tcr->tcr_relid));
}

/*#***********************************************************************\
 *
 *              trx_addcardin
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh -
 *
 *
 *      ntuples -
 *
 *
 *      nbytes -
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
static void trx_addcardin(
        trx_cardininfo_t* tci,
        rs_relh_t*  relh,
        long        ntuples,
        long        nbytes,
        long        nchanges,
        su_rbt_t**  p_rbt,
        bool*       p_relh_taken)
{
        su_rbt_node_t* rn;
        trx_cardin_t* tcr;
        long relid;

        ss_dprintf_3(("trx_addcardin:ntuples=%ld, nbytes=%ld, tci=%ld\n", ntuples, nbytes, (long)tci));
#ifndef SS_MYSQL
        ss_dassert(SsSemThreadIsEntered(tci->tci_sem));
#endif

        if (*p_rbt == NULL) {
            *p_rbt = su_rbt_inittwocmp(
                        trxcardin_insertcmp,
                        trxcardin_searchcmp,
                        NULL);
        }

        relid = rs_relh_relid(tci->tci_cd, relh);

        rn = su_rbt_search(*p_rbt, (void*)relid);

        if (rn == NULL) {
            /* Add a new entry.
             */

            tcr = SSMEM_NEW(trx_cardin_t);

            tcr->tcr_relid = relid;
            tcr->tcr_relh = relh;
            tcr->tcr_ntuples = ntuples;
            tcr->tcr_nbytes = nbytes;
            tcr->tcr_nchanges = nchanges;

            if (p_relh_taken == NULL) {
                rs_relh_link(tci->tci_cd, relh);
                SS_MEM_SETLINK(relh);
            } else {
                *p_relh_taken = TRUE;
            }

            su_rbt_insert(*p_rbt, tcr);

        } else {
            /* Update old entry.
             */
            tcr = su_rbtnode_getkey(rn);

            tcr->tcr_ntuples += ntuples;
            tcr->tcr_nbytes += nbytes;
            tcr->tcr_nchanges += nchanges;
        }
}

/*#***********************************************************************\
 *
 *              trx_cardintrans
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
 *
 *
 *      stmtp -
 *
 *
 *      rbt -
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
static void trx_cardintrans(
        trx_cardininfo_t* tci,
        bool commitp,
        bool stmtp,
        su_rbt_t* rbt)
{
        su_rbt_node_t* rn;
        trx_cardin_t* tcr;
        bool relh_taken;

        CHK_TCI(tci);
        ss_dprintf_3(("trx_cardintrans, userid = %d, commitp = %d, stmtp = %d, tci = %ld\n",
            tci->tci_userid, commitp, stmtp, (long)tci));
        ss_dassert(rbt != NULL);
#ifndef SS_MYSQL
        ss_dassert(SsSemThreadIsEntered(tci->tci_sem) || !commitp);
#endif

        rn = su_rbt_min(rbt, NULL);
        while (rn != NULL) {
            relh_taken = FALSE;
            tcr = su_rbtnode_getkey(rn);
            if (commitp) {
                if (stmtp) {
                    trx_addcardin(
                        tci,
                        tcr->tcr_relh,
                        tcr->tcr_ntuples,
                        tcr->tcr_nbytes,
                        tcr->tcr_nchanges,
                        &tci->tci_cardinrbt,
                        &relh_taken);
                } else {
                    if (rs_relh_updatecardinal(
                                tci->tci_cd,
                                tcr->tcr_relh,
                                tcr->tcr_ntuples,
                                tcr->tcr_nbytes,
                                tcr->tcr_nchanges)) {
                        dbe_db_newplan(
                            rs_sysi_db(tci->tci_cd),
                            rs_relh_relid(tci->tci_cd, tcr->tcr_relh));
                    }
                }
            }
            if (!relh_taken) {
                SS_MEM_SETUNLINK(tcr->tcr_relh);
                rs_relh_done(tci->tci_cd, tcr->tcr_relh);
            }
            SsMemFree(tcr);
            rn = su_rbt_succ(rbt, rn);
        }
        su_rbt_done(rbt);
}

/*##**********************************************************************\
 *
 *              dbe_trx_cardintrans_mutexif
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
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
void dbe_trx_cardintrans_mutexif(
        rs_sysi_t* cd,
        bool commitp,
        bool entermutex,
        bool freetrxcardininfo)
{
        trx_cardininfo_t* tci;

        ss_dprintf_1(("dbe_trx_cardintrans_mutexif, userid = %d, commitp = %d\n", rs_sysi_userid(cd), commitp));

        tci = rs_sysi_gettrxcardininfo(cd);

        if (tci != NULL) {
            CHK_TCI(tci);
            if (entermutex) {
                SsSemEnter(tci->tci_sem);
            }
            ss_dassert(tci->tci_stmtcardinrbt == NULL || freetrxcardininfo);
            if (tci->tci_cardinrbt != NULL) {
                trx_cardintrans(
                    tci,
                    commitp,
                    FALSE,
                    tci->tci_cardinrbt);

                tci->tci_cardinrbt = NULL;
            }
            if (entermutex) {
                SsSemExit(tci->tci_sem);
            }
            if (freetrxcardininfo) {
                dbe_trx_freecardininfo(cd);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_cardinstmttrans_mutexif
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      commitp -
 *
 *
 *      groupp -
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
void dbe_trx_cardinstmttrans_mutexif(
        rs_sysi_t* cd,
        bool commitp,
        bool groupstmtp,
        bool entermutex)
{
        trx_cardininfo_t* tci;

        ss_dprintf_1(("dbe_trx_cardinstmttrans_mutexif, userid = %d, commitp = %d\n", rs_sysi_userid(cd), commitp));

        tci = rs_sysi_gettrxcardininfo(cd);

        if (tci != NULL) {
            CHK_TCI(tci);

            if (entermutex) {
                SsSemEnter(tci->tci_sem);
            }
            if (groupstmtp) {
                if (commitp && tci->tci_stmtcardinrbt != NULL) {
                    if (tci->tci_stmtcardingrouplist == NULL) {
                        tci->tci_stmtcardingrouplist = su_list_init(NULL);
                    }
                    su_list_insertlast(
                        tci->tci_stmtcardingrouplist,
                        tci->tci_stmtcardinrbt);
                    tci->tci_stmtcardinrbt = NULL;
                }

            } else {
                if (tci->tci_stmtcardingrouplist != NULL) {
                    su_list_beginloop(tci->tci_stmtcardingrouplist, su_rbt_t*, rbt)
                        trx_cardintrans(
                            tci,
                            commitp,
                            TRUE,
                            rbt);
                    su_list_endloop
                    su_list_done(tci->tci_stmtcardingrouplist);
                    tci->tci_stmtcardingrouplist = NULL;
                }
            }

            if (tci->tci_stmtcardinrbt != NULL) {
                trx_cardintrans(
                    tci,
                    commitp,
                    TRUE,
                    tci->tci_stmtcardinrbt);
                tci->tci_stmtcardinrbt = NULL;
            }
            if (entermutex) {
                SsSemExit(tci->tci_sem);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_insertbytes
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh -
 *
 *
 *      nbytes -
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
void dbe_trx_insertbytes(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        nbytes,
        ulong       nchanges)
{
        trx_cardininfo_t* tci;
        su_rbt_t** p_rbt;

        tci = trx_getcardininfo(cd);

        CHK_TCI(tci);
        ss_dprintf_1(("dbe_trx_insertbytes, userid = %d, nbytes = %ld\n", tci->tci_userid, nbytes));

        SsSemEnter(tci->tci_sem);

        /* Update statement cardinal tree.
         */
        ss_dprintf_2(("dbe_trx_insertbytes, update stmt cardin tree\n"));
        p_rbt = &tci->tci_stmtcardinrbt;

        if (nbytes < 0) {
            trx_addcardin(tci, relh, -1L, nbytes, nchanges, p_rbt, NULL);
        } else {
            trx_addcardin(tci, relh, 1L, nbytes, nchanges, p_rbt, NULL);
        }

        SsSemExit(tci->tci_sem);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deletebytes
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh -
 *
 *
 *      nbytes -
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
void dbe_trx_deletebytes(
        rs_sysi_t*  cd,
        rs_relh_t*  relh,
        long        nbytes,
        ulong       nchanges)
{
        ss_dprintf_1(("dbe_trx_deletebytes, nbytes = %ld\n", nbytes));

        dbe_trx_insertbytes(cd, relh, -nbytes, nchanges);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getrelhcardin_nomutex
 *
 * Returns relation cardinality information. Also changes made in the
 * current transaction are counted.
 *
 * Parameters :
 *
 *      trx - in
 *
 *
 *      relh - in
 *
 *
 *      p_ntuples - out
 *
 *
 *      p_nbytes - out
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
void dbe_trx_getrelhcardin_nomutex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes)
{
        trx_cardininfo_t* tci;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_getrelhcardin_nomutex, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(p_ntuples != NULL);
        ss_dassert(p_nbytes != NULL);
        ss_dassert(dbe_trx_semisentered(trx));

        if (rs_relh_isglobaltemporary(trx->trx_cd, relh)) {
            dbe_mme_gettemporarytablecardin(
                    trx->trx_cd,
                    dbe_db_getmme(trx->trx_db),
                    relh,
                    p_ntuples,
                    p_nbytes);
        } else {
            rs_relh_cardininfo(
                trx->trx_cd,
                relh,
                p_ntuples,
                p_nbytes);
        }

        tci = rs_sysi_gettrxcardininfo(trx->trx_cd);

        if (tci != NULL && tci->tci_cardinrbt != NULL) {
            su_rbt_node_t* rn;
            trx_cardin_t* tcr;

            rn = su_rbt_search(
                    tci->tci_cardinrbt,
                    (void*)rs_relh_relid(tci->tci_cd, relh));
            if (rn != NULL) {
                ss_int8_t dummy1, dummy2;
                tcr = su_rbtnode_getkey(rn);

                SsInt8SetInt4(&dummy1, tcr->tcr_ntuples);
                SsInt8AddInt8(&dummy2, *p_ntuples, dummy1);
                if (SsInt8IsNegative(dummy2)) {
#ifndef AUTOTEST_RUN
                    ss_rc_dassert(ss_debug_nocardinalcheck, rs_relh_relid(tci->tci_cd, relh));
#endif
                    SsInt8Set0(&dummy2);
                }
                *p_ntuples = dummy2;

                SsInt8SetInt4(&dummy1, tcr->tcr_nbytes);
                SsInt8AddInt8(&dummy2, *p_nbytes, dummy1);
                if (SsInt8IsNegative(dummy2)) {
#ifndef AUTOTEST_RUN
                    ss_rc_dassert(ss_debug_nocardinalcheck, rs_relh_relid(tci->tci_cd, relh));
#endif
                    SsInt8Set0(&dummy2);
                }
                *p_nbytes = dummy2;
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getrelhcardin
 *
 * Returns relation cardinality information. Also changes made in the
 * current transaction are counted.
 *
 * Parameters :
 *
 *      trx - in
 *
 *
 *      relh - in
 *
 *
 *      p_ntuples - out
 *
 *
 *      p_nbytes - out
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
void dbe_trx_getrelhcardin(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        ss_int8_t* p_ntuples,
        ss_int8_t* p_nbytes)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_getrelhcardin, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(p_ntuples != NULL);
        ss_dassert(p_nbytes != NULL);

        dbe_trx_sementer(trx);

        dbe_trx_getrelhcardin_nomutex(
            trx,
            relh,
            p_ntuples,
            p_nbytes);

        dbe_trx_semexit(trx);
}

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              dbe_trx_getnewsavestmtid
 *
 * Gets a new save stmt id from counter contained in trx object
 *
 * Parameters :
 *
 *      trx - use
 *              trx handle
 *
 * Return value :
 *      new id for save stmt
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_trx_getnewsavestmtid(dbe_trx_t* trx)
{
        CHK_TRX(trx);
        trx->trx_savestmtctr++;
        return (trx->trx_savestmtctr);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getnewsyncmsgid
 *
 * Returns a new sync message id.
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
long dbe_trx_getnewsyncmsgid(
        dbe_trx_t* trx)
{
        dbe_ret_t rc;
        long syncmsgid;

        CHK_TRX(trx);

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        syncmsgid = dbe_counter_getnewsyncmsgid(trx->trx_counter);

#ifdef DBE_REPLICATION
        if (!trx->trx_replicaslave) {
            dbe_trx_initrepparams(trx, REP_INC_SYNCMSGID);

            rc = dbe_trx_replicate(trx, REP_INC_SYNCMSGID);
            if (rc != DBE_RC_SUCC && rc != DB_RC_NOHSB) {
                ss_dassert(rc != DBE_RC_CONT);
                dbe_db_exitaction(trx->trx_db, trx->trx_cd);
                return(rc);
            }
            rc = DBE_RC_SUCC;
        }
#endif /* DBE_REPLICATION */

        if (trx->trx_log != NULL) {
            rc = dbe_log_putincsysctr(
                    trx->trx_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_SYNCMSGID);
        }

        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(syncmsgid);
}

/*##**********************************************************************\
 *
 *              dbe_trx_incsyncmsgid
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
void dbe_trx_incsyncmsgid(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        dbe_counter_getnewsyncmsgid(trx->trx_counter);

        if (trx->trx_log != NULL) {
            dbe_ret_t rc;
            rc = dbe_log_putincsysctr(
                    trx->trx_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_SYNCMSGID);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getnewsyncnodeid
 *
 * Returns a new sync node id.
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
long dbe_trx_getnewsyncnodeid(
        dbe_trx_t* trx)
{
        long syncnodeid;

        CHK_TRX(trx);

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        syncnodeid = dbe_counter_getnewkeyid(trx->trx_counter);
        if (trx->trx_log != NULL) {
            dbe_ret_t rc;
            rc = dbe_log_putincsysctr(
                    trx->trx_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_KEYID);
        }

        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(syncnodeid);
}

/*##**********************************************************************\
 * 
 *      dbe_trx_getnewreadlevel
 * 
 * Gets a new read level for a transaction. Initially read level is
 * not allocated for a transaction. First D-table access will allocate a 
 * read level. If read level is released in READ COMMITTED isolation level
 * then first D-table access will allocate a new read level.
 * 
 * Parameters : 
 * 
 *      trx - in, use
 *          
 *          
 *      entertrxgate - in 
 *          If TRUE enters gtrs mutex gate.
 *          
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_trx_getnewreadlevel(dbe_trx_t* trx, bool entertrxgate)
{
        CHK_TRX(trx);

        if (trx->trx_commitst == TRX_COMMITST_DONE) {
            ss_dprintf_3(("dbe_trx_getnewreadlevel:trx ended\n"));
            ss_dassert(trx->trx_errcode != DBE_RC_SUCC);
            return;
        }

        ss_dassert(!dbe_trxinfo_canremovereadlevel(trx->trx_info));
        ss_dassert(DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        if (trx->trx_usemaxreadlevel) {
            /* Do not use read level.
             */
            ss_dprintf_3(("dbe_trx_getnewreadlevel:trx_usemaxreadlevel\n"));
            ss_derror;
            return;
        }

        ss_dprintf_3(("dbe_trx_getnewreadlevel:allocate read level\n"));
        ss_dassert(dbe_cfg_relaxedreadlevel || !entertrxgate);

        if (entertrxgate) {
            dbe_gtrs_entertrxgate(trx->trx_gtrs);
        }

        SU_BFLAG_SET(trx->trx_flags, TRX_FLAG_USEREADLEVEL);

        trx->trx_info->ti_maxtrxnum = dbe_counter_getmaxtrxnum(trx->trx_counter);
        trx->trx_searchtrxnum = trx->trx_info->ti_maxtrxnum;
        trx->trx_stmtsearchtrxnum = trx->trx_searchtrxnum;

        ss_dassert(!DBE_TRXNUM_ISNULL(trx->trx_info->ti_maxtrxnum));

        if (trx->trx_errcode == DBE_RC_SUCC) {
            dbe_gtrs_addtrxreadlevel_nomutex(trx->trx_gtrs, trx->trx_info);
        }

        if (entertrxgate) {
            dbe_gtrs_exittrxgate(trx->trx_gtrs);
        }
        if (trx->trx_mode == TRX_NOWRITES
            || trx->trx_mode == TRX_READONLY) 
        {
            dbe_trxnum_t storagetrxnum;
            storagetrxnum = dbe_counter_getstoragetrxnum(trx->trx_counter);
            ss_dprintf_3(("dbe_trx_getsearchtrxnum:storagetrxnum=%ld, trx->trx_searchtrxnum=%ld, trx->trx_hsbg2mode=%d\n", 
                DBE_TRXNUM_GETLONG(storagetrxnum), DBE_TRXNUM_GETLONG(trx->trx_searchtrxnum),
                trx->trx_hsbg2mode));
            if (DBE_TRXNUM_CMP_EX(storagetrxnum, trx->trx_searchtrxnum) >= 0
                && trx->trx_hsbg2mode != DBE_HSB_SECONDARY) 
            {
                /* All data visible to this transaction is in storage tree, use only
                 * storage tree.
                 */
                ss_dprintf_3(("dbe_trx_getsearchtrxnum:set RS_SYSI_FLAG_STORAGETREEONLY\n"));
                rs_sysi_setflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
            } else {
                ss_dprintf_3(("dbe_trx_getsearchtrxnum:clear RS_SYSI_FLAG_STORAGETREEONLY\n"));
                rs_sysi_clearflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY);
            }
        } else {
            ss_dassert(!rs_sysi_testflag(trx->trx_cd, RS_SYSI_FLAG_STORAGETREEONLY));
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getnewsynctupleversion
 *
 * Returns a new sync tuple version.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      cd -
 *
 *
 *      atype -
 *
 *
 *      aval -
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
dbe_ret_t dbe_trx_getnewsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool succp;
        rs_tuplenum_t synctupleversion;
        dbe_ret_t rc;

        CHK_TRX(trx);

        dbe_trx_ensurereadlevel(trx, TRUE);

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = trx->trx_errcode;
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        synctupleversion = dbe_gtrs_getnewsynctuplevers(
                                trx->trx_gtrs,
                                trx->trx_info);

#ifdef DBE_REPLICATION
        if (!trx->trx_replicaslave) {

            dbe_trx_initrepparams(trx, REP_INC_SYNCTUPLEVERSION);

            rc = dbe_trx_replicate(trx, REP_INC_SYNCTUPLEVERSION);
            ss_dassert(rc != DBE_RC_CONT);
            if (rc == DB_RC_NOHSB) {
                rc = DBE_RC_SUCC;
            }
        }
#endif /* DBE_REPLICATION */

        if (trx->trx_log != NULL) {
            rc = dbe_log_putincsysctr(
                    trx->trx_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_SYNCTUPLEVERSION);
        }

        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        succp = rs_tuplenum_setintoaval(
                    &synctupleversion,
                    cd,
                    atype,
                    aval);
        ss_dassert(succp);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_incsynctupleversion
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
void dbe_trx_incsynctupleversion(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        dbe_counter_getnewsynctupleversion(trx->trx_counter);

        if (trx->trx_log != NULL) {
            dbe_ret_t rc;
            rc = dbe_log_putincsysctr(
                    trx->trx_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_SYNCTUPLEVERSION);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getminsynctupleversion
 *
 * Returns minimum sync tuple version that is visible to calling
 * transaction.
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      cd -
 *
 *
 *      atype -
 *
 *
 *      aval -
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
void dbe_trx_getminsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool succp;
        rs_tuplenum_t synctupleversion;

        CHK_TRX(trx);

        dbe_trx_ensurereadlevel(trx, TRUE);

        synctupleversion = dbe_gtrs_getminsynctuplevers(trx->trx_gtrs, trx->trx_info);
        succp = rs_tuplenum_setintoaval(
                    &synctupleversion,
                    cd,
                    atype,
                    aval);
        ss_dassert(succp);
}

/*##**********************************************************************\
 *
 *              dbe_trx_getfirstsynctupleversion
 *
 * Returns the first sync tuple version that is used by some transaction.
 * This is the history cleanup level.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      cd -
 *
 *
 *      atype -
 *
 *
 *      aval -
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
void dbe_trx_getfirstsynctupleversion(
        dbe_trx_t* trx,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool succp;
        rs_tuplenum_t synctupleversion;

        CHK_TRX(trx);

        dbe_trx_ensurereadlevel(trx, TRUE);

        synctupleversion = dbe_gtrs_getfirstsynctuplevers(trx->trx_gtrs);
        succp = rs_tuplenum_setintoaval(
                    &synctupleversion,
                    cd,
                    atype,
                    aval);
        ss_dassert(succp);
}

#endif /* SS_SYNC */

#ifdef DBE_HSB_REPLICATION

#endif /* DBE_HSB_REPLICATION */

/*##**********************************************************************\
 *
 *              dbe_trx_gethsbctx
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
void* dbe_trx_gethsbctx(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_hsbctx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_sethsbctx
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      ctx -
 *
 *
 *      hsbctxfuns -
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
void dbe_trx_sethsbctx(
        dbe_trx_t* trx,
        void* ctx,
        dbe_hsbctx_funs_t* hsbctxfuns)
{
        CHK_TRX(trx);
        ss_dassert(trx->trx_hsbctx == NULL);
        ss_dassert(trx->trx_hsbctxfuns == NULL);

        trx->trx_hsbctx = ctx;
        trx->trx_hsbctxfuns = hsbctxfuns;
}

/*##**********************************************************************\
 *
 *              dbe_trx_gethsbsqlctx
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
void* dbe_trx_gethsbsqlctx(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(trx->trx_hsbsqlctx);
}

/*##**********************************************************************\
 *
 *              dbe_trx_sethsbsqlctx
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      sqlctx -
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
void dbe_trx_sethsbsqlctx(
        dbe_trx_t* trx,
        void* sqlctx)
{
        CHK_TRX(trx);
        ss_dassert(trx->trx_hsbsqlctx == NULL);
        ss_dassert(trx->trx_hsbctxfuns != NULL);

        trx->trx_hsbsqlctx = sqlctx;
}

/*##**********************************************************************\
 *
 *              dbe_trx_initrepparams
 *
 * Inits base parameter values.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      type -
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
bool dbe_trx_initrepparams(
        dbe_trx_t* trx,
        rep_type_t type)
{
        static long rp_id_ctr = 0L;
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_initrepparams, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (!trx->trx_rp.rp_activep) {
            memset(&trx->trx_rp, '\0', sizeof(trx->trx_rp));
            ss_debug(trx->trx_rp.rp_chk = DBE_CHK_REPPARAMS;)
            trx->trx_rp.rp_rc = DBE_RC_SUCC;
            trx->trx_rp.rp_trxid = trx->trx_usertrxid;
            trx->trx_rp.rp_stmttrxid = trx->trx_stmttrxid;
            SsSemEnter(ss_lib_sem);
            trx->trx_rp.rp_id = rp_id_ctr++;
            SsSemExit(ss_lib_sem);
            ss_dprintf_4(("dbe_trx_initrepparams, stmttrxid=%ld, trxid=%ld, rp_id=%ld\n",
                          DBE_TRXID_GETLONG(trx->trx_stmttrxid),
                          DBE_TRXID_GETLONG(trx->trx_usertrxid),
                          trx->trx_rp.rp_id));
            trx->trx_rp.rp_cd = trx->trx_cd;
            trx->trx_rp.rp_type = type;
            trx->trx_rp.rp_isddl = (trx->trx_trdd != NULL);
            return(TRUE);
        } else {
            if (trx->trx_rp.rp_donep
            &&  type != trx->trx_rp.rp_type
            &&  (type == REP_ABORT || type == REP_STMTABORT)
            &&  trx->trx_rp.rp_rc == DBE_RC_SUCC)
            {
                trx->trx_rp.rp_activep = FALSE;
                return(dbe_trx_initrepparams(trx, type));
            } else {
                return(FALSE);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_getrepparams
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
rep_params_t* dbe_trx_getrepparams(
        dbe_trx_t* trx)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_getrepparams, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_initrepparams(trx, REP_PING);

        return(&trx->trx_rp);
}

/*##**********************************************************************\
 *
 *              dbe_trx_replicate
 *
 * HSB replicate function for trancation. Does necessary cleanup after
 * the call.
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      type -
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
su_ret_t dbe_trx_replicate(
        dbe_trx_t* trx,
        rep_type_t type)
{
        su_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_replicate, userid = %d\n", dbe_user_getid(trx->trx_user)));

        rc = dbe_db_replicate(trx->trx_db, type, &trx->trx_rp);
        if (rc != DBE_RC_CONT) {
            trx->trx_rp.rp_type = 0;
        }
        return(rc);
}

#ifdef SS_FAKE
void dbe_trx_semexit_fake(dbe_trx_t* trx, bool dofake)
{
    if (dofake) {
        dofake = !SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE);
        if (!dofake) {
            /* ss_dprintf_1(("FAKE_HSBG2_TRXSETFAILED_ATTRXSEMENTER_D:skipped if in actiongate\n")); */
            return;
        }
    }
    if (dofake && !(trx->trx_mode == TRX_NOWRITES || trx->trx_mode == TRX_READONLY || trx->trx_mode == TRX_NOCHECK)) {

        FAKE_CODE_RESET(FAKE_HSBG2_TRXSETFAILED_ATTRXSEMENTER_D,
        {   bool retp;
            retp = dbe_trx_setfailed(trx, DBE_ERR_HSBABORTED, !SsSemStkFind(SS_SEMNUM_DBE_DB_ACTIONGATE));
            ss_dprintf_1(("FAKE_HSBG2_TRXSETFAILED_ATTRXSEMENTER_D:trx_mode %d, succp %d\n", trx->trx_mode, retp));
        });
    }
}
#else /* SS_FAKE */
void dbe_trx_semexit_fake(
        dbe_trx_t* trx __attribute__ ((unused)),
        bool dofake __attribute__ ((unused)))
{
    ss_derror;
}

#endif /* SS_FAKE */


#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *              dbe_trx_semisentered
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
bool dbe_trx_semisentered(dbe_trx_t* trx)
{
        CHK_TRX(trx);

#ifdef SS_MYSQL
        return((trx)->trx_thrid == SsThrGetid());
#else
        return(rs_sysi_isinsidedbeatomicsection(trx->trx_cd) || SsSemThreadIsEntered(trx->trx_sem));
#endif
}
/*##**********************************************************************\
 *
 *              dbe_trx_semisnotentered
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
bool dbe_trx_semisnotentered(dbe_trx_t* trx)
{
        CHK_TRX(trx);

        return(SsSemThreadIsNotEntered(trx->trx_sem));
}
#endif /* SS_DEBUG */

dbe_ret_t dbe_trx_readrelh(
        dbe_trx_t*      trx,
        ulong           relid)
{
        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_readrelh\n"));

        dbe_trx_ensurereadlevel(trx, TRUE);

        return dbe_gtrs_readrelh(
                trx->trx_gtrs,
                relid,
                dbe_trx_getsearchtrxnum(trx));
}

void dbe_trx_abortrelh(
        dbe_trx_t*      trx,
        ulong           relid)
{
        dbe_trxnum_t    trxnum;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_abortrelh\n"));

        trxnum = dbe_trx_getcommittrxnum(trx);
        ss_dassert(!DBE_TRXNUM_EQUAL(trxnum, DBE_TRXNUM_NULL));

        dbe_gtrs_abortrelh(trx->trx_gtrs, relid, trxnum);
}

