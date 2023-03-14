/*************************************************************************\
**  source       * rs0sqli.c
**  directory    * res
**  description  * SQL info structure.
**               *
**               * (C) Copyright Solid Information Technology Ltd 2006
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

#define RS0SQLI_C

#include <sstime.h>

#include <ssenv.h>
#include <ssmem.h>
#include <ssscan.h>

#include <su0cfgst.h>
#include <su0inifi.h>
#include <su0param.h>

#include <ui0msg.h>

#include "rs0types.h"
#include "rs0aval.h"
#include "rs0sqli.h"

#define RS_DEFAULT_INFOLEVEL            0
#define RS_DEFAULT_INFOFILENAME         "soltrace.out"
#define RS_DEFAULT_INFOFILESIZE         1000000L
#define RS_DEFAULT_MUSTFLUSH            TRUE
#define RS_DEFAULT_SQLWARNING           FALSE
#define RS_DEFAULT_CONVERTORSTOUNIONS   TRUE
#define RS_DEFAULT_CONVERTORSTOUNIONSCOUNT 10
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
#define RS_DEFAULT_ALLOWDUPLICATEINDEX  TRUE
#else
#define RS_DEFAULT_ALLOWDUPLICATEINDEX  FALSE
#endif
#define RS_DEFAULT_SORTEDGROUPBY        RS_SQLI_SORTEDGROUPBY_ADAPT
#define RS_DEFAULT_ESTIGNOREORDERBY     FALSE
#define RS_DEFAULT_STATEMENTCACHE       10
#define RS_DEFAULT_PROCEDURECACHE       10
#define RS_DEFAULT_TRIGGERCACHE         20
#define RS_DEFAULT_MAXNESTEDTRIG        16
#ifdef SS_DEBUG
#define RS_DEFAULT_MAXNESTEDPROC        16
#else
#define RS_DEFAULT_MAXNESTEDPROC        16
#endif
#define RS_DEFAULT_OPTN                 0
#define RS_DEFAULT_USERANGEEST          TRUE
#define RS_DEFAULT_USERVECTORCONSTR     TRUE
#define RS_DEFAULT_VECTOROPTN           0   /* All rows */
#define RS_DEFAULT_SIMPLESQLOPT         TRUE
#define RS_DEFAULT_HURCCANREVERSE       FALSE
#define RS_DEFAULT_INDEXCURSORRESET     TRUE
#define RS_DEFAULT_LATEINDEXPLAN        FALSE
#define RS_DEFAULT_SETTRANSCOMPATIBILITY3 FALSE
#define RS_DEFAULT_ISOLATIONLEVEL       RS_SQLI_ISOLATION_REPEATABLEREAD
#define RS_DEFAULT_CHARPADDING          FALSE
#define RS_DEFAULT_CURSORCLOSEATENDTRAN TRUE

#if defined(SS_DEBUG)
#  define RS_DEFAULT_SQLSORTARRAYSIZE   500
#elif defined(SS_SMALLSYSTEM)
#  define RS_DEFAULT_SQLSORTARRAYSIZE   500
#elif defined(SS_UNIX) || defined(SS_NT)
#  define RS_DEFAULT_SQLSORTARRAYSIZE   4000 /* 4000 caused problems in ptest
                                                wisc200k runs 18 and 19. 
                                                Changed from 1000 to 2000, JarmoR Apr 15, 2004
                                                Changed from 2000 to 4000, JarmoR Apr 21, 2004
                                                - wisc200k runs 18 and 19 seem to be ok, at
                                                  least with new sql using row caching
                                              */
#else /* other */
#  define RS_DEFAULT_SQLSORTARRAYSIZE   500
#endif

#define RS_DEFAULT_ESTSAMPLELIMIT       100L
#define RS_DEFAULT_ESTSAMPLECOUNT       100
#define RS_DEFAULT_ESTSAMPLEMIN         10
#define RS_DEFAULT_ESTSAMPLEMAX         1000
#define RS_DEFAULT_ESTSAMPLEINC         1000
#define RS_DEFAULT_ESTSAMPLEMAXEQROWEST 50
#define RS_DEFAULT_ENABLEHINTS          TRUE
#if defined(SS_MEM_TUNE)
#define RS_DEFAULT_SIMPLEOPTIMIZERRULES TRUE
#else
#define RS_DEFAULT_SIMPLEOPTIMIZERRULES FALSE
#endif
#define RS_DEFAULT_USERELAXEDREADCOMMITTED   TRUE

bool rs_sqli_usevectorconstr = FALSE;
bool rs_sqli_enablehints = RS_DEFAULT_ENABLEHINTS;

static void sqli_closeinfofile(void* ml);

