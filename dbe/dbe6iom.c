/*************************************************************************\
**  source       * dbe6iom.c
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


#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

The implementation uses a thread per physical I/O device and a red-black
binary tree for ordering of requests.

Limitations:
-----------

The I/O manager requires a multithreaded environment. Single-thread
version is just a thunk with no extra functionality

Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>
#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssthread.h>
#include <ssstring.h>
#include <ssservic.h>
#include <sscacmem.h>

#include <su0parr.h>
#include <su0rbtr.h>
#include <su0mesl.h>

#include "dbe6finf.h"
#include "dbe6iom.h"

#ifdef SS_DEBUG
static char iom_prefetch[] = "I/O manager prefetch";
#ifndef SS_MYSQL
static char iom_preflush[] = "I/O manager preflush";
#endif
static char iom_request[] = "I/O manager request";
static char iom_flushbatch[] = "I/O manager flush batch";
#else
#define iom_prefetch NULL
#define iom_preflush NULL
#define iom_request  NULL
#define iom_flushbatch NULL
#endif

#if defined(SS_MT)

# ifdef SS_DEBUG
static bool mtflush_active = FALSE;
static uint mtflush_ntoflush = 0;
static uint iomgr_nthreads = 0;
static uint iomgr_nthreads_sleeping = 0;
#ifdef SS_MME
static bool mmeflush_active = FALSE;
static uint mmeflush_ntoflush = 0;
#endif
# endif

extern bool dbefile_diskless; /* for diskless, no physical dbfile */

/* I/O Request type enumerator */
typedef enum {
        IOM_CACHE_PREFETCH,
        IOM_CACHE_PREFLUSH,
        IOM_CACHE_REACH,            /* Blocking read */
        IOM_CACHE_FLUSHPAGE,        /* uses ior_.cacheslot */
        IOM_CACHE_FLUSHPAGELASTUSE  /* uses ior_.cacheslot */
#if defined(DBE_MTFLUSH)
        ,IOM_CACHE_FLUSHPAGEBATCH   /* uses ior_.flushbatch */
#endif
} iomgr_reqtype_t;

typedef enum {
        IOM_DEVQUEUE_READ,
        IOM_DEVQUEUE_WRITE,
        IOM_DEVQUEUE_READWRITE
} iomgr_devqueuetype_t;

/* Possible data context for I/O request */
typedef struct {
        dbe_cacheslot_t* cacheslot;
#if defined(DBE_MTFLUSH)
        dbe_iomgr_flushbatch_t*  flushbatch;
#endif /* DBE_MTFLUSH */
#ifdef SS_MME
        bool mmepage;
#endif
} iomgr_reqdata_t;

/* Wait queue ticket */
typedef struct iomgr_wait_st iomgr_wait_t;
struct iomgr_wait_st {
        su_mes_t*       iow_mes;
        iomgr_wait_t*   iow_next;
};

/* I/O request */
typedef struct {
        iomgr_reqtype_t ior_type;
        su_daddr_t      ior_daddr;
        iomgr_wait_t*   ior_wait;
        bool            ior_expired;
        dbe_info_flags_t ior_infoflags;
        iomgr_reqdata_t ior_;
} iomgr_request_t;


/* I/O device queue */
typedef struct {
        su_rbt_t*       iodq_reqpool;
#ifdef SS_NTHREADS_PER_DEVQUEUE
        SsThreadT**     iodq_threads;
        iomgr_request_t iodq_req;
        su_meslist_t    iodq_meslist;
        su_meswaitlist_t* iodq_meswaitlist;
#else
        SsThreadT*      iodq_thread;
#endif /* SS_NTHREADS_PER_DEVQUEUE */
        SsMesT*         iodq_emptymsg;
        SsSemT*         iodq_mutex;     /* Ref. to iom_mutex */
        dbe_file_t*     iodq_file;
        dbe_filedes_t*  iodq_filedes;
        dbe_iomgr_t*    iodq_iomgr;
        bool            iodq_stop;
#ifdef SS_NTHREADS_PER_DEVQUEUE
        uint            iodq_nthrrunning;
        uint            iodq_nthreads;
        iomgr_devqueuetype_t iodq_type;
#else
        bool            iodq_thrstopped;
        SsMesT*         iodq_wait;
#endif /* SS_NTHREADS_PER_DEVQUEUE */
} iomgr_devqueue_t;

typedef enum {
    IOM_THRMODE_SEPARATE,
    IOM_THRMODE_SHARED
} iomgr_threadmode_t;

/* I/O manager */
struct dbe_iomgr_st {
        dbe_file_t*     iom_file;           /* Ref. to file object */
        dbe_filedes_t*  iom_filedes;
        SsSemT*         iom_mutex;          /* for thread synchronization */
        bool            iom_useiothreads;   /* use I/O threads for all cache I/O ? */
        su_meslist_t*   iom_meslist;
#ifdef SS_MME
        uint            iom_maxmmepages;    /* max number of allowed MME pages in cache */
        size_t          iom_mme_nflushed;
        size_t          iom_mme_ntoflush;
#endif
        size_t          iom_dbe_nflushed;
        size_t          iom_dbe_ntoflush;
#ifdef SS_NTHREADS_PER_DEVQUEUE
        long            iom_nthreads_total;       /* # of threads per device */
        long            iom_nthreads_writers;      /* # of writer threads */
        iomgr_threadmode_t iom_thrmode;
        
        su_pa_t*        iom_devqueues_read_or_rw; 
        su_pa_t*        iom_devqueues_write;      
#else /* SS_NTHREADS_PER_DEVQUEUE */
        su_pa_t*        iom_devqueues;      /* array of I/O device queues */

#endif
};

/* Context object for callback function that makes
 * the preflush requests from cache
 */
typedef struct {
        dbe_iomgr_t* iopf_iomgr;
        dbe_cache_t* iopf_cache;
} iomgr_cachepreflushctx_t;


#if defined(DBE_MTFLUSH)

struct dbe_iomgr_flushbatch_st {
        uint fb_nleft;
        void (*fb_wakeupfp)(void*);
        void*  fb_wakeupctx;
#ifdef SS_MME
        bool   fb_mme; /* mme batch */
        bool   fb_signal; /* signal when buffer has space for mme pages */
        void (*fb_spacefp)(void*);
        void*  fb_spacectx;
#endif /* SS_MME */
        dbe_iomgr_t* fb_iomgr;
};

#ifdef SS_MME
#define FLUSHBATCH_FULL(fb, iom) (fb->fb_nleft >= iom->iom_maxmmepages)
#define FLUSHBATCH_SIGNAL(fb) fb->fb_signal = TRUE;
#endif

/*#***********************************************************************\
 *
 *		iomgr_flushbatch_init
 *
 * Creates a flush batch object
 *
 * Parameters :
 *
 *	wakeupfp - in, hold
 *          callback function pointer taht is to be called
 *          upon completion of the flush batch
 *
 *	wakeupctx - in, hold
 *		context parameter to be given to be given to wakeupfp
 *
 * Return value - give :
 *      new flushbatch object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_iomgr_flushbatch_t* iomgr_flushbatch_init(
        dbe_iomgr_t* iomgr,
        bool mme,
        void (*wakeupfp)(void*),
        void* wakeupctx,
        void (*spacefp)(void*),
        void* spacectx)
{
        dbe_iomgr_flushbatch_t* flushbatch;

        flushbatch = SSMEM_NEW(dbe_iomgr_flushbatch_t);
        flushbatch->fb_nleft = 0; /* all including mme pages */
        flushbatch->fb_wakeupfp = wakeupfp;
        flushbatch->fb_wakeupctx = wakeupctx;
#ifdef SS_MME
        flushbatch->fb_mme = mme; /* just mme pages */
        flushbatch->fb_signal = FALSE;
        flushbatch->fb_spacefp = spacefp;
        flushbatch->fb_spacectx = spacectx;
        if (mme) {
            iomgr->iom_mme_nflushed = 0;
            iomgr->iom_mme_ntoflush = 0;
        } else
#endif /* SS_MME */
        {
            iomgr->iom_dbe_nflushed = 0;
            iomgr->iom_dbe_ntoflush = 0;
        }
        flushbatch->fb_iomgr = iomgr;
            

        return (flushbatch);
}

/*#***********************************************************************\
 *
 *		dbe_iomgr_flushbatch_init
 *
 * Creates a flush batch object.
 * Has to be freed with dbe_iomgr_flushbatch_done.
 *
 * Parameters :
 *
 *	wakeupfp - in, hold
 *          callback function pointer taht is to be called
 *          upon completion of the flush batch
 *
 *	wakeupctx - in, hold
 *		context parameter to be given to be given to wakeupfp
 *
 *	spacefp - in, hold
 *          callback function pointer that is to be called
 *          upon there is again space in the batch
 *
 *	spacectx - in, hold
 *		context parameter to be given to be given to spacefp

 *
 * Return value - give :
 *      new flushbatch object
 *
 * Comments : AT THE MOMENT IT'S PRESUMED THAT THIS IS ALWAYS MME BATCH!!!
 *
 * Globals used :
 *
 * See also :
 */
dbe_iomgr_flushbatch_t* dbe_iomgr_flushbatch_init(
        dbe_iomgr_t* iomgr,
        void (*wakeupfp)(void*),
        void* wakeupctx,
        void (*spacefp)(void*),
        void* spacectx)
{
        return(iomgr_flushbatch_init(iomgr, TRUE, wakeupfp, wakeupctx, spacefp, spacectx));
}

void dbe_iomgr_flushbatch_done(
        dbe_iomgr_t* iomgr __attribute__ ((unused)),
        dbe_iomgr_flushbatch_t* fb)
{
        ss_rc_dassert(fb->fb_nleft == 0, fb->fb_nleft);
        SsMemFree(fb);
}

uint dbe_iomgr_flushbatch_nleft(
        dbe_iomgr_t* iomgr __attribute__ ((unused)),
        dbe_iomgr_flushbatch_t* fb)
{
        return(fb->fb_nleft);
}

