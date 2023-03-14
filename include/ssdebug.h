/*************************************************************************\
**  source       * ssdebug.h
**  directory    * ss
**  description  * Debugging and assertion definitions.
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


#ifndef SSDEBUG_H
#define SSDEBUG_H

#include "ssenv.h"

#include "ssstdio.h"

/* MH Added. hopefully does not break anything */
#define SS_INCLUDE_VARARGFUNS


#ifdef SS_INCLUDE_VARARGFUNS
#include "ssstdarg.h"
#endif

#include "ssc.h"
#include "ssmemtrc.h"
#include "sssqltrc.h"
#include "ssfake.h"
#include "ssrtcov.h"

#if defined(SS_PURIFY) && defined(SS_NT)
#include "pure.h"
#endif

#if SS_POINTER_SIZE == 8

#define SS_ILLEGAL_PTR ((void*)(((ss_ptr_as_scalar_t)0xDeadBeef << 32) | __LINE__))

#elif SS_POINTER_SIZE == 4

#define SS_ILLEGAL_PTR ((void*)(((ss_ptr_as_scalar_t)0xDead << 16) | (ss_uint2_t)__LINE__))

#else /* SS_POINTER_SIZE */

#error weird SS_POINTER_SIZE

HogiHogi !!!!!!!!!!

#endif

/* Check enum base values, one for each compilation directory */
#define SS_CHKBASE_SS       1000U
#define SS_CHKBASE_COVINI   2000U
#define SS_CHKBASE_UTI      3000U
#define SS_CHKBASE_SU       4000U
#define SS_CHKBASE_UI       5000U
#define SS_CHKBASE_SYS      6000U
#define SS_CHKBASE_SQL      7000U
#define SS_CHKBASE_DT       8000U
#define SS_CHKBASE_RES      9000U
#define SS_CHKBASE_DBE      10000U
#define SS_CHKBASE_XS       11000U
#define SS_CHKBASE_TU       12000U
#define SS_CHKBASE_SOLUTI   13000U
#define SS_CHKBASE_SES      14000U
#define SS_CHKBASE_COM      15000U
#define SS_CHKBASE_RPC      16000U
#define SS_CHKBASE_CSS      17000U
#define SS_CHKBASE_EST      18000U
#define SS_CHKBASE_SOR      19000U
#define SS_CHKBASE_TAB      20000U
#define SS_CHKBASE_SRV      21000U
#define SS_CHKBASE_RC       22000U
#define SS_CHKBASE_SP       23000U
#define SS_CHKBASE_SA       24000U
#define SS_CHKBASE_SSE      25000U
#define SS_CHKBASE_CLI      26000U
#define SS_CHKBASE_SL       27000U
#define SS_CHKBASE_UICPP    28000U
#define SS_CHKBASE_RCUI     29000U
#define SS_CHKBASE_SOLSQL   30000U
#define SS_CHKBASE_UNIFACE  31000U
#define SS_CHKBASE_NIST     32000U
#define SS_CHKBASE_SNC      33000U
#define SS_CHKBASE_HSB      34000U
#define SS_CHKBASE_SC       34000U
#define SS_CHKBASE_REXEC    35000U
#define SS_CHKBASE_SYNCML   36000U
#define SS_CHKBASE_MME      37000U
#define SS_CHKBASE_NHAS     38000U


#define SS_CHKBASE_FREED_INCR 500U

#if (SS_POINTER_SIZE == 8)
# define SS_CHKPTR(p)  ((p) != NULL && (SS_NATIVE_UINT8_T)(p) != 0xfefefefefefefefeUL)
#elif (SS_POINTER_SIZE == 4)
# define SS_CHKPTR(p)  ((p) != NULL && (int)(p) != (int)0xfefefefe)
#else
# error Pointer size not defined
#endif

#define SS_ERRORLOG_FILENAME    "solerror.out"

#define SS_DBG_JMPBUF_MAX       32

#ifdef SS_DLL
  /* in ssverinf.tpl */
  char* ss_verinf_buildtime(void);
  char* ss_verinf_solidversno(void);