/*#***********************************************************************\
 *
 *		check_infolevel
 *
 *
 *
 * Parameters :
 *
 *	l -
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
static uint check_infolevel(long l)
{
        if (l < 0) {
            return(0);
        } else if (l > RS_SQLI_MAXINFOLEVEL) {
            return(RS_SQLI_MAXINFOLEVEL);
        } else {
            return((uint)l);
        }
}

/*#***********************************************************************\
 * 
 *		sqli_setisolation
 * 
 * Sets isolation level if it has a correct value.
 * 
 * Parameters : 
 * 
 *		sqli - 
 *			
 *			
 *		isolation - 
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
static bool sqli_setisolation(rs_sqlinfo_t* sqli, int isolation)
{
        switch (isolation) {
            case RS_SQLI_ISOLATION_READCOMMITTED:
            case RS_SQLI_ISOLATION_REPEATABLEREAD:
            case RS_SQLI_ISOLATION_SERIALIZABLE:
                sqli->sqli_isolationlevel = isolation;
                return TRUE;
            default:
                return FALSE;
        }
}

/*#***********************************************************************\
 * 
 *		rs_sqli_conf_param_set_cb
 * 
 * Callback to set the configuration parameter value.
 * 
 * Parameters : 
 * 
 *		ctx - 
 *			
 *			
 *		section - 
 *			
 *			
 *		keyname - 
 *			
 *			
 *		default_value - 
 *			
 *			
 *		default_value_loc - 
 *			
 *			
 *		current_value - 
 *			
 *			
 *		current_value_loc - 
 *			
 *			
 *		factory_value_loc - 
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
static su_ret_t rs_sqli_conf_param_set_cb(
        void* ctx,
        char* section,
        char* keyname,
        char* default_value,
        char** default_value_loc,
        char* current_value,
        char** current_value_loc,
        char** factory_value_loc)
{
        rs_sqlinfo_t* sqli = ctx;
        char* p_mismatch;
        long l;
        su_ret_t rc = SU_SUCCESS;

        CHK_SQLI(sqli);
        ss_dprintf_1(("rs_sqli_conf_param_set_cb:%s.%s:current_value=%s\n", section, keyname, current_value));

        if (strcmp(section, SU_SQL_SECTION) == 0) {
            if (strcmp(keyname, SU_SQL_ISOLATIONLEVEL) == 0) {
                if (SsStrScanLong(current_value, &l, &p_mismatch)) {
                    if (sqli_setisolation(sqli, (int)l)) {
                        rc = SU_SUCCESS;
                    } else {
                        rc = SU_ERR_PARAM_VALUE_INVALID;
                    }
                } else {
                    rc = SU_ERR_PARAM_VALUE_INVALID;
                }
            }
            if (strcmp(keyname, SU_SQL_INFO) == 0) {
                if (SsStrScanLong(current_value, &l, &p_mismatch)) {
                    if (check_infolevel(l) == l) {
                        sqli->sqli_infolevel = (uint)l;
                        rc = SU_SUCCESS;
                    } else {
                        rc = SU_ERR_PARAM_VALUE_INVALID;
                    }
                } else {
                    rc = SU_ERR_PARAM_VALUE_INVALID;
                }
            }
        }
        ss_dprintf_2(("rs_sqli_conf_param_set_cb:return %d\n", rc));
        return(rc);
}

static su_initparam_ctx_t rs_parameters[] = {

/* SQL Section */

    { /* FIXME: manual says from 0 to 8 and max is actually 15 */
        SU_SQL_SECTION, SU_SQL_INFO,
        NULL, RS_DEFAULT_INFOLEVEL, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Set the level of informational messages from the server: 0 (no info) to 9 (all info)"
    },
    { /* FIXME: manual says from 0 to 8 and max is actually 15 */
        SU_SQL_SECTION, SU_SQL_SQLINFO,
        NULL, RS_DEFAULT_INFOLEVEL, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP|SU_PARAM_AM_UNPUBLISHED,
        "Set the level of informational SQL level messages: 0 (no info) to 9 (all info)"
    },
    {
        SU_SQL_SECTION, SU_SQL_INFOFILENAME,
        RS_DEFAULT_INFOFILENAME, 0, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_STR, SU_PARAM_AM_RWSTARTUP,
        "Default global info file name"
    },
    {
        SU_SQL_SECTION, SU_SQL_INFOFILESIZE,
        NULL, RS_DEFAULT_INFOFILESIZE, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Maximum size of the info file in bytes"
    },
    {
        SU_SQL_SECTION, SU_SQL_INFOFILEFLUSH,
        NULL, 0, 0.0, RS_DEFAULT_MUSTFLUSH,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, flushes info file after every write operation"
    },
    {
        SU_SQL_SECTION, SU_SQL_SORTARRAYSIZE,
        NULL, RS_DEFAULT_SQLSORTARRAYSIZE, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "The size of the internal memory array in number of rows that SQL uses when ordering result set"
    },
    {
        SU_SQL_SECTION, SU_SQL_PROCEDURECACHE,
        NULL, RS_DEFAULT_PROCEDURECACHE, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The size of cache for parsed procedures in number of procedures"
    },
    {
        SU_SQL_SECTION, SU_SQL_MAXNESTEDPROC,
        NULL, RS_DEFAULT_MAXNESTEDPROC, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The maximum number of allowed nested procedures"
    },
    {
        SU_SQL_SECTION, SU_SQL_TRIGGERCACHE,
        NULL, RS_DEFAULT_TRIGGERCACHE, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The size of cache for parsed triggers in number of triggers"
    },
    {
        SU_SQL_SECTION, SU_SQL_MAXNESTEDTRIG,
        NULL, RS_DEFAULT_MAXNESTEDTRIG, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "The maximum number of allowed nested triggers"
    },
    {
        SU_SQL_SECTION, SU_SQL_BLOBEXPRLIMIT,
        NULL, RS_AVAL_DEFAULTMAXLOADBLOBLIMIT, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RWSTARTUP,
        "Maximum BLOb size in expressions"
    },
    {
        SU_SQL_SECTION, SU_SQL_EMULATEOLDTIMESTAMPDIFF,
        NULL, 0, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to yes, emulates old TIMESTAMPDIFF behaviour"
    },
    {
        SU_SQL_SECTION, SU_SQL_ISOLATIONLEVEL,
        NULL, RS_DEFAULT_ISOLATIONLEVEL, 0.0, 0,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RW,
        "Default transaction isolation level"
    },
    { /* deprecated since.. 4.2? at least 4.5 */
        SU_SQL_SECTION, SU_SQL_CONVERTORSTOUNIONS,
        NULL, 0, 0.0, RS_DEFAULT_CONVERTORSTOUNIONS,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY | SU_PARAM_AM_UNPUBLISHED,
        "Allow conversion of ORs to UNIONs."
    },
    {
        SU_SQL_SECTION, SU_SQL_CONVERTORSTOUNIONSCOUNT,
        NULL, RS_DEFAULT_CONVERTORSTOUNIONSCOUNT, 0.0, 0,
        NULL, NULL,
        SU_PARAM_TYPE_LONG, SU_PARAM_AM_RONLY,
        "Maximum number of OR operations that may be converted to UNION operations."
    },
    {
        SU_SQL_SECTION, SU_SQL_ALLOWDUPLICATEINDEX,
        NULL, 0, 0.0, RS_DEFAULT_ALLOWDUPLICATEINDEX,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY,
        "If set to yes, allows duplicate index definitions"
    },
    {
        SU_SQL_SECTION, SU_SQL_SETTRANSCOMPATIBILITY3,
        NULL, 0, 0.0, RS_DEFAULT_SETTRANSCOMPATIBILITY3,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY|SU_PARAM_AM_UNPUBLISHED,
        "Compatibility with 3.x or earlier version in which 'SET TRANSACTION [..] command applied not only to the current transaction (current implementation) but also to the following transactions."
    },
    {
        SU_SQL_SECTION, SU_SQL_SIMPLESQLOPT,
        NULL, 0, 0.0, RS_DEFAULT_SIMPLESQLOPT,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY|SU_PARAM_AM_UNPUBLISHED,
        "Enable simple SQL optimization feature: increases performance.  There probably is no reason to disable this ever."
    },
    {
        SU_SQL_SECTION, SU_SQL_CHARPADDING,
        NULL, 0, 0.0, RS_DEFAULT_CHARPADDING,
        NULL, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY,
        "Enable padding of CHAR column values for output. If a value of type CHARACTER(n) is shorter than n, it is right-padded with blanks to length n in any output variable."
    },
    { /* mr 20040506: used to be in Hints-section */
        SU_SQL_SECTION, SU_HINTS_ENABLE,
        NULL, 0, 0.0, RS_DEFAULT_ENABLEHINTS,
        rs_sqli_conf_param_set_cb, NULL,
        SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RWSTARTUP,
        "If set to no, disables optimizer hints"
    },
    {
        SU_SQL_SECTION, SU_SQL_TIMESTAMPDISPLAYSIZE19,
        NULL, 0, 0.0, 0,
        NULL, NULL, SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY,
        "If set to yes, timestamp display size is reset to 19"
    },
    {
        SU_SQL_SECTION, SU_SQL_SIMPLEOPTIMIZERRULES,
        NULL, 0, 0.0, RS_DEFAULT_SIMPLEOPTIMIZERRULES,
        NULL, NULL, SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY,
        "Ignore cost estimates in query optimization"
    },
    {
        SU_SQL_SECTION, SU_SQL_CURSORCLOSEATENDTRAN,
        NULL, 0, 0.0,  RS_DEFAULT_CURSORCLOSEATENDTRAN,
        NULL, NULL, SU_PARAM_TYPE_BOOL, SU_PARAM_AM_RONLY,
        "Control cursor behavior at end of transaction. Normally cursors close at the end of transaction, but in some cases ODBC driver should pretend that cursors do not close automatically. For example this behavior is expected by Centura generated applications."
    },