#ifdef SS_MME
size_t dbe_iomgr_get_mme_nflushed(dbe_iomgr_t* iomgr)
{
        return (iomgr->iom_mme_nflushed);
}
size_t dbe_iomgr_get_mme_ntoflush(dbe_iomgr_t* iomgr)
{
        return (iomgr->iom_mme_ntoflush);
}
#endif /* SS_MME */
size_t dbe_iomgr_get_dbe_nflushed(dbe_iomgr_t* iomgr)
{
        return (iomgr->iom_dbe_nflushed);
}
size_t dbe_iomgr_get_dbe_ntoflush(dbe_iomgr_t* iomgr)
{
        return (iomgr->iom_dbe_ntoflush);
}

#ifdef SS_FAKE
static void SS_CALLBACK iomgr_postpone_flushbatch_wakeup_done_thr(void* fb)
{
        dbe_iomgr_flushbatch_t* flushbatch = fb;
        ss_dprintf_1((
             "FAKE: make cache flushing completion take 20 more secs.\n"));
        SsThrSleep(20000);
        ss_dprintf_1(("FAKE: now waking up checkpoint task\n"));
        (*flushbatch->fb_wakeupfp)(flushbatch->fb_wakeupctx);
#ifdef SS_MME
        if (!flushbatch->fb_mme) {
            SsMemFree(flushbatch);
        }
#endif

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}
#endif /* SS_FAKE */

/*#***********************************************************************\
 *
 *		iomgr_flushbatch_wakeup_done
 *
 * Wakes up the task that is waiting for flush batch completion
 * and then frees the flush batch object
 *
 * Parameters :
 *
 *	flushbatch - in, take
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
static void iomgr_flushbatch_wakeup_done(dbe_iomgr_flushbatch_t* flushbatch)
{
        bool mme;
#ifdef SS_MME
        mme = flushbatch->fb_mme;
#else /* SS_MME */
        mme = FALSE;
#endif /* SS_MME */
        ss_dprintf_2(("iomgr_flushbatch_wakeup_done: flushbatch completed\n"));
        FAKE_CODE_RESET(
            FAKE_DBE_CPFLUSH_WAIT_20S,
            {if (!mme) {
                SsThreadT* thr =
                    SsThrInitParam(iomgr_postpone_flushbatch_wakeup_done_thr,
                           "FAKE_DBE_CPFLUSH_WAIT_20S",
                           0, flushbatch);
                SsThrEnable(thr);
                SsThrDone(thr); /* does not stop thread itself! */
                return;
            }});
        (*flushbatch->fb_wakeupfp)(flushbatch->fb_wakeupctx);
        if (!mme) {
            SsMemFree(flushbatch);
        }
}


/*#***********************************************************************\
 *
 *		iomgr_flushbatch_dec
 *
 * Decrements one page from flush batch and if it is the last
 * one also deletes the flushbatch object and wakes the waiting task
 *
 * Parameters :
 *
 *	flushbatch - use
 *		flush batch object
 *
 *	cache - use
 *		dbe cache object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_flushbatch_dec(
        dbe_iomgr_flushbatch_t* flushbatch,
        dbe_cache_t* cache __attribute__ ((unused)),
        bool mmepage __attribute__ ((unused)))
{
#ifdef SS_MME
        ss_debug(mmepage ?  mmeflush_ntoflush-- : mtflush_ntoflush--;)
#else
        ss_debug(mtflush_ntoflush--;)
#endif

        if (flushbatch != NULL) {
            uint nleft;

            nleft = --(flushbatch->fb_nleft);
#ifdef SS_MME
            if (flushbatch->fb_signal) {
                if (nleft < flushbatch->fb_iomgr->iom_maxmmepages/2) {
                    if (flushbatch->fb_spacefp != NULL) {
                        flushbatch->fb_spacefp(flushbatch->fb_spacectx);
                    }
                    flushbatch->fb_signal = FALSE;
                }
            }
            if (mmepage) {
                flushbatch->fb_iomgr->iom_mme_nflushed++;
            } else
#endif /* SS_MME */
            {
                flushbatch->fb_iomgr->iom_dbe_nflushed++;
            }
            if (nleft == 0) {
#ifdef SS_MME
                ss_debug(if(mmepage) {
                    ss_rc_dassert(mmeflush_ntoflush == 0, (int)mmeflush_ntoflush);
                    ss_debug(mmeflush_active = FALSE);
                } else {
                    ss_rc_dassert(mtflush_ntoflush == 0, (int)mtflush_ntoflush);
                    ss_debug(mtflush_active = FALSE);
                })
#else
                ss_rc_dassert(mtflush_ntoflush == 0, (int)mtflush_ntoflush);
                ss_debug(mtflush_active = FALSE);
#endif
                ss_debug(if(!mmepage) {
                    dbe_cache_checkafterflush(cache);
                })
                iomgr_flushbatch_wakeup_done(flushbatch);
            }
            if ((nleft % 16U) == 0) {
                ss_svc_notify_done();
            }
        }
}

/*#***********************************************************************\
 *
 *		iomgr_flushbatch_add
 *
 * Adds n to flushbatch page count
 *
 * Parameters :
 *
 *	flushbatch - use
 *		flush batch object
 *
 *	n - in
 *		number of pages to be added to batch
 *
 *      mmepage - in
 *          TRUE if MME-page
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_flushbatch_add(
        dbe_iomgr_flushbatch_t* flushbatch,
        size_t n,
        bool mmepage __attribute__ ((unused)))
{
        flushbatch->fb_nleft += n;
#ifdef SS_FAKE
        if (!mmepage) {
            FAKE_CODE_BLOCK(FAKE_DBE_DBE_FLUSHBATCH_LEAK,
                            flushbatch->fb_nleft += 1;
                            flushbatch->fb_iomgr->iom_dbe_ntoflush += 1;);
                            
        }
#ifdef SS_MME
        else {
            FAKE_CODE_RESET(FAKE_DBE_MME_FLUSHBATCH_LEAK,
                            flushbatch->fb_nleft += 1;
                            flushbatch->fb_iomgr->iom_mme_ntoflush += 1;);
        }
#endif /* SS_MME */
#endif /* SS_FAKE */
        if (!mmepage) {
            flushbatch->fb_iomgr->iom_dbe_ntoflush += n;
        } 
#ifdef SS_MME
        else {
            flushbatch->fb_iomgr->iom_mme_ntoflush += n;
        }
#ifdef SS_DEBUG
        if (mmepage) {
            if (!mmeflush_active) {
                mmeflush_active = TRUE;
                mmeflush_ntoflush = 0;
            }
            mmeflush_ntoflush += (n);
            FAKE_IF(FAKE_DBE_FLUSHBATCH_LEAK_IGNOREDEBUG) {
            } else {
                ss_dassert(mmeflush_ntoflush == flushbatch->fb_nleft);
            }
        } else {
            if (!mtflush_active) {
                mtflush_active = TRUE;
                mtflush_ntoflush = 0;
            }
            mtflush_ntoflush += (n);
            FAKE_IF(FAKE_DBE_FLUSHBATCH_LEAK_IGNOREDEBUG) {
            } else {
                ss_dassert(mtflush_ntoflush == flushbatch->fb_nleft);
            }
        }
#endif /* SS_DEBUG */
#else
        ss_debug(
                if (!mtflush_active) {
                    mtflush_active = TRUE;
                    mtflush_ntoflush = 0;
                }
                mtflush_ntoflush += (n);
                ss_dassert(mtflush_ntoflush == flushbatch->fb_nleft);
        );
#endif
}

#endif /* DBE_MTFLUSH */

/*#***********************************************************************\
 *
 *		request_init
 *
 * Creates an I/O request object
 *
 * Parameters :
 *
 *	reqtype - in
 *		request type
 *
 *	daddr - in
 *		disk address
 *
 *	p_reqdata - in, use
 *          pointer to possible request data context or NULL
 *
 * Return value - give :
 *      pointer to created request object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_request_t* request_init(
        iomgr_reqtype_t reqtype,
        su_daddr_t daddr,
        iomgr_reqdata_t* p_reqdata,
        dbe_info_flags_t infoflags)
{
        iomgr_request_t* req;

        ss_dprintf_3(("request_init\n"));

        req = SSMEM_NEW(iomgr_request_t);
        req->ior_type = reqtype;
        req->ior_daddr = daddr;
        req->ior_wait = NULL;
        req->ior_expired = FALSE;
        req->ior_infoflags = infoflags;
        if (p_reqdata != NULL) {
            req->ior_ = *p_reqdata;
        } else {
            memset(&req->ior_, 0, sizeof(req->ior_));
        }
        return (req);
}


/*#***********************************************************************\
 *
 *		request_cmp
 *
 * Compares two request for ordering the requests in the queue.
 *
 * Parameters :
 *
 *	r1 - in, use
 *		pointer to req1
 *
 *	r2 - in, use
 *		pointer to req2
 *
 * Return value :
 *      Result of logical subtraction of the request addresses:
 *          = 0 if r1 == r2
 *          < 0 if r1 < r2
 *          > 0 if r1 > r2
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int request_cmp(void* r1, void* r2)
{
        iomgr_request_t* req1 = r1;
        iomgr_request_t* req2 = r2;

        if (sizeof(int) >= sizeof(su_daddr_t)) {
            return ((int)req1->ior_daddr - (int)req2->ior_daddr);
        }
        if (req1->ior_daddr < req2->ior_daddr) {
            return (-1);
        }
        if (req1->ior_daddr > req2->ior_daddr) {
            return (1);
        }
        return (0);
}

/*#***********************************************************************\
 *
 *		request_addwait
 *
 * Adds wait ticket object into request
 *
 * Parameters :
 *
 *	iomgr - use
 *		pointer to I/O manager object
 *
 *	req - in out, use
 *		pointer to request object
 *
 *	waitobj - in out, use
 *		pointer to wait ticket
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void request_addwait(
            dbe_iomgr_t* iomgr,
            iomgr_request_t* req,
            iomgr_wait_t* waitobj)
{
        ss_dprintf_3(("request_addwait\n"));

        ss_dassert(req != NULL);
        waitobj->iow_mes = su_meslist_mesinit(iomgr->iom_meslist);
        waitobj->iow_next = req->ior_wait;
        req->ior_wait = waitobj;
}

/*#***********************************************************************\
 *
 *		iomgr_wait
 *
 * Waits for a wait ticket object to be signaled
 *
 * Parameters :
 *
 *      iomgr - use
 *          I/O manager
 *
 *	waitobj - in out, use
 *		pointer to wait ticket
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_wait(
        dbe_iomgr_t* iomgr,
        iomgr_wait_t* waitobj)
{
        ss_dprintf_3(("iomgr_wait\n"));

        su_mes_wait(waitobj->iow_mes);
        su_meslist_mesdone(iomgr->iom_meslist, waitobj->iow_mes);
}

/*#***********************************************************************\
 *
 *		iomgr_wakeup
 *
 * Wakes up the threads that are blocked to wait ticket
 *
 * Parameters :
 *
 *	p_waitobj - in out, use
 *		pointer to pointer to wait ticket object.
 *          This functions also resets *p_waitobj to NULL
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_wakeup(
        iomgr_wait_t** p_waitobj)
{
        iomgr_wait_t* waitobj;
        iomgr_wait_t* waitobj_next;

        ss_dprintf_3(("iomgr_wakeup\n"));
        ss_dassert(p_waitobj != NULL);

        waitobj = *p_waitobj;
        *p_waitobj = NULL;
        while (waitobj != NULL) {
            waitobj_next = waitobj->iow_next;
            su_mes_send(waitobj->iow_mes);
            waitobj = waitobj_next;
        }
}

/*#***********************************************************************\
 *
 *		request_done
 *
 * Deletes an I/O request object
 *
 * Parameters :
 *
 *	req - in, take
 *		pointer to request object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void request_done(iomgr_request_t* req)
{
        ss_dprintf_3(("request_done\n"));

        ss_dassert(req->ior_wait == NULL);
        SsMemFree(req);
}

/* These two variables are needed for synchronization of
 * the I/O threads
 */
