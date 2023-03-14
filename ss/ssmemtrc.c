/*************************************************************************\
**  source       * ssmemtrc.c
**  directory    * ss
**  description  * Call stack tracing for memory debugging
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

The implementation is based on memory debugging in "ssmem.c" and the
in-house coverage analyzer tool that makes a temporary source file that
contains a macro call at each function entry and exit point

Limitations:
-----------

The current implementation of the coverage analyzer does not handle return
statments that contain function call(s) correctly.


Error handling:
--------------


Objects used:
------------

Alloclist system of "ssmem.c"

Preconditions:
-------------

All your source code needs to be preprocessed by the coverage analyzer
using the 'm' (memory tracing) option.


Multithread considerations:
--------------------------

All these routines are called inside mutexed sections, so multithreaded
env. is OK.

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
#include "ssmemtrc.h"
#include "ssmemchk.h"

#define CALLSTK_MAX 200    /* MAX # of nested function calls */

typedef struct cs_fun_st cs_fun_t;

struct cs_fun_st {
        bool                csf_pending_pop;
        char*               csf_funcname;
        char*               csf_filename;
};

typedef struct callstk_st {
        int         cs_ncalls;
        cs_fun_t    cs_functions[CALLSTK_MAX];
        int*        cs_stackptr[CALLSTK_MAX];
        int         cs_lastcall;
        char*       cs_lastcalls[CALLSTK_MAX];
} callstack_t;

#if defined(SS_MT) && !defined(SS_DLLQMEM)

# if defined(SS_NT)

#ifdef SS_USE_WIN32TLS_API

#    define TRACE_CALLSTACK 1
#    define CALLSTACK(cs)   {   cs = SsThrDataGet(SS_THRDATA_CALLSTACK);\
                                if (cs == NULL) {\
                                    cs = calloc(sizeof(callstack_t), 1);\
                                    SsThrDataSet(SS_THRDATA_CALLSTACK, cs);\
                                }\
                            }
#    define CHK_THRID

#else /* SS_USE_WIN32TLS_API */

#    define TRACE_CALLSTACK 1
#    define CALLSTACK(cs)   cs = &callstack
#    define CHK_THRID
     static __declspec(thread) callstack_t          callstack;

#endif /* SS_USE_WIN32TLS_API */

# elif defined(SS_PTHREAD) /* SS_NT */

#    define TRACE_CALLSTACK 1
#    define CALLSTACK(cs)   {   cs = SsThrDataGet(SS_THRDATA_CALLSTACK);\
                                if (cs == NULL) {\
                                    cs = calloc(sizeof(callstack_t), 1);\
                                    SsThrDataSet(SS_THRDATA_CALLSTACK, cs);\
                                }\
                            }
#    define CHK_THRID

# else

#    define TRACE_CALLSTACK 1
#    define MAXTHREAD       32  /* Larger value fails in CSet/2, area >64k */
#    define CALLSTACK(cs)   cs = &callstack[SsThrGetid()]
#    define CHK_THRID       ss_assert(SsThrGetid() < MAXTHREAD)
     static callstack_t callstack[MAXTHREAD];

# endif /* SS_NT */

# else /* SS_MT */

#    define TRACE_CALLSTACK 1
#    define CALLSTACK(cs)   cs = &callstack
#    define CHK_THRID
     static callstack_t callstack;

#endif /* SS_MT && !DSS_DLLQMEM */

bool ss_memtrc_disablecallstack;

#ifdef TRACE_CALLSTACK

static char SsMemPrefix[] = "SsMem";
static char* memtrc_appinfo;

/*##**********************************************************************\
 *
 *		SsMemTrcCopyCallStk
 *
 * Makes a dynamically allocated copy of the call stack.
 *
 * Parameters :
 *
 * Return value - give :
 *      pointer to a dynamically allocated array of char*:s which all point
 *      to a function name. The end of the array is marked with a NULL.
 *      The names are statically allocated, only the array is given to
 *      the caller.
 *
 * Limitations  :
 *
 *      The returned call stack must be released using function
 *      SsMemTrcFreeCallStk.
 *
 * Globals used : callstack
 */
