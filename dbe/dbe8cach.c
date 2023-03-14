/**************************************************************************\
**  source       * dbe8cach.c
**  directory    * dbe
**  description  * Caching system for database engine.
**               *
**               * Copyright (C) 2006 Solid Information Technology Ltd
\**************************************************************************/
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

This module implements the database engine caching system. The user of
this module can requst a database block using the address of the block.
All available blocks are stored into a hash table for searching, and all
blocks that are not currently in use (reached) are kept in a LRU list.
The LRU is used for block replacement.

When the caching system is opened, the number of blocks to be used
for caching is given and the split virtual file handle that is cached.
The block size used is the same as the split virtual file block size.
During initialization all memory blocks are allocated. If enough blocks
cannot be allocated, the system will fail (propably to an assert).

Hash table is a simple hash table that uses separate chaining to resolve
collision of blocks that hash into same address. The hash table size is
the same as the maximum number of blocks in the caching system.
NOTE! Maybe the hash table should be smaller in some cases.

The LRU list is implemented as a doubly linked list. When a block is
reached it is removed from the LRU list. If the requested block is not
already in memory, a free block is taken from the tail of the LRU list.
When the block is released, it is put to the head of the LRU list.

All external functions are reentrant, and need not be called from a mutex
section. All static function, unless otherwise specified, must be called
inside the cache mutex.

If a block is reached that is in use by another thread, the later thread
waits until the block is released. Several threads can be waiting for
the same block. Several threads can use the same block for a read-only
access. If a block is requested for exclusive access (not read-only)
and there is one or more read-only accesses to the same block, a copy
of the block is made fpor exclusive access. The copy becomes primary copy
of the block and when the last read-only access is released to the old
block, that block is ignored.

All actual read and write operations (svfil read and write calls) are done
outside the cache mutex. This should ensure that several threads can be
doing disk I/O at the same time.

Limitations:
-----------

Currently, only one file can be cached with one cache object.
There must be more blocks in the cache than the number of threads
that can access the cache.

Error handling:
--------------

Error conditions are handled using asserts.

Objects used:
------------

mutex semaphores    sssen
event semaphores    sssem
split virtual file  su0svfil

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

All external function can be called concurrently by several threads
without extra protection. The locking mechanism consists of the LRU
mutex and hash bucket mutexes. Deadlocks are prevented by always locking
the hash bucket and then the LRU. When 2 buckets need to be locked, the
later one is locked using bounce-request for the semaphore and making a
retry when locking fails. In cases when the slot is reached to write
mode access by another thread the requesting thread sleep into event
wait. For that purpose each cache slot has a message semaphore.


Example:
-------

void main(void)
{
        dbe_cache_t* cache;
        dbe_cacheslot_t* slot;
        su_svfil_t* svfil;
        char* buf;

        /* initialize svfil */
        svfil = ...;

        /* Open cache with 1000 memory blocks */
        cache = dbe_cache_init(svfil, 1000, 100);

        /* Get the first address. */
        slot = dbe_cache_reach(
                    cache,
                    (su_daddr_t)0,
                    DBE_CACHE_READWRITE,
                    &buf,
                    NULL);

        strcpy(buf, "Header");

        /* Release slot and mark it as changed. */
        dbe_cache_release(cache, slot, DBE_CACHE_DIRTY, NULL);

        dbe_cache_done(cache);
}

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstdio.h>
#include <ssstddef.h>
#include <ssstring.h>
#include <sslimits.h>
#include <sssprint.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssthread.h>
#include <sscacmem.h>
#include <sspmon.h>
#include <sstime.h>
#include <ssmemtrc.h>

#include <su0rbtr.h>
#include <su0error.h>
#include <su0svfil.h>
#include <su0crc32.h>
#include <su0rand.h>
#include <su0cfgst.h>
#include <su0prof.h>

#include "dbe9type.h"
#include "dbe9bhdr.h"
#include "dbe8cach.h"
#include "dbe0erro.h"

/*#define DISABLE_LASTUSE *//* temporary */
#define DBE_IOMGR_FLUSHES_ALONE

#define CACHE_LOCK(daddr,bucket) \
        ss_dprintf_4(("Lock 0x%08lX, %d\n", bucket, __LINE__))
#define CACHE_LOCKED(daddr,bucket) \
        ss_dprintf_4(("Locked 0x%08lX, %d\n", bucket, __LINE__))
#define CACHE_NOTLOCKED(daddr,bucket) \
        ss_dprintf_4(("Lock fail 0x%08lX, %d\n", bucket, __LINE__))
#define CACHE_UNLOCK(bucket) \
        ss_dprintf_4(("Unlock 0x%08lX, %d\n", bucket, __LINE__))
#define CACHE_LRULOCK() \
        ss_dprintf_4(("LRU Lock, %d\n", __LINE__));
#define CACHE_LRULOCKED() \
        ss_dprintf_4(("LRU Locked, %d\n", __LINE__));
#define CACHE_LRUUNLOCK() \
        ss_dprintf_4(("LRU Unlock, %d\n", __LINE__));

#define CHK_SLOT(s,prefetch) ss_debug(slot_check(s, prefetch, __LINE__))

#define PREFLUSH_MAX(cache)         ((cache)->cac_preflushmax)

#define PREFLUSHPORTION_MAX 10000

#ifdef SS_MT
#define PREFLUSH_THREAD
#define SLOT_QUEUE
#endif

#if defined(SS_DEBUG)
#  define crc_debug(s)    ss_debug_1(s)
#  define crc_dassert(e)  ss_debug_1(ss_assert(e))
#else /* SS_DEBUG */
#  define crc_debug(s)
#  define crc_dassert(e)
#endif /* SS_DEBUG */

#ifdef SS_DEBUG

#  define CACHE_MAXSLOTCTX     80
static char cache_alloc_ctx[] = "CACHE Alloc";

#else

#define cache_alloc_ctx NULL

#endif

#if (defined(SS_DEBUG) || defined(AUTOTEST_RUN)) && defined(SLOT_QUEUE)
#define SLOT_REACH_HISTORY
#endif

#ifdef SLOT_QUEUE

typedef struct cache_slotqticket_st cache_slotqticket_t;
struct cache_slotqticket_st {
        bool                    sqt_exclusive;
        su_mes_t*               sqt_mes;
        cache_slotqticket_t*    sqt_next;
#ifdef SLOT_REACH_HISTORY
        dbe_cache_reachmode_t   sqt_rmode;
        dbe_cache_reachmode_t   sqt_origmode;
        int                     sqt_inuse;
        int                     sqt_thrid;
        int                     sqt_coming_from_slotwait;
        int                     sqt_retries;
        su_daddr_t              sqt_daddr;
#endif
};

typedef struct cache_slotq_st cache_slotq_t;

struct cache_slotq_st {
        cache_slotqticket_t* sq_first;
        cache_slotqticket_t* sq_last;
};

#endif /* SLOT_QUEUE */

#ifdef SLOT_REACH_HISTORY

#define MAX_SLOT_REACH_HISTORY  10

typedef struct {
        dbe_cache_reachmode_t   sh_rmode;
        dbe_cache_reachmode_t   sh_reachmode;
        dbe_cache_releasemode_t sh_releasemode;
        int                     sh_inuse;
        int                     sh_thrid;
        void*                   sh_stackptr;
        cache_slotqticket_t*    sh_sqfirstptr;
        cache_slotqticket_t     sh_sqfirst;
        bool                    sh_wait;
        int                     sh_nwakeup;
        int                     sh_wakeupthr;
        bool                    sh_relocate;
        int                     sh_nretry;
        int                     sh_coming_from_slotwait;
        bool                    sh_returnslot;
} slot_history_t;

#define SET_SLOT_HISTORY(reachmode,releasemode) {\
        slot->slot_historypos++;\
        if (slot->slot_historypos == MAX_SLOT_REACH_HISTORY) {\
            slot->slot_historypos = 0;\
        }\
        slot->slot_history[slot->slot_historypos].sh_rmode = slot->slot_rmode;\
        slot->slot_history[slot->slot_historypos].sh_reachmode = reachmode;\
        slot->slot_history[slot->slot_historypos].sh_releasemode = releasemode;\
        slot->slot_history[slot->slot_historypos].sh_inuse = slot->slot_inuse;\
        slot->slot_history[slot->slot_historypos].sh_thrid = SsThrGetid();\
        slot->slot_history[slot->slot_historypos].sh_stackptr = &cache;\
        slot->slot_history[slot->slot_historypos].sh_wait = FALSE;\
        slot->slot_history[slot->slot_historypos].sh_nwakeup = 0;\
        slot->slot_history[slot->slot_historypos].sh_wakeupthr = -1;\
        slot->slot_history[slot->slot_historypos].sh_relocate = FALSE;\
        slot->slot_history[slot->slot_historypos].sh_nretry = 0;\
        slot->slot_history[slot->slot_historypos].sh_returnslot = FALSE;\
        slot->slot_history[slot->slot_historypos].sh_sqfirstptr = slot->slot_queue.sq_first;\
        if (slot->slot_queue.sq_first != NULL) {\
            slot->slot_history[slot->slot_historypos].sh_sqfirst = *slot->slot_queue.sq_first;\
        } else {\
            memset(&slot->slot_history[slot->slot_historypos].sh_sqfirst, '\0', sizeof(slot->slot_history[0].sh_sqfirst));\
        }\
}

#define SET_SLOT_WAIT       {\
                                slot->slot_history[slot->slot_historypos].sh_wait = TRUE;\
                            }
#define SET_SLOT_WAKEUP     {\
                                slot->slot_history[slot->slot_historypos].sh_nwakeup++;\
                                if (first) {\
                                    slot->slot_history[slot->slot_historypos].sh_wakeupthr = slot->slot_queue.sq_first->sqt_thrid;\
                                }\
                            }
#define SET_SLOT_RELOCATE   {\
                                slot->slot_history[slot->slot_historypos].sh_relocate = TRUE;\
                            }
#define SET_SLOT_OWNERTHRID {\
                                slot->slot_ownerthrid = SsThrGetid();\
                                slot->slot_ownerstack = &cache;\
                                slot->slot_history[slot->slot_historypos].sh_returnslot = TRUE;\
                            }
#define SET_SLOT_RETRY      {\
                                slot->slot_history[slot->slot_historypos].sh_nretry = retries;\
                                slot->slot_history[slot->slot_historypos].sh_coming_from_slotwait = coming_from_slotwait;\
                            }

#else  /* SLOT_REACH_HISTORY */

#define SET_SLOT_HISTORY(reachmode,releasemode)
#define SET_SLOT_WAIT
#define SET_SLOT_WAKEUP
#define SET_SLOT_RELOCATE
#define SET_SLOT_OWNERTHRID
#define SET_SLOT_RETRY

#endif /* SLOT_REACH_HISTORY */

/* Cache block control structure.
*/

typedef struct {
        dbe_cacheslot_t* next;  /* Next slot in LRU chain. */
        dbe_cacheslot_t* prev;  /* Previous slot in LRU chain. */
} slot_lrulink_t;

/* forward declaration */
typedef struct lrulist_st lrulist_t;

struct dbe_cacheslot_st {
        slot_lrulink_t        slot_; /* MUST BE first field!!!! */
        su_daddr_t            slot_daddr;   /* see typedef above */
        bool                  slot_dirty;   /* Flag: block modified */
#ifdef DBE_NONBLOCKING_PAGEFLUSH
        bool                  slot_writingtodisk; /* Flag: disk write */  
#endif /* DBE_NONBLOCKING_PAGEFLUSH */        
        bool                  slot_oldvers; /* Flag: block is old version */
        bool                  slot_preflushreq; /* Flag: preflush requested */
        char*                 slot_data;    /* Pointer to the data area. */
        dbe_cache_reachmode_t slot_rmode;   /* Slot reach mode. */
        int                   slot_inuse;   /* The slot in use counter. */


        void*                 slot_userdata; /* User data associated to the
                                                slot. */
        int                   slot_level;
        int                   slot_whichtree;
        dbe_cacheslot_t*      slot_hashnext; /* Next slot in hash chain. */

        ulong                 slot_flushctr;
        dbe_cache_t*          slot_cache;
        int                   slot_lruindex; /* LRU index for this slot */
        lrulist_t*            slot_lrulist;
        ss_debug(FOUR_BYTE_T  slot_crc;)
        ss_debug(char*        slot_debug[CACHE_MAXSLOTCTX];)
        ss_debug(char**       slot_callstack;)
#ifdef SLOT_REACH_HISTORY
        slot_history_t        slot_history[MAX_SLOT_REACH_HISTORY];
        uint                  slot_historypos;
        int                   slot_ownerthrid;
        void*                 slot_ownerstack;
#endif
#ifdef SLOT_QUEUE
        cache_slotq_t         slot_queue;
#else
        SsMesT*               slot_waitmsg; /* wake signal object to waiter */
        int                   slot_nwait;   /* # of waiters for slot_waitmsg */
#endif
};

/* Hash bucket structure that contains
 * mutex semaphore + pointer to actual slot
 */
typedef struct {
        SsMutexT*           hb_mutex;
        dbe_cacheslot_t*    hb_slot;
} hashbucket_t;

/* Hash structure used to search blocks.
*/

typedef struct {
        uint            h_blocksize;   /* Storage block size in cache. */
        uint            h_tablesize;   /* Hash table size. */
        hashbucket_t*   h_table;       /* Hash table. */
} cache_hash_t;

#ifdef CACHE_CRC_CHECK
#define CACHE_CRC_ARRAY_SIZE 10000
FOUR_BYTE_T cache_crc_array[CACHE_CRC_ARRAY_SIZE];
#endif /* CACHE_CRC_CHECK */

/* ** PLRU START ** */

/* PLRU constants */
#define PLRU_NLRU           16

/* Priority LRU (PLRU) structure */

struct lrulist_st {
        slot_lrulink_t   ll_head;
        size_t           ll_n;
};

SS_INLINE void lrulist_initbuf(lrulist_t* ll)
{
        ll->ll_head.next = (dbe_cacheslot_t*)(char*)&(ll->ll_head);
        ll->ll_head.prev = (dbe_cacheslot_t*)(char*)&(ll->ll_head);
        ll->ll_n = 0;
}

SS_INLINE void lrulist_donebuf(lrulist_t* ll)
{
        lrulist_initbuf(ll); /* resets to 0 length */
}

SS_INLINE void lrulist_insertfirst(lrulist_t* ll,
                                   dbe_cacheslot_t* slot)
{
        ss_dassert(slot->slot_inuse == 0);
        ss_dassert(slot->slot_lrulist == NULL);
        slot->slot_.next = ll->ll_head.next;
        slot->slot_.prev = (dbe_cacheslot_t*)(char*)(&ll->ll_head);
        slot->slot_lrulist = ll;
        ll->ll_head.next->slot_.prev = slot;
        ll->ll_head.next = slot;
        ll->ll_n++;
}

SS_INLINE void lrulist_insertlast(lrulist_t* ll,
                                  dbe_cacheslot_t* slot)
{
        ss_dassert(slot->slot_inuse == 0);
        ss_dassert(slot->slot_lrulist == NULL);
        slot->slot_.next = (dbe_cacheslot_t*)(char*)(&ll->ll_head);
        slot->slot_.prev = ll->ll_head.prev;
        slot->slot_lrulist = ll;
        ll->ll_head.prev->slot_.next = slot;
        ll->ll_head.prev = slot;
        ll->ll_n++;
}

SS_INLINE dbe_cacheslot_t* lrulist_getlast(lrulist_t* ll)
{
        if (ll->ll_n == 0) {
            return (NULL);
        }
        return (ll->ll_head.prev);
}

SS_INLINE dbe_cacheslot_t* lrulist_getfirst(lrulist_t* ll)
{
        if (ll->ll_n == 0) {
            return (NULL);
        }
        return (ll->ll_head.next);
}

SS_INLINE void lrulist_unlink(lrulist_t* ll, dbe_cacheslot_t* slot)
{
        ss_dassert(ll->ll_n >= 1);
        ss_dassert(slot->slot_lrulist == ll);
        slot->slot_.next->slot_.prev = slot->slot_.prev;
        slot->slot_.prev->slot_.next = slot->slot_.next;
        slot->slot_lrulist = NULL;
        ss_debug(slot->slot_.prev = slot->slot_.next = NULL);
        ll->ll_n--;
}

SS_INLINE size_t lrulist_nslots(lrulist_t* ll)
{
        return (ll->ll_n);
}

typedef enum {
    PLRU_CLASS_NORMAL,
    PLRU_CLASS_LASTUSE,
    PLRU_CLASS_IGNORE
} plru_class_t;

typedef struct {
        /* dbe_cacheslot_t*  pl_head[PLRU_NCLASS]; */
        lrulist_t         pl_lrulist1; /* standard LRU chain (High priority) */
        lrulist_t         pl_lrulist2; /* standard LRU chain (Low priority) */
        uint              pl_lrulist2_percent;

        /* potential victims for LRU raplacements */
        lrulist_t         pl_victimarea_next_flushbatch;
        lrulist_t         pl_victimarea_current_flushbatch;
        lrulist_t         pl_victimarea_clean_list;
        size_t            pl_victimarea_minsize;

        size_t            pl_nslots; /* original # of slots */
        int               pl_lruindex;
        ss_debug(SsMutexT*  pl_mutex;)
} plru_t;

static void plru_initbuf(plru_t* plru, int lruindex)
{
        lrulist_initbuf(&plru->pl_lrulist1);
        lrulist_initbuf(&plru->pl_lrulist2);
        lrulist_initbuf(&plru->pl_victimarea_next_flushbatch);
        lrulist_initbuf(&plru->pl_victimarea_current_flushbatch);
        lrulist_initbuf(&plru->pl_victimarea_clean_list);
        plru->pl_lrulist2_percent = 50; /* default: LASTUSE skips 50% */
        plru->pl_victimarea_minsize = 0;
        plru->pl_nslots = 0;
        plru->pl_lruindex = lruindex;
}

static void plru_donebuf(plru_t* plru)
{
        lrulist_donebuf(&plru->pl_lrulist1);
        lrulist_donebuf(&plru->pl_lrulist2);
        lrulist_donebuf(&plru->pl_victimarea_next_flushbatch);
        lrulist_donebuf(&plru->pl_victimarea_current_flushbatch);
        lrulist_donebuf(&plru->pl_victimarea_clean_list);
}

SS_INLINE size_t plru_victimarea_nslots(
        plru_t* plru)
{
        return (lrulist_nslots(&plru->pl_victimarea_next_flushbatch) +
                lrulist_nslots(&plru->pl_victimarea_current_flushbatch) +
                lrulist_nslots(&plru->pl_victimarea_clean_list));
}

static void plru_age(plru_t* plru)
{
        size_t victimarea_nslots;
        lrulist_t* src_ll;
        
        victimarea_nslots = plru_victimarea_nslots(plru);
        src_ll = &plru->pl_lrulist2;
        while (victimarea_nslots < plru->pl_victimarea_minsize) {
            lrulist_t* ll;
            dbe_cacheslot_t* s;
            
            s = lrulist_getlast(src_ll);
            if (s == NULL) {
                if (src_ll == &plru->pl_lrulist2) {
                    src_ll = &plru->pl_lrulist1;
                    s = lrulist_getlast(src_ll);
                    if (s == NULL) {
                        break;
                    }
                } else {
                    /* No more pages available, they must be in use */
                    break;
                }
            }
            lrulist_unlink(src_ll, s);
            if (s->slot_dirty) {
                ss_dassert(s->slot_daddr != SU_DADDR_NULL);
                ll = &plru->pl_victimarea_next_flushbatch;
            } else {
                ll = &plru->pl_victimarea_clean_list;
            }
            lrulist_insertfirst(ll, s);
            victimarea_nslots++;
        }
        ss_dassert(victimarea_nslots < plru->pl_victimarea_minsize ||
                   victimarea_nslots == plru_victimarea_nslots(plru));
        {
            dbe_cacheslot_t* s;
            size_t lru2_min_nslots;
            size_t lru1_nslots;
            size_t lru2_nslots;
            size_t lru_slotsum;
            
            lru1_nslots = lrulist_nslots(&plru->pl_lrulist1);
            lru2_nslots = lrulist_nslots(&plru->pl_lrulist2);
            lru_slotsum = lru1_nslots + lru2_nslots;
            if (lru_slotsum == 0) {
                return;
            }
            lru2_min_nslots = (plru->pl_lrulist2_percent * lru_slotsum)
                / 100;
            while (lru2_nslots < lru2_min_nslots) {
                s = lrulist_getlast(&plru->pl_lrulist1);
                if (s == NULL) {
                    break;
                }
                lrulist_unlink(&plru->pl_lrulist1, s);
                lrulist_insertfirst(&plru->pl_lrulist2, s);
                lru2_nslots++;
            }
            ss_dassert(lru2_nslots < lru2_min_nslots ||
                       lru2_nslots == lrulist_nslots(&plru->pl_lrulist2));
        }
                
}
static void plru_insert(
        plru_t* plru,
        dbe_cacheslot_t* slot,
        plru_class_t class)
{
        ss_dassert(slot->slot_inuse == 0);
        ss_dassert(slot->slot_lruindex == plru->pl_lruindex);
        switch (class) {
            case PLRU_CLASS_NORMAL:
                lrulist_insertfirst(&plru->pl_lrulist1, slot);
                break;
            case PLRU_CLASS_LASTUSE:
                lrulist_insertfirst(&plru->pl_lrulist2, slot);
                break;
            default:
                ss_rc_derror(class);
                /* FALLTHROUGH, not reached */
            case PLRU_CLASS_IGNORE:
                ss_dassert(!slot->slot_dirty);
                ss_dassert(slot->slot_oldvers ||
                           slot->slot_daddr == SU_DADDR_NULL);
                lrulist_insertlast(&plru->pl_victimarea_clean_list,
                                   slot);
                return;
        }
        plru_age(plru);
}

