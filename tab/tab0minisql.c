/*************************************************************************\
**  source       * tab0minisql.c
**  directory    * tab
**  description  * Table level MINI SQL functions.
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
#include <ssdebug.h>

#include <su0err.h>
#include <su0pars.h>

#include <rs0types.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0atype.h>
#include <rs0aval.h>
#include <rs0entna.h>

#include "tab1defs.h"
#include "tab0conn.h"
#include "tab0tli.h"
#include "tab0admi.h"
#include "tab0relh.h"
#include "tab1dd.h"

#include "tab0type.h"
#include "tab0minisql.h"

/*#***********************************************************************\
 *
 *              sql_mini_parse_primarykey
 *
 * Parse primary key definition from the SQL-string
 *
 * Parameters :
 *
 *   su_pars_match_t*  m, in, use, SQL-string
 *   uint              nattrs, in, use, number of attributes
 *   char*             attrs, in, use, attribute names
 *
 * Return value : tb_sqlunique_t*
 *
 * Globals used : -
 */
static tb_sqlunique_t* sql_mini_parse_primarykey(
        su_pars_match_t* m,
        uint nattrs,
        char* attrs[])
{
        tb_sqlunique_t* primkey = NULL;
        bool done = FALSE;
        bool found;
        char buf[64];
        int n,i;

        SS_PUSHNAME("sql_mini_parse_primarykey");
        primkey = (tb_sqlunique_t*)SsMemAlloc(sizeof(tb_sqlunique_t));

        primkey->len = nattrs;
        primkey->name = NULL;
        primkey->fields = (uint*)SsMemAlloc(primkey->len*sizeof(uint));
        primkey->prefixes = NULL;
        i = 0;
        n = 0;

        while (!done) {
            if (!su_pars_get_id(m, buf, sizeof(buf))) {
                ss_error;
            }
            found = FALSE;
            for (i=0;i< (int)nattrs;i++) {
                if (SsStricmp(buf, attrs[i]) == 0) {
                    primkey->fields[n] = i;
                    found = TRUE;
                }
            }
            ss_assert(found);
            n++;
            ss_assert(n <= (int)nattrs);
            if (!su_pars_match_const(m, (char *)",")) {
                done = TRUE;
            }
        }
        primkey->len  = n;

        SS_POPNAME;
        return(primkey);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_atype
 *
 * Parse attribute type from the SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * su_pars_match_t*  m, in, use, SQL-string
 *
 * Return value : tb_atype_t*
 *
 * Globals used : -
 */
static rs_atype_t* sql_mini_prepare_atype(
        rs_sysi_t* cd,
        su_pars_match_t* m)
{
        rs_atype_t* atype;
        rs_err_t* errh;
        bool nullallowed = TRUE;
        bool islong = FALSE;
        char typename[32];
        char* pars = NULL; /* len, scale */
        uint len;
        uint scale;

        SS_PUSHNAME("sql_mini_prepare_atype");
        /* typename */
        if (su_pars_match_keyword(m, (char *)"LONG")) {
            islong = TRUE;
        }

        if (!su_pars_get_id(m, typename, sizeof(typename))) {
            ss_error;
        }
        /* len, scale */
        if (su_pars_match_const(m, (char *)"(")) {
            /* len */
            if (!su_pars_get_uint(m, &len)) {
                ss_error;
            }
            /* scale */
            scale = 0;
            if (su_pars_match_const(m, (char *)",")) {
                if (!su_pars_get_uint(m, &scale)) {
                    ss_error;
                }
            }
            if (!su_pars_match_const(m, (char *)")")) {
                ss_error;
            }
            pars = SsMemAlloc(64);
            if (scale == 0) {
                SsSprintf(pars,"%d", len);
            } else {
                SsSprintf(pars,"%d,%d", len, scale);
            }

        }
        /* nullable */
        if (su_pars_match_keyword(m, (char *)"NOT")) {
            if (su_pars_match_keyword(m, (char *)"NULL")) {
                nullallowed = FALSE;
            } else {
                ss_error;
            }
        }

        atype = rs_atype_create(cd, typename, pars, nullallowed, &errh);
        if (pars != NULL) {
            SsMemFree(pars);
        }
        SS_POPNAME;

        return(atype);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_attrtype
 *
 * Set attribute type to table type
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * su_pars_match_t*  m, in, use, SQL-string
 * uint              attr_c, in, use, attribute count
 * char*             name, in, use, attribute name
 * rs_ttype_t*       ttype, in, use, table type
 *
 * Return value : -
 *
 * Globals used : -
 */
static void sql_mini_prepare_attrtype(
        rs_sysi_t* cd,
        su_pars_match_t* m,
        uint attr_c, char* name,
        rs_ttype_t* ttype)
{
        rs_atype_t* atype;
        rs_err_t* errh;

        SS_PUSHNAME("sql_mini_prepare_attrtype");

        atype = sql_mini_prepare_atype(cd, m);
        rs_ttype_setatype(cd, ttype, attr_c, atype);
        rs_ttype_sql_setaname(cd, ttype, attr_c, name);
        rs_atype_free(cd, atype);
        SS_POPNAME;
}

#define MINI_SQL_MAX_ATTRS      300

/*#***********************************************************************\
 *
 *              sql_mini_prepare_createtable
 *
 * Parse create table from SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * sqltrans_t*       trans, in, use, transaction
 * char*             sqlstr, in, use, SQL-string
 * rs_err_t**        p_errh, in ,use, error structure
 * su_pars_match_t*  m, in, use, parsing info
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */
static bool sql_mini_prepare_createtable(
        void* cd,
        sqltrans_t* trans,
        char* sqlstr __attribute__ ((unused)),
        rs_err_t** p_errh,
        su_pars_match_t* m)
{
        char buf[64];
        char relname[64];
        bool succp = TRUE;
        rs_ttype_t* ttype;
        bool finishedp __attribute__ ((unused));
        bool done = FALSE;
#ifndef SS_MYSQL
        int n,i;
#endif
        uint                attr_c = 0;
        char**              attrs = NULL;

        tb_sqlunique_t*     primkey = NULL;
        uint                unique_c = 0;
        tb_sqlunique_t*     unique = NULL;
        uint                forkey_c = 0;
        tb_sqlforkey_t*     forkeys = NULL;
        uint*               def = NULL;
        rs_tval_t*          defvalue = NULL;
        uint                check_c = 0;
        char**              checks = NULL;
        char**              checknames = NULL;
        tb_dd_createrel_t   createtype = TB_DD_CREATEREL_USER;
        tb_dd_persistency_t persistencytype = TB_DD_PERSISTENCY_PERMANENT;
        tb_dd_store_t       storetype = TB_DD_STORE_DISK;
        tb_dd_durability_t  durability = TB_DD_DURABILITY_DEFAULT;
        char*               authid;
        char*               catalog;
        char*               extrainfo = NULL;

        finishedp = FALSE;

        SS_PUSHNAME("sql_mini_prepare_createtable");
        catalog = rs_sdefs_getcurrentdefcatalog();
        authid = (char *)RS_AVAL_SYSNAME;

        if (!su_pars_get_id(m, relname, sizeof(relname))) {
            ss_error;
        }

        if (!su_pars_match_const(m, (char *)"(")) {
            ss_error;
        }

        ttype = rs_ttype_create(cd);
        attrs = SsMemAlloc(MINI_SQL_MAX_ATTRS*sizeof(char *));
        attr_c = 0;
        while (!done) {
            if (su_pars_match_keyword(m, (char *)"PRIMARY")) {
                if (!su_pars_match_keyword(m, (char *)"KEY")) {
                    ss_error;
                }
                if (!su_pars_match_const(m, (char *)"(")) {
                    ss_error;
                }
                ss_assert(attr_c > 0);
                primkey = sql_mini_parse_primarykey(m, attr_c, attrs);
                if (!su_pars_match_const(m, (char *)")")) {
                    ss_error;
                }
                done = TRUE;
            } else {
                /* attribute name */
                if (!su_pars_get_id(m, buf, sizeof(buf))) {
                    ss_error;
                }
                attrs[attr_c] = (char *)SsMemStrdup(buf);
                sql_mini_prepare_attrtype(cd, m, attr_c, buf, ttype);
                attr_c++;
                ss_assert(attr_c < MINI_SQL_MAX_ATTRS);
                if (!su_pars_match_const(m, (char *)",")) {
                    done = TRUE;
                }
            }
        }

        if (su_pars_match_const(m, (char *)",")) {
            if (su_pars_match_keyword(m, (char *)"UNIQUE")) {
                if (!su_pars_match_const(m, (char *)"(")) {
                    ss_error;
                }
                ss_assert(attr_c > 0);
                unique = sql_mini_parse_primarykey(m, attr_c, attrs);
                if (!su_pars_match_const(m, (char *)")")) {
                    ss_error;
                }
                unique_c = 1;
            } else {
                ss_error;
            }
        }

        if (!su_pars_match_const(m, (char *)")")) {
            ss_error;
        }
        if (!su_pars_match_keyword(m, (char *)"")) {
            if (!su_pars_match_const(m, (char *)"@")) {
                ss_error;
            }
        }

        SS_PUSHNAME("sql_mini_prepare_createtable:tb_createrelation_ext");
        succp = tb_createrelation_ext(
                        cd,
                        trans,
                        relname,
                        authid,
                        catalog,
                        extrainfo,
                        ttype,
                        primkey,
                        unique_c,
                        unique,
                        forkey_c,
                        forkeys,
                        def,
                        defvalue,
                        check_c,
                        checks,
                        checknames,
                        createtype,
                        persistencytype,
                        storetype,
                        durability,
                        TB_RELMODE_SYSDEFAULT,
                        NULL,
                        p_errh);
        SS_POPNAME;
        rs_ttype_free(cd, ttype);

        if (primkey != NULL) {
            SsMemFree(primkey->fields);
            SsMemFree(primkey);
        }
        if (unique != NULL) {
            SsMemFree(unique->fields);
            SsMemFree(unique);
        }
        while (attr_c > 0) {
            attr_c--;
            SsMemFree(attrs[attr_c]);
        }
        SsMemFree(attrs);

        SS_POPNAME;
        return(succp);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_altertable
 *
 * Parse alter table from SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * sqltrans_t*       trans, in, use, transaction
 * char*             sqlstr, in, use, SQL-string
 * rs_err_t**        p_errh, in ,use, error structure
 * su_pars_match_t*  m, in, use, parsing info
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */

static bool sql_mini_prepare_altertable(
        void* cd,
        sqltrans_t* trans,
        char* sqlstr __attribute__ ((unused)),
        rs_err_t** p_errh,
        su_pars_match_t* m)
{
        char colname[64];
        char authid[64];
        char relname[64];
        su_ret_t rc;
        char* catalog;
        rs_atype_t* atype;
        tb_relh_t* tbrelh;
        rs_relh_t* relh;
        rs_auth_t* auth;
        rs_ttype_t* ttype;
        su_list_t*  attrlist;

        SS_PUSHNAME("sql_mini_prepare_altertable");

        catalog = rs_sdefs_getcurrentdefcatalog();

        if (!su_pars_get_tablename(m, authid, sizeof(authid), relname, sizeof(relname))) {
            ss_error;
        }
        if (!su_pars_match_const(m, (char *)"ADD")) {
            ss_error;
        }
        if (!su_pars_match_const(m, (char *)"COLUMN")) {
            ss_error;
        }

        /* attribute name */
        if (!su_pars_get_id(m, colname, sizeof(colname))) {
            ss_error;
        }
        /* attribute type */
        atype = sql_mini_prepare_atype(cd, m);

        tbrelh = tb_relh_create(
                    cd,
                    trans,
                    relname,
                    authid,
                    catalog,
                    p_errh);
        if (tbrelh == NULL) {
            ss_derror;
            SS_POPNAME;
            return(FALSE);
        }

        relh = tb_relh_rsrelh(cd, tbrelh);
        ttype = rs_relh_ttype(cd, relh);
        auth = rs_sysi_auth(cd);

        attrlist = tb_dd_attrlist_init();

        tb_dd_attrlist_add(
            attrlist,
            colname,
            atype,
            auth,
            NULL,
            NULL,
            NULL,
            FALSE);

        rc = tb_dd_addattributelist(
                cd,
                trans,
                relh,
                attrlist,
                FALSE,
                p_errh);

        tb_dd_attrlist_done(attrlist);

        rs_atype_free(cd, atype);

        SS_POPNAME;

        return(rc == SU_SUCCESS);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_createindex
 *
 * Parse create index from SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * sqltrans_t*       trans, in, use, transaction
 * rs_relh_t*        relh, in, use, relation
 * char*             sqlstr, in, use, SQL-string
 * bool              unique, in, use, unique index or not
 * rs_err_t**        p_errh, in ,use, error structure
 * su_pars_match_t*  m, in, use, parsing info
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */
static bool sql_mini_prepare_createindex(
        void* cd,
        sqltrans_t* trans,
        rs_relh_t* relh,
        char* sqlstr __attribute__ ((unused)),
        bool unique,
        rs_err_t** p_errh __attribute__ ((unused)),
        su_pars_match_t* m)
{
        char*               catalog;
        rs_ttype_t*         ttype = NULL;
        uint                attr_c = 0;
        char**              attrs = NULL;
        bool*               desc = NULL; /* FALSE */
        tb_dd_createrel_t   type = TB_DD_CREATEREL_USER;
        bool succp = FALSE;
        bool done = FALSE;
        char buf[64];
        char authid[64];
        char relname[64];
        char indexname[64];
        rs_err_t* errh = NULL;

        SS_PUSHNAME("sql_mini_prepare_createindex");
        catalog = rs_sdefs_getcurrentdefcatalog();

        if (!su_pars_get_id(m, indexname, sizeof(indexname))) {
            ss_error;
        }
        if (!su_pars_match_keyword(m, (char *)"ON")) {
            ss_error;
        }

        strcpy(authid, RS_AVAL_SYSNAME);
        if (!su_pars_get_tablename(m, authid, sizeof(authid), relname, sizeof(relname))) {
            ss_error;
        }

        if (relh == NULL) {
            rs_entname_t name;
            rs_entname_initbuf(&name,
                               catalog,
                               authid,
                               relname);

            relh = tb_dd_getrelh(cd, trans, &name, NULL, NULL);
        } else {
            ss_error;
        }
        ss_bassert(relh != NULL);

        if (!su_pars_match_const(m, (char *)"(")) {
            ss_error;
        }

        attrs = SsMemAlloc(30*sizeof(char *));
        desc = SsMemAlloc(30*sizeof(bool));
        attr_c = 0;
        while (!done) {
            if (!su_pars_get_id(m, buf, sizeof(buf))) {
                ss_error;
            }
            attrs[attr_c] = (char *)SsMemStrdup(buf);
            desc[attr_c] = FALSE;
            attr_c++;
            ss_assert(attr_c < 30);
            if (!su_pars_match_const(m, (char *)",")) {
                done = TRUE;
            }
        }
        if (!su_pars_match_const(m, (char *)")")) {
            ss_error;
        }
        if (!su_pars_match_keyword(m, (char *)"")) {
            if (!su_pars_match_const(m, (char *)"@")) {
                ss_error;
            }
        }

        ttype = rs_relh_ttype(cd, relh);

        succp = tb_createindex_ext(
                    cd,
                    trans,
                    indexname,
                    authid,
                    catalog,
                    relh,
                    ttype,
                    unique,
                    attr_c,
                    attrs,
                    desc,
#ifdef SS_COLLATION
                    NULL,
#endif /* SS_COLLATION */
                    type,
                    &errh);

        if (!succp) {
            if (rs_error_geterrcode(cd, errh) == E_KEYNAMEEXIST_S) {
                ss_pprintf_1(("Index on %s allready exist\n", relname));
                rs_error_free(cd, errh);
                succp = TRUE;
            }
        }

        while (attr_c > 0) {
            attr_c--;
            SsMemFree(attrs[attr_c]);
        }

        SsMemFree(attrs);
        SsMemFree(desc);
        ss_dassert(succp);

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        SS_POPNAME;
        return(succp);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_value
 *
 * Parse default value of the attribute from SQL-string
 *
 * Parameters :
 *
 * su_pars_match_t*  m, in, use, SQL-string
 * char*             buf, in, use, buffer where to store value
 * uint              bufsize, in, use, buffer size
 * bool*             p_nullval, in out, TRUE if NULL value
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */
static bool sql_mini_prepare_value(
        su_pars_match_t* m,
        char* buf,
        uint bufsize,
        bool* p_nullval)
{
        bool negat = FALSE;
        char numbuf[32];

        SS_PUSHNAME("sql_mini_prepare_value");
        *p_nullval = FALSE;

        if (su_pars_get_stringliteral(m , buf, bufsize)) {
            SS_POPNAME;
            return(TRUE);
        }

        if (su_pars_match_keyword(m, (char *)"NULL")) {
            *p_nullval = FALSE;
            SS_POPNAME;
            return(TRUE);
        }

        if (!su_pars_match_const(m, (char *)"-")) {
            negat = TRUE;
        }

        if (su_pars_get_numeric(m, numbuf, sizeof(numbuf))) {
            if (negat) {
                SsSprintf(buf, "-%s", numbuf);
            } else {
                SsSprintf(buf, "%s", numbuf);
            }
            ss_dprintf_3(("Value final:%s\n", buf));
            SS_POPNAME;
            return(TRUE);
        }
        ss_error;
        SS_POPNAME;
        return(FALSE);
}

/*#***********************************************************************\
 *
 *              sql_mini_prepare_insert
 *
 * Parse insert from SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * sqltrans_t*       trans, in, use, transaction
 * char*             sqlstr, in, use, SQL-string
 * rs_err_t**        p_errh, in ,use, error structure
 * su_pars_match_t*  m, in, use, parsing info
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */
static bool sql_mini_prepare_insert(
        void* cd,
        sqltrans_t* trans,
        char* sqlstr,
        rs_err_t** p_errh __attribute__ ((unused)),
        su_pars_match_t* m)
{
        TliConnectT* tcon;
        TliCursorT* tcur;
        TliRetT trc = 0;

        int  ano;
        char* val;
        char buf[128];
        char authid[64];
        char relname[64];
        char* colname;
        char** cols = NULL;
        bool succp = TRUE;
        rs_ttype_t* ttype;
        bool done = FALSE;
        int n,i;
        uint                attr_c = 0;
        rs_relh_t*          relh;
        rs_entname_t        name;
        bool                nullval;
        char*               catalog;

#ifndef SS_MYSQL
        bool finishedp = FALSE;
        char**              attrs = NULL;
        tb_sqlunique_t*     primkey = NULL;
        uint                unique_c = 0;
        tb_sqlunique_t*     unique = NULL;
        uint                forkey_c = 0;
        tb_sqlforkey_t*     forkeys = NULL;
        uint*               def = NULL;
        rs_tval_t*          defvalue = NULL;
        uint                check_c = 0;
        char**              checks = NULL;
        char**              checknames = NULL;
        tb_dd_createrel_t   createtype = TB_DD_CREATEREL_USER;
        tb_dd_persistency_t persistencytype = TB_DD_PERSISTENCY_PERMANENT;
        tb_dd_store_t       storetype = TB_DD_STORE_DISK;
        tb_dd_durability_t  durability = TB_DD_DURABILITY_DEFAULT;
        char*               extrainfo = NULL;
#endif /* ! SS_MYSQL */

        SS_PUSHNAME("sql_mini_prepare_insert");
        catalog = rs_sdefs_getcurrentdefcatalog();

        strcpy(authid, RS_AVAL_SYSNAME);
        if (!su_pars_get_tablename(m, authid, sizeof(authid), relname, sizeof(relname))) {
            ss_error;
        }
        strcpy(authid, RS_AVAL_SYSNAME);

        rs_entname_initbuf(&name,
                           catalog,
                           authid,
                           relname);

        relh = tb_dd_getrelh(cd, trans, &name, NULL, NULL);
        ss_bassert(relh != NULL);
        ttype = rs_relh_ttype(cd, relh);

        if (su_pars_match_const(m, (char *)"(")) {
            cols = SsMemAlloc(30*sizeof(char*));
            n = 0;
            done = FALSE;
            while (!done) {
                if (!su_pars_get_id(m, buf, sizeof(buf))) {
                    ss_error;
                }
                cols[n] = (char *)SsMemStrdup(buf);
                n++;
                if (!su_pars_match_const(m, (char *)",")) {
                    done = TRUE;
                }
            }
            attr_c = n;
            if (!su_pars_match_const(m, (char *)")")) {
                ss_error;
            }
        }

        if (!su_pars_match_keyword(m, (char *)"VALUES")) {
            ss_error;
        }
        if (!su_pars_match_const(m, (char *)"(")) {
            ss_error;
        }
        ss_pprintf_1(("[%s]\n", sqlstr));

        tcon = TliConnectInit(cd);
        trans = TliGetTrans(tcon);

        tcur = TliCursorCreate(tcon,
                               RS_AVAL_DEFCATALOG,
                               (char *)RS_AVAL_SYSNAME,
                               relname);
        ss_assert(tcur != NULL);

        i = 0;
        ano = 0;
        done = FALSE;
        while (!done) {

            nullval = FALSE;
            succp = sql_mini_prepare_value(m, buf, sizeof(buf), &nullval);
            ss_assert(succp);

            if (cols == NULL) {
                colname = rs_ttype_sql_aname(cd, ttype, ano);
            } else {
                colname = cols[ano];
            }
            ss_pprintf_2(("%s = %s\n", colname, buf));

            val = buf;
            if (nullval) {
                TliCursorColSetNULL(tcur, colname);
            } else {
                TliCursorColClearNULL(tcur, colname);
                trc = TliCursorColStr(tcur, colname, &val);
            }
            ss_rc_assert(trc == TLI_RC_SUCC, TliCursorErrorCode(tcur));
            i++;
            trc = TliCursorInsert(tcur);
            ss_dassert(trc == TLI_RC_SUCC || TliTransIsFailed(tcon));
            if (!su_pars_match_const(m, (char *)",")) {
                done = TRUE;
            }
            ano++;
        }

        if (!su_pars_match_const(m, (char *)")")) {
            ss_error;
        }

        TliCursorFree(tcur);
        TliConnectDone(tcon);

        if (cols != NULL) {
            for (i=0;i< (int)attr_c;i++) {
                SsMemFree(cols[i]);
            }
            SsMemFree(cols);
        }

        SS_MEM_SETUNLINK(relh);
        rs_relh_done(cd, relh);

        SS_POPNAME;
        return(succp);
}

/*#***********************************************************************\
 *
 *              tb_minisql_execdirect
 *
 * Execute parse functions based on SQL-string
 *
 * Parameters :
 *
 * rs_sysi_t*        cd, in, use, system information
 * sqltrans_t*       trans, in, use, transaction
 * rs_relh_t*        relh, in, use, relation
 * char*             sqlstr, in, use, SQL-string
 * rs_err_t**        p_errh, in ,use, error structure
 *
 * Return value : TRUE if successfull, FALSE otherwise
 *
 * Globals used : -
 */
bool tb_minisql_execdirect(
        void* cd,
        sqltrans_t* sqltrans,
        rs_relh_t* relh,
        char* sqlstr,
        rs_err_t** p_errh)
{
        bool succp = FALSE;
        bool done = FALSE;
        bool finishedp = FALSE;
        su_pars_match_t m;
        bool unique = FALSE;

        SS_PUSHNAME("tb_minisql_execdirect");
        su_pars_match_init(&m, sqlstr);

        while (!done) {

            ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

            if (su_pars_match_keyword(&m, (char *)"CREATE")) {
                if (su_pars_match_keyword(&m, (char *)"TABLE")) {
                    succp = sql_mini_prepare_createtable(
                                cd,
                                sqltrans,
                                sqlstr,
                                p_errh,
                                &m);
                }
                if (su_pars_match_keyword(&m, (char *)"UNIQUE")) {
                    unique = TRUE;
                }
                if (su_pars_match_keyword(&m, (char *)"INDEX")) {
                    succp = sql_mini_prepare_createindex(
                                cd,
                                sqltrans,
                                relh,
                                sqlstr,
                                unique,
                                p_errh,
                                &m);
                } else {
                    SS_POPNAME;
                    return(succp);
                }
            } else if (su_pars_match_keyword(&m, (char *)"ALTER")) {
                if (su_pars_match_keyword(&m, (char *)"TABLE")) {
                    succp = sql_mini_prepare_altertable(
                                cd,
                                sqltrans,
                                sqlstr,
                                p_errh,
                                &m);
                } else {
                    SS_POPNAME;
                    return(succp);
                }
            } else if (su_pars_match_keyword(&m, (char *)"INSERT")) {
                if (!su_pars_match_keyword(&m, (char *)"INTO")) {
                    ss_error;
                }
                succp = sql_mini_prepare_insert(
                            cd,
                            sqltrans,
                            sqlstr,
                            p_errh,
                            &m);
            } else if (su_pars_match_keyword(&m, (char *)"COMMIT")) {
                if (!su_pars_match_keyword(&m, (char *)"WORK")) {
                    ss_error;
                }
                finishedp = FALSE;
                succp = TRUE;
                while (succp && !finishedp) {
                    succp = tb_trans_commit(cd, sqltrans, &finishedp, p_errh);
                }
                tb_trans_beginif(cd, sqltrans);
            }
            if (!su_pars_match_const(&m, (char *)"@")) {
                done = TRUE;
            }
        }
        SS_POPNAME;
        return(succp);
}

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)

/*#***********************************************************************\
 *
 *              tb_minisql_column_done
 *
 * Free column data
 *
 * Parameters :
 *
 *   void*  data, in, use, column data
 *
 * Return value : -
 *
 * Globals used : -
 */
static void tb_minisql_column_done(
        void *data)
{
        SS_PUSHNAME("tb_minisql_column_done");

        ss_dassert(data != NULL);
        SsMemFree(data);

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              tb_minisql_prepare_columns
 *
 * Parse column names from the SQL-string
 *
 * Parameters :
 *
 *   su_pars_match_t*  m, in, use, SQL-string
 *   su_list_t**       col_list, in out, list of column names
 *
 * Return value : TRUE if successfull, FALSE if error
 *
 * Globals used : -
 */
static bool tb_minisql_prepare_columns(
        su_pars_match_t* m,
        su_list_t **col_list)
{
        bool done = FALSE;
        char buf[128];

        SS_PUSHNAME("tb_minisql_prepare_columns");

        ss_dassert(m != NULL);
        ss_dassert(col_list != NULL);

        if (!su_pars_match_const(m, (char *)"(")) {
            SS_POPNAME;
            return (FALSE);
        }

        *col_list = su_list_init(tb_minisql_column_done);

        while (!done) {
            if (!su_pars_get_id(m, buf, sizeof(buf))) {
                SS_POPNAME;
                return (FALSE);
            }

            /* Add column name to column list */
            su_list_insertlast(*col_list, (char *)SsMemStrdup(buf));

            if (!su_pars_match_const(m, (char *)",")) {
                done = TRUE;
            }
        }

        if (!su_pars_match_const(m, (char *)")")) {
            SS_POPNAME;
            return (FALSE);
        }

        SS_POPNAME;
        return (TRUE);
}

/*#***********************************************************************\
 *
 *              tb_minisql_skip
 *
 * Skip to next item
 *
 * Parameters :
 *
 *   su_pars_match_t*  m, in, use, SQL-string
 *   
 *
 * Return value : -
 *
 * Globals used : -
 */
static void tb_minisql_skip(
        su_pars_match_t* m)
{
        uint sulku = 0;
        uint quote = 0;

        SS_PUSHNAME("tb_minisql_skip");

        ss_dassert(m != NULL);

        while(*(m->m_pos) != '\0') {

            if (*(m->m_pos) == '(') {
                sulku++;
            }

            if (*(m->m_pos) == ')') {
                sulku --;
            }

            if (!sulku && *(m->m_pos) == ',') {
                SS_POPNAME;
                return;
            }

            m->m_pos++;
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              tb_minisql_prepare_refact
 *
 * Parse referential action from SQL-string
 *
 * Parameters :
 *
 *   su_pars_match_t*  m, in, use, SQL-string
 *   
 * Return value : sqlrefact_t - referential action
 *
 * Globals used : -
 */
static sqlrefact_t tb_minisql_prepare_refact(
        su_pars_match_t* m)
{
        SS_PUSHNAME("tb_minisql_prepare_refact");

        ss_dassert(m != NULL);

        if (su_pars_match_keyword(m, (char *) "RESTRICT")) {
            SS_POPNAME;
            return (SQL_REFACT_RESTRICT);
        } else if (su_pars_match_keyword(m, (char *) "CASCADE")) {
            SS_POPNAME;
            return (SQL_REFACT_CASCADE);
        } else if (su_pars_match_keyword(m, (char *) "SET")) {
            if (su_pars_match_keyword(m, (char *) "NULL")) {
                SS_POPNAME;
                return (SQL_REFACT_SETNULL);
            } else if (su_pars_match_keyword(m, (char *) "DEFAULT")) {
                SS_POPNAME;
                return (SQL_REFACT_SETDEFAULT);
            }
        } else if (su_pars_match_keyword(m, (char *) "NO")) {
            if (su_pars_match_keyword(m, (char *)"ACTION")) {
                SS_POPNAME;
                return (SQL_REFACT_NOACTION);
            }
        }

        SS_POPNAME;
        return (SQL_REFACT_RESTRICT); /* Default used */
}

/*#***********************************************************************\
 *
 *              tb_minisql_prepare_references
 *
 * Parse SQL-string after RERERENCES clause
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   rs_ttype_t*       ttype, in, use, table type
 *   su_pars_match_t*  m, in, use, SQL-string
 *   su_list_t*        col_list, in, out, column name list
 *   su_list_t*        fkey_list, in, out, foreign key list
 *   const char*       table_name, in, use, table name
 *   case_convertion_func_t case_converiton_func, in, use, function pointer
 *   case_convertion_func_t field_name_convertion_func, in, use, function pointer
 *   rs_err_t**        p_errh, in, out, error structure
 *   
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
static bool tb_minisql_prepare_references(
        rs_sysi_t*  cd,
        rs_ttype_t* ttype,
        su_pars_match_t* m,
        tb_sqlforkey_t* new_fkey,
        su_list_t* col_list,
        su_list_t* fkey_list,
        const char* table_name,
        case_convertion_func_t case_convertion_func,
        case_convertion_func_t field_name_convertion_func,
        rs_err_t** p_errh)
{
        uint n_cols = 0;
        uint n_ref_cols = 0;
        uint n_col = 0;
        su_list_node_t* col_list_node = NULL;
        su_list_t* ref_col_list = NULL;
        char ref_table_name[128];
        char authid[128];

        SS_PUSHNAME("tb_minisql_prepare_references");

        if(!(su_pars_get_tablename(m, authid, sizeof(authid),
                                   ref_table_name, sizeof(ref_table_name)))) {

            su_list_insertlast(fkey_list, new_fkey);

            if (col_list) {
                su_list_done(col_list);
            }

            SS_POPNAME;
            return (FALSE);
        }

        new_fkey->reftable = (char *)SsMemStrdup(ref_table_name);

        if (authid[0] != '\0') {
            new_fkey->refschema = (char *)SsMemStrdup(authid);
        } else {
            new_fkey->refschema = NULL;
        }

        if (case_convertion_func) {
            case_convertion_func(new_fkey->reftable);

            if (new_fkey->refschema) {
                case_convertion_func(new_fkey->refschema);
            }
        }

        new_fkey->refcatalog = NULL;

        tb_minisql_prepare_columns(m, &ref_col_list);

        new_fkey->delrefact = SQL_REFACT_RESTRICT;
        new_fkey->updrefact = SQL_REFACT_RESTRICT;

        n_cols = su_list_length(col_list);
        new_fkey->len = n_cols;
        new_fkey->fields = (uint*)SsMemAlloc(n_cols * sizeof(new_fkey->fields));

        su_list_do(col_list, col_list_node) {
            char *field_name;
            rs_ano_t col_ano;

            field_name = (char *)su_listnode_getdata(col_list_node);

            ss_dassert(field_name != NULL);

            if (field_name_convertion_func) {
                field_name_convertion_func(field_name);
            }

            col_ano = rs_ttype_anobyname(cd, ttype, field_name);

            /* Column not found */
            if (col_ano == RS_ANO_NULL) {
                su_list_insertlast(fkey_list, new_fkey);

                rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, field_name, table_name);

                su_list_done(col_list);

                if (ref_col_list) {
                    su_list_done(ref_col_list);
                }

                SS_POPNAME;
                return (FALSE);
            }

            new_fkey->fields[n_col] = col_ano;
            n_col++;
        }

        su_list_done(col_list);
        col_list = NULL;

        if( ref_col_list) {
            n_ref_cols = su_list_length(ref_col_list);
        } else {
            n_ref_cols = 0;
        }

        /* Check that there is not too many referenced fields */
        if (n_ref_cols > n_cols) {
            su_list_insertlast(fkey_list, new_fkey);

            rs_error_create(p_errh, E_CONSTRCHECKFAIL_S,
                            (char *)su_listnode_getdata(su_list_last(ref_col_list)));


            su_list_done(ref_col_list);

            SS_POPNAME;
            return (FALSE);
        }

        if (n_ref_cols > 0) {
            new_fkey->reffields = (char **)SsMemAlloc(sizeof(char *) * n_ref_cols);
        } else {
            new_fkey->reffields = NULL;
        }

        n_col = 0;

        if (n_ref_cols) {
            su_list_do(ref_col_list, col_list_node) {
                char *field_name;

                field_name = (char *)su_listnode_getdata(col_list_node);

                if (field_name_convertion_func) {
                    field_name_convertion_func(field_name);
                }

                new_fkey->reffields[n_col] = (char *)SsMemStrdup((char *)field_name);

                n_col++;
            }

            su_list_done(ref_col_list);
            ref_col_list = NULL;
        }

        /* MATCH options currently ignored:
         [MATCH FULL | MATCH PARTIAL | MATCH SIMPLE]*/

        if (su_pars_match_keyword(m, (char *)"MATCH")) {
            if (su_pars_match_keyword(m, (char *)"FULL")) {
            } else if (su_pars_match_keyword(m, (char *)"PARTIAL")) {
            } else if (su_pars_match_keyword(m, (char *)"SIMPLE")) {
            }
        }

        /* Referential actions */
        if (su_pars_match_keyword(m, (char *)"ON")) {
            if (su_pars_match_keyword(m, (char *)"DELETE")) {
                new_fkey->delrefact = tb_minisql_prepare_refact(m);
            } else if (su_pars_match_keyword(m, (char *)"UPDATE")) {
                new_fkey->updrefact = tb_minisql_prepare_refact(m);
            }
        }

        if (su_pars_match_keyword(m, (char *)"ON")) {
            if (su_pars_match_keyword(m, (char *)"DELETE")) {
                new_fkey->delrefact = tb_minisql_prepare_refact(m);
            } else if (su_pars_match_keyword(m, (char *)"UPDATE")) {
                new_fkey->updrefact = tb_minisql_prepare_refact(m);
            }
        }

        su_list_insertlast(fkey_list, new_fkey);

        SS_POPNAME;
        return (TRUE);
}

/*#***********************************************************************\
 *
 *              tb_minisql_prepare_drop_forkeys
 *
 * Parse names of dropped foreign keys from the SQL-string
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   char *            sqlstr, in, use, SQL-string
 *   su_list_t*        fkey_list, in, out, foreign key list
 *   rs_err_t**        p_errh, in, out, error structure
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
bool tb_minisql_prepare_drop_forkeys(
        rs_sysi_t* cd,
        char* sqlstr,
        su_list_t** fkey_list,
        rs_err_t** p_errh)
{
        char authid[128];
        char table_name[128];
        su_pars_match_t m;
        bool more = TRUE;
        bool succp = FALSE;

        SS_PUSHNAME("tb_minisql_prepare_drop_forkeys");

        ss_dassert(cd != NULL);
        ss_dassert(sqlstr != NULL);
        ss_dassert(fkey_list != NULL);

        su_pars_match_init(&m, sqlstr);

        ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

        if (!(su_pars_match_keyword(&m, (char *)"ALTER"))) {
            SS_POPNAME;
            return (FALSE);
        }

        if (!(su_pars_match_keyword(&m, (char *)"TABLE"))) {
            SS_POPNAME;
            return (FALSE);
        }

        if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
            SS_POPNAME;
            return (FALSE);
        }
        
        *fkey_list = su_list_init(tb_minisql_column_done);

        while(more) {
            if (su_pars_match_keyword(&m, (char *)"DROP")) {
                char buff[128];

                buff[0]='\0';
                
                if (su_pars_match_keyword(&m, (char *)"FOREIGN")) {
                    if (su_pars_match_keyword(&m, (char *)"KEY")) {
                        
                        /* MySQL key name */
                        
                        if (su_pars_get_id(&m, buff, sizeof(buff))) {
                            char *fkey_name = (char *)SsMemStrdup(buff);
                        
                            succp = TRUE;
                        
                            su_list_insertlast(*fkey_list, fkey_name);
                        }
                    } else {
                        tb_minisql_skip(&m);
                    }
                } else {
                    tb_minisql_skip(&m);
                }
            } else {
                tb_minisql_skip(&m);
            }
            
            if (m.m_pos == NULL || !su_pars_match_const(&m, (char *)",")) {
                more=FALSE;
            }
        }

        SS_POPNAME;
        return (succp);
}

