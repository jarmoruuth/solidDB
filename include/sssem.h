/*************************************************************************\
**  source       * sssem.h
**  directory    * ss
**  description  * Semaphore functions
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


#ifndef SSSEM_H
#define SSSEM_H

#include "ssfile.h"
#include "ssenv.h"
#include "ssstddef.h"
#include "ssstdlib.h"
#include "ssstdio.h"
#include "ssc.h" /* for ushort */

#if defined(SS_PROFILE) && !defined(SS_SEMPROFILE)
#define SS_SEMPROFILE
#endif /* SS_PROFILE && !SS_SEMPROFILE */

#if defined(SS_DEBUG) && defined(SS_MT) 

#  if defined(SS_NT) && defined(SS_MT) && !defined(SS_NT64)
#    define SS_SEMSTK_DBG
#  endif

#  if defined(SS_LINUX) || defined(SS_FREEBSD)
#    define SS_SEMSTK_DBG
#  endif

#endif /* SS_DEBUG && SS_MT */

extern int ss_sem_spincount;

typedef struct SsSemDbgStruct SsSemDbgT;

/* Return codes of SsSem functions */
typedef enum {
        SSSEM_RC_SUCC    = 0,
        SSSEM_RC_TIMEOUT = 1
} SsSemRetT;

typedef long ss_semnum_t; /* This is long for sssempux.c */

typedef struct SsSemStruct SsMutexT, SsSemT;

#if defined(SS_DEBUG) || defined(SS_PROFILE)
# define SS_SEM_DBG
#endif

#if !defined(SS_SEM_DBG)
SS_INLINE SsMutexT* SsMutexInit(ss_semnum_t semnum);
SS_INLINE void SsMutexInitBuf(SsMutexT* mutex, ss_semnum_t semnum);
SS_INLINE SsMutexT* SsMutexInitZeroTimeout(ss_semnum_t semnum);
SS_INLINE void SsMutexInitZeroTimeoutBuf(SsMutexT* mutex, ss_semnum_t semnum);
#endif
SS_INLINE void SsMutexDone(SsMutexT* mutex);
SS_INLINE void SsMutexDoneBuf(SsMutexT* mutex);
    
SS_INLINE void SsMutexLock(SsMutexT* mutex);
SS_INLINE void SsMutexUnlock(SsMutexT* mutex);
SS_INLINE void SsZeroTimeoutMutexLock(SsMutexT* mutex);
SS_INLINE SsSemRetT SsZeroTimeoutMutexTryLock(SsMutexT* mutex);
SS_INLINE void SsZeroTimeoutMutexUnlock(SsMutexT* mutex);


#  if defined(SS_PTHREAD)

#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include "ssdebug.h"
#include "ssstring.h"
#include "ssthread.h"
#include "sstime.h"

#define SS_MUTEX_TRYLOCK_IS_CHEAP

struct SsSemStruct {
        ss_debug(int sem_check;)
        pthread_mutex_t sem_mtx;
        ss_debug(bool sem_enterflag;)
        ss_debug(unsigned sem_enterthread;)
#ifdef SS_SEMPROFILE
        SsSemDbgT* sem_dbg;
        char*      sem_createfile;
        int        sem_createline;
#endif /* SS_SEMPROFILE */
#if defined(SS_SEMSTK_DBG) || defined(SS_SEMPROFILE) || defined(AUTOTEST_RUN)
        ss_semnum_t  sem_num;
#endif /* SS_SEMSTK_DBG */
};

#if !defined(SS_SEMPROFILE) && !defined(SS_SEMSTK_DBG)

#define SS_MUTEXFUNCTIONS_DEFINED

#if defined(SS_USE_INLINE) || defined(SSSEM_C)

SS_INLINE void SsMutexLock(SsMutexT* mutex)
{
        int rc = EBUSY;
        int i;

        ss_dassert(mutex != NULL);
        ss_rc_dassert(mutex->sem_check == SS_CHK_SEM, mutex->sem_check);

        for (i = 0; i < ss_sem_spincount; i++) {
            rc = pthread_mutex_trylock(&mutex->sem_mtx);
            if (rc == 0) {
                break;
            }
            ss_rc_dassert(rc == EBUSY, rc);
        }
        if (rc != 0) {
            rc = pthread_mutex_lock(&mutex->sem_mtx);
            ss_rc_dassert(rc == 0, rc);
        }

        ss_dassert(!mutex->sem_enterflag);
        ss_dassert(mutex->sem_enterthread == 0);
        ss_debug(mutex->sem_enterflag = TRUE);
        ss_debug(mutex->sem_enterthread = SsThrGetNativeId());
}

SS_INLINE void SsMutexUnlock(SsMutexT* mutex)
{
        ss_debug(int rc;)
        ss_dassert(mutex != NULL);
        ss_rc_dassert(mutex->sem_check == SS_CHK_SEM, mutex->sem_check);
        ss_dassert(mutex->sem_enterflag);
        ss_debug(mutex->sem_enterflag = FALSE);
        ss_debug(mutex->sem_enterthread = 0);
        ss_debug(rc =)
            pthread_mutex_unlock(&mutex->sem_mtx);
        ss_rc_dassert(rc == 0, rc);
}

SS_INLINE void SsZeroTimeoutMutexLock(SsMutexT* mutex)
{
        SsMutexLock(mutex);
}

SS_INLINE void SsZeroTimeoutMutexUnlock(SsMutexT* mutex)
{
        SsMutexUnlock(mutex);
}

SS_INLINE SsSemRetT SsZeroTimeoutMutexTryLock(SsMutexT* mutex)
{
        int rc;
        
        ss_dassert(mutex != NULL);
        ss_rc_dassert(mutex->sem_check == SS_CHK_SEM, mutex->sem_check);
        rc = pthread_mutex_trylock(&mutex->sem_mtx);
        if (rc  == EBUSY) {
            return (SSSEM_RC_TIMEOUT);
        }
        ss_rc_dassert(rc == 0, rc);
        ss_dassert(!mutex->sem_enterflag);
        ss_dassert(mutex->sem_enterthread == 0);
        ss_debug(mutex->sem_enterflag = TRUE);
        ss_debug(mutex->sem_enterthread = SsThrGetNativeId());
        return (SSSEM_RC_SUCC);
}

#endif /* SS_USE_INLINE || SSSEM_C */

#ifndef SS_DEBUG

#define SsSemEnter SsMutexLock
#define SsSemExit SsMutexUnlock