#endif /* SS_DLL */

/* compile time assert
Compiler should give error if the assertion does not hold.
Unfortunately error from compiler is not very illustrative.

On gcc, for example:
sa0uti.c: In function `SaDateToAsciiz':
sa0uti.c:1099: zero width for bit-field `bf'
*/
#define ss_ct_assert(expr) {struct x { unsigned int bf : expr; };}

/*##**********************************************************************\
 *
 *		ss_assert
 *
 * ss_assert works like the normal assert macro.  At failure, it calls
 * SsAssertionFailure, which calls SsErrorExit.  In Unix,
 * SsErrorExit will generate core file, if it is compiled with
 * SS_DEBUG define.
 *
 * Parameters :
 *
 *      exp - in, use
 *		asserted expression
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_assert(exp)  { if (!(exp)) SsAssertionFailure((char *)__FILE__,__LINE__); }

/*##**********************************************************************\
 *
 *		ss_rc_assert
 *
 * ss_rc_assert works like the normal assert macro.  At failure, it calls
 * SsAssertionFailure, which calls SsErrorExit.  In Unix,
 * SsErrorExit will generate core file, if it is compiled with
 * SS_DEBUG define.
 *
 * Parameters :
 *
 *      exp - in, use
 *		asserted expression
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_rc_assert(exp, rc)  { if (!(exp)) SsRcAssertionFailure((char *)__FILE__,__LINE__, rc); }

/*##**********************************************************************\
 *
 *		ss_info_assert
 *
 * ss_info_assert works like the normal assert macro.  At failure, it calls
 * SsAssertionFailure, which calls SsErrorExit.  In Unix,
 * SsErrorExit will generate core file, if it is compiled with
 * SS_DEBUG define.
 *
 * Parameters :
 *
 *      exp - in, use
 *		asserted expression
 *
 *      info - in, use
 *          additional assertion info
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_info_assert(exp, p)  { if (!(exp)) SsInfoAssertionFailure((char *)__FILE__, __LINE__, SsInfoAssertionFailureText p); }


/*##**********************************************************************\
 *
 *		ss_error
 *
 * Uncontitional version of ss_assert. Equivalent to ss_assert(0).
 * Intended to be used in places that are impossible to reach.
 * An example of such place could be default branch in case
 * statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_error            SsAssertionFailure((char *)__FILE__,__LINE__)

/*##**********************************************************************\
 *
 *		ss_rc_error
 *
 * Uncontitional version of ss_rc_assert. Equivalent to ss_assert(0).
 * Intended to be used in places that are impossible to reach.
 * An example of such place could be default branch in case
 * statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_rc_error(rc)     SsRcAssertionFailure((char *)__FILE__,__LINE__, rc)

/*##**********************************************************************\
 *
 *		ss_pprintf_[1234]
 *
 * Four different debug output macros for product compilation.
 *
 * Identical to ss_dprintf_[1234] macros.
 *
 */
#ifdef SS_COVER
# define ss_pprintf(p) ((ss_debug_level >= 1)) \
                          ? SsDbgPrintfFun1 p : FALSE)
# define ss_pprintf_1(p) ((ss_debug_level >= 1 && SsDbgFileOk((char *)__FILE__)) \
                          ? SsDbgPrintfFun1 p : FALSE)
# define ss_pprintf_2(p) ((ss_debug_level >= 2 && SsDbgFileOk((char *)__FILE__)) \
                          ? SsDbgPrintfFun2 p : FALSE)
# define ss_pprintf_3(p) ((ss_debug_level >= 3 && SsDbgFileOk((char *)__FILE__)) \
                          ? SsDbgPrintfFun3 p : FALSE)
# define ss_pprintf_4(p) ((ss_debug_level >= 4 && SsDbgFileOk((char *)__FILE__)) \
                          ? SsDbgPrintfFun4 p : FALSE)
#else /* SS_COVER */
# define ss_pprintf(p)   { if (ss_debug_level >= 1) \
                            SsDbgPrintfFun1 p; \
                         }
