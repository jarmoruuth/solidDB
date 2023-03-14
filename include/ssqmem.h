/*************************************************************************\
**  source       * ssqmem.h
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


#ifndef SSQMEM_H
#define SSQMEM_H

#include "ssc.h"
#include "ssenv.h"
#include "ssstddef.h"
#include "ssdebug.h"

#define SS_QMEM_SYSTEMSLOT 31

typedef struct {
        size_t  qms_sysptrcount;        /* Number of pointers. */
        size_t  qms_sysbytecount;       /* Allocated system bytes. */
        size_t  qms_slotptrcount;       /* Number of slot pointers. */
        size_t  qms_slotbytecount;      /* Allocated slot bytes. */
        size_t  qms_ptrmin;             /* Minimum allocated ptr address */
        size_t  qms_ptrmax;             /* Max. -"- */
        size_t  qms_thrctxptrcount;     /* Number of thread context pointers. */
        size_t  qms_thrctxbytecount;    /* Allocated thread context bytes. */
        size_t  qms_thrctxhit;          /* Thread context allocation hit. */
        size_t  qms_thrctxmiss;         /* Thread context allocation miss. */
        size_t  qms_thrctxflushcount;   /* Thread context flush count. */
        size_t  qms_thrctxflushptrcount;/* Thread context flush pointer count. */
        size_t  qms_nthrctx;            /* Number of thread contexts. */
        size_t  qms_thrctxcount;        /* Number of allocation from thread context. */
        size_t  qms_userctxcount;       /* Number of allocation from user context. */
        size_t  qms_globalctxcount;     /* Number of allocation from global context. */
        size_t  qms_slotalloccount[SS_QMEM_SYSTEMSLOT+1]; /* Allocations for each slot. */
} SsQmemStatT;

typedef struct SsMemCtxStruct SsMemCtxT;

extern SsQmemStatT ss_qmem_stat;

ss_debug(extern long ss_qmem_nptr;)
ss_debug(extern long ss_qmem_nsysptr;)

void SsQmemGlobalInit(void);
void SsQmemCtxDone(void* ctx);

SsMemCtxT* SsQmemLocalCtxInit(void);
void SsQmemLocalCtxDone(SsMemCtxT* qmem);

void* SsQmemAlloc(size_t size);
void* SsQmemCtxAlloc(SsMemCtxT* qmem, size_t size);
void* SsQmemCalloc(size_t elsize, size_t nelem);
void* SsQmemRealloc(void* oldptr, size_t newsize);
void  SsQmemFree(void* ptr);
void  SsQmemCtxFree(SsMemCtxT* qmem, void* ptr);
void* SsQmemStrdup(char* str);
size_t SsQmemGetDataSize(void* p);

#ifdef SS_PMEM
extern void *(*SsQmemAllocPtr)(size_t size);
extern void *(*SsQmemReallocPtr)(void* oldptr, size_t newsize);
extern void (*SsQmemFreePtr)(void* ptr);
extern void (*SsQmemGetInfoPtr)(SsQmemStatT* p_qmi);
extern size_t (*SsQmemGetDataSizePtr)(void* ptr);
#endif /* SS_PMEM */

void SsQmemGetInfo(SsQmemStatT* p_qmi);

#ifdef SS_PMEM
#define SsQmemAlloc         SsQmemAllocPtr
#define SsQmemRealloc       SsQmemReallocPtr
#define SsQmemFree          SsQmemFreePtr
#define SsQmemGetInfo       SsQmemGetInfoPtr
#define SsQmemGetDataSize   SsQmemGetDataSizePtr
#endif /* SS_PMEM */

void SsQmemFPrintInfo(void* fp);
void SsQmemPrintInfo(void);

size_t SsQmemGetSlotNo(void* p);

bool SsQmemTest(void);
bool SsQmemTestFast(void);

void SsQmemLinkInit(void* p);
uint SsQmemLinkGet(void* p);
uint SsQmemLinkInc(void* p);
uint SsQmemLinkDec(void* p);
uint SsQmemLinkDecZeroFree(void* p);
uint SsQmemLinkIncSafe(void* p);
uint SsQmemLinkDecSafe(void* p);
long SsQmemGetBalance(void);
void SsQmemSetOperationBalance(long balance, long peak);
void SsQmemGetOperationBalance(long* p_balance, long* p_peak);

/* Implementation specific code */

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
/*#define SS_QMEM_NO_JMP*/


# define SS_QMEM_HIBYTE(n)  ((n) >> CHAR_BIT)

extern uint ss_qmem_slotmap[];

/* --------------------------------------------------------------------- */
# ifdef SS_QMEM_NO_JMP
extern uint ss_qmem_slotmask[];

#   define SS_QMEM_LOWBYTE(n) ((n) & ((1 << CHAR_BIT)-1))
#   define SS_QMEM_GETSLOTNUM(size, slotnum) \
{\
        register uint hibyte = SS_QMEM_HIBYTE(size);\
        register uint mask = ss_qmem_slotmask[hibyte];\
        slotnum = ((CHAR_BIT & mask) +\
            (ss_qmem_slotmap[SS_QMEM_LOWBYTE(size)] & ~mask)) +\
            ss_qmem_slotmap[hibyte];\
}

/* --------------------------------------------------------------------- */
# else   /* SS_QMEM_NO_JMP is not defined */

#   define SS_QMEM_GETSLOTNUM(size, slotnum) \
        if (size < (1 << CHAR_BIT)) { \
            slotnum = ss_qmem_slotmap[size]; \
        } else { \
            slotnum = CHAR_BIT + ss_qmem_slotmap[SS_QMEM_HIBYTE(size)];\
        }

# endif  /* SS_QMEM_NO_JMP */

#ifdef SS_ZMEM
#include "sszmem.h"
#endif /* SS_ZMEM */

#endif /* SSQMEM_H */
