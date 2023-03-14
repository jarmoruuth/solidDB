/*************************************************************************\
**  source       * ssthread.c
**  directory    * ss
**  description  * General thread interface
**               *
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

Portable thread routines. These routines use system provided thread
services.

Limitations:
-----------

Available only where system supports threads.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#define SSTHREAD_C

#include "ssenv.h"
#include "sswinnt.h"
#include "ssc.h"
#include "ssdebug.h"
#include "sslimits.h"
#include "ssstddef.h"
#include "ssstring.h"
#include "sstime.h"
#include "sstraph.h"
#include "sssem.h"
#include "ssmem.h"
#include "ssmempag.h"
#include "sssprint.h"
#include "sspmon.h"
#include "ssthread.h"
#if defined(SS_LINUX) 
#include <errno.h>
#include <unistd.h>
#endif /* Linux  */
#if defined(SS_SOLARIS) && defined(SS_MT)
#include <thread.h>
#endif /* MT Solaris */

#define SS_THREAD_DEFAULTSTACKSIZE  (128 * 1024)

#ifdef SS_THRIDBYSTK

/* stack/thread id record */
typedef struct {
        void* tsb_stkbot;   /* stack bottom pointer */
        ulong tsb_id;       /* thread id >= 1  */
        void** tsb_data;
} thr_stkbot_t;

/* lookup table for stack bottoms */
typedef struct {
        size_t          ta_thrcnt;      /* # of threads */
        thr_stkbot_t    ta_stkbots[1 /* actually: ta_thrcnt */];
} thr_stkbotarray_t;

#endif /* SS_THRIDBYSTK */

/* native thread ids */
typedef struct {
        ulong    tid_count;  /* # of threads */
        ulong    tid_alloced;/* # of items allocated in tid_ids */
        ulong*   tid_ids;    /* thread ids */
} thr_nativeid_t;

#define SS_THRNATIVEIDS_ALLOCSIZE   10
static thr_nativeid_t* volatile thr_nativeids = NULL;

#ifndef SS_THRIDBYSTK
static SsSemT* thr_nativeidsmutex = NULL;
#endif


int ss_thr_numthreads = 1;

static bool thr_force_system_scope = TRUE;
void SsThrForceNewThreadsToSystemScope(bool yesorno)
{
        thr_force_system_scope = yesorno;
}

/* This is set to true when SsThrFunInit_INTERNAL() is called */
static bool thrfuninit_internal_called = FALSE;

/* Out of memory error message */
static char out_of_memory_text[] =
        "SOLID Fatal error: Out of central memory";


/*#***********************************************************************\
 *
 *		ThrCreateFailed
 *
 * Report failure to create a thread.
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
static void ThrCreateFailed(char* name, int rc)
{
        char buf[255];

        SsSprintf(
            buf,
            "SOLID Fatal Error: Failed to create a new thread (numthreads=%d, rc=%d",
            ss_thr_numthreads, rc);
        if (name != NULL) {
            SsSprintf(buf + strlen(buf), ", name=%.80s", name);
        }
        strcat(buf, ")\n");

        SsAssertionFailureText(buf, (char *)__FILE__, __LINE__);
}

/**************************************************************************/
#if defined(SS_NT) && defined(SS_MT)
/**************************************************************************/

#ifdef MSVC7_NT
#pragma runtime_checks("", off)
#endif /* MSVC7_NT */

#include <process.h>

#include "ssstdlib.h"
#include "sswinnt.h"

#include "ssmem.h"

#if defined(SS_NT) 
#  define SS_THRFUN_T   _CRTAPI1
#else
#  define SS_THRFUN_T
#endif

typedef enum {
        ts_init,
        ts_disabled,
        ts_enabled
} thrstate_t;

struct SsThreadStruct {
        SsThrFunT   sthr_initfun;
        void*       sthr_param;
        size_t      sthr_stacksize;
        thrstate_t  sthr_state;
#ifdef MSVC7_NT
        uintptr_t   sthr_id;
#else
        long        sthr_id;
#endif
        char*       sthr_name;
        int         sthr_errno;
};

typedef struct {
        SsThrFunT   thri_initfun;
        void*       thri_param;
} thr_maininfo_t;

SsThreadT* SsThrInit(SsThrFunT initfunp, const char* thrname, size_t stacksize)
{
        SsThreadT* th = (SsThreadT*)SsMemAlloc(sizeof(SsThreadT));

        if (stacksize == SS_THREAD_USEDEFAULTSTACKSIZE) {
            stacksize = SS_THREAD_DEFAULTSTACKSIZE;
        }

        th->sthr_initfun = initfunp;
        th->sthr_param = NULL;
        th->sthr_stacksize = (size_t)(SS_MAX(16*1024, (size_t)(4096 * (stacksize/4096 + 1))));
        th->sthr_state = ts_init;
        th->sthr_name = thrname;
        th->sthr_errno = 0;
        return(th);
}

void SsThrDone(SsThreadT* thr)
{
        SsMemFree(thr);
}

#ifdef SS_USE_WIN32TLS_API

#define SS_THRGLOBALINIT_DEFINED

static DWORD tls_index = 0;


void SsThrGlobalInit(void)
{
        if (tls_index == 0) {
            tls_index = TlsAlloc();
            if (tls_index == 0xFFFFFFFF) {
                DWORD errcode = GetLastError();
                ss_rc_error(errcode);
            }
        }
}

void SsThrGlobalDone(void)
{
        if (tls_index != 0) {
            TlsFree(tls_index);
            tls_index = 0;
        }
}

void SsThrDataSet(int index, void* data)
{
        void** thr_data;

        ss_rc_dassert(index >= 0, index);
        ss_rc_dassert(index < SS_THRDATA_MAX, index);

        /* Init thread specific storage */
        ss_dassert(tls_index != 0);
        thr_data = (void**)TlsGetValue(tls_index);
        if (thr_data == NULL) {
            bool r;
            thr_data = calloc(SS_THRDATA_MAX, sizeof(thr_data[0]));
            ss_assert(thr_data != NULL);
            r = TlsSetValue(tls_index, thr_data);
            ss_dassert(r);
        }
        thr_data[index] = data;
}

void* SsThrDataGet(int index)
{
        void** thr_data;

        ss_rc_dassert(index >= 0, index);
        ss_rc_dassert(index < SS_THRDATA_MAX, index);
        ss_dassert(tls_index != 0);

        thr_data = (void**)TlsGetValue(tls_index);
        if (thr_data == NULL) {
            return(NULL);
        } else {
            return(thr_data[index]);
        }
}