# define ss_pprintf_1(p) { if (ss_debug_level >= 1 && SsDbgFileOk((char *)__FILE__)) \
                            SsDbgPrintfFun1 p; \
                         }
# define ss_pprintf_2(p) { if (ss_debug_level >= 2 && SsDbgFileOk((char *)__FILE__)) \
                            SsDbgPrintfFun2 p; \
                         }
# define ss_pprintf_3(p) { if (ss_debug_level >= 3 && SsDbgFileOk((char *)__FILE__)) \
                            SsDbgPrintfFun3 p; \
                         }
# define ss_pprintf_4(p) { if (ss_debug_level >= 4 && SsDbgFileOk((char *)__FILE__)) \
                            SsDbgPrintfFun4 p; \
                         }
#endif /* SS_COVER */

#  define ss_poutput_1(p)       { if (ss_debug_level >= 1 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_poutput_2(p)       { if (ss_debug_level >= 2 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_poutput_3(p)       { if (ss_debug_level >= 3 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_poutput_4(p)       { if (ss_debug_level >= 4 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }

/* Ignore version to easily remove printfs by just search and replace.
 */
#define ss_noprintf_1(p)
#define ss_noprintf_2(p)
#define ss_noprintf_3(p)
#define ss_noprintf_4(p)
#define ss_noprintf(p)

#ifdef SS_DEBUG

/*##**********************************************************************\
 *
 *		ss_dassert
 *
 * Debug version of ss_assert. There are also versions ss_dassert_[1234]
 * (e.g. ss_dassert_1), which will check the expression only if debug
 * check is greater than or equal to the number in the macro name.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 *      exp - in, use
 *		asserted expression
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_dassert(exp) ss_assert(exp)

/*##**********************************************************************\
 *
 *		ss_rc_dassert
 *
 * Debug version of ss_rc_assert.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 *      exp - in, use
 *		asserted expression
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_rc_dassert(exp, rc) ss_rc_assert(exp, rc)

#define ss_info_dassert(exp, info) ss_info_assert(exp, info)

#define ss_dassert_1(exp) { if (ss_debug_check >= 1) ss_assert(exp); }
#define ss_dassert_2(exp) { if (ss_debug_check >= 2) ss_assert(exp); }
#define ss_dassert_3(exp) { if (ss_debug_check >= 3) ss_assert(exp); }
#define ss_dassert_4(exp) { if (ss_debug_check >= 4) ss_assert(exp); }

/*##**********************************************************************\
 *
 *		ss_derror
 *
 * Debug version of ss_error.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_derror       ss_error

/*##**********************************************************************\
 *
 *		ss_rc_derror
 *
 * Debug version of ss_rc_error.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_rc_derror(rc)       ss_rc_error(rc)

/*##**********************************************************************\
 *
 *		ss_dprintf_[1234]
 *
 * Four different debug output macros.
 *
 * All debug macros take one argument, which is an argument to the actual
 * debug output function. This argument can have variable number of
 * parameters in printf format. That means that the debug macro arguments
 * must be given inside double parenthesis. Note that also trailing newline
 * must be given.
 *
 *      Example: ss_dprintf_1(("return code = %d\n", retcode));
 *
 * Four different debug level macros are given.  The variable ss_debug_level
 * controls whether debug information is output or not.  Different debug
 * levels will indent the output, when possible.  Level 1 is displayed on the
 * left side of the screen, level 2 is indented four spaces, etc.  The
 * XVT version does not indent, and puts the text into XVT's debug file only
 * using function dbg.  Otherwise the output is displayed using the
 * SsDbgPrintf function.
 *
 * The default debug level is 0; no debug output is given.  The macro
 * SS_INIT_DEBUG will set the debug level from the environment variable
 * SS_DEBUG. The explanation of the format of the SS_DEBUG variable is
 * given with the function SsDbgSet.
 *
 * When the code is compiled without the SS_DEBUG flag, these macros expand
 * to empty statements or nothing.
 *
 * Parameters :
 *
 *      p - i, use
 *          Printf-style arguments in double parentheses.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define ss_dprintf_1(p) ss_pprintf_1(p)
#define ss_dprintf_2(p) ss_pprintf_2(p)
#define ss_dprintf_3(p) ss_pprintf_3(p)
#define ss_dprintf_4(p) ss_pprintf_4(p)
#define ss_dprintf(p)   ss_pprintf_1(p)

/* ss_output is used for debug output when the user just wants to call a
   function that does the real output.
*/
#define ss_output_1(p) { if (ss_debug_level >= 1 && SsDbgFileOk((char *)__FILE__)) \
                            {p;} \
                       }
