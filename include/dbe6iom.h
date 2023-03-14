/*************************************************************************\
**  source       * dbe6iom.h
**  directory    * dbe
**  description  * Multithreaded I/O Manager for SOLID DBMS
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


#ifndef DBE6IOM_H
#define DBE6IOM_H

#include "dbe6finf.h"
#include "dbe8cach.h"

dbe_iomgr_t* dbe_iomgr_init(
        dbe_file_t* file, 
        dbe_cfg_t* cfg);

void dbe_iomgr_done(
        dbe_iomgr_t* iomgr);

void dbe_iomgr_preflush(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size,
        dbe_info_flags_t infoflags);

void dbe_iomgr_prefetch(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size,
        dbe_info_flags_t infoflags);

void dbe_iomgr_prefetchwait(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr);

dbe_cacheslot_t* dbe_iomgr_reach(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx);

void dbe_iomgr_release(
        dbe_iomgr_t* iomgr,
        dbe_cacheslot_t* slot,
        dbe_cache_releasemode_t mode,
        void* ctx);

#if defined(DBE_MTFLUSH)

bool dbe_iomgr_flushallcaches_init(
        dbe_iomgr_t* iomgr,
        su_daddr_t** flusharray,
        size_t* flusharraysize);

void dbe_iomgr_flushallcaches_exec(
        dbe_iomgr_t* iomgr,
        void (*wakeupfp)(void*),
        void* wakeupctx,
        su_daddr_t* flusharray,
        size_t flusharraysize,
        dbe_info_flags_t infoflags);

bool dbe_iomgr_addtoflushbatch(
        dbe_iomgr_t* iomgr,
        dbe_iomgr_flushbatch_t* flushbatch,
        dbe_cacheslot_t* slot,
        su_daddr_t daddr,
        dbe_info_flags_t infoflags);

dbe_iomgr_flushbatch_t* dbe_iomgr_flushbatch_init(
        dbe_iomgr_t* iomgr,
        void (*wakeupfp)(void*),
        void* wakeupctx,
        void (*roomfp)(void*),
        void* roomctx);

void dbe_iomgr_flushbatch_done(
        dbe_iomgr_t* iomgr,
        dbe_iomgr_flushbatch_t*);

uint dbe_iomgr_flushbatch_nleft(
        dbe_iomgr_t* iomgr,
        dbe_iomgr_flushbatch_t* fb);

#ifdef SS_MME

size_t dbe_iomgr_get_mme_nflushed(dbe_iomgr_t* iomgr);
size_t dbe_iomgr_get_mme_ntoflush(dbe_iomgr_t* iomgr);

#endif /* SS_MME */

size_t dbe_iomgr_get_dbe_nflushed(dbe_iomgr_t* iomgr);
size_t dbe_iomgr_get_dbe_ntoflush(dbe_iomgr_t* iomgr);

#ifdef SS_DEBUG
void dbe_iomgr_debug_cancel_dbeflush(void);
void dbe_iomgr_debug_cancel_mmeflush(void);
#endif /* SS_DEBUG */

#endif /* DBE_MTFLUSH */

#endif /* DBE6IOM_H */
