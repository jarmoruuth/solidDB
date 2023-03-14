/*************************************************************************\
**  source       * xs2mem.c
**  directory    * xs
**  description  * eXternal Sorter MEMory allocator interface
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


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssc.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssdebug.h>
#include <ssstddef.h>
#include <ssmempag.h>
#include <sscacmem.h>

#include "xs2mem.h"

struct xs_mem_st {
        ulong                   xsm_maxblocks;
        size_t                  xsm_blocksize;
        ulong                   xsm_blocksinuse;
        ulong                   xsm_blocksreserved;
        ulong                   xsm_blocksresonfree;

        void*                   xsm_ctx;
        xs_mem_allocfun_t*      xsm_alloc_f;
        xs_mem_freefun_t*       xsm_free_f;
        xs_hmem_getbuffun_t*    xsm_getbuf_f;

        SsSemT*                 xsm_mutex;
};

typedef struct {
        SsCacMemT* mh_cacmem;
        void* mh_ptr;
} memhandle_t;

static xs_hmem_t* mem_allocfun(void* ctx, void* p_ptr)
{
        xs_mem_t* mem = ctx;
#ifdef USE_PAGED_ALLOCATOR_DIRECTLY
        void* ptr = SsMemPageAlloc(mem->xsm_blocksize);
        if (p_ptr != NULL) {
            *(void**)p_ptr = ptr;
        }
        return (ptr);
#else /* USE_PAGED_ALLOCATOR_DIRECTLY */
        memhandle_t* mh = SSMEM_NEW(memhandle_t);
        mh->mh_cacmem = SsCacMemInit(mem->xsm_blocksize, 1);
        mh->mh_ptr = SsCacMemAlloc(mh->mh_cacmem);
        if (p_ptr != NULL) {
            *(void**)p_ptr = mh->mh_ptr;
        }
        return (mh);
#endif /* USE_PAGED_ALLOCATOR_DIRECTLY */
}

static void mem_freefun(
        void* ctx __attribute__ ((unused)),
        xs_hmem_t* hmem)
{
#ifdef USE_PAGED_ALLOCATOR_DIRECTLY
        xs_mem_t* mem = ctx;
        SsMemPageFree(hmem, mem->xsm_blocksize);
#else /* USE_PAGED_ALLOCATOR_DIRECTLY */
        memhandle_t* mh = hmem;
        SsCacMemFree(mh->mh_cacmem, mh->mh_ptr);
        SsCacMemDone(mh->mh_cacmem);
        SsMemFree(mh);
#endif /* USE_PAGED_ALLOCATOR_DIRECTLY */
}

