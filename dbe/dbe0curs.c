/*************************************************************************\
**  source       * dbe0curs.c
**  directory    * dbe
**  description  * Relation cursor routines.
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

This module implements the relation cursor interface of database engine.
Functions in this module are called from outside of the dbe. The cursor
is used used to browse through the data tuples in a relation.

This level does the transaction handling.


Limitations:
-----------


Error handling:
--------------


Objects used:
------------

relation handle         rs0relh.c
tuple value             rs0tval.c

search routines         dbe4srch.c
user routines           dbe0user.c
transaction routines    dbe0trx.c
tuple reference         dbe0tref.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sspmon.h>

#include <su0error.h>
#include <su0list.h>
#include <su0prof.h>
#include <su0time.h>

#include <rs0error.h>
#include <rs0relh.h>
#include <rs0tval.h>
#include <rs0pla.h>

#include "dbe9type.h"
#include "dbe4srch.h"
#include "dbe4tupl.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0user.h"
#include "dbe0tref.h"
#include "dbe1trx.h"
#include "dbe0trx.h"
#include "dbe0curs.h"

/*##**********************************************************************\
 *
 *		dbe_cursor_init
 *
 * Initializes a cursor (starts a search).
 *
 * Parameters :
 *
 * 	trx - in, use
 *          Transaction handle.
 *
 * 	ttype - in, hold
 *          Type of the tuple value returned in dbe_cursor_next.
 *
 *      sellist - in, hold
 *          List of selected attributes, RS_ANO_NULL terminates the list.
 *
 *      plan - in, use
 *          The search plan.
 *
 *      cursor_type - in
 *          cursor type (SELECT, FOR UPDATE, searched UPDATE, searched DELETE)
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value - give :
 *
 *      pointer to the cursor
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cursor_t* dbe_cursor_init(
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        dbe_cursor_type_t   cursor_type,
        bool*               p_newplan,
        rs_err_t**          p_errh __attribute__ ((unused)))
{
        dbe_search_t* search;
        dbe_user_t* user;
        dbe_db_t* db;
        rs_sysi_t* cd;
        rs_relh_t* relh;
        su_profile_timer;

        ss_dprintf_1(("dbe_cursor_init:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        SS_PUSHNAME("dbe_cursor_init");

        su_profile_start;

        user = dbe_trx_getuser(trx);
        cd = dbe_user_getcd(user);
        relh = rs_pla_getrelh(cd, plan);

        rs_pla_link(cd, plan);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            search = (dbe_search_t*)dbe_mme_search_init(
                        cd,
                        user,
                        trx,
                        dbe_trx_getusertrxid(trx),
                        sellist,
                        plan,
                        NULL,
                        NULL,
                        cursor_type,
                        p_newplan);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            search = dbe_search_init_disk(
                        user,
                        trx,
                        dbe_trx_getsearchtrxnum(trx),
                        dbe_trx_getusertrxid(trx),
                        ttype,
                        sellist,
                        plan,
                        cursor_type,
                        p_newplan);
        }
        ss_dassert(search != NULL);

        db = dbe_trx_getdb(trx);

        if (rs_relh_reltype(cd, relh) != RS_RELTYPE_MAINMEMORY) {
            dbe_search_setmaxpoolblocks(
                    search,
                    (ulong)dbe_db_poolsizeforquery(db)
                      / (ulong)dbe_db_blocksize(db));
        }

        su_profile_stop("dbe_cursor_init");
        ss_dprintf_2(("dbe_cursor_init:end\n"));
        SS_POPNAME;

        return((dbe_cursor_t*)search);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_done
 *
 * Releases a cursor (stops the search).
 *
 * Parameters :
 *
 *	cursor - in, take
 *          Cursor pointer.
 *
 * 	trx - in, use
 *          Transaction handle.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cursor_done(
        dbe_cursor_t* cursor,
        dbe_trx_t* trx)
{
        su_profile_timer;

        if (trx != NULL) {
            ss_dprintf_1(("dbe_cursor_done:userid = %d, usertrxid = %ld\n",
                dbe_user_getid(dbe_trx_getuser(trx)),
                DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        } else {
            ss_dprintf_1(("dbe_cursor_done:trx = NULL\n"));
        }
        ss_dassert(cursor != NULL);
        SS_PUSHNAME("dbe_cursor_done");

        su_profile_start;

        if (trx != NULL && dbe_trx_needtoaddreadcheck(trx)) {
            bool search_activated;
            rs_pla_t* lastplan;
            dynvtpl_t lastkey = NULL;
            dbe_trxid_t lasttrxid;

            search_activated = dbe_search_getsearchinfo(
                                    (dbe_search_t*)cursor,
                                    &lastplan,
                                    &lastkey,
                                    &lasttrxid);
            if (search_activated) {
                dbe_trx_addreadcheck(trx, lastplan, lastkey, lasttrxid);
            }
        }

        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_mme_search_done((dbe_mme_search_t*)cursor);
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            dbe_search_done_disk((dbe_search_t*)cursor);
        }

        su_profile_stop("dbe_cursor_done");

        ss_dprintf_2(("dbe_cursor_done:end\n"));
        SS_POPNAME;
}

/*##**********************************************************************\
 * 
 *		dbe_cursor_getunique
 * 
 * Optimized version to get unique row directly from index.
 * 
 * Parameters : 
 * 
 *		trx - 
 *			
 *			
 *		ttype - 
 *			
 *			
 *		sellist - 
 *			
 *			
 *		plan - 
 *			
 *			
 *		tval - 
 *			
 *			
 *		p_errh - 
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
dbe_ret_t dbe_cursor_getunique(
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        rs_tval_t*          tval,
        dbe_bkey_t**        p_bkeybuf,
        rs_err_t**          p_errh)
{
        dbe_user_t*         user;
        rs_sysi_t*          cd;
        dbe_ret_t           perm_rc;
        dbe_ret_t           bonsai_rc;
        dbe_ret_t           ret_rc;
        dbe_index_t*        index;
        dbe_bkeyinfo_t*     bkeyinfo;
        dbe_bkey_t*         kb;
        dbe_bkey_t*         ke;
        dbe_bkey_t*         bonsaikey;
        dbe_ret_t           tmp_rc;
        long                keyid;
        bool                deletenext;
        dbe_info_t          perm_info;
        dbe_info_t          bonsai_info;
        dbe_btree_lockinfo_t  perm_lockinfo;
        dbe_btrsea_timecons_t tc;         /* Time range constraints. */
        rs_relh_t*            relh;
        su_profile_timer;

        user = dbe_trx_getuser(trx);
        cd = dbe_user_getcd(user);

        ss_dprintf_1(("dbe_cursor_getunique:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(user),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        SS_PUSHNAME("dbe_cursor_getunique");
        su_profile_start;

        relh = rs_pla_getrelh(cd, plan);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
            ss_dprintf_1(("dbe_cursor_getunique:DBE_RC_NOTFOUND, mainmemory\n"));
            SS_POPNAME;
            return(DBE_RC_NOTFOUND);
        }

        dbe_trx_ensurereadlevel(trx, TRUE);

        if (!rs_pla_usingclusterkey(cd, plan)) {
            ss_dprintf_1(("dbe_cursor_getunique:DBE_RC_NOTFOUND, not clustering key\n"));
            SS_POPNAME;
            return(DBE_RC_NOTFOUND);
        }
        if (dbe_trx_needtoaddreadcheck(trx)) {
            ss_dprintf_1(("dbe_cursor_getunique:DBE_RC_NOTFOUND, needs readcheck\n"));
            SS_POPNAME;
            return(DBE_RC_NOTFOUND);
        }

        dbe_trx_setflag(trx, TRX_FLAG_DTABLE);

        index = dbe_user_getindex(user);
        bkeyinfo = dbe_index_getbkeyinfo(index);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_PESSIMISTIC) {
            tc.tc_maxtrxnum = dbe_trx_getstmtsearchtrxnum(trx);
        } else {
            tc.tc_maxtrxnum = dbe_trx_getsearchtrxnum(trx);
        }
        tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        tc.tc_usertrxid = dbe_trx_getusertrxid(trx);
        tc.tc_maxtrxid = dbe_trx_getreadtrxid(trx);
        tc.tc_trxbuf = dbe_index_gettrxbuf(index);

        kb = dbe_bkey_initpermleaf(cd, bkeyinfo, rs_pla_get_range_start_vtpl(cd, plan));
        ke = dbe_bkey_initpermleaf(cd, bkeyinfo, rs_pla_get_range_end_vtpl(cd, plan));
        if (*p_bkeybuf == NULL) {
            *p_bkeybuf = dbe_bkey_init_ex(cd, bkeyinfo);
        }
        bonsaikey = dbe_bkey_init_ex(cd, bkeyinfo);

        dbe_info_init(perm_info, 0);
        dbe_info_init(bonsai_info, 0);
        perm_info.i_btreelockinfo = &perm_lockinfo;

        keyid = rs_pla_getkeyid(cd, plan);

        dbe_index_mergegate_enter_shared(index, keyid);

        perm_rc = dbe_btree_getunique(
                        dbe_index_getpermtree(index),
                        kb,
                        ke,
                        &tc,
                        *p_bkeybuf,
                        NULL,
                        &perm_info);

        if (rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY)) {
            bonsai_rc = DBE_RC_END;
        } else {
            bonsai_rc = dbe_btree_getunique(
                            dbe_index_getbonsaitree(index),
                            kb,
                            ke,
                            &tc,
                            bonsaikey,
                            &deletenext,
                            &bonsai_info);
        }

        ss_dprintf_2(("dbe_cursor_getunique:perm_rc=%s, bonsai_rc=%s\n", su_rc_nameof(perm_rc), su_rc_nameof(bonsai_rc)));

        dbe_btree_lockinfo_unlock(&perm_info, dbe_index_getpermtree(index));
        dbe_index_mergegate_exit(index, keyid);

        if (bonsai_rc == DBE_RC_FOUND) {
            if (deletenext) {
                ret_rc = DBE_RC_END;
            } else {
                ret_rc = DBE_RC_FOUND;
                dbe_bkey_done_ex(cd, *p_bkeybuf);
                *p_bkeybuf = bonsaikey;
                bonsaikey = NULL;
            }
        } else if (bonsai_rc == DBE_RC_NOTFOUND || perm_rc == DBE_RC_NOTFOUND) {
            ret_rc = DBE_RC_NOTFOUND;
        } else if (perm_rc == DBE_RC_FOUND) {
            ret_rc = DBE_RC_FOUND;
        } else {
            ret_rc = DBE_RC_END;
        }

        if (ret_rc == DBE_RC_FOUND) {
            ss_dprintf_2(("dbe_cursor_getunique:call dbe_bkey_setbkeytotval\n"));
            tmp_rc = dbe_bkey_setbkeytotval(
                        cd,
                        rs_pla_getkey(cd, plan),
                        *p_bkeybuf,
                        ttype,
                        tval);
            ss_rc_dassert(tmp_rc == DBE_RC_FOUND, tmp_rc);
        }

        if (ret_rc == DBE_RC_NOTFOUND) {
            SS_PMON_ADD(SS_PMON_DBEFETCHUNIQUENOTFOUND);
        } else {
            su_rc_dassert(ret_rc == DBE_RC_FOUND || ret_rc == DBE_RC_END, ret_rc);
            SS_PMON_ADD(SS_PMON_DBEFETCH);
            SS_PMON_ADD(SS_PMON_DBEFETCHUNIQUEFOUND);
        }

        ss_dprintf_2(("dbe_cursor_getunique:ret_rc=%s\n", su_rc_nameof(ret_rc)));

        dbe_bkey_done_ex(cd, kb);
        dbe_bkey_done_ex(cd, ke);
        if (bonsaikey != NULL) {
            dbe_bkey_done_ex(cd, bonsaikey);
        }

        su_profile_stop("dbe_cursor_getunique");
        SS_POPNAME;

        return(ret_rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_reset
 *
 * Resets a cursor (starts a new search).
 *
 * Parameters :
 *
 *	cursor - in, use
 *          Cursor pointer.
 *
 * 	trx - in, use
 *          Transaction handle.
 *
 * 	ttype - in, hold
 *          Type of the tuple value returned in dbe_cursor_next.
 *
 *      sellist - in, hold
 *          List of selected attributes, RS_ANO_NULL terminates the list.
 *
 *      plan - in, use
 *          The search plan.
 *
 * Return value - give :
 *
 *      pointer to the cursor
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cursor_reset(
        dbe_cursor_t*       cursor,
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan)
{
        dbe_user_t*         user;
        rs_sysi_t*          cd;
        rs_relh_t*          relh;
        su_profile_timer;

        user = dbe_trx_getuser(trx);
        cd = dbe_user_getcd(user);
        relh = rs_pla_getrelh(cd, plan);

        ss_dprintf_1(("dbe_cursor_reset:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(user),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        SS_PUSHNAME("dbe_cursor_reset");
        su_profile_start;

        if (dbe_trx_needtoaddreadcheck(trx)) {
            bool search_activated;
            rs_pla_t* lastplan;
            dynvtpl_t lastkey = NULL;
            dbe_trxid_t lasttrxid;

#ifdef SS_MME_FOO
            if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
                search_activated = FALSE;
            } else {
                search_activated = dbe_search_getsearchinfo(
                        (dbe_search_t*)cursor,
                        &lastplan,
                        &lastkey,
                        &lasttrxid);
                rs_pla_link(cd, plan);
            }
#else
            search_activated = dbe_search_getsearchinfo(
                    (dbe_search_t*)cursor,
                    &lastplan,
                    &lastkey,
                    &lasttrxid);
#endif
            if (search_activated) {
                dbe_trx_addreadcheck(trx, lastplan, lastkey, lasttrxid);
            }
        }

        rs_pla_link(cd, plan);

        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_reset(
                (dbe_mme_search_t*)cursor,
                trx,
                plan);
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            dbe_search_reset_disk(
                (dbe_search_t*)cursor,
                trx,
                dbe_trx_getsearchtrxnum(trx),
                dbe_trx_getusertrxid(trx),
                ttype,
                sellist,
                plan);
        }

        su_profile_stop("dbe_cursor_reset");
        SS_POPNAME;
}

dbe_ret_t dbe_cursor_reset_fetch(
        dbe_cursor_t*       cursor,
        dbe_trx_t*          trx,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        rs_tval_t**         p_tval,
        rs_err_t**          p_errh)
{
        dbe_user_t*         user;
        rs_sysi_t*          cd;
        dbe_ret_t           rc;
        su_profile_timer;

        user = dbe_trx_getuser(trx);
        cd = dbe_user_getcd(user);

        ss_dprintf_1(("dbe_cursor_reset_fetch:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(user),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        SS_PUSHNAME("dbe_cursor_reset_fetch_fetch");
        su_profile_start;

        if (dbe_trx_needtoaddreadcheck(trx)) {
            bool search_activated;
            rs_pla_t* lastplan;
            dynvtpl_t lastkey = NULL;
            dbe_trxid_t lasttrxid;

            search_activated = dbe_search_getsearchinfo(
                    (dbe_search_t*)cursor,
                    &lastplan,
                    &lastkey,
                    &lasttrxid);
        }

        rs_pla_link(cd, plan);

        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_reset(
                (dbe_mme_search_t*)cursor,
                trx,
                plan);
            rc = DBE_RC_NOTFOUND;
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_search_reset_disk_fetch(
                    (dbe_search_t*)cursor,
                    trx,
                    dbe_trx_getsearchtrxnum(trx),
                    dbe_trx_getusertrxid(trx),
                    ttype,
                    sellist,
                    plan,
                    p_tval);
        }

        switch (rc) {
            case DBE_RC_FOUND:
                SS_PMON_ADD(SS_PMON_DBEFETCH);
                /* FALLTHROUGH */
            case DBE_RC_NOTFOUND:
            case DBE_RC_WAITLOCK:
            case DBE_RC_END:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_cursor_reset_fetch");
        SS_POPNAME;

        return(rc);
}

void dbe_cursor_setisolation_transparent(
        dbe_cursor_t* cursor,
        bool          transparent)
{
        dbe_search_setisolation_transparent(
                (dbe_search_t*)cursor,
                transparent);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_nextorprev
 *
 * Returns the next or previous cursor value.
 *
 * Parameters :
 *
 *	nextp -
 *
 *
 *	cursor -
 *
 *
 *	trx -
 *
 *
 *	p_tval -
 *
 *
 *	p_errh -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_cursor_nextorprev(
        dbe_cursor_t* cursor,
        bool          nextp,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh)
{
        dbe_ret_t rc;
        su_profile_timer;

        SS_NOTUSED(p_errh);

        ss_dprintf_1(("dbe_cursor_%s:userid = %d, usertrxid = %ld\n",
            nextp ? "next" : "prev",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_DBE_CURSORFETCH);

        if (dbe_trx_isfailed(trx)) {
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CURSORFETCH);
            return(dbe_trx_geterrcode(trx));
        }

        su_profile_start;

        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            if (nextp) {
                rc = dbe_mme_search_next(
                        (dbe_mme_search_t*)cursor,
                        trx,
                        p_tval);
            } else {
                rc = dbe_mme_search_prev(
                        (dbe_mme_search_t*)cursor,
                        trx,
                        p_tval);
            }
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_search_nextorprev_disk(
                    (dbe_search_t*)cursor,
                    trx,
                    nextp,
                    p_tval);
        }

        switch (rc) {
            case DBE_RC_FOUND:
                SS_PMON_ADD(SS_PMON_DBEFETCH);
                /* FALLTHROUGH */
            case DBE_RC_NOTFOUND:
            case DBE_RC_WAITLOCK:
            case DBE_RC_END:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_cursor_nextorprev");
        ss_dprintf_2(("dbe_cursor_%s:end\n", nextp ? "next" : "prev"));
        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CURSORFETCH);

        return(rc);
}

dbe_ret_t dbe_cursor_relock(
        dbe_cursor_t*   cursor,
        dbe_trx_t*      trx,
        rs_err_t**      p_errh)
{
        dbe_ret_t       rc;
        
        rc = dbe_mme_search_relock((dbe_mme_search_t *) cursor, trx);

        switch (rc) {
            case SU_SUCCESS:
            case DBE_RC_WAITLOCK:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        return rc;
}

dbe_ret_t dbe_cursor_nextorprev_n(
        dbe_cursor_t*   cursor,
        bool            nextp,
        dbe_trx_t*      trx,
        rs_vbuf_t*      vb,
        rs_err_t**      p_errh)
{
        dbe_ret_t rc;
        su_profile_timer;

        SS_NOTUSED(p_errh);

        ss_dprintf_1(("dbe_cursor_%s_n:userid = %d, usertrxid = %ld\n",
            nextp ? "next" : "prev",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        SU_GENERIC_TIMER_START(SU_GENERIC_TIMER_DBE_CURSORFETCH);

        if (dbe_trx_isfailed(trx)) {
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CURSORFETCH);
            return(dbe_trx_geterrcode(trx));
        }

        su_profile_start;

        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            rc = dbe_mme_search_nextorprev_n(
                           (dbe_mme_search_t*)cursor,
                           trx,
                           nextp,
                           vb);
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            rc = DBE_ERR_FAILED;
            ss_error;
        }

        switch (rc) {
            case DBE_RC_FOUND:
                SS_PMON_ADD(SS_PMON_DBEFETCH);
                /* FALLTHROUGH */
            case DBE_RC_NOTFOUND:
            case DBE_RC_WAITLOCK:
            case DBE_RC_END:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_cursor_nextorprev_n");
        ss_dprintf_2(("dbe_cursor_%s:end\n", nextp ? "next" : "prev"));
        SU_GENERIC_TIMER_STOP(SU_GENERIC_TIMER_DBE_CURSORFETCH);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_next
 *
 * Returns the next tuple value. This function actually
 * advances the search one atomic step, and as the result of the
 * step the next tuple may or may not be found. The return value
 * specifies if the next tuple was found.
 *
 * Typically this function can be called in a loop, e.g.
 *
 *      do {
 *          rc = dbe_cursor_next(c, &tval, &errh);
 *      } while (rc != DBE_RC_NOTFOUND);
 *      / * Now the next tuple is found or the search is ended. * /
 *
 * Parameters :
 *
 *	cursor - in, use
 *          Cursor pointer.
 *
 * 	trx - in, use
 *          Transaction handle.
 *
 *	p_tval - out, take give (replace)
 *          Pointer to a tval pointer into where the found value is returned.
 *          This is of same type as ttype given in dbe_cursor_init.
 *          This parameter is updated only if the return value is
 *          DBE_RC_FOUND.
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value :
 *
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Next tuple not found in this step, it may
 *                        be found in the next step. Parameter
 *                        p_tval is not updated.
 *      DBE_RC_FOUND    - Next tuple found, parameter p_tval is
 *                        updated.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_cursor_next(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh)
{
        dbe_ret_t rc;

        SS_PUSHNAME("dbe_cursor_next");
        rc = dbe_cursor_nextorprev(
                    cursor,
                    TRUE,
                    trx,
                    p_tval,
                    p_errh);
        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_prev
 *
 * Returns the prev tuple value. This function actually
 * advances the search one atomic step, and as the result of the
 * step the prev tuple may or may not be found. The return value
 * specifies if the prev tuple was found.
 *
 * Typically this function can be called in a loop, e.g.
 *
 *      do {
 *          rc = dbe_cursor_prev(c, &tval, &errh);
 *      } while (rc != DBE_RC_NOTFOUND);
 *      / * Now the prev tuple is found or the search is ended. * /
 *
 * Parameters :
 *
 *	cursor - in, use
 *          Cursor pointer.
 *
 * 	trx - in, use
 *          Transaction handle.
 *
 *	p_tval - out, take give (replace)
 *          Pointer to a tval pointer into where the found value is returned.
 *          This is of same type as ttype given in dbe_cursor_init.
 *          This parameter is updated only if the return value is
 *          DBE_RC_FOUND.
 *
 *      p_errh - out, give
 *          Pointer to an error handle into where an error
 *          info is stored if the function fails.
 *
 * Return value :
 *
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Prev tuple not found in this step, it may
 *                        be found in the prev step. Parameter
 *                        p_tval is not updated.
 *      DBE_RC_FOUND    - Prev tuple found, parameter p_tval is
 *                        updated.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_cursor_prev(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t**   p_tval,
        rs_err_t**    p_errh)
{
        dbe_ret_t rc;

        SS_PUSHNAME("dbe_cursor_prev");
        rc = dbe_cursor_nextorprev(
                    cursor,
                    FALSE,
                    trx,
                    p_tval,
                    p_errh);
        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_gotoend
 *
 *
 *
 * Parameters :
 *
 *	cursor -
 *
 *
 *	trx -
 *
 *
 *	p_errh -
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
dbe_ret_t dbe_cursor_gotoend(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_err_t**    p_errh)
{
        dbe_ret_t rc;
        su_profile_timer;

        SS_NOTUSED(p_errh);

        ss_dprintf_1(("dbe_cursor_gotoend:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        SS_PUSHNAME("dbe_cursor_gotoend");

        if (dbe_trx_isfailed(trx)) {
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SS_POPNAME;
            return(dbe_trx_geterrcode(trx));
        }

        su_profile_start;

        rc = dbe_search_gotoend((dbe_search_t*)cursor, trx);

        switch (rc) {
            case DBE_RC_SUCC:
                break;
            default:
                dbe_trx_setfailed(trx, rc, TRUE);
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_cursor_gotoend");
        ss_dprintf_2(("dbe_cursor_gotoend:end\n"));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_setposition
 *
 *
 *
 * Parameters :
 *
 *	cursor -
 *
 *
 *	trx -
 *
 *
 *	tval -
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
dbe_ret_t dbe_cursor_setposition(
        dbe_cursor_t* cursor,
        dbe_trx_t*    trx,
        rs_tval_t*    tval,
        rs_err_t**    p_errh)
{
        dbe_ret_t rc;
        su_profile_timer;

        ss_dprintf_1(("dbe_cursor_setposition:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        SS_PUSHNAME("dbe_cursor_setposition");

        if (dbe_trx_isfailed(trx)) {
            rs_error_create(p_errh, dbe_trx_geterrcode(trx));
            SS_POPNAME;
            return(dbe_trx_geterrcode(trx));
        }

        su_profile_start;

        rc = dbe_search_setposition((dbe_search_t*)cursor, trx, tval);

        switch (rc) {
            case DBE_RC_SUCC:
                break;
            default:
                rs_error_create(p_errh, rc);
                break;
        }

        su_profile_stop("dbe_cursor_setposition");
        ss_dprintf_2(("dbe_cursor_setposition:end\n"));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_delete
 *
 * Deletes a tuple from a relation.
 *
 * Parameters :
 *
 *	cursor - in, use
 *          Cursor pointer.
 *
 *	trx - in, use
 *          Transaction handle.
 *
 *	relh - in, use
 *          Relation handle.
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
dbe_ret_t dbe_cursor_delete(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        rs_err_t**      p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        su_profile_timer;

        ss_dprintf_1(("dbe_cursor_delete:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        SS_PUSHNAME("dbe_cursor_delete");

        cd = dbe_trx_getcd(trx);
        db = dbe_trx_getdb(trx);

        dbe_db_enteraction(db, cd);

        su_profile_start;

        if (!rs_relh_islogged(cd, relh)) {
            rc = dbe_trx_markwrite_nolog(trx, TRUE);
        } else {
            rc = dbe_trx_markwrite(trx, TRUE);
        }
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(db, cd);
            su_profile_stop("dbe_cursor_delete");
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
                    dbe_search_gettref((dbe_search_t*)cursor, tval),
                    (dbe_mme_search_t*)cursor);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_delete_disk(
                    cd,
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    dbe_search_gettref((dbe_search_t*)cursor, tval),
                    (dbe_search_t*)cursor);
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
                dbe_trx_error_create(trx, rc, p_errh);
                break;
        }

        su_profile_stop("dbe_cursor_delete");
        ss_dprintf_2(("dbe_cursor_delete:end, rc=%s\n", su_rc_nameof(rc)));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_update
 *
 * Updates a tuple in a relation.
 *
 * Parameters :
 *
 *	cursor - in, use
 *          Cursor pointer.
 *
 *	trx - in, use
 *          Transaction handle.
 *
 *	relh - in, use
 *          Relation handle.
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
dbe_ret_t dbe_cursor_update(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        dbe_trx_t*      trx,
        rs_relh_t*      relh,
        bool*           upd_attrs,
        rs_tval_t*      upd_tval,
        dbe_tref_t*     new_tref,
        rs_err_t**      p_errh)
{
        dbe_db_t* db;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        su_profile_timer;

        ss_dprintf_1(("dbe_cursor_update:userid = %d, usertrxid = %ld\n",
            dbe_user_getid(dbe_trx_getuser(trx)),
            DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        ss_dassert(trx != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(upd_attrs != NULL);
        ss_dassert(upd_tval != NULL);
        SS_PUSHNAME("dbe_cursor_update");

        cd = dbe_trx_getcd(trx);
        db = dbe_trx_getdb(trx);

        dbe_db_enteraction(db, cd);

        su_profile_start;

        if (!rs_relh_islogged(cd, relh)) {
            rc = dbe_trx_markwrite_nolog(trx, TRUE);
        } else {
            rc = dbe_trx_markwrite(trx, TRUE);
        }
        if (rc != DBE_RC_SUCC) {
            dbe_db_exitaction(db, cd);
            su_profile_stop("dbe_cursor_update");
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
                    dbe_search_gettref((dbe_search_t*)cursor, tval),
                    upd_attrs,
                    upd_tval,
                    new_tref,
                    (dbe_mme_search_t*)cursor);
        } else {
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_tuple_update_disk(
                    cd,
                    trx,
                    dbe_trx_getusertrxid(trx),
                    relh,
                    dbe_search_gettref((dbe_search_t*)cursor, tval),
                    upd_attrs,
                    upd_tval,
                    new_tref,
                    (dbe_search_t*)cursor);
        }

        dbe_db_exitaction(db, cd);

        switch (rc) {
            case DBE_RC_SUCC:
                SS_PMON_ADD(SS_PMON_DBEUPDATE);
                if (rs_relh_isaborted(cd, relh)) {
                    dbe_trx_error_create(trx, E_DDOP, p_errh);
                    rc = E_DDOP;
                }
                /* FALLTHROUGH */
            case DBE_RC_WAITLOCK:
            case DBE_RC_CONT:
                break;

            default:
                SS_PMON_ADD(SS_PMON_DBEUPDATE);
                dbe_trx_error_create(trx, rc, p_errh);
                break;
        }

        su_profile_stop("dbe_cursor_update");
        ss_dprintf_2(("dbe_cursor_update:end\n"));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_getaval
 *
 * Returns attribute value at position kpno in clustering key of relation
 * in current cursor position.
 *
 * Parameters :
 *
 *	cursor - in
 *		Cursor pointer.
 *
 *	atype - in
 *		Attribute type of returned attribute value.
 *
 *	kpno - in
 *		Key part number in clustering key.
 *
 * Return value - give :
 *
 *      Attribute value.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_aval_t* dbe_cursor_getaval(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval,
        rs_atype_t*     atype,
        int             kpno)
{
        rs_aval_t* aval;

        ss_dprintf_1(("dbe_cursor_getaval\n"));
        ss_dassert(atype != NULL);
        SS_PUSHNAME("dbe_cursor_getaval");

        aval = dbe_search_getaval((dbe_search_t*)cursor, tval, atype, kpno);

        ss_dprintf_2(("dbe_cursor_getaval:end\n"));
        SS_POPNAME;

        return(aval);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_gettref
 *
 * Returns tuple reference of the current search position.
 *
 * Parameters :
 *
 *	cursor - in
 *		Cursor pointer.
 *
 * Return value - ref :
 *
 *      Pointer to a local copy of tuple reference.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_tref_t* dbe_cursor_gettref(
        dbe_cursor_t*   cursor,
        rs_tval_t*      tval)
{
        dbe_tref_t* tref;

        ss_dprintf_1(("dbe_cursor_gettref\n"));
        SS_PUSHNAME("dbe_cursor_gettref");

        tref = dbe_search_gettref((dbe_search_t*)cursor, tval);

        SS_POPNAME;

        return(tref);
}

/*##**********************************************************************\
 *
 *		dbe_cursor_setoptinfo
 *
 *
 *
 * Parameters :
 *
 *	cursor -
 *
 *
 *	trx -
 *
 *
 *	count -
 *		Number of expected rows returned from the query. Value zero
 *		means all rows.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_cursor_setoptinfo(
        dbe_cursor_t* cursor,
        ulong count)
{
        ss_dprintf_1(("dbe_cursor_setoptcount: count=%ld\n", count));
        SS_PUSHNAME("dbe_cursor_setoptinfo");

        dbe_search_setoptinfo(
            (dbe_search_t*)cursor,
            count);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *      dbe_cursor_restartsearch
 *
 * Invalidates the old search and starts a new.
 *
 * Parameters:
 *      cursor - in
 *
 *      trx - in
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_cursor_restartsearch(
        dbe_cursor_t*   cursor,
        dbe_trx_t*      trx )
{

        dbe_db_t*     db;
        dbe_trxid_t   usertrxid;
        dbe_trxnum_t  maxtrxnum;

        ss_dprintf_1(("dbe_cursor_restartsearch\n"));
        SS_PUSHNAME("dbe_cursor_restartsearch");

        db = dbe_trx_getdb(trx);
        usertrxid = dbe_trx_getusertrxid(trx);

        /* Calling restartif in order to reset canremovereadlevel flag */
        dbe_trx_restartif(trx);

        dbe_trx_startnewsearch(trx);
        
        dbe_search_invalidate((dbe_search_t*)cursor,
                              usertrxid,
                              DBE_SEARCH_INVALIDATE_COMMIT);
        
        if (dbe_search_gettype(cursor) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_restart(
                (dbe_mme_search_t*)cursor,
                trx);
        } else {
            ss_dassert(dbe_search_gettype(cursor) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);

            maxtrxnum = dbe_trx_getsearchtrxnum(trx);

            if (dbe_trxnum_equal(maxtrxnum, dbe_trxnum_null)){
                dbe_search_restart_disk(
                        (dbe_search_t*)cursor,
                        trx,
                        dbe_trx_getmaxtrxnum(trx),
                        usertrxid);
            } else {
                dbe_search_restart_disk(
                        (dbe_search_t*)cursor,
                        trx,
                        maxtrxnum,
                        usertrxid);
            }
            
        }
        
        SS_POPNAME;
}

#ifdef SS_QUICKSEARCH
void* dbe_cursor_getquicksearch(
        dbe_cursor_t* cursor,
        bool longsearch)
{
        return(dbe_search_getquicksearch((dbe_search_t*)cursor, longsearch));
}
#endif /* SS_QUICKSEARCH */
