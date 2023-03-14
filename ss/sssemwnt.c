/*************************************************************************\
**  source       * sssemwnt.c
**  directory    * ss
**  description  * Event and mutex semaphores for Windows NT
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

/***********************************************************************\
 ***                                                                 ***
 ***                    Windows NT                                   ***
 ***                                                                 ***
\***********************************************************************/

#include "ssenv.h"

#if defined(SS_NT) && defined(SS_MT)

#define SSSEM_C /* to instantiate SS_INLINE functions when not SS_USE_INLINE */

#include "sswindow.h"

#include "ssstdio.h"
#include "ssstring.h"

#include "sswinnt.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sssem.h"
/* #include "sssemstk.h" */
#include "ssutiwnt.h"

long SsSemIndefiniteWait = SSSEM_INDEFINITE_WAIT;
long SsMesIndefiniteWait = SSSEM_INDEFINITE_WAIT;

#include "sslimits.h"
#include "ssstring.h"

#include "ssthread.h"

#undef SsSemCreateLocal
#undef SsSemCreateLocalZeroTimeout
#undef SsSemCreateLocalZeroTimeoutBuf
#undef SsSemCreateLocalBuf

#ifdef SS_DEBUG
bool SsSemAllowPrint = FALSE;
#endif /* SS_DEBUG */

#ifdef SS_SEMPROFILE
extern bool ss_semdbg_uselibsem;
#endif 

struct SsMesStruct {
        HANDLE mes_handle;
};

static void SemInitLocal(
        SsSemT* sem,
        sem_type_t type,
        ss_semnum_t semnum)
{
#if defined(SS_DEBUG) || defined(SS_PROFILE)
        if (ss_semdebug && type == SEM_LOCAL) {
            type = SEM_TIMEOUT;
        }
#endif
        sem->sem_type = type;
        if (type == SEM_TIMEOUT) {
            sem->sem_handle = CreateMutex(
                                    (LPSECURITY_ATTRIBUTES)NULL,
                                    FALSE,  /* Not owned. */
                                    NULL);
            if (sem->sem_handle == NULL) {
                ss_rc_error(GetLastError());
            }
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SemInitLocal handle = %lu, sem = 0x%08lX\n",
                        sem->sem_handle, sem));
                }
            );
        } else {
            ss_dassert(type == SEM_LOCAL);
            InitializeCriticalSectionAndSpinCount(&sem->sem_critsect, (DWORD)4000);
        }
        ss_beta(sem->sem_enterflag = 0;)
#if defined(SS_BETA) || defined(SS_PROFILE) || defined(AUTOTEST_RUN) 
        sem->sem_num = semnum;
#endif /* SS_BETA */
        ss_profile(sem->sem_dbg = NULL);
        ss_debug(sem->sem_enterfile = NULL;)
        ss_debug(sem->sem_enterline = 0;)
        ss_debug(sem->sem_enterthreadid = 0;)
        ss_profile(sem->sem_createfile = "";)
        ss_profile(sem->sem_createline = 0;)
}

SsSemT* SsSemCreateLocal(ss_semnum_t semnum)
{
        SsSemT* sem;

        sem = SsMemAlloc(sizeof(SsSemT));

        SemInitLocal(sem, SEM_LOCAL, semnum);

        return(sem);
}

#ifdef SS_SEMSTK_DBG
void SsSemSetNum(SsSemT* sem, ss_semnum_t num)
{
        sem->sem_num = num;
}
#endif /* SS_SEMSTK_DBG */

void SsSemCreateLocalZeroTimeoutBuf(SsSemT* sem, ss_semnum_t semnum)
{
        SemInitLocal(sem, SEM_LOCAL, semnum);
}


SsSemT* SsSemCreateLocalZeroTimeout(ss_semnum_t semnum)
{
        SsSemT* sem;

        sem = SsMemAlloc(sizeof(SsSemT));

        SemInitLocal(sem, SEM_LOCAL, semnum);
        
        return(sem);
}

