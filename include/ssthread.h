/*************************************************************************\
**  source       * ssthread.h
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


#ifndef SSTHREAD_H
#define SSTHREAD_H

#include "ssenv.h"
#include "ssstddef.h"
#include "ssc.h"
#include "ssdebug.h"

#if defined(SS_NT) 
# ifndef _CRTAPI1
#  define _CRTAPI1 __cdecl
# endif
#endif

/* Max number of supported threads. More threads may cause overflows
 * in some thread tables.
 */
#if defined(SS_MT) && defined(SS_PTHREAD)
# define SS_THR_MAXTHREADS  128
#endif

#define SS_THREAD_USEDEFAULTSTACKSIZE  0

typedef struct SsThreadStruct SsThreadT;

typedef void (SS_CALLBACK *SsThrFunT)(void);
typedef void (SS_CALLBACK *SsThrPrmFunT)(void* prm);

void SsThrFunInit_INTERNAL(void* stackptr);
void SsThrFunInit_INTERNAL_ifnotcalled(void* stackptr);

void        SsThrFunDone(void);

void        SsThrGlobalInit(void);
void        SsThrGlobalDone(void);

SsThreadT*  SsThrInit(SsThrFunT initfunp, const char* thrname, size_t stacksize);
SsThreadT*  SsThrInitParam(SsThrPrmFunT initfunp, const char* thrname, size_t stacksize, void* prm);
void        SsThrDone(SsThreadT* thr);

void        SsThrEnable(SsThreadT* thr);
bool        SsThrEnableBool(SsThreadT* thr);
void        SsThrDisable(SsThreadT* thr);

void        SsThrExit(void);
void        SsThrExitForce(void);
void        SsThrSwitch(void);
void        SsThrSleep(ulong time);

unsigned    SsThrGetid(void);
unsigned    SsThrGetIdPar(SsThreadT* thr);
    
unsigned    SsThrGetNativeId(void);

bool SsThrIsRegistered(void);

void SsThrRegister(void);

void SsThrUnregister(void);

void SsThrForceNewThreadsToSystemScope(bool yesorno);

extern int ss_thr_numthreads;

#if defined(SS_BETA)

# define SS_MAXTHRINFO  64

/* Structure for various thread related info.
 */
typedef struct {
        char*       ti_name;
        ulong       ti_loopcount;
        unsigned    ti_id;
} SsThrInfoT;

extern SsThrInfoT ss_thr_info[SS_MAXTHRINFO];

# define SS_THRINFO_LOOP(i)  (ss_thr_info[i].ti_loopcount++)

int   SsThrInfoInit(char* name);
void  SsThrInfoDone(int id);

#else /* SS_BETA */

# define SS_THRINFO_LOOP(i)
# define SsThrInfoInit(name)    0
# define SsThrInfoDone(id)

#endif /* SS_BETA */

#if defined(SS_PTHREAD)
# define SS_THRDATA_TRAPH       0   /* Trap handler data index, sstraph.c. */
# define SS_THRDATA_ALLOCA      1   /* Alloca data index, ssalloca.h. */
# if defined(SS_PTHREAD) 
#  if defined(SS_THRIDBYSTK)
#   define SS_THRDATA_SEMSTACK  2   /* SsSem enter stack */
#   define SS_THRDATA_SQLSTR    3
#   define SS_THRDATA_SLEEPMES  4  /* Thr specific mes for SsThrSleep */
#   define SS_THRDATA_CALLSTACK 5   /* SsMemTrc callstack */
#   define SS_THRDATA_MAX       6
#  else /* SS_THRIDBYSTK */
#   define SS_THRDATA_THRID     2
#   define SS_THRDATA_QMEMCTX   3
#   define SS_THRDATA_PMEMCTX   4
#   define SS_THRDATA_PMEMDEFCTX 5
#   define SS_THRDATA_SEMSTACK  6
#   define SS_THRDATA_SQLSTR    7
#   define SS_THRDATA_CALLSTACK 8   /* SsMemTrc callstack */
#   define SS_THRDATA_SLEEPMES  9  /* Thr specific mes for SsThrSleep */
#   define SS_THRDATA_MAX       10
#  endif /* SS_THRIDBYSTK */
# else /* SS_PTHREAD */
#  define SS_THRDATA_CALLSTACK  2   /* SsMemTrc callstack */
#  define SS_THRDATA_SEMSTACK   3   /* SsSem enter stack */
#  define SS_THRDATA_SQLSTR     4
#  define SS_THRDATA_MAX        5   /* Max number of thread local data. */
# endif /* SS_PTHREAD */

void    SsThrDataSet(int index, void* data);

#if defined(SS_PTHREAD) && !defined(SS_THRIDBYSTK)
/* inline SsThrDataGet in pthread platforms */
#include <pthread.h>

SS_INLINE void* SsThrDataGet(int index);

#if defined(SSTHREAD_C) || defined(SS_USE_INLINE)

extern pthread_key_t ssthread_tls_key;

void** SsThrDataInit(void);

SS_INLINE void* SsThrDataGet(int index)
{
        void** thr_data;

        ss_rc_dassert(index >= 0, index);
        ss_rc_dassert(index < SS_THRDATA_MAX, index);

        thr_data = (void**)pthread_getspecific(ssthread_tls_key);
        if (thr_data == NULL) {
            thr_data = SsThrDataInit();
        }
        return(thr_data[index]);
}

#endif /* SSTHREAD_C || SS_USE_INLINE */
#else /* SS_PTHREAD && !SS_THRIDBYSTK */
void*   SsThrDataGet(int index);
#endif /* SS_PTHREAD && !SS_THRIDBYSTK */

#endif /* SS_PTHREAD */


#ifdef SS_USE_WIN32TLS_API
#if defined(SS_NT) 
#  define SS_THRDATA_TRAPH      0   /* Trap handler data index, sstraph.c. */
#  define SS_THRDATA_ALLOCA     1   /* Alloca data index, ssalloca.h. */
#  define SS_THRDATA_SEMSTACK   2   /* SsSem enter stack */
#  define SS_THRDATA_SQLSTR     3
#  define SS_THRDATA_CALLSTACK  4   /* SsMemTrc callstack */
#  define SS_THRDATA_SQLTRC     5
#  define SS_THRDATA_EXITCTX    6
#  define SS_THRDATA_TRAMJMPBUF 7
#  define SS_THRDATA_QMEMCTX    8
#  define SS_THRDATA_PMEMCTX    9
#  define SS_THRDATA_PMEMDEFCTX 10
#  define SS_THRDATA_MAX        11  /* Max number of thread local data. */

void    SsThrDataSet(int index, void* data);
void*   SsThrDataGet(int index);
void    SsThrDataDone();

# endif /* SS_NT */

#else
#define SsThrDataDone()

#endif /* SS_USE_WIN32TLS_API */

#endif /* SSTHREAD_H */