void SsThrDataDone()
{
        void** thr_data;

        ss_dassert(tls_index != 0);
        thr_data = (void**)TlsGetValue(tls_index);
        if (thr_data != NULL) {
            free(thr_data);
        }
        TlsSetValue(tls_index, NULL);
}

#endif /* SS_USE_WIN32TLS_API */

/*#***********************************************************************\
 *
 *		ss_thr_main
 *
 * Thread main function needed to free qmem thread context automatically
 *
 * Parameters :
 *
 *	thri - in, take
 *		Info for user thread  function call.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */

#ifndef SS_USE_WIN32TLS_API
static __declspec(thread) void* thr_exit_context = NULL;
#endif

#define SET_THR_EXIT_CONTEXT(n) thr_exit_context = (void*)(n)
#define GET_THR_EXIT_CONTEXT thr_exit_context

static void SS_THRFUN_T ss_thr_main(thr_maininfo_t* thri)
{
        jmp_buf b;
#ifdef SS_USE_WIN32TLS_API
        void* thr_exit_context;
        void** thr_data;
#endif
        SET_THR_EXIT_CONTEXT(b);

#ifdef SS_USE_WIN32TLS_API
        SsThrDataSet(SS_THRDATA_EXITCTX, thr_exit_context);
#endif
        if (setjmp(b) == 0) {
            ss_dprintf_2(("ss_thr_main(): thrid = %u\n", SsThrGetid()));
            (*((SsThrPrmFunT)thri->thri_initfun))(thri->thri_param);
        }
        SsMemFree(thri);
        SsQmemCtxDone(NULL);

#ifdef SS_USE_WIN32TLS_API
        SsThrDataDone();
#endif
        _endthread();
}

bool SsThrEnableBool(SsThreadT* thr)
{
        if (thr->sthr_state == ts_init) {
            thr_maininfo_t* thri;

            thri = SSMEM_NEW(thr_maininfo_t);

            thri->thri_initfun = thr->sthr_initfun;
            thri->thri_param = thr->sthr_param;

            thr->sthr_id = _beginthread(
                                (void (SS_THRFUN_T *)(void*))ss_thr_main,
                                thr->sthr_stacksize,
                                thri);
            if (thr->sthr_id == -1L) {
                thr->sthr_errno = errno;
                SsMemFree(thri);
                return(FALSE);
            }
            SsSemEnter(ss_lib_sem);
            ss_thr_numthreads++;
            SS_PMON_SET(SS_PMON_SS_THREADCOUNT, ss_thr_numthreads);
            SsSemExit(ss_lib_sem);
            ss_dprintf_2(("SsThrEnableBool:name '%s', id %ld\n", thr->sthr_name, (long)thr->sthr_id));

        } else if (thr->sthr_state == ts_disabled) {
            DWORD rc;

            rc = ResumeThread((HANDLE)thr->sthr_id);
            ss_rc_assert(rc != -1L, rc);
        } else {
            ss_rc_error(thr->sthr_state);
        }
        thr->sthr_state = ts_enabled;

        return(TRUE);
}

void SsThrEnable(SsThreadT* thr)
{
        if (!SsThrEnableBool(thr)) {
            ThrCreateFailed(thr->sthr_name, thr->sthr_errno);
        }
}

SsThreadT* SsThrInitParam(SsThrPrmFunT initfunp, const char* thrname, size_t stacksize, void* prm)
{
        SsThreadT* thr;
        thr = SsThrInit((SsThrFunT)initfunp, thrname, stacksize);
        if (thr == NULL) {
            return(NULL);
        }
        thr->sthr_param = prm;
        return(thr);
}

void SsThrDisable(SsThreadT* thr)
{
        DWORD rc;

        rc = SuspendThread((HANDLE)thr->sthr_id);
        /* This is allowed to fail if the thread has already exited
         *  itself
         */
        /* ss_rc_assert(rc != -1L, rc); */

        thr->sthr_state = ts_disabled;
}

void SsThrExit(void)
{
#ifdef SS_USE_WIN32TLS_API
        void* thr_exit_context;
#endif
        ss_dprintf_2(("SsThrExit:id %d\n", SsThrGetid()));
#if 0
        ss_trap_threaddone();
#endif
        SsSemEnter(ss_lib_sem);
        ss_thr_numthreads--;
        SS_PMON_SET(SS_PMON_SS_THREADCOUNT, ss_thr_numthreads);
        SsSemExit(ss_lib_sem);

#ifdef SS_USE_WIN32TLS_API
        thr_exit_context = SsThrDataGet(SS_THRDATA_EXITCTX);
#endif
        ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
        ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
        longjmp(GET_THR_EXIT_CONTEXT, 1);
}

void SsThrSwitch(void)
{
        Sleep(0L);
}

void SsThrSleep(ulong time_ms)
{
        Sleep(time_ms);
}

unsigned SsThrGetid(void)
{
        return(GetCurrentThreadId());
}

/**************************************************************************/
#elif defined(SS_MT) && defined(SS_PTHREAD)
/* Posix.4a threads */
/**************************************************************************/

#include <time.h>
#include <pthread.h>

#if defined(SS_SOLARIS) 
#  include <errno.h>
#endif

#include "ssstdlib.h"

#include "ssqmem.h"
#include "ssmem.h"
#include "sssem.h"

/* In most pthreads implementations the thread local storage API is too
 * slow for protection of memory allocator.
 * that's why we use the home-made mechanism for identifying
 * the running thread.
 */
#ifdef SS_THRIDBYSTK
#include "sssetjmp.h"
#endif /* SS_THRIDBYSTK */

typedef enum {
        TS_INIT,
        TS_DISABLED,
        TS_ENABLED
} thrstate_t;

struct SsThreadStruct {
        SsThrFunT       sthr_initfun;
        void*           sthr_param;
        size_t          sthr_stacksize;
        thrstate_t      sthr_state;
        pthread_t       sthr_thr;
        pthread_attr_t  sthr_attr;
        char*           sthr_name;
        int             sthr_errno;
};

pthread_key_t          ssthread_tls_key;
static SsSemT*         thrid_mutex;
static uint            thrid_ctr = 0;
static bool            thrsys_initialized = FALSE;

#ifdef SS_THRIDBYSTK

typedef struct {
        SsThrFunT   thri_initfun;
        void*       thri_param;
} thr_maininfo_t;

static void ss_thr_main(thr_maininfo_t* thri);
static thr_stkbot_t* thr_getstkbot(void);
#endif /* SS_THRIDBYSTK */