SS_INLINE void plru_ignore(plru_t* plru, dbe_cacheslot_t* slot)
{
        plru_insert(plru, slot, PLRU_CLASS_IGNORE);
}

SS_INLINE void plru_remove(plru_t* plru, dbe_cacheslot_t* slot)
{
        ss_dassert(slot->slot_lrulist != NULL);
        lrulist_unlink(slot->slot_lrulist, slot);
        ss_dassert(slot->slot_lrulist == NULL);
}

SS_INLINE bool plru_slot_is_in_current_flushbatch(
        plru_t* plru,
        dbe_cacheslot_t* slot)
{
        return (slot->slot_lrulist == &plru->pl_victimarea_current_flushbatch);
}

SS_INLINE dbe_cacheslot_t* plru_remove_preflushslot(plru_t* plru)
{
        dbe_cacheslot_t* slot;
        lrulist_t* ll = &plru->pl_victimarea_next_flushbatch;

        slot = lrulist_getlast(ll);
        if (slot != NULL) {
            lrulist_unlink(ll, slot);
        }
        return (slot);
}

SS_INLINE void plru_link_to_current_preflushbatch(
        plru_t* plru,
        dbe_cacheslot_t* slot)
{
        ss_dassert(slot->slot_lrulist == NULL);
        lrulist_insertfirst(&plru->pl_victimarea_current_flushbatch, slot);
        ss_dassert(slot->slot_lrulist ==
                   &plru->pl_victimarea_current_flushbatch);
}

SS_INLINE void plru_link_to_clean_victims_list(
        plru_t* plru,
        dbe_cacheslot_t* slot)
{
        ss_dassert(slot->slot_lrulist == NULL);
        lrulist_insertfirst(&plru->pl_victimarea_clean_list, slot);
        ss_dassert(slot->slot_lrulist ==
                   &plru->pl_victimarea_clean_list);
}

dbe_cacheslot_t* plru_find_victim(plru_t* plru, bool must_be_clean)
{
        dbe_cacheslot_t* slot;
        lrulist_t* ll;

        plru_age(plru);
        ll = &plru->pl_victimarea_clean_list;
        slot = lrulist_getlast(ll);
        if (slot == NULL) {
            if (must_be_clean) {
                return (NULL);
            }
            ll = &plru->pl_victimarea_current_flushbatch;
            slot = lrulist_getlast(ll);
            if (slot == NULL) {
                ll = &plru->pl_victimarea_next_flushbatch;
                slot = lrulist_getlast(ll);
            }
            if (slot == NULL) {
                return (NULL);
            }
        }
        lrulist_unlink(ll, slot);
        return (slot);
}


SS_INLINE size_t plru_ntoflush(plru_t* plru)
{
        return (lrulist_nslots(&plru->pl_victimarea_next_flushbatch));
}

SS_INLINE size_t plru_nslots(
        plru_t* plru)
{
        return (lrulist_nslots(&plru->pl_lrulist1) +
                lrulist_nslots(&plru->pl_lrulist2) +
                lrulist_nslots(&plru->pl_victimarea_next_flushbatch) +
                lrulist_nslots(&plru->pl_victimarea_current_flushbatch) +
                lrulist_nslots(&plru->pl_victimarea_clean_list));
}

SS_INLINE void plru_setvictimarea_minsize(
        plru_t* plru,
        size_t minsize)
{
        plru->pl_victimarea_minsize = minsize;
}

SS_INLINE void plru_setlastuseskippercent(
        plru_t* plru,
        uint lastuseskippercent)
{
        ss_dassert(lastuseskippercent <= 100U); 
        plru->pl_lrulist2_percent = 100 - lastuseskippercent;
}

/* ** PLRU END ** */

/* Control structure of the cache
*/

struct dbe_cache_st {
        ss_debug(dbe_chk_t cac_check;)      /* Check value. */
        int                 cac_nslot;      /* Number of cache slots. */
        size_t              cac_blocksize;  /* Memory block size. */
        su_svfil_t*         cac_svfil;      /* Cache file. */
        SsFlatMutexT        cac_lrumutex[PLRU_NLRU];

#ifdef SS_MT
        uint                cac_npagemtx;
#  ifdef SS_FLATSEM
        SsFlatMutexT*       cac_pagemtxarr;
#  else /* SS_FLATSEM */
        SsMutexT**          cac_pagemtxarr;
# endif /* SS_FLATSEM */
#endif

#ifdef SLOT_QUEUE
        su_meslist_t        cac_meslist;
#else
        SsSemT*             cac_slotwaitmutex;

#endif
        plru_t              cac_plru[PLRU_NLRU];
        cache_hash_t        cac_hash;       /* Hash table. */
        dbe_cache_info_t    cac_info;       /* Cache info storage. */

        ulong               cac_flushctr;   /* counts concurrent flushes */
        bool                cac_flushflag;  /* flags concurrent flush */
        su_rbt_t*           cac_flushpool;
        su_rbt_node_t*      cac_flushpos;
        SsCacMemT*          cac_cacmem;

        ulong               cac_ndirtyrel;  /* clean->dirty transition ctr */
        ulong               cac_npagerep;   /* page replacement ctr */
        int                 cac_preflushportion;
                                            /* portion of LRU where
                                             * the preflusher operates
                                             */
        int                 cac_preflushmax;/* max. number of slots to
                                             * flush at one preflush step
                                             * in single-threaded environment
                                             */
        void              (*cac_preflushcallback)(void*);
        void*               cac_preflushctx;
#if 0
        uint                cac_preflushsamplesize;
        uint                cac_preflushdirtyprc;
        uint                cac_cleanpagesearchlimit;
#endif /* 0 */
        uint                cac_extpreflushreq; /* External preflush req count. */
        bool                cac_readonly;
        ss_profile(SsSemDbgT* cac_semdbg_meswait;)
        ss_profile(SsSemDbgT* cac_semdbg_retry;)
};

#define HASH_INDEX(hash, daddr) (uint)((daddr) % (hash)->h_tablesize)
#define HASH_BUCKET(hash, daddr) (&(hash)->h_table[HASH_INDEX(hash, daddr)])

extern bool dbefile_diskless; /* for diskless, no physical dbfile */

static volatile uint cache_lruindex;
static volatile bool cache_getpreflusharr_active = FALSE;

#ifndef SS_MT
static void cache_preflush(
        dbe_cache_t* cache,
        dbe_cacheslot_t* s);
#endif

static dbe_cacheslot_t* cache_selectslot_enterlrumutex(
        dbe_cache_t* cache,
        bool lrulimit,
        int* p_lruindex,
        bool* p_preflushflag);

static void cache_info_init(dbe_cache_info_t* info);

#ifdef SS_BETA

#define SLOT_SET_CLEAN(slot) \
    ss_bprintf_2(("%s %d: setting slot at %ld clean, oldvers=%d dataptr = %ld\n",\
                  __FILE__,__LINE__,\
                  (long)(slot)->slot_daddr,\
                  (int)(slot)->slot_oldvers,\
                  (long)(slot)->slot_data))

#endif /* SS_BETA */

/*##**********************************************************************\
 *
 *		dbe_cache_reachmodetostr
 *
 * Returns string representation of dbe_cache_reachmode_t.
 *
 * Parameters :
 *
 *	mode - in
 *
 *
 * Return value - ref:
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* dbe_cache_reachmodetostr(dbe_cache_reachmode_t mode)
{
        switch (mode) {
            case DBE_CACHE_READONLY:
                return("DBE_CACHE_READONLY");
            case DBE_CACHE_WRITEONLY:
                return("DBE_CACHE_WRITEONLY");
            case DBE_CACHE_READWRITE:
                return("DBE_CACHE_READWRITE");
            case DBE_CACHE_READWRITE_NOCOPY:
                return("DBE_CACHE_READWRITE_NOCOPY");
            case DBE_CACHE_ALLOC:
                return("DBE_CACHE_ALLOC");
            case DBE_CACHE_PREFLUSHREACH:
                return("DBE_CACHE_PREFLUSHREACH");
            case DBE_CACHE_READONLY_IFHIT:
                return("DBE_CACHE_READONLY_IFHIT");
            case DBE_CACHE_READWRITE_IFHIT:
                return("DBE_CACHE_READWRITE_IFHIT");
            case DBE_CACHE_PREFETCH:
                return("DBE_CACHE_PREFETCH");
            default:
                ss_derror;
                return("ERROR!");
        }
}

/*#***********************************************************************\
 *
 *		cache_releasemodetostr
 *
 * Returns string representation of dbe_cache_releasemode_t.
 *
 * Parameters :
 *
 *	mode - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static const char* cache_releasemodetostr(dbe_cache_releasemode_t mode)
{
        switch (mode) {
            case DBE_CACHE_CLEAN:
                return("DBE_CACHE_CLEAN");
            case DBE_CACHE_DIRTY:
                return("DBE_CACHE_DIRTY");
            case DBE_CACHE_IGNORE:
                return("DBE_CACHE_IGNORE");
            case DBE_CACHE_FLUSH:
                return("DBE_CACHE_FLUSH");
            case DBE_CACHE_CLEANLASTUSE:
                return("DBE_CACHE_CLEANLASTUSE");
            case DBE_CACHE_DIRTYLASTUSE:
                return("DBE_CACHE_DIRTYLASTUSE");
            case DBE_CACHE_FLUSHLASTUSE:
                return("DBE_CACHE_FLUSHLASTUSE");
            case DBE_CACHE_PREFLUSH:
                return("DBE_CACHE_PREFLUSH");
            default:
                ss_derror;
                return("ERROR!");
        }
}

#ifdef SS_DEBUG

int dbe_cache_flushignoreall = 0;


/*#***********************************************************************\
 *
 *		slot_check
 *
 * Check that cache page type is in legal range.
 *
 * Parameters :
 *
 *	s -
 *
 *
 *	prefetch -
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
static void slot_check(dbe_cacheslot_t* s, bool prefetch, int line)
{
        if (!s->slot_oldvers) {
            if ((uint)((int)*(dbe_blocktype_t*)(s->slot_data)
                    -  (int)DBE_BLOCK_FREE) >
                (uint)(((int)DBE_BLOCK_LASTNOTUSED - 1)
                       -  (int)DBE_BLOCK_FREE))
            {
                if (prefetch) {
                    dbe_blocktype_t blocktype = DBE_BLOCK_FREECACHEPAGE;
                    DBE_BLOCK_SETTYPE(s->slot_data, &blocktype);
                    ss_dprintf_4(("slot_check:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", s->slot_daddr));
                    crc_debug(s->slot_crc = 0L);
                    crc_debug(su_crc32(s->slot_data,
                                s->slot_cache->cac_blocksize,
                                &s->slot_crc));
                } else {
                    SsDbgPrintf("Failed page address=%lu, called from line %d\n", s->slot_daddr, line);
                    ss_rc_error((int)*(dbe_blocktype_t*)(s->slot_data));
                }
            }
        }
}

/*#***********************************************************************\
 *
 *		slot_print
 *
 * Prints the content of one slot.
 *
 * Parameters :
 *
 *	s - in, use
 *		slot
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void slot_print(dbe_cacheslot_t* s)
{
        SsDbgPrintf("    addr:%ld dirty:%d inuse:%d ptr:%lu\n",
            s->slot_daddr,
            s->slot_dirty,
            s->slot_inuse,
            s);
}

/*#***********************************************************************\
 *
 *		hash_print
 *
 * Prints hash table content.
 *
 * Parameters :
 *
 *	hash - in, use
 *		hash table
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void hash_print(dbe_cache_t* cache)
{
        int i;
        cache_hash_t* hash = &cache->cac_hash;
        dbe_cacheslot_t* slot;

        SsDbgPrintf("hash_print:\n");
        for (i = 0; i < (int) hash->h_tablesize; i++) {
            slot = hash->h_table[i].hb_slot;
            while (slot != NULL) {
                slot_print(slot);
                if (slot == slot->slot_hashnext) {
                    SsDbgPrintf("ERROR! Cycle in the hash table!\n");
                    break;
                }
                slot = slot->slot_hashnext;
            }
        }
}

#endif /* SS_DEBUG */

#ifdef SLOT_QUEUE

/*##**********************************************************************\
 *
 *		dbe_cache_getmeslist
 *
 * Gets reference to message list object
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache object
 *
 * Return value - ref :
 *      pointer to message list object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_meslist_t* dbe_cache_getmeslist(dbe_cache_t* cache)
{
        return (&cache->cac_meslist);
}
/*#***********************************************************************\
 *
 *		cache_slotqlink
 *
 *
 *
 * Parameters :
 *
 *	slot -
 *
 *
 *	p_qticket -
 *
 *
 *	exclusive -
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
static void cache_slotqlink(
        dbe_cacheslot_t* slot,
        cache_slotqticket_t* p_qticket,
        bool exclusive)
{
        ss_dprintf_3(("cache_slotqlink:slot=%ld, p_qticket=%ld, exclusive=%d\n",
            (long)slot, (long)p_qticket, exclusive));

        /* Register to slot wait queue */
        p_qticket->sqt_exclusive = exclusive;
        p_qticket->sqt_mes = su_meslist_mesinit(&slot->slot_cache->cac_meslist);
        p_qticket->sqt_next = NULL;
        if (slot->slot_queue.sq_last != NULL) {
            slot->slot_queue.sq_last->sqt_next = p_qticket;
        } else {
            ss_dassert(slot->slot_queue.sq_first == NULL);
            slot->slot_queue.sq_first = p_qticket;
        }
        slot->slot_queue.sq_last = p_qticket;
}

/*#***********************************************************************\
 *
 *		cache_slotqwait
 *
 *
 *
 * Parameters :
 *
 *	p_qticket -
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
static void cache_slotqwait(dbe_cache_t* cache, cache_slotqticket_t* p_qticket)
{
        ss_dprintf_3(("cache_slotqwait\n"));
        SS_PMON_ADD(SS_PMON_CACHESLOTWAIT);
        ss_profile(if (cache->cac_semdbg_meswait != NULL) cache->cac_semdbg_meswait->sd_waitcnt++;)
        su_mes_wait(p_qticket->sqt_mes);
        su_meslist_mesdone(&cache->cac_meslist, p_qticket->sqt_mes);
}

/*#***********************************************************************\
 *
 *		cache_slotqwakeup
 *
 *
 *
 * Parameters :
 *
 *	slot - use
 *          pointer to cache slot
 *
 *      all - in
 *          TRUE if unconditionally wake up all
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void cache_slotqwakeup(
        dbe_cacheslot_t* slot,
        bool all)
{
        bool exclusive;
        bool first = TRUE;

        ss_dprintf_3(("cache_slotqwakeup:slot=%ld, all=%d\n", (long)slot, all));
        while (slot->slot_queue.sq_first != NULL) {
            cache_slotqticket_t* p_next =
                slot->slot_queue.sq_first->sqt_next;

            exclusive = slot->slot_queue.sq_first->sqt_exclusive;
            ss_dprintf_4(("cache_slotqwakeup:slot->slot_queue.sq_first=%ld, p_next=%ld, exclusive=%d\n",
                (long)slot->slot_queue.sq_first, (long)p_next, exclusive));
            if (all || first || !exclusive) {
                ss_dprintf_4(("cache_slotqwakeup:su_mes_send\n"));
                SET_SLOT_WAKEUP;
                su_mes_send(slot->slot_queue.sq_first->sqt_mes);
                first = FALSE;
                ss_dassert(slot->slot_queue.sq_first != p_next);
                slot->slot_queue.sq_first = p_next;
            }
            if (!all && exclusive) {
                break;
            }
        }
        if (slot->slot_queue.sq_first == NULL) {
            ss_dprintf_4(("cache_slotqwakeup:empty queue\n"));
            slot->slot_queue.sq_last = NULL;
        }
}

/*#***********************************************************************\
 *
 *		CACHE_SLOTQWAKEUP
 *
 *
 *
 * Parameters :
 *
 *	s -
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
#define CACHE_SLOTQWAKEUP(s) \
if ((s)->slot_queue.sq_first != NULL) {\
        cache_slotqwakeup(s, FALSE);\
}

/*#***********************************************************************\
 *
 *		CACHE_SLOTQWAKEUPALL
 *
 *
 *
 * Parameters :
 *
 *	s -
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
#define CACHE_SLOTQWAKEUPALL(s) \
if ((s)->slot_queue.sq_first != NULL) {\
        cache_slotqwakeup(s, TRUE);\
}

#if 0
#undef CACHE_SLOTQWAKEUP
#define CACHE_SLOTQWAKEUP(s) CACHE_SLOTQWAKEUPALL(s)
#endif /* 0 */

#else
#define CACHE_SLOTQWAKEUPALL(s)
#define CACHE_SLOTQWAKEUP(s)
#endif /* SLOT_QUEUE */


/*#**********************************************************************\
 *
 *		hash_init
 *
 * Initializes a hash search structure.
 *
 * Parameters :
 *
 *	hash - out, use
 *		hash table
 *
 *	tablesize - in
 *		hash table size
 *
 *	blocksize - in
 *		memory block size used in hashing
 *
 *      npagemtx - in
 *          number of page mutexes
 *
 *      pagemtxarr - in, use
 *          array of page mutexes
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void hash_init(
        cache_hash_t* hash,
        uint tablesize,
        uint blocksize,
        uint npagemtx,
        SsFlatMutexT* pagemtxarr)
{
        uint i;
        uint j;
        ss_dassert(tablesize > 0);
        ss_dassert(blocksize > 0);

        hash->h_tablesize = tablesize;
        hash->h_blocksize = blocksize;
        hash->h_table = SsMemCalloc(tablesize, sizeof(hashbucket_t));
        for (i = 0, j = 0; i < tablesize; i++) {
#ifdef SS_MT
            hash->h_table[i].hb_mutex = SsFlatMutexGetPtr(pagemtxarr[j]);
            j = (j + 1) % npagemtx;
#else
            hash->h_table[i].hb_mutex = SsSemCreateLocalZeroTimeout(SS_SEMNUM_DBE_CACHE_HASH);
#endif
        }
}

/*#**********************************************************************\
 *
 *		hash_done
 *
 * Releases a hash table.
 *
 * Parameters :
 *
 *	hash - in, take
 *		hash table
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void hash_done(cache_hash_t* hash)
{
#ifndef SS_MT
        uint i;
        for (i = 0; i < hash->h_tablesize; i++) {
            SsSemFree(hash->h_table[i].hb_mutex);
        }
#endif
        SsMemFree(hash->h_table);
}

/*#**********************************************************************\
 *
 *		hashbucket_insert
 *
 * Inserts slot into hash table.
 *
 * Parameters :
 *
 *      bucket - in out, use
 *          hash bucket pointer
 *
 *	s - in out, hold
 *		new slot
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void hashbucket_insert(
        hashbucket_t* bucket,
        dbe_cacheslot_t* s)
{
        /* put new slot to the head of the chain */
        s->slot_hashnext = bucket->hb_slot;
        bucket->hb_slot = s;
#ifdef SS_DEBUG
        {
            dbe_cacheslot_t* t;
            ss_assert(s->slot_daddr != SU_DADDR_NULL);
            for (t = s->slot_hashnext; t != NULL; t = t->slot_hashnext) {
                ss_assert(t->slot_daddr != s->slot_daddr);
            }
        }
#endif
}

/*#**********************************************************************\
 *
 *		hashbucket_remove
 *
 * Removes a slot from a hash bucket.
 *
 * Parameters :
 *
 *      bucket - in out, use
 *          hash bucket pointer
 *
 *	slot - in out, use
 *		slot which is removed
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void hashbucket_remove(
        hashbucket_t* bucket,
        dbe_cacheslot_t* slot)
{
        dbe_cacheslot_t* cur_slot;
        dbe_cacheslot_t* prev_slot;

        ss_dassert(slot != NULL);

        cur_slot = bucket->hb_slot;
        ss_dassert(cur_slot != NULL);

        if (slot == cur_slot) {
            /* the first entry in the chain */
            bucket->hb_slot = slot->slot_hashnext;
        } else {
            /* search slot from the chain */
            do {
                prev_slot = cur_slot;
                cur_slot = cur_slot->slot_hashnext;
                ss_dassert(cur_slot != NULL);
                ss_dassert(cur_slot != cur_slot->slot_hashnext);
            } while (cur_slot != slot);
            /* remove slot from the chain */
            prev_slot->slot_hashnext = cur_slot->slot_hashnext;
        }
        slot->slot_hashnext = NULL;
}

