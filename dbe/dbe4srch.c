/*************************************************************************\
**  source       * dbe4srch.c
**  directory    * dbe
**  description  * Database relation search routines.
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

This module implements the database search object. The database search
is used to search through the database using a key.

No security checks are done at this level. The caller of this level
must implement them.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

attribute type      rs0atype.h
attribute value     rs0aval.c
tuple type          rs0ttype.c
tuple value         rs0tval.c
relation handle     rs0relh.c
search plan         rs0pla.c

index system        dbe0inde.c
user object         dbe0user.c
transaction object  dbe0trx.c
tuple reference     dbe0tref.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE4SRCH_C

#include <ssc.h>

#include <ssmem.h>
#include <ssdebug.h>
#include <ssfile.h>

#include <su0list.h>
#include <su0bflag.h>
#include <su0prof.h>

#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>
#include <rs0pla.h>
#include <rs0sdefs.h>

#include "dbe9type.h"
#include "dbe7binf.h"
#include "dbe7gtrs.h"
#include "dbe6gobj.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe6bmgr.h"
#include "dbe6lmgr.h"
#include "dbe4mme.h"
#include "dbe5isea.h"
#include "dbe5dsea.h"
#include "dbe4srch.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0user.h"
#include "dbe1trx.h"
#include "dbe0trx.h"
#include "dbe0tref.h"
#include "dbe0db.h"

/* Search flags.
 */
#define SEA_ABORTED         SU_BFLAG_BIT(0)
#define SEA_CHECKLOCK       SU_BFLAG_BIT(1)
#define SEA_ISOLATIONCHANGE SU_BFLAG_BIT(2)

static dbe_ret_t search_createtval_simple(
        dbe_search_t* search,
        rs_tval_t** p_tval);

/*##**********************************************************************\
 *
 *		dbe_search_init_disk
 *
 * Initializes the search.
 *
 * Parameters :
 *
 *	user - in, hold
 *		User object of the user of the search.
 *
 *	maxtrxnum - in
 *
 *
 *	usertrxid - in
 *
 *	ttype - in, hold
 *		Tuple type of the tuples returned by the search.
 *
 *      sellist - in, hold
 *          List of selected attributes, RS_ANO_NULL terminates the list.
 *
 *	plan - in, take
 *		Search plan.
 *
 * Return value - give :
 *
 *      Pointer to the search object.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_search_t* dbe_search_init_disk(
        dbe_user_t* user,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan,
        dbe_cursor_type_t cursor_type,
        bool* p_newplan)
{
        dbe_search_t* search;
        dbe_searchrange_t search_range;
        rs_sysi_t* cd;
        rs_relh_t* relh;
        dbe_lock_mode_t lock_mode;
        rs_reltype_t reltype;

        ss_dprintf_1(("dbe_search_init_disk\n"));
        ss_dassert(user != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);

        cd = dbe_user_getcd(user);

        relh = rs_pla_getrelh(cd, plan);
        reltype = rs_relh_reltype(cd, relh);

        search = SSMEM_NEW(dbe_search_t);

        ss_dassert(search == (dbe_search_t*)&(search->sea_hdr));
        ss_debug(search->sea_hdr.sh_chk = DBE_CHK_SEARCH_HEADER);
        search->sea_hdr.sh_type = DBE_SEARCH_DBE;
        ss_debug(search->sea_chk = DBE_CHK_SEARCH);
        search->sea_flags = 0;
        search->sea_user = user;
        search->sea_index = dbe_user_getindex(user);
        search->sea_cd = cd;
        search->sea_ttype = ttype;
        search->sea_sellist = sellist;
        search->sea_plan = plan;
        search->sea_tref = NULL;
        search->sea_reltype = reltype;
        search->sea_key = rs_pla_getkey(cd, plan);
        search->sea_relid = rs_pla_getrelid(cd, plan);
        search->sea_posdvtpl = NULL;

        search->sea_isolationchange_transparent = FALSE;

#ifdef SS_QUICKSEARCH
        search->sea_qsvatable = NULL;
#endif
        search->sea_cursortype = cursor_type;

        switch (cursor_type) {
            case DBE_CURSOR_SELECT:      /* SELECT */
                ss_dprintf_2(("dbe_search_init_disk:DBE_CURSOR_SELECT\n"));
                lock_mode = LOCK_S;
                break;
            case DBE_CURSOR_FORUPDATE:   /* SELECT .. FOR UPDATE */
                ss_dprintf_2(("dbe_search_init_disk:DBE_CURSOR_FORUPDATE\n"));
                lock_mode = LOCK_U;
                break;
            case DBE_CURSOR_UPDATE:      /* searched UPDATE */
            case DBE_CURSOR_DELETE:      /* searched DELETE */
                ss_dprintf_2(("dbe_search_init_disk:DBE_CURSOR_UPDATE or DBE_CURSOR_DELETE (%d)\n", cursor_type));
                lock_mode = LOCK_X;
                break;
            default:
                lock_mode = LOCK_X;
                ss_error;
        }
        search->sea_relh = relh;

        search->sea_uselocks = dbe_trx_uselocking(
                                    trx,
                                    relh,
                                    lock_mode,
                                    &search->sea_locktimeout,
                                    &search->sea_optimistic_lock);
        if ((dbe_cfg_versionedpessimisticreadcommitted
             || dbe_cfg_versionedpessimisticrepeatableread)
            && search->sea_reltype == RS_RELTYPE_PESSIMISTIC
            && !search->sea_uselocks
            && lock_mode == LOCK_S
            && dbe_trx_getisolation(trx) != RS_SQLI_ISOLATION_SERIALIZABLE)
        {
            /* Pessimistic table that does not use shared locks and isolation
             * level is not serializable should work just like optimistic table.
             */
            search->sea_reltype = RS_RELTYPE_OPTIMISTIC;
            search->sea_isupdatable = FALSE;
            search->sea_versionedpessimistic = TRUE;
        } else {
            search->sea_isupdatable = TRUE;
            search->sea_versionedpessimistic = FALSE;
        }

        if (search->sea_uselocks &&
            (!search->sea_optimistic_lock ||
             search->sea_cursortype == DBE_CURSOR_FORUPDATE)) {
            /* Bounce locks are not used in pessimistic locking or
             * SELECT ... FOR UPDATE in optimistic locking.
             */
            search->sea_bouncelock = FALSE;
        } else {
            search->sea_bouncelock = TRUE;
        }

        if (search->sea_versionedpessimistic) {
            maxtrxnum = dbe_trx_getstmtsearchtrxnum(trx);
        } else if (search->sea_uselocks) {
            if (!search->sea_optimistic_lock) {
                /* True pessimistic search. All data is visible, noncommitted
                 * data causes waits.
                 */
                maxtrxnum = DBE_TRXNUM_MAX;
                search->sea_reltype = RS_RELTYPE_PESSIMISTIC;
                dbe_trx_resetstmtsearchtrxnum(trx);
            }
        } else if (reltype == RS_RELTYPE_OPTIMISTIC) {
            /* Optimistic search.
             */
            lock_mode = LOCK_FREE;
        }
        search->sea_lockmode = lock_mode;
        search->sea_lastchangecount = -1L;
        search->sea_relhlockedp = FALSE;

        search->sea_go = dbe_db_getgobj(dbe_user_getdb(user));

        search->sea_tc.tc_maxtrxnum = maxtrxnum;
        search->sea_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        search->sea_tc.tc_usertrxid = usertrxid;
        search->sea_tc.tc_maxtrxid = dbe_trx_getreadtrxid(trx);
        search->sea_tc.tc_trxbuf = NULL;

        ss_dassert(DBE_TRXID_CMP_EX(search->sea_tc.tc_usertrxid, search->sea_tc.tc_maxtrxid) <= 0
                   || DBE_TRXID_EQUAL(search->sea_tc.tc_maxtrxid, DBE_TRXID_NULL));

        search->sea_refattrs = rs_pla_get_tuple_reference(cd, plan);

        if (su_list_length(search->sea_refattrs) == 0) {
            search->sea_refattrs = NULL;
        }

        rs_pla_get_select_list(
            cd,
            plan,
            &search->sea_selkeyparts,
            &search->sea_getdata);

        search->sea_data_conslist = rs_pla_get_data_constraints(cd, plan);

        rs_pla_get_range_start(
            cd,
            plan,
            &search_range.sr_minvtpl,
            &search_range.sr_minvtpl_closed);
        rs_pla_get_range_end(
            cd,
            plan,
            &search_range.sr_maxvtpl,
            &search_range.sr_maxvtpl_closed);

        ss_dprintf_2(("dbe_search_init_disk: usertrxid %ld, trxrange [%ld %ld], maxtrxid %ld\n",
            DBE_TRXID_GETLONG(usertrxid), DBE_TRXNUM_GETLONG(DBE_TRXNUM_MIN), DBE_TRXNUM_GETLONG(maxtrxnum),
            DBE_TRXID_GETLONG(search->sea_tc.tc_maxtrxid)));

        if (search->sea_getdata) {
            search->sea_datasea = dbe_datasea_init(
                                    cd,
                                    search->sea_index,
                                    rs_relh_clusterkey(cd, relh),
                                    &search->sea_tc,
                                    search->sea_data_conslist,
                                    search->sea_reltype == RS_RELTYPE_PESSIMISTIC,
                                    "dbe_search_init_disk");
        } else {
            search->sea_datasea = NULL;
        }
        search->sea_datasrk = NULL;


        search->sea_indsea = dbe_indsea_init_ex(
                                    cd,
                                    search->sea_index,
                                    search->sea_key,
                                    &search->sea_tc,
                                    &search_range,
                                    rs_pla_get_key_constraints(cd, plan),
                                    search->sea_lockmode,
                                    search->sea_reltype == RS_RELTYPE_PESSIMISTIC,
                                    NULL,
                                    "dbe_search_init_disk");
        if (search->sea_versionedpessimistic) {
            dbe_indsea_setversionedpessimistic(search->sea_indsea);
        }

        search->sea_activated = FALSE;
        search->sea_needrestart = FALSE;
        search->sea_rc = DBE_RC_END;
        search->sea_forwardp = TRUE;
        search->sea_nseqstep = 0;
        search->sea_nseqsteplimit = dbe_index_getseqsealimit(
                                        dbe_user_getindex(user));
        search->sea_p_newplan = p_newplan;
        search->sea_id = dbe_user_addsearch(user, search);

        ss_dprintf_1(("dbe_search_init_disk: sea_id=%ld\n", (long)search->sea_id));
        return(search);
}

