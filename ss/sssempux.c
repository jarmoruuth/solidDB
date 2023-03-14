/*************************************************************************\
**  source       * sssempux.c
**  directory    * ss
**  description  * Semaphores for POSIX.4a compliant UNIX systems
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

Uses the pthread_mutex_XX / pthread_cond_XX API calls.
Only local semaphores are currently implemented.
Another limitation is in mutex timeout parameter:
only values SSSEM_INDEFINITE_WAIT and 0 are supported;
other values cause an assertion failure in the debug version

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

#ifdef SS_PTHREAD

#define SSSEM_C /* to instantiate SS_INLINE functions when not SS_USE_INLINE */

#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include "ssc.h"
#include "sscheck.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "ssstring.h"
#include "ssthread.h"
#include "sstime.h"
#include "sssem.h"


long SsSemIndefiniteWait = SSSEM_INDEFINITE_WAIT;
long SsMesIndefiniteWait = SSSEM_INDEFINITE_WAIT;

#undef SsSemCreateLocal
#undef SsSemCreateLocalTimeout
#undef SsSemCreateLocalZeroTimeout
#undef SsSemCreateLocalZeroTimeoutBuf
#undef SsSemCreateLocalBuf

#ifdef SS_SEMPROFILE
extern bool ss_semdbg_uselibsem;
#endif 

#ifdef SS_PTHREAD_CMA
#  define SS_MUTEXATTR  pthread_mutexattr_default
#  define SS_CONDATTR   pthread_condattr_default
#else 
#  define SS_MUTEXATTR  NULL
#  define SS_CONDATTR   NULL
#endif

SsSemT* SsSemCreateLocal(ss_semnum_t semnum)
{
        SsSemT* sem;

        sem = SSMEM_NEW(SsSemT);
        SsSemCreateLocalBuf(sem, semnum);
        return (sem);
}

SsSemT* SsSemCreateLocalTimeout(ss_semnum_t semnum)
{
        SsSemT* sem;

        sem = SsSemCreateLocal(semnum);
        return (sem);
}

void SsSemCreateLocalBuf(SsSemT* sembuf, ss_semnum_t semnum __attribute__ ((unused)))
{
        int rc;

        ss_debug(sembuf->sem_check = SS_CHK_SEM);
        ss_debug(sembuf->sem_enterflag = FALSE);
        ss_debug(sembuf->sem_enterthread = 0);
#ifdef SS_SEMSTK_DBG
        sembuf->sem_num = semnum;
#endif /* SS_SEMSTK_DBG */
#ifdef SS_SEMPROFILE
        sembuf->sem_dbg = NULL;
        sembuf->sem_createfile = (char *)"";
        sembuf->sem_createline = 0;
#endif /* SS_SEMPROFILE */

        rc = pthread_mutex_init(&sembuf->sem_mtx, SS_MUTEXATTR);
        ss_debug(
            if (rc != 0) {
                SsDbgPrintf("rc = %d\n", rc);
                ss_rc_error(rc);
            }
        );
}

#ifdef SS_SEM_DBG
void SsSemCreateLocalBufDbg(SsSemT* sembuf, ss_semnum_t semnum,
                            char* file, int line)
{
        SsSemCreateLocalBuf(sembuf, semnum);
        sembuf->sem_dbg =  SsSemDbgAdd(semnum, file, line, FALSE);
#ifdef SS_SEMPROFILE
        sembuf->sem_createfile = file;
        sembuf->sem_createline = line;
#endif /* SS_SEMPROFILE */
}
SsSemT* SsSemCreateLocalDbg(ss_semnum_t semnum, char* file, int line)
{
        SsSemT* sem = SSMEM_NEW(SsSemT);
        SsSemCreateLocalBufDbg(sem, semnum, file, line);
        return (sem);
}

SsSemT* SsSemCreateLocalTimeoutDbg(ss_semnum_t semnum, char* file, int line)
{
        SsSemT* sem = SsSemCreateLocalDbg(semnum, file, line);
        return (sem);
}

SsSemT* SsSemCreateLocalZeroTimeoutDbg(
        ss_semnum_t semnum,
        char* file,
        int line)
{
        SsSemT* sem = SsSemCreateLocalDbg(semnum, file, line);
        return (sem);
}
        
void SsSemCreateLocalZeroTimeoutBufDbg(SsSemT* sem, ss_semnum_t semnum, char* file, int line)
{
        SsSemCreateLocalBufDbg(sem, semnum, file, line);
        sem->sem_dbg = SsSemDbgAdd(semnum, file, line, FALSE);
        ss_profile(sem->sem_createfile = file;)
        ss_profile(sem->sem_createline = line;)
}

#endif /* SS_SEMSTK_DBG */

