/*************************************************************************\
**  source       * ssmem.h
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


#ifndef SSMEM_H
#define SSMEM_H

#include "ssenv.h"
#include "ssstddef.h"
#include "ssstdlib.h"
#include "ssstdio.h"
#include "ssc.h"
#include "sssem.h"
#include "sssqltrc.h"
#include "ssqmem.h"


void SsMemGlobalInit(void);

void* SsMemAlloc(size_t size);
void* SsMemCalloc(unsigned nelem, size_t elsize);
void* SsMemRealloc(void *oldptr, size_t size);
void  SsMemFree(void *ptr);
void* SsMemFreeIfNotNULL(void* ptr);
char* SsMemStrdup(char* str);
void* SsMemAlloca(size_t size);

void SsMemLinkInit(void* p);
uint SsMemLinkGet(void* p);
uint SsMemLinkInc(void* p);
uint SsMemLinkDec(void* p);
uint SsMemLinkDecZeroFree(void* p);
uint SsMemLinkIncSafe(void* p);
uint SsMemLinkDecSafe(void* p);

#ifndef SS_DEBUG

/* Direct calls to SsQmem on no-debug version. */

#define SsMemAlloc(size)            SsQmemAlloc(size)
#define SsMemCtxAlloc(c, size)      SsQmemCtxAlloc(c, size)
#define SsMemCalloc(nelem, elsize)  SsQmemCalloc(nelem, elsize)
#define SsMemRealloc(oldptr, size)  SsQmemRealloc(oldptr, size)
#define SsMemFree(ptr)              SsQmemFree(ptr)
#define SsMemCtxFree(c, ptr)        SsQmemCtxFree(c, ptr)
#define SsMemStrdup(str)            SsQmemStrdup(str)

#define SsMemLinkInit               SsQmemLinkInit
#define SsMemLinkGet                SsQmemLinkGet
#define SsMemLinkInc                SsQmemLinkInc
#define SsMemLinkDec                SsQmemLinkDec
#define SsMemLinkDecZeroFree        SsQmemLinkDecZeroFree
#define SsMemLinkIncSafe            SsQmemLinkIncSafe
#define SsMemLinkDecSafe            SsQmemLinkDecSafe

#endif /* not SS_DEBUG */

#if defined(SS_DOSX)

/* #define VERSION_1 */
/* #define VERSION_2 */
#define VERSION_3

#if defined(VERSION_1) || defined(VERSION_2)
void far* SsMemAllocSeg(size_t size);
void      SsMemFreeSeg(void far* ptr);
#elif defined(VERSION_3)

void* SsMemAllocSeg(size_t size);
void  SsMemFreeSeg(void* ptr);
void* SsMemGetRealModePointer(void* ptr);
int   allocDOSBuffer(unsigned int size, int *segment, int *selector);
int   freeDOSMemory(int selector);
void  freeAllDOSMemory(void);

#define RMFP_SEG(__RMPointer)  \
                    ((uint)((ulong)(void *)(__RMPointer) >> 16))
#define RMFP_OFF(__RMPointer)  \
                    ((uint)((ulong)(void *)(__RMPointer) & 0x0000FFFF))

#endif /* VERSION */

#elif defined (SS_WIN) || defined (SS_DOS) 

void* SsMemAllocSeg(size_t size);
void  SsMemFreeSeg(void* ptr);

#else /* DOSX */

#define SsMemAllocSeg     SsMemAlloc
#define SsMemFreeSeg      SsMemFree

#endif /* DOSX */

#if defined(SS_WIN) || defined(SS_NT)

void* SsMemAllocShared(char* name, size_t size);
void  SsMemFreeShared(void* ptr);

void* SsMemLinkShared(char* name);
void  SsMemUnlinkShared(void* ptr);

#endif /* WIN */

#define SSMEM_NEW(type)    (type *)SsMemAlloc(sizeof(type))

/* 2002-08-14 memory allocation size limit for debug compilation removed
   by APL: it prevented su/test/ttrie from working and was generally
   irritating. */
#if 0 && (defined(SS_DEBUG) || defined(SS_PURIFY))
# define SS_MEM_CHKALLOCSIZE(s) ss_assert((ulong)(s) < (10L * 1024L * 1024L))
#else
# define SS_MEM_CHKALLOCSIZE(s)
#endif

