/*************************************************************************\
**  source       * ssmsglog.c
**  directory    * ss
**  description  * Log file routines for messages logging.
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

#define SS_INCLUDE_VARARGFUNS

#include "ssenv.h"

#include <sys/types.h>
#include <sys/stat.h>

#define INCL_DOSPROCESS
#include "sswindow.h"

#include "ssstring.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstdarg.h"
#include "sstime.h"

#include "ssc.h"
#include "sssprint.h"
#include "ssmem.h"
#include "sssem.h"
#include "ssdebug.h"
#include "ssfile.h"
#include "sstime.h"
#include "sssysres.h"
#include "ssmsglog.h"
#define MSGLOG_BAKFNAMEEXT  ".bak"

#define MSGLOG_ISACTIVE(ml) (!ss_msglog_disable && (ml) != NULL && (ml)->ml_active == 0)

struct SsMsgLogStruct {
        SS_FILE*   ml_fp;
        char*   ml_filename;
        long    ml_loglimit;
        int     ml_counter;
        SsSemT* ml_sem;
        int     ml_nlink;
        char*   ml_buf;
        char*   ml_filebuf;
        int     ml_filebufused;
        int     ml_active;
        void*   ml_sysres;
        int     ml_forcesplit_once;
};

static SsMsgLogT* default_trace_log;
static long       default_trace_logsize = 1000000;

#ifndef SS_MYSQL
static void LogfileRename(char* old, char* new);
#endif

static void MsgLogSwap(SsMsgLogT* ml);
static void MsgLogSwapIf(SsMsgLogT* ml, bool forcecheck);
static bool ss_msglog_disable = FALSE;
static int ss_msglog_time_fraction_precision = 0;
static int ss_msglog_forcesplit_once = 0; /* If this is bigger than ml_forcesplit_once 
                                             then file is split. */

bool msglog_diskless = FALSE; /* for diskless, no output file */
bool disable_output = FALSE; /* for diskless, no output file */

/*#***********************************************************************\
 *
 *		msglog_sysres_fclose
 *
 *
 *
 * Parameters :
 *
 *	fp -
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
static void msglog_sysres_fclose(void* fp)
{
        SsFClose(fp);
}

/*#***********************************************************************\
 *
 *		MsgFileOpen
 *
 *
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	mode -
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
static void MsgFileOpen(SsMsgLogT* ml, char* mode)
{
        ml->ml_fp = SsFOpenT(ml->ml_filename, mode);
        if (ml->ml_fp != NULL) {
            ml->ml_sysres = SsSysResAdd(msglog_sysres_fclose, ml->ml_fp);
        }
}

/*#***********************************************************************\
 *
 *		MsgFileClose
 *
 *
 *
 * Parameters :
 *
 *	ml -
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
static void MsgFileClose(SsMsgLogT* ml)
{
        if (ml->ml_fp != NULL) {
            SsSysResRemove(ml->ml_sysres);
            ml->ml_sysres = NULL;
            SsFClose(ml->ml_fp);
            ml->ml_fp = NULL;
        }
}

/*#***********************************************************************\
 *
 *		MsgFileIsNull
 *
 *
 *
 * Parameters :
 *
 *	ml -
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
static bool MsgFileIsNull(SsMsgLogT* ml)
{
        return(ml->ml_fp == NULL);
}

/*#***********************************************************************\
 *
 *		MsgFileFlush
 *
 *
 *
 * Parameters :
 *
 *	ml -
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
static void MsgFileFlush(SsMsgLogT* ml)
{
    SsFFlush(ml->ml_fp);
}

/*#***********************************************************************\
 *
 *		MsgFileSize
 *
 *
 *
 * Parameters :
 *
 *	ml -
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
static long MsgFileSize(SsMsgLogT* ml)
{
        int handle;
        struct stat st;

        handle = fileno(ml->ml_fp);

        if (fstat(handle, &st) == -1) {
            return(-1L);
        } else {
            return(st.st_size);
        }
}

/*#***********************************************************************\
 *
 *		MsgLogRename
 *
 *
 *
 * Parameters :
 *
 *	old -
 *
 *
 *	new -
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
static void MsgLogRename(char* old, char* new)
{
        SsFRemove(new);
        SsFRename(old, new);
}

/*#**********************************************************************\
 *
 *		MsgLogSwap
 *
 * Does real logfile swapping by deleting old log bakfile and renaming
 * current logfile as bakfile.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Return codes from system functions are not checked.
 *
 * Globals used :
 */