SsMesT* ss_thread_getsleepmes(void)
{
        SsMesT* sleepmes = SsThrDataGet(SS_THRDATA_SLEEPMES);
        if (sleepmes != NULL) {
            return(sleepmes);
        } else {
            sleepmes = SsMesCreateLocal();
            SsMesReset(sleepmes);
            SsThrDataSet(SS_THRDATA_SLEEPMES, sleepmes);
            return(sleepmes);
        }
}

void ss_thread_freesleepmes(void)
{
        SsMesT* sleepmes = SsThrDataGet(SS_THRDATA_SLEEPMES);
        if (sleepmes != NULL) {
            SsMesFree(sleepmes);
        }
}

#ifdef SS_PTHREAD_CMA

void** ss_pthread_getspecific(pthread_key_t tls_key)
{
        void* p;
        pthread_getspecific(tls_key, &p);
        return((void**)p);
}
void ss_pthread_attr_init(pthread_attr_t* p_attr)
{
        *p_attr = pthread_attr_default;
}
void ss_pthread_attr_destroy(pthread_attr_t* p_attr)
{
        SS_NOTUSED(p_attr);
}

#define SS_THRATTR(_thr)  (pthread_attr_default)
#define ss_pthread_key_create pthread_keycreate

#else /* SS_PTHREAD_CMA */

#define ss_pthread_getspecific(tls_key) \
    ((void**)(pthread_getspecific(tls_key)))

void ss_pthread_attr_init(pthread_attr_t* p_attr)
{
        pthread_attr_init(p_attr);
}

void ss_pthread_attr_destroy(pthread_attr_t* p_attr)
{
        pthread_attr_destroy(p_attr);
}

#define SS_THRATTR(_thr)  (&((_thr)->sthr_attr))
#define ss_pthread_key_create pthread_key_create

#endif /* SS_PTHREAD_CMA */

#ifndef SS_THRIDBYSTK

void** SsThrDataInit(void)
{
        void** thr_data;

        ss_dassert(thrsys_initialized);
        thr_data = calloc(SS_THRDATA_MAX, sizeof(thr_data[0]));
        ss_assert(thr_data != NULL);
#ifdef SS_LINUX
        /*
         * ss_dprintf_[1234] functions use SsThrGetid() locally to
         * print out the thread number. Calling it from here causes a
         * infinite recursion.
         *
         * mikko / Thu Feb 14 07:05:57 PST 2002
         *
         */

        /* ss_dprintf_2(("SsThrDataInit:pthread_key=%u\n", (uint)ssthread_tls_key)); */
#endif /* SS_LINUX */
        pthread_setspecific(ssthread_tls_key, thr_data);
#ifdef SS_DEBUG
        {
            void** p;
            p = ss_pthread_getspecific(ssthread_tls_key);
            /* ss_dprintf_2(("SsThrDataSet:pthread_getspecific() gave %08lX\n", (ss_int4_t)(ulong)p)); */
            ss_assert(p == thr_data);
        }
#endif /* SS_DEBUG */
        SsSemEnter(thrid_mutex);
        thrid_ctr++;
        /* ss_dprintf_2(("SsThrDataInit thrid = %d\n", thrid_ctr)); */
        thr_data[SS_THRDATA_THRID] = (void*)(ulong)thrid_ctr;
        SsSemExit(thrid_mutex);
        return (thr_data);
}

static void ss_thrDataDone(void* p)
{
        void** data = (void**)p;
        void* ctx = data[SS_THRDATA_QMEMCTX];

        if (ctx != NULL) {
            SsQmemCtxDone(ctx);
            data[SS_THRDATA_QMEMCTX] = NULL;
        }
        free(p);
}

void SsThrDataSet(int index, void* data)
{
        void** thr_data;

        ss_rc_dassert(index >= 0, index);
        ss_rc_dassert(index < SS_THRDATA_MAX, index);
        ss_dassert(thrsys_initialized);

#ifdef SS_LINUX
        ss_dprintf_1(("SsThrDataSet:pthread_key=%u\n", (uint)ssthread_tls_key));
#endif /* SS_LINUX */
        thr_data = ss_pthread_getspecific(ssthread_tls_key);
        ss_dprintf_1(("SsThrDataSet:pthread_getspecific() gave %08lX\n", (ss_int4_t)(ulong)thr_data));
        if (thr_data == NULL) {
            thr_data = SsThrDataInit();
        }
        thr_data[index] = data;
}

#endif /* !SS_THRIDBYSTK */

#define SS_THRGLOBALINIT_DEFINED
void SsThrGlobalInit(void)
{
        int rc;

        if (!thrsys_initialized) {
#ifndef SS_THRIDBYSTK
            thrid_mutex = malloc(SsSemSizeLocal());
            ss_assert(thrid_mutex != NULL);
            SsSemCreateLocalBuf(thrid_mutex, SS_SEMNUM_NOTUSED);
            rc = ss_pthread_key_create(&ssthread_tls_key, ss_thrDataDone);
            ss_rc_assert(rc == 0, rc);
#ifdef SS_LINUX
            ss_dprintf_1(("SsThrGlobalInit:pthread_key=%u\n", (uint)ssthread_tls_key));
#endif /* SS_LINUX */
#endif /* SS_THRIDBYSTK */
            thrsys_initialized = TRUE;
        }
}

void SsThrGlobalDone(void)
{
        if (thrsys_initialized) {
#ifndef SS_THRIDBYSTK
            SsSemFreeBuf(thrid_mutex);
            free(thrid_mutex);
            thrid_mutex = NULL;
#endif /* SS_THRIDBYSTK */
            thrsys_initialized = FALSE;
        }
}

