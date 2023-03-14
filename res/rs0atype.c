/*************************************************************************\
**  source       * rs0atype.c
**  directory    * res
**  description  * Attribute type functions
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
This file implements the attribute type functions. The attribute value types
supported here correspond to the types available through SQL.
New user defined types can be added by changing the function rs_atype_create.

Each attribute type (atype) can be categorized by three main properties:

1) Attribute type
   Currently one of

        RSAT_USER_DEFINED
        RSAT_TUPLE_ID
        RSAT_TUPLE_VERSION
        RSAT_TRX_ID
        RSAT_FREELY_DEFINED
        RSAT_CLUSTER_ID
        RSAT_RELATION_ID
        RSAT_KEY_ID
        RSAT_REMOVED            Removed by ALTER TABLE

   Only RSAT_USER_DEFINED attributes can be used through SQL. All other
   are internal attribute types that are filtered out when SQL is used.

2) SQL data type
   Currently one of

        RSSQLDT_TINYINT
        RSSQLDT_CHAR
        RSSQLDT_NUMERIC
        RSSQLDT_DECIMAL
        RSSQLDT_INTEGER
        RSSQLDT_SMALLINT
        RSSQLDT_FLOAT
        RSSQLDT_REAL
        RSSQLDT_DOUBLE
        RSSQLDT_DATE
        RSSQLDT_VARCHAR
        RSSQLDT_LONGVARCHAR
        RSSQLDT_BINARY
        RSSQLDT_VARBINARY
        RSSQLDT_LONGVARBINARY
        RSSQLDT_DATE
        RSSQLDT_TIME
        RSSQLDT_TIMESTAMP

   when UNICODE data types are enabled also:
        RSSQLDT_WCHAR
        RSSQLDT_WVARCHAR
        RSSQLDT_WLONGVARCHAR

   21.1.2002 added
        RSSQLDT_BIGINT

3) Internal data type
   Currently one of

        RSDT_CHAR
        RSDT_INTEGER
        RSDT_FLOAT
        RSDT_DOUBLE
        RSDT_DATE
        RSDT_DFLOAT
   when UNICODE data types are enabled also:
        RSDT_UNICODE
        RSDT_BINARY

   21.1.2002 added
        RSDT_BIGINT

Limitations:
-----------

Error handling:
--------------
Sometimes creates rs_err_t objects.
Internal errors cause dassertions to fail.

Objects used:
------------
sscanf() replacements <ssscan.h>
dfloats               <dt0dfloa.h>

Preconditions:
-------------
None


Multithread considerations:
--------------------------
Code is fully re-entrant.
The same atype object can not be used simultaneously from many threads.


Example:
-------

tatype.c


**************************************************************************
#endif /* DOCUMENTATION */

#define RS_INTERNAL
#define RS0ATYPE_C

#include <sssprint.h>
#include <ssc.h>
#include <ssstring.h>
#include <ssscan.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sschcvt.h>
#include <ssutf.h>

#include <su0error.h>
#include <su0bflag.h>
#include <su0bsrch.h>
#include <dt0dfloa.h>

#include "rs0sdefs.h"
#include "rs0types.h"
#include "rs0sysi.h"
#include "rs0error.h"
#include "rs0tnum.h"
#include "rs0atype.h"
#include "rs0aval.h"

const ss_char_t RS_TN_BIGINT[] =            "BIGINT";
const ss_char_t RS_TN_BINARY[] =            "BINARY";
const ss_char_t RS_TN_BIT[] =               "BIT";
const ss_char_t RS_TN_CHAR[] =              "CHAR";
const ss_char_t RS_TN_CHARACTER[] =         "CHARACTER";
const ss_char_t RS_TN_CHARACTER_VARYING[] = "CHARACTER VARYING";
const ss_char_t RS_TN_CHAR_VARYING[] =      "CHAR VARYING";
const ss_char_t RS_TN_DATE[] =              "DATE";
const ss_char_t RS_TN_DEC[] =               "DEC";
const ss_char_t RS_TN_DECIMAL[] =           "DECIMAL";
const ss_char_t RS_TN_DOUBLE_PRECISION[] =  "DOUBLE PRECISION";
const ss_char_t RS_TN_FLOAT[] =             "FLOAT";
const ss_char_t RS_TN_INT[] =               "INT";
const ss_char_t RS_TN_INTEGER[] =           "INTEGER";
const ss_char_t RS_TN_LONG_VARBINARY[] =    "LONG VARBINARY";
const ss_char_t RS_TN_LONG_VARCHAR[] =      "LONG VARCHAR";
const ss_char_t RS_TN_NULL[] =              "NULL";
const ss_char_t RS_TN_NUMERIC[] =           "NUMERIC";
const ss_char_t RS_TN_REAL[] =              "REAL";
const ss_char_t RS_TN_SMALLINT[] =          "SMALLINT";
const ss_char_t RS_TN_TIME[] =              "TIME";
const ss_char_t RS_TN_TIMESTAMP[] =         "TIMESTAMP";
const ss_char_t RS_TN_TINYINT[] =           "TINYINT";
const ss_char_t RS_TN_VARBINARY[] =         "VARBINARY";
const ss_char_t RS_TN_VARCHAR[] =           "VARCHAR";
const ss_char_t RS_TN_WCHAR[] =             "WCHAR";
const ss_char_t RS_TN_WVARCHAR[] =          "WVARCHAR";
const ss_char_t RS_TN_LONG_WVARCHAR[] =     "LONG WVARCHAR";

/* the below are synonyms for WVARCHAR */
const ss_char_t RS_TN_VARWCHAR[] =          "VARWCHAR";
const ss_char_t RS_TN_NATIONAL_CHARACTER_VARYING[] = "NATIONAL CHARACTER VARYING";
const ss_char_t RS_TN_NATIONAL_CHAR_VARYING[]= "NATIONAL CHAR VARYING";
const ss_char_t RS_TN_NATIONAL_VARCHAR[] = "NATIONAL VARCHAR";
const ss_char_t RS_TN_NCHAR_VARYING[] = "NCHAR VARYING";
const ss_char_t RS_TN_NVARCHAR[] = "NVARCHAR";

/* synonyms for LONG WVARCHAR */
const ss_char_t RS_TN_LONG_VARWCHAR[] =     "LONG VARWCHAR";
const ss_char_t RS_TN_LONG_NVARCHAR[] =     "LONG NVARCHAR";
const ss_char_t RS_TN_LONG_NATIONAL_VARCHAR[] =     "LONG NATIONAL VARCHAR";
const ss_char_t RS_TN_NCLOB[] = "NCLOB";
const ss_char_t RS_TN_NATIONAL_CHARACTER_LARGE_OBJECT[] = "NATIONAL CHARACTER LARGE OBJECT";
const ss_char_t RS_TN_NATIONAL_CHAR_LARGE_OBJECT[] = "NATIONAL CHAR LARGE OBJECT";
const ss_char_t RS_TN_NCHAR_LARGE_OBJECT[] = "NCHAR LARGE OBJECT";

/* synonyms for WCHAR */
const ss_char_t RS_TN_NATIONAL_CHARACTER[] = "NATIONAL CHARACTER";
const ss_char_t RS_TN_NATIONAL_CHAR[] = "NATIONAL CHAR";
const ss_char_t RS_TN_NCHAR[] = "NCHAR";

/* synonyms for LONG VARCHAR */
const ss_char_t RS_TN_CLOB[] = "CLOB";
const ss_char_t RS_TN_CHARACTER_LARGE_OBJECT[] = "CHARACTER LARGE OBJECT";
const ss_char_t RS_TN_CHAR_LARGE_OBJECT[] = "CHAR LARGE OBJECT";

/* synonyms for LONG VARBINARY */
const ss_char_t RS_TN_BLOB[] = "BLOB";
const ss_char_t RS_TN_BINARY_LARGE_OBJECT[] =  "BINARY LARGE OBJECT";

/* An array for sql names of sql types.

   DANGER! We assume that rs_atype_types is an array that can be
           accessed directly (or indirectly when extended < -80 data
           types are present) by RSSQLDT_XXX as an index.
           Therefore we have to "know" that RSSQLDT_VARCHAR == 12.
           This is dasserted in rs_atype_name.
*/

/* this optimization makes dfloat converted value flat;
 * no need to clear conversion at copy any more
 */
#define DFL_COPYCONVERT FALSE