void SsSemCreateLocalBuf(SsSemT* sembuf, ss_semnum_t semnum)
{
        SemInitLocal(sembuf, SEM_LOCAL, semnum);
}

#ifdef SS_SEM_DBG

SsSemT* SsSemCreateLocalDbg(ss_semnum_t semnum, char* file, int line)
{
        SsSemT* sem;

        sem = SsMemAlloc(sizeof(SsSemT));

        SemInitLocal(
            sem,
            SEM_LOCAL,
            semnum);
        sem->sem_dbg = SsSemDbgAdd(semnum, file, line, FALSE);
        ss_profile(sem->sem_createfile = file;)
        ss_profile(sem->sem_createline = line;)
        
        return(sem);
}


SsSemT* SsSemCreateLocalZeroTimeoutDbg(ss_semnum_t semnum, char* file, int line)
{
        SsSemT* sem;

        sem = SsMemAlloc(sizeof(SsSemT));

        SemInitLocal(
            sem,
            SEM_TIMEOUT,
            semnum);
        sem->sem_dbg = SsSemDbgAdd(semnum, file, line, FALSE);
        ss_profile(sem->sem_createfile = file;)
        ss_profile(sem->sem_createline = line;)
        
        return(sem);
}

void SsSemCreateLocalZeroTimeoutBufDbg(SsSemT* sem, ss_semnum_t semnum, char* file, int line)
{
        SemInitLocal(
            sem,
            SEM_TIMEOUT,
            semnum);
        sem->sem_dbg = SsSemDbgAdd(semnum, file, line, FALSE);
        ss_profile(sem->sem_createfile = file;)
        ss_profile(sem->sem_createline = line;)
}

void SsSemCreateLocalBufDbg(SsSemT* sembuf, ss_semnum_t semnum, char* file, int line)
{
        SemInitLocal(
            sembuf,
            SEM_LOCAL,
            semnum);
        sembuf->sem_dbg = SsSemDbgAdd(semnum, file, line, FALSE);
        ss_profile(sembuf->sem_createfile = file;)
        ss_profile(sembuf->sem_createline = line;)
}

#endif /* SS_SEM_DBG */

SsSemT* SsSemCreateGlobal(char* semname, ss_semnum_t semnum)
{
        LPSECURITY_ATTRIBUTES  lpsa;

        SsSemT* sem;
        ss_dassert(semname != NULL);
        ss_dassert(strchr(semname, '\\') == NULL);

        sem = SsMemAlloc(sizeof(SsSemT));

        sem->sem_type = SEM_TIMEOUT;
#if defined(SS_BETA) || defined(SS_PROFILE) || defined(AUTOTEST_RUN) 
        sem->sem_num = semnum;
#endif /* SS_BETA */

        lpsa = SsWntACLInit();
        sem->sem_handle = CreateMutex(
                                lpsa,
                                FALSE,  /* Not owned. */
                                semname);
        SsWntACLDone(lpsa);

        if (sem->sem_handle != NULL && GetLastError() == 0) {
            ss_beta(sem->sem_enterflag = FALSE);
            ss_debug(sem->sem_enterfile = NULL;)
            ss_debug(sem->sem_enterline = 0;)
            ss_debug(sem->sem_enterthreadid = 0;)
            ss_profile(sem->sem_dbg = SsSemDbgAdd(semnum, __FILE__, __LINE__, FALSE);)
            ss_profile(sem->sem_createfile = "";)
            ss_profile(sem->sem_createline = 0;)
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SsSemCreateGlobal handle = %lu, sem = 0x%08lX\n",
                        sem->sem_handle, sem));
                }
            );
            return(sem);
        } else {
            /* Error */
            ss_rc_dassert(GetLastError() == ERROR_ALREADY_EXISTS, GetLastError());
            SsMemFree(sem);
            return(NULL);
        }
}

