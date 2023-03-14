/**************************************************************************
**  source       * dbe8flst.c
**  directory    * dbe
**  description  * Freelist objects
**               * 
**               * Copyright (C) 2006 Solid Information Technology Ltd
**************************************************************************/
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
Implementation:
--------------

This module implements 'freelist' objects which will be used
in parallel with the 'cache' objects from "dbe8cach.h" and partially
with the 'splitted virtual file' objects from "su0svfil.h".

A freelist object offers services for:
        - allocating a free block from the database file.
        - freeing a previously allocated allocated block of the file.
        - saving the freelist entry to disk for making checkpoints.

A freelist object is a list of so called 'superblocks'. A superblock
is a special kind of a free block, which contains a table of as many
free block addresses as fits into it. the superblock also contains
the address of the next superblock in the list. Also the number of
blocks stored in the superblock is held in the superblock.
A special address value called 'SU_DADDR_NULL' in the 'next' pointer
indicates the superblock is the last one in the list.

The first of the superblocks is kept in main memory to obtain fast
access to free blocks. The memory-resident superblock is twice as big
as a superblock on disk. If the memory part becomes full, only a half
of the block is flushed to disk and thus the memory resident block
still holds a cache of free block addresses. If the memory resident
superblock becomes empty and a new block is requested the next
superblock is loaded into memory from disk (if there is one).
The superblocks keep all addresses locally sorted so that the first
address is the highest address in that block and the last used array
entry is the lowest. Free blocks are always allocated from the end of
the array thus trying to allocate lowest addresses first. The part of the
memory-resident superblock which is written to disk is allocated from
the beginning of the array so the lowest addresses are kept in the
beginning of the list.

If a new block is requested and the end of free list is reached, the
allocator service grows the database file by a configurable amount of
disk blocks. The disk blocks are then inserted into the free list and
the allocator gives one of them to the requester.

The free list superblocks have a checkpoint number field in them.
Whenever the save service is called and a new checkpoint number is
assigned to it, the succeeding superblocks that are written to disk
have the new checkpoint number. A superblock that has a lower
checkpoint number than the newest checkpoint number will never
be overwritten. If such a block needs to be written from memory to
disk, a new copy is created.  The strategy is called 'shadow paging'
and will be documented in more detail elsewhere. The newest checkpoint
number actually refers to the next checkpoint we shall create in the
future.

The client application for freelist is allowed to be multithreaded,
because all critical sections are automatically mutexed.
Local functions are assumed to be called inside mutex:ed sections.

The file size to be recorded upon creation of a checkpoint must
be the value provided by the save service. The physical file
size must not be used, because it may be bigger.

Limitations:
-----------

Because of recursion between change list and free list some operations
have to be deferred. The methods ending with '_deferch' are designed for
this purpose only. They defer the update of change list so that the
recursion will end. They must not be called elsewhere!

Error handling:
--------------

All file system errors are returned as they come from the routine
that caused them. All internal errors cause an assertion failure.

Objects used:
------------

Splittable virtual files <su0svfil.h>
Disk cache system "dbe8cach.h"
Portable semaphore functions <sssem.h>
Binary search function su_bsearch() <su0bsrch.h>

Preconditions:
-------------

Splittable virtual file and its subobjects must be initialized before
the freelist.
Cache system must be initialized before the freelist.

Multithread considerations:
--------------------------

All Critical sections are automatically mutexed.

Example:
-------

#include <su0flst.h>

main()
{
        su_svfil_t *svfp;
        dbe_cache_t *cache;
        dbe_freelist_t freelist;
        su_daddr_t fsize_at_sp;
        su_daddr_t freelist_start;
        dbe_cpnum_t cpnum;

        ...

        /* Initialize splittable virtual file and cache
        ** and fsize_at_sp and freelist_start and cpnum
        */

        freelist = dbe_fl_init(svfp, cache, freelist_start,
                               fsize_at_sp, 100, 4, cpnum);

        ... /* Run database engine and call freelist sercvices */

        dbe_fl_done(freelist);

        ... /* cleanup svfp, cache etc.. */
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <sssem.h>
#include <ssmem.h>
#include <ssstdlib.h>
#include <ssstring.h>
#include <ssdebug.h>
#include <su0bsrch.h>
#include <su0rbtr.h>
#include <su0error.h>
#include <su0bmap.h>
#include "dbe9bhdr.h"
#include "dbe9blst.h"
#include "dbe9crec.h"
#include "dbe8flst.h"
#include "dbe8clst.h"
#include "dbe0db.h"
#include "dbe0erro.h"


#ifdef SS_DEBUG
static char flst_setchkctx[] = "Free list set chk";
#ifndef SS_MYSQL
static char flst_getchkctx[] = "Free list get chk";
#endif
static char flst_readctx[] = "Free list read";
static char flst_writectx[] = "Free list write";
#else
#define flst_setchkctx NULL
#define flst_getchkctx NULL
#define flst_readctx NULL
#define flst_writectx NULL
#endif

bool dbe_debug;
ss_debug(bool dbg_neverfree = FALSE;)

/* Index constants in superblock addresstable */

/* Disk pointer special value */

#ifndef NO_ANSI

static su_ret_t dbe_fl_freelocal(
        dbe_freelist_t *p_fl,
        su_daddr_t daddr);

static int dbe_fl_daddrcmp(
        const void *p_key,
        const void *p_datum);

static void dbe_fl_insert(
        dbe_freelist_t *p_fl,
        su_daddr_t block_daddr);

static su_daddr_t dbe_fl_extract(
        dbe_freelist_t *p_fl);

static su_ret_t dbe_fl_read1superblock(
        dbe_freelist_t *p_fl);

static su_ret_t dbe_fl_read_all_superblocks(
        dbe_freelist_t *p_fl);

static su_ret_t dbe_fl_write1superblock(
        dbe_freelist_t *p_fl);

static su_ret_t dbe_fl_write_all_superblocks(
        dbe_freelist_t *p_fl);

static void dbe_fl_addtochlist(
        dbe_freelist_t *p_fl,
        dbe_cpnum_t cpnum,
        su_daddr_t daddr);

static void dbe_fl_chrecfree(
        void *p);

#else /* NO_ANSI */

static su_ret_t dbe_fl_freelocal();
static int dbe_fl_daddrcmp();
static void dbe_fl_insert();
static su_daddr_t dbe_fl_extract();
static su_ret_t dbe_fl_read1superblock();
static su_ret_t dbe_fl_write1superblock();
static void dbe_fl_addtochlist();
static void dbe_fl_chrecfree();

#endif /* NO_ANSI */


/* structure to represent free list */
struct dbe_freelist_st {
        su_svfil_t         *fl_file;        /* ptr to database file obj. */
        dbe_cache_t        *fl_cache;       /* ptr to cacher */
        SsSemT             *fl_mutex;       /* MUTual EXclusion semaphore */
        size_t              fl_blocksize;   /* db block size (redundant) */
        size_t              fl_sbcapacity;  /* superblock capacity */
        size_t              fl_mycapacity;  /* max # of free blocks I store */
        su_daddr_t          fl_filesize;    /* logical file size in blocks */
        uint                fl_extendincr;  /* file size extend increment */
        dbe_cpnum_t         fl_nextcpnum;   /* next checkpoint number */
        bool                fl_modified;    /* modified flag */
        su_daddr_t          fl_readfrom;    /* address where we read this
                                            ** (relevant if !fl_modified)
                                            */
        su_list_t          *fl_deferchlist; /* deferred change list entries */
        dbe_chlist_t       *fl_chlist;      /* ptr to change list object */
        bool                fl_inmemory;    /* in-memory flag */
        
        SsSemT*             fl_seqmutex;
        su_rbt_t*           fl_seqrbt;
        uint                fl_maxseqalloc;
        long                fl_freeblocks;
        bool                fl_freeblocks_known;
        int                 fl_maxspareblocks;
        su_ret_t            fl_rc;
        dbe_db_t*           fl_db;

        /* the following fields have 1:1 correspondence with
        ** the superblock disk image except that the addrtable is twice
        ** as big as that of the disk image. (and, of course, we need
        ** to do alignment and big/small-endian transformations on some
        ** processor architectures)
        ** NOTE the actual size of the addrtable is not 1;
        ** The real size is calculated from the db file blocksize
        ** upon creation of the freelist object. The macro
        ** SIZEOF_DBE_FREELIST_T(blocksize) is used for calculating
        ** the memory image size of the object.
        */
        dbe_blheader_t      fl_header;
        su_bmap_t           *fl_map;       /* in-memory bit map. Size is
                                            * fl_filesize. */
        uint                fl_map_hint;   /* in-memory search hint */
        ss_debug(int        fl_nalloc;)
        ss_debug(int        fl_locked;)
        su_daddr_t          *fl_addrtable; /* actually bigger !!! */
};

typedef ss_uint4_t dbe_fl_nfreeblocks_t;

/*#**********************************************************************\
 * 
 *		FL_SBCAPACITY
 * 
 * Macro for getting number of free blocks that fit into the array
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		database file block size
 *
 * Return value :
 *          max capacity of the array
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define FL_SBCAPACITY(blocksize) \
        (((blocksize) - DBE_BLIST_DATAOFFSET) / sizeof(su_daddr_t))

/*#**********************************************************************\
 * 
 *		SIZEOF_DBE_FREELIST_T
 * 
 * Macro for calculating the size of the structure needed to represent
 * the dbe_freelist_t
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		database file block size
 *
 * Return value :
 *          size of free list actual structure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
#define SIZEOF_DBE_FREELIST_T(blocksize) \
        (sizeof(dbe_freelist_t) + \
         (2 * FL_SBCAPACITY(blocksize) + 1) * sizeof(su_daddr_t))


#ifdef SS_DEBUG


#ifndef NO_ANSI
static void dbe_fl_print(dbe_freelist_t *p_fl);
#ifndef SS_MYSQL
static void dbe_fl_setcheckmark(dbe_freelist_t *p_fl, su_daddr_t addr, dbe_blocktype_t btype);
static dbe_blocktype_t dbe_fl_getcheckmark(dbe_freelist_t *p_fl, su_daddr_t addr);
#endif
#else
static void dbe_fl_printlist();
static void dbe_fl_print();
static void dbe_fl_setcheckmark();
static dbe_blockype_t dbe_fl_getcheckmark();
#endif

/*##**********************************************************************\
 * 
 *		dbe_fl_printlist
 * 
 * prints contents of the whole freelist
 * 
 * Parameters : 
 * 
 *	p_svfile - in out, use
 *		pointer to splittable virtual file object
 *
 *	p_cache - in out, use
 *		pointer to cache object
 *
 *	superblock_disk_addr - in
 *		address of the 1st block of freelist
 *
 *	file_size_at_checkpoint - in 
 *		The logical file size when SP was created
 *
 *	extend_increment - in
 *		# of blocks the file grows at 1 step
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
void dbe_fl_printlist(p_svfile, p_cache, superblock_disk_addr,
                      file_size_at_checkpoint, extend_increment,
                      next_cpnum)
	su_svfil_t *p_svfile;
	dbe_cache_t *p_cache;
	su_daddr_t superblock_disk_addr;
	su_daddr_t file_size_at_checkpoint;
	uint extend_increment;
	dbe_cpnum_t next_cpnum;
{
        uint ctr;
        dbe_freelist_t *p_fl =
            dbe_fl_init(p_svfile, p_cache, superblock_disk_addr,
                        file_size_at_checkpoint, extend_increment, 4,
                        FALSE, next_cpnum,
                        NULL);
        for (ctr = 1; ; ctr++) {
            ss_dprintf_4(("freelist %d%s node :\n",
                        ctr,
                        (ctr==1? "st":(ctr==2? "nd":(ctr==3? "rd":"th")))));
            ss_output_2(dbe_fl_print(p_fl));
            if (p_fl->fl_header.bl_next == SU_DADDR_NULL) {
                break;
            }
            p_fl->fl_header.bl_nblocks = 0;
            dbe_fl_read1superblock(p_fl);
        }
        ss_dprintf_4(("End of free list reached\n"));
        dbe_fl_done(p_fl);
}

/*#**********************************************************************\
 * 
 *		dbe_fl_print
 * 
 * prints contents of memory resident part of freelist object
 * 
 * Parameters : 
 * 
 *	p_fl - in, use
 *		pointer to freelist object
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_fl_print(p_fl)
	dbe_freelist_t *p_fl;
{
        uint i;

        ss_dprintf(("fl_blocksize = %d\n", p_fl->fl_blocksize));
        ss_dprintf(("fl_sbcapacity = %d\n", p_fl->fl_sbcapacity));
        ss_dprintf(("fl_mycapacity = %d\n", p_fl->fl_mycapacity));
        ss_dprintf(("fl_filesize = %ld\n", p_fl->fl_filesize));
        ss_dprintf(("fl_extendincr = %d\n", p_fl->fl_extendincr));
        ss_dprintf(("fl_nextcpnum = %ld\n", p_fl->fl_nextcpnum));
        ss_dprintf(("fl_modified = %d\n", p_fl->fl_modified));
        ss_dprintf(("fl_readfrom = %ld\n", p_fl->fl_readfrom));
        ss_dprintf(("fl_map_hint = %ld\n", p_fl->fl_map_hint));
        ss_dprintf(("fl_freeblocks = %ld\n", p_fl->fl_freeblocks));
        ss_dprintf(("fl_header.bl_blocktype = %d\n", (int)p_fl->fl_header.bl_type));
        ss_dprintf(("fl_header.bl_cpnum = %ld\n", p_fl->fl_header.bl_cpnum));
        ss_dprintf(("fl_header.bl_nblocks = %d\n", p_fl->fl_header.bl_nblocks));
        ss_dprintf(("fl_header.bl_next = %ld\n", p_fl->fl_header.bl_next));

        if (!p_fl->fl_inmemory) {
            for (i = 0; i < p_fl->fl_header.bl_nblocks; i++) {
                ss_dprintf(("%ld%c%c",
                    p_fl->fl_addrtable[i],
                    i == (uint)(p_fl->fl_header.bl_nblocks - 1) ? ' ':',',
                    ((i+1) % 10 == 0) || i == (uint)(p_fl->fl_header.bl_nblocks - 1)? '\n':' '));
            }
        } else {
            size_t printed=0;
            for (i=0; i<p_fl->fl_filesize; ++i) {
                if (SU_BMAP_GET(p_fl->fl_map, i)) {
                    ++printed;
                    ss_dprintf(("%d%c ", i, printed%10==0 ? '\n':':'));
                }
            }
        }
}

/*#**********************************************************************\
 * 
 *		dbe_fl_setcheckmark
 * 
 * Sets checkmark into block
 * 
 * Parameters : 
 * 
 *	p_fl - in, use
 *		pointer to freelist object
 *
 *	addr - in
 *		disk address of block to mark
 *
 *	btype - in
 *		type field value to set
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#ifndef SS_MYSQL
static void dbe_fl_setcheckmark(
	dbe_freelist_t *p_fl,
	su_daddr_t addr,
	dbe_blocktype_t btype)
{
        dbe_cacheslot_t *cacheslot; /* cache slot from cache */
        void *diskbuf;              /* disk buffer from cacheslot */

        ss_error;   /* temporary assertion */
        cacheslot = dbe_cache_reach(p_fl->fl_cache,
                                    addr,
                                    DBE_CACHE_WRITEONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    flst_setchkctx);
        DBE_BLOCK_SETTYPE(diskbuf,&btype);
        dbe_cache_release(
            p_fl->fl_cache,
            cacheslot,
            DBE_CACHE_FLUSH,
            flst_setchkctx);
}

