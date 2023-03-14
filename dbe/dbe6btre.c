/*************************************************************************\
**  source       * dbe6btre.c
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


#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

This module implements a B+-tree.


Limitations:
-----------


Error handling:
--------------

Errors are handled using asserts.

Objects used:
------------

v-tuples                    uti0vtpl, uti0vcmp
split virtual file          su0svfil
free list of file blocks    dbe8flst
cache                       dbe8cach
key value system            dbe6bkey
key range search            dbe6bkrs
tree node                   dbe6bnod

Preconditions:
-------------


Multithread considerations:
--------------------------

Now B-tree normally takes all upper level nodes in read-ony mode. In write
operations only leaf level node is taken in read-write mode. In read-only
operations also that node is taken in read-only mode. If node split or
relocate is needed then full path is taken in read-write mode. B-tree
operations are protected using a gate in shared mode expect operations
that may change the root address. Those take the B-tree gate in exclusive
mode.

Also merge search follows the above principle.

This should add more concurency to B-tree operations.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE6BTRE_C

#include <ssstdio.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <sssprint.h>

#include <uti0vtpl.h>
#include <uti0vcmp.h>

#include <su0error.h>
#include <su0svfil.h>
#include <su0list.h>
#include <su0gate.h>
#include <su0prof.h>

#include "dbe9type.h"
#include "dbe8flst.h"
#include "dbe8cach.h"
#include "dbe6bkey.h"
#include "dbe6bkrs.h"
#include "dbe6bnod.h"
#include "dbe6btre.h"
#include "dbe6finf.h"
#include "dbe0db.h"

extern bool         dbe_reportindex;
extern long         dbe_curkeyid;
extern dbe_bkey_t*  dbe_curkey;

extern long dbe_bnode_totalnodelength;
extern long dbe_bnode_totalnodekeycount;
extern long dbe_bnode_totalnodecount;
extern long dbe_bnode_totalshortnodecount;

void dbe_btree_replacerootnode(dbe_btree_t* b, dbe_bnode_t* root)
{
        ss_pprintf_1(("dbe_btree_replacerootnode:rootaddr=%ld\n", b->b_rootaddr));
        ss_dassert(SsSemStkFind(b->b_bonsaip ? SS_SEMNUM_DBE_BTREE_BONSAI_GATE : SS_SEMNUM_DBE_BTREE_STORAGE_GATE));
        ss_dassert(b->b_exclusivegate);

        if (b->b_rootnode != NULL) {
            dbe_bnode_done_tmp(b->b_rootnode);
            b->b_rootnode = NULL;
        }
        if (b->b_maxlevel > 0 && !dbe_cfg_relaxedbtreelocking) {
            b->b_rootnode = dbe_bnode_init_tmp(b->b_go);
            dbe_bnode_copy_tmp(b->b_rootnode, root);
        }
}

#ifdef SS_DEBUG
bool dbe_btree_testrootnode(dbe_btree_t* b)
{
        bool succp;
        dbe_bnode_t* root;
        dbe_info_t info;

        ss_dassert(SsSemStkFind(b->b_bonsaip ? SS_SEMNUM_DBE_BTREE_BONSAI_GATE : SS_SEMNUM_DBE_BTREE_STORAGE_GATE));

        dbe_info_init(info, 0);

        if (b->b_rootnode == NULL) {
            succp = TRUE;
        } else {
            root = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
            succp = dbe_bnode_comparekeys(root, b->b_rootnode);
            dbe_bnode_write(root, FALSE);
        }
        return(succp);
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		dbe_btree_init
 *
 * Opens an existing index tree or creates a new one.
 *
 * Parameters :
 *
 *      go - in, hold
 *		Global object structure pointer.
 *
 *      root_addr - in
 *		tree root address, of SU_DADDR_NULL if tree does
 *          not exist
 *
 * Return value - give :
 *
 *      Pointer to the tree structure.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_btree_t* dbe_btree_init(
        dbe_gobj_t* go,
        su_daddr_t root_addr,
        bool bonsaip)
{
        dbe_btree_t* b;
        dbe_bnode_t* root;
        dbe_info_t info;

        ss_bprintf_1(("dbe_btree_init\n"));

        dbe_info_init(info, 0);
        dbe_bnode_reachinfo_init();

        b = SsMemAlloc(sizeof(dbe_btree_t));

        b->b_go = go;
        b->b_sem = SsSemCreateLocal(SS_SEMNUM_DBE_BTREE);
        b->b_gate = su_gate_init(bonsaip ? SS_SEMNUM_DBE_BTREE_BONSAI_GATE : SS_SEMNUM_DBE_BTREE_STORAGE_GATE, FALSE);
        b->b_bonsaip = bonsaip;
        ss_debug(b->b_exclusivegate = FALSE);

        if (root_addr == SU_DADDR_NULL) {

            /* Create a new index tree. */

            dbe_bkey_t* k;
            dbe_ret_t rc;

            /* Create the root node. */
            root = dbe_bnode_create(go, bonsaip, &rc, &info);
            su_rc_assert(rc == SU_SUCCESS, rc);
            ss_assert(dbe_bnode_getaddr(root) >= DBE_INDEX_HEADERSIZE);

            /* Create a key value that is smaller than any possible key
             * value in the index. This simplifies the insert and search
             * algorithms.
             */
            if (bonsaip) {
                k = dbe_bkey_initleaf(
                        NULL,
                        go->go_bkeyinfo,
                        DBE_TRXNUM_NULL,
                        DBE_TRXID_NULL,
                        VTPL_EMPTY);
            } else {
                k = dbe_bkey_initpermleaf(
                        NULL,
                        go->go_bkeyinfo,
                        VTPL_EMPTY);
            }
            dbe_bkey_settreeminvtpl(k);
            dbe_bkey_setcommitted(k);

            rc = dbe_bnode_insertkey(root, k, FALSE, NULL, NULL, NULL, NULL);
            su_rc_assert(rc == DBE_RC_SUCC, rc);

            dbe_bkey_done(k);

        } else {

            /* Open an existing index. */
            root = dbe_bnode_getreadonly(go, root_addr, bonsaip, &info);
        }

        b->b_maxlevel = dbe_bnode_getlevel(root);
        b->b_rootaddr = dbe_bnode_getaddr(root);
        b->b_rootnode = NULL;
        ss_debug(b->b_chk = DBE_CHK_BTREE;)

        if (b->b_bonsaip) {
            SS_PMON_SET(SS_PMON_BONSAIHEIGHT, b->b_maxlevel + 1);
        }

        dbe_btree_lock_exclusive(b);

        dbe_btree_replacerootnode(b, root);

        dbe_btree_unlock(b);

        dbe_bnode_write(root, FALSE);

        return(b);
}

/*##**********************************************************************\
 *
 *		dbe_btree_done
 *
 * Closes index tree and releases resources allocated to it.
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
su_daddr_t dbe_btree_done(dbe_btree_t* b)
{
        su_daddr_t root_addr;

        ss_bprintf_1(("dbe_btree_done\n"));
        BTREE_CHK(b);

        if (b->b_rootnode != NULL) {
            dbe_bnode_done_tmp(b->b_rootnode);
        }
        root_addr = b->b_rootaddr;
        SsSemFree(b->b_sem);
        su_gate_done(b->b_gate);

        SsMemFree(b);

        dbe_bnode_reachinfo_done();

        return(root_addr);
}

/*#**********************************************************************\
 *
 *		btree_splitroot
 *
 * Splits the root node of the tree. The tree grows one level higher.
 *
 * Parameters :
 *
 *	b - in, use
 *		index tree
 *
 *	or - in, use
 *		old root node
 *
 *	sk - in, use
 *		split key from the old root node
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t btree_splitroot(
        dbe_btree_t* b,
        dbe_bnode_t* or,
        dbe_bkey_t* sk,
        dbe_info_t* info)
{
        dbe_bnode_t* nr;    /* new root */
        dbe_bkey_t* k;      /* temporary key */
        dbe_bkey_t* fk;     /* first key of old root */
        dbe_ret_t rc;

        ss_pprintf_3(("btree_splitroot\n"));
        BTREE_CHK(b);
        ss_dassert(b->b_exclusivegate);

        if (b->b_rootnode != NULL) {
            dbe_bnode_done_tmp(b->b_rootnode);
            b->b_rootnode = NULL;
        }

        nr = dbe_bnode_create(b->b_go, b->b_bonsaip, &rc, info);
        if (nr == NULL) {
            return(rc);
        }

        dbe_bnode_inheritlevel(nr, or);

        fk = dbe_bnode_getfirstkey(or);

        /* Put the first key value from the old root node to the new root. */
        k = dbe_bkey_initsplit(NULL, b->b_go->go_bkeyinfo, fk);
        dbe_bkey_setaddr(k, dbe_bnode_getaddr(or));

        rc = dbe_bnode_insertkey(nr, k, FALSE, NULL, NULL, NULL, info);

        dbe_bkey_done(k);

        if (rc != DBE_RC_SUCC) {
            return(rc);
        }

        rc = dbe_bnode_insertkey(nr, sk, FALSE, NULL, NULL, NULL, info);
        if (rc != DBE_RC_SUCC) {
            return(rc);
        }

        if (b->b_bonsaip) {
            SS_PMON_SET(SS_PMON_BONSAIHEIGHT, b->b_maxlevel + 1);
        }

        dbe_btree_replacerootnode(b, nr);

        dbe_bnode_write(nr, FALSE);

        b->b_rootaddr = dbe_bnode_getaddr(nr);
        ss_pprintf_3(("btree_splitroot:b->b_rootaddr=%ld\n", (long)b->b_rootaddr));
        b->b_maxlevel = dbe_bnode_getlevel(nr);

        return(rc);
}

