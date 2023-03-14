/*************************************************************************\
**  source       * dbe6bsea.h
**  directory    * dbe
**  description  * B+-tree search.
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


#ifndef DBE6BSEA_H
#define DBE6BSEA_H

#include <ssc.h>

#include <su0list.h>

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe6bkey.h"
#include "dbe6bkrs.h"
#include "dbe6srk.h"
#include "dbe6bnod.h"

#define CHK_BTRSEA(bs) ss_dassert(SS_CHKPTR(bs) && (bs)->bs_chk == DBE_CHK_BTRSEA)

typedef struct {
        dbe_bkey_t*     kc_beginkey;
        dbe_bkey_t*     kc_endkey;
        su_list_t*      kc_conslist;
        rs_sysi_t*      kc_cd;
        rs_key_t*       kc_key;
} dbe_btrsea_keycons_t;

typedef struct {
        dbe_trxnum_t    tc_mintrxnum;
        dbe_trxnum_t    tc_maxtrxnum;
        dbe_trxid_t     tc_usertrxid;
        dbe_trxid_t     tc_maxtrxid;
        dbe_trxbuf_t*   tc_trxbuf;
} dbe_btrsea_timecons_t;

typedef enum {
        BSEA_POS_FIRST,
        BSEA_POS_MIDDLE,
        BSEA_POS_LAST
} bsea_pos_t;

/* Structure for a range search in the index tree.
 */
struct dbe_btrsea_st {
        dbe_btree_t*            bs_b;       /* Pointer to a tree from where
                                               the search is done. */
        dbe_gobj_t*             bs_go;      /* Reference to global objects. */
        bsea_pos_t              bs_pos;     /* Current search position. */
        dbe_bkrs_t*             bs_krs;     /* Key range search structure. */
        dbe_bnode_rsea_t        bs_nrs;     /* Node range search. */
        dbe_bnode_t*            bs_n;       /* Copy of current node. */
        dbe_bnode_t*            bs_tmpn;
        bool                    bs_peeked;  /* Flag to indicate if next key is
                                               peeked. */
        dbe_srk_t*              bs_srk;     /* Search return key. */
        dbe_srk_t               bs_srk_buf; /* Buffer for bs_srk. */
        dbe_srk_t*              bs_peeksrk; /* Peeked search result. */
        dbe_srk_t               bs_peeksrk_buf; /* Buffer for bs_peeksrk. */
        dbe_ret_t               bs_peekrc;  /* Peeked return code of the search. */
        dbe_btrsea_timecons_t*  bs_tc;      /* Time constraints. */
        dbe_btrsea_keycons_t*   bs_kc;      /* Key constraints. */
        bool                    bs_mergesea;/* If TRUE, this is a merge search.
                                               During merge search aborted
                                               key values are removed and
                                               transaction numbers are
                                               patched. */
        bool                    bs_validatesea; /* If TRUE, this search is
                                                   used for transaction
                                                   validation. */
        dbe_keyvld_t            bs_keyvldtype;
        bool                    bs_earlyvld;
        bool                    bs_pessimistic;
        bool                    bs_lockingread;
        bool                    bs_unlock_tree;
        bool                    bs_timeconsacceptall;
        int                     bs_longseqsea;
        long                    bs_nkeyremoved;/* Number of key values removed
                                                  during a merge search.*/
        long                    bs_nmergeremoved;
        long                    bs_nreadleafs;
        uint                    bs_readaheadsize;
        dbe_keypos_t            bs_keypos;
        dbe_ret_t               bs_mergerc;     /* Return code used during
                                                   merge. */
        dbe_trxnum_t            bs_mergetrxnum; /* Used during merge. */
        bool                    bs_bonsaip;     /* TRUE if Bonsai-tree search. */
        dbe_info_t*             bs_info;
        dbe_info_t              bs_infobuf;
        ss_debug(bool           bs_freebnode;)
        ss_debug(int            bs_usagectr;)
        ss_debug(dbe_chk_t      bs_chk;)
};

typedef struct dbe_btrsea_st dbe_btrsea_t;

void dbe_btrsea_initbufmerge(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        dbe_info_t* info);

SS_INLINE void dbe_btrsea_initbufvalidate(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool mergesea,
        bool validatesea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld);

void dbe_btrsea_initbufvalidate_ex(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool mergesea,
        bool validatesea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld,
        bool lockingread,
        bool pessimistic);

void dbe_btrsea_donebuf(
        dbe_btrsea_t* bs);

void dbe_btrsea_reset(
        dbe_btrsea_t* bs,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool lockingread);

void dbe_btrsea_resetkeycons(
        dbe_btrsea_t* bs,
        dbe_btrsea_keycons_t* kc);

void dbe_btrsea_setnodereadonly(
        dbe_btrsea_t* bs);

bool dbe_btrsea_checktimecons(
        dbe_btrsea_t* bs,
        dbe_btrsea_timecons_t* tc,
        dbe_bkey_t* k,
        bool bonsaip,
        bool validatesea,
        dbe_trxstate_t* p_trxresult);

bool dbe_btrsea_checkkeycons(
        rs_sysi_t* cd,
        dbe_srk_t* srk,
        su_list_t* conslist);