static void MsgLogSwap(ml)
        SsMsgLogT* ml;
{
        char bakfname[255];
        char* lastdot;

        ml->ml_forcesplit_once = ss_msglog_forcesplit_once;

        MsgFileClose(ml);

        strcpy(bakfname, ml->ml_filename);
        lastdot = bakfname + strlen(bakfname);
        for ( ;
                !(*lastdot == '.' || *lastdot == '\\' || *lastdot == '/' ||
                *lastdot == ':' || lastdot == bakfname);
            )
            lastdot--;
        if (*lastdot == '.') {
            strcpy(lastdot, MSGLOG_BAKFNAMEEXT);
        } else {
            strcat(bakfname, MSGLOG_BAKFNAMEEXT);
        }
        MsgLogRename(ml->ml_filename, bakfname);
}

/*#**********************************************************************\
 *
 *		MsgLogSwapIf
 *
 * Checks if log file is in use and if it must be swapped. Log file is
 * swapped when it grows too large.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void MsgLogSwapIf(SsMsgLogT* ml, bool forcecheck)
{
        bool forcesplit;

        forcesplit = (ml->ml_forcesplit_once < ss_msglog_forcesplit_once);

        if (ml->ml_counter++ >= 100 || forcecheck || forcesplit) {
            /* Size asking takes time, so don't do it every time. */
            ml->ml_counter = 0;
            if (ml->ml_loglimit || forcesplit) {
                if (ml->ml_fp == NULL || MsgFileSize(ml) >= ml->ml_loglimit || forcesplit) {
                    MsgLogSwap(ml);
                    MsgFileOpen(ml, (char *)"w");
                    ss_dassert(ml->ml_forcesplit_once == ss_msglog_forcesplit_once);
                }
            }
        }
}

/*#***********************************************************************\
 *
 *		MsgLogPutStr
 *
 * Static version, assumes that we have entered ml_sem
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	message -
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
static void MsgLogPutStr(SsMsgLogT* ml, char* message)
{
        ss_dassert(ml != NULL);
        MsgLogSwapIf(ml, FALSE);

        SsFPuts(message, ml->ml_fp);
}

/*#***********************************************************************\
 *
 *		MsgLogVfprintf
 *
 * Static version, assumes that we have entered ml_sem
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	message -
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
static void MsgLogVfprintf(SsMsgLogT* ml, char* format, va_list ap)
{
        ss_dassert(ml != NULL);
        MsgLogSwapIf(ml, FALSE);

        SsVfprintf(ml->ml_fp, format, ap);
}

/*##**********************************************************************\
 *
 *		SsMsgLogPutStr
 *
 *
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	message -
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
void SsMsgLogPutStr(ml, message)
        SsMsgLogT* ml;
    char *message;
{
        if (MSGLOG_ISACTIVE(ml)) {
            SsSemEnter(ml->ml_sem);
            MsgLogPutStr(ml, message);
            SsSemExit(ml->ml_sem);
        }
}

/*#***********************************************************************\
 *
 *		MsgLogVPrintfWithTimeIf
 *
 * Local message print function. Can optionally print date and time before
 * each message.
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	timep -
 *
 *
 *	message -
 *
 *
 *	argptr -
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
static void MsgLogVPrintfWithTimeIf(
        SsMsgLogT* ml,
        bool timep,
        char *message,
        va_list argptr)
{
        if (MSGLOG_ISACTIVE(ml)) {
            char* p;

            SsSemEnter(ml->ml_sem);

            if (ml->ml_buf == NULL) {
                ml->ml_buf = SsQmemAlloc(SS_MSGLOG_BUFSIZE);
                memset(ml->ml_buf, '\0', SS_MSGLOG_BUFSIZE);
            }

            MsgLogSwapIf(ml, FALSE);

            p = ml->ml_buf;

            if (timep) {
				if (ss_msglog_time_fraction_precision == 0) { 
                   SsPrintDateTime(p, SS_MSGLOG_BUFSIZE, SsTime(NULL));
				} else {
                   SsPrintDateTime2(p, SS_MSGLOG_BUFSIZE, ss_msglog_time_fraction_precision);
				}
                p += strlen(p);
                strcpy(p, " ");
                p++;
            }

            /*
             * vsprintf was replaced with vsnprintf by mikko 6.5.2003
             * (buffer was sometimes overrun).
             *
             * hopefully this works on all platforms.
             * 
             * vsnprintf and vsprintf were replaced with vfprintf by matti 2.8.2006
             * so that long sql stmts can be written to trace file without using static 
             * temp buffers here
             */