/*#**********************************************************************\
 *
 *		dbe_btree_nodepath_init
 *
 * Creates a path of tree nodes from the tree root to the leaf node.
 * The first node in the returned list is the tree root node and
 * the last node in the list is the leaf node. All nodes are locked
 * in read-write mode.
 *
 * Parameters :
 *
 *	b - in, use
 *		index tree
 *
 *	k - in, use
 *		key value in the leaf node to which the path is stored
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          only the v-tuple parts must match
 *
 * Return value - give :
 *
 *      list containing node path
 *
 * Limitations  :
 *
 * Globals used :
 */
su_list_t* dbe_btree_nodepath_init(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool cmp_also_header,
        dbe_info_t* info,
        dbe_nodepath_t type)
{
        dbe_bnode_t* n;
        dbe_bnode_t* prev_n;
        dbe_bnode_t* root_n;
        su_daddr_t addr;
        su_list_t* path;
        int maxkeylen;

        ss_bprintf_1(("dbe_btree_nodepath_init\n"));
        BTREE_CHK(b);

        if (!(dbe_cfg_relaxedbtreelocking & 1) && type != DBE_NODEPATH_GENERIC_NOLOCK) {
            type = DBE_NODEPATH_GENERIC;
        }

        if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
            switch (type) {
                case DBE_NODEPATH_GENERIC:
                    ss_bprintf_2(("dbe_btree_nodepath_init:DBE_NODEPATH_GENERIC\n"));
                    dbe_btree_lock_exclusive(b);
                    SS_PMON_ADD(SS_PMON_BTREE_LOCK_FULLPATH);
                    break;
                case DBE_NODEPATH_GENERIC_NOLOCK:
                    ss_bprintf_2(("dbe_btree_nodepath_init:DBE_NODEPATH_GENERIC_NOLOCK\n"));
                    SS_PMON_ADD(SS_PMON_BTREE_LOCK_FULLPATH);
                    break;
                case DBE_NODEPATH_RELOCATE:
                case DBE_NODEPATH_SPLIT:
                    ss_bprintf_2(("dbe_btree_nodepath_init:%s\n", type == DBE_NODEPATH_RELOCATE ? "DBE_NODEPATH_RELOCATE" : "DBE_NODEPATH_SPLIT"));
                    dbe_btree_lock_shared(b);
                    SS_PMON_ADD(SS_PMON_BTREE_LOCK_PARTIALPATH);
                    break;
                default:
                    ss_error;
            }
        }

        path = su_list_init(NULL);

        ss_bprintf_2(("dbe_btree_nodepath_init:rw:addr=%ld, root\n", b->b_rootaddr));
        if (b->b_maxlevel > 0 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
            n = dbe_bnode_getreadwrite_nocopy(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
        } else {
            n = dbe_bnode_getreadwrite(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
        }
        su_list_insertlast(path, n);

        root_n = n;

        /* Find all levels below the root.
        */
        for (;;) {
            int level;
            level = dbe_bnode_getlevel(n);
            if (level == 0) {
                /* This is the lowest level. */
                break;
            } else {
                /* Search the next lower level for the key.
                */
                ss_dassert(b->b_maxlevel > 0);
                addr = dbe_bnode_searchnode(n, k, cmp_also_header);
                ss_bprintf_2(("dbe_btree_nodepath_init:rw:addr=%ld, level=%d\n", addr, level));
                if (level > 1 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
                    n = dbe_bnode_getreadwrite_nocopy(b->b_go, addr, b->b_bonsaip, level-1, info);
                } else {
                    n = dbe_bnode_getreadwrite(b->b_go, addr, b->b_bonsaip, level-1, info);
                }
                switch (type) {
                    case DBE_NODEPATH_GENERIC:
                    case DBE_NODEPATH_GENERIC_NOLOCK:
                        break;
                    case DBE_NODEPATH_RELOCATE:
                        if (n->n_cpnum == dbe_counter_getcpnum(n->n_go->go_ctr)) {
                            ss_bprintf_2(("dbe_btree_nodepath_init:no need to relocate this node, we can remove previous node\n"));
                            prev_n = su_list_removefirst(path);
                            ss_assert(prev_n != NULL);
                            dbe_bnode_write(prev_n, FALSE);
                        }
                        break;
                    case DBE_NODEPATH_SPLIT:
                        maxkeylen = dbe_bkey_getlength(k);
                        if (!BNODE_SPLITNEEDED(n, maxkeylen)) {
                            ss_bprintf_2(("dbe_btree_nodepath_init:key will fit into this node, we can remove previous node\n"));
                            prev_n = su_list_removefirst(path);
                            ss_assert(prev_n != NULL);
                            dbe_bnode_write(prev_n, FALSE);
                        }
                        break;
                    default:
                        ss_error;
                }

                ss_poutput_3({ SsThrSwitch(); });

                su_list_insertlast(path, n);
            }
        }
        if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
            switch (type) {
                case DBE_NODEPATH_GENERIC:
                case DBE_NODEPATH_GENERIC_NOLOCK:
                    break;
                case DBE_NODEPATH_RELOCATE:
                case DBE_NODEPATH_SPLIT:
                    if (su_list_getfirst(path) == root_n) {
                        ss_bprintf_2(("dbe_btree_nodepath_init:root node is still in the list, we must get path in exclusive mode\n"));
                        dbe_btree_nodepath_done(path);
                        dbe_btree_unlock(b);
                        return(dbe_btree_nodepath_init(b, k, cmp_also_header, info, DBE_NODEPATH_GENERIC));
                    }
                    break;
                default:
                    ss_error;
            }
        }
        ss_poutput_3(
            {
                su_list_node_t* ln;
                dbe_bnode_t* bn;
                ss_pprintf_3(("dbe_btree_nodepath_init:locked path\n"));
                su_list_do_get(path, ln, bn) {
                    ss_pprintf_3(("dbe_btree_nodepath_init:level=%d, addr=%ld\n", dbe_bnode_getlevel(n), dbe_bnode_getaddr(n)));
                }
            }
        )
        return(path);
}

/*##**********************************************************************\
 *
 *		dbe_btree_nodepath_done
 *
 * Releases the node path.
 *
 * Parameters :
 *
 *	path - in, take
 *		node path
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btree_nodepath_done(su_list_t* path)
{
        int i;
        dbe_bnode_t* n;
        su_list_node_t* lnode;

        ss_bprintf_1(("dbe_btree_nodepath_done\n"));

        i = 0;
        su_list_do_get(path, lnode, n) {
            if (i > 0 && dbe_bnode_getkeycount(n) == 0) {
                ss_dprintf_2(("dbe_btree_nodepath_done:node that is not a root node is empty, remove it\n"));
                dbe_bnode_remove(n);
            } else {
                dbe_bnode_write(n, FALSE);
            }
            i++;
        }
        su_list_done(path);
}

/*#***********************************************************************\
 *
 *		dbe_btree_nodepath_relocate_getnewaddr
 *
 *
 *
 * Parameters :
 *
 *	path -
 *
 *
 *	b -
 *
 *
 *	p_newaddr -
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
dbe_ret_t dbe_btree_nodepath_relocate_getnewaddr(
        su_list_t* path,
        dbe_btree_t* b,
        su_daddr_t* p_newaddr,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        su_list_node_t* lnode;
        su_daddr_t newaddr;
        dbe_cpnum_t cpnum;
        dbe_ret_t rc;
        dbe_bnode_t* child_n = NULL;
        su_daddr_t   childaddr = SU_DADDR_NULL;
        bool first = TRUE;
        int nofailure = info->i_flags & DBE_INFO_DISKALLOCNOFAILURE;

        ss_pprintf_3(("dbe_btree_nodepath_relocate_getnewaddr\n"));
        BTREE_CHK(b);

        lnode = su_list_last(path);
        while (lnode != NULL) {
            n = su_listnode_getdata(lnode);

            cpnum = dbe_bnode_getcpnum(n);
            ss_dassert(cpnum <= dbe_counter_getcpnum(b->b_go->go_ctr));

            if (cpnum == dbe_counter_getcpnum(b->b_go->go_ctr)) {
                if (child_n) {
                    ss_bprintf_4(("dbe_btree_nodepath_relocate_getnewaddr:dbe_bnode_changechildaddr:addr=%ld\n",
                        childaddr));
                    dbe_bnode_changechildaddr(
                        n,
                        dbe_bnode_getfirstkey(child_n),
                        childaddr);
                    if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                        /* Root content changed. */
                        dbe_btree_replacerootnode(b, n);
                    }
                }
                break;
            }

            /* Relocate the node. */
            n = dbe_bnode_relocate(n, &newaddr, &rc, info);
            if (n == NULL) {
                if (!first) {
                    dbe_db_setfatalerror(b->b_go->go_db, rc);
                }
                goto return_rc;
            }
            if (first) {
                /* Only first relocate can fail. */
                first = FALSE;
                info->i_flags |= DBE_INFO_DISKALLOCNOFAILURE;
            }
            ss_bprintf_4(("dbe_btree_nodepath_relocate_getnewaddr:dbe_bnode_relocate:addr=%ld\n",
                newaddr));
            su_listnode_setdata(lnode, n);
            if (child_n) {
                dbe_bnode_changechildaddr(
                    n,
                    dbe_bnode_getfirstkey(child_n),
                    childaddr);
                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root content changed. */
                    dbe_btree_replacerootnode(b, n);
                }
            }

            if (p_newaddr != NULL && *p_newaddr == 0) {
                *p_newaddr = newaddr;
            }

            /* Change the node checkpoint number. This must be done after
               dbe_bnode_relocate. */
            dbe_bnode_setcpnum(n, dbe_counter_getcpnum(b->b_go->go_ctr));

            if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                /* Tree root relocated. */
                b->b_rootaddr = newaddr;
                ss_pprintf_3(("dbe_btree_nodepath_relocate_getnewaddr:b->b_rootaddr=%ld\n", (long)b->b_rootaddr));
            }

            /* Get the parent node. */
            lnode = su_list_prev(path, lnode);

            child_n = n;
            childaddr = newaddr;

