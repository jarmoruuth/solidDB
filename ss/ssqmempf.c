/*************************************************************************\
**  source       * ssqmempf.c
**  directory    * ss
**  description  * Purify versions of SsQmem routines.
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

#include "ssstddef.h"
#include "ssstdlib.h"
#include "ssstring.h"
#include "sslimits.h"
#include "sssprint.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssqmem.h"
#include "sssem.h"
#include "ssthread.h"
#include "ssltoa.h"

#if defined(SS_PURIFY)

#undef SsQmemAlloc
#undef SsQmemRealloc
#undef SsQmemFree
#undef SsQmemGetInfo
#undef SsQmemGetDataSize

void *(*SsQmemAllocPtr)(size_t size) = SsQmemAlloc;
void *(*SsQmemReallocPtr)(void* oldptr, size_t newsize) = SsQmemRealloc;
void (*SsQmemFreePtr)(void* ptr) = SsQmemFree;
void (*SsQmemGetInfoPtr)(SsQmemStatT* p_qmi) = SsQmemGetInfo;
size_t (*SsQmemGetDataSizePtr)(void* ptr) = SsQmemGetDataSize;

/* Overhead at the beginning of every allocation. This is used also when
 * the memory is allocated directly from the system.
 */
#define QMEM_ALIGNMENT          SS_ALIGNMENT
#define QMEM_OVERHEAD           QMEM_ALIGNMENT

/* Returns the user pointer from a qmem pointer.
 */
#define QMEM_GETUSERPTR(p)      ((void*)((char*)(p) + QMEM_OVERHEAD))

/* Returns the qmem pointer from a user pointer.
 */
#define QMEM_GETQMEMPTR(p)      ((qmem_slot_t*)((char*)(p) - QMEM_OVERHEAD))

typedef struct qmem_slot_st qmem_slot_t;

struct qmem_slot_st {
        ss_uint2_t s_nlink; /* Ref. count */
};

/*************************************************************************\
 * 
 *		QMEM_PAGESIZE
 * 
 * QMEM_PAGESIZE is the size in which the slots are allocated
 * from the system.
 * QMEM_PAGESIZE * QMEM_MINSLOTS must be smaller than 0xffff.
 */
#define DEFAULT_QMEM_PAGESIZE   (4*1024)

size_t ss_qmem_pagesize = DEFAULT_QMEM_PAGESIZE;

SsQmemStatT ss_qmem_stat;

unsigned int ss_qmem_slotmap[1];
unsigned int ss_qmem_slotmask[1];

ss_debug(long ss_qmem_nptr;)
ss_debug(long ss_qmem_nsysptr;)
ss_debug(extern int ss_memdebug_usefreedpool;)

/*##**********************************************************************\
 * 
 *		SsQmemGlobalInit
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
void SsQmemGlobalInit(void)
{
        ss_assert(sizeof(qmem_slot_t) <= QMEM_OVERHEAD);
        ss_debug(ss_memdebug_usefreedpool = FALSE;)
}

void SsQmemCtxDone(void* ctx)
{
        /* dummy */
}

SsMemCtxT* SsQmemLocalCtxInit(void)
{
        /* dummy */
        return(NULL);
}

void SsQmemLocalCtxDone(SsMemCtxT* qmem)
{
        /* dummy */
}

