/*************************************************************************\
**  source       * dbe6srk.h
**  directory    * dbe
**  description  * Search return key type.
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


#ifndef DBE6SRK_H
#define DBE6SRK_H

#include <ssc.h>
#include <ssthread.h>

#include <uti0vtpl.h>

#include <rs0sysi.h>
#include <rs0key.h>

#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe6bkey.h"

#define CHK_SRK(srk) ss_dassert(SS_CHKPTR(srk) && (srk)->srk_chk == DBE_CHK_SRK)

struct dbe_srk_st {
        ss_debug(dbe_chk_t      srk_chk;)
        ss_autotest_or_debug(dbe_dynbkey_t srk_tmpk;)
        ss_autotest_or_debug(bool          srk_active;)
        dbe_bkey_t*     srk_dk;
        dbe_bkey_t*     srk_prevdk;
        vtpl_vamap_t*   srk_vamap;
        bool            srk_vamap_filled;
        dbe_keypos_t    srk_keypos;
        dbe_trxid_t     srk_trxid;
};

dbe_srk_t* dbe_srk_initbuf(
        dbe_srk_t* srk,
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* bkeyinfo);

void dbe_srk_donebuf(
        dbe_srk_t* srk,
        rs_sysi_t* cd);

void dbe_srk_copy(
        dbe_srk_t* target_srk,
        dbe_srk_t* source_srk);

SS_INLINE void dbe_srk_setbkey(
        dbe_srk_t* srk,
        dbe_bkey_t* k);

SS_INLINE void dbe_srk_setkeypos(
        dbe_srk_t* srk,
        dbe_keypos_t keypos);

SS_INLINE dbe_keypos_t dbe_srk_getkeypos(
        dbe_srk_t* srk);

SS_INLINE void dbe_srk_expand(
        dbe_srk_t* srk,
        dbe_bkey_t* ck);

SS_INLINE dbe_bkey_t* dbe_srk_getbkey(
        dbe_srk_t* srk);

SS_INLINE void dbe_srk_settrxid(
        dbe_srk_t* srk,
        dbe_trxid_t trxid);

SS_INLINE dbe_trxid_t dbe_srk_gettrxid(
        dbe_srk_t* srk);

SS_INLINE dbe_trxid_t dbe_srk_getkeytrxid(
        dbe_srk_t* srk);

SS_INLINE vtpl_t* dbe_srk_getvtpl(
        dbe_srk_t* srk);

vtpl_vamap_t* dbe_srk_getvamap(
        dbe_srk_t* srk);

SS_INLINE bool dbe_srk_isblob(
        dbe_srk_t* srk);

SS_INLINE bool dbe_srk_isdeletemark(
        dbe_srk_t* srk);

#if defined(DBE6SRK_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_srk_setbkey
 *
 *
 *
 * Parameters :
 *
 *	srk -
 *
 *
 *	k -
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
SS_INLINE void dbe_srk_setbkey(dbe_srk_t* srk, dbe_bkey_t* k)
{
        CHK_SRK(srk);

        ss_aassert(!srk->srk_active);
        ss_autotest_or_debug(srk->srk_active = TRUE);
        ss_autotest_or_debug(dbe_dynbkey_setbkey(&srk->srk_tmpk, k);)
        
        dbe_bkey_copy(srk->srk_dk, k);
        srk->srk_vamap_filled = FALSE;
        srk->srk_trxid = DBE_TRXID_NULL;

        ss_autotest_or_debug(
            if (dbe_bkey_compare(srk->srk_tmpk, srk->srk_dk) != 0) {
                char debugstring[80];
                SsSprintf(
                    debugstring,
                    "/LEV:4/FIL:dbe6srk/LOG/THR/LIM:100000000/NOD/FLU/TID:%u",
                    SsThrGetid());
                SsDbgSet(debugstring);
                dbe_bkey_dprint(1, srk->srk_tmpk);
                dbe_bkey_dprint(1, srk->srk_dk);
                ss_error;
            }
        );
        ss_autotest_or_debug(srk->srk_active = FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_srk_getbkey
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE dbe_bkey_t* dbe_srk_getbkey(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(srk->srk_dk);
}

/*##**********************************************************************\
 *
 *		dbe_srk_expand
 *
 *
 *
 * Parameters :
 *
 *	srk -
 *
 *
 *	ck -
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
SS_INLINE void dbe_srk_expand(dbe_srk_t* srk, dbe_bkey_t* ck)
{
        dbe_bkey_t* prevkey;

        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);
        ss_aassert(!srk->srk_active);
        ss_autotest_or_debug(srk->srk_active = TRUE);
        SS_PUSHNAME("dbe_srk_expand");
        ss_dprintf_4(("dbe_srk_expand\n"));

        ss_autotest_or_debug(dbe_dynbkey_expand(&srk->srk_tmpk, srk->srk_tmpk, ck);)

        prevkey = srk->srk_dk;
        srk->srk_dk = srk->srk_prevdk;
        srk->srk_prevdk = prevkey;

        dbe_bkey_expand(srk->srk_dk, prevkey, ck);
        srk->srk_vamap_filled = FALSE;
        srk->srk_trxid = DBE_TRXID_NULL;

        ss_aassert(dbe_bkey_compare(srk->srk_tmpk, srk->srk_dk) == 0);
        ss_autotest_or_debug(srk->srk_active = FALSE);
        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *		dbe_srk_setkeypos
 *
 *
 *
 * Parameters :
 *
 *	srk -
 *
 *
 *	keypos -
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
SS_INLINE void dbe_srk_setkeypos(dbe_srk_t* srk, dbe_keypos_t keypos)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        srk->srk_keypos = keypos;
}

/*##**********************************************************************\
 *
 *		dbe_srk_getkeypos
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE dbe_keypos_t dbe_srk_getkeypos(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(srk->srk_keypos);
}

/*##**********************************************************************\
 *
 *		dbe_srk_settrxid
 *
 *
 *
 * Parameters :
 *
 *	srk -
 *
 *
 *	trxid -
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
SS_INLINE void dbe_srk_settrxid(
        dbe_srk_t* srk,
        dbe_trxid_t trxid)
{
        CHK_SRK(srk);

        srk->srk_trxid = trxid;
}

/*##**********************************************************************\
 *
 *		dbe_srk_gettrxid
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE dbe_trxid_t dbe_srk_gettrxid(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(srk->srk_trxid);
}

/*##**********************************************************************\
 *
 *		dbe_srk_getkeytrxid
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE dbe_trxid_t dbe_srk_getkeytrxid(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        if (dbe_bkey_istrxid(srk->srk_dk)) {
            return(dbe_bkey_gettrxid(srk->srk_dk));
        } else {
            return(DBE_TRXID_NULL);
        }
}

/*##**********************************************************************\
 *
 *		dbe_srk_getvtpl
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE vtpl_t* dbe_srk_getvtpl(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(dbe_bkey_getvtpl(srk->srk_dk));
}

/*##**********************************************************************\
 *
 *		dbe_srk_isblob
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE bool dbe_srk_isblob(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(dbe_bkey_isblob(srk->srk_dk));
}

/*##**********************************************************************\
 *
 *		dbe_srk_isdeletemark
 *
 *
 *
 * Parameters :
 *
 *	srk -
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
SS_INLINE bool dbe_srk_isdeletemark(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        return(dbe_bkey_isdeletemark(srk->srk_dk));
}

#endif /* defined(DBE6SRK_C) || defined(SS_USE_INLINE) */

#endif /* DBE6SRK_H */