#endif /* SS_DEBUG */

#endif /* !SS_SEMPROFILE */

#elif defined(SS_NT)

#include "sswindow.h"
#include "ssstdio.h"
#include "ssstring.h"

#include "sswinnt.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sssem.h"
#include "ssutiwnt.h"

#define SS_MUTEX_TRYLOCK_IS_CHEAP

#if defined(SS_BETA) || defined(SS_PROFILE) || defined(AUTOTEST_RUN) 
#define SEM_NUM(s) ((s)->sem_num)
#else /* SS_BETA */
#define SEM_NUM(s) (SS_SEMNUM_NOTUSED)
#endif /* SS_BETA */

typedef enum {
        SEM_LOCAL, /* now includes zerotimeout as well! -- pete */
        SEM_TIMEOUT
} sem_type_t;

struct SsSemStruct {
        sem_type_t            sem_type;
        ss_beta(volatile int  sem_enterflag;)
        union {
            CRITICAL_SECTION  sem_critsect;
            HANDLE            sem_handle;
        };
        ss_profile(SsSemDbgT* sem_dbg;)
        ss_debug(char*        sem_enterfile;)
        ss_debug(int          sem_enterline;)
        ss_debug(unsigned     sem_enterthreadid;)
        ss_profile(char*      sem_createfile;)
        ss_profile(int        sem_createline;)
#if defined(SS_BETA) || defined(SS_PROFILE) || defined(AUTOTEST_RUN) 
        ss_semnum_t           sem_num;
#endif /* SS_BETA */
};

#if !defined(SS_SEMPROFILE) && !defined(SS_SEMSTK_DBG)

#define SS_MUTEXFUNCTIONS_DEFINED

#if defined(SS_USE_INLINE) || defined(SSSEM_C)

SS_INLINE void SsMutexLock(SsMutexT* mutex)
{
        ss_bassert(mutex->sem_type == SEM_LOCAL);
        EnterCriticalSection(&mutex->sem_critsect);
        ss_rc_bassert(mutex->sem_enterflag == FALSE, SEM_NUM(mutex)); 
        ss_beta((mutex)->sem_enterflag = TRUE);
        ss_debug((mutex)->sem_enterthreadid = SsThrGetid());
}


SS_INLINE void SsMutexUnlock(SsMutexT* mutex)
{
        ss_bassert((mutex)->sem_type == SEM_LOCAL);
        ss_rc_bassert((mutex)->sem_enterflag == TRUE, SEM_NUM(mutex));
        ss_beta((mutex)->sem_enterflag = FALSE);
        ss_debug((mutex)->sem_enterthreadid = 0;)
        LeaveCriticalSection(&(mutex)->sem_critsect);
}

SS_INLINE void SsZeroTimeoutMutexLock(SsMutexT* mutex)
{
        SsMutexLock(mutex);
}


SS_INLINE SsSemRetT SsZeroTimeoutMutexTryLock(SsMutexT* mutex)
{
        ss_bassert(mutex->sem_type == SEM_LOCAL);
        if (!TryEnterCriticalSection(&(mutex)->sem_critsect)) {
            return (SSSEM_RC_TIMEOUT);
        }
        ss_rc_bassert(mutex->sem_enterflag == FALSE, SEM_NUM(mutex)); 
        ss_beta((mutex)->sem_enterflag = TRUE);
        ss_debug((mutex)->sem_enterthreadid = SsThrGetid());
        return (SSSEM_RC_SUCC);
}

SS_INLINE void SsZeroTimeoutMutexUnlock(SsMutexT* mutex)
{
        SsMutexUnlock(mutex);
}

#endif /* SS_USE_INLINE || SSSEMWNT_C */

#ifndef SS_DEBUG
#define SsSemEnter SsMutexLock
#define SsSemExit  SsMutexUnlock
#endif /* !SS_DEBUG */

#endif /* !SS_SEMPROFILE */

#else /* SS_PTHREAD */

#endif /* SS_PTHREAD */

#define SSSEM_INDEFINITE_WAIT (-1L)     /* Operation will not time out */
#define SSSEM_ZERO_WAIT         0L      /* Operation returns immediately */

extern long SsSemIndefiniteWait;
extern long SsMesIndefiniteWait;

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
/* Variables and default are in ssdebug.c. Values are read from solid.ini
 * in dbe_cfg_init in dbe7cfg.c.
 */
extern int  ss_semsleep_startnum;
extern int  ss_semsleep_stopnum;
extern int  ss_semsleep_mintime;
extern int  ss_semsleep_maxtime;
extern int  ss_semsleep_random_freq;
extern int  ss_semsleep_loopcount;
extern int  ss_semsleep_maxloopcount;
extern int  ss_semsleep_loopsem;
extern bool ss_semsleep_random;
#endif /* defined(SS_DEBUG) || defined(AUTOTEST_RUN) */

/* Return codes of SsSem functions */
typedef enum {
        SSMES_RC_FAILED  = -1, /* Global mes, no partner */
        SSMES_RC_SUCC    = 0,
        SSMES_RC_TIMEOUT = 1
} SsMesRetT;

typedef struct SsMesStruct SsMesT;

typedef struct {
        char* ssp_fname;
        int ssp_lineno;
} SsSemStkPosT;


