/*************************************************************************\
**  source       * sscacmem.c
**  directory    * ss
**  description  * Memory allocation routines for cache memory.
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

#include "ssenv.h"

#if defined(SS_PAGED_MEMALLOC_AVAILABLE)

#include "sswindow.h"
#include "ssstring.h"
#include "ssc.h"
#include "ssdebug.h"
#include "ssqmem.h"
#include "ssmem.h"
#include "ssltoa.h"
#include "sscacmem.h"
#include "ssmempag.h"

#ifdef SS_MYSQL
#include <su0error.h>
#else
#include "../su/su0error.h"
#endif


#if !(defined(SS_NT) && defined(SS_DEBUG))

#define OS_MAXALLOC (64U * 1024U * 1024U)

#define SIZEOF_CACMEMSTRUCT(totsize) \
        ((ss_byte_t*)&(((SsCacMemT*)NULL)->cm_sysptrarr[\
                               ((totsize) + OS_MAXALLOC - 1) /\
                               OS_MAXALLOC])\
         - (ss_byte_t*)NULL)

struct SsCacMemSt {
        size_t  cm_blocksize;
        size_t  cm_nblocks_left;
        size_t  cm_nblocks_tot;
        size_t  cm_nsysptr;
        void*   cm_sysptrarr[1];
};

SsCacMemT* SsCacMemInit(size_t blocksize, size_t nblocks)
{
        bool failed;
        size_t totsize;
        SsCacMemT* cm;
        size_t i;
        size_t nbytes_allocated;
        size_t osallocsize;


        totsize = (size_t)blocksize * nblocks;
        cm = SsMemAlloc(SIZEOF_CACMEMSTRUCT(totsize));
        failed = FALSE;
        for (nbytes_allocated = 0, i = 0;
             nbytes_allocated < totsize;
             nbytes_allocated += osallocsize, i++)
        {
            osallocsize = totsize - nbytes_allocated;
            if (osallocsize > OS_MAXALLOC) {
                osallocsize = OS_MAXALLOC;
            }
#ifdef SS_NT
            cm->cm_sysptrarr[i] =
                VirtualAlloc(
                        NULL,
                        osallocsize,
                        MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
            if (cm->cm_sysptrarr[i] == NULL) {
                failed = TRUE;
                ss_pprintf_2(("VirtualAlloc failed, error = %d\n", GetLastError()));
                SsErrorMessage(FIL_MSG_VIRTUALALLOC_FAILED_D, GetLastError());
                break;
            }
#else /* SS_NT */
            cm->cm_sysptrarr[i] =
                SsMemPageAlloc(osallocsize);
            if (cm->cm_sysptrarr[i] == NULL) {
                failed = TRUE;
                break;
            }
#endif /* SS_NT */
        }
        cm->cm_nsysptr = i;
        if (failed) {
            char buf_alloc[20];
            static char out_of_memory_text[128] =
                "SOLID Fatal error: Out of central memory when allocating buffer memory";
            SsUltoa(
                (ulong)blocksize * (ulong)nblocks,
                buf_alloc,
                10);
            strcat(out_of_memory_text, " (size = ");
            strcat(out_of_memory_text, buf_alloc);
            strcat(out_of_memory_text, ")\n");
            SsAssertionFailureText(out_of_memory_text, __FILE__, __LINE__);
            ss_error;
        }

        ss_qmem_stat.qms_sysptrcount += cm->cm_nsysptr;
        ss_qmem_stat.qms_sysbytecount += totsize;

        cm->cm_blocksize = blocksize;
        cm->cm_nblocks_tot = cm->cm_nblocks_left = nblocks;

        return(cm);
}

