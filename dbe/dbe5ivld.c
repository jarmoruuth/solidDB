/*************************************************************************\
**  source       * dbe5ivld.c
**  directory    * dbe
**  description  * Index validate routines for transaction validation.
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

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>

#include <uti0vtpl.h>

#include <su0list.h>

#include "dbe9type.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe5ivld.h"
#include "dbe0type.h"
#include "dbe0erro.h"

#define CHK_INDVLD(iv) ss_dassert(SS_CHKPTR(iv) && (iv)->iv_chk == DBE_CHK_INDVLD)

/*##**********************************************************************\
 * 
 *		dbe_indvld_init
 * 
 * Initializes index validation search.
 * 
 * Parameters : 
 * 
 *	cd - in, hold
 *		client data
 *		
 *	index - in, hold
 *		index system
 *		
 *	usertrxid - in
 *		user transaction id
 *		
 *	maxtrxnum - in
 *		maximum transaction number visible in the search
 *		
 *	mintrxnum - in
 *		minimum transaction number visible in the search
 *		
 *      sr - in, use
 *		search range
 *		
 *      conslist - in, hold
 *		search constraints, NULL if none
 *		
 *      earlyvld - in
 *		If TRUE, do early validate.
 *		
 * Return value - give : 
 * 
 *      pointer to the index search
 * 
 * Comments  : 
 * 
 *      This function depends on tuple ordering (search keyword
 *      TUPLE_ORDERING for other functions).
 * 
 * Globals used : 
 */
dbe_indvld_t* dbe_indvld_initbuf(
        dbe_indvld_t* indvld,
        rs_sysi_t* cd,
        dbe_index_t* index,
        dbe_trxid_t usertrxid,
        dbe_trxnum_t maxtrxnum,
        dbe_trxnum_t mintrxnum,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        rs_key_t* key,
        dbe_keyvld_t keyvldtype,
        bool earlyvld,
        bool pessimistic)
{
        ss_dprintf_1(("dbe_indvld_init:pessimistic=%d\n", pessimistic));
        ss_pprintf_2(("trx range = [%ld,%ld]\n",
            DBE_TRXNUM_GETLONG(mintrxnum), DBE_TRXNUM_GETLONG(maxtrxnum)));
        ss_aassert(!rs_sysi_testflag(cd, RS_SYSI_FLAG_STORAGETREEONLY));
        ss_dassert(indvld->iv_chk == 0);
        ss_debug(indvld->iv_chk = DBE_CHK_INDVLD);
        
        indvld->iv_index = index;
        indvld->iv_cd = cd;
        indvld->iv_beginkey = NULL;
        indvld->iv_endkey = NULL;

        if (sr->sr_minvtpl == NULL) {
            indvld->iv_kc.kc_beginkey = NULL;
        } else {
            indvld->iv_beginkey = dbe_bkey_initleaf(
                                    cd,
                                    dbe_index_getbkeyinfo(index),
                                    DBE_TRXNUM_NULL,
                                    DBE_TRXID_NULL,
                                    sr->sr_minvtpl);
            dbe_bkey_setdeletemark(indvld->iv_beginkey);
            indvld->iv_kc.kc_beginkey = indvld->iv_beginkey;
        }
        if (sr->sr_maxvtpl == NULL) {
            indvld->iv_kc.kc_endkey = NULL;
        } else {
            indvld->iv_endkey = dbe_bkey_initleaf(
                                    cd,
                                    dbe_index_getbkeyinfo(index),
                                    DBE_TRXNUM_NULL,
                                    DBE_TRXID_MAX,
                                    sr->sr_maxvtpl);
            indvld->iv_kc.kc_endkey = indvld->iv_endkey;
        }
        indvld->iv_kc.kc_conslist = conslist;
        indvld->iv_kc.kc_cd = cd;
        indvld->iv_kc.kc_key = key;

        indvld->iv_tc.tc_mintrxnum = mintrxnum;
        indvld->iv_tc.tc_maxtrxnum = maxtrxnum;
        indvld->iv_tc.tc_usertrxid = usertrxid;
        indvld->iv_tc.tc_maxtrxid = DBE_TRXID_MAX;
        indvld->iv_tc.tc_trxbuf = dbe_index_gettrxbuf(index);

        dbe_btrsea_initbufvalidate(
            &indvld->iv_bonsaisearch,
            dbe_index_getbonsaitree(index),
            &indvld->iv_kc,
            &indvld->iv_tc,
            FALSE,          /* mergesea */
            !pessimistic,   /* validatesea */
            keyvldtype,
            earlyvld);

        ss_debug(indvld->iv_lastkey = dbe_bkey_init(dbe_index_getbkeyinfo(index)));
        ss_debug(dbe_bkey_setsearchminvtpl(indvld->iv_lastkey));

        return(indvld);
}

/*##**********************************************************************\
 * 
 *		dbe_indvld_done
 * 
 * Releases resources from index validate search.
 * 
 * Parameters : 
 * 
 *	indvld - in, take
 *		index search
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_indvld_donebuf(
        dbe_indvld_t* indvld)
{
        ss_dprintf_1(("dbe_indvld_done\n"));

        CHK_INDVLD(indvld);

        dbe_btrsea_donebuf(&indvld->iv_bonsaisearch);

        if (indvld->iv_beginkey != NULL) {
            dbe_bkey_done_ex(indvld->iv_cd, indvld->iv_beginkey);
        }
        if (indvld->iv_endkey != NULL) {
            dbe_bkey_done_ex(indvld->iv_cd, indvld->iv_endkey);
        }
        ss_debug(dbe_bkey_done(indvld->iv_lastkey));
        ss_debug(indvld->iv_chk = 0);
}

/*##**********************************************************************\
 * 
 *		dbe_indvld_next
 * 
 * Returns the next v-tuple in the search. If the next v-tuple is not
 * found, returns NULL. NULL return value means end of search.
 * 
 * Parameters : 
 * 
 *	indvld - in out, use
 *		index search
 *
 *      p_srk - out, ref
 *          Search results is returned in *p_srk.
 *
 * Return value : 
 * 
 *      DBE_RC_END
 *      DBE_RC_FOUND
 *      DBE_RC_NOTFOUND
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_indvld_next(
        dbe_indvld_t* indvld,
        dbe_srk_t** p_srk)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_indvld_next\n"));

        CHK_INDVLD(indvld);
        ss_dassert(p_srk != NULL);
        ss_aassert(!rs_sysi_testflag(indvld->iv_kc.kc_cd, RS_SYSI_FLAG_STORAGETREEONLY));

        rc = dbe_btrsea_getnext(&indvld->iv_bonsaisearch, p_srk);

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        if (rc == DBE_RC_FOUND) {
            ss_pprintf_2(("dbe_indvld_next:found key\n"));
            ss_poutput_2(dbe_bkey_dprint(2, dbe_srk_getbkey(*p_srk)));
            ss_dassert(dbe_bkey_istrxid(dbe_srk_getbkey(*p_srk)));
            ss_dassert(dbe_bkey_compare(indvld->iv_lastkey, dbe_srk_getbkey(*p_srk)) < 0);
            ss_debug(dbe_bkey_copy(indvld->iv_lastkey, dbe_srk_getbkey(*p_srk)));
        }
#endif /* defined(AUTOTEST_RUN) || defined(SS_DEBUG) */

        return(rc);
}
