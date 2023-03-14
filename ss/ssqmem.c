/*************************************************************************\
**  source       * ssqmem.c
**  directory    * ss
**  description  * Quick memory allocation functions.
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
#undef SsQmemAlloc
#undef SsQmemRealloc
#undef SsQmemFree
#undef SsQmemGetInfo
#undef SsQmemGetDataSize

#include "sssem.h"

#if !defined(SS_PURIFY)

#ifndef SS_MYSQL
#include "ssmemw16.h"
#include "ssmemnlm.h"
#endif /* !SS_MYSQL */

#include "ssthread.h"
#include "ssltoa.h"

/*#define QMEM_TRACENEXTPTR *//* not on by default! */

#ifndef SS_QMEM_THREADCTX
#if (defined(SS_NT) && !defined(SS_DLLQMEM))
# define SS_QMEM_THREADCTX
#endif
#endif /* !SS_QMEM_THREADCTX */

#if defined(SS_PAGED_MEMALLOC_AVAILABLE) && !defined(SS_NT)
# define QMEM_PAGEALLOC
#endif

#ifdef SSSYSRES_USED
#define SSQMEM_ENTER_SYSRES 	  SsFlatMutexLock(qmem_sem)
#define SSQMEM_EXIT_SYSRES 	  SsFlatMutexUnlock(qmem_sem)
#else /* SSSYSRES_USED */
#define SSQMEM_ENTER_SYSRES
#define SSQMEM_EXIT_SYSRES
#endif /* SSSYSRES_USED */

#ifdef SS_PMEM
void *(*SsQmemAllocPtr)(size_t size) = SsQmemAlloc;
void *(*SsQmemReallocPtr)(void* oldptr, size_t newsize) = SsQmemRealloc;
void (*SsQmemFreePtr)(void* ptr) = SsQmemFree;
void (*SsQmemGetInfoPtr)(SsQmemStatT* p_qmi) = SsQmemGetInfo;
size_t (*SsQmemGetDataSizePtr)(void* ptr) = SsQmemGetDataSize;
#endif /* SS_PMEM */

#ifdef QMEM_TRACENEXTPTR

#ifndef ss_ptr_as_scalar_t
#define ss_ptr_as_scalar_t size_t
#endif /* ss_ptr_as_scalar_t */

#define QMEM_NEXTPTR_CHECK(s) \
do {\
        ss_ptr_as_scalar_t not_next = ~(ss_ptr_as_scalar_t)((s)->s_.next);\
        if ((ss_ptr_as_scalar_t)(&((s)->s_.next))[1] != not_next) {\
            char errormsg[200];\
            SsSprintf(\
            errormsg,\
            "Freed memory write at addr: %08lX, ThreadID: %u,"\
            " next: %08lX, nextchk: %08lX\n",\
            (ulong)&((s)->s_.next),\
            (uint)SsThrGetid(),\
            (ulong)(s)->s_.next,\
            ~(ulong)(&((s)->s_.next))[1]);\
            SsLogErrorMessage(errormsg);\
        }\
} while (0)

#define QMEM_NEXTPTR_SETCHECK(s) \
do {\
        ss_ptr_as_scalar_t not_next = ~(ss_ptr_as_scalar_t)((s)->s_.next);\
        (&((s)->s_.next))[1] = (void*)not_next;\
} while (0)

#define QMEM_SLOTTAB_CHECK(qst) \
do {\
        ss_ptr_as_scalar_t not_slotlist =\
            ~(ss_ptr_as_scalar_t)((qst)->qst_slotlist);\
        if ((qst)->qst_slotlist_check != not_slotlist) {\
            char errormsg[200];\
            SsSprintf(\
                errormsg,\
                "static memory corruption at addr: %08lX, ThreadID: %u,"\
                " slotlist: %08lX, slotlist_check: %08lX\n",\
                (ulong)qst,\
                (uint)SsThrGetid(),\
                (ulong)((qst)->qst_slotlist),\
                ~(ulong)((qst)->qst_slotlist_check));\
            SsLogErrorMessage(errormsg);\
        }\
} while (0)

#define QMEM_SLOTTAB_SETCHECK(qst) \
do {\
        ss_ptr_as_scalar_t not_slotlist =\
            ~(ss_ptr_as_scalar_t)((qst)->qst_slotlist);\
        (qst)->qst_slotlist_check = not_slotlist;\
} while (0)

#define QMEM_SLOTTAB_INITCHECKS(st) \
do {\
        uint i;\
        for (i = 0; i < QMEM_NSLOT+1; i++) {\
            QMEM_SLOTTAB_SETCHECK(&(st)[i]);\
        }\
} while (0);

#else /* QMEM_TRACENEXTPTR */

#define QMEM_SLOTTAB_INITCHECKS(st)
#define QMEM_NEXTPTR_SETCHECK(s)
#define QMEM_NEXTPTR_CHECK(s)
#define QMEM_SLOTTAB_SETCHECK(qst)
#define QMEM_SLOTTAB_CHECK(qst)

#endif /* QMEM_TRACENEXTPTR */
/*************************************************************************\
 * 
 *		QMEM_PAGEALLOC
 * 
 */
#ifdef QMEM_PAGEALLOC

# include "ssmempag.h"

#   define QMEM_MIN_NPAGES 8


static size_t qmem_ospagesize;

# define QMEM_SYSMALLOC(s)  SsMemPageAlloc(s)
# define QMEM_SYSFREE(p,s)  SsMemPageFree(p,s)
# define QMEM_SYSREALLOC(oldp, newsize, oldsize) SsMemPageRealloc(oldp, newsize, oldsize)
#else /* QMEM_PAGEALLOC */
# define QMEM_SYSMALLOC(s)  malloc(s)
# define QMEM_SYSFREE(p,s)  free(p)
# define QMEM_SYSREALLOC(oldp, newsize, oldsize) realloc(oldp, newsize)
# define QMEM_MIN_NPAGES 1
#endif  /* QMEM_PAGEALLOC */

/* --------------------------------------------------------------------- */

/* Defining SS_QMEM_SLOTMAP enables fast mapping of size to slot number
 * without bit shifting loop
 */
#define SS_QMEM_SLOTMAP

/* Defining SS_QMEM_NO_JMP enables further optimized mapping of size to slot
 * number. This optimization either makes the mapping faster or not
 * depending on machine architecture. It makes no conditional jumps
 * when doing the mapping and uses two map tables instead.
 */

unsigned int ss_qmem_slotmap[1 << SS_CHAR_BIT] = {
        0,                      /* 00000000 */
        1,                      /* 00000001 */
        2, 2,                   /* 0000001X */
        3, 3, 3, 3,             /* 000001XX */
        4, 4, 4, 4, 4, 4, 4, 4, /* 00001XXX */
        5, 5, 5, 5, 5, 5, 5, 5, /* 0001XXXX */
        5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, /* 001XXXXX */
        6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 
        7, 7, 7, 7, 7, 7, 7, 7, /* 01XXXXXX */
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8, /* 1XXXXXXX */
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8, 
        8, 8, 8, 8, 8, 8, 8, 8
};

/* --------------------------------------------------------------------- */
# ifdef SS_QMEM_NO_JMP
unsigned int ss_qmem_slotmask[1 << SS_CHAR_BIT] = {
         0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0,
        ~0,~0,~0,~0,~0,~0,~0,~0
};

# endif  /* SS_QMEM_NO_JMP */

/* --------------------------------------------------------------------- */

/*************************************************************************\
 * 
 *		SLOT_ALLOC
 */
#define SLOT_ALLOC(slot_table, slotno, ptr) \
do { \
        qmem_slot_t* s; \
        QMEM_SLOTTAB_CHECK(&slot_table[slotno]);\
        s = slot_table[slotno].qst_slotlist; \
        QMEM_NEXTPTR_CHECK(s);\
        slot_table[slotno].qst_slotlist = s->s_.next; \
        QMEM_SLOTTAB_SETCHECK(&slot_table[slotno]);\
        ptr = QMEM_GETUSERPTR(s); \
} while (0)

/* Number of possible slots.
 */
#define QMEM_NSLOT              31

/* Special slot number is used for allocations that are
 * done directly from the system using malloc. Allocations
 * larger than QMEM_MAXSLOTALLOC are allocated from system.
 * The value must be same as QMEM_NSLOT.
 */
#define QMEM_SYSTEMSLOT         QMEM_NSLOT

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

# define QMEM_PAGESIZE          ss_qmem_pagesize

/*************************************************************************\
 *
 *              QMEM_MAXSLOTALLOC
 *
 * Items larger than QMEM_MAXSLOATALLOC are allocated using system malloc.
 * QMEM_MAXSLOTALLOC * QMEM_MINSLOTS must be smaller than 0xffff.
 */