typedef enum {
        SS_SEMNUM_ANONYMOUS_MES =              -2,
        SS_SEMNUM_ANONYMOUS_SEM =              -1,

        SS_SEMNUM_NOTUSED =                     0,
        
        SS_SEMNUM_ODBC3 =                       2000,
        SS_SEMNUM_ODBC3_RPC =                   2010,
        SS_SEMNUM_ODBC3_DRIVER =                2020,
        SS_SEMNUM_ODBC3_HVAL =                  2025,
        SS_SEMNUM_ODBC3_DIAGNOSTIC =            2030,

        SS_SEMNUM_CLI =                         3000,
        SS_SEMNUM_CLI_HENV =                    3010,
        SS_SEMNUM_CLI_HDBC =                    3020,
        SS_SEMNUM_CLI_ESC =                     3030,
        
        SS_SEMNUM_SSE =                         5000,
#ifdef SS_LOCALSERVER
        SS_SEMNUM_SSE_LOCSRV =                  5005,
#endif
        SS_SEMNUM_SSE_ATCMD =                   5010,   
        SS_SEMNUM_SSE_SQLSRV =                  5020,
        SS_SEMNUM_SSE_RBAKMGR =                 5021,
        SS_SEMNUM_HSB_PING =                    5022,
        SS_SEMNUM_HSB_PING_TIMEOUT =            5023,
        SS_SEMNUM_HSB_SEC =                     5025,
        SS_SEMNUM_SSE_USERLIST =                5030,
        SS_SEMNUM_SSE_CONNECT =                 5040, 
        SS_SEMNUM_SSE_BAKL =                    5050,    
        
        SS_SEMNUM_SA =                          7000,
        SS_SEMNUM_SA_SRV =                      7010,
        SS_SEMNUM_SA_USERLIST =                 7020,
        
        SS_SEMNUM_SP =                          10000,
        SS_SEMNUM_SP_CACHE =                    10010,
        SS_SEMNUM_SP_COMP =                     10020,
        SS_SEMNUM_SP_STARTSTMTS =               10030,
        SS_SEMNUM_SP_BGTASKS =                  10040,
        SS_SEMNUM_SP_SLASEM =                   10050,
        
        SS_SEMNUM_SNC =                         12000,
        SS_SEMNUM_SNC_GATE =                    12010,       
        SS_SEMNUM_SNC_SRV =                     12020,        
        SS_SEMNUM_SNC_CONNECT =                 12030,    
        SS_SEMNUM_SNC_REPLLOCK =                12050,   

        SS_SEMNUM_REX =                         12500,
        SS_SEMNUM_REX_SCONNECT =                12510,
        SS_SEMNUM_REX_CONNPOOL =                12520,
        SS_SEMNUM_REX_SCON =                    12525,
        SS_SEMNUM_REX_EXEC =                    12530,
        
        SS_SEMNUM_RC =                          13000,

        SS_SEMNUM_SRV =                         14000,
        SS_SEMNUM_SRV_USER =                    14010,

        SS_SEMNUM_TAB_SCHEMA =                  15000,
        SS_SEMNUM_TAB =                         15001,
        SS_SEMNUM_TABTRX =                      15002,
        SS_SEMNUM_SNC_HISTCLEANUP =             15500,
        
        SS_SEMNUM_SOR =                         16000,
        SS_SEMNUM_EST =                         17000,

        SS_SEMNUM_SOLUTI =                      18000,
        SS_SEMNUM_TU =                          18500,
        
        SS_SEMNUM_DBE =                         20000,
        SS_SEMNUM_DBE_DB_ACTIONGATE =           20010,
        SS_SEMNUM_DBE_DB_MERGE =                20020,
        SS_SEMNUM_DBE_PESSGATE =                20022,
        SS_SEMNUM_BLOBG2MGR =                   20025,
        SS_SEMNUM_DBE_TRX =                     20030,
        SS_SEMNUM_DBE_SEQ =                     20035,
        
        SS_SEMNUM_DBE_LOG =                     20037,

        SS_SEMNUM_HSB_PRI =                     20040,
        SS_SEMNUM_HSB_SEC_SAVEOPLIST =          20042,
        SS_SEMNUM_HSB_PRI_SWITCH =              20044,
        SS_SEMNUM_HSB_LOG =                     20046,

        SS_SEMNUM_DBE_DB_SEM =                  20060,
        SS_SEMNUM_DBE_DB_CHANGED =              20070,
        SS_SEMNUM_DBE_DB_NSEARCH =              20080,
        SS_SEMNUM_DBE_USER =                    20090,
        SS_SEMNUM_DBE_TRXSTAT =					20100,	/* protecting db_trxstat in dbe0db.c */
        
        /* pete moved RELHSAMPLE mutex after MME */
        /* apl moved it back here. */
        SS_SEMNUM_RES_RELHSAMPLE =              20101,

#ifdef SS_MME
#ifdef MMEG2_MUTEXING
        SS_SEMNUM_MME_INDEXES =                 20102,
        SS_SEMNUM_MME_INDEX =                   20103,
        SS_SEMNUM_MME_IPOSES =                  20104,
        SS_SEMNUM_MME_IPOS =                    20105,
        SS_SEMNUM_MME_STORAGE =                 20106,
#else
        SS_SEMNUM_DBE_MME =                     20107,
#endif
        SS_SEMNUM_MME_ASYNC_QUEUE =             20108,
#endif

        SS_SEMNUM_DBE_INDEX_SEARCHLIST =        20110,
        SS_SEMNUM_DBE_INDEX_SEARCHACTIVE =      20120,
        SS_SEMNUM_DBE_INDEX_MERGEGATE =         20130,
        SS_SEMNUM_DBE_INDMERGE =                20131,
        SS_SEMNUM_DBE_BTREE_STORAGE_GATE =      20140,
        SS_SEMNUM_DBE_BTREE_BONSAI_GATE =       20141,
        SS_SEMNUM_DBE_BTREE =                   20150,
        SS_SEMNUM_DBE_MMINDEX_DATATREE =        20160,
        SS_SEMNUM_DBE_MMINDEX_KEYTREE =         20170,
        
        SS_SEMNUM_XS_TFMGR =                    20175,
        SS_SEMNUM_XS_TFDIR =                    20176,
        SS_SEMNUM_XS_MEM =                      20177,
        
        SS_SEMNUM_DBE_LOCKMGR =                 20180,
        SS_SEMNUM_DBE_LOG_GATE =                20190,
        SS_SEMNUM_DBE_GTRS =                    20210,
        SS_SEMNUM_DBE_TRXB =                    20230,
        SS_SEMNUM_DBE_TRXI =                    20231,
        SS_SEMNUM_DBE_CLST =                    20240,
        SS_SEMNUM_DBE_FLST_SEQ =                20250,
        SS_SEMNUM_DBE_FLST =                    20260,
        SS_SEMNUM_DBE_IOMGR =                   20270,
        SS_SEMNUM_DBE_CACHE_HASH =              20280,
        SS_SEMNUM_DBE_CACHE_LRU =               20285,
        SS_SEMNUM_DBE_CACHE_SLOTWAIT =          20290,
        SS_SEMNUM_DBE_GOBJ =                    20300,
        SS_SEMNUM_DBE_BLOBDEBUG =               20310,
        SS_SEMNUM_DBE_BLOBREFBUF =              20320,

        SS_SEMNUM_DBE_CACHE_MESWAIT =           20325,
        SS_SEMNUM_DBE_CACHE_RETRY =             20326,

        SS_SEMNUM_MME_TRIE_CACHE =              20330,
        SS_SEMNUM_MME_TRIE_CACCHE =             20340,

        SS_SEMNUM_HSBG2 =                       20400,
        SS_SEMNUM_HSBG2_CLUSTER =               20410,
        SS_SEMNUM_HSBG2_STATEMACHINE =          20420,
        SS_SEMNUM_HSBG2_STATE =                 20430,

        SS_SEMNUM_DBE_LOGWRITEQUEUE =           20432,
        SS_SEMNUM_DBE_LOGFREEQUEUE =            20434,
        SS_SEMNUM_DBE_LOGMESLIST =              20436,

        SS_SEMNUM_HSBG2_TRANSPORT =             20440,
        SS_SEMNUM_HSBG2_SECOPSCAN =             20450,
        SS_SEMNUM_HSBG2_SAVEDQUEUES =           20460,

        SS_SEMNUM_DBE_SPM =                     20470,
        
        SS_SEMNUM_HSBG2_RPC_WRITE =             20480,
        SS_SEMNUM_HSBG2_RPC_STATE =             20490,

        SS_SEMNUM_HSBG2_CPPOS =                 20500,
        SS_SEMNUM_HSBG2_SAFEPROTOCOL =          20510,

        SS_SEMNUM_HSBG2_FLUSHER =               20515,

        SS_SEMNUM_DBE_HSBBUF =                  20520,
        SS_SEMNUM_DBE_CTR0 =                    20540,
        SS_SEMNUM_DBE_CTR1 =                    20541,
        SS_SEMNUM_DBE_CTR2 =                    20542,
        SS_SEMNUM_DBE_CTR3 =                    20543,
        SS_SEMNUM_DBE_CTR4 =                    20544,
        SS_SEMNUM_DBE_CTR5 =                    20545,
        SS_SEMNUM_DBE_CTR6 =                    20546,
        SS_SEMNUM_DBE_CTR7 =                    20547,

        SS_SEMNUM_TAB_SYSPROPERTIES =           20600,
        
        SS_SEMNUM_SSE_THR =                     20930,
        SS_SEMNUM_RPC_DNET =                    20935,
        SS_SEMNUM_SSE_RCU =                     20940,

        SS_SEMNUM_SU_EVREG =                    20945,
        SS_SEMNUM_SP_EVENT =                    20946,

        SS_SEMNUM_RES =                         21000,
        SS_SEMNUM_RS_EVNOT =                    21005,
        SS_SEMNUM_RES_ESC =                     21010,
        SS_SEMNUM_RES_RBUF =                    21020,
        SS_SEMNUM_RES_RSLINK =                  21040,
        SS_SEMNUM_RES_SYSI =                    21050,

        SS_SEMNUM_SRV_BLOBGATE =                21800,
        SS_SEMNUM_SU_BACKUPBUFPOOL =            21805,
        SS_SEMNUM_SRV_TASK =                    21810,
        SS_SEMNUM_SRV_TASKCLASS =               21820,

        SS_SEMNUM_DT =                          22000,
        SS_SEMNUM_SQL =                         23000,
        
        SS_SEMNUM_CSS =                         24000,

        SS_SEMNUM_SSA_TF =                      24500,

        SS_SEMNUM_RPC =                         25000,
        SS_SEMNUM_RPC_POOL =                    25005,
        SS_SEMNUM_RPC_ACTIONGATE =              25010,
        SS_SEMNUM_RPC_SRVMUTEX =                25520,
        SS_SEMNUM_RPC_SRVNLINK =                25530,
        SS_SEMNUM_RPC_CLIMUTEX =                25540,
        SS_SEMNUM_RPC_CLINLINK =                25550,
        SS_SEMNUM_RPC_SESARRAY =                25560,
        SS_SEMNUM_RPC_SESNLINK =                25570,
        SS_SEMNUM_RPC_BUCKET =                  25580,

        SS_SEMNUM_RES_SYSILINK =                25590,

        SS_SEMNUM_TAB_SCHEMA_RBT =              25600, /* must be after SS_SEMNUM_RES_SYSILINK */
        SS_SEMNUM_TAB_SRVTASKLIST =             25610,

        SS_SEMNUM_COM =                         26000,
        SS_SEMNUM_COM_MQUE =                    26010,
        SS_SEMNUM_COM_CTXSELSEM =               26020,
        SS_SEMNUM_COM_SESARR =                  26030,
        SS_SEMNUM_COM_READENTER =               26040,
        SS_SEMNUM_COM_READGATE =                26050,
        SS_SEMNUM_COM_SESSTATE =                26060,
        SS_SEMNUM_COM_SESNLINK =                26070,
        SS_SEMNUM_COM_PQ =                      26080,

        SS_SEMNUM_SES =                         27000,

        SS_SEMNUM_SESNTB =                      27100,
        SS_SEMNUM_SESNTB_SESMUTEX =             27110,
        SS_SEMNUM_SESNTB_WNTUSERDATA =          27120,
        SS_SEMNUM_SESNTB_O16MSNETBIOS =         27130,
        SS_SEMNUM_SESNTB_O32MSNETBIOS =         27140,
        SS_SEMNUM_SESNTB_NBPOOL =               27150,
        
        SS_SEMNUM_SESNMP =                      27200,
        SS_SEMNUM_SESNMP_SELSTATE =             27210,
        SS_SEMNUM_SESNMP_SESMUTEX =             27220,
        SS_SEMNUM_SESNMP_PIPEHLIST =            27230,

        SS_SEMNUM_SESTCP =                      27300,
        SS_SEMNUM_SESTCP_BRKSELECT =            27310,

        SS_SEMNUM_SESDEC =                      27400,
        SS_SEMNUM_SESDEC_VMSWPOOL =             27450,

        SS_SEMNUM_SESUNP =                      27500,
        SS_SEMNUM_SESUNP_BRKSELECT =            27510,

        SS_SEMNUM_SESSHM =                      27600,
        SS_SEMNUM_SESSHM_SESMUTEX =             27610,
        SS_SEMNUM_SESSHM_ACCEPT =               27620,
        SS_SEMNUM_SESSHM_BRKSELECT =            27630,
        SS_SEMNUM_SESSHM_CBUF =                 27640,

        SS_SEMNUM_SESWSA =                      27700,
        SS_SEMNUM_SESWSA_SESMUTEX =             27710,
        SS_SEMNUM_SESWSA_SOCKLIST =             27720,

        SS_SEMNUM_SESSPX =                      27800,
        SS_SEMNUM_SESSPX_SESMUTEX =             27810,
        SS_SEMNUM_SESSPX_XECBPOOL =             27820,
        SS_SEMNUM_SESSPX_XECBWAIT =             27830,
        SS_SEMNUM_SESSPX_SAPSEND  =             27890,
        SS_SEMNUM_SESSPX_SAPRECV  =             27892,

        SS_SEMNUM_SESSOCK_SESMUTEX =            27900,

        SS_SEMNUM_SESPLIS_PLISMUTEX =           27910,
        

        SS_SEMNUM_UI =                          28000,

        SS_SEMNUM_HA_COLLATION =                28010,

        SS_SEMNUM_SU =                          29000,
        SS_SEMNUM_SU_SBUF =                     29010,
        SS_SEMNUM_SU_PARAM =                    29011,
        SS_SEMNUM_SU_INIFILE =                  29020,

        SS_SEMNUM_SSE_MSGSEM =                  29021, /* lowered from 20920
                                                        * to allow use for
                                                        * inifile */
        SS_SEMNUM_UI_VIO =                      29022, /* Need to be below 
                                                        * SS_SEMNUM_SSE_MSGSEM */
        SS_SEMNUM_SU_SVFIL =                    29030,
        SS_SEMNUM_SU_FLUSHGATE =                29040,
        SS_SEMNUM_SU_VFIL =                     29050,
        SS_SEMNUM_SU_MESLIST =                  29060,
        SS_SEMNUM_SU_MSGL =                     29070,
        SS_SEMNUM_SU_USRID =                    29080,
        
        SS_SEMNUM_UTI =                         30000,
        
        SS_SEMNUM_SS =                          31000,
        SS_SEMNUM_SS_MSGCACHE =                 31003,
        SS_SEMNUM_SS_MEMCHK =                   31010,
        SS_SEMNUM_SS_MSGLOG_GLOBAL =            31020,
        SS_SEMNUM_SS_MSGLOG_OBJ =               31030,
        SS_SEMNUM_SS_PMEM_CTX =                 31040,
        SS_SEMNUM_SS_PMEM_GLOBAL =              31050,
        SS_SEMNUM_SS_FFMEM =                    31055,
        SS_SEMNUM_SS_QMEM =                     31060,
        SS_SEMNUM_SS_XMEM =                     31070,
        SS_SEMNUM_SS_ZMEM =                     31080,
        SS_SEMNUM_SS_PAGEMEM =                  31090,
        SS_SEMNUM_SS_THRSTART =                 31100,
        SS_SEMNUM_SS_THRID =                    31110,
        SS_SEMNUM_HSBG2_LINKSEM =               31115,
        SS_SEMNUM_SS_LIB =                      31120, 

        SS_SEMNUM_LAST =                        32000 

} ss_semnumenum_t;

