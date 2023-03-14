/*************************************************************************\
**  source       * sstraph.c
**  directory    * ss
**  description  * Portable trap handling system
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

The system is built using standard C library routines signal() & raise()
along with setjmp() & longjmp(). The handler sections are implemented
using C preprocessor macros.

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
#include "ssc.h"

#include "ssdebug.h"
#include "ssmem.h"
#include "ssthread.h"
#include "sstraph.h"
#include "ssstdarg.h"
#include "ssstring.h"
#include "sssignal.h"
#include "sssprint.h"
#include "ssmath.h"
#if defined(SS_UNIX)
#include <unistd.h>
#endif /* SS_UNIX */
#if defined(SS_NT)
#include <float.h>
#endif

/* Windows MT ---------------------------------------------------------------------- */
#if defined(SS_NT)

#ifdef SS_USE_WIN32TLS_API

#   define trap_max_threads 1
#   define TRAP_GETTHREADIDX() 0
#   define TRAP_GETJMPBUF(threadidx)    ((trap_jmp_buf_t*)SsThrDataGet(SS_THRDATA_TRAPH))
#   define TRAP_SETJMPBUF(threadidx, b) SsThrDataSet(SS_THRDATA_TRAPH, b)

#else /* SS_USE_WIN32TLS_API */

# if defined(SS_DLLQMEM)
    static
# elif !defined(SS_MT)
    static
# else
    static __declspec(thread)
# endif
            trap_jmp_buf_t* trap_jmpbuf = NULL;

#   define trap_max_threads 1
#   define TRAP_GETTHREADIDX() 0
#   define TRAP_GETJMPBUF(threadidx)    trap_jmpbuf
#   define TRAP_SETJMPBUF(threadidx, b) trap_jmpbuf = (b)

#endif /* SS_USE_WIN32TLS_API */

/* ---------------------------------------------------------------------- */
#elif defined(SS_PTHREAD)

#   define trap_max_threads 1
#   define TRAP_GETTHREADIDX() 0
#   define TRAP_GETJMPBUF(threadidx)    ((trap_jmp_buf_t*)SsThrDataGet(SS_THRDATA_TRAPH))
#   define TRAP_SETJMPBUF(threadidx, b) SsThrDataSet(SS_THRDATA_TRAPH, b)


/* ---------------------------------------------------------------------- */
#else /* Other than SS_NT or SS_PTHREAD */

#   if defined(SS_MT)
#       define TRAP_INITTHREADS SS_THR_MAXTHREADS
#       define TRAP_GETTHREADIDX() SsThrGetid()
#   else
#       define TRAP_INITTHREADS 1
#       define TRAP_GETTHREADIDX() 0
#   endif

    typedef struct {
        trap_jmp_buf_t* th_jmpbuf;
    } trap_handler_t;

    static trap_handler_t trap_handler_initarr[TRAP_INITTHREADS];
    static int trap_max_threads =
                sizeof(trap_handler_initarr) / sizeof(trap_handler_initarr[0]);
    static trap_handler_t* trap_handler_array = trap_handler_initarr;
#   define TRAP_GETJMPBUF(threadidx)    (trap_handler_array[threadidx].th_jmpbuf)
#   define TRAP_SETJMPBUF(threadidx, b) {trap_handler_array[threadidx].th_jmpbuf = (b);}


#endif /* SS_NT */


/*##**********************************************************************\
 *
 *		ss_trap_globalinit
 *
 * Initializes trap handling system
 *
 * Parameters :
 *
 *	maxthreads - in
 *		maximum # of threads, 1 is minimum
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void ss_trap_globalinit(
        int maxthreads __attribute__ ((unused)))
{
#if defined(SS_NT) 
        /* Enable Floating point exceptions */
#if defined(_M_IX86) /* Intel x86 processor? */
# ifdef SS_MT
        _control87(
            0,
            EM_INVALID |
            EM_ZERODIVIDE |
            EM_OVERFLOW |
            EM_UNDERFLOW |
            EM_DENORMAL);   /* has effect on x86 only */
# else
        _control87(
            0,
            EM_INVALID |
            EM_ZERODIVIDE);
# endif
#else   /* processor other than Intel x86 or wce */
        _controlfp(
            0,
            EM_INVALID |
            EM_ZERODIVIDE |
            EM_OVERFLOW |
            EM_UNDERFLOW);
#endif  /* processor selection */

#elif defined(SS_PTHREAD)

        /* trap_jmpbuf_array saved into thread context using
           function SsThrDataSet */

#else /* SS_NT */
        int i;
        ss_dassert(maxthreads >= 1);
#if !defined(SS_MT)
        maxthreads = 1;