# define QMEM_MAXSLOTALLOC      QMEM_PAGESIZE
# define QMEM_MINSLOTS          4


/*************************************************************************/

/* Overhead at the beginning of every allocation. This is used also when
 * the memory is allocated directly from the system.
 */
#define QMEM_ALIGNMENT          SS_ALIGNMENT
#define QMEM_OVERHEAD           QMEM_ALIGNMENT
#define QMEM_SYSSLOTOVERHEAD    \
        (SS_ALIGNMENT * (((2 * sizeof(ss_uint2_t) + sizeof(ss_uint4_t)) +\
        SS_ALIGNMENT - 1) / SS_ALIGNMENT))

/* Returns the user pointer from a qmem pointer.
 */
#define QMEM_GETUSERPTR(p)      ((void*)((char*)(p) + QMEM_OVERHEAD))

/* Returns the qmem pointer from a user pointer.
 */
#define QMEM_GETQMEMPTR(p)      ((qmem_slot_t*)((char*)(p) - QMEM_OVERHEAD))

/* Returns the slot number that is stored at the beginning of the
 * pointer. The input pointer must be a user pointer. The slot number
 * is before the user memory area.
 */
#define QMEM_GETSLOTNO(p)       (((qmem_slot_t*)QMEM_GETQMEMPTR(p))->s_no)

/* The thread context can hold at most QMEM_CTX_MAXNPTR free pointers.
 * When the limit is reached, almost all free pointers from the thread context
 * are flushed to the global context.
 */
#define DEFAULT_QMEM_CTX_MAXNPTR    200
#define QMEM_CTX_MAXNPTR            ss_qmem_ctxmaxnptr

size_t ss_qmem_ctxmaxnptr = DEFAULT_QMEM_CTX_MAXNPTR;

/* This is the number of mem free calls after which we do flush even if pointer
 * limit is not reached. This is used to flush long queues
 */
#define DEFAULT_QMEM_CTX_FLUSHFREQ  1000
#define QMEM_CTX_FLUSHFREQ          ss_qmem_ctxflushfreq

size_t ss_qmem_ctxflushfreq = DEFAULT_QMEM_CTX_FLUSHFREQ;

/* This is the max number of spare slots that are left to a slot
 * when extra space is flushed to global context. The same
 * constant also defines the number of slots that are taken from the
 * global slot table in one mutex section
 */
#define QMEM_MAXLOCALSPARE         40
#define QMEM_MINLOCALSPARE         4

#define QMEM_SYSSLOTSETSIZEANDNO(p_sys, s, n) \
{\
    if (QMEM_SYSSLOTOVERHEAD == QMEM_OVERHEAD) {\
        *((ss_uint4_t*)(p_sys) + 1) = (s);\
        ((qmem_slot_t*)(p_sys))->s_no = (ss_uint2_t)(n);\
        ((qmem_slot_t*)(p_sys))->s_nlink = (ss_uint2_t)0;\
    } else {\
        ss_dassert(QMEM_SYSSLOTOVERHEAD - QMEM_OVERHEAD == sizeof(ss_uint4_t));\
        *((ss_uint4_t*)p_sys) = (s);\
        ((qmem_slot_t*)((ss_uint4_t*)p_sys + 1))->s_no = (ss_uint2_t)(n);\
        ((qmem_slot_t*)((ss_uint4_t*)p_sys + 1))->s_nlink = (ss_uint2_t)0;\
    }\
}

#define QMEM_SYSSLOTGETSIZE(p_usr) \
        ((QMEM_SYSSLOTOVERHEAD == QMEM_OVERHEAD) ? \
        *((ss_uint4_t*)(p_usr) - 1) : *((ss_uint4_t*)QMEM_GETQMEMPTR(p_usr)-1))

/* Slot data structure. slot_table data structure contains a list
 * of qmem_slot_t. These entries are free entries.
 */
typedef struct qmem_slot_st qmem_slot_t;

struct qmem_slot_st {
        ss_uint2_t s_nlink; /* Ref. count */
        ss_uint2_t s_no;    /* Slot number. */
        union {
            qmem_slot_t*    next;   /* Pointer to next slot. */
            char            buf[1]; /* User data buffer area. */
        } s_;
};

typedef struct qmem_slottabentry_st {
        qmem_slot_t* qst_slotlist;
#ifdef QMEM_TRACENEXTPTR
        ss_ptr_as_scalar_t qst_slotlist_check;
#endif /* QMEM_TRACENEXTPTR */
#ifdef SS_PROFILE
        ulong        qst_prevpage;
#endif /* SS_PROFILE */
} qmem_slottabentry_t;

struct SsMemCtxStruct {
        qmem_slottabentry_t qctx_slottable[QMEM_NSLOT+1];
        long         qctx_balance; /* allocation balance (in bytes) */
        long         qctx_operationbalance; /* allocation balance (in bytes) for current operation */
        long         qctx_operationpeak;    /* allocation peak (in bytes) for current operation */
        long         qctx_operationstacklevel; /* stack level for recursive calls */
        long         qctx_nptr;
        long         qctx_nflushreq;
        long         qctx_localbytecount;   /* Local value to update global counter. */
        long         qctx_localptrcount;    /* Local value to update global counter. */
        long         qctx_localhitcount;    /* Local value to update global counter. */
        ulong        qctx_slotalloccount[SS_QMEM_SYSTEMSLOT+1]; /* Allocations for each slot. */
        ss_debug(int qctx_isactive;)
};

/* Table of free memory. The last slot is an extra slot for system
 * allocations. There are never any items in there, but we may index
 * also that slot and test that it is NULL.
 */
static qmem_slottabentry_t system_slot_table[QMEM_NSLOT+1];

static bool qmem_sem_initialized = FALSE;
static SsFlatMutexT qmem_sem;

SsQmemStatT ss_qmem_stat;

ss_debug(long ss_qmem_nptr;)
ss_debug(long ss_qmem_nsysptr;)

#ifdef SS_PROFILE

#define QMEM_HARDWARE_PAGESIZE 4096UL
static ulong qmem_npagehit = 0;
static ulong qmem_npagemiss = 0;
#define QMEM_PAGEOF(p) ((ulong)(p) & ~(QMEM_HARDWARE_PAGESIZE-1UL))

#endif /* SS_PROFILE */

/*#***********************************************************************\
 * 
 *		QmemOutOfMemory
 * 
 * 
 * 
 * Parameters : 
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
static void QmemOutOfMemory(int size, char* file, int line)
{
        char buf_alloc[20];
        char buf_total[20];
        static char out_of_memory_text[80] = "SOLID Fatal error: Out of central memory";
        
        SsLtoa(
            (long)size,
            buf_alloc,
            10);
        SsLtoa(
            ss_qmem_stat.qms_sysbytecount + ss_qmem_stat.qms_slotbytecount,
            buf_total,
            10);
        strcat(out_of_memory_text, " (alloc size = ");
        strcat(out_of_memory_text, buf_alloc);
        strcat(out_of_memory_text, ", total bytes = ");
        strcat(out_of_memory_text, buf_total);
        strcat(out_of_memory_text, ")\n");

        SsAssertionFailureText(out_of_memory_text, file, line);
}

/*************************************************************************\
 * 
 *		SS_QMEM_THREADCTX
 */
#if defined(SS_QMEM_THREADCTX)

/* Windows NT --------------------------------------------------------------------- */
# if defined(SS_NT) 

/* Define qmem_threadctx as a (static) thread local storage variable.
 * A thread local storage variable has
 * automatically a separate instance for every thread.
 */

#ifdef SS_USE_WIN32TLS_API

#   define QMEM_THREADCTX_GET(qmem) {(qmem) = SsThrDataGet(SS_THRDATA_QMEMCTX);}
/* Set value for thread memory ctx
 */
#   define QMEM_THREADCTX_SET(ctx) \
       {\
           SsThrDataSet(SS_THRDATA_QMEMCTX, (ctx));\
       }

#else /* SS_USE_WIN32TLS_API */

static __declspec(thread) SsMemCtxT* qmem_threadctx = NULL;

/* Get memory context of the current thread and return it in parameter qmem.
 */
#   define QMEM_THREADCTX_GET(qmem) \
        { \
            (qmem) = qmem_threadctx; \
        }

/* Set value for thread memory ctx
 */
#   define QMEM_THREADCTX_SET(ctx) \
        {\
            qmem_threadctx = (ctx);\
        }

#endif /* SS_USE_WIN32TLS_API */

/* --------------------------------------------------------------------- */
# elif defined(SS_THRIDBYSTK)