#define ss_output_2(p) { if (ss_debug_level >= 2 && SsDbgFileOk((char *)__FILE__)) \
                            {p;} \
                       }
#define ss_output_3(p) { if (ss_debug_level >= 3 && SsDbgFileOk((char *)__FILE__)) \
                            {p;} \
                       }
#define ss_output_4(p) { if (ss_debug_level >= 4 && SsDbgFileOk((char *)__FILE__)) \
                            {p;} \
                       }
#define ss_output(p)   ss_output_1(p)

/* ss_output_begin and ss_output_end are used as pair to enclose
   part of code that is executed only when a condition is true.
*/
#define ss_output_begin(exp)    if (exp) {
#define ss_output_end           }

/* ss_debug is used for code that does e.g. some additional checks.
*/
#define ss_debug(p)   p
#define ss_debug_1(p) { if (ss_debug_check >= 1) {p;} }
#define ss_debug_2(p) { if (ss_debug_check >= 2) {p;} }
#define ss_debug_3(p) { if (ss_debug_check >= 3) {p;} }
#define ss_debug_4(p) { if (ss_debug_check >= 4) {p;} }

#define ss_trigger(func)    SsDbgCheckTrigger(func);

extern int  ss_debug_check;
extern bool ss_debug_waitp;
extern long ss_debug_mutexexitwait;
extern bool ss_debug_nocardinalcheck;

#define SS_PUSHNAME(name)           SsMemTrcEnterFunction((char *)__FILE__, (char *)name)
#define SS_POPNAME                  SsMemTrcExitFunction((char *)__FILE__, 0)
#define SS_PUSHNAME_CHK(sp,name)    sp = SsMemTrcEnterFunction((char *)__FILE__, (char *)name)
#define SS_POPNAME_CHK(sp)          ss_dassert(sp == SsMemTrcExitFunction((char *)__FILE__, 0))
#define SS_SETAPPINFO(ai)           SsMemTrcAddAppinfo(ai)

#else /* SS_DEBUG */

#define SS_PUSHNAME(name)
#define SS_POPNAME
#define SS_PUSHNAME_CHK(sp,name)
#define SS_POPNAME_CHK(sp)
#define SS_SETAPPINFO(ai)

#define ss_dassert(exp)
#define ss_rc_dassert(exp, rc)
#define ss_info_dassert(exp, info)
#define ss_derror
#define ss_rc_derror(rc)

#define ss_dassert_1(exp)
#define ss_dassert_2(exp)
#define ss_dassert_3(exp)
#define ss_dassert_4(exp)

#define ss_dprintf_1(p)
#define ss_dprintf_2(p)
#define ss_dprintf_3(p)
#define ss_dprintf_4(p)

#define ss_dprintf(p)

#define ss_output_1(p)
#define ss_output_2(p)
#define ss_output_3(p)
#define ss_output_4(p)
#define ss_output(p)

#define ss_output_begin(exp)
#define ss_output_end

#define ss_debug(p)
#define ss_debug_1(p)
#define ss_debug_2(p)
#define ss_debug_3(p)
#define ss_debug_4(p)

#define ss_trigger(func)

#endif /* SS_DEBUG */