/************************************************************************/
/* MEMORY DEBUGGING CODE BELOW                                          */
/************************************************************************/

#ifdef SS_DEBUG

#ifndef SSMEM_TRACE
#define SSMEM_TRACE
#endif

#define SSMEM_MAXLINKINFO   100

typedef struct {
        char*   ml_file;
        int     ml_line;
        bool    ml_linkp;
        int     ml_count;
} SsMemChkLinkInfoT;

typedef struct SsMemChkListStruct       SsMemChkListT;

struct SsMemChkListStruct {
        void*              memlst_ptr;      /* memory area pointer, or NULL if
                                               this node not used */
        size_t             memlst_size;     /* memory area size */
        char*              memlst_file;     /* file name */
        int                memlst_line;     /* line number*/
        long               memlst_counter;  /* allocation number */
        SsMemChkListT*     memlst_next;     /* next node, or NULL at last node */
        char**             memlst_callstk;  /* for SSMEM_TRACE option */
        size_t             memlst_qmemslotno;
        SsSQLTrcInfoT*     memlst_sqltrcinfo;
        SsMemChkLinkInfoT* memlst_linkinfo;
        void*              memlst_allocinfo;
};

extern ulong memchk_nptr;

typedef enum {
        SS_MEMOBJ_UNKNOWN,
        SS_MEMOBJ_GENERIC,
        SS_MEMOBJ_ATYPE,
        SS_MEMOBJ_AVAL,
        SS_MEMOBJ_TTYPE,
        SS_MEMOBJ_TVAL,
        SS_MEMOBJ_LIST,
        SS_MEMOBJ_LISTNODE,
        SS_MEMOBJ_SLIST,
        SS_MEMOBJ_SLISTTABLE,
        SS_MEMOBJ_ABLIST,
        SS_MEMOBJ_ABLISTTABLE,        
        SS_MEMOBJ_RBT,
        SS_MEMOBJ_RBTNODE,
        SS_MEMOBJ_MMERBT,
        SS_MEMOBJ_TRXINFO,
        SS_MEMOBJ_TRXBUFSTMT,
        SS_MEMOBJ_TRXWCHK,
        SS_MEMOBJ_TRXRCHK,
        SS_MEMOBJ_TRXKCHK,
        SS_MEMOBJ_BKEY,
        SS_MEMOBJ_MMNODEINFO,
        SS_MEMOBJ_MMLOCK,
        SS_MEMOBJ_MMNODE,
        SS_MEMOBJ_MME_LOCK,
        SS_MEMOBJ_MME_ROW,
        SS_MEMOBJ_MME_OPERATION,
        SS_MEMOBJ_HSB_QUEUE_LOGDATA,
        SS_MEMOBJ_HSB_QUEUE_CATCHUP_LOGDATA,
        SS_MEMOBJ_HSB_QUEUE_ACKNOWLEDGE,
        SS_MEMOBJ_HSB_FLUSHER_QUEUE,
        SS_MEMOBJ_RPC_RSES,
        SS_MEMOBJ_SQL,
        SS_MEMOBJ_DBECACHE,
        SS_MEMOBJ_CACMEM,
        SS_MEMOBJ_FFMEM,
        SS_MEMOBJ_SNC_MSGHOLDER,
        SS_MEMOBJ_SNC_LOCK,
        SS_MEMOBJ_LASTFIXEDID
} ss_memobjtype_t;

void* SsMemChkAlloc(size_t size, char *file, int line, ss_memobjtype_t* p_mo);
void* SsMemChkCtxAlloc(SsMemCtxT* qmem, size_t size, char *file, int line, ss_memobjtype_t* p_mo);
void* SsMemChkCalloc(unsigned nelem, size_t elsize, char *file, int line, ss_memobjtype_t* p_mo);
char* SsMemChkStrdup(char* str, char *file, int line, ss_memobjtype_t* p_mo);
void* SsMemChkRealloc(void* oldptr, size_t size, char *file, int line, ss_memobjtype_t* p_mo);
void* SsMemChkReallocEx(void** p_oldptr, size_t size, char *file, int line, ss_memobjtype_t* p_mo);
void  SsMemChkFree(void* ptr, char *file, int line);
void  SsMemChkCtxFree(SsMemCtxT* qmem, void* ptr, char *file, int line);
void  SsMemChkFreeEx(void** p_ptr, char *file, int line);
void  SsMemChkCtxFreeEx(SsMemCtxT* qmem, void** p_ptr, char *file, int line);
void  SsMemChkSetLinkInfo(void* ptr, char* file, int line, bool linkp);

