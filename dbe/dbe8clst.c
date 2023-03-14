/*************************************************************************\
**  source       * dbe8clst.c
**  directory    * dbe
**  description  * Change list management services
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

Change lists are needed for keeping track of which database file blocks
of older checkpoints are overridden. This information is read when
and old checkpoint is deleted. The blocks from a deleted checkpoint are
added to the newest freelist. Each time a new checkpoint is created the
change list must be saved to disk. The change list is implemented as a
list of disk blocks each containing as many change records as fits into it.
Each change record is a pair of two numbers: the checkpoint number and the
disk address of the replaced block.

Limitations:
-----------

none.

Error handling:
--------------

Error conditions are handled by returning status code to caller.
(The error handling strategy should be improved).


Objects used:
------------

Split virtual file <su0svfil.h>
Disk Cache <dbe8cach.h>
Free list <dbe8flst.h>

Preconditions:
-------------


Multithread considerations:
--------------------------

This package takes care of concurrency control automatically.

Example:
-------

Later...

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>

#include <su0rbtr.h>

#include "dbe9crec.h"
#include "dbe8clst.h"
#include "dbe9blst.h"

#ifdef SS_DEBUG
static char clst_writectx[] = "Change list write";
#ifndef SS_MYSQL
static char clst_delctx[] = "Change list delete";
#endif
static char clst_readctx[] = "Change list read";
#else
#define clst_writectx   NULL
#define clst_delctx     NULL
#define clst_readctx    NULL
#endif

/* one changelist node */
struct dbe_clnode_st {
        su_daddr_t      cln_daddr;

        /* 1:1 correspondence with the disk image: */
        dbe_blheader_t  cln_header;
        dbe_clrecord_t* cln_data;    /* Array of clrecords */
        dbe_cacheslot_t* cln_slot;
};

typedef struct dbe_clnode_st dbe_clnode_t;

#define DBE_CLNODE_CAPACITY(blocksize) \
        (((blocksize) - DBE_BLIST_DATAOFFSET) / \
        (sizeof(dbe_cpnum_t)+sizeof(su_daddr_t)))


/* structure to represent change list */
struct dbe_chlist_st {
        su_svfil_t     *cl_file;  
        dbe_cache_t    *cl_cache;
        dbe_freelist_t *cl_freelist;
        SsSemT         *cl_mutex;
        size_t          cl_blocksize;
        su_daddr_t      cl_spareblock;  /* reserved free db file block */
        dbe_cpnum_t     cl_nextcpnum;   /* number we're marking on blocks */
        dbe_clnode_t   *cl_firstnode;   /* always in memory when running */
        dbe_clnode_t   *cl_lastnode;    /* perhaps alias of firstnode */
        ss_debug(su_rbt_t* cl_addrtree;)    /* for debugging */
};

/* change list iterator structure */
struct dbe_chlist_iter_st {
        dbe_chlist_t       *ci_chlist;
        dbe_clnode_t       *ci_node;
        dbe_cacheslot_t    *ci_cacheslot;
        void               *ci_diskbuf;
        dbe_bl_nblocks_t    ci_pos;
};

#ifndef NO_ANSI

static su_ret_t dbe_cl_write1node(
        dbe_chlist_t *p_cl,
        su_daddr_t save_daddr,
        su_list_t **p_deferchlist);

static su_ret_t dbe_cl_read1node(
        dbe_chlist_t* p_cl,
        dbe_clnode_t* p_clnode,
        su_daddr_t daddr);

static void dbe_cln_getdata(
        dbe_clnode_t* p_clnode,
        void *diskbuf);

static void dbe_cln_putdata(
        dbe_clnode_t* p_clnode,
        void *diskbuf);

static su_ret_t dbe_cl_linktoend_nomutex(
	dbe_chlist_t* p_cl,
	su_daddr_t tail_daddr);

static su_ret_t dbe_cl_preparetosave_nomutex(
	dbe_chlist_t* p_cl,
        su_list_t** deferchlist1,
        su_list_t** deferchlist2);

#else /* NO_ANSI */

static su_ret_t dbe_cl_write1node();
static su_ret_t dbe_cl_read1node();
static void dbe_cln_getdata();
static void dbe_cln_putdata();

#endif /* NO_ANSI */

#ifdef SS_DEBUG

static int cl_addrcmp(void* a1, void* a2)
{
        if (sizeof(int) >= sizeof(su_daddr_t)) {
            return (int)(su_daddr_t)a1 - (int)(su_daddr_t)a2;
        }
        if ((su_daddr_t)a1 > (su_daddr_t)a2) {
            return (1);
        }
        if ((su_daddr_t)a1 < (su_daddr_t)a2) {
            return (-1);
        }
        return (0);
}

static void cl_add1nodetoaddrtree(
        dbe_chlist_t* p_cl,
        dbe_clnode_t *p_clnode)
{
        dbe_bl_nblocks_t i;
        su_daddr_t daddr;

        for (i = 0; i < p_clnode->cln_header.bl_nblocks; i++) {
            daddr = p_clnode->cln_data[i].clr_daddr;
            ss_assert(su_rbt_search(p_cl->cl_addrtree, (void*)daddr) == NULL);
            su_rbt_insert(p_cl->cl_addrtree, (void*)daddr);
        }
}
#endif  /* SS_DEBUG */

#define CL_MUTEX_ENTER(mutex) \
        ss_dprintf_1(("%s %d: entering cl mutex (addr=0x%08lX)..\n",\
        __FILE__, __LINE__, (ulong)(mutex)));\
        SsSemEnter(mutex);\
        ss_dprintf_1(("%s:.. mutex entered.\n", __FILE__));

#define CL_MUTEX_EXIT(mutex) \
        SsSemExit(mutex);\
        ss_dprintf_1(("%s: exited cl mutex (addr=0x%08lX)\n",\
        __FILE__, (ulong)(mutex)));