#endif
        if (trap_handler_array == trap_handler_initarr) {
            if (maxthreads <= trap_max_threads) {
                return;
            }
            trap_handler_array =
                SsMemAlloc(sizeof(trap_handler_t) * maxthreads);
            memcpy(
                trap_handler_array,
                trap_handler_initarr,
                sizeof(trap_handler_initarr));
        } else {
            trap_handler_array =
                SsMemRealloc(
                    trap_handler_array,
                    sizeof(trap_handler_t) * maxthreads);
        }
        for (i = trap_max_threads; i < maxthreads; i++) {
            trap_handler_array[i].th_jmpbuf = NULL;
        }
        trap_max_threads = maxthreads;
#endif  /* SS_NT */
}

/*##**********************************************************************\
 *
 *		ss_trap_globaldone
 *
 * Deallocates trap handling system
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
void ss_trap_globaldone(void)
{
#ifdef SS_DEBUG
#if !defined(SS_NT) && !defined(SS_PTHREAD) 
        int i;

        ss_dassert(trap_handler_array != NULL);
        for (i = 0; i < trap_max_threads; i++) {
            ss_assert(TRAP_GETJMPBUF(i) == NULL);
        }
        if (trap_handler_array != trap_handler_initarr) {
            SsMemFree(trap_handler_array);
            trap_handler_array = trap_handler_initarr;
            trap_max_threads =
                sizeof(trap_handler_initarr) / sizeof(trap_handler_initarr[0]);
            memset(trap_handler_array, 0, sizeof(trap_handler_initarr));
        }
#endif
#endif /* SS_DEBUG */
}

/*#***********************************************************************\
 *
 *		ss_trap_threadinit
 *
 * Does thread local initializations.
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
static void ss_trap_threadinit(void)
{
}

/*##**********************************************************************\
 *
 *		ss_trap_threaddone
 *
 * Does thread local initializations.
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
void ss_trap_threaddone(void)
{
}

/*##**********************************************************************\
 *
 *		ss_trap_getbuf
 *
 * Gets a jmp_buf for setjmp. It pushes a new jmp_buf to stack and
 * returns a pointer to it. Each thread has its own stack of jmp_buf's
 *
 * Parameters :
 *      p_threadidx - out, use
 *          pointer to variable where to store the thread index
 *          (0 .. maxthreads - 1)
 *
 * Return value - ref :
 *      pointer to jmp_buf
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* ss_trap_getbuf(trap_jmp_buf_t* buf, int* p_threadidx)
{
#if 0 /* SS_DLLQMEM */
        /* dummy in DLL client library */
        static SS_TRAP_JMP_BUF jbuf;

        return (jbuf);
#else /* SS_DLLQMEM */
#if 0 	/* removed unnecessary memory allocation, buf is now allocated in stack */
    trap_jmp_buf_t* buf;
#endif /* 0 */
        int threadidx;

        ss_dassert(p_threadidx != NULL);
        threadidx = TRAP_GETTHREADIDX();
        ss_assert((unsigned)threadidx < (unsigned)trap_max_threads);
#if 0 	/* removed unnecessary memory allocation, buf is now allocated in stack */
    buf = SSMEM_NEW(trap_jmp_buf_t);
#endif /* 0 */
        buf->tjb_next = TRAP_GETJMPBUF(threadidx);
        TRAP_SETJMPBUF(threadidx, buf);
        *p_threadidx = threadidx;
        return (buf->tjb_jbuf);
#endif /* SS_DLLQMEM */
}


/*##**********************************************************************\
 *
 *		ss_trap_popbuf
 *
 * Pops the jmp_buf from top of handler stack
 *
 * Parameters :
 *
 *	threadidx - in
 *		thread index (0 .. maxthreads - 1)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void ss_trap_popbuf(int threadidx __attribute__ ((unused)))
{
#if 0 /* SS_DLLQMEM */
        return; /* dummy in DLL client library */
#else /* SS_DLLQMEM */
        trap_jmp_buf_t* buf;

        ss_dassert((unsigned)threadidx < (unsigned)trap_max_threads);
        ss_dassert(TRAP_GETJMPBUF(threadidx) != NULL);
        buf = TRAP_GETJMPBUF(threadidx);
        TRAP_SETJMPBUF(threadidx, buf->tjb_next);
#if 0 	/* removed unnecessary memory allocation, buf is now allocated in stack */
         SsMemFree(buf);
#endif /* 0 */
#endif /* SS_DLLQMEM */
}

