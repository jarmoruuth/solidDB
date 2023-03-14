/*************************************************************************\
**  source       * rs1avget.c
**  directory    * res
**  description  * aval get functions for converting aval
**               * object values to C data types
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
#include <sswcs.h>
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
#include <sswscan.h>
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

#include "rs1aval.h"
#include "rs2avcvt.h"

typedef rs_avalret_t aval_copy8bitstrfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);

typedef rs_avalret_t aval_copyUTF8fun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh);

typedef rs_avalret_t aval_getlongfunc_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh);

typedef bool aval_getfloatfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh);

typedef bool aval_getdoublefun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh);

typedef bool aval_getdfloatfun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh);

typedef rs_avalret_t aval_copyunicodefun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh);

typedef rs_avalret_t aval_getdatefun_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh);

typedef rs_avalret_t aval_getint8func_t(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh);

static aval_copy8bitstrfun_t
        chfromchar,
        chfromint,
        chfromflt,
        chfromdbl,
        chfromdate,
        chfromdfl,
        chfrombin,
        chfromuni,
        chfromint8;

static aval_copyUTF8fun_t
        U8fromchar,
        U8fromuni;

static aval_getlongfunc_t
        lfromchar,
        lfromint,
        lfromflt,
        lfromdbl,
        lillegal,
        lfromdfl,
        lfromuni,
        lfromint8;

static aval_getfloatfun_t
        ffromchar,
        ffromint,
        ffromflt,
        ffromdbl,
        fillegal,
        ffromdfl,
        ffromuni,
        ffromint8;

static aval_getdoublefun_t
        dfromchar,
        dfromint,
        dfromflt,
        dfromdbl,
        dillegal,
        dfromdfl,
        dfromuni,
        dfromint8;

static aval_getdfloatfun_t
        dffromchar,
        dffromint,
        dffromflt,
        dffromdbl,
        dfillegal,
        dffromdfl,
        dffromuni,
        dffromint8;

static aval_copyunicodefun_t
        ucfromchar,
        ucfromint,
        ucfromflt,
        ucfromdbl,
        ucfromdate,
        ucfromdfl,
        ucfrombin,
        ucfromuni,
        ucfromint8;

static aval_getdatefun_t
        dtfromchar,
        dtillegal,
        dtfromdate,
        dtfromuni;

static aval_getdatefun_t
        tmfromchar,
        tmillegal,
        tmfromdate,
        tmfromuni;

static aval_getdatefun_t
        tsfromchar,
        tsillegal,
        tsfromdate,
        tsfromuni;

static aval_getint8func_t
        i8fromchar,
        i8fromint,
        i8fromflt,
        i8fromdbl,
        i8illegal,
        i8fromdfl,
        i8fromuni,
        i8fromint8;

static aval_getlongfunc_t* const aval_getlongarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        lfromchar,  lfromint,   lfromflt,   lfromdbl,   lillegal,   lfromdfl,   lillegal,   lfromuni,   lfromint8
};

static aval_getfloatfun_t* const aval_getfloatarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        ffromchar,  ffromint,   ffromflt,   ffromdbl,   fillegal,   ffromdfl,   fillegal,   ffromuni,   ffromint8
};

static aval_getdoublefun_t* const aval_getdoublearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dfromchar,  dfromint,   dfromflt,   dfromdbl,   dillegal,   dfromdfl,   dillegal,   dfromuni,   dfromint8
};

static aval_getdfloatfun_t* const aval_getdfloatarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dffromchar, dffromint,  dffromflt,  dffromdbl,  dfillegal,  dffromdfl,  dfillegal,  dffromuni,  dffromint8
};

static aval_copy8bitstrfun_t* const aval_copy8bitstrarr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        chfromchar,chfromint,   chfromflt,  chfromdbl,  chfromdate, chfromdfl,  chfrombin,  chfromuni,  chfromint8
};

static aval_copyunicodefun_t* const aval_copyunicodearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        ucfromchar,ucfromint,   ucfromflt,  ucfromdbl,  ucfromdate, ucfromdfl,  ucfrombin,  ucfromuni,  ucfromint8
};

static aval_getdatefun_t* const aval_getdatearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        dtfromchar,dtillegal,   dtillegal,  dtillegal,  dtfromdate, dtillegal,  dtillegal,  dtfromuni,  dtillegal
};

static aval_getdatefun_t* const aval_gettimearr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        tmfromchar,tmillegal,   tmillegal,  tmillegal,  tmfromdate, tmillegal,  tmillegal,  tmfromuni,  tmillegal
};

static aval_getdatefun_t* const aval_gettimestamparr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        tsfromchar,tsillegal,   tsillegal,  tsillegal,  tsfromdate, tsillegal,  tsillegal,  tsfromuni,  tsillegal
};

static aval_getint8func_t* const aval_getint8arr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
        i8fromchar,  i8fromint,   i8fromflt,   i8fromdbl,   i8illegal,   i8fromdfl,   i8illegal,   i8fromuni, i8fromint8
};

static rs_avalret_t lfromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        s = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanLong(s, p_long, &mismatch);
        if (succp) {
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_INTEGER);
        return (RSAVR_FAILURE);
}

static rs_avalret_t lfromint(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_long = rs_aval_getlong(cd, atype, aval);
        return (RSAVR_SUCCESS);
}

static rs_avalret_t lfromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        float f;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        f = rs_aval_getfloat(cd, atype, aval);
        *p_long = (long)f;
        if ((float)*p_long != f) {
            retc = RSAVR_TRUNCATION;
        }
        if (f <= ((float)SS_INT4_MIN - (float)1)
        ||  f >= ((float)SS_INT4_MAX + (float)1))
        {
            rs_error_create(
                p_errh,
                E_NUMERICOVERFLOW);
            return (RSAVR_FAILURE);
        }
        return (retc);
}

static rs_avalret_t lfromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        double d;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        d = rs_aval_getdouble(cd, atype, aval);
        *p_long = (long)d;
        if ((double)*p_long != d) {
            retc = RSAVR_TRUNCATION;
        }
        if (d <= ((double)SS_INT4_MIN - (double)1)
        ||  d >= ((double)SS_INT4_MAX + (double)1))
        {
            rs_error_create(
                p_errh,
                E_NUMERICOVERFLOW);
            return (RSAVR_FAILURE);
        }
        return (retc);
}

static rs_avalret_t lillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        long* p_long __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_INTEGER);
        return (RSAVR_FAILURE);
}


static rs_avalret_t lfromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        dt_dfl_t dfl;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dfl = rs_aval_getdfloat(cd, atype, aval);
        succp = dt_dfl_dfltolong(dfl, p_long);
        if (succp) {
            dt_dfl_t dfl2;
            rs_avalret_t retc = RSAVR_SUCCESS;

            succp = dt_dfl_setlong(&dfl2, *p_long);
            if (succp) {
                if (dt_dfl_compare(dfl, dfl2) != 0) {
                    retc = RSAVR_TRUNCATION;
                }
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static rs_avalret_t lfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        ss_char2_t tmp_buf[48];
        ss_char2_t* p_buf;
        ss_char2_t* mismatch;
        va_index_t datalen;
        size_t n_copied;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        datalen = va_netlen(aval->RA_RDVA);
        ss_dassert(datalen > 0);
        if (datalen / sizeof(ss_char2_t) >= sizeof(tmp_buf)/sizeof(tmp_buf[0])) {
            p_buf = SsMemAlloc((datalen / sizeof(ss_char2_t) + 1) * sizeof(ss_char2_t));
        } else {
            p_buf = tmp_buf;
        }
        va_copydatachar2(
            aval->RA_RDVA,
            p_buf,
            0,
            datalen / sizeof(ss_char2_t),
            &n_copied);
        ss_dassert(n_copied == datalen / sizeof(ss_char2_t));
        p_buf[n_copied] = (ss_char2_t)0;
        succp = SsWcsScanLong(p_buf, p_long, &mismatch);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        if (succp) {
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_INTEGER);
        return (RSAVR_FAILURE);
}

static rs_avalret_t lfromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        ss_int8_t i8;
        ss_int4_t i4;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));

        i8 = rs_aval_getint8(cd, atype, aval);
        if (SsInt8ConvertToInt4(&i4, i8)) {
            *p_long = (long)i4;
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_INTEGER);
        return (RSAVR_FAILURE);
}

static bool ffromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        double d;
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        s = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanDouble(s, &d, &mismatch);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (!succp) {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                return (RSAVR_FAILURE);
            }
        }
        if (succp) {
            *p_float = (float)d;
            return (TRUE);
        } 
        *p_float = (float)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_REAL);
        return (FALSE);
}

static bool ffromint(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        long l;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        l = rs_aval_getlong(cd, atype, aval);
        *p_float = (float)l;
        return (TRUE);
}

static bool ffromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_float = rs_aval_getfloat(cd, atype, aval);;
        return (TRUE);
}

static bool ffromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        double d;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        d = rs_aval_getdouble(cd, atype, aval);
        succp = SS_FLOAT_IS_PORTABLE(d);
        if (succp) {
            *p_float = (float)d;
            return (TRUE);
        }
        *p_float = (float)0;
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (FALSE);
}

static bool fillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        float* p_float,
        rs_err_t** p_errh)
{
        *p_float = (float)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_REAL);
        return (FALSE);
}

static bool ffromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        double d;
        dt_dfl_t dfl;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dfl = rs_aval_getdfloat(cd, atype, aval);
        succp = dt_dfl_dfltodouble(dfl, &d);
        if (succp) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                *p_float = (float)d;
                return (TRUE);
            } else {
                rs_error_create(
                    p_errh,
                    E_NUMERICOUTOFRANGE);
                return (FALSE);
            }
        }
        *p_float = (float)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_REAL);
        return (FALSE);
}


static bool ffromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        double d;
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, atype, aval, &len);
        if (tmp_buf != NULL) {
            succp = SsStrScanDouble(tmp_buf, &d, &mismatch);
            if (succp) {
                succp = SS_FLOAT_IS_PORTABLE(d);
                if (succp) {
                    *p_float = (float)d;
                }
            }
            SsMemFree(tmp_buf);
            if (succp) {
                return(TRUE);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_REAL);
        return (FALSE);
}

static bool ffromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        ss_int8_t i8;
        double d;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        i8 = rs_aval_getint8(cd, atype, aval);
        if (SsInt8ConvertToDouble(&d, i8)) {
            succp = SS_FLOAT_IS_PORTABLE(d);
            if (succp) {
                *p_float = (float)d;
                return (TRUE);
            }
        }
        *p_float = (float)0;
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (FALSE);
}

static bool dfromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh)
{
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        s = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanDouble(s, p_double, &mismatch);
        if (succp) {
            return (TRUE);
        } 
        *p_double = (double)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DOUBLE_PRECISION);
        return (FALSE);
}

static bool dfromint(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        long l;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        l = rs_aval_getlong(cd, atype, aval);
        *p_double = (double)l;
        return (TRUE);
}

static bool dfromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_double = (double)rs_aval_getfloat(cd, atype, aval);
        return (TRUE);
}

static bool dfromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_double = rs_aval_getdouble(cd, atype, aval);
        return (TRUE);
}

static bool dillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        double* p_double,
        rs_err_t** p_errh)
{
        *p_double = (double)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DOUBLE_PRECISION);
        return (FALSE);
}

static bool dfromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh)
{
        dt_dfl_t dfl;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dfl = rs_aval_getdfloat(cd, atype, aval);
        succp = dt_dfl_dfltodouble(dfl, p_double);
        if (succp) {
            return (TRUE);
        }
        *p_double = (double)0;
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DOUBLE_PRECISION);
        return (FALSE);
}


static bool dfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh)
{
        ss_char1_t* mismatch;
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, atype, aval, &len);
        if (tmp_buf != NULL) {
            succp = SsStrScanDouble(tmp_buf, p_double, &mismatch);
            SsMemFree(tmp_buf);
            if (succp) {
                return(TRUE);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DOUBLE_PRECISION);
        return (FALSE);
}

static bool dfromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh)
{
        ss_int8_t i8;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));

        i8 = rs_aval_getint8(cd, atype, aval);
        if (SsInt8ConvertToDouble(p_double, i8)) {
            return (TRUE);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DOUBLE_PRECISION);
        return (FALSE);
}

static bool dffromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        ss_char1_t* s;
        va_index_t datalen;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        s = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = dt_dfl_setasciiz(p_dfl, s);
        if (succp) {
            return (TRUE);
        } 
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DECIMAL);
        return (FALSE);
}

static bool dffromint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        long l;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        l = rs_aval_getlong(cd, atype, aval);
        succp = dt_dfl_setlong(p_dfl, l);
        if (succp) {
            return (TRUE);
        } 
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DECIMAL);
        return (FALSE);
}

static bool dffromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        double d;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        d = (double)rs_aval_getfloat(cd, atype, aval);
        succp = dt_dfl_setdouble(p_dfl, d);
        if (succp) {
            return (TRUE);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (FALSE);
}

static bool dffromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        double d;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        d = rs_aval_getdouble(cd, atype, aval);
        succp = dt_dfl_setdouble(p_dfl, d);
        if (succp) {
            return (TRUE);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (FALSE);
}

static bool dfillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        dt_dfl_t* p_dfl __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DECIMAL);
        return (FALSE);
}

static bool dffromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_dfl = rs_aval_getdfloat(cd, atype, aval);
        return (TRUE);
}


static bool dffromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        ss_char1_t* tmp_buf;
        bool succp;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, atype, aval, &len);
        if (tmp_buf != NULL) {
            succp = dt_dfl_setasciiz(p_dfl, tmp_buf);
            SsMemFree(tmp_buf);
            if (succp) {
                return(TRUE);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_DECIMAL);
        return (FALSE);
}

static bool dffromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        ss_char1_t  tmp_buf[24];
        size_t      len;
        ss_int8_t i8;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        i8 = rs_aval_getint8(cd, atype, aval);

        len = SsInt8ToAscii(i8, tmp_buf, RS_BIGINT_RADIX, 0, '0', TRUE);
        succp = dt_dfl_setasciiz(p_dfl, tmp_buf);
        if (succp) {
            return (TRUE);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOUTOFRANGE);
        return (FALSE);
}

static rs_avalret_t chfromchar(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_char1_t* p;
        va_index_t valen;
        size_t len;
        size_t padlen;
        size_t ntocopy;
        rs_avalret_t retc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        p = va_getdata(aval->RA_RDVA, &valen);
        ss_dassert(ntoskip < valen);
        p += ntoskip;
        len = valen - ntoskip;
        padlen = 0;
        if (bufsize < len) {
            ss_dassert(padlen == 0);
            retc = RSAVR_TRUNCATION;
            ntocopy = bufsize - 1;
            buf[bufsize - 1] = '\0';
        } else {
            ntocopy = len;
            if (RS_ATYPE_SQLDATATYPE(cd, atype) == RSSQLDT_CHAR) {
                if (len < bufsize) {
                    ulong type_len = RS_ATYPE_LENGTH(cd, atype);
                    if (type_len >= valen) {
                        padlen = type_len - valen + 1;
                        if (padlen + len > bufsize) {
                            padlen = bufsize - len;
                        }
                        ss_dassert(padlen > 0);
                        ntocopy--;
                    }
                }
            }
            retc = RSAVR_SUCCESS;
        }
        *p_totalsize = len + padlen - 1;
        memcpy(buf, p, ntocopy);
        if (padlen != 0) {
            memset(buf + len - 1, ' ', padlen);
            buf[len + padlen - 1] = '\0';
        }
        return (retc);
}


static rs_avalret_t chfromint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        long l;
        ss_char1_t tmp_buf[12];
        size_t     len;

        ss_dassert(ntoskip == 0);
        if (ntoskip == 0) {
            l = rs_aval_getlong(cd, atype, aval);
            len = SsLongToAscii(l, tmp_buf, 10, 0, '0', TRUE);
            if (len < bufsize) {
                *p_totalsize = len;
                memcpy(buf, tmp_buf, len + 1);
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t aval_chfromdouble(
        void* cd,
        rs_atype_t* atype,
        double d,
        size_t precision,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        ss_char1_t tmp_buf[40];
        size_t     len;
        size_t     print_size;
        bool need_to_truncate = FALSE;
        size_t ntocopy = 0;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(ntoskip == 0);
        if (ntoskip == 0) {
            if (d < 0.0) {
                precision++;
            }
            SsDoubleToAscii(d, tmp_buf, precision);
            len = strlen(tmp_buf);
            if (len > RS_DOUBLE_DISPSIZE) {
                need_to_truncate = TRUE;
                print_size = RS_DOUBLE_DISPSIZE;
            } else if (len >= bufsize) {
                need_to_truncate = TRUE;
                print_size = bufsize - 1;
            } else {
                print_size = len;
            }
            if (need_to_truncate) {
                SsDoubleTruncateRetT rc;

                rc = SsTruncateAsciiDoubleValue(tmp_buf, print_size + 1);
                switch (rc) {
                    case SS_DBLTRUNC_OK:
                        len = (size_t)strlen(tmp_buf);
                        ntocopy = len + 1;
                        break;
                    case SS_DBLTRUNC_TRUNCATED:
                        retc = RSAVR_TRUNCATION;
                        ntocopy = (size_t)strlen(tmp_buf) + 1;
                        break;
                    case SS_DBLTRUNC_VALUELOST:
                        retc = RSAVR_FAILURE;
                        break;
                    default:
                        ss_error;
                }
            } else {
                ntocopy = len + 1;
            }
            if (retc != RSAVR_FAILURE) {
                ss_dassert(ntocopy <= bufsize);
                memcpy(buf, tmp_buf, ntocopy);
                *p_totalsize = len;
                return (retc);
            }
            ss_dassert(retc == RSAVR_FAILURE);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t chfromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        double d;

        d = (double)rs_aval_getfloat(cd, atype, aval);
        retc = aval_chfromdouble(
                    cd,
                    atype,
                    d,
                    RS_REAL_DECPREC + 1,
                    buf,
                    bufsize,
                    ntoskip,
                    p_totalsize,
                    p_errh);
        return (retc);
}

static rs_avalret_t chfromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        double d;

        d = rs_aval_getdouble(cd, atype, aval);
        retc = aval_chfromdouble(
                    cd,
                    atype,
                    d,
                    RS_DOUBLE_DECPREC + 1,
                    buf,
                    bufsize,
                    ntoskip,
                    p_totalsize,
                    p_errh);
        return (retc);
}

static rs_avalret_t chfromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        bool succp;
        dt_date_t* date;
        ss_char1_t* dtformat;
        ss_char1_t tmp_buf[48];
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(ntoskip == 0);
        if (ntoskip == 0) {
            date = rs_aval_getdate(cd, atype, aval);
            dtformat = rs_atype_getdefaultdtformat(cd, atype);
            if (dtformat != NULL) {
                size_t len;
                ss_dassert(strlen(dtformat) < sizeof(tmp_buf));
                succp = dt_date_datetoasciiz(date, dtformat, tmp_buf);
                if (succp) {
                    len = strlen(tmp_buf);
                    if (len >= bufsize) {
                        succp = FALSE;
                    } else {
                        *p_totalsize = len;
                        memcpy(buf, tmp_buf, len + 1);
                    }
                }
            } else {
                ss_char1_t* p_buf;
                int year;
                dt_datesqltype_t datesqltype = 0;
                size_t bufsize_min;
                size_t bufsize_desired;

                year = dt_date_year(date);
                if (year > 9999) {
                    bufsize_min = 1 + 4 + 1 + 2 + 1 + 2 + 1;
                } else if (year < -9999) {
                    bufsize_min = 2 + 4 + 1 + 2 + 1 + 2 + 1;
                } else if (year < 0) {
                    bufsize_min = 1 + 4 + 1 + 2 + 1 + 2 + 1;
                } else {
                    bufsize_min =  4 + 1 + 2 + 1 + 2 + 1;
                }
                bufsize_desired = bufsize_min;
                switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
                    case RSSQLDT_DATE:
                        datesqltype = DT_DATE_SQLDATE;
                        break;
                    case RSSQLDT_TIME:
                        datesqltype = DT_DATE_SQLTIME;
                        bufsize_desired = bufsize_min = 2 + 1 + 2 + 1 + 2 + 1;
                        break;
                    case RSSQLDT_TIMESTAMP:
                        bufsize_min += 1 + 2 + 1 + 2 + 1 + 2;
                        if (dt_date_fraction(date) != 0L) {
                            bufsize_desired = bufsize_min + 10;
                        } else {
                            bufsize_desired = bufsize_min;
                        }
                        datesqltype = DT_DATE_SQLTIMESTAMP;
                        break;
                    default:
                        ss_error;
                }
                if (bufsize < bufsize_min) {
                    succp = FALSE;
                } else {
                    if (bufsize < bufsize_desired) {
                        p_buf = tmp_buf;
                        retc = RSAVR_TRUNCATION;
                    } else {
                        p_buf = buf;
                    }
                    succp = dt_date_datetoasciiz_sql(date, datesqltype, p_buf);
                    if (succp) {
                        *p_totalsize = strlen(p_buf);
                        if (bufsize < bufsize_desired) {
                            memcpy(buf, p_buf, bufsize - 1);
                            if (bufsize_desired - bufsize == 9) {
                                ss_dassert(buf[bufsize-2] == '.');
                                buf[bufsize-2] = '\0';
                            } else {
                                buf[bufsize - 1] = '\0';
                            }
                        }
                    }
                }
            }
            if (succp) {
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t chfromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        bool succp;
        rs_avalret_t retc = RSAVR_SUCCESS;
        ss_char1_t tmp_buf[48];
        dt_dfl_t dfl;
        size_t len;

        ss_dassert(ntoskip == 0);
        if (ntoskip == 0) {
            dfl = rs_aval_getdfloat(cd, atype, aval);
            succp = dt_dfl_dfltoasciiz_maxlen(dfl, tmp_buf, sizeof(tmp_buf)/sizeof(tmp_buf[0]));
            if (succp) {
                len = strlen(tmp_buf);
                *p_totalsize = len;
                if (len >= bufsize) {
                    SsDoubleTruncateRetT rc;

                    rc = SsTruncateAsciiDoubleValue(tmp_buf, bufsize);
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
                    if (retc != RSAVR_FAILURE) {
                        ss_dassert(strlen(tmp_buf) < bufsize);
                        strcpy(buf, tmp_buf);
                        return (retc);
                    }
                } else {
                    memcpy(buf, tmp_buf, len + 1);
                    return (RSAVR_SUCCESS);
                }
                ss_dassert(retc == RSAVR_FAILURE);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t chfrombin(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        va_index_t datalen;
        ss_byte_t* p_data;
        size_t n;
        rs_avalret_t retc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        p_data = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen >= 1);

        ss_dassert((ntoskip & 1) == 0);
        ntoskip /= 2;
        ss_dassert(ntoskip < datalen);
        p_data += ntoskip;
        datalen -= ntoskip;
        n = *p_totalsize = (datalen - 1) * 2;
        if (n >= bufsize) {
            retc = RSAVR_TRUNCATION;
            n = (bufsize - 1) / 2;
        } else {
            retc = RSAVR_SUCCESS;
            n /= 2;
        }
        su_chcvt_bin2hex(buf, p_data, n);
        return (retc);
}


static rs_avalret_t chfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        size_t n;
        size_t n_copied;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        n = VA_NETLEN(aval->RA_RDVA);
        ss_dassert(n & 1);
        n = (n - 1) / 2;
        ss_dassert(ntoskip <= n);
        succp = va_copydatachar2to1(
                    aval->RA_RDVA,
                    buf,
                    ntoskip,
                    bufsize - 1,
                    &n_copied);
        if (!succp) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, atype),
                RS_TN_CHAR);
            return (RSAVR_FAILURE);
        }
        ss_dassert(n_copied < bufsize);
        buf[n_copied] = '\0';
        n -= ntoskip;
        *p_totalsize = n;
        if (n >= bufsize) {
            return (RSAVR_TRUNCATION);
        }
        return (RSAVR_SUCCESS);
}

static rs_avalret_t chfromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        ss_int8_t   i8;
        ss_char1_t  tmp_buf[24];
        size_t      len;

        ss_dassert(ntoskip == 0);
        if (ntoskip == 0) {
            i8 = rs_aval_getint8(cd, atype, aval);
            len = SsInt8ToAscii(i8, tmp_buf, RS_BIGINT_RADIX, 0, '0', TRUE);
            if (len < bufsize) {
                *p_totalsize = len;
                memcpy(buf, tmp_buf, len + 1);
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t ucfromchar(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_char1_t* p;
        va_index_t len;
        size_t ntocopy;
        rs_avalret_t retc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        p = va_getdata(aval->RA_RDVA, &len);
        ss_dassert(ntoskip_in_char2 < len);
        len -= ntoskip_in_char2;
        p += ntoskip_in_char2;
        *p_totalsize_in_char2 = (size_t)(len - 1);
        if (bufsize_in_char2 < len) {
            retc = RSAVR_TRUNCATION;
            ntocopy = bufsize_in_char2 - 1;
            buf[ntocopy] = (ss_char2_t)0;
        } else {
            retc = RSAVR_SUCCESS;
            ntocopy = len;
        }
        SsSbuf2Wcs(buf, p, ntocopy);
        return (retc);
}


static rs_avalret_t ucfromint(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        long l;
        ss_char2_t tmp_buf[12];
        size_t     len;

        ss_dassert(ntoskip_in_char2 == 0);
        if (ntoskip_in_char2 == 0) {
            l = rs_aval_getlong(cd, atype, aval);
            len = SsLongToWcs(l, tmp_buf, 10, 0, '0', TRUE);
            if (len < bufsize_in_char2) {
                *p_totalsize_in_char2 = len;
                memcpy(buf, tmp_buf, (len + 1) * sizeof(ss_char2_t));
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t ucfromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = chfromflt(
                    cd,
                    atype,
                    aval,
                    (ss_char1_t*)buf,
                    bufsize_in_char2,
                    ntoskip_in_char2,
                    p_totalsize_in_char2,
                    p_errh);
        if (retc == RSAVR_FAILURE) {
            return (RSAVR_FAILURE);
        }
        SsStr2WcsInPlace(buf);
        return (retc);
}

static rs_avalret_t ucfromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = chfromdbl(
                    cd,
                    atype,
                    aval,
                    (ss_char1_t*)buf,
                    bufsize_in_char2,
                    ntoskip_in_char2,
                    p_totalsize_in_char2,
                    p_errh);
        if (retc == RSAVR_FAILURE) {
            return (RSAVR_FAILURE);
        }
        SsStr2WcsInPlace(buf);
        return (retc);
}

static rs_avalret_t ucfromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = chfromdate(
                    cd,
                    atype,
                    aval,
                    (ss_char1_t*)buf,
                    bufsize_in_char2,
                    ntoskip_in_char2,
                    p_totalsize_in_char2,
                    p_errh);
        if (retc == RSAVR_FAILURE) {
            return (RSAVR_FAILURE);
        }
        SsStr2WcsInPlace(buf);
        return (retc);
}

static rs_avalret_t ucfromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = chfromdfl(
                    cd,
                    atype,
                    aval,
                    (ss_char1_t*)buf,
                    bufsize_in_char2,
                    ntoskip_in_char2,
                    p_totalsize_in_char2,
                    p_errh);
        if (retc == RSAVR_FAILURE) {
            return (RSAVR_FAILURE);
        }
        SsStr2WcsInPlace(buf);
        return (retc);
}

static rs_avalret_t ucfrombin(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        rs_avalret_t retc;
        va_index_t datalen;
        ss_byte_t* p_data;
        size_t n;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        p_data = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen >= 1);
        ss_dassert((ntoskip_in_char2 & 1) == 0);

        ntoskip_in_char2 /= 2;
        ss_dassert(ntoskip_in_char2 < datalen);
        p_data += ntoskip_in_char2;
        datalen -= ntoskip_in_char2;
        n = *p_totalsize_in_char2 = (datalen - 1) * 2;
        if (n >= bufsize_in_char2) {
            retc = RSAVR_TRUNCATION;
            n = (bufsize_in_char2 - 1) / 2;
        } else {
            retc = RSAVR_SUCCESS;
            n /= 2;
        }
        su_chcvt_bin2hexchar2(buf, p_data, n);
        return (retc);
}


static rs_avalret_t ucfromuni(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        size_t n_copied;
        size_t n;
        rs_avalret_t retc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        ss_dassert(va_netlen(aval->RA_RDVA) & 1);
        ss_dassert(va_netlen(aval->RA_RDVA) > ntoskip_in_char2)
        n = ((VA_NETLEN(aval->RA_RDVA) - 1) / 2) - ntoskip_in_char2;
        *p_totalsize_in_char2 = n;
        va_copydatachar2(
            aval->RA_RDVA,
            buf,
            ntoskip_in_char2,
            bufsize_in_char2 - 1,
            &n_copied);
        ss_dassert(n_copied < bufsize_in_char2);
        if (n_copied < n) {
            retc = RSAVR_TRUNCATION;
        } else {
            retc = RSAVR_SUCCESS;
        }
        buf[n_copied] = (ss_char2_t)0;
        return (retc);
}

static rs_avalret_t ucfromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        ss_int8_t   i8;
        ss_char2_t  tmp_buf[24];
        size_t      len;

        ss_dassert(ntoskip_in_char2 == 0);
        if (ntoskip_in_char2 == 0) {
            ss_char1_t c1buf[24];

            i8 = rs_aval_getint8(cd, atype, aval);
            len = SsInt8ToAscii(i8, c1buf, RS_BIGINT_RADIX, 0, '0', TRUE);
            SsStr2Wcs(tmp_buf, c1buf);
            if (len < bufsize_in_char2) {
                *p_totalsize_in_char2 = len;
                memcpy(buf, tmp_buf, (len + 1) * sizeof(ss_char2_t));
                return (RSAVR_SUCCESS);
            }
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_CHAR);
        return (RSAVR_FAILURE);
}

static rs_avalret_t aval_date2date(
        void* cd __attribute__ ((unused)),
        dt_date_t* p_date,
        rs_sqldatatype_t src_sqldt,
        rs_sqldatatype_t sqldt)
{
        bool succp = TRUE;
        rs_avalret_t retc = RSAVR_SUCCESS;

        switch (sqldt) {
            case RSSQLDT_TIME:
                switch (src_sqldt) {
                    case RSSQLDT_DATE:
                        succp = FALSE;
                        break;
                    case RSSQLDT_TIME:
                        break;
                    case RSSQLDT_TIMESTAMP:
                        retc = RSAVR_TRUNCATION;
                        succp = dt_date_truncatetotime(p_date);
                        break;
                    default:
                        ss_error;
                }
                break;
            case RSSQLDT_DATE:
                switch (src_sqldt) {
                    case RSSQLDT_DATE:
                        break;
                    case RSSQLDT_TIME:
                        succp = FALSE;
                        break;
                    case RSSQLDT_TIMESTAMP:
                        if (dt_date_hour(p_date) != 0
                        ||  dt_date_min(p_date) != 0
                        ||  dt_date_sec(p_date) != 0
                        ||  dt_date_fraction(p_date) != 0UL)
                        {
                            retc = RSAVR_TRUNCATION;
                            succp = dt_date_truncatetodate(p_date);
                        }
                        break;
                    default:
                        ss_error;
                }
                break;
            case RSSQLDT_TIMESTAMP:
                switch (src_sqldt) {
                    case RSSQLDT_DATE:
                        break;
                    case RSSQLDT_TIME:
                        succp = dt_date_padtimewithcurdate(p_date);
                        break;
                    case RSSQLDT_TIMESTAMP:
                        break;
                    default:
                        ss_error;
                }
                break;
            default:
                ss_error;
        }
        if (succp) {
            return (retc);
        }
        return (RSAVR_FAILURE);
}


static bool aval_str2date(
        void* cd,
        ss_char1_t* s,
        dt_date_t* p_date,
        rs_sqldatatype_t sqldt)
{
        ss_char1_t* dtformat;
        dt_datesqltype_t datesqltype;
        rs_sqldatatype_t src_sqldt = 0;
        bool succp;

        dtformat = rs_sysi_dateformat(cd);
        succp = dt_date_setasciiz_ext(
                    p_date,
                    dtformat,
                    s,
                    &datesqltype);
        if (!succp && dtformat != NULL) {
            succp = dt_date_setasciiz_ext(
                        p_date,
                        NULL,
                        s,
                        &datesqltype);
        }
        if (succp) {
            switch (datesqltype) {
                case DT_DATE_SQLDATE:
                    src_sqldt = RSSQLDT_DATE;
                    break;
                case DT_DATE_SQLTIME:
                    src_sqldt = RSSQLDT_TIME;
                    break;
                case DT_DATE_SQLTIMESTAMP:
                    src_sqldt = RSSQLDT_TIMESTAMP;
                    break;
                default:
                    ss_error;
            }
            succp = aval_date2date(cd, p_date, src_sqldt, sqldt);
        }
        return (succp);
}


static rs_avalret_t aval_datefromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_sqldatatype_t sqldt,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        ss_char1_t* s;

        s = rs_aval_getasciiz(cd, atype, aval);
        retc = aval_str2date(cd, s, p_date, sqldt);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, atype),
                RS_TN_DATE);
            return (RSAVR_FAILURE);
        }
        return (retc);
}

static rs_avalret_t aval_dateillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        dt_date_t* p_date __attribute__ ((unused)),
        rs_sqldatatype_t sqldt,
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            rs_atype_sqldatatypenamebydt(cd, sqldt));
        return (RSAVR_FAILURE);
}

static rs_avalret_t aval_datefromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_sqldatatype_t sqldt,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_sqldatatype_t src_sqldt;
        dt_date_t* p_src_date;

        p_src_date = rs_aval_getdate(cd, atype, aval);
#if 0
        *p_date = *p_src_date;
#else /* 0 */
	memcpy(p_date, p_src_date, DT_DATE_DATASIZE);