/* fix tpr 690014: change the node only after relocate. */
#if 0
            if (lnode != NULL) {
                /* Parent node exists, change the child address in the
                   parent node. */
                dbe_bnode_t* parent;
                parent = su_listnode_getdata(lnode);
                dbe_bnode_changechildaddr(
                    parent,
                    dbe_bnode_getfirstkey(n),
                    newaddr);
            }
#endif
        }
        rc = DBE_RC_SUCC;

return_rc:
        if (!nofailure) {
            /* Restore old value (bit cleared) */
            info->i_flags &= ~DBE_INFO_DISKALLOCNOFAILURE;
        }
        return(rc);
}

/*##*********************************************************************\
 *
 *		dbe_btree_nodepath_relocate
 *
 * Relocates all nodes in a node path that has an older checkpoint
 * number than the current checkpoint number.
 *
 * Parameters :
 *
 *      path - in, use
 *          node path
 *
 *	b - in out, use
 *		index tree
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btree_nodepath_relocate(
        su_list_t* path,
        dbe_btree_t* b,
        dbe_info_t* info)
{
        BTREE_CHK(b);

        return(dbe_btree_nodepath_relocate_getnewaddr(path, b, NULL, info));
}

SS_INLINE bool dbe_btree_lockinfo_lock(dbe_btree_t* b, dbe_info_t* info, dbe_bnode_t* n, bool btree_locked)
{
        if (!dbe_cfg_usenewbtreelocking) {
            return(FALSE);
        }
        if (info == NULL || info->i_btreelockinfo == NULL) {
            return(FALSE);
        }
        ss_dprintf_1(("dbe_btree_lockinfo_lock:node addr=%ld, btree_locked=%d\n", n != NULL ? dbe_bnode_getaddr(n) : 0, btree_locked));
        ss_dassert(!(info->i_flags & DBE_INFO_TREEPRELOCKED));
        ss_dassert(!b->b_bonsaip);
        info->i_btreelockinfo->btli_bnode = n;
        info->i_btreelockinfo->btli_btreelocked = btree_locked;
        if (btree_locked) {
            SS_PMON_ADD(SS_PMON_BTREE_LOCK_TREE);
        } else {
            SS_PMON_ADD(SS_PMON_BTREE_LOCK_NODE);
        }
        return(TRUE);
}

void dbe_btree_lockinfo_unlock(dbe_info_t* info, dbe_btree_t* b)
{
        if (!dbe_cfg_usenewbtreelocking) {
            return;
        }
        if (info == NULL || info->i_btreelockinfo == NULL) {
            return;
        }
        if (info->i_btreelockinfo->btli_bnode != NULL) {
            ss_dprintf_1(("dbe_btree_lockinfo_unlock:node addr=%ld\n", dbe_bnode_getaddr(info->i_btreelockinfo->btli_bnode)));
            dbe_bnode_write(info->i_btreelockinfo->btli_bnode, FALSE);
            info->i_btreelockinfo->btli_bnode = NULL;
        }
        if (info->i_btreelockinfo->btli_btreelocked) {
            ss_dprintf_1(("dbe_btree_lockinfo_unlock:btree_locked\n"));
            dbe_btree_unlock(b);
            info->i_btreelockinfo->btli_btreelocked = FALSE;
        }
}

/*#**********************************************************************\
 *
 *		btree_insert_split
 *
 * Inserts a key value to a leaf node when the leaf node splits during
 * the insert. The insert may propagate to higher levels of the tree.
 * The path from the root node to the leaf node is given in parameter
 * 'path'. The path is locked in read-write mode.
 *
 * Parameters :
 *
 *      b - in out, use
 *          index tree
 *
 *      k - in, use
 *          key value that is inserted
 *
 *      path - in, use
 *          node path from the root node to the leaf node into where
 *          the key value is inserted
 *
 *      lnode - in
 *          if non-NULL this is the list node where insert is done
 *
 *      cd - in
 *
 *
 *      info - in
 *
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t btree_insert_split(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_list_t* path,
        su_list_node_t* lnode,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_ret_t rc;
        dbe_bnode_splitinfo_t nsi;
        dbe_bkey_t* tmpk = NULL;

        SS_PUSHNAME("btree_insert_split");
        ss_bprintf_3(("btree_insert_split\n"));
        BTREE_CHK(b);

        rc = DBE_RC_SUCC;

        if (lnode == NULL) {
            lnode = su_list_last(path);
        }

        while (lnode != NULL) {
            n = su_listnode_getdata(lnode);

            rc = dbe_bnode_insertkey(n, k, TRUE, NULL, &nsi, cd, info);

            if (rc == DBE_RC_NODESPLIT) {

                SS_PMON_ADD(SS_PMON_BNODE_NODESPLIT);

                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root split, create new root. */
                    rc = btree_splitroot(b, n, nsi.ns_k, info);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                } else {
                    if (tmpk == NULL) {
                        tmpk = dbe_bkey_init_ex(cd, b->b_go->go_bkeyinfo);
                        k = tmpk;
                    }
                    dbe_bkey_copy(k, nsi.ns_k);
                }
                dbe_bkey_done_ex(cd, nsi.ns_k);

            } else {

                /* No more split. */
                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root content changed. */
                    dbe_btree_replacerootnode(b, n);
                }
                break;
            }
            lnode = su_list_prev(path, lnode);
        }

        if (tmpk != NULL) {
            dbe_bkey_done_ex(cd, tmpk);
        }

        SS_POPNAME;
        return(rc);
}