/*#***********************************************************************\
 *
 *		ss_trap_sig2trap
 *
 * Maps system-specific signal number to trap code
 *
 * Parameters :
 *
 *	sig - in
 *		signal number
 *
 * Return value :
 *      trap code
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
typedef struct {
        int signal;
        ss_trapcode_t trap_code;
} sig_node_t;


static sig_node_t sig_array[] = {
#ifdef SIGHUP
    { SIGHUP, SS_TRAP_HUP },
#endif /* SIGHUP */

#ifdef SIGINT
    { SIGINT, SS_TRAP_INT },
#endif /* SIGINT */

#ifdef SIGQUIT
    { SIGQUIT, SS_TRAP_QUIT },
#endif /* SIGQUIT */

#ifdef SIGILL
    { SIGILL, SS_TRAP_ILL },
#endif /* SIGILL*/

#ifdef SIGABRT
    { SIGABRT, SS_TRAP_ABRT },
#endif /* SIGABRT */

#ifdef SIGFPE
    { SIGFPE, SS_TRAP_FPE },
#endif /* SIGFPE */

#ifdef SIGKILL
    { SIGKILL, SS_TRAP_KILL },
#endif /* SIGKILL */

#ifdef SIGSEGV
    { SIGSEGV, SS_TRAP_SEGV },
#endif /* SIGSEGV*/

#ifdef SIGPIPE
    { SIGPIPE, SS_TRAP_PIPE },
#endif /* SIGPIPE*/

#ifdef SIGALRM
    { SIGALRM, SS_TRAP_ALRM },
#endif /* SIGALRM*/

#ifdef SIGTERM
    { SIGTERM, SS_TRAP_TERM },
#endif /* SIGTERM */

#ifdef SIGUSR1
    { SIGUSR1, SS_TRAP_USR1 },
#endif /* SIGUSR1 */

#ifdef SIGUSR2
    { SIGUSR2, SS_TRAP_USR2 },
#endif /* SIGUSR2 */

#ifdef SIGCHLD
    { SIGCHLD, SS_TRAP_CHLD },
#endif /* SIGCHLD */

#ifdef SIGCONT
    { SIGCONT, SS_TRAP_CONT },
#endif /* SIGCONT */

#ifdef SIGSTOP
    { SIGSTOP, SS_TRAP_STOP },
#endif /* SIGSTOP */

#ifdef SIGTSTP
    { SIGTSTP, SS_TRAP_TSTP },
#endif /* SIGTSTP */

#ifdef SIGTTIN
    { SIGTTIN, SS_TRAP_TTIN },
#endif /* SIGTTIN */

#ifdef SIGTTOU
    { SIGTTOU, SS_TRAP_TTOU },
#endif /* SIGTTOU */

#ifdef SIGTRAP
    { SIGTRAP, SS_TRAP_TRAP },
#endif /* SIGTRAP */

#ifdef SIGIOT
    { SIGIOT, SS_TRAP_IOT },
#endif /* SIGIOT */

#ifdef SIGEMT
    { SIGEMT, SS_TRAP_EMT },
#endif /* SIGEMT */

#ifdef SIGBUS
    { SIGBUS, SS_TRAP_BUS },
#endif /* SIGBUS */

#ifdef SIGSYS
    { SIGSYS, SS_TRAP_SYS },
#endif /* SIGSYS */

#ifdef SIGSTKFLT
    { SIGSTKFLT, SS_TRAP_STKFLT },
#endif /* SIGSTKFLT */

#ifdef SIGURG
    { SIGURG, SS_TRAP_URG },
#endif /* SIGURG */

#ifdef SIGIO
    { SIGIO, SS_TRAP_IO },
#endif /* SIGIO */

#ifdef SIGPOLL
    { SIGPOLL, SS_TRAP_POLL },
#endif /* SIGPOLL */

#ifdef SIGCLD
    { SIGCLD, SS_TRAP_CLD },
#endif /* SIGCLD */

#ifdef SIGXCPU
    { SIGXCPU, SS_TRAP_XCPU },
#endif /* SIGXCPU */

#ifdef SIGXFSZ
    { SIGXFSZ, SS_TRAP_XFSZ },
#endif /* SIGXFSZ */

#ifdef SIGVTALRM
    { SIGVTALRM, SS_TRAP_VTALRM },
#endif /* SIGVTALRM */

#ifdef SIGPROF
    { SIGPROF, SS_TRAP_PROF },
#endif /* SIGPROF*/

#ifdef SIGPWR
    { SIGPWR, SS_TRAP_PWR },
#endif /* SIGPWR */

#ifdef SIGINFO
    { SIGINFO, SS_TRAP_INFO },
#endif /* SIGINFO */

#ifdef SIGLOST
    { SIGLOST, SS_TRAP_LOST },
#endif /* SIGLOST */

#ifdef SIGWINCH
    { SIGWINCH, SS_TRAP_WINCH },
