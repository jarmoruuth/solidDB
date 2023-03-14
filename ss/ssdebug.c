/*************************************************************************\
**  source       * ssdebug.c
**  directory    * ss
**  description  * Debugging and assertion functions.
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

Degugging routines. Most debugging routine calls are implemented as
macros. When SS_DEBUG is not defined, these macros expand to an
empty statement.

The debug system can be initialized by calling function SsDbgInit.
It uses the environment variable SS_DEBUG, parses it and initializes
the system accordingly. The syntax of SS_DEBUG environment variable is
explained in the function comment of function SsDbgSet. If SS_DEBUG
environment variable is not set, nothing is done. This initilization
can be done easily at the beginning of function main by using a macro
SS_INIT_DEBUG.

The debug system can controlled also using function SsDbgSet. It takes
a debug settings in the same format as they should be in the SS_DEBUG
environment variable.

Global functions are used to control the current debug level. There
are debug routines to display output on screen and on log file,
several assert macros, conditional debug code execition etc. See header
file ssdebug.h for latest debug routines.

Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

The code is not fully reentrant, but usually it does not matter.

Example:
-------

void main(void)
{
        int i = 1;

        SS_INIT_DEBUG;

        ss_assert(i == 1);
        ss_dassert(i == 1);
        ss_dassert_1(i == 1);

        ss_dprintf_1(("i = %d\n", i));
}


**************************************************************************
#endif /* DOCUMENTATION */

#define SS_INCLUDE_VARARGFUNS

#include "ssenv.h"
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SS_PRINTABLE_ONLY
#    include <ctype.h>
#endif /* SS_PRINTABLE_ONLY */


#define INCL_DOSPROCESS
#include "sswindow.h"

#include "ssstring.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssstdarg.h"
#include "sstime.h"
#include "ssctype.h"
#include "sschcvt.h"
#include "sscputst.h"
#include "sssetjmp.h"
#include "ssmemtrc.h"
#include "sssqltrc.h"
#include "sstimer.h"

#if defined(SS_SCO)
#include <locale.h>
#endif /* SS_SCO */

#include "ssproces.h"

#include "ssdebug.h"
#include "ssc.h"
#include "sssprint.h"
#include "ssthread.h"
#include "ssgetenv.h"
#include "sssem.h"
#include "ssmem.h"
#include "ssmemchk.h"
#include "ssfile.h"
#include "ssmsglog.h"
#include "sstraph.h"
#include "ssnumuti.h"
#include "ssservic.h"
#include "sssysres.h"
#include "sspmon.h"
#include "ssrtcov.h"

#include "ssscan.h"
#include "sstlog.h"

#ifdef SS_MYSQL
#include <su0error.h>
#else
#include "../su/su0error.h"
#endif

#define SS_ERRORLOG_BAKFILENAME "solerror.bak"
#define SS_ERRORLOG_MAXSIZE     (1024L * 1024L)

SsMsgLogT* monitorlog = NULL;
extern bool msglog_diskless; /* for diskless, no output file */
extern bool disable_output; /* for disabling output file generation*/
static bool orig_msglog_diskless = FALSE; /* original value */
static bool orig_disable_output = FALSE; /* original value */
static bool first_time = TRUE;

#ifdef SS_MYSQL_PERFCOUNT
int mysql_enable_perfcount = 0;
__int64 commit_perfcount;
__int64 commit_callcount;
__int64 index_read_idx_perfcount;
__int64 index_read_idx_callcount;
__int64 relcur_create_perfcount;
__int64 relcur_create_callcount;
__int64 relcur_open_perfcount;
__int64 relcur_open_callcount;
__int64 relcur_setconstr_perfcount;
__int64 relcur_setconstr_callcount;
__int64 fetch_perfcount;
__int64 fetch_callcount;
__int64 relcur_next_perfcount;
__int64 relcur_next_callcount;
__int64 tb_pla_create_perfcount;
__int64 tb_pla_create_callcount;
__int64 tb_pla_reset_perfcount;
__int64 tb_pla_reset_callcount;
__int64 dbe_search_init_perfcount;
__int64 dbe_search_init_callcount;
__int64 dbe_search_reset_perfcount;
__int64 dbe_search_reset_callcount;
__int64 dbe_search_reset_fetch_perfcount;
__int64 dbe_search_reset_fetch_callcount;
__int64 dbe_search_nextorprev_perfcount;
__int64 dbe_search_nextorprev_callcount;
__int64 dbe_indsea_next_perfcount;
__int64 dbe_indsea_next_callcount;
__int64 dbe_indmerge_perfcount;
__int64 dbe_indmerge_callcount;
__int64 trx_end_perfcount;
__int64 trx_end_callcount;
__int64 records_in_range_perfcount;
__int64 records_in_range_callcount;
#endif /* SS_MYSQL_PERFCOUNT */


/************************************************************************\
 *
 *      ss_copyright
 *
 * Copyright string. When you change this, change also makefile.inc.
 */
const char* ss_company_name = "Solid Information Technology Ltd";
const char* ss_copyright_short = "Copyright (C) 1993-2007";
const char* ss_copyright = "Copyright (C) Solid Information Technology Ltd 1993-2007";

/************************************************************************\
 *
 *      ss_licensetext
 *
 * Licensetext is available in server, in client the variable is NULL.
 */
char* ss_licensetext = NULL;                /* Set only in Server. */

/************************************************************************\
 * Other variables.
 */

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)
int  ss_semsleep_startnum = 0; /* zero means semsleep not in use. */
int  ss_semsleep_stopnum = 0;
int  ss_semsleep_mintime = 0;
int  ss_semsleep_maxtime = 50;
int  ss_semsleep_random_freq = 3;
int  ss_semsleep_loopcount = 0;
int  ss_semsleep_maxloopcount = 100;
int  ss_semsleep_loopsem = 0;
bool ss_semsleep_random = TRUE;
#endif

int ss_sem_spincount = 0;

static char local_cmdline[255] = "\0";
char*   ss_cmdline = (char *)local_cmdline;
uint    ss_cmdline_maxlen = sizeof(local_cmdline);

int ss_migratehsbg2;    /* bool */
int ss_convertdb;       /* bool */

ss_beta(bool ss_sem_ignoreerror;)

bool ss_skipatexit = FALSE;

/* what is the ASSERTCORE define? */
#if defined(AUTOTEST_RUN) || defined(ASSERTCORE) || defined(SS_COREOPT)
bool ss_doerrorcore = TRUE; /* autotest must dump core */
bool ss_mainmem;
#  if defined(AUTOTEST_RUN) || defined(SS_DEBUG) || !defined(SS_COREOPT)
bool ss_coreopt = TRUE;
#  else  /* product with COREOPT defaults to no core */
bool ss_coreopt = FALSE;
#  endif /* SS_DEBUG */
#else
ss_debug(bool ss_doerrorcore = TRUE;);
ss_debug(bool ss_mainmem;);
ss_debug(bool ss_coreopt = TRUE;);
#endif

jmp_buf     ss_dbg_jmpbuf[SS_DBG_JMPBUF_MAX];
uint        ss_dbg_jmpbufpos = 0;

typedef struct {
        void (*el_func)(void);
        void* el_next;
} ErrorExitListT;

static void default_assert_message(const char* msg);

static ErrorExitListT*  ErrorExitList;
static char*            SsAssertMessageHeader = (char *)"Solid Application Error";
static void             (*SsAssertMessageFunction)(const char* msg) = default_assert_message;
static void             (*SsErrorMessageFunction)(char* msg) = NULL;
static void             (*SsAssertReportFunction)(char* cmdline, char* msg) = NULL;
static char*            SsCommunicationProtocol = NULL;

static void LocalSsVprintf(char* format, va_list ap);

#if (defined(MSC) || defined(CSET2) || defined(SS_NT)) 
#include <io.h>
#endif

typedef struct {
        char* fl_name;
        uint  fl_namelen;
} DbgFileListT;

typedef struct {
        char* tl_name;
        uint  tl_namelen;
} DbgTriggerListT;

typedef struct SsDebugInfoSt SsDebugInfoT;


/* Local debug variables set from environment variable SS_DEBUG, or by
   calling function SsDbgSet with same syntax as environment variable.
   For syntax and default values, see function SsDbgSet.
*/

struct SsDebugInfoSt {

        /* DANGER !!!
         *  If you change this struct, remember to check the
         *  SS_DEBUG_INFO_DEFAULT above.
         *
         */

        int                 dbg_init;
        int                 dbg_log;
        int                 dbg_flush;
        int                 dbg_display;
        int                 dbg_stderr;
        int                 dbg_thread;
        bool                dbg_timep;
        long                dbg_loglimit;
        char                dbg_logfilename[255];
        SsMsgLogT*          dbg_msglog;
        DbgFileListT*       dbg_filelist;
        uint                dbg_filelistcount;
        uint                dbg_filelistsize;
        DbgTriggerListT*    dbg_triggerlist;
        uint                dbg_triggerlistsize;
        int                 dbg_triggerlevel;

        bool                dbg_thridset;   /* TRUE when thread id is set */
        unsigned            dbg_thrid;      /* The output thread id */
        bool                dbg_thrswitch;  /* If TRUE, do thread switch at
                                               ss_dprintf_X call.*/
        bool                dbg_thrswitchall;/* If TRUE, do thread switch at
                                               ss_dprintf_X call.*/
        bool                dbg_pause;
        bool                dbg_nocorep;
        bool                dbg_errorexit;
        bool                dbg_assertstop;
        SsDebugInfoT*       dbg_stack;
};

#if defined(SS_FAKE)
uint fake_cases[FAKE_ERROR_END];
#endif

bool ss_disableassertmessagebox;
bool ss_msg_useerrornostop;
bool ss_msg_disableallmessageboxes;

int ss_debug_level = 0;

#ifdef SS_SEMPROFILE_DEFAULT
ss_profile(int ss_semdebug = 1;)
#else
ss_profile(int ss_semdebug = 0;)
#endif
ss_debug(int ss_debug_check = 0;)
ss_debug(int ss_memdebug = 0;)
ss_debug(int ss_memdebug_segalloc = 0;)
ss_debug(bool ss_debug_waitp = FALSE;)
ss_debug(long ss_debug_mutexexitwait = -1L;)

#ifdef SS_MYSQL
ss_debug(bool ss_debug_nocardinalcheck = TRUE;)
#else
ss_debug(bool ss_debug_nocardinalcheck = FALSE;)
#endif
bool ss_debug_taskoutput = FALSE;
bool ss_debug_sqloutput = FALSE;
#ifdef SS_SEMSTK_DBG
bool ss_debug_disablesemstk = FALSE;
#endif /* SS_SEMSTK_DBG */
bool   ss_profile_active = FALSE;
double ss_profile_limit;

#define DEFAULT_LOGFNAME    "ssdebug.out"
#define MAX_LOG_SIZE        1000000L

