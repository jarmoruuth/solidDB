/*************************************************************************\
**  source       * rs1avcmp.c
**  directory    * res
**  description  * Comparisons between avals
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

typedef int aval_cmpfunc_t(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh);

static aval_cmpfunc_t
        ill_cmp,
        trivial,
        chr_v_chr,
        chr_v_dte,
        chr_v_uni,
        int_v_int,
        int_v_flt,
        int_v_dbl,
        int_v_dfl,
        int_v_int8,
        flt_v_dbl,
        flt_v_dfl,
        flt_v_int8,
        dbl_v_dfl,
        dbl_v_int8,
        dfl_v_int8,
        dte_v_dte,
        dte_v_uni,
        bin_v_bin,
        uni_v_uni,
        int8_v_int8,
        dbl_v_dbl,
        flt_v_flt;


#define INV_CMP ((aval_cmpfunc_t*)NULL)

static aval_cmpfunc_t * const cmp_matrix[RSDT_DIMENSION][RSDT_DIMENSION] =
{
               /*   RSDT_CHAR  RSDT_INTEGER RSDT_FLOAT  RSDT_DOUBLE RSDT_DATE   RSDT_DFLOAT RSDT_BINARY RSDT_UNICODE RSDT_BIGINT */
/* RSDT_CHAR    */ {chr_v_chr,  ill_cmp,    ill_cmp,    ill_cmp,    chr_v_dte,  ill_cmp,    ill_cmp,    chr_v_uni,   ill_cmp      },
/* RSDT_INTEGER */ {ill_cmp,    int_v_int,  int_v_flt,  int_v_dbl,  ill_cmp,    int_v_dfl,  ill_cmp,    ill_cmp,     int_v_int8   },
/* RSDT_FLOAT   */ {ill_cmp,    INV_CMP,    flt_v_flt,  flt_v_dbl,  ill_cmp,    flt_v_dfl,  ill_cmp,    ill_cmp,     flt_v_int8   },
/* RSDT_DOUBLE  */ {ill_cmp,    INV_CMP,    INV_CMP,    dbl_v_dbl,  ill_cmp,    dbl_v_dfl,  ill_cmp,    ill_cmp,     dbl_v_int8   },
/* RSDT_DATE    */ {INV_CMP,    ill_cmp,    ill_cmp,    ill_cmp,    dte_v_dte,  ill_cmp,    ill_cmp,    dte_v_uni,   ill_cmp      },
/* RSDT_DFLOAT  */ {ill_cmp,    INV_CMP,    INV_CMP,    INV_CMP,    ill_cmp,    trivial,    ill_cmp,    ill_cmp,     dfl_v_int8   },
/* RSDT_BINARY  */ {ill_cmp,    ill_cmp,    ill_cmp,    ill_cmp,    ill_cmp,    ill_cmp,    bin_v_bin,  ill_cmp,     ill_cmp      },
/* RSDT_UNICODE */ {INV_CMP,    ill_cmp,    ill_cmp,    ill_cmp,    INV_CMP,    ill_cmp,    ill_cmp,    uni_v_uni,   ill_cmp      },
/* RSDT_BIGINT  */ {ill_cmp,    INV_CMP,    INV_CMP,    INV_CMP,    ill_cmp,    INV_CMP,    ill_cmp,    ill_cmp,     int8_v_int8  }
};

