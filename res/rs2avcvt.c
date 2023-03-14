/*************************************************************************\
**  source       * rs2avcvt.c
**  directory    * res
**  description  * Aid services for rs_aval_xxx conversion routines
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

/*##**********************************************************************\
 *
 *		rs_aval_tmpstrfromuni
 *
 * Creates a temporary string containing all leading
 * characters from src_aval that are below or equal to 0xff
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	src_atype - in, use
 *		source atype
 *
 *	src_aval - in, use
 *		source aval
 *
 *	p_len - out, use
 *		pointer to var where to put strlen(retvalue)
 *
 * Return value - give:
 *      temporary buffer, must be freed using SsMemFree()
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
ss_char1_t* rs_aval_tmpstrfromuni(
        void* cd __attribute__ ((unused)),
        rs_atype_t* src_atype __attribute__ ((unused)),
        rs_aval_t* src_aval,
        size_t* p_len)
{
        ss_char2_t* s;
        ss_char2_t* p;
        va_index_t datalen;
        ss_char1_t* tmp_buf;
        uint i;

        p = s = va_getdata(src_aval->RA_RDVA, &datalen);
        ss_dassert(datalen & 1);
        datalen /= 2;
        for (i = 0; i < datalen; i++, p++) {
            ss_char2_t c;

            c = SS_CHAR2_LOAD_MSB1ST(p);
            if (c & (ss_char2_t)~0x00ff) {
                break;
            }
        }
        if (i != 0) {
            uint j;

            tmp_buf = SsMemAlloc(sizeof(ss_char1_t) * (i + 1));
            for (j = 0, p = s; j < i; j++, p++) {
                ss_char2_t c;

                c = SS_CHAR2_LOAD_MSB1ST(p);
                ss_dassert((c & (ss_char2_t)~0x00ff) == 0);
                tmp_buf[j] = (ss_char1_t)c;
            }
            tmp_buf[j] = '\0';
            *p_len = (size_t)j;
        } else {
            tmp_buf = NULL;
            *p_len = (size_t)0;
        }
        return (tmp_buf);
}


/*##**********************************************************************\
 *
 *		rs_aval_putlong
 *
 * Sets long to RSDT_INTEGER aval
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
 *	l -
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
rs_avalret_t rs_aval_putlong(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        long l)
{
        rs_avalret_t retc = RSAVR_SUCCESS;

        switch (RS_ATYPE_SQLDATATYPE(cd, dst_atype)) {
            case RSSQLDT_SMALLINT:
                if (l > (ss_int4_t)0x0000FFFFL
                ||  l < (ss_int4_t)0xFFFF8000L)
                {
                    retc = RSAVR_FAILURE;
                }
                break;
            case RSSQLDT_TINYINT:
                if (l > (ss_int4_t)0x000000FFL
                ||  l < (ss_int4_t)0xFFFFFF80L)
                {
                    retc = RSAVR_FAILURE;
                }
                break;
            case RSSQLDT_INTEGER:
                if (sizeof(l) > sizeof(ss_int4_t)) {
                    if (l > RS_INT_MAX || l < RS_INT_MIN) {
                        FAKE_CODE_BLOCK(
                            FAKE_RES_ASSERTPUTLONGFAILURE,
                            ss_error;);
                        retc = RSAVR_FAILURE;
                    }
                }
                break;
            default:
                ss_error;
                break;
        }
        if (retc != RSAVR_FAILURE) {
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_NULL | RA_VTPLREF |
                              RA_ONLYCONVERTED | RA_FLATVA))
            {
                dst_aval->RA_RDVA = NULL;
            } else {
                ss_derror; /* va should never be dynamic for RSDT_INTEGER */
                refdva_free(&dst_aval->ra_va);
            }
            SU_BFLAG_CLEAR(dst_aval->ra_flags,
                           RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
            SU_BFLAG_SET(dst_aval->ra_flags,
                         RA_CONVERTED | RA_ONLYCONVERTED);
            dst_aval->ra_.l = l;
            return(RSAVR_SUCCESS);
        }
        return (RSAVR_FAILURE);
}

/*##**********************************************************************\
 *
 *		rs_aval_putint8
 *
 * Sets ss_int8_t to RSDT_BIGINT aval
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
 *	l -
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
rs_avalret_t rs_aval_putint8(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* dst_atype __attribute__ ((unused)),
        rs_aval_t*  dst_aval,
        ss_int8_t   i8)
{
        ss_dassert(RS_ATYPE_SQLDATATYPE(cd, dst_atype) == RSSQLDT_BIGINT);
        if (SU_BFLAG_TEST(dst_aval->ra_flags,
                          RA_VTPLREF | RA_NULL | RA_FLATVA | RA_ONLYCONVERTED))
        {
            dst_aval->RA_RDVA = NULL;
        } else {
            ss_derror; /* va should never be dynamic for RSDT_BIGINT! */
            refdva_free(&dst_aval->ra_va);
        }
        SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_FLATVA | RA_UNKNOWN);
        SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_ONLYCONVERTED);
        dst_aval->ra_.i8 = i8;
        return(RSAVR_SUCCESS);
}