#define SS_DEBUG_INFO_DEFAULT    { 0, 0, 0, 1, 0, 0, FALSE, MAX_LOG_SIZE, DEFAULT_LOGFNAME, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

static SsDebugInfoT ss_debug_info = SS_DEBUG_INFO_DEFAULT;

static void SsDbgVprintfLevel(int level, bool display, char* format, va_list ap);
static char* SsDbgGetLogfilename(char* s);
static char* SsDbgStrstr(const char* s1, const char* s2);

/*#***********************************************************************\
 * 
 *		ss_thrswitch
 * 
 * Try to thread switches so that all other threads would have a change
 * to execute at leat once.
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
static void ss_thrswitch(void)
{
        int i;

        SsThrSleep(10); 
        for (i = 0; i < 100; i++) {
            SsThrSwitch();
        }
}

/*##**********************************************************************\
 *
 *              ss_versiontext
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 *      This function returns the value of SS_SERVER_VERSION (ssenv.h)
 *
 * Globals used :
 *
 * See also :
 */
char* ss_versiontext(void)
{
        static char versiontext[60];


        if (versiontext[0] == '\0') {
            SsSprintf(versiontext,
                        "%02d.%02d.%04d",
                        ss_vers_major(),
                        ss_vers_minor(),
                        ss_vers_release());

#if defined(SS_DEBUG)
            strcat(versiontext, " DEBUG");
#elif defined(SS_DEVSNAPSHOT)
            strcat(versiontext, " Development snapshot");
#elif defined(SS_BETA)
            strcat(versiontext, " Beta");
#endif

#ifdef SS_COVER
            strcat(versiontext, " COVER");
#endif /* SS_SYNC */

#ifdef SS_PURIFY
            strcat(versiontext, " Purify");
#endif /* SS_PURIFY */

            ss_assert(versiontext[sizeof(versiontext)-1] == (char)'\0');
        }

        return(versiontext);
}

#ifndef SS_MYSQL

/*##**********************************************************************\
 *
 *              ss_codebaseversion
 *
 * Code base version returned to client. This need to be same for all
 * different executables, e.g. Embedded Engine and SynchroNet. Code base
 * version is used e.g. to track changes in SQL syntax or RPC properties.
 * Current encoding makes a long integer from the version numbers which
 * guarantees that is is continuously increasing number.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long ss_codebaseversion(void)
{
        return ((((SS_VERS_CODEBASE_MAJOR * 100L)
                  + SS_VERS_CODEBASE_MINOR) * 10000L)
                + SS_VERS_CODEBASE_RELEASE);
}

#endif /* SS_MYSQL */

/*#**********************************************************************\
 *
 *              SsDbgGetLogfilename
 *
 * Parse log file name from a debug string and store it to debug info,
 * if the file name is given.
 *
 * Parameters :
 *
 *      str - in, use
 *              string pointing to file part of debug string
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static char* SsDbgGetLogfilename(s)
    char* s;
{
        char* fname;
        char* result;
        char* fname_end;

        fname = s + 1;  /* skip starting '/' */
        while (*fname != '\0' && *fname != '/' && *fname != ':') {
            fname++;
        }

        if (*fname != ':') {
            /* file name is not given */
            return NULL;
        }
        fname++;    /* skip ':' */

        if (*fname == '\'' || *fname == '"') {
            /* fname given in quoted string */
            fname_end = fname + 1;  /* start after '\'' or '"' */
            while (*fname_end != '\0' && *fname_end != *fname) {
                fname_end++;
            }
            if (*fname_end == '\0') {
                /* no ending quote, stop */
                return NULL;
            }
            fname++;    /* skip '\'' or '"' */
        } else {
            fname_end = fname;
            /* fname given as normal string */
            while (*fname_end != '\0' && *fname_end != '/') {
                fname_end++;
            }
        }

        result = SsMemAlloc(fname_end - fname + 1);
        memcpy(result, fname, fname_end - fname);
        result[fname_end - fname] = '\0';
        return(result);
}


/*#**********************************************************************\
 *
 *              SsDbgStrstr
 *
 * Simple replacement for ANSI C strstr(). Should be good enough for
 * debugging functions.
 *
 * Parameters :
 *
 *      s1 - in, use
 *              string from where s2 is searched
 *
 *      s2 - in, use
 *              string that is searched from s1
 *
 * Return value :
 *
 *      pointer to s1 in place where substring s2 starts, or
 *      NULL if s2 not found from s1
 *
 * Limitations  :
 *
 * Globals used :
 */
static char* SsDbgStrstr(s1, s2)
        const char* s1;
        const char* s2;
{
        size_t s2len = strlen(s2);

        while (*s1 != '\0') {
            while (*s1 != '\0' && *s1 != *s2) {
                s1++;
            }
            if (*s1 == '\0') {
                break;
            }
            if (strncmp(s1, s2, s2len) == 0) {
                return((char *)s1);
            }
            s1++;
        }
        return(NULL);
}

/*#***********************************************************************\
 * 
 *		DbgFreeFilelist
 * 
 * Releases memory from debug file list.
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
static void DbgFreeFilelist(void)
{
        DbgFileListT* fl;
        uint flcount;
        uint i;

        SsSemEnter(ss_lib_sem);

        fl = ss_debug_info.dbg_filelist;
        flcount = ss_debug_info.dbg_filelistcount;
        if (fl != NULL) {
            ss_debug_info.dbg_filelistcount = 0;
            ss_thrswitch();
            if (ss_debug_info.dbg_stack == NULL
                || ss_debug_info.dbg_stack->dbg_filelist != ss_debug_info.dbg_filelist) 
            {
                for (i = 0; i < flcount; i++) {
                    free(fl[i].fl_name);
                }
                free(fl);
            }
            ss_debug_info.dbg_filelist = NULL;
        }

        SsSemExit(ss_lib_sem);
}

/*#***********************************************************************\
 *
 *              SsDbgGetFiles
 *
 * Gets file name prefixes from string s and stores them to ss_debug_info.
 *
 * Parameters :
 *
 *      s - in, use
 *              String containing file name prefix list.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void SsDbgGetFiles(char* s)
{
        char* begin;
        char* end;
        size_t len;
        DbgFileListT* fl;
        uint flcount;
        int tmp_ss_debug_level;

        tmp_ss_debug_level = ss_debug_level;
        ss_debug_level = 0;

        DbgFreeFilelist();

        fl = ss_debug_info.dbg_filelist;
        flcount = ss_debug_info.dbg_filelistcount;

        while (*s != '\0' && *s != '/' && *s != '\n') {
            s++;
            begin = s;
            end = s;
            while (*end != '\0' && *end != '/' && *end != ',' && *end != ';'
                   && *end != '\n') {
                end++;
            }
            len = end - begin;
            if (len > 0) {
                if (fl == NULL) {
                    ss_debug_info.dbg_filelistsize = 10;
                    fl = malloc(ss_debug_info.dbg_filelistsize * sizeof(DbgFileListT));
                    ss_assert(fl != NULL);
                    flcount = 0;
                } else if (flcount == ss_debug_info.dbg_filelistsize) {
                    ss_debug_info.dbg_filelistsize += 10;
                    fl = realloc(fl, sizeof(DbgFileListT) * ss_debug_info.dbg_filelistsize);
                    ss_assert(fl != NULL);
                }
                fl[flcount].fl_name = malloc(len + 1);
                ss_assert(fl[flcount].fl_name != NULL);
                strncpy(fl[flcount].fl_name, begin, len);
                fl[flcount].fl_name[len] = '\0';
                fl[flcount].fl_namelen = len;
                flcount++;
            }
            s = end;
        }

        ss_debug_info.dbg_filelist = fl;
        ss_debug_info.dbg_filelistcount = flcount;

        ss_debug_level = tmp_ss_debug_level;
}

/*#***********************************************************************\
 * 
 *		DbgFreeTriggerlist
 * 
 * 
 * Releases memory from debug trigger list.
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
static void DbgFreeTriggerlist(void)
{
        DbgTriggerListT* tl;
        uint tlsize;

        SsSemEnter(ss_lib_sem);

        tl = ss_debug_info.dbg_triggerlist;
        tlsize = ss_debug_info.dbg_triggerlistsize;
        if (tl != NULL) {
            uint i;
            ss_debug_info.dbg_triggerlistsize = 0;
            ss_thrswitch();
            if (ss_debug_info.dbg_stack == NULL
                || ss_debug_info.dbg_stack->dbg_triggerlist != ss_debug_info.dbg_triggerlist) 
            {
                for (i = 0; i < tlsize; i++) {
                    free(tl[i].tl_name);
                }
                free(tl);
            }
            ss_debug_info.dbg_triggerlist = NULL;
        }
        SsSemExit(ss_lib_sem);
}

/*#***********************************************************************\
 *
 *              SsDbgGetTriggers
 *
 * Gets trigger function name prefixes from string s and stores them
 * to ss_debug_info.
 *
 * Parameters :
 *
 *      s - in, use
 *              String containing trigger name prefix list.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void SsDbgGetTriggers(char* s, int trigger_level)
{
        char* begin;
        char* end;
        size_t len;
        DbgTriggerListT* tl;
        uint tlsize;

        DbgFreeTriggerlist();

        tl = ss_debug_info.dbg_triggerlist;
        tlsize = ss_debug_info.dbg_triggerlistsize;

        while (*s != '\0' && *s != '/' && *s != '\n') {
            s++;
            begin = s;
            end = s;
            while (*end != '\0' && *end != '/' && *end != ',' && *end != '\n') {
                end++;
            }
            len = end - begin;
            if (len > 0) {
                if (tl == NULL) {
                    tl = malloc(sizeof(DbgTriggerListT));
                    ss_assert(tl != NULL);
                    tlsize = 0;
                } else {
                    tl = realloc(tl, sizeof(DbgTriggerListT) * (tlsize + 1));
                    ss_assert(tl != NULL);
                }
                tl[tlsize].tl_name = malloc(len + 1);
                ss_assert(tl[tlsize].tl_name != NULL);
                strncpy(tl[tlsize].tl_name, begin, len);
                tl[tlsize].tl_name[len] = '\0';
                tl[tlsize].tl_namelen = len;
                tlsize++;
            }
            s = end;
        }

        ss_debug_info.dbg_triggerlevel = trigger_level;
        ss_debug_info.dbg_triggerlist = tl;
        ss_debug_info.dbg_triggerlistsize = tlsize;
}

#if defined(WCC) || (defined(SS_LINUX) && (defined(SS_PPC) || defined(SS_LINUX_64BIT)))
#define USE_VA_LIST_VAR_ONLY_ONCE
#endif

/*#**********************************************************************\
 *
 *              SsDbgVprintfLevel
 *
 * Actual function to display debugging output. Level specifies how much
 * the output is indented from the left side of the screen (1 is none).
 * Newline is automatically printed after each message.
 *
 * Parameters :
 *
 *      level - in, use
 *              indentation level, 1 no indentation
 *
 *      display - in, use
 *              indentation level, 1 no indentation
 *
 *      format - in, use
 *              printf style format string for message
 *
 *      ap - in, use
 *              pointer to the argument list
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 *      ss_debug_level
 */
