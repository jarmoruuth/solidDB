/*************************************************************************\
**  source       * ssmem.c
**  directory    * ss
**  description  * Memory allocation functions.
**               *
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

This module contains replacement functions for ANSI C malloc, calloc,
realloc, free and strdup functions. These functions are otherwise
equivalent but they never fail. Out of memory errors are handled
inside these functions. This makes programming easier for those
programs that has no way of continuing in out of memory case. In
debug version these same functions can be used to debug the memory
usage and check for some typical memory errors.

All routines are available also for MS Windows, and can allocate
all avaliable memory. They allocate memory from the global heap.

The memory usage degugging system keeps a list of all allocated
pointers and the place where they are allocated. When an allocation
is made, the pointer is added to the list, When a pointer is
released, the pointer is removed from the list. If this list is
displayed at the end of the program, it gives all those pointers that
are allocated but never released. The pointer list is physically
implemented as a hash table with separate chaining. Several functions
are available to scan though the list, or display the list to stdout.

The memory error checking system can detect cases where a pointer is
released twice, a pointer that is never allocated is released or
there has been a memory overwrite or underwrite. When the memory
usage list is in use, the errors in releasing the pointer are
detected because the pointer is not found from the list. When the
list is not in use, the illegal release can be detected from the
underwrite and overwrite marks. When a pointer is allocated, actually
a larger block is allocated.  This extra area is used for underwrite
and overwrite marks at the end and beginning of the pointer. These
marks contain the memory area size in bytes. When these marks are
corrupted, the released pointer is invalid or there has been a memory
oveewrite or underwrite.

When a memory area is allocated in debug version, it is initialized
with a value 0xBABE . This should help detecting errors where the data
is not properly initialized before it is used. The value should be
especially useful with pointers, because it should not be inside
legal address space and cause a system protection error. If a
register contains this value at the crash time, it is very likely
that the program has been accessing an uninitialized pointer.

When a memory area deallocated in debug version, it is overwritten
with a value 0xDEAD . This should help to detect cases where the
memory area is used after it has been released in the same way as in
allocated memory case.

The debug version system is implemented by defining the normal memory
function to different names using macros. Thus, no source code
changes are necessary.


Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include "ssenv.h"

#include "ssstdio.h"
#include "ssstring.h"
#include "ssstddef.h"
#include "ssstdlib.h"

#include "ssc.h"
#include "ssdebug.h"
#include "sssem.h"
#include "ssmem.h"
#include "ssmemchk.h"
#include "ssthread.h"

#ifdef SS_FFMEM
#include "ssffmem.h"
#endif /* SS_FFMEM */

#if defined(SSMEM_TRACE)
#include "ssmemtrc.h"
#endif

void* SsMemFreeIfNotNULL(void* ptr)
{
        if (ptr != NULL) {
            SsMemFree(ptr);
        }
        return (NULL);
}

#undef SsMemAlloc
#undef SsMemCalloc
#undef SsMemRealloc
#undef SsMemFree
#undef SsMemStrdup
#undef SsFFmemAllocCtxFor
#undef SsFFmemReallocCtxFor
#undef SsFFmemFreeCtxFor
#undef SsFFmemGetDataSize
#undef SsFFmemAllocPrivCtxFor
#undef SsFFmemFreePrivCtxFor
#undef SsFFmemReallocPrivCtxFor

#ifdef SS_DEBUG

#define SSMEM_FREEDPOOL
#define MEMCHK_EXTRA        16

extern int ss_memdebug;             /* in ssdebug.c */
extern int ss_memdebug_segalloc;    /* in ssdebug.c */

SsSemT* ss_memchk_sem = NULL;
int ss_memdebug_usefreedpool = TRUE;

typedef struct {
        long            mo_cnt;     /* Current count. */
        long            mo_maxcnt;  /* Maximum count. */
        long            mo_size;    /* Allocated size. */
        long            mo_maxsize; /* Max allocated size. */
        ss_memobjtype_t mo_type;    /* Type for debug purposes */
        char*           mo_name;    /* Allocation name. */
} ss_memobj_t;

static const char* ss_memobjname[SS_MEMOBJ_LASTFIXEDID+1] = {
        "unknown",
        "generic",
        "atype",
        "aval",
        "ttype",
        "tval",
        "list",
        "listnode",
        "slist",
        "slisttbl",
        "ablist",
        "ablisttbl",
        "rbt",
        "rbtnode",
        "mme rbt",
        "trxinfo",
        "trxbufstmt",
        "trxwchk",
        "trxrchk",
        "trxkchk",
        "bkey",
        "mmnodeinfo",
        "mmlock",
        "mmnode",
        "mme_node",
        "mme_row",
        "mme_operation",
        "hsb_queue_logdata",
        "hsb_queue_catchup_logdata",
        "hsb_queue_acknowledge",
        "hsb_flusher_queue",
        "rpc_rses",
        "SQL",
        "DBE cache",
        "CacMem",
        "FFmem",
        "snc msgholder",
        "snc lock",
        NULL

};

static ss_memobj_t** ss_memobj;
static int           ss_memobj_size;
static int           ss_memobj_count;

static void MemObjInit(void)
{
        if (ss_memobj == NULL) {
            int i;
            ss_memobj_size = 100;
            ss_memobj = malloc(ss_memobj_size * sizeof(ss_memobj[0]));
            ss_assert(ss_memobj != NULL);
            for (i = 0; ss_memobjname[i] != NULL; i++) {
                if (ss_memobj_count >= ss_memobj_size) {
                    ss_memobj_size += 100;
                    ss_memobj = realloc(ss_memobj, ss_memobj_size * sizeof(ss_memobj[0]));
                    ss_assert(ss_memobj != NULL);
                }
                ss_memobj[ss_memobj_count] = calloc(1, sizeof(ss_memobj_t));
                ss_assert(ss_memobj[ss_memobj_count] != NULL);
                ss_memobj[ss_memobj_count]->mo_type = i;
                ss_memobj[ss_memobj_count]->mo_name = (char *)ss_memobjname[i];
                ss_memobj_count++;
            }
        }
}