/*##**********************************************************************\
 * 
 *              rs_atype_comppos_ext
 * 
 * Checks if comparison between the 2 types is possible.
 * (The actual comparison may still fail due to conversion
 * error)
 * 
 * Parameters : 
 * 
 *      cd - use
 *              client data
 *              
 *      atype1 - in, use
 *              first type
 *              
 *      atype2 - in, use
 *              second type
 *              
 * Return value :
 *      TRUE comparison is possible (eg. date vs. char or int vs. float)
 *      FALSE comparison is not possible (eg. date vs. float)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool rs_atype_comppos_ext(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_atype_t* atype2)
{
        rs_datatype_t dt1;
        rs_datatype_t dt2;

        dt1 = RS_ATYPE_DATATYPE(cd, atype1);
        dt2 = RS_ATYPE_DATATYPE(cd, atype2);
        if (cmp_matrix[dt1][dt2] != ill_cmp) {
            if (dt1 == RSDT_DATE && dt2 == RSDT_DATE) {
                rs_sqldatatype_t sqldt1;
                rs_sqldatatype_t sqldt2;
                
                sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
                sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
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

int rs_aval_cmp3_nullallowed(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        rs_datatype_t dt1;
        rs_datatype_t dt2;

        *p_succp = TRUE;
        dt1 = RS_ATYPE_DATATYPE(cd, atype1);
        dt2 = RS_ATYPE_DATATYPE(cd, atype2);
        if (RS_AVAL_ISNULL(cd, atype1, aval1)
        ||  RS_AVAL_ISNULL(cd, atype2, aval2))
        {
            if (!rs_atype_comppos_ext(cd, atype1, atype2)) {
                rs_error_create(
                    p_errh,
                    E_CMPTYPECLASH_SS,
                    rs_atype_name(cd, atype1),
                    rs_atype_name(cd, atype2));
                *p_succp = FALSE;
                return (0);
            }
            if (RS_AVAL_ISNULL(cd, atype1, aval1)) {
                if (RS_AVAL_ISNULL(cd, atype2, aval2)) {
                    return (0);
                }
                return (-1);
            }
            return (1);
        }
        if (dt1 > dt2) {
            ss_dassert(cmp_matrix[dt1][dt2] == INV_CMP || cmp_matrix[dt1][dt2] == ill_cmp);
            comp = -((*cmp_matrix[dt2][dt1])(cd, atype2, aval2, atype1, aval1, p_succp, p_errh));
        } else {
            comp = (*cmp_matrix[dt1][dt2])(cd, atype1, aval1, atype2, aval2, p_succp, p_errh);
        }
        return (comp);
}


int rs_aval_cmp3_notnull(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        rs_datatype_t dt1;
        rs_datatype_t dt2;

        *p_succp = TRUE;
        dt1 = RS_ATYPE_DATATYPE(cd, atype1);
        dt2 = RS_ATYPE_DATATYPE(cd, atype2);
        ss_dassert(!RS_AVAL_ISNULL(cd, atype1, aval1)
            &&     !RS_AVAL_ISNULL(cd, atype2, aval2));
        if (dt1 > dt2) {
            ss_dassert(cmp_matrix[dt1][dt2] == INV_CMP || cmp_matrix[dt1][dt2] == ill_cmp);
            comp = -((*cmp_matrix[dt2][dt1])(cd, atype2, aval2, atype1, aval1, p_succp, p_errh));
        } else {
            comp = (*cmp_matrix[dt1][dt2])(cd, atype1, aval1, atype2, aval2, p_succp, p_errh);
        }
        return (comp);
}

static int trivial(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        int comp;

        ss_dassert(!SU_BFLAG_TEST(aval1->ra_flags, RA_ONLYCONVERTED));
        ss_dassert(!SU_BFLAG_TEST(aval2->ra_flags, RA_ONLYCONVERTED));
        comp = va_compare(aval1->RA_RDVA, aval2->RA_RDVA);
        return (comp);
}

static int aval_vacmp_blob(
        va_t* v1,
        va_t* v2,
        bool v1_isblob,
        bool v2_isblob,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        va_index_t len1;
        va_index_t len2;
        ss_byte_t* p1;
        ss_byte_t* p2;
        size_t cmplen;

        p1 = va_getdata(v1, &len1);
        p2 = va_getdata(v2, &len2);
        if (v1_isblob) {
            ss_rc_dassert(len1 >= RS_VABLOBREF_SIZE, len1);
            len1 -= RS_VABLOBREF_SIZE;
            if (v2_isblob) {
                ss_rc_dassert(len2 >= RS_VABLOBREF_SIZE, len2);
                len2 -= RS_VABLOBREF_SIZE;
            } else {
                ss_rc_dassert(len2 > 0, len2);
                len2--;
            }
            cmplen = SS_MIN(len1, len2);
            comp = SsMemcmp(p1, p2, cmplen);
            if (comp != 0) {
                ss_dassert(*p_succp);
                return (comp);
            }
            if (!v2_isblob && cmplen == len2) {
                /* so v1 > v2, as v1 is longer! */
                return (1);
            }
            goto error_return;
        }
        ss_dassert(v2_isblob);
        ss_rc_dassert(len2 >= RS_VABLOBREF_SIZE, len2);
        len2 -= RS_VABLOBREF_SIZE;
        ss_rc_dassert(len1 > 0, len1);
        len1--;
        cmplen = SS_MIN(len1, len2);
        comp = SsMemcmp(p1, p2, cmplen);
        if (comp != 0) {
            ss_dassert(*p_succp);
            return (comp);
        }
        if (cmplen == len1) {
            /* so v1 < v2, as v2 is longer! */
            return (-1);
        }
        /* fall to error_return */ 
error_return:;
        *p_succp = FALSE;
        rs_error_create(p_errh, E_CMPFAILEDDUETOBLOB);
        return ((int)0xBabeFaceL); /* return something to keep compiler happy! */ 
}
                      