long dbe_btrsea_getnkeyremoved(
        dbe_btrsea_t* bs);

long dbe_btrsea_getnmergeremoved(
        dbe_btrsea_t* bs);

void dbe_btrsea_setresetkey(
        dbe_btrsea_t* bs,
        dbe_bkey_t* k,
        bool lockingread);

void dbe_btrsea_freebnode(
        dbe_btrsea_t* bs);

SS_INLINE bool dbe_btrsea_ischanged(
        dbe_btrsea_t* bs);

dbe_ret_t dbe_btrsea_getnext(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk);

dbe_ret_t dbe_btrsea_getnextblock(
        dbe_btrsea_t* bs);

dbe_ret_t dbe_btrsea_getnext_quickmerge(
        dbe_btrsea_t* bs);

dbe_ret_t dbe_btrsea_peeknext(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk);

dbe_ret_t dbe_btrsea_getprev(
        dbe_btrsea_t* bs,
        dbe_srk_t** p_srk);

SS_INLINE bool dbe_btrsea_getcurrange_next(
        dbe_btrsea_t* bs,
        dbe_bkey_t** p_begin,
        dbe_bkey_t** p_end);

bool dbe_btrsea_getcurrange_prev(
        dbe_btrsea_t* bs,
        dbe_bkey_t** p_begin,
        dbe_bkey_t** p_end);

bool dbe_btrsea_isbegin(
        dbe_btrsea_t* bs);

bool dbe_btrsea_isend(
        dbe_btrsea_t* bs);

void dbe_btrsea_setlongseqsea(
        dbe_btrsea_t* bs);

void dbe_btrsea_clearlongseqsea(
        dbe_btrsea_t* bs);

SS_INLINE void dbe_btrsea_settimeconsacceptall(
        dbe_btrsea_t* bs);

void dbe_btrsea_setreadaheadsize(
        dbe_btrsea_t* bs,
        uint readaheadsize);

int dbe_btrsea_getequalrowestimate(
        rs_sysi_t* cd,
        dbe_btree_t* b,
        vtpl_t* range_begin,
        vtpl_t* range_end);

void dbe_btrsea_errorprint(
        dbe_btrsea_t* bs);

void dbe_btrsea_getnodedata(
        dbe_btrsea_t* bs,
        char** p_data,
        int* p_len);

#if defined(DBE6BSEA_C) || defined(SS_USE_INLINE)

SS_INLINE void dbe_btrsea_initbufvalidate(
        dbe_btrsea_t* bs,
        dbe_btree_t* b,
        dbe_btrsea_keycons_t* kc,
        dbe_btrsea_timecons_t* tc,
        bool mergesea,
        bool validatesea,
        dbe_keyvld_t keyvldtype,
        bool earlyvld)
{
        dbe_btrsea_initbufvalidate_ex(
                bs,
                b,
                kc,
                tc,
                mergesea,
                validatesea,
                keyvldtype,
                earlyvld,
                FALSE,
                FALSE);
}

/*##**********************************************************************\
 *
 *		dbe_btrsea_getcurrange_next
 *
 * Returns the range begin and end keys of the current search buffer
 * range.
 *
 * Parameters :
 *
 *	bs - in, use
 *		btree search
 *
 *	p_begin - out, ref
 *		if non-NULL, pointer to the local buffer of range begin key
 *		is stored into *p_begin
 *
 *	p_end - out, ref
 *		if non-NULL, pointer to the local buffer of range end key
 *		is stored into *p_end
 *
 * Return value :
 *
 *      TRUE    - not end of search
 *      FALSE   - end of search
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_btrsea_getcurrange_next(
        dbe_btrsea_t* bs,
        dbe_bkey_t** p_begin,
        dbe_bkey_t** p_end)
{
        CHK_BTRSEA(bs);
        ss_dassert(bs->bs_usagectr++ == 0);

        if (bs->bs_pos == BSEA_POS_LAST) {
            ss_debug(bs->bs_usagectr--);
            return(FALSE);
        }

        if (p_begin != NULL) {
            *p_begin = dbe_bkrs_getbeginkey(bs->bs_krs);
        }
        if (p_end != NULL) {
            *p_end = dbe_bkrs_getendkey(bs->bs_krs);
        }
        ss_debug(bs->bs_usagectr--);

        return(TRUE);
}

SS_INLINE bool dbe_btrsea_ischanged(
        dbe_btrsea_t* bs)
{
        bool changed;

        CHK_BTRSEA(bs);

        if (bs->bs_n != NULL) {
            changed = dbe_bnode_ischanged(bs->bs_n);
        } else {
            changed = TRUE;
        }
        ss_bprintf_1(("dbe_btrsea_ischanged:changed=%d\n", changed));
        return(changed);
}

SS_INLINE void dbe_btrsea_settimeconsacceptall(dbe_btrsea_t* bs)
{
        CHK_BTRSEA(bs);

        bs->bs_timeconsacceptall = TRUE;
}

#endif /* defined(DBE6BSEA_C) || defined(SS_USE_INLINE) */

#endif /* DBE6BSEA_H */
