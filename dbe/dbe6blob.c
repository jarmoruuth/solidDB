/*************************************************************************\
**  source       * dbe6blob.c
**  directory    * dbe
**  description  * File stream BLOB support
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

BLOB (Binary Large Object) are represented using a chain of disk blocks.
The most natural solution would be a simple linked list of disk blocks.
But it is inefficient in presence of multithreaded database engine that has
a separate prefetch thread. That is why the BLOBs consist of two
kinds of blocks: list blocks and data blocks. Each list block contains
several data block addresses plus the address of the next list pointer plus
some data. A data block only contains data. If the blob size is known
in advance the list blocks can be optimized to contain exactly the required
amount of data block addresses. When the blob size is not known before
the writing of the BLOB, the list blocks are allocated in such a manner
that they use a certain part of the list block for data block address array
and most of the size is reserved for data. This default is generally a
good compromize between space efficiency and I/O performance.

Limitations:
-----------

The size field in the BLOB list block is 4 bytes which sets an upper
limit for the BLOB size to approximately 4 GB.

Error handling:
--------------

Should be improved.

Objects used:
------------

File descriptor dbe_filedes_t from dbe6finf.h
Counter dbe_counter_t from dbe7ctr.h


Preconditions:
-------------


The file descriptor and counter objects must be initialized before using
the BLOB methods.

Multithread considerations:
--------------------------

The code is re-entrant, but shared access to same object is not protected.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>

#include <ssstring.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssmemtrc.h>
#include <ssdebug.h>
#include <sssem.h>

#include <su0icvt.h>

#include "dbe9bhdr.h"
#include "dbe6blob.h"

#ifndef SS_NOBLOB

#ifdef SS_DEBUG
static char blob_reachctx[] = "BLOB reach";
#else
#define blob_reachctx NULL
#endif

typedef struct dbe_blob_st dbe_blob_t;

/* Structure for representing a BLOB
 * file block in memory (either data or list block) 
 */
typedef struct dbe_blobblock_st {
        dbe_blocktype_t     blb_type;   /* DBE_BLOCK_BLOB(DATA|LIST) */
        dbe_cpnum_t         blb_cpnum;  /* Checkpoint number */
        dbe_blobsize_t      blb_size;   /* Blob size from this downward */
        su_daddr_t          blb_next;   /* Disk pointer to next list block */
        dbe_bl_nblocks_t    blb_nblocks;/* # of address positions in list
                                         * block or # of bytes in data block
                                         */
        dbe_bl_nblocks_t    blb_nblocks_used;
                                        /* # of used addr pos. in
                                         * list block
                                         */
        dbe_blobid_t        blb_id;      /* Blob ID (number) */
        dbe_cacheslot_t*    blb_cacheslot; /* reached cache slot or NULL */
        char*               blb_data;   /* Data of cache slot or NULL */
        su_daddr_t          blb_daddr;  /* disk address */
} dbe_blobblock_t;


/* Constants & Macros for Blob List Block */
#define BLB_NEXTOFFSET \
        (DBE_BLOCKCPNUMOFFSET + sizeof(dbe_cpnum_t) + 1)    /* == 6 */
#define BLB_NBLOCKSOFFSET \
        (BLB_NEXTOFFSET + sizeof(su_daddr_t))               /* == 10 */
#define BLB_NBLOCKSUSEDOFFSET \
        (BLB_NBLOCKSOFFSET + sizeof(dbe_bl_nblocks_t))      /* == 12 */
#define BLB_SIZEOFFSET \
        (BLB_NBLOCKSUSEDOFFSET + sizeof(dbe_bl_nblocks_t))  /* == 14 */
#define BLB_BLOBIDOFFSET \
        (BLB_SIZEOFFSET + sizeof(dbe_blobsize_t))           /* == 18 */
#define BLB_ALLOCLSTOFFSET \
        (BLB_BLOBIDOFFSET + sizeof(dbe_blobid_t) + 2)       /* == 24 */
#define BLB_DATAOFFSET(block) \
        (BLB_ALLOCLSTOFFSET + sizeof(su_daddr_t) * (block)->blb_nblocks)
#define BLB_BLOCKCAPACITY(blocksize) \
        (((blocksize) - BLB_ALLOCLSTOFFSET - 1) / sizeof(su_daddr_t))
#define BLB_DATACAPACITY(blocksize) \
        ((blocksize) - BLB_ALLOCLSTOFFSET)
#define BLB_DEFNBLOCKS(blocksize) \
        SS_MIN(BLB_BLOCKCAPACITY(blocksize)/16, 16)


/* Constants & macros for Blob Data Block */
#define BDB_DATASIZEOFFSET \
        (DBE_BLOCKCPNUMOFFSET + sizeof(dbe_cpnum_t) + 1)    /* == 6 */
#define BDB_DATAOFFSET \
        (BDB_DATASIZEOFFSET + sizeof(dbe_bl_nblocks_t))     /* == 8 */
#define BDB_DATACAPACITY(blocksize) \
        ((blocksize) - BDB_DATAOFFSET)

/* Blob access modes */
typedef enum {
        BMODE_READ,
        BMODE_WRITE,
        BMODE_COPY
} blobaccessmode_t;

/* Blob states */
typedef enum {
        BSTATE_REACHED,
        BSTATE_RELEASED,
        BSTATE_CLOSED
} blobstate_t;

/* This value indicates the current read/write position
 * points to data in list block itself (must be -1 !)
 */
#define B_CURLISTPOS_SELF ((dbe_bl_nblocks_t)-1)

/* BLOB object data structure */
struct dbe_blob_st {
        blobaccessmode_t        b_mode;         /* BMODE_(READ|WRITE|COPY) */
        blobstate_t             b_state;        /* BSTATE_(REACHED|RELEASED) */
        dbe_blobid_t            b_id;           /* unique blob identifier */
        dbe_iomgr_t*            b_iomgr;        /* I/O manager object */
        dbe_filedes_t*          b_filedes;      /* file descriptor */
        dbe_counter_t*          b_counter;      /* counter */
        su_daddr_t              b_startaddr;    /* BLOB start position */
        dbe_blobblock_t*        b_startblock;   /* 1st block */
        dbe_blobblock_t*        b_curlistblock; /* curr. list block */
        dbe_blobblock_t*        b_curdatablock; /* curr. data block */
        dbe_bl_nblocks_t        b_curlistpos;   /* pos. in addr list */
        size_t                  b_curdatapos;   /* byte position in databuf */
        dbe_blobsize_t          b_curtotalpos;  /* total byte pos */
        dbe_blobsize_t          b_size;         /* blob size */
        dbe_log_t*              b_log;          /* ptr to log or NULL */
        bool                    b_logdataflag;  /* log the data of the blob */
        dbe_trxid_t             b_trxid;        /* transaction that uses this */
        
        int                     b_prefetchsize;
        int                     b_prefetchnext;
        rs_sysi_t*              b_cd;
};

#define bb_gettype(block) ((block)->blb_type)

long dbe_blob_nblock;

#ifdef SS_DEBUG

#include <su0rbtr.h>

typedef struct {
        char*        ba_file;
        int          ba_line;
        su_daddr_t   ba_daddr;
        dbe_blobid_t ba_blob_id;
        char**       ba_callstack;
} block_alloc_t;

static su_rbt_t* blob_allocrbt;
static SsSemT*   blob_allocsem;

bool dbe_blob_debug;

static int ba_insert_compare(void* key1, void* key2)
{
        block_alloc_t* ba1 = key1;
        block_alloc_t* ba2 = key2;

        return(su_rbt_long_compare(ba1->ba_daddr, ba2->ba_daddr));
}

static int ba_search_compare(void* search_key, void* rbt_key)
{
        su_daddr_t daddr = (su_daddr_t)search_key;
        block_alloc_t* ba = rbt_key;

        return(su_rbt_long_compare(daddr, ba->ba_daddr));
}

static void ba_delete(void* key)
{
        block_alloc_t* ba = key;
        
        SsMemTrcFreeCallStk(ba->ba_callstack);
        SsMemFree(ba);
}

static void blob_allocblock(const char* file, int line, su_daddr_t daddr, dbe_blobid_t blob_id)
{
        char** callstack;
        block_alloc_t* ba;

        dbe_blob_nblock++;

        if (!dbe_blob_debug) {
            return;
        }

        callstack = SsMemTrcCopyCallStk();

        if (blob_allocsem == NULL) {
            blob_allocsem = SsSemCreateLocal(SS_SEMNUM_DBE_BLOBDEBUG);
        }

        SsSemEnter(blob_allocsem);

        if (blob_allocrbt == NULL) {
            blob_allocrbt = su_rbt_inittwocmp(
                                ba_insert_compare,
                                ba_search_compare,
                                ba_delete);
        }

        ba = SSMEM_NEW(block_alloc_t);

        ba->ba_file = (char *)file;
        ba->ba_line = line;
        ba->ba_daddr = daddr;
        ba->ba_blob_id = blob_id;
        ba->ba_callstack = callstack;

        su_rbt_insert(blob_allocrbt, ba);

        ss_dprintf_3(("BLOB_COUNTALLOC:%-12s %-5d %ld, blob_id %ld\n",
            ba->ba_file, ba->ba_line, (long)ba->ba_daddr,
            (long)ba->ba_blob_id));

        SsSemExit(blob_allocsem);
}

static void blob_freeblock(su_daddr_t daddr)
{
        su_rbt_node_t* n;

        dbe_blob_nblock--;

        if (!dbe_blob_debug) {
            return;
        }

        SsSemEnter(blob_allocsem);

        if (blob_allocrbt != NULL) {
            n = su_rbt_search(blob_allocrbt, (void*)daddr);
            if (n != NULL) {
                ss_dprintf_3(("BLOB_COUNTFREE:daddr %ld\n", (long)daddr));
                su_rbt_delete(blob_allocrbt, n);
            } else {
                ss_dprintf_3(("BLOB_COUNTFREE:daddr %ld NOT FOUND\n", (long)daddr));
            }
        }

        SsSemExit(blob_allocsem);
}

void dbe_blob_printalloclist(void)
{
        SsSemEnter(blob_allocsem);

        if (blob_allocrbt == NULL) {
            SsPrintf("Empty block alloc list\n");
        } else {
            int count = 0;
            su_rbt_node_t* n;
            block_alloc_t* ba;

            n = su_rbt_min(blob_allocrbt, NULL);
            while (n != NULL) {
                ba = su_rbtnode_getkey(n);
                SsPrintf("%-12s %-5d %ld, blob_id %ld\n",
                    ba->ba_file, ba->ba_line, (long)ba->ba_daddr,
                    (long)ba->ba_blob_id);
                SsMemTrcPrintCallStk(ba->ba_callstack);
                n = su_rbt_succ(blob_allocrbt, n);
                count++;
            }
            SsPrintf("Total of %d blob allocation blocks\n", count);
        }

        SsSemExit(blob_allocsem);
}

#define BLOB_COUNTALLOC(addr, blob_id)   blob_allocblock(__FILE__, __LINE__, addr, blob_id)
#define BLOB_COUNTFREE(addr)    blob_freeblock(addr)

#else  /* SS_DEBUG */

#define BLOB_COUNTALLOC(addr, blob_id)   dbe_blob_nblock++
#define BLOB_COUNTFREE(addr)    dbe_blob_nblock--

#endif /* SS_DEBUG */

#ifdef SS_FAKE

static su_rbt_t* blob_debugpool;

static int blob_compare_blobid(void* id1, void* id2)
{
        if ((ulong)id1 < (ulong)id2) {
            return (-1);
        }
        if ((ulong)id1 > (ulong)id2) {
            return (1);
        }
        return (0);
}

static void blob_debugpool_initif(void)
{
        if (blob_debugpool != NULL) {
            return;
        }
        blob_debugpool = su_rbt_init(blob_compare_blobid, (void(*)(void*))NULL);
}

void dbe_blob_debugpool_add(dbe_blobid_t blobid)
{
        blob_debugpool_initif();
        su_rbt_insert(blob_debugpool, (void*)blobid);
}

void dbe_blob_debugpool_remove(dbe_blobid_t blobid)
{
        su_rbt_node_t* rbtn;
        blob_debugpool_initif();
        rbtn = su_rbt_search(blob_debugpool, (void*)blobid);
        if (rbtn != NULL) {
            ss_dassert((dbe_blobid_t)su_rbtnode_getkey(rbtn) == blobid);
            su_rbt_delete(blob_debugpool, rbtn);
        }
}

void dbe_blob_debugpool_print(void)
{
        su_rbt_node_t* rbtn;

        if (blob_debugpool == NULL) {
            return;
        }
        rbtn = su_rbt_min(blob_debugpool, NULL);
        if (rbtn != NULL) {
            SsDbgPrintf("List of allocated BLOB Ids:\n");
            for (;
                 rbtn != NULL;
                 rbtn = su_rbt_succ(blob_debugpool, rbtn))
            {
                dbe_blobid_t blobid = (dbe_blobid_t)su_rbtnode_getkey(rbtn);
                SsDbgPrintf("%lu\n", (ulong)blobid);
            }
        }
        su_rbt_done(blob_debugpool);
        blob_debugpool = NULL;
}