/*#***********************************************************************\
 *
 *              tb_minisql_prepare_mysql_forkeys
 *
 * Parse foreign key definitions from alter table and create table SQL-strings
 * obtained from MySQL. We can mostly assume that SQL-string does not contain
 * parse errors.
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   rs_ttype_t*       ttype, in, use, table type
 *   char *            sqlstr, in, use, SQL-string
 *   case_convertion_func_t case_converiton_func, in, use, function pointer
 *   case_convertion_func_t field_name_convertion_func, in, use, function pointer
 *   su_list_t*        fkey_list, in, out, foreign key list
 *   rs_err_t**        p_errh, in, out, error structure
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
bool tb_minisql_prepare_mysql_forkeys(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        char* sqlstr,
        case_convertion_func_t case_convertion_func,
        case_convertion_func_t field_name_convertion_func,
        su_list_t** fkey_list,
        rs_err_t** p_errh)
{
        char authid[128];
        char table_name[128];
        char buff[128];
        su_pars_match_t m;
        tb_sqlforkey_t* new_fkey = NULL;
        bool more = TRUE;
        bool succp = TRUE;

        SS_PUSHNAME("tb_minisql_prepare_column_forkeys");

        ss_dassert(cd != NULL);
        ss_dassert(sqlstr != NULL);
        ss_dassert(fkey_list != NULL);

        su_pars_match_init(&m, sqlstr);

        ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

        if (su_pars_match_keyword(&m, (char *)"CREATE")) {
            if (su_pars_match_keyword(&m, (char *)"TABLE")) {

                if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                    SS_POPNAME;
                    return (FALSE);
                }

                if (!su_pars_match_const(&m, (char *)"(")) {
                    SS_POPNAME;
                    return (FALSE);
                }
            } else {
                SS_POPNAME;
                return (FALSE);
            }
        } else if (su_pars_match_keyword(&m, (char *)"ALTER")) {
            if (su_pars_match_keyword(&m, (char *)"TABLE")) {

                if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                    SS_POPNAME;
                    return (FALSE);
                }

                if (su_pars_match_keyword(&m, (char *)"ADD") ||
                    su_pars_match_keyword(&m, (char *)"MODIFY")) {

                    /* Column is not necessary skip it */
                    su_pars_match_keyword(&m, (char *)"COLUMN");

                } else {
                    SS_POPNAME;
                    return (FALSE);
                }
            } else {
                SS_POPNAME;
                return (FALSE);
            }
        } else {
            SS_POPNAME;
            return (FALSE);
        }

        if (*fkey_list == NULL) {
            *fkey_list = su_list_init(NULL);
        }
        
        while (more) {
            su_list_t* col_list = NULL;
            bool create_table_normal_constraint = FALSE;

            new_fkey = NULL;
                
            /* Normal foreign keys */
            if (su_pars_match_keyword(&m, (char *)"CONSTRAINT")) {
                su_pars_match_t save;

                /* If we have constraint then we can have constraint
                     name */

                save = m;

                if (!(su_pars_match_keyword(&m, (char *)"FOREIGN"))) {

                    if (su_pars_get_id(&m, buff, sizeof(buff))) { 
                        new_fkey = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t));
                        memset(new_fkey, 0, sizeof(tb_sqlforkey_t));
                        new_fkey->mysqlname = (char *)SsMemStrdup(buff);
                    } else {
                        m = save; /* rollback */
                    }

                    create_table_normal_constraint = TRUE;

                } else {
                    m = save; /* rollback */
                }
            }
                
            if (su_pars_match_keyword(&m, (char *)"FOREIGN")) {
                if (su_pars_match_keyword(&m, (char *)"KEY")) {
                    su_pars_match_t save;
                    
                    save = m;

                    /* In MySQL we could also have name for the foreign key index */
                    if (su_pars_get_id(&m, buff, sizeof(buff))) {
                        if (new_fkey) {
                            if (!new_fkey->mysqlname) {
                                new_fkey->mysqlname = (char *)SsMemStrdup(buff);
                            }
                        } else { 
                            new_fkey = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t));
                            memset(new_fkey, 0, sizeof(tb_sqlforkey_t));
                            new_fkey->mysqlname = (char *)SsMemStrdup(buff);
                        }
                    } else {
                        m = save; /* rollback */
                    }

                    create_table_normal_constraint = TRUE;

                } else {
                    tb_minisql_skip(&m);
                }
            }

            if (create_table_normal_constraint) {
                tb_minisql_prepare_columns(&m, &col_list);
                
                if (col_list && su_pars_match_keyword(&m, (char *)"REFERENCES")) {

                    if (new_fkey == NULL) {
                        new_fkey = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t));
                        ss_dassert(new_fkey != NULL);
                        memset(new_fkey, 0, sizeof(tb_sqlforkey_t));
                    }
                    
                    if (!(tb_minisql_prepare_references(cd, ttype, &m, new_fkey,
                                                        col_list, *fkey_list, table_name,
                                                        case_convertion_func, 
                                                        field_name_convertion_func,
                                                        p_errh))) {
                        SS_POPNAME;
                        return (FALSE);
                    }
                } else {
                    if (col_list) {
                        su_list_done(col_list);
                    }
                }
            } else {

                /* attribute name */
                if (su_pars_get_id(&m, buff, sizeof(buff))) {

                    if(su_pars_skipto_keyword(&m, (char *)"REFERENCES", (char *)",")) {
                        char* col_name = NULL;
                        su_list_t* col_list = NULL;

                        /* This should be foreign key definition. */
                        
                        new_fkey = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t));
                        ss_dassert(new_fkey != NULL);
                        memset(new_fkey, 0, sizeof(tb_sqlforkey_t));

                        col_list = su_list_init(tb_minisql_column_done);
                        col_name = (char *)SsMemStrdup(buff);
                        su_list_insertlast(col_list, col_name);
                        
                        if (!(tb_minisql_prepare_references(cd, ttype, &m, new_fkey,
                                                            col_list, *fkey_list, table_name,
                                                            case_convertion_func, 
                                                            field_name_convertion_func,
                                                            p_errh))) {

                            SS_POPNAME;
                            return (FALSE);
                        } else {
                            succp = TRUE;
                            tb_minisql_skip(&m);
                        }
                    } else {
                        tb_minisql_skip(&m);
                    }
                } else {
                    tb_minisql_skip(&m);
                }
            }
            
            if (!su_pars_match_const(&m, (char *)",")) {
                more = FALSE;
            }

            if (more) {
                if (su_pars_match_keyword(&m, (char *)"ADD") ||
                    su_pars_match_keyword(&m, (char *)"MODIFY")) {

                    /* Column is not necessary skip it */
                    su_pars_match_keyword(&m, (char *)"COLUMN");
                }
            }
        }

        SS_POPNAME;
        return (succp);
}

