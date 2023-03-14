/*************************************************************************\
**  source       * rs1tvsrv.c
**  directory    * res
**  description  * rs_tval_xxx routines that are needed only in the
**               * server side
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
#include <ssdebug.h>
#include <ssmem.h>
#include <sslimits.h>
#include <uti0vcmp.h>
#include <uti0vtpl.h>

#include <su0parr.h>

#include "rs0types.h"
#include "rs0ttype.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0tval.h"
#include "rs0sysi.h"
#include "rs0error.h"

#ifndef SS_NOSQL

/*##**********************************************************************\
 *
 *		rs_tval_project
 *
 * Makes a projection from Table cursor to selected row
 * value. This is for optimization for simple queries,
 * eg. SELECT * FROM T;
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	src_ttype - in, use
 *		source ttype
 *
 *	dst_ttype - in, use
 *		destination tuple type
 *
 *	src_tval - in, use
 *		source tval
 *
 *	dst_tval - out, use
 *		destination tval
 *
 *	attr_cnt - in
 *		# of attributes to project
 *
 *	ano_array - in
 *		array[attr_cnt] of source tval attribute numbers.
 *          RS_ANO_NULL marks the columns that will not be
 *          projected
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void rs_tval_project(
        void* cd,
        rs_ttype_t* src_ttype,
        rs_ttype_t* dst_ttype __attribute__ ((unused)),
        rs_tval_t* src_tval,
        rs_tval_t* dst_tval,
        uint attr_cnt,
        rs_ano_t ano_array[/*attr_cnt*/])
{
        uint i;
        rs_ano_t ano;
        uint count;
        rs_aval_t* src_aval;
        rs_aval_t* dst_aval;
        rs_atype_t* atype;

        CHECK_TVAL(src_tval);
        CHECK_TVAL(dst_tval);
        count = 0;
        for (i = 0; i < attr_cnt; i++) {
            ano = (rs_ano_t)ano_array[i];
            if (ano != RS_ANO_NULL) {
                ss_dassert(RS_TTYPE_QUICKSQLANOTOPHYS(cd, dst_ttype, i) == (int)i);
                ano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, src_ttype, ano);
                dst_aval = &dst_tval->tv_attr_arr[i];
                src_aval = &src_tval->tv_attr_arr[ano];
                atype = RS_TTYPE_ATYPE(cd, src_ttype, ano);
                RS_AVAL_COPYBUF4(
                    cd,
                    atype,
                    dst_aval,
                    src_aval);
                if (SU_BFLAG_TEST(dst_aval->ra_flags, RA_VTPLREF)) {
                    count++;
                }
            }
        }
        if (dst_tval->tv_vtpl != NULL) {

            SsMemLinkDecZeroFree(dst_tval->tv_vtplalloc);
            dst_tval->tv_vtpl = NULL;
            dst_tval->tv_vtplalloc = NULL;
        }
        if (count) {
            ss_dassert(src_tval->tv_vtpl != NULL);
            dst_tval->tv_vtpl = src_tval->tv_vtpl;
            dst_tval->tv_vtplalloc = src_tval->tv_vtplalloc;
            SsMemLinkInc(dst_tval->tv_vtplalloc);
        }
}

static const int tval_cmpresarr[3] = { -1, 1, -1 };

/* == (cmp < 0)  */
#define TVAL_CMPISNEG(cmp) ((uint)(cmp) >> (sizeof(int) * CHAR_BIT - 1))

/* == ((cmp < 0 ? -1 : 1) * (asc? 1 : -1)) */
#define TVAL_CMPRET(cmp, asc) \
        (tval_cmpresarr[TVAL_CMPISNEG(cmp) + (asc)])

