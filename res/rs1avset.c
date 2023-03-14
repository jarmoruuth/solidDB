/*************************************************************************\
**  source       * rs1avset.c
**  directory    * res
**  description  * rs_aval_setxxx routines
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

#include <ssc.h>
#include <ssstring.h>

#include <sswctype.h>
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
#include <sswscan.h>
#include <sswcs.h>
#include <ssutf.h>
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
#include "rs0error.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0cons.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"
#include "rs2avcvt.h"

#define ltoint   rs_aval_setlong_raw
#define chtochar rs_aval_set8bitstr_raw
#define cbtochar rs_aval_set8bitcdata_raw
#define dftodfl  rs_aval_setdfloat_raw
#define uctouni  rs_aval_setwcs_raw
#define ubtouni  rs_aval_setwdata_raw
#define i8toint8 rs_aval_setint8_raw

typedef RS_AVALRET_T aval_set8bitstrfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setcharbuffun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setlongfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setint8fun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setdoublefun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh);

#ifdef NO_ANSI_FLOAT
typedef RS_AVALRET_T aval_setfloatfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double f_tmp,
        rs_err_t** p_errh);
#else /* NO_ANSI_FLOAT */
typedef RS_AVALRET_T aval_setfloatfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float f,
        rs_err_t** p_errh);
#endif /* NO_ANSI_FLOAT */

typedef RS_AVALRET_T aval_setdatefun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setdfloatfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setwcsfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh);

typedef RS_AVALRET_T aval_setwdatafun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh);

static aval_set8bitstrfun_t
        chtoint,
        chtoflt,
        chtodbl,
        chtodate,
        chtodfl,
        chtobin,
        chtouni,
        chtoint8;

static aval_setcharbuffun_t
        cbtoint,
        cbtoflt,
        cbtodbl,
        cbtodate,
        cbtodfl,
        cbtobin,
        cbtouni,
        cbtoint8;


static aval_setlongfun_t
        ltochar,
        ltoflt,
        ltodbl,
        lillegal,
        ltodfl,
        ltouni,
        ltoint8;

static aval_setint8fun_t
        i8tochar,
        i8toint,
        i8toflt,
        i8todbl,
        i8illegal,
        i8todfl,
        i8touni,
        i8tovarbinary;

static aval_setdoublefun_t
        dtochar,
        dtoint,
        dtoflt,
        dtodbl,
        dillegal,
        dtodfl,
        dtouni,
        dtoint8;

static aval_setfloatfun_t
        ftochar,
        ftoint,
        ftoflt,
        ftodbl,
        fillegal,
        ftodfl,
        ftouni,
        ftoint8;

static aval_setdatefun_t
        dttochar,
        dtillegal,
        dttodate,
        dttouni;

static aval_setdfloatfun_t
        dftochar,
        dftoint,
        dftoflt,
        dftodbl,
        dfillegal,
        dftouni,
        dftoint8;

static aval_setwcsfun_t
        uctochar,
        uctoint,
        uctoflt,
        uctodbl,
        uctodate,
        uctodfl,
        uctobin,
        uctoint8;

static aval_setwdatafun_t
        ubtochar,
        ubtoint,
        ubtoflt,
        ubtodbl,
        ubtodate,
        ubtodfl,
        ubtobin,
        ubtoint8;

static const double dbl_zero = 0.0;
static const float flt_zero = (float)0.0;

static aval_set8bitstrfun_t* const aval_set8bitstrarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT*/
        chtochar,   chtoint,    chtoflt,    chtodbl,    chtodate,   chtodfl,    chtobin,    chtouni,    chtoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_set8bitstr_ext
 *
 * Sets 8-bit ascii, '\0'-terminated string to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	str - in, use
 *		string value
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - the input buffer could not be converted to the
 *                      requested data type
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_set8bitstr_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_set8bitstrarr[dt])(cd, atype, aval, str, p_errh);
        return (retc);
}

static aval_setcharbuffun_t* const aval_setcharbufarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT*/
        cbtochar,   cbtoint,    cbtoflt,    cbtodbl,    cbtodate,   cbtodfl,    cbtobin,    cbtouni,    cbtoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_set8bitcdata_ext
 *
 * Sets 8-bit ASCII value (no '\0' -termination) to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	buf - in, use
 *		input buffer
 *
 *	bufsize - in, use
 *		input buffer size in bytes ('\0'-byte excluded)
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - the input buffer could not be converted to the
 *                      requested data type
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_set8bitcdata_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setcharbufarr[dt])(cd, atype, aval, buf, bufsize, p_errh);
        return (retc);
}

static aval_setlongfun_t* const aval_setlongarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        ltochar,    ltoint,     ltoflt,     ltodbl,     lillegal,   ltodfl,     lillegal,   ltouni, ltoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setlong_ext
 *
 * Sets long int value to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	l - in
 *		long int value
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setlong_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setlongarr[dt])(cd, atype, aval, l, p_errh);
        return (retc);
}

static aval_setint8fun_t* const aval_setint8arr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY     RSDT_UNICODE RSDT_BIGINT */
        i8tochar,  i8toint,     i8toflt,    i8todbl,    i8illegal,  i8todfl,    i8tovarbinary,  i8touni,     i8toint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setint8_ext
 *
 * Sets BIGINT value to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	i8 - in
 *		64 bit int value
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setint8_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setint8arr[dt])(cd, atype, aval, i8, p_errh);
        return (retc);
}

static aval_setdoublefun_t* const aval_setdoublearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dtochar,    dtoint,     dtoflt,     dtodbl,     dillegal,   dtodfl,     dillegal,   dtouni, dtoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setdouble_ext
 *
 * Sets double value to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	d - in
 *		double precision value
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation of decimal digits)
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setdouble_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;
        ss_trapcode_t trapcode;

        trapcode = SS_TRAP_NONE;
        SS_TRAP_HANDLERSECTION
            case SS_TRAP_FPE:
            case SS_TRAP_FPE_INVALID:
            case SS_TRAP_FPE_INEXACT:
            case SS_TRAP_FPE_STACKFAULT:
            case SS_TRAP_FPE_STACKOVERFLOW:
            case SS_TRAP_FPE_STACKUNDERFLOW:
            case SS_TRAP_FPE_BOUND:
            case SS_TRAP_FPE_DENORMAL:
            case SS_TRAP_FPE_EXPLICITGEN:
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTOVFLOW:
            case SS_TRAP_FPE_OVERFLOW:
                rs_error_create(p_errh, E_NUMERICOVERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTDIV0:
            case SS_TRAP_FPE_ZERODIVIDE:
                rs_error_create(p_errh, E_DIVBYZERO);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_UNDERFLOW:
                rs_error_create(p_errh, E_NUMERICUNDERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            default:
                trapcode = SS_TRAP_GETCODE();
                ss_rc_derror(trapcode);
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE_EXPLICITGEN;
                break;
        SS_TRAP_RUNSECTION
            if (!SS_DOUBLE_IS_PORTABLE(d)) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE;
            } else if (d == -0.0) {
                d = dbl_zero;
            }
        SS_TRAP_END
        if (trapcode != SS_TRAP_NONE) {
            return (RSAVR_FAILURE);
        }
        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*(aval_setdoublearr[dt]))(cd, atype, aval, d, p_errh);
        return (retc);
}

static aval_setfloatfun_t* const aval_setfloatarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        ftochar,    ftoint,     ftoflt,     ftodbl,     fillegal,   ftodfl,     fillegal,   ftouni, ftoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setfloat_ext
 *
 * See rs_aval_setdouble_ext()
 *
 */