#endif /* 0 */
        src_sqldt = RS_ATYPE_SQLDATATYPE(cd, atype);
        retc = aval_date2date(cd, p_date, src_sqldt, sqldt);
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, atype),
                rs_atype_sqldatatypenamebydt(cd, sqldt));
            return (RSAVR_FAILURE);
        }
        return (retc);
}

static rs_avalret_t aval_datefromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_sqldatatype_t sqldt,
        rs_err_t** p_errh)
{
        ss_char1_t* tmp_buf;
        rs_avalret_t retc;
        size_t len;

        tmp_buf = rs_aval_tmpstrfromuni(cd, atype, aval, &len);
        if (tmp_buf != NULL) {
            retc = aval_str2date(cd, tmp_buf, p_date, sqldt);
            SsMemFree(tmp_buf);
        } else {
            retc = RSAVR_FAILURE;
        }
        if (retc == RSAVR_FAILURE) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, atype),
                RS_TN_DATE);
            return (RSAVR_FAILURE);
        }
        return (retc);
}

static rs_avalret_t dtfromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_datefromchar(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_DATE,
                    p_errh);
        return (retc);
}

static rs_avalret_t dtillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_dateillegal(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_DATE,
                    p_errh);
        ss_dassert(retc == RSAVR_FAILURE);
        return (retc);
}

