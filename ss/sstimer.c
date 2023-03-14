/*************************************************************************\
**  source       * sstimer.c
**  directory    * ss
**  description  * Timer routines. After user given timeout period calls
**               * user given callback routine.
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


Limitations:
-----------


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

#include "ssenv.h"
#include "ssstddef.h"
#include "sstime.h"
#include "ssdebug.h"
#include "sssem.h"
#include "ssmem.h"
#include "ssthread.h"
#include "sscheck.h"

#ifdef SS_MYSQL
#include <su0rbtr.h>
#include <su0mesl.h>
#else
#include "../su/su0rbtr.h"
#include "../su/su0mesl.h"
#endif

#include "sstimer.h"

#define CHK_TR(tr) ss_assert(SS_CHKPTR(tr) && tr->tr_check == SS_CHK_TIMERREQ)
#define CHK_TIMER(t) ss_assert(SS_CHKPTR(t) && (t)->t_check == SS_CHK_TIMER)

#ifdef SS_MT
#define TIMER_SEMENTER(sem) SsSemEnter(sem)
#define TIMER_SEMEXIT(sem) SsSemExit(sem)
#define TIMER_MESSEND(mes) SsMesSend(mes)
#else /* SS_MT */
#define TIMER_SEMENTER(sem) /* no semaphores */
#define TIMER_SEMEXIT(sem)  /* no semaphores */
#endif /* SS_MT */

typedef struct {
        int        t_check;
#ifdef SS_MT
        SsThreadT* t_thread;
        bool       t_shutting_down;
        SsMesT*    t_msg;
        SsMesT*    t_shutdownmsg;
        SsSemT*    t_sem;
#endif /* SS_MT */
        ss_uint4_t t_req_id_ctr;
        ss_uint4_t t_persistent_req_id_ctr;
        SsTimeT    t_last_now;
        long       t_timetonextevent_ms;
        su_rbt_t*  t_reqtree_by_endtime;
        su_rbt_t*  t_reqtree_by_id;
} SsTimerT;

typedef struct timer_request_st timer_request_t;

struct timer_request_st {
        int                   tr_check;
        ss_uint4_t            tr_id;
        bool                  tr_cancelled;
        SsTimeT               tr_starttime;
        long                  tr_timeout;
        timeout_callbackfun_t tr_callbackfun;
        void*                 tr_callbackctx;
        void                  (*tr_freefunc)(void* ctx);
        bool                  tr_persistent;
        ss_debug(char**       tr_callstk;)
}; 

#ifdef SS_DEBUG
#define TIMER_MAX_EVENTWAIT_TIME_MS 2000
#else /* SS_DEBUG */
#define TIMER_MAX_EVENTWAIT_TIME_MS 10000
#endif /* SS_DEBUG */

#define TIMER_MAX_SCHED_DELAY_IN_SEC 10

SsTimeT ss_timer_curtime_sec;

static SsTimerT* timer = NULL;
static bool timer_initialized = FALSE;

#define TIMER_MIN_REQ_ID    ((ss_uint4_t)1)
#define TIMER_MAX_REQ_ID    ((ss_uint4_t)0x7FFFFFFFUL)

#define TIMER_MIN_PERSISTENT_REQ_ID ((ss_uint4_t)0x80000000UL)
#define TIMER_MAX_PERSISTENT_REQ_ID ((ss_uint4_t)0xFFFFFFFFUL)

static ss_uint4_t timer_new_req_id(SsTimerT* timer)
{
        CHK_TIMER(timer);

        if (timer->t_req_id_ctr++ >= TIMER_MAX_REQ_ID) {
            /* wrap around! should not be a problem */
            timer->t_req_id_ctr = TIMER_MIN_REQ_ID;
        }
        return (timer->t_req_id_ctr);
}

static ss_uint4_t timer_new_persistent_req_id(SsTimerT* timer)
{
        CHK_TIMER(timer);

        if (timer->t_persistent_req_id_ctr++ >= TIMER_MAX_PERSISTENT_REQ_ID) {
            /* wrap around! should not be a problem */
            timer->t_persistent_req_id_ctr = TIMER_MIN_PERSISTENT_REQ_ID;
        }
        return (timer->t_persistent_req_id_ctr);
}

