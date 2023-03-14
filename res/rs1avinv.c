/*************************************************************************\
**  source       * rs1avinv.c
**  directory    * res
**  description  * Aval inversion to descending key value routines
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

#include <ssdebug.h>
#include <ssmem.h>
#include <ssdtoa.h>
#include <ssltoa.h>
#include <ssfloat.h>
#include <sstraph.h>
#include <ssscan.h>
#include <sschcvt.h>
#include <ssutf.h>

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
#include <sswctype.h>
#include <su0wlike.h>


static const ss_byte_t aval_double_invnull_va[10] = {
        '\x09',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\0'
};

static const ss_byte_t aval_float_invnull_va[6] = {
        '\x05',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\0'
};

static const ss_byte_t aval_dfl_invnull_va[3] = {
        '\x02',
        '\xFF',
        '\xFF'
};

/* happens to be the same as for dfloat! */
#define aval_bigint_invnull_va aval_dfl_invnull_va

static const ss_byte_t aval_int_invnull_va[6] = {
        '\x05',
        '\xFF',
        '\xFF',
        '\xFF',
        '\xFF',
        '\0'
};

static const ss_byte_t aval_char_invnull_va[3] = {
        '\x02',
        '\xFF',
        '\x00' /* this extra null byte makes it bigger than empty string */
};

static const ss_byte_t aval_wchar_invnull_va[4] = {
        '\x02',
        '\xFF',
        '\xFF',
        '\x00' /* this extra null byte makes it bigger than empty string */
};

static const ss_byte_t aval_date_invnull_va[4] = {
        '\x03',
        '\xFF',
        '\xFF',
        '\xFF'
};

/*##**********************************************************************\
 *
 *		aval_meminvcpy
 *
 * like memcpy, but inverts every bit (bitwise not).
 *
 * Parameters :
 *
 *	dst - out, use
 *		pointer to destination buffer
 *
 *	src - in, use
 *		pointer to source buffer
 *
 *	size - in
 *		number of bytes to copy and invert
 *
 * Return value :
 *
 * Comments :
 *      uses bytewise copy because dest and source are
 *      often misaligned. Optimization to wordwise copy
 *      whenever possible is possible of course, but
 *      this code should not be a performance bottleneck
 *      after all.
 *
 * Globals used :
 *
 * See also :
 */
void aval_meminvcpy(void* dest, const void* src, size_t size)
{
        ss_byte_t* d = dest;
        const ss_byte_t* s = src;

        while (size) {
            size--;
            *d = ~(*s);
            s++;
            d++;
        }
}

