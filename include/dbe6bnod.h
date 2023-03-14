/*************************************************************************\
**  source       * dbe6bnod.h
**  directory    * dbe
**  description  * B+-tree node.
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


#ifndef DBE6BNOD_H
#define DBE6BNOD_H

#include <su0svfil.h>
#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe6bkey.h"
#include "dbe0erro.h"

/* Node split info structure. When a node is split, the information
 * about the split is returned in this structure.
 */
typedef struct {
        dbe_bkey_t* ns_k;       /* Split key that separates the two nodes. */
        int         ns_level;   /* Level into which the split key must be
                                   inserted.*/
} dbe_bnode_splitinfo_t;

#define CHK_BNODE(n) ss_dassert(SS_CHKPTR(n) && (n)->n_chk == DBE_CHK_BNODE)

/* node info bits */
#define BNODE_MISMATCHARRAY     2

#if defined(SS_DEBUG) || defined(SS_DEBUGGER)
#define BNODE_DEBUGOFFSET       1
#else
#define BNODE_DEBUGOFFSET       0
#endif

/* Field offsets in a node in disk format. */
#define BNODE_BLOCKTYPE     0   /* 1 byte */
#define BNODE_CPNUM         1   /* 4 bytes */
#define BNODE_LEN           5   /* 2 bytes */
#define BNODE_COUNT         7   /* 2 bytes */
#define BNODE_SEQINSCOUNT   9   /* 1 byte */
#define BNODE_LEVEL         10  /* 1 byte */
#define BNODE_INFO          11  /* 1 byte */
#define BNODE_KEYS          12  /* variable length key values */

/* Header section length before the key values. */
#define BNODE_HEADERLEN  BNODE_KEYS   /* same as offset of the key values */

/* Macros to retrieve node fields from the disk format. */
#define BNODE_LOADLEN(p)             SS_UINT2_LOADFROMDISK(&(p)[BNODE_LEN])
#define BNODE_LOADCOUNT(p)           SS_UINT2_LOADFROMDISK(&(p)[BNODE_COUNT])

#define BNODE_GETSEQINSCOUNT(p)     ((uint)(uchar)(p)[BNODE_SEQINSCOUNT])
#define BNODE_GETLEVEL(p)           ((uint)(uchar)(p)[BNODE_LEVEL])
#define BNODE_GETINFO(p)            ((uint)(uchar)(p)[BNODE_INFO])
#define BNODE_GETKEYPTR(p)          (&(p)[BNODE_KEYS])

/* Macros to set node fields to a disk format. */
#define BNODE_STORELEN(p, l)       SS_UINT2_STORETODISK(&(p)[BNODE_LEN], l)
#define BNODE_STORECOUNT(p, c)     SS_UINT2_STORETODISK(&(p)[BNODE_COUNT], c)

#define BNODE_SETSEQINSCOUNT(p, c) ((p)[BNODE_SEQINSCOUNT] = (uchar)(c))
#define BNODE_SETLEVEL(p, l)       ((p)[BNODE_LEVEL] = (char)(l))
#define BNODE_SETINFO(p, i)        ((p)[BNODE_INFO] = (char)(i))

#define BNODE_ISSEQINS(n)   ((n)->n_seqinscount > SEQINSCOUNT_TRESHOLD || \
                             (n)->n_seqinscount >= (uint)((n)->n_count))

/* The SEQINSCOUNT_TRESHOLD is used during node split to detect the case
   when there are sequential inserts at the end of the node. In that case
   the node is not split in the middle but at position approximately
   between SEQINSCOUNT_MINSPLITPOS and SEQINSCOUNT_MAXSPLITPOS percentages
   of the total node lenght.
*/
#define SEQINSCOUNT_TRESHOLD        5
#define SEQINSCOUNT_SPLITPOS        90
#define SEQINSCOUNT_MINSPLITPOS     85
#define SEQINSCOUNT_MAXSPLITPOS     95

