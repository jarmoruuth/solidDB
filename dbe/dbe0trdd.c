/*************************************************************************\
**  source       * dbe0trdd.c
**  directory    * dbe
**  description  * Transaction data dictionary routines.
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

#define DBE_TRX_INTERNAL
#include "dbe7cfg.h"
#include "dbe6btre.h"
#include "dbe6log.h"
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
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0user.h"

/*##**********************************************************************\
 *
 *              dbe_trx_relinserted
 *
 *
 *
 * Parameters :
 *
 *      trx - in, use
 *
 *
 *      relname - in, use
 *
 *
 *      p_relh - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_relinserted(
        dbe_trx_t* trx,
        rs_entname_t* relname,
        rs_relh_t** p_relh)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_relinserted, userid = %d\n", dbe_user_getid(trx->trx_user)));

#ifndef SS_NODDUPDATE
        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            b = dbe_trdd_relinserted(trx->trx_trdd, relname, p_relh);
        } else {
            b = FALSE;
        }
        dbe_trx_semexit(trx);
        return(b);
#else
        return(FALSE);
#endif /* SS_NODDUPDATE */
}

/*##**********************************************************************\
 *
 *              dbe_trx_reldeleted
 *
 *
 *
 * Parameters :
 *
 *      trx - in, use
 *
 *
 *      relname - in, use
 *
 *
 * Return value :
 *
 *      TRUE    - is deleted
 *      FALSE   - not deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_reldeleted(dbe_trx_t* trx, rs_entname_t* relname)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_reldeleted, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(relname) != NULL);

#ifndef SS_NODDUPDATE
        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            b = dbe_trdd_reldeleted(trx->trx_trdd, relname);
        } else {
            b = FALSE;
        }
        dbe_trx_semexit(trx);
        return(b);
#else
        return(FALSE);
#endif /* SS_NODDUPDATE */
}

/*##**********************************************************************\
 *
 *              dbe_trx_indexinserted
 *
 *
 *
 * Parameters :
 *
 *      trx - in, use
 *
 *
 *      indexname - in, use
 *
 *
 *      p_relh - out, ref
 *
 *
 *      p_key - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_indexinserted(
        dbe_trx_t* trx,
        rs_entname_t* indexname,
        rs_relh_t** p_relh,
        rs_key_t** p_key)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_indexinserted, userid = %d\n", dbe_user_getid(trx->trx_user)));

#ifndef SS_NODDUPDATE
        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            b = dbe_trdd_indexinserted(trx->trx_trdd, indexname, p_relh, p_key);
        } else {
            b = FALSE;
        }
        dbe_trx_semexit(trx);
        return(b);
#else
        return(FALSE);
#endif /* SS_NODDUPDATE */
}

/*##**********************************************************************\
 *
 *              dbe_trx_viewinserted
 *
 *
 *
 * Parameters :
 *
 *      trx - in, use
 *
 *
 *      viewname - in, use
 *
 *
 *      p_viewh - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - is inserted
 *      FALSE   - not inserted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_viewinserted(
        dbe_trx_t* trx,
        rs_entname_t* viewname,
        rs_viewh_t** p_viewh)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_viewinserted, userid = %d\n", dbe_user_getid(trx->trx_user)));

#ifndef SS_NODDUPDATE
        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            b = dbe_trdd_viewinserted(trx->trx_trdd, viewname, p_viewh);
        } else {
            b = FALSE;
        }
        dbe_trx_semexit(trx);
        return(b);
#else
        return(FALSE);
#endif /* SS_NODDUPDATE */
}

/*##**********************************************************************\
 *
 *              dbe_trx_viewdeleted
 *
 *
 *
 * Parameters :
 *
 *      trx - in, use
 *
 *
 *      viewname - in, use
 *
 *
 * Return value :
 *
 *      TRUE    - is deleted
 *      FALSE   - not deleted
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_trx_viewdeleted(dbe_trx_t* trx, rs_entname_t* viewname)
{
        bool b;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_viewdeleted, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(viewname) != NULL);

#ifndef SS_NODDUPDATE
        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            b = dbe_trdd_viewdeleted(trx->trx_trdd, viewname);
        } else {
            b = FALSE;
        }
        dbe_trx_semexit(trx);
        return(b);
#else
        return(FALSE);
#endif /* SS_NODDUPDATE */
}

#ifndef SS_NODDUPDATE