/*#**********************************************************************\
 *
 *		hashbucket_search
 *
 * Searches a slot with address daddr from the hash table.
 *
 * Parameters :
 *
 *      bucket - in out, use
 *          hash bucket pointer
 *
 *	daddr - in
 *		searched address
 *
 * Return value :
 *
 *      NULL, if daddr not found
 *      Pointer to the slot with daddr, of daddr found
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_cacheslot_t* hashbucket_search(
        hashbucket_t* bucket,
        su_daddr_t daddr)
{
        dbe_cacheslot_t* slot;

        slot = bucket->hb_slot;
        while (slot != NULL && slot->slot_daddr != daddr) {
            ss_dassert(slot != slot->slot_hashnext);
            slot = slot->slot_hashnext;
        }
        return(slot);
}

/*#***********************************************************************\
 *
 *		hashbucket_lock
 *
 * Locks hash bucket from other accessors (mutex).
 * If the bucket is locked by another thread, waits till
 * the lock is released before locking
 *
 * Parameters :
 *
 *	bucket - in out, use
 *		pointer to hash bucket
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define hashbucket_lock(bucket) \
        SsZeroTimeoutMutexLock((bucket)->hb_mutex)

/*#***********************************************************************\
 *
 *		hashbucket_lock_nowait
 *
 * Locks hash bucket if it is not already locked by another thread
 *
 * Parameters :
 *
 *	bucket - in out, use
 *		pointer to hash bucket
 *
 * Return value :
 *      TRUE if successful or
 *      FALSE when it was locked by another thread
 *
 * Limitations  :
 *
 * Globals used :
 */
#define hashbucket_lock_nowait(bucket) \
    (SsZeroTimeoutMutexTryLock((bucket)->hb_mutex) == SSSEM_RC_SUCC)

/*#***********************************************************************\
 *
 *		hashbucket_unlock
 *
 * Unlocks a hash bucket
 *
 * Parameters :
 *
 *	bucket - in out, use
 *		pointer to hash bucket
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define hashbucket_unlock(bucket) \
        CACHE_UNLOCK(bucket);\
        SsMutexUnlock((bucket)->hb_mutex)

#define hashbucket_samemutex(bucket1, bucket2) \
        ((bucket1) == (bucket2) \
        || ((bucket1) != NULL && (bucket2) != NULL\
          && (bucket1)->hb_mutex == (bucket2)->hb_mutex))\


SS_INLINE void dbe_cache_slotpmonupdate(dbe_cacheslot_t* s)
{
        if (s->slot_whichtree == 0) {
            /* Storage tree. */
            if (s->slot_level == 0) {
                SS_PMON_ADD(SS_PMON_CACHEWRITESTORAGELEAF);
            } else {
                SS_PMON_ADD(SS_PMON_CACHEWRITESTORAGEINDEX);
            }
        } else if (s->slot_whichtree == 1) {
            /* Bonsai tree. */
            if (s->slot_level == 0) {
                SS_PMON_ADD(SS_PMON_CACHEWRITEBONSAILEAF);
            } else {
                SS_PMON_ADD(SS_PMON_CACHEWRITEBONSAIINDEX);
            }
        }
}

static void slot_dowrite_buf(
        dbe_cache_t* cache,
        ss_byte_t* buf,
        su_daddr_t daddr,
        bool preflush,
        dbe_info_flags_t infoflags)
{
        su_ret_t rc;
        
        su_profile_timer;
        su_profile_start;
        /* NOTE! The following lines is not reentrant, but who cares? */
        if (preflush) {
            cache->cac_info.cachei_npreflush++;
        }
        cache->cac_info.cachei_nwrite++;
        if (infoflags & DBE_INFO_MERGE) {
            SS_PMON_ADD(SS_PMON_MERGEFILEWRITE);
        }
        if (infoflags & DBE_INFO_CHECKPOINT) {
            SS_PMON_ADD(SS_PMON_CHECKPOINTFILEWRITE);
        }
        if (infoflags & DBE_INFO_LRU) {
            SS_PMON_ADD(SS_PMON_CACHELRUWRITE);
        }
        /* end-of-non-reentrant */
       
        rc = su_svf_write(
                cache->cac_svfil,
                daddr,
                buf,
                cache->cac_blocksize);
        ss_bprintf_2(("slot_dowrite_buf:su_svf_write (addr=%ld) retcode = %d\n",
                      (long)daddr, rc));
        su_rc_assert(rc == SU_SUCCESS, rc);
        su_profile_stop("slot_dowrite_raw");
}

        
/*#**********************************************************************\
 *
 *		slot_dowrite
 *
 * Writes slot data onto cache file.
 *
 * This function can be called outside the cache mutex.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	s - in out, use
 *		slot the data of which is written
 *
 *	daddr - in
 *		disk block number of the data
 *
 *      preflush - in
 *          TRUE indicates preflush, FALSE = not preflush
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void slot_dowrite(
        dbe_cache_t* cache,
        dbe_cacheslot_t* s,
        su_daddr_t daddr,
        bool preflush,
        dbe_info_flags_t infoflags)
{
#ifdef CACHE_CRC_CHECK
        FOUR_BYTE_T crc;
#endif /* CACHE_CRC_CHECK */

        ss_dprintf_2(("slot_dowrite:daddr = %ld, file size = %ld, blocktype = %d, dataptr = %ld\n",
            (long)daddr, (long)su_svf_getsize(cache->cac_svfil),
            (int)*(dbe_blocktype_t*)(s->slot_data), (long)s->slot_data));

        ss_dassert(daddr != SU_DADDR_NULL);
        ss_dassert(s->slot_dirty);
        ss_dassert_4(daddr < su_svf_getsize(cache->cac_svfil));

        dbe_cache_slotpmonupdate(s);

        slot_dowrite_buf(
                cache,
                (ss_byte_t*)s->slot_data,
                daddr,
                preflush,
                infoflags);
#ifdef CACHE_CRC_CHECK
        cache_crc_array[daddr] = 0;
        su_crc32(s->slot_data, cache->cac_blocksize, &cache_crc_array[daddr]);
/*  	crc = 0; */
/*  	su_crc32(s->slot_data, cache->cac_blocksize, &crc); */
/*  	printf("%s: crc: %ld, daddr: %ld\n", __FUNCTION__, crc, daddr); */
#endif /* CACHE_CRC_CHECK */
        s->slot_flushctr = cache->cac_flushctr;
        s->slot_preflushreq = FALSE;
        s->slot_dirty = FALSE;
        s->slot_oldvers = FALSE;
}

/*#**********************************************************************\
 *
 *		slot_doread
 *
 * Reads slot data from the given address into memory.
 *
 * This function can be called outside the cache mutex.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	s - in out, use
 *		slot into which the data is read
 *
 *	daddr - in
 *		disk block number of data
 *
 *      prefetch - in
 *          TRUE - prefetch, FALSE not prefetch
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void slot_doread(
        dbe_cache_t* cache,
        dbe_cacheslot_t* s,
        su_daddr_t daddr,
        bool prefetch,
        dbe_info_flags_t infoflags)
{
        su_ret_t rc;
        size_t sizeread;
        su_profile_timer;
#ifdef CACHE_CRC_CHECK
    FOUR_BYTE_T crc;
#endif /* CACHE_CRC_CHECK */

        ss_dprintf_2(("slot_doread:daddr = %ld, file size = %ld\n",
            (long)daddr, (long)su_svf_getsize(cache->cac_svfil)));
        ss_dassert(daddr != SU_DADDR_NULL);

        su_profile_start;

        s->slot_level = -1;
        s->slot_whichtree = -1;

        if (prefetch) {
            cache->cac_info.cachei_nprefetch++;
        }

        if (!dbefile_diskless) {
           rc = su_svf_read(
                       cache->cac_svfil,
                       daddr,
                       s->slot_data,
                       cache->cac_blocksize,
                       &sizeread);
           ss_bprintf_2(("slot_doread:su_svf_read  \
                          retcode=%d, sizeread=%ld, daddr=%ld\n",
                         rc,
                         (long)sizeread,
                         (long)daddr));
           /* tpr 390218, netcopy could fail and cause this error */
           if (sizeread != cache->cac_blocksize) {
              su_informative_exit(__FILE__, __LINE__, DBE_ERR_WRONGSIZE);
           }
        } else {
            memset(s->slot_data, 0, cache->cac_blocksize);
            ss_debug(
                {
                    dbe_blocktype_t blocktype =
                	DBE_BLOCK_FREECACHEPAGE;
                    DBE_BLOCK_SETTYPE(
                	s->slot_data, &blocktype);
                    ss_dprintf_4(("slot_doread:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", daddr));
                }
            );
        }

        ss_dprintf_2(("slot_doread:done, blocktype = %d, dataptr = %ld\n", (int)*(dbe_blocktype_t*)(s->slot_data), (long)s->slot_data));

        s->slot_oldvers = FALSE;
        s->slot_dirty = FALSE;

        /* NOTE! The following line is not reentrant, but who cares? */
        cache->cac_info.cachei_nread++;
        if (infoflags & DBE_INFO_MERGE) {
            SS_PMON_ADD(SS_PMON_MERGEFILEREAD);
        }
        if (infoflags & DBE_INFO_CHECKPOINT) {
            SS_PMON_ADD(SS_PMON_CHECKPOINTFILEREAD);
        }
#ifdef CACHE_CRC_CHECK
    crc = 0;
    su_crc32(s->slot_data, cache->cac_blocksize, &crc);
    if ((daddr < CACHE_CRC_ARRAY_SIZE) && cache_crc_array[daddr] && (cache_crc_array[daddr] != crc)) {
      printf("cache crc error, crc: %ld, cache_crc_array[daddr]: %ld, (daddr: %ld)\n", crc, cache_crc_array[daddr], daddr);
    } else {
      printf("crc ok\n");
    }
#endif /* CACHE_CRC_CHECK */
        crc_debug(s->slot_crc = 0L);
        crc_debug(su_crc32(s->slot_data, cache->cac_blocksize, &s->slot_crc));
        su_profile_stop("slot_doread");
}

#define CACHE_PAGEMTXARR_SIZE(n) \
        ((ss_byte_t*)&(((SsFlatMutexT*)NULL)[n]) -\
         (ss_byte_t*)NULL)

/*##**********************************************************************\
 *
 *		dbe_cache_init
 *
 * Creates a cache
 *
 * Parameters :
 *
 *	svfil - in, hold
 *		split virtual file for the cache
 *          file name.
 *
 *	nblock - in
 *		number of cache blocks
 *
 *	nsem - in
 *		max number of semaphores to use for protecting
 *          cache block operations
 *
 * Return value - give :
 *
 *      cache pointer
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cache_t* dbe_cache_init(
        su_svfil_t* svfil,
        uint nblock,
        uint nsem)
{
        uint i;
        dbe_cacheslot_t* s;
        dbe_cache_t* cache;
        ss_debug(dbe_blocktype_t blocktype = DBE_BLOCK_FREECACHEPAGE;)

        ss_dassert(svfil != NULL);
        ss_dassert(nblock > 0);

#ifdef CACHE_CRC_CHECK
        for (i = 0; i< CACHE_CRC_ARRAY_SIZE; i++) {
            cache_crc_array[i] = 0;
        }
#endif /* CACHE_CRC_CHECK */
        if (nsem > nblock) {
            nsem = nblock;
        }
        ss_dassert(nsem >= 1);
        cache = SsMemAlloc(sizeof(dbe_cache_t));
        ss_dassert(cache != NULL);

        ss_debug(cache->cac_check = DBE_CHK_CACHE);
        cache->cac_nslot = nblock;
        cache->cac_blocksize = su_svf_getblocksize(svfil);
        cache->cac_svfil = svfil;
        cache->cac_readonly = su_svf_isreadonly(svfil);

        for (i = 0; i < PLRU_NLRU; i++) {
            plru_initbuf(&cache->cac_plru[i], i);
            SsFlatMutexInit(&(cache->cac_lrumutex[i]),
                            SS_SEMNUM_DBE_CACHE_LRU);
            ss_debug(cache->cac_plru[i].pl_mutex =
                     SsFlatMutexGetPtr(cache->cac_lrumutex[i]));
        }
#ifdef SLOT_QUEUE
        (void)su_meslist_init(&cache->cac_meslist);
#else
        cache->cac_slotwaitmutex = SsSemCreateLocal(SS_SEMNUM_DBE_CACHE_SLOTWAIT);
#endif
#ifdef SS_MT
        cache->cac_npagemtx = nsem;
        cache->cac_pagemtxarr = SsMemAlloc(CACHE_PAGEMTXARR_SIZE(nsem));
        for (i = 0; i < cache->cac_npagemtx; i++) {
            SsFlatZeroTimeoutMutexInit(&(cache->cac_pagemtxarr[i]),
                                       SS_SEMNUM_DBE_CACHE_HASH);
        }
#endif
        hash_init(
            &cache->cac_hash,
            nblock,
            cache->cac_blocksize,
#ifdef SS_MT
            cache->cac_npagemtx,
            cache->cac_pagemtxarr
#else
            0,
            NULL
#endif
            );
        cache_info_init(&cache->cac_info);

        cache->cac_flushctr = 0L;
        cache->cac_flushflag = FALSE;
        cache->cac_flushpool = NULL;
        cache->cac_flushpos = NULL;
        cache->cac_cacmem = SsCacMemInit(cache->cac_blocksize, nblock);
        cache->cac_preflushmax = 100;
        cache->cac_preflushcallback = (void(*)(void*))NULL;
        cache->cac_preflushctx = NULL;
#if 0
        cache->cac_preflushsamplesize = 10;
        cache->cac_preflushdirtyprc = 50;
#endif /* 0 */
        cache->cac_ndirtyrel = 0L;
        cache->cac_npagerep = 0L;
        cache->cac_preflushportion = (nblock + 9) / 10;
        if (cache->cac_preflushportion < 10) {
            cache->cac_preflushportion = 10;
            if (cache->cac_preflushportion > nblock) {
                cache->cac_preflushportion = nblock / 2;
            }
        } else if (cache->cac_preflushportion > PREFLUSHPORTION_MAX) {
            cache->cac_preflushportion = PREFLUSHPORTION_MAX;
        }
        cache->cac_extpreflushreq = 0;
        for (i = 0; i < nblock; i++) {
            s = SsMemAlloc(sizeof(dbe_cacheslot_t));
            ss_dassert(s != NULL);
            s->slot_daddr = SU_DADDR_NULL;
            s->slot_data = SsCacMemAlloc(cache->cac_cacmem);
            ss_debug(DBE_BLOCK_SETTYPE(s->slot_data, &blocktype));
            s->slot_dirty = FALSE;
#ifdef DBE_NONBLOCKING_PAGEFLUSH
            s->slot_writingtodisk = FALSE;
#endif /* DBE_NONBLOCKING_PAGEFLUSH */        
            s->slot_preflushreq = FALSE;
            s->slot_inuse = 0;
#ifdef SLOT_QUEUE
            s->slot_queue.sq_first = s->slot_queue.sq_last = NULL;
#else
            s->slot_waitmsg = SsMesCreateLocal();
            s->slot_nwait = 0;
#endif
            s->slot_userdata = NULL;
            s->slot_level = -1;
            s->slot_whichtree = -1;
            s->slot_.next = NULL;
            s->slot_.prev = NULL;
            s->slot_hashnext = NULL;
            s->slot_flushctr = cache->cac_flushctr;
            s->slot_cache = cache;
            s->slot_lruindex = i % PLRU_NLRU;
            s->slot_lrulist = NULL;
#ifdef SLOT_REACH_HISTORY
            memset(s->slot_history, '\0', sizeof(s->slot_history));
            s->slot_historypos = 0;
            s->slot_ownerthrid = -1;
            s->slot_ownerstack = NULL;
#endif
            ss_debug(memset(s->slot_debug, '\0', sizeof(s->slot_debug)));
            ss_debug(s->slot_callstack = NULL);
            ss_debug(SsFlatMutexLock(cache->cac_lrumutex[s->slot_lruindex]));
            plru_ignore(&cache->cac_plru[s->slot_lruindex], s);
            ss_debug(SsFlatMutexUnlock(cache->cac_lrumutex[s->slot_lruindex]));
        }
        {
            size_t lru_victimarea_minsize;

            lru_victimarea_minsize = cache->cac_preflushportion / PLRU_NLRU;
            if (lru_victimarea_minsize < 2) {
                lru_victimarea_minsize = 2;
            }
            for (i = 0; i < PLRU_NLRU; i++) {
                plru_setvictimarea_minsize(&cache->cac_plru[i],
                                               lru_victimarea_minsize);
            }
        }
        ss_profile(cache->cac_semdbg_meswait = SsSemDbgAdd(SS_SEMNUM_DBE_CACHE_MESWAIT, (char *)__FILE__, __LINE__, TRUE);)
        ss_profile(cache->cac_semdbg_retry = SsSemDbgAdd(SS_SEMNUM_DBE_CACHE_RETRY, (char *)__FILE__, __LINE__, TRUE);)
        return(cache);
}

/*##**********************************************************************\

 *		dbe_cache_done
 *
 * Closes cache and closes the cache file.
 *
 * Parameters :
 *
 *	cache - in, take
 *		cache pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cache_done(cache)
        dbe_cache_t* cache;
{
        dbe_cacheslot_t* s;
        uint i;

        ss_dprintf_3(("dbe_cache_done\n"));
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        hash_done(&cache->cac_hash);

        for (i = 0; i < PLRU_NLRU; i++) {
            ss_debug(SsFlatMutexLock(cache->cac_lrumutex[i]));
            while ((s = plru_find_victim(&cache->cac_plru[i], FALSE))
                   != NULL)
            {
                ss_dassert(s->slot_inuse == 0);
                if (s->slot_userdata != NULL) {
                    SsMemFree(s->slot_userdata);
                }
                SsCacMemFree(cache->cac_cacmem, s->slot_data);
#ifndef SLOT_QUEUE
                SsMesFree(s->slot_waitmsg);
#endif
                ss_debug(SsMemTrcFreeCallStk(s->slot_callstack));
                SsMemFree(s);
            }
            ss_debug(SsFlatMutexUnlock(cache->cac_lrumutex[i]));
        }
        SsCacMemDone(cache->cac_cacmem);
        for (i = 0; i < PLRU_NLRU; i++) {
            SsFlatMutexDone(cache->cac_lrumutex[i]);
        }
#ifdef SLOT_QUEUE
        su_meslist_done(&cache->cac_meslist);
#else
        SsSemFree(cache->cac_slotwaitmutex);
#endif
        if (cache->cac_preflushctx != NULL) {
            SsMemFree(cache->cac_preflushctx);
        }
        for (i = 0; i < PLRU_NLRU; i++) {
            plru_donebuf(&cache->cac_plru[i]);
        }
        if (cache->cac_flushpool != NULL) {
            su_rbt_done(cache->cac_flushpool);
        }
        ss_debug(cache->cac_check = 0);

#ifdef SS_MT
        for (i = 0; i < cache->cac_npagemtx; i++) {
            SsFlatMutexDone(cache->cac_pagemtxarr[i]);
        }
        SsMemFree(cache->cac_pagemtxarr);
#endif
        SsMemFree(cache);
}

/*#***********************************************************************\
 *
 *		cache_outofslots
 *
 * Prints fatal error message about running out of cache slots
 * and aborts the program.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void cache_outofslots(dbe_cache_t* cache __attribute__ ((unused)))
{
        char debugstring[48];

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        SsSprintf(
            debugstring,
            "/LOG/UNL/TID:%u/NOD/FLU",
            SsThrGetid());
#else
        SsSprintf(
            debugstring,
            "/LOG/LIM:100000000/TID:%u/NOD/FLU",
            SsThrGetid());
#endif
        SsDbgSet(debugstring);

        ss_debug(hash_print(cache));

        su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_OUTOFCACHEBLOCKS_SS,
                SU_DBE_INDEXSECTION,
                SU_DBE_CACHESIZE);
}

#ifdef SS_DEBUG

/*#***********************************************************************\
 *
 *		cacheslot_setctx
 *
 *
 *
 * Parameters :
 *
 *	slot -
 *
 *
 *	ctx -
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
static void cacheslot_setctx(
        dbe_cacheslot_t* slot,
        char* ctx)
{
        uint debugpos = slot->slot_inuse;

        ss_dassert(debugpos > 0);
        debugpos--;
        if (debugpos < CACHE_MAXSLOTCTX) {
            slot->slot_debug[debugpos] = ctx;
        }
}

/*##**********************************************************************\
 *
 *		cacheslot_removectx
 *
 * Removes ctx from cacheslot
 *
 * Parameters :
 *
 *	slot -
 *
 *
 *	ctx -
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
void cacheslot_removectx(
        dbe_cacheslot_t* slot,
        char* ctx)
{
        uint debugpos = slot->slot_inuse;
        uint i;

        ss_dassert(debugpos > 0);
        debugpos = SS_MIN(CACHE_MAXSLOTCTX, debugpos);
        i = debugpos;
        while (i--) {
            if (slot->slot_debug[i] == ctx) {
                uint j;
                for (j = i + 1; j < debugpos; j++) {
                    slot->slot_debug[j-1] = slot->slot_debug[j];
                }
                return;
            }
        }
        /* No more ss_error; */
}

