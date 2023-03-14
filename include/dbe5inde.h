/*************************************************************************\
**  source       * dbe5inde.h
**  directory    * dbe
**  description  * Database index system functions.
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


#ifndef DBE5INDE_H
#define DBE5INDE_H

#include <ssc.h>

#include <uti0vtpl.h>

#include <su0gate.h>
#include <su0list.h>
#include <su0prof.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe7trxb.h"
#include "dbe6gobj.h"
#include "dbe6finf.h"
#include "dbe6bkey.h"
#include "dbe6btre.h"
#include "dbe6bsea.h"
#include "dbe6lmgr.h"
#ifdef SS_MME
#include "dbe4mme.h"
#endif
#include "dbe0type.h"
#include "dbe0erro.h"

#define CHK_INDEX(i) ss_dassert(SS_CHKPTR(i) && (i)->ind_chk == DBE_CHK_INDEX)

#define DBE_INDEX_NSEABUFPERSEARCH   2
#ifdef SS_SMALLSYSTEM
#define DBE_INDEX_NMERGEGATE         5
#else
#define DBE_INDEX_NMERGEGATE         100
#endif 

typedef struct dbe_index_sealnode_st dbe_index_sealnode_t;

struct dbe_index_sealnode_st {
        dbe_index_sealnode_t*   isl_next;
        dbe_index_sealnode_t*   isl_prev;
        dbe_indsea_t*           isl_indsea;
};

/* Database index system structure. The index system consist of two
   separate index trees: permanent tree and Bonsai-tree.
   Type dbe_index_t is defined in dbe0type.h.
*/

struct dbe_index_st {
        ss_debug(dbe_chk_t ind_chk;)
        dbe_btree_t*            ind_bonsaitree;  /* bonsai tree */
        dbe_btree_t*            ind_permtree;    /* permanent */
        int                     ind_mergeactive; /* flag: is merge process
                                                    active */
        ss_debug(bool           ind_fakemerge;)
        dbe_trxbuf_t*           ind_trxbuf;      /* Transaction buffer. */
        dbe_index_sealnode_t    ind_sealist;     /* List of all searches.
                                                    Contains dbe_indsea_t*'s */
        dbe_index_sealnode_t    ind_sealru;      /* LRU list of inactive searches.
                                                    Contains dbe_indsea_t*'s */
        long                    ind_nsearch;
        SsFlatMutexT            ind_listsem;     /* Sem to protect ind_sealist. */
        su_gate_t*              ind_mergegate[DBE_INDEX_NMERGEGATE];   /* Merge gate. */
        long                    ind_seaid;       /* Search id counter. */
        ulong                   ind_seqsealimit; /* Sequential search limit. */
        long                    ind_seabuflimit; /* Limit for max number of actively
                                                    used search buffer. */
        long                    ind_seabufused;  /* Number of used search search
                                                    buffers. */
        uint                    ind_readaheadsize;
        dbe_gobj_t*             ind_go;
        dbe_bkeyinfo_t*         ind_bkeyinfo;
        va_index_t              ind_bloblimit_low;
        va_index_t              ind_bloblimit_high;
#ifdef SS_BLOCKINSERT
        su_daddr_t*             ind_blockarr;
        uint                    ind_blockarrsize;
#endif /* SS_BLOCKINSERT */
};

extern bool dbe_index_test_version_on;

/* NOTE! Type dbe_index_t is defined in dbe0type.h. */

dbe_index_t* dbe_index_init(
        dbe_gobj_t* go,
        su_daddr_t bonsairoot,
        su_daddr_t permroot);

void dbe_index_done(
        dbe_index_t* index,
        su_daddr_t* p_bonsairoot,
        su_daddr_t* p_permroot);

SS_INLINE dbe_bkeyinfo_t* dbe_index_getbkeyinfo(
        dbe_index_t* index);

SS_INLINE dbe_trxbuf_t* dbe_index_gettrxbuf(
        dbe_index_t* index);

SS_INLINE bool dbe_index_ismergeactive(
        dbe_index_t* index);

#ifdef SS_DEBUG

bool dbe_index_fakemerge(
        dbe_index_t* index);

#endif /* SS_DEBUG */

SS_INLINE bool dbe_index_isearlyvld(
        dbe_index_t* index);

SS_INLINE dbe_gobj_t* dbe_index_getgobj(
        dbe_index_t* index);

void dbe_index_setmergeactive(
        dbe_index_t* index,
        bool isactive);

SS_INLINE dbe_btree_t* dbe_index_getbonsaitree(
        dbe_index_t* index);

SS_INLINE dbe_btree_t* dbe_index_getpermtree(
        dbe_index_t* index);