#define SsQsemT SsSemT
#define SsQsemCreateLocal(semnum)           SsSemCreateLocal(semnum)
#define SsQsemFree(qsem)                    SsSemFree(qsem)
#define SsQsemCreateLocalBuf(qsem,semnum)   SsSemCreateLocalBuf(qsem,semnum)
#define SsQsemFreeBuf(qsem)                 SsSemFreeBuf(qsem)
#define SsQsemEnter(qsem)                   SsSemEnter(qsem)
#define SsQsemExit(qsem)                    SsSemExit(qsem)
#define SsQsemSizeLocal()                   SsSemSizeLocal()

#if defined(SS_SEMPROFILE) && defined(SS_FLATSEM)
# undef SS_FLATSEM
#endif

#if defined(SS_FLATSEM)

#define SsFlatMutexT SsMutexT
#define SsFlatMutexGetPtr(fm) (&(fm))
#define SsFlatMutexInit(p_mutex,semnum) \
        SsMutexInitBuf(p_mutex, semnum)

#define SsFlatMutexIsLockedByThread(mutex) \
    SsSemThreadIsEntered(&(mutex))

#define SsFlatMutexLock(mutex) SsMutexLock(&(mutex))
#define SsFlatMutexUnlock(mutex) SsMutexUnlock(&(mutex))
#define SsFlatMutexDone(mutex) \
    SsMutexDoneBuf(&(mutex))