/*##**********************************************************************\
 *
 *		rs_aval_putdfl
 *
 * Sets dfloat value to aval
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
 *	p_dfl -
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
rs_avalret_t rs_aval_putdfl(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t* dst_aval,
        dt_dfl_t* p_dfl)
{
        bool succp = TRUE;

        if (RS_ATYPE_SQLDATATYPE(cd, dst_atype) == RSSQLDT_NUMERIC) {
            succp = dt_dfl_setdflprecision(
                        p_dfl,
                        *p_dfl,
                        (int)RS_ATYPE_LENGTH(cd, dst_atype),
                        (int)rs_atype_scale(cd, dst_atype));
        }
        if (succp) {
            ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(dst_aval->ra_flags,
                              RA_VTPLREF | RA_NULL | RA_FLATVA))
            {
                dst_aval->RA_RDVA = NULL;
            }
            dst_aval->ra_.dfl = *p_dfl;
            dt_dfl_dfltova(dst_aval->ra_.dfl, &dst_aval->ra_vabuf.va);
            dst_aval->ra_va = &dst_aval->ra_vabuf.va;
            SU_BFLAG_CLEAR(dst_aval->ra_flags, RA_NULL | RA_VTPLREF | RA_UNKNOWN);
            SU_BFLAG_SET(dst_aval->ra_flags, RA_CONVERTED | RA_FLATVA);
            return (RSAVR_SUCCESS);
        }
        return (RSAVR_FAILURE);
}


/*#***********************************************************************\
 *
 *		rs_aval_putdbltochar
 *
 * Sets double to RSDT_CHAR aval
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
 *	d -
 *
 *
 *	prec -
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
rs_avalret_t rs_aval_putdbltochar(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        double d,
        int prec)
{
        ss_char1_t buf[48];
        ulong len;
        rs_avalret_t retc;
        size_t buf_len;

        if (d < 0) {
            prec++;
        }
        SsDoubleToAscii(d, buf, prec);
        len = RS_ATYPE_LENGTH(cd, dst_atype);
        retc = RSAVR_SUCCESS;
        buf_len = strlen(buf);
        if (len > 0 && len < buf_len) {
            SsDoubleTruncateRetT rc;

            rc = SsTruncateAsciiDoubleValue(buf, len + 1);
            switch (rc) {
                case SS_DBLTRUNC_OK:
                    break;
                case SS_DBLTRUNC_TRUNCATED:
                    retc = RSAVR_TRUNCATION;
                    break;
                case SS_DBLTRUNC_VALUELOST:
                    return (RSAVR_FAILURE);
                default:
                    ss_error;
            }
            buf_len = strlen(buf);
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
        buf_len += 1;
        AVAL_SETDATA(cd, dst_atype, dst_aval, buf, buf_len);
        return (retc);
}

/*#***********************************************************************\
 *
 *		rs_aval_putdbltouni
 *
 * Sets double to RSDT_UNICODE aval
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
 *	d -
 *
 *
 *	prec -
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
rs_avalret_t rs_aval_putdbltouni(
        void* cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        double d,
        int prec)
{
        rs_avalret_t retc;
        ss_char2_t buf[48];
        ulong len;
        size_t buf_len;

        if (d < 0.0) {
            prec++;
        }
        SsDoubleToWcs(d, buf, prec);
        len = RS_ATYPE_LENGTH(cd, dst_atype);
        retc = RSAVR_SUCCESS;
        buf_len = SsWcslen(buf);
        if (len > 0 && len < buf_len) {
            SsDoubleTruncateRetT rc;

            rc = SsTruncateWcsDoubleValue(buf, len + 1);
            switch (rc) {
                case SS_DBLTRUNC_OK:
                    break;
                case SS_DBLTRUNC_TRUNCATED:
                    retc = RSAVR_TRUNCATION;
                    break;
                case SS_DBLTRUNC_VALUELOST:
                    return (RSAVR_FAILURE);
                default:
                    ss_error;
            }
            buf_len = SsWcslen(buf);
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
        AVAL_SETDATACHAR2(cd, dst_atype, dst_aval, buf, buf_len);
        return (retc);
}


/*##**********************************************************************\
 *
 *		rs_aval_putchartodate
 *
 * Sets asciiz value to RSDT_DATE aval
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
 *	s -
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
rs_avalret_t rs_aval_putchartodate(
        void*       cd,
        rs_atype_t* dst_atype,
        rs_aval_t*  dst_aval,
        ss_char1_t* s)
{
        rs_avalret_t retc = RSAVR_SUCCESS;
        ss_char1_t* dtformat;
        dt_date_t tmp_date;
        dt_datesqltype_t datesqltype;
        rs_sqldatatype_t sqldt;
        bool succp;

        sqldt = RS_ATYPE_SQLDATATYPE(cd, dst_atype);
        dtformat = rs_atype_getdefaultdtformat(cd, dst_atype);
        succp = dt_date_setasciiz_ext(
                    &tmp_date,
                    dtformat,
                    s,
                    &datesqltype);
        if (!succp
        &&  dtformat != NULL
        &&  sqldt == RSSQLDT_TIMESTAMP)
        {
            /* User given timestamp format failed. Try with
             * time only format. If time only format succeeds,
             * we use current day as the data part.
             */
            succp = dt_date_setasciiz_ext(
                        &tmp_date,
                        rs_sysi_timeformat(cd),
                        s,
                        &datesqltype);
        }
        if (!succp && dtformat != NULL) {
            /* User given timestamp format failed. Try with
             * strict SQL format.
             */
            succp = dt_date_setasciiz_ext(
                        &tmp_date,
                        NULL,
                        s,
                        &datesqltype);
        }
        if (succp) {
            switch (sqldt) {
                case RSSQLDT_TIMESTAMP:
                    if (datesqltype == DT_DATE_SQLTIME) {
                        ss_dassert(dt_date_istime(&tmp_date));
                        succp = dt_date_padtimewithcurdate(&tmp_date);
                    }
                    break;
                case RSSQLDT_DATE:
                    if (datesqltype == DT_DATE_SQLTIME) {
                        ss_dassert(dt_date_istime(&tmp_date));
                        succp = FALSE;
                    } else {
                        if (dt_date_hour(&tmp_date) != 0
                        ||  dt_date_min(&tmp_date) != 0
                        ||  dt_date_sec(&tmp_date) != 0
                        ||  dt_date_fraction(&tmp_date) != 0UL)
                        {
                            retc = RSAVR_TRUNCATION;
                            succp = dt_date_truncatetodate(&tmp_date);
                        }
                    }
                    break;
                case RSSQLDT_TIME:
                    if (datesqltype == DT_DATE_SQLTIMESTAMP) {
                        ss_dassert(!dt_date_istime(&tmp_date));
                        succp = dt_date_truncatetotime(&tmp_date);
                        retc = RSAVR_TRUNCATION;
                        ss_dassert(succp);
                    } else if (datesqltype == DT_DATE_SQLDATE) {
                        succp = FALSE;
                    }
                    break;
                default:
                    ss_rc_error(sqldt);
            }
            if (succp) {
                ss_dassert(!SU_BFLAG_TEST(dst_aval->ra_flags,
                                          RA_ONLYCONVERTED));
                if (SU_BFLAG_TEST(dst_aval->ra_flags,
                                  RA_VTPLREF | RA_FLATVA | RA_NULL))
                {
                    dst_aval->ra_va = NULL;
                } else {
                    ss_derror; /* va should never be dynamic for RSDT_DATE! */
                    refdva_free(&dst_aval->ra_va);
                }
                SU_BFLAG_CLEAR(dst_aval->ra_flags,
                               RA_NULL | RA_VTPLREF | RA_CONVERTED | RA_UNKNOWN);
                SU_BFLAG_SET(dst_aval->ra_flags, RA_FLATVA);
                dt_date_datetova(&tmp_date, &dst_aval->ra_vabuf.va);
                dst_aval->ra_va = &dst_aval->ra_vabuf.va;
                return (retc);
            }
        }
        return (RSAVR_FAILURE);
}