/* Termination */
    
    {
        NULL, NULL, NULL, 0, 0.0, 0, NULL, NULL, 0, 0
    }
};

/*##**********************************************************************\
 *
 *		rs_sqli_init
 *
 *
 *
 * Parameters :
 *
 *	inifile - in, hold
 *
 *
 * Return value :
 *
 *      Sqlinfo object.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_sqlinfo_t* rs_sqli_init(
        su_inifile_t* inifile)
{
        rs_sqlinfo_t* sqli;
        bool found;
        bool b;
        long l;
        long convert_ors_to_unions_count;

        sqli = SSMEM_NEW(rs_sqlinfo_t);

        b = su_param_register_array_ctx(rs_parameters, sqli);
        ss_dassert(b);

        ss_debug(sqli->sqli_chk = RSCHK_SQLI);
        sqli->sqli_inifile = inifile;

        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_INFO,
                    &l);

        if (found) {
            sqli->sqli_infolevel = check_infolevel(l);
        } else {
            sqli->sqli_infolevel = RS_DEFAULT_INFOLEVEL;
        }
        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SQLINFO,
                    &l);

        if (found) {
            sqli->sqli_sqlinfolevel = check_infolevel(l);
        } else {
            sqli->sqli_sqlinfolevel = sqli->sqli_infolevel;
        }
        found = su_param_getvalue(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_INFOFILENAME,
                    &sqli->sqli_fname);

        if (!found) {
            sqli->sqli_fname = NULL;
        }

        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_INFOFILESIZE,
                    &sqli->sqli_maxfilesize);

        if (!found) {
            sqli->sqli_maxfilesize = RS_DEFAULT_INFOFILESIZE;
        }

        found = su_param_getbool(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_INFOFILEFLUSH,
                    &sqli->sqli_mustflush);

        if (!found) {
            sqli->sqli_mustflush = RS_DEFAULT_MUSTFLUSH;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_WARNING,
                    &b);

        if (found) {
            sqli->sqli_warning = b;
        } else {
            sqli->sqli_warning = RS_DEFAULT_SQLWARNING;
        }
        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SORTARRAYSIZE,
                    &l);

        if (found) {
            sqli->sqli_sortarraysize = (uint)l;
        } else {
            sqli->sqli_sortarraysize = RS_DEFAULT_SQLSORTARRAYSIZE;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_CONVERTORSTOUNIONSCOUNT,
                    &convert_ors_to_unions_count);
        if (!found) {
            convert_ors_to_unions_count = RS_DEFAULT_CONVERTORSTOUNIONSCOUNT;
        }

        /* this on/off is deprecated */
        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_CONVERTORSTOUNIONS,
                    &b);

