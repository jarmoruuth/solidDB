/*************************************************************************\
**  source       * dbe4srch.h
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


#ifndef DBE4SRCH_H
#define DBE4SRCH_H

#include <ssc.h>

#include <su0list.h>
#include <su0bflag.h>

#include <rs0relh.h>
#include <rs0tval.h>
#include <rs0pla.h>
#include <rs0sysi.h>
#include <rs0vbuf.h>

#include "dbe6srk.h"
#include "dbe6bsea.h"
#include "dbe5isea.h"
#include "dbe5dsea.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0trx.h"
#include "dbe0tref.h"

#define CHK_SEARCH(s) ss_dassert(SS_CHKPTR(s) && (s)->sea_chk == DBE_CHK_SEARCH && (s)->sea_hdr.sh_chk == DBE_CHK_SEARCH_HEADER)

/* The search structure.
 */
struct dbe_search_st {
#ifdef SS_MME
        dbe_search_header_t     sea_hdr;        /* Must be the first field
                                                   in all instances of
                                                   dbe_search_t. */
#endif
        ss_debug(dbe_chk_t sea_chk;)
        su_bflag_t              sea_flags;
        dbe_user_t*             sea_user;       /* User of the search. */
        dbe_index_t*            sea_index;
        rs_sysi_t*              sea_cd;
        su_list_node_t*         sea_id;         /* Search id in sea_user. */
        rs_ttype_t*             sea_ttype;      /* Tuple type of the result
                                                   rows from the search. */
        rs_ano_t*               sea_sellist;    /* Select list, terminated by
                                                   RS_ANO_NULL. */
        rs_pla_t*               sea_plan;       /* Search plan. */
        dbe_btrsea_timecons_t   sea_tc;         /* Time range constraints. */
        su_list_t*              sea_refattrs;   /* Rules to create tuple ref. */
        bool                    sea_getdata;    /* TRUE if the data tuple must
                                                   be referenced. */
        su_list_t*              sea_data_conslist; /* Data constraint list. */
        rs_ano_t*               sea_selkeyparts;/* Key part numbers of
                                                   selected attributes from
                                                   the database key. */
        dbe_indsea_t*           sea_indsea;     /* Database index search. The
                                                   actual key values for the
                                                   search come from this
                                                   index search. */
        dbe_datasea_t*          sea_datasea;
        bool                    sea_activated;
        bool                    sea_needrestart;
        rs_reltype_t            sea_reltype;
        dbe_ret_t               sea_rc;
        bool                    sea_forwardp;
        ulong                   sea_nseqstep;   /* Number of sequential steps.
                                                   Used to detect long
                                                   sequential in which the
                                                   cache is handled in a
                                                   different way. Currently
                                                   implemented only for a
                                                   forward search. */
        ulong                   sea_nseqsteplimit; /* Value after which the
                                                      search is changed to a
                                                      long sequential search.*/
        dbe_srk_t*              sea_srk;        /* Index search return values,
                                                   contains current search
                                                   key value. */
        dbe_srk_t*              sea_datasrk;    /* Data search return value
                                                   that is used when getdata
                                                   flag is not TRUE but the
                                                   data is fetched for other
                                                   reasons. */
        dbe_tref_t*             sea_tref;       /* If non-NULL, tuple
                                                   reference of the current
                                                   tuple. */
        rs_key_t*               sea_key;
        dbe_cursor_type_t       sea_cursortype;
        rs_relh_t*              sea_relh;
        dbe_gobj_t*             sea_go;
        long                    sea_relid;

        bool                    sea_isolationchange_transparent;

#ifndef SS_NOLOCKING
        bool                    sea_uselocks;
        dbe_lock_mode_t         sea_lockmode;
        long                    sea_locktimeout;
        bool                    sea_optimistic_lock;
        bool                    sea_bouncelock;
        long                    sea_lastchangecount;
        bool                    sea_relhlockedp;
#endif /* SS_NOLOCKING */
#ifdef SS_QUICKSEARCH
        dbe_quicksearch_t       sea_quicksearch;
        va_t**                  sea_qsvatable;
        bool                    sea_qsblocksearch;
#endif /* SS_QUICKSEARCH */
        bool*                   sea_p_newplan;
        bool                    sea_isupdatable;
        bool                    sea_versionedpessimistic;
        dynvtpl_t               sea_posdvtpl;
};

