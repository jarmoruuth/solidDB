/*************************************************************************\
**  source       * dbe6bkrs.h
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


#ifndef DBE6BKRS_H
#define DBE6BKRS_H

#include "dbe6bkey.h"

/* Different search states. Each state is stored as a separate bit,
   so several states can be active at the same time. */
#define KRS_INIT        1
#define KRS_BEGIN       2
#define KRS_ISNEXT      4
#define KRS_ISPREV      8
#define KRS_END         16
#define KRS_RESET       32

typedef enum {
        BKRS_CONT,
        BKRS_STOP
} dbe_bkrs_rc_t;

/* Range search structure.
*/
struct dbe_bkrs_st {
        int           krs_state;  /* search state */
        dbe_dynbkey_t krs_kb;     /* begin key of range */
        bool          krs_dynkb;  /* is krs_kb allocated dynamically */
        dbe_dynbkey_t krs_ke;     /* end key of range */
        bool          krs_dynke;  /* is krs_ke allocated dynamically */
        dbe_bkey_t*   krs_kcb;    /* begin key of current step. this is key
                                     value for the next node after current
                                     node. */
        dbe_bkey_t*   krs_kp;     /* begin key of previous step, if
                                     state & KRS_ISPREV. this is actually key
                                     value for current node and we need to
                                     go to a node before this key value */
        dbe_bkey_t*   krs_kn;     /* begin key of next step, if
                                     state & KRS_ISNEXT */

        bool           krs_ke_open;  /* Open range end. */
        rs_sysi_t*     krs_cd;
        dbe_bkeyinfo_t* krs_ki;
        ss_debug(void* krs_btree;)   /* DEBUG: pointer to btree. */
};

dbe_bkrs_t* dbe_bkrs_init(
        rs_sysi_t* cd,
        dbe_bkeyinfo_t* ki,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        bool        ke_open);

void dbe_bkrs_done(
        dbe_bkrs_t* krs);

dbe_bkrs_t* dbe_bkrs_copy(
        dbe_bkrs_t* krs);

void dbe_bkrs_reset(
        dbe_bkrs_t* krs,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        bool        ke_open);

void dbe_bkrs_setresetkey(
        dbe_bkrs_t* krs,
        dbe_bkey_t* nk);

void dbe_bkrs_fixprevstep(
        dbe_bkrs_t* krs);

SS_INLINE bool dbe_bkrs_startnextstep(
        dbe_bkrs_t* krs);

/* internal */
bool dbe_bkrs_startnextstep_copy(
        dbe_bkrs_t* krs);

bool dbe_bkrs_startprevstep(
        dbe_bkrs_t* krs);

void dbe_bkrs_setnextstepbegin(
        dbe_bkrs_t* krs,
        dbe_bkey_t* nk);

void dbe_bkrs_setnextstepbegin_fk(
        dbe_bkrs_t* krs,
        dbe_bkey_t* fk);

SS_INLINE void dbe_bkrs_setprevstepbegin(
        dbe_bkrs_t* krs,
        dbe_bkey_t* fk);

/* internal */
void dbe_bkrs_setprevstepbegin_copy(
        dbe_bkrs_t* krs,
        dbe_bkey_t* fk);

SS_INLINE void dbe_bkrs_undosearch(
        dbe_bkrs_t* krs);

SS_INLINE bool dbe_bkrs_isbegin(
        dbe_bkrs_t* krs);

SS_INLINE void dbe_bkrs_clearbegin(
        dbe_bkrs_t* krs);

SS_INLINE dbe_bkrs_rc_t dbe_bkrs_checkrangebegin(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k);

SS_INLINE dbe_bkrs_rc_t dbe_bkrs_checkrangeend(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k);

/* internal */
dbe_bkrs_rc_t dbe_bkrs_checkrangeend_compare(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k);

SS_INLINE dbe_bkey_t* dbe_bkrs_getbeginkey(
        dbe_bkrs_t* krs);

SS_INLINE dbe_bkey_t* dbe_bkrs_getendkey(
        dbe_bkrs_t* krs);