#if defined(SS_BETA)
#  define ss_beta(p)            p
#  define ss_bassert(exp)       ss_assert(exp)
#  define ss_rc_bassert(exp,rc) ss_rc_assert(exp,rc)
#  define ss_info_bassert(exp,info) ss_info_assert(exp,info)
#  define ss_berror             ss_error
#  define ss_rc_berror(rc)      ss_rc_error(rc)
#  define ss_bprintf_1(p)       ss_pprintf_1(p)
#  define ss_bprintf_2(p)       ss_pprintf_2(p)
#  define ss_bprintf_3(p)       ss_pprintf_3(p)
#  define ss_bprintf_4(p)       ss_pprintf_4(p)
#  define ss_bprintf(p)         ss_pprintf_1(p)
#  define ss_boutput_1(p)       { if (ss_debug_level >= 1 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_boutput_2(p)       { if (ss_debug_level >= 2 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_boutput_3(p)       { if (ss_debug_level >= 3 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_boutput_4(p)       { if (ss_debug_level >= 4 && SsDbgFileOk((char *)__FILE__)) \
                                    {p;} \
                                }
#  define ss_boutput(p)         ss_boutput_1(p)
#else /* SS_BETA */
#  define ss_beta(p)
#  define ss_bassert(exp)
#  define ss_rc_bassert(exp,rc)
#  define ss_berror
#  define ss_rc_berror(rc)
#  define ss_bprintf_1(p)
#  define ss_bprintf_2(p)
#  define ss_bprintf_3(p)
#  define ss_bprintf_4(p)
#  define ss_bprintf(p)
#  define ss_boutput_1(p)
#  define ss_boutput_2(p)
#  define ss_boutput_3(p)
#  define ss_boutput_4(p)
#  define ss_boutput(p)
#endif /* SS_BETA */

#if defined(SS_PROFILE)
#  define ss_profile(p)   p
#else
#  define ss_profile(p)
#endif

#if defined(SS_PURIFY)
#  define ss_purify(p)   p
#else
#  define ss_purify(p)
#endif

#ifdef AUTOTEST_RUN
# define ss_autotest(p)   p
#else
# define ss_autotest(p)
#endif

#if defined(AUTOTEST_RUN) || defined(SS_DEBUG)
# define ss_autotest_or_debug(p) p
# define ss_aassert(e)           ss_dassert(e)
#else
# define ss_autotest_or_debug(p)
# define ss_aassert(p)
#endif

#if defined(SS_DEBUG) || defined(SS_BETA)
#define SS_SETSQLSTR(sqlstr)    SsSQLTrcSetStr(sqlstr)
#define SS_CLEARSQLSTR          SsSQLTrcSetStr(NULL)
#else
#define SS_SETSQLSTR(sqlstr)
#define SS_CLEARSQLSTR
#endif

#include "ssfile.h"


/*##**********************************************************************\
 *
 *		SS_INIT_DEBUG
 *
 * Initializes the debugging system by calling SsDbgInit.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define SS_INIT_DEBUG SsDbgInit()
#define SS_INIT       SsGlobalInit()

/*##**********************************************************************\
 *
 *		SS_INIT_DEBUG_DLL
 *
 * Initializes the DLL debugging system by calling SsDbgInitDll.
 *
 * When code is compiled without SS_DEBUG flag, this macro expands to an
 * empty statement.
 *
 * Parameters :
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
#define SS_INIT_DEBUG_DLL SsDbgInitDll()

extern int   ss_debug_level;
extern int   ss_debug_level;
extern bool  ss_skipatexit;
extern char* ss_licensetext;
extern bool  ss_disableassertmessagebox;
extern bool  ss_msg_useerrornostop;
extern bool  ss_msg_disableallmessageboxes;
extern char* ss_cmdline;
extern uint  ss_cmdline_maxlen;
ss_debug(extern bool ss_mainmem;)
extern bool ss_debug_taskoutput;
extern bool ss_debug_sqloutput;
ss_beta(extern bool ss_sem_ignoreerror;)
extern bool   ss_profile_active;
extern double ss_profile_limit;

#ifdef SS_MYSQL_PERFCOUNT
extern int mysql_enable_perfcount;
extern __int64 commit_perfcount;
extern __int64 commit_callcount;
extern __int64 index_read_idx_perfcount;
extern __int64 index_read_idx_callcount;
extern __int64 relcur_create_perfcount;
extern __int64 relcur_create_callcount;
extern __int64 relcur_open_perfcount;
extern __int64 relcur_open_callcount;
extern __int64 relcur_setconstr_perfcount;
extern __int64 relcur_setconstr_callcount;
extern __int64 fetch_perfcount;
extern __int64 fetch_callcount;
extern __int64 relcur_next_perfcount;
extern __int64 relcur_next_callcount;
extern __int64 tb_pla_create_perfcount;
extern __int64 tb_pla_create_callcount;
extern __int64 tb_pla_reset_perfcount;
extern __int64 tb_pla_reset_callcount;
extern __int64 dbe_search_init_perfcount;
extern __int64 dbe_search_init_callcount;
extern __int64 dbe_search_reset_perfcount;
extern __int64 dbe_search_reset_callcount;
extern __int64 dbe_search_reset_fetch_perfcount;
extern __int64 dbe_search_reset_fetch_callcount;
extern __int64 dbe_search_nextorprev_perfcount;
extern __int64 dbe_search_nextorprev_callcount;
extern __int64 dbe_indsea_next_perfcount;
extern __int64 dbe_indsea_next_callcount;
extern __int64 dbe_indmerge_perfcount;
extern __int64 dbe_indmerge_callcount;
extern __int64 trx_end_perfcount;
extern __int64 trx_end_callcount;
extern __int64 records_in_range_perfcount;
extern __int64 records_in_range_callcount;
#endif /* SS_MYSQL_PERFCOUNT */

