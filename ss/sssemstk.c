/*************************************************************************\
**  source       * sssemstk.c
**  directory    * ss
**  description  * Semaphore debugging stack routines.
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
#include "ssstdlib.h"
#include "ssstring.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssthread.h"
#include "sssem.h"
/* #include "sssemstk.h" */

#ifdef SS_SEMSTK_DBG

#define CALLSTK_MAX 100    /* MAX # of nested semaphore calls */

typedef struct cs_fun_st cs_fun_t;
typedef struct callstk_st callstack_t;

static callstack_t* callstk_list = (callstack_t*)&callstk_list;

struct callstk_st {
        callstack_t*  cs_next;
        int         cs_ncalls;
        ss_semnum_t cs_calls[CALLSTK_MAX];
};

#if defined(SS_MT) && !defined(SS_DLLQMEM)

#define CALLSTK_LINK(cs) \
{\
         (cs)->cs_next = callstk_list;\
         callstk_list = (cs);\
}

#define CALLSTK_LINK_IF(cs) \
{\
        if ((cs)->cs_next == NULL) {\
            CALLSTK_LINK(cs);\
        }\
}

# if  defined(SS_NT)

#    define TRACE_CALLSTACK 1

#ifdef SS_USE_WIN32TLS_API

#    define CALLSTACK(cs)   {   cs = SsThrDataGet(SS_THRDATA_SEMSTACK);\
                                if (cs == NULL) {\
                                    cs = calloc(sizeof(callstack_t), 1);\
                                    CALLSTK_LINK(cs);\
                                    SsThrDataSet(SS_THRDATA_SEMSTACK, cs);\
                                }\
                            }
#    define CHK_THRID

#else /* SS_USE_WIN32TLS_API */

#    define CALLSTACK(cs)   {cs = &callstack; CALLSTK_LINK_IF(cs); }
#    define CHK_THRID
     static __declspec(thread) callstack_t callstack;

#endif /* SS_USE_WIN32TLS_API */

#  else /* SS_NT */

/* Multithreaded UNIX ? Probably. */
#    define TRACE_CALLSTACK 1
#    define CALLSTACK(cs)   {   cs = SsThrDataGet(SS_THRDATA_SEMSTACK);\
                                if (cs == NULL) {\
                                    cs = calloc(sizeof(callstack_t), 1);\
                                    CALLSTK_LINK(cs);\
                                    SsThrDataSet(SS_THRDATA_SEMSTACK, cs);\
                                }\
                            }
#    define CHK_THRID

#  endif /* SS_NT */

# else /* SS_MT */

/* #    define TRACE_CALLSTACK 1 */
/* #    define CALLSTACK(cs)   cs = &callstack */
/* #    define CHK_THRID */
/*      static callstack_t callstack; */

#endif /* SS_MT && !DSS_DLLQMEM */

#ifdef TRACE_CALLSTACK

extern bool ss_debug_disablesemstk;

static void StkPrint(callstack_t* cs)
{
        int i;
        SsPrintf("SemStack:\n");
        for (i = 0; i < cs->cs_ncalls; i++) {
            if (   (cs->cs_calls[i] < SS_SEMNUM_ANONYMOUS_MES)
                || (cs->cs_calls[i] > SS_SEMNUM_LAST)) {
                
                SsSemStkPosT* ssp;

                ssp = (SsSemStkPosT*) cs->cs_calls[i];
                SsPrintf("mesw: file=%s, line=%d\n", ssp->ssp_fname, ssp->ssp_lineno);
            } else {
                SsPrintf("%d\n", cs->cs_calls[i]);
            }
        }
}


void SsSemStkPrint(void)
{
        callstack_t* cs;

        for (cs = callstk_list;
             cs != (callstack_t*)&callstk_list;
             cs = cs->cs_next)
        {
            StkPrint(cs);
        }
}

void SsSemStkEnterCheck(ss_semnum_t num)
{
        callstack_t* cs;

        if (ss_debug_disablesemstk) {
            return;
        }

        CALLSTACK(cs);
        CHK_THRID;

        ss_assert(num != SS_SEMNUM_NOTUSED);
        if (cs->cs_ncalls >= CALLSTK_MAX) {
            ss_debug_disablesemstk = TRUE;
            SsDbgPrintf("SsSemStkEnter() Error: call stack overflow!\n");
            ss_info_assert(0, ("Call stack overflow: ncalls = %d, MAX = %d",
                               cs->cs_ncalls, (int) CALLSTK_MAX));
        }
        if (cs->cs_ncalls > 0) {
            ss_semnum_t curnum;
            curnum = cs->cs_calls[cs->cs_ncalls - 1];
            ss_assert(curnum != SS_SEMNUM_NOTUSED);
            if (curnum > num && curnum > 0 && num > 0
#ifdef SS_DEBUG /* because of sssempux's debug objects */
                && curnum < SS_SEMNUM_LAST && num < SS_SEMNUM_LAST
#endif
                ) {
                
                ss_debug_disablesemstk = TRUE;
                StkPrint(cs);
                SsPrintf("SsSemStkEnterCheck: num: %d, curnum: %d\n",
                         num, curnum);
                ss_info_assert(0, ("SsSemStkEnterCheck: num: %d, curnum: %d",
                               num, curnum));

            }
        }
}