void SsMemObjIncNomutex(ss_memobjtype_t mo, int size)
{
        ss_dassert(SsSemThreadIsEntered(ss_memchk_sem));

        MemObjInit();

        ss_assert(ss_memobj[mo]->mo_type == mo);

        ss_memobj[mo]->mo_cnt++;
        ss_memobj[mo]->mo_size += size;

        if (ss_memobj[mo]->mo_cnt > ss_memobj[mo]->mo_maxcnt) {
            ss_memobj[mo]->mo_maxcnt = ss_memobj[mo]->mo_cnt;
        }
        if (ss_memobj[mo]->mo_size > ss_memobj[mo]->mo_maxsize) {
            ss_memobj[mo]->mo_maxsize = ss_memobj[mo]->mo_size;
        }
}

void SsMemObjDecNomutex(ss_memobjtype_t mo, int size)
{
        ss_dassert(SsSemThreadIsEntered(ss_memchk_sem));
        ss_assert(ss_memobj[mo] != NULL);

        ss_memobj[mo]->mo_cnt--;
        ss_memobj[mo]->mo_size -= size;
}

void SsMemObjInc(ss_memobjtype_t mo, int size)
{
        SsSemEnter(ss_memchk_sem);
        SsMemObjIncNomutex(mo, size);
        SsSemExit(ss_memchk_sem);
}

void SsMemObjDec(ss_memobjtype_t mo, int size)
{
        SsSemEnter(ss_memchk_sem);
        SsMemObjDecNomutex(mo, size);
        SsSemExit(ss_memchk_sem);
}


static ss_memobjtype_t MemObjGetFileId(char* fname)
{
        ss_memobjtype_t mo;

        MemObjInit();

#ifdef MSC
        {
            /* In MS Visual C++ the __FILE__ expands to path and file name.
             */
            char* tmp_fname;
            tmp_fname = strrchr(fname, '\\');
            if (tmp_fname != NULL) {
                fname = tmp_fname+1;
            }
        }
#endif /* MSC */

        mo = ss_memobj_count;

        if (ss_memobj_count >= ss_memobj_size) {
            ss_memobj_size += 100;
            ss_memobj = realloc(ss_memobj, ss_memobj_size * sizeof(ss_memobj[0]));
            ss_assert(ss_memobj != NULL);
        }
        ss_memobj[ss_memobj_count] = calloc(1, sizeof(ss_memobj_t));
        ss_assert(ss_memobj[ss_memobj_count] != NULL);
        ss_memobj[ss_memobj_count]->mo_type = mo;
        ss_memobj[ss_memobj_count]->mo_name = fname;
        ss_memobj_count++;
        ss_assert(ss_memobj_count < 10000);

        return(mo);
}