char** SsMemTrcCopyCallStk()
{
#ifdef SS_DLLQMEM
        return (NULL);
#else /* SS_DLLQMEM */
        char** p;
        int fi, pi, flimit, mlimit;
        callstack_t* cs;

        if (ss_memtrc_disablecallstack) {
            return (NULL);
        }
        CALLSTACK(cs);

        flimit = cs->cs_ncalls;
        if (memtrc_appinfo != NULL) {
            mlimit = flimit + 1;
        } else {
            mlimit = flimit;
        }

        ss_dassert(flimit < CALLSTK_MAX);

        p = malloc((mlimit + 1) * sizeof(char*));
        if (p == NULL) {
            return(NULL);
        }
        if (memtrc_appinfo != NULL) {
            p[0] = memtrc_appinfo;
            pi = 1;
        } else {
            pi = 0;
        }
        for (fi = 0; fi < flimit; fi++, pi++) {
            p[pi] = cs->cs_functions[fi].csf_funcname;
        }
        p[pi] = NULL;
        return (p);
#endif /* SS_DLLQMEM */
}

/*##**********************************************************************\
 *
 *		SsMemTrcFreeCallStk
 *
 * Releases call stack returned by SsMemTrcCopyCallStk.
 *
 * Parameters :
 *
 *	callstack - in, take
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
void SsMemTrcFreeCallStk(char** callstk)
{
        if (callstk != NULL) {
            free(callstk);
        }
}

void SsMemTrcGetFunctionStk(char** callstk, int* p_len)
{
#ifdef SS_DLLQMEM
        *callstk = NULL;
        *p_len = 0;
#else /* SS_DLLQMEM */
        int fi, mlimit;
        callstack_t* cs;

        if (ss_memtrc_disablecallstack) {
            *callstk = NULL;
            *p_len = 0;
            return;
        }
        CALLSTACK(cs);

        mlimit = cs->cs_ncalls;
        ss_dassert(mlimit < CALLSTK_MAX);

        for (fi = 0; fi < mlimit; fi++) {
            callstk[fi] = cs->cs_functions[fi].csf_funcname;
        }
        callstk[fi] = NULL;
        *p_len = cs->cs_ncalls;
#endif /* SS_DLLQMEM */
}

/*##**********************************************************************\
 *
 *		SsMemTrcEnterFunction
 *
 * This function is called in the beginning of each function.
 * It puts the function name to the top of call stack.
 *
 * Parameters :
 *
 *	function_name - in, hold
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
int SsMemTrcEnterFunction(char *filename, char *function_name)
{
        int stackptr = 0;
#ifdef SS_DLLQMEM
        return (0);
#else /* SS_DLLQMEM */
        callstack_t* cs;

        if (ss_memtrc_disablecallstack) {
            return(0);
        }
        CALLSTACK(cs);
        CHK_THRID;
        ss_dprintf_1(("SsMemTrcEnterFunction:%d:%s\n", cs->cs_ncalls, function_name));

        if (cs->cs_ncalls >= CALLSTK_MAX) {
            SsDbgPrintf("SsMemTrcEnterFunction() Error: call stack overflow;\nCalls:\n");
            SsMemTrcPrintCallStk(SsMemTrcCopyCallStk());
            ss_error;
        }
        cs->cs_functions[cs->cs_ncalls].csf_funcname = function_name;
        cs->cs_functions[cs->cs_ncalls].csf_pending_pop = FALSE;
        cs->cs_stackptr[cs->cs_ncalls] = &stackptr;
        cs->cs_functions[cs->cs_ncalls].csf_filename = filename;
        cs->cs_ncalls++;

        cs->cs_lastcall++;
        if (cs->cs_lastcall == CALLSTK_MAX) {
            cs->cs_lastcall = 0;
        }
        cs->cs_lastcalls[cs->cs_lastcall] = function_name;

        return(cs->cs_ncalls);
#endif /* SS_DLLQMEM */

}