static void SsDbgVprintfLevel(int level, bool display, char* format, va_list ap)
{
        int  i;
        char* p;
        char buf[1024];

#ifdef USE_VA_LIST_VAR_ONLY_ONCE
        /* In Watcom C, va_list variable can be used only once. The content
         * is destroyed in the C library vararg function calls.
         * We make a copy for the second call into variable va2.
         */
        va_list ap2;
        ap2[0] = ap[0];
#endif /* WCC */

        if (level > 0) {
            ss_debug(SsDbgCheckAssertStop());

            if (ss_debug_info.dbg_errorexit) {
                return;
            }
        }
        if (ss_debug_level < level) {
            return;
        }
        if (!display && !ss_debug_info.dbg_display && !ss_debug_info.dbg_log) {
            return;
        }

        buf[0] = '\0';
        p = buf;

        if (ss_debug_info.dbg_timep) {
            SsSprintf(p, "%05ld:", SsTime(NULL)%100000);
            p += strlen(p);
        }
        if (ss_debug_info.dbg_thread) {
            SsSprintf(p, "%2d:", SsThrGetid());
            p += strlen(p);
        }
        for (i = 1; i < level; i++) {
            strcat(buf, "    ");
        }

        strcat(buf, format);
        format = buf;

        if (display || ss_debug_info.dbg_display) {
#ifdef SS_WIN
            LocalSsVprintf(format, ap);
#else /* SS_WIN */
            if (ss_debug_info.dbg_stderr) {
                SsVfprintf(SsStderr, format, ap);
            } else {
                LocalSsVprintf(format, ap);
            }
#endif /* SS_WIN */
        }


        if (ss_debug_info.dbg_log) {
            SsMsgLogT* msglog;
            msglog = ss_debug_info.dbg_msglog;
            if (msglog == NULL) {
                ss_debug_info.dbg_log = FALSE;  /* Avoid recursion. */
                ss_debug_info.dbg_msglog = SsMsgLogInit(
                                                ss_debug_info.dbg_logfilename,
                                                ss_debug_info.dbg_loglimit);
                ss_debug_info.dbg_log = TRUE;
                msglog = ss_debug_info.dbg_msglog;
            }
#ifdef USE_VA_LIST_VAR_ONLY_ONCE
            SsMsgLogVPrintf(msglog, format, ap2);
#else /* WCC */
            SsMsgLogVPrintf(msglog, format, ap);
#endif /* WCC */
            if (ss_debug_info.dbg_flush) {
                SsMsgLogFlush(msglog);
            }
        }
}

/*##**********************************************************************\
 *
 *              SsDbgFlush
 *
 * Flushes debug output log file, if it is used.
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
void SsDbgFlush(void)
{
        if (ss_debug_info.dbg_log && ss_debug_info.dbg_msglog != NULL) {
            SsMsgLogFlush(ss_debug_info.dbg_msglog);
        }
}

/*#**********************************************************************\
 *
 *              SsDbgPrintfFunN
 *
 * Debugging output function for user given level.
 *
 * Parameters :
 *
 *      level - in, use
 *              output level
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SS_CDECL SsDbgPrintfFunN(int level, const char* format, ...)
{
        va_list ap;

        va_start(ap, format);
        SsDbgVprintfLevel(level, FALSE, (char *)format, ap);
        va_end(ap);

        return(TRUE);
}

/*#**********************************************************************\
 *
 *              SsDbgPrintfFun1
 *
 * Debugging output function for level 1 debugging. This is the basic
 * debugging output function. Output for other debugging level functions
 * will indent the output from the left side of the screen (if possible),
 * depending on the level.
 * Newline is automatically printed after each message.
 *
 * NOTE! This is only for internal usage of the debug system.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SS_CDECL SsDbgPrintfFun1(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);
        SsDbgVprintfLevel(1, FALSE, (char *)format, ap);
        va_end(ap);

        return(TRUE);
}

/*#**********************************************************************\
 *
 *              SsDbgPrintfFun2
 *
 * Debugging output function for level 2 debugging. Debugging output is
 * indented 1 level from the left side of the screen (if possible), to
 * separate it from other levels.
 * Newline is automatically printed after each message.
 *
 * NOTE! This is only for internal usage of the debug system.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SS_CDECL SsDbgPrintfFun2(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);
        SsDbgVprintfLevel(2, FALSE, (char *)format, ap);
        va_end(ap);

        return(TRUE);
}

/*#**********************************************************************\
 *
 *              SsDbgPrintfFun3
 *
 * Debugging output function for level 3 debugging. Debugging output is
 * indented 2 levels from the left side of the screen (if possible), to
 * separate it from other levels.
 * Newline is automatically printed after each message.
 *
 * NOTE! This is only for internal usage of the debug system.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SS_CDECL SsDbgPrintfFun3(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);
        SsDbgVprintfLevel(3, FALSE, (char *)format, ap);
        va_end(ap);

        return(TRUE);
}

/*#**********************************************************************\
 *
 *              SsDbgPrintfFun4
 *
 * Debugging output function for level 4 debugging. Debugging output is
 * indented 3 levels from the left side of the screen (if possible), to
 * separate it from other levels.
 * Newline is automatically printed after each message.
 *
 * NOTE! This is only for internal usage of the debug system.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SS_CDECL SsDbgPrintfFun4(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);
        SsDbgVprintfLevel(4, FALSE, (char *)format, ap);
        va_end(ap);

        return(TRUE);
}

/*#**********************************************************************\
 *
 *              SsDbgFileOk
 *
 * Checks if the file name matches to a set of files given in the
 * SS_DEBUG environment variable. For syntax, see comment in function
 * SsDbgSet. Only files with the same prefix as one of the files in
 * the file list is accepted. If no file list is specified, then all
 * files are accepted.
 *
 * Parameters :
 *
 *      fname - in, use
 *              File name.
 *
 * Return value :
 *
 *      TRUE  - file name accepted
 *      FALSE - file name rejected
 *
 * Limitations  :
 *
 * Globals used :
 */
bool SsDbgFileOk(fname)
        char* fname;
{
        register uint i;
        register uint j;
        DbgFileListT* fl;

        if (ss_debug_info.dbg_thrswitchall) {
            SsThrSwitch();
        }

#ifdef SS_MT
        if (ss_debug_info.dbg_thridset) {
            unsigned threadid = SsThrGetid();
            if (threadid != ss_debug_info.dbg_thrid) {
                return (FALSE);
            }
        }
#endif /* SS_MT */

        if (ss_debug_info.dbg_filelistcount == 0) {
            if (ss_debug_info.dbg_thrswitch) {
                SsThrSwitch();
            }
            return(TRUE);
        }

#if defined(MSC) || defined(MSC_NT)
        {
            /* In MS Visual C++ the __FILE__ expands to path and file name.
             */
            char* tmp_fname;
            tmp_fname = strrchr(fname, '\\');
            if (tmp_fname != NULL) {
                fname = tmp_fname+1;
            }
        }
#endif /* MSC */
#ifdef SS_UNIX
        if (fname[0] == '/') {
            /* At least in some Unixes (Linux) and C++ files (.cc)
             * the full path name is included on the file name.
             */
            char* tmp_fname;
            tmp_fname = strrchr(fname, '/');
            if (tmp_fname != NULL) {
                fname = tmp_fname+1;
            }
        }
#endif

        fl = ss_debug_info.dbg_filelist;


        for (i = 0; i < ss_debug_info.dbg_filelistcount; i++) {
            uint len;
            uchar* name;
            len = (uint)fl[i].fl_namelen;
            name = (uchar *)fl[i].fl_name;
            if (len > 4) {
                /* Many files have a common prefix so test a middle character first. */
                if (ss_toupper((ss_byte_t)fname[4]) != ss_toupper((ss_byte_t)name[4])) {
                    continue;
                }
            }
            for (j = 0; j < len && fname[j] != '\0'; j++) {
                if (ss_toupper((ss_byte_t)fname[j]) != ss_toupper((ss_byte_t)name[j])) {
                    break;
                }
            }
            if (j == len) {
                if (ss_debug_info.dbg_thrswitch) {
                    SsThrSwitch();
                }
                return(TRUE);
            }
        }
        return(FALSE);
}

/*#**********************************************************************\
 *
 *              SsDbgCheckTrigger
 *
 * Checks if the function name matches to a set of triggers given in the
 * SS_DEBUG environment variable. For syntax, see comment in function
 * SsDbgSet. Only functions with the same prefix as one of the functions in
 * the trigger list is accepted. If no trigger list is specified, then all
 * functions are accepted.
 *
 * Parameters :
 *
 *      funcname - in, use
 *              Function name.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsDbgCheckTrigger(funcname)
        const char* funcname;
{
        register uint i;
        register uint j;
        DbgTriggerListT* tl;

        if (ss_debug_info.dbg_triggerlistsize == 0) {
            return;
        }

        tl = ss_debug_info.dbg_triggerlist;

        for (i = 0; i < ss_debug_info.dbg_triggerlistsize; i++) {
            uint len;
            uchar* name;
            len = (uint)tl[i].tl_namelen;
            name = (uchar *)tl[i].tl_name;
            for (j = 0; j < len && funcname[j] != '\0'; j++) {
                if (ss_toupper((ss_byte_t)funcname[j]) != ss_toupper(name[j])) {
                    break;
                }
            }
            if (j == len) {
                /* Match, swap ss_debug_level and
                 * ss_dbg_info.dbg_triggerlevel
                 */
                int tmp_level;

                tmp_level = ss_debug_level;
                ss_debug_level = ss_debug_info.dbg_triggerlevel;
                ss_debug_info.dbg_triggerlevel = tmp_level;

                return;
            }
        }
}

/*#***********************************************************************\
 * 
 *		SsDbgSetErrorExit
 * 
 * Does error exit processing for debug logging. Tries to avoid case 
 * where message log is killed when threads are accessing it.
 * 
 * Parameters : 
 * 
 *		text - 
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
static void SsDbgSetErrorExit(char* text)
{
#ifndef SS_WIN
        SsFFlush(SsStderr);
        SsFFlush(SsStdout);
#endif
        ss_debug_info.dbg_errorexit = 1;
        ss_debug_info.dbg_log = 0;
        if (ss_debug_info.dbg_msglog != NULL) {
            SsMsgLogT* msglog;
            msglog = ss_debug_info.dbg_msglog;
            ss_debug_info.dbg_msglog = NULL;
            /* Give other threads time to compelete message log writes. Message log
             * mutex does not help here if we delete the message log object.
             */
            ss_thrswitch();
            if (text != NULL) {
                SsMsgLogPutStr(msglog, text);
            }
            SsMsgLogFlush(msglog);
            SsMsgLogDone(msglog);
        }
}

