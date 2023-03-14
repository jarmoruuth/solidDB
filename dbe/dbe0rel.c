/*************************************************************************\
**  source       * dbe0rel.c
**  directory    * dbe
**  description  * Relation insert, update and delete operations.
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

This module implements the dabase engine interface to insert, delete and
update tuples. These functions are called from outside of the database
engine. For delete and update functions the deleted or updated tuple is
specified by a tuple reference. The tuple reference is returned from the
relation cursor functions.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

relation handle         rs0relh.c
tuple value             rs0tval.c
tuple type              rs0ttype.c
attribute type          rs0atype.c
attribute value         rs0aval.c

transaction object      dbe0trx.c
user object             dbe0user.c
database object         dbe0db.c
tuple reference         dbe0tref.c
tuple operations        dbe4tupl.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sspmon.h>

#include <su0error.h>
#include <su0prof.h>

#include <uti0va.h>

#include <rs0types.h>
#include <rs0error.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>

#include "dbe9type.h"
#include "dbe6srk.h"
#include "dbe5isea.h"
#include "dbe4srch.h"
#include "dbe4tupl.h"
#include "dbe0erro.h"
#include "dbe0tref.h"
#include "dbe0user.h"
#include "dbe1trx.h"
#include "dbe0trx.h"
#include "dbe0db.h"
#include "dbe0rel.h"

/*##**********************************************************************\
 *
 *		dbe_rel_insert
 *
 * Inserts a new tuple into relation.
 *
 * Parameters :
 *
 *	trx - in, use
 *          Transaction handle.
 *
 *	relh - in, use
 *          Relation handle.
 *
 *	tval - in, use
 *          Tuple value, must be of same type as relh.
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rel_insert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        su_profile_timer;

        ss_dprintf_1(("dbe_rel_insert:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(tval != NULL);
        SS_PUSHNAME("dbe_rel_insert");

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        dbe_db_enteraction(db, cd);

        su_profile_start;

        if (!rs_relh_islogged(cd, relh)) {
            rc = dbe_trx_markwrite_nolog(trx, TRUE);
        } else {
            rc = dbe_trx_markwrite(trx, TRUE);
        }
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(db, cd);
            su_profile_stop("dbe_rel_insert");
            rs_error_create(p_errh, dbe_trx_geterrcode(trx), "");
            SS_POPNAME;
            return(dbe_trx_geterrcode(trx));
        }

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_insert(
                    cd,
                    trx,
                    relh,
                    tval);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_insert_disk(
                    cd,
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    tval,
                    DBE_TUPLEINSERT_NORMAL);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
                SS_PMON_ADD(SS_PMON_DBEINSERT);
                /* FALLTHROUGH */
            case DBE_RC_WAITLOCK:
            case DBE_RC_CONT:
                break;
            default:
                SS_PMON_ADD(SS_PMON_DBEINSERT);
                dbe_trx_error_create(trx, rc, p_errh);
                break;
        }

        su_profile_stop("dbe_rel_insert");
        ss_dprintf_2(("dbe_rel_insert:end\n"));
        SS_POPNAME;

        return(rc);
}

#ifdef DBE_REPLICATION