/* memory list routines */
SsMemChkListT* SsMemChkGetListFirst(void);
SsMemChkListT* SsMemChkGetListNext(void);
long           SsMemChkGetCounter(void);
char*          SsMemChkCheckListItem(SsMemChkListT* item, bool qmem_too);

/* memory information output routines */
void SsMemChkPrintInfo(void);
void SsMemChkFPrintInfo(void* fp);
void SsMemChkPrintListByCounter(long mincounter);
void SsMemChkPrintListByMinSize(long minsize);
void SsMemChkPrintListByAllocatedPtrs(long thisormore);
void SsMemChkPrintList(void);
void SsMemChkPrintNewList(void);
void SsMemChkListCheck(char* file, int line);

void SsMemChkAllocInfoPrint(void* fp, int size, char* prefix);

SsMemChkListT* SsMemChkListFindTest(
	void* ptr,
	char* file,
	int line);

void SsMemFreedPoolCheck(void);

/* Memory size query function. */
uint SsMemSize(void* ptr);

/* Register keyword must be empty, because SsMemFree takes address
   of a pointer.
*/
#define register

static ss_memobjtype_t ss_local_memobjtype __attribute__ ((unused)) = SS_MEMOBJ_UNKNOWN;

#define SsMemAlloc(s)           SsMemChkAlloc(s,(char *)__FILE__, __LINE__, &ss_local_memobjtype)
#define SsMemCtxAlloc(c, s)     SsMemChkCtxAlloc(c,s,(char *)__FILE__, __LINE__, &ss_local_memobjtype)
#define SsMemCalloc(s, e)       SsMemChkCalloc(s, e, (char *)__FILE__, __LINE__, &ss_local_memobjtype)
#define SsMemStrdup(s)          SsMemChkStrdup(s, (char *)__FILE__, __LINE__, &ss_local_memobjtype)
#if defined(SS_NT) && !defined(SQL_ALLOCOP)
#define SsMemRealloc(p, s)      SsMemChkReallocEx((void**)&(p), s, (char *)__FILE__, __LINE__, &ss_local_memobjtype)
#define SsMemFree(p)            SsMemChkFreeEx((void**)&(p), (char *)__FILE__, __LINE__)
#define SsMemCtxFree(c,p)       SsMemChkCtxFreeEx(c, (void**)&(p), (char *)__FILE__, __LINE__)
#else
#define SsMemRealloc(p, s)      SsMemChkRealloc(p, s, (char *)__FILE__, __LINE__, &ss_local_memobjtype)
#define SsMemFree(p)            SsMemChkFree(p, (char *)__FILE__, __LINE__)
#define SsMemCtxFree(c,p)       SsMemChkCtxFree(c, p, (char *)__FILE__, __LINE__)
#endif

extern SsSemT*      ss_memchk_sem;

void SsMemObjInc(ss_memobjtype_t mo, int size);
void SsMemObjDec(ss_memobjtype_t mo, int size);
void SsMemObjIncNomutex(ss_memobjtype_t mo, int size);
void SsMemObjDecNomutex(ss_memobjtype_t mo, int size);
void SsMemObjPrint(void* fp);

#define SS_MEM_SETLINK(p)       SsMemChkSetLinkInfo(p, (char *)__FILE__, __LINE__, TRUE)
#define SS_MEM_SETUNLINK(p)     SsMemChkSetLinkInfo(p, (char *)__FILE__, __LINE__, FALSE)

#else /* SS_DEBUG */

#define SS_MEM_SETLINK(p)
#define SS_MEM_SETUNLINK(p)

#endif /* SS_DEBUG */

#define SS_MEMOBJ_INC(mo, type)
#define SS_MEMOBJ_DEC(mo)

#endif /* SSMEM_H */