RS_AVALRET_T rs_aval_setfloat_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;
        ss_trapcode_t trapcode;

        trapcode = SS_TRAP_NONE;
        SS_TRAP_HANDLERSECTION
            case SS_TRAP_FPE:
            case SS_TRAP_FPE_INVALID:
            case SS_TRAP_FPE_INEXACT:
            case SS_TRAP_FPE_STACKFAULT:
            case SS_TRAP_FPE_STACKOVERFLOW:
            case SS_TRAP_FPE_STACKUNDERFLOW:
            case SS_TRAP_FPE_BOUND:
            case SS_TRAP_FPE_DENORMAL:
            case SS_TRAP_FPE_EXPLICITGEN:
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTOVFLOW:
            case SS_TRAP_FPE_OVERFLOW:
                rs_error_create(p_errh, E_NUMERICOVERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTDIV0:
            case SS_TRAP_FPE_ZERODIVIDE:
                rs_error_create(p_errh, E_DIVBYZERO);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_UNDERFLOW:
                rs_error_create(p_errh, E_NUMERICUNDERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            default:
                trapcode = SS_TRAP_GETCODE();
                ss_rc_derror(trapcode);
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE_EXPLICITGEN;
                break;
        SS_TRAP_RUNSECTION
            if (!SS_FLOAT_IS_PORTABLE(f)) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE;
            } else
#ifdef NO_ANSI_FLOAT
            if (f == -0.0) {
                f = dbl_zero;
            }
#else /* NO_ANSI_FLOAT */
            if (f == (float)-0.0) {
                f = flt_zero;
            }
#endif /* NO_ANSI_FLOAT */
        SS_TRAP_END
        if (trapcode != SS_TRAP_NONE) {
            return (RSAVR_FAILURE);
        }
        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*(aval_setfloatarr[dt]))(cd, atype, aval, f, p_errh);
        return (retc);
}


static aval_setdatefun_t* const aval_setdatearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dttochar,   dtillegal,  dtillegal,  dtillegal,  dttodate,   dtillegal,  dtillegal,  dttouni, dtillegal
};

/*##**********************************************************************\
 *
 *		rs_aval_setdate_ext
 *
 * Sets a date or time type value to aval
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	p_date - in, use
 *		pointer to date value
 *
 *	datesqltype - in
 *		DT_DATE_SQLDATE         - the data is of type DATE
 *		DT_DATE_SQLTIME         - -"- TIME
 *		DT_DATE_SQLTIMESTAMP    - -"- TIMESTAMP
 *		DT_DATE_SQLTYPE_UNKNOWN - unknown data type, determined from
 *                                    atype and the value of the date
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation. eg. second or fractions of second)
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setdate_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setdatearr[dt])(cd, atype, aval, p_date, datesqltype, p_errh);
        return (retc);
}

static aval_setdfloatfun_t* const aval_setdfloatarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dftochar,   dftoint,    dftoflt,    dftodbl,    dfillegal,  dftodfl,    dfillegal,  dftouni, dftoint8
};


/*##**********************************************************************\
 *
 *		rs_aval_setdfloat_ext
 *
 * See rs_aval_setdouble_ext
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	atype -
 *
 *
 *	aval -
 *
 *
 *	dfl - in, use
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
RS_AVALRET_T rs_aval_setdfloat_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setdfloatarr[dt])(cd, atype, aval, dfl, p_errh);
        return (retc);
}

static aval_setwcsfun_t* aval_setwcsarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        uctochar,   uctoint,    uctoflt,    uctodbl,    uctodate,   uctodfl,    uctobin,    uctouni,    uctoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setwcs_ext
 *
 * Sets wide character string value to aval
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	wcs - in, use
 *		wcs input
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setwcs_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setwcsarr[dt])(cd, atype, aval, wcs, p_errh);
        return (retc);
}

static aval_setwdatafun_t* aval_setwdataarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        ubtochar,   ubtoint,    ubtoflt,    ubtodbl,    ubtodate,   ubtodfl,    ubtobin,    ubtouni,    ubtoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setwdata_ext
 *
 * Sets wide character data to aval
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	buf - in, use
 *		wide char buffer
 *
 *	bufsize - in
 *		number of wide chars in buf
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - conversion failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setwdata_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setwdataarr[dt])(cd, atype, aval, buf, bufsize, p_errh);
        return (retc);
}

void rs_aval_setdata_raw(
        void* cd,
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        void* buf,
        size_t bufsize)
{
        CHECK_AVAL(aval);

        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF | RA_NULL |
                          RA_FLATVA | RA_ONLYCONVERTED))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_FLATVA |
                       RA_ONLYCONVERTED |
                       RA_UNKNOWN);
        AVAL_SETDATA(cd, atype, aval, buf, bufsize);
        ss_dassert(
            RS_ATYPE_DATATYPE(cd, atype) != RSDT_UNICODE ||
            (va_netlen(aval->RA_RDVA) & 1));
}

RS_AVALRET_T rs_aval_setbdata_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;
        ulong len;

        retc = RSAVR_SUCCESS;
        len = RS_ATYPE_LENGTH(cd, atype);

        dt = RS_ATYPE_DATATYPE(cd, atype);
        switch (dt) {
            case RSDT_BINARY:
            case RSDT_CHAR:
                if (len < bufsize) {
                    retc = RSAVR_TRUNCATION;
                    bufsize = len;
                }
                break;
            case RSDT_UNICODE:
                ss_dassert(!(bufsize & 1));
                if (bufsize / 2 > len) {
                    bufsize = len * 2;
                    retc = RSAVR_TRUNCATION;
                }
                break;
            default:
                retc = rs_aval_set8bitcdata_ext(cd, atype, aval, buf, bufsize, p_errh);
                return (retc);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_NULL | RA_FLATVA)) {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATAANDNULL(cd, atype, aval, buf, bufsize);
        ss_dassert(
            RS_ATYPE_DATATYPE(cd, atype) != RSDT_UNICODE ||
            (va_netlen(aval->RA_RDVA) & 1));
        return (retc);
}

RS_AVALRET_T chtochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_CHAR);
        len = strlen(str);
        retc = cbtochar(cd, atype, aval, str, len, p_errh);
        return (retc);
}

static RS_AVALRET_T chtoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        bool succp;
        long l;
        ss_char1_t* mismatch;

        succp = SsStrScanLong(str, &l, &mismatch);
        if (succp) {
            retc = rs_aval_putlong(cd, atype, aval, l);
            if (retc == RSAVR_FAILURE) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOVERFLOW);
            }
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        bool succp;
        ss_int8_t i8;
        ss_char1_t* mismatch;

        succp = SsStrScanInt8(str, &i8, &mismatch);
        if (succp) {
            retc = rs_aval_putint8(cd, atype, aval, i8);
            if (retc == RSAVR_FAILURE) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOVERFLOW);
            }
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtoflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        double d;
        ss_char1_t* mismatch;
        bool succp;

        succp = SsStrScanDouble(str, &d, &mismatch);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                float f;
                f = (float)d;
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
                    refdva_free(&aval->ra_va);
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
                aval->ra_.f = f;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtodbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        double d;
        ss_char1_t* mismatch;
        bool succp;

        succp = SsStrScanDouble(str, &d, &mismatch);
        if (succp) {
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF |
                              RA_NULL |
                              RA_FLATVA |
                              RA_ONLYCONVERTED))
            {
                aval->RA_RDVA = NULL;
            } else {
                ss_derror; /* va should never be dynamic for RSDT_DOUBLE! */
                refdva_free(&aval->ra_va);
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
            aval->ra_.d = d;
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putchartodate(cd, atype, aval, str);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        ss_dassert(retc == RSAVR_FAILURE);
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        dt_dfl_t dfl;
        RS_AVALRET_T retc;
        bool succp;

        ss_dprintf_1(("chtodfl: calling dt_dfl_setasciiz(\"%s\")\n",
                      str));
        succp = dt_dfl_setasciiz(&dfl, str);
        ss_dprintf_1(("succp = %d\n", (int)succp));
        if (succp) {
            retc = rs_aval_putdfl(cd, atype, aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
            rs_error_create(
                p_errh,
                E_NUMERICOUTOFRANGE);
            return (RSAVR_FAILURE);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T chtobin(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = strlen(str);
        retc = cbtobin(cd, atype, aval, str, len, p_errh);
        return (retc);
}

static RS_AVALRET_T chtouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = strlen(str);
        retc = cbtouni(cd, atype, aval, str, len, p_errh);
        return (retc);
}

RS_AVALRET_T cbtochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        RS_AVALRET_T retc;
        ulong len;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_CHAR);
        len = RS_ATYPE_LENGTH(cd, atype);
        if (len < bufsize) {
            bufsize = len;
            retc = RSAVR_TRUNCATION;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        AVAL_SETDATAANDNULL(cd, atype, aval, buf, bufsize);
        return (retc);
}