static dbe_clnode_t* clnode_init(dbe_cache_t* cache)
{
        dbe_clnode_t* clnode;

        clnode = SSMEM_NEW(dbe_clnode_t);
        clnode->cln_slot =
            dbe_cache_alloc(cache, (char**)&clnode->cln_data);
        return (clnode);
}

static void clnode_done(dbe_cache_t* cache, dbe_clnode_t* clnode)
{
        dbe_cache_free(cache, clnode->cln_slot);
        SsMemFree(clnode);
}
/*##**********************************************************************\
 * 
 *		dbe_cl_init
 * 
 * Creates a changelist object
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
 *	next_cpnum - in
 *		next checkpoint number
 *
 *	list_start - in
 *		start address of list to read or
 *          SU_DADDR_NULL if new list
 *
 * Return value - give :
 *          pointer to created object
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_chlist_t *dbe_cl_init(p_svfile, p_cache, p_fl, next_cpnum, list_start)
	su_svfil_t* p_svfile;
	dbe_cache_t* p_cache;
	dbe_freelist_t* p_fl;
	dbe_cpnum_t next_cpnum;
	su_daddr_t list_start;
{
        dbe_chlist_t *p_cl;

        p_cl = SsMemAlloc(sizeof(dbe_chlist_t));
        p_cl->cl_file = p_svfile;
        p_cl->cl_cache = p_cache;
        p_cl->cl_freelist = p_fl;
        p_cl->cl_blocksize = su_svf_getblocksize(p_cl->cl_file);
        p_cl->cl_spareblock = SU_DADDR_NULL;
        p_cl->cl_nextcpnum = next_cpnum;
        p_cl->cl_firstnode = clnode_init(p_cl->cl_cache);
        ss_dprintf_1(("%s %d: clnode_init gives 0x%08lX (cl=0x%08lX)\n",
                      __FILE__, __LINE__,
                      (ulong)p_cl->cl_firstnode,
                      (ulong)p_cl));
        p_cl->cl_mutex = SsSemCreateLocal(SS_SEMNUM_DBE_CLST);
        ss_debug(p_cl->cl_addrtree = su_rbt_init(cl_addrcmp, NULL);)

#if defined(AUTOTEST_RUN) && defined(SS_DEBUG)
        su_rbt_maxnodes(p_cl->cl_addrtree, 80000);
#endif
        if (list_start != SU_DADDR_NULL) {
            su_ret_t rc;

            rc = dbe_cl_read1node(p_cl, p_cl->cl_firstnode, list_start);
            if (rc != SU_SUCCESS) {
                ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (cl=0x%08lX)\n",
                              __FILE__, __LINE__,
                              (ulong)p_cl->cl_firstnode,
                              (ulong)p_cl));
                clnode_done(p_cl->cl_cache, p_cl->cl_firstnode);
                SsMemFree(p_cl);
                return (NULL);
            }
            ss_debug(cl_add1nodetoaddrtree(p_cl, p_cl->cl_firstnode);)
        } else {
            dbe_blh_init(&p_cl->cl_firstnode->cln_header,
                         (dbe_blocktype_t)DBE_BLOCK_CHANGELIST,
                         p_cl->cl_nextcpnum);
            p_cl->cl_firstnode->cln_daddr = SU_DADDR_NULL;
        }
        if (p_cl->cl_firstnode->cln_header.bl_next == SU_DADDR_NULL) {
            p_cl->cl_lastnode = p_cl->cl_firstnode;
            ss_dprintf_1(("%s %d: p_cl->cl_lastnode = 0x%08lX\n",
                          __FILE__, __LINE__, (ulong)p_cl->cl_lastnode));
        } else {
            p_cl->cl_lastnode = NULL;
        }
        return (p_cl);
}


/*##**********************************************************************\
 * 
 *		dbe_cl_done
 * 
 * Deletes (does not save) a changelist object
 * 
 * Parameters : 
 * 
 *	p_cl - in, take
 *		pointer to changelist object
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cl_done(p_cl)
	dbe_chlist_t *p_cl;
{
        if (p_cl->cl_firstnode == p_cl->cl_lastnode) {
            p_cl->cl_lastnode = NULL;
        }
        ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (cl=0x%08lX)\n",
                      __FILE__, __LINE__,
                      (ulong)p_cl->cl_firstnode,
                      (ulong)p_cl));
        clnode_done(p_cl->cl_cache, p_cl->cl_firstnode);
        if (p_cl->cl_lastnode != NULL) {
            ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (cl=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_cl->cl_lastnode,
                          (ulong)p_cl));
            clnode_done(p_cl->cl_cache, p_cl->cl_lastnode);
        }
        SsSemFree(p_cl->cl_mutex);
        ss_debug(su_rbt_done(p_cl->cl_addrtree);)
        SsMemFree(p_cl);
}

/*##**********************************************************************\
 * 
 *		dbe_cl_add
 * 
 * Adds a change record to list
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	cpnum - in
 *		checkpoint number of replaced block
 *
 *	daddr - in
 *		disk address of -"-
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cl_add(p_cl, cpnum, daddr)
	dbe_chlist_t *p_cl;
	dbe_cpnum_t cpnum;
	su_daddr_t daddr;
{
        su_ret_t rc;
        dbe_bl_nblocks_t capacity;
        dbe_clrecord_t *p_clr;
        su_list_t *deferchlist;

        rc = SU_SUCCESS;
        deferchlist = NULL;

        ss_debug(
            if (SsMemTrcGetCallStackHeight(NULL) > 0) {
                char* caller;
                caller = SsMemTrcGetCallStackNth(NULL, 0);
                ss_assert(caller != NULL);
                ss_dprintf_2(("[caller=%s]:dbe_cl_add(blkcpnum=%ld,cpnum=%ld,daddr=%ld)\n",
                    caller, p_cl->cl_nextcpnum, cpnum, daddr));
            } else {
                ss_dprintf_2(("dbe_cl_add(blkcpnum=%ld,cpnum=%ld,daddr=%ld)\n",
                    p_cl->cl_nextcpnum, cpnum, daddr));
            }
        );
        ss_dassert(p_cl->cl_nextcpnum > cpnum);
        ss_dassert((long)daddr >= 0);
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        capacity = (dbe_bl_nblocks_t)DBE_CLNODE_CAPACITY(p_cl->cl_blocksize);
        if (p_cl->cl_firstnode->cln_header.bl_nblocks == capacity) {
            rc = dbe_cl_write1node(p_cl, SU_DADDR_NULL, &deferchlist);
        }
        p_clr =
            &p_cl->cl_firstnode->cln_data[
                    p_cl->cl_firstnode->cln_header.bl_nblocks];
        p_cl->cl_firstnode->cln_header.bl_nblocks++;
        p_clr->clr_cpnum = cpnum;
        p_clr->clr_daddr = daddr;

        ss_dassert(su_rbt_search(p_cl->cl_addrtree, (void*)daddr) == NULL);
        ss_debug(su_rbt_insert(p_cl->cl_addrtree, (void*)daddr);)

        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
        if (deferchlist != NULL) {
            dbe_cl_dochlist(p_cl, deferchlist);
        }
        return (rc);
}

bool dbe_cl_find(p_cl, p_cpnum, daddr)
    dbe_chlist_t *p_cl;
    dbe_cpnum_t *p_cpnum;
    su_daddr_t daddr;
{
        dbe_chlist_iter_t *ci;
        bool found=FALSE;
        dbe_cpnum_t block_cpnum;
        su_daddr_t block_daddr;
        dbe_bl_nblocks_t i;

        ss_dprintf_1(("dbe_cl_find: daddr=%d\n", (int)daddr));

        CL_MUTEX_ENTER(p_cl->cl_mutex);
        for (i=0; i<p_cl->cl_firstnode->cln_header.bl_nblocks; i++) {
            if (p_cl->cl_firstnode->cln_data[i].clr_daddr == daddr) {
                *p_cpnum = p_cl->cl_firstnode->cln_data[i].clr_cpnum;
                found = TRUE;
                break;
            }
        }

        CL_MUTEX_EXIT(p_cl->cl_mutex);

        if (found) {
            ss_dprintf_1(("dbe_cl_find: found in p_cl\n"));
            return TRUE;
        }

        ci = dbe_ci_init(p_cl);
        while (dbe_ci_nextnode(ci)) {
            uint nblocks;
            dbe_ci_getnodeinfo(ci, &block_cpnum, &block_daddr, &nblocks);
            if (block_daddr == daddr) {
                found = TRUE;
                break;
            }
            ss_dprintf_1(("dbe_cl_find: dbe_ci_getnodeinfo ->%d nblocks=%d\n",
                          (int)block_daddr,
                          (int)nblocks));
            if (block_daddr == SU_DADDR_NULL) {
                ss_dprintf_1(("dbe_cl_find: block_daddr == SU_DADDR_NULL"));
                break;
            }
            while (dbe_ci_getnext (ci, &block_cpnum, &block_daddr)) {
                ss_dprintf_1(("dbe_cl_find: check against %d\n",
                              (int)block_daddr));
                if (block_daddr == daddr) {
                    found = TRUE;
                    *p_cpnum = block_cpnum;
                    break;
                }
            }
        }
        dbe_ci_done(ci);

        ss_dassert(found || su_rbt_search(p_cl->cl_addrtree, (void*)daddr) == NULL);

        ss_dprintf_1(("dbe_cl_find: return %d\n", found));

        return found;
}

/*##**********************************************************************\
 * 
 *		dbe_cl_preparetosave
 * 
 * Prepares to save a changelist ie. makes sure freelist does not need
 * to be called at save operation.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cl_preparetosave(p_cl)
	dbe_chlist_t *p_cl;
{
        su_ret_t rc;
        su_list_t *deferchlist1;
        su_list_t *deferchlist2;


        deferchlist1 = NULL;
        deferchlist2 = NULL;

        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        rc = dbe_cl_preparetosave_nomutex(
                p_cl,
                &deferchlist1,
                &deferchlist2);
        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
        if (deferchlist1 != NULL) {
            dbe_cl_dochlist(p_cl, deferchlist1);
        }
        if (deferchlist2 != NULL) {
            dbe_cl_dochlist(p_cl, deferchlist2);
        }
        return (rc);
}

/*#***********************************************************************\
 * 
 *		dbe_cl_preparetosave_nomutex
 * 
 * Prepares to save a changelist ie. makes sure freelist does not need
 * to be called at save operation. No mutex version for internal use.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t dbe_cl_preparetosave_nomutex(p_cl, deferchlist1, deferchlist2)
	dbe_chlist_t *p_cl;
        su_list_t **deferchlist1;
        su_list_t **deferchlist2;
{
        su_ret_t rc;
        dbe_bl_nblocks_t nblocks;
        dbe_bl_nblocks_t capacity;
        dbe_info_t info;

        dbe_info_init(info, DBE_INFO_DISKALLOCNOFAILURE);
        rc = SU_SUCCESS;

        nblocks = p_cl->cl_firstnode->cln_header.bl_nblocks;
        capacity = (dbe_bl_nblocks_t)DBE_CLNODE_CAPACITY(p_cl->cl_blocksize);

        if (nblocks == capacity) {
            /* There has to be space for at least extra record;
            ** write the full buffer to disk
            */
            rc = dbe_cl_write1node(p_cl, SU_DADDR_NULL, deferchlist1);
            if (rc != SU_SUCCESS) {
                return (rc);
            }
        }
        /* Check whether we can use the original disk address of
        ** the current block
        */
        if (p_cl->cl_firstnode->cln_daddr != SU_DADDR_NULL
        &&  p_cl->cl_firstnode->cln_header.bl_cpnum == p_cl->cl_nextcpnum
        &&  p_cl->cl_spareblock == SU_DADDR_NULL)
        {
            p_cl->cl_spareblock = p_cl->cl_firstnode->cln_daddr;
            p_cl->cl_firstnode->cln_daddr = SU_DADDR_NULL;
        }
        if (p_cl->cl_spareblock == SU_DADDR_NULL) {
            rc = dbe_fl_alloc_deferch(p_cl->cl_freelist,
                                      &p_cl->cl_spareblock,
                                      deferchlist2,
                                      &info);
        }
        return (rc);
}

