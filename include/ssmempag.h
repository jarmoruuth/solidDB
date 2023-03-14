/*************************************************************************\
**  source       * ssmempag.h
**  directory    * ss
**  description  * OS-level memory page allocation interface
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


#ifndef SSMEMPAG_H
#define SSMEMPAG_H

#include "ssstddef.h"

void SsMemPageGlobalInit(void);
void* SsMemPageAlloc1(void);
void SsMemPageFree1(void* p);
void* SsMemPageSbrk(long nbytes);

void*   SsMemPageAlloc(size_t s);
void*   SsMemPageRealloc(void* p, size_t newsize, size_t oldsize);
void    SsMemPageFree(void* p, size_t s);
size_t  SsMemPageSize(void);

#if defined(SS_DEBUG) && defined(SS_NT)
void SsMemPageDenyAccess(void* p, size_t size);
void SsMemPageAllowAccess(void* p, size_t size);
#endif /* SS_DEBUG */


#endif /* SSMEMPAG_H */