static SsThreadT* SsThrInit2(SsThrFunT initfunp, const void* param, char* thrname, size_t stacksize)
{
        size_t page_size = 0;
        SsThreadT* th = (SsThreadT*)SsMemAlloc(sizeof(SsThreadT));

        if (page_size == 0) {
#ifdef SS_PAGED_MEMALLOC_AVAILABLE
            page_size = SsMemPageSize();
#else /* SS_PAGED_MEMALLOC_AVAILABLE */
            page_size = 4096;
#endif /* SS_PAGED_MEMALLOC_AVAILABLE */
        }
        if (stacksize == SS_THREAD_USEDEFAULTSTACKSIZE) {
            stacksize = SS_THREAD_DEFAULTSTACKSIZE;
        }
        th->sthr_initfun = initfunp;
        th->sthr_param = param;
        ss_pthread_attr_init(&th->sthr_attr);
        th->sthr_stacksize = (size_t)(SS_MAX(128*1024, (size_t)(page_size * ((stacksize-1)/page_size + 1))));
#if 0
        /* Ari tried this in solaris, but THR_MIN_STACK == -1 !!!
         * It causes a core dump.
         */
        /* ss_rc_dassert(th->sthr_stacksize >= THR_MIN_STACK, THR_MIN_STACK); */
        th->sthr_stacksize = THR_MIN_STACK;
#endif
#if 1 /* We must create a detached thread, because currently we NEVER join
         any threads we create! */
        pthread_attr_setdetachstate(&th->sthr_attr, PTHREAD_CREATE_DETACHED);
#endif /* SS_LINUX */
#if 1
        if (stacksize != 0) {
            pthread_attr_setstacksize(&th->sthr_attr, th->sthr_stacksize);
        }
#else /* if you expect that stacksize setting does not work, try this. */
        {
                size_t sz;
                th->sthr_stacksize = (size_t)(SS_MAX(100*1024, (size_t)(page_size * ((stacksize-1)/page_size + 1))));
                pthread_attr_setstacksize(&th->sthr_attr, th->sthr_stacksize);
                pthread_attr_getstacksize(&th->sthr_attr, &stacksize);
                SsPrintf("Stacksize = %ld\n", (long)stacksize);
        }
#endif /* !SPARC */
#if defined(SS_SOLARIS)
        /* this is a hack to improve SMP scalability in Solaris */
        if (thr_force_system_scope) {
            pthread_attr_setscope(&th->sthr_attr, PTHREAD_SCOPE_SYSTEM);
        }
#endif /* SS_SOLARIS */

        th->sthr_state = TS_INIT;
        th->sthr_name = thrname;
        th->sthr_errno = 0;
        return(th);
}

SsThreadT* SsThrInit(SsThrFunT initfunp, const char* thrname, size_t stacksize)
{
        SsThreadT* th;

        th = SsThrInit2(initfunp, NULL, (char *)thrname, stacksize);
        return (th);
}

SsThreadT* SsThrInitParam(SsThrPrmFunT initfunp, const char* thrname, size_t stacksize, void* prm)
{
        SsThreadT* th = SsThrInit2((SsThrFunT)initfunp, prm, (char *)thrname, stacksize);
        return (th);
}

void SsThrDone(SsThreadT* thr)
{
        void *sleepmes = SsThrDataGet(SS_THRDATA_SLEEPMES);
        if (sleepmes) {
            SsMemFree(sleepmes);
            SsThrDataSet(SS_THRDATA_SLEEPMES,NULL);
        }
        ss_pthread_attr_destroy(&thr->sthr_attr);
        SsMemFree(thr);
}

bool SsThrEnableBool(SsThreadT* thr)
{
        int rc;

        SsSemEnter(ss_lib_sem);
        ss_thr_numthreads++;
        SS_PMON_SET(SS_PMON_SS_THREADCOUNT, ss_thr_numthreads);
        SsSemExit(ss_lib_sem);

        if (thr->sthr_state == TS_INIT) {
#ifdef SS_THRIDBYSTK
            thr_maininfo_t* thri;

            thri = SSMEM_NEW(thr_maininfo_t);

            thri->thri_initfun = thr->sthr_initfun;
            thri->thri_param = thr->sthr_param;

            rc = pthread_create(
                    &thr->sthr_thr,
                    SS_THRATTR(thr),
                    (void*(*)(void*))ss_thr_main,
                    thri);
#else
            rc = pthread_create(
                    &thr->sthr_thr,
                    SS_THRATTR(thr),
                    (void*(*)(void*))thr->sthr_initfun,
                    thr->sthr_param);
#endif
            if (rc != 0) {
                thr->sthr_errno = errno;
#ifdef SS_THRIDBYSTK
                SsMemFree(thri);
#endif
                ss_dprintf_1(("SsThrEnableBool: errno = %d\n", thr->sthr_errno));
                return(FALSE);
            }
            ss_dprintf_2(("SsThrEnableBool:name '%s'\n", thr->sthr_name))
        } else if (thr->sthr_state == TS_DISABLED) {
            ss_error;
        } else {
            ss_rc_error(thr->sthr_state);
        }
        thr->sthr_state = TS_ENABLED;

        return(TRUE);
}

void SsThrEnable(SsThreadT* thr)
{
        if (!SsThrEnableBool(thr)) {
            ThrCreateFailed(thr->sthr_name, thr->sthr_errno);
        }
}

void SsThrDisable(SsThreadT* thr __attribute__ ((unused)))
{
        ss_error;
}

void SsThrExit(void)
{
        ss_dprintf_2(("SsThrExit:id %d\n", SsThrGetid()));

        SsSemEnter(ss_lib_sem);
        ss_thr_numthreads--;
        SS_PMON_SET(SS_PMON_SS_THREADCOUNT, ss_thr_numthreads);
        SsSemExit(ss_lib_sem);
        ss_thread_freesleepmes();
#ifdef SS_THRIDBYSTK
        {
            thr_stkbot_t* stkbot;
            void* jbuf;

            stkbot = thr_getstkbot();
            jbuf = stkbot->tsb_stkbot;
            ss_trap_threaddone();
            ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
            ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
            longjmp(jbuf, 1);
        }
#else
        ss_trap_threaddone();
        pthread_exit(NULL);
#endif
}

void SsThrSwitch(void)
{
#if defined(SS_SOLARIS)
        thr_yield();
#elif defined(SS_LINUX) 
        sched_yield();
#elif (defined(SS_FREEBSD) && (SS_FREEBSD >= 5))
        pthread_yield();
#else
        sleep(0);
#endif /* MT Solaris */
}

void SsThrSleep(ulong time_ms)
{
#ifdef SS_NANOSLEEP_THREADSCOPE
        struct timespec rqtp;
        struct timespec rem;

        if (time_ms != 0L) {
            int rc;
            rqtp.tv_sec = time_ms / 1000L;
            rqtp.tv_nsec = (time_ms % 1000L) * 1000000L;
            do {
                rc = nanosleep(&rqtp, &rem);
                rqtp = rem;
            } while (rc < 0 && errno == EINTR);
        } else {
            SsThrSwitch();
        }
#else /* SS_NANOSLEEP_THREADSCOPE */
#ifndef SS_GETTIMEOFDAY_AVAILABLE
        if (time_ms >= 0 && time_ms < 1000) {
            /* timeout between 0 and 1000 ms is handled as 0 in SsMesRequest */
            SsThrSwitch();
        } else
#endif /* !SS_GETTIMEOFDAY_AVAILABLE */
        {
            /* We use thread specific message instead of "busyloop" */
            SsMesT* sleepmes;
            sleepmes = ss_thread_getsleepmes();
            SsMesRequest(sleepmes, time_ms);
        }

#endif /* SS_NANOSLEEP_THREADSCOPE */
}


