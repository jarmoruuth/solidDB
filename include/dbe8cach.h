/*************************************************************************\
**  source       * dbe8cach.h
**  directory    * dbe
**  description  * Caching system for database engine.
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


#ifndef DBE8CACH_H
#define DBE8CACH_H

#include <ssstdio.h>

#include <ssc.h>

#include <su0svfil.h>
#include <su0mesl.h>

#include "dbe0type.h"

typedef enum {
        DBE_CACHE_READONLY,     /* The slot is read only. */
        DBE_CACHE_WRITEONLY,    /* The slot is write only, it is not read
                                   from the disk file. */
        DBE_CACHE_READWRITE,    /* The slot is used for both reading and
                                   writing. */
        DBE_CACHE_ALLOC,        /* Used only internally. */

        DBE_CACHE_PREFLUSHREACH,/* Used by preflusher only */
        DBE_CACHE_READONLY_IFHIT,
        DBE_CACHE_READWRITE_IFHIT,
        DBE_CACHE_PREFETCH,     /* Used by preflusher only, translated to
                                   DBE_CACHE_READONLY */
        DBE_CACHE_READWRITE_NOCOPY /* The slot is used for both reading and
                                      writing. Do not make a copy of
                                      read-only block. */
} dbe_cache_reachmode_t;

typedef enum {
        DBE_CACHE_CLEAN,        /* Slot not modified */
        DBE_CACHE_DIRTY,        /* Slot modified */
        DBE_CACHE_IGNORE,       /* Ignores the slot */
        DBE_CACHE_FLUSH,        /* Flushes the slot immediately onto disk.
                                   Implies DBE_CACHE_DIRTY. The split
                                   virtual file is NOT flushed. */
        /* The LASTUSE suffix means:
         * The slot will probably not be needed
         * again for some time.
         */
        DBE_CACHE_CLEANLASTUSE, /* Clean slot */
        DBE_CACHE_DIRTYLASTUSE, /* Dirty slot */
        DBE_CACHE_FLUSHLASTUSE, /* Flushes the slot immediately onto disk.
                                   Implies DBE_CACHE_DIRTY. The split
                                   virtual file is NOT flushed. */
        DBE_CACHE_PREFLUSH      /* Same as DBE_CACHE_FLUSHLASTUSE, but
                                   this option puts slot to the LRU
                                   position (used by preflusher only) */
} dbe_cache_releasemode_t;

/* Cache info structure.
 */
typedef struct {
        int     cachei_minchain;     /* Minimum hash chain length. */
        int     cachei_maxchain;     /* Maximum hash chain length. */
        double  cachei_avgchain;     /* Average hash chain length. */
        int     cachei_nchain;       /* Number of used hash slots. */
        int     cachei_nslot;        /* Number of hash slots. */
        int     cachei_nitem;        /* Number of cache items. */
        int     cachei_ndirty;       /* Number of dirty cache items. */
        long    cachei_nfind;        /* Number of finds done in cache. */
        long    cachei_nread;        /* Number of disk reads. */
        long    cachei_nwrite;       /* Number of disk writes. */
        long    cachei_nprefetch;
        long    cachei_npreflush;
        long    cachei_ndirtyrelease;/* Number of releases with dirty flag */
        double  cachei_writeperfind; /* Number of disk writes per finds.(%) */
        double  cachei_writeperread; /* Number of disk writes per disk reads.(%) */

        double  cachei_writeavoidrate;/* lazy write I/O avoidance % */
        double  cachei_readhitrate;  /* cache read hit rate % */
} dbe_cache_info_t;

#ifdef MME_CP_FIX
typedef struct dbe_cache_flushaddr_st {
        su_daddr_t      fa_addr;
        bool            fa_preflush;
        bool            fa_mmeslot;
        ss_byte_t*      fa_writebuf;
        dbe_info_flags_t fa_infoflags;
} dbe_cache_flushaddr_t;

#define DBE_CACHE_MAXNFLUSHES  20
#endif

dbe_cache_t* dbe_cache_init(
        su_svfil_t* svfil,
        uint nblock,
        uint nsem);

void dbe_cache_done(
        dbe_cache_t* cache);

dbe_cacheslot_t* dbe_cache_reach(
        dbe_cache_t* cache,
        su_daddr_t addr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx);