static int tr_search_compare_by_id(void* search_key, void* rbt_key)
{
        SsTimerRequestIdT id = (SsTimerRequestIdT)search_key;
        timer_request_t* tr = (timer_request_t*)rbt_key;
        if (id > tr->tr_id) {
            return (1);
        }
        if (id < tr->tr_id) {
            return (-1);
        }
        return (0);
}

static int tr_insert_compare_by_id(void* p1, void* p2)
{
        timer_request_t* r1 = (timer_request_t*) p1;
        timer_request_t* r2 = (timer_request_t*) p2;
        if (r1->tr_id > r2->tr_id) {
            return (1);
        }
        if (r1->tr_id < r2->tr_id) {
            return (-1);
        }
        return (0);
}

static int tr_compare_by_endtime(void* p1, void* p2)
{
        int cmp;
#ifdef SS_TIMER_MSEC
        timer_request_t* r1 = (timer_request_t*) p1;
        timer_request_t* r2 = (timer_request_t*) p2;
        
        cmp = SsTimeCmp(r1->tr_starttime + r1->tr_timeout,
                        r2->tr_starttime + r2->tr_timeout);
        if (cmp == 0) {
            cmp = su_rbt_ptr_compare(p1, p2);
        }
#else
        cmp = ((((timer_request_t*)p1)->tr_starttime +
                ((timer_request_t*)p1)->tr_timeout) -
               (((timer_request_t*)p2)->tr_starttime +
                ((timer_request_t*)p2)->tr_timeout));
        if (cmp == 0) {
            cmp = su_rbt_ptr_compare(p1, p2);
        }
#endif
        
        return (cmp);
}


static timer_request_t* tr_init(
        SsTimerT* timer,
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx,
        bool persistent,
        void (*freefunc)(void* ctx),
        char** callstk __attribute__ ((unused)))
{
        timer_request_t* tr;

        SS_PUSHNAME("tr_init");

        tr = SSMEM_NEW(timer_request_t);
        tr->tr_check = SS_CHK_TIMERREQ;
        if (persistent) {
            tr->tr_id = timer_new_persistent_req_id(timer);
        } else {
            tr->tr_id = timer_new_req_id(timer);
        }
        tr->tr_cancelled = FALSE;
        ss_dassert(timeout_ms >= 0 && timeout_ms <= 2000000000);
#ifdef SS_TIMER_MSEC
        tr->tr_starttime = SsTimeMs();
        FAKE_CODE_BLOCK(
                FAKE_SS_TIME_JUMPS_FWD_120S,
                tr->tr_starttime += 120UL * 1000UL;);
        tr->tr_timeout = timeout_ms;
        if (tr->tr_timeout == 0) {
            tr->tr_timeout = 1;
        }
#else
        tr->tr_starttime = SsTime(NULL);
        FAKE_CODE_BLOCK(
                FAKE_SS_TIME_JUMPS_FWD_120S,
                tr->tr_starttime += 120UL;);
        tr->tr_timeout = (timeout_ms + 999UL) / 1000UL;
        if (tr->tr_timeout == 0) {
            tr->tr_timeout = 1;
        }
#endif
        tr->tr_callbackfun = callbackfun;
        tr->tr_callbackctx = callbackctx;
        tr->tr_freefunc = freefunc;
        tr->tr_persistent = persistent;
#ifdef SS_DEBUG
        if (callstk != NULL) {
            tr->tr_callstk = callstk;
        } else {
            tr->tr_callstk = SsMemTrcCopyCallStk();
        }
#endif

        SS_POPNAME;

        return (tr);
}
        
static void tr_done(void* p)
{
        timer_request_t* tr = p;

        CHK_TR(tr);
        if (tr->tr_freefunc != NULL) {
            (*tr->tr_freefunc)(tr->tr_callbackctx);
        }
        ss_debug(SsMemTrcFreeCallStk(tr->tr_callstk));
        SsMemFree(tr);
}

static timer_request_t* TimerAddRequestEx(
        timer_request_t* tr)
{
        SS_PUSHNAME("TimerAddRequestEx");

        ss_pprintf_1(("TimerAddRequestEx: timeout = %ld ms, persistent = %d\n",
                      tr->tr_timeout, tr->tr_persistent));
        ss_dassert(timer != NULL);
        CHK_TIMER(timer);
        ss_dassert(!timer->t_shutting_down);

        su_rbt_insert(timer->t_reqtree_by_endtime, tr);
        su_rbt_insert(timer->t_reqtree_by_id, tr);

        SS_POPNAME;
        return (tr);
}