/*
        Search routines.
*/

#define dbe_search_gettype(search)  (((dbe_search_t*)(search))->sea_hdr.sh_type)

dbe_search_t* dbe_search_init_disk(
        dbe_user_t*         user,
        dbe_trx_t*          trx,
        dbe_trxnum_t        maxtrxnum,
        dbe_trxid_t         usertrxid,
        rs_ttype_t*         ttype,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        dbe_cursor_type_t   cursor_type,
        bool*               p_newplan);

void dbe_search_done_disk(
        dbe_search_t* search);

void dbe_search_done(
        dbe_search_t* search);

void dbe_search_setisolation_transparent(
        dbe_search_t* search,
        bool transparent);

void dbe_search_reset_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan);

dbe_ret_t dbe_search_reset_disk_fetch(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid,
        rs_ttype_t* ttype,
        rs_ano_t* sellist,
        rs_pla_t* plan,
        rs_tval_t** p_tval);

rs_aval_t* dbe_search_getaval(
        dbe_search_t*   search,
        rs_tval_t*      tval,
        rs_atype_t*     atype,
        uint            kpno);

void dbe_search_invalidate(
        dbe_search_t* search,
        dbe_trxid_t usertrxid,
        dbe_search_invalidate_t type);

void dbe_search_restart_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid);

void dbe_search_restart(
        dbe_search_t* search,
        dbe_trx_t* trx,
        dbe_trxnum_t maxtrxnum,
        dbe_trxid_t usertrxid);

void dbe_search_abortrelid(
        dbe_search_t* search,
        ulong relid);

void dbe_search_abortkeyid(
        dbe_search_t* search,
        ulong keyid);

void dbe_search_markrowold(
        dbe_search_t* search,
        ulong relid,
        dbe_lockname_t lockname);

SS_INLINE bool dbe_search_isactive(
        dbe_search_t* search,
        bool* p_isupdatable);

dbe_ret_t dbe_search_nextorprev_disk(
        dbe_search_t* search,
        dbe_trx_t* trx,
        bool nextp,
        rs_tval_t** p_tval);

dbe_ret_t dbe_search_gotoend(
        dbe_search_t* search,
        dbe_trx_t* trx);

dbe_ret_t dbe_search_setposition(
        dbe_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t* tval);

dbe_tref_t* dbe_search_gettref(
        dbe_search_t*   search,
        rs_tval_t*      tval);

bool dbe_search_getclustvtpl(
        dbe_search_t* search,
        dbe_srk_t** p_srk);

bool dbe_search_getsearchinfo(
        dbe_search_t* sea,
        rs_pla_t** p_plan,
        dynvtpl_t* p_lastkey,
        dbe_trxid_t* p_lasttrxid);

void dbe_search_setoptinfo(
        dbe_search_t* sea,
        ulong tuplecount);

void dbe_search_setmaxpoolblocks(
        dbe_search_t* sea,
        ulong maxpoolblocks);

#ifdef SS_QUICKSEARCH
void* dbe_search_getquicksearch(
        dbe_search_t* sea,
        bool longsearch);
#endif /* SS_QUICKSEARCH */

void dbe_search_newplan(
        dbe_search_t*   search,
        ulong           relid);

void dbe_search_printinfoheader(
        void* fp);

void dbe_search_printinfo(
        void* fp,
        dbe_search_t* sea);

#if defined(DBE4SRCH_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_search_isactive
 *
 * Checks if the search is active, i.e. it has a current tuple.
 *
 * Parameters :
 *
 *	search - in, take
 *		Search object.
 *
 * Return value :
 *
 *      TRUE    - search is active
 *      FALSE   - search is inactive
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_search_isactive(
        dbe_search_t* search,
        bool* p_isupdatable)
{
        if (dbe_search_gettype(search) == DBE_SEARCH_MME) {
            *p_isupdatable = TRUE;
            return(dbe_mme_search_isactive((dbe_mme_search_t*)search));
        } else {
            CHK_SEARCH(search);

            *p_isupdatable = search->sea_isupdatable;
            return(search->sea_activated);
        }
}

#endif /* defined(DBE4SRCH_C) || defined(SS_USE_INLINE) */

#endif /* DBE4SRCH_H */