dbe_ret_t dbe_rel_replicainsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;

        ss_dprintf_1(("dbe_rel_replicainsert:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(tval != NULL);
        SS_PUSHNAME("dbe_rel_replicainsert");

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        if (!dbe_db_setchanged(db, p_errh)) {
            SS_POPNAME;
            return(DBE_ERR_DBREADONLY);
        }

        dbe_db_enteraction(db, cd);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_insert(
                    dbe_trx_getcd(trx),
                    trx,
                    relh,
                    tval);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_insert_disk(
                    dbe_trx_getcd(trx),
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    tval,
                    DBE_TUPLEINSERT_REPLICA);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
            case DBE_RC_WAITLOCK:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        ss_dprintf_2(("dbe_rel_replicainsert:end\n"));
        SS_POPNAME;

        return(rc);
}

#endif /* DBE_REPLICATION */

dbe_ret_t dbe_rel_quickinsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;

        ss_dprintf_1(("dbe_rel_quickinsert:userid = %d\n",
            dbe_user_getid(dbe_trx_getuser(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(tval != NULL);
        SS_PUSHNAME("dbe_rel_quickinsert");

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        if (!dbe_db_setchanged(db, p_errh)) {
            SS_POPNAME;
            return(DBE_ERR_DBREADONLY);
        }

        dbe_db_enteraction(db, cd);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_insert(
                    dbe_trx_getcd(trx),
                    trx,
                    relh,
                    tval);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_insert_disk(
                    dbe_trx_getcd(trx),
                    trx,
                    DBE_TRXID_NULL,
                    relh,
                    tval,
                    DBE_TUPLEINSERT_QUICK);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
            case DBE_RC_WAITLOCK:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        ss_dprintf_2(("dbe_rel_quickinsert:end\n"));
        SS_POPNAME;

        return(rc);
}

#ifdef SS_BLOCKINSERT
dbe_ret_t dbe_rel_blockinsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        int blockindex,
        rs_err_t** p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;

        ss_dprintf_1(("dbe_rel_blockinsert:userid = %d\n",
            dbe_user_getid(dbe_trx_getuser(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(tval != NULL);
        SS_PUSHNAME("dbe_rel_blockinsert");

        ss_error;

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        if (!dbe_db_setchanged(db, p_errh)) {
            SS_POPNAME;
            return(DBE_ERR_DBREADONLY);
        }

        dbe_db_enteraction(db, cd);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_insert(
                    dbe_trx_getcd(trx),
                    trx,
                    relh,
                    tval);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_insert_disk(
                    dbe_trx_getcd(trx),
                    trx,
                    0L,
                    relh,
                    tval,
                    DBE_TUPLEINSERT_BLOCK);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
            case DBE_RC_WAITLOCK:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        ss_dprintf_2(("dbe_rel_blockinsert:end\n"));
        SS_POPNAME;

        return(rc);
}
#endif /* SS_BLOCKINSERT */

/*##**********************************************************************\
 *
 *		dbe_rel_delete
 *
 * Deletes a tuple from a relation.
 *
 * Parameters :
 *
 *	trx - in, use
 *          Transaction handle.
 *
 *	relh - in, use
 *          Relation handle.
 *
 *	tref - in, use
 *          Tuple reference used to identify the tuple.
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_NOTFOUND
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rel_delete(
        dbe_trx_t*  trx,
        rs_relh_t*  relh,
        dbe_tref_t* tref,
        rs_err_t**  p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        su_profile_timer;

        ss_dprintf_1(("dbe_rel_delete:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(tref != NULL);
#ifdef SS_MME
        if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY) {
            ss_dassert(tref->tr_vtpl != NULL);
        }
#else
        ss_dassert(tref->tr_vtpl != NULL);
#endif
        SS_PUSHNAME("dbe_rel_delete");

        dbe_db_enteraction(db, cd);

        su_profile_start;

        if (!rs_relh_islogged(cd, relh)) {
            rc = dbe_trx_markwrite_nolog(trx, TRUE);
        } else {
            rc = dbe_trx_markwrite(trx, TRUE);
        }
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(db, cd);
            su_profile_stop("dbe_rel_delete");
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SS_POPNAME;
            return(dbe_trx_geterrcode(trx));
        }

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_delete(
                    cd,
                    trx,
                    relh,
                    tref,
                    NULL);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_delete_disk(
                    cd,
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    tref,
                    NULL);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
                SS_PMON_ADD(SS_PMON_DBEDELETE);
                if (rs_relh_isaborted(cd, relh)) {
                    dbe_trx_error_create(trx, E_DDOP, p_errh);
                    rc = E_DDOP;
                }
                /* FALLTHROUGH */
            case DBE_RC_WAITLOCK:
            case DBE_RC_CONT:
                break;
            default:
                SS_PMON_ADD(SS_PMON_DBEDELETE);
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_rel_delete");
        ss_dprintf_2(("dbe_rel_delete:end\n"));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_rel_update
 *
 * Updates a tuple in a relation.
 *
 * Parameters :
 *
 *	trx - in, use
 *          Transaction handle.
 *
 *	relh - in, use
 *          Relation handle.
 *
 *	old_tref - in, use
 *          Tuple reference used to identify the old tuple.
 *
 *	upd_attrs - in, use
 *          Boolean array to specify updated atributes, the values
 *          of updated attributes are found from upd_tval.
 *
 *	upd_tval - in out, use
 *          New tuple value, contains attributes that are updated,
 *          must be of same type as relh. As a side effect those
 *          attributes that are not updated are assigned the old
 *          attribute value.
 *
 *      new_tref - out, use
 *          The tuple reference of the new, updated tuple is
 *          returned here if non-NULL.
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_ERR_EXISTS
 *      DBE_ERR_NOTFOUND
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rel_update(
        dbe_trx_t*  trx,
        rs_relh_t*  relh,
        dbe_tref_t* old_tref,
        bool*       upd_attrs,
        rs_tval_t*  upd_tval,
        dbe_tref_t* new_tref,
        rs_err_t**  p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        su_profile_timer;

        ss_dprintf_1(("dbe_rel_update:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));

        db = dbe_trx_getdb(trx);
        cd = dbe_trx_getcd(trx);

        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(old_tref != NULL);
#ifdef SS_MME
        if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY) {
            ss_dassert(old_tref->tr_vtpl != NULL);
        }
#else
        ss_dassert(old_tref->tr_vtpl != NULL);
#endif
        ss_dassert(upd_attrs != NULL);
        ss_dassert(upd_tval != NULL);
        SS_PUSHNAME("dbe_rel_update");

        dbe_db_enteraction(db, cd);

        su_profile_start;

        if (!rs_relh_islogged(cd, relh)) {
            rc = dbe_trx_markwrite_nolog(trx, TRUE);
        } else {
            rc = dbe_trx_markwrite(trx, TRUE);
        }
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(db, cd);
            su_profile_stop("dbe_rel_update");
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SS_POPNAME;
            return(dbe_trx_geterrcode(trx));
        }

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_update(
                    cd,
                    trx,
                    relh,
                    old_tref,
                    upd_attrs,
                    upd_tval,
                    new_tref,
                    NULL);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_update_disk(
                    cd,
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    old_tref,
                    upd_attrs,
                    upd_tval,
                    new_tref,
                    NULL);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
                SS_PMON_ADD(SS_PMON_DBEDELETE);
                if (rs_relh_isaborted(cd, relh)) {
                    dbe_trx_error_create(trx, E_DDOP, p_errh);
                    rc = E_DDOP;
                }
                /* FALLTHROUGH */
            case DBE_RC_WAITLOCK:
            case DBE_RC_CONT:
                break;
            default:
                SS_PMON_ADD(SS_PMON_DBEDELETE);
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_rel_update");
        ss_dprintf_2(("dbe_rel_update:end\n"));
        SS_POPNAME;

        return(rc);
}