static rs_avalret_t dtfromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromdate(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_DATE,
                    p_errh);
        return (retc);
}

static rs_avalret_t dtfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromuni(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_DATE,
                    p_errh);
        return (retc);
}

static rs_avalret_t tmfromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_datefromchar(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIME,
                    p_errh);
        return (retc);
}

static rs_avalret_t tmillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_dateillegal(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIME,
                    p_errh);
        ss_dassert(retc == RSAVR_FAILURE);
        return (retc);
}

static rs_avalret_t tmfromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromdate(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIME,
                    p_errh);
        return (retc);
}

static rs_avalret_t tmfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromuni(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIME,
                    p_errh);
        return (retc);
}


static rs_avalret_t tsfromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_datefromchar(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIMESTAMP,
                    p_errh);
        return (retc);
}

static rs_avalret_t tsillegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;

        retc = aval_dateillegal(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIMESTAMP,
                    p_errh);
        ss_dassert(retc == RSAVR_FAILURE);
        return (retc);
}

static rs_avalret_t tsfromdate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromdate(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIMESTAMP,
                    p_errh);
        return (retc);
}

static rs_avalret_t tsfromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        
        retc = aval_datefromuni(
                    cd,
                    atype,
                    aval,
                    p_date,
                    RSSQLDT_TIMESTAMP,
                    p_errh);
        return (retc);
}