#define SsFlatZeroTimeoutMutexInit(p_mutex,semnum) \
        SsMutexInitZeroTimeoutBuf(p_mutex, semnum)
#define SsFlatZeroTimeoutMutexLock(mutex) \
        SsZeroTimeoutMutexLock(&(mutex))
#define SsFlatZeroTimeoutMutexUnlock(mutex) \
        SsZeroTimeoutMutexUnlock(&(mutex))
#define SsFlatZeroTimeoutMutexTryLock(mutex) \
        SsZeroTimeoutMutexTryLock(&(mutex))

#else /* SS_FLATSEM */

#define SsFlatMutexT SsSemT*
#define SsFlatMutexGetPtr(fm) (fm)
#ifdef SS_SEMPROFILE
# define SsFlatMutexInit(p_mutex,semnum) \
    do { *(p_mutex) = SsMutexInitDbg(semnum, (char*)__FILE__, __LINE__); } while (0)
# define SsFlatZeroTimeoutMutexInit(p_mutex,semnum) \
    do { *(p_mutex) = SsMutexInitZeroTimeoutDbg(semnum, (char*)__FILE__, __LINE__); } while (0)
#else
# define SsFlatMutexInit(p_mutex,semnum) \
    do { *(p_mutex) = SsMutexInit(semnum); } while (0)
# define SsFlatZeroTimeoutMutexInit(p_mutex,semnum) \
    do { *(p_mutex) = SsMutexInitZeroTimeout(semnum); } while (0)
#endif
#define SsFlatMutexIsLockedByThread(mutex) \
    SsSemThreadIsEntered(mutex)

#define SsFlatMutexLock(mutex) SsMutexLock(mutex)
#define SsFlatMutexUnlock(mutex) SsMutexUnlock(mutex)
#define SsFlatMutexDone(mutex) \
    SsSemFree(mutex)

#define SsFlatZeroTimeoutMutexLock SsMutexLock
#define SsFlatZeroTimeoutMutexUnlock SsMutexUnlock
#define SsFlatZeroTimeoutMutexTryLock(mutex) SsSemRequest(mutex, 0)

#endif /* SS_FLATSEM */

extern SsSemT* ss_lib_sem;
extern SsSemT* ss_msglog_sem;