/* The following defines are used during node split to approximately
   specify the range in percentages between which the split key is
   selected.
*/
#define NORMAL_SPLITPOS             50
#define NORMAL_MINSPLITPOS          45
#define NORMAL_MAXSPLITPOS          55

/* The SPLITPOS_ADJUST is used to adjust the split position in
   a case when the new key will not fit into split node. The value
   is a percentage that is added or removed from the original split
   position minimum and maximum values to get a new split range.
   It is possible that the adjust value is added or removed several
   times from the split range to be able to split the node correctly.
*/
#define SPLITPOS_ADJUST             20

/* The LARGEKEYS_TRESHOLD is used during the node split to detect a case
   when there are very large (long) key values in the node. If there are
   less than LARGEKEYS_TRESHOLD key values in the node and the node splits,
   the split key is selected as the absolutely shortest key value in the
   node instead of the shortesr key value in the middle of the node.
*/
#define LARGEKEYS_TRESHOLD      3

#define BNODE_SPLITNEEDED(n, nklen) \
        (BNODE_HEADERLEN + (n)->n_len + (nklen) + ((n)->n_count + 1) * 4 > (n)->n_go->go_idxfd->fd_blocksize)

/* Structure used during a node split. An array of these structures
   is generated, and each element contains a key value position in
   the node and the full key value lenght at that position. This
   information is used to select the best key for node split.
*/
typedef struct {
        uint kp_pos;    /* Key position in the node index from the start of
                           the node. */
        uint kp_len;    /* Full key value length (in index nodes) or the
                           length of the common part with the previous
                           key (in leaf nodes). */
} bnode_keypos_t;

typedef enum {
        DBE_BNODE_CLEANUP_USER = 100,
        DBE_BNODE_CLEANUP_MERGE,
        DBE_BNODE_CLEANUP_MERGE_ALL,
} dbe_bnode_cleanup_t;

/* Index tree node structure.
 */
struct dbe_bnode_st {
        ss_debug(dbe_chk_t n_chk;)
        int               n_len;       /* Length of key values in the node. */
        int               n_count;     /* Number of key values in the node. */
        int               n_level;     /* Node level in the tree, 0 is the
                                          lowest level. */
        int               n_info;      /* Node info bits. */
        dbe_gobj_t*       n_go;        /* Global objects. */
        dbe_cacheslot_t*  n_cacheslot; /* Cache slot of the current node. */
        su_daddr_t        n_addr;      /* Node address in database file. */
        bool              n_dirty;     /* Flag: is this node modified. */
        bool              n_lastuse;   /* If TRUE, release with lastuse
                                          mode. */
        bool              n_bonsaip;   /* If TRUE, a Bonsai-tree node. */
        uint              n_seqinscount; /* Number of successive inserts to
                                            the node. */
        uint              n_lastinsindex; /* Index of last insert to the
                                             node. */
        dbe_cpnum_t       n_cpnum;     /* Node checkpoint number. */
        char*             n_p;         /* Node pointer returned from the
                                          cache. */
        char*             n_keys;      /* Pointer to the key values in the
                                          node. */
        ss_byte_t*        n_keysearchinfo_array;

};

/* State of a range search inside one node.
 */
typedef struct dbe_bnode_rsea_st {
        int             nrs_index;       /* Key index in node. */
        char*           nrs_keys;        /* Current key position. */
        dbe_bkrs_t*     nrs_krs;         /* Key range search. */
        int             nrs_count;       /* Key count in the node. */
        int             nrs_saveindex;   /* Saved position index leaf, used
                                            in prev calls. */
        char*           nrs_savepos;     /* Saved position key pointer. */
        dbe_bkey_t*     nrs_savekey;     /* Saved key value. */
        dbe_bnode_t*    nrs_n;           /* Search node. */
        bool            nrs_initialized; /* Flag: is search initialized. */
} dbe_bnode_rsea_t;

void dbe_bnode_reachinfo_init(
        void);