/*##**********************************************************************\
 *
 *		rs_tval_cmp
 *
 * Compares two tuple values as in order by or group by
 *
 * Parameters :
 *
 *	cd - use
 *		client data
 *
 *	ttype1 - in, use
 *
 *
 *	ttype2 - in, use
 *
 *
 *	tval1 - in, use
 *
 *
 *	tval2 - in, use
 *
 *
 *	ncols - in
 *		number of columns to be compared
 *
 *	col_array - in, use
 *		array of column numbers to be compared
 *
 *	asc_arr - in, use
 *		array of booleans telling whether comparison is ascending
 *          or descending (TRUE = asc, FALSE = desc)
 *
 * Return value :
 *      -1, 0, 1 as in strcmp()
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int rs_tval_cmp(
        void* cd,
        rs_ttype_t* ttype1,
        rs_ttype_t* ttype2,
        rs_tval_t* tval1,
        rs_tval_t* tval2,
        uint ncols,
        uint col_array[/* ncols */],
        bool asc_array[/* ncols */])
{
        uint i;
        rs_atype_t* atype1;
        rs_atype_t* atype2;
        rs_aval_t* aval1;
        rs_aval_t* aval2;
        rs_ano_t ano1;
        rs_ano_t ano2;
        int cmp;

        if (ncols == 0) {
            return (0);
        }
        cmp = 0;
        i = 0;
        do {
            if (col_array == NULL) {
                ano1 = i;
                ano2 = i;
            } else {
                ano2 = col_array[i];
                ano1 = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype1, ano2);
                ano2 = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype2, ano2);
            }
            atype1 = RS_TTYPE_ATYPE(cd, ttype1, ano1);
            atype2 = RS_TTYPE_ATYPE(cd, ttype2, ano2);
            aval1 = &tval1->tv_attr_arr[ano1];
            aval2 = &tval2->tv_attr_arr[ano2];
            if (SU_BFLAG_TEST(aval1->ra_flags, RA_NULL)) {
                if (!SU_BFLAG_TEST(aval2->ra_flags, RA_NULL)) {
                    cmp = -1;
                }
            } else if (SU_BFLAG_TEST(aval2->ra_flags, RA_NULL)) {
                cmp = 1;
            } else if ((RS_ATYPE_DATATYPE(cd, atype1) ==
                        RS_ATYPE_DATATYPE(cd, atype2))
                       &&
                       !(SU_BFLAG_TEST(aval1->ra_flags, RA_ONLYCONVERTED) |
                         SU_BFLAG_TEST(aval2->ra_flags, RA_ONLYCONVERTED))
                )
            {
                cmp = va_compare(aval1->RA_RDVA, aval2->RA_RDVA);
            } else {
                bool succp;
                cmp = rs_aval_cmp3(cd, atype1, aval1, atype2, aval2, &succp, NULL);
                ss_dassert(succp);
            }
            ss_dassert(asc_array == NULL || asc_array[i] == TRUE || asc_array[i] == FALSE);
            ss_dassert(TVAL_CMPISNEG(cmp) == TRUE || TVAL_CMPISNEG(cmp) == FALSE);
            if (cmp != 0) {
                bool asc;
                asc = asc_array == NULL || asc_array[i];
                return (TVAL_CMPRET(cmp, asc));
            }
            i++;
        } while (i < ncols);
        return (0);
}

bool rs_tval_convert(
        void*       cd,
        rs_ttype_t* dst_ttype,
        rs_tval_t** dst_tval,
        rs_ttype_t* src_ttype,
        rs_tval_t*  src_tval,
        rs_err_t**  p_errh)
{
        rs_tval_t* tval;
        uint nattrs;
        int i;
        bool succp = TRUE;

        nattrs = rs_ttype_nattrs(cd, src_ttype);

        if (nattrs != rs_ttype_nattrs(cd, dst_ttype)) {
            rs_error_create(p_errh, E_WRONGNUMOFPARAMS);
            return(FALSE);
        }

        tval = rs_tval_create(cd, dst_ttype);

        for (i=0;succp && i< (int)nattrs;i++) {
            rs_aval_t* src_aval;
            rs_atype_t* src_atype;
            rs_aval_t* dst_aval;
            rs_atype_t* dst_atype;

            src_atype = rs_ttype_atype(cd, src_ttype, i);
            dst_atype = rs_ttype_atype(cd, dst_ttype, i);
            src_aval = rs_tval_aval(cd, src_ttype, src_tval, i);
            dst_aval = rs_aval_create(cd, src_atype);

            succp = rs_aval_convert(
                        cd,
                        dst_atype,
                        dst_aval,
                        src_atype,
                        src_aval,
                        p_errh);

            rs_tval_insertaval(
                        cd,
                        dst_ttype,
                        tval,
                        i,
                        dst_aval);

        }

        if (succp) {
            *dst_tval = tval;
        } else {
            rs_tval_free(cd, dst_ttype, tval);
        }

        return(succp);
}