static SsMesT*              devqueue_threadstartmsg = NULL;
static iomgr_devqueue_t*    devqueue_juststarting = NULL;

#ifdef DBE_NONBLOCKING_PAGEFLUSH
static void iomgr_allocslotbuf(
        SsCacMemT** p_cacmem,
        ss_byte_t** p_buf,
        size_t* p_bufsize,
        dbe_filedes_t* filedes)
{
        if (*p_cacmem != NULL && *p_bufsize < filedes->fd_blocksize) {
            SsCacMemFree(*p_cacmem, *p_buf);
            SsCacMemDone(*p_cacmem);
            *p_cacmem = NULL;
        }
        if (*p_cacmem == NULL) {
            *p_cacmem = SsCacMemInit(filedes->fd_blocksize, 1);
            *p_bufsize = filedes->fd_blocksize;
            *p_buf = SsCacMemAlloc(*p_cacmem);
        }
}

static void iomgr_freeslotbuf(
        SsCacMemT** p_cacmem,
        ss_byte_t** p_buf)
{
        if (*p_cacmem != NULL) {
            ss_dassert(*p_buf != NULL);
            SsCacMemFree(*p_cacmem, *p_buf);
            SsCacMemDone(*p_cacmem);
            *p_cacmem = NULL;
            *p_buf = NULL;
        }
}

#endif /* DBE_NONBLOCKING_PAGEFLUSH */

/*#***********************************************************************\
 *
 *		iomgr_threadfun
 *
 * The I/O device thread function that fulfils the I/O requests
 * for that device.
 *
 * Parameters : 	 - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_threadfun(void)
{
        iomgr_devqueue_t*   devqueue;
        iomgr_request_t*    req;
        iomgr_reqtype_t     reqtype;
        su_rbt_node_t*      treenode;
        dbe_filedes_t*      filedes;
        dbe_cacheslot_t*    cacheslot;
        su_mes_t*           waitmes;
#ifdef DBE_NONBLOCKING_PAGEFLUSH
        size_t              blocksize = 0;
        SsCacMemT*          cacmem = NULL;
        ss_byte_t*          slotbuf = NULL;
#ifdef MME_CP_FIX
        ulong                   i;
        dbe_cache_flushaddr_t   flushes[DBE_CACHE_MAXNFLUSHES];
        ulong                   nflushes;
        iomgr_request_t*        reqs[DBE_CACHE_MAXNFLUSHES];
        bool                    reqispending;
#endif
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
        ss_beta(int thrinfoid;)

        ss_pprintf_1(("Started I/O manager thread, id = %u\n", SsThrGetid()));
        SS_PUSHNAME("iomgr_threadfun");
        ss_beta(thrinfoid = SsThrInfoInit((char *)"iomgr_threadfun");)

        /* Get startup data and synchronize */
        devqueue = devqueue_juststarting;
        ss_dassert(devqueue != NULL);
        devqueue_juststarting = NULL;
        ss_dassert(devqueue_threadstartmsg != NULL);
        ss_debug(iomgr_nthreads++);
        SsMesSend(devqueue_threadstartmsg);
#ifdef MME_CP_FIX
        reqispending = FALSE;