/* These routines rely on properties of SsThrGetid().
 * It assumes the thread id is densely allocated small
 * range of values which can be used as indices to a small
 * array. This is currently the case in OS/2 (both 16 and 32 bit).
 * and in systems where we have #defined SS_THRIDBYSTK.
 * Another required property for SsThrGetid() is that it must be
 * considerably quicker than local semaphore enter+exit. Otherwise
 * it makes no sense to #define SS_QMEM_THREADCTX that enables
 * the threadwise memory contexts.
 */
#   define QMEM_MAXTHREAD  SS_THR_MAXTHREADS
#   define QMEM_THRID1ST   1

static SsMemCtxT* qmem_threadctx[QMEM_MAXTHREAD];

/* Get memory context of the current thread and return it in parameter qmem.
 */
#   define QMEM_THREADCTX_GET(qmem) \
        { \
            uint thrid; \
            thrid = SsThrGetid(); \
            ss_dassert(thrid <= QMEM_MAXTHREAD); \
            (qmem) = (qmem_threadctx-QMEM_THRID1ST)[thrid]; \
        }

/* Set value for thread memory ctx
 */
#   define QMEM_THREADCTX_SET(ctx) \
        { \
            uint thrid; \
            thrid = SsThrGetid(); \
            ss_dassert(thrid <= QMEM_MAXTHREAD); \
            (qmem_threadctx-QMEM_THRID1ST)[thrid] = (ctx); \
        }


/* --------------------------------------------------------------------- */
# elif defined(SS_PTHREAD)

#   define QMEM_THREADCTX_GET(qmem) {(qmem) = SsThrDataGet(SS_THRDATA_QMEMCTX);}
/* Set value for thread memory ctx */
#   define QMEM_THREADCTX_SET(ctx) \
       {\
           SsThrDataSet(SS_THRDATA_QMEMCTX, (ctx));\
       }

# endif /* SS_NT */

/* Initialize memory context for the current thread.
 */
#   define QMEM_THREADCTX_INIT() QMEM_THREADCTX_SET(qmem_ctx_initlocal())


/* --------------------------------------------------------------------- */
# define QMEM_THREADCTX_BEGINACCESS(qmem) \
        ss_dassert((qmem)->qctx_isactive++ == 0)

# define QMEM_THREADCTX_ENDACCESS(qmem) \
        ss_dassert((qmem) == NULL || (qmem)->qctx_isactive-- == 1)

#endif /* SS_QMEM_THREADCTX */

/*#***********************************************************************\
 * 
 *		qmem_ctx_initlocal
 * 
 * Locally used context init, which uses system calloc() to prevent
 * endless recursion
 * 
 * Parameters : 	 - none
 * 
 * Return value :
 *      New memory context
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static SsMemCtxT* qmem_ctx_initlocal(void)
{
        SsMemCtxT* qmem;

        qmem = calloc(1, sizeof(SsMemCtxT));
        if (qmem == NULL) {
            QmemOutOfMemory(1 * sizeof(SsMemCtxT), (char *)__FILE__, __LINE__);
        }
        QMEM_SLOTTAB_INITCHECKS(qmem->qctx_slottable);
        ss_qmem_stat.qms_nthrctx++;
        return(qmem);
}

static int qmem_ctx_maxslotptr(int slotno, int maxptr)
{
        int n_max;

        n_max = QMEM_PAGESIZE / (1 << slotno);
        n_max = SS_MIN(maxptr, n_max);
        if (n_max < QMEM_MINLOCALSPARE) {
            n_max = QMEM_MINLOCALSPARE;
        }
        return(n_max);

}

/*#***********************************************************************\
 * 
 *		QmemCtxFlush
 * 
 * Flushes part or all of the free memory blocks from local to global
 * context
 * 
 * Parameters : 
 * 
 *	qmem - in out
 *          pointer to qmem context
 *
 *      ntoleave - in
 *          maximum number of free blocks to leave to each slot
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void QmemCtxFlush(SsMemCtxT* qmem, uint ntoleave)
{
        uint i;
        uint n;
        uint n_max;
        qmem_slot_t* s;
        qmem_slot_t* new_tail;

        ss_dassert(qmem != NULL);

        ss_qmem_stat.qms_thrctxflushcount++;

        qmem->qctx_nptr = 0;
        qmem->qctx_nflushreq = 0;
        /* Put slots from the context to the system table.
         */
        for (i = 0; i < QMEM_NSLOT; i++) {
            QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[i]);
            s = qmem->qctx_slottable[i].qst_slotlist;
            ss_qmem_stat.qms_slotalloccount[i] += qmem->qctx_slotalloccount[i];
            qmem->qctx_slotalloccount[i] = 0;
            if (s != NULL) {
                new_tail = NULL;
                if (i >= sizeof(size_t) * SS_CHAR_BIT - 1) {
                    n_max = 0;  /* avoid left shift overflow */
                } else {
                    n_max = QMEM_PAGESIZE / ((size_t)1 << i);
                    n_max = SS_MIN(ntoleave, n_max);
                }
                for (n = 0; ; n++) {
                    QMEM_NEXTPTR_CHECK(s);
                    if (n >= n_max || s->s_.next == NULL) {
                        break;
                    }
                    qmem->qctx_nptr++;
                    new_tail = s;
                    s = s->s_.next;
                }
                if (n >= n_max) {
                    /* Find the last slot. */
                    int slotsize;
                    slotsize = (1 << i);
                    ss_qmem_stat.qms_thrctxbytecount -= slotsize;
                    ss_qmem_stat.qms_thrctxptrcount--;
                    ss_qmem_stat.qms_thrctxflushptrcount++;
                    for (;;) {
                        QMEM_NEXTPTR_CHECK(s);
                        if (s->s_.next == NULL) {
                            break;
                        }
                        s = s->s_.next;
                        ss_qmem_stat.qms_thrctxbytecount -= slotsize;
                        ss_qmem_stat.qms_thrctxptrcount--;
                        ss_qmem_stat.qms_thrctxflushptrcount++;
                    }
                    /* Join the list to the head of system slot table. */
                    QMEM_SLOTTAB_CHECK(&system_slot_table[i]);
                    s->s_.next = system_slot_table[i].qst_slotlist;
                    QMEM_NEXTPTR_SETCHECK(s);
                    if (new_tail == NULL) {
                        system_slot_table[i].qst_slotlist = qmem->qctx_slottable[i].qst_slotlist;
                        qmem->qctx_slottable[i].qst_slotlist = NULL;
                        QMEM_SLOTTAB_SETCHECK(&qmem->qctx_slottable[i]);
                    } else {
                        system_slot_table[i].qst_slotlist = new_tail->s_.next;
                        new_tail->s_.next = NULL;
                        QMEM_NEXTPTR_SETCHECK(new_tail);
                    }
                    QMEM_SLOTTAB_SETCHECK(&system_slot_table[i]);
                } else {
                    /* Nothing moved because local list length is
                     * less than or equal to n_max. The length,
                     * however, is one bigger than the current counter
                     * value indicates, so the counter is incremented
                     */
                    qmem->qctx_nptr++;
                }
                ss_debug(
                        if (ntoleave == 0) {
                            ss_dassert(qmem->qctx_slottable[i].qst_slotlist
                                       == NULL);
                        }
                    )
            }
        }
}

static void qmem_ctx_donelocal(SsMemCtxT* qmem)
{
        ss_qmem_stat.qms_nthrctx--;
        QmemCtxFlush(qmem, 0);
        free(qmem);
}

#ifdef SS_QMEM_THREADCTX

void SsQmemCtxDone(void* ctx)
{
        SsMemCtxT* qmem = ctx;
        bool setnull = (ctx == NULL);

        if (qmem == NULL) {
            QMEM_THREADCTX_GET(qmem);
        }
        if (qmem != NULL) {
            SsFlatMutexLock(qmem_sem);
            qmem_ctx_donelocal(qmem);
            if (setnull) {
                /* we can set the context to NULL safely,
                   if the ctx was got by this function */
                QMEM_THREADCTX_SET(NULL);
            }
            SsFlatMutexUnlock(qmem_sem);
        }
}

/* --------------------------------------------------------------------- */
#else /* SS_QMEM_THREADCTX */

# define QMEM_THREADCTX_GET(qmem)
# define QMEM_THREADCTX_INIT()
# define QMEM_THREADCTX_BEGINACCESS(qmem)
# define QMEM_THREADCTX_ENDACCESS(qmem)

void SsQmemCtxDone(void* ctx)
{
        /* dummy, because here is no thread-specific context */
}

#endif /* SS_QMEM_THREADCTX */

SsMemCtxT* SsQmemLocalCtxInit(void)
{
#if defined(SS_QMEM_THREADCTX)
        SsMemCtxT* qmem;

        QMEM_THREADCTX_GET(qmem);
        if (qmem == NULL) {
            QMEM_THREADCTX_INIT();
            QMEM_THREADCTX_GET(qmem);
            ss_dassert(qmem != NULL);
        }
        return(qmem);
#else
        return(NULL);
#endif
}