/* Controls only win ShMem events */
void SsSemSetScss(bool on_off);

SsSemT* SsSemCreateLocal(
        ss_semnum_t semnum);

SsSemT* SsSemCreateLocalTimeout(
        ss_semnum_t semnum);

#if defined(SS_NT) && defined(SS_MT)

SsSemT* SsSemCreateLocalZeroTimeout(
        ss_semnum_t semnum);

void SsSemCreateLocalZeroTimeoutBuf(
        SsSemT* sembuf,
        ss_semnum_t semnum);


#else /* SS_NT */

#define SsSemCreateLocalZeroTimeout SsSemCreateLocalTimeout
#define SsSemCreateLocalZeroTimeoutBuf SsSemCreateLocalBuf

#endif /* SS_NT */

void SsSemCreateLocalBuf(
        SsSemT* sembuf,
        ss_semnum_t semnum);

SsSemT* SsSemCreateGlobal(
        char* semname,
        ss_semnum_t semnum);

SsSemT* SsSemOpenGlobal(
        char* semname);

void SsSemFree(
        SsSemT* sem);

void SsSemFreeBuf(
        SsSemT* sembuf);

#ifndef SsSemRequest
SsSemRetT SsSemRequest(
        SsSemT* sem,
        long timeout);
#endif /* !SsSemRequest */

#if defined(SS_DEBUG) && defined(SS_NT)

SsSemRetT SsSemRequestDbg(
        SsSemT* sem,
        long timeout,
        char* file,
        int line);

#endif

void SsSemClear(
        SsSemT* sem);

void* SsSemGetHandle(
        SsSemT* sem);

size_t SsSemSizeLocal(void);

void SsSemInitLibSem(void);

/*##**********************************************************************\
 * 
 *		SsSemEnter
 * 
 * Enters to a mutex semaphore with indefinite wait.
 * 
 * Parameters : 
 * 
 *	sem - in, use
 *		semaphore structure
 * 
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
#ifndef SsSemEnter
#ifdef SS_DEBUG

#ifdef SS_NT
#define SsSemEnter(sem)  ss_assert(SsSemRequestDbg(sem, SsSemIndefiniteWait, (char *)__FILE__, __LINE__) == SSSEM_RC_SUCC)
#else /* SS_NT */
#define SsSemEnter(sem)  ss_assert(SsSemRequest(sem, SsSemIndefiniteWait) == SSSEM_RC_SUCC)
#endif /* SS_NT */

#else /* SS_DEBUG */
#define SsSemEnter(sem)  SsSemRequest(sem, SSSEM_INDEFINITE_WAIT)
#endif
#endif /* !SsSemEnter */
/*##**********************************************************************\
 * 
 *		SsSemExit
 * 
 * Exits from a mutex semaphore.
 * 
 * Parameters : 
 * 
 *	sem - in, use
 *		semaphore structure
 *
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
#ifndef SsSemExit
#define SsSemExit(sem)   SsSemClear(sem)
#endif /* !SsSemExit */

SsMesT* SsMesCreateLocal(void);

SsMesT* SsMesCreateGlobal(
        char* mesname);

SsMesT* SsMesOpenGlobal(
        char* mesname);

SsMesT* SsMesCreateByHandle(
        void* meshandle);

void SsMesFree(
        SsMesT* mes);

SsMesRetT SsMesSend(
        SsMesT* mes);

void SsMesWait(SsMesT* mes);

SsMesRetT SsMesRequest(
        SsMesT* mes,
        long timeout);

void  SsMesReset(
        SsMesT* mes);

void* SsMesGetHandle(
        SsMesT* mes);

/* -------------------------------------------------------------------- */
#if !defined(SS_MT) && !defined(SS_WIN) 

#if !defined(SS_SEM_ENABLESEMROUTINES)
#  if !defined(SS_DEBUG)
#    define SS_SEM_DISABLESEMROUTINES
#  endif
#endif

#ifdef SS_SEM_DISABLESEMROUTINES

#undef SsSemEnter
#undef SsSemExit
#undef SsSemCreateLocal
#undef SsSemCreateLocalTimeout
#undef SsSemCreateLocalZeroTimeout
#undef SsSemCreateLocalBuf
#undef SsSemCreateGlobal
#undef SsSemOpenGlobal
#undef SsSemFree
#undef SsSemFreeBuf
#undef SsSemRequest
#undef SsSemClear
#undef SsSemGetHandle
#undef SsSemSizeLocal
#undef SsMesCreateLocal
#undef SsMesCreateGlobal
#undef SsMesOpenGlobal
#undef SsMesCreateByHandle
#undef SsMesFree
#undef SsMesSend
#undef SsMesRequest
#undef SsMesReset
#undef SsMesGetHandle

#define SsSemEnter(s)
#define SsSemExit(s)
#define SsSemCreateLocal(n)         (void *)(0xF000F000) /* foo-foo */
#define SsSemCreateLocalTimeout(n)  (void *)(0xF000F000)
#define SsSemCreateLocalZeroTimeout(n)  (void *)(0xF000F000)
#define SsSemCreateLocalBuf(buf,n)    
#define SsSemCreateGlobal(name,n)   (void *)(0xF000F000)
#define SsSemOpenGlobal(name)       (void *)(0xF000F000)
#define SsSemFree(s)
#define SsSemFreeBuf(buf)
#define SsSemRequest(s,t)           (SSSEM_RC_SUCC)
#define SsSemClear(s)
#define SsSemGetHandle(s)           (NULL)
#define SsSemSizeLocal()            (0)

#define SsMesCreateLocal()          (NULL)
#define SsMesFree(mes)              
#define SsMesSend(mes)              (SSSEM_RC_SUCC)
#define SsMesRequest(mes,t)         (SSSEM_RC_SUCC)
#define SsMesReset(mes)             
#define SsMesGetHandle(mes)         (NULL)

#endif /* SS_SEM_DISABLESEMROUTINES */

#endif /* !defined(SS_MT) && !defined(SS_WIN) */

/* -------------------------------------------------------------------- */
#if defined(SS_WIN)

void global_SsMesFree(
        SsMesT* mes);

SsMesRetT global_SsMesSend(
        SsMesT* mes);

void global_SsMesWait(SsMesT* mes);

SsMesRetT global_SsMesRequest(
        SsMesT* mes,
        long timeout);

void  global_SsMesReset(
        SsMesT* mes);

#ifndef SS_DEBUG