const rs_atypeinfo_t rs_atype_types[] = {

/* SQL type                  | SQL name             | RS type      | Default len      | Def. scale        | Copyconvert */
{  RSSQLDT_WLONGVARCHAR       , RS_TN_LONG_WVARCHAR  , RSDT_UNICODE , RS_LENGTH_NULL   , 0,                  TRUE },
{  RSSQLDT_WVARCHAR           , RS_TN_WVARCHAR       , RSDT_UNICODE , RS_VARCHAR_DEFLEN, 0,                  TRUE },
{  RSSQLDT_WCHAR              , RS_TN_WCHAR          , RSDT_UNICODE , 1                , 0,                  TRUE },
{  RSSQLDT_BIT                , RS_TN_BIT            , -1            , 0               , 0,                 TRUE },
{  RSSQLDT_TINYINT            , RS_TN_TINYINT        , RSDT_INTEGER , RS_TINYINT_PREC  , RS_TINYINT_SCALE,  FALSE },
{  RSSQLDT_BIGINT             , RS_TN_BIGINT         , RSDT_BIGINT  , RS_BIGINT_PREC   , RS_BIGINT_SCALE,    FALSE },
{  RSSQLDT_LONGVARBINARY      , RS_TN_LONG_VARBINARY , RSDT_BINARY  , RS_LENGTH_NULL   , 0,                  TRUE },
{  RSSQLDT_VARBINARY          , RS_TN_VARBINARY      , RSDT_BINARY  , RS_VARCHAR_DEFLEN, 0,                  TRUE },
{  RSSQLDT_BINARY             , RS_TN_BINARY         , RSDT_BINARY  , 1                , 0,                  TRUE },
{  RSSQLDT_LONGVARCHAR        , RS_TN_LONG_VARCHAR   , RSDT_CHAR    , RS_LENGTH_NULL   , 0,                  TRUE },
{  0                          , RS_TN_NULL           , -1           , 0                , 0,                  TRUE },
{  RSSQLDT_CHAR               , RS_TN_CHAR           , RSDT_CHAR    , 1                , 0,                  TRUE },
{  RSSQLDT_NUMERIC            , RS_TN_NUMERIC        , RSDT_DFLOAT  , RS_DFLOAT_MAXPREC, RS_DFLOAT_DEFSCALE, DFL_COPYCONVERT },
{  RSSQLDT_DECIMAL            , RS_TN_DECIMAL        , RSDT_DFLOAT  , RS_DFLOAT_MAXPREC, RS_DFLOAT_DEFSCALE, DFL_COPYCONVERT },
{  RSSQLDT_INTEGER            , RS_TN_INTEGER        , RSDT_INTEGER , RS_INT_PREC      , RS_INT_SCALE,       FALSE },
{  RSSQLDT_SMALLINT           , RS_TN_SMALLINT       , RSDT_INTEGER , RS_SMALLINT_PREC , RS_SMALLINT_SCALE,  FALSE },
{  RSSQLDT_FLOAT              , RS_TN_FLOAT          , RSDT_DOUBLE  , RS_DOUBLE_PREC   , RS_DOUBLE_SCALE,    FALSE },
{  RSSQLDT_REAL               , RS_TN_REAL           , RSDT_FLOAT   , RS_REAL_PREC     , RS_REAL_SCALE,      FALSE },
{  RSSQLDT_DOUBLE             , RS_TN_DOUBLE_PRECISION,RSDT_DOUBLE  , RS_DOUBLE_PREC   , RS_DOUBLE_SCALE,    FALSE },
{  RSSQLDT_DATE               , RS_TN_DATE           , RSDT_DATE    , RS_DATE_PREC     , RS_DATE_SCALE,      TRUE },
{  RSSQLDT_TIME               , RS_TN_TIME           , RSDT_DATE    , RS_DATE_PREC     , RS_DATE_SCALE,      TRUE },
{  RSSQLDT_TIMESTAMP          , RS_TN_TIMESTAMP      , RSDT_DATE    , RS_DATE_PREC     , RS_DATE_SCALE,      TRUE },
{  RSSQLDT_VARCHAR            , RS_TN_VARCHAR        , RSDT_CHAR    , RS_VARCHAR_DEFLEN,  0,                 TRUE }
};


typedef struct {
        const ss_char_t* tbn_name;
        uint  tbn_sqldt;
} atype_typebyname_t;

static const atype_typebyname_t atype_typebyname[] = {
{ RS_TN_BIGINT                          , RSSQLDT_BIGINT },
{ RS_TN_BINARY                          , RSSQLDT_BINARY },
{ RS_TN_BINARY_LARGE_OBJECT             , RSSQLDT_LONGVARBINARY },
{ RS_TN_BLOB                            , RSSQLDT_LONGVARBINARY },
{ RS_TN_CHAR                            , RSSQLDT_CHAR },
{ RS_TN_CHAR_LARGE_OBJECT               , RSSQLDT_LONGVARCHAR },
{ RS_TN_CHAR_VARYING                    , RSSQLDT_VARCHAR },
{ RS_TN_CHARACTER                       , RSSQLDT_CHAR },
{ RS_TN_CHARACTER_LARGE_OBJECT          , RSSQLDT_LONGVARCHAR },
{ RS_TN_CHARACTER_VARYING               , RSSQLDT_VARCHAR },
{ RS_TN_CLOB                            , RSSQLDT_LONGVARCHAR },
{ RS_TN_DATE                            , RSSQLDT_DATE },
{ RS_TN_DEC                             , RSSQLDT_DECIMAL },
{ RS_TN_DECIMAL                         , RSSQLDT_DECIMAL },
{ RS_TN_DOUBLE_PRECISION                , RSSQLDT_DOUBLE },
{ RS_TN_FLOAT                           , RSSQLDT_FLOAT },
{ RS_TN_INT                             , RSSQLDT_INTEGER },
{ RS_TN_INTEGER                         , RSSQLDT_INTEGER },
{ RS_TN_LONG_NATIONAL_VARCHAR           , RSSQLDT_WLONGVARCHAR },
{ RS_TN_LONG_NVARCHAR                   , RSSQLDT_WLONGVARCHAR },
{ RS_TN_LONG_VARBINARY                  , RSSQLDT_LONGVARBINARY },
{ RS_TN_LONG_VARCHAR                    , RSSQLDT_LONGVARCHAR },
{ RS_TN_LONG_VARWCHAR                   , RSSQLDT_WLONGVARCHAR },
{ RS_TN_LONG_WVARCHAR                   , RSSQLDT_WLONGVARCHAR },
{ RS_TN_NATIONAL_CHAR                   , RSSQLDT_WCHAR },
{ RS_TN_NATIONAL_CHAR_LARGE_OBJECT      , RSSQLDT_WLONGVARCHAR },
{ RS_TN_NATIONAL_CHAR_VARYING           , RSSQLDT_WVARCHAR },
{ RS_TN_NATIONAL_CHARACTER              , RSSQLDT_WCHAR },
{ RS_TN_NATIONAL_CHARACTER_LARGE_OBJECT , RSSQLDT_WLONGVARCHAR },
{ RS_TN_NATIONAL_CHARACTER_VARYING      , RSSQLDT_WVARCHAR },
{ RS_TN_NATIONAL_VARCHAR                , RSSQLDT_WVARCHAR },
{ RS_TN_NCHAR                           , RSSQLDT_WCHAR },
{ RS_TN_NCHAR_LARGE_OBJECT              , RSSQLDT_WLONGVARCHAR },
{ RS_TN_NCHAR_VARYING                   , RSSQLDT_WVARCHAR },
{ RS_TN_NCLOB                           , RSSQLDT_WLONGVARCHAR },
{ RS_TN_NUMERIC                         , RSSQLDT_NUMERIC },
{ RS_TN_NVARCHAR                        , RSSQLDT_WVARCHAR },
{ RS_TN_REAL             , RSSQLDT_REAL },
{ RS_TN_SMALLINT         , RSSQLDT_SMALLINT },
{ RS_TN_TIME             , RSSQLDT_TIME },
{ RS_TN_TIMESTAMP        , RSSQLDT_TIMESTAMP },
{ RS_TN_TINYINT          , RSSQLDT_TINYINT },
{ RS_TN_VARBINARY        , RSSQLDT_VARBINARY },
{ RS_TN_VARCHAR          , RSSQLDT_VARCHAR },
{ RS_TN_VARWCHAR         , RSSQLDT_WVARCHAR },
{ RS_TN_WCHAR            , RSSQLDT_WCHAR },
{ RS_TN_WVARCHAR         , RSSQLDT_WVARCHAR },
};

static int atype_getsqltypebyname(char* typename);