/*##**********************************************************************\
 *
 *		SsMemTrcExitFunction
 *
 * This function is called at the return point of each function.
 * It pops the function name from the top of the call stack.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
int SsMemTrcExitFunction(char *filename __attribute__ ((unused)), bool pending_pop)
{
        int stackptr __attribute__ ((unused));
        callstack_t* cs;

        stackptr = 0;

#ifdef SS_DLLQMEM
        return (0);
#else /* SS_DLLQMEM */

        if (ss_memtrc_disablecallstack) {
            return(0);
        }
        CALLSTACK(cs);
        ss_assert(cs->cs_ncalls > 0);
        ss_dprintf_1(("SsMemTrcExitFunction:%d:%s\n",
            cs->cs_ncalls-1, cs->cs_functions[cs->cs_ncalls-1].csf_funcname));

        if (pending_pop) {
            cs->cs_functions[cs->cs_ncalls].csf_pending_pop = TRUE;
        } else {
            cs_fun_t lcs;

            do {
                cs->cs_ncalls--;

                lcs = cs->cs_functions[cs->cs_ncalls];

                /*
                 * sanity check that SS_POPNAME is inside same file than
                 * SS_PUSHNAME, should catch some of the problems
                 *
                 */

                ss_dassert(SsStrcmp(lcs.csf_filename, filename) == 0);

                /*
                 * we don't set csf_funcname to NULL here anymore, so we
                 * can print out the entire stack when we
                 * have too many SS_POPNAMEs.
                 *
                 */
                /* lcs.csf_funcname = NULL; */

                lcs.csf_pending_pop = FALSE;

            } while (cs->cs_ncalls > 0 && lcs.csf_pending_pop);
        }
        return(cs->cs_ncalls+1);
#endif /* SS_DLLQMEM */

}

void* SsMemTrcGetCallStk(void)
{
        callstack_t* cs;
        CALLSTACK(cs);
        return(cs);
}

/*##**********************************************************************\
 *
 *		SsMemTrcFprintCallStk
 *
 * Prints call stack contents to debugging output.
 *
 * Parameters :
 *
 *	callstk - in, use
 *		copied call stack (NULL terminated)
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemTrcFprintCallStk(void* fp, char** callstk, void* pcallstack)
{
#ifdef SS_DLLQMEM
        return;
#else /* SS_DLLQMEM */
        if (ss_memtrc_disablecallstack) {
            return;
        }
        SsFprintf(fp, "    Call stack:\n");
        if (callstk != NULL) {
            for (; *callstk != NULL; callstk++) {
                SsFprintf(fp, "    %s\n", *callstk);
                if (strncmp(SsMemPrefix, *callstk, sizeof(SsMemPrefix) - 1) == 0) {
                    break;
                }
            }
        } else {
            char* p;
            int i, limit;
            callstack_t* cs;

            if(pcallstack == NULL) {
                CALLSTACK(cs);
            } else {
                cs = (callstack_t*)pcallstack;
            }

            limit = cs->cs_ncalls;

            if (limit > CALLSTK_MAX) {
                limit = CALLSTK_MAX;
            }

            for (i = 0; i < limit; i++) {
                p = cs->cs_functions[i].csf_funcname;
                SsFprintf(fp, "    %s\n", p);
                if (strncmp(SsMemPrefix, p, sizeof(SsMemPrefix) - 1) == 0) {
                    break;
                }
            }
        }
#endif /* SS_DLLQMEM */
}

