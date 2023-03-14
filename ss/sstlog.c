/*************************************************************************\
**  source       * sstlog.c
**  directory    * ss
**  description  * Utility functions used by test logging
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


#include <stdio.h>
#include <time.h>
#include <string.h>

#include "ssc.h"
#include "sswinnt.h"
#include "sswinint.h"
#include "ssdebug.h"
#include "ssthread.h"
#include "ssgetenv.h"
#include "sschcvt.h"
#include "sstime.h"
#include "sstlog.h"
#include "ssfile.h"

#define TMP_SIZE 512

#ifndef SS_MYSQL
static bool update_timedb(long test_time,long *p2,long *p3);
static char* make_tmp_tdbfname(void);
static char* make_tdbfname(void);
static uint scan_str(char *line, long *t1, long *t2, long *t3, long *t4);
#endif /* !SS_MYSQL */

static char* make_logfname(void);
static void set_test_name(char *argv[]);
static char* strip_path(char *fname);
static void strip_extension(char *fname);
static void add_fname(char *fname);
static void set_cmd_line(char *argv[]);
static void test_atexit_done(void);
static void testlog_assert(const char* msg);
static void test_done(int rc);

static SsTimeT  start_time;
static char* log_fname = NULL;

#if defined(SS_PURIFY) && defined(SS_UNIX)
char ss_test_name[TMP_SIZE] = "\0";
#else 
static char  ss_test_name[TMP_SIZE] = "\0";
#endif 

static char  test_cmd_line[TMP_SIZE] = "\0";
static char  assert_msg[TMP_SIZE] = "\0";

#define THRESHOLD 20


/*##**********************************************************************\
 * 
 *		ss_testlog_init
 * 
 * Initializes test logging system (i.e. prints assert messages and
 * optionally performance data to the file). 
 * 
 * Parameters : 
 * 
 *	argv - 
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void ss_testlog_init(char* argv[])
{

        set_cmd_line(argv);
        if (SsGetEnv("SOLMAKELOG") != NULL) {

            SS_INIT_DEBUG;
            log_fname = make_logfname();
            ss_assert(log_fname);

            set_test_name(argv);
            SsAtErrorExit(test_atexit_done);
            SsSetAssertMessageFunction(testlog_assert);

            start_time = SsTime(NULL);

        }
}

/*##**********************************************************************\
 * 
 *		ss_testlog_done
 * 
 *  Ends test logging (i.e. prints performance data to the file)
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
void ss_testlog_done()
{
        test_done(0);
}
void ss_testlog_dummy()
{
        return;
}
char* ss_testlog_gettestname(void)
{
        return ss_test_name;        
}
static void test_done(int rc)
{
        char* p;
        char* log_all;
        bool  ok;
        SS_FILE* fp;
        SsTimeT end_time, running_time;

#ifdef SS_DEBUG
        extern long memchk_nptr __attribute__ ((unused));
#endif /* SS_DEBUG */

        if (log_fname == NULL) {
            return;
        }
        if (SsGetEnv("SOLNONSTOP") == NULL) {
            fp = SsFOpenT(log_fname, (char *)"a+");
        } else {
            fp = SsFOpenT(log_fname, (char *)"w+");
        }
        if (fp == NULL) {
            SsPrintf("Cannot open testlog '%s'\n",log_fname);
            SsPrintf("Check that SSLOGFILEPATH and SSEXECTIMESPATH are properly set\n");
            /* SsFreeErrorExitList(); */
            SsExit(-1);
        }
        log_all = SsGetEnv("SOLLOGTESTNAMES");
        ok = TRUE;
        
        if (rc == 0) {
            if (log_all != NULL
              && strncmp(ss_test_name, "sollog", 6) != 0 
              && strncmp(ss_test_name, "tdiff", 5) != 0 
              && strncmp(ss_test_name, "solcon", 6) != 0) {
                end_time = SsTime(NULL);
                running_time = end_time - start_time;

                if (strcmp(log_all, "excel") == 0) {
                    SsFPrintf(fp, "%s,%ld\n", ss_test_name, running_time);
                } else {
                    SsFPrintf(fp, "Program ended in ");
                    if (running_time > 3600) {
                        /* Print hours. */
                        long h;
                        h = running_time / 3600;
                        SsFprintf(fp, "%ldh ", h);
                        running_time = running_time - h * 3600;
                    }
                    if (running_time > 60) {
                        /* Print mins. */
                        long m;
                        m = running_time / 60;
                        SsFprintf(fp, "%ldm ", m);
                        running_time = running_time - m * 60;
                    }
                    SsFPrintf(fp, "%lds", running_time);
                    SsFPrintf(fp, " : '%s'\n", ss_test_name);
                }

            }
        } else {
            for (p = ss_test_name; *p != '\0'; p++) {
                if (*p == '_') {
                    *p = ' ';
                }
            }
            SsFPrintf(fp, "%s\n", ss_test_name);
            SsFPrintf(fp, "status:\tASSERT\n");
            SsFPrintf(fp, "%s\n", assert_msg);
            ok = FALSE;
        }
        SsFClose(fp);
        /* SsFreeErrorExitList(); */
        free(log_fname);
        if (rc != 0) {
            SsExit(0);
        }
}

