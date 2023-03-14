/*************************************************************************\
**               *
**  source       * dbe5inde.c
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


#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

This module implements the database index system. The index system
is a union of two B+-index trees: bonsai tree and permanent tree.
The index system is seen as a single index tree to the user of this
level.

Only insert and delete functions are provided. Update operation must be
divided into delete and insert operations by the caller of these routines.
Delete operation actually adds a delete mark to the Bonsai-tree. A separate
merge process is used to physically delete key values.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

v-tuples            uti0va.c uti0vtpl.c uti0vcmp.c

attribute type      rs0atype.c
attribute value     rs0aval.c
search plan         rs0pla.c
search constraints  rs0cons.c

cache               dbe8cach.c
file info           dbe6finf.c
key value system    dbe6bkey.c
B+-tree             dbe6btre.c

Preconditions:
-------------


Multithread considerations:
--------------------------

The code is not reentrant.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define DBE5INDE_C

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <sstime.h>

#include <uti0vtpl.h>

#include <su0gate.h>
#include <su0prof.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe6finf.h"
#include "dbe6bkey.h"
#include "dbe6btre.h"
#include "dbe6bmgr.h"
#include "dbe7trxi.h"
#include "dbe7trxb.h"
#include "dbe5inde.h"
#include "dbe5isea.h"
#include "dbe5imrg.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#ifdef DBE_MERGEDEBUG
#include "dbe0user.h"
#include "dbe0trx.h"
#endif /* DBE_MERGEDEBUG */

bool dbe_index_test_version_on = FALSE;

static void index_searchbeginactive_nomutex(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle);

/*##**********************************************************************\
 *
 *		dbe_index_init
 *
 * Initializes (opens or creates) the index system
 *
 * Parameters :
 *
 *	go - in, hold
 *		Global object strucuture.
 *
 *      bonsairoot - in
 *
 *      permroot - in
 *
 * Return value - give :
 *
 *      Pointer to the new index object.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_index_t* dbe_index_init(
        dbe_gobj_t* go,
        su_daddr_t bonsairoot,
        su_daddr_t permroot)
{
        dbe_index_t* index;
        int i;

        ss_assert(bonsairoot != permroot ||
                  (bonsairoot == SU_DADDR_NULL && permroot == SU_DADDR_NULL));

        index = SSMEM_NEW(dbe_index_t);

        ss_debug(index->ind_chk = DBE_CHK_INDEX);
        index->ind_permtree = dbe_btree_init(go, permroot, FALSE);
        index->ind_bonsaitree = dbe_btree_init(go, bonsairoot, TRUE);

        index->ind_mergeactive = 0;
        ss_debug(dbe_cfg_getfakemerge(go->go_cfg, &index->ind_fakemerge));
        ss_debug(if (index->ind_fakemerge) index->ind_mergeactive = 1;)

        index->ind_sealist.isl_next = &index->ind_sealist;
        index->ind_sealist.isl_prev = &index->ind_sealist;
        index->ind_sealru.isl_next = &index->ind_sealru;
        index->ind_sealru.isl_prev = &index->ind_sealru;
        index->ind_trxbuf = go->go_trxbuf;
        index->ind_nsearch = 0;
        for (i = 0; i < DBE_INDEX_NMERGEGATE; i++) {
            index->ind_mergegate[i] = su_gate_init(SS_SEMNUM_DBE_INDEX_MERGEGATE, FALSE);
            su_gate_setmaxexclusive(index->ind_mergegate[i], 0); /* No max limit. */
        }
        SsFlatMutexInit(&index->ind_listsem, SS_SEMNUM_DBE_INDEX_SEARCHLIST);
        index->ind_seaid = 0;

        index->ind_go = go;
        index->ind_bkeyinfo = go->go_bkeyinfo;

        index->ind_bloblimit_low =
            dbe_bkey_getbloblimit_low(
                index->ind_bkeyinfo->ki_maxkeylen);
        index->ind_bloblimit_high =
            dbe_bkey_getbloblimit_high(
                index->ind_bkeyinfo->ki_maxkeylen);

        dbe_cfg_getseqsealimit(go->go_cfg, &index->ind_seqsealimit);
        dbe_cfg_getseabuflimit(go->go_cfg, &index->ind_seabuflimit);
        dbe_cfg_getreadaheadsize(go->go_cfg, &index->ind_readaheadsize);
        index->ind_seabufused = 0;

#ifdef SS_BLOCKINSERT
        dbe_cfg_getblockinsertsize(go->go_cfg, &index->ind_blockarrsize);
        if (index->ind_blockarrsize > 0) {
            index->ind_blockarr = SsMemCalloc(1, sizeof(su_daddr_t) * index->ind_blockarrsize);
        } else {
            index->ind_blockarr = NULL;
        }
#endif /* SS_BLOCKINSERT */

        return(index);
}

