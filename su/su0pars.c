/*************************************************************************\
**  source       * su0pars.c
**  directory    * su
**  description  * Simple string parsing utilities.
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

#include <ssenv.h>

#include <ssstring.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sschcvt.h>

#include <uti0dyn.h>

#include "su0types.h"
#include "su0pars.h"

/*##**********************************************************************\
 * 
 *		su_pars_match_init
 * 
 * Initializes a parser match structure.
 * 
 * Parameters : 
 * 
 *	m - out
 *		
 *		
 *	s - in
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
void su_pars_match_init(su_pars_match_t* m, char* s)
{
        m->m_start = s;
        m->m_pos = s;
}

/*##**********************************************************************\
 * 
 *		su_pars_check_comment
 * 
 * Checks and skips spaces and comments.
 * 
 * Parameters : 
 * 
 *	m - use
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
void su_pars_check_comment(su_pars_match_t* m)
{
        m->m_pos = SsStrTrimLeft(m->m_pos);
        while (m->m_pos[0] == '-' && m->m_pos[1] == '-') {
            m->m_pos += 2;
            while (*m->m_pos != '\0' && *m->m_pos != '\n') {
                m->m_pos++;
            }
            m->m_pos = SsStrTrimLeft(m->m_pos);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_match_const
 * 
 * Matches to a given constant.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	const_str - in
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
bool su_pars_match_const(su_pars_match_t* m, char* const_str)
{
        int len;

        su_pars_check_comment(m);

        if (const_str[0] == '\0') {
            return(m->m_pos[0] == '\0');
        }
        len = strlen(const_str);
        if (strncmp(m->m_pos, const_str, len) == 0) {
            m->m_pos += len;
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_match_keyword
 * 
 * Matches to a given constant keyword witch must also end to that.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	const_str - in
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
bool su_pars_match_keyword(su_pars_match_t* m, char* const_str)
{
        int len;

        su_pars_check_comment(m);
        if (const_str[0] == '\0') {
            return(m->m_pos[0] == '\0');
        }
        len = strlen(const_str);
#ifdef SS_DEBUG
        { /* check that the given keyword is a keyword */
            int i;
            if (len > 0) {
                ss_dassert(ss_isascii((ss_byte_t)const_str[0]));
                ss_dassert(ss_isupper((ss_byte_t)const_str[0]) || const_str[0] == '_');
                for (i = 1; i < len; i++) {
                    ss_dassert(ss_isascii((ss_byte_t)const_str[i]));
                    ss_dassert(ss_isupper((ss_byte_t)const_str[i])
                            || ss_isdigit((ss_byte_t)const_str[i])
                            || const_str[i] == '_');
                }
            }
        }
#endif /* SS_DEBUG */

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        if (SsStrnicmp(m->m_pos, const_str, len) == 0) {
#else
        if (strncmp(m->m_pos, const_str, len) == 0) {
#endif
            ss_byte_t c = (ss_byte_t)m->m_pos[len];
            if (ss_isascii(c)) {
                if (ss_isalnum(c)) {
                    return (FALSE);
                }
                if (c == '_') {
                    return (FALSE);
                }
            }
            m->m_pos += len;
            return(TRUE);
        }
        return(FALSE);
}

