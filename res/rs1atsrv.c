/*************************************************************************\
**  source       * rs1atsrv.c
**  directory    * res
**  description  * implementation of server-side-only atype functions
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

#define RS_INTERNAL

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
#include "rs0error.h"
#include "rs0atype.h"
#include "rs0sysi.h"

typedef rs_atype_t* atype_unionfunc_t(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh);

#define INV_UNION ((atype_unionfunc_t*)NULL)


/* RSDT_CHAR union RSDT_CHAR */
static rs_atype_t* char_char(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        ulong len1;
        ulong len2;
        ulong len_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        len1 = RS_ATYPE_LENGTH(cd, atype1);
        len2 = RS_ATYPE_LENGTH(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_CHAR:
                switch (sqldt2) {
                    case RSSQLDT_CHAR:
                        sqldt_res = RSSQLDT_CHAR;
                        break;
                    case RSSQLDT_VARCHAR:
                        sqldt_res = RSSQLDT_VARCHAR;
                        break;
                    case RSSQLDT_LONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_LONGVARCHAR);
                        sqldt_res = RSSQLDT_LONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_VARCHAR:
                switch (sqldt2) {
                    case RSSQLDT_CHAR:
                    case RSSQLDT_VARCHAR:
                        sqldt_res = RSSQLDT_VARCHAR;
                        break;
                    case RSSQLDT_LONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_LONGVARCHAR);
                        sqldt_res = RSSQLDT_LONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_LONGVARCHAR:
            default:
                ss_dassert(sqldt1 == RSSQLDT_LONGVARCHAR);
                ss_dassert(sqldt2 == RSSQLDT_CHAR
                    ||     sqldt2 == RSSQLDT_VARCHAR
                    ||     sqldt2 == RSSQLDT_LONGVARCHAR)
                sqldt_res = RSSQLDT_LONGVARCHAR;
                len1 = RS_LENGTH_NULL;
                break;
        }
        if (len1 == RS_LENGTH_NULL || len2 == RS_LENGTH_NULL) {
            len_res = RS_LENGTH_NULL;
            sqldt_res = RSSQLDT_LONGVARCHAR;
        } else {
            len_res = SS_MAX(len1, len2);
        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, len_res, -1L);
        return (union_atype);
}

/* always return copy of atype1 */
static rs_atype_t* ret_at1(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_atype_t* union_atype;

        union_atype = rs_atype_copy(cd, atype1);
        SU_BFLAG_SET(union_atype->at_flags, AT_NULLALLOWED);
        return (union_atype);
}

/* always return copy of atype2 */
static rs_atype_t* ret_at2(
        void* cd,
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_atype_t* union_atype;

        union_atype = rs_atype_copy(cd, atype2);
        SU_BFLAG_SET(union_atype->at_flags, AT_NULLALLOWED);
        return (union_atype);
}

/* incompatible union types */
static rs_atype_t* ill_union(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_TYPESNOTUNIONCOMPAT_SS,
            rs_atype_name(cd, atype1),
            rs_atype_name(cd, atype2));
        return (NULL);
}


/* RSDT_CHAR union RSDT_UNICODE */
static rs_atype_t* char_uni(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        ulong len1;
        ulong len2;
        ulong len_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        len1 = RS_ATYPE_LENGTH(cd, atype1);
        len2 = RS_ATYPE_LENGTH(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_CHAR:
                switch (sqldt2) {
                    case RSSQLDT_WCHAR:
                        sqldt_res = RSSQLDT_WCHAR;
                        break;
                    case RSSQLDT_WVARCHAR:
                        sqldt_res = RSSQLDT_WVARCHAR;
                        break;
                    case RSSQLDT_WLONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_WLONGVARCHAR);
                        sqldt_res = RSSQLDT_WLONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_VARCHAR:
                switch (sqldt2) {
                    case RSSQLDT_WCHAR:
                    case RSSQLDT_WVARCHAR:
                        sqldt_res = RSSQLDT_WVARCHAR;
                        break;
                    case RSSQLDT_WLONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_WLONGVARCHAR);
                        sqldt_res = RSSQLDT_WLONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_LONGVARCHAR:
            default:
                ss_dassert(sqldt1 == RSSQLDT_LONGVARCHAR);
                ss_dassert(sqldt2 == RSSQLDT_WCHAR
                    ||     sqldt2 == RSSQLDT_WVARCHAR
                    ||     sqldt2 == RSSQLDT_WLONGVARCHAR)
                sqldt_res = RSSQLDT_WLONGVARCHAR;
                len1 = RS_LENGTH_NULL;
                break;
        }
        if (len1 == RS_LENGTH_NULL || len2 == RS_LENGTH_NULL) {
            len_res = RS_LENGTH_NULL;
            sqldt_res = RSSQLDT_WLONGVARCHAR;
        } else {
            len_res = SS_MAX(len1, len2);
        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, len_res, -1L);
        return (union_atype);
}