#if 0 /* MR:20040324: old code left here if a bug appears. remove if you
       *              want */
        if (found) {
            if (b) {
                sqli->sqli_convertorstounions = convert_ors_to_unions_count;
            } else {
                sqli->sqli_convertorstounions = 0;
            }
        } else {
            /* MR: I guess this means that default is TRUE */
            sqli->sqli_convertorstounions = convert_ors_to_unions_count;
        }
#else
        if (!found) {
            b = RS_DEFAULT_CONVERTORSTOUNIONS;
        } else {
            /* make the unpublished param visible because it is used
             * in inifile */
            su_param_setvisibility(
                    su_param_getparam(SU_SQL_SECTION,
                                      SU_SQL_CONVERTORSTOUNIONS),
                    TRUE);
        }
        
        if (b) {
            if (convert_ors_to_unions_count < 0) {                
                /* here we should somehow tell parammanager that the
                 * value in use is 0 */
                sqli->sqli_convertorstounions = 0;
            } else {
                sqli->sqli_convertorstounions = convert_ors_to_unions_count;
            }
        } else {
            /* here we should somehow tell parammanager that the value
             * in use is 0 */
            sqli->sqli_convertorstounions = 0;
        }
#endif

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ALLOWDUPLICATEINDEX,
                    &b);
        if (!found) {
            sqli->sqli_allowduplicateindex = RS_DEFAULT_ALLOWDUPLICATEINDEX;
        } else {
            sqli->sqli_allowduplicateindex = b;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SORTEDGROUPBY,
                    &l);

        if (found && l >= RS_SQLI_SORTEDGROUPBY_NONE && l <= RS_SQLI_SORTEDGROUPBY_ADAPT) {
            sqli->sqli_sortedgroupby = (int)l;
        } else {
            sqli->sqli_sortedgroupby = RS_DEFAULT_SORTEDGROUPBY;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTIGNOREORDERBY,
                    &b);

        if (found) {
            sqli->sqli_estignoreorderby = b;
        } else {
            sqli->sqli_estignoreorderby = RS_DEFAULT_ESTIGNOREORDERBY;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_STATEMENTCACHE,
                    &l);
        if (found) {
            sqli->sqli_statementcache = (uint)l;
        } else {
            sqli->sqli_statementcache = RS_DEFAULT_STATEMENTCACHE;
        }
        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_PROCEDURECACHE,
                    &l);

        if (found) {
            sqli->sqli_procedurecache = (uint)l;
        } else {
            sqli->sqli_procedurecache = RS_DEFAULT_PROCEDURECACHE;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_TRIGGERCACHE,
                    &l);
        if (found) {
            sqli->sqli_triggercache = (uint)l;
        } else {
            sqli->sqli_triggercache = RS_DEFAULT_TRIGGERCACHE;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_MAXNESTEDTRIG,
                    &l);
        if (found) {
            sqli->sqli_maxnestedtrig = (uint)l;
        } else {
            sqli->sqli_maxnestedtrig = RS_DEFAULT_MAXNESTEDTRIG;
        }
        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_MAXNESTEDPROC,
                    &l);

        if (found) {
            sqli->sqli_maxnestedproc = (uint)l;
        } else {
            sqli->sqli_maxnestedproc = RS_DEFAULT_MAXNESTEDPROC;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLELIMIT,
                    &l);
        if (found) {
            sqli->sqli_estsamplelimit = l;
        } else {
            sqli->sqli_estsamplelimit = RS_DEFAULT_ESTSAMPLELIMIT;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLECOUNT,
                    &l);
        if (found) {
            sqli->sqli_estsamplecount = (uint)l;
        } else {
            sqli->sqli_estsamplecount = RS_DEFAULT_ESTSAMPLECOUNT;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLEMIN,
                    &l);
        if (found) {
            sqli->sqli_estsamplemin = l;
        } else {
            sqli->sqli_estsamplemin = RS_DEFAULT_ESTSAMPLEMIN;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLEMAX,
                    &l);
        if (found) {
            sqli->sqli_estsamplemax = l;
        } else {
            sqli->sqli_estsamplemax = RS_DEFAULT_ESTSAMPLEMAX;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLEMAXEQROWEST,
                    &l);
        if (found) {
            sqli->sqli_estsamplemaxeqrowest = l;
        } else {
            sqli->sqli_estsamplemaxeqrowest = RS_DEFAULT_ESTSAMPLEMAXEQROWEST;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ESTSAMPLEINC,
                    &l);
        if (found) {
            sqli->sqli_estsampleinc = l;
        } else {
            sqli->sqli_estsampleinc = RS_DEFAULT_ESTSAMPLEINC;
        }

        if (sqli->sqli_estsamplemax < sqli->sqli_estsamplemin) {
            sqli->sqli_estsamplemax = sqli->sqli_estsamplemin;
        }
        if (sqli->sqli_estsamplecount < sqli->sqli_estsamplemin) {
            sqli->sqli_estsamplemin = sqli->sqli_estsamplecount;
        }
        if (sqli->sqli_estsamplecount > sqli->sqli_estsamplemax) {
            sqli->sqli_estsamplemax = sqli->sqli_estsamplecount;
        }

        found = su_param_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SIMPLEOPTIMIZERRULES,
                    &sqli->sqli_simpleoptimizerrules);
        if (!found) {
            sqli->sqli_simpleoptimizerrules = RS_DEFAULT_SIMPLEOPTIMIZERRULES;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_USERELAXEDREACOMMITTED,
                    &sqli->sqli_userelaxedreacommitted);
        if (!found) {
            sqli->sqli_userelaxedreacommitted = RS_DEFAULT_USERELAXEDREADCOMMITTED;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_OPTN,
                    &l);
        if (found) {
            sqli->sqli_optn = (uint)l;
        } else {
            sqli->sqli_optn = RS_DEFAULT_OPTN;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_USERVECTORCONSTR,
                    &rs_sqli_usevectorconstr);
        if (!found) {
            rs_sqli_usevectorconstr = RS_DEFAULT_USERVECTORCONSTR;
        }

        found = su_inifile_getlong(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_VECTOROPTN,
                    &l);
        if (found) {
            sqli->sqli_vectoroptn = (uint)l;
        } else {
            sqli->sqli_vectoroptn = RS_DEFAULT_VECTOROPTN;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SIMPLESQLOPT,
                    &sqli->sqli_simplesqlopt);
        if (!found) {
            sqli->sqli_simplesqlopt = RS_DEFAULT_SIMPLESQLOPT;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_HURCREVERSE,
                    &sqli->sqli_hurccanreverse);
        if (!found) {
            sqli->sqli_hurccanreverse = RS_DEFAULT_HURCCANREVERSE;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_INDEXCURSORRESET,
                    &sqli->sqli_indexcursorreset);
        if (!found) {
            sqli->sqli_indexcursorreset = RS_DEFAULT_INDEXCURSORRESET;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_LATEINDEXPLAN,
                    &sqli->sqli_lateindexplan);
        if (!found) {
            sqli->sqli_lateindexplan = RS_DEFAULT_LATEINDEXPLAN;
        }

        found = su_inifile_getbool(
                    inifile,
                    SU_SQL_SECTION,
                    SU_SQL_SETTRANSCOMPATIBILITY3,
                    &sqli->sqli_settranscompatibility3);
        if (!found) {
            sqli->sqli_settranscompatibility3 = RS_DEFAULT_SETTRANSCOMPATIBILITY3;
        }

        found = su_param_getbool(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_CHARPADDING,
                    &sqli->sqli_charpadding);
        if (!found) {
            sqli->sqli_charpadding = RS_DEFAULT_CHARPADDING;
        }
        ss_dassert(sqli->sqli_charpadding == FALSE
                   || sqli->sqli_charpadding == TRUE);

        found = su_param_getbool(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_CURSORCLOSEATENDTRAN,
                    &sqli->sqli_cursorcloseatendtran);
        if (!found) {
            sqli->sqli_cursorcloseatendtran = RS_DEFAULT_CURSORCLOSEATENDTRAN;
        }
        ss_dassert(sqli->sqli_cursorcloseatendtran == FALSE
                   || sqli->sqli_cursorcloseatendtran == TRUE);
        
        found = su_param_getlong(
                    sqli->sqli_inifile,
                    SU_SQL_SECTION,
                    SU_SQL_ISOLATIONLEVEL,
                    &l);
        if (found) {
            if (!sqli_setisolation(sqli, (int)l)) {
                ui_msg_warning(
                    INI_MSG_ILLEGAL_ISOLATION_VALUE_USSU,
                    (int)l,
                    SU_SQL_SECTION,
                    SU_SQL_ISOLATIONLEVEL,
                    RS_DEFAULT_ISOLATIONLEVEL);
                sqli->sqli_isolationlevel = RS_DEFAULT_ISOLATIONLEVEL;
            }
        } else {
            sqli->sqli_isolationlevel = RS_DEFAULT_ISOLATIONLEVEL;
        }

        sqli->sqli_infoml = NULL;
        sqli->sqli_ver = NULL;

        /*********************************************************\
         * SET DEFAULT ESTIMATOR VALUES
        \*********************************************************/

        sqli->sqli_userangeest           = RS_DEFAULT_USERANGEEST;

        /* The estimated selectivity for different types of search constraints */
        sqli->sqli_equal_selectivity     = (5.0/100.0);
        sqli->sqli_notequal_selectivity  = (9.0/10.0);
        sqli->sqli_compare_selectivity   = (1.0/5.0); /* selectivity
                                                     for <, <= etc. */
        sqli->sqli_like_selectivity      = (1.0/30.0);
        sqli->sqli_isnull_selectivity    = (1.0/3.0);
        sqli->sqli_isnotnull_selectivity = (2.0/3.0);
        sqli->sqli_no_selectivity        = (1.0);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        /* At least in TPC-C and TPC-D selectivity can drop
         * quite fast.
         */
        sqli->sqli_selectivity_drop_limit= (0.01);