uint rs_tval_sql_assignaval(
        void*       cd,
        rs_ttype_t* dst_ttype,
        rs_tval_t*  dst_tval,
        uint        col_n,
        rs_atype_t* src_atype,
        rs_aval_t*  src_aval,
        rs_err_t**  p_errh)
{
        uint ret;
        rs_ano_t dst_ano;
        rs_atype_t* dst_atype;
        rs_aval_t* dst_aval;

        CHECK_TTYPE(dst_ttype);
        CHECK_TVAL(dst_tval);

        dst_ano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, dst_ttype, col_n);
        dst_atype = RS_TTYPE_ATYPE(cd, dst_ttype, dst_ano);
        dst_aval = RS_TVAL_AVAL(cd, dst_ttype, dst_tval, dst_ano);

        ret = rs_aval_assign(
            cd,
            dst_atype,
            dst_aval,
            src_atype,
            src_aval,
            p_errh);

        return(ret);
}

/*##**********************************************************************\
 *
 *      rs_tval_copy_over
 *
 * Same as rs_tval_copy, but this does not create a new tval.
 * Instead, it overwrites contents of existing destination tval.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      ttype - in, use
 *          tuple type object
 *
 *      dst_tval - (in) out, use
 *          destination whose old contents are (gracefully)
 *          overwritten
 *
 *      src_tval - in, use
 *          src tuple value
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void rs_tval_copy_over(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* dst_tval,
        rs_tval_t* src_tval)
{
        rs_ano_t i;
        rs_ano_t nattrs;
        rs_atype_t* atype;
        rs_aval_t*  src_aval;
        rs_aval_t* dst_aval;

        CHECK_TTYPE(ttype);
        CHECK_TVAL(dst_tval);
        CHECK_TVAL(src_tval);
        nattrs = RS_TTYPE_NATTRS(cd, ttype);
        for (i = 0; i < nattrs; i++) {
            atype = RS_TTYPE_ATYPE(cd, ttype, i);
            src_aval = RS_TVAL_AVAL(cd, ttype, src_tval, i);
            dst_aval = RS_TVAL_AVAL(cd, ttype, dst_tval, i);
            RS_AVAL_ASSIGNBUF(cd, atype, dst_aval, src_aval);
        }
        if (dst_tval->tv_vtpl != NULL) {
            ss_dassert(dst_tval->tv_vtplalloc != NULL);
            SsMemLinkDecZeroFree(dst_tval->tv_vtplalloc);
            dst_tval->tv_vtpl = NULL;
            dst_tval->tv_vtplalloc = NULL;
        }
}

/*##**********************************************************************\
 *
 *      rs_tval_sql_set1avalnull
 *
 * Sets 1 attribute to NULL from tuple value.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      ttype - in, use
 *          tuple type
 *
 *      tval - in out, use
 *          tuple value
 *
 *      sqlano - in
 *          attribute number (from SQL point of view)
 *
 * Return value:
 *      TRUE - NULLify ok,
 *      FALSE - column defined NOT NULL (value set to NULL anyway)
 *
 * Limitations:
 *
 * Globals used:
 */
bool rs_tval_sql_set1avalnull(
        void* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        rs_ano_t sqlano)
{
        bool succp;
        rs_ano_t physano;
        rs_atype_t* atype;
        rs_aval_t* aval;

        CHECK_TTYPE(ttype);
        CHECK_TVAL(tval);

        physano = RS_TTYPE_QUICKSQLANOTOPHYS(cd, ttype, sqlano);
        atype = RS_TTYPE_ATYPE(cd, ttype, physano);
        aval = RS_TVAL_AVAL(cd, ttype, tval, physano);

        succp = (SU_BFLAG_TEST(atype->at_flags, AT_NULLALLOWED) != 0);
        RS_AVAL_UNLINKBLOBIF(cd, atype, aval);
        if (!SU_BFLAG_TEST(aval->ra_flags,
                           RA_VTPLREF | RA_NULL |
                           RA_FLATVA | RA_ONLYCONVERTED))
        {
            refdva_done(&(aval->ra_va));
        } else {
            aval->ra_va = refdva_init();
        }
        SU_BFLAG_CLEAR(aval->ra_flags,
                       RA_CONVERTED | RA_VTPLREF | RA_BLOB |
                       RA_FLATVA | RA_ONLYCONVERTED | RA_UNKNOWN);
        SU_BFLAG_SET(aval->ra_flags, RA_NULL);
        return (succp);
}
#endif /* SS_NOSQL */