#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *		cache_retrysleep
 *
 * Gives turn to other threads in case of cache reach retry due
 * to deadlock situation, which has been avoided by testing
 * the semaphore without waiting. If > 5 retries has been done
 * the routine will hibernate the thread for a random period
 * of 0.1 - 1.1 seconds to avoid trashing (= more than 1 thread
 * continuously running similar retry loop).
 *
 * Parameters :
 *
 *	retries - in
 *		number of retries done this far.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void cache_retrysleep(uint retries)
{
        if (retries < DBE_BUSYPOLL_MAXLOOP) {
            ss_pprintf_3(("cache_retrysleep:spinloop, retries=%d\n", retries));
        } else if (retries < 2 * DBE_BUSYPOLL_MAXLOOP) {
            ss_pprintf_3(("cache_retrysleep:SsThrSwitch, retries=%d\n", retries));
            SsThrSwitch();
        } else {
            ss_pprintf_3(("cache_retrysleep:SsThrSleep, retries=%d, sleep=%d\n", 
                retries,
                retries < 2 * DBE_BUSYPOLL_MAXSLEEP ? retries : DBE_BUSYPOLL_MAXSLEEP));
            SsThrSleep(retries < 2 * DBE_BUSYPOLL_MAXSLEEP ? retries / 2 : DBE_BUSYPOLL_MAXSLEEP);
        }
}

/*##**********************************************************************\
 *
 *		dbe_cache_reachwithhitinfo
 *
 * Reaches a slot from the cache
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	daddr - in
 *		disk block address
 *
 *	mode - in
 *		reach mode (see dbe_cache_reachmode_t definition in dbe8cach.h)
 *
 *	p_data - out, ref
 *          pointer to variable where the address of slot data will be
 *          stored or NULL when data pointer is not desired
 *
 *      ctx - in, hold
 *          pointer to context to be associated with this reach
 *
 *      p_hit - out, use
 *          pointer to boolean: TRUE means cache hit, FALSE means cache miss
 *
 * Return value - ref :
 *      pointer to reached cache slot
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cacheslot_t* dbe_cache_reachwithhitinfo(
        dbe_cache_t* cache,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx __attribute__ ((unused)),
        bool* p_hit)
{
        dbe_cacheslot_t* slot;
        hashbucket_t* bucket1;
        hashbucket_t* bucket2;
        bool succp;
        bool preflushflag = FALSE;
        bool prefetch;
        uint retries;
        int coming_from_slotwait = 0;
#ifdef SLOT_REACH_HISTORY
        dbe_cache_reachmode_t orig_mode = mode;
#endif
        int lruindex;
        bool nocopy = FALSE;

        SS_PUSHNAME("dbe_cache_reach");

        ss_pprintf_2(("dbe_cache_reach:addr=%ld, mode=%s (%d)\n",
            (long)daddr, dbe_cache_reachmodetostr(mode), (int)mode));

        ss_profile(if (cache->cac_semdbg_meswait != NULL) cache->cac_semdbg_meswait->sd_callcnt++;)
        ss_profile(if (cache->cac_semdbg_retry != NULL) cache->cac_semdbg_retry->sd_callcnt++;)
        ss_debug(*p_hit = -1);
        prefetch = FALSE;
        switch (mode) {
            case DBE_CACHE_PREFETCH:
                prefetch = TRUE;
                mode = DBE_CACHE_READONLY;
                break;
            default:
                break;
        }
        for (retries = 0;;) {
            bucket1 = NULL;
            bucket2 = NULL;
            if (daddr != SU_DADDR_NULL) {
                bucket1 = HASH_BUCKET(&cache->cac_hash, daddr);
                CACHE_LOCK(daddr,bucket1);
                hashbucket_lock(bucket1);
                CACHE_LOCKED(daddr,bucket1);
                slot = hashbucket_search(bucket1, daddr);
                cache->cac_info.cachei_nfind++;
            } else {
                slot = NULL;
            }
            if (slot == NULL) {
                /* Cache miss !
                 */
                ss_dprintf_2(("dbe_cache_reach:cache miss\n"));
                *p_hit = FALSE;
                switch (mode) {
                    case DBE_CACHE_PREFLUSHREACH:
                    case DBE_CACHE_READONLY_IFHIT:
                    case DBE_CACHE_READWRITE_IFHIT:
                        /* These modes cause immediate return if
                         * the page is not in cache!
                         */
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        if (p_data != NULL) {
                            *p_data = NULL;
                        }
                        ss_dprintf_2(("dbe_cache_reach:return NULL\n"));
                        SS_POPNAME;
                        ss_dassert(*p_hit == TRUE || *p_hit == FALSE);
                        return (NULL);
                    default:
                        break;
                }
                /* Select a victim slot from LRU */
                slot = cache_selectslot_enterlrumutex(cache, TRUE, &lruindex, &preflushflag);
                if (slot == NULL) {
                    cache_outofslots(cache);
                }
                /* for diskless, since we can't flush a page to disk,
                 * if we are out of new slot, we are out of slot.
                 */
                if (dbefile_diskless && slot->slot_daddr != SU_DADDR_NULL) {
                    cache_outofslots(cache);
                }
                if (slot->slot_daddr != SU_DADDR_NULL) {
                    /* the victim slot has a hash table entry */
                    SS_PMON_ADD(SS_PMON_CACHESLOTREPLACE);
                    bucket2 = HASH_BUCKET(&cache->cac_hash, slot->slot_daddr);
                    if (!hashbucket_samemutex(bucket2,bucket1)) {
                        CACHE_LOCK(slot->slot_daddr, bucket2);
                        succp = hashbucket_lock_nowait(bucket2);
                        if (!succp) {
                            CACHE_NOTLOCKED(slot->slot_daddr, bucket2);
                            /* The hash bucket is already locked by
                             * another thread!
                             */
                            plru_insert(&cache->cac_plru[lruindex], slot, PLRU_CLASS_LASTUSE);
                            retries++;
                            ss_dprintf_1(("dbe_cache_reach:retry %d, line %d\n", retries, __LINE__));
                            CACHE_LRUUNLOCK();
                            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                            if (bucket1 != NULL) {
                                hashbucket_unlock(bucket1);
                            }
                            ss_profile(if (cache->cac_semdbg_retry != NULL) cache->cac_semdbg_retry->sd_waitcnt++;)
                            cache_retrysleep(retries);
                            continue;   /* retry! */
                        } else {
                            CACHE_LOCKED(slot->slot_daddr, bucket2);
                        }
                    }
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                    if (slot->slot_writingtodisk) {
                        cache_slotqticket_t qticket;

                        ss_dassert(slot->slot_dirty);
                        /* we cannot start the replacement, it is
                           being written to disk! */
                        cache_slotqlink(slot, &qticket, FALSE);
                        if (!hashbucket_samemutex(bucket2,bucket1)) {
                            hashbucket_unlock(bucket2);
                        }
                        plru_insert(
                                &cache->cac_plru[lruindex],
                                slot,
                                PLRU_CLASS_LASTUSE);
                        CACHE_LRUUNLOCK();
                        SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        cache_slotqwait(cache, &qticket);
                        retries++;
                        continue;   /* retry! */
                    }
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                    hashbucket_remove(bucket2, slot);
                    CACHE_LRUUNLOCK();
                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                    if (slot->slot_dirty) {
                        /* Dirty victim slot must be flushed before recycling
                         */
                        infoflags |= DBE_INFO_LRU;
#ifdef SS_MT

                        ss_dprintf_2(("dbe_cache_reach(): flush LRU slot: @%lu\n",
                            slot->slot_daddr));
                        slot_dowrite(cache, slot, slot->slot_daddr,
                                     mode == DBE_CACHE_PREFLUSHREACH,
                                     infoflags);
#else
                        cache_preflush(cache, slot, infoflags);
#endif
                    }
                    if (!hashbucket_samemutex(bucket2,bucket1)) {
                        /* The second hash slot need not be locked anymore */
                        hashbucket_unlock(bucket2);
                    }
                } else {
                    /* The slot has no address; It is not in
                     * any hash bucket
                     */
                    CACHE_LRUUNLOCK();
                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                }
                ss_dassert(slot->slot_inuse == 0);
                if (slot->slot_userdata != NULL) {
                    SsMemFree(slot->slot_userdata);
                    slot->slot_userdata = NULL;
                }
                slot->slot_daddr = daddr;
                ss_dassert(!slot->slot_dirty);
                if (daddr != SU_DADDR_NULL) {
                    /* Insert the recycled slot to new hash position */
                    hashbucket_insert(bucket1, slot);
                    if (mode != DBE_CACHE_WRITEONLY) {
                        /* read contents from disk */
                        slot_doread(cache, slot, daddr, prefetch, infoflags);
                    }
                }
                ss_dprintf_2(("dbe_cache_reach:set slot->slot_inuse=1\n"));
                ss_dassert(slot->slot_lrulist == NULL);
                slot->slot_inuse = 1;
                SET_SLOT_HISTORY(orig_mode, -1);
                SET_SLOT_RETRY;
            } else {
                /* Cache hit ! */
                ss_dprintf_2(("dbe_cache_reach:cache hit\n"));
                SET_SLOT_HISTORY(orig_mode, -1);
                SET_SLOT_RETRY;
                *p_hit = TRUE;
                switch (mode) {
                    case DBE_CACHE_READONLY_IFHIT:
                        mode = DBE_CACHE_READONLY;
                        break;
                    case DBE_CACHE_READWRITE_NOCOPY:
                        nocopy = TRUE;
                        /* FALLTROUGH */
                    case DBE_CACHE_READWRITE_IFHIT:
                        mode = DBE_CACHE_READWRITE;
                        break;
                    default:
                        break;
                }
                if (slot->slot_inuse != 0
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                    || (slot->slot_writingtodisk && mode != DBE_CACHE_READONLY)
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                ) {
                    /* Someone has reached the same slot already */

                    ss_dprintf_2(("dbe_cache_reach:slot->slot_inuse=%d\n", slot->slot_inuse));
#ifndef DBE_NONBLOCKING_PAGEFLUSH
                    ss_dassert(slot->slot_inuse > 0);
#endif /* !DBE_NONBLOCKING_PAGEFLUSH */
                    if (mode == DBE_CACHE_PREFLUSHREACH) {
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        if (p_data != NULL) {
                            *p_data = NULL;
                        }
                        ss_dprintf_2(("dbe_cache_reach:return NULL\n"));
                        SS_POPNAME;
                        ss_dassert(*p_hit == TRUE || *p_hit == FALSE);
                        return (NULL);
                    }
#ifdef SLOT_QUEUE
                    if (slot->slot_rmode != DBE_CACHE_READONLY ||
                        (slot->slot_rmode == DBE_CACHE_READONLY && nocopy) ||
                        (!coming_from_slotwait &&
                         slot->slot_queue.sq_first != NULL)
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                        || (slot->slot_writingtodisk &&
                            mode != DBE_CACHE_READONLY)
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                    ) {
                        /* Slot is write mode accessed or
                         * we want write mode access with slot replication
                         * not allowed; We need to wait!
                         */
                        cache_slotqticket_t qticket;

                        ss_dprintf_2(("dbe_cache_reach(): wait, slot->slot_rmode=%d, slot->slot_queue.sq_first=%ld\n",
                            slot->slot_rmode, (long)slot->slot_queue.sq_first));

#ifdef SLOT_REACH_HISTORY
                        SET_SLOT_WAIT;
                        qticket.sqt_rmode = slot->slot_rmode;
                        qticket.sqt_origmode = orig_mode;
                        qticket.sqt_inuse = slot->slot_inuse;
                        qticket.sqt_thrid = SsThrGetid();
                        qticket.sqt_coming_from_slotwait =  coming_from_slotwait;
                        qticket.sqt_retries = retries;
                        qticket.sqt_daddr = daddr;
#endif

                        cache_slotqlink(
                            slot,
                            &qticket,
                            (mode != DBE_CACHE_READONLY));
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        cache_slotqwait(cache, &qticket);
                        retries++;
                        ss_dprintf_2(("dbe_cache_reach:retry %d, line %d, addr=%ld, slot=%ld, qticket=%ld\n",
                            retries, __LINE__, daddr, (long)slot, (long)&qticket));
                        ss_profile(if (cache->cac_semdbg_retry != NULL) cache->cac_semdbg_retry->sd_waitcnt++;)
                        cache_retrysleep(retries);
                        coming_from_slotwait++;
                        continue;   /* retry! */
                    }
#else /* SLOT_QUEUE */
                    if (slot->slot_rmode != DBE_CACHE_READONLY || (slot->slot_rmode == DBE_CACHE_READONLY && nocopy)) {
                        /* It is write mode access; We need to wait!
                         */
                        /* Register to wait queue of this slot */
                        SsSemEnter(cache->cac_slotwaitmutex);
                        slot->slot_nwait++;
                        SsSemExit(cache->cac_slotwaitmutex);

                        /* Release bucket lock */
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }

                        /* sleep! */
                        SS_PUSHNAME("dbe_cache_reach: wait slot_waitmsg");
                        SsMesWait(slot->slot_waitmsg);
                        SS_POPNAME;
                        /* unregister from wait queue */
                        SsSemEnter(cache->cac_slotwaitmutex);
                        slot->slot_nwait--;
                        if (slot->slot_nwait > 0) {
                            /* Wake up others who wait for this slot */
                            SsMesSend(slot->slot_waitmsg);
                        }
                        retries++;
                        ss_dprintf_1(("dbe_cache_reach:retry %d, line %d\n", retries, __LINE__));
                        SsSemExit(cache->cac_slotwaitmutex);
                        ss_profile(if (cache->cac_semdbg_retry != NULL) cache->cac_semdbg_retry->sd_waitcnt++;)
                        cache_retrysleep(retries);
                        continue;   /* retry! */
                    }
#endif /* SLOT_QUEUE */
                    /* other accessors are readers */
                    if (mode != DBE_CACHE_READONLY) {
                        /* We want write access, so we take a copy of the
                         * slot (slot replication)
                         */
                        dbe_cacheslot_t* copy_slot;
                        copy_slot = cache_selectslot_enterlrumutex(cache, TRUE, &lruindex, &preflushflag);
                        if (copy_slot == NULL) {
                            cache_outofslots(cache);
                        }
                        /* for diskless, since we can't flush a page to disk,
                         * if we are out of new slot, we are out of slot.
                         */
                        if (dbefile_diskless &&
                            copy_slot->slot_daddr != SU_DADDR_NULL)
                        {
                            cache_outofslots(cache);
                        }
                        if (copy_slot->slot_daddr != SU_DADDR_NULL) {
                            bucket2 = HASH_BUCKET(&cache->cac_hash, copy_slot->slot_daddr);
                            if (!hashbucket_samemutex(bucket2,bucket1)) {
                                CACHE_LOCK(copy_slot->slot_daddr, bucket2);
                                succp = hashbucket_lock_nowait(bucket2);
                                if (!succp) {
                                    CACHE_NOTLOCKED(copy_slot->slot_daddr, bucket2);
                                    plru_insert(
                                        &cache->cac_plru[lruindex],
                                        copy_slot,
                                        PLRU_CLASS_LASTUSE);
                                    ss_dprintf_1(("dbe_cache_reach:retry %d, line %d\n", retries, __LINE__));
                                    CACHE_LRUUNLOCK();
                                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                                    if (bucket1 != NULL) {
                                        hashbucket_unlock(bucket1);
                                    }
                                    ss_profile(if (cache->cac_semdbg_retry != NULL) cache->cac_semdbg_retry->sd_waitcnt++;)
                                    cache_retrysleep(retries);
                                    retries++;
                                    continue;   /* retry! */
                                } else {
                                    CACHE_LOCKED(copy_slot->slot_daddr, bucket2);
                                }
                            }
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                            if (copy_slot->slot_writingtodisk) {
                                cache_slotqticket_t qticket;

                                ss_dassert(copy_slot->slot_dirty);
                                /* we cannot start the replacement, it is
                                   being written to disk! */
                                cache_slotqlink(copy_slot, &qticket, FALSE);
                                if (!hashbucket_samemutex(bucket2,bucket1)) {
                                    hashbucket_unlock(bucket2);
                                }
                                plru_insert(
                                        &cache->cac_plru[lruindex],
                                        copy_slot,
                                        PLRU_CLASS_LASTUSE);
                                CACHE_LRUUNLOCK();
                                SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                                if (bucket1 != NULL) {
                                    hashbucket_unlock(bucket1);
                                }
                                cache_slotqwait(cache, &qticket);
                                retries++;
                                continue;   /* retry! */
                            }
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                            hashbucket_remove(bucket2, copy_slot);
                            CACHE_LRUUNLOCK();
                            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                            if (copy_slot->slot_dirty) {
                                infoflags |= DBE_INFO_LRU;
#ifdef SS_MT
                                slot_dowrite(
                                    cache,
                                    copy_slot,
                                    copy_slot->slot_daddr,
                                    mode == DBE_CACHE_PREFLUSHREACH,
                                    infoflags);
#else
                                cache_preflush(cache, copy_slot, infoflags);
#endif
                            }
                            if (!hashbucket_samemutex(bucket2,bucket1)) {
                                hashbucket_unlock(bucket2);
                            }
                        } else {
                            CACHE_LRUUNLOCK();
                            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                        }
                        if (copy_slot->slot_userdata != NULL) {
                            SsMemFree(copy_slot->slot_userdata);
                            copy_slot->slot_userdata = NULL;
                        }
                        copy_slot->slot_daddr = slot->slot_daddr;
                        ss_beta(
                            if (copy_slot->slot_dirty &&
                                !slot->slot_dirty)
                            {
                                SLOT_SET_CLEAN(copy_slot);
                            });
                        copy_slot->slot_dirty = slot->slot_dirty;
                        copy_slot->slot_flushctr = slot->slot_flushctr;
                        crc_debug(copy_slot->slot_crc = slot->slot_crc;)
                        ss_dassert(copy_slot->slot_inuse == 0);
                        ss_dprintf_2(("dbe_cache_reach:set slot->slot_inuse=1\n"));
                        ss_dassert(slot->slot_lrulist == NULL);
                        copy_slot->slot_inuse = 1;
                        if (mode != DBE_CACHE_WRITEONLY) {
                            memcpy(copy_slot->slot_data, slot->slot_data, cache->cac_blocksize);
                        } else {
                            ss_debug(
                                {
                                    dbe_blocktype_t blocktype =
                                        DBE_BLOCK_FREECACHEPAGE;
                                    DBE_BLOCK_SETTYPE(
                                        copy_slot->slot_data, &blocktype);
                                    ss_dprintf_4(("dbe_cache_reach:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", copy_slot->slot_daddr));
                                }
                            );
                            ss_debug(memset(copy_slot->slot_data + 1, 0xCa, cache->cac_blocksize - 1));
                            crc_debug(copy_slot->slot_crc = 0L);
                            crc_debug(su_crc32(copy_slot->slot_data, cache->cac_blocksize, &copy_slot->slot_crc));
                        }

                        ss_pprintf_4(("dbe_cache_reach:set slot_oldvers\n"));
                        slot->slot_oldvers = TRUE;
                        ss_beta(
                            if (slot->slot_dirty) {
                                SLOT_SET_CLEAN(slot);
                            });
                        slot->slot_dirty = FALSE;
                        slot->slot_preflushreq = FALSE;
                        CACHE_SLOTQWAKEUPALL(slot);
                        hashbucket_remove(bucket1, slot);
                        hashbucket_insert(bucket1, copy_slot);
                        slot = copy_slot;
                    } else {
                        /* Read only access allows sharing! */
                        ss_dassert(slot->slot_lrulist == NULL);
                        slot->slot_inuse++;
                        ss_dprintf_2(("dbe_cache_reach:set slot->slot_inuse++=%d\n", slot->slot_inuse));
                    }
                } else {
                    /* The slot is not currently in use, so we just take it
                     */
                    ss_dassert(slot->slot_inuse == 0);
                    if (mode == DBE_CACHE_PREFLUSHREACH
                    &&  (!slot->slot_preflushreq || !slot->slot_dirty))
                    {
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        if (p_data != NULL) {
                            *p_data = NULL;
                        }
                        ss_dprintf_2(("dbe_cache_reach:return NULL\n"));
                        SS_POPNAME;
                        ss_dassert(*p_hit == TRUE || *p_hit == FALSE);
                        return (NULL);
                    }
                    lruindex = slot->slot_lruindex;
                    CACHE_LRULOCK();
                    SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                    CACHE_LRULOCKED();
                    plru_remove(&cache->cac_plru[lruindex], slot);
                    CACHE_LRUUNLOCK();
                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                    ss_dassert(slot->slot_lrulist == NULL);
                    slot->slot_inuse++;
                    ss_dprintf_2(("dbe_cache_reach:set slot->slot_inuse++=%d\n", slot->slot_inuse));
                }
                if (mode == DBE_CACHE_WRITEONLY
                &&  slot->slot_userdata != NULL) 
                {
                    SsMemFree(slot->slot_userdata);
                    slot->slot_userdata = NULL;
                }
            }