/*#***********************************************************************\
 * 
 *		cl_save_nomutex
 * 
 * Saves a changelist object to disk and breaks it.
 * This version assumes the mutex has been entered already.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 *	chlist_start - out
 *		pointer to changelist start disk address
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t cl_save_nomutex(
	dbe_chlist_t *p_cl,
	dbe_cpnum_t next_cpnum,
	su_daddr_t *chlist_start)
{
        su_ret_t rc;
        su_list_t *deferchlist;

        rc = SU_SUCCESS;
        deferchlist = NULL;
        rc = dbe_cl_write1node(p_cl, p_cl->cl_spareblock, &deferchlist);
        if (rc == SU_SUCCESS) {
            p_cl->cl_firstnode->cln_header.bl_cpnum = next_cpnum;
            p_cl->cl_spareblock = SU_DADDR_NULL;
            p_cl->cl_nextcpnum = next_cpnum;
            *chlist_start = p_cl->cl_firstnode->cln_header.bl_next;
            p_cl->cl_firstnode->cln_header.bl_next = SU_DADDR_NULL;
            if (p_cl->cl_lastnode != NULL
            &&  p_cl->cl_lastnode != p_cl->cl_firstnode)
            {
                ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (cl=0x%08lX)\n",
                              __FILE__, __LINE__,
                              (ulong)p_cl->cl_lastnode,
                              (ulong)p_cl));
                clnode_done(p_cl->cl_cache, p_cl->cl_lastnode);
            }
            p_cl->cl_lastnode = p_cl->cl_firstnode;
            ss_dprintf_1(("%s %d: p_cl->cl_lastnode = 0x%08lX\n",
                          __FILE__, __LINE__, (ulong)p_cl->cl_lastnode));
        }
        ss_debug(su_rbt_done(p_cl->cl_addrtree);)
        ss_debug(p_cl->cl_addrtree = su_rbt_init(cl_addrcmp, NULL);)

#if defined(AUTOTEST_RUN) && defined(SS_DEBUG)
        su_rbt_maxnodes(p_cl->cl_addrtree, 80000);
#endif

        if (deferchlist != NULL) {
            ss_error;   /* illegal condition */
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_cl_save
 * 
 * Saves a changelist object to disk and breaks it.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 *	chlist_start - out
 *		pointer to changelist start disk address
 *
 * 
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cl_save(
        dbe_chlist_t *p_cl,
        dbe_cpnum_t next_cpnum,
        su_daddr_t *chlist_start)
{
        su_ret_t rc;
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        rc = cl_save_nomutex(p_cl, next_cpnum, chlist_start);
        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
        return (rc);
}