static timer_request_t* TimerAddRequest(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx,
        bool persistent,
        void (*freefunc)(void* ctx))
{
        timer_request_t* tr;

        ss_pprintf_1(("TimerAddRequest: timeout = %ld ms, persistent = %d\n",
                      timeout_ms, persistent));
        SS_PUSHNAME("TimerAddRequest");

        if (timer == NULL) {
            ss_pprintf_1(("TimerAddRequest: implicitly calling SsTimerGlobalInit\n"));
            SsTimerGlobalInit();
        }
        while(!timer_initialized) {
            ss_pprintf_1(("TimerAddRequest: implicit SsTimerGlobalInit call not completed yet, wait a moment\n"));
            SsThrSleep(10);
        }
        CHK_TIMER(timer);

        TIMER_SEMENTER(timer->t_sem);
        tr = tr_init(timer, timeout_ms, callbackfun, callbackctx, persistent, freefunc, NULL);

        tr = TimerAddRequestEx(tr);

        TIMER_SEMEXIT(timer->t_sem);
#ifdef SS_MT
        SsMesSend(timer->t_msg);
#endif /* SS_MT */

        SS_POPNAME;

        return(tr);
}

static void timer_run(void)
{
#ifdef SS_TIMER_MSEC
        SsTimeT now = SsTimeMs();
#else
        SsTimeT now = SsTime(NULL);
#endif
        su_rbt_node_t* rbtn_by_endtime;
        su_rbt_node_t* rbtn_by_id;
        timer_request_t* req;
        timer_request_t req2;
        
        CHK_TIMER(timer);
        TIMER_SEMENTER(timer->t_sem);
        ss_pprintf_2(("timer_run: message timeout = %ld ms\n",
                      timer->t_timetonextevent_ms));
        SS_PUSHNAME("timer_run");
#ifdef SS_TIMER_MSEC
        FAKE_CODE_BLOCK(
                FAKE_SS_TIME_JUMPS_FWD_120S,
                now += 120UL * 1000UL;);
        if (SsTimeCmp(now, timer->t_last_now) < 0
            || SsTimeCmp(now - timer->t_last_now,
                         (timer->t_timetonextevent_ms
                          + TIMER_MAX_SCHED_DELAY_IN_SEC * 1000UL)) > 0)
#else
        FAKE_CODE_BLOCK(
                FAKE_SS_TIME_JUMPS_FWD_120S,
                now += 120UL;);
        if (now < timer->t_last_now ||
            (now - timer->t_last_now) >
            (timer->t_timetonextevent_ms / 1000UL + TIMER_MAX_SCHED_DELAY_IN_SEC))
#endif
        {
            /* time inconsistency detected!
             * that is why we patch the current time to be the
             * start time of each requests
             */
            ss_pprintf_1(("\ntimer_run: time inconsistency detected. now - timer->t_last_now = %ld\n", now - timer->t_last_now));
            rbtn_by_id = su_rbt_min(timer->t_reqtree_by_id, NULL);
            if (rbtn_by_id != NULL) {
                su_rbt_deleteall(timer->t_reqtree_by_endtime);
            }
            while (rbtn_by_id != NULL) {
                req = su_rbtnode_getkey(rbtn_by_id);
                req->tr_starttime = now;
                su_rbt_insert(timer->t_reqtree_by_endtime, req);
                rbtn_by_id = su_rbt_succ(timer->t_reqtree_by_id, rbtn_by_id);
            }
        }
        for (;;) {
            timer_request_t* persistent_tr;
            
            persistent_tr = NULL;


            rbtn_by_endtime = su_rbt_min(timer->t_reqtree_by_endtime,
                                         NULL);
            if (rbtn_by_endtime == NULL) {
                timer->t_timetonextevent_ms = TIMER_MAX_EVENTWAIT_TIME_MS;
                break;
            }
            req = su_rbtnode_getkey(rbtn_by_endtime);
#ifdef SS_TIMER_MSEC
            if (SsTimeCmp(req->tr_starttime + req->tr_timeout, now) > 0) {
                timer->t_timetonextevent_ms =
                    (req->tr_starttime + req->tr_timeout - now);
                if (timer->t_timetonextevent_ms > TIMER_MAX_EVENTWAIT_TIME_MS) {
                    timer->t_timetonextevent_ms = TIMER_MAX_EVENTWAIT_TIME_MS;
                }
                break;
            }
#else
            if (req->tr_starttime + req->tr_timeout > now) {
                timer->t_timetonextevent_ms = 1000UL *
                    (req->tr_starttime + req->tr_timeout - now);
                if (timer->t_timetonextevent_ms > TIMER_MAX_EVENTWAIT_TIME_MS) {
                    timer->t_timetonextevent_ms = TIMER_MAX_EVENTWAIT_TIME_MS;
                }
                break;
            }
#endif
            req2 = *req;
            req->tr_freefunc = NULL;    /* Do not free before timeout is signalled. */
            ss_debug(
                rbtn_by_id = su_rbt_search(timer->t_reqtree_by_id,
                                           (void*)req->tr_id);)
            ss_dassert(rbtn_by_id != NULL);
            if (req->tr_persistent) {
                char** callstk = NULL;
                ss_debug(callstk = req->tr_callstk);
                ss_debug(req->tr_callstk = NULL);
#ifdef SS_TIMER_MSEC
                persistent_tr = tr_init(
                        timer,
                        req->tr_timeout,
                        req->tr_callbackfun,
                        req->tr_callbackctx,
                        TRUE,
                        NULL,
                        callstk);
#else
                persistent_tr = tr_init(
                        timer,
                        req->tr_timeout * 1000UL,
                        req->tr_callbackfun,
                        req->tr_callbackctx,
                        TRUE,
                        NULL,
                        callstk);
#endif
            }
            su_rbt_delete(timer->t_reqtree_by_endtime, rbtn_by_endtime);
            /* su_rbt_delete(timer->t_reqtree_by_id, rbtn_by_id); */
            if (persistent_tr != NULL) {
                /* Need to add after delete operations because deletes use
                 * node pointer and that may not be valid after reinsert
                 * of persistent request.
                 */
                TimerAddRequestEx(persistent_tr);
            }
            TIMER_SEMEXIT(timer->t_sem);
            ss_pprintf_2(("timer_run: signaling timeout\n"));
            (*req2.tr_callbackfun)(req2.tr_callbackctx, req2.tr_id);
            if (req2.tr_freefunc != NULL) {
                (*req2.tr_freefunc)(req2.tr_callbackctx);
            }
            TIMER_SEMENTER(timer->t_sem);
            rbtn_by_id = su_rbt_search(timer->t_reqtree_by_id,
                                       (void*)req->tr_id);
            if (rbtn_by_id != NULL) {
                su_rbt_delete(timer->t_reqtree_by_id, rbtn_by_id);
            } else {
                ss_derror;
                /* should not happen!, because SsTimerCancelRequest
                 * now does not remove the record if timer is firing
                 * already.
                 */
            }
        }
        timer->t_last_now = now;
        TIMER_SEMEXIT(timer->t_sem);
        SS_POPNAME;
}