#ifdef DBE_IOMGR_FLUSHES_ALONE
            if (!slot->slot_dirty) {
                slot->slot_flushctr = cache->cac_flushctr;
            }
#else /* DBE_IOMGR_FLUSHES_ALONE */
            if (slot->slot_flushctr != cache->cac_flushctr) {
                if (slot->slot_dirty) {
                    /* Slot must be flushed
                    * before it is reached. That is because
                    * concurrent flush is running.
                    */
                    slot_dowrite(cache, slot, daddr, mode == DBE_CACHE_PREFLUSHREACH, infoflags);
                    if (mode == DBE_CACHE_PREFLUSHREACH) {
                        /* It has been flushed already! */
                        slot->slot_inuse--;
                        ss_dprintf_2(("dbe_cache_reach:set slot->slot_inuse--=%d\n", slot->slot_inuse));
                        ss_dassert(slot->slot_inuse == 0);
                        lruindex = slot->slot_lruindex;
                        CACHE_LRULOCK();
                        SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                        CACHE_LRULOCKED();
                        plru_insert(&cache->cac_plru[lruindex],
                                        slot,
                                        PLRU_CLASS_LASTUSE);
                        CACHE_LRUUNLOCK();
                        SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                        if (bucket1 != NULL) {
                            hashbucket_unlock(bucket1);
                        }
                        if (p_data != NULL) {
                            *p_data = NULL;
                        }
                        ss_dprintf_2(("dbe_cache_reach:return NULL\n"));
                        SS_POPNAME;
                        ss_dassert(*p_hit == TRUE || *p_hit == FALSE);
                        return (NULL);
                    }
                } else {
                    slot->slot_flushctr = cache->cac_flushctr;
                }
            }
#endif /* DBE_IOMGR_FLUSHES_ALONE */
            slot->slot_rmode = mode;
            slot->slot_oldvers = FALSE;
            slot->slot_preflushreq = FALSE;
            ss_debug(cacheslot_setctx(slot, ctx));
            ss_debug(SsMemTrcFreeCallStk(slot->slot_callstack));
            ss_debug(slot->slot_callstack = SsMemTrcCopyCallStk());
            if (mode == DBE_CACHE_READONLY) {
                /* Wake up potential exclusive waiter because otherwise
                 * we can create a deadlock.
                 * Scenario:
                 *
                 * T1,T2,T3 are threads.
                 * - T1: Reach block X in read-write mode. Slot returned.
                 * - T2: Reach block X in read-only mode. Start waiting for
                 *   the block, add to the wait queue.
                 *   Wait queue: T2
                 * - T3: Reach block X in read-write mode. Start waiting for
                 *   the block, add to the wait queue in exclusive mode.
                 *   Wait queue: T2, T3.
                 * - T1: Release block X. Wake up waiters in wait list. Wake
                 *   up T2 but not T3 because it requires exclusive access.
                 *   Wait queue: T3.
                 * - T2: Complete reach and return slot.
                 * - T2: Reach block X in read-write mode. Start waiting for
                 *   the block, add to the wait queue in exclusive mode.
                 *   Wait queue: T3, T2.
                 *
                 * Here we have a deadlock because thread T2 has reached
                 * block in read-only mode but does not call release. Only
                 * release wakes up waiters. T2 is running full table scan
                 * and randomly does update using WHERE CURRENT OF.
                 *
                 * Note that the fix does not always guarantee full fairness
                 * because we do not quarantee that readers can reach the
                 * block before exclusive access is granted. Thus readers
                 * may go back to wait list.
                 */
                CACHE_SLOTQWAKEUP(slot);
            }
            SET_SLOT_OWNERTHRID;
            if (bucket1 != NULL) {
                hashbucket_unlock(bucket1);
            }
            ss_dassert(mode != DBE_CACHE_ALLOC || slot->slot_hashnext == NULL);
            if (p_data != NULL) {
                *p_data = slot->slot_data;
            }
            if (preflushflag) {
                ss_dassert(mode != DBE_CACHE_PREFLUSHREACH);
                (*cache->cac_preflushcallback)(
                    cache->cac_preflushctx);
            }
            ss_dprintf_2(("dbe_cache_reach:return slot, blocktype = %d, dataptr = %ld\n", (int)*(dbe_blocktype_t*)(slot->slot_data), (long)slot->slot_data));
            CHK_SLOT(slot, prefetch);
#ifdef SS_DEBUG
            if (dbefile_diskless) {
                crc_debug(slot->slot_crc = 0L;
                          su_crc32(slot->slot_data,
                                   slot->slot_cache->cac_blocksize,
                                   &slot->slot_crc));
            }
#endif /* SS_DEBUG */
            SS_POPNAME;
            ss_dassert(*p_hit == TRUE || *p_hit == FALSE);
            return (slot);
        }
}

dbe_cacheslot_t* dbe_cache_reach(
        dbe_cache_t* cache,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx)
{
        bool hit;
        dbe_cacheslot_t* s;

        s = dbe_cache_reachwithhitinfo(
                cache,
                daddr,
                mode,
                infoflags,
                p_data,
                ctx,
                &hit);
        return (s);
}

void dbe_cache_setslotreadonly(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot)
{
        hashbucket_t* bucket;

        ss_bprintf_2(("dbe_cache_setslotreadonly:addr=%ld, blocktype = %d, dataptr = %ld\n",
            slot->slot_daddr, (int)*(dbe_blocktype_t*)(slot->slot_data), (long)slot->slot_data));
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);
        ss_dassert(cache->cac_nslot > 0);
        ss_dassert(slot->slot_rmode == DBE_CACHE_READWRITE);
        ss_dassert(slot->slot_daddr != SU_DADDR_NULL);

        bucket = HASH_BUCKET(&cache->cac_hash, slot->slot_daddr);

        CACHE_LOCK(slot->slot_daddr, bucket);
        hashbucket_lock(bucket);
        CACHE_LOCKED(slot->slot_daddr, bucket);

        ss_dassert(slot->slot_.prev == NULL);
        ss_dassert(slot->slot_.next == NULL);

        slot->slot_rmode = DBE_CACHE_READONLY;
        CACHE_SLOTQWAKEUPALL(slot);

        hashbucket_unlock(bucket);
}

/*##**********************************************************************\
 *
 *		dbe_cache_release
 *
 * Releases cache slot.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	slot - in, take
 *		cache slot
 *
 *      mode - in
 *		release mode, for different modes, see the header file
 *
 *      ctx - in,  use
 *          pointer to context to be associated with this reach
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cache_release(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        dbe_cache_releasemode_t mode,
        void* ctx __attribute__ ((unused)))
{
        hashbucket_t* bucket = NULL;    /* start value is essential ! */
        bool ignore = FALSE;            /* start value is essential ! */
        int lruindex;
        ss_debug(FOUR_BYTE_T slot_crc;)

        SS_PUSHNAME("dbe_cache_release");

#ifdef DISABLE_LASTUSE
        if (mode == DBE_CACHE_CLEANLASTUSE) {
            mode = DBE_CACHE_CLEAN;
        } else if (mode == DBE_CACHE_DIRTYLASTUSE) {
            mode = DBE_CACHE_DIRTY;
        }
#endif /* DISABLE_LASTUSE */
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);
        ss_dassert(cache->cac_nslot > 0);
        ss_dassert(slot != NULL);
        crc_debug(slot_crc = 0L);
        crc_debug(su_crc32(slot->slot_data, cache->cac_blocksize, &slot_crc));
#ifdef SS_DEBUG
        if (slot->slot_rmode == DBE_CACHE_READONLY) {
            ss_rc_assert(
                mode == DBE_CACHE_CLEAN || mode == DBE_CACHE_CLEANLASTUSE,
                mode);
        }
        if (cache->cac_readonly) {
            ss_rc_assert(
                mode == DBE_CACHE_CLEAN ||
                mode == DBE_CACHE_CLEANLASTUSE ||
                mode == DBE_CACHE_IGNORE,
                mode);
        }
#endif /* SS_DEBUG */
        ss_debug(
            if (mode != DBE_CACHE_IGNORE) {
                CHK_SLOT(slot, FALSE);
            }
        );

        crc_dassert(
            (mode == DBE_CACHE_CLEAN || mode == DBE_CACHE_CLEANLASTUSE) ?
            (slot->slot_crc == slot_crc) : TRUE);
        crc_debug(slot->slot_crc = slot_crc);
        ss_pprintf_2(("dbe_cache_release:addr=%ld, mode=%s (%d), dirty=%d, blocktype=%d, dataptr=%ld, inuse=%d\n",
            slot->slot_daddr, cache_releasemodetostr(mode), (int)mode, slot->slot_dirty,
            (int)*(dbe_blocktype_t*)(slot->slot_data), (long)slot->slot_data, slot->slot_inuse));

        ss_dassert(slot->slot_inuse > 0);
        if (slot->slot_daddr != SU_DADDR_NULL) {
            bucket = HASH_BUCKET(&cache->cac_hash, slot->slot_daddr);
            CACHE_LOCK(slot->slot_daddr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(slot->slot_daddr, bucket);
            SET_SLOT_HISTORY(-1, mode);
            ss_debug(cacheslot_removectx(slot, ctx));
            slot->slot_inuse--;
            ss_dprintf_2(("dbe_cache_release:set slot->slot_inuse--=%d\n", slot->slot_inuse));
            if (slot->slot_oldvers) {
                if (slot->slot_inuse == 0) {
                    ignore = TRUE;
                }
            } else {
                if (mode == DBE_CACHE_FLUSH
                ||  mode == DBE_CACHE_FLUSHLASTUSE
                ||  mode == DBE_CACHE_PREFLUSH)
                {
                    ss_dassert(slot->slot_inuse == 0);
                    /* ss_dassert(mode != DBE_CACHE_PREFLUSH || slot->slot_dirty);
                       This proved to be a wrong assert,
                       because dbe_cache_concurrent_flushstep
                       may write the page out in between.
                    */
                    cache->cac_info.cachei_ndirtyrelease++;
                    if (!slot->slot_dirty) {
                        cache->cac_ndirtyrel++;
                        slot->slot_dirty = TRUE;
                    }
                    slot_dowrite(cache, slot, slot->slot_daddr, mode == DBE_CACHE_PREFLUSH, 0);
                } else if (mode == DBE_CACHE_DIRTY
                       ||  mode == DBE_CACHE_DIRTYLASTUSE)
                {
                    ss_dassert(slot->slot_inuse == 0);
                    cache->cac_info.cachei_ndirtyrelease++;
                    if (!slot->slot_dirty) {
                        cache->cac_ndirtyrel++;
                        slot->slot_dirty = TRUE;
                    }
                } else if (mode == DBE_CACHE_IGNORE) {
                    lruindex = slot->slot_lruindex;
                    CACHE_LRULOCK();
                    SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                    CACHE_LRULOCKED();
                    hashbucket_remove(bucket, slot);
                    if (slot->slot_inuse != 0) {
                        ss_dassert(slot->slot_inuse > 0);
                        ss_pprintf_4(("dbe_cache_reach:set slot_oldvers\n"));
                        slot->slot_oldvers = TRUE;
                    } else {
                        ss_dassert(slot->slot_inuse == 0);
                        slot->slot_daddr = SU_DADDR_NULL;
                        slot->slot_dirty = FALSE;
                        ss_debug(
                            {
                                dbe_blocktype_t blocktype =
                                    DBE_BLOCK_FREECACHEPAGE;
                                DBE_BLOCK_SETTYPE(
                                    slot->slot_data, &blocktype);
                                ss_dprintf_4(("dbe_cache_release:1:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", slot->slot_daddr));
                            }
                        );
                        if (slot->slot_userdata != NULL) {
                            SsMemFree(slot->slot_userdata);
                            slot->slot_userdata = NULL;
                        }
                        plru_ignore(&cache->cac_plru[lruindex], slot);
                    }
                    ss_beta(
                        if (slot->slot_dirty) {
                            SLOT_SET_CLEAN(slot);
                        });
                    slot->slot_dirty = FALSE;
                    slot->slot_preflushreq = FALSE;
                    CACHE_SLOTQWAKEUPALL(slot);
                    CACHE_LRUUNLOCK();
                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                    if (bucket != NULL) {
                        hashbucket_unlock(bucket);
                    }
                    SS_POPNAME;
                    ss_dprintf_2(("dbe_cache_release:return1\n"));
                    return;
                } else {
                    ss_dassert(mode == DBE_CACHE_CLEAN
                            || mode == DBE_CACHE_CLEANLASTUSE);
                }
            }
        } else {
            ss_debug(cacheslot_removectx(slot, ctx));
            slot->slot_inuse--;
            ss_dprintf_2(("dbe_cache_release:set slot->slot_inuse--=%d\n", slot->slot_inuse));
            ss_dassert(slot->slot_inuse == 0);
            ignore = TRUE;
        }
        if (ignore) {
            lruindex = slot->slot_lruindex;
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            ss_beta(
                if (slot->slot_dirty) {
                    SLOT_SET_CLEAN(slot);
                });
            slot->slot_daddr = SU_DADDR_NULL;
            slot->slot_dirty = FALSE;
            slot->slot_preflushreq = FALSE;
            ss_debug(
                {
                    dbe_blocktype_t blocktype =
                        DBE_BLOCK_FREECACHEPAGE;
                    DBE_BLOCK_SETTYPE(
                        slot->slot_data, &blocktype);
                    ss_dprintf_4(("dbe_cache_release:2:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", slot->slot_daddr));
                }
            );
            if (slot->slot_userdata != NULL) {
                SsMemFree(slot->slot_userdata);
                slot->slot_userdata = NULL;
            }
            plru_ignore(&cache->cac_plru[lruindex], slot);
            CACHE_SLOTQWAKEUPALL(slot);
            CACHE_LRUUNLOCK();
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
            if (bucket != NULL) {
                hashbucket_unlock(bucket);
            }
            SS_POPNAME;
            ss_dprintf_2(("dbe_cache_release:return2\n"));
            return;
        }
        CHK_SLOT(slot, FALSE);
        if (slot->slot_inuse == 0) {
            plru_class_t pclass;

#ifdef SLOT_QUEUE
            if (slot->slot_oldvers) {
                CACHE_SLOTQWAKEUPALL(slot);
            } else {
                CACHE_SLOTQWAKEUP(slot);
            }
#else
            if (slot->slot_rmode == DBE_CACHE_WRITEONLY
            ||  slot->slot_rmode == DBE_CACHE_READWRITE
            ||  slot->slot_rmode == DBE_CACHE_PREFLUSHREACH)
            {
                SsMesSend(slot->slot_waitmsg);
            }
#endif
            /* Nobody else accesses the slot,
             * the slot must be put to LRU queue
             */
            switch (mode) {
                case DBE_CACHE_CLEAN:
                case DBE_CACHE_DIRTY:
                case DBE_CACHE_FLUSH:
                    pclass = PLRU_CLASS_NORMAL;
                    break;
                case DBE_CACHE_CLEANLASTUSE:
                case DBE_CACHE_DIRTYLASTUSE:
                case DBE_CACHE_FLUSHLASTUSE:
                    pclass = PLRU_CLASS_LASTUSE;
                    break;
                case DBE_CACHE_PREFLUSH:
                    pclass = PLRU_CLASS_IGNORE;
                    break;
                default:
                    ss_rc_error(mode);
            }
            lruindex = slot->slot_lruindex;
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            plru_insert(&cache->cac_plru[lruindex], slot, pclass);
            CACHE_LRUUNLOCK();
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
        } else {
            /* Slot is still in use by another reader.
             * The other reader is responsible for relinking
             * the slot back to LRU
             */
            ss_dprintf_4(("dbe_cache_release:slot->slot_inuse=%d\n", slot->slot_inuse));
            ss_dassert(slot->slot_rmode == DBE_CACHE_READONLY);
        }
        if (bucket != NULL) {
            hashbucket_unlock(bucket);
        }
        SS_POPNAME;
        ss_dprintf_2(("dbe_cache_release:return3\n"));
}

/*##**********************************************************************\
 *
 *		dbe_cache_relocate
 *
 * Changes the slot address in the cache.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	slot - in out, use
 *		cache slot pointer
 *
 *	newaddr - in
 *		new slot address
 *
 *	p_data - out, ref
 *          pointer to variable where the address of slot data will be
 *          stored or NULL when data pointer is not desired
 *
 *
 * Return value - ref:
 *      pointer to slot (may be dirrerent from the 'slot' argument)
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cacheslot_t* dbe_cache_relocate(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        su_daddr_t newaddr,
        char** p_data,
        dbe_info_flags_t infoflags)
{
        hashbucket_t* bucket;
        dbe_cacheslot_t* oldslot_at_newaddr;
        dbe_cacheslot_t* origslot = NULL;

        SS_PUSHNAME("dbe_cache_relocate");

        ss_bprintf_1(("dbe_cache_relocate:oldaddr=%ld, newaddr=%ld\n",
                      (long)slot->slot_daddr, (long)newaddr));
        ss_dassert(newaddr != SU_DADDR_NULL);
        ss_dassert(slot->slot_inuse == 1);
        ss_dassert(slot->slot_rmode != DBE_CACHE_READONLY);
        crc_debug(if (slot->slot_rmode != DBE_CACHE_ALLOC) {
                    FOUR_BYTE_T crc = 0;
                    su_crc32(slot->slot_data,
                           slot->slot_cache->cac_blocksize,
                           &crc);
                    ss_assert(crc == slot->slot_crc);
                  });
        /* for diskless, save the original slot in the cache so that we don't
         * have to fetch the page from the disk. Reading from dbfile in
         * diskless will cause assertion.
         */
        if (dbefile_diskless) {
            if (slot->slot_daddr != SU_DADDR_NULL) {
                origslot = dbe_cache_alloc(cache, NULL);
                if (origslot == NULL) {
                    cache_outofslots(cache);
                }
                memcpy(origslot->slot_data, slot->slot_data, cache->cac_blocksize);
                origslot->slot_rmode = slot->slot_rmode;
                origslot->slot_daddr = slot->slot_daddr;
                crc_debug(origslot->slot_crc = 0;
                          su_crc32(origslot->slot_data,
                                   origslot->slot_cache->cac_blocksize,
                                   &origslot->slot_crc););
            } else {
                origslot = NULL;
            }
        }
#ifdef DBE_IOMGR_FLUSHES_ALONE
        else {
            FAKE_IF(FAKE_DBE_PAGERELOCATE_MOVE_SLOT) {
                 dbe_cacheslot_t* copyslot;

                 if (slot->slot_daddr == SU_DADDR_NULL) {
                     goto normal_relocate;
                 }
                 copyslot = dbe_cache_alloc(cache, NULL);
                 memcpy(copyslot->slot_data, slot->slot_data, cache->cac_blocksize);
                 ss_dassert(!copyslot->slot_oldvers);
                 ss_dassert(copyslot->slot_daddr == SU_DADDR_NULL);
                 copyslot->slot_rmode = slot->slot_rmode;
                 SsDbgPrintf("FAKE_DBE_PAGERELOCATE_MOVE_SLOT: "
                             "slot=0x%08lX, copyslot=0x%08lX, "
                             "slot_daddr=%ld, newaddr=%ld, slottype=%d\n",
                             (ulong)slot, (ulong)copyslot,
                             (long)slot->slot_daddr, (long)newaddr,
                             (int)(ss_byte_t)slot->slot_data[0]);
                 if (slot->slot_dirty) {
                     hashbucket_t* bucket;
                     bucket = HASH_BUCKET(&cache->cac_hash, slot->slot_daddr);
                     hashbucket_lock(bucket);
                     slot_dowrite(cache,
                                  slot,
                                  slot->slot_daddr,
                                  FALSE,
                                  infoflags);
                     hashbucket_unlock(bucket);
                 }
                 memset(slot->slot_data,
                        0xFB, /* ForBidden */
                        cache->cac_blocksize);
                 dbe_cache_release(cache, slot, DBE_CACHE_IGNORE, NULL);
                 slot = copyslot;
            } else {
        normal_relocate:;
                ss_bprintf_2(("dbe_cache_relocate: slot at %ld dirty=%d\n",
                              (long)slot->slot_daddr, (int)slot->slot_dirty));
                if (slot->slot_dirty) {
                    dbe_cacheslot_t* copyslot;
                    

                    ss_dassert(slot->slot_daddr != SU_DADDR_NULL);
                    copyslot = dbe_cache_alloc(cache, NULL);
                    memcpy(copyslot->slot_data, slot->slot_data, cache->cac_blocksize);

                    if (slot->slot_userdata != NULL) {
                        ss_autotest(memset(slot->slot_userdata,
                                           0xDe, DBE_BNODE_SIZE_ATLEAST));
                        SsMemFree(slot->slot_userdata);
                        slot->slot_userdata = NULL;
                    }
                    ss_dassert(!copyslot->slot_oldvers);
                    ss_dassert(copyslot->slot_daddr == SU_DADDR_NULL);
                    copyslot->slot_rmode = slot->slot_rmode;
                    dbe_cache_release(cache, slot, DBE_CACHE_CLEAN, NULL);
                    slot = copyslot;
                }
            }
        }