static RS_AVALRET_T aval_cbcallbackframe(
        aval_set8bitstrfun_t* p_fun,
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        ss_char1_t tmp_buf[48];
        ss_char1_t* p_buf;
        RS_AVALRET_T retc;

        if (bufsize < sizeof(tmp_buf)) {
            p_buf = tmp_buf;
        } else {
            p_buf = SsMemAlloc(bufsize + 1);
        }
        memcpy(p_buf, buf, bufsize);
        p_buf[bufsize] = '\0';
        retc = (*p_fun)(cd, atype, aval, p_buf, p_errh);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        return (retc);
}

static RS_AVALRET_T cbtoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtoint,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtoint8,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtoflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtoflt,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtodbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtodbl,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtodate,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_cbcallbackframe(
                    chtodfl,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T cbtobin(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        if ((bufsize & 1) == 0) {
            RS_AVALRET_T retc;
            bool succp = TRUE;
            ss_byte_t* destdata;
            va_index_t destlen;
            ulong len;

            len = RS_ATYPE_LENGTH(cd, atype);
            if (bufsize / 2 > len) {
                bufsize = len * 2;
                retc = RSAVR_TRUNCATION;
            } else {
                retc = RSAVR_SUCCESS;
            }
            RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF | RA_NULL | RA_FLATVA))
            {
                aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL |
                           RA_VTPLREF |
                           RA_CONVERTED |
                           RA_BLOB |
                           RA_FLATVA |
                           RA_UNKNOWN);
            destlen = bufsize / 2;
            AVAL_SETDATAANDNULL(cd, atype, aval, NULL, destlen);
            destdata = va_getdata(aval->RA_RDVA, &destlen);
            ss_dassert(destlen == (bufsize / 2) + 1);
            if (destlen > 1) {
                succp = su_chcvt_hex2bin(destdata, buf, destlen - 1);
            }
            ss_dassert(destdata[destlen-1] == '\0');
            if (succp) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_CHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T cbtouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        RS_AVALRET_T retc;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        if (len < bufsize) {
            retc = RSAVR_TRUNCATION;
            bufsize = (size_t)len;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        AVAL_SETDATACHAR1TO2(cd, atype, aval, buf, bufsize);
        return (retc);
}

static RS_AVALRET_T ltochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        ss_char1_t buf[24];
        size_t buf_len;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        buf_len = SsLongToAscii(l, buf, 10, 0, '0', TRUE);
        ss_dassert(buf_len < sizeof(buf)/sizeof(buf[0]));
        if (len < buf_len) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                RS_TN_INTEGER,
                rs_atype_name(cd, atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        buf_len += 1;
        AVAL_SETDATA(cd, atype, aval, buf, buf_len);
        return (RSAVR_SUCCESS);
}

RS_AVALRET_T ltoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putlong(cd, atype, aval, l);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

RS_AVALRET_T ltoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_int8_t i8;

        SsInt8SetInt4(&i8, l);
        retc = rs_aval_putint8(cd, atype, aval, i8);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        ss_derror;
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ltoflt(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        float f;

        f = (float)l;
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_ONLYCONVERTED |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.f = f;
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T ltodbl(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d;

        d = (double)l;
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_ONLYCONVERTED |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_DOUBLE! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T lillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        long l __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_INTEGER,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ltodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        bool succp;
        dt_dfl_t dfl;

        succp = dt_dfl_setlong(&dfl, l);
        if (succp) {
            retc = rs_aval_putdfl(cd, atype, aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
            ss_derror;
        }
        ss_derror;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_INTEGER,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);

}

static RS_AVALRET_T ltouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long l,
        rs_err_t** p_errh)
{
        ss_char2_t buf[24];
        size_t buf_len;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        buf_len = SsLongToWcs(l, buf, 10, 0, '0', TRUE);
        ss_dassert(buf_len < sizeof(buf)/sizeof(buf[0]));
        if (len < buf_len) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                RS_TN_INTEGER,
                rs_atype_name(cd, atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        AVAL_SETDATACHAR2(cd, atype, aval, buf, buf_len);
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T dtochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putdbltochar(cd, atype, aval, d, RS_DOUBLE_DECPREC + 1);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DOUBLE_PRECISION,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dtoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        long l;

        if (d > ((double)SS_INT4_MIN - (double)1)
        &&  d < ((double)SS_INT4_MAX + (double)1))
        {
            RS_AVALRET_T retc = RSAVR_SUCCESS;
            RS_AVALRET_T retc2;

            l = (long)d;
            if ((double)l != d) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putlong(cd, atype, aval, l);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dtoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        ss_int8_t i8;

        if (SsInt8SetDouble(&i8, d)) {
            RS_AVALRET_T retc = RSAVR_SUCCESS;
            RS_AVALRET_T retc2;

            retc2 = rs_aval_putint8(cd, atype, aval, i8);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dtoflt(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        float f;

        if (!SS_FLOAT_IS_PORTABLE(d)) {
            rs_error_create(
                p_errh,
                E_NUMERICOUTOFRANGE);
            return (RSAVR_FAILURE);
        }
        f = (float)d;
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA |
                          RA_ONLYCONVERTED))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.f = f;
        return (RSAVR_SUCCESS);
}


static RS_AVALRET_T dtodbl(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DOUBLE);
        ss_dassert(SS_DOUBLE_IS_PORTABLE(d));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA |
                          RA_ONLYCONVERTED))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_DOUBLE! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}

RS_AVALRET_T rs_aval_setdouble_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_trapcode_t trapcode;

        trapcode = SS_TRAP_NONE;
        SS_TRAP_HANDLERSECTION
            case SS_TRAP_FPE:
            case SS_TRAP_FPE_INVALID:
            case SS_TRAP_FPE_INEXACT:
            case SS_TRAP_FPE_STACKFAULT:
            case SS_TRAP_FPE_STACKOVERFLOW:
            case SS_TRAP_FPE_STACKUNDERFLOW:
            case SS_TRAP_FPE_BOUND:
            case SS_TRAP_FPE_DENORMAL:
            case SS_TRAP_FPE_EXPLICITGEN:
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTOVFLOW:
            case SS_TRAP_FPE_OVERFLOW:
                rs_error_create(p_errh, E_NUMERICOVERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTDIV0:
            case SS_TRAP_FPE_ZERODIVIDE:
                rs_error_create(p_errh, E_DIVBYZERO);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_UNDERFLOW:
                rs_error_create(p_errh, E_NUMERICUNDERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            default:
                trapcode = SS_TRAP_GETCODE();
                ss_rc_derror(trapcode);
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE_EXPLICITGEN;
                break;
        SS_TRAP_RUNSECTION
            ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DOUBLE);
            if (!SS_DOUBLE_IS_PORTABLE(d)) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE;
            } else if (d == -0.0) {
                d = dbl_zero;
            }
        SS_TRAP_END
        if (trapcode != SS_TRAP_NONE) {
            return (RSAVR_FAILURE);
        }
        retc = dtodbl(cd, atype, aval, d, p_errh);
        return (retc);
}