/*##**********************************************************************\
 * 
 *		dbe_cl_linktogether
 * 
 * Links together two changelist objects.
 * The latter object becomes empty.
 * 
 * Parameters : 
 * 
 *	p_cl_head - in out, use
 *		pointer to 1st list
 *
 *	p_cl_tail - in out, use
 *		pointer to 2nd list
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cl_linktogether(p_cl_head, p_cl_tail)
	dbe_chlist_t *p_cl_head;
	dbe_chlist_t *p_cl_tail;
{
        su_ret_t rc;
        su_daddr_t tail_daddr;
        su_list_t *deferchlist1;
        su_list_t *deferchlist2;

        deferchlist1 = NULL;
        deferchlist2 = NULL;
        rc = SU_SUCCESS;
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl_head->cl_mutex);
        CL_MUTEX_ENTER(p_cl_tail->cl_mutex);
        tail_daddr = p_cl_tail->cl_firstnode->cln_daddr;
        if (tail_daddr == SU_DADDR_NULL) {
            rc = dbe_cl_preparetosave_nomutex(
                    p_cl_tail,
                    &deferchlist1,
                    &deferchlist2);
            if (rc != SU_SUCCESS) {
                goto linktogether_exitpoint;
            }
            rc = cl_save_nomutex(
                    p_cl_tail,
                    p_cl_tail->cl_nextcpnum,
                    &tail_daddr);
            if (rc != SU_SUCCESS) {
                goto linktogether_exitpoint;
            }
            ss_dassert(tail_daddr != SU_DADDR_NULL);
        }
        rc = dbe_cl_linktoend_nomutex(p_cl_head, tail_daddr);
        if (rc != SU_SUCCESS) {
            goto linktogether_exitpoint;
        }
        if (p_cl_tail->cl_lastnode != NULL
        &&  p_cl_tail->cl_lastnode != p_cl_tail->cl_firstnode)
        {
            p_cl_head->cl_lastnode = p_cl_tail->cl_lastnode;
            ss_dprintf_1(("%s %d: p_cl_head->cl_lastnode = 0x%08lX (head=0x%08lX, tail=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_cl_head->cl_lastnode,
                          (ulong)p_cl_head,
                          (ulong)p_cl_tail));
        }
        p_cl_tail->cl_lastnode = p_cl_tail->cl_firstnode;
        ss_dprintf_1(("%s %d: p_cl_tail->cl_lastnode = 0x%08lX\n",
                          __FILE__, __LINE__, (ulong)p_cl_tail->cl_lastnode));
        dbe_blh_init(&p_cl_tail->cl_firstnode->cln_header,
                     (dbe_blocktype_t)DBE_BLOCK_CHANGELIST,
                     p_cl_tail->cl_nextcpnum);
        p_cl_tail->cl_firstnode->cln_daddr = SU_DADDR_NULL;
linktogether_exitpoint:;
        CL_MUTEX_EXIT(p_cl_tail->cl_mutex);
        CL_MUTEX_EXIT(p_cl_head->cl_mutex);
        /***** MUTEXEND *******/
        if (deferchlist1 != NULL) {
            dbe_cl_dochlist(p_cl_head, deferchlist1);
        }
        if (deferchlist2 != NULL) {
            dbe_cl_dochlist(p_cl_head, deferchlist2);
        }
        return (rc);
}

