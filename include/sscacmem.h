/*************************************************************************\
**  source       * sscacmem.h
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


#ifndef SSCACMEM_H
#define SSCACMEM_H

#include "ssc.h"

typedef struct SsCacMemSt SsCacMemT;

SsCacMemT* SsCacMemInit(
        size_t blocksize,
        size_t nblocks);

void SsCacMemDone(
        SsCacMemT* cm);

void* SsCacMemAlloc(
        SsCacMemT* cm);

void SsCacMemFree(
        SsCacMemT* cm,
        void* p);

#endif /* SSCACMEM_H */