dbe_cacheslot_t* dbe_cache_reachwithhitinfo(
        dbe_cache_t* cache,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx,
        bool* p_hit);

void dbe_cache_setslotreadonly(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot);

void dbe_cache_release(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        dbe_cache_releasemode_t mode,
        void* ctx);

dbe_cacheslot_t* dbe_cache_relocate(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        su_daddr_t newaddr,
        char** p_data,
        dbe_info_flags_t infoflags);

void dbe_cache_setpageaddress(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        su_daddr_t newaddr);

bool dbe_cache_flush(
        dbe_cache_t* cache);

dbe_cacheslot_t* dbe_cache_alloc(
        dbe_cache_t* cache,
        char** p_data);

void dbe_cache_free(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot);

su_daddr_t dbe_cache_getfilesize(
        dbe_cache_t* cache);

uint dbe_cache_getblocksize(
        dbe_cache_t* cache);

su_svfil_t* dbe_cache_getsvfil(
        dbe_cache_t* cache);

void dbe_cache_setinfo(
        dbe_cache_t* cache,
        dbe_cache_info_t* info);

void dbe_cache_getinfo(
        dbe_cache_t* cache,
        dbe_cache_info_t* info);

void* dbe_cacheslot_getdata(
        dbe_cacheslot_t* slot);

void* dbe_cacheslot_getuserdata(
	dbe_cacheslot_t* slot);

bool dbe_cacheslot_setuserdata(
        dbe_cacheslot_t* slot,
        void* userdata,
        int level,
        bool bonsaip);

su_daddr_t dbe_cacheslot_getdaddr(
        dbe_cacheslot_t* slot);

bool dbe_cache_checkafterflush(
        dbe_cache_t* cache);

void dbe_cache_concurrent_flushinit(
        dbe_cache_t* cache);

bool dbe_cache_concurrent_flushstep(
        dbe_cache_t* cache,
        ulong maxwrites,
        dbe_info_flags_t infoflags);

void dbe_cache_ignoreaddr(
        dbe_cache_t* cache,
        su_daddr_t addr);

void dbe_cache_printinfo(
        void* fp,
        dbe_cache_t* cache);

#ifndef SS_MT

void dbe_cache_readahead_add(
	dbe_cache_t* cache,
	su_daddr_t* daddr_array,
        int array_size);

void dbe_cache_readahead_wait(
	dbe_cache_t* cache,
	su_daddr_t daddr);

#endif  /* !SS_MT */

void dbe_cache_setpreflushcallback(
        dbe_cache_t* cache,
        void (*callbackfun)(void*),
        void* callbackctx);

bool dbe_cache_getpreflusharr(
        dbe_cache_t* cache,
        su_daddr_t** p_array,
        size_t* p_arraysize);

bool dbe_cache_setpreflushinfo(
        dbe_cache_t* cache,
        uint percent,
        uint lastuseskippercent);

void dbe_cache_addpreflushpage(
        dbe_cache_t* cache);

su_meslist_t* dbe_cache_getmeslist(
        dbe_cache_t* cache);

bool dbe_cacheslot_isoldversion(
        dbe_cacheslot_t* slot);

#if defined(DBE_MTFLUSH)

void dbe_cache_flushaddr(
        dbe_cache_t* cache,
        su_daddr_t addr,
        bool preflush
#ifdef SS_MME
        , bool mmeslot
#endif
#ifdef DBE_NONBLOCKING_PAGEFLUSH
        , ss_byte_t* writebuf
#endif /* DBE_NONBLOCKING_PAGEFLUSH */        
        , dbe_info_flags_t infoflags);

#ifdef MME_CP_FIX
void dbe_cache_flushaddr_n(
        dbe_cache_t*            cache,
        dbe_cache_flushaddr_t*  flushes,
        ulong                   nflushes);
#endif

bool dbe_cache_getflusharr(
        dbe_cache_t* cache,
        su_daddr_t** p_addrarray,
        size_t* p_addrarraysize);

#endif /* DBE_MTFLUSH */

int dbe_cache_getnslot(
        dbe_cache_t* cache);

char* dbe_cache_reachmodetostr(
        dbe_cache_reachmode_t mode);

#endif /* DBE8CACH_H */