#ifdef SS_MYSQL_PERFCOUNT

#define report_perfcount(a,b,c) report_perfcount_ex(a,&b,&c)

static void report_perfcount_ex(char* name, __int64* perfcount, __int64* callcount)
{
    printf("%24s %7I64d tics/operation, callcount %I64d\n", 
        name, 
        *callcount > 0 
            ? *perfcount / *callcount 
            : (__int64)0, 
        *callcount);
    *perfcount = 0;
    *callcount = 0;
}

static void report_solid(void)
{
    printf("Solid\n");
    report_perfcount("read_idx", index_read_idx_perfcount, index_read_idx_callcount);
    report_perfcount("relcur_create", relcur_create_perfcount, relcur_create_callcount);
    report_perfcount("relcur_setconstr", relcur_setconstr_perfcount, relcur_setconstr_callcount);
    report_perfcount("relcur_open", relcur_open_perfcount, relcur_open_callcount);
    report_perfcount("dbe_search_init", dbe_search_init_perfcount, dbe_search_init_callcount);
    report_perfcount("dbe_search_reset", dbe_search_reset_perfcount, dbe_search_reset_callcount);
    report_perfcount("dbe_search_reset_fetch", dbe_search_reset_fetch_perfcount, dbe_search_reset_fetch_callcount);
    report_perfcount("fetch_next", fetch_perfcount, fetch_callcount);
    report_perfcount("relcur_next", relcur_next_perfcount, relcur_next_callcount);
    report_perfcount("dbe_search_nextorprev", dbe_search_nextorprev_perfcount, dbe_search_nextorprev_callcount);
    report_perfcount("dbe_indsea_next", dbe_indsea_next_perfcount, dbe_indsea_next_callcount);
    report_perfcount("records_in_range", records_in_range_perfcount, records_in_range_callcount);
    report_perfcount("tb_pla_create", tb_pla_create_perfcount, tb_pla_create_callcount);
    report_perfcount("tb_pla_reset", tb_pla_reset_perfcount, tb_pla_reset_callcount);
    report_perfcount("commit", commit_perfcount, commit_callcount);
    report_perfcount("trx_end", trx_end_perfcount, trx_end_callcount);
    report_perfcount("dbe_indmerge", dbe_indmerge_perfcount, dbe_indmerge_callcount);
}

#endif /* SS_MYSQL_PERFCOUNT */

/*##**********************************************************************\
 *
 *              SsDbgSet
 *
 * Sets internal debug info variabels to correct state. Debug state is given
 * in 'str' parameter with the same syntax as in environment variable.
 * New settings change only given parameters, other setting remain the same.
 * This function is provided mainly for DLL usage, because DLL functions
 * do not see environment variables from the executable program.
 *
 * The environment variable can contain the following options:
 *
 *      /LEVEL:<n>
 *          Sets debug output level to <n>. Levels between zero
 *          and four are available, level four is the heaviest
 *          debugging output level. Used with macros
 *          ss_dprintf and ss_dprintf_[1234].
 *
 *      /CHECK:<n>
 *          Sets debug check level to <n>. Levels between zero
 *          and four are available, level four is the heaviest
 *          debugging check level. Used with macros
 *          ss_debug_[1234].
 *
 *      /DLLOG:pathprefix
 *          Enables debug output in DISKLESS and sets default path
 *          for output files.
 *
 *      /DLNOLOG
 *          Disables debug output in DISKLESS.
 *
 *      /LOG[:fname]
 *          Logs debug output to file ssdebug.out, or to file
 *          <fname> if file name is given.
 *
 *      /NOLOG
 *          Does not log debug to file.
 *
 *      /LIM:<log file size split limit>
 *          Sets debug log file split size limit.
 *
 *      /UNL
 *          There is no limit for the log file size (unlimited).
 *
 *      /FILES:<list>
 *          Specifies the files from which the debug output is
 *          given. The list can contain file name prefixes
 *          separated with a comma. Output is given only
 *          from those files which match with one of the
 *          prefixes. If this option is not set, output is
 *          given from all files. This option is used with
 *          macros ss_dprintf and ss_dprintf_[1234].
 *
 *      /TID:<n>
 *          Specifies the thread from which the debug output is
 *          given. Useful when error printing is started by eplicitly
 *          redefining SS_DEBUG on the fly after detecting an error
 *          condition. That prevents other threads from printing their
 *          debug output in between
 *
 *      /TRIGGER:<level>:<namelist>
 *          List of trigger functions. When trigger function is entered,
 *          debug level is set to <level>. When trigger function is exited,
 *          debug level is set to 0.
 *
 *          Trigger is enabled using ss_trigger("<name>") keyword in source
 *          code. When ss_trigger with a <name> matching to <namelist> is
 *          found for the first time, debug level is set to <level>. When
 *          ss_trigger("<name>") is found for the second time, debug level
 *          is set to 0. The again next ss_trigger("<name>") enables the
 *          debug level.
 *
 *          <name> used in ss_trigger can be any user given name. To avoid
 *          name conflicts normal Solid naming convensions are recommended.
 *
 *          For an example, see file est1est.c.
 *
 *      /FLUSH
 *          Flushes debug file after every write, useful for
 *          example in  MS Windows.
 *
 *      /DISPLAY
 *          Displays output also on screen, screen is flushed
 *          automatically.
 *
 *      /NODISPLAY
 *          Does not display debug output on screen.
 *
 *      /MEM[:<n>]
 *          Start memdebug with a given level. Levels one and
 *          two are available. If no level is given after MEM,
 *          level one is assumed. Level one keeps track of
 *          all allocated and deallocated memory, level two
 *          checks all pointers for under and overwrite in
 *          each memory allocation and deallocation. Level
 *          two is very efficient when tracking memory bugs,
 *          but may be also rather slow so use it with caution.
 *
 *      /NOS
 *          No call stack is displayed when memory pointer lists
 *          are printed.
 *
 *      /MINIT:<val>
 *          Initilalizes allocated memory with <val> instead
 *          of the default initialization value.
 *
 *      /WAIT
 *          Sets ss_debug_waitp flag to TRUE. This flag is
 *          in Windows version, where the system does not
 *          exit the program and remove all windows. Both
 *          sswinmai.c and ssdllmai.c test this flag. Note
 *          that if both the executable and DLL use debug
 *          output, you have to press a key twice before the
 *          program exits.
 *
 *      /PAUSE
 *          Sets ss_debug_pausep tp TRUE.
 *
 *
 *      /NOWAIT
 *          Sets the ss_debug_waitp flag to FALSE. This is
 *          the default.
 *
 *      /STDERR
 *          Output is displayed to stderr instead of stdout.
 *
 *      /TIME
 *          Useful when debugging timing of operations. Current SsTimeT
 *          value is displayed before every message.
 *
 *      /THREAD
 *          Useful when debugging threads. Thread id is
 *          displayed before every message.
 *
 *      /NOTHREAD
 *          Thread id is not displayed before every message.
 *
 *      /NOPRINT
 *          Disables memchk pointer list display.
 *
 *      /STDERR
 *          Debug output is print to stderr instead of stdout.
 *
 *      /DONE
 *          Closes debug message log.
 *
 *      /NOC
 *          No core in error. In Unix and NT versions, error exit causes
 *          memory fault to do a core file or to start a debugger. This
 *          switch disables the memory fault.
 *
 *      /SWI
 *          Causes SsThrSwitch call on every ss_dprintf_X call that
 *          matches the debug level and file specifications.
 *
 *      /SWA
 *          Causes SsThrSwitch call on every ss_dprintf_X call that
 *          matches the debug level regardless of the file specifications.
 *
 *      /TASK
 *          Prints task start and stop info.
 *
 *      /SQL
 *          Prints SQL statements.
 *
 *      /MXW:<n>
 *          Waits <n> milliseconds after each mutex exit
 *          if <n> < 0 the sleeping is not enabled. if <n> == 0
 *          the sleep is equivalent to SsThrSwitch() call
 *          default value for <n> = -1 (no wait).
 *
 *      /FAK:[case1<#cnt1>,case_num2<#cnt2>...case_numN<#cntN>]
 *          Sets on fake error cases. Counter value is optional.
 *          If counter value is set, fake error doesn't take place until
 *          the counter reaches value one (1). If counter value is not
 *          given, it's set to one (1) by default. Fake errors can be turned
 *          off by setting counter to value zero (0).
 *          Check out possible error numbers from the header file <ssdebug.h>.
 *
 *      /SLEEPDBG: strtnum, stopnum, mintime, maxtime, rnd, loopcnt, maxloopcnt, loopsem, random
 *          Enables semaphore sleep debugging
 *
 *
 * Defaults for TTY systems (other than SS_WIN):
 *
 *      /NOFILE
 *      /DISPLAY
 *
 * Defaults for windowing systems (SS_WIN):
 *
 *      /FILE
 *      /NODISPLAY
 *
 * No default values are used, when SS_DEBUG environment variable is set,
 * but it does not contain any options. If DEBUG environment variable is
 * not set, no debug output is given and debug level is 0.
 *
 * Parameters :
 *
 *      str - in, use
 *              new debug settings
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 *
 *      ss_debug_level
 *      ss_debug_check
 *      ss_debug_waitp
 *      ss_memdebug
 *      ss_memdebug_segalloc
 */