static RS_AVALRET_T dillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        double d __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DOUBLE_PRECISION,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dtodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        bool succp;
        dt_dfl_t dfl;

        succp = dt_dfl_setdouble(&dfl, d);
        if (succp) {
            retc = rs_aval_putdfl(cd, atype, aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dtouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double d,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putdbltouni(
                    cd,
                    atype,
                    aval,
                    d,
                    RS_DOUBLE_DECPREC + 1);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DOUBLE_PRECISION,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}


static RS_AVALRET_T ftochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putdbltochar(cd, atype, aval, (double)f, RS_REAL_DECPREC + 1);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_REAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ftoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        long l;

        if (f > ((float)SS_INT4_MIN - (float)1)
        &&  f < ((float)SS_INT4_MAX + (float)1))
        {
            RS_AVALRET_T retc = RSAVR_SUCCESS;
            RS_AVALRET_T retc2;

            l = (long)f;
            if ((float)l != f) {
                retc = RSAVR_TRUNCATION;
            }
            retc2 = rs_aval_putlong(cd, atype, aval, l);
            if (retc2 != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ftoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        double d = (double)f;

        return (dtoint8(cd, atype, aval, d, p_errh));
}

static RS_AVALRET_T ftoflt(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_FLOAT);
        ss_dassert(SS_FLOAT_IS_PORTABLE(f));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA |
                          RA_ONLYCONVERTED))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.f = (float)f;
        return (RSAVR_SUCCESS);
}

RS_AVALRET_T rs_aval_setfloat_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_trapcode_t trapcode;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_FLOAT);
        trapcode = SS_TRAP_NONE;
        SS_TRAP_HANDLERSECTION
            case SS_TRAP_FPE:
            case SS_TRAP_FPE_INVALID:
            case SS_TRAP_FPE_INEXACT:
            case SS_TRAP_FPE_STACKFAULT:
            case SS_TRAP_FPE_STACKOVERFLOW:
            case SS_TRAP_FPE_STACKUNDERFLOW:
            case SS_TRAP_FPE_BOUND:
            case SS_TRAP_FPE_DENORMAL:
            case SS_TRAP_FPE_EXPLICITGEN:
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTOVFLOW:
            case SS_TRAP_FPE_OVERFLOW:
                rs_error_create(p_errh, E_NUMERICOVERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_INTDIV0:
            case SS_TRAP_FPE_ZERODIVIDE:
                rs_error_create(p_errh, E_DIVBYZERO);
                trapcode = SS_TRAP_GETCODE();
                break;
            case SS_TRAP_FPE_UNDERFLOW:
                rs_error_create(p_errh, E_NUMERICUNDERFLOW);
                trapcode = SS_TRAP_GETCODE();
                break;
            default:
                trapcode = SS_TRAP_GETCODE();
                ss_rc_derror(trapcode);
                rs_error_create(p_errh, E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE_EXPLICITGEN;
                break;
        SS_TRAP_RUNSECTION
            if (!SS_FLOAT_IS_PORTABLE(f)) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                trapcode = SS_TRAP_FPE;
            } else
#ifdef NO_ANSI_FLOAT
            if (f == -0.0) {
                f = dbl_zero;
            }
#else /* NO_ANSI_FLOAT */
            if (f == (float)-0.0) {
                f = flt_zero;
            }
#endif /* NO_ANSI_FLOAT */
        SS_TRAP_END
        if (trapcode != SS_TRAP_NONE) {
            return (RSAVR_FAILURE);
        }
        retc = ftoflt(cd, atype, aval, f, p_errh);
        return (retc);
}


static RS_AVALRET_T ftodbl(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d = (double)f;

        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA |
                          RA_ONLYCONVERTED))
        {
            aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_DOUBLE! */
            refdva_free(&aval->ra_va);
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        aval->ra_.d = d;
        return (RSAVR_SUCCESS);
}


static RS_AVALRET_T fillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f __attribute__ ((unused)),
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_REAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ftodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        dt_dfl_t dfl;
        bool succp;

        succp = dt_dfl_setdouble(&dfl, (double)f);
        if (succp) {
            retc = rs_aval_putdfl(cd, atype, aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ftouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
#ifdef NO_ANSI_FLOAT
        double f,
#else /* NO_ANSI_FLOAT */
        float f,
#endif /* NO_ANSI_FLOAT */
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_putdbltouni(
                    cd,
                    atype,
                    aval,
                    (double)f,
                    RS_REAL_DECPREC + 1);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_REAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}


static RS_AVALRET_T dttochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc = 0;
        bool succp;
        ss_char1_t buf[48];
        char* src_typename = NULL;

        succp = dt_date_datetoasciiz_sql(p_date, datesqltype, buf);
        if (succp) {
            size_t buf_len;
            ulong len;

            len = RS_ATYPE_LENGTH(cd, atype);
            buf_len = strlen(buf);

            if (len < sizeof(buf)/sizeof(buf[0])-1) {
                if (buf_len > len) {
                    switch (datesqltype) {
                        case DT_DATE_SQLTIME:
                            ss_dassert(buf_len == 8);
                            /* FALLTHROUGH */
                        case DT_DATE_SQLDATE:
                        case DT_DATE_SQLTYPE_UNKNOWN:
                            retc = RSAVR_FAILURE;
                            break;
                        case DT_DATE_SQLTIMESTAMP:
                            {
                                size_t minsize;
                                int year;

                                year = dt_date_year(p_date);
                                minsize = RS_TIMESTAMP_DISPSIZE_MIN;
                                if (year < 0) {
                                    minsize++;
                                    year = -year;
                                }
                                if (year > 9999) {
                                    minsize++;
                                }
                                if (len >= minsize) {
                                    retc = RSAVR_TRUNCATION;
                                    buf_len = len;
                                } else {
                                    retc = RSAVR_FAILURE;
                                }
                            }
                            break;
                        default:
                            ss_error;
                    }
                }
            } else {
                retc = RSAVR_SUCCESS;
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
                ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF | RA_NULL | RA_FLATVA))
                {
                    aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL |
                               RA_VTPLREF |
                               RA_CONVERTED |
                               RA_BLOB |
                               RA_FLATVA |
                               RA_UNKNOWN);
                AVAL_SETDATAANDNULL(cd, atype, aval, buf, buf_len);
                return (retc);
            }
        }
        switch (datesqltype) {
            case DT_DATE_SQLDATE:
                src_typename = (char*)RS_TN_DATE;
                break;
            case DT_DATE_SQLTIME:
                src_typename = (char*)RS_TN_TIME;
                break;
            case DT_DATE_SQLTIMESTAMP:
            case DT_DATE_SQLTYPE_UNKNOWN:
                src_typename = (char*)RS_TN_TIMESTAMP;
                break;
            default:
                ss_error;
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            src_typename,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}


