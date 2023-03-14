/*************************************************************************\
**  source       * dbe5isea.h
**  directory    * dbe
**  description  * Index search routines.
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


#ifndef DBE5ISEA_H
#define DBE5ISEA_H

#include <ssc.h>

#include <uti0vtpl.h>

#include <su0list.h>
#include <su0bflag.h>

#include "dbe9type.h"
#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe5inde.h"
#include "dbe0type.h"
#include "dbe0erro.h"

SS_INLINE dbe_indsea_t* dbe_indsea_init(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_lock_mode_t lockmode,
        char* caller);

dbe_indsea_t* dbe_indsea_init_ex(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_lock_mode_t lockmode,
        bool pessimistic,
        SsSemT* dataseasem,
        char* caller);

void dbe_indsea_done(
        dbe_indsea_t* indsea);

void dbe_indsea_reset(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist);

dbe_ret_t dbe_indsea_reset_fetch(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk);

void dbe_indsea_reset_ex(
        dbe_indsea_t* indsea,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        bool valuereset);

void dbe_indsea_gotoend(
        dbe_indsea_t* indsea);

dbe_ret_t dbe_indsea_setposition(
        dbe_indsea_t* indsea,
        vtpl_t* vtpl);

void dbe_indsea_setvalidate(
        dbe_indsea_t* indsea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld);

void dbe_indsea_setversionedpessimistic(
        dbe_indsea_t* indsea);

long dbe_indsea_getseaid(
        dbe_indsea_t* indsea);

dbe_ret_t dbe_indsea_next(
        dbe_indsea_t* indsea,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk);

dbe_ret_t dbe_indsea_prev(
        dbe_indsea_t* indsea,
        dbe_trxid_t stmttrxid,
        dbe_srk_t** p_srk);

bool dbe_indsea_getlastkey(
        dbe_indsea_t* indsea,
        dynvtpl_t* p_lastkey,
        dbe_trxid_t* p_lasttrxid);

bool dbe_indsea_ischanged(
        dbe_indsea_t* indsea);

void dbe_indsea_setmergestart(
        dbe_indsea_t* indsea,
        bool mergeactive);

void dbe_indsea_setmergestop(
        dbe_indsea_t* indsea,
        bool mergeactive);

void dbe_indsea_setretry(
        dbe_indsea_t* indsea,
        bool retryp);

bool dbe_indsea_setidle(
        dbe_indsea_t* indsea);

void dbe_indsea_setended(
        dbe_indsea_t* indsea);

void dbe_indsea_setlongseqsea(
        dbe_indsea_t* indsea);

void dbe_indsea_clearlongseqsea(
        dbe_indsea_t* indsea);

void dbe_indsea_setdatasea(
        dbe_indsea_t* indsea);

void dbe_indsea_setmaxpoolblocks(
        dbe_indsea_t* indsea,
        ulong maxpoolblocks);

#ifdef SS_QUICKSEARCH
void* dbe_indsea_getquicksearch(
        dbe_indsea_t* indsea,
        bool longsearch);
#endif /* SS_QUICKSEARCH */

bool dbe_indsea_print(
        dbe_indsea_t* indsea);

void dbe_indsea_printinfoheader(
        void* fp);

void dbe_indsea_printinfo(
        void* fp,
        dbe_indsea_t* indsea);


#if defined(DBE5ISEA_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_indsea_init
 *
 * Initializes index tree search.
 *
 * Parameters :
 *
 *	indsea - use
 *
 *
 *	cd - in, hold
 *		client data
 *
 *	index - in, hold
 *		index system
 *
 *	tc - in, hold
 *		time range constraints
 *
 *      sr - in, use
 *		search range
 *
 *      conslist - in, hold
 *		search constraints, NULL if none
 *
 * Return value - give :
 *
 *      pointer to the index search
 *
 * Comments  :
 *
 * Globals used :
 */
SS_INLINE dbe_indsea_t* dbe_indsea_init(
        void* cd,
        dbe_index_t* index,
        rs_key_t* key,
        dbe_btrsea_timecons_t* tc,
        dbe_searchrange_t* sr,
        su_list_t* conslist,
        dbe_lock_mode_t lockmode,
        char* caller)
{
        return(dbe_indsea_init_ex(
                    cd,
                    index,
                    key,
                    tc,
                    sr,
                    conslist,
                    lockmode,
                    FALSE,
                    NULL,
                    caller));
}

#endif /* defined(DBE5ISEA_C) || defined(SS_USE_INLINE) */

#endif /* DBE5ISEA_H */