#if REMOVED_BY_MATTI
#ifdef SS_LINUX
            vsnprintf(
                p,
                SS_MSGLOG_BUFSIZE - (p - ml->ml_buf) - 1,
                message,
                argptr);
#else
            vsprintf(
                p,
                message,
                argptr);
#endif
#endif
            if (ml->ml_buf[SS_MSGLOG_BUFSIZE-1] != '\0') {
                SsSemExit(ml->ml_sem);
                ss_error;
            }

            MsgLogPutStr(ml, ml->ml_buf);
            MsgLogVfprintf(ml, message, argptr);

            SsSemExit(ml->ml_sem);
        }
}

/*##*********************************************************************\
 *
 *		SsMsgLogVPrintf
 *
 * Logs message into log file.
 *
 * Parameters :
 *
 *	message - in, use
 *		message in printf-style format
 *
 *	argptr - in, use
 *		pointer to the argument list
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Very long messages do not work with WSWIN because of
 *      static buffer.
 *
 * Globals used :
 */
void SsMsgLogVPrintf(SsMsgLogT* ml, char *message, va_list argptr)
{
        MsgLogVPrintfWithTimeIf(ml, FALSE, message, argptr);
}

/*##*********************************************************************\
 *
 *		SsMsgLogVPrintfWithTime
 *
 * Logs message into log file with date and time.
 *
 * Parameters :
 *
 *	message - in, use
 *		message in printf-style format
 *
 *	argptr - in, use
 *		pointer to the argument list
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Very long messages do not work with WSWIN because of
 *      static buffer.
 *
 * Globals used :
 */
void SsMsgLogVPrintfWithTime(SsMsgLogT* ml, char *message, va_list argptr)
{
        MsgLogVPrintfWithTimeIf(ml, TRUE, message, argptr);
}

