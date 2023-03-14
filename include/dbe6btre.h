/*************************************************************************\
**  source       * dbe6btre.h
**  directory    * dbe
**  description  * B+-tree.
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


#ifndef DBE6BTRE_H
#define DBE6BTRE_H

#include <ssc.h>
#include <sssem.h>

#include <su0svfil.h>
#include <su0list.h>
#include <su0gate.h>
#include <su0prof.h>

#include <rs0sysi.h>

#include "dbe9type.h"
#include "dbe6gobj.h"
#include "dbe6bkey.h"
#include "dbe6bsea.h"
#include "dbe0type.h"

#define BTREE_CHK(b)    ss_dassert(SS_CHKPTR(b) && (b)->b_chk == DBE_CHK_BTREE)

typedef enum {
        DBE_NODEPATH_GENERIC,
        DBE_NODEPATH_GENERIC_NOLOCK,
        DBE_NODEPATH_RELOCATE,
        DBE_NODEPATH_SPLIT
} dbe_nodepath_t;

/* Structure that represents the index tree.
 */
struct dbe_btree_st {
        dbe_gobj_t*     b_go;       /* Global objects. */
        su_daddr_t      b_rootaddr; /* Tree root address. */
        uint            b_maxlevel; /* Number of the highest level in the
                                       tree. This is one less than the
                                       number of levels in the tree. */
        SsSemT*         b_sem;
        su_gate_t*      b_gate;
        bool            b_bonsaip;
        dbe_bnode_t*    b_rootnode;
        ss_debug(int    b_chk;)
        ss_debug(bool   b_exclusivegate;)
};

typedef struct {
        dbe_dynbkey_t pi_firstkey;
        dbe_dynbkey_t pi_secondkey;
} dbe_pathinfo_t;

dbe_btree_t* dbe_btree_init(
        dbe_gobj_t* go,
        su_daddr_t root_addr,
        bool bonsaip);

su_daddr_t dbe_btree_done(
        dbe_btree_t* b);

SS_INLINE void dbe_btree_lock_exclusive(
        dbe_btree_t* b);

SS_INLINE void dbe_btree_lock_shared(
        dbe_btree_t* b);

SS_INLINE void dbe_btree_unlock(
        dbe_btree_t* b);

SS_INLINE dbe_gobj_t* dbe_btree_getgobj(
        dbe_btree_t* b);

su_list_t* dbe_btree_nodepath_init(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool cmp_also_header,
        dbe_info_t* info,
        dbe_nodepath_t type);

dbe_ret_t dbe_btree_nodepath_relocate(
        su_list_t* path,
        dbe_btree_t* b,
        dbe_info_t* info);

dbe_ret_t dbe_btree_nodepath_relocate_getnewaddr(
        su_list_t* path,
        dbe_btree_t* b,
        su_daddr_t* p_newaddr,
        dbe_info_t* info);

void dbe_btree_nodepath_done(
        su_list_t* path);

void dbe_btree_lockinfo_unlock(
        dbe_info_t* info, 
        dbe_btree_t* b);

dbe_ret_t dbe_btree_delete_empty(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_list_t* path,
        bool cmp_also_header,
        bool* p_shrink_tree,
        bool deleteblob,
        dbe_pathinfo_t* pi,
        rs_sysi_t* cd,
        dbe_info_t* info);

dbe_ret_t dbe_btree_updatepathinfo(
        dbe_btree_t* b,
        dbe_pathinfo_t* pi,
        rs_sysi_t* cd,
        dbe_info_t* info);

void dbe_btree_shrink_tree(
        dbe_btree_t* b,
        dbe_info_t* info);

SS_INLINE su_daddr_t dbe_btree_getrootaddr(
        dbe_btree_t* b);

SS_INLINE su_daddr_t dbe_btree_getrootaddr_nomutex(
        dbe_btree_t* b);

SS_INLINE dbe_bnode_t* dbe_btree_getrootnode_nomutex(
        dbe_btree_t* b);

SS_INLINE int dbe_btree_getrootlevel_nomutex(
        dbe_btree_t* b);

/* This is used by merge from dbe6bsea.c. 
 */
void dbe_btree_replacerootnode(
        dbe_btree_t* b, 
        dbe_bnode_t* root);

dbe_ret_t dbe_btree_insert(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool* p_isonlydelemark,
        rs_sysi_t* cd,
        dbe_info_t* info);

