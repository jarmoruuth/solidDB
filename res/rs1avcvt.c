/*************************************************************************\
**  source       * rs1avcvt.c
**  directory    * res
**  description  * conversion routines for avals
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

#include <ssmath.h>

#include <ssc.h>
#include <ssstring.h>

#include <ssctype.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssdtoa.h>
#include <ssdtow.h>
#include <ssltoa.h>
#include <ssltow.h>
#include <sssprint.h>
#include <ssfloat.h>
#include <sstraph.h>
#include <ssscan.h>
#include <sswcs.h>
#include <ssint8.h>

#include <uti0va.h>
#include <uti0vcmp.h>
#include <uti0dyn.h>

#include <su0slike.h>
#include <su0bstre.h>
#include <su0icvt.h>

#include <dt0dfloa.h>
#include <dt0date.h>

#include "rs0types.h"
#include "rs0tnum.h"
#include "rs0error.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0cons.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"

#include "rs1aval.h"
#include "rs2avcvt.h"

typedef rs_avalret_t aval_assignfunc_t(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh);

static aval_assignfunc_t triv_asn;
static aval_assignfunc_t ill_asn;
static aval_assignfunc_t int_bin;
static aval_assignfunc_t char_char;
static aval_assignfunc_t char_int;
static aval_assignfunc_t int_int;
static aval_assignfunc_t char_flt;
static aval_assignfunc_t char_dbl;
static aval_assignfunc_t char_date;
static aval_assignfunc_t char_dfl;
static aval_assignfunc_t char_bin;
static aval_assignfunc_t char_uni;
static aval_assignfunc_t int_char;
static aval_assignfunc_t int_flt;
static aval_assignfunc_t int_dbl;
static aval_assignfunc_t int_date;
static aval_assignfunc_t int_dfl;
static aval_assignfunc_t int_uni;
static aval_assignfunc_t flt_char;
static aval_assignfunc_t flt_int;
static aval_assignfunc_t flt_dbl;
static aval_assignfunc_t flt_dfl;
static aval_assignfunc_t flt_uni;
static aval_assignfunc_t dbl_char;
static aval_assignfunc_t dbl_int;
static aval_assignfunc_t dbl_flt;
static aval_assignfunc_t dbl_dfl;
static aval_assignfunc_t dbl_uni;
static aval_assignfunc_t date_char;
static aval_assignfunc_t date_int;
static aval_assignfunc_t date_date;
static aval_assignfunc_t date_uni;
static aval_assignfunc_t dfl_char;
static aval_assignfunc_t dfl_int;
static aval_assignfunc_t dfl_flt;
static aval_assignfunc_t dfl_dbl;
static aval_assignfunc_t dfl_dfl;
static aval_assignfunc_t dfl_uni;
static aval_assignfunc_t bin_char;
static aval_assignfunc_t bin_uni;
static aval_assignfunc_t uni_char;
static aval_assignfunc_t uniFchar;
static aval_assignfunc_t uni_int;
static aval_assignfunc_t uni_flt;
static aval_assignfunc_t uni_dbl;
static aval_assignfunc_t uni_date;
static aval_assignfunc_t uni_dfl;
static aval_assignfunc_t uni_bin;
static aval_assignfunc_t uni_uni;
static aval_assignfunc_t int_int8;
static aval_assignfunc_t int8_int;
static aval_assignfunc_t int8_flt;
static aval_assignfunc_t int8_dbl;
static aval_assignfunc_t int8_dfl;
static aval_assignfunc_t int8_uni;
static aval_assignfunc_t char_int8;
static aval_assignfunc_t flt_int8;
static aval_assignfunc_t dbl_int8;
static aval_assignfunc_t dfl_int8;
static aval_assignfunc_t uni_int8;

static aval_assignfunc_t * assign_matrix[RSDT_DIMENSION][RSDT_DIMENSION] =
{
               /*   RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
/* RSDT_CHAR    */ {char_char,  char_int,   char_flt,   char_dbl,   char_date,  char_dfl,   char_char,  char_uni,    char_int8},
/* RSDT_INTEGER */ {int_char,   int_int,    int_flt,    int_dbl,    ill_asn,    int_dfl,    int_bin,    int_uni,     int_int8},
/* RSDT_FLOAT   */ {flt_char,   flt_int,    triv_asn,   flt_dbl,    ill_asn,    flt_dfl,    ill_asn,    flt_uni,     flt_int8},
/* RSDT_DOUBLE  */ {dbl_char,   dbl_int,    dbl_flt,    triv_asn,   ill_asn,    dbl_dfl,    ill_asn,    dbl_uni,     dbl_int8},
/* RSDT_DATE    */ {date_char,  ill_asn,    ill_asn,    ill_asn,    date_date,  ill_asn,    ill_asn,    date_uni,    ill_asn},
/* RSDT_DFLOAT  */ {dfl_char,   dfl_int,    dfl_flt,    dfl_dbl,    ill_asn,    dfl_dfl,    ill_asn,    dfl_uni,     dfl_int8},
/* RSDT_BINARY  */ {char_char,  ill_asn,    ill_asn,    ill_asn,    ill_asn,    ill_asn,    char_char,  ill_asn,     ill_asn},
/* RSDT_UNICODE */ {uni_char,   uni_int,    uni_flt,    uni_dbl,    uni_date,   uni_dfl,    ill_asn,    uni_uni,     uni_int8},
/* RSDT_BIGINT  */ {int_char,   int8_int,   int8_flt,   int8_dbl,   ill_asn,    int8_dfl,   int_bin,    int8_uni,    triv_asn}
};

static aval_assignfunc_t* const convert_matrix[RSDT_DIMENSION][RSDT_DIMENSION] =
{
               /*   RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
/* RSDT_CHAR    */ {char_char,  char_int,   char_flt,   char_dbl,   char_date,  char_dfl,   char_bin,   char_uni,    char_int8},
/* RSDT_INTEGER */ {int_char,   int_int,    int_flt,    int_dbl,    int_date,   int_dfl,    int_bin,    int_uni,     int_int8},
/* RSDT_FLOAT   */ {flt_char,   flt_int,    triv_asn,   flt_dbl,    ill_asn,    flt_dfl,    ill_asn,    flt_uni,     flt_int8},
/* RSDT_DOUBLE  */ {dbl_char,   dbl_int,    dbl_flt,    triv_asn,   ill_asn,    dbl_dfl,    ill_asn,    dbl_uni,     dbl_int8},
/* RSDT_DATE    */ {date_char,  date_int,   ill_asn,    ill_asn,    date_date,  ill_asn,    ill_asn,    date_uni,    date_int},
/* RSDT_DFLOAT  */ {dfl_char,   dfl_int,    dfl_flt,    dfl_dbl,    ill_asn,    dfl_dfl,    ill_asn,    dfl_uni,     dfl_int8},
/* RSDT_BINARY  */ {bin_char,   ill_asn,    ill_asn,    ill_asn,    ill_asn,    ill_asn,    char_char,  bin_uni,     ill_asn},
/* RSDT_UNICODE */ {uniFchar,   uni_int,    uni_flt,    uni_dbl,    uni_date,   uni_dfl,    uni_bin,    uni_uni,     uni_int8},
/* RSDT_BIGINT  */ {int_char,   int8_int,   int8_flt,   int8_dbl,   int_date,   int8_dfl,   int_bin,    int8_uni,    triv_asn}
};

/*************************************************************************\
 * The conversion interface
\*************************************************************************/

#ifdef SS_DEBUG

void rs_aval_notnullviolation(ss_char1_t* funcname)
{
        ss_dprintf_1((
            "%s: NULL set to NOT NULL column\n",
            funcname));
}

#endif /* SS_DEBUG */

void rs_aval_setchar2binassignallowed(bool allowed)
{
        assign_matrix[RSDT_CHAR][RSDT_BINARY] =
            assign_matrix[RSDT_BINARY][RSDT_CHAR] =
                (allowed ? char_char : ill_asn);
}