#undef SsSemEnter
#undef SsSemExit
#undef SsSemCreateLocal
#undef SsSemCreateLocalTimeout
#undef SsSemCreateLocalZeroTimeout
#undef SsSemCreateLocalBuf
#undef SsSemCreateGlobal
#undef SsSemOpenGlobal
#undef SsSemFree
#undef SsSemFreeBuf
#undef SsSemRequest
#undef SsSemClear
#undef SsSemGetHandle
#undef SsSemSizeLocal
#undef SsMesCreateLocal
#undef SsMesCreateGlobal
#undef SsMesOpenGlobal
#undef SsMesCreateByHandle
#undef SsMesFree
#undef SsMesSend
#undef SsMesWait
#undef SsMesRequest
#undef SsMesReset
#undef SsMesGetHandle

#define SsSemEnter(s)
#define SsSemExit(s)
#define SsSemCreateLocal(n)         (NULL)
#define SsSemCreateLocalTimeout(n)  (NULL)
#define SsSemCreateLocalZeroTimeout(n)  (NULL)
#define SsSemCreateLocalBuf(buf,n)  (NULL)
#define SsSemCreateGlobal(name,n)   (NULL)
#define SsSemOpenGlobal(name)       (NULL)
#define SsSemFree(s)
#define SsSemFreeBuf(buf)
#define SsSemRequest(s,t)           (SSSEM_RC_SUCC)
#define SsSemClear(s)
#define SsSemGetHandle(s)           (NULL)
#define SsSemSizeLocal()            (0)

#define SsMesCreateLocal()          (NULL)
#define SsMesFree(mes)              if (mes != NULL) global_SsMesFree(mes)
#define SsMesSend(mes)              if (mes != NULL) global_SsMesSend(mes)
#define SsMesWait(mes)              if (mes != NULL) global_SsMesWait(mes)
#define SsMesRequest(mes,t)         (mes == NULL) ? (SSMES_RC_SUCC) : global_SsMesRequest(mes,t)
#define SsMesReset(mes)             if (mes != NULL) global_SsMesReset(mes)
#define SsMesGetHandle(mes)         (NULL)

#else /* !SS_DEBUG */


#endif /* !SS_DEBUG */

#endif /* WINDOWS */

#if defined(SS_DEBUG) && (defined(SS_NT) || defined(SS_PTHREAD)) && defined(SS_MT)

bool SsSemIsEntered(SsSemT* sem);
#define SsSemIsNotEntered(sem) (!SsSemIsEntered(sem))

#if defined(SS_NT) || defined(SS_PTHREAD) 
bool SsSemThreadIsEntered(SsSemT* sem);
#define SsSemThreadIsNotEntered(sem) (!SsSemThreadIsEntered(sem))
#else /* SS_NT */
#define SsSemThreadIsEntered(sem)    (TRUE)
#define SsSemThreadIsNotEntered(sem) (TRUE)
#endif /* SS_NT */

#else /* SS_DEBUG && ... */

#define SsSemIsEntered(sem)          (TRUE)
#define SsSemIsNotEntered(sem)       (TRUE)
#define SsSemThreadIsEntered(sem)    (TRUE)
#define SsSemThreadIsNotEntered(sem) (TRUE)

#endif /* SS_DEBUG && ... */

/* -------------------------------------------------------------------- */

typedef int ss_mutexcheck_t;

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		SsMutexCheckInit
 * 
 * 
 * 
 * Parameters : 
 * 
 * Return value : 
 * 
 *      Init value for variable of type ss_mutexcheck_t.
 * 
 * Comments : 
 * 
 *      This is a macro that is defined as empty in product version.
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SsMutexCheckInit() (0)

/*##**********************************************************************\
 * 
 *		SsMutexCheckEnter
 * 
 * Checks that there are no other threads inside a mutex section.
 * 
 * Parameters : 
 * 
 *      mc - use
 *          Variable of type ss_mutexcheck_t.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 *      This is a macro that is defined as empty in product version.
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SsMutexCheckEnter(mc) ss_dassert((mc)++ == 0)

/*##**********************************************************************\
 * 
 *		SsMutexCheckExit
 * 
 * Exits from a mutex check section.
 * 
 * Parameters : 
 * 
 *      mc - use
 *          Variable of type ss_mutexcheck_t.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 *      This is a macro that is defined as empty in product version.
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SsMutexCheckExit(mc) ss_dassert((mc)-- == 1)

#else /* SS_DEBUG */

#define SsMutexCheckInit()
#define SsMutexCheckEnter(mc)
#define SsMutexCheckExit(mc)

#endif /* SS_DEBUG */

#ifdef SS_SEM_DBG

struct SsSemDbgStruct {
        long        sd_callcnt;
        long        sd_waitcnt;
        char*       sd_file;
        int         sd_line;
        bool        sd_gatep;
};

SsSemDbgT* SsSemDbgAdd(
        ss_semnum_t semnum,
        char* file,
        int line,
        bool gatep);

SsSemDbgT* SsSemDbgAddVoidChecksIf(
        ss_semnum_t semnum,
        char* file,
        int line,
        bool gatep,
        bool void_checks);

void SsSemDbgPrintList(
        SS_FILE* fp,
        bool freep);

void SsSemDbgPrintFormattedList(
        SS_FILE* fp,
        bool freep);

SsSemDbgT** SsGetSemList(void);

# if defined(SS_NT) && defined(SS_MT) || defined(SS_SEMPROFILE)

#undef SsSemCreateLocalZeroTimeout
#undef SsSemCreateLocalZeroTimeoutBuf

#   define SsSemCreateLocal(n)            SsSemCreateLocalDbg(n, (char *)__FILE__, __LINE__)
#   define SsSemCreateLocalTimeout(n)     SsSemCreateLocalTimeoutDbg(n, (char *)__FILE__, __LINE__)
#   define SsSemCreateLocalZeroTimeout(n) SsSemCreateLocalZeroTimeoutDbg(n, (char *)__FILE__, __LINE__)
#   define SsSemCreateLocalZeroTimeoutBuf(b, n) SsSemCreateLocalZeroTimeoutBufDbg(b, n, (char *)__FILE__, __LINE__)
#   define SsSemCreateLocalBuf(b, n)      SsSemCreateLocalBufDbg(b, n, (char *)__FILE__, __LINE__)
#   define SsMutexInit(n)                  SsMutexInitDbg(n, (char *)__FILE__, __LINE__)
#   define SsMutexInitBuf(b, n)            SsMutexInitBufDbg(b, n, (char *)__FILE__, __LINE__)
#   define SsMutexInitZeroTimeout(b)       SsMutexInitZeroTimeoutDbg(b, (char *)__FILE__, __LINE__)
#   define SsMutexInitZeroTimeoutBuf(b, n) SsMutexInitZeroTimeoutBufDbg(b, n, (char *)__FILE__, __LINE__)

