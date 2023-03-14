/*************************************************************************\
**  source       * dbe8cpls.c
**  directory    * dbe
**  description  * checkpoint list
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

This file implements a live checkpoint bookkeeper. It is normally kept in
main memory and only saved to disk upon request. The list on disk can
also be loaded to memory. The memory representation is a Red-black tree
thus allowing fast access. The disk image is a list of arrays of checkpoint
records. Each array is allocated in a database file block. This class
is needed for change list and checkpoint management sevices.

Limitations:
-----------

In principle there is no upper limit on memory requirement. In practice
that will never be a problem, because no database contains millions of
snapshots, because each snapshot increases the size of the database file
considerably.

Error handling:
--------------

The su_ret_t type return codes are returned in the disk operations.
(Currently they are mostly asserted, though).

Objects used:
------------

Split virtual file object <su0svfil.h>
Cache object "dbe8cach.h"
Freelist object "dbe8flst.h"
Block list header subobject "dbe9blst.h"

Preconditions:
-------------

All objects that this class uses must be initialized before creation
of this object.

Multithread considerations:
--------------------------

Any function of objects of this class must be called in a mutexed section
(ie. no automatic mutexing),
but separate instances can be used concurrently (ie. no shared data without
multithread protection between instances).

Portability:
-----------

No problem.

Example:
-------

Maybe later

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssdebug.h>
#include <ssmem.h>

#include <su0rbtr.h>

#include "dbe9type.h"
#include "dbe9blst.h"
#include "dbe8cpls.h"

#ifdef SS_DEBUG
static char cpls_readctx[] = "Checkpoint list read";
static char cpls_delctx[] = "Checkpoint list delete";
static char cpls_writectx[] = "Checkpoint list write";
#else
#define cpls_readctx    NULL
#define cpls_delctx     NULL
#define cpls_writectx   NULL
#endif

/* one checkpoint record
** NOTE: This is only a search record for checkpoint. A more complete
** record is stored in disk address pointed to by cpr_daddr
*/

struct dbe_cprecord_st {
        dbe_cpnum_t cpr_cpnum; /* checkpoint number */
        su_daddr_t  cpr_daddr; /* checkpoint record disk address */
};
typedef struct dbe_cprecord_st dbe_cprecord_t;

/* the checkpoint list object */
struct dbe_cplist_st {
        su_rbt_t*       cpl_cppool;
        su_svfil_t*     cpl_file;
        dbe_cache_t*    cpl_cache;
        dbe_freelist_t* cpl_freelist;
        dbe_chlist_t*   cpl_chlist;
        size_t          cpl_blocksize;
};

#ifndef NO_ANSI
static int dbe_cpr_cmp(void* left, void* right);
#else /* NO_ANSI */
static int dbe_cpr_cmp();
#endif /* NO_ANSI */