#endif /* SS_FAKE */
/*#***********************************************************************\
 * 
 *		blb_reachforwrite
 * 
 * 
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob list block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	blobsize - in
 *		size of the blob from this block downward
 *          (or DBE_BLOBSIZE_UNKNOWN if not known)
 *		
 *	daddr - in
 *		disk address of the block or SU_DADDR_NULL to make automatic
 *          allocation
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable, where the address of the write
 *          buffer will be stored
 *		
 *	p_bufsize - out
 *		pointer to variable where the size of the write buffer is stored
 *		
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t blb_reachforwrite(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_blobsize_t blobsize,
        su_daddr_t daddr,
        char** p_buf,
        size_t* p_bufsize,
        dbe_blobid_t blob_id __attribute__ ((unused)))
{
        uint i;
        char* p;
        ulong n_datablocks;
        su_ret_t rc;
        dbe_bl_nblocks_t bdb_datacapacity;
        dbe_bl_nblocks_t blb_blockcapacity;
        dbe_bl_nblocks_t blb_datacapacity;
        dbe_info_t info;

        dbe_info_init(info, 0);
        ss_dprintf_3(("dbe_ret_t blb_reachforwrite() blobsize = %ld\n", blobsize));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);
        ss_dassert(p_buf != NULL);
        ss_dassert(p_bufsize != NULL);
        ss_dassert(block->blb_cacheslot == NULL);
        ss_dassert(block->blb_type == DBE_BLOCK_BLOBLIST);

        blb_datacapacity = (dbe_bl_nblocks_t)BLB_DATACAPACITY(filedes->fd_blocksize);
        blb_blockcapacity = (dbe_bl_nblocks_t)BLB_BLOCKCAPACITY(filedes->fd_blocksize);
        bdb_datacapacity = (dbe_bl_nblocks_t)BDB_DATACAPACITY(filedes->fd_blocksize);

        block->blb_size = blobsize;
        block->blb_nblocks_used = 0;
        if (daddr == SU_DADDR_NULL) {   /* allocation requested */
            FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
            } else {
                SS_PUSHNAME("blb_reachforwrite:dbe_fl_alloc");
                rc = dbe_fl_alloc(filedes->fd_freelist, &daddr, &info);
                BLOB_COUNTALLOC(daddr, blob_id);
                SS_POPNAME;
            }
            if (rc != SU_SUCCESS) {
                return (rc);
            }
        }
        block->blb_daddr = daddr;
        block->blb_cacheslot =
            dbe_iomgr_reach(
                iomgr,
                daddr,
                DBE_CACHE_WRITEONLY,
                0,
                (char**)&block->blb_data,
                blob_reachctx);
        if (blobsize != DBE_BLOBSIZE_UNKNOWN) {
            /* Blob size is known
             */
            n_datablocks = (blobsize + bdb_datacapacity - 1) /
                bdb_datacapacity;
            if (n_datablocks == 1) {
                if ((dbe_blobsize_t)blb_datacapacity >= blobsize) {
                    /* all data fits into this block
                     */
                    n_datablocks = 0;
                }
                block->blb_nblocks = (dbe_bl_nblocks_t)n_datablocks;
            } else if (n_datablocks <= blb_blockcapacity) {
                /* all data block addresses fit into this block */
                if ((n_datablocks - 1) * bdb_datacapacity +
                    (blb_datacapacity -
                     (n_datablocks - 1) * sizeof(su_daddr_t)) >= blobsize)
                {
                    n_datablocks--;
                }
                block->blb_nblocks = (dbe_bl_nblocks_t)n_datablocks;
            } else {    
                /* more data blocks addresses are needed than this block
                ** can store alone
                */
                if ((n_datablocks - 1) * bdb_datacapacity +
                    (blb_datacapacity -
                     blb_blockcapacity * sizeof(su_daddr_t)) >= blobsize)
                {   /* we can save 1 data block by this trick */
                    n_datablocks--;
                }
                block->blb_nblocks = blb_blockcapacity;
            }
            p = block->blb_data + BLB_ALLOCLSTOFFSET;
            /* preallocate all required data blocks in order to optimize
            ** the clustering of the data.
            */
            for (i = 0; 
                 i < block->blb_nblocks;
                 i++, p += sizeof(su_daddr_t))
            {
                FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                    FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
                } else {
                    rc = dbe_fl_alloc(filedes->fd_freelist, &daddr, &info);
                    BLOB_COUNTALLOC(daddr, blob_id);
                }
                if (rc != SU_SUCCESS) {
                    su_ret_t rc2;

                    while (i > 0) {
                        p -= sizeof(su_daddr_t);
                        i--;
                        daddr = SS_UINT4_LOADFROMDISK(p);
                        rc2 = dbe_fl_free(filedes->fd_freelist, daddr);
                        su_rc_assert(rc2 == SU_SUCCESS, rc2);
                        BLOB_COUNTFREE(daddr);
                        daddr = SU_DADDR_NULL;
                        SS_UINT4_STORETODISK(p, daddr);
                    }
                    daddr = block->blb_daddr;
                    dbe_iomgr_release(
                        iomgr,
                        block->blb_cacheslot,
                        DBE_CACHE_IGNORE,
                        blob_reachctx);
                    rc2 = dbe_fl_free(
                            filedes->fd_freelist,
                            daddr);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                    BLOB_COUNTFREE(daddr);
                    block->blb_daddr = SU_DADDR_NULL;
                    block->blb_cacheslot = NULL;
                    block->blb_data = NULL;
                    return (rc);
                }
                SS_UINT4_STORETODISK(p, daddr);
            }
            if (n_datablocks > blb_blockcapacity) {
                /* preallocate also the next list
                 * (or maybe data) block
                 */
                FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                    FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
                } else {
                    rc = dbe_fl_alloc(filedes->fd_freelist, &daddr, &info);
                    BLOB_COUNTALLOC(daddr, blob_id);
                }
                if (rc != SU_SUCCESS) {
                    su_ret_t rc2;

                    while (i > 0) {
                        p -= sizeof(su_daddr_t);
                        i--;
                        daddr = SS_UINT4_LOADFROMDISK(p);
                        rc2 = dbe_fl_free(filedes->fd_freelist, daddr);
                        su_rc_assert(rc2 == SU_SUCCESS, rc2);
                        BLOB_COUNTFREE(daddr);
                        daddr = SU_DADDR_NULL;
                        SS_UINT4_STORETODISK(p, daddr);
                    }
                    dbe_iomgr_release(
                        iomgr,
                        block->blb_cacheslot,
                        DBE_CACHE_IGNORE,
                        blob_reachctx);
                    rc2 = dbe_fl_free(
                            filedes->fd_freelist,
                            block->blb_daddr);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                    BLOB_COUNTFREE(block->blb_daddr);
                    block->blb_daddr = SU_DADDR_NULL;
                    block->blb_cacheslot = NULL;
                    block->blb_data = NULL;
                    return (rc);
                }
                block->blb_next = daddr;
            } else {
                /* no next block is needed, because all necessary
                 * data block addresses fit into this block
                 */
                block->blb_next = SU_DADDR_NULL;
            }
        } else {
            /* Blob size is unknown;
            ** we use a default address list size for data blocks
            */
            n_datablocks = BLB_DEFNBLOCKS(filedes->fd_blocksize);
            ss_dassert(n_datablocks < BLB_BLOCKCAPACITY(filedes->fd_blocksize));
            p = block->blb_data + BLB_ALLOCLSTOFFSET;
            block->blb_nblocks = (dbe_bl_nblocks_t)n_datablocks;
            block->blb_next = SU_DADDR_NULL;
            daddr = SU_DADDR_NULL;
            for (i = 0;
                 i < block->blb_nblocks;
                 i++, p += sizeof(su_daddr_t))
            {
                SS_UINT4_STORETODISK(p, daddr);
            }
        }
        *p_buf = p;
        *p_bufsize = filedes->fd_blocksize - (p - block->blb_data);
        return (DBE_RC_SUCC);
}