static RS_AVALRET_T dtillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        dt_date_t* p_date __attribute__ ((unused)),
        dt_datesqltype_t datesqltype __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_TIMESTAMP,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}


RS_AVALRET_T rs_aval_setdate_raw(   /* dt_date_t */
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        bool succp;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DATE);

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_NULL | RA_VTPLREF | RA_FLATVA))
        {
            aval->ra_va = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_DATE! */
            refdva_free(&aval->ra_va);
        }
        succp = dt_date_datetova(p_date, &aval->ra_vabuf.va);
        ss_dassert(succp);
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_UNKNOWN);
        aval->ra_va = &aval->ra_vabuf.va;
        SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T dttodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc = RSAVR_SUCCESS;
        bool succp;
        rs_sqldatatype_t sqldatatype;
        dt_date_t tmp_date;
        char* src_typename = (char*)RS_TN_TIMESTAMP;

        sqldatatype = RS_ATYPE_SQLDATATYPE(cd, atype);
        succp = TRUE;
        switch (sqldatatype) {
            case RSSQLDT_DATE:
                switch (datesqltype) {
                    case DT_DATE_SQLDATE:
                        break;
                    case DT_DATE_SQLTIME:
                        src_typename = (char*)RS_TN_TIME;
                        succp = FALSE;
                        break;
                    case DT_DATE_SQLTIMESTAMP:
#if 0
                        tmp_date = *p_date;
#else /* 0 */
			memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                        if (dt_date_hour(&tmp_date) != 0
                        ||  dt_date_min(&tmp_date) != 0
                        ||  dt_date_sec(&tmp_date) != 0
                        ||  dt_date_fraction(&tmp_date) != 0UL)
                        {
                            retc = RSAVR_TRUNCATION;
                            succp = dt_date_truncatetodate(&tmp_date);
                        }
                        p_date = &tmp_date;
                        break;
                    case DT_DATE_SQLTYPE_UNKNOWN:
                        if (dt_date_istime(p_date)) {
                            src_typename = (char*)RS_TN_TIME;
                            succp = FALSE;
                        } else {
#if 0
                            tmp_date = *p_date;
#else /* 0 */
			    memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                            retc = RSAVR_TRUNCATION;
                            succp = dt_date_truncatetodate(&tmp_date);
                            p_date = &tmp_date;
                        }
                        break;
                    default:
                        ss_error;
                }
                break;
            case RSSQLDT_TIME:
                switch (datesqltype) {
                    case DT_DATE_SQLDATE:
                        src_typename = (char*)RS_TN_DATE;
                        succp = FALSE;
                        break;
                    case DT_DATE_SQLTIME:
                        break;
                    case DT_DATE_SQLTIMESTAMP:
                    case DT_DATE_SQLTYPE_UNKNOWN:
#if 0
			tmp_date = *p_date;
#else /* 0 */
			memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                        retc = RSAVR_TRUNCATION;
                        succp = dt_date_truncatetotime(&tmp_date);
                        p_date = &tmp_date;
                        break;
                    default:
                        ss_error;
                }
                break;
            case RSSQLDT_TIMESTAMP:
                switch (datesqltype) {
                    case DT_DATE_SQLDATE:
                        break;
                    case DT_DATE_SQLTIME:
#if 0
			tmp_date = *p_date;
#else /* 0 */
			memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                        succp = dt_date_padtimewithcurdate(&tmp_date);
                        p_date = &tmp_date;
                        break;
                    case DT_DATE_SQLTIMESTAMP:
                        break;
                    case DT_DATE_SQLTYPE_UNKNOWN:
                        if (dt_date_istime(p_date)) {
#if 0
			    tmp_date = *p_date;
#else /* 0 */
			    memcpy(&tmp_date, p_date, DT_DATE_DATASIZE);
#endif /* 0 */
                            succp = dt_date_padtimewithcurdate(&tmp_date);
                            p_date = &tmp_date;
                        }
                        break;
                    default:
                        ss_error;
                }
                break;
            default:
                ss_error;
        }
        if (succp) {
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF | RA_NULL | RA_FLATVA))
            {
                aval->ra_va = NULL;
            } else {
                ss_derror; /* va should never be dynamic for RSDT_DATE! */
                refdva_free(&aval->ra_va);
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_UNKNOWN);
            SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
            aval->ra_va = &aval->ra_vabuf.va;
            succp = dt_date_datetova(p_date, &aval->ra_vabuf.va);
            ss_dassert(succp);
            if (succp) {
                return (retc);
            }
            rs_aval_setnull(cd, atype, aval);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            src_typename,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dttouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        dt_datesqltype_t datesqltype,
        rs_err_t** p_errh)
{
        bool succp;
        ss_char1_t buf[48];
        char* src_typename = NULL;

        succp = dt_date_datetoasciiz_sql(p_date, datesqltype, buf);
        if (succp) {
            RS_AVALRET_T retc = 0;
            size_t buf_len;
            ulong len;
            len = RS_ATYPE_LENGTH(cd, atype);
            buf_len = strlen(buf);

            if (len < sizeof(buf)/sizeof(buf[0])-1) {
                if (buf_len > len) {
                    switch (datesqltype) {
                        case DT_DATE_SQLTIME:
                            ss_dassert(buf_len == 8);
                            /* FALLTHROUGH */
                        case DT_DATE_SQLDATE:
                            ss_debug(
                                if (datesqltype == DT_DATE_SQLDATE) {
                                    ss_assert(buf_len == 10);
                                }
                            );
                            /* FALLTHROUGH */
                        case DT_DATE_SQLTYPE_UNKNOWN:
                            retc = RSAVR_FAILURE;
                            break;
                        case DT_DATE_SQLTIMESTAMP:
                            if (len >= 19) {
                                retc = RSAVR_TRUNCATION;
                                buf_len = len;
                            } else {
                                retc = RSAVR_FAILURE;
                            }
                            break;
                        default:
                            ss_error;
                    }
                }
            } else {
                retc = RSAVR_SUCCESS;
            }
            RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF | RA_NULL | RA_FLATVA))
            {
                aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL |
                           RA_VTPLREF |
                           RA_CONVERTED |
                           RA_BLOB |
                           RA_FLATVA |
                           RA_UNKNOWN);
            AVAL_SETDATACHAR1TO2(cd, atype, aval, buf, buf_len);
            return (retc);
        }
        switch (datesqltype) {
            case DT_DATE_SQLDATE:
                src_typename = (char*)RS_TN_DATE;
                break;
            case DT_DATE_SQLTIME:
                src_typename = (char*)RS_TN_TIME;
                break;
            case DT_DATE_SQLTIMESTAMP:
            case DT_DATE_SQLTYPE_UNKNOWN:
                src_typename = (char*)RS_TN_TIMESTAMP;
                break;
            default:
                ss_error;
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            src_typename,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        bool succp;
        ss_char1_t buf[48];

        succp = dt_dfl_dfltoasciiz_maxlen(dfl, buf, sizeof(buf)/sizeof(buf[0]));
        if (succp) {
            RS_AVALRET_T retc;
            ulong len;
            size_t buf_len;

            len = RS_ATYPE_LENGTH(cd, atype);
            buf_len = strlen(buf);
            retc = RSAVR_SUCCESS;
            if (len > 0 && buf_len > len) {
                SsDoubleTruncateRetT rc;

                rc = SsTruncateAsciiDoubleValue(buf, len + 1);
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
                buf_len = strlen(buf);
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
                ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA))
                {
                    aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL |
                               RA_VTPLREF |
                               RA_CONVERTED |
                               RA_BLOB |
                               RA_FLATVA  |
                               RA_UNKNOWN);
                buf_len += 1;
                AVAL_SETDATA(cd, atype, aval, buf, buf_len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DECIMAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        bool succp;
        long l;

        succp = dt_dfl_dfltolong(dfl, &l);
        if (succp) {
            dt_dfl_t dfl2;

            succp = dt_dfl_setlong(&dfl2, l);
            if (succp) {
                RS_AVALRET_T retc = RSAVR_SUCCESS;
                RS_AVALRET_T retc2;

                if (dt_dfl_compare(dfl, dfl2) != 0) {
                    retc = RSAVR_TRUNCATION;
                }
                retc2 = rs_aval_putlong(cd, atype, aval, l);
                if (retc2 != RSAVR_FAILURE) {
                    return (retc);
                }
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        ss_char1_t  tmp_buf[24];
        bool succp;

        succp = dt_dfl_dfltoasciiz_maxlen(dfl, tmp_buf, sizeof(tmp_buf)-1);
        if (succp) {
            return (chtoint8(cd, atype, aval, tmp_buf, p_errh));
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftoflt(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        bool succp;
        double d;
        float f;

        succp = dt_dfl_dfltodouble(dfl, &d);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                f = (float)d;
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic with RSDT_FLOAT! */
                    refdva_free(&aval->ra_va);
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
                aval->ra_.f = f;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftodbl(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        bool succp;
        double d;

        succp = dt_dfl_dfltodouble(dfl, &d);
        if (succp) {
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF |
                              RA_NULL |
                              RA_FLATVA |
                              RA_ONLYCONVERTED))
            {
                aval->RA_RDVA = NULL;
            } else {
                ss_derror; /* va should never be dynamic with RSDT_DOUBLE! */
                refdva_free(&aval->ra_va);
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
            aval->ra_.d = d;
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dfillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        dt_dfl_t dfl __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DECIMAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

RS_AVALRET_T dftodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_DFLOAT);
        retc = rs_aval_putdfl(cd, atype, aval, &dfl);
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T dftouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t dfl,
        rs_err_t** p_errh)
{
        bool succp;
        RS_AVALRET_T retc;
        ss_char1_t buf[48];

        succp = dt_dfl_dfltoasciiz_maxlen(dfl, buf, sizeof(buf)/sizeof(buf[0]));
        if (succp) {
            size_t buf_len;
            long len;

            len = RS_ATYPE_LENGTH(cd, atype);
            buf_len = strlen(buf);
            retc = RSAVR_SUCCESS;
            if (len > 0 && (size_t)len < buf_len) {
                SsDoubleTruncateRetT rc;

                rc = SsTruncateAsciiDoubleValue(buf, len + 1);
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
                buf_len = strlen(buf);
            }
            if (retc != RSAVR_FAILURE) {
                RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
                ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA))
                {
                    aval->RA_RDVA = NULL;
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL |
                               RA_VTPLREF |
                               RA_CONVERTED |
                               RA_BLOB |
                               RA_FLATVA |
                               RA_UNKNOWN);
                AVAL_SETDATACHAR1TO2(cd, atype, aval, buf, buf_len);
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_DECIMAL,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}


static RS_AVALRET_T uctochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        size_t len;
        RS_AVALRET_T retc;

        len = SsWcslen(wcs);
        retc = ubtochar(cd, atype, aval, wcs, len, p_errh);
        return (retc);
}

static RS_AVALRET_T uctoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        long l;
        ss_char2_t* mismatch;
        RS_AVALRET_T retc;
        bool succp;

        succp = SsWcsScanLong(wcs, &l, &mismatch);
        if (succp) {
            retc = rs_aval_putlong(cd, atype, aval, l);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
            rs_error_create(
                p_errh,
                E_NUMERICOVERFLOW);
            return (RSAVR_FAILURE);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T uctoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_char1_t* p;
        ss_char1_t* mismatch;
        ss_int8_t i8;

        p = SsMemAlloc(SsWcslen(wcs)+1);

        if (SsWcs2Str(p, wcs)) {
            if (SsStrScanInt8(p, &i8, &mismatch)) {
                retc = rs_aval_putint8(cd, atype, aval, i8);
                if (retc != RSAVR_FAILURE) {
                    SsMemFree(p);
                    return (retc);
                }
                SsMemFree(p);
                rs_error_create(
                    p_errh,
                    E_NUMERICOVERFLOW);
                return (RSAVR_FAILURE);
            }
        }
        SsMemFree(p);
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T uctoflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        double d;
        ss_char2_t* mismatch;
        bool succp;

        succp = SsWcsScanDouble(wcs, &d, &mismatch);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                float f;
                f = (float)d;
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic for RSDT_FLOAT! */
                    refdva_free(&aval->ra_va);
                }
                SU_BFLAG_CLEAR(aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
                aval->ra_.f = f;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T uctodbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        double d;
        ss_char2_t* mismatch;
        bool succp;

        succp = SsWcsScanDouble(wcs, &d, &mismatch);
        if (succp) {
            succp = SS_DOUBLE_IS_PORTABLE(d);
            if (succp) {
                if (SU_BFLAG_TEST(aval->ra_flags,
                                  RA_VTPLREF |
                                  RA_NULL |
                                  RA_FLATVA |
                                  RA_ONLYCONVERTED))
                {
                    aval->RA_RDVA = NULL;
                } else {
                    ss_derror; /* va should never be dynamic for RSDT_DOUBLE! */
                    refdva_free(&aval->ra_va);
                }
                SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
                SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
                aval->ra_.d = d;
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T uctodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = SsWcslen(wcs);
        retc = ubtodate(cd, atype, aval, wcs, len, p_errh);
        return (retc);
}

static RS_AVALRET_T uctodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = SsWcslen(wcs);
        retc = ubtodfl(cd, atype, aval, wcs, len, p_errh);
        return (retc);
}

static RS_AVALRET_T uctobin(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = SsWcslen(wcs);
        retc = ubtobin(cd, atype, aval, wcs, len, p_errh);
        return (retc);
}

RS_AVALRET_T uctouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* wcs,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_UNICODE);
        len = SsWcslen(wcs);
        retc = ubtouni(cd, atype, aval, wcs, len, p_errh);
        return (retc);
}


static RS_AVALRET_T ubtochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        bool succp;
        RS_AVALRET_T retc;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        if (bufsize > len) {
            retc = RSAVR_TRUNCATION;
            bufsize = (size_t)len;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        succp = AVAL_SETDATACHAR2TO1(cd, atype, aval, buf, bufsize);

        if (succp) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T aval_ubcallbackframe(
        aval_setwcsfun_t p_fun,
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        ss_char2_t tmp_buf[48];
        ss_char2_t* p_buf;
        RS_AVALRET_T retc;

        if (bufsize < sizeof(tmp_buf)/sizeof(tmp_buf[0])) {
            p_buf = tmp_buf;
        } else {
            p_buf = SsMemAlloc((bufsize + 1) * sizeof(ss_char2_t));
        }
        memcpy(p_buf, buf, bufsize * sizeof(ss_char2_t));
        p_buf[bufsize] = (ss_char2_t)0;
        retc = (*p_fun)(cd, atype, aval, p_buf, p_errh);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        return (retc);
}

static RS_AVALRET_T ubtoint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_ubcallbackframe(
                    uctoint,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T ubtoint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_ubcallbackframe(
                    uctoint8,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T ubtoflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_ubcallbackframe(
                    uctoflt,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T ubtodbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = aval_ubcallbackframe(
                    uctodbl,
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    p_errh);
        return (retc);
}

static RS_AVALRET_T ubtodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        size_t len;
        ss_char1_t tmp_buf[48];
        ss_char1_t* p_buf;
        RS_AVALRET_T retc;
        bool succp __attribute__ ((unused));

        for (len = 0; len < bufsize; len++) {
            if (!ss_isw8bit(buf[len])) {
                break;
            }
            /* measure length till 1st non-1byte char */
        }
        if (len < sizeof(tmp_buf)) {
            p_buf = tmp_buf;
        } else {
            p_buf = SsMemAlloc(len + 1);
        }
        ss_debug(succp =) SsWbuf2Str(p_buf, buf, len);
        ss_dassert(succp);
        p_buf[len] = '\0';
        retc = chtodate(cd, atype, aval, p_buf, p_errh);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        if (p_errh != NULL) {
            rs_error_free(cd, *p_errh);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ubtodfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        size_t len;
        ss_char1_t tmp_buf[48];
        ss_char1_t* p_buf;
        RS_AVALRET_T retc;
        bool succp __attribute__ ((unused));

        for (len = 0; len < bufsize; len++) {
            if (!ss_isw8bit(buf[len])) {
                break;
            }
            /* measure length till 1st non-1byte char */
        }
        if (len < sizeof(tmp_buf)) {
            p_buf = tmp_buf;
        } else {
            p_buf = SsMemAlloc(len + 1);
        }
        ss_debug(succp =) SsWbuf2Str(p_buf, buf, len);
        ss_dassert(succp);
        p_buf[len] = '\0';
        retc = chtodfl(cd, atype, aval, p_buf, NULL);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        if (retc != RSAVR_FAILURE) {
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T ubtobin(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        if ((bufsize & 1) == 0) {
            RS_AVALRET_T retc;
            bool succp = TRUE;
            ss_byte_t* destdata;
            va_index_t destlen;
            ulong len;

            len = RS_ATYPE_LENGTH(cd, atype);
            if (bufsize / 2 > len) {
                bufsize = len * 2;
                retc = RSAVR_TRUNCATION;
            } else {
                retc = RSAVR_SUCCESS;
            }
            RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF |
                              RA_NULL |
                              RA_FLATVA))
            {
                aval->RA_RDVA = NULL;
            }
            SU_BFLAG_CLEAR(aval->ra_flags,
                           RA_NULL |
                           RA_VTPLREF |
                           RA_CONVERTED |
                           RA_BLOB |
                           RA_FLATVA |
                           RA_UNKNOWN);
            destlen = bufsize / 2;
            AVAL_SETDATAANDNULL(cd, atype, aval, NULL, destlen);
            destdata = va_getdata(aval->RA_RDVA, &destlen);
            ss_dassert(destlen == (bufsize / 2) + 1);
            if (destlen > 1) {
                succp = su_chcvt_hex2binchar2(destdata, buf, destlen - 1);
            }
            ss_dassert(destdata[destlen-1] == '\0');
            if (succp) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_WCHAR,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

RS_AVALRET_T ubtouni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ulong len;
        RS_AVALRET_T retc;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_UNICODE);
        len = RS_ATYPE_LENGTH(cd, atype);
        if (len > 0 && len < bufsize) {
            retc = RSAVR_TRUNCATION;
            bufsize = (size_t)len;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL |
                       RA_VTPLREF |
                       RA_CONVERTED |
                       RA_BLOB |
                       RA_FLATVA |
                       RA_UNKNOWN);
        AVAL_SETDATACHAR2(cd, atype, aval, buf, bufsize);
        return (retc);
}

/*##**********************************************************************\
 *
 *		rs_aval_setUTF8data_raw
 *
 * Sets UTF-8 data to aval which MUST be of RSDT_UNICODE base type
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	buf - in, use
 *		input buffer in UTF8 format
 *
 *	bufsize - in, use
 *		input buffer size in bytes ('\0'-byte excluded)
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - the input buffer was not legal UTF-8 data
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setUTF8data_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        ulong len;
        ulong chlen;
        RS_AVALRET_T retc;
        ss_char2_t* dst;
        ss_char2_t* dst_tmp;
        ss_byte_t* buf_tmp;
        va_index_t dst_bytelen;
        SsUtfRetT utfrc;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_UNICODE);

        chlen = SsUTF8CharLen((ss_byte_t*)buf, bufsize);
        len = RS_ATYPE_LENGTH(cd, atype);
        if (len < chlen) {
            retc = RSAVR_TRUNCATION;
            chlen = len;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_FLATVA | RA_UNKNOWN);
        dst_bytelen = chlen * sizeof(ss_char2_t);
        AVAL_SETDATAANDNULL(cd, atype, aval, NULL, dst_bytelen);
        dst = va_getdata(aval->RA_RDVA, &dst_bytelen);
        ss_dassert(dst_bytelen == chlen * sizeof(ss_char2_t) + 1);
        ss_dassert(((ss_byte_t*)dst)[dst_bytelen - 1] == '\0');

        dst_tmp = dst;
        buf_tmp = (ss_byte_t*)buf;
        utfrc = SsUTF8toUCS2va(
                    &dst_tmp,
                    dst + chlen,
                    &buf_tmp,
                    (ss_byte_t*)buf + bufsize);
        ss_dassert(((ss_byte_t*)dst)[dst_bytelen - 1] == '\0');
#ifdef SS_DEBUG
        if (utfrc == SS_UTF_OK) {
            ss_assert(retc == RSAVR_SUCCESS);
        } else if (utfrc == SS_UTF_TRUNCATION) {
            ss_assert(retc == RSAVR_TRUNCATION);
        }
#endif /* SS_DEBUG */
        if (utfrc == SS_UTF_ERROR) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                "UTF8 buffer",
                rs_atype_name(cd, atype));
            retc = RSAVR_FAILURE;
        }
        return (retc);
}

static RS_AVALRET_T rs_aval_setUTF8datatochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        ulong len;
        ulong chlen;
        RS_AVALRET_T retc;
        ss_char1_t* dst;
        ss_char1_t* dst_tmp;
        ss_byte_t* buf_tmp;
        va_index_t dst_bytelen;
        SsUtfRetT utfrc;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_CHAR);

        chlen = SsUTF8CharLen((ss_byte_t*)buf, bufsize);
        len = RS_ATYPE_LENGTH(cd, atype);
        if (len < chlen) {
            retc = RSAVR_TRUNCATION;
            chlen = len;
        } else {
            retc = RSAVR_SUCCESS;
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATAANDNULL(cd, atype, aval, NULL, chlen);
        dst = va_getdata(aval->RA_RDVA, &dst_bytelen);
        ss_dassert(dst_bytelen == chlen + 1);
        ss_dassert(((ss_byte_t*)dst)[dst_bytelen - 1] == '\0');

        dst_tmp = dst;
        buf_tmp = (ss_byte_t*)buf;
        utfrc = SsUTF8toASCII8(
                    &dst_tmp,
                    dst + chlen,
                    &buf_tmp,
                    (ss_byte_t*)buf + bufsize);
        ss_dassert(((ss_byte_t*)dst)[dst_bytelen - 1] == '\0');
#ifdef SS_DEBUG
        if (utfrc == SS_UTF_OK || utfrc == SS_UTF_NOCHANGE) {
            ss_assert(retc == RSAVR_SUCCESS);
        } else if (utfrc == SS_UTF_TRUNCATION) {
            ss_assert(retc == RSAVR_TRUNCATION);
        }
#endif /* SS_DEBUG */
        if (utfrc == SS_UTF_ERROR) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                "UTF8 buffer",
                rs_atype_name(cd, atype));
            retc = RSAVR_FAILURE;
        }
        return (retc);
}

