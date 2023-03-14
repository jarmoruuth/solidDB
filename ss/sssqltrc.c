/*************************************************************************\
**  source       * sssqltrc.c
**  directory    * ss
**  description  * SQL string tracing, for error reports.
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
#include "ssmsglog.h"
#include "sssqltrc.h"

struct SsSQLTrcInfoStruct {
        int     st_chk;
        int     st_nlinks;
        char*   st_sqlstr;
};

#if defined(SS_NT) && !defined(SS_DLLQMEM)

# define SQLTRC_TRACE

#ifdef SS_USE_WIN32TLS_API

# define SQLTRC_SETSQLSTR(s)    SsThrDataSet(SS_THRDATA_SQLSTR, s)
# define SQLTRC_GETSQLSTR       SsThrDataGet(SS_THRDATA_SQLSTR)

#else /* SS_USE_WIN32TLS_API */

# define SQLTRC_SETSQLSTR(s)        cursqlstr = s
# define SQLTRC_GETSQLSTR           cursqlstr
  static __declspec(thread) char*   cursqlstr;

#endif /* SS_USE_WIN32TLS_API */

# ifdef SS_DEBUG
#   ifdef SS_USE_WIN32TLS_API
#       define SS_SETSQLTRCINFO(i)      SsThrDataSet(SS_THRDATA_SQLTRC, i)
#       define SS_GETSQLTRCINFO         SsThrDataGet(SS_THRDATA_SQLTRC)
#   else
#       define SS_SETSQLTRCINFO(i)      cursqltrcinfo = i
#       define SS_GETSQLTRCINFO         cursqltrcinfo
        static __declspec(thread) SsSQLTrcInfoT* cursqltrcinfo;
#   endif /* SS_USE_WIN32TLS_API */
# endif /* SS_DEBUG */

#elif defined(SS_PTHREAD)

# define SQLTRC_TRACE
# define SQLTRC_SETSQLSTR(s)    SsThrDataSet(SS_THRDATA_SQLSTR, s)
# define SQLTRC_GETSQLSTR       SsThrDataGet(SS_THRDATA_SQLSTR)

#elif !defined(SS_MT)

# define SQLTRC_TRACE
# define SQLTRC_SETSQLSTR(s)        cursqlstr = s
# define SQLTRC_GETSQLSTR           cursqlstr
  static char*                      cursqlstr;

# ifdef SS_DEBUG
#   define SS_SETSQLTRCINFO(i)      cursqltrcinfo = i
#   define SS_GETSQLTRCINFO         cursqltrcinfo
    static SsSQLTrcInfoT*           cursqltrcinfo;
# endif /* SS_DEBUG */

#endif /* SS_NT */

#ifdef SQLTRC_TRACE

/*##**********************************************************************\
 * 
 *		SsSQLTrcSetStr
 * 
 * Sets the current active SQL string.
 * 
 * Parameters : 
 * 
 *	sqlstr - in, ref
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
void SsSQLTrcSetStr(char* sqlstr)
{
#if defined(SS_DEBUG) && defined(SS_GETSQLTRCINFO)
        SsSQLTrcInfoT* sqltrcinfo;

        sqltrcinfo = SS_GETSQLTRCINFO;
        if (sqltrcinfo != NULL) {
            SsSQLTrcInfoFree(sqltrcinfo);
            SS_SETSQLTRCINFO(NULL);
        }
#endif /* SS_DEBUG */

        SQLTRC_SETSQLSTR(sqlstr);
}

