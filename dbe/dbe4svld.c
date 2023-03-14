/*************************************************************************\
**  source       * dbe4svld.c
**  directory    * dbe
**  description  * Search validation routines for transaction
**               * validation.
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

This module implements a search read operation validation routines
used by the transaction validation system.

This module uses the index validation routines that return key values
from a single index. If the actual data must be dereferenced, it is
done in this module and necessary constraint checks are done for
the data.

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

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>

#include <su0list.h>

#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0relh.h>
#include <rs0pla.h>

#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe5ivld.h"
#include "dbe5dsea.h"
#include "dbe4srch.h"
#include "dbe4svld.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0user.h"
#include "dbe0trx.h"
#include "dbe0tref.h"

#define CHK_SEAVLD(sv) ss_dassert(SS_CHKPTR(sv) && (sv)->sv_chk == DBE_CHK_SEAVLD)

/* The search validation structure.
*/

struct dbe_seavld_st {
        ss_debug(dbe_chk_t sv_chk;)
        dbe_user_t*     sv_user;       /* User of the search. */
        rs_pla_t*       sv_plan;       /* Search plan. */
        dbe_trx_t*      sv_trx;
        dbe_trxnum_t    sv_maxtrxnum;
        rs_key_t*       sv_key;
        su_list_t*      sv_refattrs;   /* Rules to create tuple ref. */
        bool            sv_getdata;    /* TRUE if the data tuple must be
                                          referenced. */
        su_list_t*      sv_data_conslist; /* Data constraint list. */
        rs_ano_t*       sv_selattrs;   /* Selected attributes. */
        dbe_tref_t*     sv_tref;       /* Current tuple reference. */
        dbe_indvld_t    sv_indvld;     /* Database index validate search.
                                          The actual key values for the
                                          search come from this
                                          index search. */
        dbe_datasea_t*  sv_datasea;
        dbe_btrsea_timecons_t sv_tc;   /* Time range constraints. */
};