/*#***********************************************************************\
 *
 *		aval_invertrefdva
 *
 * inverts refdva either ascending to descending or vice versa
 *
 * Parameters :
 *
 *	p_refdva - use
 *		pointer to refdva object
 *
 *	padcount - in
 *		 0 - preserve original length
 *           1 - add one 0xff byte (converting UNICODE to descending)
 *          -1 - remove one byte from end (UNICODE to ascending)
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void aval_invertrefdva(refdva_t* p_refdva, int padcount)
{
        void* srcdata;
        void* destdata;
        va_index_t dlen;
        va_index_t res_dlen;
        size_t bytestocopy;
        refdva_t res = refdva_init();

        srcdata = va_getdata(*p_refdva, &dlen);
        refdva_setdata(&res, NULL, dlen + padcount);
        destdata = va_getdata(res, &res_dlen);
        if (padcount == 1) {
            ss_dassert(padcount == 1);
            bytestocopy = dlen;
            ((ss_byte_t*)destdata)[dlen] = (ss_byte_t)0xFF;
        } else {
            ss_dassert(padcount == 0 || padcount == -1);
            bytestocopy = res_dlen;
            ss_debug(
                if (padcount == -1) {
                    ss_assert(((ss_byte_t*)destdata)[dlen-1] == 0xFF);
                }
            );
        }
        aval_meminvcpy(destdata, srcdata, bytestocopy);
        refdva_done(p_refdva);
        *p_refdva = res;
}

static void aval_intrefdvatodesc(refdva_t* p_refdva)
{
        refdva_t res = refdva_init();
        ss_int4_t int4buf;
        ss_int4_t int4;

        int4 = ~((ss_int4_t)va_getlong(*p_refdva) + 0x80000000L);
        SS_UINT4_STORE_MSB1ST(&int4buf, int4);
        refdva_setdata(&res, &int4buf, (va_index_t)sizeof(int4buf));
        refdva_done(p_refdva);
        *p_refdva = res;
}

static void aval_intrefdvatoasc(refdva_t* p_refdva)
{
        refdva_t res = refdva_init();
        ss_int4_t int4;
        ss_int4_t* p_int4buf;

        p_int4buf = (ss_int4_t*)VA_GETASCIIZ(*p_refdva);
        int4 = SS_UINT4_LOAD_MSB1ST(p_int4buf);
        int4 = ~int4 - 0x80000000L;
        refdva_setlong(&res, (long)int4);
        refdva_done(p_refdva);
        *p_refdva = res;
}


/*##**********************************************************************\
 *
 *		aval_dflrefdvatodesc
 *
 * Converts a DECIMAL or NUMERIC aval to descending
 *
 * Parameters :
 *
 *	p_refdva - in out, use
 *		pointer to refdva object
 *
 *	p_dfl - in, use
 *		pointer to dt_dfl_t type object or NULL when
 *          not available
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void aval_dflrefdvatodesc(
        refdva_t* p_refdva,
        dt_dfl_t* p_dfl)
{
        refdva_t res = refdva_init();
        va_t va;
        dt_dfl_t dfl;
        bool succp;

        if (p_dfl == NULL) {
            p_dfl = &dfl;
            succp = dt_dfl_setva(&dfl, *p_refdva);
            ss_dassert(succp);
        }
        dt_dfl_change_sign(p_dfl);
        dt_dfl_dfltova(*p_dfl, &va);
        refdva_setva(&res, &va);
        refdva_done(p_refdva);
        *p_refdva = res;
}

/*##**********************************************************************\
 *
 *		aval_dflrefdvatoasc
 *
 * Converts a DECIMAL or NUMERIC refdva to ascending
 *
 * Parameters :
 *
 *	p_refdva - in out, use
 *		pointer to refdva object
 *
 *	pp_dfl - out, give
 *		pointer to converted dt_dfl_t value or NULL
 *          when converted value is not desired
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void aval_dflrefdvatoasc(
        refdva_t* p_refdva,
        dt_dfl_t* p_dfl
        )
{
        bool succp;
        refdva_t res = refdva_init();
        va_t va;
        dt_dfl_t dummy_dfl;

        if (p_dfl == NULL) {
            p_dfl = &dummy_dfl;
        }
        succp = dt_dfl_setva(p_dfl, *p_refdva);
        ss_dassert(succp);
        dt_dfl_change_sign(p_dfl);
        dt_dfl_dfltova(*p_dfl, &va);

        refdva_setva(&res, &va);
        refdva_done(p_refdva);
        *p_refdva = res;
}

/*##**********************************************************************\
 *
 *		rs_aval_isdesc
 *
 * Tests wheter an aval is descending
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in, use
 *		attribute value
 *
 * Return value :
 *      TRUE if descending (TRUE is non-FALSE here!)
 *      FALSE when ascending (normal)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_aval_isdesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);
        CHECK_AVAL(aval);

        return (SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
}

/*##**********************************************************************\
 *
 *		rs_aval_setdesc
 *
 * Sets an aval to descending without any conversion of value
 *
 * Parameters :
 *
 *	cd - in
 *		client data
 *
 *	atype - in
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_aval_setdesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        SS_NOTUSED(cd);
        SS_NOTUSED(atype);
        CHECK_AVAL(aval);
        SU_BFLAG_SET(aval->ra_flags, RA_DESC);
}

/*##**********************************************************************\
 *
 *		rs_aval_invnull_va
 *
 * Returns inverted NULL value in v-attribute format,
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
va_t* rs_aval_invnull_va(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype)
{
        va_t* va = NULL;

        ss_dassert(atype != NULL);
        switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
            case RSSQLDT_BIT:
                ss_derror;
            case RSSQLDT_BIGINT:
                va = (va_t*)aval_bigint_invnull_va;
                break;
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_BINARY:
                ss_derror;
                va = NULL;
                break;
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_CHAR:
                va = (va_t*)aval_char_invnull_va;
                break;
            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WCHAR:
                va = (va_t*)aval_wchar_invnull_va;
                break;
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
                va = (va_t*)aval_date_invnull_va;
                break;
            case RSSQLDT_REAL:
                va = (va_t*)aval_float_invnull_va;
                break;
            case RSSQLDT_FLOAT:
            case RSSQLDT_DOUBLE:
                va = (va_t*)aval_double_invnull_va;
                break;
            case RSSQLDT_TINYINT:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_INTEGER:
                va = (va_t*)aval_int_invnull_va;
                break;
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
                va = (va_t*)aval_dfl_invnull_va;
                break;
            default:
                ss_error;
        }
        return(va);
}

/*##**********************************************************************\
 *
 *		rs_aval_asctodesc
 *
 * Converts an ascending aval to descending
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_aval_asctodesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool succp = TRUE;
        va_t* va;

        CHECK_AVAL(aval);
        ss_dassert(atype != NULL);
        SS_PUSHNAME("rs_aval_asctodesc");
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        if (SU_BFLAG_TEST(aval->ra_flags, RA_NULL)) {
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA));
            aval->RA_RDVA = refdva_init();
            va = rs_aval_invnull_va(cd, atype);
            if (va == NULL) {
                SS_POPNAME;
                return (FALSE);
            }
            rs_aval_setva(cd, atype, aval, va);
        } else {
            int padcount;

            switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
                case RSSQLDT_BIT:
                    ss_derror;
                case RSSQLDT_BIGINT: {
                    ss_int8_t i8;
                    ss_uint4_t u4_hi;
                    ss_uint4_t u4_lo;
                    i8 = rs_aval_getint8(cd, atype, aval);
                    u4_hi = ~SsInt8GetMostSignificantUint4(i8);
                    u4_lo = ~SsInt8GetLeastSignificantUint4(i8);
                    SsInt8Set2Uint4s(&i8, u4_hi, u4_lo);
                    rs_aval_putint8(cd, atype, aval, i8);
                    (void)rs_aval_va(cd, atype, aval);
                    ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                    ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED));
                    ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA));
                    break;
                }
                case RSSQLDT_LONGVARBINARY:
                case RSSQLDT_VARBINARY:
                case RSSQLDT_BINARY:
                    ss_derror;
                    SS_POPNAME;
                    return (FALSE);
                case RSSQLDT_LONGVARCHAR:
                case RSSQLDT_VARCHAR:
                case RSSQLDT_CHAR:
                    va = (va_t*)aval_char_invnull_va;
                    padcount = 0;
char_common_case:;
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                    }
                    aval_invertrefdva(&aval->RA_RDVA, padcount);
                    succp = (0 > va_compare(
                                    aval->RA_RDVA,
                                    va));
                    break;
                case RSSQLDT_WLONGVARCHAR:
                case RSSQLDT_WVARCHAR:
                case RSSQLDT_WCHAR:
                    va = (va_t*)aval_wchar_invnull_va;
                    padcount = 1;
                    goto char_common_case;
                case RSSQLDT_DATE:
                case RSSQLDT_TIME:
                case RSSQLDT_TIMESTAMP:
                case RSSQLDT_REAL:
                case RSSQLDT_FLOAT:
                case RSSQLDT_DOUBLE:
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED)) {
                        (void)rs_aval_va(cd, atype, aval);
                        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                                  RA_ONLYCONVERTED));
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                    }
                    aval_invertrefdva(&aval->RA_RDVA, 0);
                    break;
                case RSSQLDT_TINYINT:
                case RSSQLDT_SMALLINT:
                case RSSQLDT_INTEGER:
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_NULL)) {
                        break;
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED)) {
                        (void)rs_aval_va(cd, atype, aval);
                        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags,
                                                  RA_ONLYCONVERTED));
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                    }
                    aval_intrefdvatodesc(&aval->RA_RDVA);
                    break;
                case RSSQLDT_NUMERIC:
                case RSSQLDT_DECIMAL:
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_NULL)) {
                        break;
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                    }
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
                        aval_dflrefdvatodesc(
                            &aval->ra_va,
                            &aval->ra_.dfl);
                    } else {
                        aval_dflrefdvatodesc(
                            &aval->ra_va,
                            NULL);
                    }
                    break;
                default:
                    ss_error;
            }
        }
        SU_BFLAG_CLEAR(aval->ra_flags, RA_CONVERTED | RA_NULL | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_DESC);
        SS_POPNAME;
        return (succp);
}

/*##**********************************************************************\
 *
 *		rs_aval_desctoasc
 *
 * Converts a descending aval to ascending
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_aval_desctoasc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        bool isnull = FALSE;
        int cmp;
        va_t* invnull_va;
        int padcount = 0;

        CHECK_AVAL(aval);
        ss_dassert(atype != NULL);
        SS_PUSHNAME("rs_aval_desctoasc");
        ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_CHAR:
                invnull_va = (va_t*)aval_char_invnull_va;
                goto common_case;
            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WCHAR:
                invnull_va = (va_t*)aval_wchar_invnull_va;
                padcount = -1;
                goto common_case;
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
                invnull_va = (va_t*)aval_date_invnull_va;
                goto common_case;
            case RSSQLDT_REAL:
                invnull_va = (va_t*)aval_float_invnull_va;
                goto common_case;
            case RSSQLDT_FLOAT:
            case RSSQLDT_DOUBLE:
                invnull_va = (va_t*)aval_double_invnull_va;
                goto common_case;
            case RSSQLDT_TINYINT:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_INTEGER:
                cmp = va_compare(aval->RA_RDVA, (va_t*)aval_int_invnull_va);
                if (cmp == 0) {
                    isnull = TRUE;
                } else {
                    ss_dassert(cmp < 0);
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                    }
                    aval_intrefdvatoasc(&aval->RA_RDVA);
                }
                break;
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL:
                cmp = va_compare(aval->RA_RDVA, (va_t*)aval_dfl_invnull_va);
                if (cmp == 0) {
                    isnull = TRUE;
                } else {
                    ss_dassert(cmp < 0);
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                        rs_aval_removevtplref(cd, atype, aval);
                    }
                    ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
                    if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                        ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                        aval->ra_va = NULL;
                        refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                        SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
                    }
                    ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED));
                    aval_dflrefdvatoasc(&aval->ra_va, &aval->ra_.dfl);
                    SU_BFLAG_SET(aval->ra_flags, RA_CONVERTED);
                }
                break;
            case RSSQLDT_BIGINT: {
                va_t* va;
                ss_int8_t i8;
                ss_uint4_t u4_hi;
                ss_uint4_t u4_lo;

                ss_dassert(SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
                SU_BFLAG_CLEAR(aval->ra_flags, RA_DESC);
                va = rs_aval_va(cd, atype, aval);
                if (va_compare(va, (va_t*)aval_bigint_invnull_va) == 0) {
                    isnull = TRUE;
                } else {
                    i8 = rs_aval_getint8(cd, atype, aval);
                    u4_hi = ~SsInt8GetMostSignificantUint4(i8);
                    u4_lo = ~SsInt8GetLeastSignificantUint4(i8);
                    SsInt8Set2Uint4s(&i8, u4_hi, u4_lo);
                    rs_aval_putint8(cd, atype, aval, i8);
                }
                break;
            }
            case RSSQLDT_BIT:
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_BINARY:
            default:
                ss_rc_error(RS_ATYPE_SQLDATATYPE(cd, atype));
        }
return_code:;
        SU_BFLAG_CLEAR(aval->ra_flags, RA_DESC);
        if (isnull) {
            rs_aval_setnull(cd, atype, aval);
        }
        SS_POPNAME;
        return;
common_case:;
        cmp = va_compare(aval->RA_RDVA, invnull_va);
        if (cmp == 0) {
            isnull = TRUE;
        } else {
            ss_dassert(cmp < 0);
            ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_ONLYCONVERTED));
            if (SU_BFLAG_TEST(aval->ra_flags, RA_FLATVA)) {
                ss_dassert(aval->ra_va == &aval->ra_vabuf.va);
                aval->ra_va = NULL;
                refdva_setva(&aval->ra_va, &aval->ra_vabuf.va);
                SU_BFLAG_CLEAR(aval->ra_flags, RA_FLATVA);
            }
            if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF)) {
                rs_aval_removevtplref(cd, atype, aval);
            }
            aval_invertrefdva(&aval->RA_RDVA, padcount);
        }
        goto return_code;
}

/*#***********************************************************************\
 *
 *		aval_likepatasctodesc_w
 *
 * Like pattern ascending to descending convert for UNICODE like pattern
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
 *	old_esc -
 *
 *
 *	new_esc -
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
static void aval_likepatasctodesc_w(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype __attribute__ ((unused)),
        rs_aval_t* aval,
        int old_esc,
        int new_esc)
{
        static const ss_char2_t invnull_char2 = ~0;
        ss_char2_t* s;
        va_index_t s_len;
        va_index_t i;
        dynva_t res = dynva_init();
        ss_char2_t res_s[2];
        va_index_t res_s_len;

        ss_dassert(new_esc != SU_SLIKE_NOESCCHAR);
        ss_dassert(RS_ATYPE_DATATYPE(cd, atype) == RSDT_UNICODE);

        s = va_getdata(aval->RA_RDVA, &s_len);
        ss_dassert(s_len & 1);
        s_len /= sizeof(ss_char2_t);

        for (i = 0; i < s_len; i++) {
            ss_char2_t* p;
            ss_char2_t* p2;
            ss_char2_t c;

            res_s_len = 0;

            p = s + i;
            c = SS_CHAR2_LOAD_MSB1ST(p);
            switch (c) {
                case '%':
                case '_':
                    res_s[res_s_len++] = s[i];
                    break;
                case ((ss_char2_t)~'%'):
                case ((ss_char2_t)~'_'):
                    p2 = res_s + res_s_len;
                    res_s_len++;
                    SS_CHAR2_STORE_MSB1ST(p2, new_esc);
                    p2 = res_s + res_s_len;
                    c = ~c;
                    res_s_len++;
                    SS_CHAR2_STORE_MSB1ST(p2, c);
                    break;
                default:
                    if (c == old_esc) {
                        p2 = res_s + res_s_len;
                        res_s_len++;
                        SS_CHAR2_STORE_MSB1ST(p2, new_esc);
                        i++;
                        p = s + i;
                        c = SS_CHAR2_LOAD_MSB1ST(p);
                        ss_dassert(i < s_len);
                    } else if ((ss_char2_t)~c == new_esc) {
                        p2 = res_s + res_s_len;
                        res_s_len++;
                        SS_CHAR2_STORE_MSB1ST(p2, new_esc);
                    }
                    p2 = res_s + res_s_len;
                    c = ~c;
                    res_s_len++;
                    SS_CHAR2_STORE_MSB1ST(p2, c);
                    break;
            }
            if (res == NULL) {
                dynva_setdata(&res, res_s, res_s_len);
            } else {
                dynva_appdata(&res, res_s, res_s_len);
            }
        }
        if (res == NULL) {
            dynva_setdata(&res, (void*)&invnull_char2, sizeof(ss_char2_t));
        } else {
            dynva_appdata(&res, (void*)&invnull_char2, sizeof(ss_char2_t));
        }
        if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
            aval->RA_RDVA = refdva_init();
            SU_BFLAG_CLEAR(aval->ra_flags, RA_VTPLREF | RA_FLATVA);
        }
        refdva_setva(&aval->RA_RDVA, res);
        dynva_free(&res);
        SU_BFLAG_SET(aval->ra_flags, RA_DESC);
}

/*##**********************************************************************\
 *
 *		rs_aval_likepatasctodesc
 *
 * Creates a like pattern equivalent for descending key value.
 * it is only to be used by DBE when using low-level comparison
 * using descending key parts. It is not legal to use rs_aval_like()
 * with descending avals.
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	atype - in, use
 *		attribute type
 *
 *	aval - in out, use
 *		attribute value
 *
 *	old_esc - in
 *		escape character or RS_CONS_NOESCCHAR when no escape
 *          character is specified
 *
 *	new_esc - in
 *		escape character used in converted string, cannot be
 *		RS_CONS_NOESCCHAR
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_aval_likepatasctodesc(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        int old_esc,
        int new_esc)
{
        ss_byte_t* s;
        va_index_t s_len;
        va_index_t i;
        dynva_t res = dynva_init();
        ss_byte_t res_s[2];
        va_index_t res_s_len;

        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_NULL));
        if (RS_ATYPE_DATATYPE(cd,atype) == RSDT_UNICODE) {
            aval_likepatasctodesc_w(cd, atype, aval, old_esc, new_esc);
            return;
        }
        ss_dassert((ss_byte_t)old_esc == old_esc || old_esc == SU_SLIKE_NOESCCHAR);
        ss_dassert(new_esc != SU_SLIKE_NOESCCHAR);
        ss_dassert((ss_byte_t)new_esc == new_esc);

        s = va_getdata(aval->RA_RDVA, &s_len);
        for (i = 0; i < s_len; i++) {
            res_s_len = 0;
            switch (s[i]) {
                case '%':
                case '_':
                    res_s[res_s_len++] = s[i];
                    break;
                case ~'%' & UCHAR_MAX:
                case ~'_' & UCHAR_MAX:
                    res_s[res_s_len++] = (ss_byte_t)new_esc;
                    res_s[res_s_len++] = ~s[i];
                    break;
                default:
                    if (s[i] == old_esc) {
                        res_s[res_s_len++] = (ss_byte_t)new_esc;
                        i++;
                        ss_dassert(i < s_len);
                    } else if ((ss_byte_t)~s[i] == new_esc) {
                        res_s[res_s_len++] = (ss_byte_t)new_esc;
                    }
                    res_s[res_s_len++] = ~s[i];
                    break;
            }
            if (res == NULL) {
                dynva_setdata(&res, res_s, res_s_len);
            } else {
                dynva_appdata(&res, res_s, res_s_len);
            }
        }
        if (SU_BFLAG_TEST(aval->ra_flags, RA_VTPLREF | RA_FLATVA)) {
            aval->RA_RDVA = refdva_init();
            SU_BFLAG_CLEAR(aval->ra_flags, RA_VTPLREF | RA_FLATVA);
        }
        refdva_setva(&aval->RA_RDVA, res);
        dynva_free(&res);
        SU_BFLAG_SET(aval->ra_flags, RA_DESC);
}

#ifdef SS_COLLATION

static va_t* aval_getnonnulldescva(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        ss_byte_t buf[/* bufsize */],
        size_t bufsize,
        va_t* va)
{
        int padcount; /* legal values -1, 0, 1 */
        
        switch (RS_ATYPE_SQLDATATYPE(cd, atype)) {
            default:
                ss_derror;
            case RSSQLDT_BIT:
                ss_derror;
            case RSSQLDT_LONGVARBINARY:
            case RSSQLDT_VARBINARY:
            case RSSQLDT_BINARY:
                ss_derror;
                return (NULL);
            case RSSQLDT_BIGINT: {
                ss_int8_t i8;
                ss_uint4_t u4_hi;
                ss_uint4_t u4_lo;
                i8 = rs_aval_getint8(cd, atype, aval);
                u4_hi = ~SsInt8GetMostSignificantUint4(i8);
                u4_lo = ~SsInt8GetLeastSignificantUint4(i8);
                SsInt8Set2Uint4s(&i8, u4_hi, u4_lo);

                va = bufva_allocva(buf, bufsize, 10);
                va_setint8(va, i8);
                goto va_set;
            }
            case RSSQLDT_TINYINT:
            case RSSQLDT_SMALLINT:
            case RSSQLDT_INTEGER: {
                ss_uint4_t u4;
                ss_byte_t u4_msb1stbuf[4];
                u4 = (ss_uint4_t)rs_aval_getlong(cd, atype, aval);
                u4 = ~u4 + (ss_uint4_t)0x80000000UL;
                SS_UINT4_STORE_MSB1ST(u4_msb1stbuf, u4);
                bufva_setdata(buf, bufsize, u4_msb1stbuf, 4);
                break;
            }
            case RSSQLDT_LONGVARCHAR:
            case RSSQLDT_VARCHAR:
            case RSSQLDT_CHAR: 
                padcount = 0;
            char_common_case:;
                {
                    ss_byte_t* data;
                    va_index_t datalen;
                    int dest_netlen;
                    int dest_lenlen;
                    ss_byte_t* dest_data;
                    size_t bytestocopy;
                    
                    data = va_getdata(va, &datalen);
                    dest_netlen = (int)datalen + padcount;
                    dest_lenlen = BYTE_LEN_TEST(dest_netlen) ?
                        VA_LENGTHMINLEN : VA_LENGTHMAXLEN;
                    va = bufva_allocva(buf, bufsize,
                                       (size_t)(dest_lenlen + dest_netlen));
                    va_setdata(va, NULL, (va_index_t)dest_netlen);
                    dest_data = (ss_byte_t*)va + dest_lenlen;
                    if (padcount == 1) {
                        bytestocopy = datalen;
                        dest_data[datalen] = (ss_byte_t)0xFF;
                    } else {
                        bytestocopy = dest_netlen;
                    }
                    aval_meminvcpy(dest_data, data, bytestocopy); 
                }
                goto va_set;
            case RSSQLDT_WLONGVARCHAR:
            case RSSQLDT_WVARCHAR:
            case RSSQLDT_WCHAR:
                padcount = 1;
                goto char_common_case;
            case RSSQLDT_DATE:
            case RSSQLDT_TIME:
            case RSSQLDT_TIMESTAMP:
            case RSSQLDT_REAL:
            case RSSQLDT_FLOAT:
            case RSSQLDT_DOUBLE:
                padcount = 0;
                goto char_common_case;
                
            case RSSQLDT_NUMERIC:
            case RSSQLDT_DECIMAL: {
                dt_dfl_t dfl;
                if (!SU_BFLAG_TEST(aval->ra_flags, RA_CONVERTED)) {
                    dfl = rs_aval_getdfloat(cd, atype, aval);
                } else {
                    dfl = aval->ra_.dfl;
                }
                va = bufva_allocva(buf, bufsize, sizeof(va_t));
                dt_dfl_change_sign(&dfl);
                dt_dfl_dfltova(dfl, va);
                goto va_set;
            }
        }
        va = bufva_getva(buf, bufsize);
va_set:;
        ss_dassert(va == bufva_getva(buf, bufsize));
        return (va);
}