/* RSDT_INTEGER union RSDT_INTEGER */
static rs_atype_t* int_int(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_TINYINT:
                switch (sqldt2) {
                    case RSSQLDT_TINYINT:
                        sqldt_res = RSSQLDT_TINYINT;
                        break;
                    case RSSQLDT_SMALLINT:
                        sqldt_res = RSSQLDT_SMALLINT;
                        break;
                    case RSSQLDT_INTEGER:
                        sqldt_res = RSSQLDT_INTEGER;
                        break;
                    case RSSQLDT_BIGINT:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_BIGINT);
                        sqldt_res = RSSQLDT_BIGINT;
                        break;
                }
                break;
            case RSSQLDT_SMALLINT:
                switch (sqldt2) {
                    case RSSQLDT_TINYINT:
                    case RSSQLDT_SMALLINT:
                        sqldt_res = RSSQLDT_SMALLINT;
                        break;
                    case RSSQLDT_INTEGER:
                        sqldt_res = RSSQLDT_INTEGER;
                        break;
                    case RSSQLDT_BIGINT:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_BIGINT);
                        sqldt_res = RSSQLDT_BIGINT;
                        break;
                }
                break;
            case RSSQLDT_INTEGER:
                switch (sqldt2) {
                    case RSSQLDT_TINYINT:
                    case RSSQLDT_SMALLINT:
                    case RSSQLDT_INTEGER:
                        sqldt_res = RSSQLDT_INTEGER;
                        break;
                    case RSSQLDT_BIGINT:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_BIGINT);
                        sqldt_res = RSSQLDT_BIGINT;
                        break;
                }
                break;
            case RSSQLDT_BIGINT:
            default:
                ss_dassert(sqldt1 == RSSQLDT_BIGINT);
                sqldt_res = RSSQLDT_BIGINT;
        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, -1L, -1L);
        return (union_atype);
}


/* always return RSDT_DOUBLE (SQL FLOAT) */
static rs_atype_t* ret_dbl(
        void* cd,
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_atype_t* union_atype;

        union_atype = rs_atype_initbysqldt(cd, RSSQLDT_FLOAT, -1L, -1L);
        return (union_atype);
}

/* RSDT_DATE union RSDT_DATE */
static rs_atype_t* date_date(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh)
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_DATE:
                switch (sqldt1) {
                    case RSSQLDT_DATE:
                        sqldt_res = RSSQLDT_DATE;
                        break;
                    case RSSQLDT_TIME:
                        goto illegal_union;
                    case RSSQLDT_TIMESTAMP:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_TIMESTAMP);
                        sqldt_res = RSSQLDT_TIMESTAMP;
                        break;
                }
                break;
            case RSSQLDT_TIME:
                switch (sqldt1) {
                    case RSSQLDT_DATE:
                        goto illegal_union;
                    case RSSQLDT_TIME:
                        sqldt_res = RSSQLDT_TIME;
                        break;
                    case RSSQLDT_TIMESTAMP:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_TIMESTAMP);
                        sqldt_res = RSSQLDT_TIMESTAMP;
                        break;
                }
                break;
            case RSSQLDT_TIMESTAMP:
            default:
                ss_dassert(sqldt1 == RSSQLDT_TIMESTAMP);
                ss_dassert(sqldt2 == RSSQLDT_DATE
                    ||     sqldt2 == RSSQLDT_TIME
                    ||     sqldt2 == RSSQLDT_TIMESTAMP);
                sqldt_res = RSSQLDT_TIMESTAMP;
                break;

        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, -1L, -1L);
        return (union_atype);
illegal_union:;
        union_atype = ill_union(cd, atype1, atype2, p_errh);
        return (union_atype);
}

/* RSDT_DFLOAT union RSDT_DFLOAT */
static rs_atype_t* dfl_dfl(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ulong prec1;
        ulong prec2;
        ulong scale1;
        ulong scale2;
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_atype_t* union_atype;

        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        if (sqldt1 == sqldt2) {
            prec1 = RS_ATYPE_LENGTH(cd, atype1);
            prec2 = RS_ATYPE_LENGTH(cd, atype2);
            if (prec1 == prec2) {
                scale1 = rs_atype_scale(cd, atype1);
                scale2 = rs_atype_scale(cd, atype2);
                if (scale1 == scale2) {
                    union_atype = rs_atype_copy(cd, atype1);
                    SU_BFLAG_SET(union_atype->at_flags, AT_NULLALLOWED);
                    return (union_atype);
                }
            }
        }
        union_atype = rs_atype_initbysqldt(cd, RSSQLDT_DECIMAL, -1L, -1L);
        return (union_atype);
}