#if defined(DBE6BKRS_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		dbe_bkrs_checkrangebegin
 * 
 * Checks if a key value is inside the range begin.
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
SS_INLINE dbe_bkrs_rc_t dbe_bkrs_checkrangebegin(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k)
{
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(!(krs->krs_state & KRS_BEGIN));

        if ((krs->krs_state & KRS_ISPREV) ||
            dbe_bkey_compare(k, krs->krs_kb) >= 0) {

            /* Key is inside the search range. */
            return(BKRS_CONT);

        } else {

            /* End of search. */
            return(BKRS_STOP);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_isbegin
 * 
 * Checks if the search is in the begin state.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		key range search
 *
 * Return value : 
 *
 *      TRUE, search is in the begin state
 *      FALSE, search is not in the begin state
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE bool dbe_bkrs_isbegin(dbe_bkrs_t* krs)
{
        ss_dassert(!(krs->krs_state & KRS_INIT));

        if (krs->krs_state & KRS_BEGIN) {
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_clearbegin
 * 
 * Clears the begin state.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void dbe_bkrs_clearbegin(dbe_bkrs_t* krs)
{
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(!(krs->krs_state & KRS_END));

        krs->krs_state &= ~KRS_BEGIN;

        if (krs->krs_state & KRS_RESET) {
            krs->krs_state &= ~KRS_RESET;
            dbe_bkrs_fixprevstep(krs);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_getbeginkey
 * 
 * Returns pointer to the begin key of the current search step.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		key range search
 *
 * Return value - ref : 
 *
 *      pointer to the begin key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE dbe_bkey_t* dbe_bkrs_getbeginkey(dbe_bkrs_t* krs)
{
        ss_dassert(krs->krs_kcb != NULL);
        return(krs->krs_kcb);
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_getendkey
 * 
 * Returns pointer to the end key of the search.
 * 
 * Parameters : 
 * 
 *	krs - in, use
 *		key range search
 *
 * Return value - ref : 
 *
 *      pointer to the end key
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE dbe_bkey_t* dbe_bkrs_getendkey(dbe_bkrs_t* krs)
{
        ss_dassert(!(krs->krs_state & KRS_INIT));

        if (krs->krs_state & KRS_ISNEXT) {
            ss_dassert(krs->krs_kn != NULL);
            return(krs->krs_kn);
        } else {
            ss_dassert(krs->krs_ke != NULL);
            return(krs->krs_ke);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_undosearch
 * 
 * Removes possible next step begin key information.
 * 
 * Parameters : 
 * 
 *	krs - in out, use
 *		key range search
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
SS_INLINE void dbe_bkrs_undosearch(dbe_bkrs_t* krs)
{
        krs->krs_state &= ~KRS_ISNEXT;
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_startnextstep
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
SS_INLINE bool dbe_bkrs_startnextstep(dbe_bkrs_t* krs)
{
        ss_dprintf_1(("dbe_bkrs_startnextstep\n"));
        if (krs->krs_state & KRS_INIT) {
            krs->krs_state &= ~KRS_INIT;
            krs->krs_state |= KRS_BEGIN;
            return(TRUE);
        }
        if (krs->krs_state & KRS_ISNEXT) {
            return(dbe_bkrs_startnextstep_copy(krs));
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_checkrangeend
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
SS_INLINE dbe_bkrs_rc_t dbe_bkrs_checkrangeend(
        dbe_bkrs_t* krs,
        dbe_bkey_t* k)
{
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(!(krs->krs_state & KRS_BEGIN));

        if (krs->krs_state & KRS_ISNEXT) {
            ss_dprintf_4(("dbe_bkrs_checkrangeend:KRS_ISNEXT, return BKRS_CONT\n"));

            /* Key is inside the search range. */
            return(BKRS_CONT);

        } else {

            return(dbe_bkrs_checkrangeend_compare(krs, k));
        }
}

/*##**********************************************************************\
 * 
 *		dbe_bkrs_setprevstepbegin
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
SS_INLINE void dbe_bkrs_setprevstepbegin(dbe_bkrs_t* krs, dbe_bkey_t* fk)
{
        int cmp;

        ss_dprintf_1(("dbe_bkrs_setprevstepbegin\n"));
        ss_dassert(!(krs->krs_state & KRS_INIT));
        ss_dassert(dbe_bkey_checkheader(fk));

        DBE_BKEY_COMPARE(krs->krs_kb, fk, cmp);
        if (cmp < 0) {
            dbe_bkrs_setprevstepbegin_copy(krs, fk);
        } else {
            krs->krs_state &= ~KRS_ISPREV;
        }
}

#endif /* defined(DBE6BKRS_C) || defined(SS_USE_INLINE) */

#endif /* DBE6BKRS_H */