/*##**********************************************************************\
 *
 *		dbe_index_done
 *
 * Releases resources allocated for the index system and closes it.
 *
 * Parameters :
 *
 *	index - in, take
 *		Index system.
 *
 *      p_bonsairoot - out
 *
 *
 *      p_permroot - out
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_index_done(
        dbe_index_t* index,
        su_daddr_t* p_bonsairoot,
        su_daddr_t* p_permroot)
{
        int i;

        CHK_INDEX(index);
        ss_dassert(index->ind_mergeactive == 0 || index->ind_fakemerge);

        ss_dassert(index->ind_sealist.isl_next == &index->ind_sealist);
        ss_dassert(index->ind_sealist.isl_prev == &index->ind_sealist);
        ss_dassert(index->ind_sealru.isl_next == &index->ind_sealru);
        ss_dassert(index->ind_sealru.isl_prev == &index->ind_sealru);

        *p_bonsairoot = dbe_btree_done(index->ind_bonsaitree);
        *p_permroot = dbe_btree_done(index->ind_permtree);
        for (i = 0; i < DBE_INDEX_NMERGEGATE; i++) {
            su_gate_done(index->ind_mergegate[i]);
        }
        SsFlatMutexDone(index->ind_listsem);

        ss_dassert(*p_bonsairoot >= DBE_INDEX_HEADERSIZE);
        ss_dassert(*p_permroot >= DBE_INDEX_HEADERSIZE);

#ifdef SS_BLOCKINSERT
        if (index->ind_blockarr != NULL) {
            SsMemFree(index->ind_blockarr);
        }
#endif /* SS_BLOCKINSERT */

        SsMemFree(index);
}

/*##**********************************************************************\
 *
 *		dbe_index_setmergeactive
 *
 * Sets the value of merge active flag according to isactive parameter.
 *
 * Parameters :
 *
 *	index - use
 *
 *
 *	isactive - in
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
void dbe_index_setmergeactive(
        dbe_index_t* index,
        bool isactive)
{
        CHK_INDEX(index);
        SsFlatMutexIsLockedByThread(index->ind_listsem);

        if (dbe_cfg_mergecleanup) {
            if (isactive) {
                index->ind_mergeactive++;
            } else {
                ss_dassert(index->ind_mergeactive > 0);
                index->ind_mergeactive--;
            }
        } else {
            index->ind_mergeactive = (int)isactive;
        }
        ss_debug(if (index->ind_fakemerge) index->ind_mergeactive = 1;)
}

/*##**********************************************************************\
 *
 *          dbe_index_indsealist_mergeactiveiter_nomutex
 *
 * Iterates through the list of index searches and calls iterfun
 * for every index search.
 *
 * Parameters :
 *
 *	index - in
 *
 *
 *	iterfun - in
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
void dbe_index_indsealist_mergeactiveiter_nomutex(
        dbe_index_t* index,
        bool startp,
        void (*mergeactivefun)(dbe_indsea_t* indsea, bool mergeactive))
{
        dbe_index_sealnode_t* node;

        CHK_INDEX(index);
        SsFlatMutexIsLockedByThread(index->ind_listsem);

        node = index->ind_sealist.isl_next;
        while (node != &index->ind_sealist) {
            if (startp) {
                (*mergeactivefun)(node->isl_indsea, TRUE);
            } else {
                /* We check for index->ind_mergeactive > 1 here because at
                 * 1 we are stopping merge, otherwise just one concurrent
                 * merge pass has stopped. We need to the function anyway
                 * to mkake sure it has curent buffers.
                 */
                (*mergeactivefun)(node->isl_indsea, index->ind_mergeactive > 1);
            }
            node = node->isl_next;
        }
}