bool rs_aval_putvadatachar2to1(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char2_t* data,
        size_t dlen)
{
        bool succp;
        size_t glen;

        CHECK_AVAL(aval);
        glen = dlen + 1; /* include extra null-byte! */
        glen = VA_GROSSLENBYNETLEN(glen);
        refdva_free(&aval->ra_va);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA));
        if (glen <= sizeof(aval->ra_vabuf)) {
            succp = va_setvadatachar2to1(&aval->ra_vabuf.va, data, dlen);
            aval->ra_va = &aval->ra_vabuf.va;
            SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
        } else {
            succp = refdva_setvadatachar2to1(&aval->ra_va, data, dlen);
        }
        CHECK_AVAL(aval);
        return (succp);
}

bool rs_aval_putdatachar2to1(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        ss_char2_t* data,
        size_t dlen)
{
        bool succp;
        size_t glen;

        CHECK_AVAL(aval);
        glen = dlen + 1; /* include extra null-byte! */
        glen = VA_GROSSLENBYNETLEN(glen);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA));
        refdva_free(&aval->ra_va);
        if (glen <= sizeof(aval->ra_vabuf)) {
            succp = va_setdatachar2to1(&aval->ra_vabuf.va, data, dlen);
            aval->ra_va = &aval->ra_vabuf.va;
            SU_BFLAG_SET(aval->ra_flags, RA_FLATVA);
        } else {
            succp = refdva_setdatachar2to1(&aval->ra_va, data, dlen);
        }
        CHECK_AVAL(aval);
        return (succp);
}