/*##**********************************************************************\
 *
 *              rs_atype_create
 *
 * Member of the SQL function block.
 * Allocates and initializes a new rs_atype_t object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      typename - in, use
 *              SQL main type name
 *
 *      pars - in, use
 *              if non-NULL, contains the parenthesized
 *          extra information associated with the
 *          type (e.g. "6,2" in NUMERIC(6,2))
 *
 *      nullallowed - in, use
 *              TRUE, if the 'NULL' value is allowed with the type
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 * Return value - give:
 *
 *      pointer into the new field type object. NULL is returned
 *      in case of an error
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_create(cd, typename, pars, nullallowed, p_errh)
    void*       cd;
    char*       typename;
    char*       pars;
    bool        nullallowed;
    rs_err_t**  p_errh;
{


        int  sqltype;
        ulong scale;
        ulong len;

        ss_dprintf_3(("%s: rs_atype_create, typename %s, pars %s\n",
                      __FILE__, typename, pars == NULL ? "NULL" : pars));
        sqltype = atype_getsqltypebyname(typename);

        scale   = ATYPE_TYPES(sqltype).st_defaultscale;
        len     = ATYPE_TYPES(sqltype).st_defaultlen;

        switch (sqltype) {

            case 0: /* Not known */
                rs_error_create(p_errh, E_ILLTYPE_S, typename);
                return(NULL);

            case RSSQLDT_NUMERIC:   /* Len and scale allowed */
            case RSSQLDT_DECIMAL:   /* If not given, use defaults */
            case RSSQLDT_REAL:
            case RSSQLDT_DOUBLE:
            case RSSQLDT_FLOAT:

                if (pars) {
                    bool b;
                    char* tmp_pars = pars;

                    b = SsStrScanLong(pars, (long*)&len, &tmp_pars);
                    if (b) {
                        while (ss_isspace(*tmp_pars)) {
                            tmp_pars++;
                        }
                        if (*tmp_pars++ == ',') {
                            b = SsStrScanLong(tmp_pars, (long*)&scale, &tmp_pars);
                            if (!b) {
                                rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                                return(NULL);
                            }
                        } else {
                            scale = 0L;
                        }
                    } else {
                        rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                        return(NULL);
                    }
                }
                break;

            case RSSQLDT_CHAR:
            case RSSQLDT_VARCHAR:   /* Len allowed, possible scale ignored */
            case RSSQLDT_BINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_WCHAR:
            case RSSQLDT_WVARCHAR:
                if (pars) {
                    bool b;
                    char* tmp_pars = pars;

                    b = SsStrScanLong(pars, (long*)&len, &tmp_pars);
                    if (!b) {
                        rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                        return(NULL);
                    } else {
                        while (ss_isspace(*tmp_pars)) {
                            tmp_pars++;
                        }
                        if (*tmp_pars++ != '\0') {
                            rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                            return(NULL);
                        }
                    }

                    if (len == 0L) {
                        len = ATYPE_TYPES(sqltype).st_defaultlen;
                    }
                    /*
                    if (len < 0L) {
                        rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                        return(NULL);
                    }
                    */
                    if (len > SS_INT4_MAX) {   /* Length should not be larger that 2GB - 1 */
                        rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                        return(NULL);
                    }
                }
                break;

            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:                             /* Len and scale not allowed */
                if (pars) {
                    rs_error_create(p_errh, E_ILLTYPEPARAM_SS, pars, typename);
                    return(NULL);
                }
                break;

            default:
                break;
        }

        return(
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(sqltype).st_rstype,
                sqltype,
                len,
                scale,
                nullallowed
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_free
 *
 * Member of the SQL function block.
 * Releases an attribute type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, take
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_free(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_free\n", __FILE__));
        SS_MEMOBJ_DEC(SS_MEMOBJ_ATYPE);
        CHECK_ATYPE(atype);

        rs_atype_releasedefaults(cd, atype);

        ss_debug(atype->at_check = RSCHK_FREED;)
        SsMemFree(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_copy
 *
 * Member of the SQL function block.
 * Makes a copy of an attribute type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value - give :
 *
 *      pointer into the newly allocated attribute type object
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_copy(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        rs_atype_t* at;

        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_copy\n", __FILE__));
        CHECK_ATYPE(atype);
        SS_MEMOBJ_INC(SS_MEMOBJ_ATYPE, rs_atype_t);

        at = SSMEM_NEW(rs_atype_t);
        RS_ATYPE_INITBUFTOUNDEF(cd, at);
        rs_atype_copybuf(cd, at, atype);

        return(at);
}

void rs_atype_copybuf(
        rs_sysi_t*      cd,
        rs_atype_t*     dst_atype,
        rs_atype_t*     src_atype)
{
        CHECK_ATYPE(src_atype);

        if (dst_atype->at_attrtype != RSAT_UNDEFINED) {
            if (dst_atype->at_originaldefault != NULL) {
                rs_aval_free(cd, dst_atype, dst_atype->at_originaldefault);
            }
            if (dst_atype->at_currentdefault != NULL) {
                rs_aval_free(cd, dst_atype, dst_atype->at_currentdefault);
            }
        }
        *dst_atype = *src_atype;
        if (src_atype->at_originaldefault != NULL) {
            dst_atype->at_originaldefault = rs_aval_copy(
                    cd, src_atype, src_atype->at_originaldefault);
        }
        if (src_atype->at_currentdefault != NULL) {
            dst_atype->at_currentdefault = rs_aval_copy(
                    cd, src_atype, src_atype->at_currentdefault);
        }
}

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_atype_copymax
 *
 * Makes a copy of atype but possibly modifies the new atype object
 * so that unnecessary truncations be avoided in arithmetics or
 * LIKE operations
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      atype - in, use
 *              input atype
 *
 * Return value - give :
 *      created new atype object, must be freed using rs_atype_free().
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_atype_t* rs_atype_copymax(
        void* cd,
        rs_atype_t* atype)
{
        rs_atype_t* new_atype;
        rs_sqldatatype_t sqldt;
        rs_sqldatatype_t new_sqldt;
        rs_datatype_t dt;
        const rs_atypeinfo_t* atinfo;
        long new_len;

        sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        new_len =  RS_ATYPE_LENGTH(cd, atype);

        switch (sqldt) {
            case RSSQLDT_CHAR:
            case RSSQLDT_VARCHAR:
                dt = RSDT_CHAR;
                new_sqldt = RSSQLDT_VARCHAR;
                atinfo = &ATYPE_TYPES(RSSQLDT_VARCHAR);
                if (atinfo->st_defaultlen > (ulong)new_len) {
                    new_len = atinfo->st_defaultlen;
                }
                break;
            case RSSQLDT_BINARY:
            case RSSQLDT_VARBINARY:
                dt = RSDT_BINARY;
                new_sqldt = RSSQLDT_VARBINARY;
                atinfo = &ATYPE_TYPES(RSSQLDT_VARBINARY);
                if (atinfo->st_defaultlen > (ulong)new_len) {
                    new_len = atinfo->st_defaultlen;
                }
                break;
            case RSSQLDT_SMALLINT:
            case RSSQLDT_TINYINT:
                dt = RSDT_INTEGER;
                new_sqldt = RSSQLDT_INTEGER;
                break;
            case RSSQLDT_NUMERIC:
                dt = RSDT_DFLOAT;
                new_sqldt = RSSQLDT_DECIMAL;
                break;
            case RSSQLDT_WCHAR:
            case RSSQLDT_WVARCHAR:
                dt = RSDT_UNICODE;
                new_sqldt = RSSQLDT_WVARCHAR;
                atinfo = &ATYPE_TYPES(RSSQLDT_WVARCHAR);
                if (atinfo->st_defaultlen > (ulong)new_len) {
                    new_len = atinfo->st_defaultlen;
                }
                break;
            default:
                new_atype = rs_atype_copy(cd, atype);
                SU_BFLAG_SET(new_atype->at_flags, AT_NULLALLOWED);
                return (new_atype);
        }
        new_atype = rs_atype_init(
                    cd,
                    RSAT_USER_DEFINED,
                    dt,
                    new_sqldt,
                    new_len,
                    atype->at_scale,
                    TRUE);

        /* VOYAGER_SYNCHIST */
        if (rs_atype_issync(cd, atype)) {
            rs_atype_setsync(cd, new_atype, TRUE);
        }

        if (atype->at_originaldefault != NULL) {
            new_atype->at_originaldefault = rs_aval_copy(
                    cd, atype, atype->at_originaldefault);
        }
        if (atype->at_currentdefault != NULL) {
            new_atype->at_currentdefault = rs_aval_copy(
                    cd, atype, atype->at_currentdefault);
        }

        return (new_atype);
}


/*##**********************************************************************\
 *
 *              rs_atype_createconst
 *
 * Member of the SQL function block.
 * Creates an attribute type object that corresponds to a constant in
 * SQL statement
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      sqlvalstr - in, use
 *              constant string
 *
 *      p_errh - out, give
 *              in case of an error, a pointer into a newly
 *          allocated error handle is stored in *p_errh
 *          if p_errh is non-NULL
 *
 *
 * Return value - give :
 *
 *      pointer into the newly allocated field type object.
 *      NULL is returned if the constant string is not legal
 *      wrt. to any type.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_createconst(
        void*       cd,
        char*       sqlvalstr,
        rs_err_t**  p_errh)
{
        int  sqltype;
        int explicit_sqltype = 0;
        ulong const_len = 0L;
        const rs_atypeinfo_t* p_atypeinfo;

        ss_dprintf_3(("%s: rs_atype_createconst, sqlvalstr=%s\n", __FILE__, sqlvalstr));

        switch (*sqlvalstr) {
            case 'N':
                explicit_sqltype = RSSQLDT_WVARCHAR;
                ss_ct_assert(RSSQLDT_WVARCHAR != 0);
                sqlvalstr++;
                goto quoted_string;
            case 'D':
                if (sqlvalstr[1] == 'A'
                &&  sqlvalstr[2] == 'T'
                &&  sqlvalstr[3] == 'E')
                {
                    explicit_sqltype = RSSQLDT_DATE;
                    ss_ct_assert(RSSQLDT_DATE != 0);
                    sqlvalstr += 4; /* += strlen("DATE") */
                    goto quoted_string;
                }
                break;
            case 'T':
                if (sqlvalstr[1] == 'I'
                &&  sqlvalstr[2] == 'M'
                &&  sqlvalstr[3] == 'E')
                {
                    if (sqlvalstr[4] == 'S'
                    &&  sqlvalstr[5] == 'T'
                    &&  sqlvalstr[6] == 'A'
                    &&  sqlvalstr[7] == 'M'
                    &&  sqlvalstr[8] == 'P')
                    {
                        explicit_sqltype = RSSQLDT_TIMESTAMP;
                        sqlvalstr += 9; /* += strlen("TIMESTAMP") */
                        goto quoted_string;
                    } else {
                        explicit_sqltype = RSSQLDT_TIME;
                        sqlvalstr += 4; /* += strlen("TIME") */
                        goto quoted_string;
                    }
                }
                break;
            case 'X':
                explicit_sqltype = RSSQLDT_VARBINARY;
                sqlvalstr++;
                goto quoted_string;
    quoted_string:;
                for (;;sqlvalstr++) {
                    switch (*sqlvalstr) {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                            continue;
                        default:
                            break;
                    }
                    break;
                }
                if (*sqlvalstr != RS_STRQUOTECH) {
                    break; /* error */
                }
                /* FALLTHROUGH */
            case RS_STRQUOTECH: {
                bool is8bitonly;
                size_t sqlvalblen;

                sqlvalblen = strlen(sqlvalstr);
                is8bitonly = SsUTF8isASCII8((ss_byte_t*)sqlvalstr,
                                            sqlvalblen,
                                            NULL);
                for (;; ) {
                    sqlvalstr++;
                    const_len++;
                    switch (*sqlvalstr) {
                        case RS_STRQUOTECH:
                            if (sqlvalstr[1] == RS_STRQUOTECH
                            &&  sqlvalstr[2] != '\0')
                            {
                                sqlvalstr++;
                                continue;
                            }
                            /* FALLTHROUGH */
                        case '\0':
                            const_len--;
                            break;
                        default:
                            continue;
                    }
                    break;
                }
                if (!is8bitonly) {
                    sqltype = RSSQLDT_WVARCHAR;
                } else {
                    sqltype = RSSQLDT_VARCHAR;
                }
                goto type_defined;
            }
            default:
                break;
        }
        {
            /* not quoted; probably a numeric type */
            bool  isdecimalpoint = FALSE;
            bool  isexponent     = FALSE;
            int   i;
            char  ch;
            char* ptr;

            for (i = 0; sqlvalstr[i]; i++) {

                ch = sqlvalstr[i];
                if (ch == '.') {
                    isdecimalpoint = TRUE;
                }
                if (ch == 'E' || ch == 'e') {
                    isexponent = TRUE;
                }
            }

            if (!isdecimalpoint && !isexponent && strlen(sqlvalstr) > 14 ) {
                /* We try to read it as a bigint as it's big enough to lose
                   precision in double conversion */
                ss_int8_t lint;
                if (SsStrScanInt8(sqlvalstr, &lint, &ptr)) {
                    sqltype = RSSQLDT_BIGINT;
                    goto type_defined;
                }
            }
            if (isdecimalpoint && !isexponent) {
                dt_dfl_t dfl;
                if (dt_dfl_setasciiz(&dfl, sqlvalstr)) {
                    sqltype = RSSQLDT_DECIMAL;
                    goto type_defined;
                }
            }
            {
                double d;
                if (!SsStrScanDouble(sqlvalstr, &d, &ptr)) {
                    /* Illegal DOUBLE PRECISION constant */
                    rs_error_create(p_errh, E_ILLDBLCONST_S, sqlvalstr);
                    return(NULL);
                }
                sqltype = RSSQLDT_DOUBLE;
            }
        }
type_defined:;
        if (explicit_sqltype != 0) {
            sqltype = explicit_sqltype;
        }
        p_atypeinfo = &ATYPE_TYPES(sqltype);
        if (const_len < p_atypeinfo->st_defaultlen) {
            const_len = p_atypeinfo->st_defaultlen;
        }
        return(
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                p_atypeinfo->st_rstype,
                sqltype,
                const_len,
                p_atypeinfo->st_defaultscale,
                FALSE
            )
        );
}