/*##**********************************************************************\
 *
 *		dbe_index_getrootaddrs
 *
 * Returns tree root addresses.
 *
 * Parameters :
 *
 *	index - in, take
 *		Index system.
 *
 *      p_bonsairoot - out
 *
 *
 *      p_permroot - out
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_index_getrootaddrs(
        dbe_index_t* index,
        su_daddr_t* p_bonsairoot,
        su_daddr_t* p_permroot)
{
        CHK_INDEX(index);
        ss_dassert(p_bonsairoot != NULL);
        ss_dassert(p_permroot != NULL);

        *p_bonsairoot = dbe_btree_getrootaddr(index->ind_bonsaitree);
        *p_permroot = dbe_btree_getrootaddr(index->ind_permtree);
}

/*##**********************************************************************\
 *
 *		dbe_index_setbkeyflags
 *
 *
 *
 * Parameters :
 *
 *	k -
 *
 *
 *	mode -
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
void dbe_index_setbkeyflags(
        dbe_bkey_t* k,
        dbe_indexop_mode_t mode)
{
        if (mode & DBE_INDEXOP_COMMITTED) {
            ss_dprintf_2(("dbe_index_setbkeyflags:DBE_INDEXOP_COMMITTED\n"));
            dbe_bkey_setcommitted(k);
        }
        if (mode & DBE_INDEXOP_BLOB) {
            ss_dprintf_2(("dbe_index_setbkeyflags:DBE_INDEXOP_BLOB\n"));
            dbe_bkey_setblob(k);
        }
        if (mode & DBE_INDEXOP_CLUSTERING) {
            ss_dprintf_2(("dbe_index_setbkeyflags:DBE_INDEXOP_CLUSTERING\n"));
            dbe_bkey_setclustering(k);
        }
        if (mode & DBE_INDEXOP_DELETEMARK) {
            ss_dprintf_2(("dbe_index_setbkeyflags:DBE_INDEXOP_DELETEMARK\n"));
            dbe_bkey_setdeletemark(k);
        }
        if (mode & DBE_INDEXOP_UPDATE) {
            ss_dprintf_2(("dbe_index_setbkeyflags:DBE_INDEXOP_UPDATE\n"));
            dbe_bkey_setupdate(k);
        }
}

/*##**********************************************************************\
 *
 *		dbe_index_locktree
 *
 * Locks both index trees. Used for NOCHECK updates to guarantee consistent
 * view for readers.
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
void dbe_index_locktree(dbe_index_t* index)
{
        ss_dprintf_1(("dbe_index_locktree\n"));

        dbe_btree_lock_exclusive(index->ind_permtree);
        dbe_btree_lock_exclusive(index->ind_bonsaitree);
}

/*##**********************************************************************\
 *
 *		dbe_index_unlocktree
 *
 * Unlocks index trees locked by dbe_index_locktree.
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
void dbe_index_unlocktree(dbe_index_t* index)
{
        ss_dprintf_1(("dbe_index_unlocktree\n"));

        dbe_btree_unlock(index->ind_bonsaitree);
        dbe_btree_unlock(index->ind_permtree);
}

/*#***********************************************************************\
 *
 *		index_insert_nocheck
 *
 * Inserts a new key value to the index. The key is inserted directly
 * to the permanent tree as committed key value.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	mode - in
 *		Mode flags or'ed in.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t index_insert_nocheck(
        dbe_index_t* index,
        vtpl_t* vtpl,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;
        dbe_info_t info;

        CHK_INDEX(index);
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));
        ss_dassert(mode & DBE_INDEXOP_NOCHECK);

        ss_dprintf_1(("index_insert_nocheck\n"));

        ss_dassert(VTPL_GROSSLEN(vtpl) <= index->ind_bloblimit_high);

        k = dbe_bkey_initpermleaf(cd, index->ind_bkeyinfo, vtpl);

        dbe_index_setbkeyflags(k, mode | DBE_INDEXOP_COMMITTED);
        dbe_info_init(info, 0);
        if (mode & DBE_INDEXOP_PRELOCKED) {
            info.i_flags |= DBE_INFO_TREEPRELOCKED;
        }

        ss_output_4(dbe_bkey_dprint(4, k));

        rc = dbe_btree_insert(index->ind_permtree, k, NULL, NULL, &info);

        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }
        ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

        dbe_bkey_done_ex(cd, k);

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_index_insert
 *
 * Inserts a new key value to the index.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	trxnum - in
 *		key transaction number
 *
 *	trxid - in
 *		key transaction id
 *
 *	mode - in
 *		Mode flags or'ed in.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_index_insert(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;

        SS_NOTUSED(keyid);

        CHK_INDEX(index);
        SS_PUSHNAME("dbe_index_insert");
        ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));

        ss_dprintf_1(("dbe_index_insert:mode=%d, trxnum=%ld\n", mode, DBE_TRXNUM_GETLONG(trxnum)));

        if (VTPL_GROSSLEN(vtpl) > index->ind_bloblimit_high) {
            ss_dprintf_4(("dbe_index_insert:VTPL_GROSSLEN(vtpl) = %ld, index->ind_bloblimit_high = %ld\n",
                VTPL_GROSSLEN(vtpl), (long)index->ind_bloblimit_high));
            SS_POPNAME;
            return(DBE_ERR_TOOLONGKEY);
        }

        if ((mode & DBE_INDEXOP_NOCHECK) && !(mode & DBE_INDEXOP_COMMITTED)) {
            rc = index_insert_nocheck(index, vtpl, mode, cd);

        } else {
            dbe_info_t info;

            k = dbe_bkey_initleaf(cd, index->ind_bkeyinfo, trxnum, trxid, vtpl);

            dbe_index_setbkeyflags(k, mode);
            dbe_info_init(info, 0);

            ss_output_4(dbe_bkey_dprint(4, k));

            if (mode & DBE_INDEXOP_PRELOCKED) {
                ss_dassert(mode & DBE_INDEXOP_NOCHECK);
                info.i_flags |= DBE_INFO_TREEPRELOCKED;
            }

            rc = dbe_btree_insert(index->ind_bonsaitree, k, NULL, cd, &info);

            dbe_bkey_done_ex(cd, k);

            if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
                rc = info.i_rc;
            }
            ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);
        }

        ss_dprintf_2(("rc = %d\n", rc));

        SS_POPNAME;
        return(rc);
}

#ifdef SS_MYSQL

dbe_ret_t dbe_index_undobkeylist(
        dbe_index_t* index,
        su_list_t* bkeylist,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        dbe_ret_t rc2;
        su_list_node_t* n;
        dbe_bkey_t* k;
        dbe_info_t info;
        bool isblob = FALSE;

        CHK_INDEX(index);
        ss_dprintf_1(("dbe_index_undobkeylist\n"));

        if (bkeylist == NULL) {
            return(DBE_RC_SUCC);
        }

        dbe_info_init(info, 0);

        if (mode & DBE_INDEXOP_PRELOCKED) {
            info.i_flags |= DBE_INFO_TREEPRELOCKED;
        }

        su_list_do_get(bkeylist, n, k) {
            if (!dbe_bkey_isdeletemark(k) && dbe_bkey_isblob(k)) {
                isblob = TRUE;
                rs_sysi_setdeferredblobunlinklist(cd, su_list_init(NULL));
            }
            rc2 = dbe_btree_delete_keyandblob(index->ind_bonsaitree, k, cd, &info);
            if (rc2 != DBE_RC_SUCC && rc == DBE_RC_SUCC) {
                rc = rc2;
            }
        }
        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }
        if (isblob) {
            rs_sysi_link(cd);
            dbe_indmerge_unlinkblobs(cd, rs_sysi_getdeferredblobunlinklist(cd));
            rs_sysi_setdeferredblobunlinklist(cd, NULL);
        }
        ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

        return(rc);
}

void dbe_index_freebkeylist(
        rs_sysi_t* cd,
        su_list_t* bkeylist)
{
        su_list_node_t* n;
        dbe_bkey_t* k;

        ss_dprintf_1(("dbe_index_freebkeylist\n"));

        if (bkeylist == NULL) {
            return;
        }

        su_list_do_get(bkeylist, n, k) {
            dbe_bkey_done_ex(cd, k);
        }
        su_list_done(bkeylist);
}

dbe_ret_t dbe_index_insert_ex(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd,
        su_list_t** bkeylist)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;

        SS_NOTUSED(keyid);

        CHK_INDEX(index);
        SS_PUSHNAME("dbe_index_insert");
        ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));

        ss_dprintf_1(("dbe_index_insert:mode=%d, trxnum=%ld\n", mode, DBE_TRXNUM_GETLONG(trxnum)));

        if (VTPL_GROSSLEN(vtpl) > index->ind_bloblimit_high) {
            ss_dprintf_4(("dbe_index_insert:VTPL_GROSSLEN(vtpl) = %ld, index->ind_bloblimit_high = %ld\n",
                VTPL_GROSSLEN(vtpl), (long)index->ind_bloblimit_high));
            SS_POPNAME;
            return(DBE_ERR_TOOLONGKEY);
        }

        if ((mode & DBE_INDEXOP_NOCHECK) && !(mode & DBE_INDEXOP_COMMITTED)) {
            rc = index_insert_nocheck(index, vtpl, mode, cd);

        } else {
            dbe_info_t info;

            k = dbe_bkey_initleaf(cd, index->ind_bkeyinfo, trxnum, trxid, vtpl);

            dbe_index_setbkeyflags(k, mode);
            dbe_info_init(info, 0);

            ss_output_4(dbe_bkey_dprint(4, k));

            if (mode & DBE_INDEXOP_PRELOCKED) {
                ss_dassert(mode & DBE_INDEXOP_NOCHECK);
                info.i_flags |= DBE_INFO_TREEPRELOCKED;
            }

            rc = dbe_btree_insert(index->ind_bonsaitree, k, NULL, cd, &info);

            if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
                rc = info.i_rc;
            }
            ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

            if (rc == DBE_RC_SUCC) {
                if (*bkeylist == NULL) {
                    *bkeylist = su_list_init(NULL);
                }
                su_list_insertlast(*bkeylist, k);
            } else {
                dbe_bkey_done_ex(cd, k);
            }
        }

        ss_dprintf_2(("rc = %d\n", rc));

        SS_POPNAME;
        return(rc);
}
#endif /* SS_MYSQL */