void SsQmemLocalCtxDone(SsMemCtxT* qmem)
{
}

/*************************************************************************\
 * 
 *		QMEM_CACHEROWALIGNMENT
 * 
 */
#define QMEM_CACHEROWALIGNMENT qmem_cacherowalignment
#define QMEM_CACHEROWALIGNMENT_MINUS_1 qmem_cacherowalignment_minus_1
#define QMEM_MINSIZE_ORMASK qmem_minsize_ormask

# if 1    /* This is now the default */

#   define QMEM_DEFAULT_CACHEROWALIGNMENT 16

# else

#   define QMEM_DEFAULT_CACHEROWALIGNMENT 1

# endif

#if 0
static size_t qmem_cacherowalignment =
        QMEM_DEFAULT_CACHEROWALIGNMENT;
#endif

static size_t qmem_cacherowalignment_minus_1 =
        QMEM_DEFAULT_CACHEROWALIGNMENT - 1;
static size_t qmem_minsize_ormask =
        (QMEM_DEFAULT_CACHEROWALIGNMENT - 1) | 15;

#if defined(SSSYSRES_USED)

static void SsQmemFreeFun(void* data) {
  free(data);
}

#endif /* defined(SSSYSRES_USED) */

/*#***********************************************************************\
 * 
 *		qmem_slot_alloc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	slotno - 
 *		
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
static void* qmem_slot_alloc(qmem_slottabentry_t* slot_table, uint slotno, size_t size)
{
        qmem_slot_t* s;

        if (slotno == QMEM_SYSTEMSLOT) {

            /* Allocate the area from the system. Special slot value
               is used to mark such allocations.
            */
            size_t* psz;

            ss_dassert(size > QMEM_MAXSLOTALLOC - QMEM_OVERHEAD);
            psz = QMEM_SYSMALLOC(size + QMEM_SYSSLOTOVERHEAD);
#if defined(SSSYSRES_USED)
	    SsSysResAdd(SsQmemFreeFun, psz);
#endif /* defined(SSSYSRES_USED) */
            if (psz == NULL) {
                QmemOutOfMemory(size + QMEM_SYSSLOTOVERHEAD, (char *)__FILE__, __LINE__);
            }
            if ((ulong)(ss_qmem_stat.qms_ptrmin - 1) > (ulong)(((ulong)psz) - 1)) {
                ss_qmem_stat.qms_ptrmin = (ulong)psz;
            }
            if (ss_qmem_stat.qms_ptrmax <= (ulong)psz) {
                ss_qmem_stat.qms_ptrmax = (ulong)psz;
            }
            /* Store area size to the beginning of allocated memory. */
            QMEM_SYSSLOTSETSIZEANDNO(psz, size, QMEM_SYSTEMSLOT);
            ss_qmem_stat.qms_sysptrcount++;
            ss_qmem_stat.qms_sysbytecount += size + QMEM_SYSSLOTOVERHEAD;
            ss_debug(ss_qmem_nsysptr++);
#if 0
            ss_dprintf_4(("qmem_slot_alloc: system, size = %u\n", size));
#endif
            return ((void*)((char*)psz + QMEM_SYSSLOTOVERHEAD));

        } else {

            /* Allocate nitems number of items at the same time and
             * add them to the free list. A larger block is allocated
             * from the system and the items are taken from the block.
             * The block size is approximately a multiple of QMEM_PAGESIZE.
             */
            size_t i;
            size_t nitems;
            char* p;
            size_t slotsize;
            size_t allocsize;

            ss_dassert(slotno < QMEM_NSLOT);
            ss_dassert(slot_table[slotno].qst_slotlist == NULL);
            ss_dassert(size <= QMEM_MAXSLOTALLOC);

            slotsize = 1 << slotno;
            if (slotsize > QMEM_PAGESIZE) {
                /* ss_dassert(slotsize <= QMEM_MAXSLOTALLOC); */
                nitems = 1;
            } else {
                nitems = QMEM_PAGESIZE / slotsize;
                if (nitems < QMEM_MINSLOTS) {
                    nitems = QMEM_MINSLOTS;
                }
            }
#ifdef QMEM_PAGEALLOC
            {
                size_t n_ospages;

                n_ospages = (nitems * slotsize + qmem_ospagesize - 1) / qmem_ospagesize;
                if (QMEM_MIN_NPAGES > n_ospages) {
                    n_ospages = QMEM_MIN_NPAGES;
                }
                nitems = (n_ospages * qmem_ospagesize) / slotsize;
            }
#endif

            ss_dassert(size <= slotsize);
            allocsize = nitems * slotsize;

#ifdef QMEM_PAGEALLOC
            p = QMEM_SYSMALLOC(allocsize);
            ss_dassert(((ulong)p & QMEM_CACHEROWALIGNMENT_MINUS_1) == 0);
#else /* QMEM_PAGEALLOC */
            p = QMEM_SYSMALLOC(allocsize + QMEM_CACHEROWALIGNMENT_MINUS_1);
#endif /* QMEM_PAGEALLOC */
            if (p == NULL) {
                QmemOutOfMemory(allocsize + QMEM_CACHEROWALIGNMENT_MINUS_1, (char *)__FILE__, __LINE__);
            }

#ifndef QMEM_PAGEALLOC
            p +=
                (QMEM_CACHEROWALIGNMENT -
                 (size_t)((ulong)p & QMEM_CACHEROWALIGNMENT_MINUS_1))
                & QMEM_CACHEROWALIGNMENT_MINUS_1;
            ss_dassert(((ulong)p & QMEM_CACHEROWALIGNMENT_MINUS_1) == 0);
#endif
            if ((ulong)(ss_qmem_stat.qms_ptrmin - 1) > (ulong)(((ulong)p) - 1)) {
                ss_qmem_stat.qms_ptrmin = (ulong)p;
            }
            if (ss_qmem_stat.qms_ptrmax <= (ulong)p) {
                ss_qmem_stat.qms_ptrmax = (ulong)p;
            }
            ss_qmem_stat.qms_slotbytecount += allocsize;
            ss_qmem_stat.qms_slotptrcount++;
            ss_debug(ss_qmem_nptr++);

            /* Put the items to the list of free slots. */
            p += allocsize - slotsize;
            for (i = 0; i < nitems; i++, p -= slotsize) {
                s = (qmem_slot_t*)p;
                s->s_no = (ss_uint2_t)slotno;
                s->s_nlink = (ss_uint2_t)0;
                QMEM_SLOTTAB_CHECK(&slot_table[slotno]);
                s->s_.next = slot_table[slotno].qst_slotlist;
                QMEM_NEXTPTR_SETCHECK(s);
                slot_table[slotno].qst_slotlist = s;
                QMEM_SLOTTAB_SETCHECK(&slot_table[slotno]);
            }
            /* Take the first item from the free list as the item
               that is returned. */
            QMEM_SLOTTAB_CHECK(&slot_table[slotno]);
            s = slot_table[slotno].qst_slotlist;
            slot_table[slotno].qst_slotlist = s->s_.next;
            QMEM_SLOTTAB_SETCHECK(&slot_table[slotno]);
        }
        /* Return the user pointer of the memory area (after the system
           header). */
        return(QMEM_GETUSERPTR(s));
}

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
        int i;
#ifndef SS_MYSQL
        ss_debug(int size1 = 0;)
        ss_debug(int size2 = 0;)
#endif

#ifdef QMEM_TRACENEXTPTR
        /* does not work if pointers are 64 bits */
        ss_ct_assert(sizeof(void*) == 4);
#endif /* QMEM_TRACENEXTPTR */
        if (SsSemSizeLocal() > 0 && !qmem_sem_initialized) {
#ifdef SS_FLATSEM
            SsFlatMutexInit(&qmem_sem, SS_SEMNUM_SS_QMEM);
#else /* SS_FLATSEM */
            qmem_sem = malloc(SsSemSizeLocal());
            if (qmem_sem == NULL) {
                QmemOutOfMemory(SsSemSizeLocal(), (char *)__FILE__, __LINE__);
            }
            SsMutexInitBuf(qmem_sem, SS_SEMNUM_SS_QMEM);
#endif /* SS_FLATSEM */ 
            qmem_sem_initialized = TRUE;
        }
#ifdef QMEM_PAGEALLOC
        qmem_ospagesize = SsMemPageSize();
