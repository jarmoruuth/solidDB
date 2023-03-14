/*************************************************************************\
**  source       * su0usrid.c
**  directory    * su
**  description  * Server user id generation routines.
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

#include <ssstdarg.h>
#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <sssem.h>
#include <ssmem.h>
#include <ssmsglog.h>
#include <sssprint.h>

#include "su0parr.h"
#include "su0usrid.h"

#define SS_USRID_DBG_MAXUSERS   22000


typedef struct {
        int         ui_nlink;
        su_pa_t*    ui_activetrace;
        int         ui_tracepos;
} su_usrid_t;


typedef struct {
        char* ut_name;
        char* ut_info;
        long  ut_id;
} su_usrid_activetrace_t;

su_bflag_t  su_usrid_traceflags;
int         su_usrid_tracelevel;

bool        su_usrid_usertaskqueue;

static char*        usrid_tracebuf;
static SsMsgLogT*   usrid_tracelog;

static bool     usrid_initp = FALSE;
static SsSemT*  usrid_sem;
static su_pa_t* usrid_supa;

static void activetrace_done(su_pa_t* tracelist)
{
        int i;
        su_usrid_activetrace_t* ut;

        su_pa_do_get(tracelist, i, ut) {
            SsMemFree(ut);
        }
        su_pa_done(tracelist);
}

int su_usrid_init(void)
{
        int id = -1;

        if (usrid_initp) {
            su_usrid_t* ui;

            ui = SSMEM_NEW(su_usrid_t);
            ui->ui_nlink = 1;
            ui->ui_activetrace = NULL;
            ui->ui_tracepos = 0;

            SsSemEnter(usrid_sem);

            id = su_pa_insert(usrid_supa, ui);
            ss_dassert(id != 0);
            ss_rc_dassert(id < SS_USRID_DBG_MAXUSERS, id);

            ss_dprintf_1(("id=%d, callstack:\n", id));
            ss_output_1(if (id > SS_USRID_DBG_MAXUSERS - 1000) SsMemTrcPrintCallStk(NULL));

            SsSemExit(usrid_sem);
        }

        ss_dprintf_1(("su_usrid_init:id=%d\n", id));

        return(id);
}

void su_usrid_link(int usrid)
{
        if (usrid != -1 && usrid_initp) {
            su_usrid_t* ui;

            ss_dprintf_1(("su_usrid_link:id=%d\n", usrid));
            ss_dassert(usrid_initp);

            SsSemEnter(usrid_sem);

            ss_dassert(su_pa_indexinuse(usrid_supa, usrid));

            ui = su_pa_getdata(usrid_supa, usrid);
            ss_dassert(ui->ui_nlink > 0);
            ui->ui_nlink++;

            SsSemExit(usrid_sem);
        }
}

void su_usrid_done(int usrid)
{
        ss_dassert(usrid != 0);

        if (usrid != -1 && usrid_initp) {
            su_usrid_t* ui;

            ss_dprintf_1(("su_usrid_done:id=%d\n", usrid));
            ss_dassert(usrid_initp);

            SsSemEnter(usrid_sem);

            ss_dassert(su_pa_indexinuse(usrid_supa, usrid));

            ui = su_pa_getdata(usrid_supa, usrid);
            ss_dassert(ui->ui_nlink > 0);
            ui->ui_nlink--;
            if (ui->ui_nlink == 0) {
                su_pa_remove(usrid_supa, usrid);
                if (ui->ui_activetrace != NULL) {
                    activetrace_done(ui->ui_activetrace);
                }
                SsMemFree(ui);
            }

            SsSemExit(usrid_sem);
        }
}

void su_usrid_traceon(void)
{
        su_usrid_tracelevel = 1;
}

void su_usrid_traceoff(void)
{
        int i;
        su_usrid_t* ui;

        SsSemEnter(usrid_sem);

        su_usrid_tracelevel = 0;

        su_pa_do_get(usrid_supa, i, ui) {
            if (i > 0) {
                ui->ui_tracepos = 0;
            }
        }

        SsSemExit(usrid_sem);
}

SsMsgLogT* su_usrid_gettracelog(void)
{
        SsSemEnter(usrid_sem);
        if (usrid_tracelog == NULL) {
            usrid_tracelog = SsMsgLogInitDefaultTrace();
        }
        SsSemExit(usrid_sem);
        return(usrid_tracelog);
}

bool su_usrid_istrace(
        su_usrid_trace_t type)
{
        return(su_usrid_tracelevel > 0 && (su_usrid_traceflags & type));
}

void SS_CDECL su_usrid_trace(
        int usrid,
        su_usrid_trace_t type,
        int level,
        char* format,
        ...)
{
        if ((su_usrid_traceflags & type) && level >= su_usrid_tracelevel) {
            va_list ap;
            char tmpbuf[512];
            su_usrid_t* ui;

            SsSemEnter(usrid_sem);

            if (usrid_tracebuf == NULL) {
                usrid_tracebuf = SsMemAlloc(4000);
            }
            if (usrid_tracelog == NULL) {
                usrid_tracelog = SsMsgLogInitDefaultTrace();
                if (usrid_tracelog == NULL) {
                    /* Can return NULL e.g. in diskless or if wrong pathprefix. */
                    SsSemExit(usrid_sem);
                    return;
                }
            }
            SsSprintf(usrid_tracebuf, "%d:", usrid);
            switch (type) {
                case SU_USRID_TRACE_SQL:
                    strcat(usrid_tracebuf, "sql:");
                    break;
                case SU_USRID_TRACE_RPC:
                    strcat(usrid_tracebuf, "rpc:");
                    break;
                case SU_USRID_TRACE_SNC:
                    strcat(usrid_tracebuf, "flow:");
                    break;
                case SU_USRID_TRACE_SNCPLANS:
                    strcat(usrid_tracebuf, "flow plans:");
                    break;
                case SU_USRID_TRACE_REXEC:
                    strcat(usrid_tracebuf, "rexec:");
                    break;
                case SU_USRID_TRACE_BATCH:
                    strcat(usrid_tracebuf, "batch:");
                    break;
                case SU_USRID_TRACE_EST:
                case SU_USRID_TRACE_ESTINFO:
                    strcat(usrid_tracebuf, "est:");
                    if (su_pa_indexinuse(usrid_supa, usrid)) {
                        ui = su_pa_getdata(usrid_supa, usrid);
                        if (ui->ui_activetrace != NULL && ui->ui_tracepos > 0) {
                            su_usrid_activetrace_t* ut;
                            ut = su_pa_getdata(ui->ui_activetrace, ui->ui_tracepos - 1);
                            SsSprintf(tmpbuf, "%ld:", ut->ut_id);
                            strcat(usrid_tracebuf, tmpbuf);
                            if (type & SU_USRID_TRACE_ESTINFO && ut->ut_info != NULL) {
                                SsSprintf(tmpbuf, "%.500s:", ut->ut_info);
                                strcat(usrid_tracebuf, tmpbuf);
                            }
                        }
                    }
                    break;
            }
            strcat(usrid_tracebuf, format);
            strcat(usrid_tracebuf, "\n");
            va_start(ap, format);
            SsMsgLogVPrintfWithTime(usrid_tracelog, usrid_tracebuf, ap);
            SsMsgLogFlush(usrid_tracelog);
            va_end(ap);

            SsSemExit(usrid_sem);
        }
}