#endif /* SIGWINCH */

#ifdef SIGBREAK
    { SIGBREAK, SS_TRAP_BREAK },
#endif /* SIGBREAK */
    
#ifdef SIGUNUSED
    { SIGUNUSED, SS_TRAP_UNUSED }
#endif /* SIGUNUSED */
}; /* sigarray */

ss_trapcode_t ss_trap_sig2trapcode(int sig)
{

        int i = 0;

        /* Match signal/trap code pair */
        for (i = 0; (size_t)i < sizeof(sig_array)/sizeof(sig_array[0]); i++) {
            if(sig == sig_array[i].signal) {
                return sig_array[i].trap_code;
            }
        }
        return SS_TRAP_NONE; /* Signal value was not found */
}

/*#***********************************************************************\
 *
 *		ss_trap_code2sig
 *
 * Maps trap code to system-specific signal numbers
 *
 * Parameters :
 *
 *	trapcode - in
 *		trap code enum value
 *
 * Return value :
 *      signal number that must be provided to signal()
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int ss_trap_code2sig(ss_trapcode_t trapcode)
{
        switch (trapcode) {
            case SS_TRAP_NONE:
                return (-1);
            case SS_TRAP_HUP:
#               if defined(SIGHUP)
                    return (SIGHUP);
#               else
                    return (-1);
#               endif
            case SS_TRAP_ABRT:
#               if defined(SIGABRT)
                    return (SIGABRT);
#               else
                    return (-1);
#               endif
            case SS_TRAP_ILL:
#               if !defined(SIGILL)
                    return (-1);
#               else
                    return (SIGILL);
#               endif
            case SS_TRAP_INT:
#               if !defined(SIGINT)
                    return (-1);
#               else
                    return (SIGINT);
#               endif
            case SS_TRAP_BREAK:
#               if defined(SIGBREAK)
                    return (SIGBREAK);
#               else
                    return (-1);
#               endif
            case SS_TRAP_SEGV:
#               if !defined(SIGSEGV)
                    return (-1);
#               else
                    return (SIGSEGV);
#               endif
            case SS_TRAP_TERM:
#               if !defined(SIGTERM)
                    return (-1);
#               else
                    return (SIGTERM);
#               endif
            case SS_TRAP_USR1:
#               if defined(SIGUSR1)
                    return (SIGUSR1);
#               else
                    return (-1);
#               endif
            case SS_TRAP_USR2:
#               if defined(SIGUSR2)
                    return (SIGUSR2);
#               else
                    return (-1);
#               endif
            case SS_TRAP_USR3:
#               if defined(SIGUSR3)
                    return (SIGUSR3);
#               else
                    return (-1);
#               endif
            case SS_TRAP_FPE:
            case SS_TRAP_FPE_INTOVFLOW:
            case SS_TRAP_FPE_INTDIV0:
            case SS_TRAP_FPE_INVALID:
            case SS_TRAP_FPE_ZERODIVIDE:
            case SS_TRAP_FPE_OVERFLOW:
            case SS_TRAP_FPE_UNDERFLOW:
            case SS_TRAP_FPE_INEXACT:
            case SS_TRAP_FPE_STACKFAULT:
            case SS_TRAP_FPE_STACKOVERFLOW:
            case SS_TRAP_FPE_STACKUNDERFLOW:
            case SS_TRAP_FPE_BOUND:
            case SS_TRAP_FPE_DENORMAL:
            case SS_TRAP_FPE_EXPLICITGEN:
                return (SIGFPE);
            case SS_TRAP_PIPE:
#               if defined(SIGPIPE)
                    return (SIGPIPE);
#               else
#                   if defined(SS_UNIX) 
                        /* All unixes should have SIGPIPE, Ari */
                        ss_rc_derror(SS_TRAP_PIPE);
#                   endif /* SS_UNIX */
                    return (-1);
#               endif
            case SS_TRAP_QUIT:
#               if !defined(SIGQUIT)
                    return (-1);
#               else
                    return (SIGQUIT);
#               endif
            case SS_TRAP_TRAP:
#               if !defined(SIGTRAP)
                    return (-1);
#               else
                    return (SIGTRAP);
#               endif
            case SS_TRAP_IOT:
#               if !defined(SIGIOT)
                    return (-1);
#               else
                    return (SIGIOT);
#               endif
            case SS_TRAP_EMT:
#               if !defined(SIGEMT)
                    return (-1);
#               else
                    return (SIGEMT);
#               endif
            case SS_TRAP_SYS:
#               if !defined(SIGSYS)
                    return (-1);
#               else
                    return (SIGSYS);
#               endif
            case SS_TRAP_ALRM:
#               if !defined(SIGALRM)
                    return (-1);
#               else
                    return (SIGALRM);
#               endif
            case SS_TRAP_CLD:
#               if !defined(SIGCLD)
                    return (-1);
#               else
                    return (SIGCLD);
#               endif
            case SS_TRAP_CHLD:
#               if !defined(SIGCHLD)
                    return (-1);
#               else
                    return (SIGCHLD);
#               endif
            case SS_TRAP_PWR:
#               if !defined(SIGPWR)
                    return (-1);
#               else
                    return (SIGPWR);
#               endif
            case SS_TRAP_WINCH:
#               if !defined(SIGWINCH)
                    return (-1);
#               else
                    return (SIGWINCH);
#               endif
            case SS_TRAP_URG:
#               if !defined(SIGURG)
                    return (-1);
#               else
                    return (SIGURG);
#               endif
            case SS_TRAP_POLL:
#               if !defined(SIGPOLL)
                    return (-1);
#               else
                    return (SIGPOLL);
#               endif
            case SS_TRAP_IO:
#               if !defined(SIGIO)
                    return (-1);
#               else
                    return (SIGIO);
#               endif
            case SS_TRAP_STOP:
#               if !defined(SIGSTOP)
                    return (-1);
#               else
                    return (SIGSTOP);
#               endif
            case SS_TRAP_TSTP:
#               if !defined(SIGTSTP)
                    return (-1);
#               else
                    return (SIGTSTP);
#               endif
            case SS_TRAP_CONT:
#               if !defined(SIGCONT)
                    return (-1);
#               else
                    return (SIGCONT);
#               endif
            case SS_TRAP_TTIN:
#               if !defined(SIGTTIN)
                    return (-1);
#               else
                    return (SIGTTIN);
#               endif
            case SS_TRAP_TTOU:
#               if !defined(SIGTTOU)
                    return (-1);
#               else
                    return (SIGTTOU);
#               endif
            case SS_TRAP_VTALRM:
#               if !defined(SIGVTALRM)
                    return (-1);
#               else
                    return (SIGVTALRM);
#               endif
            case SS_TRAP_PROF:
#               if !defined(SIGPROF)
                    return (-1);
#               else
                    return (SIGPROF);
#               endif
            case SS_TRAP_XCPU:
#               if !defined(SIGXCPU)
                    return (-1);
#               else
                    return (SIGXCPU);
#               endif
            case SS_TRAP_XFSZ:
#               if !defined(SIGXFSZ)
                    return (-1);
#               else
                    return (SIGXFSZ);
#               endif
            case SS_TRAP_WAITING:
#               if !defined(SIGWAITING)
                    return (-1);
#               else
                    return (SIGWAITING);
#               endif
            case SS_TRAP_LWP:
#               if !defined(SIGLWP)
                    return (-1);
#               else
                    return (SIGLWP);
#               endif
            case SS_TRAP_FREEZE:
#               if !defined(SIGFREEZE)
                    return (-1);
#               else
                    return (SIGFREEZE);
#               endif
            case SS_TRAP_THAW:
#               if !defined(SIGTHAW)
                    return (-1);
#               else
                    return (SIGTHAW);
#               endif
            case SS_TRAP_CANCEL:
#               if !defined(SIGCANCEL)
                    return (-1);
#               else
                    return (SIGCANCEL);
#               endif
            case SS_TRAP_LOST:
#               if !defined(SIGLOST) 
                    return (-1);
#               else
                    return (SIGLOST);
#               endif
            case SS_TRAP_STKFLT:
#               if !defined(SIGSTKFLT) 
                    return (-1);
#               else
                    return (SIGSTKFLT);
#               endif
            default:
                return (-1);
        }
}


static bool cancelarray_initialized = FALSE;
static bool cancelled_trapcodes[SS_TRAP_LAST_INDEX];