SS_INLINE void dbe_index_indsealist_reach(
        dbe_index_t* index);

SS_INLINE void dbe_index_indsealist_release(
        dbe_index_t* index);

void dbe_index_indsealist_mergeactiveiter_nomutex(
        dbe_index_t* index,
        bool startp,
        void (*mergeactivefun)(dbe_indsea_t* indsea, bool mergeactive));

SS_INLINE void dbe_index_mergegate_enter_shared(
        dbe_index_t* index, 
        long keyid);

SS_INLINE void dbe_index_mergegate_enter_exclusive(
        dbe_index_t* index, 
        long keyid);

SS_INLINE void dbe_index_mergegate_exit(
        dbe_index_t* index, 
        long keyid);

void dbe_index_getrootaddrs(
        dbe_index_t* index,
        su_daddr_t* p_bonsairoot,
        su_daddr_t* p_permroot);
                         
dbe_ret_t dbe_index_insert(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd);

dbe_ret_t dbe_index_delete(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        bool* p_isonlydelemark,
        rs_sysi_t* cd);

#ifdef SS_MYSQL

dbe_ret_t dbe_index_insert_ex(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd,
        su_list_t** bkeylist);

dbe_ret_t dbe_index_delete_ex(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        bool* p_isonlydelemark,
        rs_sysi_t* cd,
        su_list_t** bkeylist);

dbe_ret_t dbe_index_undobkeylist(
        dbe_index_t* index,
        su_list_t* bkeylist,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd);

void dbe_index_freebkeylist(
        rs_sysi_t* cd,
        su_list_t* bkeylist);

#endif /* SS_MYSQL */

dbe_ret_t dbe_index_bkey_delete_physical(
        dbe_index_t* index,
        dbe_bkey_t* k,
        rs_sysi_t* cd);

void dbe_index_locktree(
        dbe_index_t* index);

void dbe_index_unlocktree(
        dbe_index_t* index);

void dbe_index_setbkeyflags(
        dbe_bkey_t* k,
        dbe_indexop_mode_t mode);

SS_INLINE ulong dbe_index_getseqsealimit(
        dbe_index_t* index);

SS_INLINE ulong dbe_index_getreadaheadsize(
        dbe_index_t* index);

SS_INLINE uint dbe_index_getbloblimit_low(
        dbe_index_t* index);

SS_INLINE uint dbe_index_getbloblimit_high(
        dbe_index_t* index);

void dbe_index_hsbsetbloblimit_high(
        dbe_index_t* index,
        bool addslackp);

void dbe_index_searchadd(
        dbe_index_t* index,
        dbe_indsea_t* indsea,
        dbe_index_sealnode_t* sealist_node,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle);

void dbe_index_searchremove(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealist_node,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle);

void dbe_index_searchbeginactive(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle);

void dbe_index_searchendactive(
        dbe_index_t* index,
        dbe_indsea_t* indsea,
        dbe_index_sealnode_t* sealru_node);

long dbe_index_getnewseaid(
        dbe_index_t* index);

long dbe_index_getseabufused(
        dbe_index_t* index);

bool dbe_index_print(
        dbe_index_t* index,
        bool values);

bool dbe_index_printfp(
        void* fp,
        dbe_index_t* index,
        bool values);

bool dbe_index_check(
        dbe_index_t* index,
        bool full_check);

void dbe_index_printinfo(
        void* fp,
        dbe_index_t* index);

#if defined(DBE5INDE_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_index_getbkeyinfo
 *
 * Returns transaction buffer object.
 *
 * Parameters :
 *
 *	index - in
 *
 *
 * Return value - ref :
 *
 *      Key info object.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_bkeyinfo_t* dbe_index_getbkeyinfo(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_bkeyinfo);
}