void su_usrid_trace_push_fun(
        int usrid,
        char* name,
        char* info,
        long id)
{
        ss_dassert(su_usrid_tracelevel > 0);
        
        if (TRUE || su_usrid_tracelevel > 0) {
            su_usrid_t* ui;
            su_usrid_activetrace_t* ut;

            SsSemEnter(usrid_sem);

            if (su_usrid_tracelevel > 0 && su_pa_indexinuse(usrid_supa, usrid)) {

                ui = su_pa_getdata(usrid_supa, usrid);

                if (ui->ui_activetrace == NULL) {
                    ui->ui_activetrace = su_pa_init();
                }
                if (su_pa_indexinuse(ui->ui_activetrace, ui->ui_tracepos)) {
                    ut = su_pa_getdata(ui->ui_activetrace, ui->ui_tracepos);
                } else {
                    ut = SSMEM_NEW(su_usrid_activetrace_t);
                    su_pa_insertat(ui->ui_activetrace, ui->ui_tracepos, ut);
                    ss_dassert(su_pa_getdata(ui->ui_activetrace, ui->ui_tracepos) == ut);
                }

                ut->ut_name = name;
                ut->ut_info = info;
                ut->ut_id = id;

                ss_dassert(ui->ui_tracepos < 100);

                ui->ui_tracepos++;
            }

            SsSemExit(usrid_sem);
        }
}

void su_usrid_trace_pop_fun(
        int usrid)
{
        ss_dassert(su_usrid_tracelevel > 0);
        
        if (TRUE || su_usrid_tracelevel > 0) {
            su_usrid_t* ui;

            SsSemEnter(usrid_sem);

            if (su_pa_indexinuse(usrid_supa, usrid)) {

                ui = su_pa_getdata(usrid_supa, usrid);

                if (ui->ui_tracepos > 0) {
                    ui->ui_tracepos--;
                }
            }

            SsSemExit(usrid_sem);
        }
}