/*##**********************************************************************\
 *
 *		dbe_search_done_disk
 *
 * Releases the search object (ends the search).
 *
 * Parameters :
 *
 *	search - in, take
 *		Search object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_search_done_disk(
        dbe_search_t* search)
{
        ss_dprintf_1(("dbe_search_done_disk: sea_id=%ld\n", (long)search->sea_id));
        CHK_SEARCH(search);

        ss_dprintf_1(("dbe_search_done_disk\n"));
        ss_dassert(search->sea_posdvtpl == NULL);
        dynvtpl_free(&search->sea_posdvtpl);
        dbe_user_removesearch(search->sea_user, search->sea_id);
        if (search->sea_datasea != NULL) {
            dbe_datasea_done(search->sea_datasea);
        }
        if (search->sea_tref != NULL) {
            dbe_tref_done(search->sea_cd, search->sea_tref);
        }
        dbe_indsea_done(search->sea_indsea);
        rs_pla_done(search->sea_cd, search->sea_plan);

#ifdef SS_QUICKSEARCH
        if (search->sea_qsvatable != NULL) {
            SsMemFree(search->sea_qsvatable);
        }
#endif

        ss_debug(search->sea_chk = DBE_CHK_FREED);
        ss_debug(search->sea_hdr.sh_chk = DBE_CHK_FREED);

        SsMemFree(search);
}

/*##**********************************************************************\
 *
 *		dbe_search_done
 *
 *
 *
 * Parameters :
 *
 *	search -
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
void dbe_search_done(
        dbe_search_t* search)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_mme_search_done((dbe_mme_search_t*)search);
        } else {
            ss_dassert(dbe_search_gettype(search) == DBE_SEARCH_DBE);
            dbe_search_done_disk(search);
        }
}

void dbe_search_setisolation_transparent(
        dbe_search_t* search,
        bool transparent)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_DBE) {
            search->sea_isolationchange_transparent = transparent;
        } else {
            ss_dassert(!transparent); /* we are not allowed to change default (which is FALSE) for M-tables */
        }
}


/*##**********************************************************************\
 *
 *		dbe_search_reset_disk
 *
 * Resets a search. After reset cursor is in the same state as after
 * cursor init.
 *
 * Parameters :
 *
 *	search - in, use
 *		Search object.
 *
 *	trx - in, use
 *		Current transaction.
 *
 *	maxtrxnum - in
 *		New read level.
 *
 *	usertrxid - in
 *		Current user trx id.
 *
 *	ttype - in
 *		New result tuple type.
 *
 *	sellist - in
 *		New select list.
 *
 *	plan - in
 *		New search
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_search_reset_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan)
{
        dbe_searchrange_t search_range;
        rs_sysi_t* cd;
        rs_pla_t* old_plan;

        ss_dprintf_1(("dbe_search_reset_disk\n"));
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        ss_dassert((ulong)search->sea_relid == rs_pla_getrelid(search->sea_cd, plan));

        old_plan = search->sea_plan;

        /*
         * New init routines.
         */
        cd = search->sea_cd;

        if (search->sea_uselocks || search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            rs_sysi_clearflag(cd, RS_SYSI_FLAG_STORAGETREEONLY);
        }

        /* Enter user mutex to lock access to search while we are updating data structures.
         */
        dbe_user_checkoutsearches(search->sea_user);

        search->sea_flags = 0;
        search->sea_ttype = ttype;
        search->sea_sellist = sellist;
        search->sea_plan = plan;

        ss_dassert(search->sea_key == rs_pla_getkey(cd, plan));
        ss_dassert(search->sea_relh == rs_pla_getrelh(cd, plan));

        search->sea_lastchangecount = -1L;
        search->sea_relhlockedp = FALSE;

        if (search->sea_versionedpessimistic) {
            maxtrxnum = dbe_trx_getstmtsearchtrxnum(trx);
        } else if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            maxtrxnum = DBE_TRXNUM_MAX;
            dbe_trx_resetstmtsearchtrxnum(trx);
        }

        search->sea_tc.tc_maxtrxnum = maxtrxnum;
        search->sea_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        search->sea_tc.tc_usertrxid = usertrxid;
        search->sea_tc.tc_maxtrxid = dbe_trx_getreadtrxid(trx);
        search->sea_tc.tc_trxbuf = NULL;

        ss_dassert(DBE_TRXID_CMP_EX(search->sea_tc.tc_usertrxid, search->sea_tc.tc_maxtrxid) <= 0
                   || DBE_TRXID_EQUAL(search->sea_tc.tc_maxtrxid, DBE_TRXID_NULL));

        search->sea_refattrs = rs_pla_get_tuple_reference(cd, plan);

        if (su_list_length(search->sea_refattrs) == 0) {
            search->sea_refattrs = NULL;
        }

        rs_pla_get_select_list(
            cd,
            plan,
            &search->sea_selkeyparts,
            &search->sea_getdata);

        search->sea_data_conslist = rs_pla_get_data_constraints(cd, plan);

        rs_pla_get_range_start(
            cd,
            plan,
            &search_range.sr_minvtpl,
            &search_range.sr_minvtpl_closed);
        rs_pla_get_range_end(
            cd,
            plan,
            &search_range.sr_maxvtpl,
            &search_range.sr_maxvtpl_closed);

        ss_dprintf_2(("dbe_search_reset_disk: usertrxid %ld, trxrange [%ld %ld], maxtrxid %ld\n",
            DBE_TRXID_GETLONG(usertrxid), DBE_TRXNUM_GETLONG(DBE_TRXNUM_MIN), DBE_TRXNUM_GETLONG(maxtrxnum),
            DBE_TRXID_GETLONG(search->sea_tc.tc_maxtrxid)));

        if (search->sea_getdata) {
            dbe_datasea_reset(
                search->sea_datasea,
                search->sea_data_conslist);
        }
        search->sea_datasrk = NULL;

        ss_dassert(search->sea_posdvtpl == NULL);
        dynvtpl_free(&search->sea_posdvtpl);

        dbe_indsea_reset(
            search->sea_indsea,
            &search->sea_tc,
            &search_range,
            rs_pla_get_key_constraints(cd, plan));

        search->sea_activated = FALSE;
        search->sea_needrestart = FALSE;
        search->sea_rc = DBE_RC_END;
        search->sea_forwardp = TRUE;
        search->sea_nseqstep = 0;

        dbe_user_checkinsearches(search->sea_user);

        /*
         * Done routines.
         */
        rs_pla_done(search->sea_cd, old_plan);

        ss_dprintf_1(("dbe_search_reset_disk: sea_id=%ld\n", (long)search->sea_id));
}