/*##**********************************************************************\
 * 
 *		SsQmemAlloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	size - 
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
void* SsQmemAlloc(size_t size)
{
        qmem_slot_t* s;

        if (size == 0) {
            size = 1;
        }

        s = malloc(QMEM_OVERHEAD + size);
        ss_assert(s != NULL);

        s->s_nlink = 0;

        return(QMEM_GETUSERPTR(s));
}

void* SsQmemCtxAlloc(SsMemCtxT* qmem, size_t size)
{
        return(SsQmemAlloc(size));
}

/*##**********************************************************************\
 * 
 *		SsQmemCalloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	elsize - 
 *		
 *		
 *	nelem - 
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
void* SsQmemCalloc(size_t elsize, size_t nelem)
{
        char* p;
        size_t size;

        size = elsize * nelem;
        p = SsQmemAlloc(size);
        if (p != NULL) {
            memset(p, '\0', size);
        }
        return(p);
}

/*##**********************************************************************\
 * 
 *		SsQmemStrdup
 * 
 * 
 * 
 * Parameters : 
 * 
 *	str - 
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
void* SsQmemStrdup(char* str)
{
        char* p;
        size_t size;

        size = strlen(str) + 1;
        p = SsQmemAlloc(size);
        if (p != NULL) {
            memcpy(p, str, size);
        }
        return(p);
}

/*##**********************************************************************\
 * 
 *		SsQmemRealloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	oldptr - 
 *		
 *		
 *	newsize - 
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
void* SsQmemRealloc(void* oldptr, size_t newsize)
{
        qmem_slot_t* old_s;
        qmem_slot_t* new_s;

        if (newsize == 0) {
            newsize = 1;
        }

        if (oldptr == NULL) {
            return SsQmemAlloc(newsize);
        }

        old_s = QMEM_GETQMEMPTR(oldptr);

        new_s = realloc(old_s, QMEM_OVERHEAD + newsize);
        ss_assert(new_s != NULL);
        
        return(QMEM_GETUSERPTR(new_s));
}

/*##**********************************************************************\
 * 
 *		SsQmemFree
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
void SsQmemFree(void* ptr)
{
        qmem_slot_t* s;

        s = QMEM_GETQMEMPTR(ptr);

        ss_assert(s->s_nlink == 0);

        free(s);
}

void SsQmemCtxFree(SsMemCtxT* qmem, void* ptr)
{
        SsQmemFree(ptr);
}

/*##**********************************************************************\
 * 
 *		SsQmemPrintInfo
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
void SsQmemPrintInfo(void)
{
}

/*##**********************************************************************\
 * 
 *		SsQmemGetInfo
 * 
 * Stores current qmem info *p_qmi
 * 
 * Parameters : 
 * 
 *	p_qmi - out
 *		Current qmem info is stored into *p_qmi.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsQmemGetInfo(SsQmemStatT* p_qmi)
{
        *p_qmi = ss_qmem_stat;
}

/*##**********************************************************************\
 * 
 *		SsQmemGetSlotNo
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
size_t SsQmemGetSlotNo(void* p)
{
        return(1);
}

size_t SsQmemGetDataSize(void* p)
{
        ss_error;
        return 0;
}

/*#***********************************************************************\
 * 
 *		QmemTest
 * 
 * Tests integrity of qmem data structures.
 * 
 * Parameters : 
 * 
 *	fastp - in
 *		If TRUE, checks only the first element of the list.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static bool QmemTest(bool fastp)
{
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		SsQmemTest
 * 
 * Tests integrity of qmem data structures.
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
bool SsQmemTest(void)
{
        return(QmemTest(FALSE));
}

/*##**********************************************************************\
 * 
 *		SsQmemTest
 * 
 * Tests integrity of qmem data structures. This is a fast version that
 * does not scan through the lists, only the first item is checked.
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
bool SsQmemTestFast(void)
{
        return(QmemTest(TRUE));
}

#define LINK_GET(p, n) \
{ \
        qmem_slot_t* s = QMEM_GETQMEMPTR(p); \
        n = s->s_nlink;\
}

#define LINK_INC(p, n) \
{ \
        qmem_slot_t* s = QMEM_GETQMEMPTR(p); \
        n = ++s->s_nlink;\
        ss_dassert(n > 0); \
}

#define LINK_DEC(p, n) \
{ \
        qmem_slot_t* s = QMEM_GETQMEMPTR(p); \
        ss_dassert(s->s_nlink > 0); \
        n = --s->s_nlink; \
}

void SsQmemLinkInit(void* p)
{
        qmem_slot_t* s;

        s = QMEM_GETQMEMPTR(p);

        s->s_nlink = 1;
}

uint SsQmemLinkGet(void* p)
{
        uint nlink;

        LINK_GET(p, nlink);
        return (nlink);
}

uint SsQmemLinkInc(void* p)
{
        uint nlink;

        LINK_INC(p, nlink);

        return(nlink);
}

uint SsQmemLinkDec(void* p)
{
        uint nlink;

        LINK_DEC(p, nlink);

        return(nlink);
}

uint SsQmemLinkDecZeroFree(void* p)
{
        uint nlink;

        LINK_DEC(p, nlink);

        if (nlink == 0) {
            SsQmemFree(p);
        }

        return(nlink);
}

uint SsQmemLinkIncSafe(void* p)
{
        uint nlink;

        LINK_INC(p, nlink);

        return(nlink);
}

uint SsQmemLinkDecSafe(void* p)
{
        uint nlink;

        LINK_DEC(p, nlink);

        return(nlink);
}

long SsQmemGetBalance(void)
{
        return (0L);
}

void SsQmemSetOperationBalance(long balance, long peak)
{
}

void SsQmemGetOperationBalance(long* p_balance, long* p_peak)
{
        *p_balance = 0;
        *p_peak = 0;
}

void SsQmemFPrintInfo(void* fp)
{
}

#endif /* defined(SS_PURIFY) */