/*#**********************************************************************\
 *
 *		btree_insordel_simple
 *
 * Does key value insert or delete in a simple case. In the simple case
 * the node does not split during insert or become empty during delete.
 * Only the leaf node is locked in read-write mode, all other modes are
 * locked in read-only mode to increase concurrency. If a simple insert
 * or delete is not possible, it is indicated by the return code and
 * nothing is done to the leaf node. Also the case when the node needs
 * relocateing is not considered as a simple case.
 *
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value that is inserted or deleted
 *
 *      insertp - in
 *          If TRUE, key value is inserted, otherwise it is deleted.
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          only the v-tuple parts must match
 *
 *      deleteblob - in
 *          If TRUE, also possible blob data is deleted
 *
 *      p_isonlydelemark - out
 *          If it is certain, that key the inserted key value is the only
 *          delete mark in the node, TRUE is set to *p_isonlydelemark,
 *          Otherwise FALSE is set to *p_isonlydelemark. If the information
 *          is not needed, p_isonlydelemark can be NULL.
 *
 *      cd - in, use
 *          client data context
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_RC_NODERELOCATE
 *      DBE_RC_NODESPLIT    - only if insertp is TRUE (insert case)
 *      DBE_RC_NODEEMPTY    - only if insertp is FALSE (delete case)
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t btree_insordel_simple(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool insertp,
        bool cmp_also_header,
        bool deleteblob,
        bool* p_isonlydelemark,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_bnode_t* rootnode;
        su_daddr_t addr;
        dbe_ret_t rc;
        int nodelevel;
        dbe_bnode_t* ntmp;
        bool unlock_tree;
        su_profile_timer;

        ss_bprintf_3(("btree_insordel_simple\n"));
        BTREE_CHK(b);

        if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
            dbe_btree_lock_shared(b);
            unlock_tree = TRUE;
        } else {
            unlock_tree = FALSE;
        }

        su_profile_start;

        if (b->b_maxlevel == 0) {

            /* Insert to or delete from the root node.
             */
            ss_bprintf_4(("btree_insordel_simple:rw:addr=%ld, root\n", b->b_rootaddr));
            ss_dassert(b->b_rootnode == NULL);
            n = dbe_bnode_getreadwrite(b->b_go, b->b_rootaddr, b->b_bonsaip, 0, info);
            ss_dassert(dbe_bnode_getlevel(n) == 0);

            if (insertp) {
                rc = dbe_bnode_insertkey(n, k, TRUE, p_isonlydelemark, NULL, cd, info);
            } else {
                rc = dbe_bnode_deletekey(
                        n, k, cmp_also_header, FALSE, deleteblob, FALSE, cd, info);
            }

            if (!dbe_btree_lockinfo_lock(b, info, n, unlock_tree)) {
                dbe_bnode_write(n, FALSE);
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
            }
            su_profile_stop("btree_insordel_simple");
            ss_bprintf_4(("btree_insordel_simple:return %d\n", rc));
            return(rc);
        }

        ss_bprintf_4(("btree_insordel_simple:r:addr=%ld, root\n", b->b_rootaddr));
        n = dbe_btree_getrootnode_nomutex(b);
        rootnode = n;
        if (n == NULL) {
            n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, info);
        }
        nodelevel = dbe_bnode_getlevel(n);
        ss_dassert(nodelevel > 0);
        ss_dassert((uint)nodelevel == b->b_maxlevel);

        /* Find the level for insert or delete.
         */
        SS_PUSHNAME("btree_insordel_simple: find insdel level");

        for (;;) {

            /* Search the next lower level for the key.
             */
            addr = dbe_bnode_searchnode(n, k, cmp_also_header);

            if (nodelevel == 1) {

                /* Insert to or delete from the next level. Get the
                 * node in read-write mode.
                 */
                ss_bprintf_4(("btree_insordel_simple:rw:addr=%ld, level=%d\n", addr, nodelevel));
                ntmp = dbe_bnode_getreadwrite(b->b_go, addr, b->b_bonsaip, 0, info);
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                n = ntmp;
                ss_dassert(dbe_bnode_getlevel(n) == 0);

                if (insertp) {
                    rc = dbe_bnode_insertkey(n, k, TRUE, p_isonlydelemark, NULL, cd, info);
                } else {
                    rc = dbe_bnode_deletekey(
                            n, k, cmp_also_header, FALSE, deleteblob, FALSE, cd, info);
                }

                if (!dbe_btree_lockinfo_lock(b, info, n, unlock_tree)) {
                    dbe_bnode_write(n, FALSE);
                    if (unlock_tree) {
                        dbe_btree_unlock(b);
                    }
                }
                SS_POPNAME;
                su_profile_stop("btree_insordel_simple");
                ss_bprintf_4(("btree_insordel_simple:return %d\n", rc));
                return(rc);

            } else {

                /* Go to the lower node.
                */
                ss_bprintf_4(("btree_insordel_simple:r:addr=%ld, level=%d\n", addr, nodelevel));
                ntmp = dbe_bnode_getreadonly(b->b_go, addr, b->b_bonsaip, info);
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree && dbe_cfg_usenewbtreelocking) {
                    dbe_btree_unlock(b);
                    unlock_tree = FALSE;
                }
                rootnode = NULL;
                n = ntmp;
                nodelevel = dbe_bnode_getlevel(n);
            }
        }
        ss_error;
        SS_POPNAME;
        return(DBE_ERR_FAILED);
}

/*##**********************************************************************\
 *
 *		dbe_btree_insert
 *
 * Inserts a key value into the index tree.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value
 *
 *      p_isonlydelemark - out
 *          If it is certain, that key the inserted key value is the only
 *          delete mark in the node, TRUE is set to *p_isonlydelemark,
 *          Otherwise FALSE is set to *p_isonlydelemark. If the information
 *          is not needed, p_isonlydelemark can be NULL.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *      DBE_RC_SUCC, or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btree_insert(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool* p_isonlydelemark,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_ret_t rc;
        su_list_t* path;
        su_profile_timer;

        SS_PUSHNAME("dbe_btree_insert");
        ss_bprintf_1(("dbe_btree_insert\n"));
        BTREE_CHK(b);
        ss_output_2(dbe_bkey_dprint(2, k));
        ss_debug(SsDbgCheckAssertStop());

        if (p_isonlydelemark != NULL) {
            *p_isonlydelemark = FALSE;
        }

        rc = btree_insordel_simple(
                b, k, TRUE, TRUE, FALSE, p_isonlydelemark, cd, info);

        switch (rc) {

            case DBE_RC_SUCC:
                break;

            case DBE_RC_NODESPLIT:
                ss_bprintf_2(("dbe_btree_insert:DBE_RC_NODESPLIT\n"));
                dbe_btree_lockinfo_unlock(info, b);
                su_profile_start;
                SS_PMON_ADD(SS_PMON_BNODE_NODESPLIT);
                path = dbe_btree_nodepath_init(b, k, TRUE, info, DBE_NODEPATH_SPLIT);
                rc = btree_insert_split(b, k, path, NULL, cd, info);
                dbe_btree_nodepath_done(path);
                if (!dbe_btree_lockinfo_lock(b, info, NULL, !(info->i_flags & DBE_INFO_TREEPRELOCKED))) {
                    if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
                        dbe_btree_unlock(b);
                    }
                }
                su_profile_stop("dbe_btree_insert:DBE_RC_NODESPLIT");
                break;

            case DBE_RC_NODERELOCATE:
                ss_bprintf_2(("dbe_btree_insert:DBE_RC_NODERELOCATE\n"));
                dbe_btree_lockinfo_unlock(info, b);
                su_profile_start;
                SS_PMON_ADD(SS_PMON_BNODE_NODERELOCATE);
                path = dbe_btree_nodepath_init(b, k, TRUE, info, DBE_NODEPATH_RELOCATE);
                rc = dbe_btree_nodepath_relocate(path, b, info);
                dbe_btree_nodepath_done(path);
                if (!dbe_btree_lockinfo_lock(b, info, NULL, !(info->i_flags & DBE_INFO_TREEPRELOCKED))) {
                    if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
                        dbe_btree_unlock(b);
                    }
                }
                su_profile_stop("dbe_btree_insert:DBE_RC_NODERELOCATE");
                if (rc == DBE_RC_SUCC) {
                    ss_bprintf_1(("dbe_btree_insert:relocate done, recurse back to dbe_btree_insert\n"));
                    rc = dbe_btree_insert(b, k, p_isonlydelemark, cd, info);
                }
                break;

            case DBE_ERR_UNIQUE_S:
                ss_bprintf_2(("dbe_btree_insert:DBE_ERR_UNIQUE\n"));
                break;

            default:
                ss_bprintf_2(("dbe_btree_insert:%s (%d)\n", su_rc_nameof(rc), rc));
                break;
        }
        SS_POPNAME;
        return(rc);
}

#ifdef SS_BLOCKINSERT
                       <
static dbe_ret_t btree_insblock_split(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_daddr_t* p_addr)
{
        dbe_bnode_t* n;
        dbe_ret_t rc;
        su_list_t* path;
        su_list_node_t* lnode;
        dbe_bnode_splitinfo_t nsi;
        dbe_bkey_t* tmpk;

        ss_dprintf_3(("btree_insblock_split\n"));
        BTREE_CHK(b);
        ss_dassert(b->b_exclusivegate);

        rc = DBE_RC_SUCC;

        path = dbe_btree_nodepath_init(b, k, TRUE, NULL, DBE_NODEPATH_GENERIC);

        lnode = su_list_last(path);
        ss_dassert(lnode != NULL);

        /* Skip data level leaf. */
        lnode = su_list_prev(path, lnode);

        tmpk = dbe_bkey_initsplit(NULL, b->b_go->go_bkeyinfo, k);
        dbe_bkey_setaddr(tmpk, *p_addr);

        k = tmpk;

        while (lnode != NULL) {
            n = su_listnode_getdata(lnode);

            rc = dbe_bnode_insertkey(n, k, TRUE, NULL, &nsi, NULL);

            if (rc == DBE_RC_NODESPLIT) {

                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root split, create new root. */
                    rc = btree_splitroot(b, n, nsi.ns_k, NULL);
                    if (rc != DBE_RC_SUCC) {
                        break;
                    }
                } else {
                    dbe_bkey_copy(k, nsi.ns_k);
                }
                dbe_bkey_done(nsi.ns_k);

            } else {

                /* No more split. */
                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root content changed. */
                    dbe_btree_replacerootnode(b, n);
                }
                break;
            }
            lnode = su_list_prev(path, lnode);
        }

        dbe_bkey_done(tmpk);

        dbe_btree_nodepath_done(path);

        return(rc);
}