#if !defined(SS_THRIDBYSTK)
unsigned SsThrGetid(void)
{
        uint thrid;

        /*
         * ss_dprintf_[1234] functions use SsThrGetid() locally to
         * print out the thread number. Calling it from here causes a
         * infinite recursion.
         *
         * mikko / Thu Feb 14 07:05:57 PST 2002
         *
         */

        /* ss_dprintf_1(("In SsThrGetid\n")); */

        thrid = (uint)SsThrDataGet(SS_THRDATA_THRID);
        return (thrid);
}
#endif

/**************************************************************************/
#else /* !SS_NT && !SS_PTHREAD*/
/**************************************************************************/

#include "sstime.h"

#if defined(SS_WIN)

#include "ssproces.h"
#include "sswinint.h"

void SsThrSwitch(void)
{
        SsProcessSwitch();
}

unsigned SsThrGetid(void)
{
        return(SshInst);
}

void SsThrSleep(ulong time_ms)
{
        if (time_ms != 0L) {
            ulong enter_time = SsTime(NULL);
            ulong exit_time = enter_time + (time_ms/1000);

            do {
                SsThrSwitch();
            } while (SsTime(NULL) < exit_time);
        }
}

#else /* this is not SS_WIN */

void SsThrSwitch(void)
{
}

unsigned SsThrGetid(void)
{
        return(1);
}

void SsThrSleep(ulong time_ms)
{
#if defined(SS_UNIX) && !defined(SS_MT)
        /* for single-threaded unix */
        sleep(time_ms / 1000);
#endif /* SS_UNIX && !SS_MT */
}

#endif /* SS_WIN */

#endif /* SS_NT,  SS_PTHREAD */




/*#***********************************************************************\
 *
 *		thr_register_nomutex
 *
 * Registers calling thread's id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void thr_register_nomutex(void)
{
        /* Add native id to the array */
        if (thr_nativeids == NULL) {
            thr_nativeids = malloc(sizeof(thr_nativeid_t));
            thr_nativeids->tid_count = 0;
            thr_nativeids->tid_alloced = SS_THRNATIVEIDS_ALLOCSIZE;
            thr_nativeids->tid_ids = malloc((thr_nativeids->tid_alloced)
                                        * sizeof(ulong));

/*             ss_debug(SsPrintf("thr_nativeids allocated\n")); */
        } else if (thr_nativeids->tid_alloced == thr_nativeids->tid_count) {

            thr_nativeids->tid_alloced += SS_THRNATIVEIDS_ALLOCSIZE;
            thr_nativeids->tid_ids = realloc(thr_nativeids->tid_ids,
                                        (thr_nativeids->tid_alloced)
                                        * sizeof(ulong));
        }
        thr_nativeids->tid_ids[thr_nativeids->tid_count] = SsThrGetNativeId();
        (thr_nativeids->tid_count)++;
/*         SsPrintf("%s: registered thr: tid_id %x, thr->tid_count %d\n", __FUNCTION__, thr_nativeids->tid_ids[thr_nativeids->tid_count-1], thr_nativeids->tid_count); */
}

/*#***********************************************************************\
 *
 *		thr_unregister_nomutex
 *
 * Unregisters calling thread's id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void thr_unregister_nomutex(void)
{
        ulong thrid;
        uint i;

        ss_bassert(thr_nativeids != NULL);
        ss_bassert(thr_nativeids->tid_count > 0);

        thrid = SsThrGetNativeId();

        if (thr_nativeids->tid_count > 1) {

            for (i=0; i < thr_nativeids->tid_count; i++) {
                if (thrid == thr_nativeids->tid_ids[i]) {
                    /* move ids one step backwards (if not last) */
                    if (i <  (thr_nativeids->tid_count - 1)) {
                        memmove(&(thr_nativeids->tid_ids[i]),
                                &(thr_nativeids->tid_ids[i+1]),
                                sizeof(thr_nativeids->tid_ids[0]) *
                                (thr_nativeids->tid_count - (i+1)));
                    }
                    break;
                }
            }
        } else {
            ss_bassert(thrid == thr_nativeids->tid_ids[0]);;
        }
        (thr_nativeids->tid_count)--;

        /* we don't want the array to grow indefinetely,
          if the application keeps creating/destroying threads */
        if (((thr_nativeids->tid_alloced - SS_THRNATIVEIDS_ALLOCSIZE)
                > thr_nativeids->tid_count) &&
             (thr_nativeids->tid_alloced > SS_THRNATIVEIDS_ALLOCSIZE))  {

            thr_nativeids->tid_alloced -= SS_THRNATIVEIDS_ALLOCSIZE;
            thr_nativeids->tid_ids = realloc(thr_nativeids->tid_ids,
                                        (thr_nativeids->tid_alloced)
                                        * sizeof(ulong));
        }
}


/* This part contains code for identifying
 * running thread by the stack pointer.
 * Note some changes have to be made in the
 * thread starting code to utilize this.
 * the SS_PTHREAD portion contains these changes.
 */
#ifdef SS_THRIDBYSTK

#include "sssem.h"

/*##**********************************************************************\
 *
 *		THR_STKBOTARRAY_SIZE
 *
 * Calculates thread id lookup table size by number of threads
 *
 * Parameters :
 *
 *	thrcnt - in
 *		thread count
 *
 * Return value :
 *      # of bytes needed to store the lookup table
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#define THR_STKBOTARRAY_SIZE(thrcnt) \
        ((char*)&(((thr_stkbotarray_t*)NULL)->ta_stkbots[thrcnt]) - (char*)NULL)

#define SS_THRFUNINIT_DEFINED

/* thr_stackbots contains the current lookup table
 * for thread id.
 * thr_tmpstackbots contains a version of the lookup table
 * that is under modification by the thread prologue/epilogue
 * code.
 */
static thr_stkbotarray_t* volatile thr_stackbots = NULL;
static thr_stkbotarray_t* volatile thr_tmpstackbots = NULL;

/* This mutex secures the thread prologue/epilogue code */
static SsSemT* thr_startmutex = NULL;