/*##**********************************************************************\
 *
 *		SsMsgLogPrintf
 *
 *
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
void SS_CDECL SsMsgLogPrintf(SsMsgLogT* ml, char* format, ...)
{
        if (MSGLOG_ISACTIVE(ml)) {
            va_list ap;

            va_start(ap, format);
            MsgLogVPrintfWithTimeIf(ml, FALSE, format, ap);
            va_end(ap);
        }
}

/*##**********************************************************************\
 *
 *		SsMsgLogPrintfWithTime
 *
 * Prints message to message log with date and time.
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
void SS_CDECL SsMsgLogPrintfWithTime(SsMsgLogT* ml, char* format, ...)
{
        if (MSGLOG_ISACTIVE(ml)) {
            va_list ap;

            va_start(ap, format);
            MsgLogVPrintfWithTimeIf(ml, TRUE, format, ap);
            va_end(ap);
        }
}

/*##**********************************************************************\
 *
 *		SsMsgLogInitDefaultTrace
 *
 * Initializes the default trace log file. The same file may be shared with
 * several trace outputs.
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
SsMsgLogT* SsMsgLogInitDefaultTrace(void)
{
        SsMsgLogT* log;

        SsSemEnter(ss_msglog_sem);

        if (default_trace_log != NULL) {
            SsMsgLogLink(default_trace_log);
            log = default_trace_log;
        } else {
            default_trace_log = SsMsgLogInit((char *)"soltrace.out", default_trace_logsize);
            log = default_trace_log;
        }

        SsSemExit(ss_msglog_sem);
        return(log);
}

/*##**********************************************************************\
 *
 *		SsMsgLogGiveDefaultTrace
 *
 * Returns default trace log object, if one is created. Otherwise
 * returns NULL.
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
SsMsgLogT* SsMsgLogGiveDefaultTrace(void)
{
        SsMsgLogT* log;

        SsSemEnter(ss_msglog_sem);

        if (default_trace_log != NULL) {
            SsMsgLogLink(default_trace_log);
            log = default_trace_log;
        } else {
            log = NULL;
        }

        SsSemExit(ss_msglog_sem);
        return(log);
}

/*##**********************************************************************\
 *
 *		SsMsgLogSetDefaultTraceSize
 *
 * Sets max size for default trace file.
 *
 * Parameters :
 *
 *	size -
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
void SsMsgLogSetDefaultTraceSize(long size)
{
        SsSemEnter(ss_msglog_sem);

        default_trace_logsize = size;

        if (default_trace_log != NULL) {
            default_trace_log->ml_loglimit = size;
        }

        SsSemExit(ss_msglog_sem);
}

/*##**********************************************************************\
 *
 *		SsMsgLogSetTraceSecDecimals
 *
 * Sets TraceSecDecimals.
 *
 * Parameters :
 *
 *	antoni -
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
void SsMsgLogSetTraceSecDecimals(long tracesecdecimals)
{
        SsSemEnter(ss_msglog_sem);

        ss_msglog_time_fraction_precision = tracesecdecimals;

        SsSemExit(ss_msglog_sem);
}

/*##*********************************************************************\
 *
 *		SsMsgLogInitForce
 *
 * Opens and initializes log file.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SsMsgLogT* SsMsgLogInitForce(char* filename, long loglimit, bool forcep)
{
        SsMsgLogT* ml;
        SsTimeT nowtime;
        char*  nowtext;
        char ctimebuf[SS_CTIME_BUFLEN];
        char*  separatortext = (char *)"\n------------------------------------------------------------\n";


        /* do not create any output file if disable_output is true. */
        if (!msglog_diskless && disable_output) {
            return NULL;
        }
        /* for diskless, do not create any output file. */
        if (msglog_diskless && !forcep) {
                ss_dassert(!forcep);
            return NULL;
        }

        ml = SsQmemAlloc(sizeof(SsMsgLogT));

        ml->ml_filename = SsQmemStrdup(filename);
        ml->ml_loglimit = loglimit;
        ml->ml_counter = 0;
        ml->ml_nlink = 1;
        ml->ml_buf = NULL;
        ml->ml_filebuf = NULL;
        ml->ml_filebufused = 0;
        ml->ml_active = 0;
        ml->ml_sysres = NULL;
        ml->ml_forcesplit_once = ss_msglog_forcesplit_once;

        MsgFileOpen(ml, (char *)"a+");

        if (MsgFileIsNull(ml)) {
            ss_dassert(!forcep);
            SsQmemFree(ml->ml_filename);
            SsQmemFree(ml);
            return(NULL);
        }

        MsgLogSwapIf(ml, TRUE);

        if (MsgFileIsNull(ml)) {
            ss_dassert(!forcep);
            SsQmemFree(ml->ml_filename);
            SsQmemFree(ml);
            return(NULL);
        }

        ml->ml_sem = SsQmemAlloc(SsSemSizeLocal());
        SsSemCreateLocalBuf(ml->ml_sem, SS_SEMNUM_SS_MSGLOG_OBJ);

        SsTime(&nowtime);
        nowtext = SsCtime(&nowtime, ctimebuf, SS_CTIME_BUFLEN);
        MsgLogPutStr(ml, separatortext);
        MsgLogPutStr(ml, nowtext);

        SsMsgLogPrintf(ml, (char *)"Version: %s\nOperating system: %s\n",
            SS_SERVER_VERSION,
            SsEnvNameCurr());
        if (ss_licensetext != NULL) {
            SsMsgLogPrintf(ml, (char *)"%s", ss_licensetext);
        }

        return(ml);
}

