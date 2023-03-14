/*************************************************************************\
**  source       * su0chcvt.h
**  directory    * su
**  description  * Character translation tables for solid
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


#include <ssc.h>
#include <sschcvt.h>
#include <ssdebug.h>
#include <sslimits.h>
#include <ssmem.h>
#include <ssstring.h>
#include "su0cfgst.h"
#include "su0inifi.h"
#include "su0chcvt.h"

static int chcollation_cmp(uchar* s1, uchar* s2)
{
        int cmp;

        ss_dassert(!ss_isspace('\0'));
        for (;; s1++, s2++) {
            s1 = (uchar*)SsStrTrimLeft((char*)s1);
            s2 = (uchar*)SsStrTrimLeft((char*)s2);
            cmp = ss_toupper(*s1) - ss_toupper(*s2);
            if (cmp != 0) {
                break;
            }
            if (*s1 == '\0') {
                ss_dassert(*s2 == '\0');
                break;
            }
            ss_dassert(*s2 != '\0');
        }
        return (cmp);
}

bool su_chcvt_upcase_quoted = FALSE;
static char chcollation_fin[] = "Fin";
static char chcollation_iso8859_1[] = "ISO 8859-1";
static char chcollation_notvalid[] = "Not valid collation";

su_chcollation_t su_chcollation_byname(char* chcollation_name)
{
        if (chcollation_cmp((uchar*)chcollation_name, (uchar*)chcollation_fin) == 0) {
            return (SU_CHCOLLATION_FIN);
        }
        if (chcollation_cmp((uchar*)chcollation_name, (uchar*)chcollation_iso8859_1) == 0) {
            return (SU_CHCOLLATION_ISO8859_1);
        }
        return (SU_CHCOLLATION_NOTVALID);
}

char* su_chcollation_name(su_chcollation_t chcollation)
{
        switch (chcollation) {
            case SU_CHCOLLATION_ISO8859_1:
                return (chcollation_iso8859_1);
            case SU_CHCOLLATION_FIN:
                return (chcollation_fin);
            default:
                ss_derror;
                return (chcollation_notvalid);
        }
}

/*#***********************************************************************\
 * 
 *		chcvt_trivial
 * 
 * Returns its parameter
 * 
 * Parameters : 
 * 
 *	c - in
 *		character
 *		
 * Return value :
 *      same as parameter c
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static int chcvt_trivial(int c)
{
        return (c);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_clienttoserver_init
 * 
 * Creates a character translation table from client to server
 * translation
 * 
 * Parameters :
 *      chset - in
 *          tells which character set the client uses
 *
 *      chcollation - in
 *          character collation
 *
 * Return value - give : 
 *      pointer to allocated translation table
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uchar* su_chcvt_clienttoserver_init(
        su_chset_t chset,
        su_chcollation_t chcollation)
{
        uchar* table;
        uint i;
        int (*translate_fp)(int);

        ss_dprintf_2(("su_chcvt_clienttoserver_init("));
        switch (chset) {
            case SU_CHSET_DEFAULT:
                ss_dprintf_2(("SU_CHSET_DEFAULT)\n"));
                translate_fp = SsChCvtDef2Iso;
                break;
            case SU_CHSET_NOCNV    :
                ss_dprintf_2(("SU_CHSET_NOCNV)\n"));
                translate_fp = (int(*)(int))(ulong)0;
                break;
            case SU_CHSET_ANSI:
                ss_dprintf_2(("SU_CHSET_ANSI)\n"));
                translate_fp = chcvt_trivial;
                break;
            case SU_CHSET_PCOEM:
                ss_dprintf_2(("SU_CHSET_PCOEM)\n"));
                translate_fp = SsChCvtDos2Iso;
                break;
            case SU_CHSET_7BITSCAND:
                ss_dprintf_2(("SU_CHSET_7BITSCAND)\n"));
                translate_fp = SsChCvt7bitscand2Iso;
                break;
            default:
                ss_rc_error(chset);
                return (NULL);
        }
        table = SsMemAlloc(1 + UCHAR_MAX);
        if (translate_fp != (int(*)(int))(ulong)0) {
            switch (chcollation) {
                case SU_CHCOLLATION_FIN:
                    for (i = 0; i < 1 + UCHAR_MAX; i++) {
                        table[i] = (uchar)SsChCvtIso2Fin((*translate_fp)(i));
                        ss_dprintf_2(("%02X: %02X\n", i, (int)table[i]));
                    }
                    break;
                case SU_CHCOLLATION_ISO8859_1:
                    for (i = 0; i < 1 + UCHAR_MAX; i++) {
                        table[i] = (uchar)(*translate_fp)(i);
                        ss_dprintf_2(("%02X: %02X\n", i, (int)table[i]));
                    }
                    break;
                default:
                    ss_rc_error(chcollation);
            }
        } else {
            for (i = 0; i < 1 + UCHAR_MAX; i++) {
                table[i] = (uchar)i;
                ss_dprintf_2(("%02X: %02X\n", i, (int)table[i]));
            }
        }
        return (table);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_servertoclient_init
 * 
 * Creates a character translation table from server to client
 * translation
 * 
 * 
 * Parameters :
 *      chset - in
 *          tells which character set the client uses
 * 
 *      chcollation - in
 *          character collation
 *
 * Return value - give : 
 *      pointer to allocated translation table
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uchar* su_chcvt_servertoclient_init(
        su_chset_t chset,
        su_chcollation_t chcollation)
{
        uchar* table;
        uint i;
        int (*translate_fp)(int);

        switch (chset) {
            case SU_CHSET_DEFAULT:
                translate_fp = SsChCvtIso2Def;
                break;
            case SU_CHSET_NOCNV    :
                translate_fp = (int(*)(int))(ulong)0;
                break;
            case SU_CHSET_ANSI:
                translate_fp = chcvt_trivial;
                break;
            case SU_CHSET_PCOEM:
                translate_fp = SsChCvtIso2Dos;
                break;
            case SU_CHSET_7BITSCAND:
                translate_fp = SsChCvtIso27bitscand;
                break;
            default:
                ss_error;
                return (NULL);
        }
        table = SsMemAlloc(1 + UCHAR_MAX);
        if (translate_fp != (int(*)(int))(ulong)0) {
            switch (chcollation) {
                case SU_CHCOLLATION_FIN:
                    for (i = 0; i < 1 + UCHAR_MAX; i++) {
                        table[i] = (uchar)(*translate_fp)(SsChCvtFin2Iso(i));
                        ss_dprintf_2(("%02X: %02X\n", i, (int)table[i]));
                    }
                    break;
                case SU_CHCOLLATION_ISO8859_1:
                    for (i = 0; i < 1 + UCHAR_MAX; i++) {
                        table[i] = (uchar)(*translate_fp)(i);
                        ss_dprintf_2(("%02X: %02X\n", i, (int)table[i]));
                    }
                    break;
                default:
                    ss_error;
            }
        } else {
            for (i = 0; i < 1 + UCHAR_MAX; i++) {
                table[i] = (uchar)i;
            }
        }
        return (table);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_servertoupper_init
 * 
 * Creates a server character set toupper translation table
 * 
 * Parameters :
 *      chcollation - in
 *          character collation
 * 
 * Return value - give :
 *      Created translation table
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uchar* su_chcvt_servertoupper_init(su_chcollation_t chcollation)
{
        uchar* table = NULL;
        uint i;

        switch (chcollation) {
            case SU_CHCOLLATION_FIN:
                table = SsMemAlloc(1 + UCHAR_MAX);
                for (i = 0; i < 1 + UCHAR_MAX; i++) {
                    table[i] = (uchar)SsChCvtIso2Fin(ss_toupper(SsChCvtFin2Iso(i)));
                }
                break;
            case SU_CHCOLLATION_ISO8859_1:
                table = ss_chtoupper;
                break;
            default:
                ss_error;

        }
        return (table);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_servertolower_init
 * 
 * Creates a server character set tolower translation table
 * 
 * Parameters :
 * 
 *      chcollation - in
 *          character collation
 *
 * Return value - give :
 *      Created translation table
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
uchar* su_chcvt_servertolower_init(su_chcollation_t chcollation)
{
        uchar* table = NULL;
        uint i;

        switch (chcollation) {
            case SU_CHCOLLATION_FIN:
                table = SsMemAlloc(1 + UCHAR_MAX);
                for (i = 0; i < 1 + UCHAR_MAX; i++) {
                    table[i] = (uchar)SsChCvtIso2Fin(ss_tolower(SsChCvtFin2Iso(i)));
                }
                break;
            case SU_CHCOLLATION_ISO8859_1:
                table = ss_chtolower;
                break;
            default:
                ss_error;

        }
        return (table);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_done
 * 
 * Deletes translation table created using the above functions
 * 
 * Parameters : 
 * 
 *	table - in, take
 *		the table
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_chcvt_done(uchar* table)
{
        if (table == NULL
        ||  table == ss_chtoupper
        ||  table == ss_chtolower)
        {
            return;
        }
        SsMemFree(table);
}


/*##**********************************************************************\
 * 
 *		su_chcvt_strcvtuprdup
 * 
 * Creates a converted & uppercased duplicate of string
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		input string in native character set
 *		
 *      chcollation - in
 *          character collation
 *
 * Return value - give :
 *      uppercased heap-allocated copy of s in server character set
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_chcvt_strcvtuprdup(char* s, su_chcollation_t chcollation)
{
        uchar* table;
        size_t s_len;
        char* s_ret;

        s_len = strlen(s);
        s_ret = SsMemAlloc(s_len + 1);
        table = su_chcvt_clienttoserver_init(SU_CHSET_DEFAULT, chcollation);
        SsChCvtBuf(s_ret, s, s_len + 1, table);
        su_chcvt_done(table);
        table = su_chcvt_servertoupper_init(chcollation);
        SsChCvtBuf(s_ret, s_ret, s_len + 1, table);
        su_chcvt_done(table);
        return (s_ret);
}

typedef enum {
        SCANSTAT_NORMAL,
        SCANSTAT_SINGLEQUOT,
        SCANSTAT_DOUBLEQUOT,
        SCANSTAT_EOS,
        SCANSTAT_COMMENT,
        SCANSTAT_LIKEPAT
} sqlscanstat_t;



/*##**********************************************************************\
 * 
 *		su_chcvt_sqlstruprquotif
 * 
 * Uppercases an SQL string in-place
 * 
 * Parameters : 
 * 
 *      s - in out, use
 *          null terminated SQL string or part of an sql string
 *		
 *      client2server - in, use
 *          client to server translation table [256]
 *		
 *      server2client - in, use
 *          server to client translation table [256]
 *		
 *      state_init_char - in
 *          character that indicates the scan state where the scanning
 *          will be started:
 *          '"':  begin inside a double-quoted identifier
 *          '\'': begin inside a single-quoted SQL string literal
 *          '%':  the whole sql string is a like pattern
 *          ' ':  begin at begin-state of SQL string
 *		
 *      chcollation - in
 *          character collation
 *
 *      FLAGS - in
 *          option flags see "su0chcvt.h" for flag values
 *
 * Return value :
 *          TRUE when the string contains ODBC escape clauses
 *          FALSE when no escape clauses detected
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_chcvt_sqlstruprquotif(
        char* s,
        uchar* client2server,
        uchar* server2client,
        char state_init_char,
        su_chcollation_t chcollation,
        su_bflag_t flags)
{
        sqlscanstat_t scanstat;
        uchar* s_orig __attribute__ ((unused));
        bool escclause_detected = FALSE;
        bool nonblank_detected = FALSE;

        ss_dassert(s != NULL);
        s_orig = (uchar *)s;

        if (state_init_char == '"') {
            scanstat = SCANSTAT_DOUBLEQUOT;
        } else if (state_init_char == '\'') {
            scanstat = SCANSTAT_SINGLEQUOT;
        } else if (state_init_char == '%') {
            ss_dprintf_1(("su_chcvt_sqlstrupr() string = \"%s\"\n", s_orig));
            scanstat = SCANSTAT_LIKEPAT;
        } else {
            scanstat = SCANSTAT_NORMAL;
        }
        ss_dassert('z' - 'a' == 25 && 'Z' - 'A' == 25);
        for (;; s++) {
            switch (scanstat) {
                case SCANSTAT_NORMAL:
                    for (;;s++) {
                        switch (*s) {
                            case '\'':
                                scanstat = SCANSTAT_SINGLEQUOT;
                                break;
                            case '\"':
                                if (nonblank_detected) {
                                    scanstat = SCANSTAT_DOUBLEQUOT;
                                    break;
                                } else {
                                    nonblank_detected = TRUE;
                                }
                                continue;
                            case '\0':
                                scanstat = SCANSTAT_EOS;
                                break;
                            case '-':
                                if (s[1] == '-') {
                                    scanstat = SCANSTAT_COMMENT;
                                    s++;
                                    break;
                                }
                                continue;
                            case '{':
                                nonblank_detected = TRUE;
                                escclause_detected = TRUE;
                                continue;
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\r':
                            case '\f':
                            case '\v':
                                continue;
                            default:
                                nonblank_detected = TRUE;
                                if (!SU_BFLAG_TEST(flags, SU_CHCVT_CONVERT_ONLY))
                                { 
                                    if (SU_SQLISLOWER(*s)) {
                                        *s -= 'a' - 'A';
                                    }
                                }
                                continue;
                        }
                        break;
                    }
                    continue;
                case SCANSTAT_SINGLEQUOT:
                    nonblank_detected = TRUE;
                    if (client2server != NULL) {
                        for (;;s++) {
                            switch (*s) {
                                case '\'':
                                    scanstat = SCANSTAT_NORMAL;
                                    break;
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    ss_dprintf_2(("su_chcvt_sqlstrupr() xlat: %02X: %02X\n",
                                        (int)(uchar)*s,
                                        (int)client2server[(uchar)*s]));
                                    *s = (char)client2server[(uchar)*s];
                                    continue;
                            }
                            break;
                        }
                    } else {
                        for (;;s++) {
                            switch (*s) {
                                case '\'':
                                    scanstat = SCANSTAT_NORMAL;
                                    break;
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    continue;
                            }
                            break;
                        }
                    }
                    continue;
                case SCANSTAT_DOUBLEQUOT:
                    if (client2server != NULL) {
                        for (;;s++) {
                            switch (*s) {
                                case '"':
                                    scanstat = SCANSTAT_NORMAL;
                                    break;
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    *s = (char)client2server[(uchar)*s];
                                    if (SU_BFLAG_TEST(flags, SU_CHCVT_UPCASE_QUOTED))
                                    {
                                        ss_dassert(!SU_BFLAG_TEST(
                                                flags, SU_CHCVT_CONVERT_ONLY));
                                        if (SU_SQLISLOWER(*s)) {
                                            *s -= 'a' - 'A';
                                        }
                                    }
                                    continue;
                            }
                            break;
                        }
                    } else {
                        for (;;s++) {
                            switch (*s) {
                                case '"':
                                    scanstat = SCANSTAT_NORMAL;
                                    break;
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    if (SU_BFLAG_TEST(flags, SU_CHCVT_UPCASE_QUOTED))
                                    {
                                        ss_dassert(!SU_BFLAG_TEST(
                                                flags, SU_CHCVT_CONVERT_ONLY));
                                        if (SU_SQLISLOWER(*s)) {
                                            *s -= 'a' - 'A';
                                        }
                                    }
                                    continue;
                            }
                            break;
                        }
                    }
                    continue;
                case SCANSTAT_COMMENT:
                    if (s[0] == '(' && s[1] == '*') {
                        nonblank_detected = TRUE;
                        escclause_detected = TRUE;
                        scanstat = SCANSTAT_NORMAL;
                        continue;

                    }
                    for (;;s++) {
                        switch (*s) {
                            case '\0':
                                scanstat = SCANSTAT_EOS;
                                break;
                            case '\n':
                                scanstat = SCANSTAT_NORMAL;
                                break;
                            default:
                                continue;
                        }
                        break;
                    }
                    continue;
                case SCANSTAT_LIKEPAT:
                    nonblank_detected = TRUE;
                    if (SU_BFLAG_TEST(flags, SU_CHCVT_CONVERT_ONLY)) {
                        scanstat = SCANSTAT_EOS;
                        break;
                    }
                    if (client2server != NULL) {
                        ss_dassert(server2client != NULL);
                        for (;;s++) {
                            switch (*s) {
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    *s = (char)client2server[(uchar)*s];
                                    if (chcollation == SU_CHCOLLATION_FIN) {
                                        if (SU_SQLISLOWER(*s)) {
                                            *s -= 'a' - 'A';
                                        } else {
                                            *s = (char)SsChCvtFin2Iso((int)(uchar)*s);
                                            *s = (char)ss_toupper((int)(uchar)*s);
                                            *s = (char)SsChCvtIso2Fin((int)(uchar)*s);
                                        }
                                    } else {
                                        ss_dassert(chcollation == SU_CHCOLLATION_ISO8859_1);
                                        *s = (char)ss_toupper((int)(uchar)*s);
                                    }
                                    *s = (char)server2client[(uchar)*s];
                                    continue;
                            }
                            break;
                        }
                    } else {
                        for (;;s++) {
                            switch (*s) {
                                case '\0':
                                    scanstat = SCANSTAT_EOS;
                                    break;
                                default:
                                    if (SU_SQLISLOWER(*s)) {
                                        *s -= 'a' - 'A';
                                    }
                                    continue;
                            }
                            break;
                        }
                    }
                    continue;
                case SCANSTAT_EOS:
                    break;
                default:
                    ss_error;
            }
            break;
        }
#ifdef SS_DEBUG
        if (state_init_char == '%') {
            ss_dprintf_1(("su_chcvt_sqlstrupr() converted string = \"%s\"\n", s_orig));
        }
#endif
        return (escclause_detected);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_sqlstrupr
 * 
 * Uppercases an SQL string in-place uppercases also quoted identifiers
 * 
 * Parameters : 
 * 
 *	s - in out, use
 *		null terminated SQL string or part of an sql string
 *		
 *	client2server - in, use
 *		client to server translation table [256]
 *		
 *	server2client - in, use
 *		server to client translation table [256]
 *		
 *	state_init_char - in
 *		character that indicates the scan state where the scanning
 *          will be started:
 *          '"':  begin inside a double-quoted identifier
 *          '\'': begin inside a single-quoted SQL string literal
 *          '%':  the whole sql string is a like pattern
 *          ' ':  begin at begin-state of SQL string
 *		
 *      chcollation - in
 *          character collation
 *
 * Return value :
 *          TRUE when the string contains ODBC escape clauses
 *          FALSE when no escape clauses detected
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_chcvt_sqlstrupr(
        char* s,
        uchar* client2server,
        uchar* server2client,
        char state_init_char,
        su_chcollation_t chcollation)
{
        bool escclause_detected;

        escclause_detected =
            su_chcvt_sqlstruprquotif(
                s,
                client2server,
                server2client,
                state_init_char,
                chcollation,
                SU_CHCVT_UPCASE_QUOTED);
        return (escclause_detected);
}

/*##**********************************************************************\
 * 
 *		su_chcvt_inifilechset
 * 
 * Returns the default character set used. The default character
 * set is first tried to find from the inifile. If the character set
 * is not specified in the inifile, SU_CHSET_DEFAULT is returned.
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 *      Default character set.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_chset_t su_chcvt_inifilechset(void)
{
        su_chset_t chset;
        bool foundp;
        su_inifile_t* inifile;
        char* name;

        inifile = su_inifile_init((char *)"solid.ini", &foundp);
        foundp = su_inifile_getstring(
                    inifile,
                    SU_CLI_SECTION,
                    SU_CLI_CHSET,
                    &name);
        if (foundp) {
            if (SsStricmp(name, "nocnv") == 0) {
                chset = SU_CHSET_NOCNV;
            } else if (SsStricmp(name, "ansi") == 0) {
                chset = SU_CHSET_ANSI;
            } else if (SsStricmp(name, "pcoem") == 0) {
                chset = SU_CHSET_PCOEM;
            } else if (SsStricmp(name, "7bitscand") == 0) {
                chset = SU_CHSET_7BITSCAND;
            } else {
                chset = SU_CHSET_DEFAULT;
            }
            SsMemFree(name);
        } else {
            chset = SU_CHSET_DEFAULT;
        }
        su_inifile_done(inifile);

        return(chset);
}

static const ss_char1_t chcvt_byte_to_hex[1 << SS_CHAR_BIT][2] = {
        { '0','0' }, { '0','1' }, { '0','2' }, { '0','3' },
        { '0','4' }, { '0','5' }, { '0','6' }, { '0','7' },
        { '0','8' }, { '0','9' }, { '0','A' }, { '0','B' },
        { '0','C' }, { '0','D' }, { '0','E' }, { '0','F' },
        { '1','0' }, { '1','1' }, { '1','2' }, { '1','3' },
        { '1','4' }, { '1','5' }, { '1','6' }, { '1','7' },
        { '1','8' }, { '1','9' }, { '1','A' }, { '1','B' },
        { '1','C' }, { '1','D' }, { '1','E' }, { '1','F' },
        { '2','0' }, { '2','1' }, { '2','2' }, { '2','3' },
        { '2','4' }, { '2','5' }, { '2','6' }, { '2','7' },
        { '2','8' }, { '2','9' }, { '2','A' }, { '2','B' },
        { '2','C' }, { '2','D' }, { '2','E' }, { '2','F' },
        { '3','0' }, { '3','1' }, { '3','2' }, { '3','3' },
        { '3','4' }, { '3','5' }, { '3','6' }, { '3','7' },
        { '3','8' }, { '3','9' }, { '3','A' }, { '3','B' },
        { '3','C' }, { '3','D' }, { '3','E' }, { '3','F' },
        { '4','0' }, { '4','1' }, { '4','2' }, { '4','3' },
        { '4','4' }, { '4','5' }, { '4','6' }, { '4','7' },
        { '4','8' }, { '4','9' }, { '4','A' }, { '4','B' },
        { '4','C' }, { '4','D' }, { '4','E' }, { '4','F' },
        { '5','0' }, { '5','1' }, { '5','2' }, { '5','3' },
        { '5','4' }, { '5','5' }, { '5','6' }, { '5','7' },
        { '5','8' }, { '5','9' }, { '5','A' }, { '5','B' },
        { '5','C' }, { '5','D' }, { '5','E' }, { '5','F' },
        { '6','0' }, { '6','1' }, { '6','2' }, { '6','3' },
        { '6','4' }, { '6','5' }, { '6','6' }, { '6','7' },
        { '6','8' }, { '6','9' }, { '6','A' }, { '6','B' },
        { '6','C' }, { '6','D' }, { '6','E' }, { '6','F' },
        { '7','0' }, { '7','1' }, { '7','2' }, { '7','3' },
        { '7','4' }, { '7','5' }, { '7','6' }, { '7','7' },
        { '7','8' }, { '7','9' }, { '7','A' }, { '7','B' },
        { '7','C' }, { '7','D' }, { '7','E' }, { '7','F' },
        { '8','0' }, { '8','1' }, { '8','2' }, { '8','3' },
        { '8','4' }, { '8','5' }, { '8','6' }, { '8','7' },
        { '8','8' }, { '8','9' }, { '8','A' }, { '8','B' },
        { '8','C' }, { '8','D' }, { '8','E' }, { '8','F' },
        { '9','0' }, { '9','1' }, { '9','2' }, { '9','3' },
        { '9','4' }, { '9','5' }, { '9','6' }, { '9','7' },
        { '9','8' }, { '9','9' }, { '9','A' }, { '9','B' },
        { '9','C' }, { '9','D' }, { '9','E' }, { '9','F' },
        { 'A','0' }, { 'A','1' }, { 'A','2' }, { 'A','3' },
        { 'A','4' }, { 'A','5' }, { 'A','6' }, { 'A','7' },
        { 'A','8' }, { 'A','9' }, { 'A','A' }, { 'A','B' },
        { 'A','C' }, { 'A','D' }, { 'A','E' }, { 'A','F' },
        { 'B','0' }, { 'B','1' }, { 'B','2' }, { 'B','3' },
        { 'B','4' }, { 'B','5' }, { 'B','6' }, { 'B','7' },
        { 'B','8' }, { 'B','9' }, { 'B','A' }, { 'B','B' },
        { 'B','C' }, { 'B','D' }, { 'B','E' }, { 'B','F' },
        { 'C','0' }, { 'C','1' }, { 'C','2' }, { 'C','3' },
        { 'C','4' }, { 'C','5' }, { 'C','6' }, { 'C','7' },
        { 'C','8' }, { 'C','9' }, { 'C','A' }, { 'C','B' },
        { 'C','C' }, { 'C','D' }, { 'C','E' }, { 'C','F' },
        { 'D','0' }, { 'D','1' }, { 'D','2' }, { 'D','3' },
        { 'D','4' }, { 'D','5' }, { 'D','6' }, { 'D','7' },
        { 'D','8' }, { 'D','9' }, { 'D','A' }, { 'D','B' },
        { 'D','C' }, { 'D','D' }, { 'D','E' }, { 'D','F' },
        { 'E','0' }, { 'E','1' }, { 'E','2' }, { 'E','3' },
        { 'E','4' }, { 'E','5' }, { 'E','6' }, { 'E','7' },
        { 'E','8' }, { 'E','9' }, { 'E','A' }, { 'E','B' },
        { 'E','C' }, { 'E','D' }, { 'E','E' }, { 'E','F' },
        { 'F','0' }, { 'F','1' }, { 'F','2' }, { 'F','3' },
        { 'F','4' }, { 'F','5' }, { 'F','6' }, { 'F','7' },
        { 'F','8' }, { 'F','9' }, { 'F','A' }, { 'F','B' },
        { 'F','C' }, { 'F','D' }, { 'F','E' }, { 'F','F' }
};

/*##**********************************************************************\
 * 
 *		su_chcvt_bin2hex
 * 
 * Converts binary data to char by generating two hexadecimal
 * digits for every binary byte
 * 
 * Parameters : 
 * 
 *	dest - out
 *		destination buffer
 *		
 *	src - in
 *		source buffer
 *		
 *	nbytes - in
 *		number of binary bytes to convert
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_chcvt_bin2hex(
        ss_char1_t* dest,
        ss_byte_t* src,
        size_t nbytes)
{
        register const char* p;

        src += nbytes;
        dest += 2 * nbytes;
        *dest = '\0';
        while (nbytes) {
            --src;
            dest -= 2;
            --nbytes;
            p = &chcvt_byte_to_hex[*src][0];
            dest[0] = p[0];
            dest[1] = p[1];
        }
}

void su_chcvt_bin2hexchar2(
        ss_char2_t* dest,
        ss_byte_t* src,
        size_t nbytes)
{
        register const ss_byte_t* p;

        src += nbytes;
        dest += nbytes * 2;
        *dest = (ss_char2_t)0;
        while (nbytes) {
            --src;
            dest -= 2;
            --nbytes;
            p = (ss_byte_t*)&chcvt_byte_to_hex[*src][0];
            dest[0] = (ss_char2_t)p[0];
            dest[1] = (ss_char2_t)p[1];
        }
}

void su_chcvt_bin2hexchar2_va(
        ss_char2_t* dest,
        ss_byte_t* src,
        size_t nbytes)
{
        register const ss_byte_t* p;

        ss_dassert((ss_byte_t*)dest != src);
        while (nbytes) {
            ss_char2_t c;
            --nbytes;
            p = (ss_byte_t*)&chcvt_byte_to_hex[*src][0];
            c = (ss_char2_t)*p++;
            SS_CHAR2_STORE_MSB1ST(dest, c);
            dest++;
            c = (ss_char2_t)*p;
            SS_CHAR2_STORE_MSB1ST(dest, c);
            src++;
            dest++;
        }
}

bool su_chcvt_hex2binchar2(
        ss_byte_t* dest,
        ss_char2_t* src,
        size_t nbytes)
{
        uint i;
        uint value;

        while (nbytes) {
            nbytes--;
            for (i = 0, value = 0; i < 2; i++, src++) {
                value <<= 4;
                switch (*src) {
                    case '0':
                        break;
                    case '1':
                        value += 0x01;
                        break;
                    case '2':
                        value += 0x02;
                        break;
                    case '3':
                        value += 0x03;
                        break;
                    case '4':
                        value += 0x04;
                        break;
                    case '5':
                        value += 0x05;
                        break;
                    case '6':
                        value += 0x06;
                        break;
                    case '7':
                        value += 0x07;
                        break;
                    case '8':
                        value += 0x08;
                        break;
                    case '9':
                        value += 0x09;
                        break;
                    case 'A': case 'a':
                        value += 0x0A;
                        break;
                    case 'B': case 'b':
                        value += 0x0B;
                        break;
                    case 'C': case 'c':
                        value += 0x0C;
                        break;
                    case 'D': case 'd':
                        value += 0x0D;
                        break;
                    case 'E': case 'e':
                        value += 0x0E;
                        break;
                    case 'F': case 'f':
                        value += 0x0F;
                        break;
                    default:
                        return (FALSE);
                }
            }
            *dest++ = (ss_byte_t)value;
        }
        return (TRUE);
}

bool su_chcvt_hex2binchar2_va(
        ss_byte_t* dest,
        ss_char2_t* src,
        size_t nbytes)
{
        uint i;
        uint value;

        while (nbytes) {
            nbytes--;
            for (i = 0, value = 0; i < 2; i++, src++) {
                ss_char2_t c = SS_CHAR2_LOAD_MSB1ST(src);

                value <<= 4;
                switch (c) {
                    case '0':
                        break;
                    case '1':
                        value += 0x01;
                        break;
                    case '2':
                        value += 0x02;
                        break;
                    case '3':
                        value += 0x03;
                        break;
                    case '4':
                        value += 0x04;
                        break;
                    case '5':
                        value += 0x05;
                        break;
                    case '6':
                        value += 0x06;
                        break;
                    case '7':
                        value += 0x07;
                        break;
                    case '8':
                        value += 0x08;
                        break;
                    case '9':
                        value += 0x09;
                        break;
                    case 'A': case 'a':
                        value += 0x0A;
                        break;
                    case 'B': case 'b':
                        value += 0x0B;
                        break;
                    case 'C': case 'c':
                        value += 0x0C;
                        break;
                    case 'D': case 'd':
                        value += 0x0D;
                        break;
                    case 'E': case 'e':
                        value += 0x0E;
                        break;
                    case 'F': case 'f':
                        value += 0x0F;
                        break;
                    default:
                        return (FALSE);
                }
            }
            *dest++ = (ss_byte_t)value;
        }
        return (TRUE);
}

/*##***********************************************************************\
 * 
 *		su_chcvt_hex2bin
 * 
 * Translates a hexadecimal encoded binary data from buffer to
 * binary data type
 * 
 * Parameters : 
 * 
 *	dest - out
 *		destination binary buffer
 *		
 *	src - in
 *		source hexdump buffer
 *		
 *	nbytes - in
 *		number of binary bytes to be fed into dest buffer
 *		
 * Return value :
 *      TRUE when OK or
 *      FALSE when the source buffer was not a legal hexdump
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool su_chcvt_hex2bin(
        ss_byte_t* dest,
        ss_char1_t* src,
        size_t nbytes)
{
        uint i;
        uint value;

        while (nbytes) {
            nbytes--;
            for (i = 0, value = 0; i < 2; i++, src++) {
                value <<= 4;
                switch (*src) {
                    case '0':
                        break;
                    case '1':
                        value += 0x01;
                        break;
                    case '2':
                        value += 0x02;
                        break;
                    case '3':
                        value += 0x03;
                        break;
                    case '4':
                        value += 0x04;
                        break;
                    case '5':
                        value += 0x05;
                        break;
                    case '6':
                        value += 0x06;
                        break;
                    case '7':
                        value += 0x07;
                        break;
                    case '8':
                        value += 0x08;
                        break;
                    case '9':
                        value += 0x09;
                        break;
                    case 'A': case 'a':
                        value += 0x0A;
                        break;
                    case 'B': case 'b':
                        value += 0x0B;
                        break;
                    case 'C': case 'c':
                        value += 0x0C;
                        break;
                    case 'D': case 'd':
                        value += 0x0D;
                        break;
                    case 'E': case 'e':
                        value += 0x0E;
                        break;
                    case 'F': case 'f':
                        value += 0x0F;
                        break;
                    default:
                        return (FALSE);
                }
            }
            *dest++ = (ss_byte_t)value;
        }
        return (TRUE);
}