/*#***********************************************************************\
 *
 *		MemObjPrintGetPerc
 *
 * Returns percentage of l1 from l2.
 *
 * Parameters :
 *
 *	l1 -
 *
 *
 *	l2 -
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
static double MemObjPrintGetPerc(long l1, long l2)
{
        if (l2 == 0) {
            return(0.0);
        } else {
            return((double)l1 / (double)l2 * 100.0);
        }
}

/*#***********************************************************************\
 *
 *		MemObjPrintOne
 *
 * Prints info of one memory object.
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	name -
 *
 *
 *	curptr -
 *
 *
 *	maxptr -
 *
 *
 *	curbyt -
 *
 *
 *	maxbyt -
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
static void MemObjPrintOne(
        void* fp,
        char* name,
        long curptr,
        long maxptr,
        long curbyt,
        long maxbyt)
{
        SsFprintf(fp, "%-26s: %8ld (%4.1lf) %8ld (%4.1lf) %8ld (%4.1lf) %8ld (%4.1lf)\n",
            name,
            curptr, MemObjPrintGetPerc(curptr, memchk_nptr),
            maxptr, MemObjPrintGetPerc(maxptr, memchk_maxptr),
            curbyt, MemObjPrintGetPerc(curbyt, memchk_bytes),
            maxbyt,  MemObjPrintGetPerc(maxbyt, memchk_maxbytes));
}

/*##**********************************************************************\
 *
 *		SsMemObjPrint
 *
 * Prints a report of memory object allocations.
 *
 * Parameters :
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
void SsMemObjPrint(void* fp)
{
        int i;

        SsSemEnter(ss_memchk_sem);

        SsFprintf(fp, "Memory object information:\n");
        SsFprintf(fp, "%-26s: %8s %6s %8s %6s %8s %6s %8s %6s\n",
            "Name",
            "CurPtr", "(%)",
            "MaxPtr", "(%)",
            "CurByt", "(%)",
            "MaxByt", "(%)");

        SsFprintf(fp, "%-26s: %8ld %6s %8ld %6s %8ld %6s %8ld %6s\n",
            "TOTAL",
            memchk_nptr, "",
            memchk_maxptr, "",
            memchk_bytes, "",
            memchk_maxbytes,  "");

        for (i = 0; i < ss_memobj_count; i++) {
            MemObjPrintOne(
                fp,
                ss_memobj[i]->mo_name,
                ss_memobj[i]->mo_cnt,
                ss_memobj[i]->mo_maxcnt,
                ss_memobj[i]->mo_size,
                ss_memobj[i]->mo_maxsize);
        }

        SsSemExit(ss_memchk_sem);
}

#ifdef SSMEM_FREEDPOOL

#define SSMEM_FREEDPOOLSIZE 1024U
#define SSMEM_FREEDPOOLMAX (SSMEM_FREEDPOOLSIZE - 1U)

#define SsMemFreedPoolN \
        ((SsMemFreedPoolEnd + SSMEM_FREEDPOOLSIZE - SsMemFreedPoolBegin) \
         % SSMEM_FREEDPOOLSIZE)

static struct SsMemFreedSt {
        void*   fp_ptr;
        char**  fp_callstk;
        char*   fp_alloc_file;
        long    fp_alloc_line;
        char*   fp_free_file;
        long    fp_free_line;
} SsMemFreedPool[SSMEM_FREEDPOOLSIZE];

static size_t SsMemFreedPoolBegin = 0;
static size_t SsMemFreedPoolEnd = 0;

/*#***********************************************************************\
 *
 *		SsMemFreedPoolAdd
 *
 *
 *
 * Parameters :
 *
 *	ptr -
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
static void SsMemFreedPoolAdd(void* ptr,
                              size_t size,
                              char** callstack,
                              char *alloc_file,
                              long alloc_line,
                              char *free_file,
                              long free_line)
{
        size_t qmemsize;
        size_t datasize;
        size_t n = SsMemFreedPoolN;

        if (ss_memdebug_usefreedpool) {
            if (n == SSMEM_FREEDPOOLMAX) {
                size_t i;
                char* p;
                p = SsMemFreedPool[SsMemFreedPoolBegin].fp_ptr;
                datasize = *(size_t*)p;
                /* Check that data is not changed. */
                for (i = sizeof(datasize); i < datasize; i++) {
                    if (p[i] != '\xfe') {
                        SsDbgPrintf(
                          "Found changes inside freed memory block\n");
                        if (SsMemFreedPool[SsMemFreedPoolBegin].fp_alloc_file
                        != NULL) {
                            SsDbgPrintf("Allocated at %s:%ld\n",
                              SsMemFreedPool[SsMemFreedPoolBegin].fp_alloc_file,
                              SsMemFreedPool[SsMemFreedPoolBegin].fp_alloc_line);
                        }
                        if (SsMemFreedPool[SsMemFreedPoolBegin].fp_free_file
                        != NULL) {
                            SsDbgPrintf("Freed at %s:%ld\n",
                              SsMemFreedPool[SsMemFreedPoolBegin].fp_free_file,
                              SsMemFreedPool[SsMemFreedPoolBegin].fp_free_line);
                        }
                        if (SsMemFreedPool[SsMemFreedPoolBegin].fp_callstk
                        != NULL) {
                            SsMemTrcPrintCallStk(
                              SsMemFreedPool[SsMemFreedPoolBegin].fp_callstk);
                        }

                        /* check whether it was caused a memory overwrite/
                           underwrite or not (in which case it is likely to
                           be a runaway pointer) */

                        SsMemChkListCheckQmemNoMutex((char *)__FILE__, __LINE__);

                        ss_rc_error(p[i]);
                    }
                }
                SsQmemFree(p);
                if (SsMemFreedPool[SsMemFreedPoolBegin].fp_callstk != NULL) {
                    SsMemTrcFreeCallStk(SsMemFreedPool[SsMemFreedPoolBegin].fp_callstk);
                    SsMemFreedPool[SsMemFreedPoolBegin].fp_callstk = NULL;
                }
                SsMemFreedPool[SsMemFreedPoolBegin].fp_alloc_file =  alloc_file;
                SsMemFreedPool[SsMemFreedPoolBegin].fp_alloc_line = alloc_line;
                SsMemFreedPool[SsMemFreedPoolBegin].fp_free_file = free_file;
                SsMemFreedPool[SsMemFreedPoolBegin].fp_free_line = free_line;
                SsMemFreedPool[SsMemFreedPoolBegin].fp_ptr = NULL;
                SsMemFreedPoolBegin =
                    (SsMemFreedPoolBegin + 1U) % SSMEM_FREEDPOOLSIZE;
            }
            SsMemFreedPool[SsMemFreedPoolEnd].fp_alloc_file = alloc_file;
            SsMemFreedPool[SsMemFreedPoolEnd].fp_alloc_line = alloc_line;
            SsMemFreedPool[SsMemFreedPoolEnd].fp_free_file = free_file;
            SsMemFreedPool[SsMemFreedPoolEnd].fp_free_line = free_line;
            SsMemFreedPool[SsMemFreedPoolEnd].fp_ptr = ptr;
            SsMemFreedPool[SsMemFreedPoolEnd].fp_callstk = callstack;
            SsMemFreedPoolEnd = (SsMemFreedPoolEnd + 1U) % SSMEM_FREEDPOOLSIZE;

            qmemsize = SsQmemGetDataSize(ptr);
            ss_dassert(qmemsize >= size);
            datasize = SS_MAX(size, qmemsize) - sizeof(datasize);
            *(size_t*)ptr = datasize;

            memset((char*)ptr + sizeof(datasize), '\xfe', datasize);
        } else {
            SsQmemFree(ptr);
        }
}

/*##**********************************************************************\
 *
 *		SsMemFreedPoolCheck
 *
 * Checks that freed memory pool is consistent.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsMemFreedPoolCheck(void)
{
        size_t datasize;
        size_t j;

        if (ss_memdebug_usefreedpool) {
            SsSemEnter(ss_memchk_sem);
            for (j = 0; j < SSMEM_FREEDPOOLSIZE; j++) {
                size_t i;
                char* p;
                p = SsMemFreedPool[j].fp_ptr;
                if (p == NULL) {
                    continue;
                }
                datasize = *(size_t*)p;
                /* Check that data is not changed. */
                for (i = sizeof(datasize); i < datasize; i++) {
                    if (p[i] != '\xfe') {
                        SsDbgPrintf(
                          "Found changes inside freed memory block\n");
                        if (SsMemFreedPool[j].fp_alloc_file
                        != NULL) {
                            SsDbgPrintf("Allocated at %s:%ld\n",
                              SsMemFreedPool[j].fp_alloc_file,
                              SsMemFreedPool[j].fp_alloc_line);
                        }
                        if (SsMemFreedPool[j].fp_free_file
                        != NULL) {
                            SsDbgPrintf("Freed at %s:%ld\n",
                              SsMemFreedPool[j].fp_free_file,
                              SsMemFreedPool[j].fp_free_line);
                        }
                        if (SsMemFreedPool[j].fp_callstk
                        != NULL) {
                            SsMemTrcPrintCallStk(
                              SsMemFreedPool[j].fp_callstk);
                        }

                        /* check whether it was caused a memory overwrite/
                           underwrite or not (in which case it is likely to
                           be a runaway pointer) */

                        SsMemChkListCheckQmemNoMutex((char *)__FILE__, __LINE__);

                        ss_rc_error(p[i]);
                    }
                }
            }
            SsSemExit(ss_memchk_sem);
        }
}