void SsCacMemDone(SsCacMemT* cm)
{
        size_t i;
        size_t totsize = (size_t)cm->cm_blocksize * cm->cm_nblocks_tot;
        size_t nbytes_freed;
        size_t bytestofree;

        for (i = 0, nbytes_freed = 0;
             i < cm->cm_nsysptr;
             i++, nbytes_freed += bytestofree)
        {
            bytestofree = totsize - nbytes_freed;
            if (bytestofree > OS_MAXALLOC) {
                bytestofree = OS_MAXALLOC;
            }
#ifdef SS_NT
            VirtualFree(cm->cm_sysptrarr[i], 0, MEM_RELEASE);
#else /* SS_NT */
            SsMemPageFree(cm->cm_sysptrarr[i], bytestofree);
#endif /* SS_NT */
        }
        ss_qmem_stat.qms_sysptrcount -= cm->cm_nsysptr;
        ss_qmem_stat.qms_sysbytecount -= totsize;
        
        SsMemFree(cm);
}

void* SsCacMemAlloc(SsCacMemT* cm)
{
        ss_byte_t* p;
        size_t byteposition;

        ss_assert(cm->cm_nblocks_left > 0);

        byteposition = (size_t)(cm->cm_nblocks_tot - cm->cm_nblocks_left)
            * cm->cm_blocksize;
        p = cm->cm_sysptrarr[byteposition / OS_MAXALLOC];
        p += byteposition % OS_MAXALLOC;
        cm->cm_nblocks_left--;
        ss_purify(memset(p, '\1', cm->cm_blocksize));

        return(p);
}

void SsCacMemFree(
        SsCacMemT* cm,
        void* p)
{
}

#else /* SS_NT && !SS_NT64 && !SS_DEBUG */

struct SsCacMemSt {
        void*   cm_sysptr;
        char*   cm_curptr;
        size_t  cm_blocksize;
        size_t  cm_nblocks_left;
        size_t  cm_nblocks_tot;
};

#if defined(SS_NT) && defined(SS_DEBUG)
static size_t cacmem_ospagesize;
#endif

SsCacMemT* SsCacMemInit(size_t blocksize, size_t nblocks)
{
        SsCacMemT* cm;

        cm = SSMEM_NEW(SsCacMemT);

#ifdef SS_NT
#  ifdef SS_DEBUG
        cacmem_ospagesize = SsMemPageSize();
        ss_dassert(cacmem_ospagesize == 512
            ||     cacmem_ospagesize == 1024
            ||     cacmem_ospagesize == 2048
            ||     cacmem_ospagesize == 4096
            ||     cacmem_ospagesize == 8192);
        if (cacmem_ospagesize <= blocksize
        &&  (blocksize % cacmem_ospagesize) == 0)
        {
            size_t i;


            cm->cm_sysptr = VirtualAlloc(
                            NULL,
                            (blocksize + cacmem_ospagesize) * nblocks,
                            MEM_RESERVE,
                            PAGE_READWRITE);
            for (i = 0; i < nblocks; i++) {
                VirtualAlloc(
                    (char*)cm->cm_sysptr +
                        i * (blocksize + cacmem_ospagesize),
                    blocksize,
                    MEM_COMMIT,
                    PAGE_READWRITE);
            }

        } else
#  endif /* SS_DEBUG */
        {
            cm->cm_sysptr = VirtualAlloc(
                                NULL,
                                blocksize * nblocks,
                                MEM_RESERVE | MEM_COMMIT,
                                PAGE_READWRITE);
            if (cm->cm_sysptr == NULL) {
                ss_pprintf_2(("VirtualAlloc failed, error = %d\n", GetLastError()));
                SsErrorMessage(FIL_MSG_VIRTUALALLOC_FAILED_D, GetLastError());
            }
        }
#else /* SS_NT */
        cm->cm_sysptr = SsMemPageAlloc(blocksize * nblocks);
#endif /* SS_NT */
        if (cm->cm_sysptr == NULL) {
            char buf_alloc[20];
            static char out_of_memory_text[128] =
                "SOLID Fatal error: Out of central memory when allocating buffer memory";
            SsUltoa(
                (ulong)blocksize * (ulong)nblocks,
                buf_alloc,
                10);
            strcat(out_of_memory_text, " (size = ");
            strcat(out_of_memory_text, buf_alloc);
            strcat(out_of_memory_text, ")\n");
            SsAssertionFailureText(out_of_memory_text, (char *)__FILE__, __LINE__);
            ss_error;
        }

        ss_qmem_stat.qms_sysptrcount++;
        ss_qmem_stat.qms_sysbytecount += (blocksize * nblocks);

        cm->cm_curptr = cm->cm_sysptr;
        cm->cm_blocksize = blocksize;
        cm->cm_nblocks_tot = cm->cm_nblocks_left = nblocks;

        return(cm);
}

