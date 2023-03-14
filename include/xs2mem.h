/*************************************************************************\
**  source       * xs2mem.h
**  directory    * xs
**  description  * eXternal Sort Memory allocation interface
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


#ifndef XS2MEM_H
#define XS2MEM_H

#include <ssstddef.h>
#include <ssc.h>

/* memory buffer interface object */
typedef struct xs_mem_st xs_mem_t;

/* memory handle object */
#define xs_hmem_t void

typedef xs_hmem_t* xs_mem_allocfun_t(void*, void*);
typedef void xs_mem_freefun_t(void*, xs_hmem_t*);
typedef void* xs_hmem_getbuffun_t(xs_hmem_t*);

xs_mem_t* xs_mem_init(
            ulong max_blocks,
            size_t block_size,
            void* ctx,
            xs_mem_allocfun_t* p_alloc_f,
            xs_mem_freefun_t* p_free_f,
            xs_hmem_getbuffun_t* p_getbuf_f);

void xs_mem_done(xs_mem_t* mem);

bool xs_mem_reserve(xs_mem_t* mem, ulong n);
bool xs_mem_reserveonfree(xs_mem_t* mem, ulong n);
void xs_mem_unreserve(xs_mem_t* mem, ulong n);

xs_hmem_t* xs_mem_allocreserved(xs_mem_t* mem, void* /* (void**, really) */ p_buf);
xs_hmem_t* xs_mem_alloc(xs_mem_t* mem, void* /* (void**, really) */ p_buf);
void xs_mem_free(xs_mem_t* mem, xs_hmem_t* hmem);
void* xs_hmem_getbuf(xs_mem_t* mem, xs_hmem_t* hmem);

ulong xs_mem_getmaxblocks(xs_mem_t* mem);
ulong xs_mem_getnblocksavail(xs_mem_t* mem);
size_t xs_mem_getblocksize(xs_mem_t* mem);

#endif /* XS2MEM_H */