static int chr_v_chr(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        va_t* v1 = aval1->RA_RDVA;
        va_t* v2 = aval2->RA_RDVA;
        bool v1_isblob;
        bool v2_isblob;
        va_index_t len1;
        va_index_t len2;
        ss_byte_t* p1;
        ss_byte_t* p2;
        size_t cmplen;
#ifdef SS_COLLATION
        su_collation_t* collation = NULL;
#endif /* SS_COLLATION */
        
        v1_isblob = va_testblob(v1);
        v2_isblob = va_testblob(v2);
        
        if (v1_isblob || v2_isblob) {
            comp = aval_vacmp_blob(v1, v2, v1_isblob, v2_isblob, p_succp, p_errh);
            return (comp);
        }            
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        if (sqldt1 == RSSQLDT_CHAR || sqldt2 == RSSQLDT_CHAR) {
            /* if either type is SQL CHAR
             * 'trim' trailing spaces!
             */

            p1 = va_getdata(v1, &len1);
            p2 = va_getdata(v2, &len2);
            for (--len1; len1 && (p1-1)[len1] == ' '; ) {
                len1--;
            }
            for (--len2; len2 && (p2-1)[len2] == ' '; ) {
                len2--;
            }
#ifdef SS_COLLATION
            ss_dassert(atype1->at_collation == atype2->at_collation ||
                       atype1->at_collation == NULL || atype2->at_collation == NULL);
            if (atype1->at_collation != NULL) {
                collation = atype1->at_collation;
                goto compare_with_collation;
            }
            if (atype2->at_collation != NULL) {
                collation = atype2->at_collation;
                goto compare_with_collation;
            }
#endif /* SS_COLLATION */
    compare_originals:;
            cmplen = (size_t)SS_MIN(len1, len2);
            if (cmplen != 0) {
                comp = SsMemcmp(p1, p2, cmplen);
            } else {
                comp = 0;
            }
            if (comp == 0) {
                comp = (int)(len1 - len2);
            }
        } else {
#ifdef SS_COLLATION
            ss_dassert(atype1->at_collation == atype2->at_collation ||
                       atype1->at_collation == NULL || atype2->at_collation == NULL);
            if (atype1->at_collation != NULL) {
                collation = atype1->at_collation;
                goto prepare_for_compare_with_collation;
            }
            if (atype2->at_collation != NULL) {
                collation = atype2->at_collation;
                goto compare_with_collation;
            }
            goto binary_compare;
    prepare_for_compare_with_collation:;
            p1 = va_getdata(v1, &len1);
            ss_dassert(len1 != 0);
            len1--;
            p2 = va_getdata(v2, &len2);
            ss_dassert(len2 != 0);
            len2--;
            goto compare_with_collation;
    binary_compare:;
#endif /* SS_COLLATION */
            comp = va_compare(v1, v2);
        }
ret_comp:;
        ss_dassert(*p_succp == TRUE);
        return (comp);
#ifdef SS_COLLATION
compare_with_collation:;
        ss_dassert(collation != NULL);
        ss_dprintf_1(("rs_aval_cmp3_notnull: compare_with_collation\n"));
        comp = su_collation_compare(collation, p1, len1, p2, len2);
#if 0 /* This would implement a tie-breaker! */
        if (comp == 0) {
            goto compare_originals;
        }
#endif /* 0 */        
        goto ret_comp;
#endif /* SS_COLLATION */
}

static int chr_v_dte(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp = 0;
        bool succp = FALSE;

        ss_char1_t* s;
        dt_date_t tmp_date;
        dt_datesqltype_t datesqltype;
        rs_sqldatatype_t sqldt = 0;
        ss_char1_t* dtformat;

        s = rs_aval_getasciiz(cd, atype1, aval1);
        dtformat = rs_sysi_timestampformat(cd);
        if (dtformat != NULL) {
            succp = dt_date_setasciiz_ext(&tmp_date, dtformat, s, &datesqltype);
        }
        if (!succp) {
            dtformat = rs_sysi_dateformat(cd);
            if (dtformat != NULL) {
                succp = dt_date_setasciiz_ext(&tmp_date, dtformat, s, &datesqltype);
            }
        }
        if (!succp) {
            dtformat = rs_sysi_timeformat(cd);
            if (dtformat != NULL) {
                succp = dt_date_setasciiz_ext(&tmp_date, dtformat, s, &datesqltype);
            }
        }
        if (!succp) {
            succp = dt_date_setasciiz_ext(&tmp_date, NULL, s, &datesqltype);
        }
        if (succp) {
            rs_avalret_t retc;
            rs_atype_t* tmp_atype;
            rs_aval_t* tmp_aval;

            switch (datesqltype) {
                case DT_DATE_SQLDATE:
                    sqldt = RSSQLDT_DATE;
                    break;
                case DT_DATE_SQLTIME:
                    sqldt = RSSQLDT_TIME;
                    break;
                case DT_DATE_SQLTIMESTAMP:
                case DT_DATE_SQLTYPE_UNKNOWN:
                    sqldt = RSSQLDT_TIMESTAMP;
                    break;
                default:
                    ss_error;
            }
            tmp_atype = rs_atype_initbysqldt(cd, sqldt, -1, -1);
            tmp_aval = rs_aval_create(cd, tmp_atype);
            retc = rs_aval_setdate_ext(
                    cd,
                    tmp_atype, tmp_aval,
                    &tmp_date, datesqltype,
                    NULL);
            if (retc != RSAVR_FAILURE) {
                comp = dte_v_dte(cd, tmp_atype, tmp_aval, atype2, aval2, p_succp, p_errh);
            } else {
                succp = FALSE;
            }
            rs_aval_free(cd, tmp_atype, tmp_aval);
            rs_atype_free(cd, tmp_atype);
        }
        if (!succp) {
            rs_error_create(
                p_errh,
                E_CMPTYPECLASH_SS,
                rs_atype_name(cd, atype1),
                rs_atype_name(cd, atype2));
            *p_succp = FALSE;
            comp = 0;
        }
        return (comp);
}