/* RSDT_BINARY union RSDT_BINARY */
static rs_atype_t* bin_bin(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        ulong len1;
        ulong len2;
        ulong len_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        len1 = RS_ATYPE_LENGTH(cd, atype1);
        len2 = RS_ATYPE_LENGTH(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_BINARY:
                switch (sqldt2) {
                    case RSSQLDT_BINARY:
                        if (len1 == len2) {
                            sqldt_res = RSSQLDT_BINARY;
                            break;
                        }
                        /* else FALLTHROUGH */
                    case RSSQLDT_VARBINARY:
                        sqldt_res = RSSQLDT_VARBINARY;
                        break;
                    case RSSQLDT_LONGVARBINARY:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_LONGVARBINARY);
                        sqldt_res = RSSQLDT_LONGVARBINARY;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_VARBINARY:
                switch (sqldt2) {
                    case RSSQLDT_BINARY:
                    case RSSQLDT_VARBINARY:
                        sqldt_res = RSSQLDT_VARBINARY;
                        break;
                    case RSSQLDT_LONGVARBINARY:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_LONGVARBINARY);
                        sqldt_res = RSSQLDT_LONGVARBINARY;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_LONGVARBINARY:
            default:
                ss_dassert(sqldt1 == RSSQLDT_LONGVARBINARY);
                ss_dassert(sqldt2 == RSSQLDT_BINARY
                    ||     sqldt2 == RSSQLDT_VARBINARY
                    ||     sqldt2 == RSSQLDT_LONGVARBINARY)
                sqldt_res = RSSQLDT_LONGVARBINARY;
                len1 = RS_LENGTH_NULL;
                break;
        }
        if (len1 == RS_LENGTH_NULL || len2 == RS_LENGTH_NULL) {
            len_res = RS_LENGTH_NULL;
            sqldt_res = RSSQLDT_LONGVARBINARY;
        } else {
            len_res = SS_MAX(len1, len2);
        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, len_res, -1L);
        return (union_atype);
}

/* RSDT_UNICODE union RSDT_UNICODE */
static rs_atype_t* uni_uni(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        rs_sqldatatype_t sqldt_res;
        ulong len1;
        ulong len2;
        ulong len_res;
        rs_atype_t* union_atype;
       
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        len1 = RS_ATYPE_LENGTH(cd, atype1);
        len2 = RS_ATYPE_LENGTH(cd, atype2);
        switch (sqldt1) {
            case RSSQLDT_WCHAR:
                switch (sqldt2) {
                    case RSSQLDT_WCHAR:
                        sqldt_res = RSSQLDT_WCHAR;
                        break;
                    case RSSQLDT_WVARCHAR:
                        sqldt_res = RSSQLDT_WVARCHAR;
                        break;
                    case RSSQLDT_WLONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_WLONGVARCHAR);
                        sqldt_res = RSSQLDT_WLONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_WVARCHAR:
                switch (sqldt2) {
                    case RSSQLDT_WCHAR:
                    case RSSQLDT_WVARCHAR:
                        sqldt_res = RSSQLDT_WVARCHAR;
                        break;
                    case RSSQLDT_WLONGVARCHAR:
                    default:
                        ss_dassert(sqldt2 == RSSQLDT_WLONGVARCHAR);
                        sqldt_res = RSSQLDT_WLONGVARCHAR;
                        len1 = RS_LENGTH_NULL;
                        break;
                }
                break;
            case RSSQLDT_WLONGVARCHAR:
            default:
                ss_dassert(sqldt1 == RSSQLDT_WLONGVARCHAR);
                ss_dassert(sqldt2 == RSSQLDT_WCHAR
                    ||     sqldt2 == RSSQLDT_WVARCHAR
                    ||     sqldt2 == RSSQLDT_WLONGVARCHAR)
                sqldt_res = RSSQLDT_WLONGVARCHAR;
                len1 = RS_LENGTH_NULL;
                break;
        }
        if (len1 == RS_LENGTH_NULL || len2 == RS_LENGTH_NULL) {
            len_res = RS_LENGTH_NULL;
            sqldt_res = RSSQLDT_WLONGVARCHAR;
        } else {
            len_res = SS_MAX(len1, len2);
        }
        union_atype = rs_atype_initbysqldt(cd, sqldt_res, len_res, -1L);
        return (union_atype);
}