RS_AVALRET_T rs_aval_converttolong(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        long* p_long,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_getlongarr[dt])(cd, atype, aval, p_long, p_errh);
        return ((RS_AVALRET_T)retc);
}

bool rs_aval_converttofloat(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        float* p_float,
        rs_err_t** p_errh)
{
        bool succp;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        succp = (*aval_getfloatarr[dt])(cd, atype, aval, p_float, p_errh);
        return (succp);
}

bool rs_aval_converttodouble(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        double* p_double,
        rs_err_t** p_errh)
{
        bool succp;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        succp = (*aval_getdoublearr[dt])(cd, atype, aval, p_double, p_errh);
        return (succp);
}

bool rs_aval_converttodfloat(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_dfl_t* p_dfl,
        rs_err_t** p_errh)
{
        bool succp;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        succp = (*aval_getdfloatarr[dt])(cd, atype, aval, p_dfl, p_errh);
        return (succp);
}

RS_AVALRET_T rs_aval_convertto8bitstr(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_copy8bitstrarr[dt])(
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    ntoskip,
                    p_totalsize,
                    p_errh);
        return ((RS_AVALRET_T)retc);
}

RS_AVALRET_T rs_aval_converttowcs(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char2_t* buf,
        size_t bufsize_in_char2,
        size_t ntoskip_in_char2,
        size_t* p_totalsize_in_char2,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_copyunicodearr[dt])(
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize_in_char2,
                    ntoskip_in_char2,
                    p_totalsize_in_char2,
                    p_errh);
        return ((RS_AVALRET_T)retc);
}