bool rs_atype_sqldtcanbeconvertedto(
        void* cd __attribute__ ((unused)),
        rs_sqldatatype_t sqldt_from,
        size_t* p_ntypes,
        rs_sqldatatype_t p_sqldt_to[RS_SQLDATATYPES_COUNT])
{
        size_t i;
        size_t j;
        rs_sqldatatype_t dt_from;
        static const rs_sqldatatype_t sqldt_array[] = {
            RSSQLDT_WLONGVARCHAR, RSSQLDT_WVARCHAR, RSSQLDT_WCHAR,
            RSSQLDT_BIT,          RSSQLDT_TINYINT,  RSSQLDT_BIGINT,
            RSSQLDT_LONGVARBINARY,RSSQLDT_VARBINARY,RSSQLDT_BINARY,
            RSSQLDT_LONGVARCHAR,  RSSQLDT_CHAR,     RSSQLDT_NUMERIC,
            RSSQLDT_DECIMAL,      RSSQLDT_INTEGER,  RSSQLDT_SMALLINT,
            RSSQLDT_FLOAT,        RSSQLDT_REAL,     RSSQLDT_DOUBLE,
            RSSQLDT_DATE,         RSSQLDT_TIME,     RSSQLDT_TIMESTAMP,
            RSSQLDT_VARCHAR
        };

        if (!RS_ATYPE_ISLEGALSQLDT(cd, sqldt_from)) {
            *p_ntypes = 0;
            return (FALSE);
        }
        ss_dassert(RS_SQLDATATYPES_COUNT == sizeof(sqldt_array) / sizeof(sqldt_array[0]));
        dt_from = RS_ATYPE_SQLDTTODT(cd, sqldt_from);
        for (j = 0, i = 0; i < RS_SQLDATATYPES_COUNT; i++) {
            rs_datatype_t dt_to;
            rs_sqldatatype_t sqldt_to;

            sqldt_to = sqldt_array[i];
            if (RS_ATYPE_ISLEGALSQLDT(cd, sqldt_to)) {
                dt_to = RS_ATYPE_SQLDTTODT(cd, sqldt_to);
                if (convert_matrix[dt_from][dt_to] != ill_asn) {
                    if (dt_from == RSDT_DATE && dt_to == RSDT_DATE) {

                        if ((sqldt_from == RSSQLDT_DATE && sqldt_to == RSSQLDT_TIME)
                        ||  (sqldt_from == RSSQLDT_TIME && sqldt_to == RSSQLDT_DATE))
                        {
                            continue;
                        }
                    }
                    p_sqldt_to[j++] = sqldt_to;
                }
            }
        }
        *p_ntypes = j;
        return (TRUE);
}

/*##**********************************************************************\
 *
 *		rs_atype_assignpos
 *
 * Checks if assignment between the specified types is possible.
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	dst_atype - in, use
 *		destination attribute type
 *
 *	src_atype - in, use
 *		source attribute type
 *
 * Return value :
 *      TRUE - assignment is possible (run time errors still possible)
 *      FALSE - assignment is illegal (always)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_atype_assignpos(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype,
        rs_atype_t* src_atype)
{
        rs_datatype_t dst_dt;
        rs_datatype_t src_dt;

        dst_dt = RS_ATYPE_DATATYPE(cd, dst_atype);
        src_dt = RS_ATYPE_DATATYPE(cd, src_atype);
        if (assign_matrix[src_dt][dst_dt] != ill_asn) {
            if (src_dt == RSDT_DATE && dst_dt == RSDT_DATE) {
                rs_sqldatatype_t sqldt1;
                rs_sqldatatype_t sqldt2;

                sqldt1 = RS_ATYPE_SQLDATATYPE(cd, src_atype);
                sqldt2 = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
                if ((sqldt1 == RSSQLDT_DATE && sqldt2 == RSSQLDT_TIME)
                ||  (sqldt1 == RSSQLDT_TIME && sqldt2 == RSSQLDT_DATE))
                {
                    return (FALSE);
                }

            }
            return (TRUE);
        }
        return (FALSE);
}

/*##**********************************************************************\
 *
 *		rs_atype_convertpos
 *
 * Checks if explicit type conversion between the specified types is
 * possible.
 *
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	dst_atype - in, use
 *		destination data type
 *
 *	src_atype - in, use
 *		src data type
 *
 * Return value :
 *      TRUE - conversion is possible (run time errors still possible)
 *      FALSE - conversion is illegal (always)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_atype_convertpos(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype,
        rs_atype_t* src_atype)
{
        rs_datatype_t dst_dt;
        rs_datatype_t src_dt;

        dst_dt = RS_ATYPE_DATATYPE(cd, dst_atype);
        src_dt = RS_ATYPE_DATATYPE(cd, src_atype);
        if (convert_matrix[src_dt][dst_dt] != ill_asn) {
            if (src_dt == RSDT_DATE && dst_dt == RSDT_DATE) {
                rs_sqldatatype_t sqldt1;
                rs_sqldatatype_t sqldt2;

                sqldt1 = RS_ATYPE_SQLDATATYPE(cd, src_atype);
                sqldt2 = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
                if ((sqldt1 == RSSQLDT_DATE && sqldt2 == RSSQLDT_TIME)
                ||  (sqldt1 == RSSQLDT_TIME && sqldt2 == RSSQLDT_DATE))
                {
                    return (FALSE);
                }

            }
            return (TRUE);
        }
        return (FALSE);
}



/*##**********************************************************************\
 *
 *		rs_aval_convert_ext
 *
 * Explicit type conversion routine
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	dst_atype - in, use
 *		destination type
 *
 *	dst_aval - use
 *		destination value
 *
 *	src_atype - in, use
 *		source type
 *
 *	src_aval - in, use
 *		source value
 *
 *	p_errh - out, give
 *		if return value is RSAVR_FAILURE a pointer into a newly
 *          allocated error handle, or NULL if no error handle wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - Success
 *      RSAVR_TRUNCATION - Success with info (truncation of value)
 *      RSAVR_FAILURE - Failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_convert_ext(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_datatype_t dst_dt;
        rs_datatype_t src_dt;
        rs_avalret_t retc;

        if (RS_AVAL_ISNULL(cd, src_atype, src_aval)) {
            ss_debug(
                if (!rs_atype_nullallowed(cd, dst_atype)) {
                    rs_aval_notnullviolation((char *)"rs_aval_convert_ext");
                });
            rs_aval_setnull(cd, dst_atype, dst_aval);
            return (RSAVR_SUCCESS);
        }
        dst_dt = RS_ATYPE_DATATYPE(cd, dst_atype);
        src_dt = RS_ATYPE_DATATYPE(cd, src_atype);
        retc = (*convert_matrix[src_dt][dst_dt])(
                    cd,
                    dst_atype, dst_aval,
                    src_atype, src_aval,
                    p_errh);
        return ((RS_AVALRET_T)retc);
}

/*##**********************************************************************\
 *
 *		rs_aval_assign_ext
 *
 * Assignment routine
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	dst_atype - in, use
 *		destination type
 *
 *	dst_aval - use
 *		destination value
 *
 *	src_atype - in, use
 *		source type
 *
 *	src_aval - in, use
 *		source value
 *
 *	p_errh - out, give
 *		if return value is RSAVR_FAILURE a pointer into a newly
 *          allocated error handle, or NULL if no error handle wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - Success
 *      RSAVR_TRUNCATION - Success with info (truncation of value)
 *      RSAVR_FAILURE - Failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_assign_ext(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_datatype_t dst_dt;
        rs_datatype_t src_dt;
        rs_avalret_t retc;
        su_bflag_t null_test;

        ss_dprintf_1(("rs_aval_assign_ext(dst_aval=0x%08lX,src_aval=0x%08lX)\n",
                      (ulong)dst_aval, (ulong)src_aval));

        if (p_errh != NULL) {
            /* We are not quite compatible with function definition
             * because we do not return error object in case of
             * truncation warning. So we set the error object to NULL.
             */
            *p_errh = NULL;
        }

        if ((null_test = src_aval->ra_flags & (RA_NULL | RA_UNKNOWN)) != 0) {
            ss_debug(
                if (!rs_atype_nullallowed(cd, dst_atype)) {
                    rs_aval_notnullviolation((char *)"rs_aval_assign_ext");
                });
            rs_aval_setnull(cd, dst_atype, dst_aval);
            SU_BFLAG_SET(dst_aval->ra_flags, null_test);
            return (RSAVR_SUCCESS);
        }
        dst_dt = RS_ATYPE_DATATYPE(cd, dst_atype);
        src_dt = RS_ATYPE_DATATYPE(cd, src_atype);
        retc = (*assign_matrix[src_dt][dst_dt])(
                    cd,
                    dst_atype, dst_aval,
                    src_atype, src_aval,
                    p_errh);
        return ((RS_AVALRET_T)retc);
}