static atype_unionfunc_t* const union_matrix[RSDT_DIMENSION][RSDT_DIMENSION] =
{
               /*   RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE    RSDT_BIGINT*/
/* RSDT_CHAR    */ {char_char, ret_at1,     ret_at1,    ret_at1,    ret_at1,    ret_at1,    ill_union,  char_uni,       ret_at1  },
/* RSDT_INTEGER */ {INV_UNION, int_int,     ret_at2,    ret_at2,    ill_union,  ret_at2,    ill_union,  ret_at2,        int_int  },
/* RSDT_FLOAT   */ {INV_UNION, INV_UNION,   ret_at1,    ret_at2,    ill_union,  ret_dbl,    ill_union,  ret_at2,        ret_dbl  },
/* RSDT_DOUBLE  */ {INV_UNION, INV_UNION,   INV_UNION,  ret_at1,    ill_union,  ret_at1,    ill_union,  ret_at2,        ret_at1  },
/* RSDT_DATE    */ {INV_UNION, INV_UNION,   INV_UNION,  INV_UNION,  date_date,  ill_union,  ill_union,  ret_at2,        ill_union},
/* RSDT_DFLOAT  */ {INV_UNION, INV_UNION,   INV_UNION,  INV_UNION,  INV_UNION,  dfl_dfl,    ill_union,  ret_at2,        ret_dbl  },
/* RSDT_BINARY  */ {INV_UNION, INV_UNION,   INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,  bin_bin,    ill_union,      ill_union},
/* RSDT_UNICODE */ {INV_UNION, INV_UNION,   INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,  uni_uni,        ret_at1  },
/* RSDT_BIGINT  */ {INV_UNION, INV_UNION,   INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,  INV_UNION,      int_int  }
};


/*##**********************************************************************\
 * 
 *              rs_atype_union
 * 
 * Forms union type
 * 
 * Parameters : 
 * 
 *      cd - use
 *              client data
 *              
 *      atype1 - in, use
 *              attribute type 1
 *              
 *      atype2 - in, use
 *              attribute type 2
 *              
 *      p_errh - out, give
 *              in case of error and p_errh != NULL a pointer to newly
 *          allocated error object
 *              
 * Return value - give :
 *      pointer to newly allocated atype object or NULL in case of error
 * 
 * Comments :
 *      part of SQL function block
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_atype_t* rs_atype_union(
        void* cd,
        rs_atype_t* atype1,
        rs_atype_t* atype2,
        rs_err_t** p_errh)
{
        rs_datatype_t dt1;
        rs_datatype_t dt2;
        rs_atype_t* union_atype;

        dt1 = RS_ATYPE_DATATYPE(cd, atype1);
        dt2 = RS_ATYPE_DATATYPE(cd, atype2);
        if (dt1 > dt2) {
            ss_dassert(union_matrix[dt1][dt2] == INV_UNION);
            union_atype = (*union_matrix[dt2][dt1])(cd, atype2, atype1, p_errh);
        } else {
            union_atype = (*union_matrix[dt1][dt2])(cd, atype1, atype2, p_errh);
        }
        return (union_atype);
}


/*##**********************************************************************\
 * 
 *              rs_atype_givefullname
 * 
 * Gives a full SQL name for an atype
 * 
 * Parameters : 
 * 
 *      cd - use
 *              client data
 *              
 *      atype - in, use
 *              attribute type object
 *              
 * Return value - give :
 *      pointer into newly allocated string containing the name
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_atype_givefullname(
        void* cd,
        rs_atype_t* atype)
{
        rs_sqldatatype_t sqldt;
        char* type_name;
        char* printed_typename = NULL;
        char buf[48];
        ulong len;
        ulong scale;

        sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        type_name = rs_atype_name(cd, atype);
        len = RS_ATYPE_LENGTH(cd, atype);
        switch (sqldt) {
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WCHAR:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_BINARY:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_CHAR:
            case RSSQLDT_FLOAT:
print_no_scale:;
                SsSprintf(buf, "%.20s(%ld)", type_name, len);
                printed_typename = SsMemStrdup(buf);
                break;
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
                scale = rs_atype_scale(cd, atype);
                if (scale != (ulong)RS_SCALE_NULL) {
                    SsSprintf(buf, "%.20s(%ld,%ld)", type_name, len, scale);
                } else {
                    goto print_no_scale;
                }
                printed_typename = SsMemStrdup(buf);
                break;

            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_TINYINT:
            case RSSQLDT_BIGINT:
            case RSSQLDT_INTEGER:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_REAL:
            case RSSQLDT_DOUBLE:
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
                printed_typename = SsMemStrdup(type_name);
                break;
            default:
                ss_rc_error(sqldt);
        }
        return (printed_typename);
}

void rs_atype_outputfullname(
        void* cd,
        rs_atype_t* atype,
        void (*outputfun)(void*, void*),
        void *outputpar)
{
        char* par;

        par = rs_atype_givefullname(cd, atype);
        (*outputfun)(outputpar, par);
        SsMemFree(par);
}