#endif /* DBE_IOMGR_FLUSHES_ALONE */
        bucket = NULL;

        if (slot->slot_daddr != SU_DADDR_NULL) {
            bucket = HASH_BUCKET(&cache->cac_hash, slot->slot_daddr);
            CACHE_LOCK(slot->slot_daddr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(slot->slot_daddr, bucket);
            SET_SLOT_HISTORY(-1, -1);
            SET_SLOT_RELOCATE;
#ifndef DBE_IOMGR_FLUSHES_ALONE
            if (slot->slot_dirty) {
                slot_dowrite(cache, slot, slot->slot_daddr, FALSE, infoflags);
            }
#endif /* !DBE_IOMGR_FLUSHES_ALONE */
            ss_dassert(dbefile_diskless || !slot->slot_dirty);
            /* Perhaps someone is waiting for a slot at
             * the old address; wake em up!
             */
#ifdef SLOT_QUEUE
            CACHE_SLOTQWAKEUPALL(slot);
#else
            if (slot->slot_rmode == DBE_CACHE_WRITEONLY
            ||  slot->slot_rmode == DBE_CACHE_READWRITE
            ||  slot->slot_rmode == DBE_CACHE_PREFLUSHREACH)
            {
                SsMesSend(slot->slot_waitmsg);
            }
#endif
            if (!slot->slot_oldvers) {
                ss_dassert(hashbucket_search(bucket, slot->slot_daddr) == slot);
                hashbucket_remove(bucket, slot);
            } else {
                ss_dassert(hashbucket_search(bucket, slot->slot_daddr) != slot);
                ss_derror;
            }
            hashbucket_unlock(bucket);
        }
        /* Now we have a cache slot that is removed from original
         * position. It can be moved to its new location
         */
        slot->slot_dirty = TRUE;
        slot->slot_oldvers = FALSE;
        slot->slot_daddr = newaddr;
        bucket = HASH_BUCKET(&cache->cac_hash, newaddr);
        CACHE_LOCK(newaddr, bucket);
        hashbucket_lock(bucket);
        CACHE_LOCKED(newaddr, bucket);
        oldslot_at_newaddr = hashbucket_search(bucket, newaddr);
        if (oldslot_at_newaddr != NULL) {
            if (oldslot_at_newaddr->slot_inuse != 0) {
                /* This situation used to be illegal,
                 * but it had to be legalized, because
                 * prefetch thread may hold the slot.
                 */
                /* Remove old slot from hash bucket */
                hashbucket_remove(bucket, oldslot_at_newaddr);
                ss_beta(
                    if (oldslot_at_newaddr->slot_dirty) {
                        SLOT_SET_CLEAN(oldslot_at_newaddr);
                    });
                oldslot_at_newaddr->slot_oldvers = TRUE;
                oldslot_at_newaddr->slot_dirty = FALSE;
                oldslot_at_newaddr->slot_preflushreq = FALSE;
            } else {
                int lruindex;
                lruindex = oldslot_at_newaddr->slot_lruindex;
                CACHE_LRULOCK();
                SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                CACHE_LRULOCKED();
                /* Remove old slot from hash bucket */
                hashbucket_remove(bucket, oldslot_at_newaddr);
                if (oldslot_at_newaddr->slot_userdata != NULL) {
                    SsMemFree(oldslot_at_newaddr->slot_userdata);
                    oldslot_at_newaddr->slot_userdata = NULL;
                }
                ss_beta(
                    if (oldslot_at_newaddr->slot_dirty) {
                        SLOT_SET_CLEAN(oldslot_at_newaddr);
                    });
                oldslot_at_newaddr->slot_daddr = SU_DADDR_NULL;
                oldslot_at_newaddr->slot_dirty = FALSE;
                oldslot_at_newaddr->slot_preflushreq = FALSE;
                plru_remove(&cache->cac_plru[lruindex],
                                oldslot_at_newaddr);
                plru_ignore(&cache->cac_plru[lruindex],
                                oldslot_at_newaddr);
                CACHE_LRUUNLOCK();
                SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
            }
        }
        hashbucket_insert(bucket, slot);
        hashbucket_unlock(bucket);
        if (p_data != NULL) {
            *p_data = slot->slot_data;
        }

        /* put the old slot in the hash bucket. */
        if (dbefile_diskless && origslot != NULL) {
            bucket = HASH_BUCKET(&cache->cac_hash, origslot->slot_daddr);
            CACHE_LOCK(origslot->slot_daddr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(origslot->slot_daddr, bucket);
            hashbucket_insert(bucket, origslot);
            hashbucket_unlock(bucket);

            dbe_cache_release(cache, origslot, DBE_CACHE_CLEAN, NULL);
        }
        SS_POPNAME;
        ss_dprintf_2(("dbe_cache_relocate:return\n"));
        return (slot);
}


/*##**********************************************************************\
 *
 *		dbe_cache_setpageaddr
 *
 * assigns a disk address to a slot that has been purchased using
 * dbe_cache_alloc()
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	slot - in out, use
 *		cache slot pointer
 *
 *	newaddr - in
 *		new slot address
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cache_setpageaddress(
        dbe_cache_t* cache,
        dbe_cacheslot_t* slot,
        su_daddr_t newaddr)
{
        dbe_cacheslot_t* newslot;
        ss_dassert(slot->slot_daddr == SU_DADDR_NULL);
        ss_dassert(slot->slot_rmode == DBE_CACHE_ALLOC);
        newslot = dbe_cache_relocate(cache, slot, newaddr, NULL, 0);
        ss_dassert(newslot == slot);
        slot->slot_rmode = DBE_CACHE_WRITEONLY;
        slot->slot_flushctr = cache->cac_flushctr;
}



/*##**********************************************************************\
 *
 *		dbe_cache_flush
 *
 * Flushes all cache blocks onto disk. If some of the blocks are in use,
 * they cannot be flushed and the function returns FALSE, Otherwise
 * function returns TRUE.
 *
 * Also the split virtual file (su_svfil_t) associated to the cache is
 * flushed.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 * Return value :
 *
 *      TRUE    - all cache blocks flushed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cache_flush(cache)
        dbe_cache_t* cache;
{
        bool incomplete;

        ss_dprintf_2(("dbe_cache_flush\n"));
        dbe_cache_concurrent_flushinit(cache);
        incomplete = dbe_cache_concurrent_flushstep(cache, cache->cac_nslot, 0);
        ss_dassert(!incomplete);
        return(TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_cache_alloc
 *
 * Allocates a block from the cache.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *      p_data - out, ref
 *		pointer to the variable into where the data pointer
 *          of the slot is stored, if p_data is not NULL
 *
 * Return value - ref :
 *
 *      pointer to the cache slot, the actual memory pointer can
 *      be retrieved using function dbe_cacheslot_getdata
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_cacheslot_t* dbe_cache_alloc(
        dbe_cache_t* cache,
        char** p_data)
{
        ss_dprintf_2(("dbe_cache_alloc\n"));

        return(dbe_cache_reach(
                    cache,
                    SU_DADDR_NULL,
                    DBE_CACHE_ALLOC,
                    0,
                    p_data,
                    cache_alloc_ctx));
}

/*##**********************************************************************\
 *
 *		dbe_cache_free
 *
 * Frees cache slot allocated using dbe_cache_alloc.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	slot - in, take
 *		cache slot
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cache_free(cache, slot)
        dbe_cache_t* cache;
        dbe_cacheslot_t* slot;
{
        ss_dprintf_2(("dbe_cache_free\n"));

        dbe_cache_release(cache, slot, DBE_CACHE_IGNORE, cache_alloc_ctx);
}

/*##**********************************************************************\
 *
 *		dbe_cache_getfilesize
 *
 * Returns the split virtual file size of the cache in blocks.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 * Return value :
 *
 *      cache file size in blocks
 *
 * Limitations  :
 *
 * Globals used :
 */
su_daddr_t dbe_cache_getfilesize(cache)
        dbe_cache_t* cache;
{
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        return(su_svf_getsize(cache->cac_svfil));
}

/*##**********************************************************************\
 *
 *		dbe_cache_getblocksize
 *
 * Returns the cache block size in bytes.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 * Return value :
 *
 *      block size in bytes
 *
 * Limitations  :
 *
 * Globals used :
 */
uint dbe_cache_getblocksize(cache)
        dbe_cache_t* cache;
{
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        return(cache->cac_blocksize);
}

/*##**********************************************************************\
 *
 *		dbe_cache_getsvfil
 *
 * Returns the split virtual file onto which the cache is stored.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 * Return value - ref :
 *
 *      pointer to the split virtual file
 *
 * Limitations  :
 *
 * Globals used :
 */
su_svfil_t* dbe_cache_getsvfil(cache)
        dbe_cache_t* cache;
{
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        return(cache->cac_svfil);
}

/*#**********************************************************************\
 *
 *		dbe_cache_getinfo_internal
 *
 * Same as dbe_cache_getinfo but this function is for internal use
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	info - out, use
 *		pointer to the cache info structure into where the info
 *          is stored
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_cache_getinfo_internal(
        dbe_cache_t* cache,
        dbe_cache_info_t* info,
        bool quickp)
{
        uint i;
        int n;
        dbe_cacheslot_t* slot;
        hashbucket_t* bucket;

        if (cache->cac_info.cachei_nfind > 0) {
            cache->cac_info.cachei_readhitrate =
                (double)100.0 * (cache->cac_info.cachei_nfind - cache->cac_info.cachei_nread)
                / cache->cac_info.cachei_nfind;
            if (cache->cac_info.cachei_ndirtyrelease > 0) {
                cache->cac_info.cachei_writeavoidrate =
                    (double)100.0 *
                    (cache->cac_info.cachei_ndirtyrelease - cache->cac_info.cachei_nwrite)
                    / cache->cac_info.cachei_ndirtyrelease;
            } else {
                cache->cac_info.cachei_writeavoidrate = (double)100.0;
            }
            cache->cac_info.cachei_writeperfind =
                (double)100.0 * cache->cac_info.cachei_nwrite / cache->cac_info.cachei_nfind;
            if (cache->cac_info.cachei_nread > 0) {
                cache->cac_info.cachei_writeperread =
                    (double)100.0 * cache->cac_info.cachei_nwrite / cache->cac_info.cachei_nread;
            } else {
                cache->cac_info.cachei_writeperread = (double)0.0;
            }
        } else {
            cache->cac_info.cachei_readhitrate = (double)0.0;
            cache->cac_info.cachei_writeavoidrate = (double)0.0;
            cache->cac_info.cachei_writeperfind = (double)0.0;
            cache->cac_info.cachei_writeperread = (double)0.0;
        }

        cache->cac_info.cachei_minchain = INT_MAX;
        cache->cac_info.cachei_maxchain = 0;
        cache->cac_info.cachei_avgchain = (double)0.0;
        cache->cac_info.cachei_nslot = cache->cac_hash.h_tablesize;
        cache->cac_info.cachei_nchain = 0;
        cache->cac_info.cachei_nitem = 0;
        cache->cac_info.cachei_ndirty = 0;

        if (quickp) {
            cache->cac_info.cachei_nchain = 0;
            cache->cac_info.cachei_nitem = 0;
            cache->cac_info.cachei_ndirty = 0;
            cache->cac_info.cachei_minchain = 0;
            cache->cac_info.cachei_maxchain = 0;
        } else {
            for (i = 0; i < cache->cac_hash.h_tablesize; i++) {
                bucket = &cache->cac_hash.h_table[i];
                CACHE_LOCK(i, bucket);
                hashbucket_lock(bucket);
                CACHE_LOCKED(i, bucket);
                slot = bucket->hb_slot;
                if (slot != NULL) {
                    cache->cac_info.cachei_nchain++;
                    n = 0;
                    do {
                        cache->cac_info.cachei_nitem++;
                        if (slot->slot_dirty) {
                            cache->cac_info.cachei_ndirty++;
                        }
                        n++;
                        slot = slot->slot_hashnext;
                    } while (slot != NULL);
                    cache->cac_info.cachei_minchain = SS_MIN(cache->cac_info.cachei_minchain, n);
                    cache->cac_info.cachei_maxchain = SS_MAX(cache->cac_info.cachei_maxchain, n);
                }
                hashbucket_unlock(bucket);
            }
        }

        if (cache->cac_info.cachei_nchain > 0) {
            cache->cac_info.cachei_avgchain =
                (double)cache->cac_info.cachei_nitem /
                (double)cache->cac_info.cachei_nchain;
        } else {
            cache->cac_info.cachei_avgchain = (double)0.0;
        }

        *info = cache->cac_info;
}

/*#***********************************************************************\
 *
 *		cache_info_init
 *
 *
 *
 * Parameters :
 *
 *	info -
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
static void cache_info_init(dbe_cache_info_t* info)
{
        info->cachei_nfind = 0;
        info->cachei_nread = 0;
        info->cachei_nwrite = 0;
        info->cachei_nprefetch = 0;
        info->cachei_npreflush = 0;
        info->cachei_ndirtyrelease = 0;
}

/*##**********************************************************************\
 *
 *		dbe_cache_getinfo
 *
 * Updates the cache info structure. The info structure must be the same
 * given in dbe_cache_setinfo.
 *
 * Parameters :
 *
 *	cache - in, use
 *		cache pointer
 *
 *	info - out, use
 *		pointer to the cache info structure into where the info
 *          is stored
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_cache_getinfo(cache, info)
        dbe_cache_t* cache;
        dbe_cache_info_t* info;
{
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        dbe_cache_getinfo_internal(cache, info, TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_cacheslot_getdata
 *
 * Returns the data block pointer associated to the cache slot. The number
 * of bytes available through the returned pointer is the same as the
 * cache block size.
 *
 * Parameters :
 *
 *	slot - in, use
 *		cache slot pointer
 *
 * Return value - ref :
 *
 *      pointer to the data area of the slot
 *
 * Limitations  :
 *
 * Globals used :
 */
void* dbe_cacheslot_getdata(slot)
        dbe_cacheslot_t* slot;
{
        ss_dassert(slot != NULL);
        ss_dassert(slot->slot_inuse);

        return(slot->slot_data);
}

/*##**********************************************************************\
 *
 *		dbe_cacheslot_getuserdata
 *
 * Returns the user data pointer of the cache slot.
 *
 * Parameters :
 *
 *	slot - in, use
 *		cache slot pointer
 *
 * Return value :
 *
 *      pointer to the user data of the slot
 *
 * Limitations  :
 *
 * Globals used :
 */
void* dbe_cacheslot_getuserdata(slot)
        dbe_cacheslot_t* slot;
{
        hashbucket_t* bucket;

        ss_dassert(slot != NULL);
        ss_dassert(slot->slot_inuse);

        if (slot->slot_daddr != SU_DADDR_NULL) {
            void *userdata;

            bucket =
                HASH_BUCKET(&slot->slot_cache->cac_hash, slot->slot_daddr);
            CACHE_LOCK(slot->slot_daddr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(slot->slot_daddr, bucket);
            userdata = slot->slot_userdata;
            hashbucket_unlock(bucket);
            return (userdata);
        }
        /* Slot has no disk address, and cannot have
         * simultaneous users
         */
        ss_dassert(slot->slot_inuse == 1);
        return(slot->slot_userdata);
}

/*##**********************************************************************\
 *
 *		dbe_cacheslot_getdaddr
 *
 * Gets disk address of a cache slot
 *
 * Parameters :
 *
 *	slot - in, use
 *		pointer to cache slot
 *
 * Return value :
 *          disk address of the slot
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_daddr_t dbe_cacheslot_getdaddr(
        dbe_cacheslot_t* slot)
{
        return (slot->slot_daddr);
}

/*##**********************************************************************\
 *
 *		dbe_cacheslot_setuserdata
 *
 * Sets the user data pointer of the cache slot. The user data must be
 * a pointer to a memory area allocated with SsMemAlloc. The memory area
 * must be also flat, that is, it cannot contain pointers to other
 * memory areas that should be released with the user data pointer.
 * The cache system will free the user data pointer when the slot data
 * inside the slot changes.
 *
 * Parameters :
 *
 *	slot - in out, use
 *		cache slot pointer
 *
 *	userdata - in, hold
 *		user data pointer
 *
 * Return value :
 *      TRUE when succesful or
 *      FALSE when the slot already has user data
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_cacheslot_setuserdata(
        dbe_cacheslot_t* slot,
        void* userdata,
        int level,
        bool bonsaip)
{
        hashbucket_t* bucket;

        ss_dassert(slot != NULL);
        ss_dassert(slot->slot_inuse);

        if (slot->slot_daddr != SU_DADDR_NULL) {
            bool succp;

            bucket =
                HASH_BUCKET(&slot->slot_cache->cac_hash, slot->slot_daddr);
            CACHE_LOCK(slot->slot_daddr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(slot->slot_daddr, bucket);
            if (slot->slot_userdata == NULL) {
                succp = TRUE;
                slot->slot_userdata = userdata;
            } else {
                succp = FALSE;
            }
            slot->slot_level = level;
            slot->slot_whichtree = bonsaip;
            hashbucket_unlock(bucket);
            return (succp);
        }
        /* Slot has no disk address, and can not have
         * user data!
         */
        ss_dassert(slot->slot_userdata == NULL);
        slot->slot_userdata = userdata;
        slot->slot_level = level;
        slot->slot_whichtree = bonsaip;
        return (TRUE);
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *		dbe_cache_checkafterflush
 *
 * Checks cache for consistency after flush (checkpoint).
 *
 * Parameters :
 *
 *	cache -
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
bool dbe_cache_checkafterflush(
        dbe_cache_t* cache)
{
        cache_hash_t* hash;
        dbe_cacheslot_t* s;
        hashbucket_t* bucket;
        uint i;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        ss_dprintf_2(("dbe_cache_checkafterflush()\n"));
        if (!dbefile_diskless) {
            hash = &cache->cac_hash;
            
            for (i = 0; i < hash->h_tablesize; i++) {
                
                bucket = &hash->h_table[i];
                CACHE_LOCK(i, bucket);
                hashbucket_lock(bucket);
                CACHE_LOCKED(i, bucket);
                for (s = bucket->hb_slot; s != NULL; s = s->slot_hashnext) {
                    ss_assert(s->slot_daddr != SU_DADDR_NULL);
                    ss_assert(s->slot_flushctr == cache->cac_flushctr);
                }
                hashbucket_unlock(bucket);
            }
        }
        return (TRUE);
}

/*#***********************************************************************\
 *
 *		cache_checkflushslot
 *
 * Checks that slot is consistent with flushing algorithm. The slot
 * cannot be dirty and reached for writing if it is about to be flushed
 * by during checkpoint flush.
 *
 * Parameters :
 *
 *	s -
 *
 *
 *	initp -
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
static bool cache_checkflushslot(
        dbe_cacheslot_t* s,
        bool initp __attribute__ ((unused)))
{
        if (s->slot_inuse == 0) {
            return(TRUE);
        }

        if (s->slot_inuse <= CACHE_MAXSLOTCTX) {
            char* p;
            p = s->slot_debug[s->slot_inuse-1];
            if (p == NULL || *p == 'x') {
                return(TRUE);
            }
        }
#ifdef DBE_IOMGR_FLUSHES_ALONE
        return (TRUE);
#else /* DBE_IOMGR_FLUSHES_ALONE */
        if (initp) {
            return(s->slot_rmode != DBE_CACHE_READWRITE);
        } else {
            return(s->slot_rmode == DBE_CACHE_READONLY);
        }
#endif /* DBE_IOMGR_FLUSHES_ALONE */
}

#endif /* SS_DEBUG */

/*#***********************************************************************\
 *
 *		slot_cmp
 *
 * Compares two cacheslots according to their addresses
 *
 * Parameters :
 *
 *	s1 - in, (use)
 *		disk address of left slot
 *
 *	s2 - in, (use)
 *		disk address of right slot
 *
 * Return value :
 *      == 0 when s1 == s2
 *      <  0 when s1 <  s2
 *      >  0 when s1 >  s2
 *
 * Comments :
 *      when the difference of the disk addresses exceeds INT_MAX
 *      in a 32-bit environment the sign of the result is incorrect
 *      (that is not a fatal error, because it just changes the flushing
 *      order).
 *
 * Globals used :
 *
 * See also :
 */
static int slot_cmp(void* s1, void* s2)
{
        if (sizeof(int) >= sizeof(su_daddr_t)) {
            /* 32 bit or bigger int */
            return ((int)(su_daddr_t)s1 -
                    (int)(su_daddr_t)s2);
        }
        /* 16 bit int */
        if ((su_daddr_t)s1 < (su_daddr_t)s2) {
            return (-1);
        }
        if ((su_daddr_t)s1 > (su_daddr_t)s2) {
            return (1);
        }
        return (0);
}

/*##**********************************************************************\
 *
 *		dbe_cache_concurrent_flushinit
 *
 * Initializes the cache for concurrent flush! This is to be called
 * in an action-consistent state when no cache slots are reached in write
 * mode!
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_cache_concurrent_flushinit(dbe_cache_t* cache)
{
        cache_hash_t* hash;
        dbe_cacheslot_t* s;
        hashbucket_t* bucket;
        uint i;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        ss_dprintf_2(("dbe_cache_concurrent_flushinit()\n"));
        if (dbefile_diskless) {
            ss_dprintf_2(("dbe_cache_concurrent_flushinit():return\n"));
            return;
        }
        hash = &cache->cac_hash;
        if (!cache->cac_flushflag) {
            cache->cac_flushflag = TRUE;
            cache->cac_flushpool = su_rbt_init(slot_cmp, NULL);
            cache->cac_flushctr++;

            for (i = 0; i < hash->h_tablesize; i++) {
                bucket = &hash->h_table[i];
                ss_dprintf_4(("dbe_cache_concurrent_flushinit():%s %d\n", __FILE__, __LINE__));
                CACHE_LOCK(i, bucket);
                hashbucket_lock(bucket);
                CACHE_LOCKED(i, bucket);
                ss_dprintf_4(("dbe_cache_concurrent_flushinit():%s %d\n", __FILE__, __LINE__));
                for (s = bucket->hb_slot; s != NULL; s = s->slot_hashnext) {

                    if (s->slot_flushctr < cache->cac_flushctr) {
                        if (s->slot_dirty) {
                            ss_dassert(s->slot_daddr != SU_DADDR_NULL);
                            ss_dassert(
                                su_rbt_search(
                                    cache->cac_flushpool,
                                    (void*)s->slot_daddr)
                                == NULL);
                            ss_rc_dassert(cache_checkflushslot(s, TRUE), s->slot_rmode);
                            su_rbt_insert(cache->cac_flushpool, (void*)s->slot_daddr);
                        } else {
                            s->slot_flushctr = cache->cac_flushctr;
                        }
                    }
                }
                ss_dprintf_4(("dbe_cache_concurrent_flushinit():%s %d\n", __FILE__, __LINE__));
                hashbucket_unlock(bucket);
                ss_dprintf_4(("dbe_cache_concurrent_flushinit():%s %d\n", __FILE__, __LINE__));
            }
            if (su_rbt_nelems(cache->cac_flushpool) == 0) {
                cache->cac_flushflag = FALSE;
            } else {
                cache->cac_flushpos = su_rbt_min(cache->cac_flushpool, NULL);
            }
        }
        ss_dprintf_2(("dbe_cache_concurrent_flushinit():return\n"));
}


/*##**********************************************************************\
 *
 *		dbe_cache_concurrent_flushstep
 *
 * Runs a step of concurrent flush
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	maxwrites - in
 *		maximum # of block writes to run at one flush step
 *
 * Return value :
 *      TRUE if there are more dirty blocks to write or
 *      FALSE when the task is completed
 *
 * Comments :
 *      This function must not be called from more than one thread
 *      at a time! multiple calls would result in unpredictable behavior
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cache_concurrent_flushstep(
        dbe_cache_t* cache,
        ulong maxwrites,
        dbe_info_flags_t infoflags)
{
        dbe_cacheslot_t* s;
        su_daddr_t daddr;
        ulong nwrites;
        bool completed;
        hashbucket_t* bucket;
        su_profile_timer;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        ss_dprintf_2(("dbe_cache_concurrent_flushstep() start\n"));
        if (dbefile_diskless) {
            ss_dprintf_2(("dbe_cache_concurrent_flushstep() end\n"));
            return FALSE;
        }

        su_profile_start;

        completed = FALSE;
        if (!cache->cac_flushflag) {
            completed = TRUE;
        }
        if (!completed) {
            for (nwrites = 0;
                 nwrites < maxwrites;
                 cache->cac_flushpos =
                    su_rbt_succ(cache->cac_flushpool, cache->cac_flushpos))
            {
                if (cache->cac_flushpos == NULL) {
                    completed = TRUE;
                    break;
                }
                daddr = (su_daddr_t)su_rbtnode_getkey(cache->cac_flushpos);
                bucket = HASH_BUCKET(&cache->cac_hash, daddr);
                CACHE_LOCK(daddr, bucket);
                hashbucket_lock(bucket);
                CACHE_LOCKED(daddr, bucket);
                s = hashbucket_search(bucket, daddr);
                if (s != NULL && s->slot_flushctr < cache->cac_flushctr) {
                    /* Flushcounter not changed, the slot
                     * is still dirty !
                     */
                    ss_rc_dassert(cache_checkflushslot(s, FALSE), s->slot_rmode);
                    slot_dowrite(cache, s, daddr, FALSE, infoflags);
                }
                hashbucket_unlock(bucket);
            }
        }
        if (completed) {
            cache->cac_flushflag = FALSE;
#ifdef SS_DEBUG
            if (cache->cac_flushpool != NULL) {
                for (cache->cac_flushpos = su_rbt_min(cache->cac_flushpool, NULL);
                    cache->cac_flushpos != NULL;
                    cache->cac_flushpos =
                        su_rbt_succ(cache->cac_flushpool, cache->cac_flushpos))
                {
                    daddr = (su_daddr_t)su_rbtnode_getkey(cache->cac_flushpos);
                    bucket = HASH_BUCKET(&cache->cac_hash, daddr);
                    CACHE_LOCK(daddr, bucket);
                    hashbucket_lock(bucket);
                    CACHE_LOCKED(daddr, bucket);
                    s = hashbucket_search(bucket, daddr);
                    if (s != NULL) {
                        ss_dassert(!s->slot_dirty
                            || s->slot_flushctr == cache->cac_flushctr);
                        if (dbe_cache_flushignoreall) {
                            hashbucket_remove(bucket, s);
                            s->slot_daddr = SU_DADDR_NULL;
                        }
                    }
                    hashbucket_unlock(bucket);
                }
            }
#endif /* SS_DEBUG */
            cache->cac_flushpos = NULL;
            if (cache->cac_flushpool != NULL) {
                su_rbt_done(cache->cac_flushpool);
                cache->cac_flushpool = NULL;
            }
            su_svf_flush(cache->cac_svfil);
            ss_debug(dbe_cache_checkafterflush(cache));
            ss_dprintf_2(("dbe_cache_concurrent_flushstep() completed\n"));
            su_profile_stop("dbe_cache_concurrent_flushstep");
            return (FALSE);
        }
        ss_dprintf_2(("dbe_cache_concurrent_flushstep() end\n"));
        su_profile_stop("dbe_cache_concurrent_flushstep");
        return (TRUE);
}