/*##**********************************************************************\
 *
 *      ss_trap_blocktraphandlerinstallation
 *
 * Prevents the installation of trap handlers for a given trap code. This
 * function overrides the following attempts to install a new trap handler
 * for a given trap code.
 *
 * Parameters:
 *      trapcode - in
 *          
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void ss_trap_preventtraphandlerinstallation( ss_trapcode_t trapcode ){
        ss_bassert(trapcode > 0 && trapcode < SS_TRAP_LAST_INDEX);
        if (cancelarray_initialized == FALSE){
            memset(cancelled_trapcodes, FALSE, SS_TRAP_LAST_INDEX);
            cancelarray_initialized = TRUE;
        } 
        cancelled_trapcodes[trapcode] = TRUE;
}

static bool trap_installprevented( ss_trapcode_t trapcode ){
        ss_bassert(trapcode > 0 && trapcode < SS_TRAP_LAST_INDEX);
        if (cancelarray_initialized == FALSE){
            memset(cancelled_trapcodes, FALSE, SS_TRAP_LAST_INDEX);
            cancelarray_initialized = TRUE;
        } 
        return cancelled_trapcodes[trapcode];
}

/*##**********************************************************************\
 *
 *		ss_trap_installhandlerfun
 *
 * Installs handler to specified signal code
 *
 * Parameters :
 *
 *	trapcode - in
 *		signal code
 *
 *	handler_fun - in
 *		pointer to signal handler function
 *          SS_TRAPHANDLERFUN_IGN to ignore the signal or
 *          SS_TRAPHANDLERFUN_DFL to restore default handler
 *
 * Return value - ref :
 *      pointer to previous handler function
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
ss_traphandlerfp_t ss_trap_installhandlerfun(
        ss_trapcode_t trapcode,
        ss_traphandlerfp_t handler_fun)
{
        int sig;
        ss_traphandlerfp_t rc;
        
        sig = ss_trap_code2sig(trapcode);
        if (sig != -1 && !trap_installprevented(trapcode)) {
            ss_trap_threadinit();
            ss_dprintf_2(("installed signal handler for trap %d\n",trapcode));
            if (handler_fun == SS_TRAPHANDLERFUN_IGN) {
                handler_fun = (ss_traphandlerfp_t)SIG_IGN;
            } else if (handler_fun == SS_TRAPHANDLERFUN_DFL) {
                handler_fun = (ss_traphandlerfp_t)SIG_DFL;
            }
            rc = ((ss_traphandlerfp_t)signal(sig, handler_fun));

            return rc;
        }
        return ((ss_traphandlerfp_t)-1);
}


/*##**********************************************************************\
 *
 *		trap_fpeextcode2trapcode
 *
 * Gets a subcode for SIGFPE exception
 *
 * Parameters :
 *
 *	extcode - in
 *		extended signal code
 *
 * Return value :
 *      Trap code corresponding to signal
 *
 * Comments :
 *      currently defined only under 32-bit OS/2
 *
 * Globals used :
 *
 * See also :
 */
#if defined(SS_NT) 
ss_trapcode_t trap_fpeextcode2trapcode(int extcode)
{
#if defined(CSET2)
        switch (extcode) {
            case FPE_INTDIV0:
                return (SS_TRAP_FPE_INTDIV0);
            case FPE_INT_OFLOW:
                return (SS_TRAP_FPE_INTOVFLOW);
            case FPE_BOUND:
                return (SS_TRAP_FPE_BOUND);
            case FPE_INVALID:
                return (SS_TRAP_FPE_INVALID);
            case FPE_DENORMAL:
                return (SS_TRAP_FPE_DENORMAL);
            case FPE_ZERODIVIDE:
                return (SS_TRAP_FPE_ZERODIVIDE);
            case FPE_OVERFLOW:
                return (SS_TRAP_FPE_OVERFLOW);
            case FPE_UNDERFLOW:
                return (SS_TRAP_FPE_UNDERFLOW);
            case FPE_INEXACT:
                return (SS_TRAP_FPE_INEXACT);
            case FPE_STACKOVERFLOW:
                return (SS_TRAP_FPE_STACKOVERFLOW);
            case FPE_STACKUNDERFLOW:
                return (SS_TRAP_FPE_STACKUNDERFLOW);
            case FPE_EXPLICITGEN:
                return (SS_TRAP_FPE_EXPLICITGEN);
            default:
                return (SS_TRAP_FPE);
        }
#elif defined(BORLANDC)
        switch (extcode) {
            case FPE_INTDIV0:
                return (SS_TRAP_FPE_INTDIV0);
            case FPE_INTOVFLOW:
                return (SS_TRAP_FPE_INTOVFLOW);
            case FPE_INVALID:
                return (SS_TRAP_FPE_INVALID);
            case FPE_ZERODIVIDE:
                return (SS_TRAP_FPE_ZERODIVIDE);
            case FPE_OVERFLOW:
                return (SS_TRAP_FPE_OVERFLOW);
            case FPE_UNDERFLOW:
                return (SS_TRAP_FPE_UNDERFLOW);
            case FPE_INEXACT:
                return (SS_TRAP_FPE_INEXACT);
            case FPE_STACKFAULT:
                return (SS_TRAP_FPE_STACKFAULT);
            case FPE_EXPLICITGEN:
                return (SS_TRAP_FPE_EXPLICITGEN);
            default:
                return (SS_TRAP_FPE);
        }
#elif defined(SS_NT) 
        switch (extcode) {
            case _FPE_INVALID:
                return (SS_TRAP_FPE_INVALID);
            case _FPE_DENORMAL:
                return (SS_TRAP_FPE_DENORMAL);
            case _FPE_ZERODIVIDE:
                return (SS_TRAP_FPE_ZERODIVIDE);
            case _FPE_OVERFLOW:
                return (SS_TRAP_FPE_OVERFLOW);
            case _FPE_UNDERFLOW:
                return (SS_TRAP_FPE_UNDERFLOW);
            case _FPE_INEXACT:
                return (SS_TRAP_FPE_INEXACT);
            case _FPE_STACKOVERFLOW:
                return (SS_TRAP_FPE_STACKOVERFLOW);
            case _FPE_STACKUNDERFLOW:
                return (SS_TRAP_FPE_STACKUNDERFLOW);
            case _FPE_EXPLICITGEN:
                return (SS_TRAP_FPE_EXPLICITGEN);
            default:
                return (SS_TRAP_FPE);
        }
#else /* compiler */
# error This environment is not specified for extended FPE codes!
#endif /* compiler */
}
#endif /* 32 bit OS/2  || Windows NT */