/*#***********************************************************************\
 *
 *              trx_createtrddif
 *
 * Creates trx->trx_trdd object, if it does not already exist.
 *
 * Parameters :
 *
 *      trx - use
 *              Transaction object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void trx_createtrddif(dbe_trx_t* trx)
{
        ss_dprintf_3(("trx_createtrddif\n"));
        ss_dassert(dbe_trx_semisentered(trx));

        dbe_trx_ensurereadlevel(trx, TRUE);

        if (trx->trx_trdd == NULL) {
            trx->trx_trdd = dbe_trdd_init(
                                trx->trx_cd,
                                trx->trx_db,
                                trx,
                                trx->trx_usertrxid,
                                trx->trx_stmttrxid,
                                trx->trx_log);
        }
}

/*##**********************************************************************\
 *
 *              dbe_trx_insertrel
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_insertrel(
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_insertrel, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putcreatetable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_CREATETABLE_FULLYQUALIFIED,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_relh_entname(cd, relh),
                    su_pa_nelems(rs_relh_keys(cd, relh)),
                    rs_relh_nattrs(cd, relh));
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertrel(trx->trx_trdd, relh);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deleterel
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_deleterel(
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_deleterel, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }
        
        ss_dassert(!DBE_TRXID_ISNULL(trx->trx_stmttrxid));

#ifdef DBE_DDRECOV_BUGFIX
        trx->trx_delaystmtcommit = TRUE;
#else
        /* NOTE! Log file is updated from dbe1trdd.c when operation is
           completed. */
#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL) {
            void* cd;
            ss_derror;
            cd = trx->trx_cd;
            rc = dbe_log_putdroptable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_DROPTABLE,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_entname_getname(rs_relh_entname(cd, relh)));
        }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */

        dbe_trx_sementer(trx);
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_deleterel(trx->trx_trdd, relh);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*#***********************************************************************\
 *
 *              trx_addtruncaterefkeycheck
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t trx_addtruncaterefkeycheck(
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        rs_sysi_t* cd;
        su_pa_t* refkeys;
        rs_key_t* key;
        uint i;
        dbe_ret_t rc = DBE_RC_SUCC;

        cd = trx->trx_cd;

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        refkeys = rs_relh_refkeys(cd, relh);
        su_pa_do_get(refkeys, i, key) {
            /* On truncate we need to add checks against delete. */
            if (rs_key_type(cd, key) == RS_KEY_PRIMKEYCHK) {
                rc = dbe_trx_addrefkeycheck(
                        cd,
                        trx,
                        relh,
                        rs_relh_clusterkey(cd, relh),
                        key,
                        relh,
                        NULL,
                        NULL,
                        rs_relh_reltype(cd, relh));
                if (rc != DBE_RC_SUCC) {
                    break;
                }
            }
        }
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_truncaterel
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_truncaterel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        bool physdelete)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_truncaterel, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, FALSE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }
        
        ss_dassert(!DBE_TRXID_ISNULL(trx->trx_stmttrxid));

        trx->trx_delaystmtcommit = TRUE;

        dbe_trx_sementer(trx);
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_truncaterel(trx->trx_trdd, relh, physdelete);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        if (rc == DBE_RC_SUCC && !physdelete) {
            rc = trx_addtruncaterefkeycheck(trx, relh);
        }

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_alterrel
 *
 * Adds info that a relation has been altered to the trx.
 *
 * Parameters :
 *
 *      trx - use
 *              Transaction handle.
 *
 *      relh - in
 *              Relation handle.
 *
 *      add - in
 *              If > 0, this is add column. Otherwise this is drop column.
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_trx_alterrel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        int ncolumns)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_alterrel, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }
        
        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }
        
        dbe_trx_sementer(trx);

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putaltertable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_ALTERTABLE,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_entname_getname(rs_relh_entname(cd, relh)),
                    ncolumns);
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_alterrel(trx->trx_trdd, relh, ncolumns);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

int dbe_trx_newcolumncount(
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        int count;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_newcolumncount, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

        dbe_trx_sementer(trx);

        if (trx->trx_trdd != NULL) {
            count = dbe_trdd_newcolumncount(trx->trx_trdd, relh);
        } else {
            count = 0;
        }
        dbe_trx_semexit(trx);

        return(count);
}

/*##**********************************************************************\
 *
 *              dbe_trx_renamerel
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
dbe_ret_t dbe_trx_renamerel(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_entname_t* newname)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_renamerel, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }
        
        dbe_trx_sementer(trx);

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putrenametable(
                    trx->trx_log,
                    trx->trx_cd,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    newname);
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_renamerel(trx->trx_trdd, relh, newname);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_insertindex
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 *      key - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_insertindex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dassert(key != NULL);
        ss_dprintf_1(("dbe_trx_insertindex, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

#ifdef DBE_DDRECOV_BUGFIX
        trx->trx_delaystmtcommit = TRUE;
#else
        /* NOTE! Log file is updated from dbe1trdd.c when operation is
           completed. */