#if defined(DBE_MTFLUSH)

/*##**********************************************************************\
 *
 *		dbe_cache_getflusharr
 *
 * Gets array of disk addresses that are in dirty cache pages
 *
 * Parameters :
 *
 *	cache - use
 *		cache object
 *
 *	p_addrarray - out, give
 *	    pointer to pointer where the array start address will be stored
 *
 *	p_addrarraysize - out
 *		pointer to variable telling the size of address array
 *
 * Return value :
 *      TRUE at least one element is given in *p_addrarray,
 *      FALSE the *p_addrarray == NULL (no dirty pages found)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cache_getflusharr(
        dbe_cache_t* cache,
        su_daddr_t** p_addrarray,
        size_t* p_addrarraysize)
{
        su_daddr_t* addrarray;
        size_t addrarraysize;

        cache_hash_t* hash;
        dbe_cacheslot_t* s;
        hashbucket_t* bucket;
        uint i;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        ss_dprintf_1(("dbe_cache_getflusharr()\n"));
        hash = &cache->cac_hash;
        cache->cac_flushctr++;
        addrarray = SsMemAlloc(sizeof(su_daddr_t) * cache->cac_nslot);
        addrarraysize = 0;

        for (i = 0; i < hash->h_tablesize; i++) {
            bucket = &hash->h_table[i];
            CACHE_LOCK(i, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(i, bucket);
            for (s = bucket->hb_slot; s != NULL; s = s->slot_hashnext) {
                ss_dassert(s->slot_daddr != SU_DADDR_NULL);
                if (s->slot_flushctr < cache->cac_flushctr) {
                    if (s->slot_dirty) {
                        ss_dassert(s->slot_daddr != SU_DADDR_NULL);
                        ss_rc_dassert(cache_checkflushslot(s, TRUE), s->slot_rmode);
                        addrarray[addrarraysize++] = s->slot_daddr;
                        ss_dprintf_2(("dbe_cache_getflusharr: addr=%ld \n",
                                     (long)s->slot_daddr));
                    } else {
                        s->slot_flushctr = cache->cac_flushctr;
                    }
                }
            }
            hashbucket_unlock(bucket);
        }
        *p_addrarraysize = addrarraysize;
        if (addrarraysize == 0) {
            *p_addrarray = NULL;
            SsMemFree(addrarray);
            return (FALSE);
        }
        *p_addrarray = SsMemRealloc(
                            addrarray,
                            addrarraysize * sizeof(su_daddr_t));
        return (TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_cache_flushaddr
 *
 * Flushes a cache slot at specified disk address.
 * If a cache slot for that address is not found, this is
 * a no-op
 *
 * Parameters :
 *
 *      cache - use
 *          cache object
 *
 *      addr - in
 *          disk address
 *
 *      preflush - in
 *          information whether this is a preflush request
 *      
 *      writebuf - in, use
 *          a buffer of same size as cache slot's data area.
 *          used to implement non-blockin disk writes
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
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
        , dbe_info_flags_t infoflags)
{
        dbe_cacheslot_t* s;
        hashbucket_t* bucket;
        bool written = FALSE;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);
        ss_profile(if (cache->cac_semdbg_meswait != NULL) cache->cac_semdbg_meswait->sd_callcnt++;)

        ss_bprintf_1(("dbe_cache_flushaddr: addr=%ld, preflush=%d\n",
                      (long)addr, (int)preflush));
        for (;;) { 
            bucket = HASH_BUCKET(&cache->cac_hash, addr);
            CACHE_LOCK(addr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(addr, bucket);
            s = hashbucket_search(bucket, addr);
            if (s == NULL) {
                /* the slot is not in cache anymore! */
                break;
            }
            if (preflush) {
                if (!s->slot_dirty) {
                    break; /* no need to flush */
                }
                if (s->slot_inuse != 0) {
                    break; /* preflush is not relevant anymore! */
                }
            } else {
                /* checkpoint / close flush */
                if (!s->slot_dirty) {
                    /* The dassert below is due to Solid Server
                     * logic, not required by cache manager!
                     */
                    ss_dassert(s->slot_flushctr == cache->cac_flushctr);
                    break; /* no need to flush */
                }
                /* Flushcounter not changed, the slot
                 * is still dirty !
                 */
#ifdef SS_MME
                if (!mmeslot) {
                    ss_dassert(s->slot_flushctr < cache->cac_flushctr);
                    ss_dassert(s->slot_dirty);
                    ss_rc_dassert(cache_checkflushslot(s, FALSE),
                                  s->slot_rmode);
                }
#else
                ss_dassert(s->slot_flushctr < cache->cac_flushctr);
                ss_debug(
                    ss_dassert(s->slot_dirty);
                    ss_rc_assert(cache_checkflushslot(s, FALSE), s->slot_rmode);
                );
#endif
            }
#ifdef DBE_NONBLOCKING_PAGEFLUSH
            if (s->slot_writingtodisk) {
                cache_slotqticket_t qticket;
                
                ss_dassert(s->slot_dirty);
                cache_slotqlink(s, &qticket, FALSE);
                hashbucket_unlock(bucket);
                cache_slotqwait(cache, &qticket);
                continue; /* retry! (never iterates several times) */
            }
                
            writebuf = (ss_byte_t*)s->slot_data;
            s->slot_writingtodisk = TRUE;
            
            dbe_cache_slotpmonupdate(s);

            hashbucket_unlock(bucket);
            FAKE_CODE_BLOCK(FAKE_DBE_NONBLOCKINGFLUSHSLEEP, SsThrSleep(100););
            /* now do disk write without bucket lock */
            slot_dowrite_buf(cache, writebuf, addr, preflush, infoflags);
            hashbucket_lock(bucket);
            s->slot_writingtodisk = FALSE;
            s->slot_flushctr = cache->cac_flushctr;
            s->slot_preflushreq = FALSE;
            ss_beta(
                if (s->slot_dirty) {
                    SLOT_SET_CLEAN(s);
                });
            s->slot_dirty = FALSE;
            s->slot_oldvers = FALSE;
            CACHE_SLOTQWAKEUPALL(s);
#else /* DBE_NONBLOCKING_PAGEFLUSH */
            slot_dowrite(cache, s, addr, preflush, infoflags);
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
            written = TRUE;
            break;
        }
        if (written && s->slot_inuse == 0) {
            plru_t* plru;
            int lruindex;
            lruindex = s->slot_lruindex;
            plru = &cache->cac_plru[lruindex];
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            if (plru_slot_is_in_current_flushbatch(plru, s)
            &&  !s->slot_dirty
            &&  s->slot_daddr == addr)
            {
                plru_remove(plru, s);
                plru_link_to_clean_victims_list(plru, s);
            }
            CACHE_LRUUNLOCK();
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
        }
        hashbucket_unlock(bucket);
}

#ifdef MME_CP_FIX
void dbe_cache_flushaddr_n(
        dbe_cache_t*            cache,
        dbe_cache_flushaddr_t*  flushes,
        ulong                   nflushes)
{
        dbe_cacheslot_t*        s;
        hashbucket_t*           bucket;
        ulong                   i;
        ulong                   j;
        su_svf_lioreq_t         ioreq[DBE_CACHE_MAXNFLUSHES];
        su_daddr_t              addr;
        bool                    preflush;
        bool                    mmeslot;
        ss_byte_t*              writebuf;
        su_ret_t                rc;
        dbe_cacheslot_t*        slots[DBE_CACHE_MAXNFLUSHES];
        su_profile_timer;

        ss_dassert(cache != NULL);
        ss_dassert(cache->cac_check == DBE_CHK_CACHE);
        ss_dassert(nflushes <= DBE_CACHE_MAXNFLUSHES);

        su_profile_start;

        /* SsPrintf("DBE CACHE FLUSHING %d PAGES\n", nflushes); */

        for (i = 0, j = 0; i < nflushes; i++) {
            addr = flushes[i].fa_addr;
            preflush = flushes[i].fa_preflush;
            mmeslot = flushes[i].fa_mmeslot;
            writebuf = flushes[i].fa_writebuf;

            ss_profile(if (cache->cac_semdbg_meswait != NULL) cache->cac_semdbg_meswait->sd_callcnt++;)

            for (;;) { 
                ss_bprintf_1(("dbe_cache_flushaddr_n: addr=%ld, preflush=%d\n",
                              (long)addr, (int)preflush));
                bucket = HASH_BUCKET(&cache->cac_hash, addr);
                CACHE_LOCK(addr, bucket);
                hashbucket_lock(bucket);
                CACHE_LOCKED(addr, bucket);
                s = hashbucket_search(bucket, addr);
                if (s == NULL) {
                    /* the slot is not in cache anymore! */
                    hashbucket_unlock(bucket);
                    break;
                }
                if (preflush) {
                    if (!s->slot_dirty || s->slot_inuse != 0) {
                        hashbucket_unlock(bucket);
                        break; /* no need to flush */
                    }
                } else {
                    /* checkpoint / close flush */
                    if (!s->slot_dirty) {
                        /* The dassert below is due to Solid Server
                         * logic, not required by cache manager!
                         */
                        ss_dassert(s->slot_flushctr == cache->cac_flushctr);
                        hashbucket_unlock(bucket);
                        break; /* no need to flush */
                    }
                    /* Flushcounter not changed, the slot
                     * is still dirty !
                     */
                    if (!mmeslot) {
                        ss_dassert(s->slot_flushctr < cache->cac_flushctr);
                        ss_dassert(s->slot_dirty);
                        ss_rc_dassert(cache_checkflushslot(s, FALSE),
                                      s->slot_rmode);
                    }
                }
                if (s->slot_writingtodisk) {
                    cache_slotqticket_t qticket;
                    
                    ss_dassert(s->slot_dirty);
                    cache_slotqlink(s, &qticket, FALSE);
                    hashbucket_unlock(bucket);
                    cache_slotqwait(cache, &qticket);
                    continue; /* retry! (never iterates several times) */
                }
                writebuf = (ss_byte_t*)s->slot_data;
                s->slot_writingtodisk = TRUE;
                
                hashbucket_unlock(bucket);

                dbe_cache_slotpmonupdate(s);

                /* now do disk write without bucket lock */
                /* NOTE! The following lines are not reentrant,
                   but who cares? */
                if (preflush) {
                    cache->cac_info.cachei_npreflush++;
                }
                cache->cac_info.cachei_nwrite++;
                if (flushes[i].fa_infoflags & DBE_INFO_MERGE) {
                    SS_PMON_ADD(SS_PMON_MERGEFILEWRITE);
                }
                if (flushes[i].fa_infoflags & DBE_INFO_CHECKPOINT) {
                    SS_PMON_ADD(SS_PMON_CHECKPOINTFILEWRITE);
                }
                /* end-of-non-reentrant */

                ioreq[j].lr_reqtype = SS_LIO_WRITE;
                ioreq[j].lr_daddr = addr;
                ioreq[j].lr_data = writebuf;
                ioreq[j].lr_size = cache->cac_blocksize;
                slots[j] = s;
                j++;
                break;
            }
        }

        if (j > 0) {
            rc = su_svf_listio(cache->cac_svfil, ioreq, j);
            su_rc_assert(rc == SU_SUCCESS, rc);

        }
        for (i = 0; i < j; i++) {
            s = slots[i];
            addr = s->slot_daddr;
            ss_dassert(ioreq[i].lr_daddr == addr);
            bucket = HASH_BUCKET(&cache->cac_hash, addr);
            CACHE_LOCK(addr, bucket);
            hashbucket_lock(bucket);
            CACHE_LOCKED(addr, bucket);
            ss_debug(s = hashbucket_search(bucket, addr));
            ss_dassert(slots[i] == s);
            ss_dassert(s->slot_writingtodisk);
            s->slot_writingtodisk = FALSE;
            s->slot_flushctr = cache->cac_flushctr;
            s->slot_preflushreq = FALSE;
            ss_beta(
                    if (s->slot_dirty) {
                        SLOT_SET_CLEAN(s);
                    });
            s->slot_dirty = FALSE;
            s->slot_oldvers = FALSE;
            if (s->slot_inuse == 0) {
                plru_t* plru;
                int lruindex;

                lruindex = s->slot_lruindex;
                plru = &cache->cac_plru[lruindex];
                CACHE_LRULOCK();
                SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                CACHE_LRULOCKED();
                if (plru_slot_is_in_current_flushbatch(plru, s)) {
                    plru_remove(plru, s);
                    plru_link_to_clean_victims_list(plru, s);
                }
                CACHE_LRUUNLOCK();
                SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
            }
            CACHE_SLOTQWAKEUPALL(s);
            hashbucket_unlock(bucket);
        }
        su_profile_stop("dbe_cache_flushaddr_n");
}
#endif /* MME_CP_FIX */

#endif /* DBE_MTFLUSH */