static dbe_ret_t btree_insblock_simple(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_daddr_t* p_addr,
        bool* p_gate_exclusive,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_ret_t rc;

        SS_PUSHNAME("btree_insblock_simple");
        ss_dprintf_3(("btree_insblock_simple\n"));
        BTREE_CHK(b);

        if (*p_addr == 0) {
            if (!*p_gate_exclusive) {
                dbe_btree_unlock(b);
                dbe_btree_lock_exclusive(b);
                *p_gate_exclusive = TRUE;
            }
            n = dbe_bnode_create(b->b_go, b->b_bonsaip, &rc, info);
            su_rc_assert(rc == SU_SUCCESS, rc);
            *p_addr = dbe_bnode_getaddr(n);
            dbe_bnode_write(n, FALSE);
            rc = btree_insblock_split(b, k, p_addr);
            if (rc != DBE_RC_SUCC) {
                SS_POPNAME;
                return(rc);
            }
        }

        n = dbe_bnode_getreadwrite(b->b_go, *p_addr, b->b_bonsaip, 0, info);
        ss_dassert(dbe_bnode_getlevel(n) == 0);

        rc = dbe_bnode_insertkey_block(n, k, NULL);

        dbe_bnode_write(n, FALSE);
        SS_POPNAME;
        return(rc);
}

dbe_ret_t dbe_btree_insert_block(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_daddr_t* p_addr)
{
        dbe_ret_t rc;
        su_list_t* path;
        bool gate_exclusive = FALSE;

        SS_PUSHNAME("dbe_btree_insert_block");

        ss_dprintf_1(("dbe_btree_insert_block\n"));
        BTREE_CHK(b);
        ss_debug(SsDbgCheckAssertStop());

        dbe_btree_lock_shared(b);

        rc = btree_insblock_simple(b, k, p_addr, &gate_exclusive);

        switch (rc) {

            case DBE_RC_SUCC:
                break;

            case DBE_RC_NODESPLIT:
                ss_dprintf_2(("dbe_btree_insert:DBE_RC_NODESPLIT\n"));
                if (!gate_exclusive) {
                    dbe_btree_unlock(b);
                    dbe_btree_lock_exclusive(b);
                    gate_exclusive = TRUE;
                }
                /* Start from a new node. */
                *p_addr = 0;
                rc = btree_insblock_simple(b, k, p_addr, &gate_exclusive);
                dbe_cache_addpreflushpage(dbe_btree_getcache(b));
                break;

            case DBE_RC_NODERELOCATE:
                ss_dprintf_2(("dbe_btree_insert:DBE_RC_NODERELOCATE\n"));
                if (!gate_exclusive) {
                    dbe_btree_unlock(b);
                    dbe_btree_lock_exclusive(b);
                    gate_exclusive = TRUE;
                }
                path = dbe_btree_nodepath_init(b, k, TRUE, NULL, DBE_NODEPATH_GENERIC);
                *p_addr = 0;
                rc = dbe_btree_nodepath_relocate_getnewaddr(path, b, p_addr);
                dbe_btree_nodepath_done(path);
                if (rc == DBE_RC_SUCC) {
                    rc = btree_insblock_simple(b, k, p_addr, &gate_exclusive);
                }
                dbe_cache_addpreflushpage(dbe_btree_getcache(b));
                break;

            case DBE_ERR_UNIQUE_S:
                ss_dprintf_2(("dbe_btree_insert:DBE_ERR_UNIQUE\n"));
                break;

            default:
                ss_dprintf_2(("dbe_btree_insert:%s (%d)\n", su_rc_nameof(rc), rc));
                break;
        }
        dbe_btree_unlock(b);
        ss_dprintf_2(("dbe_btree_insert_block:rc = %d (%s)\n", rc, su_rc_nameof(rc)));
        SS_POPNAME;
        return(rc);
}

#endif /* SS_BLOCKINSERT */

/*##*********************************************************************\
 *
 *		dbe_btree_delete_empty
 *
 * Deletes a key value from the leaf node when the leaf node may become
 * empty. If the node becomes empty, also the reference to it is removed
 * from the parent node. This may propagate to the higher levels of the
 * tree.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value that is inserted
 *
 *      path - in, use
 *          node path to leaf the node that contains key value k
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          a key value with the same v-tuple entry is
 *          deleted
 *
 *      p_shrink_tree - out
 *          if non-NULL, TRUE is set into *p_shrink_tree when the
 *          tree height can be reduced by calling function
 *          dbe_bnode_shrink_tree. Note that the node path should
 *          be removed before dbe_bnode_shrink_tree can be called,
 *          otherwise a deadlock will occur.
 *
 *      deleteblob - in
 *          If TRUE, also possible blob data is deleted
 *
 *      pi - out, give
 *          If return value is DBE_RC_FIRSTLEAFKEY then pathinfo is filled
 *          info *pi.
 *
 *      cd - in, use
 *          client data context for blob unlink
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_btree_delete_empty(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        su_list_t* path,
        bool cmp_also_header,
        bool* p_shrink_tree,
        bool deleteblob,
        dbe_pathinfo_t* pi,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_ret_t rc;
        su_list_node_t* lnode;

        ss_bprintf_1(("dbe_btree_delete_empty\n"));
        BTREE_CHK(b);
        ss_dassert(b->b_exclusivegate);
        ss_output_2(dbe_bkey_dprint(2, k));

        rc = DBE_RC_SUCC;

        lnode = su_list_last(path);

        while (lnode != NULL) {
            n = su_listnode_getdata(lnode);

            rc = dbe_bnode_deletekey(
                    n,
                    k,
                    cmp_also_header,
                    TRUE,
                    deleteblob,
                    FALSE,
                    cd,
                    info);
            if (rc != DBE_RC_NODEEMPTY) {
                if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                    /* Root content changed. */
                    dbe_btree_replacerootnode(b, n);
                }
                break;
            }
            ss_bprintf_2(("dbe_btree_delete_empty:node at addr %ld has become empty\n", dbe_bnode_getaddr(n)));
            lnode = su_list_prev(path, lnode);
        }
        ss_assert(rc != DBE_RC_NODEEMPTY);

        if (rc == DBE_RC_FIRSTLEAFKEY) {
            /* First leaf key should be deleted. Replace reference to first key in upper
             * levels to second key.
             */
            pi->pi_firstkey = NULL;
            pi->pi_secondkey = NULL;
            ss_dprintf_2(("dbe_btree_delete_empty:DBE_RC_FIRSTLEAFKEY\n"));
            dbe_bnode_getpathinfo(n, &pi->pi_firstkey, &pi->pi_secondkey);
            if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                /* Root content changed. */
                dbe_btree_replacerootnode(b, n);
            }
        }

        /* Check if the root node contains only one key value.
           In that case we can shrink the tree.
        */
        if (p_shrink_tree != NULL) {
            ss_bprintf_2(("should shring tree\n"));
            lnode = su_list_first(path);
            n = su_listnode_getdata(lnode);
            *p_shrink_tree = b->b_maxlevel > 0 &&
                             dbe_bnode_getkeycount(n) == 1;
        }

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_btree_updatepathinfo
 *
 * Updates B-tree path info to be correct after first key ina leaf node
 * is deleted. We need to update upper levels to contains pointers to
 * the second key value.
 *
 * Parameters :
 *
 *		b - in, use
 *			B-tree
 *
 *		pi - in, take
 *			Path info
 *
 *		info - in
 *
 *
 *		cd - in
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
dbe_ret_t dbe_btree_updatepathinfo(
        dbe_btree_t* b,
        dbe_pathinfo_t* pi,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_ret_t rc;
        su_list_t* path;
        su_list_node_t* lnode;
        bool contp;
        ss_debug(bool secondkeyfound;)
        su_daddr_t addr;

        ss_dprintf_1(("dbe_btree_updatepathinfo\n"));
        ss_dprintf_2(("dbe_btree_updatepathinfo:firstkey\n"));
        ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_updatepathinfo:", pi->pi_firstkey));
        ss_dprintf_2(("dbe_btree_updatepathinfo:secondkey\n"));
        ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_updatepathinfo:", pi->pi_secondkey));
        BTREE_CHK(b);

        rc = DBE_RC_SUCC;

        do {
            contp = FALSE;
            ss_debug(secondkeyfound = FALSE);

            ss_dprintf_2(("dbe_btree_updatepathinfo:dbe_btree_nodepath_init\n"));
            path = dbe_btree_nodepath_init(b, pi->pi_secondkey, TRUE, info, DBE_NODEPATH_GENERIC_NOLOCK);

            lnode = su_list_last(path);

            while (lnode != NULL) {
                n = su_listnode_getdata(lnode);

                if (dbe_bnode_getaddrinkey(n, pi->pi_firstkey, &addr)) {
                    ss_dprintf_2(("dbe_btree_updatepathinfo:found firstkey in node, addr = %ld\n", addr));

                    rc = dbe_bnode_deletekey(n, pi->pi_firstkey, TRUE, TRUE, FALSE, TRUE, cd, info);
                    ss_rc_assert(rc == DBE_RC_SUCC || rc == DBE_RC_NODEEMPTY, rc);

                    dbe_bkey_setaddr(pi->pi_secondkey, addr);

                    if (dbe_bnode_getlevel(n) == b->b_maxlevel) {
                        /* Root content changed. */
                        dbe_btree_replacerootnode(b, n);
                    }

                    if (!dbe_bnode_getaddrinkey(n, pi->pi_secondkey, &addr)) {
                        ss_dprintf_2(("dbe_btree_updatepathinfo:NOT found secondkey in node, addr = %ld\n", addr));
                        rc = btree_insert_split(b, pi->pi_secondkey, path, lnode, cd, info);
                        ss_rc_assert(rc == DBE_RC_SUCC, rc);
                    }
                    ss_rc_assert(rc == DBE_RC_SUCC, rc);

                    contp = TRUE;
                    break;
                }

                lnode = su_list_prev(path, lnode);
            }

            ss_dprintf_2(("dbe_btree_updatepathinfo:dbe_btree_nodepath_done\n"));
            dbe_btree_nodepath_done(path);

        } while (contp);

        dbe_dynbkey_free(&pi->pi_firstkey);
        dbe_dynbkey_free(&pi->pi_secondkey);

        return(rc);
}

