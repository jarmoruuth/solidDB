/*************************************************************************\
**  source       * dbe6srk.c
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

#define DBE6SRK_C

#include <ssenv.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <uti0vtpl.h>

#include "dbe0type.h"
#include "dbe6bkey.h"
#include "dbe6srk.h"

/*##**********************************************************************\
 *
 *		dbe_srk_initbuf
 *
 *
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_srk_t* dbe_srk_initbuf(
        dbe_srk_t* srk,
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* bkeyinfo)
{
        ss_dprintf_1(("dbe_srk_initbufs\n"));

        ss_debug(srk->srk_chk = DBE_CHK_SRK;)
        srk->srk_dk = dbe_bkey_init_ex(cd, bkeyinfo);
        srk->srk_prevdk = dbe_bkey_init_ex(cd, bkeyinfo);
        srk->srk_vamap = NULL;
        srk->srk_vamap_filled = FALSE;
        srk->srk_keypos = DBE_KEYPOS_FIRST;
        srk->srk_trxid = DBE_TRXID_NULL;
        ss_autotest_or_debug(srk->srk_tmpk = NULL;)
        ss_autotest_or_debug(srk->srk_active = FALSE;)
        ss_aassert(rs_sysi_testthrid(cd));

        return(srk);
}

/*##**********************************************************************\
 *
 *		dbe_srk_donebuf
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
void dbe_srk_donebuf(
        dbe_srk_t* srk,
        rs_sysi_t* cd)
{
        CHK_SRK(srk);

        ss_aassert(rs_sysi_testthrid(cd));
        ss_aassert(!srk->srk_active);
        ss_autotest_or_debug(srk->srk_active = TRUE);
        ss_autotest_or_debug(dbe_dynbkey_free(&srk->srk_tmpk);)

        dbe_bkey_done_ex(cd, srk->srk_dk);
        dbe_bkey_done_ex(cd, srk->srk_prevdk);
        if (srk->srk_vamap != NULL) {
            vtpl_vamap_done(srk->srk_vamap);
        }
        ss_autotest_or_debug(srk->srk_active = FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_srk_copy
 *
 *
 *
 * Parameters :
 *
 *	target_srk -
 *
 *
 *	source_srk -
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
void dbe_srk_copy(dbe_srk_t* target_srk, dbe_srk_t* source_srk)
{
        CHK_SRK(target_srk);
        CHK_SRK(source_srk);
        ss_dassert(source_srk->srk_dk != NULL);
        ss_aassert(!target_srk->srk_active);
        ss_aassert(!source_srk->srk_active);
        ss_autotest_or_debug(target_srk->srk_active = TRUE);
        ss_autotest_or_debug(source_srk->srk_active = TRUE);
        ss_aassert(source_srk->srk_tmpk != NULL);

        ss_autotest_or_debug(dbe_dynbkey_setbkey(&target_srk->srk_tmpk, source_srk->srk_tmpk);)
        dbe_bkey_copy(target_srk->srk_dk, source_srk->srk_dk);
        if (target_srk->srk_vamap != NULL) {
            vtpl_vamap_done(target_srk->srk_vamap);
            target_srk->srk_vamap = NULL;
            target_srk->srk_vamap_filled = FALSE;
        }
        target_srk->srk_keypos = source_srk->srk_keypos;
        target_srk->srk_trxid = source_srk->srk_trxid;

        ss_aassert(dbe_bkey_compare(target_srk->srk_dk, target_srk->srk_tmpk) == 0);
        ss_autotest_or_debug(target_srk->srk_active = FALSE);
        ss_autotest_or_debug(source_srk->srk_active = FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_srk_getvamap
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
vtpl_vamap_t* dbe_srk_getvamap(dbe_srk_t* srk)
{
        CHK_SRK(srk);
        ss_dassert(srk->srk_dk != NULL);

        if (!srk->srk_vamap_filled) {
            vtpl_t* vtpl;

            vtpl = dbe_bkey_getvtpl(srk->srk_dk);

            if (srk->srk_vamap == NULL) {
                srk->srk_vamap = vtpl_vamap_alloc(vtpl_vacount(vtpl) + 1);
            }
            srk->srk_vamap = vtpl_vamap_refill(srk->srk_vamap, vtpl);
            if (dbe_bkey_isblob(srk->srk_dk)) {
                /* Remove blob info va (last va) from the vamap.
                 */
                vtpl_vamap_removelast(srk->srk_vamap);
            }
            srk->srk_vamap_filled = TRUE;
        }

        ss_dassert(srk->srk_vamap != NULL);

        return(srk->srk_vamap);
}