#else  /* SSMEM_FREEDPOOL */

void SsMemFreedPoolCheck(void)
{
}

#endif /* SSMEM_FREEDPOOL */

/*#**********************************************************************\
 *
 *		SsMemChkAlloc
 *
 * Check version of SsMemAlloc. Adds the allocated pointer to the check
 * list.
 *
 * When compiled with SS_DEBUG, SsMemAlloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	size - in
 *		number of bytes to allocate
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemChkAlloc(size, file, line, p_mo)
        size_t size;
        char*    file;
        int      line;
        ss_memobjtype_t* p_mo;
{
        void* ptr;

        SS_MEM_CHKALLOCSIZE(size);
        ss_dassert(ss_memchk_sem != NULL);

        SsSemEnter(ss_memchk_sem);

        if (*p_mo == SS_MEMOBJ_UNKNOWN) {
            *p_mo = MemObjGetFileId(file);
        }

        if (size == 0) {
            size = 1;
        }

        memchk_alloc++;
        memchk_nptr++;
        if (memchk_nptr > memchk_maxptr) {
            memchk_maxptr = memchk_nptr;
        }
        ptr = SsQmemAlloc(size + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA);
        if (ptr == NULL) {
            SsMemChkAbort((char *)"Out of memory in SsMemChkAlloc", file, line);
        }
        if (ss_memdebug) {
            SsMemChkListAdd(ptr, size, file, line);
        }

        ptr = SsMemChkSetCheck(ptr, size, TRUE, memchk_newborn, NULL, *p_mo);

        memchk_bytes += size;
        if (memchk_bytes > memchk_maxbytes) {
            memchk_maxbytes = memchk_bytes;
        }

        SsSemExit(ss_memchk_sem);

        return(ptr);
}

void* SsMemChkCtxAlloc(
        SsMemCtxT* qmem,
        size_t size,
        char*    file,
        int      line,
        ss_memobjtype_t* p_mo)
{
        void* ptr;

        SS_MEM_CHKALLOCSIZE(size);
        ss_dassert(ss_memchk_sem != NULL);

        SsSemEnter(ss_memchk_sem);

        if (*p_mo == SS_MEMOBJ_UNKNOWN) {
            *p_mo = MemObjGetFileId(file);
        }

        memchk_alloc++;
        memchk_nptr++;
        if (memchk_nptr > memchk_maxptr) {
            memchk_maxptr = memchk_nptr;
        }
        ptr = SsQmemCtxAlloc(qmem, size + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA);
        if (ptr == NULL) {
            SsMemChkAbort((char *)"Out of memory in SsMemChkAlloc", file, line);
        }
        if (ss_memdebug) {
            SsMemChkListAdd(ptr, size, file, line);
        }

        ptr = SsMemChkSetCheck(ptr, size, TRUE, memchk_newborn, NULL, *p_mo);

        memchk_bytes += size;
        if (memchk_bytes > memchk_maxbytes) {
            memchk_maxbytes = memchk_bytes;
        }

        SsSemExit(ss_memchk_sem);

        return(ptr);
}

/*#**********************************************************************\
 *
 *		SsMemChkCalloc
 *
 * Check version of SsMemCalloc. Adds the allocated pointer to the
 * check list.
 *
 * When compiled with SS_DEBUG, SsMemCalloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	size - in
 *		number of elements to allocate
 *
 *	elsize - in
 *		element size
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemChkCalloc(
        unsigned nelem,
        size_t elsize,
        char*    file,
        int      line,
        ss_memobjtype_t* p_mo)
{
        void* ptr;

        SS_MEM_CHKALLOCSIZE(nelem * elsize);
        SsSemEnter(ss_memchk_sem);

        if (*p_mo == SS_MEMOBJ_UNKNOWN) {
            *p_mo = MemObjGetFileId(file);
        }

        memchk_calloc++;
        memchk_nptr++;
        if (memchk_nptr > memchk_maxptr) {
            memchk_maxptr = memchk_nptr;
        }
        ptr = SsQmemAlloc(nelem * elsize + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA);
        if (ptr == NULL) {
            SsMemChkAbort((char *)"Out of memory in SsMemChkCalloc", file, line);
        }
        memset(ptr, '\0', nelem * elsize + MEMCHK_ALLOCSIZE);
        if (ss_memdebug) {
            SsMemChkListAdd(ptr, nelem * elsize, file, line);
        }

        ptr = SsMemChkSetCheck(ptr, nelem * elsize, FALSE, memchk_newborn, NULL, *p_mo);

        memchk_bytes += nelem * elsize;
        if (memchk_bytes > memchk_maxbytes) {
            memchk_maxbytes = memchk_bytes;
        }

        SsSemExit(ss_memchk_sem);

        return(ptr);
}

/*#**********************************************************************\
 *
 *		SsMemChkRealloc
 *
 * Check version of SsMemRealloc. Adds the allocated pointer to the
 * check list.
 *
 * When compiled with SS_DEBUG, SsMemRealloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	oldptr - in, take
 *		old pointer
 *
 *	size - in
 *		new size for memory area
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 *
 * Return value - give :
 *
 *      pointer to the reallocated memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemChkRealloc(
        void*    oldptr,
        size_t   size,
        char*    file,
        int      line,
        ss_memobjtype_t* p_mo)
{
        void*   ptr;
        size_t  oldsize;

        SS_MEM_CHKALLOCSIZE(size);

        memchk_realloc++;

        ptr = SsMemChkAlloc(size, file, line, p_mo);
        memchk_alloc--;
        if (oldptr == NULL) {
            return (ptr);
        }
        oldsize = SsMemChkGetSize(MEMCHK_GETSYSTEMPTR(oldptr));
        memcpy(ptr, oldptr, SS_MIN(size, oldsize));
        SsMemChkFree(oldptr, file, line);
        memchk_free--;
        return(ptr);
}

/*##**********************************************************************\
 *
 *		SsMemChkReallocEx
 *
 * Extended version that scrambles caller pointer.
 *
 * Parameters :
 *
 *	p_oldptr -
 *
 *
 *	size -
 *
 *
 *	file -
 *
 *
 *	line -
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
void* SsMemChkReallocEx(
        void**   p_oldptr,
        size_t   size,
        char*    file,
        int      line,
        ss_memobjtype_t* p_mo)
{
        void*   ptr;

        ptr = SsMemChkRealloc(*p_oldptr, size, file, line, p_mo);
        *p_oldptr = (void*)0xDeadBeef;
        return(ptr);
}

/*#**********************************************************************\
 *
 *		SsMemChkFree
 *
 * Check version of SsMemFree. Removes the pointer from the check list.
 *
 * When compiled with SS_DEBUG, SsMemFree is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	ptr - in, take
 *		pointer to be released
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkFree(
        void*  ptr,
        char*  file,
        int    line)
{
        SsMemChkListT* list;
        size_t         oldsize;
#ifdef SSMEM_FREEDPOOL
        char**          callstack = NULL;
        char*           alloc_file = NULL;
        long            alloc_line = -1;
#endif

        SsSemEnter(ss_memchk_sem);

        memchk_free++;
        memchk_nptr--;
        if (ptr != NULL) {
            ptr = MEMCHK_GETSYSTEMPTR(ptr);
        } else {
            SsMemChkAbort((char *)"NULL pointer passed to SsMemFree", file, line);
        }
        if (ss_memdebug) {
            list = SsMemChkListFind(ptr, file, line);
            SsMemChkListRemove(list);
#if defined(SSMEM_TRACE)
#ifdef SSMEM_FREEDPOOL
            callstack = list->memlst_callstk;
            alloc_file = list->memlst_file;
            alloc_line = list->memlst_line;
#else /* SSMEM_FREEDPOOL */
            SsMemTrcFreeCallStk(list->memlst_callstk);