/*#***********************************************************************\
 *
 *		trap_resetfpsystem
 *
 * Resets floating-point signal after floating-point error
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
static void trap_resetfpsystem(void)
{
#if defined(SS_NT) 
        _fpreset();
#endif
}

/*##**********************************************************************\
 *
 *		ss_trap_handlerfun
 *
 * Handler function that makes a longjmp() to the handling position
 *
 * Parameters :
 *
 *      trapcode - in
 *          system-specific signal number
 *
 *      ... - in
 *          possible extended code
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#if defined(WINDOWS) || defined(SS_NT) 
  void SS_CDECL ss_trap_handlerfun(int sig)
#else
  void SS_CLIBCALLBACK ss_trap_handlerfun(int sig)
#endif

{
        int threadidx;
        int extcode __attribute__ ((unused));
        int trapcode;
        va_list ap __attribute__ ((unused));
        char text[128];

        threadidx = (int)TRAP_GETTHREADIDX();
        ss_dassert(threadidx >= 0);
        ss_assert(threadidx < trap_max_threads);

        trapcode = ss_trap_sig2trapcode(sig);
        if (trapcode == SS_TRAP_FPE) {
            trap_resetfpsystem();
#if (defined(SS_NT) && !defined(SS_NT64))
            va_start(ap, sig);
            extcode = va_arg(ap, int);
            va_end(ap);
            trapcode = trap_fpeextcode2trapcode(extcode);
#endif
        }
        if (trapcode == -1) {
            ss_rc_error(sig);
        }
        ss_trap_installhandlerfun(
            (ss_trapcode_t)trapcode,
            (ss_traphandlerfp_t)ss_trap_handlerfun);
        if (TRAP_GETJMPBUF(threadidx) == NULL) {
            if (trapcode == SS_TRAP_ALRM) {
                return;
            }
            SsSprintf(text, "Error! handler stack is empty, trapcode %d\n", trapcode);
            SsAssertionMessage(text, (char *)__FILE__, __LINE__);
        }
        ss_assert(TRAP_GETJMPBUF(threadidx) != NULL);
        ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
        ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
        SS_TRAP_LONGJMP(TRAP_GETJMPBUF(threadidx)->tjb_jbuf, trapcode);
}

/*##**********************************************************************\
 *
 *		ss_trap_reraise_as
 *
 * Reraises the trap signal (passes control to next handler)
 *
 * Parameters :
 *
 *	trapcode - in
 *		trap code
 *
 *	threadidx - in
 *		thread # (0 .. maxthreads - 1)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void ss_trap_reraise_as(ss_trapcode_t trapcode, int threadidx)
{
        ss_assert(trap_max_threads > threadidx);
        ss_trap_popbuf(threadidx);
        ss_assert(TRAP_GETJMPBUF(threadidx) != NULL);
        ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
        ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
        SS_TRAP_LONGJMP(TRAP_GETJMPBUF(threadidx)->tjb_jbuf, trapcode);
}


/*##**********************************************************************\
 *
 *		ss_trap_raise
 *
 * Raises a trap condition.
 *
 * Parameters :
 *
 *	trapcode - in
 *		trap code
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void ss_trap_raise(ss_trapcode_t trapcode)
{
#if defined(SS_MT) 
        {
            int threadidx __attribute__ ((unused));

            threadidx = TRAP_GETTHREADIDX();
            ss_assert(TRAP_GETJMPBUF(threadidx) != NULL);
            ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
            ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
            SS_TRAP_LONGJMP(TRAP_GETJMPBUF(threadidx)->tjb_jbuf, trapcode);
            ss_error;
        }
#else /* Linux with glibc 2 */
        int sig;

        sig = ss_trap_code2sig(trapcode);

        raise(sig);