/*#***********************************************************************\
 *
 *		index_delete_nocheck
 *
 * Physically deletes a key value from the index.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	mode - in
 *		Mode flags or'ed in.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t index_delete_nocheck(
        dbe_index_t* index,
        vtpl_t* vtpl,
        dbe_indexop_mode_t mode,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;
        dbe_info_t info;

        ss_dprintf_1(("index_delete_nocheck\n"));
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));
        ss_dassert(mode & DBE_INDEXOP_NOCHECK);

        CHK_INDEX(index);

        k = dbe_bkey_initpermleaf(cd, index->ind_bkeyinfo, vtpl);

        dbe_index_setbkeyflags(k, mode | DBE_INDEXOP_COMMITTED);
        dbe_info_init(info, 0);

        if (mode & DBE_INDEXOP_PRELOCKED) {
            info.i_flags |= DBE_INFO_TREEPRELOCKED;
        }

        ss_output_4(dbe_bkey_dprint(4, k));

        rc = dbe_btree_delete_keyandblob(index->ind_permtree, k, cd, &info);

        dbe_bkey_done_ex(cd, k);

        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }
        ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_index_delete
 *
 * Deletes a key value from the index.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	trxnum - in
 *		key transcation number
 *
 *	trxid - in
 *		key transaction id
 *
 *	mode - in
 *		Mode flags or'ed in.
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
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_index_delete(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        bool* p_isonlydelemark,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;

        SS_PUSHNAME("dbe_index_delete");
        ss_dprintf_1(("dbe_index_delete:mode=%ld, trxnum=%ld\n", mode, DBE_TRXNUM_GETLONG(trxnum)));

        SS_NOTUSED(keyid);

        CHK_INDEX(index);
        ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));

        ss_dassert(vtpl_grosslen(vtpl) <= index->ind_bloblimit_high);

        if ((mode & DBE_INDEXOP_NOCHECK) && !(mode & DBE_INDEXOP_COMMITTED)) {
            rc = index_delete_nocheck(index, vtpl, mode, cd);

        } else {
            dbe_info_t info;

            k = dbe_bkey_initleaf(cd, index->ind_bkeyinfo, trxnum, trxid, vtpl);
            dbe_bkey_setdeletemark(k);

            dbe_index_setbkeyflags(k, mode);
            dbe_info_init(info, 0);

            if (mode & DBE_INDEXOP_PRELOCKED) {
                ss_dassert(mode & DBE_INDEXOP_NOCHECK);
                info.i_flags |= DBE_INFO_TREEPRELOCKED;
            }

#ifdef DBE_MERGEDEBUG
            if (cd != NULL) {
                dbe_user_t* user;
                dbe_trx_t* trx = NULL;

                user = rs_sysi_user(cd);
                if (user != NULL) {
                    trx = dbe_user_gettrx(user);
                }

                if (trx != NULL) {
                    dbe_bkey_setmergedebuginfo(
                        k,
                        dbe_trx_getsearchtrxnum(trx),
                        dbe_trx_getwritemode(trx));
                }
            }
#endif /* DBE_MERGEDEBUG */

            ss_output_4(dbe_bkey_dprint(4, k));

            rc = dbe_btree_insert(index->ind_bonsaitree, k, p_isonlydelemark, cd, &info);

            dbe_bkey_done_ex(cd, k);

            if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
                rc = info.i_rc;
            }
            ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);
        }

        ss_dprintf_2(("rc = %d\n", rc));

        SS_POPNAME;
        return(rc);
}