#ifdef SS_MT

static void SS_CALLBACK timer_threadfun(void)
{
        SS_PUSHNAME("timer_threadfun");

#ifdef SS_TIMER_MSEC
        timer->t_last_now = SsTimeMs();
#else
        timer->t_last_now = SsTime(NULL);
#endif
        while (!timer->t_shutting_down) {
            timer_run();
            SsMesRequest(timer->t_msg, timer->t_timetonextevent_ms);
        }
        /* shutting down */
        SsMesSend(timer->t_shutdownmsg);

        SS_POPNAME;

#if (defined(SS_MYSQL) || defined(SS_MYSQL_AC)) && defined(MYSQL_DYNAMIC_PLUGIN)
        return;
#else
        SsThrExit();
#endif
}

#else /* SS_MT */

long SsTimerAdvance(void)
{
        if (timer == NULL) {
            return TIMER_MAX_EVENTWAIT_TIME_MS;
        }
        timer_run();
        return (timer->t_timetonextevent_ms);
}

#endif /* SS_MT */

static void timer_curtime_callback(
        void* ctx __attribute__ ((unused)),
        SsTimerRequestIdT tr_id __attribute__ ((unused)))
{
        ss_timer_curtime_sec = SsTime(NULL);
}

void SsTimerGlobalInit(void)
{
        ss_pprintf_1(("SsTimerGlobalInit entered\n"));

        if (timer != NULL) {
            CHK_TIMER(timer);
            return;
        }
        SS_PUSHNAME("SsTimerGlobalInit");
        timer = SSMEM_NEW(SsTimerT);
        timer->t_check = SS_CHK_TIMER;
        timer->t_req_id_ctr = TIMER_MAX_REQ_ID;
        timer->t_persistent_req_id_ctr = TIMER_MAX_PERSISTENT_REQ_ID;
        timer->t_reqtree_by_endtime = su_rbt_init(tr_compare_by_endtime, NULL);
        timer->t_reqtree_by_id = su_rbt_inittwocmp(tr_insert_compare_by_id,
                                                   tr_search_compare_by_id,
                                                   tr_done);
#ifdef SS_TIMER_MSEC
        timer->t_last_now = SsTimeMs();
#else
        timer->t_last_now = SsTime(NULL);
#endif
        timer->t_timetonextevent_ms = TIMER_MAX_EVENTWAIT_TIME_MS;
#ifdef SS_MT
        timer->t_shutting_down = FALSE;
        timer->t_msg = SsMesCreateLocal();
        timer->t_shutdownmsg = SsMesCreateLocal();
        timer->t_sem = SsSemCreateLocal(SS_SEMNUM_NOTUSED);
        timer->t_thread = SsThrInit(timer_threadfun,
                                    "SsTimerThread",
                                    32768);
        SsThrEnable(timer->t_thread);
#endif /* SS_MT */
        timer_initialized = TRUE;
        SsTimerAddPersistentRequest(1000L, timer_curtime_callback, NULL);
        SS_POPNAME;
}