SsSemT* SsSemOpenGlobal(char* semname)
{
        SsSemT* sem;

        ss_dassert(semname != NULL);
        ss_dassert(strchr(semname, '\\') == NULL);

        sem = SsMemAlloc(sizeof(SsSemT));

        sem->sem_type = SEM_TIMEOUT;
#if defined(SS_BETA) || defined(SS_PROFILE) || defined(AUTOTEST_RUN) 
        sem->sem_num = SS_SEMNUM_NOTUSED;
#endif /* SS_BETA */

        sem->sem_handle = OpenMutex(
                                MUTEX_ALL_ACCESS,
                                FALSE,
                                semname);
        if (sem->sem_handle != NULL) {
            ss_beta(sem->sem_enterflag = FALSE);
            ss_debug(sem->sem_enterthreadid = 0;)
            ss_profile(sem->sem_dbg = SsSemDbgAdd(sem->sem_num, __FILE__, __LINE__, FALSE);)
            ss_profile(sem->sem_createfile = "";)
            ss_profile(sem->sem_createline = 0;)
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SsSemCreateGlobal handle = %lu, sem = 0x%08lX\n",
                        sem->sem_handle, sem));
                }
            );
            return(sem);
        } else {
            /* Error */
            SsMemFree(sem);
            return(NULL);
        }
}

void SsSemFree(SsSemT* sem)
{
        SsSemFreeBuf(sem);
        SsMemFree(sem);
}

void SsSemFreeBuf(SsSemT* sembuf)
{
        if (sembuf->sem_type == SEM_LOCAL) {
            DeleteCriticalSection(&sembuf->sem_critsect);
        } else {
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SsSemFreeBuf handle = %lu, sem = 0x%08lX\n",
                        sembuf->sem_handle, sembuf));
                }
            );
            CloseHandle(sembuf->sem_handle);
        }
}

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)

static void check_semsleep(SsSemT* sem)
{
        if (ss_semsleep_startnum > 0) {
            if (sem->sem_num >= ss_semsleep_startnum
            && sem->sem_num <= ss_semsleep_stopnum)
            {
                long sleeptime;
                sleeptime = (rand() % (ss_semsleep_maxtime - ss_semsleep_mintime))
                            + ss_semsleep_mintime;
                if (ss_semsleep_random) {
                    if (rand() % ss_semsleep_random_freq == 0) {
                        SsThrSleep(sleeptime);
                    }
                } else {
                    ss_semsleep_loopcount++;
                    if (ss_semsleep_loopcount >= ss_semsleep_maxloopcount) {
                        ss_semsleep_loopcount = 0;
                        ss_semsleep_loopsem++;
                        if (ss_semsleep_loopsem > ss_semsleep_stopnum) {
                            ss_semsleep_loopsem = ss_semsleep_startnum;
                        }
                    }
                    if (sem->sem_num == ss_semsleep_loopsem
                    &&  rand() % ss_semsleep_random_freq == 0)
                    {
                        SsThrSleep(sleeptime);
                    }
                }
            }
        }
}
#endif /* SS_BETA */

