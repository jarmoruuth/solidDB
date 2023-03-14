/*************************************************************************\
**  source       * sstraph.h
**  directory    * ss
**  description  * Portable trap handler system
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


#ifndef SSTRAPH_H
#define SSTRAPH_H

#include "ssc.h"
#include "ssenv.h"
#include "sssetjmp.h"
#include "sstimer.h"

#ifndef SS_UNIX
#define SS_SIGLONGJMP_NOT_AVAILABLE
#endif /* SS_UNIX */

#ifdef SS_SIGLONGJMP_NOT_AVAILABLE

#define SS_TRAP_LONGJMP longjmp
#define SS_TRAP_SETJMP(buf)  setjmp(buf)
#define SS_TRAP_JMP_BUF jmp_buf

#else /* SS_SIGLONGJMP_NOT_AVAILABLE */

#define SS_TRAP_LONGJMP siglongjmp
#define SS_TRAP_SETJMP(buf)  sigsetjmp(buf, TRUE)
#define SS_TRAP_JMP_BUF sigjmp_buf

#endif /* SS_SIGLONGJMP_NOT_AVAILABLE */

typedef struct trap_jmp_buf_st trap_jmp_buf_t;

struct trap_jmp_buf_st {
          SS_TRAP_JMP_BUF tjb_jbuf;
          trap_jmp_buf_t* tjb_next;
};

typedef enum {
        SS_TRAP_NONE = 0,
        SS_TRAP_ABRT,
        SS_TRAP_ILL,
        SS_TRAP_INT,
        SS_TRAP_BREAK,
        SS_TRAP_SEGV,                   /* 5 */
        SS_TRAP_BUS,
        SS_TRAP_TERM,
        SS_TRAP_USR1,
        SS_TRAP_USR2,
        SS_TRAP_USR3,                   /* 10 */
        SS_TRAP_FPE,
        SS_TRAP_FPE_INTOVFLOW,
        SS_TRAP_FPE_INTDIV0,
        SS_TRAP_FPE_INVALID,
        SS_TRAP_FPE_ZERODIVIDE,         /* 15 */
        SS_TRAP_FPE_OVERFLOW,
        SS_TRAP_FPE_UNDERFLOW,
        SS_TRAP_FPE_INEXACT,
        SS_TRAP_FPE_STACKFAULT,
        SS_TRAP_FPE_STACKOVERFLOW,      /* 20 */
        SS_TRAP_FPE_STACKUNDERFLOW,
        SS_TRAP_FPE_BOUND,
        SS_TRAP_FPE_DENORMAL,
        SS_TRAP_FPE_EXPLICITGEN,
        SS_TRAP_FPE_END,                /* 25 */
        SS_TRAP_PIPE, /* broken pipe */
        SS_TRAP_HUP, 
        SS_TRAP_QUIT, 
        SS_TRAP_TRAP,
        SS_TRAP_IOT,                    /* 30 */
        SS_TRAP_EMT,
        SS_TRAP_SYS,
        SS_TRAP_ALRM,
        SS_TRAP_CLD,
        SS_TRAP_CHLD,                   /* 35 */
        SS_TRAP_PWR,
        SS_TRAP_WINCH,
        SS_TRAP_URG,
        SS_TRAP_POLL,
        SS_TRAP_IO,                     /* 40 */
        SS_TRAP_STOP,
        SS_TRAP_TSTP,
        SS_TRAP_CONT,
        SS_TRAP_TTIN,
        SS_TRAP_TTOU,                   /* 45 */
        SS_TRAP_VTALRM,
        SS_TRAP_PROF,
        SS_TRAP_XCPU,
        SS_TRAP_XFSZ,
        SS_TRAP_WAITING,                /* 50 */
        SS_TRAP_LWP,
        SS_TRAP_FREEZE,
        SS_TRAP_THAW,
        SS_TRAP_CANCEL,
        SS_TRAP_LOST,                   /* 55 */
        SS_TRAP_KILL,
        SS_TRAP_UNUSED,
        SS_TRAP_STKFLT,
        SS_TRAP_INFO,
        SS_TRAP_LAST_INDEX  /* the final index of the enum - NOT a trap signal */
} ss_trapcode_t;