void SsDbgSet(const char* str)
{
        char* s;
        int i __attribute__ ((unused));

        if (str != NULL && strcmp(str, "/ERROREXIT") == 0) {
            SsDbgSetErrorExit(NULL);
            return;
        }

        if (!ss_debug_info.dbg_init) {
            ss_debug_info.dbg_init = 1;
            SsDbgSet(SsGetEnv("SS_DEBUG"));
        }

        if (str == NULL || str[0] == '\0') {
            return;
        }

        if (strcmp(str, "/DON") == 0) {
            SsMsgLogDone(ss_debug_info.dbg_msglog);
            ss_debug_info.dbg_msglog = NULL;
            ss_thrswitch();
            return;
        }

        s = SsDbgStrstr(str, "/PUSH");
        if (s != NULL) {
            SsDebugInfoT* stack;
            stack = malloc(sizeof(SsDebugInfoT));
            memcpy(stack, &ss_debug_info, sizeof(SsDebugInfoT));
            ss_debug_info.dbg_stack = stack;
        }
        s = SsDbgStrstr(str, "/POP");
        if (s != NULL && ss_debug_info.dbg_stack != NULL) {
            SsDebugInfoT* stack;
            /* Clear current settings. */
            SsDbgSet("/FIL:/TRI:0:/NOL");
            SsSemEnter(ss_lib_sem);
            if (ss_debug_info.dbg_stack != NULL) {
                stack = ss_debug_info.dbg_stack;
                memcpy(&ss_debug_info, stack, sizeof(SsDebugInfoT));
                free(stack);
            }
            SsSemExit(ss_lib_sem);
        }
        s = SsDbgStrstr(str, "/FIL");
        if (s != NULL) {
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                SsDbgGetFiles(s);
            }
        }
        s = SsDbgStrstr(str, "/TRI");
        if (s != NULL) {
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                int trigger_level;
                s++;
                trigger_level = atoi(s);
                while (*s != '\0' && *s != ':') {
                    s++;
                }
                if (*s == ':') {
                    SsDbgGetTriggers(s, trigger_level);
                }
            }
        }

        s = SsDbgStrstr(str, "/PROF");
        if (s != NULL) {
            long limit = 1;
            ss_profile_active = TRUE;
            /* parse debug log file limit */
            s++;
            while (*s != '\0' && *s != ':' && *s != '/') {
                s++;
            }
            if (*s == ':') {
                s++;
                limit = atol(s);
                ss_profile_limit = (double)limit / 1000.0;
            } else {
                ss_profile_limit = (double)limit;
            }
            if (limit == 0) {
                ss_profile_active = FALSE;
            }
        }
#ifdef SS_MYSQL_PERFCOUNT
        s = SsDbgStrstr(str, "/PERFCOUNT");
        if (s != NULL) {
            s++;
            while (*s != '\0' && *s != ':' && *s != '/') {
                s++;
            }
            if (*s == ':') {
                s++;
                if (*s == 's') {
                    report_solid();
                    mysql_enable_perfcount = 0;
                } else {
                    mysql_enable_perfcount = atoi(s);
                }
            } else {
                mysql_enable_perfcount = 1;
            }
        }
#endif /* SS_MYSQL_PERFCOUNT */

        s = SsDbgStrstr(str, "/DLLOG");
        if (s != NULL) {
            char* fname;
            fname = SsDbgGetLogfilename(s);
            if(fname != NULL) {
                SsFileSetPathPrefix(fname);
                SsMemFree(fname);
                if(first_time) {
                    first_time = FALSE;
                    orig_msglog_diskless = msglog_diskless;
                    orig_disable_output = disable_output;
                }
                msglog_diskless = FALSE;
                disable_output = FALSE;
            }
        }
        s = SsDbgStrstr(str, "/DLNOLOG");
        if (s != NULL) {
            msglog_diskless = orig_msglog_diskless;
            disable_output = orig_disable_output;
            if (monitorlog != NULL) {
                SsMsgLogFlush(monitorlog);
                SsMsgLogDone(monitorlog);
                monitorlog = NULL;
            }
            ss_debug_info.dbg_log = 0;
            if (ss_debug_info.dbg_msglog != NULL) {
                SsMsgLogT* msglog;
                msglog = ss_debug_info.dbg_msglog;
                ss_debug_info.dbg_msglog = NULL;
                SsMsgLogFlush(msglog);
                ss_thrswitch();
                SsMsgLogDone(msglog);
            }
        }
        s = SsDbgStrstr(str, "/NOL");
        if (s != NULL) {
            ss_debug_info.dbg_log = 0;
            if (ss_debug_info.dbg_msglog != NULL) {
                SsMsgLogT* msglog;
                msglog = ss_debug_info.dbg_msglog;
                ss_debug_info.dbg_msglog = NULL;
                SsMsgLogFlush(msglog);
                if (ss_debug_info.dbg_stack == NULL
                    || ss_debug_info.dbg_stack->dbg_msglog != msglog) 
                {
                    /* Free message log if it is not used in stack.
                     */
                    ss_thrswitch();
                    SsMsgLogDone(msglog);
                }
            }
        }
        s = SsDbgStrstr(str, "/LOG");
        if (s != NULL) {
            char* fname;
            bool newname;
            fname = SsDbgGetLogfilename(s);
            if(fname != NULL) {
                newname = strcmp(ss_debug_info.dbg_logfilename, fname) != 0;
                SsDbgSetDebugFile(fname);
                SsMemFree(fname);
            } else {
                newname = strcmp(ss_debug_info.dbg_logfilename, DEFAULT_LOGFNAME) != 0;
                SsDbgSetDebugFile((char *)DEFAULT_LOGFNAME);
            }
            if (newname) {
                SsDbgSet("/NOL");
            }
            if (ss_debug_info.dbg_msglog == NULL) {
                ss_debug_info.dbg_msglog =
                    SsMsgLogInitForce(
                        ss_debug_info.dbg_logfilename,
                        ss_debug_info.dbg_loglimit,
                        TRUE);
                ss_dassert(ss_debug_info.dbg_msglog != NULL);
            }
            ss_debug_info.dbg_log = 1;
        }
        s = SsDbgStrstr(str, "/LIM");
        if (s != NULL) {
            /* parse debug log file limit */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                s++;
                ss_debug_info.dbg_loglimit = atol(s);
                SsMsgLogSetLimit(ss_debug_info.dbg_msglog, ss_debug_info.dbg_loglimit);
            }
        }
        s = SsDbgStrstr(str, "/UNL");
        if (s != NULL) {
            ss_debug_info.dbg_loglimit = 0;
            SsMsgLogSetLimit(ss_debug_info.dbg_msglog, ss_debug_info.dbg_loglimit);
        }
        
        s = SsDbgStrstr(str, "/FLU");
        if (s != NULL) {
            ss_debug_info.dbg_flush = 1;
        }
        s = SsDbgStrstr(str, "/PAU");
        if (s != NULL) {
            ss_debug_info.dbg_pause = TRUE;
        }
        s = SsDbgStrstr(str, "/DIS");
        if (s != NULL) {
            ss_debug_info.dbg_display = 1;
        }
        s = SsDbgStrstr(str, "/NOD");
        if (s != NULL) {
            ss_debug_info.dbg_display = 0;
        }
        s = SsDbgStrstr(str, "/STDERR");
        if (s != NULL) {
            ss_debug_info.dbg_stderr = 1;
        }
        s = SsDbgStrstr(str, "/TIM");
        if (s != NULL) {
            ss_debug_info.dbg_timep = 1;
        }
        s = SsDbgStrstr(str, "/THR");
        if (s != NULL) {
            ss_debug_info.dbg_thread = 1;
        }
        s = SsDbgStrstr(str, "/NOT");
        if (s != NULL) {
            ss_debug_info.dbg_thread = 0;
        }
        s = SsDbgStrstr(str, "/NOC");
        if (s != NULL) {
            ss_debug_info.dbg_nocorep = TRUE;
            ss_msg_useerrornostop = TRUE;
        }
        s = SsDbgStrstr(str, "/LEV");
        if (s != NULL) {
            /* parse debug level */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s != '\0') {
                s++;
                ss_debug_level = atoi(s);
            }
        }
        s = SsDbgStrstr(str, "/SEM");
        if (s != NULL) {
            ss_profile(ss_semdebug = 1);
        }
        s = SsDbgStrstr(str, "/TASK");
        if (s != NULL) {
            ss_debug_taskoutput = TRUE;
        }
        s = SsDbgStrstr(str, "/NOTASK");
        if (s != NULL) {
            ss_debug_taskoutput = FALSE;
        }
        s = SsDbgStrstr(str, "/SQL");
        if (s != NULL) {
            ss_debug_sqloutput = TRUE;
        }
        s = SsDbgStrstr(str, "/NOSQL");
        if (s != NULL) {
            ss_debug_sqloutput = FALSE;
        }
#ifdef SS_MT
        s = SsDbgStrstr(str, "/TID");
        if (s != NULL) {
            /* parse thread id */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s != '\0') {
                s++;
                ss_debug_info.dbg_thridset = TRUE;
                ss_debug_info.dbg_thrid = atoi(s);
            }
        }
#endif /* SS_MT */

#ifdef SS_DEBUG
#  ifdef SS_MT
        s = SsDbgStrstr(str, "/MXW");
        if (s != NULL) {
            /* parse mutex exit wait */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                s++;
                ss_debug_mutexexitwait = atol(s);
            }
        }
#  endif /* SS_MT */
        s = SsDbgStrstr(str, "/MEM");
        if (s != NULL) {
            /* parse debug level */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                s++;
                if (strncmp(s, "SEG", 3) == 0) {
                    ss_memdebug_segalloc = 1;
                } else {
                    ss_memdebug = atoi(s);
                    if (ss_memdebug == 0) {
                        ss_memdebug = 1;
                    }
                }
            }
        }
        s = SsDbgStrstr(str, "/NOSTACK");
        if (s != NULL) {
            ss_memtrc_disablecallstack = TRUE;
        }
        s = SsDbgStrstr(str, "/MIN");
        if (s != NULL) {
            /* parse init value */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                s++;
                memchk_newborn = atoi(s);
            } else {
                memchk_newborn = 0;
            }
        }
        s = SsDbgStrstr(str, "/NOP");
        if (s != NULL) {
            memchk_disableprintlist = TRUE;
        }
        s = SsDbgStrstr(str, "/SWI");
        if (s != NULL) {
            ss_debug_info.dbg_thrswitch = TRUE;
        }
        s = SsDbgStrstr(str, "/NOSWI");
        if (s != NULL) {
            ss_debug_info.dbg_thrswitch = FALSE;
        }
        s = SsDbgStrstr(str, "/SWA");
        if (s != NULL) {
            ss_debug_info.dbg_thrswitchall = TRUE;
        }
        s = SsDbgStrstr(str, "/NOSWA");
        if (s != NULL) {
            ss_debug_info.dbg_thrswitchall = FALSE;
        }
        s = SsDbgStrstr(str, "/WAI");
        if (s != NULL) {
            ss_debug_waitp = TRUE;
        }
        s = SsDbgStrstr(str, "/NOW");
        if (s != NULL) {
            ss_debug_waitp = FALSE;
        }
        s = SsDbgStrstr(str, "/CAR");
        if (s != NULL) {
            ss_debug_nocardinalcheck = TRUE;
        }
        s = SsDbgStrstr(str, "/SPLIT");
        if (s != NULL) {
            SsMsgLogSetForceSplitOnce();
        }
        s = SsDbgStrstr(str, "/ASSERTSTOP");
        if (s != NULL) {
            ss_debug_info.dbg_assertstop = TRUE;
        }
        s = SsDbgStrstr(str, "/CHE");
        if (s != NULL) {
            /* parse debug level */
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s != '\0') {
                s++;
                ss_debug_check = atoi(s);
            }
        }

#endif /* SS_DEBUG */