/*#***********************************************************************\
 * 
 *		bb_releasefromwrite
 * 
 * Releases Blob Block (list or data) from write reach
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	cpnum - in
 *		checkpoint number
 *		
 *	byteswritten - in
 *		number of bytes written
 *
 *      flushflag - in
 *          TRUE when the cache slot needs to be released in flush mode
 *          FALSE when regular lazy-write release is sufficient
 *		
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void bb_releasefromwrite(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_cpnum_t cpnum,
        size_t byteswritten,
        bool flushflag)
{
        char* p;
        dbe_cache_releasemode_t cache_releasemode = 0;

        ss_debug(dbe_bl_nblocks_t bdb_datacapacity);
        ss_debug(dbe_bl_nblocks_t blb_blockcapacity);
        ss_debug(dbe_bl_nblocks_t blb_datacapacity);

        ss_dprintf_3(("bb_releasefromwrite()\n"));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);
        if (block->blb_cacheslot == NULL) {
            return;
        }
        ss_dassert(block->blb_data != NULL);
        block->blb_cpnum = cpnum;
        DBE_BLOCK_SETTYPE(block->blb_data, &block->blb_type);
        ss_dassert((dbe_blocktype_t)*(block->blb_data) == block->blb_type);
        DBE_BLOCK_SETCPNUM(block->blb_data, &cpnum);
        switch (block->blb_type) {
            case DBE_BLOCK_BLOBLIST:
                ss_dprintf_4(("bb_releasefromwrite(): List block\n"));
                ss_rc_dassert(
                    (dbe_blocktype_t)*(block->blb_data) == DBE_BLOCK_BLOBLIST,
                    (int)(dbe_blocktype_t)*(block->blb_data));
                ss_debug(blb_datacapacity =
                    (dbe_bl_nblocks_t)BLB_DATACAPACITY(filedes->fd_blocksize));
                ss_debug(blb_blockcapacity =
                    (dbe_bl_nblocks_t)BLB_BLOCKCAPACITY(filedes->fd_blocksize));
                ss_dassert(block->blb_nblocks <= blb_blockcapacity);
                if (block->blb_nblocks_used == 0) {
                    block->blb_size = (dbe_blobsize_t)byteswritten;
                } else {
                    ss_dassert(byteswritten == 
                        blb_datacapacity - sizeof(su_daddr_t) * block->blb_nblocks);
                }
                p = block->blb_data + BLB_NEXTOFFSET;
                SS_UINT4_STORETODISK(p, block->blb_next);
                p = block->blb_data + BLB_NBLOCKSOFFSET;
                SS_UINT2_STORETODISK(p, block->blb_nblocks);
                p = block->blb_data + BLB_NBLOCKSUSEDOFFSET;
                SS_UINT2_STORETODISK(p, block->blb_nblocks_used);
                p = block->blb_data + BLB_SIZEOFFSET;
                SS_UINT4_STORETODISK(p, block->blb_size);
                p = block->blb_data + BLB_BLOBIDOFFSET;
                SS_UINT4_STORETODISK(p, block->blb_id);
                cache_releasemode = flushflag ?
                    DBE_CACHE_FLUSH : DBE_CACHE_DIRTY;
                break;
            case DBE_BLOCK_BLOBDATA:
                ss_dprintf_4(("bb_releasefromwrite(): Data block\n"));
                ss_rc_dassert(
                    (dbe_blocktype_t)*(block->blb_data) == DBE_BLOCK_BLOBDATA,
                    (int)(dbe_blocktype_t)*(block->blb_data));
                ss_debug(bdb_datacapacity =
                    (dbe_bl_nblocks_t)BDB_DATACAPACITY(filedes->fd_blocksize));
                block->blb_nblocks = (dbe_bl_nblocks_t)byteswritten;
                ss_dassert(byteswritten <= bdb_datacapacity);
                p = block->blb_data + BDB_DATASIZEOFFSET;
                SS_UINT2_STORETODISK(p, block->blb_nblocks);
                cache_releasemode = flushflag ?
                    DBE_CACHE_FLUSHLASTUSE : DBE_CACHE_DIRTYLASTUSE;
                break;
            default:
                ss_rc_error(block->blb_type);
        }
        ss_dassert((dbe_blocktype_t)*(block->blb_data) == block->blb_type);
        dbe_iomgr_release(
            iomgr,
            block->blb_cacheslot,
            cache_releasemode,
            blob_reachctx);
        block->blb_cacheslot = NULL;
        block->blb_data = NULL;
}


/*#***********************************************************************\
 * 
 *		bb_reachforreadorupdate
 * 
 * Reaches a Blob Block for read or update mode access
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	daddr - in
 *		disk address of the block
 *		
 *	reachmode - in
 *          DBE_CACHE_READONLY or DBE_CACHE_READWRITE
 *		
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where the address of the
 *          read buffer will be stored
 *		
 *	p_bufsize - out
 *		pointer to variable where the size of the read buffer
 *          will be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code on failure
 *	reachmode - 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static dbe_ret_t bb_reachforreadorupdate(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,
        dbe_cache_reachmode_t reachmode,
        char** p_buf,
        size_t* p_bufsize)
{
        dbe_ret_t rc;
        char* p;
        dbe_bl_nblocks_t blb_datacapacity;

        ss_ct_assert(sizeof(dbe_bl_nblocks_t) == 2);
        ss_dprintf_3(("bb_reachforreadorupdate\n"));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);
        ss_dassert(p_buf != NULL);
        ss_dassert(p_bufsize != NULL);
        rc = DBE_RC_SUCC;
        if (block->blb_cacheslot == NULL) {
            block->blb_daddr = daddr;
            block->blb_cacheslot =
                dbe_iomgr_reach(
                    iomgr,
                    daddr,
                    reachmode,
                    0,
                    &block->blb_data,
                    blob_reachctx);
        } else {
            ss_dassert(block->blb_daddr == daddr);
        }
        DBE_BLOCK_GETTYPE(block->blb_data, &block->blb_type);
        DBE_BLOCK_GETCPNUM(block->blb_data, &block->blb_cpnum);
        switch (block->blb_type) {
            case DBE_BLOCK_BLOBLIST:
                p = block->blb_data + BLB_NEXTOFFSET;
                block->blb_next = SS_UINT4_LOADFROMDISK(p);
                p = block->blb_data + BLB_NBLOCKSOFFSET;
                block->blb_nblocks = SS_UINT2_LOADFROMDISK(p);
                p = block->blb_data + BLB_NBLOCKSUSEDOFFSET;
                block->blb_nblocks_used = SS_UINT2_LOADFROMDISK(p);

                p = block->blb_data + BLB_SIZEOFFSET;
                block->blb_size = SS_UINT4_LOADFROMDISK(p);
                
                p = block->blb_data + BLB_BLOBIDOFFSET;
                block->blb_id = SS_UINT4_LOADFROMDISK(p);

                blb_datacapacity =
                    (dbe_bl_nblocks_t)BLB_DATACAPACITY(filedes->fd_blocksize);
                if (block->blb_size <=
                    blb_datacapacity -
                        sizeof(su_daddr_t) * block->blb_nblocks)
                {
                    ss_dassert(block->blb_nblocks_used == 0);
                    *p_bufsize = (size_t)block->blb_size;
                } else {
                    *p_bufsize = (size_t)(blb_datacapacity -
                        sizeof(su_daddr_t) * block->blb_nblocks);
                }
                *p_buf = block->blb_data + BLB_DATAOFFSET(block);
                break;
            case DBE_BLOCK_BLOBDATA:
                p = block->blb_data + BDB_DATASIZEOFFSET;
                block->blb_nblocks = SS_UINT2_LOADFROMDISK(p);
                *p_bufsize = (size_t)block->blb_nblocks;
                *p_buf = block->blb_data + BDB_DATAOFFSET;
                break;
            default:
                ss_rc_error(block->blb_type);
        }
        return (rc);
}

/*#***********************************************************************\
 * 
 *		bb_reachforread
 * 
 * Reaches a Blob Block to read mode access
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	daddr - in
 *		disk address of the block
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where the address of the
 *          read buffer will be stored
 *		
 *	p_bufsize - out
 *		pointer to variable where the size of the read buffer
 *          will be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t bb_reachforread(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,
        char** p_buf,
        size_t* p_bufsize)
{
        dbe_ret_t rc;

        ss_dprintf_3(("bb_reachforread\n"));
        rc = bb_reachforreadorupdate(
                block,
                iomgr,
                filedes,
                daddr,
                DBE_CACHE_READONLY,
                p_buf,
                p_bufsize);
        return (rc);
}


/*#***********************************************************************\
 * 
 *		bb_reachforupdate
 * 
 * Reaches a Blob Block to update mode access
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	daddr - in
 *		disk address of the block
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where the address of the
 *          read buffer will be stored
 *		
 *	p_bufsize - out
 *		pointer to variable where the size of the read buffer
 *          will be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t bb_reachforupdate(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,
        char** p_buf,
        size_t* p_bufsize)
{
        dbe_ret_t rc;

        ss_dprintf_3(("bb_reachforupdate\n"));
        rc = bb_reachforreadorupdate(
                block,
                iomgr,
                filedes,
                daddr,
                DBE_CACHE_READWRITE,
                p_buf,
                p_bufsize);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		bb_releasefromread
 * 
 * Releases a blob block from read mode reach
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void bb_releasefromread(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes)
{
        dbe_cache_releasemode_t cache_releasemode = 0;
        ss_dprintf_3(("bb_releasefromread\n"));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);

        if (block->blb_cacheslot == NULL) {
            return;
        }
        switch (block->blb_type) {
            case DBE_BLOCK_BLOBLIST:
                ss_dprintf_4(("bb_releasefromread(): List block\n"));
                cache_releasemode = DBE_CACHE_CLEAN;
                break;
            case DBE_BLOCK_BLOBDATA:
                ss_dprintf_4(("bb_releasefromread(): Data block\n"));
                cache_releasemode = DBE_CACHE_CLEANLASTUSE;
                break;
            default:
                ss_rc_error((int)block->blb_type);
                break; /* Not reached */
        }
        dbe_iomgr_release(
            iomgr,
            block->blb_cacheslot,
            cache_releasemode,
            blob_reachctx);
        block->blb_cacheslot = NULL;
        block->blb_data = NULL;
        block->blb_next = SU_DADDR_NULL;
        block->blb_nblocks = 0;
        block->blb_nblocks_used = 0;
        block->blb_size = 0L;
        block->blb_daddr = SU_DADDR_NULL;
}

/*#***********************************************************************\
 * 
 *		blb_init
 * 
 * Creates a blob list block object
 * 
 * Parameters : 
 * 
 *	buf - in, take
 *		pointer to storage where the buffer is to reside
 *          or NULL to allocate a new object
 *
 * Return value - give :
 *      pointer to created block list block object
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_blobblock_t* blb_init(void* buf)
{
        dbe_blobblock_t* block;

        ss_dprintf_3(("blb_init\n"));
        if (buf == NULL) {
            block = SSMEM_NEW(dbe_blobblock_t);
        } else {
            block = buf;
        }
        block->blb_type = DBE_BLOCK_BLOBLIST;
        block->blb_cpnum = 0L;
        block->blb_next = SU_DADDR_NULL;
        block->blb_nblocks = 0;
        block->blb_nblocks_used = 0;
        block->blb_size = 0L;
        block->blb_cacheslot = NULL;
        block->blb_data = NULL;
        block->blb_daddr = SU_DADDR_NULL;
        block->blb_id = 0L;
        return (block);
}

/*#***********************************************************************\
 * 
 *		bb_done
 * 
 * releases a Blob Block object
 * 
 * Parameters : 
 * 
 *	block - in, take
 *		pointer to blob block
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void bb_done(dbe_blobblock_t* block)
{
        ss_dprintf_3(("bb_done\n"));
        ss_dassert(block->blb_cacheslot == NULL);
        SsMemFree(block);
}

/*#***********************************************************************\
 * 
 *		bdb_init
 * 
 * Creates a Blob Data Block object
 * 
 * Parameters : 
 * 
 *	buf - in, take
 *		pointer to storage where the object is to reside or
 *          NULL to allocate new storage
 *		
 * Return value - give : 
 *      pointer to new blob data block
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_blobblock_t* bdb_init(void* buf)
{
        dbe_blobblock_t* block;

        ss_dprintf_3(("bdb_init\n"));
        block = blb_init(buf);
        block->blb_type = DBE_BLOCK_BLOBDATA;
        return (block);
}

/*#***********************************************************************\
 * 
 *		bdb_reachforwrite
 * 
 * Reaches a Blob Data Block for write mode access
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to blob data block
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	daddr - in
 *		disk address of block
 *          or SU_DADDR_NULL to allocate a new
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where the address of the write
 *          buffer will be stored
 *		
 *	p_bufsize - out
 *		pointer to variable where the write buffer size will be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when ok or
 *      error code on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_ret_t bdb_reachforwrite(
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,   
        char** p_buf,
        size_t* p_bufsize)
{
        su_ret_t rc;
        dbe_info_t info;

        dbe_info_init(info, 0);
        ss_dprintf_3(("bdb_reachforwrite\n"));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);
        ss_dassert(p_buf != NULL);
        ss_dassert(p_bufsize != NULL);
        ss_dassert(block->blb_type == DBE_BLOCK_BLOBDATA);
        if (daddr == SU_DADDR_NULL) {
            FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
            } else {
                rc = dbe_fl_alloc(filedes->fd_freelist, &daddr, &info);
                BLOB_COUNTALLOC(daddr, -1);
            }
            if (rc != SU_SUCCESS) {
                return (rc);
            }
        }
        block->blb_daddr = daddr;
        block->blb_cacheslot =
            dbe_iomgr_reach(
                iomgr,
                daddr,
                DBE_CACHE_WRITEONLY,
                0,
                (char**)&block->blb_data,
                blob_reachctx);
        block->blb_nblocks = 0;
        *p_buf = block->blb_data + BDB_DATAOFFSET;
        *p_bufsize = BDB_DATACAPACITY(filedes->fd_blocksize);
        return (DBE_RC_SUCC);
}

/*#***********************************************************************\
 * 
 *		bb_getsize
 * 
 * Gets size of the blob storage hierarchy from this block downward
 * 
 * Parameters : 
 * 
 *	block - in, use
 *		pointer to blob bloc object
 *		
 * Return value :
 *      the size mentioned above
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_blobsize_t bb_getsize(dbe_blobblock_t* block)
{
        ss_dassert(block != NULL);

        ss_dprintf_3(("bb_getsize\n"));
        switch (block->blb_type) {
            case DBE_BLOCK_BLOBLIST:
                return (block->blb_size);
            case DBE_BLOCK_BLOBDATA:
                return ((dbe_blobsize_t)block->blb_nblocks);
            default:
                ss_error;
                break;
        }
        return 0L;
}

/*#***********************************************************************\
 * 
 *		blob_init
 * 
 * Creates a BLOB stream object
 * 
 * Parameters : 
 *
 *      iomgr - in, hold
 *          pointer to I/O manager object
 *
 *	filedes - in, hold
 *		pointer to file descriptor object
 *		
 *	counter - in, hold
 *		pointer to counter object (NULL when readblob)
 *		
 *	mode - in
 *		either BMODE_READ or BMODE_WRITE or BMODE_COPY
 *		
 *	daddr - in
 *		start address of the blob (SU_DADDR_NULL for write mode)
 *		
 *	size - in
 *		either total blob size or DBE_BLOBSIZE_UNKNOWN
 *
 *      log - in, hold
 *          pointer to logical log object or NULL when BMODE_READ or
 *          no logging requested.
 *
 *      logdataflag - in
 *          Not used for BMODE_READ;
 *          when TRUE and log != NULL the whole data of the write
 *          BLOB is written to log file. If FALSE, only the allocation list
 *          of the BLOBS is written. (allocation list is list of disk block
 *          addresses where the blob is stored).
 *
 *      trxid - in
 *          Transaction ID of transaction that uses this blob or
 *          DBE_TRXID_NULL when BMODE_READ
 *
 * Return value - give :
 *      pointer to new blob object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_blob_t* blob_init(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        blobaccessmode_t mode,
        su_daddr_t daddr,
        dbe_blobsize_t size,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid)
{
        dbe_blob_t* blob;

        ss_dprintf_3(("blob_init\n"));
        blob = SSMEM_NEW(dbe_blob_t);
        blob->b_mode = mode;
        blob->b_state = BSTATE_RELEASED;
        blob->b_id = 0L;
        blob->b_iomgr = iomgr;
        blob->b_filedes = filedes;
        blob->b_counter = counter;
        blob->b_startaddr = daddr;
        blob->b_startblock = NULL;
        blob->b_curlistblock = NULL;
        blob->b_curlistpos = B_CURLISTPOS_SELF;
        blob->b_curdatablock = NULL;
        blob->b_curdatapos = 0;
        blob->b_curtotalpos = 0;
        blob->b_size = size;
        blob->b_log = log;    
        blob->b_logdataflag = logdataflag;
        blob->b_trxid       = trxid;
        blob->b_prefetchsize = 0;
        blob->b_prefetchnext = 0;

        return (blob);
}

/*#***********************************************************************\
 * 
 *		blob_done
 * 
 * Deletes a blob object
 * 
 * Parameters : 
 * 
 *	blob - in, take
 *		pointer to blob object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void blob_done(dbe_blob_t* blob)
{
        ss_dassert(blob != NULL);

        ss_dprintf_3(("blob_done\n"));
        if (blob->b_startblock != NULL) {
            bb_done(blob->b_startblock);
        }
        if (blob->b_curlistblock != NULL
        &&  blob->b_curlistblock != blob->b_startblock)
        {
            bb_done(blob->b_curlistblock);
        }
        if (blob->b_curdatablock != NULL
        &&  blob->b_curdatablock != blob->b_curlistblock
        &&  blob->b_curdatablock != blob->b_startblock)
        {
            bb_done(blob->b_curdatablock);
        }
        SsMemFree(blob);
}


/*##**********************************************************************\
 * 
 *		dbe_readblob_init
 * 
 * Creates a Read Blob stream
 * 
 * Parameters : 
 * 
 *      iomgr - in, hold
 *          pointer to I/O manager object
 *
 *	filedes - in, hold
 *		pointer to file descriptor object
 *		
 *	daddr - in
 *		start disk address of the blob
 *
 *      prefetchsize - in
 *          read-ahead size (in blocks) for this blob
 *
 * Return value - give :
 *      pointer to new blob object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_readblob_t* dbe_readblob_init(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,
        int prefetchsize)
{
        dbe_readblob_t* blob;

        ss_dprintf_2(("dbe_readblob_init\n"));
        blob = (dbe_readblob_t*)blob_init(
                    iomgr,
                    filedes,
                    NULL,
                    BMODE_READ,
                    daddr,
                    DBE_BLOBSIZE_UNKNOWN,
                    NULL,
                    FALSE,
                    DBE_TRXID_NULL);
        blob->b_prefetchsize = prefetchsize;
        return (blob);
}

/*##**********************************************************************\
 * 
 *		dbe_readblob_done
 * 
 * Deletes a read blob object
 * 
 * Parameters : 
 * 
 *	blob - in, take
 *		pointer to blob object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_readblob_done(dbe_readblob_t* blob)
{
        ss_dprintf_2(("dbe_readblob_done\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_READ);

        if (blob->b_state == BSTATE_REACHED) {
            dbe_readblob_release(blob, 0);
        }
        ss_dassert(blob->b_state == BSTATE_RELEASED);
        if (blob->b_startblock != NULL) {
            bb_releasefromread(
                blob->b_startblock,
                blob->b_iomgr,
                blob->b_filedes);
        }
        if (blob->b_curlistblock != NULL
        &&  blob->b_curlistblock != blob->b_startblock)
        {
            bb_releasefromread(
                blob->b_curlistblock,
                blob->b_iomgr,
                blob->b_filedes);
        }
        if (blob->b_curdatablock != NULL
        &&  blob->b_curdatablock != blob->b_curlistblock
        &&  blob->b_curdatablock != blob->b_startblock)
        {
            bb_releasefromread(
                blob->b_curdatablock,
                blob->b_iomgr,
                blob->b_filedes);
        }
        blob_done((dbe_blob_t*)blob);
}

/*#***********************************************************************\
 * 
 *		blob_prefetch
 * 
 * Makes a prefetch request to I/O manager according to current
 * blob reach position
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to blob object (must not be a write blob)
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void blob_prefetch(dbe_blob_t* blob)
{
        su_daddr_t* prefetch_array;
        int array_size;
        int endidx;
        int i;
        int j;

        if (blob->b_prefetchsize == 0) {
            /* No prefetch configured! */
            return;
        }
        ss_dassert(blob->b_prefetchsize >= 2);
        /* If we have not advanced to half of the previous prefetch
         * batch, we need not make a new one
         */
        if (blob->b_curlistpos == B_CURLISTPOS_SELF) {
            if (blob->b_prefetchnext > (blob->b_prefetchsize + 1) / 2)
            {
                return;
            }
        } else if (blob->b_prefetchnext - blob->b_curlistpos - 1
                   > (blob->b_prefetchsize + 1) / 2)
        {
            return;
        }

        /* Calculate index one past last block in the
         * prefetch batch
         */
        if (blob->b_curlistpos == B_CURLISTPOS_SELF) {
            endidx = blob->b_prefetchsize;
        } else {
            endidx = blob->b_curlistpos + blob->b_prefetchsize + 1;
        }
        /* If end of list block becomes before the prefetch set size
         * we decrease the size
         */
        if (endidx >= blob->b_curlistblock->blb_nblocks_used + 1) {
            endidx = blob->b_curlistblock->blb_nblocks_used + 1;
            if (blob->b_curlistblock->blb_next == SU_DADDR_NULL) {
                /* No next list block */
                endidx--;
            }
        }
        array_size = endidx - blob->b_prefetchnext;
        if (array_size == 0) {
            return;
        }
        ss_dassert(array_size > 0);
        prefetch_array =
            SsMemAlloc((size_t)array_size * sizeof(su_daddr_t));
        for (j = 0, i = blob->b_prefetchnext; i < endidx; i++, j++) {
            ss_dassert(j < array_size);
            if (i >= blob->b_curlistblock->blb_nblocks_used) {
                ss_dassert(i == blob->b_curlistblock->blb_nblocks_used);
                ss_dassert(i == endidx - 1);
                prefetch_array[j] = blob->b_curlistblock->blb_next;
                ss_dassert(prefetch_array[j] != SU_DADDR_NULL);
                ss_dassert(prefetch_array[j]
                    < dbe_cache_getfilesize(blob->b_filedes->fd_cache));
            } else {
                void* p = 
                    blob->b_curlistblock->blb_data +
                    BLB_ALLOCLSTOFFSET +
                    sizeof(su_daddr_t) * i;
                prefetch_array[j] = SS_UINT4_LOADFROMDISK(p);
                ss_dassert(prefetch_array[j]
                    < dbe_cache_getfilesize(blob->b_filedes->fd_cache));
            }
        }
        ss_dassert(j == array_size);
        blob->b_prefetchnext = endidx;
        dbe_iomgr_prefetch(
            blob->b_iomgr,
            prefetch_array,
            array_size,
            0);
        SsMemFree(prefetch_array);
}