#define SS_TRAP_FPE_ANY \
SS_TRAP_FPE:case \
SS_TRAP_FPE_INTOVFLOW:case \
SS_TRAP_FPE_INTDIV0:case \
SS_TRAP_FPE_INVALID:case \
SS_TRAP_FPE_ZERODIVIDE:case \
SS_TRAP_FPE_OVERFLOW:case \
SS_TRAP_FPE_UNDERFLOW:case \
SS_TRAP_FPE_INEXACT:case \
SS_TRAP_FPE_STACKFAULT:case \
SS_TRAP_FPE_STACKOVERFLOW:case \
SS_TRAP_FPE_STACKUNDERFLOW:case \
SS_TRAP_FPE_BOUND:case \
SS_TRAP_FPE_DENORMAL:case \
SS_TRAP_FPE_EXPLICITGEN:case \
SS_TRAP_FPE_END


#define SS_TRAP_HANDLERSECTION {\
        int ss_trap_threadidx, ss_trap_code;\
        trap_jmp_buf_t buf; \
        if ((ss_trap_code = SS_TRAP_SETJMP(ss_trap_getbuf(&buf, &ss_trap_threadidx))) != SS_TRAP_NONE) {\
            switch (ss_trap_code) {
#define SS_TRAP_GETCODE()       (ss_trap_code)
#define SS_TRAP_RERAISE_AS(tc)  ss_trap_reraise_as(tc, ss_trap_threadidx)
#define SS_TRAP_RERAISE()       ss_trap_reraise_as(ss_trap_code, ss_trap_threadidx)
#define SS_TRAP_QUITHANDLER()   ss_trap_popbuf(ss_trap_threadidx)

#	define SS_TRAP_RUNSECTION      } } else {

#	define SS_TRAP_END             } ss_trap_popbuf(ss_trap_threadidx); }


#define SS_TRAP_STARTCATCH(trapcode) \
            ss_trap_installhandlerfun(\
                trapcode, (ss_traphandlerfp_t)ss_trap_handlerfun)

#define SS_TRAP_STOPCATCH(trapcode) \
            ss_trap_installhandlerfun(\
                trapcode, SS_TRAPHANDLERFUN_DFL)

#define SS_TRAP_IGNORE(trapcode) \
            ss_trap_installhandlerfun(\
                trapcode, SS_TRAPHANDLERFUN_IGN)

#define SS_TRAPHANDLERFUN_IGN ((ss_traphandlerfp_t)(ulong)1)
#define SS_TRAPHANDLERFUN_DFL ((ss_traphandlerfp_t)(ulong)0)

void ss_trap_globalinit(int maxthreads);
void ss_trap_globaldone(void);

void ss_trap_threaddone(void);

typedef void (SS_CLIBCALLBACK * ss_traphandlerfp_t)(int);

void* ss_trap_getbuf(trap_jmp_buf_t* jmp_buf, int* p_threadidx);
void ss_trap_popbuf(int threadidx);
void ss_trap_reraise_as(ss_trapcode_t trapcode, int threadidx);
void ss_trap_raise(ss_trapcode_t trapcode);
void ss_trap_kill(int pid, ss_trapcode_t trapcode);

ss_traphandlerfp_t ss_trap_installhandlerfun(
        ss_trapcode_t trapcode,
        ss_traphandlerfp_t handler_fun);

void SsMatherrLink(void);

ss_trapcode_t ss_trap_sig2trapcode(int sig);
int ss_trap_code2sig(ss_trapcode_t trapcode);

#if defined(WINDOWS) || defined(SS_NT)
  void SS_CDECL ss_trap_handlerfun(int trapcode);
#else
  void SS_CLIBCALLBACK ss_trap_handlerfun(int trapcode);
#endif

/* raise SS_TRAP_ALRM after msecs (millisecs) */
SsTimerRequestIdT ss_trap_setalarm(ulong msecs);

/* returns back time left in alarm timer (in ms) */
long ss_trap_cancelalarm(SsTimerRequestIdT alarmtimer);

extern ss_trapcode_t ss_exittrap_array[];
void ss_trap_preventtraphandlerinstallation( ss_trapcode_t trapcode );

#endif /* SSTRAPH_H */