#endif
#ifdef SS_QMEM_THREADCTX
        ss_qmem_ctxmaxnptr = 0;
        SS_QMEM_GETSLOTNUM(QMEM_MINSIZE_ORMASK + 1, i);
        for (; i < QMEM_NSLOT; i++) {
            uint n_max;
            if ((1 << i) > (int)QMEM_PAGESIZE) {
                break;
            }
            n_max = qmem_ctx_maxslotptr(i, QMEM_MAXLOCALSPARE);
            ss_qmem_ctxmaxnptr += n_max;
#ifdef SS_DEBUG_XXX
            size1 += (QMEM_PAGESIZE / ((size_t)1 << i)) * (1 << i);
            size2 += n_max * (1 << i);
            printf("%5d %5d %5d %5d %5d %5d %5d\n", i, 1 << i, QMEM_PAGESIZE / ((size_t)1 << i), n_max, ss_qmem_ctxmaxnptr, size1, size2);
#endif
        }
        ss_qmem_ctxmaxnptr = 2 * ss_qmem_ctxmaxnptr;
#endif /* SS_QMEM_THREADCTX */
        QMEM_SLOTTAB_INITCHECKS(system_slot_table);
}

/*#***********************************************************************\
 * 
 *		QmemGlobalAlloc
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
static void* QmemGlobalAlloc(SsMemCtxT* qmem, size_t size)
{
        void* ptr;
        register uint slotno;
        int slotsize;
        ss_autotest(size_t original_size = size;)

        ss_dassert(qmem_sem_initialized || SsSemSizeLocal() == 0);
        /* Calculate the slot number for the memory request size.
         */
        size += QMEM_OVERHEAD;
        ss_dassert(~((size_t)0) > (size_t)0);   /* size_t must not be signed ! */
        ss_debug(
            if (size > 2048 && size <= 4096) {
                ss_dprintf_4(("size = %u\n", (uint)size));
            }
        );
        ss_dassert(size <= (size_t)QMEM_MAXSLOTALLOC);
        size--;
        size |= QMEM_MINSIZE_ORMASK;
        SS_QMEM_GETSLOTNUM(size, slotno);
        ss_dassert(slotno < QMEM_NSLOT);

        slotsize = (1 << slotno);

        /* Allocate from the global context.
         */
        SsFlatMutexLock(qmem_sem);

        ss_qmem_stat.qms_globalctxcount++;

        ss_qmem_stat.qms_slotalloccount[slotno]++;

#if defined(SS_QMEM_THREADCTX)
        if (qmem != NULL) {
            ss_qmem_stat.qms_thrctxbytecount += qmem->qctx_localbytecount;
            ss_qmem_stat.qms_thrctxptrcount += qmem->qctx_localptrcount;
            ss_qmem_stat.qms_thrctxhit += qmem->qctx_localhitcount;

            qmem->qctx_localbytecount = 0;
            qmem->qctx_localptrcount = 0;
            qmem->qctx_localhitcount = 0;
        }
        ss_qmem_stat.qms_thrctxmiss++;
#endif /* defined(SS_QMEM_THREADCTX) */

        if (system_slot_table[slotno].qst_slotlist != NULL) {
            /* There is a free slot, use it.
             */
            ss_dassert(slotno != QMEM_SYSTEMSLOT);
            /* ss_dassert(size <= QMEM_MAXSLOTALLOC); */
            SLOT_ALLOC(system_slot_table, slotno, ptr);
        } else {
            /* No free slot, allocate some.
             */
            QMEM_SLOTTAB_CHECK(&system_slot_table[slotno]);
            ptr = qmem_slot_alloc(system_slot_table, slotno, size);
        }
#if defined(SS_PROFILE)
#if defined(SS_QMEM_THREADCTX) 
        if (qmem != NULL) {
            ulong page = QMEM_PAGEOF(ptr);
            if (page == qmem->qctx_slottable[slotno].qst_prevpage) {
                qmem_npagehit++;
            } else {
                qmem->qctx_slottable[slotno].qst_prevpage = page;
                qmem_npagemiss++;
            }
        }
#else /* SS_QMEM_THREADCTX */
        {
            ulong page = QMEM_PAGEOF(ptr);
            if (page == system_slot_table[slotno].qst_prevpage) {
                qmem_npagehit++;
            } else {
                system_slot_table[slotno].qst_prevpage = page;
                qmem_npagemiss++;
            }
        }
#endif /* SS_QMEM_THREADCTX */
#endif /* SS_PROFILE */
#if defined(SS_QMEM_THREADCTX)
        if (qmem != NULL) {
            /* Try to allocate a few more slots inside the same mutex
             * section to avoid semaphore overhead
             */
            uint n;
            uint n_max;

            n_max = qmem_ctx_maxslotptr(slotno, QMEM_MAXLOCALSPARE / 2);
            for (n = 1; /* start at one because one has been taken already */
                 ;
                 n++)
            {
                qmem_slot_t* p;

                QMEM_SLOTTAB_CHECK(&system_slot_table[slotno]);
                if (system_slot_table[slotno].qst_slotlist == NULL
                ||  n >= n_max)
                {
                    break;
                }
                p = system_slot_table[slotno].qst_slotlist;
                QMEM_NEXTPTR_CHECK(p);
                system_slot_table[slotno].qst_slotlist = p->s_.next;
                QMEM_SLOTTAB_SETCHECK(&system_slot_table[slotno]);
                QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[slotno]);
                p->s_.next = qmem->qctx_slottable[slotno].qst_slotlist;
                QMEM_NEXTPTR_SETCHECK(p);
                qmem->qctx_slottable[slotno].qst_slotlist = p;
                QMEM_SLOTTAB_SETCHECK(&qmem->qctx_slottable[slotno]);
                ss_qmem_stat.qms_thrctxbytecount += slotsize;
                ss_qmem_stat.qms_thrctxptrcount++;
            }
            qmem->qctx_nptr += n - 1;
            if (qmem->qctx_nptr >= (int)QMEM_CTX_MAXNPTR) {
                QmemCtxFlush(qmem, QMEM_MAXLOCALSPARE);
            }
        }
#endif  /* SS_QMEM_THREADCTX */
        SsFlatMutexUnlock(qmem_sem);

        QMEM_THREADCTX_ENDACCESS(qmem);

#ifdef SS_DEBUG
        {
            qmem_slot_t* s = QMEM_GETQMEMPTR(ptr);
            if (s->s_no != slotno || s->s_nlink != 0) {
                char buf[80];
                SsSprintf(buf, "SsQmemAlloc: s->s_no = %d, slotno = %d, s->s_nlink = %d\n", (int)s->s_no, (int)slotno, (int)s->s_nlink);
                SsAssertionMessage(buf, (char *)__FILE__, __LINE__);
            }
            ss_assert(s->s_no == slotno);
            ss_assert(s->s_nlink == 0);
        }
#endif /* SS_DEBUG */

        ss_autotest(if (original_size > 255) memset(ptr, 0xfe, original_size));

        return(ptr);
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
        void* ptr;
        register uint slotno;
        int slotsize;
        size_t original_size = size;
        SsMemCtxT* qmem = NULL;

        ss_dassert(qmem_sem_initialized || SsSemSizeLocal() == 0);
        /* Calculate the slot number for the memory request size.
         */
        size += QMEM_OVERHEAD;
        ss_dassert(~((size_t)0) > (size_t)0);   /* size_t must not be signed ! */
        ss_debug(
            if (size > 2048 && size <= 4096) {
                ss_dprintf_4(("size = %u\n", (uint)size));
            }
        );
        FAKE_CODE_BLOCK_LT(FAKE_SS_MEMALLOCCHECK, size, {ss_error; });

#if defined(SS_QMEM_THREADCTX)
        QMEM_THREADCTX_GET(qmem);
#endif  /* SS_QMEM_THREADCTX */

        if (size > (size_t)QMEM_MAXSLOTALLOC) {
#if defined(SS_QMEM_THREADCTX)
            if (qmem != NULL) {
                long delta;
                delta = (long)(size + QMEM_SYSSLOTOVERHEAD - QMEM_OVERHEAD);
                qmem->qctx_balance = qmem->qctx_balance + delta;
                qmem->qctx_operationbalance = qmem->qctx_operationbalance + delta;
                if (qmem->qctx_operationbalance > qmem->qctx_operationpeak) {
                    qmem->qctx_operationpeak = qmem->qctx_operationbalance;
                }
            }
#endif  /* SS_QMEM_THREADCTX */
            SsFlatMutexLock(qmem_sem);
            ptr = qmem_slot_alloc(system_slot_table, QMEM_SYSTEMSLOT, size - QMEM_OVERHEAD);
            ss_qmem_stat.qms_slotalloccount[QMEM_SYSTEMSLOT]++;
            SsFlatMutexUnlock(qmem_sem);

            ss_debug({
                qmem_slot_t* s = QMEM_GETQMEMPTR(ptr);
                ss_assert(s->s_no == QMEM_SYSTEMSLOT);
            });
            ss_autotest(if (original_size > 255) memset(ptr, 0xfe, original_size));
            return (ptr);
        }
        size--;
        size |= QMEM_MINSIZE_ORMASK;
        SS_QMEM_GETSLOTNUM(size, slotno);
        ss_dassert(slotno < QMEM_NSLOT);

        slotsize = (1 << slotno);