/* internal use only */
dbe_ret_t dbe_btree_delete_aux(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool cmp_also_header,
        bool deleteblob,
        rs_sysi_t* cd,
        dbe_info_t* info);

dbe_ret_t dbe_btree_insert_block(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_daddr_t* p_addr);

SS_INLINE dbe_ret_t dbe_btree_delete(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        dbe_info_t* info);

SS_INLINE dbe_ret_t dbe_btree_delete_keyandblob(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        rs_sysi_t* cd,
        dbe_info_t* info);

SS_INLINE dbe_ret_t dbe_btree_delete_vtplandblob(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        rs_sysi_t* cd,
        dbe_info_t* info);

bool dbe_btree_exists(
        dbe_btree_t* b,
        dbe_bkey_t* k);

dbe_ret_t dbe_btree_readpathforwrite(
        dbe_btree_t* b,
        dbe_bkey_t* k);

dbe_ret_t dbe_btree_getunique(
        dbe_btree_t* b,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_btrsea_timecons_t* tc,
        dbe_bkey_t* found_key,
        bool* p_deletenext,
        dbe_info_t* info);

bool dbe_btree_print(
        void* fp,
        dbe_btree_t* b,
        bool values,
        bool fail_on_error);

bool dbe_btree_check(
        dbe_btree_t* b,
        bool check_values);

void dbe_btree_getkeysamples(
        dbe_btree_t* b,
        vtpl_t* range_min,
        vtpl_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep);

SS_INLINE dbe_cache_t* dbe_btree_getcache(
        dbe_btree_t* b);

SS_INLINE int dbe_btree_getheight(
        dbe_btree_t* b);

#ifdef SS_DEBUG
bool dbe_btree_testrootnode(
        dbe_btree_t* b);
#endif


#if defined(DBE6BTRE_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		dbe_btree_lock_exclusive
 *
 * Enters B-tree gate in exclusive mode.
 *
 * Used in read-write operations until optimistic B-tree concurrency
 * control is tested and proven to work correctly.
 *
 * Parameters :
 *
 *	b -
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
SS_INLINE void dbe_btree_lock_exclusive(dbe_btree_t* b)
{
        su_profile_timer;

        ss_dprintf_1(("dbe_btree_lock_exclusive:\n"));
        BTREE_CHK(b);

        su_profile_start;

        su_gate_enter_exclusive(b->b_gate);

        ss_dprintf_2(("dbe_btree_lock_exclusive:lock ok\n"));
        ss_debug(b->b_exclusivegate = TRUE);
        SS_PMON_ADD(SS_PMON_BNODE_EXCLUSIVE);
        
        su_profile_stop("dbe_btree_lock_exclusive");
}