#define U8touni ((aval_setcharbuffun_t*)rs_aval_setUTF8data_raw)
#define U8tochar ((aval_setcharbuffun_t*)rs_aval_setUTF8datatochar)

static aval_setcharbuffun_t* const aval_setUTF8bufarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        U8tochar,   cbtoint,    cbtoflt,    cbtodbl,    cbtodate,   cbtodfl,    cbtobin,    U8touni, cbtoint8
};

/*##**********************************************************************\
 *
 *		rs_aval_setUTF8data_ext
 *
 * Sets UTF-8 value (no '\0' -termination) to aval
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - use
 *		attribute value
 *
 *	buf - in, use
 *		input buffer
 *
 *	bufsize - in, use
 *		input buffer size in bytes ('\0'-byte excluded)
 *
 *	p_errh - out, give
 *		pointer to error handle or NULL if not wanted
 *
 *
 * Return value :
 *      RSAVR_SUCCESS - OK
 *      RSAVR_TRUNCATION - the data type is not long enough to hold the
 *                     result (right truncation)
 *      RSAVR_FAILURE - the input buffer could not be converted to the
 *                      requested data type
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
RS_AVALRET_T rs_aval_setUTF8data_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        rs_datatype_t dt;
        RS_AVALRET_T retc;

        dt = RS_ATYPE_DATATYPE(cd, atype);
        ss_dassert((uint)dt < RSDT_DIMENSION);
        retc = (*aval_setUTF8bufarr[dt])(
                    cd,
                    atype, aval,
                    buf, bufsize,
                    p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setUTF8str_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = strlen(str);
        retc = rs_aval_setUTF8data_ext(
                cd,
                atype, aval,
                str, len,
                p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setUTF8str_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        size_t len;

        len = strlen(str);
        retc = rs_aval_setUTF8data_raw(
                cd,
                atype, aval,
                str, len,
                p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setstr_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_setUTF8str_ext(
                    cd,
                    atype, aval,
                    str,
                    p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setstr_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* str,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        ss_dassert(rs_atype_datatype(cd, atype) == RSDT_UNICODE);
        retc = rs_aval_setUTF8str_raw(
                    cd,
                    atype, aval,
                    str,
                    p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setcdata_ext(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_setUTF8data_ext(
                    cd,
                    atype, aval,
                    buf, bufsize,
                    p_errh);
        return (retc);
}

RS_AVALRET_T rs_aval_setcdata_raw(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_setUTF8data_raw(
                    cd,
                    atype, aval,
                    buf, bufsize,
                    p_errh);
        return (retc);
}


/* BIGINT TO XXXX routines */