#if defined(SS_QMEM_THREADCTX)
        if (qmem != NULL) {
            /* Try to allocate from the thread context.
             */
            QMEM_THREADCTX_BEGINACCESS(qmem);
#if defined(SS_QMEM_THREADCTX)
            qmem->qctx_balance = qmem->qctx_balance + (long)slotsize;
            qmem->qctx_operationbalance = qmem->qctx_operationbalance + (long)slotsize;
            if (qmem->qctx_operationbalance > qmem->qctx_operationpeak) {
                qmem->qctx_operationpeak = qmem->qctx_operationbalance;
            }
#endif  /* SS_QMEM_THREADCTX */
            QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[slotno]);
            if (qmem->qctx_slottable[slotno].qst_slotlist != NULL) {
                /* There is a free slot in this context, take it.
                 */
                ss_dassert(slotno != QMEM_SYSTEMSLOT);
                SLOT_ALLOC(qmem->qctx_slottable, slotno, ptr);
                qmem->qctx_nptr--;
                qmem->qctx_localbytecount -= slotsize;
                qmem->qctx_localptrcount--;
                qmem->qctx_localhitcount++;
                qmem->qctx_slotalloccount[slotno]++;
                QMEM_THREADCTX_ENDACCESS(qmem);
                ss_profile(
                    {
                        ulong page = QMEM_PAGEOF(ptr);
                        if (page == qmem->qctx_slottable[slotno].qst_prevpage) {
                            qmem_npagehit++;
                        } else {
                            qmem->qctx_slottable[slotno].qst_prevpage = page;
                            qmem_npagemiss++;
                        }
                    }
                );
                ss_qmem_stat.qms_thrctxcount++;
                ss_autotest(if (original_size > 255) memset(ptr, 0xfe, original_size));
                return(ptr);
            }
        } else {
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            /* Do not allocate thread local context here. */
#else
            QMEM_THREADCTX_INIT();
#endif
        }
#endif  /* SS_QMEM_THREADCTX */

        /* Allocate from the global context.
         */
        return(QmemGlobalAlloc(qmem, original_size));
}

/*##**********************************************************************\
 * 
 *		SsQmemCtxAlloc
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
void* SsQmemCtxAlloc(SsMemCtxT* qmem, size_t size)
{
        void* ptr;
        register uint slotno;
        int slotsize;
        size_t original_size = size;
#if defined(SS_QMEM_THREADCTX) && defined(SS_DEBUG)
        SsMemCtxT* thread_qmem;
#endif
        ss_dassert(qmem_sem_initialized || SsSemSizeLocal() == 0);

        if (qmem == NULL) {
            return(SsQmemAlloc(original_size));
        }

        ss_dassert(qmem != NULL);

        /* Calculate the slot number for the memory request size.
         */
        size += QMEM_OVERHEAD;
        ss_dassert(~((size_t)0) > (size_t)0);   /* size_t must not be signed ! */
        ss_debug(
            if (size > 2048 && size <= 4096) {
                ss_dprintf_4(("size = %u\n", (uint)size));
            }
        );
        FAKE_CODE_BLOCK_LT(FAKE_SS_MEMALLOCCHECK, size, {ss_error; });

#if defined(SS_QMEM_THREADCTX) && defined(SS_DEBUG)
        QMEM_THREADCTX_GET(thread_qmem);
        ss_dassert(thread_qmem == qmem);
#endif  /* SS_QMEM_THREADCTX */

        if (size > (size_t)QMEM_MAXSLOTALLOC) {
            long delta;
            delta = (long)(size + QMEM_SYSSLOTOVERHEAD - QMEM_OVERHEAD);
            qmem->qctx_balance = qmem->qctx_balance + delta;
            qmem->qctx_operationbalance = qmem->qctx_operationbalance + delta;
            if (qmem->qctx_operationbalance > qmem->qctx_operationpeak) {
                qmem->qctx_operationpeak = qmem->qctx_operationbalance;
            }
            SsFlatMutexLock(qmem_sem);
            ptr = qmem_slot_alloc(system_slot_table, QMEM_SYSTEMSLOT, size - QMEM_OVERHEAD);
            ss_qmem_stat.qms_slotalloccount[QMEM_SYSTEMSLOT]++;
            SsFlatMutexUnlock(qmem_sem);

            ss_debug({
                qmem_slot_t* s = QMEM_GETQMEMPTR(ptr);
                ss_assert(s->s_no == QMEM_SYSTEMSLOT);
            });
            ss_autotest(if (original_size > 255) memset(ptr, 0xfe, original_size));
            return (ptr);
        }
        size--;
        size |= QMEM_MINSIZE_ORMASK;
        SS_QMEM_GETSLOTNUM(size, slotno);
        ss_dassert(slotno < QMEM_NSLOT);

        slotsize = (1 << slotno);

        /* Try to allocate from local context.
         */
        QMEM_THREADCTX_BEGINACCESS(qmem);
        qmem->qctx_balance = qmem->qctx_balance + (long)slotsize;
        qmem->qctx_operationbalance = qmem->qctx_operationbalance + (long)slotsize;
        if (qmem->qctx_operationbalance > qmem->qctx_operationpeak) {
            qmem->qctx_operationpeak = qmem->qctx_operationbalance;
        }
        QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[slotno]);
        if (qmem->qctx_slottable[slotno].qst_slotlist != NULL) {
            /* There is a free slot in this context, take it.
                */
            ss_dassert(slotno != QMEM_SYSTEMSLOT);
            SLOT_ALLOC(qmem->qctx_slottable, slotno, ptr);
            qmem->qctx_nptr--;
            qmem->qctx_localbytecount -= slotsize;
            qmem->qctx_localptrcount--;
            qmem->qctx_localhitcount++;
            qmem->qctx_slotalloccount[slotno]++;
            QMEM_THREADCTX_ENDACCESS(qmem);
            ss_profile(
                {
                    ulong page = QMEM_PAGEOF(ptr);
                    if (page == qmem->qctx_slottable[slotno].qst_prevpage) {
                        qmem_npagehit++;
                    } else {
                        qmem->qctx_slottable[slotno].qst_prevpage = page;
                        qmem_npagemiss++;
                    }
                }
            );
            ss_autotest(if (original_size > 255) memset(ptr, 0xfe, original_size));
            ss_qmem_stat.qms_userctxcount++;
            return(ptr);
        }

        /* Allocate from the global context.
         */
        return(QmemGlobalAlloc(qmem, original_size));
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
#ifdef SS_PMEM
        p = (*SsQmemAllocPtr)(size);
#else
        p = SsQmemAlloc(size);
#endif
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
#ifdef SS_PMEM
        p = (*SsQmemAllocPtr)(size);
#else
        p = SsQmemAlloc(size);