SsSemT* SsSemCreateGlobal(
        char* semname __attribute__ ((unused)),
        ss_semnum_t semnum __attribute__ ((unused)))
{
        ss_error;
        return (NULL);
}

#ifdef SS_SEMSTK_DBG
void SsSemSetNum(SsSemT* sem, ss_semnum_t num)
{
        sem->sem_num = num;
}
#endif /* SS_SEMSTK_DBG */

void SsSemFree(SsSemT* sem)
{
        ss_dassert(sem != NULL);
        SsSemFreeBuf(sem);
        SsMemFree(sem);
}

void SsSemFreeBuf(SsSemT* sembuf)
{
        int rc;

        ss_rc_dassert(sembuf->sem_check == SS_CHK_SEM, sembuf->sem_check);
        ss_debug(sembuf->sem_check = 0);

        rc = pthread_mutex_destroy(&sembuf->sem_mtx);
        ss_rc_dassert(rc == 0, rc);
}

void SsSemSetDbgName(
        SsSemT* sem __attribute__ ((unused)),
        char* name __attribute__ ((unused)))
{
}

void SsSemSet(SsSemT* sem __attribute__ ((unused)))
{
        ss_error;
}

void SsSemClear(SsSemT* sem)
{
        int rc;

        ss_dassert(sem != NULL);
        ss_rc_dassert(sem->sem_check == SS_CHK_SEM, sem->sem_check);
        ss_dassert(sem->sem_enterflag);
        ss_debug(sem->sem_enterflag = FALSE);
        ss_debug(sem->sem_enterthread = 0);
#ifdef SS_SEMSTK_DBG
        if (sem->sem_num != SS_SEMNUM_NOTUSED) {
            SsSemStkExit(sem->sem_num);
        }
#endif /* SS_SEMSTK_DBG */
        rc = pthread_mutex_unlock(&sem->sem_mtx);
        ss_rc_dassert(rc == 0, rc);
#ifdef SS_DEBUG
        if (ss_debug_mutexexitwait >= 0L) {
            SsThrSleep(ss_debug_mutexexitwait);
        }
#endif

}