#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putcreateordropidx(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_CREATEINDEX,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_key_id(cd, key),
                    rs_entname_getname(rs_relh_entname(cd, relh)));
        }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */

        dbe_trx_sementer(trx);

        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertindex(trx->trx_trdd, relh, key);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deleteindex
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      relh - in, use
 *
 *
 *      key - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_deleteindex(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dassert(key != NULL);
        ss_dprintf_1(("dbe_trx_deleteindex, userid = %d relid = %d relh = %08x\n", dbe_user_getid(trx->trx_user), rs_relh_relid(trx->trx_cd, relh), relh));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

#ifdef DBE_DDRECOV_BUGFIX
        trx->trx_delaystmtcommit = TRUE;
#else
        /* NOTE! Log file is updated from dbe1trdd.c when operation is
           completed. */
#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putcreateordropidx(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_DROPINDEX,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_key_id(cd, key),
                    rs_entname_getname(rs_relh_entname(cd, relh)));
        }
#endif /* SS_NOLOGGING */
#endif /* DBE_DDRECOV_BUGFIX */
        dbe_trx_sementer(trx);
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_deleteindex(trx->trx_trdd, relh, key);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

#ifndef SS_NOSEQUENCE

/*##**********************************************************************\
 *
 *              dbe_trx_insertseq
 *
 * Adds create sequence mark to the log.
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
dbe_ret_t dbe_trx_insertseq(
        dbe_trx_t* trx,
        long seq_id,
        rs_entname_t* seq_name,
        bool densep)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_insertseq, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(seq_name) != NULL);

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);
        trx->trx_nlogwrites++;
        if (trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertseq(trx->trx_trdd, seq_name, seq_id, densep);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deleteseq
 *
 * Adds drop sequence mark to the log.
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
dbe_ret_t dbe_trx_deleteseq(
        dbe_trx_t* trx,
        long seq_id,
        rs_entname_t* seq_name,
        bool densep)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_deleteseq, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(seq_name) != NULL);

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);
        trx->trx_nlogwrites++;

        if (trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_deleteseq(trx->trx_trdd, seq_name, seq_id, densep);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(rc);
}

#endif /* SS_NOSEQUENCE */