/*##*********************************************************************\
 *
 *		SsMsgLogInit
 *
 * Opens and initializes log file.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SsMsgLogT* SsMsgLogInit(char* filename, long loglimit)
{
        return(SsMsgLogInitForce(filename, loglimit, FALSE));
}

/*##**********************************************************************\
 *
 *		SsMsgLogLink
 *
 * Increments message log usage counter.
 *
 * Parameters :
 *
 *	ml -
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
SsMsgLogT* SsMsgLogLink(SsMsgLogT* ml)
{
        if (ml != NULL) {
            SsSemEnter(ml->ml_sem);
            ml->ml_nlink++;
            SsSemExit(ml->ml_sem);
        }
        return(ml);
}

/*##*********************************************************************\
 *
 *		SsMsgLogDone
 *
 * Closes log file.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMsgLogDone(SsMsgLogT* ml)
{
        if (ml != NULL) {

            SsSemEnter(ss_msglog_sem);

            SsSemEnter(ml->ml_sem);
            ml->ml_nlink--;
            if (ml->ml_nlink > 0) {
                SsSemExit(ml->ml_sem);
                SsSemExit(ss_msglog_sem);
                return;
            }

            if (ml == default_trace_log) {
                /* Last list removed from default_trace_log.
                 */
                default_trace_log = NULL;
            }
            SsSemExit(ml->ml_sem);

            SsSemExit(ss_msglog_sem);

            SsMsgLogPutStr(ml, (char *)"\n");
            MsgFileClose(ml);
            SsQmemFree(ml->ml_filename);
            SsSemFreeBuf(ml->ml_sem);
            SsQmemFree(ml->ml_sem);
            if (ml->ml_buf) {
                SsQmemFree(ml->ml_buf);
            }
            if (ml->ml_filebuf) {
                SsQmemFree(ml->ml_filebuf);
            }
            SsQmemFree(ml);
        }
}

/*##*********************************************************************\
 *
 *		SsMsgLogFlush
 *
 * Closes log file.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsMsgLogFlush(SsMsgLogT* ml)
{
        if (ml != NULL) {
            SsSemEnter(ml->ml_sem);
            MsgFileFlush(ml);
            SsSemExit(ml->ml_sem);
        }
}

/*##**********************************************************************\
 *
 *		SsMsgLogDisable
 *
 * Disables all output through all msglog objects.
 *
 * Parameters :
 *
 *	none
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsMsgLogDisable(void)
{
        SsSemEnter(ss_msglog_sem);
        ss_msglog_disable = TRUE;
        SsSemExit(ss_msglog_sem);
}

/*##**********************************************************************\
 *
 *		SsMsgLogGetFileName
 *
 * Returns message log file name.
 *
 * Parameters :
 *
 *	ml -
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
char* SsMsgLogGetFileName(SsMsgLogT* ml)
{
        if (ml == NULL) {
            return((char *)"Non-existent");
        } else {
            return(ml->ml_filename);
        }
}

/*##**********************************************************************\
 *
 *		SsMsgLogSetLimit
 *
 * Sets a new message log max size limit.
 *
 * Parameters :
 *
 *	ml -
 *
 *
 *	loglimit -
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
void SsMsgLogSetLimit(SsMsgLogT* ml, long loglimit)
{
        if (ml != NULL) {
            ml->ml_loglimit = loglimit;
        }
}

/*##**********************************************************************\
 *
 *		SsMsgLogSetForceSplitOnce
 *
 *
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
void SsMsgLogSetForceSplitOnce(void)
{
        ss_msglog_forcesplit_once++;
}

/*##**********************************************************************\
 *
 *		SsMsgLogGlobalInit
 *
 *
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
void SsMsgLogGlobalInit(void)
{
        SsSemCreateLocalBuf(ss_msglog_sem, SS_SEMNUM_SS_MSGLOG_GLOBAL);
        ss_msglog_disable = FALSE;
}