#ifndef SsSemRequest
SsSemRetT SsSemRequest(SsSemT* sem, long timeout)
{
        int rc;

#ifdef SS_SEMSTK_DBG
        if (sem->sem_num != SS_SEMNUM_NOTUSED) {
            if (timeout == 0) {
                SsSemStkEnterZeroTimeout(sem->sem_num);
            } else {
                SsSemStkEnter(sem->sem_num);
            }
        }
#endif /* SS_SEMSTK_DBG */

        ss_dassert(sem != NULL);
        ss_rc_dassert(sem->sem_check == SS_CHK_SEM, sem->sem_check);
        ss_dassert(timeout == 0 || timeout == SSSEM_INDEFINITE_WAIT);
#ifdef SS_SEMPROFILE
        if (ss_semdebug) {
            if (sem->sem_dbg == NULL) {
                sem->sem_dbg = SsSemDbgAdd(sem->sem_num,
                                           sem->sem_createfile,
                                           sem->sem_createline,
                                           FALSE);
            }
            sem->sem_dbg->sd_callcnt++;
            rc = pthread_mutex_trylock(&sem->sem_mtx);
            if (rc == EBUSY) {
                sem->sem_dbg->sd_waitcnt++;
                if (timeout == 0) {
#ifdef SS_SEMSTK_DBG
                    if (sem->sem_num != SS_SEMNUM_NOTUSED) {
                        SsSemStkExit(sem->sem_num);
                    }
#endif /* SS_SEMSTK_DBG */
                    return (SSSEM_RC_TIMEOUT);
                }
            } else {
                goto successfully_locked;
            }
        }
#endif /* SS_SEMPROFILE */
        if (timeout == 0) {
            rc = pthread_mutex_trylock(&sem->sem_mtx);
#if defined(SS_PTHREAD_CMA)
            ss_dprintf_1(("trylock rc=%d, errno=%d.\n", rc, errno));
            if (rc == 0) {
#else
            if (rc == EBUSY) {
#endif
#ifdef SS_SEMSTK_DBG
                if (sem->sem_num != SS_SEMNUM_NOTUSED) {
                    SsSemStkExit(sem->sem_num);
                }
#endif /* SS_SEMSTK_DBG */
                return (SSSEM_RC_TIMEOUT);
            }
        } else {
            rc = pthread_mutex_lock(&sem->sem_mtx);
#if defined(SS_PTHREAD_CMA) && defined(SS_DEBUG)
            ss_dprintf_1(("lock rc=%d, errno=%d.\n", rc, errno));
#endif
        }
    successfully_locked:;
#if defined(SS_PTHREAD_CMA) && defined (SS_DEBUG)
        if (timeout == 0) {
            /* ensure trylock returned 1 */
            if (rc != 1) SsPrintf("to=%ld, rc=%d, errno=%d.\n", timeout, rc, errno);
            ss_rc_dassert(rc == 1, rc);
        } else {
            /* ensure lock returned 0 */
            if (rc != 0) SsPrintf("to=%ld, rc=%d, errno=%d.\n", timeout, rc, errno);
            ss_rc_dassert(rc == 0, rc);
        }
#else
        ss_rc_dassert(rc == 0, rc);
#endif
        ss_dassert(!sem->sem_enterflag);
        ss_dassert(sem->sem_enterthread == 0);
        ss_debug(sem->sem_enterflag = TRUE);
        ss_debug(sem->sem_enterthread = SsThrGetNativeId());
        return (SSSEM_RC_SUCC);
}
#endif /* !SsSemRequest */

#ifdef SS_DEBUG

bool SsSemIsEntered(SsSemT* sem)
{
        return (sem->sem_enterthread == SsThrGetNativeId());
}

bool SsSemThreadIsEntered(SsSemT* sem)
{
        return (sem->sem_enterthread == SsThrGetNativeId());
}

#endif /* SS_DEBUG */
        
struct SsMesStruct {
        bool            mes_posted;
        pthread_mutex_t mes_mtx;
        pthread_cond_t  mes_cv;
#if defined(SS_LINUX) && defined(SS_OLD_LINUXTHREADS)
        /* this padding is needed to circumvent a NPTL bug when running
           old linuxthreads builds with new thread library
           sizeof(pthread_cond_t) was 12, in newer thread
           libraries it is 48. This especially showed while
           running ODBC driver built in Redhat 7.3 in
           Fedora Core 3
        */
        char mes_dummy_pad[48 - sizeof(pthread_cond_t)];
#endif /* SS_LINUX && SS_OLD_LINUXTHREADS */
};

SsMesT* SsMesCreateLocal(void)
{
        int rc;
        SsMesT* mes;

#ifdef SS_LINUX
        /* these compile-time assertions are here to check that
           the build environment matches the #(un)define of
           SS_OLD_LINUXTHREADS
        */
#  ifdef SS_OLD_LINUXTHREADS
        ss_ct_assert(sizeof(pthread_cond_t) == 12);
#  else /* SS_OLD_LINUXTHREADS */
        ss_ct_assert(sizeof(pthread_cond_t) >= 48);
#  endif /* SS_OLD_LINUXTHREADS */     
#endif /* SS_LINUX */
        mes = SSMEM_NEW(SsMesT);
        mes->mes_posted = FALSE;
        rc = pthread_mutex_init(&(mes->mes_mtx), SS_MUTEXATTR);
        ss_rc_dassert(rc == 0, rc);
        rc = pthread_cond_init(&(mes->mes_cv), SS_CONDATTR);
        ss_rc_dassert(rc == 0, rc);
        return (mes);
}

SsMesT* SsMesCreateGlobal(char* mesname __attribute__ ((unused)))
{
        ss_error;
        return (NULL);
}

void SsMesFree(SsMesT* mes)
{
        int rc;

        ss_dassert(mes != NULL);
        rc = pthread_cond_destroy(&mes->mes_cv);
        ss_rc_dassert(rc == 0, rc);
        rc = pthread_mutex_destroy(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        SsMemFree(mes);
}

SsMesRetT SsMesSend(SsMesT* mes)
{
        int rc;

        ss_dassert(mes != NULL);
        rc = pthread_mutex_lock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        mes->mes_posted = TRUE;
        rc = pthread_cond_signal(&mes->mes_cv);
        ss_rc_dassert(rc == 0, rc);
        rc = pthread_mutex_unlock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        return (SSMES_RC_SUCC);
}

#ifdef SS_SEMSTK_DBG
void SsMesWaitDbg(SsMesT* mes, char* file, int line)
#else /* SS_SEMSTK_DBG */
void SsMesWait(SsMesT* mes)
#endif /* SS_SEMSTK_DBG */
{
        int rc;


#ifdef SS_SEMSTK_DBG
        SsSemStkPosT semstkpos;
        long semstkvalue;

        if (file != NULL) {
            semstkpos.ssp_fname = file;
            semstkpos.ssp_lineno = line;
            semstkvalue = (long) &semstkpos;
            ss_dassert(semstkvalue < SS_SEMNUM_ANONYMOUS_MES
                       || semstkvalue > SS_SEMNUM_LAST);
        } else {
            semstkvalue = SS_SEMNUM_ANONYMOUS_MES;
        }
        SsSemStkEnter(semstkvalue);
#endif /* SS_SEMSTK_DBG */
        ss_dassert(mes != NULL);
        rc = pthread_mutex_lock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        while (!mes->mes_posted) {
            rc = pthread_cond_wait(&mes->mes_cv, &mes->mes_mtx);
            ss_rc_dassert(rc == 0, rc);
        }
        mes->mes_posted = FALSE;
        rc = pthread_mutex_unlock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
#ifdef SS_SEMSTK_DBG
        SsSemStkExit(semstkvalue);
#endif /* SS_SEMSTK_DBG */
}

SsMesRetT SsMesRequest(SsMesT* mes, long timeout)
{
        int rc;
        SsMesRetT mesret;

        if (timeout == SSSEM_INDEFINITE_WAIT) {
            SsMesWait(mes);
            return (SSMES_RC_SUCC);
        }
        mesret = SSMES_RC_SUCC;
#ifdef SS_SEMSTK_DBG
        SsSemStkEnter(SS_SEMNUM_ANONYMOUS_MES);
#endif /* SS_SEMSTK_DBG */
        rc = pthread_mutex_lock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        if (!mes->mes_posted) {
            ulong time_s = (ulong)timeout / 1000UL;
            ulong time_ms = (ulong)timeout % 1000UL;
            struct timespec ts;
            ss_debug(long retry_ctr = 0;)
            
#ifdef SS_GETTIMEOFDAY_AVAILABLE
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ts.tv_nsec = (tv.tv_usec * 1000) + (time_ms * 1000000UL);
            ts.tv_sec = tv.tv_sec + time_s + (ts.tv_nsec / 1000000000UL);
            ss_dassert((ulong)ts.tv_nsec < 2000000000UL);
            ts.tv_nsec %= 1000000000UL;
            {
#else /* SS_GETTIMEOFDAY_AVAILABLE */
            if (timeout == 0L || (ulong)timeout < 1000UL) {
                mesret = SSMES_RC_TIMEOUT;
            } else {
                ts.tv_sec = (ulong)SsTime(NULL) + time_s;
                ts.tv_nsec = 0L;
#endif /* SS_GETTIMEOFDAY_AVAILABLE */
                do {
                    rc = pthread_cond_timedwait(
                            &mes->mes_cv,
                            &mes->mes_mtx,
                            &ts);
#if defined(SS_PTHREAD_CMA)
                    if (rc == -1) ss_dprintf_1(("rc= %d, errno=%d.\n", rc, errno)); 
                    if (rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
#else
                    if (rc == ETIMEDOUT) {
#endif
                        mesret = SSMES_RC_TIMEOUT;
                        break;
                    }
                    ss_debug(retry_ctr++);
                } while (!mes->mes_posted);
            }
        }
        if (mesret == SSMES_RC_SUCC) {
            mes->mes_posted = FALSE;
        }
        rc = pthread_mutex_unlock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
#ifdef SS_SEMSTK_DBG
        SsSemStkExit(SS_SEMNUM_ANONYMOUS_MES);
#endif /* SS_SEMSTK_DBG */
        return (mesret);
}

void SsMesReset(SsMesT* mes)
{
        int rc;
        rc = pthread_mutex_lock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
        mes->mes_posted = FALSE;
        rc = pthread_mutex_unlock(&mes->mes_mtx);
        ss_rc_dassert(rc == 0, rc);
}
#ifndef SsSemSizeLocal /* If not defined empty. */

/*##**********************************************************************\
 * 
 *		SsSemSizeLocal
 * 
 * Returns size of the local semaphore structure.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsSemSizeLocal(void)
{
        return(sizeof(SsSemT));
}

#endif /* SsSemSizeLocal */

static SsSemT lib_sem_buf;
static SsSemT msglog_sem_buf;

SsSemT* ss_lib_sem = &lib_sem_buf;
SsSemT* ss_msglog_sem = &msglog_sem_buf;

/*##**********************************************************************\
 * 
 *		SsSemInitLibSem
 * 
 * Initializes a library semaphore that is used to protect non-reentrant
 * library functions.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsSemInitLibSem(void)
{
        SsSemCreateLocalBuf(ss_lib_sem, SS_SEMNUM_SS_LIB);
#ifdef SS_SEMPROFILE
        ss_lib_sem->sem_createfile = (char *)__FILE__;
        ss_lib_sem->sem_createline = __LINE__;
        ss_lib_sem->sem_dbg =
            SsSemDbgAddVoidChecksIf(SS_SEMNUM_SS_LIB,
                                    ss_lib_sem->sem_createfile,
                                    ss_lib_sem->sem_createline,
                                    FALSE,
                                    TRUE); /* avoid endless recursion */
        ss_semdbg_uselibsem = TRUE;
#endif /* SS_SEMPROFILE */
}

#endif /* SS_PTHREAD */