RS_AVALRET_T rs_aval_converttodate(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_getdatearr[dt])(cd, atype, aval, p_date, p_errh);
        return ((RS_AVALRET_T)retc);
}

RS_AVALRET_T rs_aval_converttotime(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_gettimearr[dt])(cd, atype, aval, p_date, p_errh);
        return ((RS_AVALRET_T)retc);
}

RS_AVALRET_T rs_aval_converttotimestamp(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dt_date_t* p_date,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_gettimestamparr[dt])(cd, atype, aval, p_date, p_errh);
        return ((RS_AVALRET_T)retc);
}

RS_AVALRET_T rs_aval_converttobinary(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        void* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        ss_byte_t* p_data;
        va_index_t datalen;
        rs_datatype_t dt;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        if (dt != RSDT_BINARY && dt != RSDT_CHAR) {
            rs_error_create(
                p_errh,
                E_ILLASSIGN_SS,
                rs_atype_name(cd, atype),
                RS_TN_BINARY);
            return (RSAVR_FAILURE);
        }
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_ONLYCONVERTED));
        p_data = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen >= 1);
        ss_dassert(ntoskip < datalen);
        p_data += ntoskip;
        datalen -= ntoskip + 1;
        *p_totalsize = datalen;
        if (datalen > bufsize) {
            datalen = bufsize;
            retc = RSAVR_TRUNCATION;
        }
        memcpy(buf, p_data, datalen);
        return ((RS_AVALRET_T)retc);
}