void SsCacMemDone(SsCacMemT* cm)
{
#ifdef SS_NT
        VirtualFree(cm->cm_sysptr, 0, MEM_RELEASE);
#else
        SsMemPageFree(cm->cm_sysptr, cm->cm_blocksize * cm->cm_nblocks_tot);
#endif
        ss_qmem_stat.qms_sysptrcount--;
        ss_qmem_stat.qms_sysbytecount -= (cm->cm_blocksize * cm->cm_nblocks_tot);
        
        SsMemFree(cm);
}

void* SsCacMemAlloc(SsCacMemT* cm)
{
        void* p;

        ss_assert(cm->cm_nblocks_left > 0);

        p = cm->cm_curptr;
#if defined(SS_NT) && defined(SS_DEBUG)
        if (cacmem_ospagesize <= cm->cm_blocksize
        &&  (cm->cm_blocksize % cacmem_ospagesize) == 0)
        {
            cm->cm_curptr += cm->cm_blocksize + cacmem_ospagesize;
        } else
#endif /* SS_NT && SS_DEBUG */
        {
            cm->cm_curptr += cm->cm_blocksize;
        }
        cm->cm_nblocks_left--;

        ss_purify(memset(p, '\1', cm->cm_blocksize));

        return(p);
}

void SsCacMemFree(
        SsCacMemT* cm __attribute__ ((unused)),
        void* p)
{
        SS_NOTUSED(p);
}
#endif /* SS_NT && !SS_NT64 && !SS_DEBUG */

#else /* SS_PAGED_MEMALLOC_AVAILABLE */

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sscacmem.h"

struct SsCacMemSt {
        size_t  cm_blocksize;
        size_t  cm_nblocks_left;
};

/*##**********************************************************************\
 * 
 *		SsCacMemInit
 * 
 * Createa a cache memory allocation object. The user can allocate
 * 'nblocks' blocks of size 'blocksize' from this object.
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		
 *		
 *	nblocks - in
 *		
 *		
 * Return value - give : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsCacMemT* SsCacMemInit(size_t blocksize, size_t nblocks)
{
        SsCacMemT* cm;

        cm = SSMEM_NEW(SsCacMemT);

        cm->cm_blocksize = blocksize;
        cm->cm_nblocks_left = nblocks;

        return(cm);
}

/*##**********************************************************************\
 * 
 *		SsCacMemDone
 * 
 * Releases the cache memory object. Before calling this function all
 * individual cache allocationss must be freed using function SsCacMemFree.
 * 
 * Parameters : 
 * 
 *	cm - in, take
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
void SsCacMemDone(SsCacMemT* cm)
{
        SsMemFree(cm);
}

/*##**********************************************************************\
 * 
 *		SsCacMemAlloc
 * 
 * Allocates one block from the cache. The allocated block must be
 * released with function SsCacMemFree.
 * 
 * Parameters : 
 * 
 *	cm - use
 *		
 *		
 * Return value - give : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void* SsCacMemAlloc(SsCacMemT* cm)
{
        void* p;

        ss_assert(cm->cm_nblocks_left > 0);

        p = SsMemAlloc(cm->cm_blocksize);
        cm->cm_nblocks_left--;

        ss_purify(memset(p, '\1', cm->cm_blocksize));

        return(p);
}

/*##**********************************************************************\
 * 
 *		SsCacMemFree
 * 
 * Releases a cache memory block allocated with SsCacMemAlloc.
 * 
 * Parameters : 
 * 
 *	cm - use
 *		
 *		
 *	p - in, take
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
void SsCacMemFree(
        SsCacMemT* cm,
        void* p)
{
        SS_NOTUSED(cm);

        SsMemFree(p);
}

#endif /* SS_PAGED_MEMALLOC_AVAILABLE */
