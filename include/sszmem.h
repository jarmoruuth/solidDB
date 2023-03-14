/*************************************************************************\
**  source       * sszmem.h
**  directory    * ss
**  description  * Memory allocator for low-end systems with little
**               * physical memory
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


#ifndef SSZMEM_H
#define SSZMEM_H

#include "ssc.h"
#include "ssstddef.h"

void* SsZmemAlloc(size_t n);
void* SsZmemRealloc(void* p, size_t n);
void* SsZmemCalloc(size_t elsize, size_t nelem);
void* SsZmemStrdup(char* s);
void  SsZmemFree(void* p);
void  SsZmemLinkInit(void* p);
uint  SsZmemLinkGet(void* p);
uint  SsZmemLinkInc(void* p);
uint  SsZmemLinkDec(void* p);
uint  SsZmemLinkIncSafe(void* p);
uint  SsZmemLinkDecSafe(void* p);
void  SsZmemGetInfo(SsQmemStatT* p_qmi);
size_t SsZmemGetSlotSize(void* p);
void SsZmemGlobalInit(void);
void SsZmemCtxDone(void);

extern SsQmemStatT ss_zmem_stat;
extern size_t ss_zmem_pagesize;
ss_debug(extern long ss_zmem_nptr;)
ss_debug(extern long ss_zmem_nsysptr;)


#endif /* SSZMEM_H */