static rs_avalret_t U8fromchar(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_char1_t* p_src;
        ss_char1_t* p_src_tmp;
        va_index_t src_totlen;
        size_t src_len;
        ss_byte_t* p_dst_tmp;
        size_t dst_len;
        rs_avalret_t retc;
        SsUtfRetT utfrc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        p_src = va_getdata(aval->RA_RDVA, &src_totlen);
        ss_dassert(ntoskip < src_totlen);
        p_src += ntoskip;
        src_len = src_totlen - ntoskip - 1;
        dst_len = SsASCII8ByteLenAsUTF8(p_src, src_len);
        *p_totalsize = dst_len;
        p_dst_tmp = (ss_byte_t*)buf;
        p_src_tmp = p_src;
        utfrc = SsASCII8toUTF8(
                    &p_dst_tmp, (ss_byte_t*)buf + bufsize - 1,
                    &p_src_tmp, p_src + src_len);
        if (utfrc == SS_UTF_TRUNCATION) {
            retc = RSAVR_TRUNCATION;
            ss_rc_assert(retc == RSAVR_TRUNCATION, retc);
        } else {
            retc = RSAVR_SUCCESS;
            ss_rc_assert(utfrc == SS_UTF_OK || utfrc == SS_UTF_NOCHANGE, utfrc);
        }
        ss_dassert((ulong)p_dst_tmp < (ulong)buf + bufsize);
        *p_dst_tmp = '\0';
        return (retc);
}


static rs_avalret_t U8fromuni(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_char2_t* p_src;
        ss_char2_t* p_src_tmp;
        va_index_t src_totlen;
        size_t src_len;
        ss_byte_t* p_dst_tmp;
        size_t dst_len;
        rs_avalret_t retc;
        SsUtfRetT utfrc;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED | RA_BLOB));
        p_src = va_getdata(aval->RA_RDVA, &src_totlen);
        ss_dassert(src_totlen & 1);
        src_totlen /= sizeof(ss_char2_t);
        ss_dassert(ntoskip <= src_totlen);
        p_src += ntoskip;
        src_len = src_totlen - ntoskip;
        dst_len = SsUCS2vaByteLenAsUTF8(p_src, src_len);
        *p_totalsize = dst_len;
        p_dst_tmp = (ss_byte_t*)buf;
        p_src_tmp = p_src;
        utfrc = SsUCS2vatoUTF8(
                    &p_dst_tmp, (ss_byte_t*)buf + bufsize - 1,
                    &p_src_tmp, p_src + src_len);
        if (utfrc == SS_UTF_TRUNCATION) {
            retc = RSAVR_TRUNCATION;
        } else {
            retc = RSAVR_SUCCESS;
            ss_rc_assert(utfrc == SS_UTF_OK, utfrc);
        }
        ss_dassert((ulong)p_dst_tmp < (ulong)buf + bufsize);
        *p_dst_tmp = '\0';
        return (retc);
}