/*#***********************************************************************\
 *
 *		thr_startmutex_enter
 *
 * Enters thread prologue/epilogue mutex
 *
 * Parameters :
 *
 *	mtx - use
 *		mutex object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void thr_startmutex_enter(SsSemT* mtx)
{
        int i;

        SsSemEnter(mtx);
        for (i = 0; i < 5000; i++) {
            SsThrSwitch();
        }
}

/*#***********************************************************************\
 *
 *		thr_stackbot_cmp
 *
 * Comparison function for qsort
 *
 * Parameters :
 *
 *	p_st1 - in, use
 *		stack bottom record pointer #1
 *
 *	p_st2 - in, use
 *		stack bottom record pointer #2
 *
 * Return value :
 *      logical subtraction result of the
 *      stack bottom pointers (-1 or 1)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int SS_CLIBCALLBACK thr_stackbot_cmp(
        const void* p_st1,
        const void* p_st2)
{
        if ((ulong)((const thr_stkbot_t*)p_st1)->tsb_stkbot >
            (ulong)((const thr_stkbot_t*)p_st2)->tsb_stkbot)
        {
            return (1);
        } else {
            ss_dassert((ulong)((const thr_stkbot_t*)p_st1)->tsb_stkbot <
                       (ulong)((const thr_stkbot_t*)p_st2)->tsb_stkbot);
            return (-1);
        }
}


/*#***********************************************************************\
 *
 *		thr_createid
 *
 * Creates an unused thread id for a new thread. The thread id is
 * the smallest unused value > 0.
 *
 * Parameters :
 *
 *	stackbots - in, use
 *          thread id lookup table
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
static uint thr_createid(thr_stkbotarray_t* stackbots)
{
        ss_byte_t* idbitmap;
        long firstfree;
        size_t bmsize;
        size_t bmbytesize;
        size_t i;
        size_t id;

        if (stackbots == NULL) {
            return (1);
        }
        bmsize = stackbots->ta_thrcnt + 1;
        bmbytesize = (bmsize + SS_CHAR_BIT - 1) / SS_CHAR_BIT;
        idbitmap = malloc(bmbytesize);
        memset(idbitmap, 0, bmbytesize);
        for (i = 0; i < bmsize - 1; i++) {
            id = stackbots->ta_stkbots[i].tsb_id - 1;
            if (id < bmsize) {
                idbitmap[id / SS_CHAR_BIT] |= 1 << (id % SS_CHAR_BIT);
            }
        }
        for (i = 0; i < bmbytesize; i++) {
            if (idbitmap[i] != (uchar)~0) {
                break;
            }
        }
        ss_dassert(i < bmbytesize);
        firstfree = i;
        for (i = 0; ;i++) {
            if ((idbitmap[firstfree] & (1 << i)) == 0) {
                break;
            }
        }
        firstfree *= SS_CHAR_BIT;
        firstfree += i;
        id = (uint)firstfree + 1;
        free(idbitmap);
        return (id);
}


/*##**********************************************************************\
 *
 *		SsThrFunInit_INTERNAL
 *
 * Initializes the thread ID by stack pointer lookup table for
 * the new thread. Must not be called explicitly by application.
 * the stack ID is as densely allocated as possible,
 * starting from 1 (=main).
 *
 * Parameters :
 *
 *	stackptr - in, use
 *		pointer jmp_buf record at bottom of stack
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsThrFunInit_INTERNAL(void* stackptr)
{
        bool is_main = FALSE;
        size_t thr_cnt;
        thr_stkbotarray_t* tmp;

        /* Flag that this function is called */
        thrfuninit_internal_called = TRUE;

#if SS_STACK_GROW_DIRECTION < 0   /* Stack grows to lower addr. */
        ss_dassert((ulong)stackptr > (ulong)&is_main);
#else
        ss_dassert((ulong)stackptr < (ulong)&is_main);
#endif
        if (thr_startmutex == NULL) {
            thr_startmutex = malloc(SsSemSizeLocal());
            if (thr_startmutex == NULL) {
                SsAssertionFailureText(out_of_memory_text, __FILE__, __LINE__);
            }
            SsSemCreateLocalBuf(thr_startmutex, SS_SEMNUM_NOTUSED);
        }
        thr_startmutex_enter(thr_startmutex);
        if (thr_stackbots == NULL) {
            thr_stackbots = malloc(THR_STKBOTARRAY_SIZE(1));
            if (thr_stackbots == NULL) {
                SsAssertionFailureText(out_of_memory_text, __FILE__, __LINE__);
            }
            thr_tmpstackbots = thr_stackbots;
            is_main = TRUE;
            thr_cnt = 1;
        } else {
            thr_cnt = thr_stackbots->ta_thrcnt + 1;
            if (thr_tmpstackbots == NULL) {
                thr_tmpstackbots = malloc(THR_STKBOTARRAY_SIZE(thr_cnt));
            } else {
                thr_tmpstackbots = realloc(thr_tmpstackbots,
                                        THR_STKBOTARRAY_SIZE(thr_cnt));
            }
        }
        thr_tmpstackbots->ta_thrcnt = thr_cnt;

        thr_tmpstackbots->ta_stkbots[thr_cnt - 1].tsb_stkbot = stackptr;
        thr_tmpstackbots->ta_stkbots[thr_cnt - 1].tsb_id =
            (ulong)thr_createid(thr_stackbots);
        thr_tmpstackbots->ta_stkbots[thr_cnt - 1].tsb_data =
            calloc(SS_THRDATA_MAX, sizeof(void*));
        if (thr_cnt > 1) {
            memcpy(
                thr_tmpstackbots->ta_stkbots,
                thr_stackbots->ta_stkbots,
                thr_stackbots->ta_thrcnt
                * sizeof(thr_stackbots->ta_stkbots[0]));
            qsort(
                thr_tmpstackbots->ta_stkbots,
                thr_cnt,
                sizeof(thr_tmpstackbots->ta_stkbots[0]),
                thr_stackbot_cmp);
        } else {
            ss_dassert(is_main);
        }
        tmp = thr_stackbots;
        /* The following assignment is
         * supposed to be atomic!
         */
        thr_stackbots = thr_tmpstackbots;

        if (is_main) {
            thr_tmpstackbots = NULL;
        } else {
            thr_tmpstackbots = tmp;
        }
        /* Register thread */
        thr_register_nomutex();

        SsSemExit(thr_startmutex);
}

#undef SsThrGetid