#ifdef NOT_USED

/*##**********************************************************************\
 * 
 *		dbe_cl_linktoend
 * 
 * As linktogether() but takes the second list as disk address.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to change list object
 *
 *	tail_daddr - in
 *		disk address of the second change list
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_cl_linktoend(p_cl, tail_daddr)
	dbe_chlist_t *p_cl;
	su_daddr_t tail_daddr;
{
        su_ret_t rc;

        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        rc = dbe_cl_linktoend_nomutex(p_cl, tail_daddr);
        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
        return (rc);
}

#endif /* NOT_USED */

/*#***********************************************************************\
 * 
 *		dbe_cl_linktoend_nomutex
 * 
 * As linktogether() but takes the second list as disk address.
 * Non-mutexed version for internal use
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to change list object
 *
 *	tail_daddr - in
 *		disk address of the second change list
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t dbe_cl_linktoend_nomutex(p_cl, tail_daddr)
	dbe_chlist_t *p_cl;
	su_daddr_t tail_daddr;
{
        dbe_cpnum_t lastblock_cpnum;
        su_daddr_t lastblock_daddr;
        uint lastblock_nblocks;

        if (tail_daddr == SU_DADDR_NULL) {
            return (SU_SUCCESS);
        }
        if (p_cl->cl_lastnode == NULL) {
            dbe_chlist_iter_t *p_ci;

            p_ci = dbe_ci_init(p_cl);
            while (dbe_ci_nextnode(p_ci))
            {
                /* do nothing */
            }
            dbe_ci_getnodeinfo(p_ci,
                               &lastblock_cpnum,
                               &lastblock_daddr,
                               &lastblock_nblocks);
            dbe_ci_done(p_ci);
        } else {
            lastblock_daddr = p_cl->cl_lastnode->cln_daddr;
        }
        p_cl->cl_lastnode->cln_header.bl_next = tail_daddr;
        if (p_cl->cl_lastnode != p_cl->cl_firstnode) {
            dbe_cacheslot_t *cacheslot;
            void *diskbuf;

            cacheslot = dbe_cache_reach(p_cl->cl_cache,
                                        p_cl->cl_lastnode->cln_daddr,
                                        DBE_CACHE_WRITEONLY,
                                        DBE_INFO_CHECKPOINT,
                                        (char**)&diskbuf,
                                        clst_writectx);
            dbe_blh_put(&p_cl->cl_lastnode->cln_header, diskbuf);
            dbe_cln_putdata(p_cl->cl_lastnode, diskbuf);
            dbe_cache_release(p_cl->cl_cache,
                              cacheslot,
                              DBE_CACHE_DIRTYLASTUSE,
                              clst_writectx);
            ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (cl=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_cl->cl_lastnode,
                          (ulong)p_cl));
            clnode_done(p_cl->cl_cache, p_cl->cl_lastnode);
            p_cl->cl_lastnode = NULL;
        }
        return (SU_SUCCESS);
}
/*##**********************************************************************\
 * 
 *		dbe_cl_setnextcpnum
 * 
 * Sets next checkpoint number.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_cl_setnextcpnum(p_cl, next_cpnum)
	dbe_chlist_t *p_cl;
	dbe_cpnum_t next_cpnum;
{
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        p_cl->cl_nextcpnum = next_cpnum;
        if (p_cl->cl_firstnode->cln_daddr == SU_DADDR_NULL) {
            p_cl->cl_firstnode->cln_header.bl_cpnum = next_cpnum;
        }
        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
}

/*##**********************************************************************\
 * 
 *		dbe_cl_dochlist
 * 
 * Updates the deferred change list to the real one.
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object, NULL means: ignore changes
 *
 *	deferchlist - in, take
 *		pointer to deferred change list
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_cl_dochlist(p_cl, deferchlist)
	dbe_chlist_t *p_cl;
	su_list_t *deferchlist;
{
        su_ret_t rc;

        rc = SU_SUCCESS;
        SS_PUSHNAME("dbe_cl_dochlist");
        if (deferchlist != NULL) {
            su_list_node_t *p_node;
            dbe_clrecord_t *p_clr;

            p_node = su_list_first(deferchlist);
            while (p_node != NULL) {
                p_clr = su_listnode_getdata(p_node);
                ss_dprintf_2(("dbe_cl_dochlist: blkcpnum=%ld,cpnum=%ld,daddr=%ld\n",
                             p_cl->cl_nextcpnum,
                             p_clr->clr_cpnum,
                             p_clr->clr_daddr));
                if (p_cl != NULL) {
                    rc = dbe_cl_add(p_cl,
                                    p_clr->clr_cpnum,
                                    p_clr->clr_daddr);
                }
                su_rc_assert(rc == SU_SUCCESS, rc);
                p_node = su_list_next(deferchlist, p_node);
            }
            su_list_done(deferchlist);
        }
        SS_POPNAME;
}

/*#***********************************************************************\
 * 
 *		dbe_cl_write1node
 * 
 * Writes one (first) changelist node to disk
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	save_daddr - in
 *		disk address where to store the node
 *
 *      p_deferchlist - out, give
 *		pointer^2 to deferred change list object
 * 
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t dbe_cl_write1node(p_cl, save_daddr, p_deferchlist)
	dbe_chlist_t *p_cl;
	su_daddr_t save_daddr;
        su_list_t **p_deferchlist;
{
        su_ret_t rc;
        dbe_cacheslot_t *cacheslot;
        void *diskbuf;
        dbe_info_t info;

        dbe_info_init(info, DBE_INFO_DISKALLOCNOFAILURE);
        rc = SU_SUCCESS;
        *p_deferchlist = NULL;
        if  (p_cl->cl_firstnode->cln_daddr != SU_DADDR_NULL
        &&  p_cl->cl_firstnode->cln_header.bl_cpnum == p_cl->cl_nextcpnum)
        {
            ss_dassert(save_daddr == p_cl->cl_firstnode->cln_daddr
                || save_daddr == SU_DADDR_NULL);
            save_daddr = p_cl->cl_firstnode->cln_daddr;
        } else if (save_daddr == SU_DADDR_NULL) {
            p_cl->cl_firstnode->cln_header.bl_cpnum = p_cl->cl_nextcpnum;
            rc = dbe_fl_alloc_deferch(p_cl->cl_freelist, &save_daddr, p_deferchlist, &info);
            if (rc != SU_SUCCESS) {
                return rc;
            }
        }
        p_cl->cl_firstnode->cln_daddr = save_daddr;
        ss_dprintf_2(("dbe_cl_write1node(): writing chlist block cpnum=%ld daddr=%ld\n",
            p_cl->cl_firstnode->cln_header.bl_cpnum,
            save_daddr));
        cacheslot = dbe_cache_reach(p_cl->cl_cache,
                                    save_daddr,
                                    DBE_CACHE_WRITEONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    clst_writectx);
        dbe_blh_put(&p_cl->cl_firstnode->cln_header, diskbuf);
        dbe_cln_putdata(p_cl->cl_firstnode, diskbuf);
        dbe_cache_release(p_cl->cl_cache,
                          cacheslot,
                          DBE_CACHE_DIRTY,
                          clst_writectx);
        ss_dassert(p_cl->cl_firstnode->cln_header.bl_next != SU_DADDR_NULL
                   || (p_cl->cl_lastnode == NULL
                       || p_cl->cl_lastnode == p_cl->cl_firstnode));
        if (p_cl->cl_firstnode == p_cl->cl_lastnode ||
            (p_cl->cl_lastnode == NULL &&
             p_cl->cl_firstnode->cln_header.bl_next == SU_DADDR_NULL))
        {
            p_cl->cl_lastnode = p_cl->cl_firstnode;
            ss_dprintf_1(("%s %d: p_cl->cl_lastnode = 0x%08lX\n",
                          __FILE__, __LINE__, (ulong)p_cl->cl_lastnode));
            p_cl->cl_firstnode = clnode_init(p_cl->cl_cache);
            ss_dprintf_1(("%s %d: clnode_init gives 0x%08lX (cl=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_cl->cl_lastnode,
                          (ulong)p_cl));
        }
        dbe_blh_init(&p_cl->cl_firstnode->cln_header,
                     (dbe_blocktype_t)DBE_BLOCK_CHANGELIST,
                     p_cl->cl_nextcpnum);
        p_cl->cl_firstnode->cln_header.bl_next = save_daddr;
        p_cl->cl_firstnode->cln_daddr = SU_DADDR_NULL;
        return (rc);
}


/*#***********************************************************************\
 * 
 *		dbe_cl_read1node
 * 
 * Reads one changelist node from disk
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object
 *
 *	p_clnode - in out, use 
 *		pointer to node object where to put read result
 *
 *	daddr - in
 *		disk address of the node to read
 *
 * Return value : SU_SUCCESS if OK or
 *                something else if error
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t dbe_cl_read1node(p_cl, p_clnode, daddr)
	dbe_chlist_t *p_cl;
	dbe_clnode_t *p_clnode;
	su_daddr_t daddr;
{
        su_ret_t rc;
        dbe_cacheslot_t *cacheslot;
        void *diskbuf;

        rc = SU_SUCCESS;
        cacheslot = dbe_cache_reach(p_cl->cl_cache,
                                    daddr,
                                    DBE_CACHE_READONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    clst_readctx);
        dbe_blh_get(&p_clnode->cln_header,
                    diskbuf);
        dbe_cln_getdata(p_clnode, diskbuf);
        dbe_cache_release(p_cl->cl_cache,
                          cacheslot,
                          DBE_CACHE_CLEAN,
                          clst_readctx);
        p_clnode->cln_daddr = daddr;
        return (rc);
}


/*#***********************************************************************\
 * 
 *		dbe_cln_getdata
 * 
 * Gets the data from changelist node disk image to memory image
 * Note: dbe_blh_get() must be called before this to obtain
 * correct nblocks value!
 * 
 * Parameters : 
 * 
 *	p_clnode - in out, use 
 *		pointer to changelist node object
 *
 *	diskbuf - in, use
 *		pointer to disk buffer
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_cln_getdata(p_clnode, diskbuf)
	dbe_clnode_t *p_clnode;
	void *diskbuf;
{
        dbe_bl_nblocks_t i;
        char *p_clr_dbuf;
        dbe_clrecord_t *p_clr;

        p_clr = p_clnode->cln_data;
        p_clr_dbuf = (char*)diskbuf + DBE_BLIST_DATAOFFSET;
        for (i = p_clnode->cln_header.bl_nblocks;
             i > 0;
             i--)
        {
            p_clr->clr_cpnum = SS_UINT4_LOADFROMDISK(p_clr_dbuf);
            p_clr_dbuf += sizeof(p_clr->clr_cpnum);
            p_clr->clr_daddr = SS_UINT4_LOADFROMDISK(p_clr_dbuf);
            p_clr_dbuf += sizeof(p_clr->clr_daddr);
            p_clr++;
        }
}

/*#***********************************************************************\
 * 
 *		dbe_cln_putdata
 * 
 * Puts data from changelist node memory image to disk buffer
 * 
 * Parameters : 
 * 
 *	p_clnode - in, use
 *		pointer to changelist node
 *
 *	diskbuf - out, use
 *		pointer to disk buffer
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_cln_putdata(p_clnode, diskbuf)
	dbe_clnode_t *p_clnode;
	void *diskbuf;
{
        dbe_bl_nblocks_t i;
        char *p_clr_dbuf;
        dbe_clrecord_t *p_clr;

        p_clr = p_clnode->cln_data;
        p_clr_dbuf = (char*)diskbuf + DBE_BLIST_DATAOFFSET;
        for (i = p_clnode->cln_header.bl_nblocks;
             i > 0;
             i--)
        {
            SS_UINT4_STORETODISK(p_clr_dbuf, p_clr->clr_cpnum);
            p_clr_dbuf += sizeof(p_clr->clr_cpnum);
            SS_UINT4_STORETODISK(p_clr_dbuf, p_clr->clr_daddr);
            p_clr_dbuf += sizeof(p_clr->clr_daddr);
            p_clr++;
        }
}

/*##**********************************************************************\
 * 
 *		dbe_ci_init
 * 
 * Creates a changelist iterator object
 * 
 * Parameters : 
 * 
 *	p_cl - in out, use
 *		pointer to changelist object where the iterator operates
 *
 * Return value - give :
 *          pointer to created changelist iterator
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_chlist_iter_t *dbe_ci_init(p_cl)
	dbe_chlist_t *p_cl;
{
        dbe_chlist_iter_t *p_ci;

        p_ci = SSMEM_NEW(dbe_chlist_iter_t);

        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_cl->cl_mutex);
        ss_dassert(p_cl->cl_firstnode != NULL);
        p_ci->ci_chlist = p_cl;
        p_ci->ci_node = p_cl->cl_firstnode;
        p_ci->ci_cacheslot = NULL;
        p_ci->ci_pos = 0;
        CL_MUTEX_EXIT(p_cl->cl_mutex);
        /***** MUTEXEND *******/
        return (p_ci);
}