#if defined(SS_DEBUG) || defined(AUTOTEST_RUN)

        s = SsDbgStrstr(str, "/SLEEPDBG");
        if (s != NULL) {
            while (*s != '\0' && *s != ':') {
                s++;
            }
            if (*s == ':') {
                s++;
                ss_semsleep_startnum = atoi(s);
                while (*s != '\0' && *s != ',') {
                    s++;
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_stopnum = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_mintime = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_maxtime = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_random_freq = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_loopcount = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_maxloopcount = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_loopsem = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
                if (*s == ',') {
                    s++;
                    ss_semsleep_random = atoi(s);
                    while (*s != '\0' && *s != ',') {
                        s++;
                    }
                }
            }
        }
#endif

#if defined(SS_FAKE)

        s = SsDbgStrstr(str, "/FAK");
        if (s != NULL) {
            int fc = 0;
            int val = 0;

            while (*s != '\0' && *s != ':') {
                s++;
            }
            while (*s != '\0' && *s != '/' && *s != '\n') {
                if (*s == ',' || *s == ':') {
                    s++;

                    /*
                     * Special case (clear all FAKEs /FAK:clear)
                     */
                    if (SsStrnicmp(s, (char *)"clear", 5) == 0) {
                        for (i = FAKE_ERROR_BEGIN; i < FAKE_ERROR_END; i++) {
                            fake_cases[i] = 0;
                        }
                        break;
                    }
                    fc = atoi(s);
                    if (fc > FAKE_ERROR_BEGIN && fc < FAKE_ERROR_END) {
                        fake_cases[fc] = 1;
                    }
                } else if (*s == '#'){
                    s++;
                    val = atoi(s);
                    if (fc > FAKE_ERROR_BEGIN && fc < FAKE_ERROR_END) {
#ifdef SS_BLOBBUG
                        if (fc == FAKE_SSE_DAXBUG && val == 0 && fake_cases[fc] != 0) {
                            FAKE_BLOBMON_OUTPUT();
                        }
#endif /* SS_BLOBBUG */
                        fake_cases[fc] = val;
                    }
                } else {
                    s++;
                }
            }
        }

#endif /* SS_FAKE */

}

#if defined(SS_FAKE) && defined(SS_BLOBBUG)

uint* fake_blobmon[3];
uint fake_blobmon_idx[2];
char* fake_prev_blobbuf;
uint fake_prev_blobbufsize;
ulong fake_prev_blobdaddr;
ulong fake_blobdaddr;

void fake_blobmon_output(void)
{
        ulong sum1;
        ulong sum2;
        uint i;
        uint j;
        char *fmt;
        for (j = 0; j < 2; j++) {
            if (fake_blobmon[j] != NULL) {
                ss_pprintf_1(("blob monitor #%04d :", j));
                for (i = 0; i < fake_blobmon_idx[j]; i++) {
                    fmt = "%04d, ";
                    if (i + 1 == fake_blobmon_idx[j]) {
                        fmt = "%04d\n";
                    }
                    ss_pprintf_1((fmt, fake_blobmon[j][i]));
                }
            }
        }
        if (fake_blobmon[2] != NULL) {
            ss_pprintf_1(("blob monitor #%04d :", 2));
            for (i = 0; i < fake_blobmon_idx[1]; i++) {
                fmt = "%04d, ";
                if (i + 1 == fake_blobmon_idx[1]) {
                    fmt = "%04d\n";
                }
                ss_pprintf_1((fmt, fake_blobmon[2][i]));
            }
            ss_assert(fake_blobmon_idx[0] == fake_blobmon_idx[1]);
            sum1 = 0;
            sum2 = 0;
            for (i = 0; i < fake_blobmon_idx[0]; i++) {
                ss_assert(fake_blobmon[0][i] == fake_blobmon[1][i]);
                sum1 += fake_blobmon[0][i];
                sum2 += fake_blobmon[2][i];

            }
            ss_assert(sum1 == sum2);
        }
        FAKE_BLOBMON_RESET();
}

#endif /* defined(SS_FAKE) && defined(SS_BLOBBUG) */

/*#**********************************************************************\
 *
 *              LocalSsVprintf
 *
 * Replacement for vprintf.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf style format string for message
 *
 *      ap - in, use
 *              pointer to the argument list
 *
 * Return value :
 *
 * Comments :
 *
 *      We should not call Ss777ProcessSwitch() in SS_WIN version here
 *      because then there are different process switch places in
 *      debug version than the product version.
 *
 * Globals used :
 */
static void LocalSsVprintf(char* format, va_list ap)
{
#if defined(SS_PRINTABLE_ONLY)
        char buf[64 * 1024];
        uint i;

        SsVsprintf(buf, format, ap);
        for (i = 0; i < 64 * 1024 && buf[i] != '\0'; i++) {
            if (!ss_isprint(buf[i]) && buf[i] != '\n' && buf[i] != '\r') {
                buf[i] = '.';
            }
        }
        buf[64 * 1024 - 1] = '\0';
        SsFPuts(buf, SsStdout);
#else
        vprintf(format, ap);
#endif
}

/*##*********************************************************************\
 *
 *              SsVfprintf
 *
 * Replacement for vfprintf.
 *
 * Parameters :
 *
 *      fp - use
 *          output file
 *
 *      format - in, use
 *              printf style format string for message
 *
 *      ap - in, use
 *              pointer to the argument list
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsVfprintf(SS_FILE* fp, char* format, va_list ap)
{
#if defined(SS_WIN) 
        static char buf[4096];
        SsVsprintf(buf, format, ap);
        SsFPuts(buf, fp);
#elif defined(SS_PRINTABLE_ONLY)
        char buf[64 * 1024];
        uint i;

        if (fp == SsStdout || fp == SsStderr) {
            SsVsprintf(buf, format, ap);
            for (i = 0; i < 64 * 1024 && buf[i] != '\0'; i++) {
                if (!ss_isprint(buf[i]) && buf[i] != '\n' && buf[i] != '\r') {
                    buf[i] = '.';
                }
            }
            buf[64 * 1024 - 1] = '\0';
            SsFPuts(buf, fp);
        } else {
            vfprintf(fp, format, ap);
        }
#else
        vfprintf(fp, format, ap);
#endif
}

/*##**********************************************************************\
 *
 *              SsDbgPrintf
 *
 * Unconditionally displays output using debugging printf-function.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SS_CDECL SsDbgPrintf(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);

        SsDbgVprintfLevel(0, FALSE, (char *)format, ap);

        va_end(ap);
}

/*##**********************************************************************\
 *
 *              SsPrintf
 *
 * Replacement for printf.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsVprintf(char* format, va_list ap)
{
        SsDbgVprintfLevel(0, TRUE, format, ap);

#ifndef SS_WIN
        SsFFlush(SsStdout);
#endif /* !SS_WIN */
}

/*##**********************************************************************\
 *
 *              SsPrintf
 *
 * Replacement for printf.
 *
 * Parameters :
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SS_CDECL SsPrintf(const char* format, ...)
{
        va_list ap;

        va_start(ap, format);

        SsVprintf((char *)format, ap);

        va_end(ap);
}

/*##**********************************************************************\
 *
 *              SsFprintf
 *
 * Replacement for SsFPrintf.
 *
 * Parameters :
 *
 *      fp - use
 *          Output file pointer. NULL is same as stdout.
 *
 *      format - in, use
 *              printf-style format string for output
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SS_CDECL SsFprintf(SS_FILE* fp, const char* format, ...)
{
        va_list ap;

        if (fp == (SS_FILE*)-1L) {
            return;
        }

        va_start(ap, format);

        if (fp == NULL) {
            SsDbgVprintfLevel(0, FALSE, (char *)format, ap);
        } else {
            SsVfprintf(fp, (char *)format, ap);
        }

        va_end(ap);
}

/*#***********************************************************************\
 *
 *              default_assert_message
 *
 *
 *
 * Parameters :
 *
 *      msg -
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
static void default_assert_message(const char* msg)
{
        SsPrintf(msg);
}

/*##**********************************************************************\
 *
 *              SsLogMessage
 *
 * Log message to a given file.
 *
 * Parameters :
 *
 *      fname -
 *
 *
 *      bakfname -
 *
 *
 *      maxsize -
 *
 *
 *      text -
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
void SsLogMessage(char* fname, char* bakfname, long maxsize, char* text)
{
        SS_FILE* fp;

        /* for diskless, send the error message to stderr. also do that if disable_output is true */
        if (msglog_diskless || disable_output) {
            fp = SsStderr;
        } else {
            if (SsFSize(fname) > maxsize) {
                SsFRemove(bakfname);
                SsFRename(fname, bakfname);
            }

            fp = SsFOpenT(fname, (char *)"a+");
        }

        if (fp != NULL) {
            SsTimeT curtime;
            char ctimebuf[SS_CTIME_BUFLEN];

            curtime = SsTime(NULL);
            SsFPuts(SsCtime(&curtime, ctimebuf, SS_CTIME_BUFLEN), fp);
            SsFPuts(text, fp);
            SsFPuts("\n", fp);
            SsFFlush(fp);
            ss_debug(SsMemTrcFprintCallStk(fp, NULL, NULL));
            if (!msglog_diskless && !disable_output) {
                SsFClose(fp);
            }
       }
}


void SsLogMessageAndCallstack(char* fname, char* bakfname, long maxsize, char* text, void* pcallstack)
{
        FILE* fp;

        /* for diskless, send the error message to stderr. also do that if disable_output is true */
        if (msglog_diskless || disable_output) {
            fp = stderr;
        } else {
            if (SsFSize(fname) > maxsize) {
                SsFRemove(bakfname);
                SsFRename(fname, bakfname);
            }

            fp = SsFOpenT(fname, (char *)"a+");
        }

        if (fp != NULL) {
            SsTimeT curtime;
            char ctimebuf[SS_CTIME_BUFLEN];

            curtime = SsTime(NULL);

            fputs(SsCtime(&curtime, ctimebuf, SS_CTIME_BUFLEN), fp);
            fputs(text, fp);
            fputs("\n", fp);
            fflush(fp);
            SsMemTrcFprintCallStk(fp, NULL, pcallstack);
            if (!msglog_diskless && !disable_output) {
                SsFClose(fp);
            }
        }
}


/*##**********************************************************************\
 *
 *              SsLogErrorMessage
 *
 * Logs message to the serror.log file.
 *
 * Parameters :
 *
 *      text -
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
void SsLogErrorMessage(char* text)
{
        SsLogMessage(
            (char *)SS_ERRORLOG_FILENAME,
            (char *)SS_ERRORLOG_BAKFILENAME,
            SS_ERRORLOG_MAXSIZE,
            text);
}

/*#***********************************************************************\
 *
 *              AssertionMessage
 *
 * Prints message using assertion message function but does not exit from
 * the program.
 *
 * Parameters :
 *
 *      text -
 *
 *
 *      file -
 *
 *
 *      line -
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
static void AssertionMessage(char* text)
{
        SsLogErrorMessage(text);

#ifdef SS_DEBUG
        SsDbgSet("/LOG");
        SsMemTrcPrintCallStk(NULL);
#endif /* SS_DEBUG */

        if (SsAssertReportFunction != NULL) {
            (*SsAssertReportFunction)((char *)ss_cmdline, text);
        }
        if (SsAssertMessageFunction != NULL) {
            (*SsAssertMessageFunction)(text);
        } else {
            SsPrintf("%s\n", text);
        }
}