ss_profile(extern int  ss_semdebug;)

typedef void (*ss_assertmessagefunc_t)(const char* msg);
typedef void (*ss_assertreportfunc_t)(const char* cmdline, const char* msg);

bool SS_CDECL SsDbgPrintfFunN(int level, const char* format, ...);

bool SS_CDECL SsDbgPrintfFun1(const char* format, ...);
bool SS_CDECL SsDbgPrintfFun2(const char* format, ...);
bool SS_CDECL SsDbgPrintfFun3(const char* format, ...);
bool SS_CDECL SsDbgPrintfFun4(const char* format, ...);

void SsAssertionFailure(char* file, int line);
void SsAssertionFailureText(char* text, char* file, int line);
void SsRcAssertionFailure(char* file, int line, int rc);
void SsInfoAssertionFailure(char* file, int line, char* info);
char* SS_CDECL SsInfoAssertionFailureText(const char* format, ...);
void SsAssertionExit(char* text);
void SsAssertionMessage(char* text, char* file, int line);
void SsLogMessage(char* fname, char* bakfname, long maxsize, char* text);
void SsLogMessageAndCallstack(char* fname, char* bakfname, long maxsize, char* text, void* pcallstack);
void SsLogErrorMessage(char* text);
ss_assertmessagefunc_t SsSetAssertMessageFunction(void (*func)(const char* msg));
ss_assertreportfunc_t SsSetAssertReportFunction(void (*func)(char* cmdline, char* msg));
void SsSetAssertMessageHeader(char* header);
char* SsGetAssertMessageHeader(void);
void SsSetAssertMessageProtocol(char* protocol);
void SS_CDECL SsErrorMessage(int msgcode, ...);
void SsSetErrorMessageFunction(void (*func)(char* text));
void SS_CDECL SsDbgMessage(const char* format, ...);
void SsErrorExit(void);
void SsAtErrorExit(void (*func)(void));
void SsFreeErrorExitList(void);
void SsExit(int value);
void SsBrk(void);
void SsDbgFlush(void);
#ifdef SS_INCLUDE_VARARGFUNS
void SsVfprintf(SS_FILE *fp, char* format, va_list ap);
void SsVprintf(char* format, va_list ap);
#endif
void SS_CDECL SsDbgPrintf(const char* format, ...);
void SS_CDECL SsPrintf(const char* format, ...);


void SS_CDECL SsFprintf(SS_FILE* fp, const char* format, ...);




void SsDbgInit(void);
void SsDbgInitDll(void);
void SsGlobalInit(void);
void SsDbgSet(const char* str);
bool SsDbgFileOk(char* fname);
void SsDbgCheckTrigger(const char* funcname);
void SsDbgSetDebugFile(char* fname);
void SsDbgCheckAssertStop(void);
char* SsGetVersionstring(bool islocal);