#endif /* SSMEM_FREEDPOOL */
            list->memlst_callstk = NULL;
            SsSQLTrcInfoFree(list->memlst_sqltrcinfo);
            list->memlst_sqltrcinfo = NULL;
#endif /* SSMEM_TRACE */
            if (list->memlst_linkinfo != NULL) {
                free(list->memlst_linkinfo);
            }
        } else {
            SsMemChkCheckPtr(ptr, file, line);
        }
        /* remove old check marks */
        SsMemChkFreeCheck(ptr, MEMCHK_UNKNOWNSIZE, TRUE, MEMCHK_DEAD, &oldsize);
        ss_dassert(SsQmemGetDataSize(ptr) >= oldsize);

        memchk_bytes -= oldsize;

#ifdef SSMEM_FREEDPOOL
        SsMemFreedPoolAdd(ptr, oldsize + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA,
          callstack, alloc_file, alloc_line, file, line);
        SsSemExit(ss_memchk_sem);
#else /* SSMEM_FREEDPOOL */

        SsSemExit(ss_memchk_sem);
        SsQmemFree(ptr);
#endif /* SSMEM_FREEDPOOL */
}

/*#**********************************************************************\
 *
 *		SsMemChkCtxFree
 *
 * Check version of SsMemFree. Removes the pointer from the check list.
 *
 * When compiled with SS_DEBUG, SsMemFree is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	ptr - in, take
 *		pointer to be released
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemChkCtxFree(
        SsMemCtxT* qmem,
        void*  ptr,
        char*  file,
        int    line)
{
        SsMemChkListT* list;
        size_t         oldsize;

        SsSemEnter(ss_memchk_sem);

        memchk_free++;
        memchk_nptr--;
        if (ptr != NULL) {
            ptr = MEMCHK_GETSYSTEMPTR(ptr);
        } else {
            SsMemChkAbort((char *)"NULL pointer passed to SsMemFree", file, line);
        }
        if (ss_memdebug) {
            list = SsMemChkListFind(ptr, file, line);
            SsMemChkListRemove(list);
#if defined(SSMEM_TRACE)
            SsMemTrcFreeCallStk(list->memlst_callstk);
            list->memlst_callstk = NULL;
            SsSQLTrcInfoFree(list->memlst_sqltrcinfo);
            list->memlst_sqltrcinfo = NULL;
#endif /* SSMEM_TRACE */
            if (list->memlst_linkinfo != NULL) {
                free(list->memlst_linkinfo);
            }
        } else {
            SsMemChkCheckPtr(ptr, file, line);
        }
        /* remove old check marks */
        SsMemChkFreeCheck(ptr, MEMCHK_UNKNOWNSIZE, TRUE, MEMCHK_DEAD, &oldsize);
        ss_dassert(SsQmemGetDataSize(ptr) >= oldsize);

        memchk_bytes -= oldsize;

        SsSemExit(ss_memchk_sem);
        SsQmemCtxFree(qmem, ptr);
}