/*#***********************************************************************\
 * 
 *		blob_prefetchwait
 * 
 * Waits for a prefetch request to be satisfied. This is done to
 * improve system total throughput at the expense of response
 * time.
 * 
 * Parameters : 
 * 
 *	blob - in, use
 *		blob object
 *		
 *	daddr - in
 *		disk address to wait for
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void blob_prefetchwait(dbe_blob_t* blob, su_daddr_t daddr)
{
        dbe_iomgr_prefetchwait(
            blob->b_iomgr,
            daddr);
}

/*##**********************************************************************\
 * 
 *		dbe_readblob_reach
 * 
 * Reaches a read buffer for a blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to read blob object
 *		
 *	p_nbytes - out
 *          pointer to variable where the size of the read
 *          buffer will be stored
 *		
 *		
 * Return value - ref :
 *      pointer to read buffer or
 *      NULL when end of BLOB reached before this reach
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* dbe_readblob_reach(
        dbe_readblob_t* blob,
        size_t* p_nbytes)
{
        dbe_ret_t rc;
        char* p = NULL;
        su_daddr_t daddr;

        ss_dprintf_2(("dbe_readblob_reach\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_READ);
        ss_dassert(blob->b_state == BSTATE_RELEASED)
        ss_dassert(p_nbytes != NULL);

        blob->b_state = BSTATE_REACHED;
        if (blob->b_curdatablock == NULL) {     /* 1st reach */
            ss_dassert(blob->b_curdatapos == 0);
            ss_dassert(blob->b_curtotalpos == 0L);
            ss_dassert(blob->b_curlistblock == NULL);
            if (blob->b_startblock != NULL) {
                blob->b_curdatablock = blob->b_startblock;
            } else {
                blob->b_curdatablock = blb_init(NULL);
                blob->b_startblock = blob->b_curdatablock;
            }
            rc = bb_reachforread(
                    blob->b_curdatablock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    blob->b_startaddr,
                    &p,
                    p_nbytes);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            switch (bb_gettype(blob->b_curdatablock)) {
                case DBE_BLOCK_BLOBDATA:
                    break;
                case DBE_BLOCK_BLOBLIST:
                    blob->b_curlistblock = blob->b_curdatablock;
                    blob->b_curlistpos = B_CURLISTPOS_SELF;
                    blob->b_id = blob->b_curlistblock->blb_id;
                    blob_prefetch(blob);
                    break;
                default:
                    ss_rc_error(bb_gettype(blob->b_curdatablock));
            }
            blob->b_size = bb_getsize(blob->b_curdatablock);
            blob->b_curdatapos = p - blob->b_curdatablock->blb_data;
            return (p);
        }
        if (blob->b_curdatapos >= blob->b_filedes->fd_blocksize) {
            ss_dassert(blob->b_curdatapos ==
                blob->b_filedes->fd_blocksize);
            if (blob->b_curlistblock == NULL) {
                p = NULL;
                *p_nbytes = 0;
                return (p);
            }
            blob->b_curlistpos++;
            if (blob->b_curlistpos < blob->b_curlistblock->blb_nblocks_used) {
                if (blob->b_curdatablock == blob->b_curlistblock) {
                    blob->b_curdatablock = bdb_init(NULL);
                }
                blob_prefetch(blob);
                {
                    void* p =
                        blob->b_curlistblock->blb_data +
                        BLB_ALLOCLSTOFFSET +
                        sizeof(su_daddr_t) * blob->b_curlistpos;
                    daddr = SS_UINT4_LOADFROMDISK(p);
                }       
                blob_prefetchwait(blob, daddr);
                rc = bb_reachforread(
                        blob->b_curdatablock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        daddr,
                        &p,
                        p_nbytes);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
                blob->b_curdatapos =
                    p - blob->b_curdatablock->blb_data;
                return (p);
            }
            /* Else we need to fetch next list block
             */
            daddr = blob->b_curlistblock->blb_next;
            bb_releasefromread(
                blob->b_curlistblock,
                blob->b_iomgr,
                blob->b_filedes);
            if (daddr != SU_DADDR_NULL) {
                blob_prefetchwait(blob, daddr);
                rc = bb_reachforread(
                        blob->b_curdatablock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        daddr,
                        &p,
                        p_nbytes);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
                switch (bb_gettype(blob->b_curdatablock)) {
                    case DBE_BLOCK_BLOBDATA:
                        if (blob->b_startblock !=
                            blob->b_curlistblock)
                        {
                            bb_done(blob->b_curlistblock);
                            blob->b_curlistblock = NULL;
                        }
                        break;
                    case DBE_BLOCK_BLOBLIST:
                        if (blob->b_startblock !=
                            blob->b_curlistblock)
                        {
                            bb_done(blob->b_curlistblock);
                        }
                        blob->b_curlistblock =
                            blob->b_curdatablock;
                        blob->b_curlistpos = B_CURLISTPOS_SELF;
                        blob->b_prefetchnext = 0;
                        blob_prefetch(blob);
                        break;
                    default:
                        ss_error;
                }
                blob->b_curdatapos =
                    p - blob->b_curdatablock->blb_data;
                return (p);
            }
            p = NULL;
            *p_nbytes = 0;
            return (p);
        }
        switch (bb_gettype(blob->b_curdatablock)) {
            case DBE_BLOCK_BLOBDATA:
                if (blob->b_curdatablock->blb_nblocks + BDB_DATAOFFSET >
                    blob->b_curdatapos)
                {
                    *p_nbytes =
                        (blob->b_curdatablock->blb_nblocks +
                            BDB_DATAOFFSET) -
                        blob->b_curdatapos;
                    p = blob->b_curdatablock->blb_data +
                        blob->b_curdatapos;
                } else {
                    ss_dassert(blob->b_curdatablock->blb_nblocks +
                        BDB_DATAOFFSET == blob->b_curdatapos)
                    *p_nbytes = 0;
                    p = NULL;
                }
                return (p);
            case DBE_BLOCK_BLOBLIST:
                if (blob->b_curdatablock->blb_nblocks_used == 0) {
                    if ((BLB_DATAOFFSET(blob->b_curdatablock) +
                         blob->b_curdatablock->blb_size)
                        > blob->b_curdatapos)
                    {
                        *p_nbytes = 
                            (BLB_DATAOFFSET(blob->b_curdatablock) +
                             blob->b_curdatablock->blb_size) -
                            blob->b_curdatapos;
                        p = blob->b_curdatablock->blb_data +
                            blob->b_curdatapos;
                        return (p);
                    } 
                    ss_dassert(
                        (BLB_DATAOFFSET(blob->b_curdatablock) +
                         blob->b_curdatablock->blb_size)
                        == blob->b_curdatapos);
                    *p_nbytes = 0;
                    p = NULL;
                    return (p);
                }
                *p_nbytes = blob->b_filedes->fd_blocksize -
                    blob->b_curdatapos;
                if (*p_nbytes == 0) {
                    p = NULL;
                } else {
                    p = blob->b_curdatablock->blb_data +
                        blob->b_curdatapos;
                }
                return (p);
            default:
                ss_rc_error(bb_gettype(blob->b_curdatablock));
        }
        ss_error;
        return (NULL);
}