#ifdef SS_MYSQL
dbe_ret_t dbe_index_delete_ex(
        dbe_index_t* index,
        long keyid,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode,
        bool* p_isonlydelemark,
        rs_sysi_t* cd,
        su_list_t** bkeylist)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;

        SS_PUSHNAME("dbe_index_delete");
        ss_dprintf_1(("dbe_index_delete:mode=%ld, trxnum=%ld\n", mode, DBE_TRXNUM_GETLONG(trxnum)));

        SS_NOTUSED(keyid);

        CHK_INDEX(index);
        ss_dassert(!DBE_TRXID_EQUAL(trxid, DBE_TRXID_NULL));
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));

        ss_dassert(vtpl_grosslen(vtpl) <= index->ind_bloblimit_high);

        if ((mode & DBE_INDEXOP_NOCHECK) && !(mode & DBE_INDEXOP_COMMITTED)) {
            rc = index_delete_nocheck(index, vtpl, mode, cd);

        } else {
            dbe_info_t info;

            k = dbe_bkey_initleaf(cd, index->ind_bkeyinfo, trxnum, trxid, vtpl);
            dbe_bkey_setdeletemark(k);

            dbe_index_setbkeyflags(k, mode);
            dbe_info_init(info, 0);

            if (mode & DBE_INDEXOP_PRELOCKED) {
                ss_dassert(mode & DBE_INDEXOP_NOCHECK);
                info.i_flags |= DBE_INFO_TREEPRELOCKED;
            }

#ifdef DBE_MERGEDEBUG
            if (cd != NULL) {
                dbe_user_t* user;
                dbe_trx_t* trx = NULL;

                user = rs_sysi_user(cd);
                if (user != NULL) {
                    trx = dbe_user_gettrx(user);
                }

                if (trx != NULL) {
                    dbe_bkey_setmergedebuginfo(
                        k,
                        dbe_trx_getsearchtrxnum(trx),
                        dbe_trx_getwritemode(trx));
                }
            }
#endif /* DBE_MERGEDEBUG */

            ss_output_4(dbe_bkey_dprint(4, k));

            rc = dbe_btree_insert(index->ind_bonsaitree, k, p_isonlydelemark, cd, &info);

            if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
                rc = info.i_rc;
            }
            ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

            if (rc == DBE_RC_SUCC) {
                if (*bkeylist == NULL) {
                    *bkeylist = su_list_init(NULL);
                }
                su_list_insertlast(*bkeylist, k);
            } else {
                dbe_bkey_done_ex(cd, k);
            }
        }

        ss_dprintf_2(("rc = %d\n", rc));

        SS_POPNAME;
        return(rc);
}
#endif /* SS_MYSQL */

#ifdef NOT_USED

/*##**********************************************************************\
 *
 *		dbe_index_delete_physical
 *
 * Physically deletes a key value from the index.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	trxnum - in
 *		key transcation number
 *
 *	trxid - in
 *		key transaction id
 *
 *	mode - in
 *		Mode flags or'ed in.
 *
 * Return value :
 *
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_index_delete_physical(
        dbe_index_t* index,
        vtpl_t* vtpl,
        dbe_trxnum_t trxnum,
        dbe_trxid_t trxid,
        dbe_indexop_mode_t mode)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;
        dbe_info_t info;

        ss_dprintf_1(("dbe_index_delete_physical\n"));
        ss_derror;

        CHK_INDEX(index);

        k = dbe_bkey_initleaf(NULL, index->ind_bkeyinfo, trxnum, trxid, vtpl);

        dbe_index_setbkeyflags(k, mode);
        dbe_info_init(info, 0);

        if (mode & DBE_INDEXOP_PRELOCKED) {
            ss_dassert(mode & DBE_INDEXOP_NOCHECK);
            info.i_flags |= DBE_INFO_TREEPRELOCKED;
        }

        ss_output_4(dbe_bkey_dprint(4, k));

        rc = dbe_btree_delete_keyandblob(index->ind_bonsaitree, k, &info);

        dbe_bkey_done(k);

        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }
        ss_dassert((mode & DBE_INDEXOP_PRELOCKED) ? (info.i_flags & DBE_INFO_TREEPRELOCKED) : TRUE);

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_index_bkey_insert
 *
 * Insert routine to restore physical deletes in case of transaction
 * statement rollback.
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	k -
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
dbe_ret_t dbe_index_bkey_insert(
        dbe_index_t* index,
        dbe_bkey_t* k)
{
        dbe_ret_t rc;
        dbe_info_t info;

        CHK_INDEX(index);

        ss_dprintf_1(("dbe_index_bkey_insert\n"));

        ss_output_4(dbe_bkey_dprint(4, k));

        dbe_info_init(info, 0);

        rc = dbe_btree_insert(index->ind_bonsaitree, k, NULL, cd, &info);

        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}

/*##**********************************************************************\
 *
 *		dbe_index_bkey_delete_blobs
 *
 * Deletes blobs from a key value. Used to delete blobs when a statement
 * commits and key values were physicalle deleted by routine
 * dbe_index_bkey_delete_physical.
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	k -
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
dbe_ret_t dbe_index_bkey_delete_blobs(
        dbe_index_t* index,
        dbe_bkey_t* k)
{
#ifndef SS_NOBLOB
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_index_bkey_delete_blobs\n"));

        CHK_INDEX(index);

        if (!dbe_bkey_isblob(k)) {
            return(DBE_RC_SUCC);
        }

        ss_output_4(dbe_bkey_dprint(4, k));

        rc = dbe_blobmgr_deletevtpl_maybedeferred(
                index->ind_go->go_blobmgr,
                dbe_bkey_getvtpl(k));
        su_rc_dassert(rc == DBE_RC_SUCC, rc);

        return(rc);
#else
        return(DBE_ERR_FAILED);
#endif /* SS_NOBLOB */
}