#endif

        ss_dprintf_1(("iomgr_threadfun: starting thread\n"));
        SsSemEnter(devqueue->iodq_mutex);
        for (;;) {
            SS_THRINFO_LOOP(thrinfoid);
            treenode = su_rbt_min(devqueue->iodq_reqpool, NULL);
            if (treenode == NULL) {
                if (devqueue->iodq_stop) {
                    ss_dprintf_1(("iomgr_threadfun: devqueue->iodq_stop = TRUE\n"));
                    ss_debug(iomgr_nthreads--);

                    SsMesSend(devqueue->iodq_emptymsg);
#ifdef SS_NTHREADS_PER_DEVQUEUE
                    devqueue->iodq_nthrrunning--;
#else
                    devqueue->iodq_thrstopped = TRUE;
#endif
                    SsSemExit(devqueue->iodq_mutex);
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                    iomgr_freeslotbuf(&cacmem, &slotbuf);
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                    SS_POPNAME;
                    
#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
                    return;
#else
                    SsThrExit();
#endif
                } else {
                    SsMesSend(devqueue->iodq_emptymsg);
#ifndef SS_NTHREADS_PER_DEVQUEUE
                    SsMesReset(devqueue->iodq_wait); /* To avoid false wakeup */
#endif
                    ss_debug(iomgr_nthreads_sleeping++);
                    ss_dprintf_1(("iomgr_threadfun: start wait\n"));
#ifdef SS_NTHREADS_PER_DEVQUEUE
                    waitmes = su_meslist_mesinit(&devqueue->iodq_meslist);
                    su_meswaitlist_add(devqueue->iodq_meswaitlist, waitmes);
                    SsSemExit(devqueue->iodq_mutex);

                    /* Wait for wakeup */
                    su_mes_wait(waitmes);

                    SsSemEnter(devqueue->iodq_mutex);
                    su_meslist_mesdone(&devqueue->iodq_meslist, waitmes);
#else
                    SsSemExit(devqueue->iodq_mutex);
                    SsMesWait(devqueue->iodq_wait);
                    SsSemEnter(devqueue->iodq_mutex);
#endif
                    ss_dprintf_1(("iomgr_threadfun: stop wait\n"));
                    ss_debug(iomgr_nthreads_sleeping--);
                    treenode = su_rbt_min(devqueue->iodq_reqpool, NULL);
                }
            }
#ifdef MME_CP_FIX
            nflushes = 0;
#endif
            while (treenode != NULL) {
#ifdef SS_NTHREADS_PER_DEVQUEUE
                req = su_rbtnode_getkey(treenode);
                memcpy(&devqueue->iodq_req, req, sizeof(devqueue->iodq_req));
                ss_debug(devqueue->iodq_req.ior_wait = (void*)0xbabebabe;)
#else
                req = su_rbtnode_getkey(treenode);
#endif
                su_rbt_delete(devqueue->iodq_reqpool, treenode);
                SsSemExit(devqueue->iodq_mutex);
                filedes = devqueue->iodq_filedes;
                ss_dassert(filedes != NULL);
#ifdef MME_CP_FIX
                reqispending = FALSE;
#endif
                switch (reqtype = req->ior_type) {
                    case IOM_CACHE_REACH:
                        ss_rc_dassert(devqueue->iodq_type == IOM_DEVQUEUE_READ
                                   || devqueue->iodq_type ==
                                      IOM_DEVQUEUE_READWRITE,
                                      devqueue->iodq_type);
                        ss_dassert(req->ior_wait != NULL);
                        ss_dprintf_2(("iomgr_threadfun: %s addr=%lu\n",
                            "REACH",
                            req->ior_daddr));
                        cacheslot = dbe_cache_reach(
                                        filedes->fd_cache,
                                        req->ior_daddr,
                                        DBE_CACHE_READONLY,
                                        req->ior_infoflags,
                                        NULL,
                                        iom_request);
                        ss_dassert(cacheslot != NULL);
                        ss_debug(SsThrSwitch());
                        dbe_cache_release(
                            filedes->fd_cache,
                            cacheslot,
                            DBE_CACHE_CLEAN,
                            iom_request);
                        break;
                    case IOM_CACHE_PREFETCH:
                        ss_dprintf_2(("iomgr_threadfun: %s addr=%lu\n",
                            "PREFETCH",
                            req->ior_daddr));
                        ss_rc_dassert(devqueue->iodq_type == IOM_DEVQUEUE_READ
                                   || devqueue->iodq_type ==
                                      IOM_DEVQUEUE_READWRITE,
                                      devqueue->iodq_type);
                        if (!req->ior_expired) {
                            cacheslot =
                                dbe_cache_reach(
                                        filedes->fd_cache,
                                        req->ior_daddr,
                                        DBE_CACHE_PREFETCH,
                                        req->ior_infoflags,
                                        NULL,
                                        iom_prefetch);
                            ss_dassert(cacheslot != NULL);
                            ss_debug(SsThrSwitch());
                            dbe_cache_release(
                                    filedes->fd_cache,
                                    cacheslot,
                                    DBE_CACHE_CLEAN,
                                    iom_prefetch);
                        }
                        break;
                    case IOM_CACHE_PREFLUSH:
                        ss_dprintf_2(("iomgr_threadfun: PREFLUSH addr=%lu\n",
                            req->ior_daddr));
#if defined(DBE_MTFLUSH)
                        /* This function is only available
                         * if DBE_MTFLUSH is on
                         */
                        ss_rc_dassert(devqueue->iodq_type == IOM_DEVQUEUE_WRITE
                                   || devqueue->iodq_type ==
                                      IOM_DEVQUEUE_READWRITE,
                                      devqueue->iodq_type);
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                        iomgr_allocslotbuf(
                                &cacmem,
                                &slotbuf,
                                &blocksize,
                                filedes);
                        dbe_cache_flushaddr(
                            filedes->fd_cache,
                            req->ior_daddr,
                            TRUE /* TRUE means: this is preflush */
#ifdef SS_MME
                            , req->ior_.mmepage
#endif /* SS_MME */
                            
                            , slotbuf,
                            req->ior_infoflags
                            );
#else /* DBE_NONBLOCKING_PAGEFLUSH */
                        dbe_cache_flushaddr(
                            filedes->fd_cache,
                            req->ior_daddr,
                            TRUE /* TRUE means: this is preflush */
#ifdef SS_MME
                            , req->ior_.mmepage
#endif /* SS_MME */
                            ,req->ior_infoflags);
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                        break;
#else /* DBE_MTFLUSH */
                        cacheslot = dbe_cache_reach(
                                        filedes->fd_cache,
                                        req->ior_daddr,
                                        DBE_CACHE_PREFLUSHREACH,
                                        req->ior_infoflags,
                                        NULL,
                                        iom_preflush);
                        if (cacheslot != NULL) {
                            ss_debug(SsThrSwitch());
                            dbe_cache_release(
                                filedes->fd_cache,
                                cacheslot,
                                DBE_CACHE_PREFLUSH,
                                iom_preflush);
                        }
                        break;
#endif /* DBE_MTFLUSH */
                    case IOM_CACHE_FLUSHPAGE:
                    case IOM_CACHE_FLUSHPAGELASTUSE:
                        ss_dprintf_2(("iomgr_threadfun: %s addr=%lu\n",
                            reqtype == IOM_CACHE_FLUSHPAGE ?
                                "FLUSHPAGE" : "FLUSHPAGELASTUSE",
                            req->ior_daddr));
                        ss_dassert(req->ior_.cacheslot != NULL);
                        ss_rc_dassert(devqueue->iodq_type == IOM_DEVQUEUE_WRITE
                                   || devqueue->iodq_type ==
                                      IOM_DEVQUEUE_READWRITE,
                                      devqueue->iodq_type);
                        cacheslot = req->ior_.cacheslot;
                        dbe_cache_release(
                            filedes->fd_cache,
                            cacheslot,
                            reqtype == IOM_CACHE_FLUSHPAGE ?
                                DBE_CACHE_FLUSH : DBE_CACHE_FLUSHLASTUSE,
                            iom_request);
                        break;
#if defined(DBE_MTFLUSH)
                    case IOM_CACHE_FLUSHPAGEBATCH:
                        ss_dprintf_2(("iomgr_threadfun: flush addr=%lu\n",
                            req->ior_daddr));
                        ss_rc_dassert(devqueue->iodq_type == IOM_DEVQUEUE_WRITE
                                   || devqueue->iodq_type ==
                                      IOM_DEVQUEUE_READWRITE,
                                      devqueue->iodq_type);
#ifdef DBE_NONBLOCKING_PAGEFLUSH
                        iomgr_allocslotbuf(
                                &cacmem,
                                &slotbuf,
                                &blocksize,
                                filedes);
#ifdef MME_CP_FIX
                        flushes[nflushes].fa_addr = req->ior_daddr;
                        flushes[nflushes].fa_preflush = FALSE;
                        flushes[nflushes].fa_mmeslot = req->ior_.mmepage;
                        flushes[nflushes].fa_writebuf = NULL;
                        flushes[nflushes].fa_infoflags = req->ior_infoflags;
                        reqs[nflushes] = req;
                        reqispending = TRUE;
                        nflushes++;

                        if (nflushes == DBE_CACHE_MAXNFLUSHES) {
                            dbe_cache_flushaddr_n(
                                    filedes->fd_cache,
                                    flushes,
                                    nflushes);
                            SsSemEnter(devqueue->iodq_mutex);
                            for (i = 0; i < nflushes; i++) {
                                if (reqs[i]->ior_type
                                    == IOM_CACHE_FLUSHPAGEBATCH) {
                                    iomgr_flushbatch_dec(
                                            reqs[i]->ior_.flushbatch,
                                            filedes->fd_cache,
                                            reqs[i]->ior_.mmepage);
                                }
                                iomgr_wakeup(&reqs[i]->ior_wait);
                                request_done(reqs[i]);
                            }

                            nflushes = 0;
                            SsSemExit(devqueue->iodq_mutex);
                        }
#else
                        dbe_cache_flushaddr(
                                filedes->fd_cache,
                                req->ior_daddr,
                                FALSE /* FALSE means: this is NOT preflush */
#ifdef SS_MME
                                , req->ior_.mmepage
#endif /* SS_MME */
                                , slotbuf,
                                req->ior_infoflags);
#endif /* MME_CP_FIX */
#else /* DBE_NONBLOCKING_PAGEFLUSH */
                        dbe_cache_flushaddr(
                                filedes->fd_cache,
                                req->ior_daddr,
                                FALSE /* FALSE means: this is NOT preflush */
#ifdef SS_MME
                                , req->ior_.mmepage
#endif /* SS_MME */
                                , req->ior_infoflags);
#endif /* DBE_NONBLOCKING_PAGEFLUSH */
                        break;
#endif /* DBE_MTFLUSH */
                    default:
                        ss_rc_error(reqtype);
                }
                SsSemEnter(devqueue->iodq_mutex);
#if defined(DBE_MTFLUSH) && !defined(MME_CP_FIX)
                if (reqtype == IOM_CACHE_FLUSHPAGEBATCH) {
                    iomgr_flushbatch_dec(
                            req->ior_.flushbatch,
                            filedes->fd_cache,
#ifdef SS_MME
                            req->ior_.mmepage
#else
                            FALSE
#endif
                        );
                }
#endif /* DBE_MTFLUSH */
                
#ifdef MME_CP_FIX
                if (!reqispending)
#endif
                    iomgr_wakeup(&req->ior_wait); /* wake up waiting threads */

#ifdef SS_NTHREADS_PER_DEVQUEUE
                treenode = su_rbt_search_atleast(
                                devqueue->iodq_reqpool,
                                &devqueue->iodq_req);
#else
                treenode = su_rbt_search_atleast(
                                devqueue->iodq_reqpool,
                                req);
#endif
#ifdef MME_CP_FIX
                if (!reqispending)
#endif
                    request_done(req);
            }
            ss_dassert(treenode == NULL);
            ss_dprintf_1(("iomgr_threadfun: req pool empty\n"));

#ifdef MME_CP_FIX
            if (nflushes > 0) {
                SsSemExit(devqueue->iodq_mutex);
                dbe_cache_flushaddr_n(
                        filedes->fd_cache,
                        flushes,
                        nflushes);
                SsSemEnter(devqueue->iodq_mutex);
                for (i = 0; i < nflushes; i++) {
                    if (reqs[i]->ior_type
                        == IOM_CACHE_FLUSHPAGEBATCH) {
                        iomgr_flushbatch_dec(
                                reqs[i]->ior_.flushbatch,
                                filedes->fd_cache,
                                reqs[i]->ior_.mmepage);
                    }
                    iomgr_wakeup(&reqs[i]->ior_wait);
                    request_done(reqs[i]);
                }
                
                nflushes = 0;
            }
#endif
        }
}