SsSemRetT SsSemRequest(SsSemT* sem, long timeout)
{
#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
        check_semsleep(sem);
#endif

#ifdef SS_SEMSTK_DBG
        if (sem->sem_num != SS_SEMNUM_NOTUSED) {
            FAKE_CODE_BLOCK_EQ(FAKE_SS_STOPONMUTEXENTER, sem->sem_num, 
                {
                    SsBrk();
                }
            )
            if (timeout == 0) {
                SsSemStkEnterZeroTimeout(sem->sem_num);
            } else {
                SsSemStkEnter(sem->sem_num);
            }
        }
#endif /* SS_SEMSTK_DBG */
        
        if (sem->sem_type == SEM_LOCAL) {
            ss_dassert(timeout == 0 || timeout == SsSemIndefiniteWait);
            if (timeout == 0) {
                if (!TryEnterCriticalSection(&sem->sem_critsect)) {
#ifdef SS_SEMSTK_DBG
                    if (sem->sem_num != SS_SEMNUM_NOTUSED) {
                        SsSemStkExit(sem->sem_num);
                    }
#endif /* SS_SEMSTK_DBG */
                    return (SSSEM_RC_TIMEOUT);
                }
            } else {
                EnterCriticalSection(&sem->sem_critsect);
            }
            ss_rc_bassert(sem->sem_enterflag == FALSE, SEM_NUM(sem));
            ss_beta(sem->sem_enterflag = TRUE);
            ss_debug(sem->sem_enterthreadid = SsThrGetid());
            return(SSSEM_RC_SUCC);
        } else {

            DWORD to;
            DWORD rc;

            if (timeout == SSSEM_INDEFINITE_WAIT) {
                to = INFINITE;
            } else {
                to = timeout;
            }

#if defined(SS_DEBUG) || defined(SS_PROFILE)
            if (ss_semdebug) {
                if (sem->sem_dbg == NULL) {
                    sem->sem_dbg = SsSemDbgAdd(sem->sem_num, sem->sem_createfile, sem->sem_createline, FALSE);
                }
                sem->sem_dbg->sd_callcnt++;
                rc = WaitForSingleObject(sem->sem_handle, (DWORD)0);
                switch (rc) {
                    case WAIT_TIMEOUT:
                        sem->sem_dbg->sd_waitcnt++;
                        if (to == 0) {
#ifdef SS_SEMSTK_DBG
                            if (sem->sem_num != SS_SEMNUM_NOTUSED) {
                                SsSemStkExit(sem->sem_num);
                            }
#endif /* SS_SEMSTK_DBG */
                            return(SSSEM_RC_TIMEOUT);
                        }
                        break;

                    case WAIT_OBJECT_0:
                        ss_rc_bassert(sem->sem_enterflag == FALSE, SEM_NUM(sem));
                        ss_beta(sem->sem_enterflag = TRUE);
                        ss_debug(sem->sem_enterthreadid = SsThrGetid());
                        return(SSSEM_RC_SUCC);

                    default:
                        ss_debug(SsDbgPrintf("SsSemRequest: Error %ld, handle = %lu, sem = 0x%08lX\n",
                            (long)GetLastError(), sem->sem_handle, sem));
                        ss_rc_error(GetLastError());
                        return(SSSEM_RC_SUCC);
                }
            }
#endif /* defined(SS_DEBUG) || defined(SS_PROFILE) */

            for (;;) {

                rc = WaitForSingleObject(sem->sem_handle, to);

                switch (rc) {
                    case WAIT_TIMEOUT:
                        ss_rc_dassert(timeout != SsSemIndefiniteWait, SEM_NUM(sem));
#ifdef SS_SEMSTK_DBG
                        if (sem->sem_num != SS_SEMNUM_NOTUSED) {
                            SsSemStkExit(sem->sem_num);
                        }
#endif /* SS_SEMSTK_DBG */
                        return(SSSEM_RC_TIMEOUT);

                    case WAIT_OBJECT_0:
                    case WAIT_ABANDONED: /* previous owner died */
                        ss_rc_bassert(sem->sem_enterflag == FALSE, SEM_NUM(sem));
                        ss_beta(sem->sem_enterflag = TRUE;)
                        ss_debug(sem->sem_enterthreadid = SsThrGetid());
                        return(SSSEM_RC_SUCC);

                    default:
                        ss_debug(SsDbgPrintf("SsSemRequest: rc %ld, Error %ld, handle = %lu, sem = 0x%08lX\n",
                            (long)rc, (long)GetLastError(), sem->sem_handle, sem));
                        /* This is a global sem, probably other end has closed
                         * the sem because the session is disconnected etc.
                         * It is unnecessary to cause an "internal error" here, the
                         * disconnection will surely be noticed elsewhere.
                         * Ari Dec 09, 1996
                         */
                        ss_rc_derror(GetLastError());
                        return(SSSEM_RC_SUCC);
                }
            }
        }
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 * 
 *		SsSemRequestDbg
 * 
 * Enters the semaphore and stores file and line where it was entered.
 * 
 * Parameters : 
 * 
 *	sem - 
 *		
 *		
 *	timeout - 
 *		
 *		
 *	file - 
 *		
 *		
 *	line - 
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
SsSemRetT SsSemRequestDbg(SsSemT* sem, long timeout, char* file, int line)
{
        SsSemRetT rc;

        rc = SsSemRequest(sem, timeout);

        sem->sem_enterfile = file;
        sem->sem_enterline = line;

        return(rc);
}
#endif /* SS_DEBUG */

void SsSemClear(SsSemT* sem)
{
#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
        check_semsleep(sem);
#endif
#ifdef SS_SEMSTK_DBG
        if (sem->sem_num != SS_SEMNUM_NOTUSED) {
            SsSemStkExit(sem->sem_num);
        }
#endif /* SS_SEMSTK_DBG */

        if (sem->sem_type == SEM_LOCAL) {
            
            ss_rc_bassert(sem->sem_enterflag == TRUE, SEM_NUM(sem));
            ss_beta(sem->sem_enterflag = FALSE);
            ss_debug(sem->sem_enterthreadid = 0;)
            LeaveCriticalSection(&sem->sem_critsect);
        } else {

            BOOL succp;

            ss_rc_bassert(sem->sem_enterflag == TRUE, SEM_NUM(sem));
            ss_beta(sem->sem_enterflag = FALSE);
            ss_debug(sem->sem_enterthreadid = 0;)

            succp = ReleaseMutex(sem->sem_handle);
#ifdef SS_DEBUG
            if (!succp) {
                SsDbgPrintf("SsSemClear: Error %ld, handle = %lu, sem = 0x%08lX\n",
                    (long)GetLastError(), sem->sem_handle, sem);
            }
#endif
            ss_rc_bassert(ss_sem_ignoreerror || succp, (int)GetLastError());
        }
#ifdef SS_DEBUG
        if (ss_debug_mutexexitwait >= 0L) {
            Sleep(ss_debug_mutexexitwait);
        }
#endif

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
        check_semsleep(sem);
#endif
}

void* SsSemGetHandle(SsSemT* sem)
{
        if (sem->sem_type == SEM_LOCAL) {
            ss_error;
            return((void*)&sem->sem_critsect);
        } else {
            return((void*)&sem->sem_handle);
        }
}

#ifdef SS_DEBUG

bool SsSemIsEntered(SsSemT* sem)
{
        return(sem->sem_enterflag);
}

bool SsSemThreadIsEntered(SsSemT* sem)
{
        return(sem->sem_enterthreadid == SsThrGetid());
}


#endif /* SS_DEBUG */

SsMesT* SsMesCreateLocal(void)
{
        SsMesT* mes;

        mes = SsMemAlloc(sizeof(SsMesT));

        mes->mes_handle = CreateEvent(
                            (LPSECURITY_ATTRIBUTES)NULL,
                            FALSE,      /* Use auto reset. */
                            FALSE,      /* Initial state is nonsignalled. */
                            NULL);
        ss_dassert(mes->mes_handle != NULL);
        ss_debug(
            if (SsSemAllowPrint) {
                ss_dprintf_1(("SsMesCreateLocal handle = %lu, mes = 0x%08lX\n",
                    mes->mes_handle, mes));
            }
        );

        return(mes);
}

SsMesT* SsMesCreateGlobal(char* mesname)
{
        LPSECURITY_ATTRIBUTES lpsa;

        SsMesT* mes;

        ss_dassert(mesname != NULL);
        ss_dassert(strchr(mesname, '\\') == NULL);

        mes = SsMemAlloc(sizeof(SsMesT));

        lpsa = SsWntACLInit();
        mes->mes_handle = CreateEvent(
                            lpsa,
                            FALSE,      /* Use auto reset. */
                            FALSE,      /* Initial state is nonsignalled. */
                            mesname);
        SsWntACLDone(lpsa);

        if (mes->mes_handle != NULL && GetLastError() == 0) {
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SsMesCreateGlobal handle = %lu, mes = 0x%08lX\n",
                        mes->mes_handle, mes));
                }
            );
            return(mes);
        } else {
            /* Error */
            ss_dprintf_1(("DosCreateEventSem rc = %ld.\n", (long)GetLastError()));
            ss_rc_dassert(GetLastError() == ERROR_ALREADY_EXISTS, GetLastError());
            SsMemFree(mes);
            return(NULL);
        }
}