/*##**********************************************************************\
 * 
 *		dbe_readblob_release
 * 
 * Releases a reach for a read blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to read blob
 *		
 *	nbytes - in
 *		number of bytes handled
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_readblob_release(
        dbe_readblob_t* blob,
        size_t nbytes)
{
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_READ);
        ss_dassert(blob->b_state == BSTATE_REACHED)

        ss_dprintf_2(("dbe_readblob_release\n"));
        blob->b_state = BSTATE_RELEASED;
        blob->b_curdatapos += nbytes;
        blob->b_curtotalpos += nbytes;
        if (blob->b_curdatapos >= blob->b_filedes->fd_blocksize) {
            ss_dassert(blob->b_curdatapos ==
                blob->b_filedes->fd_blocksize);
            if (blob->b_curdatablock != blob->b_curlistblock) {
                ss_dassert(
                    bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBDATA);
                bb_releasefromread(
                    blob->b_curdatablock,
                    blob->b_iomgr,
                    blob->b_filedes);
            }
        }
}

/*##**********************************************************************\
 * 
 *		dbe_readblob_read
 * 
 * Read function for blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to blob object
 *		
 *	buf - out, use
 *		pointer to caller's buffer
 *		
 *	bufsize - in
 *		size of buf
 *		
 * Return value :
 *      number of bytes read
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
size_t dbe_readblob_read(
        dbe_readblob_t* blob,
        char* buf,
        size_t bufsize)
{
        char* sourcebuf;
        size_t sourcebufsize;
        size_t ntoread;
        size_t nread;

        ss_dprintf_2(("dbe_readblob_read\n"));
        nread = 0;
        while (bufsize) {
            sourcebuf = dbe_readblob_reach(blob, &sourcebufsize);
            ntoread = SS_MIN(sourcebufsize, bufsize);
            if (ntoread == 0) {
                break;
            }
            memcpy(buf, sourcebuf, ntoread);
            dbe_readblob_release(blob, ntoread);
            bufsize -= ntoread;
            buf += ntoread;
            nread += ntoread;
        }
        return (nread);
}


/*##**********************************************************************\
 * 
 *		dbe_readblob_getid
 * 
 * Gets the blob ID
 * 
 * Parameters : 
 * 
 *	blob - in (out), use
 *		pointer to blob object
 *		
 * Return value :
 *      blob ID
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_blobid_t dbe_readblob_getid(dbe_readblob_t* blob)
{
        ss_dprintf_2(("dbe_readblob_getid\n"));
        if (blob->b_startblock == NULL) {
            size_t s;

            (void)dbe_readblob_reach(blob, &s);
            dbe_readblob_release(blob, 0);
        }
        return (blob->b_id);
}

/*##**********************************************************************\
 * 
 *		dbe_readblob_getsize
 * 
 * Gets blob size
 * 
 * Parameters : 
 * 
 *	blob - in (out), use
 *		pointer to blob object
 *		
 * Return value :
 *      Blob size
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_blobsize_t dbe_readblob_getsize(dbe_readblob_t* blob)
{
        ss_dprintf_2(("dbe_readblob_getsize\n"));
        if (blob->b_startblock == NULL) {
            size_t s;

            (void)dbe_readblob_reach(blob, &s);
            dbe_readblob_release(blob, 0);
        }
        return (blob->b_size);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_init
 * 
 * Creates a Write blob stream
 * 
 * Parameters : 
 * 
 *      iomgr - in, hold
 *          pointer to I/O manager object
 *
 *	filedes - in, hold
 *		pointer to file descriptor object
 *		
 *	counter - in, hold
 *		pointer to counter object
 *		
 *	size - in
 *		blob size or DBE_BLOBSIZE_UNKNON
 *		
 *      log - in, hold
 *          pointer to log subsystem object or when NULL
 *          no logging requested.
 *
 *      logdataflag - in
 *          when TRUE and log != NULL the whole data of the write
 *          BLOB is written to log file. If FALSE, only the allocation list
 *          of the BLOBS is written. (allocation list is list of disk block
 *          addresses where the blob is stored).
 *
 *      trxid - in
 *          Transaction ID of transaction that uses this blob
 *
 *      p_blobid - out, use
 *          when not NULL, a pointer to variable where the BLOB ID
 *          will be stored
 *
 * Return value - give :
 *      pointer to new blob object    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_writeblob_t* dbe_writeblob_init(
        rs_sysi_t* cd,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        dbe_blobsize_t size,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid,
        dbe_blobid_t* p_blobid)
{
        dbe_writeblob_t* blob;

        ss_dprintf_2(("dbe_writeblob_init\n"));
        blob = (dbe_writeblob_t*)blob_init(
                    iomgr,
                    filedes,
                    counter,
                    BMODE_WRITE,
                    SU_DADDR_NULL,
                    size,
                    log,
                    logdataflag,
                    trxid);
        blob->b_cd = cd;
        blob->b_id = dbe_counter_getnewblobid(counter);
        if (p_blobid != NULL) {
            *p_blobid = blob->b_id;
        }
        ss_dprintf_1(("dbe_writeblob_init: blobid = %ld\n", blob->b_id));
        FAKE_BLOBID_ADD(blob->b_id);
        return (blob);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_done
 * 
 * Deletes a write blob object. and closes the blob
 * 
 * Parameters : 
 * 
 *	blob - in, take
 *		pointer to blob object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_writeblob_done(dbe_writeblob_t* blob)
{
        ss_dprintf_1(("dbe_writeblob_done blobid = %ld\n", blob->b_id));
        dbe_writeblob_close(blob);
        blob_done((dbe_blob_t*)blob);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_abort
 * 
 * Deletes a write blob objects and cancels all data that has been written.
 * 
 * Parameters : 
 * 
 *	blob - in, take
 *		pointer to write blob object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_writeblob_abort(
        dbe_writeblob_t* blob)
{
        ss_dprintf_1(("dbe_writeblob_abort blobid = %ld\n", blob->b_id));
        FAKE_BLOBID_REMOVE(blob->b_id);
        switch (blob->b_state) {
            case BSTATE_REACHED:
                dbe_writeblob_release(blob, 0);
                /* FALLTHROUGH */
            case BSTATE_RELEASED:
                dbe_writeblob_close(blob);
                break;
            case BSTATE_CLOSED:
                break;
            default:
                ss_rc_error(blob->b_state);
                break;
        }
        dbe_blob_delete(
            blob->b_iomgr,
            blob->b_filedes,
            blob->b_counter,
            blob->b_startaddr);
        blob_done((dbe_blob_t*)blob);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_close
 * 
 * Closes the write blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to write blob object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_writeblob_close(
        dbe_writeblob_t* blob)
{
        dbe_cpnum_t cpnum;
        dbe_bl_nblocks_t i;
        char* p;
        su_ret_t rc;
        su_daddr_t daddr, daddr_null;
        size_t dataoffset;
        size_t datasize;
        size_t datasize_written __attribute__ ((unused));
        dbe_logrectype_t logrectype;
        char* p_datastart;

        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_WRITE);

        switch (blob->b_state) {
            case BSTATE_RELEASED:
                break;
            case BSTATE_CLOSED:
                return;
            case BSTATE_REACHED:
                ss_error;
                break;
            default:
                ss_rc_error(blob->b_state);
                break;
        }
        ss_dprintf_1(("dbe_writeblob_close() blobid = %ld\n", blob->b_id));
        cpnum = dbe_counter_getcpnum(blob->b_counter);

#ifndef SS_NOLOGGING
        if (blob->b_log != NULL /* logging desired */
        &&  blob->b_curdatapos < blob->b_filedes->fd_blocksize
        &&  blob->b_logdataflag
        &&  blob->b_curdatablock != NULL)
        {
            /* All data of the BLOB is logged
             */
            if (bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBDATA) {
                dataoffset = BDB_DATAOFFSET;
                logrectype = DBE_LOGREC_BLOBDATA_CONT_OLD;
            } else {
                ss_dassert(bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBLIST);
                dataoffset = BLB_DATAOFFSET(blob->b_curdatablock);
                if (blob->b_startblock != blob->b_curlistblock) {
                    logrectype = DBE_LOGREC_BLOBDATA_CONT_OLD;
                } else {
                    logrectype = DBE_LOGREC_BLOBDATA_OLD;
                }
            }
            p_datastart = blob->b_curdatablock->blb_data + dataoffset;
            datasize = blob->b_curdatapos - dataoffset;
            if (datasize != 0) {
#ifdef SS_BLOBBUG
                FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
                    FAKE_BLOBMON_ADD2(datasize));
#endif
                rc = dbe_log_putblobdata(
                        blob->b_log,
                        blob->b_cd,
                        logrectype,
                        blob->b_trxid,
                        p_datastart,
                        datasize);
                SS_NOTUSED(rc);
            }
        }
#endif /* SS_NOLOGGING */
        if (blob->b_curlistblock != NULL) {
            if (blob->b_curlistblock->blb_nblocks_used <
                blob->b_curlistblock->blb_nblocks)
            {
                daddr_null = SU_DADDR_NULL;
                
                if (blob->b_curlistblock->blb_next != SU_DADDR_NULL) {
                    rc = dbe_fl_free(
                            blob->b_filedes->fd_freelist,
                            blob->b_curlistblock->blb_next);
                    su_rc_assert(rc == SU_SUCCESS, rc);
                    BLOB_COUNTFREE(blob->b_curlistblock->blb_next);
                    blob->b_curlistblock->blb_next = SU_DADDR_NULL;
                }
                p = blob->b_curlistblock->blb_data +
                    BLB_DATAOFFSET(blob->b_curlistblock);
                for (i = blob->b_curlistblock->blb_nblocks;
                     i > blob->b_curlistblock->blb_nblocks_used;
                     i--)
                {
                    p -= sizeof(su_daddr_t);
                    daddr = SS_UINT4_LOADFROMDISK(p);
                    if (daddr != SU_DADDR_NULL) {
                        rc = dbe_fl_free(blob->b_filedes->fd_freelist, daddr);
                        su_rc_assert(rc == SU_SUCCESS, rc);
                        BLOB_COUNTFREE(daddr);
                        SS_UINT4_STORETODISK(p, daddr_null);
                    }
                }
            }
        }
        if (blob->b_startblock != NULL) {
            if (blob->b_size == DBE_BLOBSIZE_UNKNOWN) {
                blob->b_startblock->blb_size =
                    blob->b_size =
                        blob->b_curtotalpos;
            } else {
                blob->b_startblock->blb_size =
                    blob->b_size =
                        blob->b_curtotalpos;
            }
            switch (bb_gettype(blob->b_startblock)) {
                case DBE_BLOCK_BLOBLIST:
                    ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                    bb_releasefromwrite(
                        blob->b_startblock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        cpnum,
                        (size_t)(blob->b_startblock->blb_nblocks_used == 0 ?
                            blob->b_size :
                            (BLB_DATACAPACITY(blob->b_filedes->fd_blocksize) -
                             sizeof(su_daddr_t) *
                              blob->b_startblock->blb_nblocks)),
                        !blob->b_logdataflag);
                    break;
                case DBE_BLOCK_BLOBDATA:
                    ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                    bb_releasefromwrite(
                        blob->b_startblock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        cpnum,
                        (size_t)blob->b_size,
                        !blob->b_logdataflag);
                    break;
                default:
                    ss_error;
            }
        }
        if (blob->b_curlistblock != NULL
        &&  blob->b_curlistblock != blob->b_startblock)
        {
            if (blob->b_curdatablock == blob->b_curlistblock) {
                ss_dassert(blob->b_curlistpos == B_CURLISTPOS_SELF);
                ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                bb_releasefromwrite(
                    blob->b_curlistblock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    cpnum,
                    BLB_DATACAPACITY(blob->b_filedes->fd_blocksize) -
                        sizeof(su_daddr_t) * blob->b_curlistblock->blb_nblocks -
                        (blob->b_filedes->fd_blocksize - blob->b_curdatapos),
                    !blob->b_logdataflag);
            } else {
                ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                bb_releasefromwrite(
                    blob->b_curlistblock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    cpnum,
                    BLB_DATACAPACITY(blob->b_filedes->fd_blocksize) -
                        sizeof(su_daddr_t) * blob->b_curlistblock->blb_nblocks,
                    !blob->b_logdataflag);
            }
        }
        if (blob->b_curdatablock != NULL
        &&  blob->b_curdatablock != blob->b_curlistblock
        &&  blob->b_curdatablock != blob->b_startblock)
        {
            ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
            bb_releasefromwrite(
                blob->b_curdatablock,
                blob->b_iomgr,
                blob->b_filedes,
                cpnum,
                BDB_DATACAPACITY(blob->b_filedes->fd_blocksize) -
                (blob->b_filedes->fd_blocksize - blob->b_curdatapos),
                !blob->b_logdataflag);
        }
        blob->b_state = BSTATE_CLOSED;
}