void dbe_bnode_reachinfo_done(
        void);

dbe_bnode_t* dbe_bnode_create(
        dbe_gobj_t* go,
        bool bonsaip,
        dbe_ret_t* p_rc,
        dbe_info_t* info);

void dbe_bnode_remove(
        dbe_bnode_t* n);

dbe_bnode_t* dbe_bnode_get(
        dbe_gobj_t* go,
        su_daddr_t addr,
        bool bonsaip,
        int level,
        dbe_cache_reachmode_t reachmode,
        dbe_info_t* info);

#define dbe_bnode_getreadonly(go, addr, bonsaip, info) \
        dbe_bnode_get(go, addr, bonsaip, -1, DBE_CACHE_READONLY, info)

#define dbe_bnode_getreadwrite(go, addr, bonsaip, level, info) \
        dbe_bnode_get(go, addr, bonsaip, level, DBE_CACHE_READWRITE, info)

#define dbe_bnode_getreadwrite_nocopy(go, addr, bonsaip, level, info) \
        dbe_bnode_get(go, addr, bonsaip, level, DBE_CACHE_READWRITE_NOCOPY, info)

#define dbe_bnode_getreadwrite_search(go, addr, bonsaip, level, info) \
        dbe_bnode_get(go, addr, bonsaip, level, DBE_CACHE_READWRITE, info)

SS_INLINE void dbe_bnode_write(
        dbe_bnode_t* n,
        bool lastuse);

void dbe_bnode_writewithmode(
        dbe_bnode_t* n,
        dbe_cache_releasemode_t mode);

dbe_bnode_t* dbe_bnode_init_tmp(
        dbe_gobj_t* go);

void dbe_bnode_done_tmp(
        dbe_bnode_t* n);

void dbe_bnode_copy_tmp(
        dbe_bnode_t* target,
        dbe_bnode_t* source);

dbe_bnode_t* dbe_bnode_relocate(
        dbe_bnode_t* n,
        su_daddr_t* p_addr,
        dbe_ret_t* p_rc,
        dbe_info_t* info);

void dbe_bnode_setreadonly(
        dbe_bnode_t* n);

dbe_bnode_t* dbe_bnode_initbyslot(
        dbe_cacheslot_t* cacheslot,
        char* p,
        su_daddr_t addr,
        bool bonsaip,
        dbe_gobj_t* go);

SS_INLINE su_daddr_t dbe_bnode_getaddr(
        dbe_bnode_t* n);

SS_INLINE int dbe_bnode_getkeycount(
        dbe_bnode_t* n);

SS_INLINE uint dbe_bnode_getlevel(
        dbe_bnode_t* n);

SS_INLINE dbe_cpnum_t dbe_bnode_getcpnum(
        dbe_bnode_t* n);

SS_INLINE bool dbe_bnode_ischanged(
        dbe_bnode_t* n);

void dbe_bnode_setcpnum(
        dbe_bnode_t* n,
        dbe_cpnum_t cpnum);

dbe_bkey_t* dbe_bnode_getfirstkey(
        dbe_bnode_t* n);

bool dbe_bnode_getbonsai(
        dbe_bnode_t* n);

dbe_ret_t dbe_bnode_insertkey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool split_cleanup,
        bool* p_isonlydeletemark,
        dbe_bnode_splitinfo_t* nsi,
        rs_sysi_t* cd,
        dbe_info_t* info);

dbe_ret_t dbe_bnode_insertkey_block(
        dbe_bnode_t* n,
        dbe_bkey_t* k);

dbe_ret_t dbe_bnode_deletekey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool cmp_also_header,
        bool delete_when_empty,
        bool deleteblob,
        bool deletefirstleafkey,
        rs_sysi_t* cd,
        dbe_info_t* info);

void dbe_bnode_getpathinfo(
        dbe_bnode_t* n,
        dbe_dynbkey_t* firstk,
        dbe_dynbkey_t* secondk);