SsMesT* SsMesOpenGlobal(char* mesname)
{
        SsMesT* mes;

        ss_dassert(mesname != NULL);
        ss_dassert(strchr(mesname, '\\') == NULL);

        mes = SsMemAlloc(sizeof(SsMesT));

        mes->mes_handle = OpenEvent(
                            EVENT_ALL_ACCESS,
                            FALSE,      /* Do not allow inheritance. */
                            mesname);
        if (mes->mes_handle != NULL) {
            ss_debug(
                if (SsSemAllowPrint) {
                    ss_dprintf_1(("SsMesOpenGlobal handle = %lu, mes = 0x%08lX\n",
                        mes->mes_handle, mes));
                }
            );
            return(mes);
        } else {
            /* Error */
            SsMemFree(mes);
            return(NULL);
        }
}

void SsMesFree(SsMesT* mes)
{
        ss_debug(
            if (SsSemAllowPrint) {
                ss_dprintf_1(("SsMesFree handle = %lu, mes = 0x%08lX\n",
                    mes->mes_handle, mes));
            }
        );
        CloseHandle(mes->mes_handle);
        SsMemFree(mes);
}

SsMesRetT SsMesSend(SsMesT* mes)
{
        BOOL succp;

        /* ResetEvent(mes->mes_handle);  */

        succp = SetEvent(mes->mes_handle);

#ifdef SS_DEBUG
        {
            int rc;
            if (!succp) {
                rc = GetLastError();
                SsDbgPrintf("SsMesSend: Error %d\n", rc);
            }
            ss_rc_dassert(succp, rc);
        }
#endif
        return(SSMES_RC_SUCC);
}