/*##**********************************************************************\
 *
 *		SsMemChkFreeEx
 *
 * Extended version that scrambles caller pointer.
 *
 * Parameters :
 *
 *	p_ptr -
 *
 *
 *	file -
 *
 *
 *	line -
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
void SsMemChkFreeEx(
        void** p_ptr,
        char*  file,
        int    line)
{
        SsMemChkFree(*p_ptr, file, line);
        *p_ptr = (void*)0xDeadBeef;
}

/*##**********************************************************************\
 *
 *		SsMemChkCtxFreeEx
 *
 * Extended version that scrambles caller pointer.
 *
 * Parameters :
 *
 *	p_ptr -
 *
 *
 *	file -
 *
 *
 *	line -
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
void SsMemChkCtxFreeEx(
        SsMemCtxT* qmem,
        void** p_ptr,
        char*  file,
        int    line)
{
        SsMemChkCtxFree(qmem, *p_ptr, file, line);
        *p_ptr = (void*)0xDeadBeef;
}

#ifdef SS_FFMEM
/*#**********************************************************************\
 *
 *		SsFFmemChkAllocFor
 *
 * Check version of SsMemAlloc. Adds the allocated pointer to the check
 * list.
 *
 * When compiled with SS_DEBUG, SsMemAlloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	size - in
 *		number of bytes to allocate
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsFFmemChkAlloc(
        void* ctx,
        SsFFmemAllocForT f,
        size_t size,
        char *file,
        int line,
        ss_memobjtype_t* p_mo)
{
        void* ptr;

        SS_MEM_CHKALLOCSIZE(size);
        ss_dassert(ss_memchk_sem != NULL);

        ptr = SsFFmemAllocCtxFor(ctx, f, size + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA);

        SsSemEnter(ss_memchk_sem);

        if (*p_mo == SS_MEMOBJ_UNKNOWN) {
            *p_mo = MemObjGetFileId(file);
        }

        memchk_alloc++;
        memchk_nptr++;
        if (memchk_nptr > memchk_maxptr) {
            memchk_maxptr = memchk_nptr;
        }
        if (ptr == NULL) {
            SsMemChkAbort("Out of memory in SsFFmemAllocCtx", file, line);
        }
        if (ss_memdebug) {
            SsMemChkListAdd(ptr, size, file, line);
        }

        ptr = SsMemChkSetCheck(ptr, size, TRUE, memchk_newborn, NULL, *p_mo);

        memchk_bytes += size;
        if (memchk_bytes > memchk_maxbytes) {
            memchk_maxbytes = memchk_bytes;
        }

        SsSemExit(ss_memchk_sem);

        return(ptr);
}

void SsFFmemChkFree(
        void* ctx,
        SsFFmemAllocForT f,
        void* ptr,
        char *file,
        int line);

/*#**********************************************************************\
 *
 *		SsFFmemChkRealloc
 *
 * Check version of SsMemRealloc. Adds the allocated pointer to the
 * check list.
 *
 * When compiled with SS_DEBUG, SsMemRealloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	oldptr - in, take
 *		old pointer
 *
 *	size - in
 *		new size for memory area
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 *
 * Return value - give :
 *
 *      pointer to the reallocated memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsFFmemChkRealloc(
        void* ctx,
        SsFFmemAllocForT f,
        void* oldptr,
        size_t size,
        char *file,
        int line,
        ss_memobjtype_t* p_mo)
{
        void*   ptr;
        size_t  oldsize;

        SS_MEM_CHKALLOCSIZE(size);

        memchk_realloc++;

        ptr = SsFFmemChkAlloc(ctx, f, size, file, line, p_mo);
        memchk_alloc--;
        if (oldptr == NULL) {
            return (ptr);
        }
        oldsize = SsMemChkGetSize(MEMCHK_GETSYSTEMPTR(oldptr));
        memcpy(ptr, oldptr, SS_MIN(size, oldsize));
        SsFFmemChkFree(ctx, f, oldptr, file, line);
        memchk_free--;
        return(ptr);
}

/*#**********************************************************************\
 *
 *		SsFFmemChkFree
 *
 * Check version of SsMemFree. Removes the pointer from the check list.
 *
 * When compiled with SS_DEBUG, SsMemFree is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	ptr - in, take
 *		pointer to be released
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsFFmemChkFree(
        void* ctx,
        SsFFmemAllocForT f,
        void* ptr,
        char *file,
        int line)
{
        SsMemChkListT* list;
        size_t         oldsize;

        SsSemEnter(ss_memchk_sem);

        memchk_free++;
        memchk_nptr--;
        if (ptr != NULL) {
            ptr = MEMCHK_GETSYSTEMPTR(ptr);
        } else {
            SsMemChkAbort("NULL pointer passed to SsFFmemChkFree", file, line);
        }
        if (ss_memdebug) {
            list = SsMemChkListFind(ptr, file, line);
            SsMemChkListRemove(list);
#if defined(SSMEM_TRACE)
            SsMemTrcFreeCallStk(list->memlst_callstk);
            list->memlst_callstk = NULL;
            SsSQLTrcInfoFree(list->memlst_sqltrcinfo);
            list->memlst_sqltrcinfo = NULL;
#endif /* SSMEM_TRACE */
            if (list->memlst_linkinfo != NULL) {
                free(list->memlst_linkinfo);
            }
        } else {
            SsMemChkCheckPtr(ptr, file, line);
        }
        /* remove old check marks */
        SsMemChkFreeCheck(ptr, MEMCHK_UNKNOWNSIZE, TRUE, MEMCHK_DEAD, &oldsize);

        memchk_bytes -= oldsize;

        SsSemExit(ss_memchk_sem);

        SsFFmemFreeCtxFor(ctx, f, ptr);
}

/*#**********************************************************************\
 *
 *		SsFFmemChkAllocPrivCtxFor
 *
 * Check version of SsMemAlloc. Adds the allocated pointer to the check
 * list.
 *
 * When compiled with SS_DEBUG, SsMemAlloc is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	size - in
 *		number of bytes to allocate
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsFFmemChkAllocPrivCtxFor(
        void* pctx,
        SsFFmemAllocForT f,
        size_t size,
        char *file,
        int line,
        ss_memobjtype_t* p_mo)
{
        void* ptr;

        SS_MEM_CHKALLOCSIZE(size);
        ss_dassert(ss_memchk_sem != NULL);

        ptr = SsFFmemAllocPrivCtxFor(pctx, f, size + MEMCHK_ALLOCSIZE + MEMCHK_EXTRA);

        SsSemEnter(ss_memchk_sem);

        if (*p_mo == SS_MEMOBJ_UNKNOWN) {
            *p_mo = MemObjGetFileId(file);
        }

        memchk_alloc++;
        memchk_nptr++;
        if (memchk_nptr > memchk_maxptr) {
            memchk_maxptr = memchk_nptr;
        }
        if (ptr == NULL) {
            SsMemChkAbort("Out of memory in SsFFmemAllocPrivCtxFor", file, line);
        }
        if (ss_memdebug) {
            SsMemChkListAdd(ptr, size, file, line);
        }

        ptr = SsMemChkSetCheck(ptr, size, TRUE, memchk_newborn, NULL, *p_mo);

        memchk_bytes += size;
        if (memchk_bytes > memchk_maxbytes) {
            memchk_maxbytes = memchk_bytes;
        }

        SsSemExit(ss_memchk_sem);

        return(ptr);
}

void SsFFmemChkFreePrivCtxFor(
        void* pctx,
        SsFFmemAllocForT f,
        void* ptr,
        char *file,
        int line)
{
        SsMemChkListT* list;
        size_t         oldsize;

        SsSemEnter(ss_memchk_sem);

        memchk_free++;
        memchk_nptr--;
        if (ptr != NULL) {
            ptr = MEMCHK_GETSYSTEMPTR(ptr);
        } else {
            SsMemChkAbort("NULL pointer passed to SsFFmemChkFree", file, line);
        }
        if (ss_memdebug) {
            list = SsMemChkListFind(ptr, file, line);
            SsMemChkListRemove(list);
#if defined(SSMEM_TRACE)
            SsMemTrcFreeCallStk(list->memlst_callstk);
            list->memlst_callstk = NULL;
            SsSQLTrcInfoFree(list->memlst_sqltrcinfo);
            list->memlst_sqltrcinfo = NULL;
#endif /* SSMEM_TRACE */
            if (list->memlst_linkinfo != NULL) {
                free(list->memlst_linkinfo);
            }
        } else {
            SsMemChkCheckPtr(ptr, file, line);
        }
        /* remove old check marks */
        SsMemChkFreeCheck(ptr, MEMCHK_UNKNOWNSIZE, TRUE, MEMCHK_DEAD, &oldsize);

        memchk_bytes -= oldsize;

        SsSemExit(ss_memchk_sem);

        SsFFmemFreePrivCtxFor(pctx, f, ptr);
}