bool dbe_bnode_getaddrinkey(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        su_daddr_t* p_addr);

su_daddr_t dbe_bnode_searchnode(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        bool cmp_also_header);

void dbe_bnode_rsea_initst(
        dbe_bnode_rsea_t* nrs,
        dbe_bnode_t* n,
        dbe_bkrs_t* krs);

void dbe_bnode_rsea_initst_error(
        dbe_bnode_rsea_t* nrs);

SS_INLINE void dbe_bnode_rsea_donest(
        dbe_bnode_rsea_t* nrs);

void dbe_bnode_rsea_skipleaf(
        dbe_bnode_rsea_t* nrs);

SS_INLINE void dbe_bnode_rsea_getkeypos(
        dbe_bnode_rsea_t* nrs,
        dbe_keypos_t* p_keypos);

dbe_ret_t dbe_bnode_rsea_next(
        dbe_bnode_rsea_t* nrs,
        dbe_srk_t* srk);

dbe_ret_t dbe_bnode_rsea_prev(
        dbe_bnode_rsea_t* nrs,
        dbe_srk_t* srk);

su_daddr_t dbe_bnode_rsea_nextnode(
        dbe_bnode_t* n,
        dbe_bkrs_t* krs,
        int longseqsea,
        uint readaheadsize,
        dbe_info_t* info);

su_daddr_t dbe_bnode_rsea_prevnode(
        dbe_bnode_t* n,
        dbe_bkrs_t* krs,
        rs_sysi_t* cd);

su_daddr_t dbe_bnode_rsea_resetnode(
        dbe_bnode_t* n,
        dbe_bkrs_t* krs,
        rs_sysi_t* cd);

dbe_ret_t dbe_bnode_getunique(
        dbe_bnode_t* n,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        int* p_i,
        char** p_keys,
        dbe_bkey_t* found_key);

dbe_ret_t dbe_bnode_getunique_next(
        dbe_bnode_t* n,
        dbe_bkey_t* ke,
        int* p_i,
        char** p_keys,
        dbe_bkey_t* found_key);

bool dbe_bnode_keyexists(
        dbe_bnode_t* n,
        dbe_bkey_t* k);

void dbe_bnode_inheritlevel(
        dbe_bnode_t* nn,
        dbe_bnode_t* on);

void dbe_bnode_changechildaddr(
        dbe_bnode_t* n,
        dbe_bkey_t* k,
        su_daddr_t newaddr);

dbe_ret_t dbe_bnode_cleanup(
        dbe_bnode_t* n,
        long* p_nkeyremoved,
        long* p_nmergeremoved,
        rs_sysi_t* cd,
        dbe_bnode_cleanup_t cleanup_type);

long dbe_bnode_getlength(
        char* n);

bool dbe_bnode_print(
        void* fp,
        char* n,
        size_t blocksize);

bool dbe_bnode_printtree(
        void* fp,
        dbe_bnode_t* n,
        bool values);

bool dbe_bnode_checktree(
        dbe_bnode_t* n,
        bool check_values);

bool dbe_bnode_getrandomsample(
        dbe_bnode_t* n,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_bkey_t** p_found_key);

bool dbe_bnode_getrandomaddress(
        dbe_bnode_t* n,
        dbe_bkey_t* kmin,
        dbe_bkey_t* kmax,
        su_daddr_t* p_addr);

void dbe_bnode_getkeysamples(
        dbe_bnode_t* n,
        dbe_bkey_t* range_min,
        dbe_bkey_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep);

bool dbe_bnode_test(
        dbe_bnode_t* n);

bool dbe_bnode_comparekeys(
        dbe_bnode_t* n1,
        dbe_bnode_t* n2);

uint dbe_bnode_maxkeylen(
        uint bufsize);

void dbe_bnode_getdata(
        dbe_bnode_t* n,
        char** p_data,
        int* p_len);

void dbe_bnode_initempty(
        char* buf);