void SsMesWait(SsMesT* mes)
{
        SsSemRetT rc;

        rc = SsMesRequest(mes, SsMesIndefiniteWait);
        ss_rc_bassert(rc == SSMES_RC_SUCC, rc);
}

SsMesRetT SsMesRequest(SsMesT* mes, long timeout)
{
        DWORD rc;
        DWORD to;

        if ((long)INFINITE != (long)SSSEM_INDEFINITE_WAIT) {
            if (timeout == SSSEM_INDEFINITE_WAIT) {
                to = INFINITE;
            } else {
                to = timeout;
            }
        } else {
            to = timeout;
        }

        SetLastError((DWORD)0);
        for (;;) {
            rc = WaitForSingleObject(mes->mes_handle, to);
            switch (rc) {
                case WAIT_TIMEOUT: 
                    return(SSMES_RC_TIMEOUT);

                case WAIT_OBJECT_0:
                    /* ResetEvent(mes->mes_handle);  */
                    return(SSMES_RC_SUCC);

                default:
                    ss_debug(SsDbgPrintf("SsMesRequest: Error %ld\n", (long)GetLastError()));
                    ss_rc_error(GetLastError());
                    return(SSMES_RC_SUCC);
            }
        }
}

void SsMesReset(SsMesT* mes)
{
        BOOL succp;

        succp = ResetEvent(mes->mes_handle);
        ss_bassert(succp);
}

void* SsMesGetHandle(SsMesT* mes)
{
        return((void*)mes->mes_handle);
}

#ifndef SsSemSizeLocal /* If not defined empty. */

size_t SsSemSizeLocal(void)
{
        return(sizeof(SsSemT));
}

#endif /* SsSemSizeLocal */

static SsSemT lib_sem_buf;
static SsSemT msglog_sem_buf;

SsSemT* ss_lib_sem = &lib_sem_buf;
SsSemT* ss_msglog_sem = &msglog_sem_buf;

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
        ss_debug(SsSemAllowPrint = TRUE;)
}

#endif /* defined(SS_NT) && defined(SS_MT) */