static void* mem_getbuffun(xs_hmem_t* hmem)
{
#ifdef USE_PAGED_ALLOCATOR_DIRECTLY
        return ((void*)hmem);
#else /* USE_PAGED_ALLOCATOR_DIRECTLY */
        memhandle_t* mh = hmem;
        return (mh->mh_ptr);
#endif /* USE_PAGED_ALLOCATOR_DIRECTLY */
}
/*##**********************************************************************\
 * 
 *		xs_mem_init
 * 
 * Creates a memory manager object for eXternal Sorter
 * 
 * Parameters : 
 * 
 *	max_blocks - in
 *		max # of blocks to be allocated from this object
 *		
 *	block_size - in
 *		block size
 *		
 *	ctx - in, hold
 *		context needed for actual allocation
 *		
 *	p_alloc_f - in, hold
 *		pointer to allocation function that returns a memory
 *          handle
 *		
 *	p_free_f - in, hold
 *		pointer to deallocation function
 *		
 *	p_getbuf_f - in, hold
 *		pointer to functions that gives pointer to memory
 *          block associated with a memory handle
 *		
 * Return value - give:
 *      pointer to created memory manager object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_mem_t* xs_mem_init(
            ulong max_blocks,
            size_t block_size,
            void* ctx,
            xs_mem_allocfun_t* p_alloc_f,
            xs_mem_freefun_t* p_free_f,
            xs_hmem_getbuffun_t* p_getbuf_f)
{
        xs_mem_t* mem;

        mem = SSMEM_NEW(xs_mem_t);
        mem->xsm_maxblocks = max_blocks;
        mem->xsm_blocksize = block_size;
        mem->xsm_blocksinuse = 0L;
        mem->xsm_blocksreserved = 0L;
        mem->xsm_blocksresonfree = 0L;
        if (ctx == NULL) {
            mem->xsm_ctx = mem;
            ss_dassert(p_alloc_f == NULL);
            mem->xsm_alloc_f = mem_allocfun;
            ss_dassert(p_free_f == NULL);
            mem->xsm_free_f = mem_freefun;
            ss_dassert(p_getbuf_f == NULL);
            mem->xsm_getbuf_f = mem_getbuffun;
        } else {
            mem->xsm_ctx = ctx;
            mem->xsm_alloc_f = p_alloc_f;
            mem->xsm_free_f = p_free_f;
            mem->xsm_getbuf_f = p_getbuf_f;
        }
        mem->xsm_mutex = SsSemCreateLocal(SS_SEMNUM_XS_MEM);
        return (mem);
}

/*##**********************************************************************\
 * 
 *		xs_mem_done
 * 
 * Deletes a memory manager object
 * 
 * Parameters : 
 * 
 *	mem - in, take
 *		memory manager object to be deleted
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_mem_done(xs_mem_t* mem)
{
        ss_dassert(mem != NULL);
        ss_dassert(mem->xsm_blocksinuse == 0L);
        SsSemFree(mem->xsm_mutex);
        SsMemFree(mem);
}


/*##**********************************************************************\
 * 
 *		xs_mem_reserve
 * 
 * Reserves a bunch of memory handles to be allocated later. This
 * ensures that the allocations will succeed
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	n - in
 *		# of memory handles to reserve
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when n is bigger than number of available handles
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_mem_reserve(xs_mem_t* mem, ulong n)
{
        bool succp;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        if (mem->xsm_maxblocks -
            mem->xsm_blocksinuse -
            mem->xsm_blocksreserved < n)
        {
            succp = FALSE;
        } else {
            mem->xsm_blocksreserved += n;
            succp = TRUE;
        }
        SsSemExit(mem->xsm_mutex);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_mem_reserveonfree
 * 
 * Leaves a request to reserve n blocks of memory whenever they are freed
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	n - in
 *		# of blocks to request
 *		
 * Return value :
 *      TRUE when successful
 *      FALSE when # of requested blocks exceeds the count
 *            of unrequested blocks in use
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool xs_mem_reserveonfree(xs_mem_t* mem, ulong n)
{
        bool succp;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        if (mem->xsm_blocksinuse - mem->xsm_blocksresonfree < n) {
            succp = FALSE;
        } else {
            mem->xsm_blocksresonfree += n;
            succp = TRUE;
        }
        SsSemExit(mem->xsm_mutex);
        return (succp);
}

/*##**********************************************************************\
 * 
 *		xs_mem_unreserve
 * 
 * Cancels a reservation for n memory handles
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	n - in
 *		number of handles the reservation of which will be cancelled
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_mem_unreserve(xs_mem_t* mem, ulong n)
{
        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        ss_dassert(n <= mem->xsm_blocksreserved);
        mem->xsm_blocksreserved -= n;
        SsSemExit(mem->xsm_mutex);
}

/*#***********************************************************************\
 * 
 *		mem_alloclocal
 * 
 * Allocates a block that is either reserved or not
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	p_buf - out, ref
 *		pointer to pointer where the memory address will be stored
 *		
 *	reserved - in
 *		TRUE when block has been resrved or FALSE otherwise
 *		
 * Return value - give:
 *      memory handle which must be freed using xs_mem_free()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static xs_hmem_t* mem_alloclocal(
        xs_mem_t* mem,
        void* p_buf /* (void**, really) */,
        bool reserved)
{ 
        void* dummy;
        xs_hmem_t* hmem;

        if (p_buf == NULL) {
            p_buf = &dummy;
        }
        ss_dassert(!reserved || mem->xsm_blocksreserved > 0L);
        if (mem->xsm_blocksinuse >= mem->xsm_maxblocks) {
            ss_dassert(!reserved);
            ss_dassert(mem->xsm_blocksinuse == mem->xsm_maxblocks);
            *(void**)p_buf = NULL;
            return (NULL);
        }
        hmem = (*mem->xsm_alloc_f)(mem->xsm_ctx, p_buf);
        if (hmem == NULL) {
            *(void**)p_buf = NULL;
            return (NULL);
        }
        ss_dassert(*(void**)p_buf != NULL);
        mem->xsm_blocksinuse++;
        if (reserved) {
            mem->xsm_blocksreserved--;
        }
        return (hmem);
}