void* SsFFmemChkReallocPrivCtxFor(
        void* pctx,
        SsFFmemAllocForT f,
        void* oldptr,
        size_t size,
        char *file,
        int line,
        ss_memobjtype_t* p_mo)
{
        void*   ptr;
        size_t  oldsize;

        SS_MEM_CHKALLOCSIZE(size);

        memchk_realloc++;

        ptr = SsFFmemChkAllocPrivCtxFor(pctx, f, size, file, line, p_mo);
        memchk_alloc--;
        if (oldptr == NULL) {
            return (ptr);
        }
        oldsize = SsMemChkGetSize(MEMCHK_GETSYSTEMPTR(oldptr));
        memcpy(ptr, oldptr, SS_MIN(size, oldsize));
        SsFFmemChkFreePrivCtxFor(pctx, f, oldptr, file, line);
        memchk_free--;
        return(ptr);
}

/*#**********************************************************************\
 *
 *		SsFFmemChkGetDataSize
 *
 * Check version of SsFFmemGetDataSize.
 *
 * When compiled with SS_DEBUG, SsFFmemGetDataSize is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	ptr - in, take
 *		pointer to be released
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
size_t SsFFmemChkGetDataSize(
        void* ptr,
        char *file,
        int line)
{
        SsSemEnter(ss_memchk_sem);

        memchk_free++;
        memchk_nptr--;
        if (ptr != NULL) {
            ptr = MEMCHK_GETSYSTEMPTR(ptr);
        } else {
            SsMemChkAbort("NULL pointer passed to SsFFmemChkFree", file, line);
        }
        SsMemChkCheckPtr(ptr, file, line);

        SsSemExit(ss_memchk_sem);

        return SsFFmemGetDataSize(ptr);
}
#endif /* SS_FFMEM */


/*##**********************************************************************\
 *
 *		SsMemChkSetLinkInfo
 *
 *
 *
 * Parameters :
 *
 *	ptr -
 *
 *
 *	file -
 *
 *
 *	line -
 *
 *
 *	linkp -
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
void SsMemChkSetLinkInfo(
        void* ptr,
        char* file,
        int   line,
        bool  linkp)
{
        SsMemChkListT* list;

        if (ss_memdebug) {
            int i;

            SsSemEnter(ss_memchk_sem);

            ptr = MEMCHK_GETSYSTEMPTR(ptr);
            list = SsMemChkListFind(ptr, file, line);

            if (list->memlst_linkinfo == NULL) {
                list->memlst_linkinfo = calloc(
                                            sizeof(list->memlst_linkinfo),
                                            SSMEM_MAXLINKINFO);
                ss_assert(list->memlst_linkinfo != NULL);
            }

            for (i = 0; i < SSMEM_MAXLINKINFO; i++) {
                if (list->memlst_linkinfo[i].ml_file == NULL) {
                    /* Add new info. */
                    list->memlst_linkinfo[i].ml_file = file;
                    list->memlst_linkinfo[i].ml_line = line;
                    list->memlst_linkinfo[i].ml_linkp = linkp;
                    list->memlst_linkinfo[i].ml_count = 1;
                    break;
                }
                if (list->memlst_linkinfo[i].ml_line == line &&
                    strcmp(list->memlst_linkinfo[i].ml_file, file) == 0 &&
                    list->memlst_linkinfo[i].ml_linkp == linkp) {
                    list->memlst_linkinfo[i].ml_count++;
                    break;
                }
            }
            ss_assert(i < SSMEM_MAXLINKINFO);

            SsSemExit(ss_memchk_sem);
        }
}

/*#**********************************************************************\
 *
 *		SsMemChkStrdup
 *
 * Check version of SsMemStrdup. Adds the allocated pointer to the check list.
 *
 * When compiled with SS_DEBUG, SsMemStrdup is expanded to a call to
 * this function.
 *
 * Parameters :
 *
 *	str - in, use
 *		string to be duplicated
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 * Return value - give :
 *
 *      pointer to the copy of str
 *
 * Limitations  :
 *
 * Globals used :
 */
char* SsMemChkStrdup(
        char* str,
        char* file,
        int   line,
        ss_memobjtype_t* p_mo)
{
        char* s;
        size_t size;

        memchk_strdup++;
        memchk_alloc--;

        size = strlen(str) + 1;
        s = SsMemChkAlloc(size, file, line, p_mo);
        memcpy(s, str, size);
        return(s);
}

