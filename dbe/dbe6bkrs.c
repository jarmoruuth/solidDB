/*************************************************************************\
**  source       * dbe6bkrs.c
**  directory    * dbe
**  description  * B+-tree range search info.
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

Key range search object. Used during range search from the index tree.
This module contains methods to keep track of the next index leaf that
must be accessed when the currect leaf has ended. Also the range checks
are done to see when the key values in the index belong to the search
range. A user given search function is called for those key values that
are in the given range.

Limitations:
-----------


Error handling:
--------------

Error conditions are handled using asserts.


Objects used:
------------

key value system    dbe6bkey

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE6BKRS_C

#include <ssstdio.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>

#include "dbe6bkey.h"
#include "dbe6bkrs.h"

/*##**********************************************************************\
 * 
 *		dbe_bkrs_init
 * 
 * Initializes a range search.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		Search object.
 *		
 *	kb - in, hold
 *		range begin key (lower limit), included in the range
 *
 *	ke - in, hold
 *		range end key (upper limit), included in the range
 *
 * Return value - give : 
 *
 *      pointer to a key range search structure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_bkrs_t* dbe_bkrs_init(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        bool        ke_open)
{
        dbe_bkrs_t* krs;
        dbe_bkey_t* tmpk;

        ss_dprintf_1(("dbe_bkrs_init\n"));
        ss_dassert(ki != NULL);

        krs = SsMemAlloc(sizeof(dbe_bkrs_t));

        if (kb != NULL) {
            ss_dassert(dbe_bkey_checkheader(kb));
            krs->krs_kb = kb;
            krs->krs_dynkb = FALSE;
        } else {
            krs->krs_kb = NULL;
            tmpk = dbe_bkey_init(ki);
            dbe_bkey_setsearchminvtpl(tmpk);
            dbe_dynbkey_setbkey(&krs->krs_kb, tmpk);
            dbe_bkey_done(tmpk);
            krs->krs_dynkb = TRUE;
        }
        if (ke != NULL) {
            ss_dassert(dbe_bkey_checkheader(ke));
            krs->krs_ke = ke;
            krs->krs_dynke = FALSE;
        } else {
            /* WARNING! This code expects that a maximum v-attribute
             * is known. The code should work if every key value
             * is always preceded with a key id.
             */
            krs->krs_ke = NULL;
            tmpk = dbe_bkey_init(ki);
            dbe_bkey_setsearchmaxvtpl(tmpk);
            dbe_dynbkey_setbkey(&krs->krs_ke, tmpk);
            dbe_bkey_done(tmpk);
            krs->krs_dynke = TRUE;
            ke_open = FALSE;
        }
        
        ss_dprintf_2(("dbe_bkrs:kb\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kb));
        ss_dprintf_2(("dbe_bkrs:ke\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_ke));
        
        krs->krs_kcb = krs->krs_kb;

        krs->krs_kn = NULL;
        krs->krs_kp = NULL;
        krs->krs_state = KRS_INIT;
        krs->krs_ke_open = ke_open;
        krs->krs_cd = cd;
        krs->krs_ki = ki;

        return(krs);
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_done
 * 
 * Releases resources allocated to a key range search.
 * 
 * Parameters : 
 * 
 *	krs - in, take
 *		key range search
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_bkrs_done(dbe_bkrs_t* krs)
{
        ss_dprintf_1(("dbe_bkrs_done\n"));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_kcb != krs->krs_kb && krs->krs_kcb != NULL) {
            ss_dassert(dbe_bkey_checkheader(krs->krs_kcb));
            dbe_bkey_done_ex(krs->krs_cd, krs->krs_kcb);
        }
        if (krs->krs_kp != NULL) {
            dbe_bkey_done_ex(krs->krs_cd, krs->krs_kp);
        }
        if (krs->krs_kn != NULL) {
            dbe_bkey_done_ex(krs->krs_cd, krs->krs_kn);
        }
        if (krs->krs_dynkb) {
            dbe_dynbkey_free(&krs->krs_kb);
        }
        if (krs->krs_dynke) {
            dbe_dynbkey_free(&krs->krs_ke);
        }
        SsMemFree(krs);
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_copy
 * 
 * Makes a copy krs object.
 * 
 * Parameters : 
 * 
 *	krs - 
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
dbe_bkrs_t* dbe_bkrs_copy(
        dbe_bkrs_t* krs)
{
        dbe_bkrs_t* newkrs;

        ss_dprintf_1(("dbe_bkrs_done\n"));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kcb));

        newkrs = SsMemCalloc(sizeof(dbe_bkrs_t), 1);

        newkrs->krs_dynkb = TRUE;
        ss_dassert(krs->krs_kb != NULL);
        dbe_dynbkey_setbkey(&newkrs->krs_kb, krs->krs_kb);
        newkrs->krs_dynke = TRUE;
        ss_dassert(krs->krs_ke != NULL);
        dbe_dynbkey_setbkey(&newkrs->krs_ke, krs->krs_ke);
        
        if (krs->krs_kcb != NULL) {
            newkrs->krs_kcb = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            dbe_bkey_copy(newkrs->krs_kcb, krs->krs_kcb);
        }
        if (krs->krs_kp != NULL) {
            newkrs->krs_kp = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            dbe_bkey_copy(newkrs->krs_kp, krs->krs_kp);
        }
        if (krs->krs_kn != NULL) {
            newkrs->krs_kn = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            dbe_bkey_copy(newkrs->krs_kn, krs->krs_kn);
        }

        newkrs->krs_state = krs->krs_state;
        newkrs->krs_ke_open = krs->krs_ke_open;
        newkrs->krs_cd = krs->krs_cd;
        newkrs->krs_ki = krs->krs_ki;

        ss_debug(newkrs->krs_btree = krs->krs_btree;)

        return(newkrs);
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_reset
 * 
 * Resets key range search. After this call the bkrs is in the same state
 * as after init.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		Search object.
 *		
 *	ki - in
 *		Key info.
 *		
 *	kb - in, hold
 *		range begin key (lower limit), included in the range
 *		
 *	ke - in, hold
 *		range end key (upper limit), included in the range
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_bkrs_reset(
        dbe_bkrs_t* krs,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        bool        ke_open)
{
        dbe_bkey_t* tmpk;

        ss_dprintf_1(("dbe_bkrs_reset\n"));
        ss_dassert(krs->krs_dynkb ? dbe_bkey_checkheader(krs->krs_kb) : TRUE);
        ss_dassert(krs->krs_dynke ? dbe_bkey_checkheader(krs->krs_ke) : TRUE);

        if (krs->krs_kcb != krs->krs_kb && krs->krs_kcb != NULL) {
            dbe_bkey_done_ex(krs->krs_cd, krs->krs_kcb);
            krs->krs_kcb = NULL;
        }

        if (kb != NULL) {
            ss_dassert(dbe_bkey_checkheader(kb));
            krs->krs_kb = kb;
            krs->krs_dynkb = FALSE;
        } else {
            tmpk = dbe_bkey_init(krs->krs_ki);
            dbe_bkey_setsearchminvtpl(tmpk);
            dbe_dynbkey_setbkey(&krs->krs_kb, tmpk);
            dbe_bkey_done(tmpk);
            krs->krs_dynkb = TRUE;
        }
        if (ke != NULL) {
            ss_dassert(dbe_bkey_checkheader(ke));
            krs->krs_ke = ke;
            krs->krs_dynke = FALSE;
        } else {
            /* WARNING! This code expects that a maximum v-attribute
             * is known. The code should work if every key value
             * is always preceded with a key id.
             */
            tmpk = dbe_bkey_init(krs->krs_ki);
            dbe_bkey_setsearchmaxvtpl(tmpk);
            dbe_dynbkey_setbkey(&krs->krs_ke, tmpk);
            dbe_bkey_done(tmpk);
            krs->krs_dynke = TRUE;
            ke_open = FALSE;
        }
        
        ss_dprintf_2(("dbe_bkrs:kb\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kb));
        ss_dprintf_2(("dbe_bkrs:ke\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_ke));
        
        krs->krs_kcb = krs->krs_kb;

        krs->krs_state = KRS_INIT;
        krs->krs_ke_open = ke_open;
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_setresetkey
 * 
 * Resets the search for a new start at key nk.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 *	nk - in, use
 *		next step begin key
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_bkrs_setresetkey(dbe_bkrs_t* krs, dbe_bkey_t* nk)
{
        ss_dprintf_1(("dbe_bkrs_setresetkey\n"));
        ss_dassert(dbe_bkey_checkheader(nk));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_kcb == krs->krs_kb) {
            krs->krs_kcb = NULL;
        }
        if (krs->krs_kcb == NULL) {
            krs->krs_kcb = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
        }
        dbe_bkey_copy(krs->krs_kcb, nk);
        krs->krs_state = KRS_INIT|KRS_RESET;
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_fixprevstep
 * 
 * 
 * 
 * Parameters : 
 * 
 *	krs - 
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
void dbe_bkrs_fixprevstep(dbe_bkrs_t* krs)
{
        ss_dprintf_1(("dbe_bkrs_fixprevstep\n"));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_state & KRS_ISPREV) {
            if (krs->krs_kcb == krs->krs_kb) {
                krs->krs_kcb = NULL;
            }
            if (krs->krs_kcb == NULL) {
                krs->krs_kcb = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            }
            dbe_bkey_copy(krs->krs_kcb, krs->krs_kp);
        } else {
            if (krs->krs_kcb != krs->krs_kb && krs->krs_kcb != NULL) {
                dbe_bkey_done_ex(krs->krs_cd, krs->krs_kcb);
                krs->krs_kcb = NULL;
            }
            krs->krs_kcb = krs->krs_kb;
        }
        ss_dprintf_2(("dbe_bkrs_fixprevstep:krs->krs_kcb\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kcb));
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_startnextstep_copy
 * 
 * Starts a new range search step.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 * Return value : 
 *
 *      TRUE, next step started
 *      FALSE, at the end of search
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_bkrs_startnextstep_copy(dbe_bkrs_t* krs)
{
        ss_dprintf_1(("dbe_bkrs_startnextstep_copy\n"));
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(krs->krs_state & KRS_ISNEXT);
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_kcb == krs->krs_kb) {
            krs->krs_kcb = NULL;
        }
        if (krs->krs_kcb == NULL) {
            krs->krs_kcb = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
        }
        dbe_bkey_copy(krs->krs_kcb, krs->krs_kn);
        ss_dassert(dbe_bkey_checkheader(krs->krs_kcb));
        krs->krs_state &= ~(KRS_ISNEXT|KRS_ISPREV);
#ifdef DBE_NEXTNODEBUG
        krs->krs_state |= KRS_BEGIN;
#endif
        ss_dprintf_2(("dbe_bkrs_startnextstep_copy:krs->krs_kcb\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kcb));
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_startprevstep
 * 
 * Starts a new range search step.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 * Return value : 
 *
 *      TRUE, previous step started
 *      FALSE, at the end of search
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_bkrs_startprevstep(dbe_bkrs_t* krs)
{
        ss_dprintf_1(("dbe_bkrs_startprevstep\n"));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_state & KRS_INIT) {
            krs->krs_state &= ~KRS_INIT;
            krs->krs_state |= KRS_BEGIN;
            return(TRUE);
        }
        if (krs->krs_state & KRS_ISPREV) {
            if (krs->krs_kcb == krs->krs_kb) {
                krs->krs_kcb = NULL;
            }
            if (krs->krs_kcb == NULL) {
                krs->krs_kcb = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            }
            dbe_bkey_copy(krs->krs_kcb, krs->krs_kp);
            krs->krs_state &= ~(KRS_ISNEXT|KRS_ISPREV);
            ss_dprintf_2(("dbe_bkrs_startprevstep:krs->krs_kcb\n"));
            ss_output_2(dbe_bkey_dprint(2, krs->krs_kcb));
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_setnextstepbegin
 * 
 * Sets a candidate for a begin key of a next step. If next step begin
 * key candidate is already set, overwrites the old key.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 *	nk - in, use
 *		next step begin key candidate
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_bkrs_setnextstepbegin(dbe_bkrs_t* krs, dbe_bkey_t* nk)
{
        int cmp;

        ss_dprintf_1(("dbe_bkrs_setnextstepbegin\n"));
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(dbe_bkey_checkheader(nk));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        if (krs->krs_kn == NULL) {
            krs->krs_kn = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
        }
        dbe_bkey_expand(krs->krs_kn, krs->krs_kcb, nk);
        ss_dassert(dbe_bkey_checkheader(krs->krs_kcb));
        ss_dprintf_1(("dbe_bkrs_setnextstepbegin:krs->krs_kn\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kn));
        DBE_BKEY_COMPARE(krs->krs_kn, krs->krs_ke, cmp);
        if (cmp < 0) {
            krs->krs_state |= KRS_ISNEXT;
        } else {
            krs->krs_state &= ~KRS_ISNEXT;
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_setnextstepbegin_fk
 * 
 * Sets a candidate for a begin key of a next step using a full,
 * uncompressed key. If next step begin key candidate is already set,
 * overwrites the old key.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 *	fk - in, use
 *		next step begin key candidate as a full key
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_bkrs_setnextstepbegin_fk(dbe_bkrs_t* krs, dbe_bkey_t* fk)
{
        int cmp;

        ss_dprintf_1(("dbe_bkrs_setnextstepbegin_fk\n"));
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(dbe_bkey_checkheader(fk));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        DBE_BKEY_COMPARE(fk, krs->krs_ke, cmp);
        if (cmp < 0) {
            if (krs->krs_kn == NULL) {
                krs->krs_kn = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
            }
            dbe_bkey_copy(krs->krs_kn, fk);
            krs->krs_state |= KRS_ISNEXT;
            ss_dprintf_2(("dbe_bkrs_setnextstepbegin_fk:krs->krs_kn\n"));
            ss_output_2(dbe_bkey_dprint(2, krs->krs_kn));
        } else {
            krs->krs_state &= ~KRS_ISNEXT;
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_setprevstepbegin_copy
 * 
 * Sets the range begin check state for an index leaf.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 *	fk - in, use
 *		first key in the current leaf
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_bkrs_setprevstepbegin_copy(dbe_bkrs_t* krs, dbe_bkey_t* fk)
{
        ss_debug(int cmp;)

        ss_dprintf_1(("dbe_bkrs_setprevstepbegin_copy\n"));
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(dbe_bkey_checkheader(fk));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        ss_debug(DBE_BKEY_COMPARE(krs->krs_kb, fk, cmp));
        ss_dassert(cmp < 0);

        if (krs->krs_kp == NULL) {
            krs->krs_kp = dbe_bkey_init_ex(krs->krs_cd, krs->krs_ki);
        }
        dbe_bkey_copy(krs->krs_kp, fk);
        krs->krs_state |= KRS_ISPREV;
        ss_dprintf_2(("dbe_bkrs_setprevstepbegin_copy:krs->krs_kp\n"));
        ss_output_2(dbe_bkey_dprint(2, krs->krs_kp));
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_checkrangeend_compare
 * 
 * Checks if a key value is inside the range end.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		key range search
 *
 *	k - in, use
 *		current search key value
 *
 * Return value : 
 *
 *      State of the search.
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_bkrs_rc_t dbe_bkrs_checkrangeend_compare(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k)
{
        int cmp;
        
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(!(krs->krs_state & KRS_BEGIN));
        ss_dassert(!(krs->krs_state & KRS_ISNEXT));
        ss_dassert(dbe_bkey_checkheader(krs->krs_kb));
        ss_dassert(dbe_bkey_checkheader(krs->krs_ke));

        cmp = dbe_bkey_compare(k, krs->krs_ke);
        if ((krs->krs_ke_open && cmp < 0) 
            || (!krs->krs_ke_open && cmp <= 0)) 
        {

            ss_dprintf_4(("dbe_bkrs_checkrangeend:BKRS_CONT\n"));

            /* Key is inside the search range. */
            return(BKRS_CONT);

        } else {

            ss_dprintf_4(("dbe_bkrs_checkrangeend:BKRS_STOP\n"));
            ss_dprintf_4(("dbe_bkrs_checkrangeend:k\n"));
            ss_output_4(dbe_bkey_dprint(4, k));
            ss_dprintf_4(("dbe_bkrs_checkrangeend:krs_ke\n"));
            ss_output_4(dbe_bkey_dprint(4, krs->krs_ke));

            /* End of search. */
            return(BKRS_STOP);
        }
}