/*##**********************************************************************\
 * 
 *		dbe_search_reset_disk_fetch
 * 
 * 
 * 
 * Parameters : 
 * 
 *		search - 
 *			
 *			
 *		trx - 
 *			
 *			
 *		maxtrxnum - 
 *			
 *			
 *		usertrxid - 
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
 *		p_tval - 
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
dbe_ret_t dbe_search_reset_disk_fetch(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan,
        rs_tval_t** p_tval)
{
        dbe_searchrange_t search_range;
        rs_sysi_t* cd;
        rs_pla_t* old_plan;
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_search_reset_disk_fetch\n"));
        ss_dassert(ttype != NULL);
        ss_dassert(plan != NULL);
        ss_dassert((ulong)search->sea_relid == rs_pla_getrelid(search->sea_cd, plan));

        ss_debug(rs_tval_resetexternalflatva(search->sea_cd, search->sea_ttype, *p_tval));

        old_plan = search->sea_plan;

        /*
         * New init routines.
         */
        cd = search->sea_cd;

        if (search->sea_uselocks || search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            rs_sysi_clearflag(cd, RS_SYSI_FLAG_STORAGETREEONLY);
        }

        /* Enter user mutex to lock access to search while we are updating data structures.
         */
        dbe_user_checkoutsearches(search->sea_user);

        search->sea_flags = 0;
        search->sea_ttype = ttype;
        search->sea_sellist = sellist;
        search->sea_plan = plan;

        ss_dassert(search->sea_key == rs_pla_getkey(cd, plan));
        ss_dassert(search->sea_relh == rs_pla_getrelh(cd, plan));

        search->sea_lastchangecount = -1L;
        search->sea_relhlockedp = FALSE;

        if (search->sea_versionedpessimistic) {
            maxtrxnum = dbe_trx_getstmtsearchtrxnum(trx);
        } else if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            maxtrxnum = DBE_TRXNUM_MAX;
            dbe_trx_resetstmtsearchtrxnum(trx);
        }

        search->sea_tc.tc_maxtrxnum = maxtrxnum;
        search->sea_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        search->sea_tc.tc_usertrxid = usertrxid;
        search->sea_tc.tc_maxtrxid = dbe_trx_getreadtrxid(trx);
        search->sea_tc.tc_trxbuf = NULL;

        ss_dassert(DBE_TRXID_CMP_EX(search->sea_tc.tc_usertrxid, search->sea_tc.tc_maxtrxid) <= 0
                   || DBE_TRXID_EQUAL(search->sea_tc.tc_maxtrxid, DBE_TRXID_NULL));

        search->sea_refattrs = rs_pla_get_tuple_reference(cd, plan);

        if (su_list_length(search->sea_refattrs) == 0) {
            search->sea_refattrs = NULL;
        }

        rs_pla_get_select_list(
            cd,
            plan,
            &search->sea_selkeyparts,
            &search->sea_getdata);

        search->sea_data_conslist = rs_pla_get_data_constraints(cd, plan);

        rs_pla_get_range_start(
            cd,
            plan,
            &search_range.sr_minvtpl,
            &search_range.sr_minvtpl_closed);
        rs_pla_get_range_end(
            cd,
            plan,
            &search_range.sr_maxvtpl,
            &search_range.sr_maxvtpl_closed);

        ss_dprintf_2(("dbe_search_reset_disk: usertrxid %ld, trxrange [%ld %ld], maxtrxid %ld\n",
            DBE_TRXID_GETLONG(usertrxid), DBE_TRXNUM_GETLONG(DBE_TRXNUM_MIN), DBE_TRXNUM_GETLONG(maxtrxnum),
            DBE_TRXID_GETLONG(search->sea_tc.tc_maxtrxid)));

        if (search->sea_getdata) {
            dbe_datasea_reset(
                search->sea_datasea,
                search->sea_data_conslist);
        }
        search->sea_datasrk = NULL;
        search->sea_activated = FALSE;
        search->sea_rc = DBE_RC_END;
        search->sea_forwardp = TRUE;
        search->sea_nseqstep = 0;


        if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC
            || search->sea_flags != 0
            || search->sea_getdata
            || search->sea_uselocks) 
        {
            dbe_indsea_reset(
                search->sea_indsea,
                &search->sea_tc,
                &search_range,
                rs_pla_get_key_constraints(cd, plan));
            rc = DBE_RC_NOTFOUND;
        } else {
            rc = dbe_indsea_reset_fetch(
                    search->sea_indsea,
                    &search->sea_tc,
                    &search_range,
                    rs_pla_get_key_constraints(cd, plan),
                    dbe_trx_getstmttrxid(trx),
                    &search->sea_srk);

            su_rc_dassert(rc == DBE_RC_FOUND ||
                          rc == DBE_RC_NOTFOUND ||
                          rc == DBE_RC_END, rc);

            search->sea_activated = TRUE;
            search->sea_rc = rc;
            search->sea_nseqstep = 1;
        }

        dbe_user_checkinsearches(search->sea_user);

        if (rc == DBE_RC_FOUND) {
            rc = search_createtval_simple(search, p_tval);
        }

        /*
         * Done routines.
         */
        rs_pla_done(search->sea_cd, old_plan);

        ss_dprintf_1(("dbe_search_reset_disk_fetch: sea_id=%ld, rc=%d\n", (long)search->sea_id, rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_search_reset
 *
 *
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	trx -
 *
 *
 *	maxtrxnum -
 *
 *
 *	usertrxid -
 *
 *
 *	ttype -
 *
 *
 *	sellist -
 *
 *
 *	plan -
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
void dbe_search_reset(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan)
{
        SS_PUSHNAME("dbe_search_reset");
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_reset(
                (dbe_mme_search_t*)search,
                trx,
                plan);
        } else {
            ss_dassert(dbe_search_gettype(search) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            dbe_search_reset_disk(
                search,
                trx,
                dbe_trx_getsearchtrxnum(trx),
                usertrxid,
                ttype,
                sellist,
                plan);
        }
        SS_POPNAME;
}

/*##**********************************************************************\
 * 
 *		dbe_search_reset_fetch
 * 
 * 
 * 
 * Parameters : 
 * 
 *		search - 
 *			
 *			
 *		trx - 
 *			
 *			
 *		usertrxid - 
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
 *		p_tval - 
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
dbe_ret_t dbe_search_reset_fetch(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan,
        rs_tval_t** p_tval)
{
        dbe_ret_t rc;

        SS_PUSHNAME("dbe_search_reset_fetch");
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_reset(
                (dbe_mme_search_t*)search,
                trx,
                plan);
            rc = DBE_RC_NOTFOUND;
        } else {
            ss_dassert(dbe_search_gettype(search) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            rc = dbe_search_reset_disk_fetch(
                    search,
                    trx,
                    dbe_trx_getsearchtrxnum(trx),
                    usertrxid,
                    ttype,
                    sellist,
                    plan,
                    p_tval);
        }
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_search_getaval
 *
 * Returns attribute value at position kpno in clustering key of relation
 * in current search position.
 *
 * Parameters :
 *
 *	search - in
 *		Search pointer.
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
rs_aval_t* dbe_search_getaval(
        dbe_search_t*   search,
        rs_tval_t*      tval,
        rs_atype_t*     atype,
        uint            kpno)
{
        rs_aval_t* aval;
        va_t* va;
        vtpl_vamap_t* vamap;
        dbe_srk_t* srk;

        ss_dprintf_1(("dbe_search_getaval\n"));
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            return dbe_mme_search_getaval((dbe_mme_search_t*)search, tval, atype, kpno);
        } else {
            ss_assert(search->sea_activated);
            ss_dassert(!search->sea_needrestart);
            su_rc_assert(search->sea_rc == DBE_RC_FOUND, search->sea_rc);

            if (!search->sea_getdata &&
                !rs_pla_usingclusterkey(search->sea_cd, search->sea_plan)) {

                if (search->sea_datasrk == NULL) {

                    /* Data tuple not already fetched and not using clustering key,
                     * fetch it now.
                     */
                    dbe_tref_t* tref;
                    dbe_ret_t rc;

                    tref = dbe_search_gettref(search, tval);

                    if (search->sea_datasea == NULL) {

                        search->sea_datasea = dbe_datasea_init(
                                search->sea_cd,
                                search->sea_index,
                                rs_relh_clusterkey(
                                        search->sea_cd,
                                        search->sea_relh),
                                &search->sea_tc,
                                NULL,
                                FALSE,
                                "dbe_search_getaval");
                    } else {
                        /* Old data search from previous call to this function. */
                        dbe_datasea_reset(
                                search->sea_datasea,
                                NULL);
                    }

                    rc = dbe_datasea_search(
                            search->sea_datasea,
                            tref->tr_vtpl,
                            DBE_TRXID_NULL,
                            &search->sea_datasrk);
                    su_rc_assert(rc == DBE_RC_FOUND, rc);
                }   
                ss_dassert(search->sea_datasea != NULL);

                srk = search->sea_datasrk;

            } else {

                srk = search->sea_srk;
            }

            vamap = dbe_srk_getvamap(srk);
            va = vtpl_vamap_getva_at(vamap, kpno);

            aval = rs_aval_create(search->sea_cd, atype);
            rs_aval_setva(search->sea_cd, atype, aval, va);

            return(aval);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_invalidate
 *
 * Invalidates a searches with usertrxid. Typically used when cursor
 * is not closed in transaction commit or isolation is changed.
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	usertrxid -
 *
 *
 *	type -
 *      If DBE_SEARCH_INVALIDATE_COMMIT, this is called from commit. 
 *      In commit case only M-table cursors are marked in aborted/invalid. 
 *      If DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE then all searches are 
 *      marked as invalid.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_search_invalidate(
        dbe_search_t* search,
        dbe_trxid_t usertrxid,
        dbe_search_invalidate_t type)
{
        ss_dprintf_1(("dbe_search_invalidate:type=%d\n", type));

        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_mme_search_invalidate((dbe_mme_search_t*)search, usertrxid);
        } else {
            ss_dprintf_2(("dbe_search_invalidate:D-table\n"));
            CHK_SEARCH(search);

            if (type == DBE_SEARCH_INVALIDATE_COMMIT) {
                search->sea_needrestart = TRUE;
            }

            if (type == DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE &&
                search->sea_isolationchange_transparent)
            {
                return;
            }

            if (DBE_TRXID_EQUAL(search->sea_tc.tc_usertrxid, usertrxid)
                || (DBE_TRXID_ISNULL(search->sea_tc.tc_usertrxid)
                    && type == DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE))
            {
                ss_dprintf_2(("dbe_search_invalidate:invalidate, sea_id=%ld\n", (long)search->sea_id));
                search->sea_tc.tc_usertrxid = DBE_TRXID_NULL;
                search->sea_activated = FALSE;
                if (type == DBE_SEARCH_INVALIDATE_ISOLATIONCHANGE) {
                    ss_dprintf_2(("dbe_search_invalidate:set SEA_ISOLATIONCHANGE\n"));
                    SU_BFLAG_SET(search->sea_flags, SEA_ISOLATIONCHANGE);
                }
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_restart_disk
 *
 * Restarts the search with new time constraints. Typically used when
 * cursor is not closed in transaction commit and new transaction starts.
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	trx -
 *
 *
 *	maxtrxnum -
 *
 *
 *	usertrxid -
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
void dbe_search_restart_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid)
{
        ss_dprintf_1(("dbe_search_restart_disk: maxtrxnum = %ld, usertrxid = %ld, sea_id=%ld\n", DBE_TRXNUM_GETLONG(maxtrxnum), DBE_TRXID_GETLONG(usertrxid), (long)search->sea_id));
        CHK_SEARCH(search);

        if (!search->sea_needrestart) {
            /* Nothing else to do. */
            return;
        }

        search->sea_needrestart = FALSE;

        if (!DBE_TRXID_EQUAL(search->sea_tc.tc_usertrxid, DBE_TRXID_NULL)) {
            /* Transaction using this search is still active. */
            ss_dprintf_2(("dbe_search_restart_disk:transaction using this search is still active\n"));
            return;
        }

        if (search->sea_versionedpessimistic) {
            maxtrxnum = dbe_trx_getstmtsearchtrxnum(trx);
        } else if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            maxtrxnum = DBE_TRXNUM_MAX;
            dbe_trx_resetstmtsearchtrxnum(trx);
        }
        
        /* Update time constraints.
         */
        search->sea_tc.tc_maxtrxnum = maxtrxnum;
        search->sea_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        search->sea_tc.tc_usertrxid = usertrxid;
        search->sea_tc.tc_maxtrxid = dbe_trx_getreadtrxid(trx);

        if (search->sea_uselocks || search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            rs_sysi_clearflag(search->sea_cd, RS_SYSI_FLAG_STORAGETREEONLY);
        }

        ss_dassert(DBE_TRXID_CMP_EX(search->sea_tc.tc_usertrxid, search->sea_tc.tc_maxtrxid) <= 0
                   || DBE_TRXID_EQUAL(search->sea_tc.tc_maxtrxid, DBE_TRXID_NULL));

        /* Update search flags.
         */
        search->sea_activated = FALSE;
        search->sea_nseqstep = 0;
        SU_BFLAG_CLEAR(search->sea_flags, SEA_CHECKLOCK);
        search->sea_relhlockedp = FALSE;

        ss_dassert(search->sea_posdvtpl == NULL);
        dynvtpl_free(&search->sea_posdvtpl);

        /* Restart the index search.
         */
        dbe_indsea_reset(
            search->sea_indsea,
            &search->sea_tc,
            NULL,
            NULL);

        /* Restart the data search.
         */
        if (search->sea_getdata) {

            if (search->sea_datasea != NULL) {
                dbe_datasea_done(search->sea_datasea);
            }
            search->sea_datasea = dbe_datasea_init(
                                    search->sea_cd,
                                    search->sea_index,
                                    rs_relh_clusterkey(
                                        search->sea_cd,
                                        search->sea_relh),
                                    &search->sea_tc,
                                    search->sea_data_conslist,
                                    search->sea_reltype == RS_RELTYPE_PESSIMISTIC,
                                    "dbe_search_restart_disk");
        }
        search->sea_datasrk = NULL;
}