va_t* rs_aval_getkeyva(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_collation_t* collation,
        rs_attrtype_t attrtype,
        bool ascending,
        ss_byte_t buf[/* bufsize */],
        size_t bufsize,
        size_t prefixlen /* characters, unlimited if 0 */)
{
        va_t* va;
        ss_byte_t* data;
        va_index_t datalen;
        size_t maxlen;
        size_t maxgrosslen;
        size_t actual_len;
        ss_byte_t* dbuf = NULL;
        size_t dbufsize;
        union {
                void* dummy_for_alignment;
                ss_byte_t _buf[BUFVA_MAXBUFSIZE];
        } tmp_key;

        CHECK_AVAL(aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));

        bufva_init(tmp_key._buf, sizeof(tmp_key._buf));

        if (SU_BFLAG_TEST(aval->ra_flags, RA_NULL)) {
            if (ascending) {
                va = VA_NULL;
                ss_dassert(va->c[0] == 0);
            } else {
                va = rs_aval_invnull_va(cd, atype);
            }
        } else {
            va = rs_aval_va(cd, atype, aval);
            if (attrtype == RSAT_COLLATION_KEY && collation != NULL) {
                bool succp;

                switch (rs_atype_attrtype(cd, atype)) {
                    case RSAT_TUPLE_ID:
                    case RSAT_TUPLE_VERSION:
                        ss_error;
                    default:
                        break;
                }
                
                data = va_getdata(va, &datalen);

                maxlen = su_collation_get_maxlen_for_weightstr(
                            collation,
                            (void*)data,
                            datalen,
                            prefixlen);

                if (ascending) {
                    dbuf = buf;
                    dbufsize = bufsize;
                } else {
                    dbuf = tmp_key._buf;
                    dbufsize = sizeof(tmp_key._buf);
                }

                ++maxlen; /* + 1 for null terminator */

                maxgrosslen = (maxlen) +
                    (BYTE_LEN_TEST(maxlen)? VA_LENGTHMINLEN : VA_LENGTHMAXLEN);
                
                va = bufva_allocva(dbuf, dbufsize, maxgrosslen);
                succp = su_collation_create_weightstr(
                            collation,
                            data,
                            datalen,
                            prefixlen,
                            (void*)((ss_byte_t*)va + VA_LENGTHMINLEN),
                            maxlen,
                            &actual_len);

                ss_dassert(succp);

                if (!succp) {
                    goto failed;
                }

                if (!BYTE_LEN_TEST(actual_len)) {
                    memmove((ss_byte_t*)va + VA_LENGTHMAXLEN,
                            (ss_byte_t*)va + VA_LENGTHMINLEN,
                            actual_len);
                }
                /* NULL as data pointer to va_setdata()
                 * sets only length field!
                 */
                va_setdata(va, NULL, (va_index_t)actual_len);
            }
            if (!ascending) {
                va = aval_getnonnulldescva(cd, atype, aval, buf, bufsize, va);
            }
        }