/*##**********************************************************************\
 * 
 *		SsSQLTrcGetStr
 * 
 * Returns current SQL string, or NULL if none.
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
char* SsSQLTrcGetStr(void)
{
        return(SQLTRC_GETSQLSTR);
}

# if defined(SS_DEBUG) && defined(SS_GETSQLTRCINFO)

/*##**********************************************************************\
 * 
 *		SsSQLTrcInfoCopy
 * 
 * Returns a copy of current SQL string info, or NULL.
 * 
 * Parameters :
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 *      The return value must be released using function 
 *      SsSQLTrcInfoFree.
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsSQLTrcInfoT* SsSQLTrcInfoCopy(void)
{
        char* sqlstr;

        sqlstr = SQLTRC_GETSQLSTR;

        if (sqlstr != NULL) {
            SsSQLTrcInfoT* sqltrcinfo;

            sqltrcinfo = SS_GETSQLTRCINFO;
            if (sqltrcinfo == NULL) {
                sqltrcinfo = malloc(sizeof(SsSQLTrcInfoT));
                sqltrcinfo->st_chk = 1876;
                ss_assert(sqltrcinfo != NULL);
                sqltrcinfo->st_nlinks = 2;
                sqltrcinfo->st_sqlstr = malloc(strlen(sqlstr) + 1);
                ss_assert(sqltrcinfo->st_sqlstr != NULL);
                strcpy(sqltrcinfo->st_sqlstr, sqlstr);
                SS_SETSQLTRCINFO(sqltrcinfo);
            } else {
                sqltrcinfo->st_nlinks++;
            }
            return(sqltrcinfo);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 * 
 *		SsSQLTrcInfoFree
 * 
 * Releases SQL string info structure.
 * 
 * Parameters : 
 * 
 *	sqltrcinfo - 
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
void SsSQLTrcInfoFree(SsSQLTrcInfoT* sqltrcinfo)
{
        if (sqltrcinfo != NULL) {
            ss_assert(sqltrcinfo->st_chk == 1876);
            sqltrcinfo->st_nlinks--;
            if (sqltrcinfo->st_nlinks == 0) {
                free(sqltrcinfo->st_sqlstr);
                free(sqltrcinfo);
            }
        }
}

/*##**********************************************************************\
 * 
 *		SsSQLTrcInfoGetStr
 * 
 * Returns the SQL string from info structure, or NULL if info is NULL.
 * 
 * Parameters : 
 * 
 *	sqltrcinfo - 
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
char* SsSQLTrcInfoGetStr(SsSQLTrcInfoT* sqltrcinfo)
{
        if (sqltrcinfo != NULL) {
            ss_assert(sqltrcinfo->st_chk == 1876);
            return(sqltrcinfo->st_sqlstr);
        } else {
            return(NULL);
        }
}

/*##**********************************************************************\
 * 
 *		SsSQLTrcInfoPrint
 * 
 * Prints SQL string info.
 * 
 * Parameters : 
 * 
 *	sqltrcinfo - 
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
void SsSQLTrcInfoPrint(SsSQLTrcInfoT* sqltrcinfo)
{
        if (sqltrcinfo != NULL) {
            ss_assert(sqltrcinfo->st_chk == 1876);
            SsFprintf(NULL, "    SQL:\n");
            ss_dassert(SS_MSGLOG_BUFSIZE >= 8000);
            SsFprintf(NULL, "    '%.7000s'\n", sqltrcinfo->st_sqlstr);
        }
}

# else /* defined(SS_DEBUG) && defined(SS_GETSQLTRCINFO) */

SsSQLTrcInfoT* SsSQLTrcInfoCopy(void)
{
        return(NULL);
}

void SsSQLTrcInfoFree(SsSQLTrcInfoT* sqltrcinfo __attribute__ ((unused)))
{
}

char* SsSQLTrcInfoGetStr(SsSQLTrcInfoT* sqltrcinfo __attribute__ ((unused)))
{
        return(NULL);
}

void SsSQLTrcInfoPrint(SsSQLTrcInfoT* sqltrcinfo __attribute__ ((unused)))
{
}

# endif /* defined(SS_DEBUG) && defined(SS_GETSQLTRCINFO) */

#else /* SQLTRC_TRACE */

void SsSQLTrcSetStr(char* sqlstr)
{
}

char* SsSQLTrcGetStr(void)
{
        return(NULL);
}

# ifdef SS_DEBUG

SsSQLTrcInfoT* SsSQLTrcInfoCopy(void)
{
        return(NULL);
}

void SsSQLTrcInfoFree(SsSQLTrcInfoT* sqltrcinfo)
{
}

char* SsSQLTrcInfoGetStr(SsSQLTrcInfoT* sqltrcinfo)
{
        return(NULL);
}

void SsSQLTrcInfoPrint(SsSQLTrcInfoT* sqltrcinfo)
{
}

# endif /* SS_DEBUG */

#endif /* SQLTRC_TRACE */