/*#***********************************************************************\
 * 
 *		writeblob_adjustnbytes
 * 
 * Adjusts nbytes out parameter not to exceed the total blob size
 * 
 * Parameters : 
 * 
 *	blob - in, use
 *		pointer to write blob object
 *		
 *	p_nbytes - in out, use
 *		pointer to buffer size variable
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#if 0   /* Removed to allow BLOB size to exceed orig. specification */
static void writeblob_adjustnbytes(
        dbe_writeblob_t* blob,
        size_t* p_nbytes)
{
        if (blob->b_size != DBE_BLOBSIZE_UNKNOWN) {
            dbe_blobsize_t sizeleft;

            sizeleft = blob->b_size - blob->b_curtotalpos;
            if (sizeleft < (dbe_blobsize_t)*p_nbytes) {
                *p_nbytes = sizeleft;
            }
        }
}
#else
#define writeblob_adjustnbytes(blob, nbytes)    /* Nothing */
#endif

/*##**********************************************************************\
 * 
 *		dbe_writeblob_reach
 * 
 * Reaches a write buffer for a blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to blob
 *
 *      pp_buf - out, ref
 *          pointer to pointer to write buffer
 *     
 *	p_nbytes - out
 *		pointer to variable where the size of the buffer will
 *          be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when OK or error code when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_writeblob_reach(
        dbe_writeblob_t* blob,
        char** pp_buf,
        size_t* p_nbytes)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        su_daddr_t daddr;
        dbe_info_t info;

        dbe_info_init(info, 0);
        ss_dprintf_2(("dbe_writeblob_reach\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_WRITE);
        ss_dassert(blob->b_state == BSTATE_RELEASED)
        ss_dassert(p_nbytes != NULL);

        if (blob->b_curdatablock == NULL) {     /* 1st reach */
            ss_dassert(blob->b_curdatapos == 0);
            ss_dassert(blob->b_curtotalpos == 0L);
            ss_dassert(blob->b_curlistblock == NULL);
            ss_dassert(blob->b_startblock == NULL);
            blob->b_curdatablock = blb_init(NULL);
            blob->b_curlistblock = blob->b_curdatablock;
            blob->b_curlistpos = B_CURLISTPOS_SELF;
            ss_dprintf_1(("dbe_writeblob_reach line %d blobid = %ld\n", __LINE__, blob->b_id));
            rc = blb_reachforwrite(
                    blob->b_curdatablock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    blob->b_size,
                    SU_DADDR_NULL,
                    pp_buf,
                    p_nbytes,
                    blob->b_id);
            if (rc != DBE_RC_SUCC) {
                bb_done(blob->b_curdatablock);
                blob->b_curlistblock = blob->b_curdatablock = NULL;
                *pp_buf = NULL;
                *p_nbytes = 0;
                return (rc);
            }
            blob->b_startblock = blob->b_curdatablock;
            blob->b_startblock->blb_id = blob->b_id;
            blob->b_startaddr = blob->b_startblock->blb_daddr;
            blob->b_curdatapos = *pp_buf - blob->b_curdatablock->blb_data;
            writeblob_adjustnbytes(blob, p_nbytes);
            blob->b_state = BSTATE_REACHED;
#ifdef SS_BLOBBUG
            FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
                FAKE_REGISTER_BLOBDADDR(blob->b_curdatablock->blb_daddr));
#endif

            return (rc);
        }
        if (blob->b_curdatapos >= blob->b_filedes->fd_blocksize) {
            /* No space left in the current data block */
            ss_dassert(blob->b_curdatapos ==
                blob->b_filedes->fd_blocksize);
            if (blob->b_curdatablock != blob->b_curlistblock) {
                ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                bb_releasefromwrite(
                    blob->b_curdatablock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    dbe_counter_getcpnum(blob->b_counter),
                    BDB_DATACAPACITY(blob->b_filedes->fd_blocksize),
                    !blob->b_logdataflag);
                (void)bdb_init(blob->b_curdatablock);
            } 
            if (blob->b_curlistblock == NULL) {
                *pp_buf = NULL;
                *p_nbytes = 0;
                ss_derror; /* weird situation! */
                return (SU_ERR_FILE_WRITE_DISK_FULL);
            }
            blob->b_curlistpos++;
            if (blob->b_curlistpos < blob->b_curlistblock->blb_nblocks) {
                /* List block positions still available */
                if (blob->b_curdatablock == blob->b_curlistblock) {
                    blob->b_curdatablock = bdb_init(NULL);
                }
                blob->b_curlistblock->blb_nblocks_used =
                    (dbe_bl_nblocks_t)(blob->b_curlistpos + 1);
                {
                    void* p =
                        blob->b_curlistblock->blb_data +
                        BLB_ALLOCLSTOFFSET +
                        sizeof(su_daddr_t) * blob->b_curlistpos;

                    daddr = SS_UINT4_LOADFROMDISK(p);
                }
                ss_dprintf_1(("dbe_writeblob_reach line %d blobid = %ld\n", __LINE__, blob->b_id));
                rc = bdb_reachforwrite(
                        blob->b_curdatablock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        daddr,
                        pp_buf,
                        p_nbytes);
                if (rc != DBE_RC_SUCC) {
                    blob->b_curlistpos--;
                    blob->b_curlistblock->blb_nblocks_used =
                        (dbe_bl_nblocks_t)(blob->b_curlistpos + 1);
                    *pp_buf = NULL;
                    *p_nbytes = 0;
                    return (rc);
                }
                if (daddr == SU_DADDR_NULL) {
                    void* p;

                    daddr = blob->b_curdatablock->blb_daddr;
                    ss_dassert(daddr != SU_DADDR_NULL);

                    p = blob->b_curlistblock->blb_data +
                        BLB_ALLOCLSTOFFSET +
                        sizeof(su_daddr_t) * blob->b_curlistpos;
                    SS_UINT4_STORETODISK(p, daddr);
                }
                blob->b_curdatapos =
                    *pp_buf - blob->b_curdatablock->blb_data;
                writeblob_adjustnbytes(blob, p_nbytes);
                blob->b_state = BSTATE_REACHED;
#ifdef SS_BLOBBUG
                FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
                    FAKE_REGISTER_BLOBDADDR(blob->b_curdatablock->blb_daddr));
#endif
                return (rc);
            } 
            daddr = blob->b_curlistblock->blb_next;
            if (daddr == SU_DADDR_NULL) {
                FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                    FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
                } else {
                    rc = dbe_fl_alloc(blob->b_filedes->fd_freelist, &daddr, &info);
                    BLOB_COUNTALLOC(daddr, blob->b_id);
                }
                if (rc != SU_SUCCESS) {
                    *pp_buf = NULL;
                    *p_nbytes = 0;
                    return (rc);
                }
                blob->b_curlistblock->blb_next = daddr;
            }
            if (blob->b_curlistblock != blob->b_startblock) {
                ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                bb_releasefromwrite(
                    blob->b_curlistblock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    dbe_counter_getcpnum(blob->b_counter),
                    BLB_DATACAPACITY(blob->b_filedes->fd_blocksize) -
                        sizeof(su_daddr_t) * blob->b_curlistblock->blb_nblocks,
                    !blob->b_logdataflag);
            }

            ss_dassert(daddr != SU_DADDR_NULL);
            ss_dassert(blob->b_curdatablock != NULL); 
            ss_dassert(blob->b_curdatablock != blob->b_curlistblock);
            if (blob->b_size != DBE_BLOBSIZE_UNKNOWN
            &&  blob->b_size - blob->b_curtotalpos <=
                (dbe_blobsize_t)BDB_DATACAPACITY(blob->b_filedes->fd_blocksize))
            {
                if (blob->b_startblock != blob->b_curlistblock) {
                    bb_done(blob->b_curlistblock);
                }
                blob->b_curlistblock = NULL;
                ss_dprintf_1(("dbe_writeblob_reach line %d blobid = %ld\n", __LINE__, blob->b_id));
                rc = bdb_reachforwrite(
                        blob->b_curdatablock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        daddr,
                        pp_buf,
                        p_nbytes);
            } else {
                if (blob->b_startblock == blob->b_curlistblock) {
                    blob->b_curlistblock = blb_init(blob->b_curdatablock);
                } else {
                    bb_done(blob->b_curdatablock);
                    blob->b_curdatablock = NULL;
                }
                blob->b_curdatablock = blob->b_curlistblock;
                blob->b_curlistpos = B_CURLISTPOS_SELF;
                ss_dprintf_1(("dbe_writeblob_reach line %d blobid = %ld\n", __LINE__, blob->b_id));
                rc = blb_reachforwrite(
                        blob->b_curlistblock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        (blob->b_size == DBE_BLOBSIZE_UNKNOWN ?
                            blob->b_size :
                            (blob->b_size - blob->b_curtotalpos)),
                        daddr,
                        pp_buf,
                        p_nbytes,
                        blob->b_id);
            }
            if (rc != DBE_RC_SUCC) {
                *pp_buf = NULL;
                *p_nbytes = 0;
                return (rc);
            }
            blob->b_curdatapos = *pp_buf - blob->b_curdatablock->blb_data;
            writeblob_adjustnbytes(blob, p_nbytes);
            blob->b_state = BSTATE_REACHED;
#ifdef SS_BLOBBUG
            FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
                FAKE_REGISTER_BLOBDADDR(blob->b_curdatablock->blb_daddr));
#endif
            return (rc);
        }
        /* blob->b_curdatapos < blob->b_filedes->fd_blocksize,
         * i.e. there is space left in the current data block
         */
        *p_nbytes = blob->b_filedes->fd_blocksize - blob->b_curdatapos;
        *pp_buf = blob->b_curdatablock->blb_data + blob->b_curdatapos;
        writeblob_adjustnbytes(blob, p_nbytes);
        blob->b_state = BSTATE_REACHED;
#ifdef SS_BLOBBUG
        FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
            FAKE_REGISTER_BLOBDADDR(blob->b_curdatablock->blb_daddr));