/*#***********************************************************************\
 *
 *		devqueue_init
 *
 * Creates a device queue object and starts the thread
 *
 * Parameters :
 *
 *	iomgr - in out, hold
 *		pointer to I/O manager object
 *
 * Return value - give :
 *      pointer to created I/O device queue
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_devqueue_t* devqueue_init(
        dbe_iomgr_t* iomgr,
        uint nthreads,
        iomgr_devqueuetype_t dqtype)
{
        iomgr_devqueue_t* devqueue;
#ifdef SS_NTHREADS_PER_DEVQUEUE
        uint i;
#endif
        ss_dprintf_3(("devqueue_init\n"));

        devqueue = SSMEM_NEW(iomgr_devqueue_t);
        devqueue->iodq_reqpool =
            su_rbt_init(request_cmp, (void (*)(void*))NULL);

        ss_autotest(su_rbt_maxnodes(devqueue->iodq_reqpool, 30000));

#ifdef SS_NTHREADS_PER_DEVQUEUE
        devqueue->iodq_type = dqtype;
        devqueue->iodq_nthreads = nthreads;
        devqueue->iodq_threads = SsMemAlloc(nthreads * sizeof(SsThreadT*));
        for (i=0; i < nthreads; i++) {
            devqueue->iodq_threads[i] = SsThrInit((SsThrFunT)iomgr_threadfun, "iomgr_threadfun", 8192);
        }
        (void)su_meslist_init(&devqueue->iodq_meslist);
        devqueue->iodq_meswaitlist = su_meswaitlist_init();
#else
        devqueue->iodq_thread = SsThrInit((SsThrFunT)iomgr_threadfun, "iomgr_threadfun", 8192);
#endif
        devqueue->iodq_emptymsg = SsMesCreateLocal();
        devqueue->iodq_mutex = iomgr->iom_mutex;
        ss_debug(su_rbt_setdebugsem(devqueue->iodq_reqpool, devqueue->iodq_mutex));
        devqueue->iodq_file = iomgr->iom_file;
        devqueue->iodq_filedes = iomgr->iom_filedes;
        devqueue->iodq_iomgr = iomgr;
        devqueue->iodq_stop = FALSE;
#ifdef SS_NTHREADS_PER_DEVQUEUE
        devqueue->iodq_nthrrunning = 0;
#else
        devqueue->iodq_thrstopped = FALSE;
        devqueue->iodq_wait = SsMesCreateLocal();
#endif

        if (devqueue_threadstartmsg == NULL) {
            devqueue_threadstartmsg = SsMesCreateLocal();
        }
        ss_dassert(devqueue_juststarting == NULL);

#ifdef SS_NTHREADS_PER_DEVQUEUE
        for (i=0; i < nthreads; i++) {
            devqueue_juststarting = devqueue;
            SsThrEnable(devqueue->iodq_threads[i]);
            SsMesWait(devqueue_threadstartmsg);
            devqueue->iodq_nthrrunning++;
            ss_dassert(devqueue_juststarting == NULL);
        }
#else
        devqueue_juststarting = devqueue;
        SsThrEnable(devqueue->iodq_thread);
        SsMesWait(devqueue_threadstartmsg);
        ss_dassert(devqueue_juststarting == NULL);
#endif
        SsMesFree(devqueue_threadstartmsg);
        devqueue_threadstartmsg = NULL;

        return (devqueue);
}

/*#***********************************************************************\
 *
 *		devqueue_done
 *
 * Deletes a device queue
 *
 * Parameters :
 *
 *	devqueue - in, take
 *		pointer to queue
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void devqueue_done(iomgr_devqueue_t* devqueue)
{
#ifdef SS_NTHREADS_PER_DEVQUEUE
        uint nthrrunning = 0;
        int i;
#else
        bool thrstopped = FALSE;
#endif

        ss_dprintf_3(("devqueue_done\n"));

        for (;;) {
            /* wait till I/O request pool is empty */
            SsSemEnter(devqueue->iodq_mutex);
            if (su_rbt_nelems(devqueue->iodq_reqpool) == 0) {
                ss_dprintf_1(("devqueue_done(): req pool is empty\n"));
                SsSemExit(devqueue->iodq_mutex);
                break;
            }
            SsSemExit(devqueue->iodq_mutex);
            ss_dprintf_1(("devqueue_done(): wait iodq_emptymsg\n"));
            SsMesWait(devqueue->iodq_emptymsg);
        }
        devqueue->iodq_stop = TRUE;
        SsSemEnter(devqueue->iodq_mutex);
        ss_dprintf_1(("devqueue_done(): wake up thread\n"));
#ifdef SS_NTHREADS_PER_DEVQUEUE
        su_meswaitlist_wakeupall(devqueue->iodq_meswaitlist);
        SsSemExit(devqueue->iodq_mutex);
#else
        SsSemExit(devqueue->iodq_mutex);
        SsMesSend(devqueue->iodq_wait);
#endif
        /*
         * Wait for the mutex here in order to avoid race condition
         * between the thread setting the iodq_thrstopped flag
         *
         */
        SsSemEnter(devqueue->iodq_mutex);
#ifdef SS_NTHREADS_PER_DEVQUEUE
        nthrrunning = devqueue->iodq_nthrrunning;
#else
        thrstopped = devqueue->iodq_thrstopped;
#endif
        SsSemExit(devqueue->iodq_mutex);

#ifdef SS_NTHREADS_PER_DEVQUEUE
        while (nthrrunning > 0) {
#else
        while (!thrstopped) {
#endif
#ifdef SS_NTHREADS_PER_DEVQUEUE
            SsSemEnter(devqueue->iodq_mutex);
            su_meswaitlist_wakeupall(devqueue->iodq_meswaitlist);
            SsSemExit(devqueue->iodq_mutex);
            SsMesRequest(devqueue->iodq_emptymsg, 1000);
#else
            ss_dprintf_1(("devqueue_done(): wait iodq_emptymsg pos.2\n"));
            SsMesWait(devqueue->iodq_emptymsg);
#endif
            /*
             * Wait for the mutex here in order to avoid race condition
             * between the thread setting the iodq_thrstopped flag
             *
             */

            SsSemEnter(devqueue->iodq_mutex);
#ifdef SS_NTHREADS_PER_DEVQUEUE
            nthrrunning = devqueue->iodq_nthrrunning;
#else
            thrstopped = devqueue->iodq_thrstopped;
#endif
            SsSemExit(devqueue->iodq_mutex);
        }
        SsThrSleep(100);

        ss_debug(su_rbt_setdebugsem(devqueue->iodq_reqpool, NULL));

#ifdef SS_NTHREADS_PER_DEVQUEUE
        for (i=0; i < (int)devqueue->iodq_nthreads; i++) {
            SsThrDone(devqueue->iodq_threads[i]);
        }
        SsMemFree(devqueue->iodq_threads);
        su_meslist_done(&devqueue->iodq_meslist);
        ss_bassert(su_meswaitlist_isempty(devqueue->iodq_meswaitlist));
#else
        SsThrDone(devqueue->iodq_thread);
        SsMesFree(devqueue->iodq_wait);
#endif
        su_rbt_done(devqueue->iodq_reqpool);
        SsMesFree(devqueue->iodq_emptymsg);
        SsMemFree(devqueue);
}

/*#***********************************************************************\
 *
 *		devqueue_addreq
 *
 * Adds a request to a device queue
 *
 * Parameters :
 *
 *	devqueue - in out, use
 *		pointer to device queue
 *
 *	daddr - in
 *		disk address in file
 *
 *	reqtype - in
 *          request type
 *
 *	p_reqdata - in, use
 *		pointer to possible request data or NULL
 *
 * Return value - ref :
 *      pointer to inserted request object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_request_t* devqueue_addreq(
        iomgr_devqueue_t* devqueue,
        su_daddr_t daddr,
        iomgr_reqtype_t reqtype,
        iomgr_reqdata_t* p_reqdata,
        dbe_info_flags_t infoflags)
{
        iomgr_request_t* req;
        su_rbt_node_t* treenode;
        iomgr_request_t dummy_req;

        dummy_req.ior_daddr = daddr;
        treenode = su_rbt_search(devqueue->iodq_reqpool, &dummy_req);
        if (treenode == NULL) {
            ss_dprintf_3(("devqueue_addreq:daddr=%ld, new request\n", daddr));
            req = request_init(reqtype, daddr, p_reqdata, infoflags);
            su_rbt_insert(devqueue->iodq_reqpool, req);
        } else {
            ss_dprintf_3(("devqueue_addreq:daddr=%ld, old request\n", daddr));
            req = su_rbtnode_getkey(treenode);
#if defined(DBE_MTFLUSH)
            {
                dbe_filedes_t* filedes;
                dbe_cacheslot_t* cacheslot;

                req->ior_expired = FALSE;
                switch (reqtype) {
                    case IOM_CACHE_PREFETCH:
                    case IOM_CACHE_PREFLUSH:
                    case IOM_CACHE_REACH:
                        /* do not override the old request */
                        break;
                    case IOM_CACHE_FLUSHPAGE:
                    case IOM_CACHE_FLUSHPAGELASTUSE:
                        filedes = devqueue->iodq_filedes;
                        if (req->ior_type == IOM_CACHE_FLUSHPAGEBATCH) {
                            cacheslot = p_reqdata->cacheslot;
                            dbe_cache_release(
                                filedes->fd_cache,
                                cacheslot,
                                reqtype == IOM_CACHE_FLUSHPAGE ?
                                    DBE_CACHE_DIRTY : DBE_CACHE_DIRTYLASTUSE,
                                iom_request);
                        } else {
                            ss_dassert(req->ior_.cacheslot == NULL);
                            req->ior_type = reqtype;
                            req->ior_ = *p_reqdata;
                        }
                        break;
                    case IOM_CACHE_FLUSHPAGEBATCH:
                        switch (req->ior_type) {
                            case IOM_CACHE_FLUSHPAGE:
                            case IOM_CACHE_FLUSHPAGELASTUSE:
                                /* Technically feasibe, but semantically
                                * should never happen!
                                */
                                ss_derror;
                                filedes = devqueue->iodq_filedes;
                                cacheslot = req->ior_.cacheslot;
                                dbe_cache_release(
                                    filedes->fd_cache,
                                    cacheslot,
                                    req->ior_type == IOM_CACHE_FLUSHPAGE ?
                                        DBE_CACHE_DIRTY : DBE_CACHE_DIRTYLASTUSE,
                                    iom_request);
                                /* FALLTHROUGH */
                            case IOM_CACHE_PREFETCH:
                            case IOM_CACHE_PREFLUSH:
                            case IOM_CACHE_REACH:
                                req->ior_type = reqtype;
                                ss_dassert(p_reqdata != NULL);
                                req->ior_ = *p_reqdata;
                                ss_dassert(req->ior_.flushbatch != NULL);
                                break;
                            case IOM_CACHE_FLUSHPAGEBATCH:
                                /* do not override the old request */
                                break;
                            default:
                                ss_rc_error(req->ior_type);
                                break;
                        }
                        break;
                    default:
                        ss_rc_error(reqtype);
                        break;
                }
            }
#endif  /* DBE_MTFLUSH */

        }
        return (req);
}