/*##*********************************************************************\
 *
 *		dbe_btree_shrink_tree
 *
 * The tree height is tried to reduce. This is done of the tree root
 * has only one child and the tree height is greater than one.
 *
 * Note that the possible node path created by function dbe_btree_nodepath_init
 * should be removed before dbe_bnode_shrink_tree can be called,
 * otherwise a deadlock will occur. Also the B-tree semaphore must be
 * set before calling this function.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_btree_shrink_tree(dbe_btree_t* b, dbe_info_t* info)
{
        dbe_bnode_t* n;
        dbe_bkey_t* k;

        ss_pprintf_1(("dbe_btree_shrink_tree\n"));
        BTREE_CHK(b);
        ss_dassert(b->b_exclusivegate);

        /* Check if the root node contains only one key value.
           In that case we can shrink the tree.
        */
        ss_pprintf_2(("dbe_btree_shrink_tree:rw:addr=%ld, root\n", b->b_rootaddr));
        if (b->b_rootnode != NULL) {
            dbe_bnode_done_tmp(b->b_rootnode);
            b->b_rootnode = NULL;
        }
        if (b->b_maxlevel > 0 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
            n = dbe_bnode_getreadwrite_nocopy(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
        } else {
            n = dbe_bnode_getreadwrite(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
        }
        for (;;) {
            if (b->b_maxlevel > 0 && dbe_bnode_getkeycount(n) == 1) {
                /* Shrink the tree. Remove node from the cache, set the
                   new tree root address and decrease height.
                */
                ss_bprintf_2(("dbe_btree_shrink_tree:decrease tree height, old root %ld\n", (long)b->b_rootaddr));
                k = dbe_bnode_getfirstkey(n);
                b->b_rootaddr = dbe_bkey_getaddr(k);
                ss_pprintf_3(("dbe_btree_shrink_tree:b->b_rootaddr=%ld\n", (long)b->b_rootaddr));
                b->b_maxlevel--;
                dbe_bnode_remove(n);
                if (b->b_bonsaip) {
                    SS_PMON_SET(SS_PMON_BONSAIHEIGHT, b->b_maxlevel + 1);
                }
                ss_bprintf_2(("dbe_btree_shrink_tree:rw:addr=%ld, root\n", b->b_rootaddr));
                if (b->b_maxlevel > 0 && (dbe_cfg_usenewbtreelocking || dbe_cfg_relaxedbtreelocking)) {
                    n = dbe_bnode_getreadwrite_nocopy(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
                } else {
                    n = dbe_bnode_getreadwrite(b->b_go, b->b_rootaddr, b->b_bonsaip, b->b_maxlevel, info);
                }
                ss_bprintf_2(("new root %ld, height %d\n",
                    (long)b->b_rootaddr, b->b_maxlevel));
            } else {
                break;
            }
        }
        dbe_btree_replacerootnode(b, n);
        dbe_bnode_write(n, FALSE);
}

/*##*********************************************************************\
 *
 *		dbe_btree_delete_aux
 *
 * Auxiliary function to delete the key value from the index tree.
 *
 * Parameters :
 *
 *	b - in out, use
 *		index tree
 *
 *	k - in, use
 *		key value that is deleted
 *
 *      cmp_also_header - in
 *		if TRUE, also header parts must match, otherwise
 *          a key value with the same v-tuple entry is
 *          deleted
 *
 *      deleteblob - in
 *          If TRUE, also possible blob data is deleted
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
dbe_ret_t dbe_btree_delete_aux(
        dbe_btree_t* b,
        dbe_bkey_t* k,
        bool cmp_also_header,
        bool deleteblob,
        rs_sysi_t* cd,
        dbe_info_t* info)
{
        dbe_ret_t rc;
        su_list_t* path;
        bool shrink = FALSE;
        dbe_pathinfo_t pathinfo;
        su_profile_timer;

        ss_bprintf_3(("dbe_btree_delete_aux\n"));
        BTREE_CHK(b);

        rc = btree_insordel_simple(
                b, k, FALSE, cmp_also_header, deleteblob, NULL, cd, info);

        switch (rc) {

            case DBE_RC_SUCC:
                break;

            case DBE_RC_NODEEMPTY:
                ss_bprintf_3(("dbe_btree_delete_aux:DBE_RC_NODEEMPTY\n"));
                dbe_btree_lockinfo_unlock(info, b);
                su_profile_start;
                SS_PMON_ADD(SS_PMON_BNODE_NODEEMPTY);
                path = dbe_btree_nodepath_init(b, k, cmp_also_header, info, DBE_NODEPATH_GENERIC);
                rc = dbe_btree_delete_empty(
                        b,
                        k,
                        path,
                        cmp_also_header,
                        &shrink,
                        deleteblob,
                        &pathinfo,
                        cd,
                        info);
                dbe_btree_nodepath_done(path);
                if (shrink) {
                    dbe_btree_shrink_tree(b, info);
                }
                if (rc == DBE_RC_FIRSTLEAFKEY) {
                    dbe_btree_updatepathinfo(b, &pathinfo, cd, info);
                    rc = DBE_RC_SUCC;
                }
                if (!dbe_btree_lockinfo_lock(b, info, NULL, !(info->i_flags & DBE_INFO_TREEPRELOCKED))) {
                    if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
                        dbe_btree_unlock(b);
                    }
                }
                su_profile_stop("dbe_btree_delete_aux:DBE_RC_NODEEMPTY");
                break;

            case DBE_RC_NODERELOCATE:
                ss_bprintf_3(("dbe_btree_delete_aux:DBE_RC_NODERELOCATE\n"));
                dbe_btree_lockinfo_unlock(info, b);
                su_profile_start;
                SS_PMON_ADD(SS_PMON_BNODE_NODERELOCATE);
                path = dbe_btree_nodepath_init(b, k, cmp_also_header, info, DBE_NODEPATH_RELOCATE);
                rc = dbe_btree_nodepath_relocate(path, b, info);
                dbe_btree_nodepath_done(path);
                if (!dbe_btree_lockinfo_lock(b, info, NULL, !(info->i_flags & DBE_INFO_TREEPRELOCKED))) {
                    if (!(info->i_flags & DBE_INFO_TREEPRELOCKED)) {
                        dbe_btree_unlock(b);
                    }
                }
                su_profile_stop("dbe_btree_delete_aux:DBE_RC_NODERELOCATE");
                if (rc == DBE_RC_SUCC) {
                    ss_bprintf_3(("dbe_btree_delete_aux:relocate done, recurse back to dbe_btree_delete_aux\n"));
                    rc = dbe_btree_delete_aux(b, k, cmp_also_header, deleteblob, cd, info);
                }
                break;

            default:
                ss_bprintf_2(("dbe_btree_delete_aux:%s (%d)\n", su_rc_nameof(rc), rc));
                break;
        }

        return(rc);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_btree_exists
 *
 * Checks is a given key value is in the index tree.
 *
 * Parameters :
 *
 *	b - in, use
 *		index tree
 *
 *	k - in, use
 *		key value that is searched
 *
 * Return value :
 *
 *      TRUE    - the key is found from the index tree
 *      FALSE   - the key is not in the index tree
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btree_exists(dbe_btree_t* b, dbe_bkey_t* k)
{
        su_daddr_t addr;
        dbe_bnode_t* n;
        dbe_bnode_t* tmpn;
        bool foundp;
        dbe_info_t info;
        bool unlock_tree;
        dbe_bnode_t* rootnode;
        su_profile_timer;

        ss_bprintf_1(("dbe_btree_exists\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        dbe_btree_lock_shared(b);
        unlock_tree = TRUE;

        su_profile_start;

        n = dbe_btree_getrootnode_nomutex(b);
        rootnode = n;
        if (n == NULL) {
            n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
        }

        for (;;) {

            if (dbe_bnode_getlevel(n) == 0) {
                foundp = dbe_bnode_keyexists(n, k);
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
                su_profile_stop("dbe_btree_exists");
                return(foundp);
            }

            addr = dbe_bnode_searchnode(n, k, TRUE);
            tmpn = dbe_bnode_getreadonly(b->b_go, addr, b->b_bonsaip, &info);
            if (n != rootnode) {
                dbe_bnode_write(n, FALSE);
            }
            if (unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(b);
                unlock_tree = FALSE;
            }
            n = tmpn;
        }
        ss_error;
        return(FALSE);
}

dbe_ret_t dbe_btree_readpathforwrite(dbe_btree_t* b, dbe_bkey_t* k)
{
        su_daddr_t addr;
        dbe_bnode_t* n;
        dbe_bnode_t* tmpn;
        bool foundp;
        dbe_info_t info;
        bool unlock_tree;
        dbe_bnode_t* rootnode;
        su_profile_timer;

        ss_bprintf_1(("dbe_btree_readpathforwrite\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        dbe_btree_lock_shared(b);
        unlock_tree = TRUE;

        su_profile_start;

        n = dbe_btree_getrootnode_nomutex(b);
        rootnode = n;
        if (n == NULL) {
            n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
        }

        for (;;) {

            if (n->n_cpnum != dbe_counter_getcpnum(n->n_go->go_ctr)) {
                /* The node must be relocateed. */
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
                su_profile_stop("dbe_btree_readpathforwrite");
                ss_dprintf_4(("dbe_btree_readpathforwrite:DBE_RC_NODERELOCATE\n"));
                return(DBE_RC_NODERELOCATE);
            }

            if (dbe_bnode_getlevel(n) == 0) {
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
                su_profile_stop("dbe_btree_readpathforwrite");
                return(DBE_RC_SUCC);
            }

            addr = dbe_bnode_searchnode(n, k, TRUE);
            tmpn = dbe_bnode_getreadonly(b->b_go, addr, b->b_bonsaip, &info);
            if (n != rootnode) {
                dbe_bnode_write(n, FALSE);
            }
            if (unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(b);
                unlock_tree = FALSE;
            }
            n = tmpn;
        }
        ss_error;
        return(DBE_ERR_FAILED);
}