void dbe_search_restart(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            dbe_mme_search_restart(
                (dbe_mme_search_t*)search,
                trx);
        } else {
            ss_dassert(dbe_search_gettype(search) == DBE_SEARCH_DBE);
            dbe_trx_setflag(trx, TRX_FLAG_DTABLE);

            if (dbe_trxnum_equal(maxtrxnum, dbe_trxnum_null)){
                dbe_search_restart_disk(
                        search,
                        trx,
                        dbe_trx_getmaxtrxnum(trx),
                        usertrxid);
            } else {
                dbe_search_restart_disk(
                        search,
                        trx,
                        maxtrxnum,
                        usertrxid);
            }
            
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_abortrelid
 *
 * Marks the search as aborted, if it is using relation with id relid.
 *
 * Parameters :
 *
 *	search - in, take
 *		Search object.
 *
 *	relid - in
 *		Relation id.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_search_abortrelid(
        dbe_search_t* search,
        ulong relid)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_mme_search_abortrelid((dbe_mme_search_t*)search, relid);
        } else {
            ss_dprintf_1(("dbe_search_abortrelid: relid = %ld\n", relid));
            CHK_SEARCH(search);

            if ((ulong)search->sea_relid == relid) {
                SU_BFLAG_SET(search->sea_flags, SEA_ABORTED);
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_abortkeyid
 *
 * Marks the search as aborted, if it is using key with id keyid.
 *
 * Parameters :
 *
 *	search - in, take
 *		Search object.
 *
 *	keyid - in
 *		Key id.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_search_abortkeyid(
        dbe_search_t* search,
        ulong keyid)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_mme_search_abortkeyid((dbe_mme_search_t*)search, keyid);
        } else {
            ss_dprintf_1(("dbe_search_abortkeyid: keyid = %ld\n", keyid));
            CHK_SEARCH(search);

            if (rs_pla_getkeyid(search->sea_cd, search->sea_plan) == keyid) {
                SU_BFLAG_SET(search->sea_flags, SEA_ABORTED);
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_markrowold
 *
 * Marks current row old if it has same rel id and lock name.
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	relid -
 *
 *
 *	lockname -
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
void dbe_search_markrowold(
        dbe_search_t* search,
        ulong relid,
        dbe_lockname_t lockname)
{
        ss_dprintf_1(("dbe_search_markrowold: relid = %ld\n", relid));
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            return;
        }
        CHK_SEARCH(search);

        if (search->sea_tref != NULL &&
            (ulong)search->sea_relid == relid &&
            dbe_tref_isvalidlockname(search->sea_tref) &&
            dbe_tref_getcurrentlockname(search->sea_tref) == lockname)
        {
            dbe_tref_removevalidlockname(search->sea_tref);
        }
}

/*#***********************************************************************\
 *
 *		search_createtval
 *
 * Create tuple value that is returned by the search.
 *
 * Parameters :
 *
 *	search - in
 *		Search object.
 *
 *	p_tval - out, give
 *		Returned tuple value.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t search_createtval(
        dbe_search_t* search,
        rs_tval_t** p_tval)
{
        int index;
        dbe_ret_t rc;
        rs_sysi_t* cd;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_key_t* key;
        bool* blobattrs = NULL;
        vtpl_vamap_t* vamap;
        rs_ano_t* selkeyparts;
        rs_ano_t* sellist;
        rs_ano_t kpno;
        size_t vtplsize;
        vtpl_t* vtpl;
        dbe_bkey_t* bkey;

        ss_dprintf_3(("search_createtval\n"));

        rc = DBE_RC_FOUND;
        cd = search->sea_cd;
        ttype = search->sea_ttype;
        tval = *p_tval;
        key = search->sea_key;

        if (tval == NULL) {
            /* Tval not allocated, create a new tval.
             */
            tval = rs_tval_create(cd, ttype);
        }

        bkey = dbe_srk_getbkey(search->sea_srk);
        vtpl = dbe_bkey_getvtpl(bkey);
        vtplsize = VTPL_GROSSLEN(vtpl);
        if (dbe_bkey_isblob(bkey)) {
            blobattrs = dbe_blobinfo_getattrs(
                            vtpl,
                            rs_ttype_nattrs(cd, ttype),
                            NULL);
        }
        if (dbe_bkey_isupdate(bkey)) {
            rs_tval_setrowflags(cd, ttype, tval, RS_AVAL_ROWFLAG_UPDATE);
        }

        vamap = dbe_srk_getvamap(search->sea_srk);
        selkeyparts = search->sea_selkeyparts;
        sellist = search->sea_sellist;

        /* Create a tuple value that is returned.
         */
        for (index = 0; (kpno = selkeyparts[index]) != RS_ANO_NULL; index++) {
            rs_ano_t ano;
            va_t* va;
            bool asc;

            if (kpno == RS_ANO_PSEUDO) {
                /* Skip this attribute. */
                ss_dassert(sellist[index] == RS_ANO_PSEUDO);
                continue;
            }

            ano = sellist[index];
            ss_dassert(ano != RS_ANO_PSEUDO);
            ss_dassert(ano != RS_ANO_NULL);

            va = vtpl_vamap_getva_at(vamap, kpno);

            /* The va value is in ascending format if we retrieved the data
             * from the clustering key by a secondary search or the
             * key part is in ascending order.
             */
            asc = search->sea_getdata || RS_KEYP_ISASCENDING(cd, key, kpno);

            if (blobattrs != NULL && blobattrs[kpno]) {
                /* This is a blob attribute.
                 * We cannot test that va is a blob va, because key
                 * compression may remove special blob va length
                 * information
                 * ss_dassert(va_testblob(va));
                 */
                rs_atype_t* atype;
                rs_aval_t* desc_aval = NULL;

                atype = rs_ttype_atype(cd, ttype, ano);

                if (!asc) {
                    desc_aval = rs_aval_create(cd, atype);
                    rs_aval_setva(cd, atype, desc_aval, va);
                    rs_aval_setdesc(cd, atype, desc_aval);
                    rs_aval_desctoasc(cd, atype, desc_aval);
                    va = rs_aval_va(cd, atype, desc_aval);
                }

                /* A BLOB! Set a blob attribute just as normal attribute.
                 * The BLOB data is read in pieces upon request by the client.
                 */
                {
                    /* Create a blob va. */
                    dynva_t blobva = NULL;
                    char* data;
                    va_index_t datalen;

                    data = va_getdata(va, &datalen);
                    dynva_setblobdata(&blobva, data, datalen, NULL, 0);
                    rs_tval_setva(cd, ttype, tval, ano, blobva);
                    dynva_free(&blobva);
                }
                if (!asc) {
                    rs_aval_free(cd, atype, desc_aval);
                }

            } else {

                /* Set normal attribute. */
                ss_dassert(!va_testblob(va));

                if (asc) {
                    rs_tval_setva(cd, ttype, tval, ano, va);
                } else {
                    rs_atype_t* atype;
                    rs_aval_t* desc_aval;

                    atype = rs_ttype_atype(cd, ttype, ano);
                    desc_aval = rs_aval_create(cd, atype);
                    rs_aval_setva(cd, atype, desc_aval, va);
                    rs_aval_setdesc(cd, atype, desc_aval);
                    rs_aval_desctoasc(cd, atype, desc_aval);
                    va = rs_aval_va(cd, atype, desc_aval);

                    rs_tval_setva(cd, ttype, tval, ano, va);

                    rs_aval_free(cd, atype, desc_aval);
                }
            }
        }
        ss_dassert(sellist[index] == RS_ANO_NULL);

        if (blobattrs != NULL) {
            SsMemFree(blobattrs);
        }
        *p_tval = tval;

        return(rc);
}