/*##**********************************************************************\
 *
 *		dbe_btree_lock_shared
 *
 * Enters B-tree gate in shared mode.
 *
 * Used in read-only operations until optimistic B-tree concurrency
 * control is tested and proven to work correctly.
 *
 * Parameters :
 *
 *	b -
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
SS_INLINE void dbe_btree_lock_shared(dbe_btree_t* b)
{
        su_profile_timer;

        ss_dprintf_1(("dbe_btree_lock_shared\n"));
        BTREE_CHK(b);

        su_profile_start;

        su_gate_enter_shared(b->b_gate);

        ss_dprintf_2(("dbe_btree_lock_shared:lock ok\n"));
        su_profile_stop("dbe_btree_lock_shared");
}

/*##**********************************************************************\
 *
 *		dbe_btree_unlock
 *
 * Exits B-tree gate.
 *
 * Parameters :
 *
 *	b -
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
SS_INLINE void dbe_btree_unlock(dbe_btree_t* b)
{
        ss_dprintf_1(("dbe_btree_unlock\n"));
        BTREE_CHK(b);
        ss_debug(b->b_exclusivegate = FALSE);
        ss_dassert(dbe_btree_testrootnode(b));

        su_gate_exit(b->b_gate);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getgobj
 *
 * Returns global object reference.
 *
 * Parameters :
 *
 *	b - in
 *		btree
 *
 * Return value - ref :
 *
 *      Global object reference.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_gobj_t* dbe_btree_getgobj(dbe_btree_t* b)
{
        ss_dprintf_1(("dbe_btree_getgobj\n"));
        BTREE_CHK(b);

        return(b->b_go);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getrootaddr_nomutex
 *
 * Returns tree root address without entering btree mutex.
 *
 * Parameters :
 *
 *	b - in, take
 *		pointer to the tree structure
 *
 * Return value :
 *
 *      Root node address of the tree.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE su_daddr_t dbe_btree_getrootaddr_nomutex(dbe_btree_t* b)
{
        BTREE_CHK(b);

        return(b->b_rootaddr);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getrootnode_nomutex
 *
 * Returns tree root node without entering btree mutex.
 *
 * Parameters :
 *
 *	b - in, take
 *		pointer to the tree structure
 *
 * Return value :
 *
 *      Root node address of the tree.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_bnode_t* dbe_btree_getrootnode_nomutex(dbe_btree_t* b)
{
        BTREE_CHK(b);
        ss_dassert(SsSemStkFind(b->b_bonsaip ? SS_SEMNUM_DBE_BTREE_BONSAI_GATE : SS_SEMNUM_DBE_BTREE_STORAGE_GATE));

        return(b->b_rootnode);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getrootlevel_nomutex
 *
 * Returns tree root level without entering btree mutex.
 *
 * Parameters :
 *
 *	b - in, take
 *		pointer to the tree structure
 *
 * Return value :
 *
 *      Root node address of the tree.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE int dbe_btree_getrootlevel_nomutex(dbe_btree_t* b)
{
        BTREE_CHK(b);

        return(b->b_maxlevel);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getrootaddr
 *
 * Returns tree root address.
 *
 * Parameters :
 *
 *	b - in, take
 *		pointer to the tree structure
 *
 * Return value :
 *
 *      Root node address of the tree.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE su_daddr_t dbe_btree_getrootaddr(dbe_btree_t* b)
{
        su_daddr_t root_addr;

        BTREE_CHK(b);

        dbe_btree_lock_shared(b);

        root_addr = b->b_rootaddr;

        dbe_btree_unlock(b);

        return(root_addr);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getcache
 *
 * Returns pointer to the cache structure onto which the index
 * tree is stored.
 *
 * Parameters :
 *
 *	b - in, use
 *		index tree
 *
 * Return value - ref :
 *
 *      cache pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_cache_t* dbe_btree_getcache(dbe_btree_t* b)
{
        BTREE_CHK(b);

        return(b->b_go->go_idxfd->fd_cache);
}

/*##**********************************************************************\
 *
 *		dbe_btree_getheight
 *
 *
 *
 * Parameters :
 *
 *	b -
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
SS_INLINE int dbe_btree_getheight(dbe_btree_t* b)
{
        BTREE_CHK(b);

        return(b->b_maxlevel + 1);
}

/*##**********************************************************************\
 *
 *		dbe_btree_delete
 *
 * Deletes a key value from the index tree.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_ret_t dbe_btree_delete(dbe_btree_t* b, dbe_bkey_t* k, dbe_info_t* info)
{
        dbe_ret_t rc;

        SS_PUSHNAME("dbe_btree_delete");
        ss_dprintf_1(("dbe_btree_delete\n"));
        BTREE_CHK(b);
        ss_output_2(dbe_bkey_dprint(2, k));
        ss_debug(SsDbgCheckAssertStop());

        rc = dbe_btree_delete_aux(b, k, TRUE, FALSE, NULL, info);

        SS_POPNAME;
        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_btree_delete_keyandblob
 *
 * Deletes a key value from the index tree. Also possible blob data
 * associated to the the key value is deleted.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value
 *
 *      cd - in, use
 *          client data context
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_ret_t dbe_btree_delete_keyandblob(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_btree_delete_keyandblob\n"));
        BTREE_CHK(b);
        ss_output_2(dbe_bkey_dprint(2, k));
        ss_debug(SsDbgCheckAssertStop());

        rc = dbe_btree_delete_aux(b, k, TRUE, TRUE, cd, info);

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_btree_delete_vtplandblob
 *
 * Deletes a key value from the index tree with the same v-tuple entry,
 * the header part can be different. Also possible blob data
 * associated to the the key value is deleted.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value
 *
 *      cd - in, use
 *          client data context
 *
 * Return value :
 *
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_ret_t dbe_btree_delete_vtplandblob(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_btree_delete_vtpl\n"));
        BTREE_CHK(b);
        ss_output_2(dbe_bkey_dprint(2, k));
        ss_debug(SsDbgCheckAssertStop());

        rc = dbe_btree_delete_aux(b, k, FALSE, TRUE, cd, info);

        return(rc);
}

#endif /* defined(DBE6BTRE_C) || defined(SS_USE_INLINE) */

#endif /* DBE6BTRE_H */
