/*************************************************************************\
**  source       * tab0info.c
**  directory    * tab
**  description  * Query processing information production
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


#include <ssstdio.h>
#include <ssdebug.h>

#include <ui0msg.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0sqli.h>

#include "tab1defs.h"
#include "tab0info.h"

/*##**********************************************************************\
 * 
 *		tb_info_level
 * 
 * Checks if the miscellaneous information about the query processing
 * should be produced. Returns the current info level
 * Member of the SQL function block.
 * 
 * NOTE! Info levels in SQL are not those that are given by the user.
 *       User given levels are mapped to SQL levels.
 * 
 * Info output from different SQL levels:
 *      0   No output
 *      1   Execution graphs
 *      2   Some estimation info
 *      3   All estimation messages
 *      4   All info, also from every fetch
 * 
 * Input params : 
 * 
 *	cd	    - client data  
 *	trans	- 
 * 
 * Output params: 
 * 
 * Return value : 
 * 
 *      The current info level below which the information should be produced
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
uint tb_info_level(
        rs_sysi_t*  cd,
        tb_trans_t* trans __attribute__ ((unused))
) {
        uint level;
        static uint sqlinfo[RS_SQLI_MAXINFOLEVEL + 1] = {
         /* SQL   Inifile */
            0, /* 0 */
            0, /* 1 */
            1, /* 2 */
            2, /* 3 */
            3, /* 4 */
            3, /* 5 */
            3, /* 6 */
            4, /* 7 */
            4, /* 8 */
            4, /* 9 */
            4, /* 10 */
            4, /* 11 */
            4, /* 12 */
            4, /* 13 */
            4, /* 14 */
            4  /* 15 */
        };

        ss_bassert(cd != NULL);

        level = rs_sysi_sqlinfolevel(cd, TRUE);
        ss_bassert(level <= RS_SQLI_MAXINFOLEVEL);
        ss_bassert(sqlinfo[level] <= 4);

        return(sqlinfo[level]);
}

/*##**********************************************************************\
 * 
 *		tb_info_option
 * 
 * The tb_info_option function queries the string value of a configuration
 * parameter (like setenv function).
 *
 * The possibly queried values are:
 *
 * "SELFINSERTALLOWED": if the string begins with "Y" or "y", SQL inserts
 * or updates referring to the target table in the search part are allowed.
 *
 * "GROUPBYMETHOD": this affects the way that GROUP BY is performed in
 * the case that explicit information on the number of result groups is
 * not available. If the string begins with "A" or "a" ("adaptive"), the
 * GROUP BY input is pre-sorted if it turns out that the real number of
 * result groups exceeds the number of rows that fit into the central memory
 * array for GROUP BY. If the string begins with "S" or "s" ("static"), the
 * GROUP BY input is pre-sorted whenever there are at least two items
 * in the GROUP BY list. Otherwise, the GROUP BY input is not pre-sorted.
 *
 * Any undefined queried values should be answered with "" (empty string).
 * 
 * Parameters : 
 * 
 *	cd - in
 *		application state
 *		
 *	trans - in
 *		handle to the current transaction
 *		
 *	option - in
 *		name of the configuration parameter
 *		
 * Return value - out, give : 
 *		
 *      Return value: newly allocated configuration string value
 *		
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* tb_info_option(
        rs_sysi_t*  cd,
        tb_trans_t* trans __attribute__ ((unused)),
        char*       option)
{
        char* option_value;

        switch (option[4]) {
            case 'I':
                ss_dassert(strcmp(option, "SELFINSERTALLOWED") == 0);
                option_value = (char *)"Y";
                break;
            case 'L':
                ss_dassert(strcmp(option, "SIMPLEQUERYOPT") == 0);
                if (rs_sqli_simplesqlopt(rs_sysi_sqlinfo(cd))) {
                    option_value = (char *)"Y";
                } else {
                    option_value = (char *)"N";
                }
                break;
            case 'P':
                ss_dassert(strcmp(option, "GROUPBYMETHOD") == 0);
                {
                    int sortedgroupby_option;

                    sortedgroupby_option = rs_sysi_sortedgroupby(cd);
                    switch (sortedgroupby_option) {
                        case RS_SQLI_SORTEDGROUPBY_STATIC:
                            option_value = (char *)"Adaptive";
                            break;
                        case RS_SQLI_SORTEDGROUPBY_ADAPT:
                            option_value = (char *)"Static";
                            break;
                        case RS_SQLI_SORTEDGROUPBY_NONE:
                        default:
                            option_value = (char *)"";
                            break;
                    }
                }
                break;
            case 'B':
                ss_dassert(strcmp(option, "DROPBEHAVIOURDEF") == 0);
                option_value = (char *)"R";
                break;
            default:
                ss_dassert(strcmp(option, "SELFINSERTALLOWED") != 0);
                ss_dassert(strcmp(option, "SIMPLEQUERYOPT") != 0);
                ss_dassert(strcmp(option, "GROUPBYMETHOD") != 0);
                ss_dassert(strcmp(option, "DROPBEHAVIOURDEF") != 0);
                option_value = (char *)"";
                break;
        }
        option_value = SsMemStrdup(option_value);
        return (option_value);
}

/*##**********************************************************************\
 * 
 *		tb_info_print
 * 
 * The info function is used to output miscellaneus information
 * about the query processing.
 * Member of the SQL function block.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		application state
 *		
 *	trans - in, use
 *		transaction handle
 *		
 *	level - in
 *		level of the information
 *		
 *	str - in, use
 *		a string containing information
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void tb_info_print(
        rs_sysi_t*  cd,
        tb_trans_t* trans __attribute__ ((unused)),
        uint        level,
        char*       str
) {
        ss_dprintf_1(("%s", str));

        if ((uint)rs_sysi_sqlinfolevel(cd, TRUE) >= level) {
            rs_sysi_printsqlinfo(cd, level, str);
        }
}

/*##**********************************************************************\
 * 
 *		tb_info_printwarning
 * 
 * The warning function is used to produce a warning on a query.
 * Member of the SQL function block.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		application state
 *		
 *	trans -  in, use
 *		transaction handle
 *		
 *	code - in
 *		the warning code
 *		
 *	str - in, use
 *		the warning string
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void tb_info_printwarning(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        uint        code,
        char*       str
) {
        SS_NOTUSED(trans);

        ss_dassert(cd != NULL);

        if (rs_sqli_getwarning(rs_sysi_sqlinfo(cd))) {
            ui_msg_sqlwarning(code, str);
        }
}

/*##**********************************************************************\
 * 
 *		tb_info_defaultnullcoll
 * 
 * Returns NULL collation info
 * 
 * Parameters : 
 * 
 *	cd - notused
 *		
 *		
 *	trans - notused
 *		
 *		
 * Return value :
 *      SQL_NULLCOLL_LOW
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
sql_nullcoll_t tb_info_defaultnullcoll(
        rs_sysi_t*  cd,
        tb_trans_t* trans)
{
        SS_NOTUSED(cd);
        SS_NOTUSED(trans);
        return (SQL_NULLCOLL_LOW);
}