#if defined(DBE6BNOD_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_bnode_getaddr
 *
 * Returns the node address in database file.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value :
 *
 *      node address
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE su_daddr_t dbe_bnode_getaddr(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(n->n_addr);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getkeycount
 *
 * Returns the number of key values in a node.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value :
 *
 *      number of key values in the node
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE int dbe_bnode_getkeycount(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(n->n_count);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getlevel
 *
 * Returns the node level in the tree.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value :
 *
 *      node level in the tree
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE uint dbe_bnode_getlevel(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(n->n_level);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_getcpnum
 *
 * Returns the cpnum of the node.
 *
 * Parameters :
 *
 *	n - in, use
 *		node pointer
 *
 * Return value :
 *
 *      node cpnum
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_cpnum_t dbe_bnode_getcpnum(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(n->n_cpnum);
}

SS_INLINE bool dbe_bnode_ischanged(dbe_bnode_t* n)
{
        CHK_BNODE(n);

        return(dbe_cacheslot_isoldversion(n->n_cacheslot));
}

/*##**********************************************************************\
 *
 *		dbe_bnode_write
 *
 * Writes a node back to cache. No physical write is done if
 * the node is not modified or the node is the root node. All nodes
 * retrieved using functions dbe_bnode_create or dbe_bnode_read must be
 * released using this function, function dbe_bnode_release or function
 * dbe_bnode_writewithmode.
 *
 * Parameters :
 *
 *	n - in, take
 *		node pointer
 *
 *      lastuse - in
 *          If TRUE, this is the last use of this node and the node
 *          is not put at the head of the cache LRU chain.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bnode_write(dbe_bnode_t* n, bool lastuse)
{
        dbe_cache_releasemode_t mode;

        CHK_BNODE(n);
        ss_dprintf_1(("dbe_bnode_write:addr = %ld\n", n->n_addr));

        if (lastuse || n->n_lastuse) {
            mode = n->n_dirty ? DBE_CACHE_DIRTYLASTUSE : DBE_CACHE_CLEANLASTUSE;
        } else {
            mode = n->n_dirty ? DBE_CACHE_DIRTY : DBE_CACHE_CLEAN;
        }

        dbe_bnode_writewithmode(n, mode);
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_donest
 *
 * Releases a node range search.
 *
 * Parameters :
 *
 *	nrs - in out, use
 *		node range search
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void dbe_bnode_rsea_donest(dbe_bnode_rsea_t* nrs)
{
        SS_NOTUSED(nrs);
        ss_dassert(nrs != NULL);

        if (nrs->nrs_savekey != NULL) {
            dbe_dynbkey_free(&nrs->nrs_savekey);
        }
}

/*##**********************************************************************\
 *
 *		dbe_bnode_rsea_getkeypos
 *
 * Returns the current search key position inside the node in
 * *p_keypos, if p_keypos is not NULL.
 *
 * Parameters :
 *
 *	nrs - in, use
 *		node range search
 *
 *	p_keypos - out
 *		If non-NULL, the position of the current key in the node
 *		is stored into *p_keypos
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE void dbe_bnode_rsea_getkeypos(
        dbe_bnode_rsea_t* nrs,
        dbe_keypos_t* p_keypos)
{
        CHK_BNODE(nrs->nrs_n);

        if (p_keypos != NULL) {
            *p_keypos = (dbe_keypos_t)0;
            if (nrs->nrs_index == 0) {
                *p_keypos = (dbe_keypos_t)((int)(*p_keypos) | (int)DBE_KEYPOS_FIRST);
            }
            if (nrs->nrs_index >= nrs->nrs_count - 1) {
                *p_keypos = (dbe_keypos_t)((int)(*p_keypos) | (int)DBE_KEYPOS_LAST);
            }
        }
}

#endif /* defined(DBE6BNOD_C) || defined(SS_USE_INLINE) */

#endif /* DBE6BNOD_H */