/*##**********************************************************************\
 *
 *              SsAssertionMessage
 *
 * Prints message using assertion message function but does not exit from
 * the program.
 *
 * Parameters :
 *
 *      text -
 *
 *
 *      file -
 *
 *
 *      line -
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
void SsAssertionMessage(text, file, line)
    char* text;
    char* file;
    int   line;
{
        char buf[255];

        SsSprintf(buf, text, file, line);

        AssertionMessage(buf);
}

/*##**********************************************************************\
 *
 *              SsDbgMessage
 *
 *
 *
 * Parameters :
 *
 *      format -
 *
 *
 *      ... -
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
void SS_CDECL SsDbgMessage(const char* format, ...)
{
        SS_FILE* fp;
        va_list ap;

        if (!ss_msg_disableallmessageboxes) {
           va_start(ap, format);
           LocalSsVprintf((char *)format, ap);
           va_end(ap);
        }

        fp = SsFOpenT((char *)"ssdebug.log", (char *)"a+");

        if (fp != NULL) {
            va_start(ap, format);
            SsVfprintf(fp, (char *)format, ap);
            va_end(ap);

            SsFClose(fp);
        }
}

/*##**********************************************************************\
 *
 *              SsAssertionExit
 *
 *
 *
 * Parameters :
 *
 *      text -
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
void SsAssertionExit(char* text)
{
        static bool already_here = FALSE;

        FAKE_CODE_BLOCK(FAKE_SS_NOASSERTEXIT, { return; } );

        if (ss_dbg_jmpbufpos > 0) {
            ss_debug(SsDbgPrintf(text));
            SsLogErrorMessage(text);
            ss_dprintf_1(("longjmp: %s %d\n", __FILE__, __LINE__));
            ss_output_1(SsMemTrcFprintCallStk(NULL, NULL, NULL));
            longjmp(ss_dbg_jmpbuf[ss_dbg_jmpbufpos-1], -1);
        }

        if (!already_here) {
            already_here = TRUE;
            SsDbgSetErrorExit(text);
            AssertionMessage(text);
            ss_svc_stop(TRUE);
            SsErrorExit();
        } else {
            SsThrSleep(100);    /* Give time for the first assert to proceed. */
        }
}

/*##**********************************************************************\
 *
 *              SsAssertionFailure
 *
 * Handler for assertion error situation. Displays error message and exits
 * the program with error code.
 *
 * Parameters :
 *
 *      file - in, use
 *              source file name
 *
 *      line - in
 *              source line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsAssertionFailure(file, line)
    char* file;
    int line;
{
#if 0
        SsAssertionFailureText((char *)"Assertion failure: file %s, line %d\n", file, line);
#else
        SsAssertionFailureText((char *)"Status: %d@%s\n", file, line);
#endif
}

/*##**********************************************************************\
 *
 *              SsRcAssertionFailure
 *
 * Handler for assertion error situation with error code. Displays error
 * message and exits the program with error code.
 *
 * Parameters :
 *
 *      file - in, use
 *              source file name
 *
 *      line - in
 *              source line number
 *
 *      rc - in
 *              error code
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsRcAssertionFailure(file, line, rc)
    char* file;
    int line;
    int rc;
{
        char buf[80];

#if 0
        SsSprintf(
            buf,
            "Assertion failure: file %s, line %d\nCode: %d\n",
            file,
            line,
            rc);
#else
        SsSprintf(
            buf,
            "Status: %d@%s\nCode: %d\n",
            line,
            file,
            rc);
#endif

        SsAssertionFailureText(buf, file, line);
}

/*##**********************************************************************\
 *
 *              SsInfoAssertionFailure
 *
 * Handler for assertion error situation with error info text. Displays error
 * message and exits the program with error code.
 *
 * Parameters :
 *
 *      file - in, use
 *              source file name
 *
 *      line - in
 *              source line number
 *
 *      info_text - in
 *              info text
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsInfoAssertionFailure(char* file, int line, char* info)
{
        char buf[512];

        if (info == NULL) {
            info = (char *)"";
        }
        SsSprintf(
            buf,
            "Status: %d@%s\nInfo: %.256s\n",
            line,
            file,
            info);

        SsAssertionFailureText(buf, file, line);
}

char* SS_CDECL SsInfoAssertionFailureText(const char* format, ...)
{
        static char buf[512];
        va_list ap;

        va_start(ap, format);
        vsprintf(buf, (char *)format, ap);
        va_end(ap);
        return buf;
}


/*##**********************************************************************\
 *
 *              SsAssertionFailureText
 *
 * Handler for assertion error situation. Displays given message and exits
 * the program with error code.
 *
 * Parameters :
 *
 *      text - in, use
 *          Explanation text, should contain %s, %d for file and line.
 *
 *      file - in, use
 *              source file name
 *
 *      line - in
 *              source line number
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsAssertionFailureText(text, file, line)
    char* text;
    char* file;
    int   line;
{
        static bool already_here = FALSE;
        char dt[20];
#ifdef SS_STACK_TRACE
#ifdef SS_DEBUG
        char *cp;
#endif /* SS_DEBUG */
#endif /* SS_STACK_TRACE */

        if (!already_here) {
            static char buf[1024 + 512];
            char* p;
            char* sqlstr;

            already_here = TRUE;

            strcpy(buf, "");
            p = buf;
#if 0
            /* This works, but some information in the message is
             * duplicated.  Maybe we should change the default or...
             */
            if (SsAssertMessageFunction == default_assert_message) {
                /* User has not set an own assert message handler,
                 * so we print the header from here.
                 */
                strcat(buf, SsAssertMessageHeader);
                if (buf[strlen(buf) - 1] != '\n') {
                    strcat(buf, "\n");
                }
                p += strlen(p);
            }
#endif /* 0 */
#if 0
            strcat(p, "Solid internal error, please report the following information:\n");
#else
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            strcat(p,
"Please file a bug report at http://dev.soliddb.com/bug/\n\
to report the following information to\n\
Solid Information Technology <techsupp@solidtech.com>.\n");
#else
            strcat(p,
"Please report the following information\n\
to Solid Information Technology <techsupp@solidtech.com>.\n");
#endif
#endif

            p += strlen(p);
            SsSprintf(p, text, line, file);
            p += strlen(p);
            if (buf[strlen(buf) - 1] != '\n') {
                strcat(buf, "\n");
            }
            SsPrintDateTime(dt, 20, SsTime(NULL));
            p += strlen(p);
            SsSprintf(p, "Date: %s\n", dt);

            p += strlen(p);
            SsSprintf(p, "Product: %s\nVersion: %s\nOperating system: %s\n",
                SS_SERVER_NAME,
                SS_SERVER_VERSION,
                SsEnvNameCurr());
            p += strlen(p);
            if (SsCommunicationProtocol != NULL) {
                SsSprintf(p, "Communication protocol: %s\n", SsCommunicationProtocol);
                p += strlen(p);
            }
            sqlstr = SsSQLTrcGetStr();
            if (sqlstr != NULL) {
                SsSprintf(p, "SQL: %.512s\n", sqlstr);
                p += strlen(p);
            }

#ifdef SS_STACK_TRACE
#ifdef SS_DEBUG
            cp = SsStackTrace();
            if (cp != NULL) {
                SsSprintf(p, "Stack backtrace:\n%s", cp);
                p += strlen(p);
            }
#endif /* SS_DEBUG */
#endif /* SS_STACK_TRACE */

#ifdef FUTURE
            SsSprintf(p, "(This information should also be found in file %s)\n",
                      SS_ERRORLOG_FILENAME);
            p += strlen(p);
#endif /* FUTURE */
            ss_dassert(strlen(buf) < sizeof(buf));
            SsAssertionExit(buf);
        } else {
            SsThrSleep(100);    /* Give time for the first assert to proceed. */
            SsAssertionExit((char *)"");
        }
}

/*##**********************************************************************\
 *
 *              SsSetAssertMessageFunction
 *
 *
 *
 * Parameters :
 *
 *      func -
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
ss_assertmessagefunc_t SsSetAssertMessageFunction(void (*func)(const char *msg))
{
        ss_assertmessagefunc_t oldfunc;

        oldfunc = (ss_assertmessagefunc_t)SsAssertMessageFunction;
        SsAssertMessageFunction = func;
        return(oldfunc);
}

ss_assertreportfunc_t SsSetAssertReportFunction(void (*func)(char* cmdline, char* msg))
{
        ss_assertreportfunc_t oldfunc;

        oldfunc = (ss_assertreportfunc_t)SsAssertReportFunction;
        SsAssertReportFunction = func;
        return(oldfunc);

}

/*##**********************************************************************\
 *
 *              SsSetAssertMessageHeader
 *
 *
 *
 * Parameters :
 *
 *      header -
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
void SsSetAssertMessageHeader(header)
        char* header;
{
        SsAssertMessageHeader = header;
}

/*##**********************************************************************\
 *
 *              SsGetAssertMessageHeader
 *
 *
 *
 * Parameters :
 *
 *      header -
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
char* SsGetAssertMessageHeader()
{
        return(SsAssertMessageHeader);
}

/*##**********************************************************************\
 *
 *              SsSetAssertMessageProtocol
 *
 *
 *
 * Parameters :
 *
 *      protocol - in, hold
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
void SsSetAssertMessageProtocol(char* protocol)
{
        SsCommunicationProtocol = protocol;
}

/*##**********************************************************************\
 *
 *              SsErrorMessage
 *
 * Functions for an error message. The default behaviour logs the message
 * to the sserror.log file, if SsSetErrorMessageFunction is not used
 * to set the message function.
 *
 * Parameters :
 *
 *      msgcode - in
 *          Message code that identifies each message.
 *          Message format will be retrieved based on that code.
 *          If 0 then next argument represents the message format
 *
 *      ...     - in
 *          variable number of arguments to format string
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL SsErrorMessage(int msgcode, ...)
{
        va_list ap;
        char buf[512];
        char* msgtext;
        char* format;

        if(msgcode != 0){
            /* retrieve a message format that maps to the msgcode */
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number") != 0);
            va_start(ap, msgcode);
            msgtext = su_rc_vgivetext(msgcode, ap);
            va_end(ap);
            strcpy(buf, msgtext);
            SsMemFree(msgtext);
        } else {
            /* the next argument represents the message format... */
            va_start(ap, msgcode);
            format = va_arg(ap, char*);
            vsprintf(buf, format, ap);
            va_end(ap);
        }
        SsLogErrorMessage(buf);
        if (SsErrorMessageFunction != NULL) {
            (*SsErrorMessageFunction)(buf);
        }
}