static int ill_cmp(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1 __attribute__ ((unused)),
        rs_atype_t* atype2,
        rs_aval_t*  aval2 __attribute__ ((unused)),
        bool* p_succp,
        rs_err_t** p_errh)
{
        rs_error_create(
            p_errh,
            E_CMPTYPECLASH_SS,
            rs_atype_name(cd, atype1),
            rs_atype_name(cd, atype2));
        *p_succp = FALSE;
        return (0);
}

static int aval_cmp_char1v2_vabuf(ss_byte_t* buf1, ss_char2_t* buf2, size_t n)
{
        int diff;
        for (;n != 0 ;n--, buf1++, buf2++) {
            diff = *buf1 - SS_CHAR2_LOAD_MSB1ST(buf2);
            if (diff != 0) {
                return (diff);
            }
        }
        return (0);
}

static int aval_vacmp_blob_chr_v_uni(
        va_t* v1,
        va_t* v2,
        bool v1_isblob,
        bool v2_isblob,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        va_index_t len1;
        va_index_t len2;
        ss_byte_t* p1;
        ss_char2_t* p2;
        size_t cmplen;

        p1 = va_getdata(v1, &len1);
        p2 = va_getdata(v2, &len2);
        if (v1_isblob) {
            ss_rc_dassert(len1 >= RS_VABLOBREF_SIZE, len1);
            len1 -= RS_VABLOBREF_SIZE;
            if (v2_isblob) {
                ss_rc_dassert(len2 >= RS_VABLOBREF_SIZE, len2);
                len2 -= RS_VABLOBREF_SIZE;
            } else {
                ss_rc_dassert(len2 & 1, len2);
            }
            len2 /= sizeof(ss_char2_t);
            cmplen = SS_MIN(len1, len2);
            comp =  aval_cmp_char1v2_vabuf(p1, p2, cmplen);
            if (comp != 0) {
                ss_dassert(*p_succp);
                return (comp);
            }
            if (!v2_isblob && cmplen == len2) {
                /* so v1 > v2, as v1 is longer! */
                return (1);
            }
            goto error_return;
        }
        ss_dassert(v2_isblob);
        ss_rc_dassert(len2 >= RS_VABLOBREF_SIZE, len2);
        len2 -= RS_VABLOBREF_SIZE;
        ss_rc_dassert(len1 > 0, len1);
        len1--;
        cmplen = SS_MIN(len1, len2);
        comp = aval_cmp_char1v2_vabuf(p1, p2, cmplen);
        if (comp != 0) {
            ss_dassert(*p_succp);
            return (comp);
        }
        if (cmplen == len1) {
            /* so v1 < v2, as v2 is longer! */
            return (-1);
        }
        /* fall to error_return */ 
error_return:;
        *p_succp = FALSE;
        rs_error_create(p_errh, E_CMPFAILEDDUETOBLOB);
        return ((int)0xBabeFaceL); /* return something to keep compiler happy! */ 
}