void SsSemStkEnterZeroTimeout(ss_semnum_t num)
{
        if (ss_debug_disablesemstk) {
            return;
        }

        if (num != SS_SEMNUM_DBE_CACHE_HASH) {
            SsSemStkEnter(num);

        } else {
            callstack_t* cs;
            ss_semnum_t lastnum;

            CALLSTACK(cs);
            CHK_THRID;

            if (cs->cs_ncalls >= CALLSTK_MAX) {
                ss_debug_disablesemstk = TRUE;
                StkPrint(cs);
                SsDbgPrintf("SsSemStkEnterZeroTimeout() Error: call stack overflow!\n");
                ss_error;
            }
            ss_assert(cs->cs_ncalls > 0);
            lastnum = cs->cs_calls[cs->cs_ncalls - 1];
            if (lastnum != SS_SEMNUM_DBE_CACHE_LRU
            &&  lastnum != SS_SEMNUM_DBE_CACHE_HASH) {
                ss_debug_disablesemstk = TRUE;
                StkPrint(cs);
                SsPrintf("SsSemStkEnterZeroTimeout:num: %d\n", num);
                ss_error;
            }
            cs->cs_calls[cs->cs_ncalls++] = num;
        }
}

void SsSemStkEnter(ss_semnum_t num)
{
        callstack_t* cs;

        if (ss_debug_disablesemstk) {
            return;
        }

        CALLSTACK(cs);
        CHK_THRID;
        ss_assert(num != SS_SEMNUM_NOTUSED);
        SsSemStkEnterCheck(num);
        cs->cs_calls[cs->cs_ncalls++] = num;
}

void SsSemStkExit(ss_semnum_t num)
{
        int i;
        callstack_t* cs;

        if (ss_debug_disablesemstk) {
            return;
        }

        CALLSTACK(cs);
        ss_assert(cs->cs_ncalls > 0);
        ss_assert(num != SS_SEMNUM_NOTUSED);

        for (i = cs->cs_ncalls - 1; i >= 0; i--) {
            if (cs->cs_calls[i] == num) {
                cs->cs_calls[i] = SS_SEMNUM_NOTUSED;
                break;
            }
        }
        ss_assert(i >= 0);
        for (i = cs->cs_ncalls - 1; i >= 0; i--) {
            if (cs->cs_calls[i] == SS_SEMNUM_NOTUSED) {
                cs->cs_ncalls--;
            } else {
                break;
            }
        }
}

bool SsSemStkFind(ss_semnum_t num)
{
        callstack_t* cs;

        if (ss_debug_disablesemstk) {
            return(TRUE);
        }

        CALLSTACK(cs);
        CHK_THRID;
        ss_assert(num != SS_SEMNUM_NOTUSED);

        if (cs->cs_ncalls > 0) {
            int i;
            for (i = cs->cs_ncalls - 1; i >= 0; i--) {
                if (cs->cs_calls[i] == num) {
                    return(TRUE);
                }
            }
        }
        return(FALSE);
}

bool SsSemStkNotFound(ss_semnum_t num)
{
        if (ss_debug_disablesemstk) {
            return(TRUE);
        }

        return(!SsSemStkFind(num));
}

int SsSemStkNCalls(void)
{
        callstack_t* cs;

        if (ss_debug_disablesemstk) {
            return(0);
        }

        CALLSTACK(cs);
        CHK_THRID;

        return(cs->cs_ncalls > 0);
}

#else /* TRACE_CALLSTACK */

#if defined(SS_SEMSTK_DBG) && !defined(SsSemStkEnter)
/* DEBUG version, where semtrace cannot be used.
* Also, the functions are NOT defined empty in sssem.h.
* This is true at least in portssdl, WNT
*/
void SsSemStkEnterCheck(ss_semnum_t num)
{
        SS_NOTUSED(num);
}
void SsSemStkEnter(ss_semnum_t num)
{
        SS_NOTUSED(num);
}
void SsSemStkEnterZeroTimeout(ss_semnum_t num)
{
        SS_NOTUSED(num);
}
void SsSemStkExit(ss_semnum_t num)
{
        SS_NOTUSED(num);
}
bool SsSemStkFind(ss_semnum_t num)
{
        return(TRUE);
}
bool SsSemStkNotFound(ss_semnum_t num)
{
        return(TRUE);
}
void SsSemStkPrint(void)
{
}

int SsSemStkNCalls(void)
{
        return(0);
}
#endif


#endif /* TRACE_CALLSTACK */

#endif /* SS_SEMSTK_DBG */