/*##**********************************************************************\
 *
 *		rs_aval_sql_assign
 *
 * Assignment routine, member of SQL function block
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	dst_atype - in, use
 *		destination type
 *
 *	dst_aval - use
 *		destination value
 *
 *	src_atype - in, use
 *		source type
 *
 *	src_aval - in, use
 *		source value
 *
 *      p_errh - out, give
 *          if return value is RSAVR_FAILURE or RSAVR_TRUNCATION
 *          a pointer into a newly
 *          allocated error handle, or NULL if no error handle wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - Success
 *      RSAVR_TRUNCATION - Success with info (truncation of value)
 *
 *      RSAVR_FAILURE - Failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_sql_assign(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc;

        retc = rs_aval_assign_ext(cd,
                                  dst_atype, dst_aval,
                                  src_atype, src_aval,
                                  p_errh);
        if (retc == RSAVR_TRUNCATION) {
            uint code;
            if (rs_atype_isnum(cd, dst_atype)) {
                code = RS_WARN_NUMERICTRUNC_SS;
            } else {
                code = RS_WARN_STRINGTRUNC_SS;
            }
            rs_error_create(p_errh, code,
                            rs_atype_name(cd, src_atype),
                            rs_atype_name(cd, dst_atype));
        }
        return ((RS_AVALRET_T)retc);
}

/*************************************************************************\
 * The conversion functions
\*************************************************************************/

/*#***********************************************************************\
 *
 *		triv_asn
 *
 * Trivial conversion (no conversion needed)
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
 *
 *
 * Return value :
 *      TRUE
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_avalret_t triv_asn(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype __attribute__ ((unused)),
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh __attribute__ ((unused)))
{
        RS_AVAL_ASSIGNBUF(cd, dst_atype, dst_aval, src_aval);
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		ill_asn
 *
 * Illegal conversion
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
 *
 *
 * Return value :
 *      FALSE
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static rs_avalret_t ill_asn(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval __attribute__ ((unused)),
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval __attribute__ ((unused)),
        rs_err_t**  p_errh)
{
        ss_dprintf_2(("%s: rs_aval_assign: illegal.\n", __FILE__));
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

static rs_avalret_t int_bin(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_tuplenum_t tnum;
        rs_datatype_t dt;
        long l;
        ss_int8_t i8;
        ulong type_len;
        bool succp;

        ss_dprintf_2(("%s: rs_aval_assign:int_bin\n", __FILE__));

        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        succp = !(type_len < 2*sizeof(long));

        if (succp) {
            dt = RS_ATYPE_DATATYPE(cd, src_atype);
            switch (dt) {
                case RSDT_INTEGER:
                    l = rs_aval_getlong(cd, src_atype, src_aval);
                    rs_tuplenum_ulonginit(&tnum, 0L, l);
                    break;
                /* Pete doesn't like this. He wants this to an own function. tommiv 12.2.2002 */
                case RSDT_BIGINT:
                default:
                    ss_rc_dassert(dt == RSDT_BIGINT, dt);
                    i8 = rs_aval_getint8(cd, src_atype, src_aval);
                    rs_tuplenum_int8init(&tnum, i8);
                    break;
            }
            succp = rs_tuplenum_setintoaval(&tnum, cd, dst_atype, dst_aval);
        }
        if (!succp) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            ss_dprintf_2(("%s: rs_aval_assign:int_bin:RSAVR_FAILURE\n", __FILE__));
            return (RSAVR_FAILURE);
        }
        ss_dprintf_2(("%s: rs_aval_assign:int_bin:RSAVR_SUCCESS\n", __FILE__));
        return (RSAVR_SUCCESS);
}