/*##**********************************************************************\
 *
 *		SsMemSize
 *
 * Returns memory allocation size. Only for DEBUG version.
 *
 * Parameters :
 *
 *	ptr -
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
uint SsMemSize(void* ptr)
{
        return(SsMemChkGetSize(MEMCHK_GETSYSTEMPTR(ptr)));
}

void SsMemLinkInit(void* p)
{
        SsQmemLinkInit(MEMCHK_GETSYSTEMPTR(p));
}

uint SsMemLinkGet(void* p)
{
        uint nlinks;
        nlinks = SsQmemLinkGet(MEMCHK_GETSYSTEMPTR(p));
        return (nlinks);
}
uint SsMemLinkInc(void* p)
{
        uint nlinks;

        nlinks = SsQmemLinkInc(MEMCHK_GETSYSTEMPTR(p));
        return (nlinks);
}
uint SsMemLinkDec(void* p)
{
        uint nlinks;

        nlinks = SsQmemLinkDec(MEMCHK_GETSYSTEMPTR(p));
        return (nlinks);
}
uint SsMemLinkDecZeroFree(void* p)
{
        uint nlinks;

        nlinks = SsQmemLinkDecZeroFree(MEMCHK_GETSYSTEMPTR(p));
        if (nlinks == 0) {
            SsMemFree(p);
        }
        return (nlinks);
}
uint SsMemLinkIncSafe(void* p)
{
        uint nlinks;

        nlinks = SsQmemLinkIncSafe(MEMCHK_GETSYSTEMPTR(p));
        return (nlinks);
}
uint SsMemLinkDecSafe(void* p)
{
        uint nlinks;

        nlinks = SsQmemLinkDecSafe(MEMCHK_GETSYSTEMPTR(p));
        return (nlinks);
}

/* SsMemAlloc for SS_DEBUG version, normal SsMemAlloc described below.
*/

void* SsMemAlloc(size)
        size_t size;
{
        ss_memobjtype_t mo = SS_MEMOBJ_GENERIC;
        return(SsMemChkAlloc(size, (char *)__FILE__, __LINE__, &mo));
}

/* SsMemCalloc for SS_DEBUG version, normal SsMemCalloc described below.
*/

void* SsMemCalloc(
        unsigned nelem,
        size_t elsize)
{
        ss_memobjtype_t mo = SS_MEMOBJ_GENERIC;
        return(SsMemChkCalloc(nelem, elsize, (char *)__FILE__, __LINE__, &mo));
}

/* SsMemRealloc for SS_DEBUG version, normal SsMemRealloc described below.
*/

void* SsMemRealloc(
        void*    oldptr,
        size_t size)
{
        ss_memobjtype_t mo = SS_MEMOBJ_GENERIC;
        return(SsMemChkRealloc(oldptr, size, (char *)__FILE__, __LINE__, &mo));
}

/* SsMemFree for SS_DEBUG version, normal SsMemFree described below.
*/

void  SsMemFree(
        void* ptr)
{
        SsMemChkFree(ptr, (char *)__FILE__, __LINE__);
}

/* SsMemStrdup for SS_DEBUG version, normal SsMemStrdup described below.
*/

char* SsMemStrdup(
        char* str)
{
        ss_memobjtype_t mo = SS_MEMOBJ_GENERIC;
        return(SsMemChkStrdup(str, (char *)__FILE__, __LINE__, &mo));
}

#else /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		SsMemAlloc
 *
 * Replacement for library function malloc.
 *
 * Parameters :
 *
 *	size - in
 *		number of bytes to allocate
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemAlloc(
        size_t size)
{
        char* p;

        if (size == 0) {
            size = 1;
        }
        p = SsQmemAlloc(size);
        ss_assert(p != NULL);
        return(p);
}

/*##**********************************************************************\
 *
 *		SsMemCalloc
 *
 * Replacement for library function calloc.
 *
 * Parameters :
 *
 *	nelem - in
 *		number of elements to allocate
 *
 *	elsize - in
 *		element size
 *
 * Return value - give :
 *
 *      pointer to the memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemCalloc(
        unsigned nelem,
        size_t elsize)
{
        char* p;

        if (nelem == 0 || elsize == 0) {
            nelem = 1;
            elsize = 1;
        }
        p = SsQmemCalloc(nelem, elsize);
        ss_assert(p != NULL);
        return(p);
}

/*##**********************************************************************\
 *
 *		SsMemRealloc
 *
 * Replacement for library function realloc.
 *
 * Parameters :
 *
 *	oldptr - in, take
 *		old pointer
 *
 *	size - in
 *		new size for memory area
 *
 * Return value - give :
 *
 *      pointer to the reallocated memory area
 *
 * Limitations  :
 *
 * Globals used :
 */
void* SsMemRealloc(
        void*    oldptr,
        size_t size)
{
        char* p;

        if (size == 0) {
            size = 1;
        }
        p = SsQmemRealloc(oldptr, size);
        ss_assert(p != NULL);
        return(p);
}

/*##**********************************************************************\
 *
 *		SsMemFree
 *
 * Replacement for library function free.
 *
 * Parameters :
 *
 *	ptr - in, take
 *		pointer to be released
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemFree(
        void* ptr)
{
        ss_dassert(ptr != NULL);
        SsQmemFree(ptr);
}

/*##**********************************************************************\
 *
 *		SsMemStrdup
 *
 * Replacement for library function strdup.
 *
 * Parameters :
 *
 *	str - in, use
 *		string to be duplicated
 *
 * Return value - give :
 *
 *      pointer to the copy of str
 *
 * Limitations  :
 *
 * Globals used :
 */
char* SsMemStrdup(
        char* str)
{
        char* s;

        s = SsQmemStrdup(str);
        ss_assert(s != NULL);
        return(s);
}

#endif /* SS_DEBUG */

/*##**********************************************************************\
 *
 *		SsMemGlobalInit
 *
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsMemGlobalInit(void)
{
        SsQmemGlobalInit();
#ifdef SS_DEBUG
        ss_memchk_sem = SsQmemAlloc(SsSemSizeLocal());
        SsSemCreateLocalBuf(ss_memchk_sem, SS_SEMNUM_SS_MEMCHK);
#endif /* SS_DEBUG */
}