static SS_FILE* testlog_open(void)
{
        SS_FILE* fp;

        if (log_fname == NULL) {
            return(NULL);
        }

        fp = SsFOpenT(log_fname, (char *)"a+");

        if (fp == NULL) {
            SsPrintf("Cannot open testlog '%s'\n",log_fname);
            SsPrintf("Check that SSLOGFILEPATH and SSEXECTIMESPATH are properly set\n");
            /* SsFreeErrorExitList(); */
            SsExit(-1);
        }
        return(fp);
}

void ss_testlog_print(char* str, ...)
{
        va_list ap;
        SS_FILE* fp;

        fp = testlog_open();
        if (fp == NULL) {
            return;
        }
        va_start(ap, str);
        SsVfprintf(fp, str, ap);
        va_end(ap);
        SsFClose(fp);
}

#ifdef SS_DEBUG

void ss_testlog_printcallstack(char** callstack)
{
        SS_FILE* fp;

        fp = testlog_open();
        if (fp == NULL) {
            return;
        }
        SsMemTrcFprintCallStk(fp, callstack, NULL);
        SsFClose(fp);
}

#endif /* SS_DEBUG */

#if 0
static bool update_timedb(long time,long *test_avg,long *test_prev)
{
        char* tdbfn;
        char* tmpfn;
        char name[200];
        char comment[20];
        char line[200];
        long t1, t2, t3, t4;
        double perc1, perc2, avg;
        SS_FILE *infp, *outfp;
        bool found, note;
        uint len;

        tdbfn = make_tdbfname();
        infp  = SsFOpenT(tdbfn,"r");

        if (infp == NULL) {
            outfp = SsFOpenT(tdbfn,"w");
            ss_assert(outfp != NULL);
            SsFPrintf(outfp,"%s %ld %ld %ld %ld\n",ss_test_name,time,time,time,time);
            free(tdbfn);
            SsFClose(outfp);
            return FALSE;
        }
        tmpfn = make_tmp_tdbfname();
        outfp = SsFOpenT(tmpfn,"w");
        ss_assert(outfp != NULL);

        found = FALSE;
        note  = FALSE;

        while (SsFGets(line,150,infp) !=NULL) {
            len = scan_str(line,&t1,&t2,&t3,&t4);
            strncpy(name,line,len);
            name[len] = '\0';
            
            if (SsStrcmp(ss_test_name,name) == 0) {
                
                avg = (t1 + t2 + t3 + t4) / 4.0;
                if (avg == 0) {
                    perc1 = ((avg - time) / 1e-50) * 100;
                 } else {
                    perc1 = ((avg - time) / avg) * 100;
                }
                if (t4 == 0) {
                    perc2 = ((time - t4) / 1e-50) * 100;
                } else {
                    perc2 = ((time - t4) / (double)t4) * 100;
                }
                
                if (perc1 > THRESHOLD || perc1 < -THRESHOLD ||
                    perc2 > THRESHOLD || perc2 < -THRESHOLD) {
                    sprintf(comment,"NOTE !!!!");
                    note = TRUE;
                } else {
                    comment[0] = '\0';
                }
                *test_avg =  (long)avg;
                *test_prev = (long)t4;

                SsFPrintf(outfp,"%s %ld %ld %ld %ld",name,t2,t3,t4,time);
                SsFPrintf(outfp," %s\n",comment);
                found = TRUE;
            } else {
                SsFPrintf(outfp,line);
            }
        }
        if (found == FALSE) {
            SsFPrintf(outfp,"%s %ld %ld %ld %ld\n",ss_test_name,time,time,time,time);
        }
        SsFClose(infp);
        SsFClose(outfp);

        remove(tdbfn);
        rename(tmpfn,tdbfn);
        free(tdbfn);
        free(tmpfn);
        return note;
}

static uint scan_str(char *line,long *t1,long *t2,long *t3,long *t4)
{
        char* p;
        int   state;
        uint  len;

        ss_assert(line != NULL);
        *t1 = *t2 = *t3 = *t4 = 0;

        state = 0;
        p = line;
        for( ; *p != '\0'; p++) {
            if (ss_isspace(*p)) {
                switch (state) {
                    case 0:
                        *t1 = atol(p);
                        len = p - line;
                        state++;
                        break;
                    case 1:
                        *t2 = atol(p);
                        state++;
                        break;
                    case 2:
                        *t3 = atol(p);
                        state++;
                        break;
                    case 3:
                        *t4 = atol(p);
                        state++;
                        break;
                    case 4:
                        break;
                    default:
                        ss_error;
                }
            }
        }
        return len;
}
#endif
static char *strip_path(char *fn)
{
        int     i;
        size_t  len;

        len = strlen(fn);
        for (i = len-1;i > 0; i--) {
            if (fn[i] == '\\' || fn[i] == '/') {
                return &fn[i+1];
            }
        }
        return fn;
}