#endif /* SS_NOSQL */

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_atype_name
 *
 * Member of the SQL function block.
 * Returns the name of the SQL main type of an attribute type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value - ref :
 *
 *      pointer into a string containing the main type name
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_atype_name(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_name\n", __FILE__));
        CHECK_ATYPE(atype);
        return((char*)ATYPE_TYPES(atype->at_sqldatatype).st_sqlname);
}

#endif /* SS_NOSQL */

/*##**********************************************************************\
 *
 *              rs_atype_length
 *
 * Member of the SQL function block.
 * Returns the length of the attribute type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 *      >0, length of the attr type object
 *       0, if the length of the attr type object is not defined
 *
 * Limitations  :
 *
 * Globals used :
 */
ss_int4_t rs_atype_length(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{

        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_length\n", __FILE__));
        CHECK_ATYPE(atype);
        return (_RS_ATYPE_LENGTH_(cd,atype));
}

/*##**********************************************************************\
 *
 *              rs_atype_scale
 *
 * Member of the SQL function block.
 * Returns the scale of the attribute type object
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 *      >0, scale of the atype object
 *       0, if the scale of the atype object is not defined
 *
 * Limitations  :
 *
 * Globals used :
 */
ss_int1_t rs_atype_scale(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_scale\n", __FILE__));
        CHECK_ATYPE(atype);
        return(atype->at_scale);
}

/*##**********************************************************************\
 *
 *              rs_atype_nullallowed
 *
 * Member of the SQL function block.
 * Checks if NULL is allowed for values for a given attr type object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 *      TRUE, if NULL is allowed
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_nullallowed(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_nullallowed\n", __FILE__));
        CHECK_ATYPE(atype);
        return (SU_BFLAG_TEST(atype->at_flags, AT_NULLALLOWED) != 0);
}

/*##**********************************************************************\
 *
 *              rs_atype_setnullallowed
 *
 * Member of the SQL function block.
 * Sets the NULL permission of a given field type object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 *      nullallowed -
 *              TRUE, if NULL is to be allowed
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_setnullallowed(cd, atype, nullallowed)
    void*       cd;
    rs_atype_t* atype;
    bool        nullallowed;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_setnullallowed\n", __FILE__));
        CHECK_ATYPE(atype);
        if (nullallowed) {
            SU_BFLAG_SET(atype->at_flags, AT_NULLALLOWED);
        } else {
            SU_BFLAG_CLEAR(atype->at_flags, AT_NULLALLOWED);
        }
}

/*##**********************************************************************\
 *
 *              rs_atype_setpseudo
 *
 * Sets the atype as a pseudo atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_setpseudo(cd, atype, ispseudo)
    void*       cd;
    rs_atype_t* atype;
        bool        ispseudo;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_setpseudo\n", __FILE__));
        CHECK_ATYPE(atype);
        if (ispseudo) {
            SU_BFLAG_SET(atype->at_flags, AT_PSEUDO);
        } else {
            SU_BFLAG_CLEAR(atype->at_flags, AT_PSEUDO);
        }
}

/*##**********************************************************************\
 *
 *              rs_atype_autoinc
 *
 * Checks if the atype is a auto_increment atype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_autoinc(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_autoinc\n", __FILE__));
        CHECK_ATYPE(atype);

        return (SU_BFLAG_TEST(atype->at_flags, AT_AUTO_INC) != 0);
}

/*##**********************************************************************\
 *
 *              rs_atype_setautoinc
 *
 * Sets the atype as a auto_increment atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 *      seq_id - in, use
 *              sequence identifier
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_setautoinc(cd, atype, isautoinc, seq_id)
    void*       cd;
    rs_atype_t* atype;
    bool        isautoinc;
    long        seq_id;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_setautoinc\n", __FILE__));
        CHECK_ATYPE(atype);
        if (isautoinc) {
            SU_BFLAG_SET(atype->at_flags, AT_AUTO_INC);
        } else {
            SU_BFLAG_CLEAR(atype->at_flags, AT_AUTO_INC);
        }

        atype->at_autoincseqid = (ss_uint4_t) seq_id;
}

/*##**********************************************************************\
 *
 *              rs_atype_setmysqldatatype
 *
 * Sets the MySQL data type to atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 *      type - in
 *              MySQL type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_setmysqldatatype(
        rs_sysi_t*  cd,
        rs_atype_t* atype,
        rs_mysqldatatype_t type)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_setmysqldatatype\n", __FILE__));
        CHECK_ATYPE(atype);

        atype->at_mysqldatatype = (ss_uint1_t)type;

        ss_dassert(atype->at_mysqldatatype >= RS_MYSQLTYPE_NONE && atype->at_mysqldatatype <= RS_MYSQLTYPE_BIT);
}

/*##**********************************************************************\
 *
 *              rs_atype_mysqldatatype
 *
 * Returns MySQL data type to atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 *      type - in
 *              MySQL type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_mysqldatatype_t rs_atype_mysqldatatype(
        rs_sysi_t*  cd,
        rs_atype_t* atype)
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_mysqldatatype\n", __FILE__));
        CHECK_ATYPE(atype);

        return((rs_mysqldatatype_t)atype->at_mysqldatatype);
}

#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              rs_atype_sql_pseudo
 *
 * Checks if the atype is a pseudo atype. A special version for sync
 * publication insert in replica, used from a special sqlsystem.
 * This trick allows insert to sync tuple history version attribute that
 * is received from master.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_sql_pseudo(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_sql_pseudo\n", __FILE__));
        CHECK_ATYPE(atype);

        if (rs_sysi_subscribe_write(cd) && rs_atype_issync(cd, atype)) {
            return(FALSE);
        } else {
            return(rs_atype_pseudo(cd, atype));
        }
}

bool rs_atype_syncpublinsert_pseudo(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_syncpublinsert_pseudo\n", __FILE__));
        CHECK_ATYPE(atype);

        if (rs_atype_issync(cd, atype)) {
            return(FALSE);
        } else {
            return(rs_atype_pseudo(cd, atype));
        }
}

/*##**********************************************************************\
 *
 *              rs_atype_synctuplevers
 *
 * Checks if the atype is a synctuplevers atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_issync(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_issync\n", __FILE__));
        CHECK_ATYPE(atype);
        return (SU_BFLAG_TEST(atype->at_flags, AT_SYNC) != 0);
}

/*##**********************************************************************\
 *
 *              rs_atype_setsync
 *
 * Sets the atype as a sync atype.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in out, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_setsync(cd, atype, issync)
    void*       cd;
    rs_atype_t* atype;
        bool        issync;
{
        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_setsync\n", __FILE__));
        CHECK_ATYPE(atype);
        if (issync) {
            SU_BFLAG_SET(atype->at_flags, AT_SYNC);
        } else {
            SU_BFLAG_CLEAR(atype->at_flags, AT_SYNC);
        }
}

#endif /* SS_SYNC */

/*##**********************************************************************\
 *
 *              rs_atype_getparammode
 *
 * Returns procedure parameter mode.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              atype -
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
rs_attrparammode_t rs_atype_getparammode(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype)
{
        CHECK_ATYPE(atype);

        switch (atype->at_flags & AT_PARAM_INOUT) {
            case AT_PARAM_IN:
                return(RSPM_IN);
            case AT_PARAM_OUT:
                return(RSPM_OUT);
            case AT_PARAM_INOUT:
                return(RSPM_INOUT);
            default:
                return(RSPM_IN);
        }
}

/*##**********************************************************************\
 *
 *              rs_atype_setparammode
 *
 * Sets procedure parameter mode.
 *
 * Parameters :
 *
 *              cd -
 *
 *
 *              atype -
 *
 *
 *              mode -
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
void rs_atype_setparammode(
        void*              cd __attribute__ ((unused)),
        rs_atype_t*        atype,
        rs_attrparammode_t mode)
{
        CHECK_ATYPE(atype);

        switch (mode) {
            case RSPM_IN:
                SU_BFLAG_SET(atype->at_flags, AT_PARAM_IN);
                break;
            case RSPM_OUT:
                SU_BFLAG_SET(atype->at_flags, AT_PARAM_OUT);
                break;
            case RSPM_INOUT:
                SU_BFLAG_SET(atype->at_flags, AT_PARAM_INOUT);
                break;
            default:
                ss_derror;
                break;
        }
}

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_atype_pars
 *
 * Member of the SQL function block.
 * Returns the parameter information for an attr type object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value - give :
 *
 *      Pointer into a newly allocated string containing
 *      the parameter information for the main type. NULL
 *      is returned if there are no parameters involved
 *      with the specified attribute type object
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_atype_pars(cd, atype)
    void*       cd;
    rs_atype_t* atype;
{
        char buf[50];
        char *r;
        ulong scale;
        ulong length;

        SS_NOTUSED(cd);
        ss_dprintf_3(("%s: rs_atype_pars\n", __FILE__));
        CHECK_ATYPE(atype);

        scale = rs_atype_scale(cd, atype);
        length = rs_atype_length(cd, atype);

        if (scale > 0) {
            SsSprintf(buf, "%lu,%lu", length, scale);
        } else {
            SsSprintf(buf, "%lu", length);
        }

        r = SsMemStrdup(buf);
        return(r);
}

#endif /* SS_NOSQL */


#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_atype_likepos
 *
 * Member of the SQL function block.
 * Checks if the "LIKE" operator can be applied to an attribute type object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype1 - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *
 *      TRUE, if "LIKE" can be applied
 *      FALSE, if not
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_likepos(cd, atype)
    void*       cd __attribute__ ((unused));
    rs_atype_t* atype;
{
        rs_datatype_t dt;


        ss_dprintf_3(("%s: rs_atype_likepos\n", __FILE__));
        CHECK_ATYPE(atype);
        dt = RS_ATYPE_DATATYPE(cd, atype);
        return (dt == RSDT_CHAR || dt == RSDT_UNICODE);
}

#endif /* SS_NOSQL */

/*##**********************************************************************\
 *
 *              rs_atype_init
 *
 * Allocates a new rs_atype_t object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      attrtype - in, use
 *              type of the attribute
 *
 *      datatype - in, use
 *              attribute's internal datatype (RSDT_xxx)
 *
 *      sqldatatype - in, use
 *              attribute's sql datatype (RSSQLDT_xxx)
 *
 *      len - in, use
 *              length
 *
 *      scale - in, use
 *              scale
 *
 *      nullallowed - in, use
 *              TRUE if NULL is allowed
 *
 * Return value - give :
 *
 *      pointer to a newly allocated rs_atype_t object
 *
 * Limitations  :
 *
 * Globals used :
 *
 */
rs_atype_t* rs_atype_init(
        void*            cd,
        rs_attrtype_t    attrtype,
        rs_datatype_t    datatype __attribute__ ((unused)),
        rs_sqldatatype_t sqldatatype,
        long             len,
        long             scale,
        bool             nullallowed
) {
        rs_atype_t* at = SSMEM_NEW(rs_atype_t);
        ss_dprintf_3(("%s: rs_atype_init\n", __FILE__));
        SS_NOTUSED(cd);
        ss_dassert(rs_atype_checktypes(cd, attrtype, datatype, sqldatatype));
        /* ss_assert(ATYPE_TYPES(sqldatatype).st_rstype == datatype); */
        SS_MEMOBJ_INC(SS_MEMOBJ_ATYPE, rs_atype_t);

        ss_debug(at->at_check = RSCHK_ATYPE;)
        at->at_len          = len;
        at->at_scale        = (ss_int1_t)scale;
        at->at_sqldatatype  = (ss_int1_t)sqldatatype;
        at->at_attrtype     = (ss_uint1_t)attrtype;
        if (nullallowed) {
            at->at_flags    = AT_NULLALLOWED;
        } else {
            at->at_flags    = 0;
        }
        at->at_originaldefault = NULL;
        at->at_currentdefault = NULL;
        at->at_autoincseqid   = 0;
        at->at_autoincseqid   = 0;
        at->at_mysqldatatype  = (ss_uint1_t)RS_MYSQLTYPE_NONE;
        at->at_extlen         = 0;
        at->at_extscale       = 0;
#ifdef SS_COLLATION
        at->at_collation = NULL;
        FAKE_CODE_BLOCK(
                FAKE_RES_ELBONIAN_COLLATION,
                if (at->at_len == 1313 &&
                    rs_atype_datatype(cd, at) == RSDT_CHAR)
                {
                    su_collation_fake_elbonian_link_in();
                    at->at_collation = &su_collation_fake_elbonian;
                });
#endif /* SS_COLLATION */
        return(at);
}

/*##**********************************************************************\
 *
 *              rs_atype_init_sqldt
 *
 * Allocates a new rs_atype_t object using given sqldatatype.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      sqldatatype -
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
rs_atype_t* rs_atype_init_sqldt(
        void* cd,
        rs_sqldatatype_t sqldatatype
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(sqldatatype).st_rstype,
                sqldatatype,
                ATYPE_TYPES(sqldatatype).st_defaultlen,
                ATYPE_TYPES(sqldatatype).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_init_rsdt
 *
 * Allocates a new rs_atype_t object using given internal data type (RSDT_*).
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      datatype -
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
rs_atype_t* rs_atype_init_rsdt(
        void* cd,
        rs_datatype_t datatype
) {
        switch (datatype) {
            case RSDT_CHAR:
                return (rs_atype_initchar(cd));
            case RSDT_DATE:
                return (rs_atype_initdate(cd));
            case RSDT_BINARY:
                return (rs_atype_initbinary(cd));
            case RSDT_UNICODE:
                return (
                    rs_atype_init(
                        cd,
                        RSAT_USER_DEFINED,
                        ATYPE_TYPES(RSSQLDT_WVARCHAR).st_rstype,
                        RSSQLDT_WVARCHAR,
                        ATYPE_TYPES(RSSQLDT_WVARCHAR).st_defaultlen,
                        ATYPE_TYPES(RSSQLDT_WVARCHAR).st_defaultscale,
                        TRUE
                    )
                );
            case RSDT_INTEGER:
                return(rs_atype_initlong(cd));
            case RSDT_BIGINT:
                return(rs_atype_initbigint(cd));
            case RSDT_FLOAT:
                return(rs_atype_initfloat(cd));
            case RSDT_DOUBLE:
                return(rs_atype_initdouble(cd));
            case RSDT_DFLOAT:
                return(rs_atype_initdfloat(cd));
            default:
                ss_error;
                return(NULL);
        }
}

/*##**********************************************************************\
 *
 *              rs_atype_checktypes
 *
 * Checks that attribute type enum values are correct. Used e.g. to check
 * that type numbers from an RPC message are correct.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      attrtype -
 *
 *
 *      datatype -
 *
 *
 *      sqldatatype -
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
bool rs_atype_checktypes(
    void*            cd,
    rs_attrtype_t    attrtype,
        rs_datatype_t    datatype,
        rs_sqldatatype_t sqldatatype
) {
        ss_dprintf_3(("%s: rs_atype_checktypes\n", __FILE__));
        SS_NOTUSED(cd);

        return(
            (int)attrtype >= RSAT_USER_DEFINED && attrtype <= RSAT_UNDEFINED &&
            (int)datatype >= RSDT_CHAR && datatype <= RSDT_BIGINT &&
            ((sqldatatype >= RSSQLDT_BIT && sqldatatype <= RSSQLDT_VARCHAR)
             || (sqldatatype >= RSSQLDT_WLONGVARCHAR &&
                 sqldatatype <= RSSQLDT_WCHAR)
            ) &&
            sqldatatype != 0
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initchar
 *
 * Initializes a user defined VARCHAR type atype
 *
 * Parameters :
 *
 *      cd - in, don't use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_CHAR atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initchar(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_VARCHAR).st_rstype,
                RSSQLDT_VARCHAR,
                ATYPE_TYPES(RSSQLDT_VARCHAR).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_VARCHAR).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initlongvarchar
 *
 * Initializes a user defined LONG VARCHAR type atype
 *
 * Parameters :
 *
 *      cd - in, don't use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_CHAR atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initlongvarchar(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_LONGVARCHAR).st_rstype,
                RSSQLDT_LONGVARCHAR,
                ATYPE_TYPES(RSSQLDT_LONGVARCHAR).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_LONGVARCHAR).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initbinary
 *
 * Initializes a user defined binary CHAR type atype
 *
 * Parameters :
 *
 *      cd - in, don't use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated binary RSDT_CHAR atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initbinary(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_VARBINARY).st_rstype,
                RSSQLDT_VARBINARY,
                ATYPE_TYPES(RSSQLDT_VARBINARY).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_VARBINARY).st_defaultscale,
                TRUE
            )
        );
}


/*##**********************************************************************\
 *
 *              rs_atype_initlongvarbinary
 *
 * Initializes a user defined binary CHAR type atype
 *
 * Parameters :
 *
 *      cd - in, don't use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated long binary RSDT_CHAR atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initlongvarbinary(
        void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_LONGVARBINARY).st_rstype,
                RSSQLDT_LONGVARBINARY,
                ATYPE_TYPES(RSSQLDT_LONGVARBINARY).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_LONGVARBINARY).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initlong
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_INTEGER atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initlong(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_INTEGER).st_rstype,
                RSSQLDT_INTEGER,
                ATYPE_TYPES(RSSQLDT_INTEGER).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_INTEGER).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initbigint
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_BIGINT atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initbigint(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_BIGINT).st_rstype,
                RSSQLDT_BIGINT,
                ATYPE_TYPES(RSSQLDT_BIGINT).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_BIGINT).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initsmallint
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSSQLDT_SMALLINT atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initsmallint(void* cd)
{
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_SMALLINT).st_rstype,
                RSSQLDT_SMALLINT,
                ATYPE_TYPES(RSSQLDT_SMALLINT).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_SMALLINT).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initsmallint
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSSQLDT_TINYINT atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_inittinyint(void* cd)
{
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_TINYINT).st_rstype,
                RSSQLDT_TINYINT,
                ATYPE_TYPES(RSSQLDT_TINYINT).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_TINYINT).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initfloat
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_FLOAT atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initfloat(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_REAL).st_rstype,
                RSSQLDT_REAL,
                ATYPE_TYPES(RSSQLDT_REAL).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_REAL).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initdouble
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_DOUBLE atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initdouble(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_DOUBLE).st_rstype,
                RSSQLDT_DOUBLE,
                ATYPE_TYPES(RSSQLDT_DOUBLE).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_DOUBLE).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initdfloat
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_DFLOAT atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initdfloat(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_DECIMAL).st_rstype,
                RSSQLDT_DECIMAL,
                ATYPE_TYPES(RSSQLDT_DECIMAL).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_DECIMAL).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initdate
 *
 * Initializes a user defined atype
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 * Return value :
 *
 *      Pointer to newly allocated RSDT_DATE atype
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_atype_t* rs_atype_initdate(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_DATE).st_rstype,
                RSSQLDT_DATE,
                ATYPE_TYPES(RSSQLDT_DATE).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_DATE).st_defaultscale,
                TRUE
            )
        );
}

rs_atype_t* rs_atype_inittime(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_TIME).st_rstype,
                RSSQLDT_TIME,
                ATYPE_TYPES(RSSQLDT_TIME).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_TIME).st_defaultscale,
                TRUE
            )
        );
}

rs_atype_t* rs_atype_inittimestamp(
                void* cd
) {
        return (
            rs_atype_init(
                cd,
                RSAT_USER_DEFINED,
                ATYPE_TYPES(RSSQLDT_TIMESTAMP).st_rstype,
                RSSQLDT_TIMESTAMP,
                ATYPE_TYPES(RSSQLDT_TIMESTAMP).st_defaultlen,
                ATYPE_TYPES(RSSQLDT_TIMESTAMP).st_defaultscale,
                TRUE
            )
        );
}

/*##**********************************************************************\
 *
 *              rs_atype_initbysqldt
 *
 * Initializes atype object by SQL Data type and scale & length info
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      sqldt - in
 *              SQL data type
 *
 *      length - in
 *              length (<0 if default )
 *
 *      scale - in
 *              scale (<0 if default)
 *
 * Return value - give:
 *      new atype object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_atype_t* rs_atype_initbysqldt(
        void* cd,
        rs_sqldatatype_t sqldt,
        long length,
        long scale)
{
        rs_atype_t* atype;
        const rs_atypeinfo_t* atinfo = &ATYPE_TYPES(sqldt);

        if (length < 0) {
            length = atinfo->st_defaultlen;
        }
        if (scale < 0) {
            scale = atinfo->st_defaultscale;
        }
        atype = rs_atype_init(
                    cd,
                    RSAT_USER_DEFINED,
                    atinfo->st_rstype,
                    sqldt,
                    length,
                    scale,
                    TRUE);
        return (atype);
}


/*##**********************************************************************\
 *
 *              rs_atype_isuserdefined
 *
 * Checks if the given attribute type is user defined.
 * Attribute type that is user defined is accessible to the SQL user.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *      TRUE, if user defined
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_atype_isuserdefined(
        void*       cd,
        rs_atype_t* atype
) {
        SS_NOTUSED(cd);
        CHECK_ATYPE(atype);
        return(atype->at_attrtype == RSAT_USER_DEFINED);
}

#ifndef rs_atype_datatype
/*##**********************************************************************\
 *
 *              rs_atype_datatype
 *
 * Returns the internal (RSDT_) attribute value type.
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      atype - in, use
 *              pointer to an attribute type object
 *
 * Return value :
 *      Attribute value type associated with atype.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_datatype_t rs_atype_datatype(
        void*       cd,
        rs_atype_t* atype
) {
        SS_NOTUSED(cd);
        CHECK_ATYPE(atype);
        return(_RS_ATYPE_DATATYPE_(cd, atype));
}
#endif /* !defined(rs_atype_datatype) */

#ifndef rs_atype_attrtype
/*##**********************************************************************\
 *
 *              rs_atype_attrtype
 *
 * Returns the atype's attribute type (RSAT_).
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      atype - in, use
 *              pointer to an atype object
 *
 * Return value :
 *      Attribute type associated with atype.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_attrtype_t rs_atype_attrtype(
        void*       cd,
        rs_atype_t* atype
) {
        SS_NOTUSED(cd);
        return((rs_attrtype_t)atype->at_attrtype);
}
#endif /* !defined(rs_atype_attrtype) */

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *              rs_atype_attrtypename
 * (ALIAS TYPENAMES:should check this)
 * Returns the name of attribute type.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      attrtype - in, use
 *              attribute type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
char* rs_atype_attrtypename(cd, attrtype)
        void*         cd;
        rs_attrtype_t attrtype;
{
        static const char* attrtypes[] = {
            "USER_DEFINED",
            "TUPLE_ID",
            "TUPLE_VERSION",
            "TRX_ID",
            "FREELY_DEFINED",
            "CLUSTER_ID",
            "RELATION_ID",
            "KEY_ID",
            "REMOVED",
            "SYNC",
            "UNDEFINED"
        #ifdef SS_COLLATION
            ,"COLLATION_KEY"
        #endif /* SS_COLLATION */
        };

        SS_NOTUSED(cd);

        if (attrtype >= sizeof(attrtypes)/sizeof(attrtypes[0])) {
            ss_rc_derror(attrtype);
        }

        return((char *)attrtypes[attrtype]);
}

/*##**********************************************************************\
 *
 *              rs_atype_print
 *
 * Prints attribute type using SsDbgPrintf.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      attrtype - in, use
 *              attribute type
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_atype_print(cd, atype)
        void*       cd;
        rs_atype_t* atype;
{
        CHECK_ATYPE(atype);
        SsDbgPrintf("%-14s %-10s %2ld %2d %-3s\n",
            rs_atype_attrtypename(cd, atype->at_attrtype),
            rs_atype_name(cd, atype),
            atype->at_len,
            (int)atype->at_scale,
            rs_atype_nullallowed(cd,atype)? "YES" : "NO");
}
#endif /* SS_DEBUG */


/*#***********************************************************************\
 *
 *              atype_namecmp
 *
 * compares the names of two data types
 *
 * Parameters :
 *
 *      const -
 *
 *
 *      p2 -
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
static int atype_namecmp(const void* p1, const void* p2)
{
        int cmp = strcmp(
                    (const char*)p1,
                    ((const atype_typebyname_t*)p2)->tbn_name);
        return (cmp);
}


/*#***********************************************************************\
 *
 *              atype_getsqltypebyname
 *
 * Returns the type code of the SQL main type name.
 *
 * Parameters :
 *
 *      typename - in, use
 *          SQl typename
 *
 * Return value - ref :
 *
 *          SQL main type code (RSSQLDT_)
 *          0, not known
 *
 * Limitations  :
 *
 * Globals used :
 */

static int atype_getsqltypebyname(typename)
    char* typename;
{
        ss_dprintf_3(("%s: atype_getsqltypebyname, typename %s\n",
                       __FILE__, typename));
        ss_dassert(typename != NULL);

        {
            ss_debug(static bool checked = FALSE;)

            atype_typebyname_t* p_typebyname;
            bool foundp;

            ss_debug (
                if (!checked) {
                    uint i;
                    checked = TRUE;

                    for (i = 1;
                         i < sizeof(atype_typebyname)/sizeof(atype_typebyname[0]);
                         i++)
                    {
                        ss_rc_assert(
                            strcmp(
                                atype_typebyname[i-1].tbn_name,
                                atype_typebyname[i].tbn_name) < 0,
                            i);
                    }

                }
            );
            foundp = su_bsearch(
                        typename,
                        (void*)atype_typebyname,
                        sizeof(atype_typebyname)/sizeof(atype_typebyname[0]),
                        sizeof(atype_typebyname[0]),
                        atype_namecmp,
                        (void**)&p_typebyname);
            if (foundp) {
                return (p_typebyname->tbn_sqldt);
            }
            return (0);

        }
}

/*##**********************************************************************\
 *
 *              rs_atype_sqldttodt
 *
 * Converts given SQL datatype (RSSQLDT_) to internal datatype (RSDT_)
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      sqldatatype - in, use
 *              One of SQL datatypes RSSQLDT_
 *
 * Return value :
 *
 *          Internal datatype number
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_datatype_t rs_atype_sqldttodt(
        void*       cd,
        rs_sqldatatype_t sqldatatype
) {
        SS_NOTUSED(cd);
        ss_dassert(RS_ATYPE_ISLEGALSQLDT(cd, sqldatatype));

        return(_RS_ATYPE_SQLDTTODT_(cd, sqldatatype));
}


/*##**********************************************************************\
 *
 *              rs_atype_datatyperadix
 *
 * Returns the radix of given rs_datatype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      datatype - in , use
 *          datatype as RSDT_
 *
 * Return value :
 *
 *      radix
 *      RS_RADIX_NULL, if not applicable
 *
 * Limitations  :
 *
 * Globals used :
 */
int rs_atype_datatyperadix(
        void*         cd,
        rs_datatype_t datatype
){
        SS_NOTUSED(cd);
        switch (datatype) {
            case RSDT_CHAR:
            case RSDT_DATE:
            case RSDT_BINARY:
            case RSDT_UNICODE:
                return(RS_RADIX_NULL);
            case RSDT_INTEGER:
                return(RS_INT_RADIX);
            case RSDT_BIGINT:
                return(RS_BIGINT_RADIX);
            case RSDT_FLOAT:
                return(RS_REAL_RADIX);
            case RSDT_DOUBLE:
                return(RS_DOUBLE_RADIX);
            case RSDT_DFLOAT:
                return(RS_DFLOAT_RADIX);
        }
        ss_rc_error(datatype);
        return(RS_RADIX_NULL);
}

/*##**********************************************************************\
 *
 *              rs_atype_getdefaultdtformat
 *
 * Returns the default dt-format based on attribute type.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      atype -
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
char* rs_atype_getdefaultdtformat(void* cd, rs_atype_t* atype)
{
        char* dtformat;

        switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
            case RSSQLDT_TIMESTAMP:
                dtformat = rs_sysi_timestampformat(cd);
                break;
            case RSSQLDT_DATE:
                dtformat = rs_sysi_dateformat(cd);
                break;
            case RSSQLDT_TIME:
                dtformat = rs_sysi_timeformat(cd);
                break;
            default:
                dtformat = NULL;
                break;
        }
        return(dtformat);
}


/*##**********************************************************************\
 *
 *              rs_atype_sqltypelength
 *
 * Returns SQL datatype byte length and length.
 *
 * Parameters :
 *
 *      cd -
 *
 *      atype -
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
void rs_atype_sqltypelength(
        void* cd,
        rs_atype_t* atype,
        ulong* out_bytelen,
        ulong* out_sqllen)
{
        ulong len, bytelen, sqllen;
        rs_sqldatatype_t sqldt;

        bytelen = 0;
        sqllen = 0;
        len = rs_atype_length(cd, atype);
        sqldt = rs_atype_sqldatatype(cd, atype);

        switch (sqldt) {

            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WCHAR:
                bytelen = len * 2;
                if (bytelen > (ulong)SS_INT4_MAX) {
                    bytelen = len;
                }
                sqllen = len;
                break;
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_BINARY:
            case RSSQLDT_CHAR:
            case RSSQLDT_VARCHAR:
                bytelen = len;
                sqllen = len;
                break;
            case RSSQLDT_BIT:
                bytelen = 1;
                sqllen = len;
                break;
            case RSSQLDT_TINYINT:
                bytelen = 1;
                sqllen = 3;
                break;
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
                bytelen = len + 1;
                sqllen = len;
                break;
            case RSSQLDT_INTEGER:
                bytelen = 4;
                sqllen = len;
                break;
            case RSSQLDT_BIGINT:
                bytelen = 8;
                sqllen = len;
                break;
            case RSSQLDT_SMALLINT:
                bytelen = 2;
                sqllen = 5;
                break;
            case RSSQLDT_FLOAT:
                bytelen = 8;
                sqllen = len;
                break;
            case RSSQLDT_REAL:
                bytelen = 4;
                sqllen = len;
                break;
            case RSSQLDT_DOUBLE:
                bytelen = 8;
                sqllen = len;
                break;
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
                bytelen = len;
                sqllen = len;
                break;
            default:
                ss_derror;
                break;
        }
        *out_bytelen = bytelen;
        *out_sqllen = sqllen;
        return;
}

long rs_atype_maxstoragelength(
        void* cd,
        rs_atype_t* atype)
{
        long maxlen = 0;

        switch (rs_atype_datatype(cd, atype)) {
            case RSDT_CHAR:
                maxlen = rs_atype_length(cd, atype);
                if (maxlen <= 0) {
                    maxlen = SS_INT4_MAX;
                }
                if (maxlen < SS_INT4_MAX - VA_LENGTHMAXLEN - 1) {
                    maxlen += VA_LENGTHMAXLEN + 1;
                }
                break;
            case RSDT_INTEGER:
                maxlen = VA_LONGMAXLEN;
                break;
            case RSDT_FLOAT:
                maxlen = VA_FLOATMAXLEN;
                break;
            case RSDT_DOUBLE:
                maxlen = VA_DOUBLEMAXLEN;
                break;
            case RSDT_DATE:
                maxlen = 1 + DT_DATE_DATASIZE;
                break;
            case RSDT_DFLOAT:
                maxlen = DFL_VA_MAXLEN;
                break;
            case RSDT_BINARY:
                maxlen = rs_atype_length(cd, atype);
                if (maxlen <= 0) {
                    maxlen = SS_INT4_MAX;
                }
                if (maxlen < SS_INT4_MAX - VA_LENGTHMAXLEN - 1) {
                    maxlen += VA_LENGTHMAXLEN + 1;
                }
                break;
            case RSDT_UNICODE:
                maxlen = rs_atype_length(cd, atype);
                if (maxlen <= 0) {
                    maxlen = SS_INT4_MAX;
                }
                if (maxlen < SS_INT4_MAX - VA_LENGTHMAXLEN - 1) {
                    maxlen += VA_LENGTHMAXLEN + 1;
                }
                if (maxlen < SS_INT4_MAX / 2) {
                    maxlen = maxlen * 2;
                } else {
                    maxlen = SS_INT4_MAX;
                }
                break;
            case RSDT_BIGINT:
                maxlen = VA_INT8MAXLEN;
                break;
            default:
                ss_error;
        }
        return(maxlen);
}

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *              rs_atype_sqldatatypename
 *
 * Gets SQL Name for atype
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      atype - in, use
 *              attribute type
 *
 * Return value - ref :
 *      string containing SQL name for atype (eg. RS_TN_FLOAT)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* rs_atype_sqldatatypename(
        void* cd,
        rs_atype_t* atype)
{
        SS_NOTUSED(cd);
        return((char*)ATYPE_TYPES(atype->at_sqldatatype).st_sqlname);
}

/*##**********************************************************************\
 *
 *              rs_atype_sqldatatypenamebydt
 *
 * Returns data type name by sql data type enum value
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      sqldt - in
 *              RSSQLDT_XXXX
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* rs_atype_sqldatatypenamebydt(
        void* cd,
        rs_sqldatatype_t sqldt)
{
        SS_NOTUSED(cd);
        return((char*)ATYPE_TYPES(sqldt).st_sqlname);
}
/*##**********************************************************************\
 *
 *              rs_atype_isnum
 *
 * Returns information whether an atype is numeric type
 *
 * Parameters :
 *
 *      cd - in, use
 *
 *
 *      atype - in, use
 *
 *
 * Return value :
 *      TRUE atype is of a numerical type
 *      FALSE atype is nonnumeric.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_atype_isnum(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype)
{
        rs_datatype_t dt;

        CHECK_ATYPE(atype);
        dt = ATYPE_TYPES(atype->at_sqldatatype).st_rstype;
        return (dt_isnumber(dt));
}

/*##**********************************************************************\
 *
 *              rs_atype_defpar
 *
 * returns default type for a ? parameter
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 * Return value - give :
 *      atype object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_atype_t* rs_atype_defpar(void* cd)
{
        rs_atype_t* atype;

        atype = rs_atype_initbysqldt(cd, RSSQLDT_FLOAT, -1L, -1L);
        return (atype);
}

#endif /* SS_NOSQL */


/*##**********************************************************************\
 *
 *              rs_atype_chartouni
 *
 * Returns data type that is UNICODE of same length as the
 * input atype that is CHAR
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      atype - in, use
 *              input atype (RSSQLDT_CHAR, RSSQLDT_VARCHAR, RSSQLDT_LONGVARCHAR)
 *
 * Return value - give:
 *      created new atype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_atype_t* rs_atype_chartouni(void* cd, rs_atype_t* atype)
{
        rs_sqldatatype_t sqldt;
        long type_len;
        rs_atype_t* res_atype;

        sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        type_len = RS_ATYPE_LENGTH(cd, atype);
        switch (sqldt) {
            case RSSQLDT_CHAR:
                sqldt = RSSQLDT_WCHAR;
                break;
            case RSSQLDT_VARCHAR:
                sqldt = RSSQLDT_WVARCHAR;
                break;
            case RSSQLDT_LONGVARCHAR:
                sqldt = RSSQLDT_WLONGVARCHAR;
                break;
            default:
                ss_error;
        }
        res_atype = rs_atype_initbysqldt(cd, sqldt, type_len, -1);
        return (res_atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_unitochar
 *
 * The same as rs_atype_chartouni, but the opposite
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      atype - in, use
 *              input atype (RSSQLDT_WXXX)
 *
 * Return value - give:
 *      created new atype
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_atype_t* rs_atype_unitochar(void* cd, rs_atype_t* atype)
{
        rs_sqldatatype_t sqldt;
        long type_len;
        rs_atype_t* res_atype;

        sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        type_len = RS_ATYPE_LENGTH(cd, atype);
        switch (sqldt) {
            case RSSQLDT_WCHAR:
                sqldt = RSSQLDT_CHAR;
                break;
            case RSSQLDT_WVARCHAR:
                sqldt = RSSQLDT_VARCHAR;
                break;
            case RSSQLDT_WLONGVARCHAR:
                sqldt = RSSQLDT_LONGVARCHAR;
                break;
            default:
                ss_error;
        }
        res_atype = rs_atype_initbysqldt(cd, sqldt, type_len, -1);
        return (res_atype);
}


/*##**********************************************************************\
 *
 *              rs_atype_initrowid
 *
 * ROWID pseudo column.
 *
 * Parameters :
 *
 *      cd -
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
rs_atype_t* rs_atype_initrowid(
        rs_sysi_t* cd)
{
        rs_atype_t* atype;
        atype = rs_atype_init(
                    cd,
                    RSAT_USER_DEFINED,
                    RSDT_BINARY,
                    RSSQLDT_VARBINARY,
                    RS_VARCHAR_DEFLEN,
                    RS_SCALE_NULL,
                    FALSE);

        rs_atype_setpseudo(cd, atype, TRUE);

        return(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_initrowver
 *
 * ROWVER pseudo column or RS_ANAME_TUPLE_VERSION system column.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      pseudop -
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
rs_atype_t* rs_atype_initrowver(
        rs_sysi_t* cd,
        bool pseudop)
{
        rs_atype_t* atype;
        ulong scale;
        ulong length;
        bool nullallowed;
        rs_attrtype_t at;
        rs_sqldatatype_t sqldt;
        rs_datatype_t dt;

        if (pseudop) {
            at = RSAT_USER_DEFINED;
            sqldt = RSSQLDT_VARBINARY;
            dt = RSDT_BINARY;
            length = RS_TUPLENUM_ATYPESIZE;
            scale = RS_SCALE_NULL;
            nullallowed = FALSE;
        } else {
            at = RSAT_TUPLE_VERSION;
            sqldt = RSSQLDT_BIGINT;
            dt = RSDT_BIGINT;
            length = RS_BIGINT_PREC;
            scale = RS_BIGINT_SCALE;
            nullallowed = TRUE;
            /* Allow null, since initial version number
               has a null value. */
        }
        atype = rs_atype_init(
                    cd,
                    at,
                    dt,
                    sqldt,
                    length,
                    scale,
                    nullallowed);
        if (pseudop) {
            rs_atype_setpseudo(cd, atype, TRUE);
        }
        return(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_initrowflags
 *
 * Row flags column. Includes flags for sync history deleted info and
 * row updated info.
 *
 * Parameters :
 *
 *      cd -
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
rs_atype_t* rs_atype_initrowflags(
        rs_sysi_t* cd)
{
        rs_atype_t* atype;

        atype = rs_atype_initlong(cd);

        rs_atype_setpseudo(cd, atype, TRUE);

        return(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_initsynctuplevers
 *
 * Sync typle version.
 *
 * NOTE! When changing the type, check also tab1dd.c macro $(SYNCVERS).
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      pseudop -
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
rs_atype_t* rs_atype_initsynctuplevers(
        rs_sysi_t* cd,
        bool pseudop)
{
        rs_atype_t* atype;

        atype = rs_atype_init(
                    cd,
                    pseudop ? RSAT_USER_DEFINED
                            : RSAT_SYNC,
                    RSDT_BINARY,
                    RSSQLDT_BINARY,
                    (ulong)RS_TUPLENUM_ATYPESIZE,
                    (ulong)RS_SCALE_NULL,
                    pseudop);

        if (pseudop) {
            rs_atype_setpseudo(cd, atype, TRUE);
        }
        return(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_initsyncispubltuple
 *
 * Sync ispubltuple.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      pseudop -
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
rs_atype_t* rs_atype_initsyncispubltuple(
        rs_sysi_t* cd,
        bool pseudop)
{
        rs_atype_t* atype;

        atype = rs_atype_init(
                    cd,
                    pseudop ? RSAT_USER_DEFINED
                            : RSAT_SYNC,
                    RSDT_INTEGER,
                    RSSQLDT_INTEGER,
                    (ulong)0,
                    (ulong)0,
                    pseudop);

        if (pseudop) {
            rs_atype_setpseudo(cd, atype, TRUE);
        }
        return(atype);
}

/*##**********************************************************************\
 *
 *              rs_atype_givecoltype
 *
 * Returns the column type in same format as in SQL table definition,
 * e.g. CHAR(10). The returned string must be released by the caller.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      atype - in
 *
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* rs_atype_givecoltypename(
        rs_sysi_t* cd,
        rs_atype_t* atype)
{
        char* p;
        char buf[255];
        ulong len;
        ulong scale;

        len = rs_atype_length(cd, atype);
        scale = rs_atype_scale(cd, atype);

        strcpy(buf, rs_atype_name(cd, atype));
        p = buf + strlen(buf);
        if (len > 0) {
            if (scale > 0) {
                SsSprintf(p, "(%ld,%ld)", len, scale);
            } else {
                SsSprintf(p, "(%ld)", len);
            }
        }
        return(SsMemStrdup(buf));
}

/*##**********************************************************************\
 *
 *      rs_atype_insertoriginaldefault
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 *      defval - in, take
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_atype_insertoriginaldefault(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype,
        void*           defval)
{
        CHECK_ATYPE(atype);

        atype->at_originaldefault = defval;
}

/*##**********************************************************************\
 *
 *      rs_atype_getautoincseqid
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value - <usage>:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
long rs_atype_getautoincseqid(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype)
{
        CHECK_ATYPE(atype);

        return (atype->at_autoincseqid);
}

/*##**********************************************************************\
 *
 *      rs_atype_getoriginaldefault
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value - <usage>:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
rs_aval_t* rs_atype_getoriginaldefault(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype)
{
        CHECK_ATYPE(atype);

        return atype->at_originaldefault;
}

/*##**********************************************************************\
 *
 *      rs_atype_insertcurrentdefault
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 *      defval - in, take
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_atype_insertcurrentdefault(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype,
        void*           defval)
{
        CHECK_ATYPE(atype);

        atype->at_currentdefault = defval;
}

/*##**********************************************************************\
 *
 *      rs_atype_getcurrentdefault
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value - <usage>:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
rs_aval_t* rs_atype_getcurrentdefault(
        rs_sysi_t*      cd __attribute__ ((unused)),
        rs_atype_t*     atype)
{
        CHECK_ATYPE(atype);

        return atype->at_currentdefault;
}

/*##**********************************************************************\
 *
 *      rs_atype_releasedefaults
 *
 * <function description>
 *
 * Parameters:
 *      cd - <usage>
 *          <description>
 *
 *      atype - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_atype_releasedefaults(
        rs_sysi_t*      cd,
        rs_atype_t*     atype)
{
        CHECK_ATYPE(atype);

        if (atype->at_originaldefault != NULL) {
            rs_aval_free(cd, atype, atype->at_originaldefault);
            atype->at_originaldefault = NULL;
        }
        if (atype->at_currentdefault != NULL) {
            rs_aval_free(cd, atype, atype->at_currentdefault);
            atype->at_currentdefault = NULL;
        }
}