static dbe_ret_t search_createtval_simple(
        dbe_search_t* search,
        rs_tval_t** p_tval)
{
        dbe_ret_t rc;
        rs_sysi_t* cd;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_key_t* key;
        rs_ano_t kpno;
        rs_ano_t nparts;
        vtpl_t* vtpl;
        va_t* va;
        dbe_bkey_t* bkey;

        ss_dprintf_3(("search_createtval_simple\n"));
        ss_dassert(rs_sysi_testflag(search->sea_cd, RS_SYSI_FLAG_MYSQL));
        ss_dassert(rs_sysi_testflag(search->sea_cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL));

        rc = DBE_RC_FOUND;
        cd = search->sea_cd;
        ttype = search->sea_ttype;
        tval = *p_tval;
        key = search->sea_key;

        bkey = dbe_srk_getbkey(search->sea_srk);
        vtpl = dbe_bkey_getvtpl(bkey);

        if (tval == NULL
            || dbe_bkey_isblob(bkey) 
            || !rs_key_isprimary(cd, key)) 
        {
            return(search_createtval(search, p_tval));
        }

        ss_dassert(!search->sea_getdata);

        if (dbe_bkey_isupdate(bkey)) {
            rs_tval_setrowflags(cd, ttype, tval, RS_AVAL_ROWFLAG_UPDATE);
        }

        nparts = rs_key_nparts(cd, key);

        va = VTPL_GETVA_AT0(vtpl);

        /* Create a tuple value that is returned.
         */
        for (kpno = 0; kpno < nparts; kpno++) {
            rs_ano_t ano;

            ano = rs_keyp_ano(cd, key, kpno);
            ss_dassert(ano != RS_ANO_PSEUDO);

            if (ano != RS_ANO_NULL) {
                rs_tval_setvaref_flat(cd, ttype, tval, ano, va);
            }

            va = VTPL_SKIPVA(va);
        }

        return(rc);
}

/*#***********************************************************************\
 *
 *		search_createdebugtval
 *
 * Creates a partial tval only from the secondary key. This routine is
 * used only during debugging.
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	p_tval -
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
static dbe_ret_t search_createdebugtval(
        dbe_search_t* search,
        vtpl_vamap_t* vamap,
        rs_tval_t** p_tval)
{
        ss_dprintf_3(("search_createdebugtval\n"));
        ss_dassert(dbe_search_noindexassert);
        ss_dassert(search->sea_getdata);

        if (p_tval == NULL) {

            return(DBE_RC_NOTFOUND);

        } else {
            /* Create a tuple value that is returned. */
            rs_key_t* key;
            void* cd;
            int i;
            static SS_FILE* fp;


            if (fp == NULL) {
                fp = SsFOpenT((char *)"idxerrs.log", (char *)"a+");
                if (fp == NULL) {
                    SsDbgMessage("Error: Unable to open file idxerrs.log\n");
                }
            }

            if (fp != NULL) {
                int i;
                uchar* refdata;
                int reflen;
                refdata = (uchar*)dbe_tref_getvtpl(search->sea_tref);
                reflen = vtpl_grosslen((vtpl_t*)refdata);
                SsFPrintf(fp, "%ld,", search->sea_relid);
                for (i = 0; i < reflen; i++) {
                    SsFPrintf(fp, "%02X", (int)refdata[i]);
                }
                SsFPrintf(fp, "\n");
                SsFFlush(fp);
            }

            cd = search->sea_cd;
            if (*p_tval != NULL) {
                rs_tval_free(cd, search->sea_ttype, *p_tval);
            }
            *p_tval = rs_tval_create(cd, search->sea_ttype);
            key = search->sea_key;
            for (i = 0; search->sea_sellist[i] != RS_ANO_NULL; i++) {
                rs_ano_t kpno;
                if (search->sea_sellist[i] == RS_ANO_PSEUDO) {
                    continue;
                }
                kpno = rs_key_searchkpno_data(cd, key, search->sea_sellist[i]);
                if (kpno != RS_ANO_NULL) {
                    rs_tval_setva(
                        cd,
                        search->sea_ttype,
                        *p_tval,
                        search->sea_sellist[i],
                        vtpl_vamap_getva_at(vamap, kpno));
                }
            }
            return(DBE_RC_FOUND);
        }
}

/*#***********************************************************************\
 *
 *		search_locktuple
 *
 *
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	trx -
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
static dbe_ret_t search_locktuple(
        dbe_search_t* search,
        dbe_trx_t* trx,
        bool* p_newlock)
{
        dbe_lock_reply_t reply;
        dbe_tref_t* tref;
        dbe_ret_t rc;
        su_profile_timer;

        su_profile_start;

        tref = dbe_search_gettref(search, NULL);

        reply = dbe_trx_lock(
                    trx,
                    search->sea_relh,
                    tref,
                    search->sea_lockmode,
                    search->sea_locktimeout,
                    search->sea_bouncelock,
                    p_newlock);

        switch (reply) {
            case LOCK_OK:
                break;
            case LOCK_TIMEOUT:
            case LOCK_DEADLOCK:
                if (search->sea_optimistic_lock) {
                    su_profile_stop("search_locktuple");
                    return(DBE_ERR_LOSTUPDATE);
                } else {
                    su_profile_stop("search_locktuple");
                    return(DBE_ERR_DEADLOCK);
                }
            case LOCK_WAIT:
                search->sea_rc = DBE_RC_WAITLOCK;
                SU_BFLAG_SET(search->sea_flags, SEA_CHECKLOCK);
                su_profile_stop("search_locktuple");
                return(DBE_RC_WAITLOCK);
            default:
                ss_error;
        }
        SU_BFLAG_CLEAR(search->sea_flags, SEA_CHECKLOCK);

        if (search->sea_cursortype == DBE_CURSOR_FORUPDATE &&
            search->sea_reltype == RS_RELTYPE_OPTIMISTIC) {
            /* Optimistic locking may read data updated by someone else.
             * Do lost update check for read tuple to ensure that we
             * can later update the tuple.
             */
            rc = dbe_trx_checklostupdate(trx, tref, search->sea_key, FALSE);
            if (rc != DBE_RC_SUCC) {
                dbe_trx_unlock(
                    trx,
                    search->sea_relh,
                    tref);
            }
        } else {
            rc = DBE_RC_SUCC;
        }

        su_profile_stop("search_locktuple");
        return(rc);
}