/*##**********************************************************************\
 *
 *		SsThrGetid
 *
 * Gets thread id by searching the stack address using binary search
 *
 * Parameters : 	 - none
 *
 * Return value :
 *      thread id, starting from 1, as densely allocated as possible.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint SsThrGetid(void)
{
        int p[1]; /* stack placeholder to allow other vars to be registers */
        thr_stkbotarray_t* stackbots;
        thr_stkbot_t* t_try; /* Ari renamed, try is reserved in c++ */
        uint n_per_2;
        uint n;
        ss_debug(thr_stkbot_t* base;)

        stackbots = thr_stackbots;
        n = stackbots->ta_thrcnt;
        ss_dassert(n);
        ss_debug(base =)
        t_try = stackbots->ta_stkbots;
        do {
            n_per_2 = n >> 1;
            t_try = t_try + n_per_2;
            if ((ulong)&p[0] < (ulong)t_try->tsb_stkbot) {
                n = n_per_2;
                t_try -= n_per_2;
            } else {
                ss_dassert((ulong)&p[0] > (ulong)t_try->tsb_stkbot);
                n -= n_per_2 + 1;
                t_try++;
            }
        } while (n);
#if SS_STACK_GROW_DIRECTION < 0   /* Stack grows to lower addr. */
        ss_dassert((uint)(t_try - base) < stackbots->ta_thrcnt);
        return ((uint)t_try->tsb_id);
#else
        ss_dassert((uint)(t_try - base - 1) < stackbots->ta_thrcnt);
        return ((uint)(t_try-1)->tsb_id);
#endif
}


/*#***********************************************************************\
 *
 *		thr_getstkbot
 *
 * Gets stack bottom record applicable to this thread.
 *
 * Parameters : 	 - none
 *
 * Return value - ref :
 *      pointer to stack ID record
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static thr_stkbot_t* thr_getstkbot(void)
{
        int p[1]; /* stack placeholder to allow other vars to be registers */
        thr_stkbotarray_t* stackbots;
        thr_stkbot_t* t_try;
        uint n_per_2;
        uint n;
        ss_debug(thr_stkbot_t* base;)

        stackbots = thr_stackbots;
        n = stackbots->ta_thrcnt;
        ss_dassert(n);
        ss_debug(base =)
        t_try = stackbots->ta_stkbots;
        do {
            n_per_2 = n >> 1;
            t_try = t_try + n_per_2;
            if ((ulong)&p[0] < (ulong)t_try->tsb_stkbot) {
                n = n_per_2;
                t_try -= n_per_2;
            } else {
                ss_dassert((ulong)&p[0] > (ulong)t_try->tsb_stkbot);
                n -= n_per_2 + 1;
                t_try++;
            }
        } while (n);
#if SS_STACK_GROW_DIRECTION < 0   /* Stack grows to lower addr. */
        ss_dassert((uint)(t_try - base) < stackbots->ta_thrcnt);
        return (t_try);
#else
        ss_dassert((uint)(t_try - base - 1) < stackbots->ta_thrcnt);
        return (t_try-1);
#endif
}

/*#***********************************************************************\
 *
 *		SsThrFunDone
 *
 * Deinitializes a thread ID lookup table entry
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
void SsThrFunDone(void)
{
        size_t thr_cnt;
        thr_stkbotarray_t* tmp;
        thr_stkbot_t* stkbot;
        size_t i;
        size_t j;
        void* thrdata;

        thr_startmutex_enter(thr_startmutex);
        thr_cnt = thr_stackbots->ta_thrcnt - 1;
        if (thr_tmpstackbots == NULL) {
            thr_tmpstackbots = malloc(THR_STKBOTARRAY_SIZE(thr_cnt));
        } else {
            thr_tmpstackbots = realloc(thr_tmpstackbots,
                                    THR_STKBOTARRAY_SIZE(thr_cnt));
        }
        thr_tmpstackbots->ta_thrcnt = thr_cnt;
        stkbot = thr_getstkbot();
        thrdata = stkbot->tsb_data;
        for (i = j = 0; i <= thr_cnt; i++) {
            if (&thr_stackbots->ta_stkbots[i] != stkbot) {
                thr_tmpstackbots->ta_stkbots[j] =
                    thr_stackbots->ta_stkbots[i];
                j++;
            }
        }
        tmp = thr_stackbots;
        /* The following assignment is
         * supposed to be atomic!
         */
        thr_stackbots = thr_tmpstackbots;

        thr_tmpstackbots = tmp;

        thr_unregister_nomutex();

        SsSemExit(thr_startmutex);
        if (thrdata != NULL) {
            uint i;
            for (i = 0; i < 1000; i++) {
                SsThrSwitch();
            }
            free(thrdata);
        }
}

/*#***********************************************************************\
 *
 *		ss_thr_main
 *
 * Thread main function when thread id is identified by stack
 * pointer.
 *
 * Parameters :
 *
 *	thr -
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
static void ss_thr_main(SsThreadT* thr)
{
        jmp_buf b;

        if (setjmp(b) == 0) {
            SsThrFunInit_INTERNAL(&b);
            ss_dprintf_1(("ss_thr_main(): thrid = %u\n", SsThrGetid()));
            (*((void(*)(void*))thr->sthr_initfun))(thr->sthr_param);
        }
        SsThrFunDone();

}

void SsThrDataSet(int index, void* data)
{
        thr_stkbot_t* stkbot;

        ss_assert((uint)index < SS_THRDATA_MAX);
        stkbot = thr_getstkbot();
        stkbot->tsb_data[index] = data;
}

void* SsThrDataGet(int index)
{
        thr_stkbot_t* stkbot;

        ss_assert((uint)index < SS_THRDATA_MAX);
        stkbot = thr_getstkbot();
        return (stkbot->tsb_data[index]);
}


#endif /* SS_THRIDBYSTK */

#if defined(SS_BETA)

SsThrInfoT ss_thr_info[SS_MAXTHRINFO];

int SsThrInfoInit(char* name)
{
        int i;

        SsSemEnter(ss_lib_sem);

        for (i = 1; i < SS_MAXTHRINFO; i++) {
            if (ss_thr_info[i].ti_name == NULL) {
                ss_thr_info[i].ti_name = name;
                ss_thr_info[i].ti_id = SsThrGetid();
                break;
            }
        }

        if (i == SS_MAXTHRINFO) {
            /* Too many threads. */
            i = 0;
        }

        SsSemExit(ss_lib_sem);

        return(i);
}

void SsThrInfoDone(int id)
{
        ss_dassert(id < SS_MAXTHRINFO);

        ss_thr_info[id].ti_name = NULL;
        ss_thr_info[id].ti_loopcount = 0;
}


#endif /* SS_BETA */