#endif
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
        void* newptr;
        size_t oldslotno;

        if (oldptr == NULL) {
            newptr = SsQmemAlloc(newsize);
            return (newptr);
        }
        oldslotno = QMEM_GETSLOTNO(oldptr);

        if (oldslotno == QMEM_SYSTEMSLOT) {

            /* Old pointer was allocated using system malloc. */
            if (newsize > QMEM_MAXSLOTALLOC - QMEM_OVERHEAD) {
                /* Allocate also the new pointer from system. */
                size_t* psz;
                size_t oldgrosssize;
#ifdef SS_QMEM_THREADCTX
                SsMemCtxT* qmem;
#endif

                /* Area size is stored to the beginning of allocated memory. */
                oldgrosssize = QMEM_SYSSLOTGETSIZE(oldptr) + 
                    QMEM_SYSSLOTOVERHEAD;
                psz = (size_t*)((char*)oldptr - QMEM_SYSSLOTOVERHEAD);
#if defined(SS_QMEM_THREADCTX)
                QMEM_THREADCTX_GET(qmem);
                if (qmem != NULL) {
                    long delta;
                    delta = newsize + QMEM_SYSSLOTOVERHEAD - oldgrosssize;
                    qmem->qctx_balance = qmem->qctx_balance + delta;
                    qmem->qctx_operationbalance = qmem->qctx_operationbalance + delta;
                    if (qmem->qctx_operationbalance > qmem->qctx_operationpeak) {
                        qmem->qctx_operationpeak = qmem->qctx_operationbalance;
                    }
                }
#endif  /* SS_QMEM_THREADCTX */
        
                SsFlatMutexLock(qmem_sem);
                ss_qmem_stat.qms_sysbytecount -= oldgrosssize;

#if defined(SSSYSRES_USED)
                SsSysResRemoveByData(oldptr-8);
#endif /* defined(SSSYSRES_USED) */
                psz = QMEM_SYSREALLOC(psz, newsize + QMEM_SYSSLOTOVERHEAD, oldgrosssize);
		
                if (psz == NULL) {
                    SSQMEM_EXIT_SYSRES;
                    QmemOutOfMemory(newsize + QMEM_SYSSLOTOVERHEAD, (char *)__FILE__, __LINE__);
                }
#if defined(SSSYSRES_USED)
		SsSysResAdd(SsQmemFreeFun, psz);
#endif /* defined(SSSYSRES_USED) */
		
                if ((ulong)(ss_qmem_stat.qms_ptrmin - 1) > (ulong)(((ulong)psz) - 1)) {
                    ss_qmem_stat.qms_ptrmin = (ulong)psz;
                }
                if (ss_qmem_stat.qms_ptrmax <= (ulong)psz) {
                    ss_qmem_stat.qms_ptrmax = (ulong)psz;
                }
                /* Store area size to the beginning of allocated memory. */
                QMEM_SYSSLOTSETSIZEANDNO(psz, newsize, QMEM_SYSTEMSLOT);
                ss_qmem_stat.qms_sysbytecount += newsize + QMEM_SYSSLOTOVERHEAD;
                ss_dassert(QMEM_GETSLOTNO((void*)((char*)psz + QMEM_SYSSLOTOVERHEAD))
                    == QMEM_SYSTEMSLOT);
                SsFlatMutexUnlock(qmem_sem);
                return ((void*)((char*)psz + QMEM_SYSSLOTOVERHEAD));
            } else {
                /* Allocate new pointer using quick malloc.
                   The new size is smaller than the old size. */
                newptr = SsQmemAlloc(newsize);
                memcpy(newptr, oldptr, newsize);
                /* Quick free actually calls system free. */
                SsQmemFree(oldptr); 
            }
            return(newptr);

        } else {

            /* Old pointer was allocated using quick malloc. */
            size_t oldslotsize;
            size_t oldslotgrosssize;

            ss_dassert(oldslotno < QMEM_NSLOT);

            oldslotgrosssize = (1 << oldslotno);
            oldslotsize =  oldslotgrosssize - QMEM_OVERHEAD;

            if (newsize <= oldslotsize
            &&  (newsize > oldslotgrosssize / 4 - QMEM_OVERHEAD
             ||  oldslotgrosssize == QMEM_MINSIZE_ORMASK + 1))
            {
                /* Use the same memory area. */
                return(oldptr);
            } else {
                /* Allocate new area. The new area may be smaller or
                   larger than the old area. */
                newptr = SsQmemAlloc(newsize);
                memcpy(newptr, oldptr, SS_MIN(newsize, oldslotsize));
                SsQmemFree(oldptr);
                return(newptr);
            }
        }
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
        size_t slotno;
#ifdef SS_QMEM_THREADCTX
        SsMemCtxT* qmem;
#endif

        s = QMEM_GETQMEMPTR(ptr);

        slotno = s->s_no;
        ss_dassert(slotno > 1);
        ss_dassert(slotno <= QMEM_NSLOT);
        ss_dassert(s->s_nlink == 0);
#if defined(SS_QMEM_THREADCTX)
        QMEM_THREADCTX_GET(qmem);
#endif  /* SS_QMEM_THREADCTX */

        if (slotno == QMEM_SYSTEMSLOT) {
            /* Pointer was allocated using system malloc.
             */
            size_t* psz;
            size_t grosssize;
            /* Area size is stored to the beginning of allocated memory. */
            grosssize = QMEM_SYSSLOTGETSIZE(ptr) + QMEM_SYSSLOTOVERHEAD;
            psz = (size_t*)((char*)ptr - QMEM_SYSSLOTOVERHEAD);

#if defined(SS_QMEM_THREADCTX)
            if (qmem != NULL) {
                qmem->qctx_balance = qmem->qctx_balance - (long)grosssize;
                qmem->qctx_operationbalance = qmem->qctx_operationbalance - (long)grosssize;
            }
#endif  /* SS_QMEM_THREADCTX */
            
            SsFlatMutexLock(qmem_sem);
            ss_qmem_stat.qms_sysbytecount -= grosssize; 
            ss_qmem_stat.qms_sysptrcount--;
            ss_debug(ss_qmem_nsysptr--);
#if defined(SSSYSRES_USED)
            SsSysResRemoveByData(ptr-8);
#endif /* defined(SSSYSRES_USED) */
            SsFlatMutexUnlock(qmem_sem);
            QMEM_SYSFREE(psz, grosssize);
        } else {
            /* Pointer was allocated using quick malloc. Put the slot
             * to the head of the free list.
             */
            ss_dassert(slotno < QMEM_NSLOT);


#if defined(SS_QMEM_THREADCTX)
            if (qmem != NULL) {
                /* Free the pointer to the thread context.
                 */
                int slotsize;

                QMEM_THREADCTX_BEGINACCESS(qmem);
                QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[slotno]);
                s->s_.next = qmem->qctx_slottable[slotno].qst_slotlist;
                QMEM_NEXTPTR_SETCHECK(s);
                qmem->qctx_slottable[slotno].qst_slotlist = s;
                QMEM_SLOTTAB_SETCHECK(&qmem->qctx_slottable[slotno]);
                slotsize = (1 << slotno);
                qmem->qctx_balance = qmem->qctx_balance - (long)slotsize;
                qmem->qctx_operationbalance = qmem->qctx_operationbalance - (long)slotsize;
                qmem->qctx_localbytecount += slotsize;
                qmem->qctx_localptrcount++;
                if (qmem->qctx_nptr++ >= (int)QMEM_CTX_MAXNPTR
                    || qmem->qctx_nflushreq++ >= (int)QMEM_CTX_FLUSHFREQ) 
                {
                    SsFlatMutexLock(qmem_sem);
                    
                    ss_qmem_stat.qms_thrctxbytecount += qmem->qctx_localbytecount;
                    ss_qmem_stat.qms_thrctxptrcount += qmem->qctx_localptrcount;
                    ss_qmem_stat.qms_thrctxhit += qmem->qctx_localhitcount;

                    qmem->qctx_localbytecount = 0;
                    qmem->qctx_localptrcount = 0;
                    qmem->qctx_localhitcount = 0;

                    QmemCtxFlush(qmem, QMEM_MAXLOCALSPARE);
                    
                    SsFlatMutexUnlock(qmem_sem);
                }
            } else
#endif
            {
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            /* Do not allocate thread local context here. */
#else
                QMEM_THREADCTX_INIT();
#endif

                /* Free the pointer to the global context.
                 */
                SsFlatMutexLock(qmem_sem);
                QMEM_SLOTTAB_CHECK(&system_slot_table[slotno]);
                s->s_.next = system_slot_table[slotno].qst_slotlist;
                QMEM_NEXTPTR_SETCHECK(s);
                system_slot_table[slotno].qst_slotlist = s;
                QMEM_SLOTTAB_SETCHECK(&system_slot_table[slotno]);
                SsFlatMutexUnlock(qmem_sem);
            }
            QMEM_THREADCTX_ENDACCESS(qmem);
        }
}