/*#**********************************************************************\
 * 
 *		DBE_CPLNODE_CAPACITY
 * 
 * Macro for calculating number of checkpoint records that fit into
 * one disk block.
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		database file block size 
 *
 * 
 * Return value : number of records that fit into one block
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_CPLNODE_CAPACITY(blocksize) \
        (((blocksize) - DBE_BLIST_DATAOFFSET) / (sizeof(dbe_cpnum_t)+sizeof(su_daddr_t)))


/*#***********************************************************************\
 * 
 *		cppool_rbt_deletefun
 * 
 * 
 * 
 * Parameters : 
 * 
 *	p - 
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
static void cppool_rbt_deletefun(void* p)
{
        SsMemFree(p);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_init
 * 
 * Creates a checkpoint list object.
 * 
 * Parameters : 
 * 
 *	p_svfile - in out, hold
 *		pointer to split virtual file object
 *
 *	p_cache - in out, hold
 *		pointer to cache object
 *
 *	p_fl - in out, hold
 *		pointer to freelist object
 *
 *      p_cl - in out, hold
 *          pointer to changelist object
 *
 *	list_daddr - in
 *		disk address of checkpoint list to load or
 *          SU_DADDR_NULL if no list to load
 *          
 *
 * Return value : pointer to created checkpoint list object
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_cplist_t *dbe_cpl_init(p_svfile, p_cache, p_fl, p_cl, list_daddr)
	su_svfil_t* p_svfile;
	dbe_cache_t* p_cache;
	dbe_freelist_t* p_fl;
        dbe_chlist_t* p_cl;
	su_daddr_t list_daddr;
{
        dbe_cplist_t *p_cpl;
        dbe_blheader_t header;
        dbe_cacheslot_t *cacheslot;
        void *diskbuf;

        p_cpl = SSMEM_NEW(dbe_cplist_t);
        p_cpl->cpl_cppool = su_rbt_init(dbe_cpr_cmp, cppool_rbt_deletefun);
        p_cpl->cpl_file = p_svfile;
        p_cpl->cpl_cache = p_cache;
        p_cpl->cpl_freelist = p_fl;
        p_cpl->cpl_chlist = p_cl;
        p_cpl->cpl_blocksize = su_svf_getblocksize(p_svfile);
        while (list_daddr != SU_DADDR_NULL) {  /* load from disk */
            uint i;
            char *p_cprec_dbuf;
            dbe_cprecord_t *p_cprec;

            cacheslot = dbe_cache_reach(p_cpl->cpl_cache,
                                        list_daddr,
                                        DBE_CACHE_READONLY,
                                        DBE_INFO_CHECKPOINT,
                                        (char**)&diskbuf,
                                        cpls_readctx);
            dbe_blh_get(&header, diskbuf);
            ss_debug(
                if (header.bl_type != (dbe_blocktype_t)DBE_BLOCK_CPLIST) {
                    SsDbgPrintf("\nHeader.bl_type = %d, list_daddr = %ld\n",
                        (int)header.bl_type, list_daddr);
                    ss_rc_error(header.bl_type);
                }
            );

            if (header.bl_type != (dbe_blocktype_t)DBE_BLOCK_CPLIST) {
                if (dbe_debug) {
                    /* Stop reading the list. */
                    list_daddr = SU_DADDR_NULL;
                } else {
                    /* Fatal error. */
                    ss_rc_error((int)header.bl_type);
                }
            } else {
                p_cprec_dbuf = (char*)diskbuf + DBE_BLIST_DATAOFFSET;
                for (i = header.bl_nblocks; i > 0; i--) {
                    p_cprec = SSMEM_NEW(dbe_cprecord_t);
                    p_cprec->cpr_cpnum = SS_UINT4_LOADFROMDISK(p_cprec_dbuf);
                    p_cprec_dbuf += sizeof(p_cprec->cpr_cpnum);
                    p_cprec->cpr_daddr = SS_UINT4_LOADFROMDISK(p_cprec_dbuf);
                    p_cprec_dbuf += sizeof(p_cprec->cpr_cpnum);
                    su_rbt_insert(p_cpl->cpl_cppool, p_cprec);
                }

                list_daddr = header.bl_next;
            }
            dbe_cache_release(
                p_cpl->cpl_cache,
                cacheslot,
                DBE_CACHE_CLEANLASTUSE,
                cpls_readctx);
        }
        return (p_cpl);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_done
 * 
 * Deletes a checkpoint list object in memory (not the saved disk image).
 * 
 * Parameters : 
 * 
 *	p_cpl - in, take
 *		pointer to checkpoint list object
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cpl_done(p_cpl)
	dbe_cplist_t *p_cpl;
{
        su_rbt_done(p_cpl->cpl_cppool);
        SsMemFree(p_cpl);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_deletefromdisk
 * 
 * Deletes a disk image of a checkpoint list (ie. puts it into free list)
 * when the checkpoint number matches. If the checkpoint list is inherited
 * from an earlier checkpoint the disk blocks are put to the change list.
 * 
 * Parameters : 
 * 
 *	p_cpl_current - in, use
 *		pointer to current cp list
 *
 *      cpnum - in
 *          checkpoint number below which the list block are not freed
 * 
 *	list_to_delete_daddr - in
 *		disk address of saved cp list
 *          (probably an old one)
 *
 * Return value : SU_SUCCESS if OK or
 *                something else otherwise
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cpl_deletefromdisk(p_cpl_current, cpnum, list_to_delete_daddr)
	dbe_cplist_t *p_cpl_current;
        dbe_cpnum_t cpnum;
	su_daddr_t list_to_delete_daddr;
{
        su_ret_t rc;
        dbe_blheader_t header;
        dbe_cacheslot_t* cacheslot;
        void* diskbuf;

        SS_PUSHNAME("dbe_cpl_deletefromdisk");
        while (list_to_delete_daddr != SU_DADDR_NULL) {
            cacheslot =
                dbe_cache_reach(
                    p_cpl_current->cpl_cache,
                    list_to_delete_daddr,
                    DBE_CACHE_READONLY,
                    DBE_INFO_CHECKPOINT,
                    (char**)&diskbuf,
                    cpls_delctx);
            dbe_blh_get(&header, diskbuf);
            dbe_cache_release(
                p_cpl_current->cpl_cache,
                cacheslot,
                DBE_CACHE_CLEANLASTUSE,
                cpls_delctx);
            if (header.bl_cpnum < cpnum) {
                ss_dprintf_2(("dbe_cpl_deletefromdisk() add to chlist: cpnum=%ld daddr=%ld\n",
                              header.bl_cpnum, list_to_delete_daddr));
                rc = dbe_cl_add(
                        p_cpl_current->cpl_chlist,
                        header.bl_cpnum,
                        list_to_delete_daddr);
                su_rc_assert(rc == SU_SUCCESS, rc);
            } else if (header.bl_cpnum == cpnum) {
                ss_dprintf_2(("dbe_cpl_deletefromdisk() freeing: %ld\n",
                              list_to_delete_daddr));
                rc = dbe_fl_free(
                        p_cpl_current->cpl_freelist,
                        list_to_delete_daddr);
                su_rc_assert(rc == SU_SUCCESS, rc);
            } else {
                ss_error;
            }
            list_to_delete_daddr = header.bl_next;
        }
        SS_POPNAME;
        return (SU_SUCCESS);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_add
 * 
 * Adds a checkpoint to the list         
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		number of the new checkpoint
 *
 *	record_daddr - in
 *		disk address of the checkpoint record
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cpl_add(p_cpl, cpnum, record_daddr)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
	su_daddr_t record_daddr;
{
        dbe_cprecord_t *p_cpr;

        p_cpr = SSMEM_NEW(dbe_cprecord_t);
        p_cpr->cpr_cpnum = cpnum;
        p_cpr->cpr_daddr = record_daddr;
        su_rbt_insert(p_cpl->cpl_cppool, p_cpr);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_remove
 * 
 * Removes checkpoint from checkpoint list.
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		number of checkpoint to remove
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cpl_remove(p_cpl, cpnum)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
{
        dbe_cprecord_t cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        if (p_rbt_node != NULL) {
            su_rbt_delete(p_cpl->cpl_cppool, p_rbt_node);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_isalive
 * 
 * Checks whether the specified checkpoint number exists.
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		checkpoint number to test
 *
 * 
 * Return value : TRUE if found
 *                FALSE if not
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
bool dbe_cpl_isalive(p_cpl, cpnum)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
{
        dbe_cprecord_t cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        return ((p_rbt_node != NULL) ? TRUE : FALSE);
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_getdaddr
 * 
 * Gets disk address of checkpoint record of the specified checkpoint
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		checkpoint number
 *
 * 
 * Return value : disk address of checkpoint record
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_daddr_t dbe_cpl_getdaddr(p_cpl, cpnum)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
{
        dbe_cprecord_t cpr;
        dbe_cprecord_t *p_cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        ss_dassert(p_rbt_node != NULL);
        p_cpr = su_rbtnode_getkey(p_rbt_node);
        return (p_cpr->cpr_daddr);
}

#ifdef NOT_USED

/*##**********************************************************************\
 * 
 *		dbe_cpl_setdaddr
 * 
 * Sets disk address of an existing checkpoint
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		point to checkpoint list object
 *
 *	cpnum - in
 *		number of checkpoint to update
 *
 *	record_daddr - in
 *		disk address of checkpoint record
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cpl_setdaddr(p_cpl, cpnum, record_daddr)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
	su_daddr_t record_daddr;
{
        dbe_cprecord_t cpr;
        dbe_cprecord_t *p_cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        ss_dassert(p_rbt_node != NULL);
        p_cpr = su_rbtnode_getkey(p_rbt_node);
        p_cpr->cpr_daddr = record_daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_lastcommon
 * 
 * Finds biggest checkpoint number of the newest checkpoint that is in the
 * common equal tail of two checkpoint lists.
 * 
 * Parameters : 
 * 
 *	p_cpl - in, use
 *		pointer to checkpoint list object
 *
 *	p_oldcpl - in, use
 *		pointer to older checkpoint list object
 *
 * Return value : 0 if no common part exists or the last common
 *                checkpoint number
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_cpnum_t dbe_cpl_lastcommon(p_cpl, p_oldcpl)
	dbe_cplist_t *p_cpl;
	dbe_cplist_t *p_oldcpl;
{
        su_rbt_t *p_rbt1;
        su_rbt_t *p_rbt2;
        su_rbt_node_t *p_rbt_node1;
        su_rbt_node_t *p_rbt_node2;
        dbe_cprecord_t *p_cpr1;
        dbe_cprecord_t *p_cpr2;
        dbe_cpnum_t last_common;

        p_rbt1 = p_cpl->cpl_cppool;
        p_rbt2 = p_oldcpl->cpl_cppool;
        p_rbt_node1 = su_rbt_min(p_rbt1, NULL);
        p_rbt_node2 = su_rbt_min(p_rbt2, NULL);
        for (last_common = 0;;) {
            if (p_rbt_node1 == NULL || p_rbt_node2 == NULL) {
                return last_common;
            }
            p_cpr1 = su_rbtnode_getkey(p_rbt_node1);
            p_cpr2 = su_rbtnode_getkey(p_rbt_node2);
            if (p_cpr1->cpr_cpnum != p_cpr2->cpr_cpnum) {
                return last_common;
            }
            last_common = p_cpr1->cpr_cpnum;
            p_rbt_node1 = su_rbt_succ(p_rbt1, p_rbt_node1);
            p_rbt_node2 = su_rbt_succ(p_rbt2, p_rbt_node2);
        }
}

#endif /* NOT_USED */

/*##**********************************************************************\
 * 
 *		dbe_cpl_prevfrom
 * 
 * Gets previous checkpoint number of the specified existing checkpoint.
 * 
 * Parameters : 
 * 
 *	p_cpl - in, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		checkpoint number
 *
 * Return value : number of the biggest existing checkpoint that is
 *                smaller than cpnum  or 0 if checkpoint meeting
 *                the criteria doesn't exist.
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_cpnum_t dbe_cpl_prevfrom(p_cpl, cpnum)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
{
        dbe_cprecord_t cpr;
        dbe_cprecord_t *p_cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        if (p_rbt_node == NULL) {
            p_rbt_node = su_rbt_max(p_cpl->cpl_cppool, NULL);
        }
        while (p_rbt_node != NULL) {
            p_cpr = su_rbtnode_getkey(p_rbt_node);
            if (p_cpr->cpr_cpnum < cpnum) {
                return (p_cpr->cpr_cpnum);
            }
            p_rbt_node = su_rbt_pred(p_cpl->cpl_cppool, p_rbt_node);
        }
        return 0L;
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_nextto
 * 
 * Gets next checkpoint number of the specified existing checkpoint.
 * 
 * Parameters : 
 * 
 *	p_cpl - in, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		checkpoint number
 *
 * Return value : number of the smallest existing checkpoint number
 *                that is bigger than cpnum or 0 if checkpoint meeting
 *                the criteria doesn't exist.
 *
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_cpnum_t dbe_cpl_nextto(p_cpl, cpnum)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
{
        dbe_cprecord_t cpr;
        dbe_cprecord_t *p_cpr;
        su_rbt_node_t *p_rbt_node;

        cpr.cpr_cpnum = cpnum;
        p_rbt_node = su_rbt_search(p_cpl->cpl_cppool, &cpr);
        if (p_rbt_node == NULL) {
            p_rbt_node = su_rbt_min(p_cpl->cpl_cppool, NULL);
        }
        while (p_rbt_node != NULL) {
            p_cpr = su_rbtnode_getkey(p_rbt_node);
            if (p_cpr->cpr_cpnum > cpnum) {
                return (p_cpr->cpr_cpnum);
            }
            p_rbt_node = su_rbt_succ(p_cpl->cpl_cppool, p_rbt_node);
        }
        return 0L;
}

/*##**********************************************************************\
 * 
 *		dbe_cpl_last
 * 
 * Gets last checkpoint # 
 * 
 * Parameters : 
 * 
 *	p_cpl - in, use
 *		pointer to checkpoint list object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_cpnum_t dbe_cpl_last(
        dbe_cplist_t *p_cpl)
{
        dbe_cprecord_t *p_cpr;
        su_rbt_node_t *p_rbt_node;

        p_rbt_node = su_rbt_max(p_cpl->cpl_cppool, NULL);
        if (p_rbt_node == NULL) {
            return 0L;
        }
        p_cpr = su_rbtnode_getkey(p_rbt_node);
        return (p_cpr->cpr_cpnum);

}

/*##**********************************************************************\
 * 
 *		dbe_cpl_save
 * 
 * Saves checkpoint list to disk.
 * 
 * Parameters : 
 * 
 *	p_cpl - in out, use
 *		pointer to checkpoint list object
 *
 *	cpnum - in
 *		checkpoint number
 *
 *	p_daddr - out
 *		pointer to variable where the disk address of
 *          the saved list is stored
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                something else if not
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cpl_save(p_cpl, cpnum, p_daddr)
	dbe_cplist_t *p_cpl;
	dbe_cpnum_t cpnum;
	su_daddr_t *p_daddr;
{
        su_ret_t rc;
        dbe_blheader_t header;
        dbe_cacheslot_t *cacheslot;
        void *diskbuf;
        su_daddr_t list_daddr;
        su_rbt_node_t *p_rbt_node;
        char *p_cprec_dbuf;

        dbe_bl_nblocks_t nblocks;
        dbe_bl_nblocks_t blockcapacity;
        dbe_info_t info;

        dbe_info_init(info, DBE_INFO_DISKALLOCNOFAILURE);
        blockcapacity =
            (dbe_bl_nblocks_t)DBE_CPLNODE_CAPACITY(p_cpl->cpl_blocksize);

        dbe_blh_init(&header, (dbe_blocktype_t)DBE_BLOCK_CPLIST, cpnum);
        p_rbt_node = su_rbt_min(p_cpl->cpl_cppool, NULL);
        nblocks = 0;
        cacheslot = NULL;
        p_cprec_dbuf = NULL;
        list_daddr = SU_DADDR_NULL;
        rc = SU_SUCCESS;

        while (p_rbt_node != NULL) {
            dbe_cprecord_t *p_cprec;

            if (nblocks == 0) {
                header.bl_next = list_daddr;
                rc = dbe_fl_alloc(p_cpl->cpl_freelist, &list_daddr, &info);
                su_rc_assert(rc == SU_SUCCESS, rc);
                cacheslot = dbe_cache_reach(p_cpl->cpl_cache,
                                            list_daddr,
                                            DBE_CACHE_WRITEONLY,
                                            DBE_INFO_CHECKPOINT,
                                            (char**)&diskbuf,
                                            cpls_writectx);
                ss_dassert(cacheslot != NULL);
                p_cprec_dbuf = (char*)diskbuf + DBE_BLIST_DATAOFFSET;
            }
            p_cprec = su_rbtnode_getkey(p_rbt_node);

            SS_UINT4_STORETODISK(p_cprec_dbuf, p_cprec->cpr_cpnum);
            p_cprec_dbuf += sizeof(p_cprec->cpr_cpnum);
            SS_UINT4_STORETODISK(p_cprec_dbuf, p_cprec->cpr_daddr);
            p_cprec_dbuf += sizeof(p_cprec->cpr_daddr);
            nblocks++;
            p_rbt_node = su_rbt_succ(p_cpl->cpl_cppool, p_rbt_node);
            if (nblocks == blockcapacity || p_rbt_node == NULL) {
                header.bl_nblocks = nblocks;
                nblocks = 0;
                dbe_blh_put(&header, diskbuf);
                dbe_cache_release(
                    p_cpl->cpl_cache,
                    cacheslot,
                    DBE_CACHE_FLUSH,
                    cpls_writectx);
            }
        }
        su_svf_flush(p_cpl->cpl_file);
        *p_daddr = list_daddr;
        return (rc);
}        

/*#***********************************************************************\
 * 
 *		dbe_cpr_cmp
 * 
 * Compares numbers of two checkpoint records. (A callback function
 * for red-black binary tree).
 * 
 * Parameters : 
 * 
 *	left - in, use
 *		pointer to first checkpoint record
 *
 *	right - in, use
 *		pointer to second checkpoint record
 *
 * Return value : logical subtraction of checkpoint  numbers of
 *                *left and *right
 *                (ie. < 0 if left < right,
 *                     = 0 if left = right or
 *                     > 0 if left > right)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static int dbe_cpr_cmp(left, right)
	void *left;
	void *right;
{
        if (sizeof(int) == sizeof(dbe_cpnum_t)) {
            return (int)(((dbe_cprecord_t*)left)->cpr_cpnum -
                         ((dbe_cprecord_t*)right)->cpr_cpnum);
        }
        if (((dbe_cprecord_t*)left)->cpr_cpnum <
            ((dbe_cprecord_t*)right)->cpr_cpnum)
        {
            return (-1);
        }
        if (((dbe_cprecord_t*)left)->cpr_cpnum >
            ((dbe_cprecord_t*)right)->cpr_cpnum)
        {
            return (1);
        }
        return (0);
}