RS_AVALRET_T i8toint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        RS_AVALRET_T retc;

        retc = rs_aval_putint8(cd, atype, aval, i8);
        ss_dassert (retc != RSAVR_FAILURE);
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T i8tochar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        ss_char1_t buf[24];
        size_t buf_len;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        buf_len = SsInt8ToAscii(i8, buf, RS_BIGINT_RADIX, 0, '0', TRUE);
        ss_dassert(buf_len < sizeof(buf)/sizeof(buf[0]));
        if (len < buf_len) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                RS_TN_BIGINT,
                rs_atype_name(cd, atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        buf_len += 1;
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB |
                       RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATA(cd, atype, aval, buf, buf_len);
        return (RSAVR_SUCCESS);
}

RS_AVALRET_T i8toint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_int4_t i4;

        if (SsInt8ConvertToInt4(&i4, i8)) {
            retc = rs_aval_putlong(cd, atype, aval, (long)i4);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T i8toflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        double d;

        if (SsInt8ConvertToDouble(&d, i8)) {
            return (dtoflt(cd, atype, aval, d, p_errh));
        }
        ss_derror;
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T i8todbl(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        double d;

        if (SsInt8ConvertToDouble(&d, i8)) {
            if (SU_BFLAG_TEST(aval->ra_flags,
                              RA_VTPLREF |
                              RA_NULL |
                              RA_FLATVA |
                              RA_ONLYCONVERTED))
            {
                aval->RA_RDVA = NULL;
            } else {
                ss_derror; /* va should never be dynamic with RSDT_DOUBLE! */
                refdva_free(&aval->ra_va);
            }
            SU_BFLAG_CLEAR(aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
            SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
            aval->ra_.d = d;
            return (RSAVR_SUCCESS);
        }
        ss_derror;
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T i8illegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        ss_int8_t i8 __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_BIGINT,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);
}

static RS_AVALRET_T i8todfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_char1_t  tmp_buf[24];
        dt_dfl_t dfl;

        SsInt8ToAscii(i8, tmp_buf, RS_BIGINT_RADIX, 0, '0', TRUE);
        if (dt_dfl_setasciiz(&dfl, tmp_buf)) {
            retc = rs_aval_putdfl(cd, atype, aval, &dfl);
            if (retc != RSAVR_FAILURE) {
                return (retc);
            }
            ss_derror;
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            RS_TN_BIGINT,
            rs_atype_name(cd, atype));
        return (RSAVR_FAILURE);

}