#ifndef SS_THRFUNINIT_DEFINED

void SsThrFunInit_INTERNAL(void* stackptr)
{
        thrfuninit_internal_called = TRUE;
        SS_NOTUSED(stackptr);

#ifdef SS_USE_WIN32TLS_API
        if (tls_index == 0) {
            tls_index = TlsAlloc();
            if (tls_index == 0xFFFFFFFF) {
                DWORD errcode = GetLastError();
                ss_rc_error(errcode);
            }
        }
#endif /* SS_USE_WIN32TLS_API */

#if !defined(SS_THRIDBYSTK)
        if (thr_nativeidsmutex == NULL) {
            thr_nativeidsmutex = malloc(SsSemSizeLocal());
            if (thr_nativeidsmutex == NULL) {
                SsAssertionFailureText(out_of_memory_text, (char *)__FILE__, __LINE__);
            }
            SsSemCreateLocalBuf(thr_nativeidsmutex, SS_SEMNUM_SS_THRSTART);
        }

        SsSemEnter(thr_nativeidsmutex);
        thr_register_nomutex();
        SsSemExit(thr_nativeidsmutex);
#endif
}

void SsThrFunDone(void)
{
#if !defined(SS_THRIDBYSTK)
        ss_bassert(thr_nativeidsmutex != NULL);
        SsSemEnter(thr_nativeidsmutex);
        thr_unregister_nomutex();
        SsSemExit(thr_nativeidsmutex);
#endif
}
#endif /* !SS_THRFUNINIT_DEFINED */

#ifndef SS_THRGLOBALINIT_DEFINED

void SsThrGlobalInit(void)
{
        /* Dummy */
}

void SsThrGlobalDone(void)
{
        /* Dummy */
}

#endif  /* SS_THRGLOBALINIT_DEFINED */

/*##**********************************************************************\
 *
 *		SsThrFunInit_INTERNAL_ifnotcalled
 *
 * Calls SsThrFunInit_INTERNAL() if it hasn't been called before.
 *
 * Parameters :
 *
 *	stackptr - in, use
 *		pointer jmp_buf record at bottom of stack
 *
 * Return value :
 *
 * Comments :
 *      Created for SuperFast
 *
 * Globals used :
 *
 * See also :
 */
void SsThrFunInit_INTERNAL_ifnotcalled(void* stackptr)
{
/*         SsPrintf("%s\n", __FUNCTION__); */
        if (thrfuninit_internal_called == FALSE) {
            SsThrFunInit_INTERNAL(stackptr);
        }
}

/*##**********************************************************************\
 *
 *		SsThrGetNativeId
 *
 * Returns the thread id of the current thread.
 * Uses native thread identification also when SS_THRIDBYSTK is defined.
 *
 * Parameters :
 *
 * Return value :
 *
 *      Thread id.
 *
 * Limitations  :
 *
 * Globals used :
 */
unsigned SsThrGetNativeId(void)
{
#ifdef SS_MT

#if defined(SS_NT)
        return(GetCurrentThreadId());
#elif defined(SS_PTHREAD)

# ifdef SS_PTHREAD_CMA
        return(pthread_self());
# else /* SS_PTHREAD_CMA */
        return(pthread_self());
# endif /* SS_PTHREAD_CMA */

#else  /* SS_PTHREAD */
        ss_error;
#endif /* SS_NT */

#endif /* SS_MT */

#ifdef SS_WIN
        return(SshInst);
#endif

        return 1;
}

/*##**********************************************************************\
 *
 *		SsThrIsRegistered
 *
 * Returns TRUE if the calling thread is registered to ss-level
 *
 * Parameters :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsThrIsRegistered()
{
        ulong   thrid;
        bool    rtn = FALSE;
        uint     i;

#if defined(SS_THRIDBYSTK)
        if (thr_startmutex == NULL) {
#else
        if (thr_nativeidsmutex == NULL) {
#endif
            return FALSE;
        }

        thrid = SsThrGetNativeId();

#if defined(SS_THRIDBYSTK)
        thr_startmutex_enter(thr_startmutex);
#else
        SsSemEnter(thr_nativeidsmutex);
#endif

        for (i=0; i < thr_nativeids->tid_count; i++) {
            if (thrid == thr_nativeids->tid_ids[i]) {
                rtn = TRUE;
                break;
            }
        }

#if defined(SS_THRIDBYSTK)
        SsSemExit(thr_startmutex);
#else
        SsSemExit(thr_nativeidsmutex);
#endif

        return rtn;
}

/*##**********************************************************************\
 *
 *		SsThrRegister
 *
 * Registers the calling thread to ss-level
 *
 * Parameters :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsThrRegister()
{
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        {
            int stackvar;
            SsThrFunInit_INTERNAL(&stackvar);
        }
#endif /* SS_MYSQL */

#if defined(SS_THRIDBYSTK)
        thr_startmutex_enter(thr_startmutex);
#else
        SsSemEnter(thr_nativeidsmutex);
#endif

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        /* Allocate thread local memory context for this thread. */
        SsQmemLocalCtxInit();
#endif
        thr_register_nomutex();

#if defined(SS_THRIDBYSTK)
        SsSemExit(thr_startmutex);
#else
        SsSemExit(thr_nativeidsmutex);
#endif
}

/*##**********************************************************************\
 *
 *		SsThrUnregister
 *
 * Unregisters the calling thread
 *
 * Parameters :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsThrUnregister()
{
        SsThrFunDone();
        SsQmemCtxDone(NULL);

#ifdef SS_THRDATA_SEMSTACK
        {
            void* ptr;
            ptr = SsThrDataGet(SS_THRDATA_SEMSTACK);
            if (ptr != NULL) {
                free(ptr);
            }
        }
#endif /* SS_THRDATA_SEMSTACK */
#ifdef SS_THRDATA_CALLSTACK
        {
            void* ptr;
            ptr = SsThrDataGet(SS_THRDATA_CALLSTACK);
            if (ptr != NULL) {
                free(ptr);
            }
        }
#endif /* SS_THRDATA_CALLSTACK */

#ifdef SS_USE_WIN32TLS_API
        SsThrDataDone();
#endif /* SS_USE_WIN32TLS_API */
}

void SsThrDestroyStaticVariables(void)
{
  if (thr_nativeids->tid_count == 1) {
    free(thr_nativeids->tid_ids);
  }
  if (thr_nativeids != NULL) {
    free(thr_nativeids);
    if (thr_nativeids != NULL) {
      thr_nativeids = NULL;
    }
  }

}