/*##**********************************************************************\
 * 
 *		dbe_seavld_init
 * 
 * Initializes a validation search.
 * 
 * Parameters : 
 * 
 *	user - in, hold
 *		User object of the user of the search.
 *		
 *	trx - in, use
 *          Transaction handle.
 *		
 *	plan - in, use
 *		Search plan. May be also NULL, if only a key
 *		range specified by search_range is checked with
 *		no additional constraints.
 *		
 *	search_range - in, use
 *		The key range for the validate search.
 *		
 *	maxtrxnum - in
 *		
 *		
 *	mintrxnum - in
 *		
 *		
 *	earlyvld - in
 *		
 *		
 * Return value - give : 
 * 
 *      Pointer to the validation search object.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_seavld_t* dbe_seavld_init(
        dbe_user_t* user,
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dbe_searchrange_t* search_range,
        dbe_trxnum_t maxtrxnum,
        dbe_trxnum_t mintrxnum,
        bool earlyvld)
{
        void* cd;
        dbe_seavld_t* seavld;
        su_list_t* key_conslist;

        ss_dassert(user != NULL);
        ss_dassert(plan != NULL);

        cd = dbe_user_getcd(user);

        ss_dassert(!rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY));

        seavld = SsMemAlloc(sizeof(dbe_seavld_t));

        ss_debug(seavld->sv_chk = DBE_CHK_SEAVLD);
        seavld->sv_user = user;
        seavld->sv_tref = dbe_tref_init();
        seavld->sv_trx = trx;
        seavld->sv_maxtrxnum = maxtrxnum;

        if (plan == NULL) {

            seavld->sv_plan = NULL;
            seavld->sv_key = NULL;
            seavld->sv_refattrs = NULL;
            seavld->sv_selattrs = NULL;
            seavld->sv_getdata = FALSE;
            seavld->sv_data_conslist = NULL;
            key_conslist = NULL;

        } else {

            rs_pla_link(cd, plan);
            seavld->sv_plan = plan;

            seavld->sv_refattrs = rs_pla_get_tuple_reference(cd, plan);
            seavld->sv_key = rs_pla_getkey(cd, plan);

            if (seavld->sv_refattrs == NULL
                || su_list_length(seavld->sv_refattrs) == 0) {
                seavld->sv_refattrs = NULL;
            }

            rs_pla_get_select_list(
                cd,
                plan,
                &seavld->sv_selattrs,
                &seavld->sv_getdata);

            if (!seavld->sv_getdata) {
                seavld->sv_refattrs = NULL;
            }

            seavld->sv_data_conslist = rs_pla_get_data_constraints(cd, plan);
            key_conslist = rs_pla_get_key_constraints(cd, plan);
        }

        seavld->sv_tc.tc_maxtrxnum = maxtrxnum;
        seavld->sv_tc.tc_mintrxnum = DBE_TRXNUM_MIN;
        seavld->sv_tc.tc_usertrxid = dbe_trx_getusertrxid(trx);
        seavld->sv_tc.tc_maxtrxid = DBE_TRXID_MAX;
        seavld->sv_tc.tc_trxbuf = NULL;

        if (seavld->sv_getdata) {
            seavld->sv_datasea = dbe_datasea_init(
                                    cd,
                                    dbe_user_getindex(user),
                                    rs_pla_getkey(cd, plan),
                                    &seavld->sv_tc,
                                    seavld->sv_data_conslist,
                                    FALSE,
                                    "dbe_seavld_init");
        } else {
            seavld->sv_datasea = NULL;
        }
        ss_debug(seavld->sv_indvld.iv_chk = 0);

        dbe_indvld_initbuf(
            &seavld->sv_indvld,
            cd,
            dbe_user_getindex(user),
            seavld->sv_tc.tc_usertrxid,
            maxtrxnum,
            mintrxnum,
            search_range,
            key_conslist,
            seavld->sv_key,
            DBE_KEYVLD_NONE,
            earlyvld,
            FALSE);

        return(seavld);
}

/*##**********************************************************************\
 * 
 *		dbe_seavld_done
 * 
 * Releases the validation search object (ends the search).
 * 
 * Parameters : 
 * 
 *	seavld - in, take
 *		Search object.
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_seavld_done(dbe_seavld_t* seavld)
{
        CHK_SEAVLD(seavld);

        dbe_tref_done(dbe_user_getcd(seavld->sv_user), seavld->sv_tref);
        if (seavld->sv_datasea != NULL) {
            dbe_datasea_done(seavld->sv_datasea);
        }
        dbe_indvld_donebuf(&seavld->sv_indvld);
        if (seavld->sv_plan != NULL) {
            rs_pla_done(dbe_user_getcd(seavld->sv_user), seavld->sv_plan);
        }

        SsMemFree(seavld);
}

/*##**********************************************************************\
 * 
 *		dbe_seavld_next
 * 
 * Returns the next tuple from the validation search. This function
 * actually advances the search one atomic step, and as the result of
 * the step the next tuple may or may not be found. The return value
 * specifies if the next tuple was found.
 * 
 * Parameters : 
 * 
 *	seavld - in out, use
 *		Search object.
 *		
 *	p_tuple_trxid - out
 *		Trx id of the found tuple is stored into *p_tuple_trxid, if
 *		p_tuple_trxid != NULL and return value is DBE_RC_FOUND.
 *		
 * Return value : 
 * 
 *      DBE_RC_END      - End of search.
 *      DBE_RC_NOTFOUND - Next tuple not found in this step, it may
 *                        be found in the next step.
 *      DBE_RC_FOUND    - Next tuple found.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_seavld_next(
        dbe_seavld_t* seavld,
        dbe_trxid_t* p_tuple_trxid)
{
        dbe_ret_t rc;
        dbe_srk_t* srk;

        ss_dprintf_1(("dbe_seavld_next\n"));

        CHK_SEAVLD(seavld);
        ss_dassert(!rs_sysi_testflag(dbe_user_getcd(seavld->sv_user), RS_SYSI_FLAG_STORAGETREEONLY));

        rc = dbe_indvld_next(&seavld->sv_indvld, &srk);

        if (rc != DBE_RC_FOUND) {
            ss_dprintf_2(("dbe_seavld_next:not found, rc = %d\n", rc));
            return(rc);
        }

        ss_dprintf_2(("dbe_seavld_next:index v-tuple\n"));
        ss_output_2(dbe_bkey_dprintvtpl(2, dbe_srk_getvtpl(srk)));

        if (seavld->sv_getdata) {
            void* cd;

            ss_dprintf_2(("dbe_seavld_next:get data\n"));
            ss_dassert(seavld->sv_refattrs != NULL);
            ss_dassert(seavld->sv_datasea != NULL);

            cd = dbe_user_getcd(seavld->sv_user);

            dbe_tref_buildsearchtref(
                cd,
                seavld->sv_tref,
                seavld->sv_plan,
                dbe_srk_getvamap(srk),
                dbe_srk_gettrxid(srk));

            rc = dbe_datasea_search(
                    seavld->sv_datasea,
                    seavld->sv_tref->tr_vtpl,
                    DBE_TRXID_NULL,
                    &srk);

            switch (rc) {
                case DBE_RC_FOUND:
                    break;
                case DBE_RC_END:
                    ss_dprintf_2(("dbe_seavld_next:not found\n"));
                    ss_dassert(seavld->sv_data_conslist != NULL);
                    return(DBE_RC_NOTFOUND);
                default:
                    su_rc_derror(rc);
                    return(rc);
            }

            ss_output_begin(srk != NULL)
                ss_dprintf_2(("dbe_seavld_next:data v-tuple\n"));
                ss_output_2(dbe_bkey_dprintvtpl(2, dbe_srk_getvtpl(srk)));
            ss_output_end
        }

        if (p_tuple_trxid != NULL) {
            *p_tuple_trxid = dbe_srk_gettrxid(srk);
        }

        return(DBE_RC_FOUND);
}