#endif /* NOT_USED */

/*##**********************************************************************\
 *
 *		dbe_index_bkey_delete_physical
 *
 * Physically deletes a dbe_bkey_t from the index. Used from a transaction
 * statement. This routine do not remove blobs, they must be removed
 * separately using routine dbe_index_bkey_delete_blobs.
 *
 * Parameters :
 *
 *	index - in out, use
 *		index system
 *
 *	vtpl - in, use
 *		v-tuple of the key value
 *
 *	trxnum - in
 *		key transcation number
 *
 *	trxid - in
 *		key transaction id
 *
 *	mode - in
 *		Mode flags or'ed in.
 *
 * Return value :
 *
 *      DBE_RC_SUCC or error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_index_bkey_delete_physical(
        dbe_index_t* index,
        dbe_bkey_t* k,
        rs_sysi_t* cd)
{
        dbe_ret_t rc;
        dbe_info_t info;

        ss_dprintf_1(("dbe_index_bkey_delete_physical\n"));

        CHK_INDEX(index);
        /* Merge must not be active because we are deleting rows from
         * Bonsai-tree.*/
        ss_bassert(index->ind_mergeactive == 0);

        ss_output_4(dbe_bkey_dprint(4, k));

        dbe_index_setbkeyflags(k, DBE_INDEXOP_COMMITTED);
        dbe_info_init(info, 0);

        if (BKEY_NLONGUSED(k) == BKEY_2LONGUSED) {
            SS_RTCOVERAGE_INC(SS_RTCOV_INDEX_PHYSDEL_BONSAI);
            rc = dbe_btree_delete_keyandblob(index->ind_bonsaitree, k, cd, &info);
        } else {
            SS_RTCOVERAGE_INC(SS_RTCOV_INDEX_PHYSDEL_PERM);
            rc = dbe_btree_delete_keyandblob(index->ind_permtree, k, cd, &info);
        }

        if (rc == DBE_RC_SUCC && (info.i_flags & DBE_INFO_OUTOFDISKSPACE)) {
            rc = info.i_rc;
        }

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}

#ifdef SS_BLOCKINSERT
dbe_ret_t dbe_index_insert_block(
        dbe_index_t* index,
        vtpl_t* vtpl,
        dbe_indexop_mode_t mode,
        int blockindex)
{
        dbe_ret_t rc;
        dbe_bkey_t* k;

        CHK_INDEX(index);
        ss_dassert(!(mode & DBE_INDEXOP_DELETEMARK));
        ss_assert(blockindex < index->ind_blockarrsize);

        ss_dprintf_1(("dbe_index_insert_block\n"));

        if (VTPL_GROSSLEN(vtpl) > index->ind_bloblimit_high) {
            return(DBE_ERR_TOOLONGKEY);
        }

        k = dbe_bkey_initpermleaf(NULL, index->ind_bkeyinfo, vtpl);

        dbe_index_setbkeyflags(k, mode | DBE_INDEXOP_COMMITTED);

        ss_output_4(dbe_bkey_dprint(4, k));

        rc = dbe_btree_insert_block(
                index->ind_permtree,
                k,
                &index->ind_blockarr[blockindex]);

        dbe_bkey_done(k);

        ss_dprintf_2(("rc = %d\n", rc));

        return(rc);
}
#endif /* SS_BLOCKINSERT */

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *		dbe_index_fakemerge
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
bool dbe_index_fakemerge(dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_fakemerge);
}
#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		dbe_index_hsbsetbloblimit_high
 *
 * Sets new high blob limit value. If addslackp is TRUE some limit is
 * set lower. This is typically done in primary node so that there is
 * room is secondary to update some system columns.
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	addslackp -
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
void dbe_index_hsbsetbloblimit_high(
        dbe_index_t* index,
        bool addslackp)
{
        CHK_INDEX(index);

        index->ind_bloblimit_high =
            dbe_bkey_getbloblimit_high(
                index->ind_bkeyinfo->ki_maxkeylen);

        if (addslackp) {
            index->ind_bloblimit_high -= 16;
        }
}