#ifdef SS_DEBUG

static void btree_getunique_errorprint(
        dbe_btree_t* b,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_btrsea_timecons_t* tc,
        dbe_bnode_t* n,
        int i,
        dbe_bkey_t* found_key)
{
        char debugstring[80];

        SsSprintf(debugstring, "/LEV:4/FIL:dbe/LOG/UNL/NOD/FLU/TID:%u", SsThrGetid());
        SsDbgSet(debugstring);

        SsDbgPrintf("i=%d, level=%d, b->b_bonsaip=%d\n", i, dbe_bnode_getlevel(n), b->b_bonsaip);

        dbe_bkey_print_ex(NULL, "found_key:", found_key);
        dbe_bkey_print_ex(NULL, "kb:", kb);
        dbe_bkey_print_ex(NULL, "ke:", ke);

        SsDbgPrintf("Current search node:\n");
        dbe_bnode_printtree(NULL, n, TRUE);

        SsDbgPrintf("Timecons: mintrxnum=%ld, maxtrxnum=%ld, usertrxid=%ld, maxtrxid=%ld\n", 
            DBE_TRXNUM_GETLONG(tc->tc_mintrxnum), DBE_TRXNUM_GETLONG(tc->tc_maxtrxnum), 
            DBE_TRXID_GETLONG(tc->tc_usertrxid), DBE_TRXID_GETLONG(tc->tc_maxtrxid));

        dbe_trxbuf_print(n->n_go->go_trxbuf);

        SsDbgFlush();

        ss_error;
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 * 
 *		dbe_btree_getunique
 * 
 * Gets unique key value.
 * 
 * Parameters : 
 * 
 *		b - 
 *			
 *			
 *		kb - 
 *			
 *			
 *		ke - 
 *			
 *			
 *		found_key - 
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
dbe_ret_t dbe_btree_getunique(
        dbe_btree_t* b,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_btrsea_timecons_t* tc,
        dbe_bkey_t* found_key,
        bool* p_deletenext,
        dbe_info_t* info)
{
        su_daddr_t addr;
        dbe_bnode_t* rootnode;
        dbe_bnode_t* n;
        dbe_bnode_t* tmpn;
        dbe_ret_t rc;
        bool unlock_tree;
        int i;
        char* keys;
        dbe_trxstate_t trxresult;
        su_profile_timer;

        ss_dprintf_1(("dbe_btree_getunique\n"));
        BTREE_CHK(b);

        dbe_btree_lock_shared(b);
        unlock_tree = TRUE;

        su_profile_start;

        n = dbe_btree_getrootnode_nomutex(b);
        rootnode = n;
        if (n == NULL) {
            n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, info);
        }

        for (;;) {
            int level;
            bool locked;

            level = dbe_bnode_getlevel(n);

            ss_dprintf_2(("dbe_btree_getunique:level=%d, b->b_bonsaip=%d\n", level, b->b_bonsaip));
            ss_dprintf_2(("dbe_btree_getunique:kb\n"));
            ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_getunique:", kb));
            ss_dprintf_2(("dbe_btree_getunique:ke\n"));
            ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_getunique:", ke));

            if (level == 0) {
                rc = dbe_bnode_getunique(n, kb, ke, &i, &keys, found_key);
                ss_dprintf_2(("dbe_btree_getunique:dbe_bnode_getunique, rc=%s, i=%d\n", su_rc_nameof(rc), i));
                if (rc == DBE_RC_FOUND) {
                    ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_getunique:", found_key));
                }
                if (b->b_bonsaip) {
                    bool deletenext;
                    ss_debug(bool alone_deletemark = FALSE;)

                    deletenext = FALSE;
                    ss_dprintf_2(("dbe_btree_getunique:bonsai tree\n"));
                    while (rc == DBE_RC_FOUND) {
                        bool check;
                        /* Check time constraints. */
                        check = dbe_btrsea_checktimecons(
                                    NULL,
                                    tc,
                                    found_key,
                                    TRUE,
                                    FALSE,
                                    &trxresult);
                        if (!check) {
                            ss_dprintf_2(("dbe_btree_getunique:!checktimecons\n"));
                            rc = DBE_RC_NOTFOUND;
                        } else {
                            if (deletenext) {
                                ss_dprintf_2(("dbe_btree_getunique:deletenext\n"));
                                if (dbe_bkey_isdeletemark(found_key)) {
                                    ss_debug(if (alone_deletemark) btree_getunique_errorprint(b, kb, ke, tc, n, i, found_key));
                                    ss_debug(alone_deletemark = TRUE);
                                    deletenext = TRUE;
                                } else {
                                    deletenext = FALSE;
                                }
                                rc = DBE_RC_NOTFOUND;
                            } else if (dbe_bkey_isdeletemark(found_key)) {
                                ss_dprintf_2(("dbe_btree_getunique:deletemark\n"));
                                deletenext = TRUE;
                                rc = DBE_RC_NOTFOUND;
                            } else {
                                break;
                            }
                        }
                        if (rc == DBE_RC_NOTFOUND) {
                            rc = dbe_bnode_getunique_next(n, ke, &i, &keys, found_key);
                            ss_dprintf_2(("dbe_btree_getunique:dbe_bnode_getunique_next, rc=%s, i=%d\n", su_rc_nameof(rc), i));
                            if (rc == DBE_RC_FOUND) {
                                ss_output_2(dbe_bkey_print_ex(NULL, "dbe_btree_getunique:", found_key));
                            } else if (rc == DBE_RC_END && deletenext) {
                                /* We have found one deletemark. */
                                rc = DBE_RC_FOUND;
                                break;
                            }
                        }
                    }
                    *p_deletenext = deletenext;
                    locked = FALSE;
                } else {
                    locked = dbe_btree_lockinfo_lock(
                                b, 
                                info, 
                                n != rootnode ? n : NULL, 
                                unlock_tree);
                }
                if (!locked) {
                    if (n != rootnode) {
                        dbe_bnode_write(n, FALSE);
                    }
                    if (unlock_tree) {
                        dbe_btree_unlock(b);
                    }
                }
                su_profile_stop("dbe_btree_getunique");
                ss_dprintf_2(("dbe_btree_getunique:rc=%s\n", su_rc_nameof(rc)));
                return(rc);
            }

            addr = dbe_bnode_searchnode(n, kb, FALSE);

            if (dbe_cfg_usenewbtreelocking && !b->b_bonsaip && level == 1) {
                tmpn = dbe_bnode_getreadwrite_search(b->b_go, addr, b->b_bonsaip, level-1, info);
            } else {
                tmpn = dbe_bnode_getreadonly(b->b_go, addr, b->b_bonsaip, info);
            }
            if (n != rootnode) {
                dbe_bnode_write(n, FALSE);
            }
            if (unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(b);
                unlock_tree = FALSE;
            }
            rootnode = NULL;
            n = tmpn;
        }
        ss_error;
        return(DBE_ERR_FAILED);
}