SsSemT* SsSemCreateLocalDbg(ss_semnum_t semnum, char* file, int line);
SsSemT* SsSemCreateLocalTimeoutDbg(ss_semnum_t semnum, char* file, int line);
SsSemT* SsSemCreateLocalZeroTimeoutDbg(ss_semnum_t semnum, char* file, int line);
void    SsSemCreateLocalZeroTimeoutBufDbg(SsSemT* sembuf, ss_semnum_t semnum, char* file, int line);
void    SsSemCreateLocalBufDbg(SsSemT* sembuf, ss_semnum_t semnum, char* file, int line);
SS_INLINE SsMutexT* SsMutexInitDbg(ss_semnum_t semnum, char* file, int line);
SS_INLINE void      SsMutexInitBufDbg(SsMutexT* mutex, ss_semnum_t semnum, char* file, int line);
SS_INLINE SsMutexT* SsMutexInitZeroTimeoutDbg(ss_semnum_t semnum, char* file, int line);
SS_INLINE void      SsMutexInitZeroTimeoutBufDbg(SsMutexT* mutex, ss_semnum_t semnum, char* file, int line);

# endif /* defined(SS_NT) */

#endif /* SS_SEM_DBG */

#if defined(SS_USE_INLINE) || defined(SSSEM_C)

/* These SsMutex functions are platform independent */

# if defined(SS_SEM_DBG)

SS_INLINE SsMutexT* SsMutexInitDbg(ss_semnum_t semnum, char* file, int line)
{
        SsMutexT* mutex;
        mutex = SsSemCreateLocalDbg(semnum, file, line);
        return (mutex);
}

SS_INLINE void SsMutexInitBufDbg(SsMutexT* mutex, ss_semnum_t semnum, char* file, int line)
{
        SsSemCreateLocalBufDbg(mutex, semnum, file, line);
}

SS_INLINE SsMutexT* SsMutexInitZeroTimeoutDbg(ss_semnum_t semnum, char* file, int line)
{
        SsMutexT* mutex;
        mutex = SsSemCreateLocalZeroTimeoutDbg(semnum, file, line);
        return (mutex);
}

SS_INLINE void SsMutexInitZeroTimeoutBufDbg(SsMutexT* mutex, ss_semnum_t semnum, char* file, int line)
{
        SsSemCreateLocalZeroTimeoutBufDbg(mutex, semnum, file, line);
}

# else /* SS_SEM_DBG */

SS_INLINE SsMutexT* SsMutexInit(ss_semnum_t semnum)
{
        SsMutexT* mutex;
        mutex = SsSemCreateLocal(semnum);
        return (mutex);
}

SS_INLINE void SsMutexInitBuf(SsMutexT* mutex, ss_semnum_t semnum)
{
        SsSemCreateLocalBuf(mutex,semnum);
}

SS_INLINE SsMutexT* SsMutexInitZeroTimeout(ss_semnum_t semnum)
{
        SsMutexT* mutex;
        mutex = SsSemCreateLocalZeroTimeout(semnum);
        return (mutex);
}

SS_INLINE void SsMutexInitZeroTimeoutBuf(SsMutexT* mutex, ss_semnum_t semnum)
{
        SsSemCreateLocalZeroTimeoutBuf(mutex, semnum);
}

# endif /* SS_SEM_DBG */

SS_INLINE void SsMutexDone(SsMutexT* mutex)
{
        SsSemFree(mutex);
}

SS_INLINE void SsMutexDoneBuf(SsMutexT* mutex)
{
        SsSemFreeBuf(mutex);
}
    
#endif /* SS_USE_INLINE || SSSEM_C */

#ifndef SS_MUTEXFUNCTIONS_DEFINED

#if defined(SS_USE_INLINE) || defined(SSSEM_C)

SS_INLINE void SsMutexLock(SsMutexT* mutex)
{
        SsSemRequest(mutex, SSSEM_INDEFINITE_WAIT);
}
SS_INLINE void SsMutexUnlock(SsMutexT* mutex)
{
        SsSemClear(mutex);
}
SS_INLINE void SsZeroTimeoutMutexLock(SsMutexT* mutex)
{
        SsSemRequest(mutex, SSSEM_INDEFINITE_WAIT);
}
SS_INLINE SsSemRetT SsZeroTimeoutMutexTryLock(SsMutexT* mutex)
{
        SsSemRetT rc;
        
        rc = SsSemRequest(mutex, 0);
        return (rc);
}
SS_INLINE void SsZeroTimeoutMutexUnlock(SsMutexT* mutex)
{
        SsSemClear(mutex);
}

#endif /* SS_USE_INLINE || SSSEM_C */
#endif /* SS_MUTEXFUNCTIONS_DEFINED */

#if defined(SS_SEMSTK_DBG)

#if defined(SS_UNIX)
void SsMesWaitDbg(SsMesT* mes, char* file, int line);
#define SsMesWait(mes) SsMesWaitDbg(mes, (char *)__FILE__,__LINE__)
#endif

void SsSemStkEnterCheck(ss_semnum_t num);
void SsSemStkEnterZeroTimeout(ss_semnum_t num);
void SsSemStkEnter(ss_semnum_t num);
void SsSemStkExit(ss_semnum_t num);
bool SsSemStkFind(ss_semnum_t num);
bool SsSemStkNotFound(ss_semnum_t num);
void SsSemStkPrint(void);
void SsSemSetNum(SsSemT* sem, ss_semnum_t num);
int  SsSemStkNCalls(void);

#else /* SS_SEMSTK_DBG */

# define SsSemStkEnterCheck(num)
# define SsSemStkEnterZeroTimeout(num)
# define SsSemStkEnter(num)
# define SsSemStkExit(num)
# define SsSemStkFind(num)  TRUE
# define SsSemStkNotFound(num) TRUE
# define SsSemStkPrint()
# define SsSemSetNum(sem, num)
# define SsSemStkNCalls()    (0)

#endif /* SS_SEMSTK_DBG */

#endif /* SSSEM_H */