/*##**********************************************************************\
 * 
 *		SsQmemCtxFree
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
void SsQmemCtxFree(SsMemCtxT* qmem, void* ptr)
{
        qmem_slot_t* s;
        size_t slotno;
        int slotsize;
#if defined(SS_QMEM_THREADCTX) && defined(SS_DEBUG)
        SsMemCtxT* thread_qmem;
#endif

        if (qmem == NULL) {
            SsQmemFree(ptr);
            return;
        }

        s = QMEM_GETQMEMPTR(ptr);

        slotno = s->s_no;
        ss_dassert(slotno <= QMEM_NSLOT);
        ss_dassert(s->s_nlink == 0);
#if defined(SS_QMEM_THREADCTX) && defined(SS_DEBUG)
        QMEM_THREADCTX_GET(thread_qmem);
        ss_dassert(thread_qmem == qmem)
#endif  /* SS_QMEM_THREADCTX */

        if (slotno == QMEM_SYSTEMSLOT) {
            /* Pointer was allocated using system malloc.
             */
            size_t* psz;
            size_t grosssize;
            /* Area size is stored to the beginning of allocated memory. */
            grosssize = QMEM_SYSSLOTGETSIZE(ptr) + QMEM_SYSSLOTOVERHEAD;
            psz = (size_t*)((char*)ptr - QMEM_SYSSLOTOVERHEAD);

            qmem->qctx_balance = qmem->qctx_balance - (long)grosssize;
            qmem->qctx_operationbalance = qmem->qctx_operationbalance - (long)grosssize;
            
            SsFlatMutexLock(qmem_sem);
            ss_qmem_stat.qms_sysbytecount -= grosssize; 
            ss_qmem_stat.qms_sysptrcount--;
            ss_debug(ss_qmem_nsysptr--);
#if defined(SSSYSRES_USED)
            SsSysResRemoveByData(ptr-8);
#endif /* defined(SSSYSRES_USED) */
            SsFlatMutexUnlock(qmem_sem);
            QMEM_SYSFREE(psz, grosssize);
        } else {
            /* Pointer was allocated using quick malloc. Put the slot
             * to the head of the free list.
             */
            ss_dassert(slotno < QMEM_NSLOT);

            /* Free the pointer to the thread context.
             */
            QMEM_THREADCTX_BEGINACCESS(qmem);
            QMEM_SLOTTAB_CHECK(&qmem->qctx_slottable[slotno]);
            s->s_.next = qmem->qctx_slottable[slotno].qst_slotlist;
            QMEM_NEXTPTR_SETCHECK(s);
            qmem->qctx_slottable[slotno].qst_slotlist = s;
            QMEM_SLOTTAB_SETCHECK(&qmem->qctx_slottable[slotno]);
            slotsize = (1 << slotno);
            qmem->qctx_balance = qmem->qctx_balance - (long)slotsize;
            qmem->qctx_operationbalance = qmem->qctx_operationbalance - (long)slotsize;
            qmem->qctx_localbytecount += slotsize;
            qmem->qctx_localptrcount++;
            if (qmem->qctx_nptr++ >= (int)QMEM_CTX_MAXNPTR
                || qmem->qctx_nflushreq++ >= (int)QMEM_CTX_FLUSHFREQ) 
            {
                SsFlatMutexLock(qmem_sem);
                
                ss_qmem_stat.qms_thrctxbytecount += qmem->qctx_localbytecount;
                ss_qmem_stat.qms_thrctxptrcount += qmem->qctx_localptrcount;
                ss_qmem_stat.qms_thrctxhit += qmem->qctx_localhitcount;

                qmem->qctx_localbytecount = 0;
                qmem->qctx_localptrcount = 0;
                qmem->qctx_localhitcount = 0;

                QmemCtxFlush(qmem, QMEM_MAXLOCALSPARE);
                
                SsFlatMutexUnlock(qmem_sem);
            }
            QMEM_THREADCTX_ENDACCESS(qmem);
        }
}

/*#***********************************************************************\
 * 
 *		QmemPrintRangeInfo
 * 
 * 
 * 
 * Parameters : 
 * 
 *	first - 
 *		
 *		
 *	last - 
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
static void QmemPrintRangeInfo(void* fp, int first, int last)
{
        int i;
        int cnt;
        qmem_slot_t* s;
        qmem_slottabentry_t* slot_table;

        slot_table = system_slot_table;

        if (last > QMEM_NSLOT) {
            last = QMEM_NSLOT;
        }

        for (i = first; i < last; i++) {
            if ((1 << i) > (int)QMEM_MAXSLOTALLOC) {
                break;
            }
            SsFprintf(fp, "%4u ", 1 << i);
        }
        SsFprintf(fp, "\n");
        for (i = first; i < last; i++) {
            if ((1 << i) > (int)QMEM_MAXSLOTALLOC) {
                break;
            }
            cnt = 0;
            if (slot_table[i].qst_slotlist != NULL) {
                s = slot_table[i].qst_slotlist;
                /* count the slots. */
                while (s != NULL) {
                    s = s->s_.next;
                    cnt++;
                }
            }
            SsFprintf(fp, "%4u ", cnt);
        }
        SsFprintf(fp, "\n");
}

/*##**********************************************************************\
 * 
 *		SsQmemFPrintInfo
 * 
 * 
 * 
 * Parameters :
 * 
 *      fp - use
 *          File pointer, or NULL.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsQmemFPrintInfo(void* fp)
{
        int i;

        SsFlatMutexLock(qmem_sem);

        SsFprintf(fp, "Qmem slots:\n");
        for (i = 0; i < QMEM_NSLOT; i += 16) {
            QmemPrintRangeInfo(fp, i, i + 16);
        }

        SsFlatMutexUnlock(qmem_sem);
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
        SsQmemFPrintInfo(NULL);
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
        SsFlatMutexLock(qmem_sem);

        *p_qmi = ss_qmem_stat;

        SsFlatMutexUnlock(qmem_sem);
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
        ss_dassert(p != NULL);
        return (QMEM_GETSLOTNO(p));
}

/*##**********************************************************************\
 * 
 *		SsQmemGetDataSize
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
size_t SsQmemGetDataSize(void* p)
{
        size_t slotno;

        ss_dassert(p != NULL);

        slotno = QMEM_GETSLOTNO(p);
        if (slotno == QMEM_SYSTEMSLOT) {
            return (QMEM_SYSSLOTGETSIZE(p));
        } else {
            return ((1 << slotno) - QMEM_OVERHEAD);
        }
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
        int i;
        qmem_slot_t* s;
#if defined(SS_QMEM_THREADCTX)
        SsMemCtxT* qmem;
#endif

        SsFlatMutexLock(qmem_sem);

#if defined(SS_QMEM_THREADCTX)
        QMEM_THREADCTX_GET(qmem);
        if (qmem != NULL) {
            QMEM_THREADCTX_BEGINACCESS(qmem);
            for (i = 0; i < QMEM_NSLOT; i++) {
                s = qmem->qctx_slottable[i].qst_slotlist;
                if (fastp) {
                    if (s != NULL) {
                        ss_assert(s->s_no == i);
                    }
                } else {
                    while (s != NULL) {
                        ss_assert(s->s_no == i);
                        s = s->s_.next;
                    }
                }
            }
            QMEM_THREADCTX_ENDACCESS(qmem);
        }
#endif  /* SS_QMEM_THREADCTX */

        for (i = 0; i < QMEM_NSLOT; i++) {
            s = system_slot_table[i].qst_slotlist;
            if (fastp) {
                if (s != NULL) {
                    ss_assert(s->s_no == i);
                }
            } else {
                while (s != NULL) {
                    ss_assert(s->s_no == i);
                    s = s->s_.next;
                }
            }
        }

        SsFlatMutexUnlock(qmem_sem);

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

#ifndef SS_DEBUG
        if (nlink == 0) {
#ifdef SS_PMEM
            (*SsQmemFreePtr)(p);
#else /* SS_PMEM */
            SsQmemFree(p);
#endif /* SS_PMEM */
        }
#endif /* SS_DEBUG */

        return(nlink);
}

uint SsQmemLinkIncSafe(void* p)
{
        uint nlink;

        SsFlatMutexLock(qmem_sem);

        LINK_INC(p, nlink);

        SsFlatMutexUnlock(qmem_sem);

        return(nlink);
}

uint SsQmemLinkDecSafe(void* p)
{
        uint nlink;

        SsFlatMutexLock(qmem_sem);

        LINK_DEC(p, nlink);

        SsFlatMutexUnlock(qmem_sem);

        return(nlink);
}

long SsQmemGetBalance(void)
{
#if defined(SS_QMEM_THREADCTX)
        SsMemCtxT* qmem;

        QMEM_THREADCTX_GET(qmem);
        if (qmem != NULL) {
            return (qmem->qctx_balance);
        }
#endif  /* SS_QMEM_THREADCTX */
        return (0L);
}

void SsQmemSetOperationBalance(long balance, long peak)
{
#if defined(SS_QMEM_THREADCTX)
        SsMemCtxT* qmem;

        QMEM_THREADCTX_GET(qmem);
        if (qmem != NULL) {
            ss_dassert(peak >= 0);
            if (qmem->qctx_operationstacklevel++ == 0) {
                qmem->qctx_operationbalance = balance;
                qmem->qctx_operationpeak = peak;
            }
        }
#endif  /* SS_QMEM_THREADCTX */
}
        
void SsQmemGetOperationBalance(long* p_balance, long* p_peak)
{
#if defined(SS_QMEM_THREADCTX)
        SsMemCtxT* qmem;

        QMEM_THREADCTX_GET(qmem);
        if (qmem != NULL) {
            ss_dassert(qmem->qctx_operationstacklevel > 0);
            qmem->qctx_operationstacklevel--;
            *p_balance = qmem->qctx_operationbalance;
            *p_peak = qmem->qctx_operationpeak;
        }
#else  /* SS_QMEM_THREADCTX */
        *p_balance = 0;
        *p_peak = 0;
#endif  /* SS_QMEM_THREADCTX */
}

#endif /* !defined(SS_PURIFY) */