/*##**********************************************************************\
 *
 *		dbe_btree_print
 *
 * Prints the index tree to stdout.
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *	b - in, use
 *		index tree
 *
 *	values - in
 *		if TRUE, prints also all key values in the index tree
 *
 * Return value :
 *
 *      TRUE always
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btree_print(void* fp, dbe_btree_t* b, bool values, bool fail_on_error)
{
        dbe_bnode_t* n;
        bool succp;
        bool old_dbe_debug = dbe_debug;
        dbe_info_t info;

        ss_bprintf_1(("dbe_btree_print\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        dbe_btree_lock_shared(b);

        if (!fail_on_error) {
            dbe_debug = TRUE;
        }
        if (values) {
            dbe_reportindex = TRUE;
            dbe_curkeyid = 0;
        }

        n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
        if (n == NULL) {
            SsDbgMessage("Illegal index root block address %ld\n", (long)b->b_rootaddr);
            dbe_debug = old_dbe_debug;
            dbe_reportindex = FALSE;
            dbe_btree_unlock(b);
            return(FALSE);
        }

        dbe_curkeyid = 0;

        dbe_bnode_totalnodelength = 0;
        dbe_bnode_totalnodekeycount = 0;
        dbe_bnode_totalnodecount = 0;
        dbe_bnode_totalshortnodecount = 0;

        succp = dbe_bnode_printtree(fp, n, values);

        dbe_bnode_write(n, FALSE);

        SsDbgMessage("Node count: %ld\n", dbe_bnode_totalnodecount);
        SsDbgMessage("Key count: %ld\n", dbe_bnode_totalnodekeycount);
        SsDbgMessage("Size: %.2lf MB\n", (double)dbe_bnode_totalnodelength / (double)(1024 * 1024));
        SsDbgMessage("Filled: %.2lf\n", 
            dbe_bnode_totalnodecount == 0
                ? 0.0
                : ((double)dbe_bnode_totalnodelength / ((double)dbe_bnode_totalnodecount * (double)b->b_go->go_idxfd->fd_blocksize)) * 100.0);
        SsDbgMessage("Nodes filled less than 25%: %ld\n", dbe_bnode_totalshortnodecount);

        dbe_reportindex = FALSE;
        dbe_debug = old_dbe_debug;
        if (dbe_curkey != NULL) {
            dbe_bkey_done(dbe_curkey);
            dbe_curkey = NULL;
        }
        dbe_btree_unlock(b);

        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_btree_check
 *
 * Checks the index tree.
 *
 * Parameters :
 *
 *	b - in, use
 *		index tree
 *
 *	check_values - in
 *		If TRUE, checks also key values.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_btree_check(dbe_btree_t* b, bool check_values)
{
        dbe_bnode_t* n;
        bool succp;
        bool old_dbe_debug = dbe_debug;
        dbe_info_t info;

        ss_bprintf_1(("dbe_btree_check\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        dbe_btree_lock_shared(b);
        dbe_debug = TRUE;

        n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
        if (n == NULL) {
            SsDbgMessage("Illegal index root block address %ld\n", (long)b->b_rootaddr);
            dbe_debug = old_dbe_debug;
            dbe_btree_unlock(b);
            return(FALSE);
        }

        succp = dbe_bnode_checktree(n, check_values);

        dbe_bnode_write(n, FALSE);

        dbe_debug = old_dbe_debug;
        dbe_btree_unlock(b);

        return(succp);
}

#endif /* SS_LIGHT */

#ifndef SS_NOESTSAMPLES

static bool btree_getrandomsample(
        dbe_btree_t* b,
        dbe_bkey_t* kb,
        dbe_bkey_t* ke,
        dbe_bkey_t** p_found_key)
{
        su_daddr_t addr;
        dbe_bnode_t* rootnode;
        dbe_bnode_t* n;
        dbe_bnode_t* tmpn;
        bool foundp;
        dbe_info_t info;
        bool unlock_tree;
        su_profile_timer;

        ss_bprintf_1(("btree_getrandomsample\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        dbe_btree_lock_shared(b);
        unlock_tree = TRUE;

        su_profile_start;

        rootnode = dbe_btree_getrootnode_nomutex(b);
        if (rootnode == NULL) {
            n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
        } else {
            n = rootnode;
        }

        for (;;) {

            if (dbe_bnode_getlevel(n) == 0) {
                foundp = dbe_bnode_getrandomsample(n, kb, ke, p_found_key);
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
                su_profile_stop("dbe_btree_getrandomsample");
                return(foundp);
            }

            if (!dbe_bnode_getrandomaddress(n, kb, ke, &addr)) {
                if (n != rootnode) {
                    dbe_bnode_write(n, FALSE);
                }
                if (unlock_tree) {
                    dbe_btree_unlock(b);
                }
                return(FALSE);
            }
            tmpn = dbe_bnode_getreadonly(b->b_go, addr, b->b_bonsaip, &info);
            if (n != rootnode) {
                dbe_bnode_write(n, FALSE);
            }
            if (unlock_tree && dbe_cfg_usenewbtreelocking) {
                dbe_btree_unlock(b);
                unlock_tree = FALSE;
            }
            rootnode = NULL;
            n = tmpn;
        }
        ss_error;
        return(FALSE);
}

static int SS_CLIBCALLBACK keysamples_qsortcmp(const void* s1, const void* s2)
{
        vtpl_t* v1 = *(vtpl_t**)s1;
        vtpl_t* v2 = *(vtpl_t**)s2;
        int cmp;

        cmp = vtpl_compare(v1, v2);
        if (cmp < 0) {
            return(-1);
        } else if (cmp > 0) {
            return(1);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 *
 *		dbe_btree_getkeysamples
 *
 * Gets key value samples from index for attribute distribution estimates.
 * At most sample_size samles are gathered.
 *
 * Parameters :
 *
 *	b - in
 *
 *
 *	range_min - in
 *
 *
 *	range_max - in
 *
 *
 *	sample_vtpl - use
 *
 *
 *	sample_size - in
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
void dbe_btree_getkeysamples(
        dbe_btree_t* b,
        vtpl_t* range_min,
        vtpl_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size,
        bool mergep)
{
        dbe_bkey_t* k;
        dbe_dynbkey_t kmin = NULL;
        dbe_dynbkey_t kmax = NULL;
        dbe_bnode_t* n;
        dbe_info_t info;
        su_profile_timer;

        ss_bprintf_1(("dbe_btree_getkeysamples\n"));
        BTREE_CHK(b);

        dbe_info_init(info, 0);

        k = dbe_bkey_init(b->b_go->go_bkeyinfo);

        dbe_bkey_setvtpl(k, range_min);
        dbe_dynbkey_setbkey(&kmin, k);

        dbe_bkey_setvtpl(k, range_max);
        dbe_dynbkey_setbkey(&kmax, k);

        dbe_bkey_done(k);

        if (dbe_cfg_userandomkeysampleread) {
            int i;
            int found_samplecount;
            int final_samplecount;
            bool foundp;
            vtpl_t* vtpl;

            su_profile_start;
        
            for (i = 0, found_samplecount = 0; i < sample_size; i++) {
                foundp = btree_getrandomsample(b, kmin, kmax, &k);
                if (foundp) {
                    vtpl = dbe_bkey_getvtpl(k);
                    dynvtpl_setvtpl(&sample_vtpl[found_samplecount++], vtpl);
                    dbe_bkey_done(k);
                }
            }
            if (found_samplecount > 0) {
                /* Sort samples. */
                qsort(
                    sample_vtpl,
                    found_samplecount,
                    sizeof(sample_vtpl[0]),
                    keysamples_qsortcmp);
                /* Remove duplicates. */
                for (i = 1, final_samplecount = 1; i < found_samplecount; i++) {
                    int cmp;
                    cmp = vtpl_compare(sample_vtpl[final_samplecount-1], sample_vtpl[i]);
                    ss_dassert(cmp <= 0);
                    if (cmp != 0) {
                        sample_vtpl[final_samplecount++] = sample_vtpl[i];
                    } else {
                        ss_bprintf_2(("dbe_btree_getkeysamples:remove duplicate\n"));
                        SsMemFree(sample_vtpl[i]);
                    }
                }
                for (i = final_samplecount; i < sample_size; i++) {
                    sample_vtpl[i] = NULL;
                }
                ss_bprintf_2(("dbe_btree_getkeysamples:final_samplecount=%d, sample_size=%d\n", final_samplecount, sample_size));
            }
        } else {
            dbe_btree_lock_shared(b);

            su_profile_start;

            ss_bprintf_2(("dbe_btree_getkeysamples:r:addr=%ld, root\n", b->b_rootaddr));
            n = dbe_btree_getrootnode_nomutex(b);
            if (n == NULL) {
                n = dbe_bnode_getreadonly(b->b_go, b->b_rootaddr, b->b_bonsaip, &info);
            }
            if (n == NULL) {
                dbe_btree_unlock(b);
                su_profile_stop("dbe_btree_getkeysamples");
                return;
            }

            dbe_bnode_getkeysamples(n, kmin, kmax, sample_vtpl, sample_size, mergep);

            if (n != b->b_rootnode) {
                dbe_bnode_write(n, FALSE);
            }

            dbe_btree_unlock(b);
        }
        su_profile_stop("dbe_btree_getkeysamples");

        dbe_dynbkey_free(&kmin);
        dbe_dynbkey_free(&kmax);
}

#endif /* SS_NOESTSAMPLES */