static int chr_v_uni(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        va_t* v1 = aval1->RA_RDVA;
        va_t* v2 = aval2->RA_RDVA;
        bool v1_isblob;
        bool v2_isblob;

        v1_isblob = va_testblob(v1);
        v2_isblob = va_testblob(v2);
        if (v1_isblob || v2_isblob) {
            comp = aval_vacmp_blob_chr_v_uni(v1, v2, v1_isblob, v2_isblob, p_succp, p_errh);
            return (comp);
        }
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        if (sqldt1 == RSSQLDT_CHAR || sqldt2 == RSSQLDT_WCHAR) {
            /* if either type is SQL CHAR
             * 'trim' trailing spaces!
             */
            va_index_t len1;
            va_index_t len2;
            size_t cmplen;
            ss_byte_t* p1;
            ss_char2_t* p2;

            p1 = va_getdata(v1, &len1);
            p2 = va_getdata(v2, &len2);
            for (--len1; len1 && (p1-1)[len1] == ' '; ) {
                len1--;
            }
            for (len2 /= sizeof(ss_char2_t); len2; len2--) {
                ss_char2_t* p = p2 - 1 + len2;
                if (SS_CHAR2_LOAD(p) != ' ') {
                    break;
                }
            }
            cmplen = (size_t)SS_MIN(len1, len2);
            while (cmplen != 0) {
                cmplen--;
                if (sizeof(ss_char2_t) < sizeof(int)) {
                    comp = (int)*p1 - (int)SS_CHAR2_LOAD(p2);
                    if (comp != 0) {
                        return (comp);
                    }
                } else {
                    uint c1 = (uint)*p1;
                    uint c2 = (uint)SS_CHAR2_LOAD(p2);
                    if (c1 != c2) {
                        if (c1 < c2) {
                            return (-1);
                        } else {
                            return (1);
                        }
                    }
                }
                p1++;
                p2++;
            }
            comp = (int)(len1 - len2);
        } else {
            comp = va_compare_char1v2(aval1->RA_RDVA, aval2->RA_RDVA);
        }
        return (comp);
}
static int int_v_int(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_int4_t i1;
        ss_int4_t i2;


        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.l = va_getlong(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        i1 = (ss_int4_t)aval1->ra_.l;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.l = va_getlong(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        i2 = (ss_int4_t)aval2->ra_.l;
        if (i1 < i2) {
            return (-1);
        }
        if (i1 > i2) {
            return (1);
        }
        return (0);
}
        
static int int_v_flt(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        float f1;
        float f2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.l = va_getlong(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        f1 = (float)aval1->ra_.l;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.f = va_getfloat(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        ss_dassert(*p_succp == TRUE);
        f2 = aval2->ra_.f;
        if (f1 < f2) {
            return (-1);
        }
        if (f1 > f2) {
            return (1);
        }
        ss_dassert(f1 == f2);
        return (0);
}

static int int_v_dbl(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d1;
        double d2;


        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.l = va_getlong(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        d1 = (double)aval1->ra_.l;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.d = va_getdouble(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        d2 = aval2->ra_.d;
        ss_dassert(*p_succp == TRUE);
        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int int_v_dfl(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        bool succp = TRUE;
        int comp;
        long l;
        dt_dfl_t dfl1;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.l = va_getlong(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        l = aval1->ra_.l;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            dt_dfl_setva(&aval2->ra_.dfl, aval2->ra_va);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        succp = dt_dfl_setlong(&dfl1, l);
        if (succp) {
            comp = dt_dfl_compare(dfl1, aval2->ra_.dfl);
            return (comp);
        }
        rs_error_create(
            p_errh,
            E_CMPTYPECLASH_SS,
            rs_atype_name(cd, atype1),
            rs_atype_name(cd, atype2));
        *p_succp = FALSE;
        return (0);
}


static int flt_v_dbl(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d1;
        double d2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.f = va_getfloat(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        d1 = (double)aval1->ra_.f;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.d = va_getdouble(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        d2 = aval2->ra_.d;
        ss_dassert(*p_succp == TRUE);
        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int flt_v_flt(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        float f1;
        float f2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.f = va_getfloat(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        f1 = aval1->ra_.f;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.f = va_getfloat(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        f2 = aval2->ra_.f;
        if (f1 < f2) {
            return (-1);
        }
        if (f1 > f2) {
            return (1);
        }
        ss_dassert(f1 == f2);
        return (0);
}

static int flt_v_dfl(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        double d1;
        double d2;
        bool succp;

        d1 = (double)rs_aval_getfloat(cd, atype1, aval1);
        succp = dt_dfl_dfltodouble(
                    rs_aval_getdfloat(cd, atype2, aval2),
                    &d2);
        ss_dassert(*p_succp == TRUE);
        if (!succp) {
            rs_error_create(
                p_errh,
                E_CMPTYPECLASH_SS,
                rs_atype_name(cd, atype1),
                rs_atype_name(cd, atype2));
            *p_succp = FALSE;
            return (0);
        }
        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int dbl_v_dbl(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d1;
        double d2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.d = va_getdouble(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        d1 = aval1->ra_.d;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.d = va_getdouble(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        d2 = aval2->ra_.d;
        ss_dassert(*p_succp == TRUE);
        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int dbl_v_dfl(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        double d1;
        double d2;
        bool succp;

        d1 = rs_aval_getdouble(cd, atype1, aval1);
        succp = dt_dfl_dfltodouble(
                    rs_aval_getdfloat(cd, atype2, aval2),
                    &d2);
        ss_dassert(*p_succp == TRUE);
        if (!succp) {
            rs_error_create(
                p_errh,
                E_CMPTYPECLASH_SS,
                rs_atype_name(cd, atype1),
                rs_atype_name(cd, atype2));
            *p_succp = FALSE;
            return (0);
        }
        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int dte_v_dte(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh __attribute__ ((unused)))
{
        bool succp = TRUE;
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;

        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);

        if (sqldt1 == RSSQLDT_TIME && sqldt2 == RSSQLDT_DATE) {
            succp = FALSE;
        } else if (sqldt1 == RSSQLDT_DATE && sqldt2 == RSSQLDT_TIME) {
            succp = FALSE;
        }
        if (succp) {
            int comp;

            comp = va_compare(aval1->RA_RDVA, aval2->RA_RDVA);
            return (comp);
        }
        *p_succp = FALSE;
        return (0);
}


static int dte_v_uni(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp = 0;
        rs_avalret_t retc;
        rs_atype_t* tmp_atype;
        rs_aval_t* tmp_aval;

        tmp_atype = rs_atype_initbysqldt(cd, RSSQLDT_VARCHAR, -1, -1);
        tmp_aval = rs_aval_create(cd, tmp_atype);
        retc = rs_aval_assign_ext(
                cd,
                tmp_atype, tmp_aval,
                atype2, aval2,
                NULL);
        if (retc != RSAVR_FAILURE) {
            comp = -(chr_v_dte(cd, tmp_atype, tmp_aval, atype1, aval1, p_succp, p_errh));
        }
        rs_aval_free(cd, tmp_atype, tmp_aval);
        rs_atype_free(cd, tmp_atype);
        if (retc != RSAVR_FAILURE) {
            return (comp);
        }
        rs_error_create(
            p_errh,
            E_CMPTYPECLASH_SS,
            rs_atype_name(cd, atype1),
            rs_atype_name(cd, atype2));
        *p_succp = FALSE;
        return (0);
}

static int bin_v_bin(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        va_t* v1 = aval1->RA_RDVA;
        va_t* v2 = aval2->RA_RDVA;
        bool v1_isblob;
        bool v2_isblob;

        v1_isblob = va_testblob(v1);
        v2_isblob = va_testblob(v2);
        
        if (v1_isblob | v2_isblob) {
            comp = aval_vacmp_blob(v1, v2, v1_isblob, v2_isblob, p_succp, p_errh);
            return (comp);
        }
        comp = va_compare(v1, v2);
        return (comp);
}

static int uni_v_uni(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        int comp;
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;
        va_t* v1 = aval1->RA_RDVA;
        va_t* v2 = aval2->RA_RDVA;
        bool v1_isblob;
        bool v2_isblob;

        v1_isblob = va_testblob(v1);
        v2_isblob = va_testblob(v2);
        if (v1_isblob || v2_isblob) {
            comp = aval_vacmp_blob(v1, v2, v1_isblob, v2_isblob, p_succp, p_errh);
            return (comp);
        }
        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        if (sqldt1 == RSSQLDT_WCHAR || sqldt2 == RSSQLDT_WCHAR) {
            /* if either type is SQL UNICODE_CHAR
             * 'trim' trailing spaces!
             */
            va_index_t len1;
            va_index_t len2;
            size_t cmplen;
            ss_char2_t* p1;
            ss_char2_t* p2;

            p1 = va_getdata(v1, &len1);
            p2 = va_getdata(v2, &len2);
            ss_dassert(len1 & 1);
            ss_dassert(len2 & 1);
            for (len1 /= sizeof(ss_char2_t); len1; len1--) {
                ss_char2_t* p = p1 - 1 + len1;
                if (SS_CHAR2_LOAD(p) != ' ') {
                    break;
                }
            }
            for (len2 /= sizeof(ss_char2_t); len2; len2--) {
                ss_char2_t* p = p2 - 1 + len2;
                if (SS_CHAR2_LOAD(p) != ' ') {
                    break;
                }
            }
            cmplen = (size_t)SS_MIN(len1, len2);
            cmplen *= sizeof(ss_char2_t);
            comp = SsMemcmp(p1, p2, cmplen);
            if (comp == 0) {
                comp = (int)(len1 - len2);
            }
        } else {
            comp = va_compare(v1, v2);
        }
        ss_dassert(*p_succp == TRUE);
        return (comp);
}

static int int8_v_int8(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_int8_t i81;
        ss_int8_t i82;
        int cmp;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.i8 = va_getint8(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        i81 = aval1->ra_.i8;
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.i8 = va_getint8(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        i82 = aval2->ra_.i8;
        cmp =  SsInt8Cmp(i81, i82);
        return (cmp);
}

static int int_v_int8(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        ss_int8_t i81;
        ss_int8_t i82;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.l = va_getlong(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        SsInt8SetInt4(&i81, aval1->ra_.l);
        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.i8 = va_getint8(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        i82 = aval2->ra_.i8;
        return (SsInt8Cmp(i81, i82));
}

static int flt_v_int8(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d;
        float f1, f2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.f = va_getfloat(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        f1 = aval1->ra_.f;

        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.i8 = va_getint8(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        SsInt8ConvertToDouble(&d, aval2->ra_.i8);
        f2 = (float)d;

        if (f1 < f2) {
            return (-1);
        }
        if (f1 > f2) {
            return (1);
        }
        ss_dassert(f1 == f2);
        return (0);
}

static int dbl_v_int8(
        void*       cd __attribute__ ((unused)),
        rs_atype_t* atype1 __attribute__ ((unused)),
        rs_aval_t*  aval1,
        rs_atype_t* atype2 __attribute__ ((unused)),
        rs_aval_t*  aval2,
        bool* p_succp __attribute__ ((unused)),     /* parameter not used */
        rs_err_t** p_errh __attribute__ ((unused)))
{
        double d1, d2;

        if (!SU_BFLAG_TEST(aval1->ra_flags, RA_CONVERTED)) {
            aval1->ra_.d = va_getdouble(aval1->RA_RDVA);
            SU_BFLAG_SET(aval1->ra_flags, RA_CONVERTED);
        }
        d1 = aval1->ra_.d;

        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.i8 = va_getint8(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        SsInt8ConvertToDouble(&d2, aval2->ra_.i8);

        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

static int dfl_v_int8(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        bool* p_succp,
        rs_err_t** p_errh)
{
        double d1, d2;
        bool succp;

        succp = dt_dfl_dfltodouble(
                    rs_aval_getdfloat(cd, atype1, aval1),
                    &d1);
        if (!succp) {
            rs_error_create(
                p_errh,
                E_CMPTYPECLASH_SS,
                rs_atype_name(cd, atype1),
                rs_atype_name(cd, atype2));
            *p_succp = FALSE;
            return (0);
        }

        if (!SU_BFLAG_TEST(aval2->ra_flags, RA_CONVERTED)) {
            aval2->ra_.i8 = va_getint8(aval2->RA_RDVA);
            SU_BFLAG_SET(aval2->ra_flags, RA_CONVERTED);
        }
        SsInt8ConvertToDouble(&d2, aval2->ra_.i8);

        if (d1 < d2) {
            return (-1);
        }
        if (d1 > d2) {
            return (1);
        }
        ss_dassert(d1 == d2);
        return (0);
}

/*##**********************************************************************\
 * 
 *              rs_atype_issame
 * 
 * Checks if two field type objects are the same i.e. behave equally wrt.
 * rs_atype_cmp and rs_atype_comppos and can be used interchanged in any
 * of the DM level function calls.
 * 
 * Parameters : 
 * 
 *      cd - use
 *              client data
 *              
 *      atype1 - in, use
 *              first type
 *              
 *      atype2 - in, use
 *              second type
 *              
 * Return value :
 *      TRUE types are same
 *      FALSE types as not same
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool rs_atype_issame(
        void* cd __attribute__ ((unused)),
        rs_atype_t* atype1,
        rs_atype_t* atype2)
{
        rs_sqldatatype_t sqldt1;
        rs_sqldatatype_t sqldt2;

        sqldt1 = RS_ATYPE_SQLDATATYPE(cd, atype1);
        sqldt2 = RS_ATYPE_SQLDATATYPE(cd, atype2);
        return (sqldt1 == sqldt2
            &&  atype1->at_len == atype2->at_len
            &&  atype1->at_scale == atype2->at_scale
            &&  (atype1->at_flags & (AT_NULLALLOWED | AT_PSEUDO)) ==
                (atype2->at_flags & (AT_NULLALLOWED | AT_PSEUDO)));
}

/*##**********************************************************************\
 * 
 *		rs_aval_cmp
 * 
 * Compares two attribute values. Attribute values are assumed to be
 * non-NULL and comparable.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	atype1 - in, use
 *		first argument type
 *
 *	aval1 - in, use
 *		first argument value 
 *
 *	atype2 - in, use
 *		second argument type 
 *
 *	aval2 - in, use
 *		second argument value 
 *
 *	relop - in, use
 *		one of SQL_RELOP_EQUAL, SQL_RELOP_NOTEQUAL,
 *          SQL_RELOP_LT, SQL_RELOP_GT, SQL_RELOP_LE or
 *          SQL_RELOP_GE
 *
 * Return value : 
 *
 *      2 if the attribute comparison fails
 *      TRUE (== 1) if the attribute values meet the condition
 *      FALSE (== 0) otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int rs_aval_cmp(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop
) {
        int comp;
        bool succp;

        comp = rs_aval_cmp3(cd, atype1, aval1, atype2, aval2, &succp, NULL);
        if (!succp) {
            return (2); /* FAILURE */
        }
        switch (relop) {
            case RS_RELOP_EQUAL:
                return (comp == 0);
            case RS_RELOP_NOTEQUAL:
                return (comp != 0);
            case RS_RELOP_LT:
                return (comp < 0);
            case RS_RELOP_GT:
                return (comp > 0);
            case RS_RELOP_LE:
                return (comp <= 0);
            case RS_RELOP_GE:
                return (comp >= 0);
            default:
                ss_derror;
                return (2);
        }
}

int rs_aval_cmp_nullallowed(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop
) {
        int comp;
        bool succp;

        comp = rs_aval_cmp3_nullallowed(cd, atype1, aval1,
                                        atype2, aval2, &succp, NULL);
        if (!succp) {
            return (2); /* FAILURE */
        }
        switch (relop) {
            case RS_RELOP_EQUAL:
                return (comp == 0);
            case RS_RELOP_NOTEQUAL:
                return (comp != 0);
            case RS_RELOP_LT:
                return (comp < 0);
            case RS_RELOP_GT:
                return (comp > 0);
            case RS_RELOP_LE:
                return (comp <= 0);
            case RS_RELOP_GE:
                return (comp >= 0);
            default:
                ss_derror;
                return (2);
        }
}

/*##**********************************************************************\
 * 
 *		rs_aval_cmpwitherrh
 * 
 * Compares two attribute values. Attribute values are assumed to be
 * non-NULL and comparable.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	atype1 - in, use
 *		first argument type
 *
 *	aval1 - in, use
 *		first argument value 
 *
 *	atype2 - in, use
 *		second argument type 
 *
 *	aval2 - in, use
 *		second argument value 
 *
 *	relop - in, use
 *		one of SQL_RELOP_EQUAL, SQL_RELOP_NOTEQUAL,
 *          SQL_RELOP_LT, SQL_RELOP_GT, SQL_RELOP_LE or
 *          SQL_RELOP_GE
 *
 *      p_succp - out, use
 *          pointer to success indicator
 *          (TRUE = success, FALSE = error)
 *
 *      p_errh - out, give
 *          on error an error information object is returned
 *          through this pointer
 *		
 * Return value : 
 * 
 *      TRUE if the attribute values meet the condition
 *      FALSE otherwise
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_aval_cmpwitherrh(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        uint        relop,
        bool*       p_succp,
        rs_err_t**  p_errh)
{
        int comp;

        comp = rs_aval_cmp3(cd, atype1, aval1, atype2, aval2, p_succp, p_errh);
        switch (relop) {
            case RS_RELOP_EQUAL:
                return (comp == 0);
            case RS_RELOP_NOTEQUAL:
                return (comp != 0);
            case RS_RELOP_LT:
                return (comp < 0);
            case RS_RELOP_GT:
                return (comp > 0);
            case RS_RELOP_LE:
                return (comp <= 0);
            case RS_RELOP_GE:
                return (comp >= 0);
            default:
                ss_error;
                return (FALSE);
        }
}

/*##**********************************************************************\
 * 
 *		rs_aval_sql_cmpwitherrh
 * 
 * Member of the SQL function block.
 * Compares two attribute values. Attribute values are assumed to be
 * non-NULL and comparable.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	atype1 - in, use
 *		first argument type
 *
 *	aval1 - in, use
 *		first argument value 
 *
 *	atype2 - in, use
 *		second argument type 
 *
 *	aval2 - in, use
 *		second argument value 
 *
 *      p_errh - out, give
 *          on error an error information object is returned
 *          through this pointer
 *		
 * Return value : 
 * 
 *      -2 in case of error
 *      -1 if finst1 is lesser than field2
 *      0 if finst1 is equal to field2
 *      1 if finst1 is larger than finst2
 *      2 if finst1 is NULL and finst2 is NULL
 *      3 if finst1 is not NULL and finst2 is NULL
 *      4 if finst1 is NULL and finst2 is not NULL
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
int rs_aval_sql_cmpwitherrh(
        void*       cd,
        rs_atype_t* atype1,
        rs_aval_t*  aval1,
        rs_atype_t* atype2,
        rs_aval_t*  aval2,
        rs_err_t**  p_errh)
{
        int comp;
        bool succp;
        bool null1, null2;

        CHECK_AVAL(aval1);
        CHECK_AVAL(aval2);
        ss_dassert(rs_atype_comppos(cd, atype1, atype2));

        null1 = RS_AVAL_ISNULL(cd, atype1, aval1);
        null2 = RS_AVAL_ISNULL(cd, atype2, aval2);
        if (null1 || null2) {
            if (null1 && null2) {
                return(2);
            } else if (null2) {
                return(3);
            } else {
                ss_dassert(null1);
                return(4);
            }
        }

        comp = rs_aval_cmp3(cd, atype1, aval1, atype2, aval2, &succp, p_errh);
        if (!succp) {
            return (-2); /* failure */
        }
        if (comp < 0) {
            return(-1);
        } else if (comp > 0) {
            return(1);
        } else {
            return(0);
        }
}