typedef enum {
        SS_EXE_SUCCESS = 0,

        /* Solid Server specific  */
        SS_EXE_DBOPENFAIL        = 10, /* Failed to open database. */
        SS_EXE_DBCONNFAIL,      /* 11     Failed to connect to database. */
        SS_EXE_DBTESTFAIL,      /* 12     Database test failed. */
        SS_EXE_DBFIXFAIL,       /* 13     Database fix failed. */
        SS_EXE_LICENSEFAIL,     /* 14     License error. */
        SS_EXE_MUSTCONVERT,     /* 15     Database must be converted. */
        SS_EXE_DBNOTEXIST,      /* 16     Database does not exist. */
        SS_EXE_DBEXIST,         /* 17     Database exists. */
        SS_EXE_DBNOTCREATED,    /* 18     Database not created. */
        SS_EXE_DBCREATEFAIL,    /* 19     Database create failed.  */
        SS_EXE_COMINITFAIL,     /* 20     Communication init failed. */
        SS_EXE_COMLISTENFAIL,   /* 21     Communication listen failed. */
        SS_EXE_SERVICEERROR,    /* 22     Service operation failed. */
        SS_EXE_DBFILESPECERROR, /* 23     Failed to open all the defined database files. */
        SS_EXE_BROKENNETCOPY,   /* 24     Database is a broken netcopy database. */

        /* Argument related errors. */
        SS_EXE_ILLARGUMENT       = 50, /* Illegal command line argument. */
        SS_EXE_CHDIRFAIL,       /* 51     Failed to change directory. */
        SS_EXE_INFILEOPENFAIL,  /* 52     Input file open failed. */
        SS_EXE_OUTFILEOPENFAIL, /* 53     Output file open failed. */
        SS_EXE_SRVCONNFAIL,     /* 54     Server connect failed. */
        SS_EXE_INITERROR,       /* 55     Operation init failed. */

        /* SQLSQL error codes */
        SS_EXE_SQLERROR          = 60, /* SQL error. */
        SS_EXE_PROCEDUREERROR,  /* 61     Procedure error. */

        /* Fatal error, assert */
        SS_EXE_FATALERROR        = 100 /* Assert or other fatal error. */
} ss_exeret_t;

#ifdef SS_DPRINTF_TO_PPRINTF
#error Use ss_bprintf_X instead
#endif

int ss_vers_ispurify(void);

void SS_CDECL ss_testlog_print(char* str, ...);
ss_debug(void ss_testlog_printcallstack(char** callstack);)

#if 1
/*
 *  Performance test macros
 */

void ss_plog_insert(char type, char* test_name, double time, char* note);
void ss_plog_fprint_hms(SS_FILE* fp, double d);

#define PLOG_COMMENT         'c'
#define PLOG_TESTSET_HEADER  'h'
#define PLOG_TESTSET_TOTAL   't'
#define PLOG_TEST_START      's'
#define PLOG_TEST_END        'e'
#define PLOG_PARTIAL_RESULT  'p'

#define PLOG_INSERT_TEST_START(test_name, note) \
        { \
            ss_plog_insert(PLOG_TEST_START, test_name, 0, note); \
        }

#define PLOG_INSERT_TEST_END(test_name, time, note) \
        { \
            ss_plog_insert(PLOG_TEST_END, test_name, time, note); \
        }

#define PLOG_INSERT_PARTIAL_RESULT(test_name, time, note) \
        { \
            ss_plog_insert(PLOG_PARTIAL_RESULT, test_name, time, note); \
        }

#define PLOG_GENERATE_TESTNAME(buffer) \
            { \
                int i = 0; \
                buffer[0] = '\0'; \
                while(argv[i] != NULL) { \
                    if (i > 0) { \
                        strcat(buffer, " "); \
                    } \
                    strcat(buffer, argv[i]); \
                    i++; \
                } \
                ss_assert(buffer[sizeof(buffer)-1] == '\0');\
            }
#endif /* 1 */

#endif /* SSDEBUG_H */