/*##**********************************************************************\
 *
 *		dbe_cache_ignoreaddr
 *
 * Ignores a slot address if it is found in cache. This function
 * is called upon freeing the disk block. Its main purpose is to prevent
 * extra I/O when the slot is dirty and it is recycled from the LRU queue.
 * It also lengthens the lifetime of usable slots because the ignored
 * slot is linked as the LRU slot.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	addr - in
 *		disk address to be ignored
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_cache_ignoreaddr(
        dbe_cache_t* cache,
        su_daddr_t addr)
{
        dbe_cacheslot_t* s;
        hashbucket_t* bucket;
        int lruindex;

        ss_pprintf_2(("dbe_cache_ignoreaddr:addr=%ld\n", addr));
        bucket = HASH_BUCKET(&cache->cac_hash, addr);
        CACHE_LOCK(addr, bucket);
        hashbucket_lock(bucket);
        CACHE_LOCKED(addr, bucket);
        s = hashbucket_search(bucket, addr);
        if (s != NULL) {
#ifdef DBE_NONBLOCKING_PAGEFLUSH
            if (!s->slot_writingtodisk)
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
            {
                if (s->slot_inuse == 0) {
                    /* nobody is waiting for or using the slot,
                     * we can safely ignore it
                     */
                    CHK_SLOT(s, FALSE);
                    ss_debug(
                    {
                        dbe_blocktype_t blocktype =
                            DBE_BLOCK_FREECACHEPAGE;
                        DBE_BLOCK_SETTYPE(
                                s->slot_data, &blocktype);
                        ss_dprintf_4(("dbe_cache_ignoreaddr:addr=%ld:set type to DBE_BLOCK_FREECACHEPAGE\n", s->slot_daddr));
                    }
                        );
                    lruindex = s->slot_lruindex;
                    CACHE_LRULOCK();
                    SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                    CACHE_LRULOCKED();
#ifdef SLOT_QUEUE
                    CACHE_SLOTQWAKEUPALL(s);
#else
                    if (s->slot_rmode == DBE_CACHE_WRITEONLY
                        ||  s->slot_rmode == DBE_CACHE_READWRITE
                        ||  s->slot_rmode == DBE_CACHE_PREFLUSHREACH)
                    {
                        SsMesSend(s->slot_waitmsg);
                    }
#endif
                    ss_beta(
                            if (s->slot_dirty) {
                                SLOT_SET_CLEAN(s);
                            });
                    s->slot_dirty = FALSE;
                    s->slot_preflushreq = FALSE;
                    s->slot_daddr = SU_DADDR_NULL;
                    hashbucket_remove(bucket, s);
                    plru_remove(&cache->cac_plru[lruindex], s);
                    plru_ignore(&cache->cac_plru[lruindex], s);
                    CACHE_LRUUNLOCK();
                    SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                } else {
                    /* Someone accesses or waits for the slot,
                     * semantically not meaningful but
                     * prefetch thread may, however,
                     * reach this slot occasionally while
                     * we're currently ignoring it ...
                     */
                    ss_beta(
                            if (s->slot_dirty) {
                                SLOT_SET_CLEAN(s);
                            });
                    ss_pprintf_4(("dbe_cache_ignoreaddr:set slot_oldvers\n"));
                    s->slot_oldvers = TRUE;
                    s->slot_dirty = FALSE;
                    s->slot_preflushreq = FALSE;
                    hashbucket_remove(bucket, s);
                }
            }
        }
        hashbucket_unlock(bucket);
}

#ifndef SS_MT   /* Only single-threaded version uses this kind of preflush */

/*#***********************************************************************\
 *
 *		slot_cmp2
 *
 * Compares slot disk addresses
 *
 * Parameters :
 *
 *	p1 - in, use
 *		pointer to pointer to 1st cache slot
 *
 *	p2 - in, use
 *		pointer to pointer to 2nd cache slot
 *
 * Return value :
 *      >  0 if 1st addr >  2nd address
 *      <  0 if 1st addr <  2nd address
 *      == 0 if 1st addr == 2nd address (should never happen here!)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int SS_CLIBCALLBACK slot_cmp2(const void* p1, const void* p2)
{
        if (sizeof(int) >= sizeof(su_daddr_t)) {
            /* 32 bit or bigger int */
            return ((int)(*(const dbe_cacheslot_t**)p1)->slot_daddr -
                    (int)(*(const dbe_cacheslot_t**)p2)->slot_daddr);
        }
        /* 16 bit int */
        if ((*(const dbe_cacheslot_t**)p1)->slot_daddr >
            (*(const dbe_cacheslot_t**)p2)->slot_daddr)
        {
            return (1);
        }
        if ((*(const dbe_cacheslot_t**)p1)->slot_daddr <
            (*(const dbe_cacheslot_t**)p2)->slot_daddr)
        {
            return (-1);
        }
        return (0);

}

/*#***********************************************************************\
 *
 *		cache_dopreflush
 *
 * Sorts, then writes a bunch of dirty cache slots. The sorting usually
 * improves the I/O performance dramatically, especially when the
 * disk addresses are close to each other.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *	preflush_array - in out, use
 *		array of cache slot pointers
 *
 *	n - in
 *		number of slots to flush
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void cache_dopreflush(
        dbe_cache_t* cache,
        dbe_cacheslot_t** preflush_array,
        size_t n,
        dbe_info_flags_t infoflags)
{
        int i;
        dbe_cacheslot_t* slot;

        ss_dassert(n > 0);  /* There must be something to flush ! */

        if (n > 1) {
            /* There is something to sort!
                */
            qsort(
                preflush_array,
                n,
                sizeof(dbe_cacheslot_t*),
                slot_cmp2);
        }
        for (i = 0; i < n; i++) {
            slot = preflush_array[i];
            slot_dowrite(cache, slot, slot->slot_daddr, TRUE, infoflags);
        }
}

/*#***********************************************************************\
 *
 *		cache_preflush
 *
 * Finds (does not remove) the LRU slot and flushes some dirty slots
 * from the LRU chain. Macro PREFLUSH_MAX(cache) defines the maximum # of
 * slots to write at one preflush. The other limit is set by the macro
 * cache->cac_preflushportion which limits how many slots from LRU chain
 * are to be scanned in order to find the dirty slots to flush.
 * The scan stops to the limit that is first reached.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		cache pointer
 *
 *      s - in out, use
 *          pointer to first slot to be flushed. Note: it must not be
 *          found from the LRU chain!
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void cache_preflush(
        dbe_cache_t* cache,
        dbe_cacheslot_t* s,
        dbe_info_flags_t infoflags)
{
        plru_t* plru;
        dbe_cacheslot_t* slot;
        int i;      /* index */
        int j;      /* # of dirty slots to flush */
        int k;      /* # of slots scanned */
        int lrustart;
        int lruloop;
        int lruindex; /* PLRU index */
        int max_k;  /* max # of slots to scan */

        dbe_cacheslot_t** preflush_array;

        preflush_array =
            SsMemAlloc(PREFLUSH_MAX(cache) * sizeof(dbe_cacheslot_t*));
        preflush_array[0] = s;
        j = 1;
        k = 0;
        max_k = cache->cac_preflushportion;
        if (max_k == 0) {
            max_k = 1;
        }
        lrustart = cache_lruindex++;
        for (lruloop = 0; lruloop < PLRU_NLRU; lruloop++) {
            lruindex = (lrustart + lruloop) % PLRU_NLRU;
            plru = cache->cac_plru[lruindex];
            for (i = PLRU_NCLASS - 1; i >= 0; i--) {
                slot = plru->pl_head[i]->slot_.prev;
                ss_debug(if (slot != plru->pl_head[i]) { ss_dprintf_2(("cache_preflush() class %d\n", i)); })
                while (slot != plru->pl_head[i]) {
                    if (slot->slot_dirty) {
                        ss_dassert(slot->slot_daddr != SU_DADDR_NULL);
                        preflush_array[j] = slot;
                        j++;
                        if (j >= PREFLUSH_MAX(cache)) {
                            /* Max # of slots to preflush has been reached
                             */
                            ss_dassert(j == PREFLUSH_MAX(cache));
                            goto loop_exit;
                        }
                    }
                    k++;
                    if (k >= max_k) {
                        /* Max proportinal part of LRU reached
                         */
                        ss_dassert(k == max_k);
                        goto loop_exit;
                    }
                    if (k >= max_k / PLRU_NLRU) {
                        /* Max proportinal part of this LRU reached
                         */
                        goto lru_exit;
                    }
                    slot = slot->slot_.prev; /* move to previous slot */
                }
            }
lru_exit:;
        }
loop_exit:; /* Sorry about the goto label!
             * Unfortunately it is the only proper way to exit a
             * double loop without using some weird 'exitflags'
             */
        cache_dopreflush(cache, preflush_array, (size_t)j, infoflags);
        SsMemFree(preflush_array);
}

#endif /* !SS_MT */

#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *		dbe_cache_printinfo
 *
 *
 *
 * Parameters :
 *
 *	fp -
 *
 *
 *	cache -
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
void dbe_cache_printinfo(
        void* fp,
        dbe_cache_t* cache)
{
        uint nitem;
        uint ndirty;
        dbe_cache_info_t info;
        int lruindex;

        ss_dassert(cache->cac_check == DBE_CHK_CACHE);

        dbe_cache_getinfo_internal(cache, &info, FALSE);

        SsFprintf(fp, "    Nslot %d Blocksize %d Flush counter %d Flush flag %d\n",
            cache->cac_nslot,
            cache->cac_blocksize,
            cache->cac_flushctr,
            cache->cac_flushflag);

        SsFprintf(fp, "    Hash tablesize %d nitem %d nchain %d\n",
            cache->cac_hash.h_tablesize,
            info.cachei_nitem,
            info.cachei_nchain);
        SsFprintf(fp, "         minchain %d maxchain %d avgchain %.1lf\n",
            info.cachei_minchain,
            info.cachei_maxchain,
            info.cachei_avgchain);

        SsFprintf(fp, "    Hit rate %.1lf%% Write avoid rate %.1lf%% nfind %ld nread %ld nwrite %ld\n",
            (double)info.cachei_readhitrate,
            (double)info.cachei_writeavoidrate,
            info.cachei_nfind,
            info.cachei_nread,
            info.cachei_nwrite);

        for (lruindex = 0; lruindex < PLRU_NLRU; lruindex++) {
            nitem = 0;
            ndirty = 0;
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            CACHE_LRUUNLOCK();
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
        }
}
#endif /* SS_LIGHT */

/*##**********************************************************************\
 *
 *		dbe_cache_setpreflushinfo
 *
 * Sets preflushing info.
 *
 * Parameters :
 *
 *	cache - in out, use
 *		pointer to cache object
 *
 *	percent - in
 *		percentage of LRU to scan (0 .. 50) for preflushing,
 *		this area is used by the preflusher process.
 *
 *      lastuseskippercent - in
 *          percentage of LRU list (preflush area not included)
 *          that should be skipped if releasemode ==
 *          DBE_CACHE_(DIRTY|CLEAN)LASTUSE
 *

 * Return value :
 *      TRUE when succesful or
 *      FALSE when the percent value was not in [1 - 90]
 *            or the lastuseskippercent was not [0 - 100]
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cache_setpreflushinfo(
        dbe_cache_t* cache,
        uint percent,
        uint lastuseskippercent)
{
        bool succp = TRUE;

        if (percent < 1) {
            succp = FALSE;
            percent = 1;
        } else if (percent > 90) {
            succp = FALSE;
            percent = 90;
        }
        cache->cac_preflushportion = (int)
            ((cache->cac_nslot * (long)percent + 99) / 100);
        if (cache->cac_preflushportion < 2) {
            cache->cac_preflushportion = 2;
        }
        if (lastuseskippercent < 0) {
            lastuseskippercent = 0;
            succp = FALSE;
        }
        if (lastuseskippercent > 100) {
            lastuseskippercent = 100;
            succp = FALSE;
        }
        {
            size_t lru_victimarea_minsize;
            size_t i;

            lru_victimarea_minsize = cache->cac_preflushportion / PLRU_NLRU;
            if (lru_victimarea_minsize < 2) {
                lru_victimarea_minsize = 2;
            }
            for (i = 0; i < PLRU_NLRU; i++) {
                plru_setlastuseskippercent(&cache->cac_plru[i],
                                           lastuseskippercent);
                plru_setvictimarea_minsize(&cache->cac_plru[i],
                                           lru_victimarea_minsize);
            }
        }
        return (succp);
}


/*##**********************************************************************\
 *
 *		dbe_cache_addpreflushpage
 *
 * Adds one page to external preflush request count. Used to trigger
 * preflush in special cases where normal preflusher does not work properly
 * like block insert where several blocks become dirty at the same time.
 * If more than preflushportion blocks are requested for preflushing
 * by this function, preflushing is started.
 *
 * Parameters :
 *
 *	cache -
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
void dbe_cache_addpreflushpage(
        dbe_cache_t* cache)
{
        cache->cac_extpreflushreq++;
        if (cache->cac_extpreflushreq >= (uint)cache->cac_preflushportion) {
            if (cache->cac_preflushcallback != (void(*)(void*))NULL) {
                (*cache->cac_preflushcallback)(cache->cac_preflushctx);
            }
            cache->cac_extpreflushreq = 0;
        }
}

/*##**********************************************************************\
 *
 *		dbe_cacheslot_isoldversion
 *
 * Tells whether a cache slot is made out-dated.
 *
 * Parameters :
 *
 *	slot - in, use
 *
 *
 * Return value :
 *      TRUE if cache slot is old version
 *      FALSE when cache slot is still the current version
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cacheslot_isoldversion(dbe_cacheslot_t* slot)
{
        ss_dassert(slot != NULL);
        return (slot->slot_oldvers);
}

#ifdef PREFLUSH_THREAD

/*#***********************************************************************\
 *
 *		cache_selectslot_enterlrumutex
 *
 * Select a slot for reuse from lru
 *
 * Parameters :
 *
 *	    cache - in out, use
 *
 *      lrulimit - in
 *          limit the victim search to clean pages only
 *
 *		pointer to cache object
 *
 *      p_preflushflag - out
 *          pointer to boolean variable which indicates need
 *          to do preflushing
 *
 * Return value - ref:
 *      pointer to cache slot that is still linked into LRU chain
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_cacheslot_t* cache_selectslot_enterlrumutex(
        dbe_cache_t* cache,
        bool lrulimit,
        int* p_lruindex,
        bool* p_preflushflag)
{
        plru_t* plru;
        dbe_cacheslot_t* slot;
        int lruindex;
        int lrustart;
        int lruloop;
        size_t ndirty;

        lrustart = cache_lruindex++;
        for (lruloop = 0; lruloop < PLRU_NLRU; lruloop++) {
        
            lruindex = (lrustart + lruloop) % PLRU_NLRU;
            plru = &cache->cac_plru[lruindex];
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            slot = plru_find_victim(plru, lrulimit);
            if (slot != NULL) {
                goto victim_slot_found;
            }
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
        }
        ss_dassert(slot == NULL);
        if (lrulimit) {
            ss_bprintf_2(("cache_selectslot: no slot, try recusive call without lrulimit\n"));
            ss_dassert(SsSemThreadIsNotEntered(cache->cac_lrumutex[lruindex]));
            slot = cache_selectslot_enterlrumutex(cache, 
                                                  FALSE,
                                                  p_lruindex, 
                                                  p_preflushflag);
        }
        goto ret_cleanup;
victim_slot_found:;
        ss_dassert(slot != NULL);
        *p_preflushflag = FALSE;
        ndirty = 0;
        for (lruloop = 0; lruloop < PLRU_NLRU; lruloop++) {
            /* Note: this is a dirty read from most of the LRU pools!
               It is safe because sligh inexactness is
               allowed.
            */
            ndirty += plru_ntoflush(&cache->cac_plru[lruloop]);
        }        
        if ((ndirty >= 10) ||
            (ndirty > 0 && cache->cac_preflushportion <= 40))
        {
            if (cache->cac_preflushcallback
                != (void(*)(void*))NULL)
            {
                *p_preflushflag = TRUE;
            }
        }
        cache->cac_npagerep++;
        *p_lruindex = lruindex;
ret_cleanup:;
        ss_dassert(slot == NULL || *p_lruindex == slot->slot_lruindex);
        ss_dassert(slot == NULL || SsSemThreadIsEntered(cache->cac_lrumutex[*p_lruindex]));
        return (slot);
}

/*##**********************************************************************\
 *
 *		dbe_cache_getpreflusharr
 *
 * Gets array of slot addresses that should be flushed by the preflusher
 * thread.
 *
 * Parameters :
 *
 *	cache - in, use
 *		pointer to cache object
 *
 *	p_array - out, give
 *		pointer to pointer to array of disk addresses or NULL
 *
 *	p_arraysize - out
 *		pointer to variable where the array size will be stored
 *
 * Return value :
 *      TRUE when there is something to be preflushed or
 *      FALSE otherwise (*p_array == NULL)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_cache_getpreflusharr(
        dbe_cache_t* cache,
        su_daddr_t** p_array,
        size_t* p_arraysize)
{
        plru_t* plru;
        dbe_cacheslot_t* slot;
        int lruindex;
        int lrustart;
        int lruloop;

        size_t preflush_array_size;
        size_t preflush_array_used;
        su_daddr_t* preflush_array;

        ss_dprintf_2(("dbe_cache_getpreflusharr\n"));

        /* check for multiple instances, which would be unnecessaery */
        if (cache_getpreflusharr_active) {
            *p_arraysize = 0;
            *p_array = NULL;
            return (FALSE);
        }
        cache_getpreflusharr_active = TRUE;

        preflush_array_size = PREFLUSH_MAX(cache);
        if (preflush_array_size > 126) {
            preflush_array_size = 126;
        }
        preflush_array_used = 0;
        preflush_array =
            SsMemAlloc(preflush_array_size * sizeof(su_daddr_t));

        lrustart = cache_lruindex++;
        for (lruloop = 0; lruloop < PLRU_NLRU; lruloop++) {
            lruindex = (lrustart + lruloop) % PLRU_NLRU;
            plru = &cache->cac_plru[lruindex];

            if (plru_ntoflush(plru) != 0) {
                CACHE_LRULOCK();
                SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
                CACHE_LRULOCKED();
                plru_age(plru);
                for (;;) {
                    slot = plru_remove_preflushslot(plru);
                    if (slot == NULL) {
                        break;
                    }
                    ss_dassert(slot->slot_daddr != SU_DADDR_NULL);
                    if (preflush_array_used >= preflush_array_size) {
                        preflush_array_size =
                            ((preflush_array_size + 2) * 2) - 2;
                        preflush_array =
                            SsMemRealloc(
                                    preflush_array,
                                    preflush_array_size * sizeof(su_daddr_t));
                    }
                    plru_link_to_current_preflushbatch(plru, slot);
                    slot->slot_preflushreq = TRUE;
                    preflush_array[preflush_array_used] = slot->slot_daddr;
                    preflush_array_used++;
                    if (preflush_array_used >= (uint)cache->cac_preflushmax) {
                        /* enough pages gotten, proceeding would cause
                         * flooding the I/O threads with too many write
                         * requests thus causing starvation to other
                         * I/O requests.
                         */
                        CACHE_LRUUNLOCK();
                        SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
                        goto loop_exit;
                    }
                }
                CACHE_LRUUNLOCK();
                SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
            }
        }
loop_exit:;
        *p_arraysize = preflush_array_used;
        if (preflush_array_used == 0) {
            SsMemFree(preflush_array);
            *p_array = NULL;
            cache_getpreflusharr_active = FALSE;
            return (FALSE);
        }
        *p_array = preflush_array;
        cache_getpreflusharr_active = FALSE;
        ss_dprintf_2(("dbe_cache_getpreflusharr:array size=%d\n", preflush_array_used));
        return (TRUE);
}

/*##**********************************************************************\
 *
 *		dbe_cache_setpreflushcallback
 *
 * Sets callback function to invoke preflusher
 *
 * Parameters :
 *
 *      cache - in out, use
 *          pointer to cache object
 *
 *	callbackfun - in
 *          pointer to callback function
 *
 *	callbackctx - in, take
 *		pointer to context structure used for parameter for
 *          the callback function. It can be removed using SsMemFree().
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_cache_setpreflushcallback(
        dbe_cache_t* cache,
        void (*callbackfun)(void*),
        void* callbackctx)
{
        /* no need to flush for diskless */
        if (dbefile_diskless) {
            return;
        }

        cache->cac_preflushcallback = callbackfun;
        if (cache->cac_preflushctx != NULL) {
            SsMemFree(cache->cac_preflushctx);
        }
        cache->cac_preflushctx = callbackctx;
}

#else /* PREFLUSH_THREAD */

# error Not updated!!!!
static dbe_cacheslot_t* cache_selectslot_enterlrumutex(
        dbe_cache_t* cache, 
        bool lrulimit,
        int* p_lruindex,
        bool* p_preflushflag)
{
        int lruindex;
        int lrustart;
        int lruloop;
        dbe_cacheslot_t* slot;

        *p_preflushflag = FALSE;
        lrustart = cache_lruindex++;
        for (lruloop = 0; lruloop < PLRU_NLRU; lruloop++) {
            lruindex = (lrustart + lruloop) % PLRU_NLRU;
            CACHE_LRULOCK();
            SsFlatMutexLock(cache->cac_lrumutex[lruindex]);
            CACHE_LRULOCKED();
            slot = plru_getslot(cache->cac_plru[lruindex]);
            if (slot != NULL) {
                break;
            }
            CACHE_LRUUNLOCK();
            SsFlatMutexUnlock(cache->cac_lrumutex[lruindex]);
        }
        return (slot);
}

#endif /* PREFLUSH_THREAD */

int dbe_cache_getnslot(
        dbe_cache_t* cache)
{
        return(cache->cac_nslot);
}