#else
        sqli->sqli_selectivity_drop_limit= (0.33);
#endif

        /* estimated default hit rate for an index block
         */
        sqli->sqli_min_hit_rate_for_index    = (1.0/100.0);
        sqli->sqli_max_hit_rate_for_index    = (100.0/100.0);

        /* estimated default hit rate for a data block
         */
        sqli->sqli_min_hit_rate_for_data     = (1.0/100.0);
        sqli->sqli_max_hit_rate_for_data     = (100.0/100.0);

        /* estimated maximum key entry size in bytes
         */
        sqli->sqli_max_key_entry_size    = 100.0;

        /* if a secondary index is prejoined, then the following parameter
         * tells how densely are the relevant entries there
         */
        sqli->sqli_prejoin_density       = (1.0/3.0);

        /* time for a B+-tree search in microseconds
         */
        sqli->sqli_time_for_index_search = 1000.0;

        /* time for processing a single index entry in a block in microseconds
         */
        sqli->sqli_time_per_index_entry  = 300.0;

        /* time for random access of a disk block in microseconds
         */
        sqli->sqli_block_access_time     = 25000.0;

        /* time for sorting one row
         */
        sqli->sqli_row_sort_time         = 200.0;

        /*********************************************************\
         * GET POSSIBLE CHANGED VALUES FROM INIFILE
        \*********************************************************/

        su_inifile_getbool(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_EQUAL_USERANGEEST,
            &sqli->sqli_userangeest);

        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_EQUAL_SELECTIVITY,
            &sqli->sqli_equal_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_NOTEQUAL_SELECTIVITY,
            &sqli->sqli_notequal_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_COMPARE_SELECTIVITY,
            &sqli->sqli_compare_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_LIKE_SELECTIVITY,
            &sqli->sqli_like_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_ISNULL_SELECTIVITY,
            &sqli->sqli_isnull_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_ISNOTNULL_SELECTIVITY,
            &sqli->sqli_isnotnull_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_NO_SELECTIVITY,
            &sqli->sqli_no_selectivity);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_SELECTIVITY_DROP_LIMIT,
            &sqli->sqli_selectivity_drop_limit);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_MIN_HIT_RATE_FOR_INDEX,
            &sqli->sqli_min_hit_rate_for_index);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_MAX_HIT_RATE_FOR_INDEX,
            &sqli->sqli_max_hit_rate_for_index);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_MIN_HIT_RATE_FOR_DATA,
            &sqli->sqli_min_hit_rate_for_data);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_MAX_HIT_RATE_FOR_DATA,
            &sqli->sqli_max_hit_rate_for_data);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_MAX_KEY_ENTRY_SIZE,
            &sqli->sqli_max_key_entry_size);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_PREJOIN_DENSITY,
            &sqli->sqli_prejoin_density);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_TIME_FOR_INDEX_SEARCH,
            &sqli->sqli_time_for_index_search);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_TIME_PER_INDEX_ENTRY,
            &sqli->sqli_time_per_index_entry);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_BLOCK_ACCESS_TIME,
            &sqli->sqli_block_access_time);
        su_inifile_getdouble(
            inifile,
            SU_SQL_SECTION,
            SU_SQL_ROW_SORT_TIME,
            &sqli->sqli_row_sort_time);

        su_inifile_getbool(
            inifile,
            SU_SQL_SECTION,
            SU_HINTS_ENABLE,
            &rs_sqli_enablehints);

        return(sqli);
}