static RS_AVALRET_T i8tovarbinary(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        ss_char1_t buf[9];
        size_t buf_len;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);
        /* The presentation of ss_int8_t varies, but let's have the varbinary
           in exactly the right order. */
        buf[0] = SsInt8GetNthOrderByte(i8, 7);
        buf[1] = SsInt8GetNthOrderByte(i8, 6);
        buf[2] = SsInt8GetNthOrderByte(i8, 5);
        buf[3] = SsInt8GetNthOrderByte(i8, 4);
        buf[4] = SsInt8GetNthOrderByte(i8, 3);
        buf[5] = SsInt8GetNthOrderByte(i8, 2);
        buf[6] = SsInt8GetNthOrderByte(i8, 1);
        buf[7] = SsInt8GetNthOrderByte(i8, 0);
        buf_len = 8;
        buf[8] = 0;
        ss_dassert(buf_len < sizeof(buf)/sizeof(buf[0]));
        if (len < buf_len) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                RS_TN_BIGINT,
                rs_atype_name(cd, atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA)) {
            aval->RA_RDVA = NULL;
        }
        buf_len += 1;
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATA(cd, atype, aval, buf, buf_len);
        return (RSAVR_SUCCESS);
}

static RS_AVALRET_T i8touni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t i8,
        rs_err_t** p_errh)
{
        ss_char1_t buf[24];
        size_t buf_len;
        ulong len;

        len = RS_ATYPE_LENGTH(cd, atype);

        /* Maybe we should write SsInt8tToWcs... */
        buf_len = SsInt8ToAscii(i8, buf, RS_BIGINT_RADIX, 0, '0', TRUE);

        ss_dassert(buf_len < sizeof(buf)/sizeof(buf[0]));
        if (len < buf_len) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                RS_TN_INTEGER,
                rs_atype_name(cd, atype));
            return (RSAVR_FAILURE);
        }
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
        if (SU_BFLAG_TEST(aval->ra_flags,
                          RA_VTPLREF |
                          RA_NULL |
                          RA_FLATVA))
        {
            aval->RA_RDVA = NULL;
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_BLOB | RA_FLATVA | RA_UNKNOWN);
        AVAL_SETDATACHAR1TO2(cd, atype, aval, buf, buf_len);
        return (RSAVR_SUCCESS);
}