/*#***********************************************************************\
 *
 *		search_nextorprev
 *
 *
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	trx - in
 *
 *
 *	p_tval -
 *
 *
 *	nextp -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t search_nextorprev(
        dbe_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t** p_tval,
        bool nextp)
{
        dbe_ret_t rc;
        vtpl_vamap_t* vamap = NULL;
        bool locktuple = FALSE;
        bool newlock = TRUE;
        dbe_trxid_t srk_trxid;
        ss_debug(dbe_trxid_t srk_keytrxid;)

        ss_dprintf_3(("search_nextorprev\n"));
        CHK_SEARCH(search);
        SS_PUSHNAME("search_nextorprev");

        ss_debug(rs_tval_resetexternalflatva(search->sea_cd, search->sea_ttype, *p_tval));

        search->sea_datasrk = NULL;

        /* this 'if' block is for sync conflict resolution
         */
        if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC &&
            !search->sea_relhlockedp) {
            rc = dbe_trx_lockrelh(trx, search->sea_relh, FALSE, -1L);
            if (rc != DBE_RC_SUCC) {
                ss_dprintf_4(("search_nextorprev: return, rc = %s\n", su_rc_nameof(rc)));
                SS_POPNAME;
                return(rc);
            }
            search->sea_relhlockedp = TRUE;
        }

        if (search->sea_flags != 0) {
            /* Special case in the search, check the flags.
             */
            if (SU_BFLAG_TEST(search->sea_flags, SEA_ABORTED)) {
                search->sea_rc = DBE_ERR_LOCKED;
                SS_POPNAME;
                return(DBE_ERR_LOCKED);
            }

            if (search->sea_isolationchange_transparent == FALSE) 
            {
                if (SU_BFLAG_TEST(search->sea_flags, SEA_ISOLATIONCHANGE)) {
                    ss_dprintf_4(("search_nextorprev: flag SEA_ISOLATIONCHANGE, rc = DBE_ERR_CURSORISOLATIONCHANGE\n"));
                    search->sea_rc = DBE_ERR_CURSORISOLATIONCHANGE;
                    SS_POPNAME;
                    return(DBE_ERR_CURSORISOLATIONCHANGE);
                }
            }
            
            search->sea_activated = TRUE;

            if (SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK)) {
                dbe_ret_t lockrc;
                lockrc = search_locktuple(search, trx, NULL);
                if (lockrc != DBE_RC_SUCC) {
                    SS_POPNAME;
                    return(lockrc);
                }
            }
        }

        search->sea_activated = TRUE;

        if (nextp) {

            rc = dbe_indsea_next(
                    search->sea_indsea,
                    dbe_trx_getstmttrxid(trx),
                    &search->sea_srk);
        } else {
            rc = dbe_indsea_prev(
                    search->sea_indsea,
                    dbe_trx_getstmttrxid(trx),
                    &search->sea_srk);
        }

        ss_dprintf_4(("search_nextorprev:indsea rc=%s\n", su_rc_nameof(rc)));

        switch (rc) {

            case DBE_RC_FOUND:
            case DBE_RC_NOTFOUND:
                if (nextp == search->sea_forwardp) {
                    /* Continue to same direction.
                     */
                    if (search->sea_nseqstep == search->sea_nseqsteplimit) {
                        /* Mark search as long sequential searches.
                         */
                        dbe_indsea_setlongseqsea(
                            search->sea_indsea);
                        if (search->sea_getdata) {
                            dbe_datasea_setlongseqsea(search->sea_datasea);
                        }
                    }
                    search->sea_nseqstep++;
                } else {
                    /* Change direction.
                     */
                    search->sea_forwardp = nextp;
                    if (search->sea_nseqstep >= search->sea_nseqsteplimit) {
                        /* Clear long sequential search status from search.
                         */
                        dbe_indsea_clearlongseqsea(
                            search->sea_indsea);
                        if (search->sea_getdata) {
                            dbe_datasea_clearlongseqsea(search->sea_datasea);
                        }
                    }
                    search->sea_nseqstep = 0;
                }
                if (rc == DBE_RC_NOTFOUND) {
                    search->sea_rc = DBE_RC_NOTFOUND;
                    ss_dprintf_4(("search_nextorprev: %d return DBE_RC_NOTFOUND\n", __LINE__));
                    SS_POPNAME;
                    return(DBE_RC_NOTFOUND);
                }
                break;

            case DBE_RC_LOCKTUPLE:
                locktuple = TRUE;
                break;

            case DBE_RC_CANCEL:
            case DBE_RC_END:
                search->sea_rc = rc;
                SS_POPNAME;
                return(rc);

            case DBE_RC_WAITLOCK:
                search->sea_rc = DBE_RC_WAITLOCK;
                SS_POPNAME;
                return(DBE_RC_WAITLOCK);

            case DBE_ERR_UNIQUE_S:
                ss_derror;
                search->sea_rc = DBE_ERR_UNIQUE_S;
                SS_POPNAME;
                return(DBE_ERR_UNIQUE_S);

            case DBE_ERR_DEADLOCK:
                search->sea_rc = DBE_ERR_DEADLOCK;
                SS_POPNAME;
                return(DBE_ERR_DEADLOCK);

            case DBE_ERR_HSBSECONDARY:
                search->sea_rc = DBE_ERR_HSBSECONDARY;
                SS_POPNAME;
                return(DBE_ERR_DEADLOCK);

            default:
                su_rc_derror(rc);
                search->sea_rc = rc;
                SS_POPNAME;
                return(rc);
        }

        if (search->sea_getdata) {
            
            ss_dprintf_4(("search_nextorprev: get data\n"));
            ss_dassert(search->sea_refattrs != NULL);
            ss_dassert(search->sea_datasea != NULL);

            if (search->sea_tref == NULL) {
                search->sea_tref = dbe_tref_init();
            }

            vamap = dbe_srk_getvamap(search->sea_srk);

            srk_trxid = dbe_srk_gettrxid(search->sea_srk);
            ss_debug(srk_keytrxid = dbe_srk_getkeytrxid(search->sea_srk));
            
            dbe_tref_buildsearchtref(
                search->sea_cd,
                search->sea_tref,
                search->sea_plan,
                vamap,
                srk_trxid);
        } else {

            if (search->sea_tref != NULL) {
                dbe_tref_done(search->sea_cd, search->sea_tref);
                search->sea_tref = NULL;
            }
        }

        /*
          Search data from the primary key, this should be done if we are in the getdata step and
          index search does not return DBE_RC_LOCKTUPLE. Index search can return DBE_RC_LOCKTUPLE e.g.
          in the situation when record is delete marked but transaction doing that delete is not
          committed thus at this stage we do not know wheather tuple belong to the result set or
          not.
        */
        if (search->sea_getdata && !locktuple) {
            
            dbe_tref_setreadlevel(
                search->sea_tref,
                search->sea_tc.tc_maxtrxnum);

            rc = dbe_datasea_search(
                    search->sea_datasea,
                    search->sea_tref->tr_vtpl,
                    dbe_trx_getstmttrxid(trx),
                    &search->sea_srk);

            ss_dprintf_4(("search_nextorprev:datasea rc=%s\n", su_rc_nameof(rc)));

            switch (rc) {
                case DBE_RC_FOUND:
                    break;
                case DBE_RC_LOCKTUPLE:
                    locktuple = TRUE;
                    break;
                case DBE_RC_END:

#ifdef SS_DEBUG
                    if (search->sea_data_conslist == NULL) {
                        dbe_trxbuf_t* tb = dbe_index_gettrxbuf(search->sea_index);
                        dbe_trxstate_t ts = dbe_trxbuf_gettrxstate(tb, srk_keytrxid, NULL, NULL);

                        if (!(ts == DBE_TRXST_ABORT || ts == DBE_TRXST_COMMIT)) {
                            SsDbgSet("/LOG/FLU");
                            dbe_bkey_print(NULL, dbe_srk_getbkey(search->sea_srk));
                            ss_error;
                        }
                    }                
#endif
                    search->sea_rc = DBE_RC_NOTFOUND;
                    ss_dprintf_4(("search_nextorprev: %d return DBE_RC_NOTFOUND\n", __LINE__));
                    SS_POPNAME;
                    return(DBE_RC_NOTFOUND);
                case DBE_ERR_ASSERT:
                    ss_dassert(dbe_search_noindexassert);
                    rc = search_createdebugtval(search, vamap, p_tval);
                    search->sea_rc = rc;
                    SS_POPNAME;
                    return(rc);
                default:
                    su_rc_derror(rc);
                    search->sea_rc = rc;
                    SS_POPNAME;
                    return(rc);
            }
        }

        search->sea_rc = rc;

        if (search->sea_uselocks && search->sea_lockmode != LOCK_FREE) {
            dbe_ret_t lockrc;
            lockrc = search_locktuple(search, trx, &newlock);
            if (lockrc != DBE_RC_SUCC) {
                SS_POPNAME;
                return(lockrc);
            }
        }

        SS_POPNAME;

        if (locktuple) {
            ss_dprintf_4(("search_nextorprev: %d return DBE_RC_NOTFOUND\n", __LINE__));
            if (newlock) {
                /* We must do retry in the search. */
                SU_BFLAG_SET(search->sea_flags, SEA_CHECKLOCK);
            }
            return(DBE_RC_NOTFOUND);
        } else if (p_tval == NULL) {
            /* Tuple value is not needed, maybe this call is used to get
             * a tuple count.
             */
            ss_dprintf_4(("search_nextorprev: %d return %s\n", __LINE__, su_rc_nameof(rc)));
            return(rc);
        } else {
            if (rs_sysi_testflag(search->sea_cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL)) {
                rc = search_createtval_simple(search, p_tval);
            } else {
                rc = search_createtval(search, p_tval);
            }
            ss_dprintf_4(("search_nextorprev: %d return %s\n", __LINE__, su_rc_nameof(rc)));
            return(rc);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_nextorprev_disk
 *
 * Returns the next or previous tuple from the search. This function
 * actually advances the search one atomic step, and as a result of the
 * step the tuple may or may not be found. The return value
 * specifies if the tuple was found.
 *
 * Parameters :
 *
 *	search - in out, use
 *		Search object.
 *
 *	trx - in
 *		Current transaction handle
 *
 *	nextp - in
 *		If TRUE, fetch next. Otherwise fetch previous.
 *
 *	p_tval - out, take give (replace)
 *		Pointer to a tuple value variable into where the
 *		newly allocated tuple value is returned. The value
 *		is of same type as tuple value given when the search
 *		was created. This parameter is updated only if the return
 *          value is DBE_RC_FOUND.
 *
 * Return value :
 *
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Tuple not found in this step, it may
 *                        be found in the following step. Parameter
 *                        p_tval is not updated.
 *      DBE_RC_FOUND    - Tuple found, parameter p_tval is
 *                        updated.
 *      DBE_RC_WAITLOCK - Wait for a lock.
 *
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_search_nextorprev_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        bool nextp,
        rs_tval_t** p_tval)
{
        int nloop;
        dbe_ret_t rc;

        ss_dprintf_1(("*** dbe_search_nextorprev_disk: BEGIN, userid = %d, sea_id=%ld\n", dbe_user_getid(dbe_trx_getuser(trx)), (long)search->sea_id));

        SS_PUSHNAME("dbe_search_nextorprev_disk");

        if (search->sea_needrestart) {
            dbe_search_restart(search, trx, dbe_trxnum_null, dbe_trx_getusertrxid(trx));
            ss_dassert(!search->sea_needrestart);
        }

        if (search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
            long changecount;

            ss_dprintf_2(("dbe_search_nextorprev_disk: PESSIMISTIC, table='%s'\n", rs_relh_name(search->sea_cd, search->sea_relh)));
            
            if (dbe_cfg_usepessimisticgate) {
                SS_PUSHNAME("rs_relh_pessgate_enter_shared");
                changecount = rs_relh_pessgate_enter_shared(search->sea_cd, search->sea_relh);
                SS_POPNAME;
            } else {
                changecount = 0;
            }
            if (search->sea_posdvtpl != NULL) {
                ss_dprintf_2(("dbe_search_nextorprev_disk: position the search\n"));
                ss_dassert(!SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK));
                rc = dbe_indsea_setposition(
                        search->sea_indsea,
                        search->sea_posdvtpl);
                su_rc_dassert(rc == DBE_RC_SUCC, rc);
                dynvtpl_free(&search->sea_posdvtpl);
            } else if (SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK)) {
               ss_dprintf_2(("dbe_search_nextorprev_disk: waiting for a lock, retry the search\n"));
                dbe_indsea_setretry(
                    search->sea_indsea,
                    TRUE);
            } else if ((dbe_cfg_usepessimisticgate && changecount != search->sea_lastchangecount)
                       || (!dbe_cfg_usepessimisticgate && dbe_indsea_ischanged(search->sea_indsea)))
            {
                ss_dprintf_2(("dbe_search_nextorprev_disk: table changed, restart the search (%ld != %ld)\n", changecount, search->sea_lastchangecount));
                dbe_indsea_reset_ex(
                    search->sea_indsea,
                    &search->sea_tc,
                    NULL,
                    NULL,
                    FALSE);
            } else {
                ss_dprintf_2(("dbe_search_nextorprev_disk: no changes, continue old search\n"));
            }
            search->sea_lastchangecount = changecount;

            for (nloop = 0; nloop < DBE_MAXLOOP; nloop++) {

                rc = search_nextorprev(search, trx, p_tval, nextp);

                ss_dprintf_2(("dbe_search_nextorprev_disk() %d: rc = %s userid = %d\n",
                    __LINE__, su_rc_nameof(rc), dbe_user_getid(dbe_trx_getuser(trx))));

                if (rc != DBE_RC_NOTFOUND || SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK)) {
                    break;
                }
            }
            if (dbe_cfg_usepessimisticgate) {
                SS_PUSHNAME("rs_relh_pessgate_exit");
                rs_relh_pessgate_exit(search->sea_cd, search->sea_relh);
                SS_POPNAME;
            }

        } else {

            ss_dprintf_2(("dbe_search_nextorprev_disk: OPTIMISTIC, table='%s'\n", rs_relh_name(search->sea_cd, search->sea_relh)));

            if (search->sea_posdvtpl != NULL) {
                ss_dprintf_2(("dbe_search_nextorprev_disk: position the search\n"));
                ss_dassert(!SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK));
                rc = dbe_indsea_setposition(
                        search->sea_indsea,
                        search->sea_posdvtpl);
                su_rc_dassert(rc == DBE_RC_SUCC, rc);
                dynvtpl_free(&search->sea_posdvtpl);
            } else if (SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK)) {
               ss_dprintf_2(("dbe_search_nextorprev_disk: waiting for a lock, retry the search\n"));
                dbe_indsea_setretry(
                    search->sea_indsea,
                    TRUE);
            }

            for (nloop = 0; nloop < DBE_MAXLOOP; nloop++) {

                rc = search_nextorprev(search, trx, p_tval, nextp);

                ss_dprintf_2(("dbe_search_nextorprev_disk() %d: rc = %s userid = %d\n",
                    __LINE__, su_rc_nameof(rc), dbe_user_getid(dbe_trx_getuser(trx))));

                if (rc != DBE_RC_NOTFOUND || SU_BFLAG_TEST(search->sea_flags, SEA_CHECKLOCK)) {
                    break;
                }
            }
        }

        if (rc == DBE_ERR_DEADLOCK) {
            dbe_trx_setdeadlock(trx);
        }

        ss_dprintf_1(("*** dbe_search_nextorprev_disk: RETURN, rc = %s, userid = %d\n",
            su_rc_nameof(rc), dbe_user_getid(dbe_trx_getuser(trx))));
        SS_POPNAME;

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_search_gotoend
 *
 *
 *
 * Parameters :
 *
 *	search -
 *
 *
 *	trx -
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
dbe_ret_t dbe_search_gotoend(
        dbe_search_t* search,
        dbe_trx_t* trx)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            return(dbe_mme_search_gotoend((dbe_mme_search_t*)search, trx));
        } else {
            ss_dprintf_1(("dbe_search_gotoend\n"));
            CHK_SEARCH(search);
            SS_NOTUSED(trx);

            if (search->sea_needrestart) {
                dbe_search_restart(search, trx, dbe_trxnum_null, dbe_trx_getusertrxid(trx));
                ss_dassert(!search->sea_needrestart);
                ss_dassert(dbe_trx_isflag(trx, TRX_FLAG_DTABLE));
            } else {
                dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            }

            search->sea_forwardp = TRUE;

            if (dbe_cfg_usepessimisticgate && search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
                rs_relh_pessgate_enter_shared(search->sea_cd, search->sea_relh);
            }

            dbe_indsea_gotoend(search->sea_indsea);

            if (dbe_cfg_usepessimisticgate && search->sea_reltype == RS_RELTYPE_PESSIMISTIC) {
                rs_relh_pessgate_exit(search->sea_cd, search->sea_relh);
            }

            return(DBE_RC_SUCC);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_setposition
 *
 *
 *
 * Parameters :
 *
 *	search -
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
dbe_ret_t dbe_search_setposition(
        dbe_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t* tval)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_trx_setflag(trx, TRX_FLAG_MTABLE);
            return dbe_mme_search_setposition((dbe_mme_search_t*)search, trx, tval);
        } else {
            int i;
            int nparts;
            dynvtpl_t dvtpl = NULL;
            rs_sysi_t* cd;
            rs_ttype_t* ttype;
            rs_key_t* key;

            ss_dprintf_1(("dbe_search_setposition\n"));
            CHK_SEARCH(search);
            SS_NOTUSED(trx);

            if (search->sea_needrestart) {
                dbe_search_restart(search, trx, dbe_trxnum_null, dbe_trx_getusertrxid(trx));
                ss_dassert(!search->sea_needrestart);
            } else {
                dbe_trx_setflag(trx, TRX_FLAG_DTABLE);
            }

            cd = search->sea_cd;
            ttype = search->sea_ttype;
            key = search->sea_key;

            ss_dassert(dbe_trx_isflag(trx, TRX_FLAG_DTABLE));

            dynvtpl_setvtpl(&dvtpl, VTPL_EMPTY);

            nparts = rs_key_nparts(cd, key);

            for (i = 0; i < nparts; i++) {
                rs_ano_t ano;
                va_t* va;

                if (rs_keyp_isconstvalue(cd, key, i)) {
                    va = rs_keyp_constvalue(cd, key, i);
                } else {
                    ano = rs_keyp_ano(cd, key, i);
                    va = rs_tval_va(cd, ttype, tval, ano);
                }
                dynvtpl_appva(&dvtpl, va);
            }

            ss_dassert(search->sea_posdvtpl == NULL);
            dynvtpl_free(&search->sea_posdvtpl);

            search->sea_posdvtpl = dvtpl;

            return(DBE_RC_SUCC);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_gettref
 *
 * Returns the tuple reference of the current tuple in search.
 *
 * Parameters :
 *
 *	search - in, use
 *
 *
 * Return value - ref :
 *
 *      Pointer to a local copy of tuple reference.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_tref_t* dbe_search_gettref(
        dbe_search_t*   search,
        rs_tval_t*      tval)
{
        /* XXX - kludge, NULL cd, but what can I do? */
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            return dbe_mme_search_gettref((dbe_mme_search_t*)search, tval);
        } else {
            ss_dprintf_1(("dbe_search_gettref\n"));
            CHK_SEARCH(search);
            ss_assert(search->sea_refattrs != NULL);
            su_rc_assert(search->sea_rc == DBE_RC_FOUND ||
                        search->sea_rc == DBE_RC_LOCKTUPLE ||
                        search->sea_rc == DBE_RC_WAITLOCK,
                search->sea_rc);

            if (search->sea_tref == NULL) {

                search->sea_tref = dbe_tref_init();

                dbe_tref_buildsearchtref(
                    search->sea_cd,
                    search->sea_tref,
                    search->sea_plan,
                    dbe_srk_getvamap(search->sea_srk),
                    dbe_srk_gettrxid(search->sea_srk));
                dbe_tref_setreadlevel(
                    search->sea_tref,
                    search->sea_tc.tc_maxtrxnum);
            }
            return(search->sea_tref);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_getclustvtpl
 *
 * Returns the clustering key v-tuple, if it is retrieved by the
 * search.
 *
 * Parameters :
 *
 *	search - in, use
 *
 *
 *	p_srk - out, ref
 *
 *
 * Return value :
 *
 *      TRUE    - clustering v-tuple returned in *p_srk
 *      FALSE   - clustering key not used in search
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_search_getclustvtpl(
        dbe_search_t* search,
        dbe_srk_t** p_srk)
{
        ss_dprintf_1(("dbe_search_getclustvtpl\n"));
        CHK_SEARCH(search);
        ss_dassert(dbe_search_gettype(search) != DBE_SEARCH_MME);

        if (!search->sea_activated ||
            SU_BFLAG_TEST(search->sea_flags, SEA_ABORTED|SEA_ISOLATIONCHANGE) ||
            search->sea_rc != DBE_RC_FOUND) {

            return(FALSE);
        }

        if (search->sea_getdata ||
            rs_pla_usingclusterkey(search->sea_cd, search->sea_plan)) {
            /* Data tuple fetched explicitly, or using clustering key in
             * search.
             */
            *p_srk = search->sea_srk;
            return(TRUE);

        } else if (search->sea_datasea) {
            /* There is a data search, but getdata flag is not on. This is
             * the case where data is searched by function dbe_search_getaval.
             */
            ss_dassert(search->sea_datasrk != NULL);
            *p_srk = search->sea_datasrk;
            return(TRUE);
        }

        return(FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_search_getsearchinfo
 *
 * Returns the search info and the last returned key value.
 *
 * Parameters :
 *
 *	sea - in, use
 *		Search object.
 *
 *	p_plan - out, give
 *
 *
 *	p_lastkey - out, give
 *
 *
 *	p_lasttrxid - out, give
 *
 *
 * Return value :
 *
 *      TRUE    if the search was started
 *      FALSE   if the search was never really started
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_search_getsearchinfo(
        dbe_search_t* sea,
        rs_pla_t** p_plan,
        dynvtpl_t* p_lastkey,
        dbe_trxid_t* p_lasttrxid)
{
        ss_dprintf_1(("dbe_search_getsearchinfo: sea_id=%ld\n", (long)sea->sea_id));

        if (sea->sea_hdr.sh_type == DBE_SEARCH_MME) {
            *p_lasttrxid = DBE_TRXID_NULL;
            return dbe_mme_search_getsearchinfo((dbe_mme_search_t*)sea, p_plan);
        }

        CHK_SEARCH(sea);

        if (!sea->sea_activated || sea->sea_lockmode != LOCK_FREE) {
            return(FALSE);
        } else {
            bool succp;
            succp = dbe_indsea_getlastkey(
                        sea->sea_indsea,
                        p_lastkey,
                        p_lasttrxid);
            if (succp) {
                sea->sea_activated = FALSE;
                rs_pla_link(sea->sea_cd, sea->sea_plan);
                *p_plan = sea->sea_plan;
            }

            return(succp);
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_setoptinfo
 *
 *
 *
 * Parameters :
 *
 *	sea -
 *
 *
 *	tuplecount -
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
void dbe_search_setoptinfo(
        dbe_search_t* sea,
        ulong tuplecount)
{
        if (sea->sea_hdr.sh_type == DBE_SEARCH_MME) {
            dbe_mme_search_setoptinfo((dbe_mme_search_t*)sea, tuplecount);
        } else {
            ss_dprintf_1(("dbe_search_setoptinfo: tuplecount = %ld\n", tuplecount));
            CHK_SEARCH(sea);

            if (tuplecount >= sea->sea_nseqsteplimit) {
                dbe_indsea_setlongseqsea(sea->sea_indsea);
                if (sea->sea_getdata) {
                    dbe_datasea_setlongseqsea(sea->sea_datasea);
                }
            }
        }
}

/*##**********************************************************************\
 *
 *		dbe_search_setmaxpoolblocks
 *
 *
 *
 * Parameters :
 *
 *	sea -
 *
 *
 *	maxpoolblocks -
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
void dbe_search_setmaxpoolblocks(
        dbe_search_t* sea,
        ulong maxpoolblocks)
{
        ss_dprintf_1(("dbe_search_setmaxpoolblocks: maxpoolblocks = %ld\n", maxpoolblocks));
        CHK_SEARCH(sea);
        ss_dassert(sea->sea_hdr.sh_type != DBE_SEARCH_MME);

        dbe_indsea_setmaxpoolblocks(
            sea->sea_indsea,
            maxpoolblocks);
}

#ifdef SS_QUICKSEARCH

static dbe_ret_t quicksearch_nextorprev(
        dbe_quicksearch_t* qs,
        bool nextp)
{
        dbe_search_t* search;

        search = qs->qs_search;

        CHK_SEARCH(search);
        ss_dprintf_3(("quicksearch_nextorprev\n"));

        if (search->sea_qsblocksearch) {
            ss_dprintf_4(("quicksearch_nextorprev:blocksearch\n"));
            ss_dassert(nextp);
            if (search->sea_rc == SU_SUCCESS) {
                search->sea_rc = DBE_RC_FOUND;
            } else {
                search->sea_rc = dbe_btrsea_getnextblock(
                                    qs->qs_indexsearch);
            }
        } else {
            ss_dprintf_4(("quicksearch_nextorprev:tuplesearch\n"));
            do {
                if (nextp) {
                    search->sea_rc = dbe_btrsea_getnext(
                                        qs->qs_indexsearch,
                                        &search->sea_srk);
                } else {
                    search->sea_rc = dbe_btrsea_getprev(
                                        qs->qs_indexsearch,
                                        &search->sea_srk);
                }
            } while (search->sea_rc == DBE_RC_NOTFOUND);
        }

        return(search->sea_rc);
}

static void quicksearch_write(
        dbe_quicksearch_t* qs)
{
        int nattrs;
        int i;
        vtpl_vamap_t* vamap;
        dbe_search_t* search;
        rs_ano_t* selkeyparts;
        rs_ano_t* sellist;
        rs_ano_t kpno;
        long datalen;
        rs_sysi_t* cd;
        rs_ttype_t* ttype;
        va_t** vatable;
        int va_null_grosslen;

        search = qs->qs_search;

        CHK_SEARCH(search);
        su_rc_dassert(search->sea_rc == DBE_RC_FOUND, search->sea_rc);
        ss_dprintf_3(("quicksearch_write\n"));

        cd = search->sea_cd;

        if (search->sea_qsblocksearch) {
            int len;
            char* p;
            ss_dprintf_4(("quicksearch_write:blocksearch\n"));
            dbe_btrsea_getnodedata(qs->qs_indexsearch, &p, &len);
            (*qs->qs_writelong)(qs->qs_rses, len);
            (*qs->qs_writedata)(qs->qs_rses, p, len);
        } else {
            ss_dprintf_4(("quicksearch_write:tuplesearch\n"));
            ttype = search->sea_ttype;
            vamap = dbe_srk_getvamap(search->sea_srk);
            selkeyparts = search->sea_selkeyparts;
            sellist = search->sea_sellist;

            vatable = search->sea_qsvatable;
            nattrs = rs_ttype_nattrs(cd, ttype);
            va_null_grosslen = VA_GROSSLEN(VA_NULL);

            datalen = nattrs * va_null_grosslen;

            /* Set va pointers to vatable and get gross data length.
             */
            for (i = 0; (kpno = selkeyparts[i]) != RS_ANO_NULL; i++) {
                va_t* va;
                rs_ano_t ano;

                if (kpno == RS_ANO_PSEUDO) {
                    continue;
                }

                ano = sellist[i];
                ss_dassert(ano != RS_ANO_PSEUDO);
                ss_dassert(ano != RS_ANO_NULL);
                ss_dassert(ano >= 0);
                ss_dassert(ano < nattrs);

                va = vtpl_vamap_getva_at(vamap, kpno);

                vatable[ano] = va;

                datalen -= va_null_grosslen;
                datalen += VA_GROSSLEN(va);
            }

            (*qs->qs_writelong)(qs->qs_rses, nattrs);
            (*qs->qs_writelong)(qs->qs_rses, datalen);

            /* Write the actual va-data.
             */
            for (i = 0; i < nattrs; i++) {
                va_t* va;
                va = vatable[i];
                if (va == NULL) {
                    (*qs->qs_writedata)(qs->qs_rses, VA_NULL, va_null_grosslen);
                } else {
                    (*qs->qs_writedata)(qs->qs_rses, va, VA_GROSSLEN(va));
                }
            }
        }
}

void* dbe_search_getquicksearch(
        dbe_search_t* sea,
        bool longsearch)
{
        CHK_SEARCH(sea);

        if (!rs_relh_isnocheck(sea->sea_cd, sea->sea_relh)
            || sea->sea_reltype == RS_RELTYPE_PESSIMISTIC
            || !rs_key_isclustering(sea->sea_cd, sea->sea_key)) {
            return(NULL);
        } else {
            int nattrs;
            nattrs = rs_ttype_nattrs(sea->sea_cd, sea->sea_ttype);
            sea->sea_quicksearch.qs_search = sea;
            sea->sea_quicksearch.qs_indexsearch = dbe_indsea_getquicksearch(
                                                    sea->sea_indsea,
                                                    longsearch);
            sea->sea_quicksearch.qs_nextorprev = quicksearch_nextorprev;
            sea->sea_quicksearch.qs_write = quicksearch_write;
            sea->sea_qsvatable = SsMemCalloc(nattrs, sizeof(sea->sea_qsvatable[0]));
            sea->sea_qsblocksearch = longsearch >= 3;
            sea->sea_rc = SU_SUCCESS;
            return(&sea->sea_quicksearch);
        }
}

#endif /* SS_QUICKSEARCH */

/*##**********************************************************************\
 *
 *      dbe_search_newplan
 *
 * Inform higher level (sp) cursor using this search that it should replan
 * itself.
 *
 * Parameters:
 *      search - in, use
 *          the search
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_search_newplan(
        dbe_search_t*   search,
        ulong           relid)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            dbe_mme_search_newplan((dbe_mme_search_t*)search, relid);
        } else {
            ss_dprintf_1(("dbe_search_newplan\n"));
            CHK_SEARCH(search);

            /* ss_dassert(search->sea_p_newplan != NULL); */
            if (search->sea_p_newplan != NULL
                && (ulong)search->sea_relid == relid) {
                *(search->sea_p_newplan) = TRUE;
            }
        }
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_search_printinfoheader
 *
 *
 *
 * Parameters :
 *
 *      fp -
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
void dbe_search_printinfoheader(void* fp)
{
        SsFprintf(fp, "    %-8s %-6s %-5s %-5s %-6s %-5s %-3s %-4s %-6s %s\n",
            "Id",
            "Iseaid",
            "Relid",
            "Keyid",
            "Readlv",
            "Getdt",
            "Act",
            "Abrt",
            "NSeqSt",
            "NSeqStLim");
}

/*##**********************************************************************\
 *
 *		dbe_search_printinfo
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	sea -
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
void dbe_search_printinfo(
        void* fp,
        dbe_search_t* sea)
{
        if (sea->sea_hdr.sh_type == DBE_SEARCH_MME) {
            dbe_mme_search_printinfo(fp, (dbe_mme_search_t*)sea);
        } else {
            int seaid;

            seaid = dbe_indsea_getseaid(sea->sea_indsea);

            SsFprintf(fp, "    %-8ld %-6ld %-5ld %-5ld %-6lu %-5d %-3d %-4d %-6ld %ld\n",
                (long)sea->sea_id,
                seaid,
                sea->sea_relid,
                rs_pla_getkeyid(sea->sea_cd, sea->sea_plan),
                DBE_TRXNUM_GETLONG(sea->sea_tc.tc_maxtrxnum),
                sea->sea_getdata,
                sea->sea_activated,
                SU_BFLAG_TEST(sea->sea_flags, SEA_ABORTED|SEA_ISOLATIONCHANGE) != 0,
                sea->sea_nseqstep,
                sea->sea_nseqsteplimit);
        }
}

#endif