/*##**********************************************************************\
 *
 *		rs_sqli_done
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
void rs_sqli_done(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        if (sqli->sqli_infoml != NULL) {
            sqli_closeinfofile(sqli->sqli_infoml);
        }
        if (sqli->sqli_fname != NULL) {
            SsMemFree(sqli->sqli_fname);
        }
        if (sqli->sqli_ver != NULL) {
            SsMemFree(sqli->sqli_ver);
        }

        SsMemFree(sqli);
}

/*##**********************************************************************\
 *
 *		rs_sqli_setsqlversion
 *
 *
 *
 * Parameters :
 *
 *	sqli -
 *
 *
 *	ver -
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
void rs_sqli_setsqlversion(
        rs_sqlinfo_t* sqli,
        char* ver)
{
        CHK_SQLI(sqli);
        ss_dassert(sqli->sqli_ver == NULL);

        sqli->sqli_ver = SsMemStrdup(ver);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getinfolevel
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getinfolevel(rs_sqlinfo_t* sqli, bool sqlp)
{
        CHK_SQLI(sqli);

        if (sqlp) {
            return(sqli->sqli_sqlinfolevel);
        } else {
            return(sqli->sqli_infolevel);
        }
}

/*##**********************************************************************\
 *
 *		rs_sqli_setinfolevel
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
void rs_sqli_setinfolevel(rs_sqlinfo_t* sqli, uint infolevel, bool sqlp)
{
        CHK_SQLI(sqli);

        if (sqlp) {
            sqli->sqli_sqlinfolevel = check_infolevel(infolevel);
        } else {
            sqli->sqli_infolevel = check_infolevel(infolevel);
#if 0 /* mr 20040506: removed: if you want to set both, call twice. */    
            sqli->sqli_sqlinfolevel = check_infolevel(infolevel);
#endif
        }
}