#endif
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_release
 * 
 * Releases a write buffer from reach
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to write blob
 *		
 *	nbytes - in
 *		number of bytes written
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_writeblob_release(
        dbe_writeblob_t* blob,
        size_t nbytes)
{
        char* p_datastart;
        size_t datasize;
        size_t datasize_written __attribute__ ((unused));
        size_t dataoffset;
        dbe_logrectype_t logrectype;
        dbe_ret_t rc;

        ss_dprintf_2(("dbe_writeblob_release\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_WRITE);
        ss_dassert(blob->b_state == BSTATE_REACHED)
        
        blob->b_state = BSTATE_RELEASED;
        blob->b_curdatapos += nbytes;
        ss_dassert(blob->b_curdatapos <= blob->b_filedes->fd_blocksize);
        blob->b_curtotalpos += nbytes;
#ifdef SS_BLOBBUG
        FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
            FAKE_BLOBMON_ADD(1,nbytes));
#endif
        if (blob->b_curdatapos >= blob->b_filedes->fd_blocksize) {
            ss_dassert(blob->b_curdatapos ==
                blob->b_filedes->fd_blocksize);
#ifndef SS_NOLOGGING
            if (blob->b_log != NULL && blob->b_logdataflag) {
                /* All data of the BLOB is logged
                 */
                if (bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBDATA) {
                    dataoffset = BDB_DATAOFFSET;
                    logrectype = DBE_LOGREC_BLOBDATA_CONT_OLD;
                } else {
                    ss_dassert(bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBLIST);
                    dataoffset = BLB_DATAOFFSET(blob->b_curdatablock);
                    if (blob->b_startblock != blob->b_curlistblock) {
                        logrectype = DBE_LOGREC_BLOBDATA_CONT_OLD;
                    } else {
                        logrectype = DBE_LOGREC_BLOBDATA_OLD;
                    }
                }
                p_datastart = blob->b_curdatablock->blb_data + dataoffset;
                datasize = blob->b_filedes->fd_blocksize - dataoffset;
#ifdef SS_BLOBBUG
                FAKE_CODE_BLOCK(FAKE_SSE_DAXBUG,
                    FAKE_BLOBMON_ADD2(datasize));
#endif
                rc = dbe_log_putblobdata(
                        blob->b_log,
                        blob->b_cd,
                        logrectype,
                        blob->b_trxid,
                        p_datastart,
                        datasize);
                SS_NOTUSED(rc);
            }
#endif /* SS_NOLOGGING */
            if (blob->b_curdatablock != blob->b_curlistblock) {
                ss_dassert(bb_gettype(blob->b_curdatablock) == DBE_BLOCK_BLOBDATA);
                ss_dprintf_1(("call to bb_releasefromwrite line %d blobid = %ld\n", __LINE__, blob->b_id));
                bb_releasefromwrite(
                    blob->b_curdatablock,
                    blob->b_iomgr,
                    blob->b_filedes,
                    dbe_counter_getcpnum(blob->b_counter),
                    BDB_DATACAPACITY(blob->b_filedes->fd_blocksize),
                    !blob->b_logdataflag);
                (void)bdb_init(blob->b_curdatablock);
            }
        }
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_getsize
 * 
 * Gets write blob size
 * 
 * Parameters : 
 * 
 *	blob - in, use
 *		
 *		
 * Return value :
 *      blob size
 * 
 * Limitations  :
 *      The Blob must be in BSTATE_CLOSED state
 * 
 * Globals used : 
 */
dbe_blobsize_t dbe_writeblob_getsize(
        dbe_writeblob_t* blob)
{
        ss_dprintf_2(("dbe_writeblob_getsize\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_WRITE);
        ss_dassert(blob->b_state == BSTATE_CLOSED);

        return (blob->b_size);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_getstartaddr
 * 
 * Gets start address of a blob
 * 
 * Parameters : 
 * 
 *	blob - in, use
 *		pointer to blob
 *		
 * Return value : 
 *      start address of the blob
 *
 * Limitations  : 
 *      The Blob must be reached at least once (may be released or
 *      closed after that).
 * 
 * Globals used : 
 */
su_daddr_t dbe_writeblob_getstartaddr(
        dbe_writeblob_t* blob)
{
        ss_dprintf_2(("dbe_writeblob_getstartaddr\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_WRITE);
        ss_dassert(blob->b_startblock != NULL);

        return (blob->b_startaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_write
 * 
 * Writes to blob
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to blob object
 *		
 *	buf - in, use
 *		pointer to caller's buffer
 *		
 *	bufsize - in
 *		size of buf
 *		
 * Return value :
 *      number of bytes written
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_ret_t dbe_writeblob_write(
        dbe_writeblob_t* blob,
        char* buf,
        size_t bufsize)
{
        dbe_ret_t rc = DBE_RC_SUCC;
        char* destbuf;
        size_t destbufsize;
        size_t ntowrite;

        ss_dprintf_2(("dbe_writeblob_write\n"));
        while (bufsize) {
            rc = dbe_writeblob_reach(blob, &destbuf, &destbufsize);
            if (rc != DBE_RC_SUCC) {
                break;
            }
            ntowrite = SS_MIN(destbufsize, bufsize);
            if (ntowrite == 0) {
                break;
            }
            memcpy(destbuf, buf, ntowrite);
            dbe_writeblob_release(blob, ntowrite);
            bufsize -= ntowrite;
            buf += ntowrite;
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_writeblob_getid
 * 
 * Gets ID of write blob
 * 
 * Parameters : 
 * 
 *	blob - in, use
 *		pointer to blob object
 *		
 * Return value :
 *      The blob ID
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_blobid_t dbe_writeblob_getid(dbe_writeblob_t* blob)
{
        ss_dprintf_2(("dbe_writeblob_getid\n"));
        return (blob->b_id);
}


/*##**********************************************************************\
 * 
 *		dbe_blob_delete
 * 
 * Deletes a blob
 * 
 * Parameters : 
 * 
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to file descriptor object
 *		
 *	counter - in, use
 *		pointer to counter object
 *		
 *	daddr - in
 *		start address of the blob
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_blob_delete(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        su_daddr_t daddr)
{
        dbe_ret_t rc;
        char* p;
        char* buf;
        size_t bufsize;
        dbe_blobblock_t* block;
        dbe_cpnum_t cpnum, block_cpnum;
        uint i;
        su_daddr_t tmp_daddr;

        ss_dprintf_2(("dbe_blob_delete\n"));
        cpnum = dbe_counter_getcpnum(counter);
        block = blb_init(NULL);
        while (daddr != SU_DADDR_NULL) {
            rc = bb_reachforread(
                    block,
                    iomgr,
                    filedes,
                    daddr,
                    &buf,
                    &bufsize);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            block_cpnum = block->blb_cpnum;
            switch (bb_gettype(block)) {
                case DBE_BLOCK_BLOBDATA:
                    bb_releasefromread(
                        block,
                        iomgr,
                        filedes);
                    if (block_cpnum == cpnum) {
                        rc = dbe_fl_free(filedes->fd_freelist, daddr);
                        BLOB_COUNTFREE(daddr);
                    } else {
                        ss_dassert(block_cpnum < cpnum);
                        SS_PUSHNAME("dbe_blob_delete:1");
                        rc = dbe_cl_add(
                                filedes->fd_chlist,
                                block_cpnum,
                                daddr);
                        SS_POPNAME;
                        BLOB_COUNTFREE(daddr);
                    }
                    su_rc_assert(rc == SU_SUCCESS, rc);
                    daddr = SU_DADDR_NULL;
                    break;
                case DBE_BLOCK_BLOBLIST:
                    for (i = block->blb_nblocks_used,
                         p = block->blb_data + BLB_ALLOCLSTOFFSET +
                            sizeof(su_daddr_t) * i;
                         i;
                         i--)
                    {
                        p -= sizeof(su_daddr_t);
                        tmp_daddr = SS_UINT4_LOADFROMDISK(p);
                        ss_dassert(tmp_daddr != SU_DADDR_NULL);
                        if (block_cpnum == cpnum) {
                            rc = dbe_fl_free(filedes->fd_freelist, tmp_daddr);
                            BLOB_COUNTFREE(tmp_daddr);
                        } else {
                            ss_dassert(block_cpnum < cpnum);
                            SS_PUSHNAME("dbe_blob_delete:2");
                            rc = dbe_cl_add(
                                    filedes->fd_chlist,
                                    block_cpnum,
                                    tmp_daddr);
                            SS_POPNAME;
                            BLOB_COUNTFREE(tmp_daddr);
                        }
                        su_rc_assert(rc == SU_SUCCESS, rc);
                    }
                    tmp_daddr = daddr;
                    daddr = block->blb_next;
                    bb_releasefromread(block, iomgr, filedes);
                    if (block_cpnum == cpnum) {
                        rc = dbe_fl_free(filedes->fd_freelist, tmp_daddr);
                        BLOB_COUNTFREE(tmp_daddr);
                    } else {
                        ss_dassert(block_cpnum < cpnum);
                        SS_PUSHNAME("dbe_blob_delete:3");
                        rc = dbe_cl_add(
                                filedes->fd_chlist,
                                block_cpnum,
                                tmp_daddr);
                        SS_POPNAME;
                        BLOB_COUNTFREE(tmp_daddr);
                    }
                    su_rc_assert(rc == SU_SUCCESS, rc);
                    break;
                default:
                    ss_rc_error(bb_gettype(block));
            }
        }
        bb_done(block);
}

/*#***********************************************************************\
 * 
 *		bdb_relocate
 * 
 * Relocates (shadow copies) a Blob Data Block. Used in copying of a BLOB
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to Blob Data Block object
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to filedes object
 *		
 *	old_daddr - in
 *		old disk address
 *		
 *	new_daddr - in
 *		new disk address
 *		
 *	cpnum - checkpoint # to be marked to the block
 *		
 *      log - in, use
 *          pointer to logical log object
 *
 *      logdataflag - in
 *          FALSE when BLOB data logging is requested or
 *          FALSE when not.
 *
 *      trxid - in
 *          transaction ID
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static dbe_ret_t bdb_relocate(
        rs_sysi_t* cd,
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t old_daddr,
        su_daddr_t new_daddr,
        dbe_cpnum_t cpnum,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid)
{
        dbe_ret_t rc;
        char* buf = NULL;
        size_t bufsize = 0;

        ss_dprintf_3(("bdb_relocate\n"));
        ss_dassert(block != NULL);
        ss_dassert(block->blb_type == DBE_BLOCK_BLOBDATA);
        ss_dassert(filedes != NULL);
        ss_dassert(old_daddr != SU_DADDR_NULL);
        ss_dassert(new_daddr != SU_DADDR_NULL);
        SS_PUSHNAME("bdb_relocate");

        rc = bb_reachforupdate(
                block,
                iomgr,
                filedes,
                old_daddr,
                &buf,
                &bufsize);
        ss_dassert(block->blb_type == DBE_BLOCK_BLOBDATA);
        if (rc != DBE_RC_SUCC) {
            SS_POPNAME;
            return (rc);
        }
#ifndef SS_NOLOGGING
        if (log != NULL) {
            if (logdataflag) {
                dbe_log_putblobdata(    /* ignore return value! */
                    log,
                    cd,
                    DBE_LOGREC_BLOBDATA_CONT_OLD,
                    trxid,
                    buf,
                    bufsize);
            }
        }
#endif /* SS_NOLOGGING */
        block->blb_daddr = new_daddr;
        block->blb_cpnum = cpnum;
        block->blb_cacheslot = dbe_cache_relocate(
                                    filedes->fd_cache,
                                    block->blb_cacheslot,
                                    new_daddr,
                                    &block->blb_data,
                                    0);
        ss_dprintf_1(("bdb_relocate call to bb_releasefromwrite line %d\n", __LINE__));
        bb_releasefromwrite(
            block,
            iomgr,
            filedes,
            cpnum,
            (size_t)block->blb_nblocks,
            !logdataflag);
        
        SS_POPNAME;
        
        return (rc);
}

/*#***********************************************************************\
 * 
 *		blb_copy
 * 
 * Copies Blob List Block
 * 
 * Parameters : 
 * 
 *	block - in out, use
 *		pointer to Blob List Block
 *		
 *      iomgr - in, use
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to filedes object
 *		
 *	srcblock - in, use
 *		pointer to source block
 *		
 *	cpnum - in
 *		check point # to be marked to new block
 *		
 *	blobid - in
 *		blob ID for the new blob
 *
 *      log - in, use
 *          pointer to logical log object
 *
 *      logdataflag - in
 *          TRUE when BLOB data logging is requested or
 *          FALSE when not.
 *
 *      isstartblock - in
 *          TRUE when the blob is 1st block of the BLOB or
 *          FALSE otherwise
 *
 *      trxid - in
 *          transaction ID
 *
 *	p_daddr - out, use
 *		pointer to variable where the disk address of the copied block
 *          will be stored
 *
 *      blob - in out, use
 *          pointer to blob object
 *
 * Return value : 
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static dbe_ret_t blb_copy(
        rs_sysi_t* cd,
        dbe_blobblock_t* block,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_blobblock_t* srcblock,
        dbe_cpnum_t cpnum,
        dbe_blobid_t blobid,
        dbe_log_t* log,
        bool logdataflag,
        bool isstartblock,
        dbe_trxid_t trxid,
        su_daddr_t* p_daddr,
        dbe_blob_t* blob)
{
        dbe_ret_t rc;
        dbe_ret_t rc2;
        su_daddr_t src_daddr;
        su_daddr_t dest_daddr;
        char* p_srcdata;
        char* p_destdata;
        size_t datasize;
        size_t dataoffset;
        dbe_bl_nblocks_t i;
        dbe_blobblock_t* datablock;
        dbe_info_t info;

        dbe_info_init(info, 0);
        ss_dprintf_3(("blb_copy\n"));
        ss_dassert(block != NULL);
        ss_dassert(filedes != NULL);
        ss_dassert(block->blb_cacheslot == NULL);
        ss_dassert(block->blb_type == DBE_BLOCK_BLOBLIST);
        ss_dassert(srcblock != NULL);
        ss_dassert(srcblock->blb_type == DBE_BLOCK_BLOBLIST);
        ss_dassert(srcblock->blb_cacheslot != NULL);
        ss_dassert(p_daddr != NULL);

        rc = DBE_RC_SUCC;
        block->blb_cpnum = cpnum;
        if (*p_daddr == SU_DADDR_NULL) {
            FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
            } else {
                rc = dbe_fl_alloc(filedes->fd_freelist, &block->blb_daddr, &info);
                BLOB_COUNTALLOC(block->blb_daddr, -1);
            }
            if (rc != SU_SUCCESS) {
                return (rc);
            }
        } else {
            block->blb_daddr = *p_daddr;
        }
        block->blb_cpnum = cpnum;
        block->blb_size = srcblock->blb_size;
        block->blb_nblocks = srcblock->blb_nblocks;
        block->blb_nblocks_used = srcblock->blb_nblocks_used;
        block->blb_id = blobid;
        ss_dprintf_1(("blb_copy reach line %d blobid = %ld\n", __LINE__, blob->b_id));
        block->blb_cacheslot =
            dbe_iomgr_reach(
                iomgr,
                block->blb_daddr,
                DBE_CACHE_WRITEONLY,
                0,
                &block->blb_data,
                blob_reachctx);
        p_destdata = block->blb_data + BLB_ALLOCLSTOFFSET;
        for (i = 0;
             i < block->blb_nblocks_used;
             i++, p_destdata += sizeof(su_daddr_t))
        {
            FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
            } else {
                rc = dbe_fl_alloc(filedes->fd_freelist, &dest_daddr, &info);
                BLOB_COUNTALLOC(dest_daddr, block->blb_id);
            }
            if (rc != SU_SUCCESS) {
                while (i) {
                    i--;
                    p_destdata -= sizeof(su_daddr_t);
                    dest_daddr = SS_UINT4_LOADFROMDISK(p_destdata);
                    rc2 = dbe_fl_free(filedes->fd_freelist, dest_daddr);
                    BLOB_COUNTFREE(dest_daddr);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                }
                ss_dprintf_1(("blb_copy release line %d blobid = %ld\n", __LINE__, blob->b_id));
                dbe_iomgr_release(
                    iomgr,
                    block->blb_cacheslot,
                    DBE_CACHE_IGNORE,
                    blob_reachctx);
                block->blb_cacheslot = NULL;
                block->blb_data = NULL;
                rc2 = dbe_fl_free(filedes->fd_freelist, block->blb_daddr);
                BLOB_COUNTFREE(block->blb_daddr);
                su_rc_assert(rc2 == SU_SUCCESS, rc2);
                return (rc);
            }
            SS_UINT4_STORETODISK(p_destdata, dest_daddr);
        }
        dest_daddr = SU_DADDR_NULL;
        for (;
             i < block->blb_nblocks;
             i++, p_destdata += sizeof(su_daddr_t))
        {
            SS_UINT4_STORETODISK(p_destdata, dest_daddr);
        }
        dataoffset = BLB_DATAOFFSET(srcblock);
        p_srcdata = srcblock->blb_data + dataoffset;
        datasize = filedes->fd_blocksize - dataoffset;
        if (datasize > srcblock->blb_size) {
            ss_dassert((size_t)(p_destdata - block->blb_data) == dataoffset);
            ss_dassert(dataoffset + datasize == filedes->fd_blocksize);
            ss_dassert(p_destdata + datasize
                == block->blb_data + filedes->fd_blocksize);
            memset(
                p_destdata + srcblock->blb_size,
                0,
                datasize - srcblock->blb_size);
            datasize = srcblock->blb_size;
        }
        memcpy(p_destdata, p_srcdata, datasize);
        if (srcblock->blb_next != SU_DADDR_NULL) {
            FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
            } else {
                rc = dbe_fl_alloc(filedes->fd_freelist, &block->blb_next, &info);
                BLOB_COUNTALLOC(block->blb_next, block->blb_id);
            }
            if (rc != SU_SUCCESS) {
                i = block->blb_nblocks;
                p_destdata = block->blb_data + BLB_ALLOCLSTOFFSET +
                    sizeof(su_daddr_t) * i;
                while (i) {
                    i--;
                    p_destdata -= sizeof(su_daddr_t);
                    dest_daddr = SS_UINT4_LOADFROMDISK(p_destdata);
                    rc2 = dbe_fl_free(filedes->fd_freelist, dest_daddr);
                    BLOB_COUNTFREE(dest_daddr);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                }
                ss_dprintf_1(("blb_copy release line %d blobid = %ld\n", __LINE__, blob->b_id));
                dbe_iomgr_release(
                    iomgr,
                    block->blb_cacheslot,
                    DBE_CACHE_IGNORE,
                    blob_reachctx);
                block->blb_cacheslot = NULL;
                block->blb_data = NULL;
                rc2 = dbe_fl_free(filedes->fd_freelist, block->blb_daddr);
                su_rc_assert(rc2 == SU_SUCCESS, rc2);
                BLOB_COUNTFREE(block->blb_daddr);
                return (rc);
            }
        } else {
            block->blb_next = SU_DADDR_NULL;
        }
#ifndef SS_NOLOGGING
        if (log != NULL) {
            if (logdataflag) {
                dbe_log_putblobdata(    /* ignore return value! */
                    log,
                    cd,
                    isstartblock ?
                        DBE_LOGREC_BLOBDATA_OLD : DBE_LOGREC_BLOBDATA_CONT_OLD,
                    trxid,
                    p_destdata,
                    datasize);
            }
        }
#endif /* SS_NOLOGGING */
        *p_daddr = block->blb_next;
        p_destdata = block->blb_data + BLB_ALLOCLSTOFFSET;
        p_srcdata = srcblock->blb_data + BLB_ALLOCLSTOFFSET;
        datablock = bdb_init(NULL);
        for (i = 0;
             i < block->blb_nblocks_used;
             i++, p_destdata += sizeof(su_daddr_t),
                p_srcdata += sizeof(su_daddr_t))
        {
            src_daddr = SS_UINT4_LOADFROMDISK(p_srcdata);
            dest_daddr = SS_UINT4_LOADFROMDISK(p_destdata);
            blob->b_curlistpos = i;
            blob_prefetchwait(blob, src_daddr);
            rc = bdb_relocate(
                    cd,
                    datablock,
                    iomgr,
                    filedes,
                    src_daddr,
                    dest_daddr,
                    cpnum,
                    log,
                    logdataflag,
                    trxid);
            blob_prefetch(blob);
            if (rc != SU_SUCCESS) {
                i = block->blb_nblocks;
                p_destdata = block->blb_data + BLB_ALLOCLSTOFFSET +
                    sizeof(su_daddr_t) * i;
                while (i) {
                    i--;
                    p_destdata -= sizeof(su_daddr_t);
                    dest_daddr = SS_UINT4_LOADFROMDISK(p_destdata);
                    rc2 = dbe_fl_free(filedes->fd_freelist, dest_daddr);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                    BLOB_COUNTFREE(dest_daddr);
                }
                dbe_iomgr_release(
                    iomgr,
                    block->blb_cacheslot,
                    DBE_CACHE_IGNORE,
                    blob_reachctx);
                block->blb_cacheslot = NULL;
                block->blb_data = NULL;
                rc2 = dbe_fl_free(filedes->fd_freelist, block->blb_daddr);
                su_rc_assert(rc2 == SU_SUCCESS, rc2);
                BLOB_COUNTFREE(block->blb_daddr);
                if (block->blb_next != SU_DADDR_NULL) {
                    rc2 = dbe_fl_free(filedes->fd_freelist, block->blb_next);
                    su_rc_assert(rc2 == SU_SUCCESS, rc2);
                    BLOB_COUNTFREE(block->blb_next);
                }
                bb_done(datablock);
                return (rc);
            }
        }
        ss_dprintf_1(("blb_copy call to bb_releasefromwrite line %d\n", __LINE__));
        bb_releasefromwrite(
            block,
            iomgr,
            filedes,
            cpnum,
            block->blb_nblocks_used == 0 ?
                (size_t)block->blb_size :
                (size_t)(filedes->fd_blocksize - BLB_DATAOFFSET(block)),
            FALSE);
        bb_done(datablock);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_copyblob_init
 * 
 * Creates a copyblob object
 * 
 * Parameters : 
 * 
 *      iomgr - in, hold
 *          pointer to I/O manager object
 *
 *	filedes - in, use
 *		pointer to filedes
 *		
 *	counter - in out, use
 *		pointer to counter object
 *		
 *	daddr - in
 *		start disk address of blob to be copied
 *		
 *	log - in out, use
 *		pointer to log file object or NULL if no logging
 *		
 *	logdataflag - in
 *		TRUE if the data is to be logged or
 *          FALSE otherwise
 *		
 *	trxid - in
 *		transaction ID
 *		
 *	p_blobid - out, use
 *		pointer to blob ID of the new BLOB
 *		
 *	p_blobsize - out, use
 *		pointer to size of the blob
 *		
 * Return value - give :
 *      pointer to copyblob object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_copyblob_t* dbe_copyblob_init(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        su_daddr_t daddr,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid,
        dbe_blobid_t* p_blobid,
        dbe_blobsize_t* p_blobsize,
        int prefetchsize)
{
        dbe_copyblob_t* blob;
        dbe_ret_t rc;
        char* buf;
        size_t bufsize;

        ss_dprintf_2(("dbe_copyblob_init\n"));
        blob = blob_init(
                    iomgr,
                    filedes,
                    counter,
                    BMODE_COPY,
                    daddr,
                    DBE_BLOBSIZE_UNKNOWN,
                    log,
                    logdataflag,
                    trxid);
        blob->b_id = dbe_counter_getnewblobid(counter);
        ss_dprintf_1(("dbe_copyblob_init: blobid = %lu\n", (ulong)blob->b_id));
        FAKE_BLOBID_ADD(blob->b_id);
        blob->b_prefetchsize = prefetchsize;
        if (p_blobid != NULL) {
            *p_blobid = blob->b_id;
        }
        blob->b_startblock = blb_init(NULL);
        rc = bb_reachforread(
                blob->b_startblock,
                blob->b_iomgr,
                blob->b_filedes,
                blob->b_startaddr,
                &buf,
                &bufsize);
        if (bb_gettype(blob->b_startblock) == DBE_BLOCK_BLOBLIST) {
            blob->b_curlistblock = blob->b_startblock;
            blob->b_curlistpos = B_CURLISTPOS_SELF;
            blob_prefetch(blob);
        }
        su_rc_assert(rc == DBE_RC_SUCC, rc);
        if (p_blobsize != NULL) {
            *p_blobsize = blob->b_startblock->blb_size;
        }
        return (blob);
}

/*##**********************************************************************\
 * 
 *		dbe_copyblob_done
 * 
 * Deletes a copyblob object
 * 
 * Parameters : 
 * 
 *	blob - 
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
void dbe_copyblob_done(
        dbe_copyblob_t* blob)
{
        ss_dprintf_2(("dbe_copyblob_done\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_COPY);
        if (blob->b_startblock != NULL) {
            if (blob->b_startblock->blb_cacheslot != NULL) {
                bb_releasefromread(
                    blob->b_startblock,
                    blob->b_iomgr,
                    blob->b_filedes);
                bb_done(blob->b_startblock);
                blob->b_startblock = NULL;
            }
        }
        blob_done(blob);
}
/*##**********************************************************************\
 * 
 *		dbe_copyblob_copy
 * 
 * Copies a BLOB
 * 
 * Parameters : 
 * 
 *	blob - in out, use
 *		pointer to copyblob object
 *		
 *	p_daddr - out, use
 *		pointer to disk address of the new blob
 *		
 * Return value : 
 *      DBE_RC_SUCC when OK or
 *      error code otherwise
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_copyblob_copy(
        dbe_copyblob_t* blob,
        su_daddr_t* p_daddr)
{
        dbe_blobblock_t* srclistblock;
        dbe_blobblock_t* destlistblock;
        char* buf;
        size_t bufsize;
        dbe_ret_t rc = 0;
        su_daddr_t tmp_daddr;
        su_daddr_t daddr;
        dbe_cpnum_t cpnum;
        long i;

        ss_dprintf_2(("dbe_copyblob_copy\n"));
        ss_dassert(blob != NULL);
        ss_dassert(blob->b_mode == BMODE_COPY);

        cpnum = dbe_counter_getcpnum(blob->b_counter);

        destlistblock = blb_init(NULL);
        daddr = blob->b_startaddr;
        tmp_daddr = SU_DADDR_NULL;
        i = 0;
        do {
            if (i == 0) {
                srclistblock = blob->b_startblock;
                blob->b_startblock = NULL;
                ss_dassert(srclistblock != NULL);
            } else {
                blob_prefetchwait(blob, daddr);
                rc = bb_reachforread(
                        srclistblock,
                        blob->b_iomgr,
                        blob->b_filedes,
                        daddr,
                        &buf,
                        &bufsize);
                blob->b_prefetchnext = 0;
                blob_prefetch(blob);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
            }
            switch (bb_gettype(srclistblock)) {
                case DBE_BLOCK_BLOBLIST:
                    rc = blb_copy(
                            blob->b_cd,
                            destlistblock,
                            blob->b_iomgr,
                            blob->b_filedes,
                            srclistblock,
                            cpnum,
                            blob->b_id,
                            blob->b_log,
                            blob->b_logdataflag,
                            (i == 0),   /* first block ? */
                            blob->b_trxid,
                            &tmp_daddr,
                            blob);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                    break;
                case DBE_BLOCK_BLOBDATA:
                    ss_dassert(i);
                    bb_releasefromread(
                            srclistblock,
                            blob->b_iomgr,
                            blob->b_filedes);
                    rc = bdb_relocate(
                            blob->b_cd,
                            srclistblock,
                            blob->b_iomgr,
                            blob->b_filedes,
                            daddr,
                            tmp_daddr,
                            cpnum,
                            blob->b_log,
                            blob->b_logdataflag,
                            blob->b_trxid);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                    tmp_daddr = SU_DADDR_NULL;
                    break;
                default:
                    ss_rc_error(bb_gettype(srclistblock));
                    break;
            }
            if (i == 0) {
                *p_daddr = destlistblock->blb_daddr;
            }
            daddr = srclistblock->blb_next;
            bb_releasefromread(
                    srclistblock,
                    blob->b_iomgr,
                    blob->b_filedes);
            i++;
        } while (tmp_daddr != SU_DADDR_NULL);
        blob->b_curlistblock = NULL;
        bb_done(srclistblock);
        bb_done(destlistblock);
        return (rc);
}

#endif /* SS_NOBLOB */