/*##**********************************************************************\
 *
 *		dbe_index_gettrxbuf
 *
 * Returns transaction buffer object.
 *
 * Parameters :
 *
 *	index - in
 *
 *
 * Return value - ref :
 *
 *      Transaction buffer object.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_trxbuf_t* dbe_index_gettrxbuf(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_trxbuf);
}

/*##**********************************************************************\
 *
 *		dbe_index_getbonsaitree
 *
 * Returns bonsai tree object.
 *
 * Parameters :
 *
 *	index - in
 *
 *
 * Return value - ref :
 *
 *      Bonsai-tree object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_btree_t* dbe_index_getbonsaitree(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_bonsaitree);
}

/*##**********************************************************************\
 *
 *		dbe_index_getpermtree
 *
 * Returns permanent tree object.
 *
 * Parameters :
 *
 *	index - in
 *
 *
 * Return value - ref :
 *
 *      Permanent tree object.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_btree_t* dbe_index_getpermtree(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_permtree);
}

/*##**********************************************************************\
 *
 *		dbe_index_indsealist_reach
 *
 *
 *
 * Parameters :
 *
 *	index -
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
SS_INLINE void dbe_index_indsealist_reach(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        SsFlatMutexLock(index->ind_listsem);
}

/*##**********************************************************************\
 *
 *		dbe_index_indsealist_release
 *
 *
 *
 * Parameters :
 *
 *	index -
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
SS_INLINE void dbe_index_indsealist_release(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        SsFlatMutexUnlock(index->ind_listsem);
}

/*##**********************************************************************\
 * 
 *		dbe_index_mergegate_enter_shared
 * 
 * Enters merge gate in shared mode.
 * 
 * Parameters : 
 * 
 *		index - 
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
SS_INLINE void dbe_index_mergegate_enter_shared(dbe_index_t* index, long keyid)
{
        su_profile_timer;

        CHK_INDEX(index);

        su_profile_start;
        
        if (!dbe_cfg_usenewbtreelocking) {
            su_gate_enter_shared(index->ind_mergegate[keyid % DBE_INDEX_NMERGEGATE]);
        }
        
        su_profile_stop("dbe_index_mergegate_enter_shared");
}

/*##**********************************************************************\
 * 
 *		dbe_index_mergegate_enter_exclusive
 * 
 * Enters merge gate in exclusive mode.
 * 
 * Parameters : 
 * 
 *		index - 
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
SS_INLINE void dbe_index_mergegate_enter_exclusive(dbe_index_t* index, long keyid)
{
        su_profile_timer;

        CHK_INDEX(index);

        su_profile_start;

        if (!dbe_cfg_usenewbtreelocking) {
            su_gate_enter_exclusive(index->ind_mergegate[keyid % DBE_INDEX_NMERGEGATE]);
        }
        
        su_profile_stop("dbe_index_mergegate_enter_exclusive");
}

/*##**********************************************************************\
 * 
 *		dbe_index_mergegate_exit
 * 
 * Exits from merge gate.
 * 
 * Parameters : 
 * 
 *		index - 
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
SS_INLINE void dbe_index_mergegate_exit(dbe_index_t* index, long keyid)
{
        CHK_INDEX(index);

        if (!dbe_cfg_usenewbtreelocking) {
            su_gate_exit(index->ind_mergegate[keyid % DBE_INDEX_NMERGEGATE]);
        }
}

/*##**********************************************************************\
 *
 *		dbe_index_ismergeactive
 *
 * Checks if the index merge process is active.
 *
 * NOTE! This does not check if the merge is at the end state.
 *       This just checks if dbe_index_mergeinit is called and
 *       dbe_index_mergedone is not yet called. To check if merge
 *       is at the end state, call function dbe_index_mergestep.
 *
 * Parameters :
 *
 *	index - in, use
 *		index system
 *
 * Return value :
 *
 *      TRUE  - merge is active
 *      FALSE - merge is not active
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool dbe_index_ismergeactive(dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_mergeactive > 0);
}

/*##**********************************************************************\
 *
 *		dbe_index_isearlyvld
 *
 * Checks is early validate set set to on.
 *
 * Parameters :
 *
 *	index - in
 *		Index system.
 *
 * Return value :
 *
 *      TRUE    - early validate is used
 *      FALSE   - no early validate
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE bool dbe_index_isearlyvld(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_go->go_earlyvld);
}

SS_INLINE dbe_gobj_t* dbe_index_getgobj(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_go);
}

/*##**********************************************************************\
 *
 *		dbe_index_getseqsealimit
 *
 * Returns sequential search limit.
 *
 * Parameters :
 *
 *	index -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE ulong dbe_index_getseqsealimit(dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_seqsealimit);
}

/*##**********************************************************************\
 *
 *		dbe_index_getreadaheadsize
 *
 *
 *
 * Parameters :
 *
 *	index -
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
SS_INLINE ulong dbe_index_getreadaheadsize(dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_readaheadsize);
}

/*##**********************************************************************\
 *
 *		dbe_index_getbloblimit_low
 *
 *
 *
 * Parameters :
 *
 *	index -
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
SS_INLINE uint dbe_index_getbloblimit_low(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_bloblimit_low);
}

/*##**********************************************************************\
 *
 *		dbe_index_getbloblimit_high
 *
 *
 *
 * Parameters :
 *
 *	index -
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
SS_INLINE uint dbe_index_getbloblimit_high(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_bloblimit_high);
}

#endif /* defined(DBE5INDE_C) || defined(SS_USE_INLINE) */
           
#endif /* DBE5INDE_H */