/*#***********************************************************************\
 *
 *		devqueue_searchreq
 *
 * Searches request from device queue
 *
 * Parameters :
 *
 *	devqueue - in, use
 *		pointer to device queue
 *
 *	daddr - in
 *		disk address
 *
 * Return value - ref :
 *      pointer to found request or NULL if not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_request_t* devqueue_searchreq(
        iomgr_devqueue_t* devqueue,
        su_daddr_t daddr)
{
        iomgr_request_t* req;
        su_rbt_node_t* treenode;
        iomgr_request_t dummy_req;

        dummy_req.ior_daddr = daddr;
        treenode = su_rbt_search(devqueue->iodq_reqpool, &dummy_req);
        if (treenode == NULL) {
            return (NULL);
        }
        req = su_rbtnode_getkey(treenode);
        return (req);
}


/*#***********************************************************************\
 *
 *		iomgr_getdevqueue
 *
 * Gets device queue according to file number and disk address
 *
 * Parameters :
 *
 *	iomgr - in, use
 *		I/O manager object
 *
 *	daddr - in
 *		disk address
 *
 *      reqtype - in
 *          request type
 *
 * Return value - ref :
 *      pointer to correct I/O device queue for that request. If
 *      device queue did not exist, it is created here.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_devqueue_t* iomgr_getdevqueue(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr,
        iomgr_reqtype_t reqtype)
{
        int devnum;
        iomgr_devqueue_t* devqueue;
        su_pa_t* devqueues_supa = NULL;
        uint nthreads = 0;
        iomgr_devqueuetype_t dqtype = 0;

        devnum = (int)dbe_file_getdiskno(iomgr->iom_file, daddr);
#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
        ss_assert(devnum >= 0);
#endif /* AUTOTEST_RUN || SS_DEBUG */
        if (devnum < 0) {
            return (NULL);
        }
        if (iomgr->iom_thrmode == IOM_THRMODE_SEPARATE) {
            switch (reqtype) {
                case IOM_CACHE_PREFETCH:
                case IOM_CACHE_REACH:            /* Blocking read */
                    dqtype = IOM_DEVQUEUE_READ;
                    nthreads = (uint)iomgr->iom_nthreads_total -
                        (uint)iomgr->iom_nthreads_writers;
                    devqueues_supa = iomgr->iom_devqueues_read_or_rw;
                    break;
                case IOM_CACHE_PREFLUSH:
                case IOM_CACHE_FLUSHPAGE:        /* uses ior_.cacheslot */
                case IOM_CACHE_FLUSHPAGELASTUSE:  /* uses ior_.cacheslot */
                case IOM_CACHE_FLUSHPAGEBATCH:   /* uses ior_.flushbatch */
                    dqtype = IOM_DEVQUEUE_WRITE;
                    nthreads = (uint)iomgr->iom_nthreads_writers;
                    devqueues_supa = iomgr->iom_devqueues_write;
                    break;
                default:
                    ss_rc_error(reqtype);
            }
        } else {
            ss_dassert(iomgr->iom_thrmode == IOM_THRMODE_SHARED);
            ss_dassert(iomgr->iom_devqueues_write == NULL);
            dqtype = IOM_DEVQUEUE_READWRITE;
            nthreads = (uint)iomgr->iom_nthreads_total;
            devqueues_supa = iomgr->iom_devqueues_read_or_rw;
        }
        if (!su_pa_indexinuse(devqueues_supa, (uint)devnum)) {
            devqueue = devqueue_init(iomgr, nthreads, dqtype);
            su_pa_insertat(devqueues_supa, (uint)devnum, devqueue);
        } else {
            devqueue = su_pa_getdata(devqueues_supa, (uint)devnum);
        }
        return (devqueue);
}

/*#***********************************************************************\
 *
 *		iomgr_cachepreflushctx_init
 *
 * Creates a cache callback context
 *
 * Parameters :
 *
 *	iomgr - in, use
 *		pointer to I/O manager object
 *
 *	cache - in, use
 *		pointer to cache object
 *
 * Return value - give :
 *      pointer to created callback context
 *
 * Comments :
 *      The context should be deleted using SsMemFree()
 *
 * Globals used :
 *
 * See also :
 */
static iomgr_cachepreflushctx_t* iomgr_cachepreflushctx_init(
        dbe_iomgr_t* iomgr,
        dbe_cache_t* cache)
{
        iomgr_cachepreflushctx_t* ctx;

        ctx = SSMEM_NEW(iomgr_cachepreflushctx_t);
        ctx->iopf_iomgr = iomgr;
        ctx->iopf_cache = cache;
        return (ctx);
}

/*#***********************************************************************\
 *
 *		iomgr_cachepreflushfun
 *
 * Callback function for cache that makes the preflush requests
 *
 * Parameters :
 *
 *	ctxptr - in, use
 *		pointer to cache callback context
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_cachepreflushfun(void* ctxptr)
{
        bool succp;
        su_daddr_t* preflush_array;
        size_t arraysize;
        iomgr_cachepreflushctx_t* ctx = ctxptr;

        ss_dprintf_2(("iomgr_cachepreflushfun()\n"));
        succp = dbe_cache_getpreflusharr(
                    ctx->iopf_cache,
                    &preflush_array,
                    &arraysize);
        if (!succp) {
            /* Nothing to flush */
            return;
        }
        dbe_iomgr_preflush(
            ctx->iopf_iomgr,
            preflush_array,
            (int)arraysize,
            0);
        SsMemFree(preflush_array);
}

#ifdef SS_MME

static int iomgr_calculatemaxmmepagecount(dbe_cache_t* cache)
{
        int nslots;

        /* Use up to half of the cache for MME. */
        nslots = dbe_cache_getnslot(cache);
        nslots = (int)((double)nslots * 0.50);

        return(nslots);
}

#endif /* SS_MME */

/*##**********************************************************************\
 *
 *		dbe_iomgr_init
 *
 * Creates an I/O manager object
 *
 * Parameters :
 *
 *	file - in, hold
 *		pointer to db file object
 *
 *	cfg - in, use
 *		pointer to config object
 *
 * Return value - give :
 *          pointer to created I/O manager
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_iomgr_t* dbe_iomgr_init(dbe_file_t* file, dbe_cfg_t* cfg)
{
        bool found;
        dbe_iomgr_t* iomgr;
        long maxmmecacheusage;

        iomgr = SSMEM_NEW(dbe_iomgr_t);
        iomgr->iom_file = file;
        iomgr->iom_filedes = dbe_file_getfiledes(file);
        iomgr->iom_mutex = SsSemCreateLocal(SS_SEMNUM_DBE_IOMGR);
        iomgr->iom_meslist = NULL;
        found = dbe_cfg_getuseiothreads(cfg, &iomgr->iom_useiothreads);
        found = dbe_cfg_getmmemaxcacheusage(cfg, &maxmmecacheusage);

#ifdef SS_NTHREADS_PER_DEVQUEUE
        dbe_cfg_getnumiothreads(cfg, &iomgr->iom_nthreads_total);
        dbe_cfg_getnumwriteriothreads(cfg, &iomgr->iom_nthreads_writers);
        iomgr->iom_devqueues_read_or_rw = su_pa_init();
        if (iomgr->iom_nthreads_writers == 0 ||
            (iomgr->iom_nthreads_writers == 1 &&
             iomgr->iom_nthreads_total == 1))
        {
            iomgr->iom_thrmode = IOM_THRMODE_SHARED;
            iomgr->iom_devqueues_write = NULL;
        } else {
            iomgr->iom_thrmode = IOM_THRMODE_SEPARATE;
            ss_dassert(iomgr->iom_nthreads_writers <
                       iomgr->iom_nthreads_total);
            iomgr->iom_devqueues_write = su_pa_init();
        }
        
#else /* SS_NTHREADS_PER_DEVICE */
        iomgr->iom_devqueues = su_pa_init();
#endif /* SS_NTHREADS_PER_DEVICE */

        /* nothing to read or write for diskless */
        if (dbefile_diskless) {
            iomgr->iom_useiothreads = 0;
        }
        {
            dbe_filedes_t* filedes;
            iomgr_cachepreflushctx_t* ctx;

            filedes = iomgr->iom_filedes;
            if (iomgr->iom_meslist == NULL) {
                iomgr->iom_meslist =
                    dbe_cache_getmeslist(filedes->fd_cache);
            }
            if (!dbefile_diskless) {
                ctx = iomgr_cachepreflushctx_init(
                            iomgr,
                            filedes->fd_cache);

                dbe_cache_setpreflushcallback(
                    filedes->fd_cache,
                    iomgr_cachepreflushfun,
                    ctx);
            }
#ifdef SS_MME
            iomgr->iom_maxmmepages =
                iomgr_calculatemaxmmepagecount(filedes->fd_cache);
            if (maxmmecacheusage != 0 && /* 0 means unlimited */
                iomgr->iom_maxmmepages
                > maxmmecacheusage / filedes->fd_blocksize) {
                iomgr->iom_maxmmepages =
                    maxmmecacheusage / filedes->fd_blocksize;
            }
            if (iomgr->iom_maxmmepages < DBE_MIN_MMEPAGES) {
                su_informative_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_NOTENOUGHMMEMEM_DD,
                        maxmmecacheusage/1024, (filedes->fd_blocksize*DBE_MIN_MMEPAGES)/1024);
            }
#endif
        }
        ss_dassert(iomgr->iom_meslist != NULL);
        return (iomgr);
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_done
 *
 * Deletes I/O manager object
 *
 * Parameters :
 *
 *	iomgr - in, take
 *		pointer to I/O manager object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_iomgr_done(dbe_iomgr_t* iomgr)
{
        int i;
        iomgr_devqueue_t* devqueue;

        dbe_cache_setpreflushcallback(
            iomgr->iom_filedes->fd_cache,
            (void(*)(void*))NULL,
            NULL);
#ifdef SS_NTHREADS_PER_DEVQUEUE
        su_pa_do(iomgr->iom_devqueues_read_or_rw, i) {
             devqueue = su_pa_remove(iomgr->iom_devqueues_read_or_rw, i);
             devqueue_done(devqueue);
        }
        su_pa_done(iomgr->iom_devqueues_read_or_rw);
        if (iomgr->iom_devqueues_write != NULL) {
            su_pa_do(iomgr->iom_devqueues_write, i) {
                devqueue = su_pa_remove(iomgr->iom_devqueues_write, i);
                devqueue_done(devqueue);
            }
            su_pa_done(iomgr->iom_devqueues_write);
        }
        
#else /* SS_NTHREADS_PER_DEVICE */
        su_pa_do(iomgr->iom_devqueues, i) {
            devqueue = su_pa_remove(iomgr->iom_devqueues, i);
            devqueue_done(devqueue);
        }
        su_pa_done(iomgr->iom_devqueues);
#endif /* SS_NTHREADS_PER_DEVICE */
        SsSemFree(iomgr->iom_mutex);
        SsMemFree(iomgr);
}