/*#***********************************************************************\
 *
 *              tb_minisql_is_drop_index
 *
 * Return TRUE if SQL-string contains DROP INDEX -clause
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   char *            sqlstr, in, use, SQL-string
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
bool tb_minisql_is_drop_index(
        rs_sysi_t* cd,
        const char* sqlstr)
{
        char table_name[128];
        char authid[128];
        su_pars_match_t m;
        bool more = TRUE;
        bool succp = TRUE;

        SS_PUSHNAME("tb_minisql_is_drop_index");

        ss_dassert(cd != NULL);
        ss_dassert(sqlstr != NULL);

        su_pars_match_init(&m, sqlstr);

        ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

        if (su_pars_match_keyword(&m, (char *)"ALTER")) {
            if (su_pars_match_keyword(&m, (char *)"TABLE")) {

                if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                    SS_POPNAME;
                    return (FALSE);
                }

                while(more) {
                    if(su_pars_skipto_keyword(&m, (char *)"DROP", (char *)",")) {
                        if (su_pars_match_keyword(&m, (char *)"INDEX")) {
                            SS_POPNAME;
                            return (TRUE);
                        } else {
                            tb_minisql_skip(&m);
                        }
                    } else {
                        tb_minisql_skip(&m);
                    }
            
                    if (!su_pars_match_const(&m, (char *)",")) {
                        more = FALSE;
                    }
                }
            }
        }

        SS_POPNAME;
        return (FALSE);
}

/*#***********************************************************************\
 *
 *              tb_minisql_get_table_name
 *
 * Return table name from SQL-string if found
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   char *            sqlstr, in, use, SQL-string
 *   case_convertion_func_t case_converiton_func, in, use, function pointer
 *   rs_entname_t*     alter_table_name, in out, table name in alter table 
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
bool tb_minisql_get_table_name(
        rs_sysi_t* cd,
        char* sqlstr,
        case_convertion_func_t case_convertion_func,
        rs_entname_t* alter_table_name)
{
        char authid[128];
        char table_name[128];
        su_pars_match_t m;
        bool succp = FALSE;

        SS_PUSHNAME("tb_minisql_get_table_name");

        ss_dassert(cd != NULL);
        ss_dassert(sqlstr != NULL);

        su_pars_match_init(&m, sqlstr);

        ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

        if (su_pars_match_keyword(&m, (char *)"ALTER")) {
            if (su_pars_match_keyword(&m, (char *)"TABLE")) {

                if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                    SS_POPNAME;
                    return (FALSE);
                }

                succp = TRUE;
            }
        } else if (su_pars_match_keyword(&m, (char *)"CREATE") ||
                   su_pars_match_keyword(&m, (char *)"DROP")) {
            /* Very special MySQL cases
               create index <name> on <table>(columns) or
               drop index <name> on <table> */

            su_pars_match_keyword(&m, (char *)"UNIQUE");
            
            if (su_pars_match_keyword(&m, (char *)"INDEX")) {

                /* This is really index name, we skip it */
                if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                    SS_POPNAME;
                    return (FALSE);
                }

                if (su_pars_match_keyword(&m, (char *)"ON")) {
                    if (!(su_pars_get_tablename(&m, authid, sizeof(authid), table_name, sizeof(table_name)))) {
                        SS_POPNAME;
                        return (FALSE);
                    }

                    succp = TRUE;
                }
            }
        }

        if (succp) {
            alter_table_name->en_schema = (char *)SsMemStrdup(authid);
            alter_table_name->en_name = (char *)SsMemStrdup(table_name);

            if (case_convertion_func) {
                case_convertion_func(alter_table_name->en_name);
                case_convertion_func(alter_table_name->en_schema);
            }
        }
        
        SS_POPNAME;
        return (succp);
}