/*##**********************************************************************\
 *
 *		SsMemTrcPrintCallStk
 *
 * Prints call stack contents to debugging output.
 *
 * Parameters :
 *
 *	callstk - in, use
 *		copied call stack (NULL terminated)
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMemTrcPrintCallStk(char** callstk)
{
        SsMemTrcFprintCallStk(NULL, callstk, NULL);
}

/*##**********************************************************************\
 *
 *		SsMemTrcGetCallStackHeight
 *
 *
 *
 * Parameters :
 *
 *	callstk -
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
uint SsMemTrcGetCallStackHeight(char** callstk)
{
        if (callstk != NULL) {
            char** cs;
            uint cs_height = 0;
            for (cs = callstk; *cs != NULL; cs++) {
                cs_height++;
            }
            return (cs_height);
        } else {
            callstack_t* cs;

            CALLSTACK(cs);
            return (cs->cs_ncalls);
        }
}

/*##**********************************************************************\
 *
 *		SsMemTrcGetCallStackNth
 *
 *
 *
 * Parameters :
 *
 *	callstk -
 *
 *
 *	nth -
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
char* SsMemTrcGetCallStackNth(char** callstk, uint nth)
{
        if (callstk != NULL) {
            char** cs;
            uint cs_height = 0;
            for (cs = callstk; *cs != NULL; cs++) {
                cs_height++;
            }
            if (nth >= cs_height) {
                return (NULL);
            }
            cs -= nth + 1;
            return (*cs);
        } else {
            cs_fun_t* p;
            int limit;
            callstack_t* cs;

            CALLSTACK(cs);

            limit = cs->cs_ncalls;
            if (nth >= (uint)limit || limit > CALLSTK_MAX) {
                return (NULL);
            }
            p = &cs->cs_functions[limit - 1 - nth];
            ss_assert(p != NULL);
            ss_assert(p->csf_funcname != NULL);
            return (p->csf_funcname);
        }
}

#define APPINFOSIZE 1543

typedef struct {
        int     ai_check;
        char*   ai_appinfo;
        void*   ai_next;
} appinfo_t;

static appinfo_t* appinfotab[APPINFOSIZE];

int ss_memtrc_hashpjw(char* s)
{
        char* p;
        unsigned h = 0;
        unsigned g;

        for (p = s; *p; p++) {
            h = (h << 4) + *p;
            if ((g = h & 0xf0000000)) {
                h = h ^ (g >> 24);
                h = h ^ g;
            }
        }
        return h;
}

/*##**********************************************************************\
 *
 *		SsMemTrcAddAppinfo
 *
 * Adds new appinfo. We keep a hash table of existing appinfos so that
 * duplicates do not keep allocating more and more memory. This should
 * be useful e.g. in autotests where same applications run again and again.
 *
 * Note that we do not have any mutexing here. The caller must make sure
 * all calls are made inside one global mutex.
 *
 * Note also that if multiple applications are running concurrently we do
 * not get correct appinfo because the latest setting is always printed.
 *
 * Parameters :
 *
 *	appinfo -
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
void SsMemTrcAddAppinfo(char* appinfo)
{
        int h;
        appinfo_t* ai;

        if (appinfo == NULL) {
            memtrc_appinfo = (char *)"no appinfo";
            return;
        }

        h = ss_memtrc_hashpjw(appinfo) % APPINFOSIZE;
        ai = appinfotab[h];
        while (ai != NULL) {
            ss_rc_dassert(ai->ai_check == APPINFOSIZE, ai->ai_check);
            ss_dassert(ai->ai_appinfo != NULL);
            if (strcmp(appinfo, ai->ai_appinfo) == 0) {
                break;
            }
            ai = ai->ai_next;
        }
        if (ai == NULL) {
            /* Not found, add a new entry */
            ai = malloc(sizeof(appinfo_t));
            if (ai == NULL) {
                memtrc_appinfo = (char *)"malloc failed";
                return;
            }
            ai->ai_appinfo = strdup(appinfo);
            if (ai->ai_appinfo == NULL) {
                free(ai);
                memtrc_appinfo = (char *)"malloc failed";
                return;
            }
            ai->ai_check = APPINFOSIZE;
            ai->ai_next = appinfotab[h];
            appinfotab[h] = ai;
        }
        memtrc_appinfo = ai->ai_appinfo;
}

#else /* TRACE_CALLSTACK */

char** SsMemTrcCopyCallStk()
{
#ifdef SS_DLLQMEM
        return;
#else /* SS_DLLQMEM */
        char** p;

        if (ss_memtrc_disablecallstack) {
            return;
        }
        p = malloc(sizeof(char*));
        if (p == NULL) {
            return(NULL);
        }
        p[0] = NULL;
        return (p);
#endif /* SS_DLLQMEM */
}

void SsMemTrcFreeCallStk(char** callstk)
{
        if (callstk != NULL) {
            free(callstk);
        }
}

void* SsMemTrcGetCallStk(void)
{
}

void SsMemTrcEnterFunction(char *filename, char *function_name)
{
        SS_NOTUSED(function_name);
}

void SsMemTrcExitFunction(char *filename, bool pending_pop)
{
        SS_NOTUSED(pending_pop);
}

void SsMemTrcPrintCallStk(char** callstk)
{
        SS_NOTUSED(callstk);
}

void SsMemTrcFprintCallStk(void* fp, char** callstk, callstack_t* pcallstack)
{
        SS_NOTUSED(fp);
        SS_NOTUSED(callstk);
        SS_NOTUSED(pcallstack);
}

uint SsMemTrcGetCallStackHeight(char** callstk)
{
        SS_NOTUSED(callstk);
        return (0);
}

char* SsMemTrcGetCallStackNth(char** callstk, uint nth)
{
        SS_NOTUSED(callstk);
        SS_NOTUSED(nth);
        return (NULL);
}

void SsMemTrcAddAppinfo(char* appinfo)
{
}

#endif /* TRACE_CALLSTACK */
