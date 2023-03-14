/*************************************************************************\
**  source       * ssmemchk.h
**  directory    * ss
**  description  * 
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


#ifndef SSMEMCHK_H
#define SSMEMCHK_H

#ifdef SS_DEBUG

#include "sslimits.h"
#include "ssmem.h"

extern bool memchk_disableprintlist;

extern ulong memchk_nptr;
extern ulong memchk_maxptr;
extern ulong memchk_alloc;
extern ulong memchk_calloc;
extern ulong memchk_realloc;
extern ulong memchk_free;
extern ulong memchk_strdup;
extern ulong memchk_bytes;
extern ulong memchk_maxbytes;

extern long memchk_newborn;     /* Newborn memory check value. */

typedef struct {
        size_t          mc_size;
        ss_memobjtype_t mc_memobj;
} ss_memchk_t;

#define MEMCHK_CHKSIZE              (ss_memdebug_segalloc \
                                        ? 0 : sizeof(ss_memchk_t))
#define MEMCHK_HEADERSIZE           (ss_memdebug_segalloc ? \
                                     0 : SS_MAX(sizeof(ss_memchk_t),SS_ALIGNMENT))
#define MEMCHK_ALLOCSIZE            (MEMCHK_HEADERSIZE + MEMCHK_CHKSIZE)
#define MEMCHK_GETUSERPTR(p)        ((char*)(p) + MEMCHK_HEADERSIZE)
#define MEMCHK_GETSYSTEMPTR(p)      ((char*)(p) - MEMCHK_HEADERSIZE)
#define MEMCHK_GETBEGINMARKPTR(p)   ((char*)(p) + MEMCHK_HEADERSIZE - MEMCHK_CHKSIZE)
#define MEMCHK_GETENDMARKPTR(p, s)  ((char*)(p) + MEMCHK_HEADERSIZE + (size_t)(s))
#define MEMCHK_ILLEGALCHK           (~0)
#define MEMCHK_UNKNOWNSIZE          (~(size_t)0)
#define MEMCHK_DEFAULT_NEWBORN      0xBABE
#define MEMCHK_DEAD                 0XDEAD

void SsMemChkMessage(
	char* msg,
	char* file,
	int line);

void SsMemChkAbort(
	char* msg,
	char* file,
	int line);

void SsMemChkListCheck(
	char* file,
	int line);

void SsMemChkListCheckQmem(
	char* file,
	int line);

void SsMemChkListCheckQmemNoMutex(
	char* file,
	int line);

SsMemChkListT* SsMemChkListFind(
	void* ptr,
	char* file,
	int line);

SsMemChkListT* SsMemChkListFindTest(
	void* ptr,
	char* file,
	int line);

void SsMemChkListSet(
	SsMemChkListT* list,
	void* ptr,
        size_t size,
	char* file,
	int line);

void SsMemChkListAdd(
	void*    ptr,
        size_t   size,
	char*    file,
	int      line);

void SsMemChkListRemove(
        SsMemChkListT* list);

void* SsMemChkSetCheck(
	char* p,
	size_t size,
        bool clear,
        uint fill,
	size_t* p_oldsize,
        ss_memobjtype_t mo);

void* SsMemChkFreeCheck(
        char* p,
        size_t size,
        bool clear,
        uint fill,
        size_t* p_oldsize);

uint SsMemChkGetSize(
        char* p);

void SsMemChkCheckPtr(
        char* p,
        char* file,
        int line);

#endif /* SS_DEBUG */

#endif /* SSMEMCHK_H */