/*##**********************************************************************\
 *
 *		rs_sqli_getuseindexcursorreset
 *
 * Returns flag that tells if we should use index cursor reset or not
 * in table cursor.
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_getuseindexcursorreset(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_indexcursorreset);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getuselateindexplan
 *
 * Returns flag that tells if we should use late index selection 
 * that does index selection at run time in table cursor.
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_getuselateindexplan(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_lateindexplan);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getsettranscompatibility3
 *
 * Returns flag that tells if we should use old version 3 or older set
 * syntax behaviour.
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_getsettranscompatibility3(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_settranscompatibility3);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getwarning
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_getwarning(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_warning);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getsortarraysize
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getsortarraysize(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_sortarraysize);
}

/*##**********************************************************************\
 *
 *		rs_sqli_useinternalsorter
 *
 * Checks if internal sorter should be used with given numer of lines.
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_useinternalsorter(
        rs_sqlinfo_t* sqli,
        uint sortarraysize,
        ulong lines,
        bool exact)
{
        if (sqli != NULL) {
            CHK_SQLI(sqli);
            sortarraysize = sqli->sqli_sortarraysize;
        }
        ss_dassert(sortarraysize != 0);

        if (exact) {
            if (lines < (sortarraysize * 9UL) / 10UL) {
                return (TRUE);
            }
        } else {
            if (lines < (sortarraysize * 2UL) / 10UL) {
                return (TRUE);
            }
        }
        return(FALSE);
}


/*##**********************************************************************\
 *
 *		rs_sqli_getconvertorstounions
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getconvertorstounions(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_convertorstounions);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getallowduplicateindex
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_getallowduplicateindex(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_allowduplicateindex);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getsortedgroupby
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getsortedgroupby(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_sortedgroupby);
}

/*##**********************************************************************\
 *
 *		rs_sqli_estignoreorderby
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_estignoreorderby(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estignoreorderby);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getstatementcachesize
 *
 * Returns statement cache size
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getstatementcachesize(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_statementcache);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getprocedurecachesize
 *
 * Returns procedure cache size
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getprocedurecachesize(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_procedurecache);
}

/*##**********************************************************************\
 *
 *		rs_sqli_gettriggercachesize
 *
 * Returns trigger cache size
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_gettriggercachesize(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_triggercache);
}

bool rs_sqli_getcharpadding(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);
        ss_dassert(sqli->sqli_charpadding == FALSE
                   || sqli->sqli_charpadding == TRUE);
        return (sqli->sqli_charpadding);
}

bool rs_sqli_cursorcloseatendtran(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);
        ss_dassert(sqli->sqli_cursorcloseatendtran == FALSE
                   || sqli->sqli_cursorcloseatendtran == TRUE);
        return (sqli->sqli_cursorcloseatendtran);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getmaxnestedtrig
 *
 * Returns max number of nested trigger calls.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getmaxnestedtrig(
        rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_maxnestedtrig);
}

uint rs_sqli_getmaxnestedproc(
        rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_maxnestedproc);
}

/*##**********************************************************************\
 *
 *		rs_sqli_usesimpleoptimizerrules
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_usesimpleoptimizerrules(rs_sqlinfo_t* sqli, double ntuples_in_table)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_simpleoptimizerrules || ntuples_in_table < (double)sqli->sqli_estsamplelimit);
}

/*##**********************************************************************\
 *
 *		rs_sqli_userelaxedreacommitted
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_userelaxedreacommitted(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_userelaxedreacommitted);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getestsamplecount
 *
 * Number of key value samples taken from the database.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getestsamplecount(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estsamplecount);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getestsamplemin
 *
 * Minimum number of key value samples taken from the database.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getestsamplemin(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estsamplemin);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getestsamplemax
 *
 * Maximum number of key value samples taken from the database.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getestsamplemax(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estsamplemax);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getestsamplemaxeqrowest
 *
 * Maximum number of key value samples checjked from database for equal
 * constraint estimate.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getestsamplemaxeqrowest(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estsamplemaxeqrowest);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getestsampleinc
 *
 * Number of rows in table that increment estsamplecount by one.
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getestsampleinc(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_estsampleinc);
}

/*##**********************************************************************\
 *
 *		rs_sqli_getvectoroptn
 *
 *
 * Parameters :
 *
 *	sqli -
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
uint rs_sqli_getvectoroptn(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_vectoroptn);
}

/*##**********************************************************************\
 *
 *		rs_sqli_simplesqlopt
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_simplesqlopt(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_simplesqlopt);
}

void rs_sqli_setsimplesqlopt(rs_sqlinfo_t* sqli, bool opt)
{
        CHK_SQLI(sqli);

        sqli->sqli_simplesqlopt = opt;
}

/*##**********************************************************************\
 *
 *		rs_sqli_hurccanreverse
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_hurccanreverse(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_hurccanreverse);
}


/*##**********************************************************************\
 *
 *		rs_sqli_settestmode
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
void rs_sqli_settestmode(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

/******** WARNING!!!!!!!!!!!!! Do not change the test version's values
for repeatability of tests. ******************************************/

        sqli->sqli_equal_selectivity     = (1.0/100.0);
        sqli->sqli_notequal_selectivity  = (9.0/10.0);
        sqli->sqli_compare_selectivity   = (1.0/5.0); /* selectivity
                                                       for <, <= etc. */
        sqli->sqli_like_selectivity      = (1.0/30.0);
        sqli->sqli_isnull_selectivity    = (1.0/3.0);
        sqli->sqli_isnotnull_selectivity = (2.0/3.0);
        sqli->sqli_no_selectivity        = (1.0);
        sqli->sqli_selectivity_drop_limit= (0.25);

        sqli->sqli_min_hit_rate_for_index    = (1.0/100.0);
        sqli->sqli_max_hit_rate_for_index    = (80.0/100.0);
        sqli->sqli_min_hit_rate_for_data     = (80.0/100.0);
        sqli->sqli_max_hit_rate_for_data     = (100.0/100.0);
        sqli->sqli_data_size_per_index   = 4.0;
        sqli->sqli_max_key_entry_size    = 30.0;
        sqli->sqli_prejoin_density       = (1.0/3.0);
        sqli->sqli_time_for_index_search = 1000.0;
        sqli->sqli_time_per_index_entry  = 300.0;
        sqli->sqli_block_access_time     = 25000.0;
        sqli->sqli_row_sort_time         = 100.0;
}