static rs_avalret_t char_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        va_index_t src_datalen;
        ss_char1_t* p_src_data;
        ulong type_len;

        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        ss_dassert(!SU_BFLAG_TEST(src_aval->ra_flags, RA_ONLYCONVERTED));
        if (type_len > 0) {
            p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
            src_datalen--;

            if (type_len < src_datalen) {
                if (rs_aval_isblob(cd, src_atype, src_aval)) {
                    bool succp =
                        rs_aval_loadblob(cd,
                                         src_atype, src_aval,
                                         rs_aval_getloadblobsizelimit(cd));
                    if (!succp) {
                        rs_error_create(
                                p_errh,
                                E_ILLASSIGN_SS,
                                rs_atype_name(cd, src_atype),
                                rs_atype_name(cd, dst_atype));
                        return (RSAVR_FAILURE);
                    }
                }
                src_datalen = type_len;
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                if (SU_BFLAG_TEST(dst_aval->ra_flags, RA_VTPLREF | RA_FLATVA | RA_NULL))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                               RA_ONLYCONVERTED | RA_FLATVA | RA_UNKNOWN);
                AVAL_SETDATAANDNULL(
                        cd,
                        dst_atype,
                        dst_aval,
                        p_src_data,
                        src_datalen);
                return (RSAVR_TRUNCATION);
            }
        }
        RS_AVAL_ASSIGNBUF(cd, dst_atype, dst_aval, src_aval);
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		char_int
 *
 * RSDT_CHAR -> RSDT_INTEGER
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        long l;
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        rs_avalret_t retc;
        bool succp;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanLong(s, &l, &mismatch);
        if (succp) {
            succp = (*mismatch == '\0' || ss_isspace(*mismatch));
            if (succp) {
                retc = rs_aval_putlong(cd, dst_atype, dst_aval, l);
                if (retc != RSAVR_FAILURE) {
                    return (retc);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		char_int8
 *
 * RSDT_CHAR -> RSDT_BIGINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_int8_t i8;
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        rs_avalret_t retc;
        bool succp;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanInt8(s, &i8, &mismatch);
        if (succp) {
            succp = (*mismatch == '\0' || ss_isspace(*mismatch));
            if (succp) {
                retc = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
                if (retc != RSAVR_FAILURE) {
                    return (retc);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		char_flt
 *
 * RSDT_CHAR -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_flt(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanDouble(s, &d, &mismatch);
        if (succp) {
            succp = (*mismatch == '\0' || ss_isspace(*mismatch));
            if (succp) {
                succp = SS_FLOAT_IS_PORTABLE(d);
                if (succp) {
                    float f;
                    f = (float)d;
                    if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                      RA_VTPLREF |
                                      RA_FLATVA |
                                      RA_ONLYCONVERTED |
                                      RA_NULL))
                    {
                        dst_aval->ra_va = NULL;
                    } else {
                        ss_derror; /* va should never be
                                      dynamic for RSDT_FLOAT */
                        refdva_free(&dst_aval->ra_va);
                    }
                    SU_BFLAG_CLEAR(dst_aval->ra_flags,
                                   RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                    SU_BFLAG_SET(dst_aval->ra_flags,
                                 RA_CONVERTED | RA_ONLYCONVERTED);
                    dst_aval->ra_.f = f;
                    return (RSAVR_SUCCESS);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		char_dbl
 *
 * RSDT_CHAR -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_dbl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanDouble(s, &d, &mismatch);
        if (succp) {
            succp = (*mismatch == '\0' || ss_isspace(*mismatch));
            if (succp) {
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                    RA_VTPLREF |
                                    RA_FLATVA |
                                    RA_ONLYCONVERTED |
                                    RA_NULL))
                {
                    dst_aval->ra_va = NULL;
                } else {
                    ss_derror; /* va should never be
                                    dynamic for RSDT_DOUBLE! */
                    refdva_free(&dst_aval->ra_va);
                }
                ss_dassert(dst_aval->ra_va == NULL);
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                                RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(dst_aval->ra_flags,
                                RA_CONVERTED | RA_ONLYCONVERTED);
                dst_aval->ra_.d = d;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

static rs_avalret_t char_date(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t* s;
        va_index_t datalen;
        rs_avalret_t retc;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);

        retc = rs_aval_putchartodate(cd, dst_atype, dst_aval, s);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        ss_dassert(retc == RSAVR_FAILURE);
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		char_dfl
 *
 * RSDT_CHAR -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        dt_dfl_t dfl;
        ss_char1_t* s;
        va_index_t datalen;
        rs_avalret_t retc;
        bool succp;

        s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = dt_dfl_setasciiz(&dfl, s);
        if (succp) {
            retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		char_bin
 *
 * RSDT_CHAR -> RSDT_BINARY
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_bin(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t* s;
        va_index_t datalen;
        bool succp;
        rs_avalret_t retc;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }
        if (succp) {
            s = va_getdata(src_aval->RA_RDVA, &datalen);
            ss_dassert(datalen > 0);
            if (!(datalen & 1)) {
                succp = FALSE;
            }
        }
        if (succp) {
            ulong len;
            ss_byte_t* destdata;
            va_index_t destlen;
            size_t destlen1;

            retc = RSAVR_SUCCESS;
            len = RS_ATYPE_LENGTH(cd, dst_atype);
            if (len < datalen / 2 && len > 0) {
                retc = RSAVR_TRUNCATION;
                datalen = len * 2 + 1;
            }
            RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
            ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_VTPLREF | RA_FLATVA | RA_NULL))
            {
                dst_aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
            destlen1 = (datalen / 2) + 1;
            AVAL_SETDATA(cd, dst_atype, dst_aval, NULL, destlen1);
            destdata = va_getdata(dst_aval->RA_RDVA, &destlen);
            ss_dassert(destlen >= 1);
            ss_dassert(destlen1 == destlen);
            destdata[destlen-1] = '\0';
            if (destlen > 1) {
                succp = su_chcvt_hex2bin(destdata, s, destlen - 1);
            }
            if (succp) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		char_uni
 *
 * RSDT_CHAR -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t char_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ulong len;
        ss_char1_t* p_data;
        va_index_t datalen;
        rs_avalret_t retc;
        bool succp;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }
        if (!succp) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        retc = RSAVR_SUCCESS;
        len = RS_ATYPE_LENGTH(cd, dst_atype);
        p_data = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen >= 1);
        datalen--;
        if (len < datalen && len > 0) {
            datalen = len;
            retc = RSAVR_TRUNCATION;
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_FLATVA |
                          RA_NULL))
        {
            dst_aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
            RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
            RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATACHAR1TO2(cd, dst_atype, dst_aval, p_data, datalen);
        return (retc);
}


/*#***********************************************************************\
 *
 *		int_char
 *
 * RSDT_INTEGER -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t buf[24];
        rs_datatype_t dt;
        ss_int8_t i8;
        long l;
        size_t len;
        ulong type_len;

        dt = RS_ATYPE_DATATYPE(cd, src_atype);
        switch (dt) {
            case RSDT_INTEGER:
                l = rs_aval_getlong(cd, src_atype, src_aval);
                len = SsLongToAscii(l, buf, 10, 0, '0', TRUE);
                break;
            case RSDT_BIGINT:
            default:
                ss_rc_dassert(dt == RSDT_BIGINT, dt);
                i8 = rs_aval_getint8(cd, src_atype, src_aval);
                len = SsInt8ToAscii(i8, buf, 10, 0, '0', TRUE);
                break;
        }
        ss_dassert(len < sizeof(buf)/sizeof(buf[0]));
        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        if (type_len < len && type_len > 0) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            dst_aval->RA_RDVA = NULL;
        }
        len += 1;
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        AVAL_SETDATA(cd, dst_atype, dst_aval, buf, len);
        return (RSAVR_SUCCESS);
}


/*#***********************************************************************\
 *
 *		int_int
 *
 * INTEGER -> INTEGER
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc;
        rs_sqldatatype_t src_sqldt;
        rs_sqldatatype_t dst_sqldt;

        dst_sqldt = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
        src_sqldt = RS_ATYPE_SQLDATATYPE(cd, src_atype);
        if (dst_sqldt == src_sqldt) {
            RS_AVAL_ASSIGNBUF(cd, dst_atype, dst_aval, src_aval);
            return (RSAVR_SUCCESS);
        } else {
            long l;

            l = rs_aval_getlong(cd, src_atype, src_aval);
            retc = rs_aval_putlong(cd, dst_atype, dst_aval, l);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
}

/*#***********************************************************************\
 *
 *		int_int8
 *
 * INTEGER/SMALLINT/TINYINT -> BIGINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc;
        ss_int8_t i8;
        long l;

        l = rs_aval_getlong(cd, src_atype, src_aval);
        ss_dassert(rs_atype_datatype(cd, dst_atype) == RSDT_BIGINT);
        ss_dassert(rs_atype_datatype(cd, src_atype) == RSDT_INTEGER);
        SsInt8SetInt4(&i8, (ss_int4_t)l);
        retc = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                    p_errh,
                    E_ILLASSIGN_SS,
                    rs_atype_name(cd, src_atype),
                    rs_atype_name(cd, dst_atype));
        }
        return (retc);
}

/*#***********************************************************************\
 *
 *		int8_int
 *
 * BIGINT -> INTEGER/SMALLINT/TINYINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int8_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc;
        ss_int8_t i8;
        ss_int4_t i4;
        ss_dassert(rs_atype_datatype(cd, src_atype) == RSDT_BIGINT);
        ss_dassert(rs_atype_datatype(cd, dst_atype) == RSDT_INTEGER);

        i8 = rs_aval_getint8(cd, src_atype, src_aval);
        if (SsInt8ConvertToInt4(&i4, i8)) {
            retc = rs_aval_putlong(cd, dst_atype, dst_aval, (long)i4);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		int_flt
 *
 * RSDT_INTEGER -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_flt(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype __attribute__ ((unused)),
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype __attribute__ ((unused)),
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh __attribute__ ((unused)))
{
        long l;
        float f;

        l = rs_aval_getlong(cd, src_atype, src_aval);
        f = (float)l;
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_FLATVA |
                          RA_ONLYCONVERTED |
                          RA_NULL)) {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* Should never bee dynamically allocated
                          for RSDT_FLOAT! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.f = f;
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		int8_flt
 *
 * RSDT_BIGINT -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int8_flt(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_int8_t i8;
        double d;
        float f;
        bool succ;

        i8 = rs_aval_getint8(cd, src_atype, src_aval);
        succ = SsInt8ConvertToDouble(&d, i8);
        if (succ) {
            succ = SS_FLOAT_IS_PORTABLE(d);
        }

        if (!succ) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        f = (float)d;
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_FLATVA |
                          RA_ONLYCONVERTED |
                          RA_NULL)) {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* should never be dynamically allocated for RSDT_FLOAT! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.f = f;
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		int_dbl
 *
 * RSDT_INTEGER -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_dbl(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype __attribute__ ((unused)),
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype __attribute__ ((unused)),
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh __attribute__ ((unused)))
{
        long l;
        double d;

        l = rs_aval_getlong(cd, src_atype, src_aval);
        d = (double)l;
        if (SU_BFLAG_TEST(dst_aval->ra_flags, RA_VTPLREF | RA_FLATVA | RA_ONLYCONVERTED | RA_NULL)) {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* should never be dynamically allocated
                          for RSDT_DOUBLE */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		int8_dbl
 *
 * RSDT_BIGINT -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int8_dbl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_int8_t i8;
        double d;

        i8 = rs_aval_getint8(cd, src_atype, src_aval);
        if (!SsInt8ConvertToDouble(&d, i8)) {
            ss_derror;
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_FLATVA |
                          RA_ONLYCONVERTED |
                          RA_NULL))
        {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* should never be dynamically allocated
                          for RSDT_DOUBLE! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		int_date
 *
 * RSDT_INTEGER -> RSDT_DATE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_date(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc = RSAVR_SUCCESS;
        bool succp;
        SsTimeT t = 0;
        dt_date_t tmp_date;

        switch (RS_ATYPE_DATATYPE(cd, src_atype)) {
            case RSDT_INTEGER:
                t = (SsTimeT)rs_aval_getlong(cd, src_atype, src_aval);
                break;
            case RSDT_BIGINT: {
                ss_int8_t i8;
                ss_int4_t i4;
                i8 = rs_aval_getint8(cd, src_atype, src_aval);
                if (!SsInt8ConvertToInt4(&i4, i8)) {
                    rs_error_create(
                        p_errh,
                        E_ILLASSIGN_SS,
                        rs_atype_name(cd, src_atype),
                        rs_atype_name(cd, dst_atype));
                    return (RSAVR_FAILURE);
                }
                t = (SsTimeT) i4;
                break;
            }
            default:
                ss_rc_derror(RS_ATYPE_DATATYPE(cd, src_atype));
        }
        succp = dt_date_settimet_raw(&tmp_date, t);
        if (succp) {
            switch (RS_ATYPE_SQLDATATYPE(cd, dst_atype)) {
                case RSSQLDT_TIMESTAMP:
                    break;
                case RSSQLDT_DATE:
                    retc = RSAVR_TRUNCATION;
                    succp = dt_date_truncatetodate(&tmp_date);
                    break;
                case RSSQLDT_TIME:
                    retc = RSAVR_TRUNCATION;
                    succp = dt_date_truncatetotime(&tmp_date);
                    break;
                default:
                    ss_error;
            }
            if (succp) {
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    dst_aval->ra_va = NULL;
                } else {
                    ss_derror; /* should never have dynamic va for RSDT_DATE */
                    refdva_free(&dst_aval->ra_va);
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                    RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_UNKNOWN);
                dt_date_datetova(&tmp_date, &dst_aval->ra_vabuf.va);
                dst_aval->ra_va = &dst_aval->ra_vabuf.va;
                SU_BFLAG_SET(dst_aval->ra_flags, RA_FLATVA);

                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		int_dfl
 *
 * RSDT_INTEGER -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        rs_avalret_t retc;
        long l;
        dt_dfl_t dfl;

        l = rs_aval_getlong(cd, src_atype, src_aval);
        succp = dt_dfl_setlong(&dfl, l);
        if (succp) {
            retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		int8_dfl
 *
 * RSDT_BIGINT -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int8_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t tmp_buf[24];
        dt_dfl_t dfl;
        ss_int8_t i8;
        rs_avalret_t retc;

        i8 = rs_aval_getint8(cd, src_atype, src_aval);
        SsInt8ToAscii(i8, tmp_buf, 10, 0, '0', TRUE);
        if (dt_dfl_setasciiz(&dfl, tmp_buf)) {
            retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		int_uni
 *
 * RSDT_INTEGER -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char2_t buf[24];
        long l;
        size_t len;
        ulong type_len;

        l = rs_aval_getlong(cd, src_atype, src_aval);
        len = SsLongToWcs(l, buf, 10, 0, '0', TRUE);
        ss_dassert(len < sizeof(buf)/sizeof(buf[0]));
        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        if (type_len < len && type_len > 0) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            dst_aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
            RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATACHAR2(cd, dst_atype, dst_aval, buf, len);
        return (RSAVR_SUCCESS);
}


/*#***********************************************************************\
 *
 *		int8_uni
 *
 * RSDT_BIGINT -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t int8_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t str[24];
        ss_int8_t i8;
        size_t len;
        ulong type_len;

        i8 = rs_aval_getint8(cd, src_atype, src_aval);
        len = SsInt8ToAscii(i8, str, 10, 0, '0', TRUE);
        ss_dassert(len < sizeof(str)/sizeof(str[0]));
        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        if (type_len < len && type_len > 0) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            dst_aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
            RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATACHAR1TO2(cd, dst_atype, dst_aval, str, len);
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		flt_char
 *
 * RSDT_FLOAT -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        rs_avalret_t retc;

        d = (double)rs_aval_getfloat(cd, src_atype, src_aval);
        retc = rs_aval_putdbltochar(cd, dst_atype, dst_aval, d, RS_REAL_DECPREC + 1);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
        }
        return (retc);
}

/*#***********************************************************************\
 *
 *		flt_int
 *
 * RSDT_FLOAT -> RSDT_INTEGER (truncate toward zero)
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        float f;
        long l;

        f = rs_aval_getfloat(cd, src_atype, src_aval);
        if (f > ((float)SS_INT4_MIN - (float)1)
        &&  f < ((float)SS_INT4_MAX + (float)1))
        {
            rs_avalret_t retc = RSAVR_SUCCESS;
            rs_avalret_t retc2;

            l = (long)f;
            if ((float)l != f) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putlong(cd, dst_atype, dst_aval, l);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		flt_int8
 *
 * RSDT_FLOAT -> RSDT_BIGINT (truncate toward zero)
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        ss_int8_t i8;

        d = (double)rs_aval_getfloat(cd, src_atype, src_aval);

        if (SsInt8SetDouble(&i8, d)) {
            rs_avalret_t retc = RSAVR_SUCCESS;
            rs_avalret_t retc2;
            double d2;

            SsInt8ConvertToDouble(&d2, i8);
            if (d != d2) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		flt_dbl
 *
 * RSDT_FLOAT -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_dbl(
        void*       cd,
        rs_atype_t* dst_atype __attribute__ ((unused)),
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh __attribute__ ((unused)))
{
        float f;
        double d;

        f = rs_aval_getfloat(cd, src_atype, src_aval);
        d = (double)f;
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_FLATVA |
                          RA_ONLYCONVERTED |
                          RA_NULL))
        {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* should never be dynamically allocated
                          for RSDT_DOUBLE! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}


/*#***********************************************************************\
 *
 *		flt_dfl
 *
 * RSDT_FLOAT -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        rs_avalret_t retc;
        double d;
        dt_dfl_t dfl;

        d = (double)rs_aval_getfloat(cd, src_atype, src_aval);
        succp = dt_dfl_setdouble(&dfl, d);
        if (succp) {
            retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		flt_uni
 *
 * RSDT_FLOAT -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t flt_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        rs_avalret_t retc;

        d = (double)rs_aval_getfloat(cd, src_atype, src_aval);
        retc = rs_aval_putdbltouni(cd, dst_atype, dst_aval, d, RS_REAL_DECPREC + 1);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
        }
        return (retc);
}

/*#***********************************************************************\
 *
 *		dbl_char
 *
 * RSDT_DOUBLE -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        rs_avalret_t retc;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        retc = rs_aval_putdbltochar(cd, dst_atype, dst_aval, d, RS_DOUBLE_DECPREC + 1);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
        }
        return (retc);
}


/*#***********************************************************************\
 *
 *		dbl_int
 *
 * RSDT_DOUBLE -> RSDT_INTEGER
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        long l;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        if (d > ((double)SS_INT4_MIN - (double)1)
        &&  d < ((double)SS_INT4_MAX + (double)1))
        {
            rs_avalret_t retc = RSAVR_SUCCESS;
            rs_avalret_t retc2;
            l = (long)d;
            if ((double)l != d) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putlong(cd, dst_atype, dst_aval, l);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dbl_int8
 *
 * RSDT_DOUBLE -> RSDT_BIGINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_int8_t i8;
        double d;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        if (SsInt8SetDouble(&i8, d)) {
            rs_avalret_t retc = RSAVR_SUCCESS;
            rs_avalret_t retc2;
            double d2;

            SsInt8ConvertToDouble(&d2, i8);
            if (d != d2) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dbl_flt
 *
 * RSDT_DOUBLE -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_flt(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        float f;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        if (!SS_FLOAT_IS_PORTABLE(d)) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        f = (float)d;
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA |
                          RA_ONLYCONVERTED))
        {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic with RSDT_FLOAT! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.f = f;
        return (RSAVR_SUCCESS);
}

/*#***********************************************************************\
 *
 *		dbl_dfl
 *
 * RSDT_DOUBLE -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        dt_dfl_t dfl;
        bool succp;
        rs_avalret_t retc;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        succp = dt_dfl_setdouble(&dfl, d);
        if (succp) {
            retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		dbl_uni
 *
 * RSDT_DOUBLE -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dbl_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        rs_avalret_t retc;

        d = rs_aval_getdouble(cd, src_atype, src_aval);
        retc = rs_aval_putdbltouni(cd, dst_atype, dst_aval, d, RS_DOUBLE_DECPREC + 1);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
        }
        return (retc);
}

/*#***********************************************************************\
 *
 *		date_char
 *
 * RSDT_DATE -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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

static rs_avalret_t date_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        dt_date_t* date;
        ss_char1_t* dtformat;
        ss_char1_t  buf[48];
        size_t buf_len;
        ulong type_len;

        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        date = rs_aval_getdate(cd, src_atype, src_aval);
        dtformat = rs_atype_getdefaultdtformat(cd, src_atype);
        if (dtformat != NULL) {
            succp = dt_date_datetoasciiz(date, dtformat, buf);
        } else {
            dt_datesqltype_t datesqltype = 0;

            switch (RS_ATYPE_SQLDATATYPE(cd, src_atype)) {
                case RSSQLDT_DATE:
                    datesqltype = DT_DATE_SQLDATE;
                    break;
                case RSSQLDT_TIME:
                    datesqltype = DT_DATE_SQLTIME;
                    break;
                case RSSQLDT_TIMESTAMP:
                    datesqltype = DT_DATE_SQLTIMESTAMP;
                    break;
                default:
                    ss_error;
            }
            succp = dt_date_datetoasciiz_sql(date, datesqltype, buf);
        }
        if (succp) {
            rs_avalret_t retc = RSAVR_SUCCESS;

            buf_len = strlen(buf);
            if (type_len < buf_len && type_len > 0) {
                switch (RS_ATYPE_SQLDATATYPE(cd, src_atype)) {
                    case RSSQLDT_DATE:
                        retc = RSAVR_FAILURE;
                        break;
                    case RSSQLDT_TIME:
                        retc = RSAVR_FAILURE;
                        break;
                    case RSSQLDT_TIMESTAMP:
                        {
                            int year = dt_date_year(date);
                            size_t min_length =
                                RS_TIMESTAMP_DISPSIZE_MIN;

                            ss_dassert(min_length == strlen("1996-11-15 09:06:55"));
                            if (year < 0) {
                                min_length++;
                                year = -year;
                            }
                            if (year > 9999) {
                                min_length++;
                            }
                            if (type_len < min_length) {
                                retc = RSAVR_FAILURE;
                            } else {
                                retc = RSAVR_TRUNCATION;
                                buf_len = type_len;
                            }
                        }
                        break;
                    default:
                        ss_error;
                }
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL |
                               RA_VTPLREF |
                               RA_CONVERTED |
                               RA_FLATVA |
                               RA_BLOB |
                               RA_UNKNOWN);
                AVAL_SETDATAANDNULL(cd, dst_atype, dst_aval, buf, buf_len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		date_int
 *
 * RSDT_DATE -> RSDT_INTEGER
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t date_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp = TRUE;
        SsTimeT t;
        dt_date_t* p_timestamp;
        dt_date_t timestamp_buf;
        rs_sqldatatype_t from_sqldt;

        from_sqldt = RS_ATYPE_SQLDATATYPE(cd, src_atype);
        p_timestamp = rs_aval_getdate(
                        cd,
                        src_atype,
                        src_aval);
#if 0
        timestamp_buf = *p_timestamp;
#else /* 0 */
	memcpy(&timestamp_buf,p_timestamp, DT_DATE_DATASIZE);
#endif /* 0 */

        p_timestamp = &timestamp_buf;
        if (from_sqldt == RSSQLDT_TIME) {
            succp = dt_date_padtimewithcurdate(
                        p_timestamp);
        }
        if (succp) {
            rs_datatype_t dt;
            ss_int8_t i8;

            succp = dt_date_datetotimet_raw(p_timestamp, &t);
            if (succp) {
                rs_avalret_t retc = RSAVR_SUCCESS;

                if (dt_date_fraction(p_timestamp) != 0L) {
                    retc = RSAVR_TRUNCATION;
                }
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    dst_aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic
                                  for RSDT_INTEGER! */
                    refdva_free(&dst_aval->ra_va);
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);

                dt = RS_ATYPE_DATATYPE(cd, dst_atype);
                switch (dt) {
                    case RSDT_INTEGER:
                        dst_aval->ra_.l = (long)t;
                        break;
                    case RSDT_BIGINT:
                    default:
                        ss_rc_dassert(dt == RSDT_BIGINT, dt);
                        SsInt8SetInt4(&i8, (ss_int4_t)t);
                        dst_aval->ra_.i8 = i8;
                        break;
                }
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		date_date
 *
 * RSDT_DATE -> RSDT_DATE
 * Non-trivial because RSDT_DATE include TIME, DATE and TIMESTAMP
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t date_date(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp = TRUE;
        rs_avalret_t retc = RSAVR_SUCCESS;
        dt_date_t tmp_date;
        dt_date_t* p_date = NULL;
        rs_sqldatatype_t dst_sqldatatype;
        rs_sqldatatype_t src_sqldatatype;

        dst_sqldatatype = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
        src_sqldatatype = RS_ATYPE_SQLDATATYPE(cd, src_atype);
        if (dst_sqldatatype == src_sqldatatype) {
            retc = triv_asn(
                        cd,
                        dst_atype,
                        dst_aval,
                        src_atype,
                        src_aval,
                        p_errh);
            ss_dassert(retc == RSAVR_SUCCESS);
            return (retc);
        }
        switch (dst_sqldatatype) {
            case RSSQLDT_TIMESTAMP:
                if (src_sqldatatype == RSSQLDT_TIME) {
                    p_date = rs_aval_getdate(cd, src_atype, src_aval);
#if 0
                    tmp_date = *p_date;
#else /* 0 */
		    memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                    succp = dt_date_padtimewithcurdate(&tmp_date);
                    ss_dassert(succp);
                    p_date = &tmp_date;
                } else {
                    retc = triv_asn(
                                cd,
                                dst_atype,
                                dst_aval,
                                src_atype,
                                src_aval,
                                p_errh);
                    ss_dassert(retc == RSAVR_SUCCESS);
                    return (retc);
                }
                break;
            case RSSQLDT_TIME:
                if (src_sqldatatype == RSSQLDT_DATE) {
                    succp = FALSE;
                } else {
                    retc = RSAVR_TRUNCATION;
                    p_date = rs_aval_getdate(cd, src_atype, src_aval);
#if 0
                    tmp_date = *p_date;
#else /* 0 */
		    memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                    succp = dt_date_truncatetotime(&tmp_date);
                    p_date = &tmp_date;
                    ss_dassert(succp);
                }
                break;
            case RSSQLDT_DATE:
                if (src_sqldatatype == RSSQLDT_TIME) {
                    succp = FALSE;
                } else {
                    p_date = rs_aval_getdate(cd, src_atype, src_aval);
                    if (dt_date_hour(p_date) != 0
                    ||  dt_date_min(p_date) != 0
                    ||  dt_date_sec(p_date) != 0
                    ||  dt_date_fraction(p_date) != 0UL)
                    {
                        retc = RSAVR_TRUNCATION;
#if 0
                        tmp_date = *p_date;
#else /* 0 */
			memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                        succp = dt_date_truncatetodate(&tmp_date);
                        ss_dassert(succp);
                        p_date = &tmp_date;
                    }
                }
                break;
            default:
                ss_rc_error(dst_sqldatatype);
        }
        if (succp) {
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_VTPLREF | RA_NULL | RA_FLATVA))
            {
                dst_aval->ra_va = NULL;
            } else {
                ss_derror; /* va should never be dynamic for RSDT_DATE! */
                refdva_free(&dst_aval->ra_va);
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_UNKNOWN);
            SU_BFLAG_SET(dst_aval->ra_flags, RA_FLATVA);
            dt_date_datetova(p_date, &dst_aval->ra_vabuf.va);
            dst_aval->ra_va = &dst_aval->ra_vabuf.va;
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		date_uni
 *
 * RSDT_DATE -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t date_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        dt_date_t* date;
        char* dtformat;
        ss_char1_t buf[50];
        size_t buf_len;
        ulong type_len;

        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        date = rs_aval_getdate(cd, src_atype, src_aval);
        dtformat = rs_atype_getdefaultdtformat(cd, src_atype);
        if (dtformat != NULL) {
            succp = dt_date_datetoasciiz(date, dtformat, buf);
        } else {
            dt_datesqltype_t datesqltype = 0;

            switch (RS_ATYPE_SQLDATATYPE(cd, src_atype)) {
                case RSSQLDT_DATE:
                    datesqltype = DT_DATE_SQLDATE;
                    break;
                case RSSQLDT_TIME:
                    datesqltype = DT_DATE_SQLTIME;
                    break;
                case RSSQLDT_TIMESTAMP:
                    datesqltype = DT_DATE_SQLTIMESTAMP;
                    break;
                default:
                    ss_error;
            }
            succp = dt_date_datetoasciiz_sql(date, datesqltype, buf);
        }
        if (succp) {
            rs_avalret_t retc = RSAVR_SUCCESS;

            buf_len = strlen(buf);
            if (type_len < buf_len && type_len > 0) {
                switch (RS_ATYPE_SQLDATATYPE(cd, src_atype)) {
                    case RSSQLDT_DATE:
                        retc = RSAVR_FAILURE;
                        break;
                    case RSSQLDT_TIME:
                        retc = RSAVR_FAILURE;
                        break;
                    case RSSQLDT_TIMESTAMP:
                        {
                            int year = dt_date_year(date);
                            size_t min_length =
                                RS_TIMESTAMP_DISPSIZE_MIN;
                            ss_dassert(min_length == strlen("1996-11-15 09:06:55"));

                            if (year < 0) {
                                min_length++;
                                year = -year;
                            }
                            if (year > 9999) {
                                min_length++;
                            }
                            if (type_len < min_length) {
                                retc = RSAVR_FAILURE;
                            } else {
                                retc = RSAVR_TRUNCATION;
                                buf_len = type_len;
                            }
                        }
                        break;
                    default:
                        ss_error;
                }
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_FLATVA |
                                  RA_NULL))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                               RA_FLATVA | RA_UNKNOWN);
                AVAL_SETDATACHAR1TO2(cd, dst_atype, dst_aval, buf, buf_len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_char
 *
 * RSDT_DFLOAT -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        ss_char1_t buf[48];
        dt_dfl_t dfl;
        ulong type_len;
        size_t len;
        rs_avalret_t retc = RSAVR_SUCCESS;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltoasciiz_maxlen(dfl, buf, sizeof(buf)/sizeof(buf[0]));
        if (succp) {
            len = strlen(buf);
            type_len = RS_ATYPE_LENGTH(cd, dst_atype);
            if (type_len > 0
            &&  type_len < sizeof(buf)/sizeof(buf[0])
            &&  type_len < len)
            {
                SsDoubleTruncateRetT rc;

                rc = SsTruncateAsciiDoubleValue(buf, type_len + 1);
                switch (rc) {
                    case SS_DBLTRUNC_OK:
                        break;
                    case SS_DBLTRUNC_TRUNCATED:
                        retc = RSAVR_TRUNCATION;
                        break;
                    case SS_DBLTRUNC_VALUELOST:
                        retc = RSAVR_FAILURE;
                        break;
                    default:
                        ss_error;
                }
                len = strlen(buf);
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                len++; /* include term. null character! */
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                               RA_FLATVA | RA_UNKNOWN);
                AVAL_SETDATA(cd, dst_atype, dst_aval, buf, len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_int
 *
 * RSDT_DFLOAT -> RSDT_INT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        long l;
        dt_dfl_t dfl;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltolong(dfl, &l);
        if (succp) {
            rs_avalret_t retc;
            dt_dfl_t dfl2;

            retc = rs_aval_putlong(cd, dst_atype, dst_aval, l);
            if (retc != RSAVR_FAILURE) {
                succp = dt_dfl_setlong(&dfl2, l);
                if (succp) {
                    if (dt_dfl_compare(dfl, dfl2) != 0) {
                        retc = RSAVR_TRUNCATION;
                    }
                    return (retc);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_int8
 *
 * RSDT_DFLOAT -> RSDT_BIGINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t tmp_buf[24];
        ss_int8_t i8;
        dt_dfl_t dfl;
        bool succp;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltoasciiz_maxlen(dfl, tmp_buf, sizeof(tmp_buf)-1);
        if (succp) {
            rs_avalret_t retc;
            char* mismatch;

            if (SsStrScanInt8(tmp_buf, &i8, &mismatch)) {
                retc = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
                if (retc != RSAVR_FAILURE) {
                    dt_dfl_t dfl2;
                    SsInt8ToAscii(i8, tmp_buf, RS_BIGINT_RADIX, 0, '0', TRUE);
                    dt_dfl_setasciiz(&dfl2, tmp_buf);

                    if (dt_dfl_compare(dfl, dfl2) != 0) {
                        retc = RSAVR_TRUNCATION;
                    }
                    return (retc);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_flt
 *
 * RSDT_DFLOAT -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_flt(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        double d;
        dt_dfl_t dfl;
        float f;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltodouble(dfl, &d);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                f = (float)d;
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    dst_aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
                    refdva_free(&dst_aval->ra_va);
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(dst_aval->ra_flags,
                             RA_CONVERTED | RA_ONLYCONVERTED);
                dst_aval->ra_.f = f;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_dbl
 *
 * RSDT_DFLOAT -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_dbl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        double d;
        dt_dfl_t dfl;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltodouble(dfl, &d);
        if (succp) {
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_VTPLREF |
                              RA_NULL |
                              RA_FLATVA |
                              RA_ONLYCONVERTED))
            {
                dst_aval->RA_RDVA = NULL;
            } else {
                ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
                refdva_free(&dst_aval->ra_va);
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
            SU_BFLAG_SET(dst_aval->ra_flags,
                         RA_CONVERTED | RA_ONLYCONVERTED);
            dst_aval->ra_.d = d;
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_dfl
 *
 * RSDT_DFLOAT -> RSDT_DFLOAT
 * Non-trivial, because of possibly different scales and precisions
 * in NUMERIC data
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        rs_avalret_t retc;
        rs_sqldatatype_t dst_sqldatatype;
        rs_sqldatatype_t src_sqldatatype;
        dt_dfl_t dfl;

        dst_sqldatatype = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
        src_sqldatatype = RS_ATYPE_SQLDATATYPE(cd, src_atype);
        if (dst_sqldatatype != RSSQLDT_NUMERIC
        ||  (src_sqldatatype == RSSQLDT_NUMERIC
          && RS_ATYPE_LENGTH(cd, src_atype) == RS_ATYPE_LENGTH(cd, dst_atype)
          && rs_atype_scale(cd, src_atype) == rs_atype_scale(cd, dst_atype)))
        {
            retc = triv_asn(cd, dst_atype, dst_aval, src_atype, src_aval, p_errh);
            ss_dassert(retc == RSAVR_SUCCESS);
            return (retc);
        }
        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		dfl_uni
 *
 * RSDT_DFLOAT -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t dfl_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;
        ss_char1_t buf[48];
        dt_dfl_t dfl;
        rs_avalret_t retc = RSAVR_SUCCESS;
        ulong type_len;
        size_t len;

        dfl = rs_aval_getdfloat(cd, src_atype, src_aval);
        succp = dt_dfl_dfltoasciiz_maxlen(dfl, buf, sizeof(buf)/sizeof(buf[0]));
        if (succp) {
            len = strlen(buf);
            type_len = RS_ATYPE_LENGTH(cd, dst_atype);
            if (type_len < sizeof(buf)/sizeof(buf[0])
            &&  type_len > 0
            &&  type_len < len)
            {
                SsDoubleTruncateRetT rc;

                rc = SsTruncateAsciiDoubleValue(buf, type_len + 1);
                len = strlen(buf);
                switch (rc) {
                    case SS_DBLTRUNC_OK:
                        break;
                    case SS_DBLTRUNC_TRUNCATED:
                        retc = RSAVR_TRUNCATION;
                        break;
                    case SS_DBLTRUNC_VALUELOST:
                        retc = RSAVR_FAILURE;
                        break;
                    default:
                        ss_error;
                }
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                               RA_FLATVA | RA_UNKNOWN);
                AVAL_SETDATACHAR1TO2(cd, dst_atype, dst_aval, buf, len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		bin_char
 *
 * RSDT_BINARY -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t bin_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        va_index_t src_datalen;
        va_index_t dst_datalen;
        size_t dst_bytelen;
        ss_byte_t* p_src_data;
        ss_char1_t* p_dst_data;
        ulong type_len;
        rs_avalret_t retc;
        bool succp;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }

        if (!succp) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
        src_datalen--;
        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        dst_datalen = src_datalen * 2;
        retc = RSAVR_SUCCESS;
        if (dst_datalen > type_len && type_len > 0) {
            src_datalen = type_len / 2;
            dst_datalen = src_datalen * 2;
            retc = RSAVR_TRUNCATION;
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);

        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            dst_aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                       RA_FLATVA | RA_UNKNOWN);
        dst_bytelen = dst_datalen * sizeof(ss_char1_t) + 1;
        AVAL_SETDATA(cd, dst_atype, dst_aval, NULL, dst_bytelen);
        p_dst_data = va_getdata(dst_aval->RA_RDVA, &dst_datalen);
        ss_dassert(dst_datalen == dst_bytelen);
        ss_dassert(dst_datalen == (src_datalen * 2 * sizeof(ss_char1_t)) + 1);
        su_chcvt_bin2hex(p_dst_data, p_src_data, src_datalen);
        ((ss_byte_t*)p_dst_data)[dst_datalen - 1] = '\0';
        return (retc);
}

/*#***********************************************************************\
 *
 *		bin_uni
 *
 * RSDT_BINARY -> RSDT_UNICODE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t bin_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        va_index_t src_datalen;
        va_index_t dst_datalen;
        size_t dst_bytelen;
        ss_byte_t* p_src_data;
        ss_char2_t* p_dst_data;
        ulong type_len;
        rs_avalret_t retc;
        bool succp;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }
        if (!succp) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, src_atype),
                rs_atype_name(cd, dst_atype));
            return (RSAVR_FAILURE);
        }
        p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
        src_datalen--;
        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        dst_datalen = src_datalen * 2;
        retc = RSAVR_SUCCESS;
        if (dst_datalen > type_len && type_len > 0) {
            src_datalen = type_len / 2;
            dst_datalen = src_datalen * 2;
            retc = RSAVR_TRUNCATION;
        }
        RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
        ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            dst_aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                       RA_FLATVA | RA_UNKNOWN);
        dst_bytelen = dst_datalen * sizeof(ss_char2_t) + 1;
        AVAL_SETDATA(cd, dst_atype, dst_aval, NULL, dst_bytelen);
        p_dst_data = va_getdata(dst_aval->RA_RDVA, &dst_datalen);
        ss_dassert(dst_datalen == dst_bytelen);
        ss_dassert(dst_datalen == src_datalen * 2 * sizeof(ss_char2_t) + 1);
        su_chcvt_bin2hexchar2_va(p_dst_data, p_src_data, src_datalen);
        ((ss_byte_t*)p_dst_data)[dst_datalen - 1] = '\0';
        return (retc);
}


static rs_avalret_t aval_uni2char_forceif(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh,
        bool force)
{
        bool succp;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }
        if (succp) {
            ulong type_len;
            rs_avalret_t retc;
            va_index_t src_datalen;
            void* p_src_data;

            type_len = RS_ATYPE_LENGTH(cd, dst_atype);
            p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
            ss_dassert(src_datalen & 1);
            src_datalen /= 2;
            retc = RSAVR_SUCCESS;
            if (type_len < src_datalen) {
                retc = RSAVR_TRUNCATION;
                src_datalen = type_len;
            }
            RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
            ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_NULL | RA_VTPLREF | RA_FLATVA))
            {
                dst_aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
            succp = AVAL_SETVADATACHAR2TO1(cd,
                                           dst_atype, dst_aval,
                                           p_src_data, src_datalen);
            succp |= force;
            if (succp) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_char
 *
 * RSDT_UNICODE -> RSDT_CHAR
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_char(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;

        succp = aval_uni2char_forceif(
                cd,
                dst_atype, dst_aval,
                src_atype, src_aval,
                p_errh,
                FALSE);
        return (succp);
}

/*#***********************************************************************\
 *
 *		uniFchar
 *
 * RSDT_UNICODE -> RSDT_CHAR (Forced, used in CAST expressions)
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uniFchar(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;

        succp = aval_uni2char_forceif(
                cd,
                dst_atype, dst_aval,
                src_atype, src_aval,
                p_errh,
                TRUE);
        return (succp);
}

/*#***********************************************************************\
 *
 *		uni_int
 *
 * RSDT_UNICODE -> RSDT_INTEGER
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_int(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        long l;
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            rs_avalret_t retc = 0;

            succp = SsStrScanLong(tmp_buf, &l, &mismatch);
            if (succp) {
                succp = (*mismatch == '\0' || ss_isspace(*mismatch));
                if (succp) {
                    retc = rs_aval_putlong(cd, dst_atype, dst_aval, l);
                }
            }
            SsMemFree(tmp_buf);
            if (succp && retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_int8
 *
 * RSDT_UNICODE -> RSDT_BIGINT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_int8(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_int8_t i8;
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            rs_avalret_t retc = 0;

            succp = SsStrScanInt8(tmp_buf, &i8, &mismatch);
            if (succp) {
                succp = (*mismatch == '\0' || ss_isspace(*mismatch));
                if (succp) {
                    retc = rs_aval_putint8(cd, dst_atype, dst_aval, i8);
                }
            }
            SsMemFree(tmp_buf);
            if (succp && retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_flt
 *
 * RSDT_UNICODE -> RSDT_FLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_flt(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            succp = SsStrScanDouble(tmp_buf, &d, &mismatch);
            if (succp) {
                succp = (*mismatch == '\0' || ss_isspace(*mismatch));
                if (succp) {
                    succp = SS_FLOAT_IS_PORTABLE(d);
                    if (succp) {
                        float f = (float)d;

                        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_VTPLREF |
                                          RA_NULL |
                                          RA_ONLYCONVERTED |
                                          RA_FLATVA))
                        {
                            dst_aval->RA_RDVA = NULL;
                        } else {
                            ss_derror; /* va should never be dynamic
                                          for RSDT_FLOAT! */
                            refdva_free(&dst_aval->ra_va);
                        }
                        SU_BFLAG_CLEAR(dst_aval->ra_flags,
                                       RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                        SU_BFLAG_SET(dst_aval->ra_flags,
                                     RA_CONVERTED | RA_ONLYCONVERTED);
                        dst_aval->ra_.f = f;
                    }
                }
            }
            SsMemFree(tmp_buf);
            if (succp) {
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_dbl
 *
 * RSDT_UNICODE -> RSDT_DOUBLE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_dbl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        double d;
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            succp = SsStrScanDouble(tmp_buf, &d, &mismatch);
            if (succp) {
                succp = (*mismatch == '\0' || ss_isspace(*mismatch));
                if (succp) {
                    if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                      RA_VTPLREF |
                                      RA_NULL |
                                      RA_ONLYCONVERTED |
                                      RA_FLATVA))
                    {
                        dst_aval->RA_RDVA = NULL;
                    } else {
                        ss_derror; /* va should never be dynamic
                                      for RSDT_DOUBLE! */
                        refdva_free(&dst_aval->ra_va);
                    }
                    SU_BFLAG_CLEAR(dst_aval->ra_flags,
                                   RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                    SU_BFLAG_SET(dst_aval->ra_flags,
                                 RA_CONVERTED | RA_ONLYCONVERTED);
                    dst_aval->ra_.d = d;
                }
            }
            SsMemFree(tmp_buf);
            if (succp) {
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_date
 *
 * RSDT_UNICODE -> RSDT_DATE
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_date(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        ss_char1_t* tmp_buf;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            rs_avalret_t retc;
            retc = rs_aval_putchartodate(cd, dst_atype, dst_aval, tmp_buf);
            SsMemFree(tmp_buf);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_dfl
 *
 * RSDT_UNICODE -> RSDT_DFLOAT
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_dfl(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        dt_dfl_t dfl;
        ss_char1_t* tmp_buf;
        bool succp;
        rs_avalret_t retc = 0;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, src_atype, src_aval, &len);
        if (tmp_buf != NULL) {
            succp = dt_dfl_setasciiz(&dfl, tmp_buf);
            if (succp) {
                retc = rs_aval_putdfl(cd, dst_atype, dst_aval, &dfl);
            }
            SsMemFree(tmp_buf);
            if (succp && retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

/*#***********************************************************************\
 *
 *		uni_bin
 *
 * RSDT_UNICODE -> RSDT_BINARY
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	dst_atype -
 *
 *
 *	dst_aval -
 *
 *
 *	src_atype -
 *
 *
 *	src_aval -
 *
 *
 *	p_errh -
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
static rs_avalret_t uni_bin(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        bool succp;

        if (rs_aval_isblob(cd, src_atype, src_aval)) {
            succp = rs_aval_loadblob(cd,
                                     src_atype, src_aval,
                                     rs_aval_getloadblobsizelimit(cd));
        } else {
            succp = TRUE;
        }
        if (succp) {
            rs_avalret_t retc;
            ulong type_len;
            ss_char2_t* p_src_data;
            va_index_t src_datalen;
            ss_byte_t* p_dst_data;
            va_index_t dst_datalen;

            p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
            ss_dassert(src_datalen & 1);
            if ((src_datalen & 3) != 1) {
                goto failed;
            }
            src_datalen /= 4;
            type_len = RS_ATYPE_LENGTH(cd, dst_atype);
            retc = RSAVR_SUCCESS;
            if (type_len < src_datalen && type_len > 0) {
                retc = RSAVR_TRUNCATION;
                src_datalen = type_len;
            }
            RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
            ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_VTPLREF | RA_FLATVA | RA_NULL))
            {
                dst_aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                           RA_FLATVA | RA_UNKNOWN);
            AVAL_SETDATAANDNULL(cd, dst_atype, dst_aval, NULL, src_datalen);
            p_dst_data = va_getdata(dst_aval->RA_RDVA, &dst_datalen);
            ss_dassert(dst_datalen == src_datalen + 1);
            succp = su_chcvt_hex2binchar2_va(p_dst_data, p_src_data, src_datalen);
            if (succp) {
                return (retc);
            }
        }
 failed:;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, src_atype),
            rs_atype_name(cd, dst_atype));
        return (RSAVR_FAILURE);
}

static rs_avalret_t uni_uni(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        va_index_t src_datalen;
        ss_char2_t* p_src_data;
        ulong type_len;
        size_t dst_bytelen_minus_1;

        type_len = RS_ATYPE_LENGTH(cd, dst_atype);
        if (type_len > 0) {
            p_src_data = va_getdata(src_aval->RA_RDVA, &src_datalen);
            src_datalen /= 2;

            if (type_len < src_datalen) {
                bool succp;

                if (rs_aval_isblob(cd, src_atype, src_aval)) {
                    succp = rs_aval_loadblob(cd,
                                             src_atype, src_aval,
                                             rs_aval_getloadblobsizelimit(cd));
                } else {
                    succp = TRUE;
                }
                if (!succp) {
                    rs_error_create(
                        p_errh,
                        E_ILLASSIGN_SS,
                        rs_atype_name(cd, src_atype),
                        rs_atype_name(cd, dst_atype));
                    return (RSAVR_FAILURE);
                }
                src_datalen = type_len;
                RS_AVAL_UNLINKBLOBIF(cd, dst_atype, dst_aval);
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    dst_aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                               RA_FLATVA | RA_UNKNOWN);
                dst_bytelen_minus_1 = src_datalen * sizeof(ss_char2_t);
                AVAL_SETDATAANDNULL(cd, dst_atype, dst_aval,
                                    p_src_data, dst_bytelen_minus_1);
                return (RSAVR_TRUNCATION);
            }
        }
        RS_AVAL_ASSIGNBUF(cd, dst_atype, dst_aval, src_aval);
        return (RSAVR_SUCCESS);
}