/*#**********************************************************************\
 * 
 *		dbe_fl_getcheckmark
 * 
 * Gets type field of disk block
 * 
 * Parameters : 
 * 
 *	p_fl - in, use
 *		pointer to freelist object
 *
 *	addr - in
 *		disk address of block to read
 *
 * Return value :
 *          the type field value of the block
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static dbe_blocktype_t dbe_fl_getcheckmark(p_fl, addr)
	dbe_freelist_t *p_fl;
	su_daddr_t addr;
{
	dbe_blocktype_t btype;
        dbe_cacheslot_t *cacheslot; /* cache slot from cache */
        void *diskbuf;              /* disk buffer from cacheslot */

        cacheslot =
            dbe_cache_reach(p_fl->fl_cache,
                            addr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            (char**)&diskbuf,
                            flst_getchkctx);
        DBE_BLOCK_GETTYPE(diskbuf, &btype);
        dbe_cache_release(
            p_fl->fl_cache,
            cacheslot,
            DBE_CACHE_CLEAN,
            flst_getchkctx);
        return (btype);
}
#endif /* !SS_MYSQL */

#undef dbe_fl_alloc
#undef dbe_fl_free

/*##**********************************************************************\
 * 
 *		dbe_fl_dbgfree
 * 
 * Debugging free which is enabled by #define DBE_FL_DEBUG at compilation
 * of each file that uses the free list
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		freelist
 *		
 *	daddr - in
 *		disk address to free
 *		
 *	file - in
 *		caller source file name
 *		
 *	line - in
 *		caller source file line #
 *		
 * Return value :
 *      as with dbe_fl_free()
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_fl_dbgfree(
    dbe_freelist_t *p_fl,
    su_daddr_t daddr,
    char* file,
    int line)
{
        su_ret_t rc;

        /* you can customize this switch and what it
         * prints on what conditions !
         */
        switch (daddr) {
            case SU_DADDR_NULL:
                ss_dprintf_1(("dbe_fl_free(fl, %ld) called from %s %d\n",
                    daddr, file, line));
                break;
            default:
                break;
        }
        rc = dbe_fl_free(p_fl, daddr);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_dbgalloc
 * 
 * Debugging alloc
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		freelist
 *		
 *	p_daddr - out
 *		pointer to disk address variable
 *		
 *	file - in
 *		caller source file name
 *		
 *	line - in
 *		caller source file line #
 *		
 * Return value : 
 *      as with dbe_fl_alloc()
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_fl_dbgalloc(
    dbe_freelist_t *p_fl,
    su_daddr_t *p_daddr,
    dbe_info_t* info,
    char* file,
    int line)
{
        su_ret_t rc;

        rc = dbe_fl_alloc(p_fl, p_daddr, info);
        if (rc == SU_SUCCESS) {
            switch (*p_daddr) {
                case SU_DADDR_NULL:
                    ss_dprintf_1(("dbe_fl_alloc(fl, %ld) called from %s %d\n",
                        *p_daddr, file, line));
                    break;
                default:
                    break;
            }
        } else {
            ss_dprintf_1(("dbe_fl_alloc(fl, p_daddr) failed; called from %s %d\n",
                file, line));
        }
        return (rc);
}

#endif /* SS_DEBUG */