/*#***********************************************************************\
 *
 *              tb_minisql_is_drop_table_cascade
 *
 * Return TRUE if SQL-string contains DROP TABLE cascade -clause
 *
 * Parameters :
 *
 *   rs_sysi_t*        cd, in, use, system information
 *   char *            sqlstr, in, use, SQL-string
 *
 * Return value : TRUE or FALSE
 *
 * Globals used : -
 */
bool tb_minisql_is_drop_table_cascade(
        rs_sysi_t* cd,
        const char* sqlstr)
{
        su_pars_match_t m;

        SS_PUSHNAME("tb_minisql_is_drop_table_cascade");

        ss_dassert(cd != NULL);
        ss_dassert(sqlstr != NULL);

        su_pars_match_init(&m, sqlstr);

        ss_pprintf_1(("sql_mini_prepare:[%.256s]\n", sqlstr));

        if (su_pars_match_keyword(&m, (char *)"DROP")) {
            if (su_pars_match_keyword(&m, (char *)"TABLE")) {

                if(su_pars_skipto_keyword(&m, (char *)"CASCADE", (char *)";")) {
                    SS_POPNAME;
                    return (TRUE);
                }
            }
        }

        SS_POPNAME;
        return (FALSE);
}

#endif /* SS_MYSQL || SS_MYSQL_AC */