#define U8fromint   ((aval_copyUTF8fun_t*)chfromint)
#define U8fromflt   ((aval_copyUTF8fun_t*)chfromflt)
#define U8fromdbl   ((aval_copyUTF8fun_t*)chfromdbl)
#define U8fromdate  ((aval_copyUTF8fun_t*)chfromdate)
#define U8fromdfl   ((aval_copyUTF8fun_t*)chfromdfl)
#define U8frombin   ((aval_copyUTF8fun_t*)chfrombin)

static aval_copyUTF8fun_t* const aval_copyUTF8arr[RSDT_DIMENSION] = {
/*      RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE */
        U8fromchar,U8fromint,   U8fromflt,  U8fromdbl,  U8fromdate, U8fromdfl,  U8frombin,  U8fromuni
};

/*##**********************************************************************\
 * 
 *		rs_aval_converttoUTF8
 * 
 * Converts an aval to buffer encoded in UTF-8
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 *	buf - out, use
 *		pointer to buffer
 *		
 *	bufsize - in
 *		size of buf
 *		
 *	ntoskip - in
 *		number of characters (not bytes) to skip
 *		
 *	p_totalsize - out, use
 *		total size in bytes needed to store the value without
 *          truncation ('\0'-byte excluded, buffer should be
 *          *ptotalsize + 1)
 *		
 *	p_errh - out, give
 *		pointer to error handle or NULL (to not give error handle)
 *		
 * Return value :
 *      RSAVR_SUCCESS,
 *      RSAVR_FAILURE
 *      RSAVR_TRUNCATION
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
RS_AVALRET_T rs_aval_converttoUTF8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        rs_avalret_t retc;
        rs_datatype_t dt;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dt = RS_ATYPE_DATATYPE(cd, atype);
        retc = (*aval_copyUTF8arr[dt])(
                    cd,
                    atype,
                    aval,
                    buf,
                    bufsize,
                    ntoskip,
                    p_totalsize,
                    p_errh);
        return ((RS_AVALRET_T)retc);
}



/*##**********************************************************************\
 * 
 *		rs_aval_requiredUTF8bufsize
 * 
 * Gets buffer size in bytes needed to store the value in aval into
 * UTF-8 buffer.
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      the size for UTF-8 buffer needed to store the value in it
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t rs_aval_requiredUTF8bufsize(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        rs_datatype_t dt;
        void* data;
        va_index_t datalen;
        size_t required_bufsize = 0;
       
        if (RS_AVAL_ISNULL(cd, atype, aval)) {
            ss_derror;
            return (0);
        }
        dt = RS_ATYPE_DATATYPE(cd, atype);
        switch (dt) {
            case RSDT_CHAR:
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen >= 1);
                required_bufsize = SsASCII8ByteLenAsUTF8(data, datalen);
                break;
            case RSDT_UNICODE:
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen & 1);
                required_bufsize = 1 +
                    SsUCS2vaByteLenAsUTF8(
                        data,
                        datalen / sizeof(ss_char2_t));
                break;
            case RSDT_BINARY :
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen >= 1);
                required_bufsize = (datalen - 1) * 2 + 1;
                break;
            case RSDT_INTEGER:
                required_bufsize = 12;
                break;
            case RSDT_BIGINT:
                required_bufsize = 21;
                break;
            case RSDT_FLOAT  :
                required_bufsize = 20;
                break;
            case RSDT_DOUBLE :
                required_bufsize = 28;
                break;
            case RSDT_DATE   :
                required_bufsize = 32;
                ss_dassert(32 == 1 + strlen("-12345-12-13 11:12:13.123456789"));
                break;
            case RSDT_DFLOAT:
                required_bufsize = 20;
                break;
            default:
                ss_error;
        }
        return (required_bufsize);
}

/*##**********************************************************************\
 * 
 *		rs_aval_required8bitstrbufsize
 * 
 * Gets buffer size in bytes needed to store the value in aval into
 * 8-bit string buffer.
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      the size for 8-bit string buffer needed to store the value in it
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t rs_aval_required8bitstrbufsize(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        rs_datatype_t dt;
        void* data;
        va_index_t datalen;
        size_t required_bufsize = 0;
       
        if (RS_AVAL_ISNULL(cd, atype, aval)) {
            ss_derror;
            return (0);
        }
        dt = RS_ATYPE_DATATYPE(cd, atype);
        switch (dt) {
            case RSDT_CHAR:
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen >= 1);
                required_bufsize = (size_t)datalen;
                break;
            case RSDT_UNICODE:
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen & 1);
                required_bufsize = (size_t)(datalen / 2) + 1;
                break;
            case RSDT_BINARY:
                data = va_getdata(aval->RA_RDVA, &datalen);
                ss_dassert(datalen >= 1);
                required_bufsize = (size_t)(datalen - 1) * 2 + 1;
                break;
            case RSDT_INTEGER:
                required_bufsize = 12;
                break;
            case RSDT_BIGINT:
                required_bufsize = 21;
                break;
            case RSDT_FLOAT:
                required_bufsize = 20;
                break;
            case RSDT_DOUBLE:
                required_bufsize = 28;
                break;
            case RSDT_DATE:
                required_bufsize = 32;
                ss_dassert(32 == 1 + strlen("-12345-12-13 11:12:13.123456789"));
                break;
            case RSDT_DFLOAT:
                required_bufsize = 20;
                break;
            default:
                ss_error;
        }
        return (required_bufsize);
}

size_t rs_aval_requiredwcsbufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        size_t required_size;

        required_size = sizeof(ss_char2_t) *
            rs_aval_required8bitstrbufsize(cd, atype, aval);
        return (required_size);
}

size_t rs_aval_requiredstrbufsize(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        size_t required_size;
        required_size = rs_aval_requiredUTF8bufsize(cd, atype, aval);
        return (required_size);
}

RS_AVALRET_T rs_aval_converttostr(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t* buf,
        size_t bufsize,
        size_t ntoskip,
        size_t* p_totalsize,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;

        retc = rs_aval_convertto8bitstr(
                    cd,
                    atype, aval,
                    buf, bufsize,
                    ntoskip,
                    p_totalsize,
                    p_errh);
        return (retc);
}

/*##**********************************************************************\
 * 
 *		rs_aval_givestr
 * 
 * Converts an aval to newly allocated string buffer
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 *      p_str_give - out, give
 *          pointer to string pointer
 *		
 *      p_length - out
 *          if (p_length != NULL) strlen(*p_str_give)
 *
 *      p_errh - out, give
 *          pointer to error handle or NULL (to not give error handle)
 *		
 * Return value :
 *      RSAVR_SUCCESS,
 *      RSAVR_FAILURE
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
RS_AVALRET_T rs_aval_givestr(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t** p_str_give,
        size_t* p_length,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_char1_t* str;
        size_t bufsize;
        size_t length_dummy;
        
        if (p_length == NULL) {
            p_length = &length_dummy;
        }
        if (rs_aval_isnull(cd, atype, aval)) {
            *p_length = 0;
            *p_str_give = NULL;
            return (RSAVR_SUCCESS);
        }
        bufsize = rs_aval_requiredstrbufsize(cd, atype, aval);
        str = SsMemAlloc(bufsize);
        retc = rs_aval_converttostr(
                    cd, 
                    atype, aval, 
                    str, bufsize, 0, p_length, 
                    p_errh);
        ss_debug(
                if (retc == RSAVR_SUCCESS) {
                    ss_dassert(*p_length < bufsize);
                } else {
                    ss_dassert(retc == RSAVR_FAILURE);
                });
        if (retc != RSAVR_FAILURE) {
            *p_str_give = str;
        } else {
            SsMemFree(str);
            *p_length = 0;
            *p_str_give = NULL;
        }
        return (retc);
}
/*##**********************************************************************\
 * 
 *		rs_aval_giveUTF8str
 * 
 * Converts an aval to newly allocated UTF8 string buffer
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 *      p_UTF8str_give - out, give
 *          pointer to string pointer
 *		
 *      p_length - out
 *          if (p_length != NULL) strlen(*p_str_give)
 *
 *      p_errh - out, give
 *          pointer to error handle or NULL (to not give error handle)
 *		
 * Return value :
 *      RSAVR_SUCCESS,
 *      RSAVR_FAILURE
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
RS_AVALRET_T rs_aval_giveUTF8str(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_char1_t** p_UTF8str_give,
        size_t* p_length,
        rs_err_t** p_errh)
{
        RS_AVALRET_T retc;
        ss_char1_t* UTF8str;
        size_t bufsize;
        size_t length_dummy;
        
        if (p_length == NULL) {
            p_length = &length_dummy;
        }
        if (rs_aval_isnull(cd, atype, aval)) {
            *p_length = 0;
            *p_UTF8str_give = NULL;
            return (RSAVR_SUCCESS);
        }
        bufsize = rs_aval_requiredUTF8bufsize(cd, atype, aval);
        UTF8str = SsMemAlloc(bufsize);
        retc = rs_aval_converttoUTF8(
                    cd, 
                    atype, aval, 
                    UTF8str, bufsize, 0, p_length, 
                    p_errh);
        ss_debug(
                if (retc == RSAVR_SUCCESS) {
                    ss_dassert(*p_length < bufsize);
                } else {
                    ss_dassert(retc == RSAVR_FAILURE);
                });
        if (retc != RSAVR_FAILURE) {
            *p_UTF8str_give = UTF8str;
        } else {
            SsMemFree(UTF8str);
            *p_length = 0;
            *p_UTF8str_give = NULL;
        }
        return (retc);
}

/* BIGINT conversions */
static rs_avalret_t i8fromchar(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh)
{
        ss_char1_t* s;
        ss_char1_t* mismatch;
        va_index_t datalen;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        s = va_getdata(aval->RA_RDVA, &datalen);
        ss_dassert(datalen > 0);
        succp = SsStrScanInt8(s, p_int8, &mismatch);
        if (succp) {
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_BIGINT);
        return (RSAVR_FAILURE);
}