/*##**********************************************************************\
 * 
 *		dbe_ci_done
 * 
 * Deletes a changelist iterator object
 * 
 * Parameters : 
 * 
 *	p_ci - in, take
 *		pointer to changelist iterator to delete
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_ci_done(p_ci)
	dbe_chlist_iter_t *p_ci;
{
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        if (p_ci->ci_node != p_ci->ci_chlist->cl_firstnode
        &&  p_ci->ci_node != p_ci->ci_chlist->cl_lastnode)
        {
            ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (ci=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_ci->ci_node,
                          (ulong)p_ci));
            clnode_done(p_ci->ci_chlist->cl_cache, p_ci->ci_node);
        }
        if (p_ci->ci_cacheslot != NULL) {
            dbe_cache_release(p_ci->ci_chlist->cl_cache,
                              p_ci->ci_cacheslot,
                              DBE_CACHE_CLEAN,
                              clst_readctx);
        }
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/
        SsMemFree(p_ci);
}

/*##**********************************************************************\
 * 
 *		dbe_ci_getnodeinfo
 * 
 * Gets information about a changelist node which is under iterator
 * 
 * Parameters : 
 * 
 *	p_ci - in, use
 *		pointer to iterator
 * 
 *	p_block_cpnum - out
 *		pointer to variable where checkpoint # is stored
 *
 *	p_block_daddr - out
 *		pointer to disk address variable
 *
 *	p_nblocks - out
 *		pointer to uint where to put # of data items in node
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_ci_getnodeinfo(p_ci, p_block_cpnum, p_block_daddr, p_nblocks)
	dbe_chlist_iter_t *p_ci;
	dbe_cpnum_t *p_block_cpnum;
	su_daddr_t *p_block_daddr;
	uint *p_nblocks;
{
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        ss_dassert(p_ci->ci_node != NULL);
        *p_block_daddr = p_ci->ci_node->cln_daddr;
        *p_block_cpnum = p_ci->ci_node->cln_header.bl_cpnum;
        *p_nblocks = (uint)p_ci->ci_node->cln_header.bl_nblocks;
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/

}

/*##**********************************************************************\
 * 
 *		dbe_ci_nextnode
 * 
 * Advances the iterator to next node
 * 
 * Parameters : 
 * 
 *	p_ci - in out, use
 *		pointer to iterator
 *
 * Return value : TRUE if next was loaded successfully or
 *                FALSE if the current node was last
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
bool dbe_ci_nextnode(p_ci)
	dbe_chlist_iter_t *p_ci;
{
        su_daddr_t next_daddr;

        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        p_ci->ci_pos = 0;
        if (p_ci->ci_node == NULL) {
            CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
            /***** MUTEXEND *******/
            return (FALSE);
        }
        next_daddr = p_ci->ci_node->cln_header.bl_next;
        if (p_ci->ci_cacheslot != NULL) {
            dbe_cache_release(p_ci->ci_chlist->cl_cache,
                              p_ci->ci_cacheslot,
                              DBE_CACHE_CLEAN,
                              clst_readctx);
            p_ci->ci_cacheslot = NULL;
        }
        if (next_daddr == SU_DADDR_NULL) {
            CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
            /***** MUTEXEND *******/
            return (FALSE);
        }
        if (p_ci->ci_chlist->cl_lastnode != NULL &&
            p_ci->ci_chlist->cl_lastnode->cln_daddr == next_daddr)
        {
            p_ci->ci_node = p_ci->ci_chlist->cl_lastnode;
        } else {
            if (p_ci->ci_node == p_ci->ci_chlist->cl_firstnode) {
                p_ci->ci_node = clnode_init(p_ci->ci_chlist->cl_cache);
                ss_dprintf_1(("%s %d: clnode_init gives 0x%08lX (ci=0x%08lX)\n",
                              __FILE__, __LINE__,
                              (ulong)p_ci->ci_node,
                              (ulong)p_ci));
            }
            p_ci->ci_node->cln_daddr = next_daddr;
            p_ci->ci_cacheslot =
                dbe_cache_reach(p_ci->ci_chlist->cl_cache,
                                next_daddr,
                                DBE_CACHE_READONLY,
                                DBE_INFO_CHECKPOINT,
                                (char**)&p_ci->ci_diskbuf,
                                clst_readctx);
            dbe_blh_get(&p_ci->ci_node->cln_header, p_ci->ci_diskbuf);
            if (p_ci->ci_node->cln_header.bl_next == SU_DADDR_NULL) {
                ss_dassert(p_ci->ci_chlist->cl_lastnode == NULL ||
                           p_ci->ci_chlist->cl_lastnode == p_ci->ci_chlist->cl_firstnode);
                p_ci->ci_chlist->cl_lastnode = p_ci->ci_node;
                ss_dprintf_1(("%s %d: p_ci->ci_chlist->cl_lastnode = 0x%08lX (chlist=0x%08lX\n",
                              __FILE__, __LINE__,
                              (ulong)p_ci->ci_chlist->cl_lastnode,
                              (ulong)p_ci->ci_chlist));
                dbe_cln_getdata(p_ci->ci_node, p_ci->ci_diskbuf);
                dbe_cache_release(p_ci->ci_chlist->cl_cache,
                                  p_ci->ci_cacheslot,
                                  DBE_CACHE_CLEAN,
                                  clst_readctx);
                p_ci->ci_cacheslot = NULL;
            }
        }
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		dbe_ci_getnext
 * 
 * Gets next unread data item from the node under iterator cursor.
 * 
 * Parameters : 
 * 
 *	p_ci - in out, use
 *		pointer to change list iterator
 *
 *	p_cpnum - out
 *		pointer to where the checkpoint # of record is stored
 *
 *	p_daddr - out
 *		pointer to the disk address       -"-
 *
 * 
 * Return value : TRUE if an unread item existed or
 *                FALSE if at end of node
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
bool dbe_ci_getnext(p_ci, p_cpnum, p_daddr)
	dbe_chlist_iter_t *p_ci;
	dbe_cpnum_t *p_cpnum;
	su_daddr_t *p_daddr;
{
        dbe_clrecord_t *p_clr;

        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        if (p_ci->ci_pos >= p_ci->ci_node->cln_header.bl_nblocks) {
            CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
            /***** MUTEXEND *******/
            return (FALSE);
        }
        if (p_ci->ci_cacheslot != NULL) {
            dbe_cln_getdata(p_ci->ci_node, p_ci->ci_diskbuf);
            dbe_cache_release(p_ci->ci_chlist->cl_cache,
                              p_ci->ci_cacheslot,
                              DBE_CACHE_CLEAN,
                              clst_readctx);
            p_ci->ci_cacheslot = NULL;
        }
        p_clr = p_ci->ci_node->cln_data + p_ci->ci_pos;
        p_ci->ci_pos++;
        *p_cpnum = p_clr->clr_cpnum;
        *p_daddr = p_clr->clr_daddr;
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/
        return (TRUE);
}