/*#***********************************************************************\
 * 
 *		pars_get_quoted
 * 
 * Gets a quoted string.
 * 
 * Parameters : 
 * 
 *	m - 
 *		
 *		
 *	buf - 
 *		
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
static bool pars_get_quoted(
        su_pars_match_t* m,
        char* buf,
        uint size,
        bool keep_quotes)
{
        char quote;
        su_pars_match_t saved_m;

        ss_dassert(size > 2);

        saved_m = *m;

        quote = *m->m_pos++;
        if (keep_quotes) {
            *buf++ = quote;
            size--;
        }
        while (*m->m_pos != '\0') {
            if (*m->m_pos == quote) {
                if ((m->m_pos)[1] == quote) {
                    m->m_pos++;
                    if (keep_quotes) {
                        *buf++ = quote;
                        if (size-- <= 1) {
                            *m = saved_m;
                            return(FALSE);
                        }
                    }
                } else {
                    break;
                }
            }
            *buf++ = *m->m_pos++;
            if (size-- <= 1) {
                *m = saved_m;
                return(FALSE);
            }
        }
        if (*m->m_pos != quote) {
            *m = saved_m;
            return(FALSE);
        }
        m->m_pos++;
        if (keep_quotes) {
            *buf++ = quote;
            if (size-- <= 1) {
                *m = saved_m;
                return(FALSE);
            }
        }

        *buf = '\0';

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		su_pars_get_pwd
 * 
 * Matches and returns password all non-whitespace marks are usable for
 * password (excluding comments, which are filtered away before)
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	pwd_buf - out
 *		
 *		
 *	pwd_size - in
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
bool su_pars_get_pwd(
        su_pars_match_t* m,
        char* pwd_buf,
        uint pwd_size)
{
        su_pars_match_t saved_m;
        bool ok = FALSE;

        ss_dassert(m != NULL);
        ss_dassert(pwd_buf != NULL);
        ss_dassert(pwd_size != 0);
        
        saved_m = *m;

        su_pars_check_comment(m);

        for (;;(m->m_pos)++, pwd_buf++) {
            ss_byte_t c = (ss_byte_t)*(m->m_pos);
            if (c == '\0') {
                break;
            }
            if (ss_isspace(c)) {
                break;
            }
            *pwd_buf = (char)c;
            ok = TRUE;
            if (pwd_size-- <= 1) {
                *m = saved_m;
                return(FALSE);
            }
        }
        *pwd_buf = '\0';
        if (!ok) {
            *m = saved_m;
        }
        return (ok);
}

/*##**********************************************************************\
 * 
 *		su_pars_get_id
 * 
 * Matches and returns SQL identifier. Identifier can be quoted with
 * double quotes.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	id_buf - out
 *		
 *		
 *	id_size - in
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
bool su_pars_get_id(
        su_pars_match_t* m,
        char* id_buf,
        uint id_size)
{
        char* org_id_buf = id_buf;

        su_pars_match_t saved_m;

        saved_m = *m;

        su_pars_check_comment(m);

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        if (*m->m_pos == '"' || *m->m_pos == '`') {
#else
        if (*m->m_pos == '"') {
#endif
            if (!pars_get_quoted(m, id_buf, id_size, FALSE)) {
                *m = saved_m;
                return(FALSE);
            }
        } else {
            while (ss_isalnum(*m->m_pos) || *m->m_pos == '_') {
                *id_buf++ = *m->m_pos++;
                if (id_size-- <= 1) {
                    *m = saved_m;
                    return(FALSE);
                }
            }
            *id_buf = '\0';
        }
        if (strlen(org_id_buf) > 0) {
            return(TRUE);
        } else {
            *m = saved_m;
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_get_tablename
 * 
 * Matches and returns SQL table name.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	authid_buf - out
 *		
 *		
 *	authid_size - in
 *		
 *		
 *	tabname_buf - out
 *		
 *		
 *	tabname_size - in
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
bool su_pars_get_tablename(
        su_pars_match_t* m,
        char* authid_buf,
        uint authid_size,
        char* tabname_buf,
        uint tabname_size)
{
        su_pars_match_t saved_m;

        saved_m = *m;

        if (su_pars_match_const(m, (char *)".")) {
            /* Empty schema. */
            authid_buf[0] = '\0';
            return(su_pars_get_id(m, tabname_buf, tabname_size));
        }
        if (!su_pars_get_id(m, authid_buf, authid_size)) {
            return(FALSE);
        }
        if (!su_pars_match_const(m, (char *)".")) {
            /* There is no schema qualifier. */
            *m = saved_m;
            authid_buf[0] = '\0';
            if (!su_pars_get_id(m, tabname_buf, tabname_size)) {
                return(FALSE);
            }
            return(TRUE);
        }
        if (!su_pars_get_id(m, tabname_buf, tabname_size)) {
            return(FALSE);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		su_pars_get_filename
 * 
 * Gets a file name. File name may be queted with double questes. All
 * characters except space characters (unless quoted) are accepted to
 * the file name.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	fname_buf - out
 *		
 *		
 *	fname_size - in
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
bool su_pars_get_filename(
        su_pars_match_t* m,
        char* fname_buf,
        uint fname_size)
{
        su_pars_match_t saved_m;
        char* p = fname_buf;

        saved_m = *m;

        su_pars_check_comment(m);

        if (*m->m_pos == '"') {
            if (pars_get_quoted(m, fname_buf, fname_size, FALSE)) {
                return(TRUE);
            } else {
                *m = saved_m;
                return(FALSE);
            }
        } else if (*m->m_pos == '\'') {
            if (pars_get_quoted(m, fname_buf, fname_size, FALSE)) {
                return(TRUE);
            } else {
                *m = saved_m;
                return(FALSE);
            }
        }

        while (!ss_isspace(*m->m_pos) && *m->m_pos != '\0') {
            *p++ = *m->m_pos++;
            if (fname_size-- <= 1) {
                *m = saved_m;
                return(FALSE);
            }
        }
        *p = '\0';
        if (strlen(fname_buf) > 0) {
            return(TRUE);
        } else {
            *m = saved_m;
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_get_stringliteral
 * 
 * Gets a string literal, e.g. 'string'. Quotes are removed from returned
 * value.
 * 
 * Parameters : 
 * 
 *	m - 
 *		
 *		
 *	string_buf - 
 *		
 *		
 *	string_size - 
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
bool su_pars_get_stringliteral(
        su_pars_match_t* m,
        char* string_buf,
        uint string_size)
{
        su_pars_match_t saved_m;

        saved_m = *m;

        su_pars_check_comment(m);

        if (*m->m_pos == '\'' &&
            pars_get_quoted(m, string_buf, string_size, FALSE)) {
            return(TRUE);
        } else {
            *m = saved_m;
            return(FALSE);
        }
}

bool su_pars_get_stringliteral_withquotes(
        su_pars_match_t* m,
        char* string_buf,
        uint string_size)
{
        su_pars_match_t saved_m;

        saved_m = *m;

        su_pars_check_comment(m);

        if (*m->m_pos == '\'' &&
            pars_get_quoted(m, string_buf, string_size, TRUE)) {
            return(TRUE);
        } else {
            *m = saved_m;
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_get_uint
 * 
 * Gets unsigned integer.
 * 
 * Parameters : 
 * 
 *	m - 
 *		
 *		
 *	p_num - 
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
bool su_pars_get_uint(
        su_pars_match_t* m,
        uint* p_num)
{
        long num;

        if (su_pars_get_long(m, &num)) {
            *p_num = (uint)num;
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_get_long
 * 
 * Gets long integer.
 * 
 * Parameters : 
 * 
 *	m - 
 *		
 *		
 *	p_num - 
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
bool su_pars_get_long(
        su_pars_match_t* m,
        long* p_num)
{
        long digit;
        su_pars_match_t saved_m;

        saved_m = *m;
        *p_num = 0;

        su_pars_check_comment(m);

        if (!ss_isdigit(*m->m_pos)) {
            return(FALSE);
        }
        while (ss_isdigit(*m->m_pos)) {
            digit = *m->m_pos++ - '0';
            *p_num *= 10;
            *p_num += digit;
        }
        return(TRUE);
}

static bool pars_isspecial(char c)
{
        return(ss_ispunct(c) &&
               c != '\'' &&
               c != '"' &&
               c != ':');
}

bool su_pars_get_special(
        su_pars_match_t* m,
        char* buf,
        uint buf_size __attribute__ ((unused)))
{
        su_pars_match_t saved_m;

        saved_m = *m;

        su_pars_check_comment(m);

        if (!pars_isspecial(*m->m_pos)) {
            return(FALSE);
        }
        do {
             *buf++ = *m->m_pos++;
        } while (pars_isspecial(*m->m_pos));

        *buf = '\0';

        return(TRUE);
}

bool su_pars_get_numeric(
        su_pars_match_t* m,
        char* buf,
        uint buf_size __attribute__ ((unused)))
{
        su_pars_match_t saved_m;

        saved_m = *m;

        su_pars_check_comment(m);

        if (!ss_isdigit(*m->m_pos)) {
            return(FALSE);
        }
        do {
             *buf++ = *m->m_pos++;
        } while (ss_isdigit(*m->m_pos) || *m->m_pos == '.' ||
                 ss_toupper(*m->m_pos) == 'E');

        *buf = '\0';

        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		su_pars_give_objname
 * 
 * Gets 2-part procedure name from a string.
 * 
 * Parameters : 
 * 
 *	m - use
 *		Current parsing state.
 *		
 *	p_authid - out, give
 *		Authid.
 *		
 *	p_name - out, give
 *		Name.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_pars_give_objname(
        su_pars_match_t* m,
        char** p_authid,
        char** p_name)
{
        bool succp;
        char name[SU_MAXNAMELEN+1];
        char authid[SU_MAXNAMELEN+1];

        succp = su_pars_get_tablename(
                    m,
                    authid,
                    sizeof(authid),
                    name,
                    sizeof(name));
        if (succp) {
            if (p_name != NULL) {
                *p_name = SsMemStrdup(name);
            }
            if (p_authid != NULL) {
                *p_authid = SsMemStrdup(authid);
            }
        }
        return(succp);
}


bool su_pars_give_objname3(
        su_pars_match_t* m,
        char** p_catalog,
        char** p_schema,
        char** p_name)
{
        char strbuf[SU_MAXNAMELEN+1];
        char* strarr[3];
        uint i;
        uint pos;

        if (p_catalog != NULL) {
            *p_catalog = NULL;
        }
        if (p_schema != NULL) {
            *p_schema = NULL;
        }
        if (p_name != NULL) {
            *p_name = NULL;
        }
        for (i = 0; ;) {
            if (!su_pars_get_id(m, strbuf, sizeof(strbuf))) {
                goto failed;
            }
            strarr[i] = SsMemStrdup(strbuf);
            i++;
            if (i >= 3) {
                ss_dassert(i == 3);
                break;
            }
            if (!su_pars_match_const(m, (char *)".")) {
                break;
            }
        }
        pos = 0;
        switch (i) {
            case 3:
                if (p_catalog != NULL) {
                    *p_catalog = strarr[pos];
                } else {
                    SsMemFree(strarr[pos]);
                }
                pos++;
                /* FALLTHROUGH */
            case 2:
                if (p_schema != NULL) {
                    *p_schema = strarr[pos];
                } else {
                    SsMemFree(strarr[pos]);
                }
                pos++;
                /* FALLTHROUGH */
            case 1:
                if (p_name != NULL) {
                    *p_name = strarr[pos];
                } else {
                    SsMemFree(strarr[pos]);
                }
                break;
            default:
                ss_derror;
        }
        return (TRUE);
 failed:;
        while (i != 0) {
            i--;
            SsMemFree(strarr[i]);
        }
        return (FALSE);
}

static bool su_pars_get_nonspace(
        su_pars_match_t* m,
        char* buf,
        uint bufsize)
{
        int pos = 0;
        su_pars_match_t saved_m;

        saved_m = *m;

        m->m_pos = SsStrTrimLeft(m->m_pos);
        while (!ss_isspace(*m->m_pos)) {
            buf[pos++] = *m->m_pos++;
            if ((uint)pos == bufsize-1) {
                *m = saved_m;
                return(FALSE);
            }
        }
        buf[pos] = '\0';
        return(*buf != '\0');
}

bool su_pars_give_hint(su_pars_match_t* m, char** p_hint)
{
        char name[SU_MAXNAMELEN+1];
        su_pars_match_t saved_m;

        saved_m = *m;

        ss_dprintf_1(("su_pars_give_hint:%s\n", m->m_pos));

        if (su_pars_match_const(m, (char *)"HINT")) {
            bool succp = TRUE;
            dstr_t ds = NULL;

            dstr_app(&ds, (char *)" HINT ");
            while (!su_pars_match_const(m, (char *)"HINT")) {
                if (su_pars_get_nonspace(m, name, SU_MAXNAMELEN)) {
                    dstr_app(&ds, name);
                    dstr_app(&ds, (char *)" ");
                } else {
                    succp = FALSE;
                    break;
                }
            }
            if (succp && su_pars_match_const(m, (char *)"END")) {
                dstr_app(&ds, (char *)"HINT END ");
                *p_hint = ds;
                return(TRUE);
            } else {
                dstr_free(&ds);
                *m = saved_m;
                return(FALSE);
            }
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		su_pars_skipto_keyword
 * 
 * Skip to given keyword up to point end_str string.
 * 
 * Parameters : 
 * 
 *	m - use
 *		
 *		
 *	const_str - in
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
bool su_pars_skipto_keyword(
        su_pars_match_t* m,
        const char* const_str,
        const char* end_str)
{
        int len;
        bool more = TRUE;

        su_pars_check_comment(m);

        if (const_str[0] == '\0') {
            return(m->m_pos[0] == '\0');
        }
        
        len = strlen(const_str);

        while (more) {

            su_pars_check_comment(m);

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
            if (SsStrnicmp(m->m_pos, const_str, len) == 0) {
#else
            if (strncmp(m->m_pos, const_str, len) == 0) {
#endif                    
                m->m_pos += len;

                return(TRUE);
            } else {
                if(!(su_pars_match_const(m, (char *)end_str))) {
                    m->m_pos++;

                    if (*(m->m_pos) == '\0'){
                        return (FALSE);
                    }
                } else {
                    m->m_pos-=strlen(end_str);
                    return (FALSE);
                }
            }
        }
            
        return(FALSE);
}