/*##**********************************************************************\
 * 
 *		xs_mem_allocreserved
 * 
 * Alloctes a block of memory that has been reserved with function
 * xs_mem_reserve()
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where address of
 *          memory area associated with returned handle will
 *          be stored
 *		
 * Return value - give :
 *      memory handle which must be freed using xs_mem_free()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_hmem_t* xs_mem_allocreserved(xs_mem_t* mem, void* p_buf)
{
        xs_hmem_t* hmem;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        ss_dassert(mem->xsm_blocksreserved > 0L);
        hmem = mem_alloclocal(mem, p_buf, TRUE);
        SsSemExit(mem->xsm_mutex);
        return (hmem);
}

/*##**********************************************************************\
 * 
 *		xs_mem_alloc
 * 
 * Alloctes a block of memory 
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	p_buf - out, ref
 *		pointer to pointer variable where address of
 *          memory area associated with returned handle will
 *          be stored
 *		
 * Return value - give :
 *      memory handle which must be freed using xs_mem_free()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
xs_hmem_t* xs_mem_alloc(xs_mem_t* mem, void* /* (void**, really) */ p_buf)
{
        xs_hmem_t* hmem;

        SsSemEnter(mem->xsm_mutex);
        hmem = mem_alloclocal(mem, p_buf, FALSE);
        SsSemExit(mem->xsm_mutex);
        return (hmem);
}


/*##**********************************************************************\
 * 
 *		xs_mem_free
 * 
 * Deallocates a memory block
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	hmem - use
 *		memory block handle
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void xs_mem_free(xs_mem_t* mem, xs_hmem_t* hmem)
{
        SsSemEnter(mem->xsm_mutex);
        ss_dassert(mem != NULL);
        ss_dassert(mem->xsm_blocksinuse > 0);
        ss_dassert(hmem != NULL);
        mem->xsm_blocksinuse--;
        if (mem->xsm_blocksresonfree != 0) {
            mem->xsm_blocksresonfree--;
            mem->xsm_blocksreserved++;
        }
        (*mem->xsm_free_f)(mem->xsm_ctx, hmem);
        SsSemExit(mem->xsm_mutex);
}

/*##**********************************************************************\
 * 
 *		xs_hmem_getbuf
 * 
 * Gets a pointer to memory buffer associated with memory handle
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 *	hmem - use
 *		memory handle
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* xs_hmem_getbuf(xs_mem_t* mem, xs_hmem_t* hmem)
{
        void* buf;

        ss_dassert(hmem != NULL);
        SsSemEnter(mem->xsm_mutex);
        buf = (*mem->xsm_getbuf_f)(hmem);
        ss_dassert(buf != NULL);
        SsSemExit(mem->xsm_mutex);
        return (buf);
}

/*##**********************************************************************\
 * 
 *		xs_mem_getmaxblocks
 * 
 * Get max number of memory blocks that can be allocated using this
 * mem mgr object
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ulong xs_mem_getmaxblocks(xs_mem_t* mem)
{
        ulong maxblocks;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        maxblocks = mem->xsm_maxblocks;
        SsSemExit(mem->xsm_mutex);
        return (maxblocks);
}

/*##**********************************************************************\
 * 
 *		xs_mem_getnblocksavail
 * 
 * Get number of available memory blocks
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 * Return value :
 *      # of available memory blocks
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ulong xs_mem_getnblocksavail(xs_mem_t* mem)
{
        ulong navail;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        navail = mem->xsm_maxblocks -
            mem->xsm_blocksinuse -
            mem->xsm_blocksreserved; 
        SsSemExit(mem->xsm_mutex);
        return (navail);
}


/*##**********************************************************************\
 * 
 *		xs_mem_getblocksize
 * 
 * Get memory block size
 * 
 * Parameters : 
 * 
 *	mem - use
 *		memory manager
 *		
 * Return value :
 *      memory block size in bytes
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t xs_mem_getblocksize(xs_mem_t* mem)
{
        size_t blocksize;

        ss_dassert(mem != NULL);
        SsSemEnter(mem->xsm_mutex);
        blocksize = mem->xsm_blocksize;
        SsSemExit(mem->xsm_mutex);
        return (blocksize);
}