#ifdef NOT_USED

/*##**********************************************************************\
 * 
 *		dbe_ci_reset
 * 
 * Resets the iterator read pointer to start of node
 * 
 * Parameters : 
 * 
 *	p_ci - in out, use
 *		pointer to chlist iterator
 *
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_ci_reset(p_ci)
	dbe_chlist_iter_t *p_ci;
{
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        p_ci->ci_pos = 0;
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/
}

/*##**********************************************************************\
 * 
 *		dbe_ci_resetnode
 * 
 * Resets the iterator to start of chlist
 * 
 * Parameters : 
 * 
 *	p_ci - in out, use
 *		pointer to chlist iterator
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_ci_resetnode(p_ci)
	dbe_chlist_iter_t *p_ci;
{
        /***** MUTEXBEGIN *****/
        CL_MUTEX_ENTER(p_ci->ci_chlist->cl_mutex);
        ss_dassert(p_ci->ci_chlist->cl_firstnode != NULL);
        if (p_ci->ci_cacheslot != NULL) {
            dbe_cache_release(p_ci->ci_chlist->cl_cache,
                              p_ci->ci_cacheslot,
                              DBE_CACHE_CLEAN,
                              clst_readctx);
            p_ci->ci_cacheslot = NULL;
        }
        if (p_ci->ci_node != p_ci->ci_chlist->cl_firstnode
        &&  p_ci->ci_node != p_ci->ci_chlist->cl_lastnode)
        {
            ss_dprintf_1(("%s %d:clnode_done called for 0x%08lX (ci=0x%08lX)\n",
                          __FILE__, __LINE__,
                          (ulong)p_ci->ci_node,
                          (ulong)p_ci));
            clnode_done(p_ci->ci_chlist->cl_cache, p_ci->ci_node);
        }
        p_ci->ci_node = p_ci->ci_chlist->cl_firstnode;
        p_ci->ci_pos = 0;
        CL_MUTEX_EXIT(p_ci->ci_chlist->cl_mutex);
        /***** MUTEXEND *******/
}

#endif /* NOT_USED */