void SsTimerAddPersistentRequest(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx)
{
        SS_PUSHNAME("SsTimerAddPersistentRequest");

        TimerAddRequest(
            timeout_ms,
            callbackfun,
            callbackctx,
            TRUE,
            NULL);

        SS_POPNAME;
}

SsTimerRequestIdT SsTimerAddRequest(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx)
{
        timer_request_t* req;

        SS_PUSHNAME("SsTimerAddRequest");

        req = TimerAddRequest(
                timeout_ms,
                callbackfun,
                callbackctx,
                FALSE,
                NULL);

        SS_POPNAME;

        return(req->tr_id);
}

SsTimerRequestIdT SsTimerAddRequestWithFreefunc(
        long timeout_ms,
        timeout_callbackfun_t callbackfun,
        void* callbackctx,
        void (*freefunc)(void* ctx))
{
        timer_request_t* req;

        SS_PUSHNAME("SsTimerAddRequestWithFreefunc");

        req = TimerAddRequest(
                timeout_ms,
                callbackfun,
                callbackctx,
                FALSE,
                freefunc);

        SS_POPNAME;

        return(req->tr_id);
}

long SsTimerCancelRequestGetCtx(SsTimerRequestIdT tr_id, void** p_ctx)
{
        long time_left;
        su_rbt_node_t* rbtn_by_id;
        su_rbt_node_t* rbtn_by_endtime;
        timer_request_t* tr;
        
        CHK_TIMER(timer);
        TIMER_SEMENTER(timer->t_sem);
        rbtn_by_id = su_rbt_search(timer->t_reqtree_by_id, (void*)tr_id);
        if (rbtn_by_id != NULL) {
            tr = (timer_request_t*)su_rbtnode_getkey(rbtn_by_id);
            CHK_TR(tr);
            ss_dassert(tr->tr_id == tr_id);
            if (p_ctx != NULL) {
                *p_ctx = tr->tr_callbackctx;
            }
#ifdef SS_TIMER_MSEC
            time_left = (tr->tr_starttime + tr->tr_timeout - SsTimeMs());
            if (time_left < 0) {
                time_left = 0;
            }
#else
            time_left = (tr->tr_starttime + tr->tr_timeout - SsTime(NULL));
            if (time_left < 0) {
                time_left = 0;
            }
            time_left *= 1000UL;
#endif
            ss_pprintf_2(("SsTimerCancelRequest: request 0x%08lX cancelled, time left = %ld ms\n",
                          (long)tr_id, time_left));
            rbtn_by_endtime = su_rbt_search(timer->t_reqtree_by_endtime,
                                            tr);
            if (rbtn_by_endtime != NULL) {
                su_rbt_delete(timer->t_reqtree_by_endtime, rbtn_by_endtime);
                su_rbt_delete(timer->t_reqtree_by_id, rbtn_by_id);
            } else {
                /* else timer request has fired in the meantime */
#ifndef SS_MT
                ss_derror;
#endif /* !SS_MT */
                tr->tr_cancelled = TRUE;
            }
        } else {
            if (p_ctx != NULL) {
                *p_ctx = NULL;
            }
            time_left = 0;
        }
        TIMER_SEMEXIT(timer->t_sem);
        return (time_left);
}