static rs_avalret_t i8fromint(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        SsInt8SetInt4(p_int8, rs_aval_getlong(cd, atype, aval));
        return (RSAVR_SUCCESS);
}

static rs_avalret_t i8fromflt(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh)
{
        float f;
        double d;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        f = rs_aval_getfloat(cd, atype, aval);
        d = f;

        if (SsInt8SetDouble(p_int8, d)) {
            double d2;
            SsInt8ConvertToDouble(&d2, *p_int8);
            if (d2 != d) {
                retc = RSAVR_TRUNCATION;
            }
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static rs_avalret_t i8fromdbl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh)
{
        double d;
        rs_avalret_t retc = RSAVR_SUCCESS;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        d = rs_aval_getdouble(cd, atype, aval);
        if (SsInt8SetDouble(p_int8, d)) {
            double d2;
            SsInt8ConvertToDouble(&d2, *p_int8);
            if (d2 != d) {
                retc = RSAVR_TRUNCATION;
            }
            return (retc);
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static rs_avalret_t i8illegal(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval __attribute__ ((unused)),
        ss_int8_t* p_int8 __attribute__ ((unused)),
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_BIGINT);
        return (RSAVR_FAILURE);
}


static rs_avalret_t i8fromdfl(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh)
{
        ss_char1_t tmp_buf[24];
        dt_dfl_t dfl;
        bool succp;

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        dfl = rs_aval_getdfloat(cd, atype, aval);
        succp = dt_dfl_dfltoasciiz_maxlen(dfl, tmp_buf, sizeof(tmp_buf)-1);
        if (succp) {
            rs_avalret_t retc = RSAVR_SUCCESS;
            char* mismatch;

            if (SsStrScanInt8(tmp_buf, p_int8, &mismatch)) {
                ss_char1_t tmp_buf2[24];
                SsInt8ToAscii(*p_int8, tmp_buf2, RS_BIGINT_RADIX, 0, '0', TRUE);
                if (SsStrcmp(tmp_buf, tmp_buf2)) {
                    retc = RSAVR_TRUNCATION;
                }
                return (retc);
            }
        }
        rs_error_create(
            p_errh,
            E_NUMERICOVERFLOW);
        return (RSAVR_FAILURE);
}

static rs_avalret_t i8fromuni(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh)
{
        ss_char2_t tmp_buf[48];
        ss_char2_t* p_buf;
        ss_char1_t* mismatch;
        ss_char1_t tmp_str[48];
        ss_char1_t* p_str;
        va_index_t datalen;
        size_t n_copied;
        bool succp;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                  RA_NULL | RA_ONLYCONVERTED));
        datalen = va_netlen(aval->RA_RDVA);
        ss_dassert(datalen > 0);
        if (datalen / sizeof(ss_char2_t) >= sizeof(tmp_buf)/sizeof(tmp_buf[0])) {
            p_buf = SsMemAlloc((datalen / sizeof(ss_char2_t) + 1) * sizeof(ss_char2_t));
            p_str = SsMemAlloc((datalen / sizeof(ss_char2_t) + 1) * sizeof(ss_char1_t));
        } else {
            p_buf = tmp_buf;
            p_str = tmp_str;
        }
        va_copydatachar2(
            aval->RA_RDVA,
            p_buf,
            0,
            datalen / sizeof(ss_char2_t),
            &n_copied);
        ss_dassert(n_copied == datalen / sizeof(ss_char2_t));
        p_buf[n_copied] = (ss_char2_t)0;

        SsWcs2Str(p_str, p_buf);
        succp = SsStrScanInt8(p_str, p_int8, &mismatch);
        if (p_buf != tmp_buf) {
            SsMemFree(p_buf);
        }
        if (p_str != tmp_str) {
            SsMemFree(p_str);
        }
        if (succp) {
            return (RSAVR_SUCCESS);
        }
        rs_error_create(
            p_errh,
            E_ILLASSIGN_SS,
            rs_atype_name(cd, atype),
            RS_TN_BIGINT);
        return (RSAVR_FAILURE);
}

static rs_avalret_t i8fromint8(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_int8_t* p_int8,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        *p_int8 = rs_aval_getint8(cd, atype, aval);
        return (RSAVR_SUCCESS);
}