/*##**********************************************************************\
 *
 *		rs_sqli_userangeest
 *
 * Returns TRUE if range estimates should be used.
 *
 * Parameters :
 *
 *	sqli -
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
bool rs_sqli_userangeest(
        rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_userangeest);
}


/*##**********************************************************************\
 *
 *		rs_sqli_equal_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_equal_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_equal_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_notequal_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_notequal_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_notequal_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_compare_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_compare_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_compare_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_like_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_like_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_like_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_isnull_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_isnull_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_isnull_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_isnotnull_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_isnotnull_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_isnotnull_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_no_selectivity
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_no_selectivity(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_no_selectivity);
}

/*##**********************************************************************\
 *
 *		rs_sqli_selectivity_drop_limit
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_selectivity_drop_limit(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_selectivity_drop_limit);
}

/*##**********************************************************************\
 *
 *		rs_sqli_min_hit_rate_for_index
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_min_hit_rate_for_index(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_min_hit_rate_for_index);
}

/*##**********************************************************************\
 *
 *		rs_sqli_max_hit_rate_for_index
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_max_hit_rate_for_index(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_max_hit_rate_for_index);
}

/*##**********************************************************************\
 *
 *		rs_sqli_min_hit_rate_for_data
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_min_hit_rate_for_data(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_min_hit_rate_for_data);
}

/*##**********************************************************************\
 *
 *		rs_sqli_max_hit_rate_for_data
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_max_hit_rate_for_data(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_max_hit_rate_for_data);
}

/*##**********************************************************************\
 *
 *		rs_sqli_data_size_per_index
 *
 * test version only
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_data_size_per_index(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_data_size_per_index);
}

/*##**********************************************************************\
 *
 *		rs_sqli_max_key_entry_size
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_max_key_entry_size(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_max_key_entry_size);
}

/*##**********************************************************************\
 *
 *		rs_sqli_prejoin_density
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_prejoin_density(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_prejoin_density);
}

/*##**********************************************************************\
 *
 *		rs_sqli_time_for_index_search
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_time_for_index_search(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_time_for_index_search);
}

/*##**********************************************************************\
 *
 *		rs_sqli_time_per_index_entry
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_time_per_index_entry(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_time_per_index_entry);
}

/*##**********************************************************************\
 *
 *		rs_sqli_block_access_time
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_block_access_time(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_block_access_time);
}

/*##**********************************************************************\
 *
 *		rs_sqli_row_sort_time
 *
 *
 *
 * Parameters :
 *
 *	sqli -
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
double rs_sqli_row_sort_time(rs_sqlinfo_t* sqli)
{
        CHK_SQLI(sqli);

        return(sqli->sqli_row_sort_time);
}

static void sqli_printinifileline(void* ctx, char* line)
{
        SsMsgLogT* ml = ctx;

        SsMsgLogPutStr(ml, line);
}

/*##**********************************************************************\
 *
 *		rs_sqli_openinfofile
 *
 * Opens SQL info output file. If given file name is NULL, then a global
 * info output file is returned.
 *
 * Parameters :
 *
 *	sqli -
 *
 *
 *	fname -
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
void* rs_sqli_openinfofile(
        rs_sqlinfo_t* sqli,
        char* fname)
{
        SsMsgLogT* ml;

        if (fname == NULL) {
            if (sqli->sqli_infoml != NULL) {
                /* Use existing default file. */
                SsMsgLogLink(sqli->sqli_infoml);
                return(sqli->sqli_infoml);
            }
            if (sqli->sqli_fname != NULL) {
                ml = SsMsgLogInit(sqli->sqli_fname, sqli->sqli_maxfilesize);
            } else {
                /* Use default global file. */
                ml = SsMsgLogInitDefaultTrace();
            }
        } else {
            ml = SsMsgLogInit(fname, sqli->sqli_maxfilesize);
        }

        if (ml == NULL) {
            return(NULL);
        }

        SsMsgLogPrintf(ml, "-- SQL info help:\n");
        SsMsgLogPrintf(ml, "--   EST(lines, exact, c0, c1, order)\n");
        if (sqli->sqli_ver != NULL) {
            SsMsgLogPrintf(ml, "--   Ver: %s\n", sqli->sqli_ver);
        }
        SsMsgLogPrintf(ml, "-- inifile begin\n");
        su_inifile_savecallback(sqli->sqli_inifile, sqli_printinifileline, ml);
        SsMsgLogPrintf(ml, "-- inifile end\n");

        if (fname == NULL) {
            sqli->sqli_infoml = ml;
            SsMsgLogLink(ml);
        }
        return(ml);
}

/*#***********************************************************************\
 *
 *		sqli_closeinfofile
 *
 * Closes a file.
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
static void sqli_closeinfofile(
        void* ml)
{
        SsMsgLogDone(ml);
}

/*##**********************************************************************\
 *
 *		rs_sqli_closeinfofile
 *
 * Closes info out file.
 *
 * Parameters :
 *
 *	sqli -
 *
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
void rs_sqli_closeinfofile(
        rs_sqlinfo_t* sqli,
        void* ml)
{
        if (ml != NULL) {
            sqli_closeinfofile(ml);
        }
}

/*##**********************************************************************\
 *
 *		rs_sqli_printinfo
 *
 * Prints info to a info output file.
 *
 * Parameters :
 *
 *	sqli -
 *
 *
 *	ml -
 *
 *
 *	level -
 *
 *
 *	str -
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
void rs_sqli_printinfo(
        rs_sqlinfo_t* sqli,
        void* ml,
        int level,
        char* str)
{
        SS_NOTUSED(sqli);
        SS_NOTUSED(level);

        if (ml != NULL) {
            SsMsgLogPrintf(ml, "%.3800s", str);
            if (sqli->sqli_mustflush) {
                SsMsgLogFlush(ml);
            }
        }
}


/*##**********************************************************************\
 *
 *		rs_sqli_addcfgtocfgl
 *
 *
 *
 * Parameters :
 *
 *	sqli - in
 *
 *
 *	cfgl - use
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
void rs_sqli_addcfgtocfgl(
        rs_sqlinfo_t* sqli,
        su_cfgl_t* cfgl)
{
        su_inifile_t* inifile;

        CHK_SQLI(sqli);

        inifile = sqli->sqli_inifile;

        su_cfgl_addlong(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_INFO,
            (long)RS_DEFAULT_INFOLEVEL,
            SU_CFGL_ISADVANCED);
#if 0
        su_cfgl_addbool(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_WARNING,
            RS_DEFAULT_SQLWARNING,
            SU_CFGL_ISADVANCED);
#endif

        su_cfgl_addlong(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_SORTARRAYSIZE,
            (long)RS_DEFAULT_SQLSORTARRAYSIZE,
            SU_CFGL_ISADVANCED);

        su_cfgl_addlong(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_PROCEDURECACHE,
            (long)RS_DEFAULT_PROCEDURECACHE,
            SU_CFGL_ISADVANCED);
        
        su_cfgl_addbool(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_ALLOWDUPLICATEINDEX,
            RS_DEFAULT_ALLOWDUPLICATEINDEX,
            SU_CFGL_ISADVANCED);
#if 0
        su_cfgl_addbool(
            cfgl,
            inifile,
            SU_SQL_SECTION,
            SU_SQL_CONVERTORSTOUNIONS,
            RS_DEFAULT_CONVERTORSTOUNIONSCOUNT,
            SU_CFGL_ISADVANCED);
#endif
}