/*#***********************************************************************\
 *
 *		iomgr_prefetchorflush
 *
 * Common function to add prefetch and preflush requests
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager
 *
 *	daddr_array - in, use
 *		pointer to array of disk addresses
 *
 *	array_size - in
 *		number of elements in daddr_array
 *
 *	reqtype - in
 *		request type - IOM_CACHE_PREFLUSH or IOM_CACHE_PREFETCH
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void iomgr_prefetchorflush(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size,
        iomgr_reqtype_t reqtype,
        dbe_info_flags_t infoflags)
{
        int i;
        iomgr_devqueue_t* devqueue;
        su_daddr_t daddr;

        ss_dprintf_3(("iomgr_prefetchorflush\n"));

        SsSemEnter(iomgr->iom_mutex);
        for (i = 0; i < array_size; i++) {
            daddr = daddr_array[i];
            devqueue = iomgr_getdevqueue(iomgr, daddr, reqtype);
            ss_dassert(devqueue != NULL);
            if (devqueue != NULL) {
                (void)devqueue_addreq(
                        devqueue,
                        daddr,
                        reqtype,
                        NULL,
                        infoflags);
#ifdef SS_NTHREADS_PER_DEVQUEUE
                su_meswaitlist_wakeup1st(devqueue->iodq_meswaitlist);
#else
                SsMesSend(devqueue->iodq_wait);
#endif
            }
        }
        SsSemExit(iomgr->iom_mutex);
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_preflush
 *
 * Adds preflush requests to I/O manager
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager
 *
 *	daddr_array - in, use
 *		pointer to array of disk addresses
 *
 *	array_size - in
 *		number of elements in daddr_array
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_iomgr_preflush(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size,
        dbe_info_flags_t infoflags)
{
        ss_pprintf_2(("dbe_iomgr_preflush:array_size = %d\n", array_size));

        /* no need to preflush for diskless */
        if (dbefile_diskless) {
            return;
        }

        iomgr_prefetchorflush(
            iomgr,
            daddr_array,
            array_size,
            IOM_CACHE_PREFLUSH,
            infoflags);
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_prefetch
 *
 * Adds prefetch requests to I/O manager
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager
 *
 *	daddr_array - in, use
 *		pointer to array of disk addresses
 *
 *	array_size - in
 *		number of elements in daddr_array
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_iomgr_prefetch(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size,
        dbe_info_flags_t infoflags)
{
        ss_pprintf_2(("dbe_iomgr_prefetch:array_size = %d\n", array_size));

        /* no need to prefetch for diskless, everything is in the cache. */
        if (dbefile_diskless) {
            return;
        }

        iomgr_prefetchorflush(
            iomgr,
            daddr_array,
            array_size,
            IOM_CACHE_PREFETCH,
            infoflags);
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_prefetchwait
 *
 * Waits for an address to be prefetched
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager
 *
 *	daddr - in
 *		disk address
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_iomgr_prefetchwait(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr)
{
        iomgr_devqueue_t* devqueue;
        iomgr_request_t* req;

        ss_dprintf_2(("dbe_iomgr_prefetchwait\n"));

        if (!iomgr->iom_useiothreads) {
            return;
        }
        SsSemEnter(iomgr->iom_mutex);
        devqueue = iomgr_getdevqueue(iomgr, daddr, IOM_CACHE_PREFETCH);
        ss_dassert(devqueue != NULL);
        req = devqueue_searchreq(
                    devqueue,
                    daddr);
        if (req != NULL) {
            iomgr_wait_t waitobj;

            request_addwait(iomgr, req, &waitobj);
            SS_PMON_ADD(SS_PMON_CACHEPREFETCHWAIT);
            SsSemExit(iomgr->iom_mutex);
            iomgr_wait(iomgr, &waitobj);
        } else {
            /* The request has been fulfilled! */
            SsSemExit(iomgr->iom_mutex);
        }
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_reach
 *
 * Reaches a cache slot
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager object
 *
 *	daddr - in
 *		disk address
 *
 *	mode - in
 *		reach mode (see dbe8cach.h)
 *
 *	p_data - out
 *		pointer to data area of reached cache slot
 *
 *      ctx - in, hold
 *          reach context
 *
 * Return value - ref :
 *      pointer to reached cache slot
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_cacheslot_t* dbe_iomgr_reach(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        dbe_info_flags_t infoflags,
        char** p_data,
        char* ctx)
{
        dbe_cacheslot_t* cacheslot;
        iomgr_devqueue_t* devqueue;
        iomgr_request_t* req;
        iomgr_wait_t waitobj;
        dbe_filedes_t* filedes;
        dbe_cache_t* cache;
        dbe_cache_reachmode_t mode2 = 0;
        bool hit;

        ss_dprintf_2(("dbe_iomgr_reach:addr = %ld, mode = %d\n", daddr, mode));
        SS_PUSHNAME("dbe_iomgr_reach");

        filedes = iomgr->iom_filedes;
        cache = filedes->fd_cache;

        if (iomgr->iom_useiothreads
        &&  mode != DBE_CACHE_WRITEONLY
        &&  mode != DBE_CACHE_ALLOC)
        {
            switch (mode) {
                case DBE_CACHE_READONLY:
                    mode2 = DBE_CACHE_READONLY_IFHIT;
                    break;
                case DBE_CACHE_READWRITE:
                    mode2 = DBE_CACHE_READWRITE_IFHIT;
                    break;
                default:
                    ss_error;
            }
            cacheslot = dbe_cache_reach(cache, daddr, mode2, infoflags, p_data, ctx);
            if (cacheslot != NULL) {
                /* Cache hit, no I/O needed! */
                ss_dprintf_2(("dbe_iomgr_reach: cache hit\n"));
                SS_POPNAME;
                return (cacheslot);
            }
            /* else we need to leave request to I/O thread */
            SsSemEnter(iomgr->iom_mutex);
            ss_dprintf_2(("dbe_iomgr_reach: leave request\n"));
            devqueue = iomgr_getdevqueue(iomgr, daddr, IOM_CACHE_REACH);
            ss_dassert(devqueue != NULL);
            req = devqueue_addreq(
                        devqueue,
                        daddr,
                        IOM_CACHE_REACH,
                        NULL,
                        infoflags);
            request_addwait(iomgr, req, &waitobj);
#ifdef SS_NTHREADS_PER_DEVQUEUE
            su_meswaitlist_wakeup1st(devqueue->iodq_meswaitlist);
            SsSemExit(iomgr->iom_mutex);
#else
            SsSemExit(iomgr->iom_mutex);
            SsMesSend(devqueue->iodq_wait);
#endif
            iomgr_wait(iomgr, &waitobj);
        }
        cacheslot = dbe_cache_reachwithhitinfo(cache,
                                               daddr,
                                               mode,
                                               infoflags,
                                               p_data,
                                               ctx,
                                               &hit);
        if (!hit) {
            /* in case of cache miss there may be a pending prefetch
             * request which must be expired to avoid unnecessary I/O
             */
            SsSemEnter(iomgr->iom_mutex);
            devqueue =
                iomgr_getdevqueue(iomgr, daddr, IOM_CACHE_PREFETCH);
            ss_dassert(devqueue != NULL);
            req = devqueue_searchreq(
                    devqueue,
                    daddr);
            if (req != NULL) {
                if (req->ior_type == IOM_CACHE_PREFETCH) {
                    ss_dprintf_1(("dbe_iomgr_reach: request to page %lu expired\n",
                                  (ulong)daddr));

                    req->ior_expired = TRUE;
                }
            }
            SsSemExit(iomgr->iom_mutex);
        }
        ss_dprintf_2(("dbe_iomgr_reach: return\n"));
        SS_POPNAME;

        return (cacheslot);
}

/*##**********************************************************************\
 *
 *		dbe_iomgr_release
 *
 * Releases a reached cache slot
 *
 * Parameters :
 *
 *	iomgr - in out, use
 *		pointer to I/O manager object
 *
 *	slot - in, take
 *		pointer to cache slot
 *
 *	mode - in
 *		release mode (see dbe8cach.h)
 *
 *      ctx - in, use
 *          reach context
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_iomgr_release(
        dbe_iomgr_t* iomgr,
        dbe_cacheslot_t* slot,
        dbe_cache_releasemode_t mode,
        void* ctx)
{
        iomgr_devqueue_t* devqueue;
        iomgr_request_t* req;
        iomgr_wait_t waitobj;
        dbe_filedes_t* filedes;
        dbe_cache_t* cache;
        su_daddr_t daddr;

        ss_dprintf_2(("dbe_iomgr_release:mode = %d\n", mode));

        filedes = iomgr->iom_filedes;
        cache = filedes->fd_cache;

        if (iomgr->iom_useiothreads
        &&  (mode == DBE_CACHE_FLUSH
          || mode == DBE_CACHE_FLUSHLASTUSE))
        {
            iomgr_reqdata_t req_;

            req_.cacheslot = slot;
            daddr = dbe_cacheslot_getdaddr(slot);
            SsSemEnter(iomgr->iom_mutex);
            devqueue = iomgr_getdevqueue(iomgr, daddr, IOM_CACHE_FLUSHPAGE);
            ss_dassert(devqueue != NULL);
            req = devqueue_addreq(
                        devqueue,
                        daddr,
                        mode == DBE_CACHE_FLUSH ?
                            IOM_CACHE_FLUSHPAGE : IOM_CACHE_FLUSHPAGELASTUSE,
                        &req_,
                        0);

            request_addwait(iomgr, req, &waitobj);
#ifdef SS_NTHREADS_PER_DEVQUEUE
            su_meswaitlist_wakeup1st(devqueue->iodq_meswaitlist);
            SsSemExit(iomgr->iom_mutex);
#else
            SsSemExit(iomgr->iom_mutex);
            SsMesSend(devqueue->iodq_wait);
#endif
            iomgr_wait(iomgr, &waitobj);
        } else {
            if (mode == DBE_CACHE_DIRTYLASTUSE) {
                daddr = dbe_cacheslot_getdaddr(slot);
            }
            dbe_cache_release(cache, slot, mode, ctx);
            if (mode == DBE_CACHE_DIRTYLASTUSE) {
                ss_pprintf_1(("dbe_iomgr_release: preflush: %ld\n",
                              (long)daddr));
                dbe_iomgr_preflush(iomgr, &daddr, 1, 0);
            }
        }
}

#if defined(DBE_MTFLUSH)

/*##**********************************************************************\
 * 
 *		dbe_iomgr_flushallcaches_init
 * 
 * 
 * 
 * Parameters : 
 * 
 *		iomgr - 
 *			
 *			
 *		flusharray - 
 *			
 *			
 *		flusharraysize - 
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
bool dbe_iomgr_flushallcaches_init(
        dbe_iomgr_t* iomgr,
        su_daddr_t** flusharray,
        size_t* flusharraysize)
{
        bool anything_to_flush;
        dbe_filedes_t* filedes;

        SsSemEnter(iomgr->iom_mutex);
        ss_dassert(!mtflush_active);
        ss_dassert(!mtflush_ntoflush);

        filedes = iomgr->iom_filedes;
        ss_assert(filedes != NULL);
        anything_to_flush = dbe_cache_getflusharr(filedes->fd_cache,
                                      flusharray,
                                      flusharraysize);
        SsSemExit(iomgr->iom_mutex);
        return (anything_to_flush);
}

/*##**********************************************************************\
 * 
 *		dbe_iomgr_flushallcaches_exec
 * 
 * 
 * 
 * Parameters : 
 * 
 *		wakeupctx - 
 *			
 *			
 *		flusharray - 
 *			
 *			
 *		flusharraysize - 
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
void dbe_iomgr_flushallcaches_exec(
        dbe_iomgr_t* iomgr,
        void (*wakeupfp)(void*),
        void* wakeupctx,
        su_daddr_t* flusharray,
        size_t flusharraysize,
        dbe_info_flags_t infoflags)
{
        iomgr_reqdata_t req_;
        dbe_filedes_t* filedes;
        int i;

        SsSemEnter(iomgr->iom_mutex);
        ss_dassert(!mtflush_active);
        ss_dassert(!mtflush_ntoflush);

        filedes = iomgr->iom_filedes;
        ss_assert(filedes != NULL);
        req_.flushbatch = iomgr_flushbatch_init(iomgr, FALSE, wakeupfp, wakeupctx, NULL, NULL);
        req_.cacheslot = NULL;
#ifdef SS_MME
        req_.mmepage = FALSE;
#endif
        for (i = 0; i < (int)flusharraysize; i++) {
            su_daddr_t daddr = flusharray[i];
            iomgr_devqueue_t* devqueue;

            devqueue = iomgr_getdevqueue(iomgr, daddr,
                                         IOM_CACHE_FLUSHPAGEBATCH);
            (void)devqueue_addreq(
                    devqueue,
                    daddr,
                    IOM_CACHE_FLUSHPAGEBATCH,
                    &req_,
                    infoflags);
#ifdef SS_NTHREADS_PER_DEVQUEUE
            su_meswaitlist_wakeup1st(devqueue->iodq_meswaitlist);
#else
            SsMesSend(devqueue->iodq_wait);
#endif
        }
        SsMemFree(flusharray);
        iomgr_flushbatch_add(req_.flushbatch, flusharraysize, FALSE);

        SsSemExit(iomgr->iom_mutex);
}

#ifdef SS_MME

/*##**********************************************************************\
 *
 *		dbe_iomgr_addtoflushbatch
 *
 * Adds a page to flushbatch object to be flushed to the disk.
 * The flushes are left to the I/O manager thread(s)
 *
 * Parameters :
 *
 *	iomgr - use
 *		I/O manager
 *
 *      flushbatch - in,use
 *          flushbatch object
 *
 *      slot - in, take
 *          cache slot to be flushed
 *
 *      daddr - in, use
 *          disk address of the slot
 *
 * Return value :
 *      TRUE success
 *      FALSE there are no space in the flushbatch
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_iomgr_addtoflushbatch(
        dbe_iomgr_t*        iomgr,
        dbe_iomgr_flushbatch_t* flushbatch,
        dbe_cacheslot_t*    slot,
        su_daddr_t          daddr,
        dbe_info_flags_t    infoflags)
{
        dbe_filedes_t*      filedes;
        iomgr_devqueue_t*   devqueue;
        iomgr_reqdata_t     req_;

        SsSemEnter(iomgr->iom_mutex);

        /* Name of the function is confusing, but it returns
           pointer to the index file descriptor */
        filedes = iomgr->iom_filedes;
        if (filedes == NULL) {
            /* This should never happen */
            ss_derror;
            SsSemExit(iomgr->iom_mutex);
            return(FALSE);
        }

        /* If buffer is full, we are not allowed to add more */
        if (FLUSHBATCH_FULL(flushbatch, iomgr)) {
            FLUSHBATCH_SIGNAL(flushbatch);
            SsSemExit(iomgr->iom_mutex);
            return(FALSE);
        }

        /* Set it's disk page address */
        dbe_cache_setpageaddress(filedes->fd_cache, slot, daddr);

        dbe_cache_release(
            filedes->fd_cache,
            slot,
            DBE_CACHE_DIRTYLASTUSE,
            iom_flushbatch);

        req_.cacheslot = NULL;
        req_.flushbatch = flushbatch;
        req_.mmepage = TRUE;

        /* Add flush request for the page */
        devqueue = iomgr_getdevqueue(iomgr, daddr,
                                     IOM_CACHE_FLUSHPAGEBATCH);
        (void)devqueue_addreq(
                devqueue,
                daddr,
                IOM_CACHE_FLUSHPAGEBATCH,
                &req_,
                infoflags);
#ifdef SS_NTHREADS_PER_DEVQUEUE
        su_meswaitlist_wakeup1st(devqueue->iodq_meswaitlist);
#else
        SsMesSend(devqueue->iodq_wait);
#endif
        iomgr_flushbatch_add(req_.flushbatch, 1, TRUE);

        SsSemExit(iomgr->iom_mutex);

        return(TRUE);
}

#endif /* SS_MME */

#ifdef SS_DEBUG
void dbe_iomgr_debug_cancel_dbeflush(void)
{
        mtflush_active = FALSE;
        mtflush_ntoflush = 0;
}
void dbe_iomgr_debug_cancel_mmeflush(void)
{
#ifdef SS_MME
        mmeflush_active = FALSE;
        mmeflush_ntoflush = 0;
#endif /* SS_MME */
}
#endif /* SS_DEBUG */


#endif /* DBE_MTFLUSH */

#else   /* SS_MT */

/* I/O manager */
struct dbe_iomgr_st {
        dbe_file_t*     iom_file;           /* Ref. to file object */
};

dbe_iomgr_t* dbe_iomgr_init(dbe_file_t* file, dbe_cfg_t* cfg)
{
        dbe_iomgr_t* iomgr;

        SS_NOTUSED(cfg);
        iomgr = SSMEM_NEW(dbe_iomgr_t);
        iomgr->iom_file = file;
        return (iomgr);
}

void dbe_iomgr_done(dbe_iomgr_t* iomgr)
{
        ss_dassert(iomgr != NULL);
        SsMemFree(iomgr);
}

void dbe_iomgr_preflush(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size)
{
        SS_NOTUSED(iomgr);
        SS_NOTUSED(daddr_array);
        SS_NOTUSED(array_size);
}

static int SS_CLIBCALLBACK prefetch_qsortcmp(void* s1, void* s2)
{
        su_daddr_t a1 = *(su_daddr_t*)s1;
        su_daddr_t a2 = *(su_daddr_t*)s2;

        if (sizeof(int) >= sizeof(su_daddr_t)) {
            /* 32 bit or bigger int */
            return ((int)a1 - (int)a2);
        }
        /* 16 bit int */
        if (a1 < a2) {
            return (-1);
        }
        if (a1 > a2) {
            return (1);
        }
        return (0);
}

void dbe_iomgr_prefetch(
        dbe_iomgr_t* iomgr,
        su_daddr_t* daddr_array,
        int array_size)
{
        int i;
        dbe_cacheslot_t* slot;
        char* dummy_data;
        dbe_filedes_t* filedes;

        filedes = iomgr->iom_filedes;

        qsort(
            daddr_array,
            array_size,
            sizeof(su_daddr_t),
            prefetch_qsortcmp);

        for (i = 0; i < array_size; i++) {
            ss_dprintf_2(("dbe_iomgr_prefetch:add %ld\n", daddr_array[i]));

            slot = dbe_cache_reach(
                        filedes->fd_cache,
                        daddr_array[i],
                        DBE_CACHE_PREFETCH,
                        infoflags,
                        &dummy_data,
                        iom_prefetch);

            dbe_cache_release(
                filedes->fd_cache,
                slot,
                DBE_CACHE_CLEAN,
                iom_prefetch);
        }
}

void dbe_iomgr_prefetchwait(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr)
{
        SS_NOTUSED(iomgr);
        SS_NOTUSED(daddr);
}

dbe_cacheslot_t* dbe_iomgr_reach(
        dbe_iomgr_t* iomgr,
        su_daddr_t daddr,
        dbe_cache_reachmode_t mode,
        char** p_data,
        char* ctx)
{
        dbe_cacheslot_t* cacheslot;

        cacheslot = dbe_cache_reach(
                        iomgr->iom_filedes->fd_cache,
                        daddr,
                        mode,
                        infoflags,
                        p_data,
                        ctx);
        return (cacheslot);
}

void dbe_iomgr_release(
        dbe_iomgr_t* iomgr,
        dbe_cacheslot_t* slot,
        dbe_cache_releasemode_t mode,
        void* ctx)
{
        dbe_cache_release(
            iomgr->iom_filedes->fd_cache,
            slot,
            mode,
            ctx);
}

#endif  /* SS_MT */