static void tracelist_done(void* data)
{
        SsMemFree(data);
}

su_list_t* su_usrid_trace_activelist(
        void)
{
        int i;
        int j;
        su_usrid_t* ui;
        su_usrid_activetrace_t* ut;
        su_list_t* list;

        list = su_list_init(tracelist_done);

        SsSemEnter(usrid_sem);

        if (usrid_tracebuf == NULL) {
            usrid_tracebuf = SsMemAlloc(4000);
        }

        su_pa_do_get(usrid_supa, i, ui) {
            if (i > 0 && ui->ui_activetrace != NULL) {
                su_pa_do_get(ui->ui_activetrace, j, ut) {
                    ss_dassert(j < 100);
                    if (j >= ui->ui_tracepos) {
                        break;
                    }
                    if (j == 0) {
                        SsSprintf(usrid_tracebuf, "--- USER %d ---", i);
                        su_list_insertlast(list, SsMemStrdup(usrid_tracebuf));
                    }
                    SsSprintf(usrid_tracebuf, "%d:%d:%.80s:%.1024s", i, j, ut->ut_name, ut->ut_info);
                    su_list_insertlast(list, SsMemStrdup(usrid_tracebuf));
                    ss_dassert(su_list_length(list) < 100);
                }
            }
        }

        SsSemExit(usrid_sem);

        return(list);
}

bool su_usrid_settracetype(char* tracetype)
{
        if (SsStricmp(tracetype, "SQL") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_SQL;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "RPC") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_RPC;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Sync") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_SNC;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "SNC") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_SNC;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Flow") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_SNC;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "FlowPlans") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_SNCPLANS;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Rexec") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_REXEC;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Batch") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_BATCH;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "est") == 0) {
            su_usrid_traceflags |= SU_USRID_TRACE_EST;
            return(TRUE);
        }
        if (SsStricmp(tracetype, "All") == 0) {
            su_usrid_traceflags =  ~SU_USRID_TRACE_SNCPLANS;
            return(TRUE);
        }
        return(FALSE);
}

bool su_usrid_unsettracetype(char* tracetype)
{
        if (SsStricmp(tracetype, "SQL") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_SQL);
            return (TRUE);
        }
        if (SsStricmp(tracetype, "RPC") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_RPC);
            return (TRUE);
        }
        if (SsStricmp(tracetype, "Sync") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_SNC);
            return (TRUE);
        }
        if (SsStricmp(tracetype, "SNC") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_SNC);
            return (TRUE);
        }
        if (SsStricmp(tracetype, "Flow") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_SNC);
            return(TRUE);
        }
        if (SsStricmp(tracetype, "FlowPlans") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_SNCPLANS);
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Rexec") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_REXEC);
            return(TRUE);
        }
        if (SsStricmp(tracetype, "Batch") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_BATCH);
            return(TRUE);
        }
        if (SsStricmp(tracetype, "est") == 0) {
            SU_BFLAG_CLEAR(su_usrid_traceflags, SU_USRID_TRACE_EST);
            return(TRUE);
        }
        if (SsStricmp(tracetype, "All") == 0) {
            su_usrid_traceflags =  0;
            return(TRUE);
        }
        return (FALSE);
}

void su_usrid_globalinit(void)
{
        if (!usrid_initp) {
            usrid_supa = su_pa_init();
            ss_autotest(su_pa_setmaxlen(usrid_supa, 30000));
            su_pa_setrecyclecount(usrid_supa, 10);
            usrid_sem = SsSemCreateLocal(SS_SEMNUM_SU_USRID);
            usrid_tracebuf = NULL;
            usrid_initp = TRUE;

            /* MME requires userid's to be != 0, reserve 0. */
            su_pa_insertat(usrid_supa, 0, usrid_supa);
            ss_dassert(su_pa_getdata(usrid_supa, 0) == usrid_supa);
        }
}

void su_usrid_globaldone(void)
{
        ss_dassert(usrid_initp);

        su_pa_done(usrid_supa);
        SsSemFree(usrid_sem);
        usrid_initp = FALSE;
        if (usrid_tracebuf != NULL) {
            SsMemFree(usrid_tracebuf);
            usrid_tracebuf = NULL;
        }
        if (usrid_tracelog != NULL) {
            SsMsgLogDone(usrid_tracelog);
        }
}