/*##**********************************************************************\
 *
 *              dbe_trx_insertname
 *
 * Adds create <name> mark to the trx.
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_insertname(
        dbe_trx_t* trx,
        rs_entname_t* name)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_insertname, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(name) != NULL);

        dbe_trx_sementer(trx);
        if (trx->trx_errcode != DBE_RC_SUCC) {
            rc = trx->trx_errcode;
        } else {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertname(trx->trx_trdd, name);
        }
        dbe_trx_semexit(trx);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deletename
 *
 * Adds drop <name> mark to the trx.
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_deletename(
        dbe_trx_t* trx,
        rs_entname_t* name)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_deletename, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(name) != NULL);

        dbe_trx_sementer(trx);
        if (trx->trx_errcode != DBE_RC_SUCC) {
            rc = trx->trx_errcode;
        } else {
            trx_createtrddif(trx);
            rc = dbe_trdd_deletename(trx->trx_trdd, name);
        }
        dbe_trx_semexit(trx);

        return(rc);
}

bool dbe_trx_namedeleted(
        dbe_trx_t* trx,
        rs_entname_t* name)
{
        bool deletep;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_namedeleted, userid = %d\n", dbe_user_getid(trx->trx_user)));
        ss_dassert(rs_entname_getschema(name) != NULL);

        dbe_trx_sementer(trx);
        if (trx->trx_trdd != NULL) {
            deletep = dbe_trdd_namedeleted(trx->trx_trdd, name);
        } else {
            deletep = FALSE;
        }
        dbe_trx_semexit(trx);

        return(deletep);
}

/*##**********************************************************************\
 *
 *              dbe_trx_insertevent
 *
 * Adds create <event> mark to the trx.
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_insertevent(
        dbe_trx_t* trx,
        rs_event_t* event)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_insertevent, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);
        if (trx->trx_errcode != DBE_RC_SUCC) {
            rc = trx->trx_errcode;
        } else {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertevent(trx->trx_trdd, event);
        }
        dbe_trx_semexit(trx);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deleteevent
 *
 * Adds drop <event> mark to the trx.
 *
 * Parameters :
 *
 *      trx -
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
dbe_ret_t dbe_trx_deleteevent(
        dbe_trx_t* trx,
        rs_entname_t* eventname)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_deleteevent, userid = %d\n", dbe_user_getid(trx->trx_user)));

        dbe_trx_sementer(trx);
        if (trx->trx_errcode != DBE_RC_SUCC) {
            rc = trx->trx_errcode;
        } else {
            trx_createtrddif(trx);
            rc = dbe_trdd_deleteevent(trx->trx_trdd, eventname);
        }
        dbe_trx_semexit(trx);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_insertview
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      viewh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_insertview(
        dbe_trx_t* trx,
        rs_viewh_t* viewh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(viewh != NULL);
        ss_dprintf_1(("dbe_trx_insertview, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putcreatetable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED,
                    trx->trx_stmttrxid,
                    rs_viewh_viewid(cd, viewh),
                    rs_viewh_entname(cd, viewh),
                    0,
                    0);
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_insertview(trx->trx_trdd, viewh);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_deleteview
 *
 *
 *
 * Parameters :
 *
 *      trx - in out, use
 *
 *
 *      viewh - in, use
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_trx_deleteview(
        dbe_trx_t* trx,
        rs_viewh_t* viewh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(viewh != NULL);
        ss_dprintf_1(("dbe_trx_deleteview, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);

#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putdroptable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_DROPVIEW,
                    trx->trx_stmttrxid,
                    rs_viewh_viewid(cd, viewh),
                    rs_entname_getname(rs_viewh_entname(cd, viewh)));
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_deleteview(trx->trx_trdd, viewh);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_setrelhchanged
 *
 * Marks the relation handle as changed.
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
dbe_ret_t dbe_trx_setrelhchanged(
        dbe_trx_t* trx,
        rs_relh_t* relh)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dassert(relh != NULL);
        ss_dprintf_1(("dbe_trx_setrelhchanged, userid = %d\n", dbe_user_getid(trx->trx_user)));

#ifdef SS_MME
        if (rs_relh_reltype(trx->trx_cd, relh) == RS_RELTYPE_MAINMEMORY
        && dbe_db_ishsb(trx->trx_db)) {
            return (DBE_ERR_HSBMAINMEMORY);
        }
#endif /* SS_MME */

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        rc = dbe_trx_lockrelh(trx, relh, TRUE, 0);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);
#ifndef SS_NOLOGGING
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            void* cd;
            cd = trx->trx_cd;
            rc = dbe_log_putaltertable(
                    trx->trx_log,
                    trx->trx_cd,
                    DBE_LOGREC_ALTERTABLE,
                    trx->trx_stmttrxid,
                    rs_relh_relid(cd, relh),
                    rs_entname_getname(rs_relh_entname(cd, relh)),
                    (rs_ano_t)0);
        }
#endif /* SS_NOLOGGING */
        if (rc == DBE_RC_SUCC && trx->trx_errcode == DBE_RC_SUCC) {
            trx_createtrddif(trx);
            rc = dbe_trdd_setrelhchanged(trx->trx_trdd, relh);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_createuser
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
dbe_ret_t dbe_trx_createuser(dbe_trx_t* trx)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_createuser, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

#ifndef SS_NOLOGGING
        dbe_trx_sementer(trx);
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            rc = dbe_log_putcreateuser(
                    trx->trx_log,
                    DBE_LOGREC_CREATEUSER);
        }
        dbe_trx_semexit(trx);
#endif /* SS_NOLOGGING */
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_trx_logauditinfo
 *
 *
 *
 * Parameters :
 *
 *      trx -
 *
 *
 *      userid -
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
dbe_ret_t dbe_trx_logauditinfo(
        dbe_trx_t* trx,
        long userid,
        char* info)
{
        dbe_ret_t rc;

        CHK_TRX(trx);
        ss_dprintf_1(("dbe_trx_logauditinfo, userid = %d\n", dbe_user_getid(trx->trx_user)));

        if (trx->trx_errcode != DBE_RC_SUCC) {
            return(trx->trx_errcode);
        }

        dbe_db_enteraction(trx->trx_db, trx->trx_cd);

        rc = dbe_trx_markwrite(trx, TRUE);
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(trx->trx_db, trx->trx_cd);
            return(rc);
        }

        dbe_trx_sementer(trx);
        if (trx->trx_log != NULL && trx->trx_errcode == DBE_RC_SUCC) {
            rc = dbe_log_putauditinfo(
                    trx->trx_log,
                    trx->trx_cd,
                    trx->trx_stmttrxid,
                    userid,
                    info);
        }
        dbe_trx_semexit(trx);
        dbe_db_exitaction(trx->trx_db, trx->trx_cd);
        return(rc);
}

#endif /* SS_NODDUPDATE */