static void strip_extension(char *fn)
{
        int     i;
        size_t  len;

        len = strlen(fn);
        for (i = len-1;i > 0; i--) {
            if (fn[i] == '\\' || fn[i] == '/') {
                break;
            }
            if (fn[i] == '.') {
                fn[i] = '\0';
                break;
            }
        }
        return;
}
#if 0
static char* make_tmp_tdbfname(void)
{
        char    *fn;
        size_t  len;

        fn = make_tdbfname();
        ss_assert(fn);
        len = strlen(fn);
        sprintf(&fn[len-3], "BDT");
        return fn;
}
#endif

#ifndef SS_MYSQL
static char* make_tdbfname(void)
{
        char    *tmp_fn, *fn;
        size_t  len;

        if ((fn = SsGetEnv("SSEXECTIMESPATH")) == NULL) {
            SsPrintf("Testlog error: SSEXECTIMESPATH not properly set\n");
            SsExit(-1);
        }
        if ((len = strlen(fn)) <= 0) {
            SsPrintf("Testlog error: SSEXECTIMESPATH not properly set\n");
            SsExit(-1);
        }
        tmp_fn = malloc(len + 15);
        ss_assert(tmp_fn);
        strcpy(tmp_fn,fn);
      
        if (fn[len-1] != '\\' && fn[len-1] != '/') {
#ifdef SS_UNIX
            strcat(tmp_fn, "/");
#else
            strcat(tmp_fn, "\\");
#endif
        }
        add_fname(tmp_fn);
        
        strcat(tmp_fn,".tdb");
        return tmp_fn;
}
#endif /* !SS_MYSQL */

static char *make_logfname(void)
{
        char    *tmp_fn,*fn;
        size_t  len;

        if ((fn = SsGetEnv("SSLOGFILEPATH")) == NULL) {
            SsPrintf("Testlog error: SSLOGFILEPATH not properly set\n");
            SsExit(-1);
        }
        if ((len = strlen(fn)) <= 0) {
            SsPrintf("Testlog error: SSLOGFILEPATH not properly set\n");
            SsExit(-1);
        }
        tmp_fn = malloc(len+15);
        ss_assert(tmp_fn);
        strcpy(tmp_fn,fn);
      
        if (fn[len-1] != '\\' && fn[len-1] != '/') {
#ifdef SS_UNIX
            strcat(tmp_fn,"/");
#else
            strcat(tmp_fn,"\\");
#endif
        }
        add_fname(tmp_fn);
        
        strcat(tmp_fn,".log");
        return tmp_fn;
}


static void add_fname(char *fname)
{
        strcat(fname,"sol");
        strcat(fname, SsEnvTokenCurr());
}

static void set_test_name(char *argv[])
{
        int i;
        char *p;
        size_t len;

        i = 0;
        len = 0;
        ss_test_name[0] = '\0';

        while (argv[i] != NULL) {
            len += (strlen(argv[i]) + 1);
            if (len >= TMP_SIZE) {
                return;
            }
            if (i == 0) {
                p = strip_path(argv[0]);
                strcat(ss_test_name, p);
                strip_extension(ss_test_name);
            } else {
                strcat(ss_test_name, "_");
                strcat(ss_test_name, argv[i]);
            }
            i++;
        }
        SsStrlwr(ss_test_name);
}

static void set_cmd_line(char *argv[])
{
        int     i;
        char*   p;

        i = 0;
        test_cmd_line[0] = '\0';

        while (argv[i] != NULL) {
            if (strlen(test_cmd_line) + strlen(argv[i]) + 2 > sizeof(test_cmd_line)) {
                /* Forget the rest */
                break;
            }
            if (i == 0) {
                p = strip_path(argv[0]);
                strcat(test_cmd_line, p);
                strip_extension(test_cmd_line);
            } else {
                strcat(test_cmd_line, " ");
                strcat(test_cmd_line, argv[i]);
            }
            i++;
        }
        SsStrlwr(test_cmd_line);
        strncpy(ss_cmdline, test_cmd_line, ss_cmdline_maxlen);
}


static void test_atexit_done(void)
{
        test_done(3);
}
static void testlog_assert(const char* msg)
{
        strncpy(assert_msg, msg, TMP_SIZE);
        assert_msg[TMP_SIZE - 1] = '\0';
        SsPrintf("%s", msg);
}