static long dbe_fl_measurelength_nomutex(dbe_freelist_t* p_fl)
{
        dbe_cacheslot_t *cacheslot; /* cache slot from cache */
        void *diskbuf;              /* disk buffer from cacheslot */
        long length;
        su_daddr_t next_block;

        if (p_fl->fl_inmemory) {
            uint i;

            length = 0;
            for (i=0; i < p_fl->fl_filesize; i++) {
                if (SU_BMAP_GET(p_fl->fl_map, i)) {
                    length++;
                }
            }
            return length;
        }

        ss_dassert(!p_fl->fl_inmemory);
        length = (long)p_fl->fl_header.bl_nblocks;
        next_block = p_fl->fl_header.bl_next;
        while (next_block != SU_DADDR_NULL) {
            dbe_blheader_t blheader;

            cacheslot = dbe_cache_reach(p_fl->fl_cache,
                                        next_block,
                                        DBE_CACHE_READONLY,
                                        DBE_INFO_CHECKPOINT,
                                        (char**)&diskbuf,
                                        flst_readctx);
            dbe_blh_get(&blheader, diskbuf);
            dbe_cache_release(
                p_fl->fl_cache,
                cacheslot,
                DBE_CACHE_CLEAN,
                flst_readctx);
            next_block = blheader.bl_next;
            length += blheader.bl_nblocks;
            if (blheader.bl_cpnum == p_fl->fl_header.bl_cpnum) {
                length++;
            }
            ss_dassert(blheader.bl_cpnum <= p_fl->fl_header.bl_cpnum);
        }
        ss_rc_dassert((long)length >= 0, (int)length);

        return (length);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_init
 * 
 * Creates a new free list
 * 
 * Parameters : 
 * 
 *	p_svfile - in out, hold
 *		pointer to splittable virtual file
 *
 *	p_cache - in out, hold
 *		pointer to cache object 
 *
 *	superblock_disk_addr - in
 *		disk address of first superblock or
 *          SU_DADDR_NULL if none
 *
 *	file_size_at_checkpoint - in
 *		recorded file size from dbe_fl_save()
 *          or 0L if a new file.
 *
 *	extend_increment - in
 *		# of blocks the file size extends
 *
 *	max_seq_alloc - in
 *		# of blocks allocated sequentially, if sequential allocation
 *          is requested
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 * Return value - give :
 *          pointer to the created freelist object
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
dbe_freelist_t *dbe_fl_init(p_svfile, p_cache, superblock_disk_addr,
                            file_size_at_checkpoint, extend_increment,
                            max_seq_alloc, globaly_sorted, next_cpnum, db)
	su_svfil_t *p_svfile;
	dbe_cache_t *p_cache;
	su_daddr_t superblock_disk_addr;
	su_daddr_t file_size_at_checkpoint;
	uint extend_increment;
    uint max_seq_alloc;
    bool globaly_sorted;
	dbe_cpnum_t next_cpnum;
    dbe_db_t* db;
{
        su_ret_t rc;
        size_t blocksize;
        dbe_freelist_t *p_fl;

        ss_dprintf_1(("dbe_fl_init\n"));

        blocksize = su_svf_getblocksize(p_svfile);
        p_fl = SsMemAlloc(SIZEOF_DBE_FREELIST_T(blocksize));
        p_fl->fl_file = p_svfile;
        p_fl->fl_cache = p_cache;
        p_fl->fl_mutex = SsSemCreateLocal(SS_SEMNUM_DBE_FLST);
        p_fl->fl_blocksize = blocksize;
        p_fl->fl_sbcapacity = FL_SBCAPACITY(blocksize);
        p_fl->fl_mycapacity = 2 * p_fl->fl_sbcapacity + 2;
        p_fl->fl_filesize = file_size_at_checkpoint;
        p_fl->fl_extendincr = extend_increment;
        dbe_blh_init(&p_fl->fl_header, 
                     (dbe_blocktype_t)DBE_BLOCK_FREELIST,
                     next_cpnum);
        p_fl->fl_nextcpnum = next_cpnum;
        p_fl->fl_header.bl_next = superblock_disk_addr;
        p_fl->fl_modified = FALSE;
        p_fl->fl_readfrom = SU_DADDR_NULL;
        p_fl->fl_deferchlist = NULL;
        p_fl->fl_chlist = NULL;
        p_fl->fl_freeblocks = 0L;
        p_fl->fl_freeblocks_known = FALSE;
        p_fl->fl_maxspareblocks = 0;
        p_fl->fl_rc = SU_SUCCESS;
        p_fl->fl_db = db;

        p_fl->fl_seqmutex = SsSemCreateLocal(SS_SEMNUM_DBE_FLST_SEQ);
        p_fl->fl_seqrbt = NULL;
        p_fl->fl_maxseqalloc = max_seq_alloc;

        p_fl->fl_inmemory = globaly_sorted;
        p_fl->fl_addrtable = (su_daddr_t*)
                SsMemAlloc(p_fl->fl_mycapacity*sizeof(su_daddr_t));

        ss_debug(p_fl->fl_nalloc = 0;)
        ss_debug(p_fl->fl_locked = FALSE;)
        p_fl->fl_map = NULL;
        p_fl->fl_map_hint = 0;

        if (p_fl->fl_inmemory) {
            p_fl->fl_map = SU_BMAP_INIT(p_fl->fl_filesize, 0);
        }

        if (superblock_disk_addr == SU_DADDR_NULL) {
            p_fl->fl_header.bl_cpnum = next_cpnum;
            rc = SU_SUCCESS;
        } else if (p_fl->fl_inmemory) {
            rc = dbe_fl_read_all_superblocks(p_fl);
        } else {
            rc = dbe_fl_read1superblock(p_fl);
        }
        su_rc_assert(rc == SU_SUCCESS, rc);
        ss_dprintf_2(("p_fl->fl_nextcpnum=%d\n", p_fl->fl_nextcpnum));
        ss_dprintf_2(("p_fl->fl_header.bl_cpnum=%d\n", p_fl->fl_header.bl_cpnum));

        return (p_fl);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_setchlist
 * 
 * Gives change list object pointer to freelist
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	p_chlist - in, hold
 *		pointer to changelist object
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_fl_setchlist(p_fl, p_chlist)
	dbe_freelist_t *p_fl;
	void *p_chlist;
{
        su_list_t *p_deferchlist;

        /***** MUTEXBEGIN *****/
        SsSemEnter(p_fl->fl_mutex);
        p_fl->fl_chlist = (dbe_chlist_t*)p_chlist;
        p_deferchlist = p_fl->fl_deferchlist;
        p_fl->fl_deferchlist = NULL;
        SsSemExit(p_fl->fl_mutex);
        /***** MUTEXEND *******/

        dbe_cl_dochlist(p_fl->fl_chlist, p_deferchlist);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_done
 * 
 * Deletes a freelist object. dbe_fl_save() is assumed to be called
 * before this.
 * 
 * Parameters : 
 * 
 *	p_fl - in, take
 *		pointer to free list object
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
void dbe_fl_done(p_fl)
	dbe_freelist_t *p_fl;
{
        SsSemFree(p_fl->fl_mutex);
        SsSemFree(p_fl->fl_seqmutex);
        SsMemFree(p_fl->fl_addrtable);
        if (p_fl->fl_deferchlist != NULL) {
            su_list_done(p_fl->fl_deferchlist);
        }
        if (p_fl->fl_map) {
            SU_BMAP_DONE(p_fl->fl_map);
        }
        SsMemFree(p_fl);
}

void dbe_fl_setreservesize(
        dbe_freelist_t *p_fl,
        int reservesize)
{
        ss_dprintf_1(("dbe_fl_setreservesize:reservesize=%d\n", reservesize));
        p_fl->fl_maxspareblocks = reservesize;
}

/*##**********************************************************************\
 * 
 *		dbe_fl_free
 * 
 * Frees one disk block to freelist
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist
 *
 *	daddr - in
 *		disk address of block to be freed
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_fl_free(p_fl, daddr)
	dbe_freelist_t *p_fl;
	su_daddr_t daddr;
{
        su_ret_t rc;                /* return code */
        su_list_t *p_deferchlist;

        ss_dprintf_2(("dbe_fl_free(daddr=%ld)\n", daddr));
        ss_dassert((ss_int4_t)daddr >= 0);
        rc = dbe_fl_free_deferch(p_fl, daddr, &p_deferchlist);
        if (p_deferchlist != NULL) {
            dbe_cl_dochlist(p_fl->fl_chlist, p_deferchlist);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *      dbe_fl_remove
 *
 * Removes disk block from the freelist.
 * To be used only for shrinking, othervise it does not make any sence
 * to remove it from the free list without doing real allocation.
 *
 * Parameters :
 *
 *  p_fl - in out, use
 *      pointer to freelist
 *
 *  daddr - in
 *      disk address of block to be freed
 *
 * Return value :
 *
 * Limitations  : none
 *
 * Globals used : none
 */
void dbe_fl_remove(
    dbe_freelist_t *p_fl,
    su_daddr_t daddr)
{
        ss_dprintf_2(("dbe_fl_remove(daddr=%ld)\n", daddr));
        ss_assert (p_fl->fl_inmemory);
        SU_BMAP_SET(p_fl->fl_map, daddr, 0);
        p_fl->fl_freeblocks--;
        ss_dassert(p_fl->fl_freeblocks >= 0);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_alloc
 * 
 * Allocates one disk block from free list or by growing file size
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	p_daddr - out
 *		pointer to variable holding the obtained block address
 *
 * 
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
su_ret_t dbe_fl_alloc(
    dbe_freelist_t *p_fl,
    su_daddr_t *p_daddr,
    dbe_info_t* info)
{
        su_ret_t rc;
        su_list_t *p_deferchlist;

        rc = dbe_fl_alloc_deferch(p_fl, p_daddr, &p_deferchlist, info);
        if (p_deferchlist != NULL) {
            dbe_cl_dochlist(p_fl->fl_chlist, p_deferchlist);
        }
        ss_dassert(*p_daddr < su_svf_getsize(p_fl->fl_file));
        ss_debug(
            if (rc == SU_SUCCESS) {
                ss_dprintf_2(("dbe_fl_alloc(): daddr=%ld\n", *p_daddr));
            }
        );
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_save
 * 
 * Saves current state of free list to disk. Used when eg. when
 * a checkpoint is created.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 *	p_superblock_daddr - out
 *		pointer to varible where the disk address
 *          of the first superblock is stored
 *
 *      p_filesize - out
 *		pointer to variable where the logical
 *          file size is stored
 *
 * 
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_fl_save(p_fl, next_cpnum, p_superblock_daddr, p_filesize)
	dbe_freelist_t *p_fl;
	dbe_cpnum_t next_cpnum;
	su_daddr_t *p_superblock_daddr;
        su_daddr_t *p_filesize;
{
        su_ret_t rc;
        su_list_t *p_deferchlist;

        rc = dbe_fl_save_deferch(p_fl,
                                 next_cpnum,
                                 p_superblock_daddr,
                                 p_filesize,
                                 &p_deferchlist);
        if (p_deferchlist != NULL) {
            dbe_cl_dochlist(p_fl->fl_chlist, p_deferchlist);
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_flgetfreeblocks
 * 
 * Returns number of free blocks in free list object.
 * 
 * Parameters : 
 * 
 *	p_fl - 
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
ulong dbe_fl_getfreeblocks(
    dbe_freelist_t *p_fl)

{
        ulong freeblocks;

        SsSemEnter(p_fl->fl_mutex);
        if (!p_fl->fl_freeblocks_known) {
            p_fl->fl_freeblocks =
                dbe_fl_measurelength_nomutex(p_fl);
            ss_dassert(p_fl->fl_freeblocks >= 0);
            p_fl->fl_freeblocks_known = TRUE;
        }
        freeblocks = (ulong)p_fl->fl_freeblocks;
        SsSemExit(p_fl->fl_mutex);
        ss_rc_dassert((long)freeblocks >= 0, (int)freeblocks);
        return(freeblocks);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_free_deferch
 * 
 * Frees one disk block to freelist
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist
 *
 *	daddr - in
 *		disk address of block to be freed
 *
 *      p_chl - out, give
 *		pointer to memory linked list where changes are added
 *
 * 
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : Only to be used internally and from change list routines
 * 
 * Globals used : none
 */
su_ret_t dbe_fl_free_deferch(p_fl, daddr, p_chl)
	dbe_freelist_t *p_fl;
	su_daddr_t daddr;
        su_list_t **p_chl;
{
        su_ret_t rc;                /* return code */

        /***** MUTEXBEGIN *****/
        SsSemEnter(p_fl->fl_mutex);
        ss_dprintf_2(("enter dbe_fl_free_deferch(daddr=%ld)\n", daddr));
        p_fl->fl_freeblocks++;
        rc = dbe_fl_freelocal(p_fl, daddr);
        *p_chl = p_fl->fl_deferchlist;
        p_fl->fl_deferchlist = NULL;
        SsSemExit(p_fl->fl_mutex);
        /***** MUTEXEND *******/
        return (rc);
}

/*#***********************************************************************\
 * 
 *		fl_allocate_from_memory
 * 
 * Allocates one block from in-memory block cache.
 * 
 * Parameters : 
 * 
 *	p_fl - 
 *		
 *		
 *	p_daddr - 
 *		
 *		
 *	endoflist - 
 *		
 *		
 *	limit - 
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
static su_ret_t fl_allocate_from_memory(
	dbe_freelist_t *p_fl,
	su_daddr_t *p_daddr,
    bool* endoflist,
    int limit)
{
        su_ret_t rc;
        bool gotten;

        ss_dprintf_2(("fl_allocate_from_memory:limit=%d\n", limit));
        rc = SU_SUCCESS;
        gotten = FALSE;
        *endoflist = FALSE;

        do {
            if (p_fl->fl_inmemory && p_fl->fl_header.bl_nblocks <= limit) {
                /* False alarm. */
                ss_dassert(sizeof(p_fl->fl_header.bl_nblocks == 2));
                p_fl->fl_header.bl_nblocks = (dbe_bl_nblocks_t)((1>>16)-1);
                if (p_fl->fl_header.bl_nblocks > p_fl->fl_mycapacity) {
                    p_fl->fl_header.bl_nblocks = (dbe_bl_nblocks_t)p_fl->fl_mycapacity;
                }
                if (p_fl->fl_freeblocks > p_fl->fl_header.bl_nblocks) {
                    p_fl->fl_freeblocks = p_fl->fl_header.bl_nblocks;
                    ss_dassert(p_fl->fl_freeblocks >= 0);
                }
                ss_dprintf_2(("fl_allocate_from_memory: false alarm "
                        "fl_freeblocks=%d bl_nblocks=%d\n",
                        (int)p_fl->fl_freeblocks, (int)p_fl->fl_header.bl_nblocks
                ));
            }
            if (p_fl->fl_header.bl_nblocks > limit) {
                ss_dprintf_2(("fl_allocate_from_memory:take free block\n"));
                *p_daddr = dbe_fl_extract(p_fl);
                if (*p_daddr == SU_DADDR_NULL) {
                    *endoflist = TRUE;
                    break;
                }
                gotten = TRUE;
            } else if (p_fl->fl_header.bl_next != SU_DADDR_NULL &&
                       !p_fl->fl_inmemory)
            {
                /* Superblock in memory is empty;
                ** Take a new superblock from disk
                */
                ss_dprintf_2(("fl_allocate_from_memory:read from disk\n"));
                rc = dbe_fl_read1superblock(p_fl);
            } else {
                ss_dprintf_2(("fl_allocate_from_memory:endoflist\n"));
                *endoflist = TRUE;
            }
        } while (rc == SU_SUCCESS && !gotten && !*endoflist);

        ss_assert(rc!=SU_SUCCESS || *p_daddr != SU_DADDR_NULL || *endoflist);
        return(rc);
}

/*#***********************************************************************\
 * 
 *		fl_handle_disk_error
 * 
 * Handles file allocation error. In some cases when operation can not
 * fail spare blocks are allocated altough other operations will get
 * an error.
 * 
 * Parameters : 
 * 
 *	p_fl - 
 *		
 *		
 *	p_daddr - 
 *		
 *		
 *	info - 
 *		
 *		
 *	rc - 
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
static su_ret_t fl_handle_disk_error(
    dbe_freelist_t *p_fl,
    su_daddr_t *p_daddr,
    dbe_info_t* info,
    su_ret_t rc)
{
        bool canfail;
        bool endoflist;

        canfail = info != NULL &&
                  !SU_BFLAG_TEST(info->i_flags, DBE_INFO_DISKALLOCNOFAILURE);

        ss_dprintf_2(("fl_handle_disk_error:using spare blocks, canfail=%d\n", canfail));

        if (p_fl->fl_rc == SU_SUCCESS) {
            /* First call after failure. */
            ss_dassert(rc != SU_SUCCESS);
            p_fl->fl_rc = rc;
            dbe_db_setoutofdiskspace(p_fl->fl_db, rc);
        }
        ss_dassert(p_fl->fl_rc != SU_SUCCESS);

        if (info != NULL) {
            SU_BFLAG_SET(info->i_flags, DBE_INFO_OUTOFDISKSPACE);
            info->i_rc = p_fl->fl_rc;
        }

        if (canfail) {
            rc = p_fl->fl_rc;
        } else {
            /* Try to allocate spare block. */
            rc = fl_allocate_from_memory(p_fl, p_daddr, &endoflist, 0);
        }
        ss_dprintf_2(("fl_handle_disk_error:rc=%d\n", rc));
        return(rc);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_alloc_deferch
 * 
 * Allocates one disk block from free list or by growing file size
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	p_daddr - out
 *		pointer to variable holding the obtained block address
 *
 *      p_chl - out, give
 *		pointer to memory linked list where changes are added
 *
 * 
 * Return value : SU_SUCCESS when OK or
 *                something else on error
 *                (=return code from failed subroutine)
 * 
 * Limitations  : Only to be used internally and from change list routines
 * 
 * Globals used : none
 */
su_ret_t dbe_fl_alloc_deferch(
    dbe_freelist_t *p_fl,
    su_daddr_t *p_daddr,
    su_list_t **p_chl,
    dbe_info_t* info)
{
        su_ret_t rc;
        su_daddr_t physical_filesize;
        su_daddr_t desired_filesize;
        su_daddr_t block_index;
        bool endoflist;

        rc = p_fl->fl_rc;
        endoflist = FALSE;
        *p_chl = NULL;
        *p_daddr = 0;

        /***** MUTEXBEGIN *****/
        SsSemEnter(p_fl->fl_mutex);
        ss_dprintf_2(("enter dbe_fl_alloc_deferch()\n"));
        ss_debug(p_fl->fl_nalloc++);
        FAKE_CODE_BLOCK(
            FAKE_DBE_FREELISTDISKFULL,
            {
                SsPrintf("FAKE_DBE_FREELISTDISKFULL\n");
                rc = SU_ERR_FILE_WRITE_DISK_FULL;
            }
        );
        if (rc != SU_SUCCESS) {
            /* Already failed and using spare blocks. */
            rc = fl_handle_disk_error(p_fl, p_daddr, info, rc);
            SsSemExit(p_fl->fl_mutex);
            /***** MUTEXEND *******/
            return(rc);
        }
        rc = fl_allocate_from_memory(p_fl, p_daddr, &endoflist, p_fl->fl_maxspareblocks);
        if (endoflist) {
            su_daddr_t old_filesize = p_fl->fl_filesize;
            /* extend file */
            ss_dprintf_2(("dbe_fl_alloc_deferch:extend file\n"));
            physical_filesize = su_svf_getsize(p_fl->fl_file);
            desired_filesize = old_filesize + p_fl->fl_extendincr;
            if (physical_filesize < desired_filesize) {
                rc = su_svf_extendsize(p_fl->fl_file, desired_filesize);
            }
            if (rc == SU_SUCCESS) {
                p_fl->fl_filesize = desired_filesize;
                if (p_fl->fl_inmemory) {
                    p_fl->fl_map = SsMemRealloc(p_fl->fl_map,
                                                desired_filesize/CHAR_BIT+1);
                    for (block_index = desired_filesize;
                         block_index > old_filesize;)
                    {
                        /* Initialize extended map as busy */
                        block_index--;
                        SU_BMAP_SET(p_fl->fl_map, block_index, 0);
                        p_fl->fl_freeblocks++;
                    }
                    if (p_fl->fl_map_hint>block_index) {
                        p_fl->fl_map_hint = block_index;
                        ss_dprintf_3(("dbe_fl_alloc_deferch: set hint to %ld\n",
                                       block_index));
                    }
                }
                for (block_index = desired_filesize;
                     block_index > old_filesize; )
                {
                    block_index--;
                    rc = dbe_fl_freelocal(p_fl, block_index);
                    if (rc != SU_SUCCESS) {
                        break;
                    }
                    p_fl->fl_freeblocks++;
                }
            }
            if (rc == SU_SUCCESS) {
                *p_daddr = dbe_fl_extract(p_fl);
            } else {
                p_fl->fl_filesize = old_filesize;
            }
        }
        if (rc == SU_SUCCESS) {
            ss_debug(
                if (p_fl->fl_freeblocks_known) {
                    ss_rc_assert(p_fl->fl_freeblocks > 0L,
                        (int)p_fl->fl_freeblocks);
                }
            );
            p_fl->fl_freeblocks--;
            ss_dprintf_2(("exit dbe_fl_alloc_deferch() block = %ld\n", *p_daddr));
        }
        *p_chl = p_fl->fl_deferchlist;
        p_fl->fl_deferchlist = NULL;
        if (rc != SU_SUCCESS) {
            rc = fl_handle_disk_error(p_fl, p_daddr, info, rc);
        }
        SsSemExit(p_fl->fl_mutex);
        /***** MUTEXEND *******/
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_save_deferch
 * 
 * Saves current state of free list to disk. Used when eg. when
 * a checkpoint is created.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	next_cpnum - in
 *		next checkpoint number
 *
 *	p_superblock_daddr - out
 *		pointer to varible where the disk address
 *          of the first superblock is stored
 *
 *      p_filesize - out
 *		pointer to variable where the logical
 *          file size is stored
 *
 *      p_chl - out, give
 *		pointer to memory linked list where changes are added
 *
 * 
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : Only to be used internally and from change list routines
 * 
 * Globals used : none
 */
su_ret_t dbe_fl_save_deferch(p_fl,
                             next_cpnum,
                             p_superblock_daddr,
                             p_filesize,
                             p_chl)
	dbe_freelist_t *p_fl;
	dbe_cpnum_t next_cpnum;
	su_daddr_t *p_superblock_daddr;
    su_daddr_t *p_filesize;
    su_list_t **p_chl;
{
        su_ret_t rc;

        rc = SU_SUCCESS;
        /***** MUTEXBEGIN *****/
        SsSemEnter(p_fl->fl_mutex);
        ss_dprintf_2(("dbe_fl_save_deferch() next_cpnum = %ld\n", next_cpnum));
        ss_output_4(dbe_fl_print(p_fl));
        if (p_fl->fl_modified || p_fl->fl_header.bl_nblocks == 0) {
            /* Modified or empty list */
            if (p_fl->fl_inmemory) {
                rc = dbe_fl_write_all_superblocks(p_fl);
                *p_superblock_daddr = p_fl->fl_readfrom;
            } else {
                while (p_fl->fl_header.bl_nblocks > 0 && rc == SU_SUCCESS) {
                    rc = dbe_fl_write1superblock(p_fl);
                }
                if (rc == SU_SUCCESS) {
                    *p_superblock_daddr = p_fl->fl_header.bl_next;
                }
            }
        } else {    /* unmodified need not be saved */
            *p_superblock_daddr = p_fl->fl_readfrom;
        }
        *p_filesize = p_fl->fl_filesize;
        p_fl->fl_nextcpnum = next_cpnum;
        ss_dprintf_4(("p_fl->fl_nextcpnum=%d\n", p_fl->fl_nextcpnum));
        *p_chl = p_fl->fl_deferchlist;
        p_fl->fl_deferchlist = NULL;
        p_fl->fl_freeblocks_known = FALSE;
        SsSemExit(p_fl->fl_mutex);
        /***** MUTEXEND *******/
        return (rc);
}


/*#**********************************************************************\
 * 
 *		dbe_fl_freelocal
 * 
 * Locally used block free. Needed for concurrency control reasons,
 * because the extern functions are not allowed to call each other.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	daddr - in
 *		address of disk block to be freed
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static su_ret_t dbe_fl_freelocal(p_fl, daddr)
	dbe_freelist_t *p_fl;
	su_daddr_t daddr;
{
        ss_output_4(dbe_fl_print(p_fl));
        ss_dassert(p_fl->fl_inmemory ||
                   p_fl->fl_header.bl_nblocks < p_fl->fl_mycapacity);
        ss_dassert(daddr != SU_DADDR_NULL);
        ss_dprintf_3(("enter dbe_fl_freelocal(daddr=%ld)\n", daddr));

        dbe_cache_ignoreaddr(p_fl->fl_cache, daddr);
#ifdef SS_DEBUG
        if (dbg_neverfree) {
            return (SU_SUCCESS);
        }
#endif
        dbe_fl_insert(p_fl, daddr);
        if (p_fl->fl_header.bl_nblocks < p_fl->fl_mycapacity ||
            p_fl->fl_inmemory)
        {
            return (SU_SUCCESS);
        }
        /* get disk addr for superblock to write */
        return (dbe_fl_write1superblock(p_fl));
}               /* write out half of the memory-resident address table */

/*#**********************************************************************\
 * 
 *		dbe_fl_daddrcmp
 * 
 * Callback comparison function for su_bsearch().
 * 
 * Parameters : 
 * 
 *	p_key - in, use
 *		pointer to key disk address
 *
 *	p_datum - in, use
 *		pointer to disk address in the array
 *
 * Return value :
 *          == 0 if *p_key == *p_datum
 *          <  0 if *p_key > *p_datum
 *          >  0 if *p_key < *p_datum
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static int dbe_fl_daddrcmp(p_key, p_datum)
	const void *p_key;
	const void *p_datum;
{
        if (*(const su_daddr_t*)p_key < *(const su_daddr_t*)p_datum) {
            return (1);
        }
        if (*(const su_daddr_t*)p_key > *(const su_daddr_t*)p_datum) {
            return (-1);
        }
        return (0);
}
static int SS_CLIBCALLBACK fl_daddrcmp_for_qsort(
        const void *p1,
        const void *p2)
{
        if (*(const su_daddr_t*)p1 < *(const su_daddr_t*)p2) {
            return (1);
        }
        if (*(const su_daddr_t*)p1 > *(const su_daddr_t*)p2) {
            return (-1);
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		dbe_fl_insert
 * 
 * Inserts a free block address into the superblock in memory.
 * always keeps the block sorted in descending order.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	block_daddr - in
 *		disk address of block to insert
 *
 * Return value : none
 * 
 * Limitations  : There must be space for 1 addtional block
 *                address in the superblock in memory
 * 
 * Globals used : 
 */
static void dbe_fl_insert(p_fl, block_daddr)
	dbe_freelist_t *p_fl;
	su_daddr_t block_daddr;
{
        uint entry_index;
        bool found = FALSE;

        ss_dprintf_3(("enter dbe_fl_insert(block_addr=%ld)\n", block_daddr));
        ss_output_4(dbe_fl_print(p_fl));
        ss_dassert(p_fl->fl_inmemory ||
                   p_fl->fl_mycapacity > p_fl->fl_header.bl_nblocks);
        ss_dassert(block_daddr != SU_DADDR_NULL);
        ss_dassert(block_daddr<p_fl->fl_filesize);

        if (p_fl->fl_inmemory) {
            ss_assert(block_daddr < p_fl->fl_filesize);
            ss_dassert(!p_fl->fl_locked);
            found = SU_BMAP_GET(p_fl->fl_map, block_daddr);
            SU_BMAP_SET(p_fl->fl_map, block_daddr, 1);
            if (p_fl->fl_map_hint>block_daddr) {
                p_fl->fl_map_hint = block_daddr;
                ss_dprintf_3(("dbe_fl_insert: reset hint to %ld\n", block_daddr));
            }
            p_fl->fl_header.bl_nblocks++;
        } else {
            su_daddr_t *p_entry;

            if (!p_fl->fl_modified
            &&  p_fl->fl_nextcpnum != p_fl->fl_header.bl_cpnum
            &&  p_fl->fl_header.bl_nblocks > 0)
            {
                dbe_fl_addtochlist(p_fl, p_fl->fl_header.bl_cpnum, p_fl->fl_readfrom);
            }

            p_entry = &p_fl->fl_addrtable[p_fl->fl_header.bl_nblocks];
            if (p_fl->fl_header.bl_nblocks>0) {
                if (block_daddr > *(p_entry-1)) {
                    found = su_bsearch(&block_daddr,
                                       p_fl->fl_addrtable,
                                       p_fl->fl_header.bl_nblocks,
                                       sizeof(su_daddr_t),
                                       dbe_fl_daddrcmp,
                                       (void**)&p_entry);
                    if (!found) {
                        entry_index = (uint)(p_entry - p_fl->fl_addrtable);
                        memmove(p_entry + 1, p_entry,
                                (p_fl->fl_header.bl_nblocks - entry_index) * sizeof(su_daddr_t));
                    }
                } else if (block_daddr == *(p_entry-1)) {
                    found = TRUE;
                }
            }

            if (!found) {
                *p_entry = block_daddr;
                p_fl->fl_header.bl_nblocks++;
            }
        }

        if (found) {
            /* same block twice! */
            if (dbe_debug) {
                SsDbgMessage("Free list insert failed, address %ld already in free list\n",
                    (long)block_daddr);
                return;
            }
            su_emergency2rc_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_FREELISTDUPLICATE,
                DBE_ERR_DBCORRUPTED
            );
        }

        /* else it's allright as is */
        p_fl->fl_modified = TRUE;
        ss_dprintf_3(("exit dbe_fl_insert()\n"));
        ss_output_4(dbe_fl_print(p_fl));
}

/*#**********************************************************************\
 * 
 *		dbe_fl_extract
 * 
 * Extracts 1 free block address from the superblock in memory.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 * Return value :
 *          extracted free block disk address
 * 
 * Limitations  : at least 1 free block must be available
 * 
 * Globals used : none
 */
static su_daddr_t dbe_fl_extract(p_fl)
	dbe_freelist_t *p_fl;
{
        su_daddr_t ret;

        ss_dprintf_3(("dbe_fl_extract\n"));
        ss_dassert(p_fl->fl_header.bl_nblocks > 0);
        if (!p_fl->fl_modified
         && !p_fl->fl_inmemory 
         &&  p_fl->fl_nextcpnum != p_fl->fl_header.bl_cpnum)
        {
            ss_dprintf_4(("dbe_fl_extract:p_fl->fl_nextcpnum=%d, p_fl->fl_header.bl_cpnum=%d\n",
                p_fl->fl_nextcpnum, p_fl->fl_header.bl_cpnum));
            dbe_fl_addtochlist(p_fl, p_fl->fl_header.bl_cpnum, p_fl->fl_readfrom);
        }
        p_fl->fl_modified = TRUE;
        if (p_fl->fl_inmemory) {
            long pos;
            ss_dprintf_3(("dbe_fl_extract: start with hint %ld\n", p_fl->fl_map_hint));
            pos = su_bmap_findnext (p_fl->fl_map, p_fl->fl_filesize, 1,
                                    p_fl->fl_map_hint);
            ss_dassert(pos==su_bmap_findnext (p_fl->fl_map, p_fl->fl_filesize, 1, 0));
            if (pos!=SU_BMAPRC_NOTFOUND) {
                ret = pos;
                p_fl->fl_header.bl_nblocks--;
                p_fl->fl_map_hint = ret;
                ss_dprintf_3(("dbe_fl_extract: reset hint to %ld\n", ret));
                ss_dassert(!p_fl->fl_locked);
                ss_dassert(SU_BMAP_GET(p_fl->fl_map, ret));
                SU_BMAP_SET(p_fl->fl_map, ret, 0);
            } else {
                p_fl->fl_map_hint = 0;
                ss_dprintf_3(("dbe_fl_extract: reset hint to zero"));
                ret = SU_DADDR_NULL;
            }
        } else {
            p_fl->fl_header.bl_nblocks--; 
            ret = p_fl->fl_addrtable[p_fl->fl_header.bl_nblocks];
        }
        return ret;
}

/*#**********************************************************************\
 * 
 *		dbe_fl_read1superblock
 * 
 * Reads 1 superblock from disk to memory
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : The superblock currently in memory must be empty.
 *                -"- must not be the last in the list
 * 
 * Globals used : none
 */
static su_ret_t dbe_fl_read1superblock(p_fl)
	dbe_freelist_t *p_fl;
{
        dbe_cacheslot_t *cacheslot; /* cache slot from cache */
        void *diskbuf;              /* disk buffer from cacheslot */
        su_daddr_t superblock_daddr; /* address of read-in superblock */
        su_daddr_t old_blocks;
        uchar *p_data_dbuf;
        su_daddr_t* p_data;
        uint i;                     /* index */
        dbe_blheader_t tmp_header;

        ss_dprintf_2(("enter dbe_fl_read1superblock(bl_next addr=%ld)\n",
                     p_fl->fl_header.bl_next));
        ss_output_4(dbe_fl_print(p_fl));
        ss_dassert(p_fl->fl_header.bl_next < p_fl->fl_filesize);

        superblock_daddr = p_fl->fl_header.bl_next;
        cacheslot = dbe_cache_reach(p_fl->fl_cache,
                                    superblock_daddr,
                                    DBE_CACHE_READONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    flst_readctx);

        dbe_blh_get(&tmp_header, diskbuf);
        if (tmp_header.bl_type != (dbe_blocktype_t)DBE_BLOCK_FREELIST) {
            return (SU_ERR_FILE_FREELIST_CORRUPT);
        }
        p_fl->fl_header.bl_cpnum = tmp_header.bl_cpnum;
        p_data_dbuf = (uchar*)diskbuf + DBE_BLIST_DATAOFFSET;

        if (tmp_header.bl_nblocks+p_fl->fl_header.bl_nblocks > p_fl->fl_mycapacity) {
            p_fl->fl_addrtable = SsMemRealloc(p_fl->fl_addrtable, (tmp_header.bl_nblocks+p_fl->fl_header.bl_nblocks+1)*sizeof(su_daddr_t));
            p_fl->fl_mycapacity = tmp_header.bl_nblocks+p_fl->fl_header.bl_nblocks+1;
        }
        p_data = &p_fl->fl_addrtable[p_fl->fl_header.bl_nblocks];
        for (i = (uint)tmp_header.bl_nblocks; i > 0; i--) {
            *p_data = SS_UINT4_LOADFROMDISK(p_data_dbuf);
            ss_dassert(*p_data<p_fl->fl_filesize);
            p_data_dbuf += sizeof(su_daddr_t);
            p_data++;
        }
        old_blocks = p_fl->fl_header.bl_nblocks;
        tmp_header.bl_nblocks = tmp_header.bl_nblocks + p_fl->fl_header.bl_nblocks;
        p_fl->fl_header = tmp_header;
        dbe_cache_release(
            p_fl->fl_cache,
            cacheslot,
            DBE_CACHE_CLEAN,
            flst_readctx);
        p_fl->fl_modified = FALSE;
        p_fl->fl_readfrom = superblock_daddr;

        if (!p_fl->fl_inmemory) {
            qsort(p_fl->fl_addrtable, tmp_header.bl_nblocks,
                  sizeof(su_daddr_t), fl_daddrcmp_for_qsort);
            if (p_fl->fl_header.bl_cpnum == p_fl->fl_nextcpnum) {
                dbe_fl_insert(p_fl, superblock_daddr);
            } else if (p_fl->fl_header.bl_nblocks == 0) {
                dbe_fl_addtochlist(p_fl, p_fl->fl_header.bl_cpnum, superblock_daddr);
            } else {
                /* we defer adding to change list to the moment when the
                ** block is either written back to disk or modified first time
                */
            }
        } else {
            size_t i;
            for (i=old_blocks; i<tmp_header.bl_nblocks; i++) {
                ss_dassert(p_fl->fl_addrtable[i]<p_fl->fl_filesize);
                ss_dassert(!SU_BMAP_GET(p_fl->fl_map, p_fl->fl_addrtable[i]));
                ss_dassert(!p_fl->fl_locked);
                SU_BMAP_SET(p_fl->fl_map, p_fl->fl_addrtable[i], 1);
                if (p_fl->fl_addrtable[i]<p_fl->fl_map_hint) {
                    p_fl->fl_map_hint = p_fl->fl_addrtable[i];
                    ss_dprintf_3(("read1superblock: set hint to %ld\n", p_fl->fl_addrtable[i]));
                }
                ss_dprintf_3(("read1superblock: add page %ld\n", p_fl->fl_addrtable[i]));
            }
            if (p_fl->fl_header.bl_cpnum != p_fl->fl_nextcpnum) {
                dbe_fl_addtochlist(p_fl, p_fl->fl_header.bl_cpnum, superblock_daddr);
            } else {
                dbe_fl_insert(p_fl, superblock_daddr);
            }
        }

        ss_dprintf_3(("exit dbe_fl_read1superblock()\n"));
        ss_output_4(dbe_fl_print(p_fl));

        return (SU_SUCCESS);
}

/*#**********************************************************************\
 *
 *      dbe_fl_read_all_superblocks
 *
 * Reads all superblocks from disk to memory
 *
 * Parameters :
 *
 *  p_fl - in out, use
 *      pointer to freelist object
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 *
 * Limitations  : The superblock currently in memory must be empty.
 *
 * Globals used : none
 */
static su_ret_t dbe_fl_read_all_superblocks(p_fl)
	dbe_freelist_t *p_fl;
{
        su_ret_t rc = SU_SUCCESS;
        su_daddr_t start_form = p_fl->fl_header.bl_next;

        ss_dprintf_3(("enter dbe_fl_read_all_superblocks\n"));

        while (p_fl->fl_header.bl_next != SU_DADDR_NULL) {
            rc = dbe_fl_read1superblock(p_fl);
            if (rc!=SU_SUCCESS) {
                return rc;
            }
        }

        p_fl->fl_readfrom = start_form;

        ss_dprintf_3(("exit dbe_fl_read_all_superblocks\n"));
        return SU_SUCCESS;
}


/*#**********************************************************************\
 * 
 *		dbe_fl_write1superblock_part
 * 
 * Writes one part of superblock from memory to disk (one page)
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	superblock_daddr - in 
 *		disk address where the superblock is to
 *          be stored
 * 
 *  ntowrite - in
 *      number of block addresses to write
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_fl_write1superblock_part(p_fl, superblock_daddr, ntowrite)
	dbe_freelist_t *p_fl;
    su_daddr_t superblock_daddr;
    dbe_fl_nfreeblocks_t ntowrite;
{
        dbe_cacheslot_t *cacheslot; /* cache slot from cache */
        void *diskbuf;              /* disk buffer from cacheslot */
        uchar *p_data_dbuf;
        su_daddr_t *p_data;
        uint i;                     /* index */

        ss_dprintf_3(("enter dbe_fl_write1superblock_part superblock_daddr=%ld\n", superblock_daddr));
        ss_output_4(dbe_fl_print(p_fl));
        ss_dassert(p_fl->fl_modified);

        cacheslot = dbe_cache_reach(p_fl->fl_cache,
                                    superblock_daddr,
                                    DBE_CACHE_WRITEONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    flst_writectx);
        p_fl->fl_header.bl_cpnum = p_fl->fl_nextcpnum;
        ss_dprintf_4(("p_fl->fl_header.bl_cpnum=%d\n", p_fl->fl_header.bl_cpnum));
        dbe_blh_put(&p_fl->fl_header, diskbuf);
        SS_UINT2_STORETODISK((uchar*)diskbuf + DBE_BLIST_NBLOCKSOFFSET,
                             ntowrite);
        
        p_data_dbuf = (uchar*)diskbuf + DBE_BLIST_DATAOFFSET;
        p_data = p_fl->fl_addrtable;
        for (i = (uint)ntowrite; i > 0 ; i--) {
            SS_UINT4_STORETODISK(p_data_dbuf, *p_data);
            ss_dprintf_3(("fl_data[%d] = %ld\n", i, *p_data));
            ss_dassert(*p_data<p_fl->fl_filesize);
            p_data++;
            p_data_dbuf += sizeof(su_daddr_t);
        }
        dbe_cache_release(
            p_fl->fl_cache,
            cacheslot,
            DBE_CACHE_DIRTY,
            flst_writectx);
        p_fl->fl_header.bl_next = superblock_daddr;

        ss_dprintf_3(("exit dbe_fl_write1superblock_part\n"));
        ss_output_4(dbe_fl_print(p_fl));
}

/*#**********************************************************************\
 *
 *      dbe_fl_write1superblock
 *
 * Writes 1 superblock from memory to disk
 *
 * Parameters :
 *
 *  p_fl - in out, use
 *      pointer to freelist object
 *
 *  superblock_daddr - in
 *      disk address where the superblock is to
 *          be stored
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 *
 * Limitations  : none
 *
 * Globals used : none
 */

static su_ret_t dbe_fl_write1superblock(p_fl)
    dbe_freelist_t *p_fl;
{
        su_daddr_t superblock_daddr;
        dbe_fl_nfreeblocks_t new_nblocks;/* new # of block addresses in *p_fl */
        dbe_fl_nfreeblocks_t ntowrite;

        ss_dprintf_3(("enter dbe_fl_write1superblock()\n"));

        superblock_daddr = dbe_fl_extract(p_fl);
        
        ss_dprintf_3(("superblock_Daddr=%ld)\n", superblock_daddr));

        if (p_fl->fl_header.bl_nblocks < p_fl->fl_sbcapacity) {
            ntowrite = p_fl->fl_header.bl_nblocks;
        } else {
            ntowrite = (dbe_fl_nfreeblocks_t)p_fl->fl_sbcapacity;
        }

        dbe_fl_write1superblock_part(p_fl, superblock_daddr, ntowrite);
        new_nblocks = (dbe_fl_nfreeblocks_t)(p_fl->fl_header.bl_nblocks - ntowrite);

        if (new_nblocks > 0) {
            memmove(p_fl->fl_addrtable,
                    p_fl->fl_addrtable + ntowrite,
                    new_nblocks * sizeof(su_daddr_t));
        }
        p_fl->fl_header.bl_nblocks = (dbe_bl_nblocks_t)new_nblocks;
        p_fl->fl_readfrom = SU_DADDR_NULL;
        p_fl->fl_header.bl_next = superblock_daddr;

        ss_dprintf_3(("exit dbe_fl_write1superblock()\n"));

        return (SU_SUCCESS);
}

/*#**********************************************************************\
 *
 *      dbe_fl_write_all_superblocks
 *
 * Writes all free list from memory to disk.
 *
 * Parameters :
 *
 *  p_fl - in out, use
 *      pointer to freelist object
 *
 * Return value :
 *          SU_SUCCESS when OK or
 *          something else on error
 *          (=return code from failed subroutine)
 *
 * Limitations  : none
 *
 * Globals used : none
 */
static su_ret_t dbe_fl_write_all_superblocks(p_fl)
    dbe_freelist_t *p_fl;
{
        su_daddr_t superblock_daddr;
        su_daddr_t page;
        size_t to_write;
        int k;

        ss_debug (
            size_t i_freeblocks;
            su_daddr_t fl_blocks[1000];
            int fl_blocks_n = 0;
        )

        ss_dprintf_3(("enter dbe_fl_write_all_superblocks\n"));

        ss_debug(p_fl->fl_locked = TRUE;)
        if (!p_fl->fl_freeblocks_known) {
            p_fl->fl_freeblocks =
                dbe_fl_measurelength_nomutex(p_fl);
            p_fl->fl_freeblocks_known = TRUE;
        } else {
            ss_dassert(p_fl->fl_freeblocks == dbe_fl_measurelength_nomutex(p_fl));
            ss_dassert(p_fl->fl_freeblocks >= 0);
        }
        to_write = p_fl->fl_freeblocks;
        ss_debug(i_freeblocks = to_write);
        superblock_daddr = 0;

        ss_output_2(dbe_fl_print(p_fl));
        while (1) {
            superblock_daddr = su_bmap_findnext(
                    p_fl->fl_map, p_fl->fl_filesize, 1, superblock_daddr
            );
            ss_debug (
                if ((size_t)fl_blocks_n < sizeof(fl_blocks)/sizeof(fl_blocks[0])) {
                    fl_blocks[fl_blocks_n++] = superblock_daddr;
                }
            )
            ss_dprintf_3(("dbe_fl_write_all_superblocks get page %ld\n",
                          superblock_daddr));
            superblock_daddr++;
            ss_dprintf_3(("dbe_fl_write_all_superblocks capacity %ld towrite %ld\n",
                          p_fl->fl_sbcapacity, to_write));
            if (to_write>p_fl->fl_sbcapacity+1) {
                to_write -= p_fl->fl_sbcapacity+1;
            } else {
                break;
            }
        }

        p_fl->fl_map_hint = superblock_daddr;
        ss_dprintf_2(("dbe_fl_write_all_superblocks: hit set %ld\n", superblock_daddr));
        p_fl->fl_readfrom = 0;

        while (p_fl->fl_header.bl_next != SU_DADDR_NULL) {
            void *diskbuf;
            dbe_cacheslot_t *cacheslot;
            dbe_blheader_t tmp_header;

            superblock_daddr = p_fl->fl_header.bl_next;
            cacheslot = dbe_cache_reach(p_fl->fl_cache,
                                    superblock_daddr,
                                    DBE_CACHE_READONLY,
                                    DBE_INFO_CHECKPOINT,
                                    (char**)&diskbuf,
                                    flst_readctx);
            dbe_blh_get(&tmp_header, diskbuf);
            p_fl->fl_header.bl_cpnum = tmp_header.bl_cpnum;
            p_fl->fl_header.bl_next = tmp_header.bl_next;
            dbe_fl_addtochlist(p_fl, p_fl->fl_header.bl_cpnum, superblock_daddr);
            dbe_cache_release(
                p_fl->fl_cache,
                cacheslot,
                DBE_CACHE_CLEAN,
                flst_setchkctx
            );
        }

        superblock_daddr = 0;

        ss_dassert((size_t)p_fl->fl_freeblocks == i_freeblocks);
        ss_dassert(p_fl->fl_freeblocks >= 0);
        ss_output_2(dbe_fl_print(p_fl));

        for (k=0; TRUE; k++) {
            long bmap_daddr;
            p_fl->fl_header.bl_nblocks--;
            p_fl->fl_freeblocks--;
            ss_dassert(p_fl->fl_freeblocks >= 0);

            bmap_daddr = su_bmap_findnext(
                    p_fl->fl_map, p_fl->fl_filesize, 1, superblock_daddr
            );
            if (bmap_daddr==SU_BMAPRC_NOTFOUND) {
                break;
            }
            superblock_daddr = bmap_daddr;
            ss_debug ({
                if (k == fl_blocks_n) {
                    long bmap_daddr = su_bmap_findnext(
                        p_fl->fl_map, p_fl->fl_filesize, 1, p_fl->fl_map_hint
                    );
                    ss_dassert(bmap_daddr==SU_BMAPRC_NOTFOUND);
                } else {
                    ss_dassert(superblock_daddr == fl_blocks[k]);
                }
            });
            SU_BMAP_SET(p_fl->fl_map, superblock_daddr, 0);

            ss_dprintf_3(("dbe_fl_write_all_superblocks superblock_daddr = %ld\n",
                          superblock_daddr));

            to_write = 0;
            while (to_write < p_fl->fl_sbcapacity) {
                bmap_daddr = su_bmap_findnext(
                    p_fl->fl_map, p_fl->fl_filesize, 1, p_fl->fl_map_hint
                );
                if (bmap_daddr==SU_BMAPRC_NOTFOUND) {
                    break;
                }
                ss_dassert(bmap_daddr>=0);
                ss_debug ({
                    int i;
                    for (i=0; i< (int)fl_blocks_n; i++) {
                        ss_dassert((long)(fl_blocks[i]) != bmap_daddr);
                    }
                })
                page = bmap_daddr;
                p_fl->fl_map_hint = page+1;
                p_fl->fl_addrtable[to_write] = page;
                to_write++;
            }
            dbe_fl_write1superblock_part(p_fl, superblock_daddr, to_write);
            if (to_write<p_fl->fl_sbcapacity) {
                break;
            }
            superblock_daddr++;
        }

        p_fl->fl_map_hint = 0;
        p_fl->fl_readfrom = superblock_daddr;
        ss_dassert(superblock_daddr != 0);
        ss_dprintf_3(("dbe_fl_write_all_superblocks: set hint to zero\n"));

        /* Swap value in the header and new head (now in superblock_daddr) */
        ss_debug(p_fl->fl_locked = FALSE;)
        ss_dprintf_3(("exit dbe_fl_write_all_superblocks\n"));

        return (SU_SUCCESS);
}

/*#**********************************************************************\
 *
 *      dbe_fl_first_free.
 *
 * Returns number of the first free page known, without taking it out from
 * the free list. To be used for shrinking.
 *
 * Parameters :
 *
 *  p_fl - in, use
 *      pointer to freelist object
 *
 * Return value :
 *      page number or SU_DADDR_NULL if the freelist is empty.
 *
 * Limitations  :
 *      To be used ony with in-memory free list, othervise this function just
 *      cannot determine real global minimal page number.
 *
 * Globals used : none
 */
su_daddr_t dbe_fl_first_free(p_fl)
    dbe_freelist_t *p_fl;
{
        su_daddr_t ret;
        ss_assert(p_fl->fl_inmemory);
        if (p_fl->fl_header.bl_nblocks==0) {
            return SU_DADDR_NULL;
        }
        ret = su_bmap_findnext(p_fl->fl_map, p_fl->fl_filesize, 1,
                                p_fl->fl_map_hint);
        ss_dassert(ret==(su_daddr_t)su_bmap_findnext(p_fl->fl_map, p_fl->fl_filesize, 1, 0));
        return ret;
}

/*#**********************************************************************\
 *
 *      dbe_fl_last_busy.
 *
 * Returns number of the last allocated page known, without taking it back to
 * the free list. To be used for shrinking.
 *
 * Parameters :
 *
 *  p_fl - in, use
 *      pointer to freelist object
 *
 * Return value :
 *      page number or SU_DADDR_NULL if the freelist is empty.
 *
 * Limitations  :
 *      To be used ony with in-memory free list, othervise this function just
 *      cannot determine real global minimal page number.
 *
 * Globals used : none
 */
su_daddr_t dbe_fl_last_busy(p_fl)
    dbe_freelist_t *p_fl;
{
        ss_assert(p_fl->fl_inmemory);
        if (p_fl->fl_header.bl_nblocks==0) {
            return SU_DADDR_NULL;
        }
        return su_bmap_findlast(p_fl->fl_map, p_fl->fl_filesize, 0);
}

/*#**********************************************************************\
 *
 *      dbe_fl_is_free.
 *
 * Check whese block is in free list or not.
 *
 * Parameters :
 * 
 *  p_fl - in, use
 *      pointer to freelist object 
 *
 * Return value : 
 *      page number or SU_DADDR_NULL if the freelist is empty.
 *
 * Limitations  :
 *      To be used ony with in-memory free list, othervise this function just
 *      cannot determine real global minimal page number. 
 * 
 * Globals used : none 
 */

bool dbe_fl_is_free(p_fl, daddr)
        dbe_freelist_t *p_fl;
        su_daddr_t daddr;
{
        bool found;
        SsSemEnter(p_fl->fl_mutex);
        ss_assert(daddr<p_fl->fl_filesize);
        if (p_fl->fl_inmemory) {
            found = SU_BMAP_GET(p_fl->fl_map, daddr);
        } else {
            su_daddr_t *p_entry;
            found = su_bsearch(&daddr,
                               p_fl->fl_addrtable,
                               p_fl->fl_header.bl_nblocks,
                               sizeof(su_daddr_t),
                               dbe_fl_daddrcmp,
                               (void**)&p_entry);
            /* Not necessarily FALSE. The address might be on the
             * next freelist page page.
             */
        }
        SsSemExit(p_fl->fl_mutex);
        return found;
}

/*#***********************************************************************\
 * 
 *		dbe_fl_addtochlist
 * 
 * Adds an entry to the deferred change list.
 * 
 * Parameters : 
 * 
 *	p_fl - in out, use
 *		pointer to freelist object
 *
 *	cpnum - in
 *		checkpoint number of replaced block
 *
 *	daddr - in
 *		disk address -"-
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_fl_addtochlist(p_fl, cpnum, daddr)
	dbe_freelist_t *p_fl;
	dbe_cpnum_t cpnum;
	su_daddr_t daddr;
{
        dbe_clrecord_t *p_chrec;

        if (p_fl->fl_deferchlist == NULL) {
            p_fl->fl_deferchlist = su_list_init(dbe_fl_chrecfree);
        }
        ss_dassert(daddr<p_fl->fl_filesize);
        p_chrec = SSMEM_NEW(dbe_clrecord_t);
        p_chrec->clr_cpnum = cpnum;
        p_chrec->clr_daddr = daddr;
        su_list_insertlast(p_fl->fl_deferchlist, p_chrec);
}

/*#***********************************************************************\
 * 
 *		dbe_fl_chrecfree
 * 
 * Frees memory allocated for deferred change list record
 * 
 * Parameters : 
 * 
 *	p - in, take
 *		pointer to change list record
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
static void dbe_fl_chrecfree(p)
	void *p;
{
        SsMemFree(p);
}

/**********************************************************************\
        SEQUENTIAL ALLOCATION ROUTINES
\**********************************************************************/

/* #define FL_SEQ */

#ifdef FL_SEQ

typedef struct {
        su_daddr_t  sa_name;
        int         sa_naddrs;
        su_daddr_t  sa_addrs[1];    /* Actually variable length, depends
                                       on field p_fl->fl_maxseqalloc. */
} fl_seqaddr_t;

static int seq_insertcmp(void* key1, void* key2)
{
        fl_seqaddr_t* sa1 = key1;
        fl_seqaddr_t* sa2 = key2;

        if (sa1->sa_name < sa2->sa_name) {
            return(-1);
        } else if (sa1->sa_name > sa2->sa_name) {
            return(1);
        } else {
            return(0);
        }
}

static int seq_searchcmp(void* search_key, void* rbt_key)
{
        su_daddr_t search_name = (long)search_key;
        fl_seqaddr_t* rbt_sa = rbt_key;

        if (search_name < rbt_sa->sa_name) {
            return(-1);
        } else if (search_name > rbt_sa->sa_name) {
            return(1);
        } else {
            return(0);
        }
}

su_ret_t dbe_fl_seq_alloc(
    dbe_freelist_t *p_fl,
    su_daddr_t prev_daddr,
    su_daddr_t *p_daddr)
{
        su_ret_t rc;
        su_rbt_node_t* n;
        fl_seqaddr_t* sa;

        ss_dprintf_1(("dbe_fl_seq_alloc:prev_addr=%ld\n", prev_daddr));

        SsSemEnter(p_fl->fl_seqmutex);

        if (p_fl->fl_seqrbt == NULL ||
            (n = su_rbt_search(p_fl->fl_seqrbt, (void*)prev_daddr))==NULL)
        {
            rc = DBE_RC_NOTFOUND;
        } else {
            sa = su_rbtnode_getkey(n);
            ss_dassert(sa->sa_naddrs > 0);
            su_rbt_delete(p_fl->fl_seqrbt, n);
            *p_daddr = sa->sa_addrs[p_fl->fl_maxseqalloc - sa->sa_naddrs];
            sa->sa_naddrs--;
            if (sa->sa_naddrs == 0) {
                SsMemFree(sa);
            } else {
                sa->sa_name = *p_daddr;
                su_rbt_insert(p_fl->fl_seqrbt, sa);
            }
            rc = SU_SUCCESS;
            ss_dprintf_2(("dbe_fl_seq_alloc:returned addr=%ld\n", *p_daddr));
        }

        SsSemExit(p_fl->fl_seqmutex);

        return(rc);
}

su_ret_t dbe_fl_seq_create(
    dbe_freelist_t *p_fl,
    su_daddr_t *p_daddr,
    dbe_info_t* info)
{
        int i;
        su_ret_t rc;
        fl_seqaddr_t* sa;
        su_daddr_t daddr;

        ss_dprintf_1(("dbe_fl_seq_create\n"));

        SsSemEnter(p_fl->fl_seqmutex);

        if (p_fl->fl_seqrbt == NULL) {
            p_fl->fl_seqrbt = su_rbt_inittwocmp(
                                seq_insertcmp,
                                seq_searchcmp,
                                NULL);
        }

        rc = dbe_fl_alloc(p_fl, p_daddr, info);

        if (rc == SU_SUCCESS) {
            sa = SsMemAlloc(sizeof(fl_seqaddr_t)
                            + (sizeof(sa->sa_addrs[0])
                               * (p_fl->fl_maxseqalloc - 1)));
            for (i = 0; i < p_fl->fl_maxseqalloc; i++) {
                rc = dbe_fl_alloc(p_fl, &daddr, info);
                if (rc != SU_SUCCESS) {
                    int j;
                    for (j = 0; j < i; j++) {
                        dbe_fl_free(p_fl, sa->sa_addrs[j]);
                    }
                    SsMemFree(sa);
                    break;
                }
                sa->sa_addrs[i] = daddr;
            }
        }

        if (rc == SU_SUCCESS) {
            sa->sa_name = *p_daddr;
            sa->sa_naddrs = p_fl->fl_maxseqalloc;
            su_rbt_insert(p_fl->fl_seqrbt, sa);
            ss_dprintf_2(("dbe_fl_seq_create:returned addr=%ld\n", *p_daddr));
        }

        SsSemExit(p_fl->fl_seqmutex);

        return(rc);
}

void dbe_fl_seq_flush(
    dbe_freelist_t *p_fl,
    su_daddr_t daddr)
{
        int i;
        su_rbt_node_t* n;
        fl_seqaddr_t* sa;

        SsSemEnter(p_fl->fl_seqmutex);

        if (p_fl->fl_seqrbt != NULL) {
            n = su_rbt_search(p_fl->fl_seqrbt, (void*)daddr);
            if (n != NULL) {
                sa = su_rbtnode_getkey(n);
                ss_dassert(sa->sa_naddrs > 0);
                su_rbt_delete(p_fl->fl_seqrbt, n);
                for (i = p_fl->fl_maxseqalloc - sa->sa_naddrs; i < p_fl->fl_maxseqalloc; i++) {
                    dbe_fl_free(p_fl, sa->sa_addrs[i]);
                }
                SsMemFree(sa);
            }
        }

        SsSemExit(p_fl->fl_seqmutex);
}

void dbe_fl_seq_flushall(
    dbe_freelist_t *p_fl)
{
        int i;
        su_rbt_node_t* n;
        fl_seqaddr_t* sa;

        ss_dprintf_1(("dbe_fl_seq_flushall\n"));

        SsSemEnter(p_fl->fl_seqmutex);

        if (p_fl->fl_seqrbt != NULL) {
            while ((n = su_rbt_min(p_fl->fl_seqrbt, NULL)) != NULL) {
                sa = su_rbtnode_getkey(n);
                ss_dassert(sa->sa_naddrs > 0);
                su_rbt_delete(p_fl->fl_seqrbt, n);
                for (i = p_fl->fl_maxseqalloc - sa->sa_naddrs; i < p_fl->fl_maxseqalloc; i++) {
                    dbe_fl_free(p_fl, sa->sa_addrs[i]);
                }
                SsMemFree(sa);
            }
            su_rbt_done(p_fl->fl_seqrbt);
            p_fl->fl_seqrbt = NULL;
        }

        SsSemExit(p_fl->fl_seqmutex);
}

#else /* FL_SEQ */

su_ret_t dbe_fl_seq_alloc(
    dbe_freelist_t *p_fl __attribute__ ((unused)),
    su_daddr_t prev_daddr __attribute__ ((unused)),
    su_daddr_t *p_daddr __attribute__ ((unused)))
{
        return(DBE_RC_NOTFOUND);
}

su_ret_t dbe_fl_seq_create(
    dbe_freelist_t *p_fl __attribute__ ((unused)),
    su_daddr_t *p_daddr __attribute__ ((unused)),
    dbe_info_t* info __attribute__ ((unused)))
{
        return(DBE_RC_NOTFOUND);
}

void dbe_fl_seq_flush(
    dbe_freelist_t *p_fl __attribute__ ((unused)),
    su_daddr_t daddr __attribute__ ((unused)))
{
}

void dbe_fl_seq_flushall(
    dbe_freelist_t *p_fl __attribute__ ((unused)))
{
}

void dbe_fl_set_filesize(
    dbe_freelist_t *p_fl, su_daddr_t filesize)
{
        su_daddr_t i;
        bool out_pages = FALSE;
        ss_assert(filesize<=p_fl->fl_filesize);
        ss_assert(p_fl->fl_inmemory);

        for (i=filesize; i<p_fl->fl_filesize; i++) {
            if (!dbe_fl_is_free(p_fl, i)) {
                ss_dprintf_1(("dbe_fl_set_filesize: page %ld is not free\n", i));
                out_pages = TRUE;
            }
        }
        ss_assert(!out_pages);
        p_fl->fl_freeblocks -= p_fl->fl_filesize-filesize;
        ss_dassert(p_fl->fl_freeblocks >= 0);

        p_fl->fl_header.bl_nblocks = (dbe_bl_nblocks_t)((1>>16)-1);
        if (p_fl->fl_header.bl_nblocks > p_fl->fl_freeblocks) {
            p_fl->fl_header.bl_nblocks = (dbe_bl_nblocks_t)p_fl->fl_freeblocks;
        }

        p_fl->fl_filesize = filesize;
}

#endif /* FL_SEQ */
