/*************************************************************************\
**  source       * sssemunx.c
**  directory    * ss
**  description  * Event and mutex semaphores for UNIX with no thread support
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

#include "ssstdio.h"
#include "ssstring.h"

/***********************************************************************\
 ***                                                                 ***
 ***                    UNIX with no thread support                  ***
 ***                                                                 ***
\***********************************************************************/

#if defined(SS_UNIX) && !defined(SS_MT)

#ifndef SS_SEM_NOGLOBALSEM
#  if defined(SS_NEW_ST) && defined(SS_LINUX)
#    include <linux/sem.h>  /* for 'union semun' */
#  else /* -> not (SS_NEW_ST and SS_LINUX) */
#    include <sys/types.h>
#    include <sys/ipc.h>
#    include <sys/sem.h>
#  endif /* SS_NEW_ST */
#endif /* SS_SEM_NOGLOBALSEM */

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sssem.h"

long SsSemIndefiniteWait = SSSEM_INDEFINITE_WAIT;
long SsMesIndefiniteWait = SSSEM_INDEFINITE_WAIT;

#define SEM_PERM    0600
#define SEM_SZ      1

struct SsSemStruct {
        bool    sem_local;
        int     sem_id;
};

#ifdef SS_DEBUG

SsSemT* SsSemCreateLocal(ss_semnum_t semnum)
{
        SsSemT* sem;

        sem = SsMemAlloc(sizeof(SsSemT));

        sem->sem_local = TRUE;

        return(sem);
}

SsSemT* SsSemCreateLocalTimeout(ss_semnum_t semnum)
{
        return(SsSemCreateLocal(semnum));
}

void SsSemCreateLocalBuf(SsSemT* sembuf, ss_semnum_t semnum)
{
        ss_dassert(sembuf != NULL);

        sembuf->sem_local = TRUE;
}

SsSemT* SsSemCreateGlobal(char* semname, ss_semnum_t semnum)
{
#ifdef SS_SEM_NOGLOBALSEM
        ss_error;
        return(NULL);
#else /* SS_SEM_NOGLOBALSEM */

        int         semflg;
        key_t       sem_key;
        SsSemT*     sem;
/* This could be a better way */
#if defined(SS_LINUXLIBC6) || (defined(SS_UNIX) && !defined(SS_LINUX)) 
        int arg;
#else
        union semun arg;
#endif

        sem = SsMemAlloc(sizeof(SsSemT));

        sem->sem_local = FALSE;

        sem_key = ftok(semname, 'a');  /* generate key */

        semflg = SEM_PERM|IPC_CREAT;

        /* get semaphore array */
        if ((sem->sem_id = semget(sem_key, SEM_SZ, semflg)) == -1) {
            ss_error;
            SsMemFree(sem);
            return(NULL);
        }

        /* initialize semaphore */
/* This could be a better way */
#if defined(SS_UNIX) && (!defined(SS_LINUX) || defined(SS_LINUXLIBC6))
        arg = 1;
#else
        arg.val = 1;
#endif
        if (semctl(sem->sem_id, 0, SETVAL, arg) == -1) {
            ss_error;
            SsMemFree(sem);
            return(NULL);
        }

        return(sem);
#endif /* SS_SEM_NOGLOBALSEM */
}

void SsSemFree(SsSemT* sem)
{
        int rc;

#ifndef SS_SEM_NOGLOBALSEM
        if (!sem->sem_local) {
            /* remove semaphore array */
# if defined(SS_SCO)
            rc = semctl(sem->sem_id, IPC_RMID, 0);
# elif (defined(SS_LINUX) && !defined(SS_LINUXLIBC6)) || defined(SS_FREEBSD) || defined(SS_BSI)
            union semun arg;
            rc = semctl(sem->sem_id, IPC_RMID, 0, arg);
# else
            rc = semctl(sem->sem_id, IPC_RMID, NULL);
# endif
            ss_dassert(rc != -1);
        }
#endif /* SS_SEM_NOGLOBALSEM */

        SsMemFree(sem);
}

void SsSemFreeBuf(SsSemT* sembuf)
{
        ss_dassert(sembuf->sem_local);
}

void SsSemSet(SsSemT* sem)
{
        int rc;

#ifndef SS_SEM_NOGLOBALSEM
        if (!sem->sem_local) {
            struct sembuf sem_buf;

            /* lock semaphore */

            sem_buf.sem_num = 0;         /* semaphore number */
            sem_buf.sem_op = -1;         /* decrement semaphore */
            sem_buf.sem_flg = SEM_UNDO;  /* operation flags */

            rc = semop(sem->sem_id, &sem_buf, 1);
            ss_dassert(rc != -1);
        }
#endif /* SS_SEM_NOGLOBALSEM */
}

SsSemRetT SsSemRequest(SsSemT* sem, long timeout)
{
        ss_dassert(sem->sem_local);
        return(SSSEM_RC_SUCC);
}

void SsSemClear(SsSemT* sem)
{
        int rc;

#ifndef SS_SEM_NOGLOBALSEM
        if (!sem->sem_local) {
            struct sembuf sem_buf;

            /* unlock semaphore */

            sem_buf.sem_num = 0;         /* semaphore number */
            sem_buf.sem_op = 1;          /* increment semaphore */
            sem_buf.sem_flg = SEM_UNDO;  /* operation flags */

            rc = semop(sem->sem_id, &sem_buf, 1);
            ss_dassert(rc != -1);
        }
#endif /* SS_SEM_NOGLOBALSEM */
}

SsMesT* SsMesCreateLocal(void)
{
        return(NULL);
}

SsMesT* SsMesCreateGlobal(char* mesname)
{
        SS_NOTUSED(mesname);
        ss_error;
        return(NULL);
}

void SsMesFree(SsMesT* mes)
{
        SS_NOTUSED(mes);
}

SsMesRetT SsMesSend(SsMesT* mes)
{
        SS_NOTUSED(mes);
        return(SSMES_RC_SUCC);
}

SsMesRetT SsMesRequest(SsMesT* mes, long timeout)
{
        SS_NOTUSED(mes);
        SS_NOTUSED(timeout);
        return(SSMES_RC_SUCC);
}

#ifndef SsMesReset
void SsMesReset(SsMesT* mes)
{
        SS_NOTUSED(mes);
}
#endif /* SsMesReset */

#endif /* SS_DEBUG */

void SsMesWait(SsMesT* mes)
{
        SS_NOTUSED(mes);
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
}

#endif /* UNIX */