/*#***********************************************************************\
 *
 *		index_searchsetidle
 *
 * Tries to mark searches as idle until there are not too many searches
 * active.
 *
 * Parameters :
 *
 *	index - use
 *
 *
 *	indsea - in
 *		Current index search.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void index_searchsetidle(
        dbe_index_t* index,
        dbe_indsea_t* indsea)
{
        dbe_index_sealnode_t* n;
        dbe_indsea_t* list_indsea;

        n = index->ind_sealru.isl_next;
        while (n != &index->ind_sealru) {
            list_indsea = n->isl_indsea;
            if (list_indsea != indsea && dbe_indsea_setidle(list_indsea)) {
                index->ind_seabufused -= DBE_INDEX_NSEABUFPERSEARCH;
                ss_dassert(index->ind_seabufused >= 0);
                if (index->ind_seabufused <= index->ind_seabuflimit) {
                    break;
                }
            }
            n = n->isl_next;
        }
}

/*#***********************************************************************\
 *
 *		sealist_insertlast
 *
 *
 *
 * Parameters :
 *
 *	node -
 *
 *
 *	prev_node -
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
static void sealist_insertlast(
        dbe_index_sealnode_t* node,
        dbe_index_sealnode_t* prev_node)
{
        node->isl_next = prev_node->isl_next;
        prev_node->isl_next->isl_prev = node;
        prev_node->isl_next = node;
        node->isl_prev = prev_node;
}

/*#***********************************************************************\
 *
 *		sealist_remove
 *
 *
 *
 * Parameters :
 *
 *	node -
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
static void sealist_remove(
        dbe_index_sealnode_t* node)
{
        node->isl_prev->isl_next = node->isl_next;
        node->isl_next->isl_prev = node->isl_prev;
}

/*##**********************************************************************\
 *
 *		dbe_index_searchadd
 *
 * Adds new search to the list of index searches.
 *
 * Parameters :
 *
 *	index - use
 *		Index system.
 *
 *	indsea - in, hold
 *		New index search added to the list.
 *
 *	p_sealist_node - out, ref
 *
 *
 *	p_sealru_node - out, ref
 *
 *
 *	p_isidle - in, out
 *		If not NULL then search is also activated and p_isidle is
 *		updated.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_index_searchadd(
        dbe_index_t* index,
        dbe_indsea_t* indsea,
        dbe_index_sealnode_t* sealist_node,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle)
{
        CHK_INDEX(index);
        ss_dassert(indsea != NULL);
        ss_dassert(sealist_node != NULL);
        ss_dassert(sealru_node != NULL);

        SsFlatMutexLock(index->ind_listsem);

        index->ind_nsearch++;

        sealist_node->isl_indsea = indsea;
        sealru_node->isl_indsea = indsea;

        sealist_insertlast(sealist_node, index->ind_sealist.isl_prev);
        sealist_insertlast(sealru_node, index->ind_sealru.isl_prev);

        index->ind_seabufused += DBE_INDEX_NSEABUFPERSEARCH;

        if (p_isidle != NULL) {
            index_searchbeginactive_nomutex(index, sealru_node, p_isidle);

        } else if (index->ind_seabufused > index->ind_seabuflimit) {
            index_searchsetidle(index, indsea);
        }

        SsFlatMutexUnlock(index->ind_listsem);
}

/*##**********************************************************************\
 *
 *		dbe_index_searchremove
 *
 * Removes search from the list.
 *
 * Parameters :
 *
 *	index - use
 *		Index system.
 *
 *	sealist_node - in, take
 *		Search position in the list. The postion is returned when
 *		the search was added to the list.
 *
 *	sealru_node - in, take
 *
 *
 *	*p_isidle - in, out
 *		TRUE is the search is in idle state, FALSE otherwise.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_index_searchremove(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealist_node,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle)
{
        CHK_INDEX(index);
        ss_dassert(sealist_node != NULL);
        ss_dassert(sealru_node != NULL);

        SsFlatMutexLock(index->ind_listsem);

        index->ind_nsearch--;

        sealist_remove(sealist_node);
        sealist_remove(sealru_node);

        if (!(*p_isidle)) {
            index->ind_seabufused -= DBE_INDEX_NSEABUFPERSEARCH;
            ss_dassert(index->ind_seabufused >= 0);
            *p_isidle = TRUE;
        }

        SsFlatMutexUnlock(index->ind_listsem);
}

/*#***********************************************************************\
 *
 *		index_searchbeginactive_nomutex
 *
 * Same as dbe_index_searchbeginactive but does not enter the mutex.
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	sealru_node -
 *
 *
 *	p_isidle -
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
static void index_searchbeginactive_nomutex(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle)
{
        CHK_INDEX(index);
        ss_dassert(sealru_node != NULL);
        ss_dassert(p_isidle != NULL);

        sealist_remove(sealru_node);
        if (*p_isidle) {
            index->ind_seabufused += DBE_INDEX_NSEABUFPERSEARCH;
            *p_isidle = FALSE;
        }

        if (index->ind_seabufused > index->ind_seabuflimit) {
            index_searchsetidle(index, sealru_node->isl_indsea);
        }
        sealist_insertlast(sealru_node, index->ind_sealru.isl_prev);
}

/*##**********************************************************************\
 *
 *		dbe_index_searchbeginactive
 *
 *
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	sealru_node -
 *
 *
 *	p_isidle - in, out
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
void dbe_index_searchbeginactive(
        dbe_index_t* index,
        dbe_index_sealnode_t* sealru_node,
        bool* p_isidle)
{
        CHK_INDEX(index);
        ss_dassert(sealru_node != NULL);
        ss_dassert(p_isidle != NULL);

        SsFlatMutexLock(index->ind_listsem);

        index_searchbeginactive_nomutex(index, sealru_node, p_isidle);

        SsFlatMutexUnlock(index->ind_listsem);
}

/*##**********************************************************************\
 *
 *		dbe_index_getnewseaid
 *
 * Returns a new, unique search id.
 *
 * Parameters :
 *
 *	index - use
 *
 *
 * Return value :
 *
 *      New, unique search id.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_index_getnewseaid(
        dbe_index_t* index)
{
        long seaid;

        CHK_INDEX(index);

        SsFlatMutexLock(index->ind_listsem);

        seaid = index->ind_seaid++;

        SsFlatMutexUnlock(index->ind_listsem);

        return(seaid);
}

long dbe_index_getseabufused(
        dbe_index_t* index)
{
        CHK_INDEX(index);

        return(index->ind_seabufused);
}

#ifndef SS_LIGHT

/*##**********************************************************************\
 *
 *		dbe_index_printfp
 *
 * Print inndex tree using givel file pointer.
 *
 * Parameters :
 *
 *	fp - use
 *		Output file pointer. Special pointer values are:
 *		    NULL    - stdout
 *		    -1L     - silent, nothing is printed
 *
 *	index -
 *
 *
 *	values -
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
bool dbe_index_printfp(void* fp, dbe_index_t* index, bool values)
{
        bool succp;

        CHK_INDEX(index);

        SsDbgMessage("Bonsai tree:\n");
        succp = dbe_btree_print(fp, index->ind_bonsaitree, values, FALSE);
        if (succp) {
            SsDbgMessage("Bonsai tree is ok.\n");
        } else {
            SsDbgMessage("Bonsai tree is NOT ok!\n");
        }
        SsDbgMessage("Storage tree:\n");
        if (!dbe_btree_print(fp, index->ind_permtree, values, FALSE)) {
            succp = FALSE;
            SsDbgMessage("Storage tree is NOT ok!\n");
        } else {
            SsDbgMessage("Storage tree is ok.\n");
        }

        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_index_print
 *
 * Prints both index trees to stdout.
 *
 * Parameters :
 *
 *	index - in, use
 *		index system
 *
 *	values - in
 *		if TRUE, also key values are printed
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_index_print(dbe_index_t* index, bool values)
{
        bool succp;

        CHK_INDEX(index);

        succp = dbe_index_printfp(NULL, index, values);

        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_index_check
 *
 * Checks both index trees for consistency.
 *
 * Parameters :
 *
 *	index - in, use
 *		index system
 *
 *	full_check - in
 *		If TRUE, check also index leaf content.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_index_check(dbe_index_t* index, bool full_check)
{
        bool succp;

        CHK_INDEX(index);

        succp = dbe_btree_check(index->ind_bonsaitree, full_check);
        if (!dbe_btree_check(index->ind_permtree, full_check)) {
            succp = FALSE;
        }

        return(succp);
}

/*##**********************************************************************\
 *
 *		dbe_index_printinfo
 *
 *
 *
 * Parameters :
 *
 *	index -
 *
 *
 *	fp -
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
void dbe_index_printinfo(
        void* fp,
        dbe_index_t* index)
{
        dbe_indsea_t* indsea;
        dbe_index_sealnode_t* node;

        SsFprintf(fp, "  Mrgact NSea SeqSeaLim Seabuf: used limit\n");
        SsFprintf(fp, "  %6d %4ld %9ld         %4ld %5ld\n",
            index->ind_mergeactive,
            index->ind_nsearch,
            index->ind_seqsealimit,
            index->ind_seabufused,
            index->ind_seabuflimit);

        SsFprintf(fp, "  Tree height: Bonsai %d Storage %d\n",
            dbe_btree_getheight(index->ind_bonsaitree),
            dbe_btree_getheight(index->ind_permtree));

        SsFprintf(fp, "    TRXBUF:\n");
        dbe_trxbuf_printinfo(fp, index->ind_trxbuf);

        SsFprintf(fp, "    CACHE:\n");
        dbe_cache_printinfo(fp, index->ind_go->go_idxfd->fd_cache);

#ifdef SS_DEBUG
        SsFprintf(fp, "    BONSAI TREE (skeleton):\n");
        dbe_btree_print(fp, index->ind_bonsaitree, FALSE, TRUE);
#endif /* SS_DEBUG */

        SsFprintf(fp, "    INDEX SEARCHES:\n");
        dbe_indsea_printinfoheader(fp);
        SsFlatMutexLock(index->ind_listsem);
        node = index->ind_sealist.isl_next;
        while (node != &index->ind_sealist) {
            indsea = node->isl_indsea;
            dbe_indsea_printinfo(fp, indsea);
            node = node->isl_next;
        }
        SsFlatMutexUnlock(index->ind_listsem);
}

#endif /* SS_LIGHT */