#endif  /* Linux with glibc 2 */
}

/*##**********************************************************************\
 *
 *		ss_trap_kill
 *
 * Sends a trap to pid.
 *
 * Parameters :
 *
 *  pid - in
 *      pid of process to be signalled
 *	trapcode - in
 *		trap code
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void ss_trap_kill(int pid, ss_trapcode_t trapcode)
{
#if defined(SS_UNIX) 
        kill(pid, ss_trap_code2sig(trapcode));
#endif
}

#if defined(SS_MT) && defined(SS_PTHREAD)

#include <pthread.h>
#include "sstimer.h"

static void trap_alarmcallback(
        void* p,
        SsTimerRequestIdT req_id __attribute__ ((unused)))
{
        pthread_kill(*((pthread_t*)p), SIGALRM);
        SsMemFree(p);
}

#endif /* SS_MT && SS_PTHREAD */

SsTimerRequestIdT ss_trap_setalarm(ulong msecs)
{
#if defined(SS_MT) && defined(SS_PTHREAD)
        pthread_t* p = SSMEM_NEW(pthread_t);
        *p = pthread_self();
        return (SsTimerAddRequest(msecs,
                                  trap_alarmcallback,
                                  p));
#elif defined(SS_UNIX)
        ulong secs;

        secs = msecs / 1000UL;
        if (secs == 0) {
            if (msecs != 0) {
                secs = 1;
            }
        }
        ss_dassert((uint)secs == secs);
        secs = alarm(secs);
        msecs = secs * 1000UL;
        return ((SsTimerRequestIdT)msecs);
#else /* SS_UNIX, SS_PTHREAD */
        return ((SsTimerRequestIdT)0);
#endif /* SS_UNIX, SS_PTHREAD */
}

long ss_trap_cancelalarm(SsTimerRequestIdT alarmtimer)
{
#if defined(SS_MT) && defined(SS_PTHREAD)
        void* ctx;
        long time_left = SsTimerCancelRequestGetCtx(alarmtimer, &ctx);
        if (ctx != NULL) {
            SsMemFree(ctx);
        }
        return (time_left);
#elif defined(SS_UNIX) 
        return (alarm(0) * 1000L);
#else /* SS_UNIX, SS_PTHREAD */
        return (0L);
#endif /* SS_UNIX, SS_PTHREAD */
}
#ifdef SS_UNIX

#include "sstraph.h"

#if defined(SS_LINUX) 
ss_trapcode_t ss_exittrap_array[] = {SS_TRAP_HUP, SS_TRAP_QUIT, SS_TRAP_ILL, SS_TRAP_ABRT, SS_TRAP_USR2, SS_TRAP_BUS, SS_TRAP_STKFLT, SS_TRAP_IO, SS_TRAP_POLL, SS_TRAP_XCPU, SS_TRAP_XFSZ, SS_TRAP_VTALRM, SS_TRAP_PROF, SS_TRAP_PWR, SS_TRAP_LOST, SS_TRAP_UNUSED, -1};
#endif /* SS_LINUX */
#ifdef SS_SOLARIS
ss_trapcode_t ss_exittrap_array[] = {SS_TRAP_HUP, SS_TRAP_USR1, SS_TRAP_USR2, SS_TRAP_POLL, SS_TRAP_VTALRM, SS_TRAP_PROF, -1};
#endif /* SS_SOLARIS */
#ifdef SS_FREEBSD
ss_trapcode_t ss_exittrap_array[] = {SS_TRAP_HUP, SS_TRAP_USR1, SS_TRAP_USR2, SS_TRAP_XCPU, SS_TRAP_XFSZ, SS_TRAP_VTALRM, SS_TRAP_PROF, -1};
#endif /* SS_FREEBSD */
#ifdef SS_BSI
ss_trapcode_t ss_exittrap_array[] = {SS_TRAP_HUP, SS_TRAP_USR1, SS_TRAP_USR2, SS_TRAP_XCPU, SS_TRAP_XFSZ, SS_TRAP_VTALRM, SS_TRAP_PROF, -1};
#endif /* SS_BSI */
#ifdef SS_SCO
ss_trapcode_t ss_exittrap_array[] = {SS_TRAP_HUP, SS_TRAP_USR1, SS_TRAP_USR2, SS_TRAP_VTALRM, SS_TRAP_PROF, SS_TRAP_WINCH, -1};
#endif /* SS_SCO */

#else /* SS_UNIX */

ss_trapcode_t ss_exittrap_array[] = {-1};

#endif /* SS_UNIX */