long SsTimerCancelRequest(SsTimerRequestIdT tr_id)
{
        return (SsTimerCancelRequestGetCtx(tr_id, NULL));
}

bool SsTimerRequestIsValid(SsTimerRequestIdT tr_id)
{
        su_rbt_node_t* rbtn_by_id;
        timer_request_t* tr;
        bool is_valid;
        
        CHK_TIMER(timer);
        TIMER_SEMENTER(timer->t_sem);
        rbtn_by_id = su_rbt_search(timer->t_reqtree_by_id, (void*)tr_id);
        if (rbtn_by_id != NULL) {
            tr = (timer_request_t*)su_rbtnode_getkey(rbtn_by_id);
            CHK_TR(tr);
            ss_dassert(tr->tr_id == tr_id);
            is_valid = !tr->tr_cancelled;
        } else {
            is_valid = FALSE;
        }
        TIMER_SEMEXIT(timer->t_sem);
        return (is_valid);
}

void SsTimerGlobalDone(void)
{
        int i;
        ss_debug(timer_request_t* req;)

        ss_pprintf_1(("SsTimerGlobalDone called.\n"));
        if (timer == NULL) {
            return;
        }
        CHK_TIMER(timer);
#ifdef SS_MT
        timer->t_shutting_down = TRUE;
        SsMesSend(timer->t_msg);
        SsMesWait(timer->t_shutdownmsg);
        SsMesFree(timer->t_shutdownmsg);
        SsMesFree(timer->t_msg);
        SsSemFree(timer->t_sem);
        for (i = 0; i < 10; i++) {
            SsThrSwitch();
        }
        SsThrDone(timer->t_thread);
#endif /* SS_MT */
#ifdef SS_DEBUG
        /* Check that we have no requests left. */
        {
            su_rbt_node_t* rbtn;

            rbtn = su_rbt_min(timer->t_reqtree_by_id, NULL);
            while (rbtn != NULL) {
                req = su_rbtnode_getkey(rbtn);
                CHK_TR(req);
#ifdef SS_DEBUG
#if 0
                if (!req->tr_persistent) {
                    ss_testlog_print((char *)"sstimer.c:SsTimerGlobalDone:!req->tr_persistent\n");
                    ss_testlog_print((char *)"req callstack\n");
                    ss_testlog_printcallstack(req->tr_callstk);
                }
#endif
#endif /* SS_DEBUG */
                rbtn = su_rbt_succ(timer->t_reqtree_by_id, rbtn);
            }
        }
#endif /* SS_DEBUG */
        su_rbt_done(timer->t_reqtree_by_endtime);
        su_rbt_done(timer->t_reqtree_by_id);
        SsMemFree(timer);
        timer = NULL;
        timer_initialized = FALSE;
}

long SsTimerNextTimeout(void)
{
        SsTimeT now;
        SsTimeT next;
        
        CHK_TIMER(timer);
        TIMER_SEMENTER(timer->t_sem);
#ifdef SS_TIMER_MSEC
        now = SsTimeMs();
        if (now < timer->t_last_now) {
            /* time inconsistency */
            now = timer->t_last_now;
        }
        next = timer->t_last_now + timer->t_timetonextevent_ms;
        TIMER_SEMEXIT(timer->t_sem);
        if (next < now) {
            return (0);
        }
        return next - now;
#else
        now = SsTime(NULL);
        if (now < timer->t_last_now) {
            /* time inconsistency */
            now = timer->t_last_now;
        }
        next = timer->t_last_now + timer->t_timetonextevent_ms / 1000UL;
        TIMER_SEMEXIT(timer->t_sem);
        if (next < now) {
            return (0);
        }
        return ((next - now) * 1000UL);
#endif
}