/*##**********************************************************************\
 *
 *              SsSetErrorMessageFunction
 *
 * Sets the function that is used to report error messages.
 *
 * Parameters :
 *
 *      func -
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
void SsSetErrorMessageFunction(void (*func)(char* text))
{
        SsErrorMessageFunction = func;
}

/*##**********************************************************************\
 *
 *              SsErrorExit
 *
 * Exits the program with error code. Stderr and SsStdout files are flushed
 * to ensure that possible buffered output is written. In debug Unix
 * version, this function generates also core file.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsErrorExit(void)
{
        static bool already_here = FALSE;
        ErrorExitListT* prev;

        if (!already_here) {
            already_here = TRUE;

            while (ErrorExitList != NULL) {
                (*ErrorExitList->el_func)();
                prev = ErrorExitList;
                ErrorExitList = ErrorExitList->el_next;
                SsMemFree(prev);
            }
        }

        SsDbgSet("/ERROREXIT");

        ss_debug(ss_debug_info.dbg_assertstop = TRUE;);

#if defined(SS_COREOPT) || defined(ASSERTCORE) || defined(AUTOTEST_RUN) || (defined(SS_DEBUG) && (defined(UNIX) || defined(SS_NT)))
        /* In unix, it is nice to generate a core dump
           from the error situation. Some of the following
           pointers must surely be outside the process's
           address space.
           In emx/gcc for OS/2 we can generate a core file, too.
           In NT, we can start debugger after a trap.
        */
        if (ss_doerrorcore
#if defined(SS_COREOPT)            
            && ss_coreopt
#endif
            ) {
            char* ptr1 = NULL;
            char* ptr2 = (char*)0x80000000;
            char* ptr3 = (char*)0x40000000;
            char* ptr4 = (char*)0xffff0000;
            char* ptr5 = (char*)0x00b00000;
            static char x;

            if (!ss_debug_info.dbg_nocorep) {
                ss_debug_info.dbg_nocorep = TRUE;
                x = x + *ptr1 + *ptr2 + *ptr3 + *ptr4 + *ptr5;
            }
        }

#else /* SS_DEBUG && (UNIX || NT) */
#endif /* SS_DEBUG && (UNIX || NT ) */
        SsExit(SS_EXE_FATALERROR);
}

/*##**********************************************************************\
 *
 *              SsAtErrorExit
 *
 *
 *
 * Parameters :
 *
 *      func -
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
void SsAtErrorExit(void (*func)(void))
{
        ErrorExitListT* NewErrorExitList;

        NewErrorExitList = SSMEM_NEW(ErrorExitListT);

        NewErrorExitList->el_func = func;
        NewErrorExitList->el_next = ErrorExitList;

        ErrorExitList = NewErrorExitList;
}

void SsFreeErrorExitList()
{
        ErrorExitListT* prev;

        while (ErrorExitList != NULL) {
            prev = ErrorExitList;
            ErrorExitList = ErrorExitList->el_next;
            SsMemFree(prev);
        }
}


/*##**********************************************************************\
 *
 *              SsExit
 *
 *
 *
 * Parameters :
 *
 *      value -
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
void SsExit(value)
        int value;
{
        SsBrk();

        SsDbgSet("/NOL");
        SsSysResGlobalDone();
#if defined(SS_WIN)
        SS_NOTUSED(value);
        abort();
#else
        exit(value);
#endif
}

/*##**********************************************************************\
 *
 *              SsBrk
 *
 * Dummy function to act as a debugger breapoint function just before
 * the program is exited.
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
void SsBrk()
{
}

/*##**********************************************************************\
 *
 *              SsDbgInit
 *
 * Initializes the debugging variables from the environment variable
 * SS_DEBUG.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsDbgInit()
{
        SsGlobalInit();

        if (!ss_debug_info.dbg_init) {
#ifdef SS_DEBUG
            SsDbgSet(NULL);
#else /* SS_DEBUG */
            SsDbgSet(SsGetEnv("SS_DEBUG"));
#endif /* SS_DEBUG */
        }
}

/*##**********************************************************************\
 *
 *              SsDbgCheckAssertStop
 *
 * Checks if stop on assert is requested.
 *
 * Parameters :
 *
 *      none
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SsDbgCheckAssertStop(void)
{
#ifdef SS_DEBUG
        while (ss_debug_info.dbg_assertstop) {
            SsThrSleep(1000L);
        }
#endif /* SS_DEBUG */
}

/*##**********************************************************************\
 *
 *              SsDbgInitDll
 *
 * Initializes the debugging variables from the environment variable
 * SS_DEBUG. This function should be used to initialize the debug system
 * in a DLL.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void SsDbgInitDll()
{
        SsGlobalInit();

        if (!ss_debug_info.dbg_init) {
            SsDbgSetDebugFile((char *)"ssdebugd.out");
            SsDbgInit();
        }
}

/*#***********************************************************************\
 *
 *              LocaleInit
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void LocaleInit(void)
{
#if defined(SS_SCO)
        setlocale(LC_NUMERIC, "C");
#endif /* SS_SCO */
}

/*#***********************************************************************\
 *
 *              SsAtExitCleanup
 *
 * The OS calls this function when the program is about to die.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void SS_CLIBCALLBACK SsAtExitCleanup(void)
{
        if (!ss_skipatexit) {
            SsTimerGlobalDone();
            SsSysResGlobalDone();
        }
}

/*##**********************************************************************\
 *
 *              SsGlobalInit
 *
 *
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
void SsGlobalInit()
{
        static bool initp = FALSE;

        if (!initp) {
            /* char* ptr=NULL; *ptr = 1; */
            initp = TRUE;
            SsSysResGlobalInit();
            if (!ss_skipatexit) {
                /* Without atexit, for Uniface use */
                atexit(SsAtExitCleanup);
            }
            LocaleInit();
            SS_NUMUTI_INIT;
            SsThrGlobalInit();
            SsMemGlobalInit();
            SsSemInitLibSem();
            SsMsgLogGlobalInit();
            SsPmonInit();
            SsRtCovInit();
            SsCPUTest();
            ss_testlog_dummy();

        }
}

char* SsGetVersionstring(bool islocal)
{
        static char versionstring[180];

        if (versionstring[0] == '\0') {
            char    envstr[40];
            static  const char* localtext = "Local";
            char* osname = SsEnvNameCurr(); /* SS_ENV_OSNAME */

#ifdef SS_MYSQL
            SsSprintf(versionstring,
                        "%s %s - %s %s",
                        SS_SERVER_NAME,
                        SS_SOLIDDB_SERVER_VERSION,
                        SS_SERVER_VERSION,
#ifdef SS_MYSQL_ENTER
                        SS_SERVER_ENTER
#else
                        " "
#endif
                );
#else /* ! SS_MYSQL */
            
            SsSprintf(versionstring,
                        "%s - v.%s",
                        SS_SERVER_NAME,
                        SS_SERVER_VERSION);
#endif
            if (islocal) {
                ss_dassert(strlen(osname)+strlen(localtext)+1 < 40);
                SsSprintf(envstr, " (%s, %s)", osname, localtext);
            } else {
                SsSprintf(envstr, " (%s)", osname);
            }
            strcat(versionstring, envstr);
            ss_dassert(strlen(versionstring) < 180);
        }
        return(versionstring);
}


/*##**********************************************************************\
 *
 *              SsDbgSetDebugFile
 *
 * Sets the default log file name. Must be called before any logged
 * debug messages.
 *
 * Parameters :
 *
 *      fname -
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
void SsDbgSetDebugFile(char* fname)
{
        if (strlen(fname) < 254) {
            strcpy(ss_debug_info.dbg_logfilename, fname);
#ifdef SS_UNIX
            {
                /* Convert backslash to slash. Needed because
                 * SS_DEBUG string cannot have a slash in file
                 * names.
                 */
                int i;
                int len;
                len = strlen(ss_debug_info.dbg_logfilename);
                for (i = 0; i < len; i++) {
                    if (ss_debug_info.dbg_logfilename[i] == '\\') {
                        ss_debug_info.dbg_logfilename[i] = '/';
                    }
                }
            }
#endif /* SS_UNIX */
        }
}

/*##**********************************************************************\
 *
 *              ss_vers_ispurify
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int ss_vers_ispurify()
{
#ifdef SS_PURIFY
        return(1);
#else
        return(0);
#endif
}


#if 1

void ss_plog_insert(char type, char* test_name, double time, char* note)
{
        SS_FILE* plogfp;
        char* plogfn;
        char* addprefix;

        if ((plogfn = SsGetEnv("SOLPERFLOGFILE")) != NULL &&
            (plogfp = SsFOpenT(plogfn, (char *)"a+")) != NULL) {

            switch (type) {
                case PLOG_COMMENT:
                    SsFprintf(plogfp, "#%c\t%s\n",
                                    type,
                                    ((note == NULL) ? "" : note));
                    break;
                case PLOG_TEST_START:
                    addprefix = SsGetEnv("PTESTPREFIX");
                    SsFprintf(plogfp, "#%c\t%s%s\t%s\n",
                                    type,
                                    addprefix == NULL ? "" : addprefix,
                                    test_name == NULL ? "" : test_name,
                                    note == NULL ? "" : note);
                    break;
                case PLOG_TEST_END:
                    SsFprintf(plogfp, "#%c\t%s\t%.2f",
                                    type,
                                    test_name == NULL ? "" : test_name,
                                    time
                                    );
                    /* ss_plog_fprint_hms(plogfp, time); */
                    SsFprintf(plogfp, "\t%s\n", note == NULL ? "" : note);
                    break;
                case PLOG_PARTIAL_RESULT:
                    SsFprintf(plogfp, "#%c\t%s\t%.2f",
                                    type,
                                    test_name == NULL ? "" : test_name,
                                    time
                                    );
                    /* ss_plog_fprint_hms(plogfp, time); */
                    SsFprintf(plogfp, "\t%s\n", note == NULL ? "" : note);
                    break;
                default:
                    ss_error;
            }
            SsFClose(plogfp);
        }
}
void ss_plog_fprint_hms(SS_FILE* fp, double d)
{
        if (d < 60.0) {
            return;
        }
        SsFprintf(fp, " (");

        if (d > 3600.0) {
            /* Print hours. */
            long h;
            h = (long)(d / 3600.0);
            SsFprintf(fp, "%ldh ", h);
            d = d - (double)h * 3600.0;
        }
        if (d > 60.0) {
            /* Print mins. */
            long m;
            m = (long)(d / 60.0);
            SsFprintf(fp, "%ldm ", m);
            d = d - (double)m * 60.0;
        }
        SsFprintf(fp, "%lds)", (long)d);

}
#endif