cleanup_ret_va:;
        bufva_done(tmp_key._buf, sizeof(tmp_key._buf));
        return (va);
failed:;
        va = NULL;
        goto cleanup_ret_va;
}

void rs_aval_convert_to_collation_key(rs_sysi_t* cd,
                                      rs_atype_t* atype,
                                      rs_aval_t* aval,
                                      su_collation_t* collation)
{
        va_t* va;
        union {
                void* dummy_for_alignment;
                ss_byte_t _buf[BUFVA_MAXBUFSIZE];
        } tmp_key;

        CHECK_AVAL(aval);
        ss_dassert(!SU_BFLAG_TEST(aval->ra_flags, RA_DESC));
        if (SU_BFLAG_TEST(aval->ra_flags, RA_COLLATION_KEY)) {
            return;
        }
        bufva_init(tmp_key._buf, sizeof(tmp_key._buf));
        va = rs_aval_getkeyva(cd, atype, aval, collation, RSAT_COLLATION_KEY,
                              TRUE, tmp_key._buf, sizeof(tmp_key._buf), 0);
        rs_aval_setva(cd, atype, aval, va);
        SU_BFLAG_SET(aval->ra_flags, RA_COLLATION_KEY);
        bufva_done(tmp_key._buf, sizeof(tmp_key._buf));
}        

#endif /* SS_COLLATION */
