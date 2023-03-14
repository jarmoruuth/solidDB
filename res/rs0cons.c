/*************************************************************************\
**  source       * rs0cons.c
**  directory    * res
**  description  * Constraint implementation
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
A general constraint object is implemented in this file. The implementation
follows the guidelines specified in SQL-function tb_relcur_constraint
(tabcurconstr).

Limitations:
-----------
Currently supported data types are

        RSDT_CHAR
        RSDT_INTEGER
        RSDT_CHAR

If new types are added you have to revise also the conversion routines
presented here.

Error handling:
--------------
dasserts

Objects used:
------------
Attribute type  <rs0atype.h>
Attribute value <rs0aval.h>

Preconditions:
-------------
None.

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same constraint object can not be used simultaneously from many threads.

Example:
-------
See test\tcons.c

**************************************************************************
#endif /* DOCUMENTATION */

#define RS_INTERNAL
#define RS0CONS_C

#include <ssmath.h>
#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssfloat.h>
#include <ssint8.h>
#include <sswctype.h>
#include <su0wlike.h>

#include <su0bflag.h>

#include "rs0types.h"
#include "rs0atype.h"
#include "rs0aval.h"
#include "rs0cons.h"
#include "rs0sdefs.h"
#include "rs0sysi.h"
#include "rs0error.h"

static void rs_cons_setalwaystrue(
        void*       cd,
        rs_cons_t*  cons);

static void rs_cons_setalwaysfalse(
        void*       cd,
        rs_cons_t*  cons);

static su_ret_t cons_convert(
        void*       cd,
        rs_cons_t*  cons,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* col_atype);


static su_ret_t cons_checklikepat(
        void* cd,
        rs_cons_t* cons,
        uint relop __attribute__ ((unused)),
        rs_ano_t ano __attribute__ ((unused)),
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_atype_t* col_atype __attribute__ ((unused)))
{
        char* s;
        rs_err_t* errh;
        void* p;
        ulong len;

        ss_dassert(relop == RS_RELOP_LIKE);
        if (rs_aval_isnull(cd, atype, aval)) {
            return (SU_SUCCESS);
        }
        switch (rs_atype_datatype(cd, atype)) {
            case RSDT_CHAR:
                p = rs_aval_getdata(cd, atype, aval, &len);
                if (su_slike_legalpattern(p, (size_t)len, cons->cr_escchar)) {
                    return (SU_SUCCESS);
                }
                goto illegal_pattern;
            case RSDT_UNICODE:
                p = rs_aval_getdata(cd, atype, aval, &len);
                ss_dassert(!(len & 1));
                if (su_wlike_legalpattern(p,
                                          (size_t)(len / sizeof(ss_char2_t)),
                                          cons->cr_escchar,
                                          TRUE))
                {
                    return (SU_SUCCESS);
                }
                goto illegal_pattern;
            default:
                goto illegal_type;
        }
illegal_pattern:;
        {
            ss_debug(bool succp =)
            rs_aval_givestr(cd, atype, aval, &s, NULL, NULL);
            ss_dassert(succp);
            cons->cr_rc = E_ILLEGALLIKEPAT_S;
            goto error_return;
        }
illegal_type:;
        s = rs_atype_givefullname(cd, atype);
        cons->cr_rc = E_ILLEGALLIKETYPE_S;
error_return:;
        rs_error_create(&errh, cons->cr_rc, s);
        SsMemFree(s);
        if (cons->cr_errh != NULL) {
            rs_error_free(cd, cons->cr_errh);
        }
        cons->cr_errh = errh;
        rs_cons_setalwaysfalse(cd, cons);
        return (cons->cr_rc);
}

bool rs_cons_value_aliased(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_cons_t* cons)
{
        CONS_CHECK(cons);
        return (!SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_NEEDS_CONVERSION));
}

static void cons_initbuf(
        void*       cd,
        rs_cons_t*  cons,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_sqlcons_t* sqlcons,
        su_bflag_t  flags,
        int         escchar,
        rs_atype_t* column_atype,
        rs_err_t**  p_errh)
{
        su_ret_t rc = SU_SUCCESS;

        SS_NOTUSED(cd);
        ss_dprintf_3(("cons_initbuf:relop=%s (%d)\n", rs_cons_relopname(relop), relop));
        SS_PUSHNAME("cons_initbuf");

        ss_output_4(
        {
            char* buf;
            buf = (aval != NULL)
                    ? rs_aval_print(cd, atype, aval)
                    : SsMemStrdup((char *)"<NULL>");
            ss_dprintf_4(("cons_initbuf:ano=%d, value %s, unknownvalue=%d, flags=%d\n",
                ano, buf,
                sqlcons->sc_aval == NULL ? FALSE : rs_aval_isunknown(cd, sqlcons->sc_atype, sqlcons->sc_aval),
                flags));
            SsMemFree(buf);
        }
        );

        ss_debug(cons->cr_check = RSCHK_CONSTR;)
        cons->cr_relop = relop;
        cons->cr_ano = ano;
        cons->cr_aval = NULL;
        cons->cr_atype = NULL;
        cons->cr_rc = SU_SUCCESS;
        cons->cr_errh = NULL;
        cons->cr_flags = flags;
        cons->cr_escchar = escchar;
        cons->cr_vectorno = -1;
        cons->cr_sqlcons = *sqlcons;
        if (SU_BFLAG_TEST(flags, RS_CONSFLAG_COPYSQLCONS)) {
            if (cons->cr_sqlcons.sc_atype != NULL) {
                cons->cr_sqlcons.sc_atype = rs_atype_copy(
                                                cd,
                                                cons->cr_sqlcons.sc_atype);
            }
            if (cons->cr_sqlcons.sc_aval != NULL) {
                cons->cr_sqlcons.sc_aval = rs_aval_copy(
                                                cd,
                                                cons->cr_sqlcons.sc_atype,
                                                cons->cr_sqlcons.sc_aval);
            }
            if (cons->cr_sqlcons.sc_escatype != NULL) {
                cons->cr_sqlcons.sc_escatype = rs_atype_copy(
                                                    cd,
                                                    cons->cr_sqlcons.sc_escatype);
                cons->cr_sqlcons.sc_escaval = rs_aval_copy(
                                                    cd,
                                                    cons->cr_sqlcons.sc_escatype,
                                                    cons->cr_sqlcons.sc_escaval);
            }
        }
        cons->cr_columnatype = column_atype;
        ss_dassert(column_atype != NULL);
        if (relop == RS_RELOP_ISNULL || relop == RS_RELOP_ISNOTNULL) {
            /* unary operators do not have second operand! */
            cons->cr_atype = column_atype;
            cons->cr_aval = rs_aval_create(cd, column_atype);
        } else {
            /* relop is binary operator */
            ss_dassert(atype != NULL);

            if (relop == RS_RELOP_LIKE
            &&  !rs_aval_isunknown(cd, cons->cr_sqlcons.sc_atype, cons->cr_sqlcons.sc_aval))
            {
                ss_dassert(aval != NULL);
                rc = cons_checklikepat(cd,
                                       cons,
                                       relop,
                                       ano,
                                       atype, aval,
                                       column_atype);
                if (rc != SU_SUCCESS) {
                    goto failed;
                }
            }
            if (RS_ATYPE_DATATYPE(cd, atype) !=
                RS_ATYPE_DATATYPE(cd, column_atype)
            ||  SU_BFLAG_TEST(flags, RS_CONSFLAG_FORCECONVERT))
            {
                SU_BFLAG_SET(cons->cr_flags, RS_CONSFLAG_NEEDS_CONVERSION);
                if (!rs_aval_isunknown(cd, cons->cr_sqlcons.sc_atype, cons->cr_sqlcons.sc_aval)
                &&  aval != NULL)
                {
                    rc = cons_convert(
                            cd,
                            cons,
                            relop,
                            ano,
                            atype,
                            aval,
                            column_atype);
                    if (rc != SU_SUCCESS) {
                        goto failed;
                    }
                } /* else no value available for conversion */
            } else {
                /* no conversion needed, can use parameter aliasing */
                cons->cr_atype = atype;
                cons->cr_aval = aval;
            }
        }
        SS_POPNAME;
        return;
failed:;
        ss_dassert(rc != SU_SUCCESS);
        if (p_errh != NULL) {
            *p_errh = cons->cr_errh;
            cons->cr_errh = NULL;
        }
        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              cons_donebuf
 *
 * Releases cons but not the cons pointer.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cons -
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
static void cons_donebuf(
        void* cd,
        rs_cons_t* cons)
{
        ss_dprintf_3(("cons_donebuf.\n"));
        CONS_CHECK(cons);

        if (SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_COPYSQLCONS)) {
            if (cons->cr_sqlcons.sc_aval != NULL) {
                rs_aval_free(
                    cd,
                    cons->cr_sqlcons.sc_atype,
                    cons->cr_sqlcons.sc_aval);
            }
            if (cons->cr_sqlcons.sc_atype != NULL) {
                rs_atype_free(
                    cd,
                    cons->cr_sqlcons.sc_atype);
            }
            if (cons->cr_sqlcons.sc_escatype != NULL) {
                rs_aval_free(
                    cd,
                    cons->cr_sqlcons.sc_escatype,
                    cons->cr_sqlcons.sc_escaval);
                rs_atype_free(
                    cd,
                    cons->cr_sqlcons.sc_escatype);
            }
        }

        if (cons->cr_aval != NULL &&
            cons->cr_aval != cons->cr_sqlcons.sc_aval)
        {
            rs_aval_free(cd, cons->cr_atype, cons->cr_aval);
        }
        if (cons->cr_atype != NULL &&
            cons->cr_atype != cons->cr_sqlcons.sc_atype &&
            cons->cr_atype != cons->cr_columnatype)
        {
            rs_atype_free(cd, cons->cr_atype);
        }
        if (cons->cr_errh != NULL) {
            rs_error_free(cd, cons->cr_errh);
        }
}

/*#***********************************************************************\
 *
 *              cons_convert
 *
 * Makes appropriate data conversion for constraint when the supplied
 * value is of different numerical type from the attribute. Also assigns
 * the cr_atype and cr_aval fields of the constraint object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in out, use
 *              constraint object
 *
 *      relop - in, use
 *              relation operator
 *
 *      ano - in
 *              attribute #
 *
 *      atype - in, use
 *              attribute type of condition
 *
 *      aval - in, use
 *              attribute value of condition
 *
 *      col_atype - in, use
 *              attribute type in database
 *
 * Return value :
 *
 * Limitations  :
 *
 *      Makes conversions only between different
 *      numerical data types. Non-numerical types
 *      or equal types cause assertion failure.
 *
 * Globals used :
 */
static su_ret_t cons_convert(
        void*       cd,
        rs_cons_t*  cons,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        rs_atype_t* col_atype)
{
        double d;
        rs_datatype_t from_dt;
        rs_datatype_t to_dt;
        RS_AVALRET_T succp;

        ss_dprintf_1(("cons_convert.\n"));

        from_dt = RS_ATYPE_DATATYPE(cd, atype);
        to_dt = RS_ATYPE_DATATYPE(cd, col_atype);

        ss_dprintf_1(("cons_convert, from_dt=%d, to_dt=%d.\n",
                       from_dt, to_dt ));

        if (rs_aval_isnull(cd, atype, aval)) {
            ss_dprintf_3(("cons_convert, NULL\n"));
            cons->cr_atype = rs_atype_copymax(cd, col_atype);
            cons->cr_aval = rs_aval_create(cd, cons->cr_atype);
            rs_aval_setnull(cd, cons->cr_atype, cons->cr_aval);
            return(SU_SUCCESS);
        }

        SS_NOTUSED(ano);

#ifndef SS_NOSQL
        switch (from_dt) {
            case RSDT_FLOAT:
            case RSDT_DFLOAT:
            case RSDT_DOUBLE:
            case RSDT_BIGINT:
                switch (to_dt) {
                    case RSDT_DFLOAT:
                        succp = rs_aval_converttodouble(
                                    cd,
                                    atype,
                                    aval,
                                    &d,
                                    NULL);
                        if (succp == RSAVR_FAILURE) {
                            goto error_return;
                        }
                        if (d > DFL_DBL_MAX) {
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                            }
                            return (cons->cr_rc);
                        }
                        if (d < DFL_DBL_MIN) {
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                                    break;
                            }
                            return (cons->cr_rc);
                        }
                        break;
                    case RSDT_FLOAT:
                        if (from_dt != RSDT_DOUBLE) {
                            /* Use direct assignment */
                            break;
                        }
                        d = rs_aval_getdouble(cd, atype, aval);
                        if (d > SS_MOST_POSITIVE_PORTABLE_FLOAT) {
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                            }
                            return (cons->cr_rc);
                        }
                        if (d < SS_MOST_NEGATIVE_PORTABLE_FLOAT) {
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                                    break;
                            }
                            return (cons->cr_rc);
                        }
                        break;
                    case RSDT_INTEGER:
                        succp = rs_aval_converttodouble(
                                    cd,
                                    atype,
                                    aval,
                                    &d,
                                    NULL);
                        if (succp == RSAVR_FAILURE) {
                            goto error_return;
                        }
                        if (d > (double)RS_INT_MAX) {

                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                            }
                            return (cons->cr_rc);
                        }
                        if (d < (double)RS_INT_MIN) {
                            switch (relop) {
                                case RS_RELOP_EQUAL:
                                case RS_RELOP_LT:
                                case RS_RELOP_LT_VECTOR:
                                case RS_RELOP_LE:
                                case RS_RELOP_LE_VECTOR:
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    break;
                                case RS_RELOP_NOTEQUAL:
                                case RS_RELOP_GT:
                                case RS_RELOP_GT_VECTOR:
                                case RS_RELOP_GE:
                                case RS_RELOP_GE_VECTOR:
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    break;
                                default:
                                    ss_error;
                                    break;
                            }
                            return (cons->cr_rc);
                        }
                        ss_dprintf_1(("%s, %d. Val=%lg.\n",
                                    __FILE__, __LINE__, d));
                        ss_dprintf_4(("%s, %d. val=%lg, ceil=%lg, floor=%lg.\n",
                                    __FILE__, __LINE__, d, ceil(d), floor(d)));
                        switch (relop) {
                            case RS_RELOP_LT:
                            case RS_RELOP_LT_VECTOR:
                                d = ceil(d);
                                if (d == (double)RS_INT_MIN) {
                                    rs_cons_setalwaysfalse(cd, cons);
                                    return (cons->cr_rc);
                                } else if (d == (double)(RS_INT_MIN + 1)) {
                                    cons->cr_relop = RS_RELOP_EQUAL;
                                    d = (double)RS_INT_MIN;
                                }
                                break;
                            case RS_RELOP_GE:
                            case RS_RELOP_GE_VECTOR:
                                d = ceil(d);
                                if (d == (double)RS_INT_MAX) {
                                    cons->cr_relop = RS_RELOP_EQUAL;
                                }
                                break;
                            case RS_RELOP_NOTEQUAL:
                                if (((double)((long)d)) != d) {
                                    /* always TRUE when non-null */
                                    rs_cons_setalwaystrue(cd, cons);
                                    return (cons->cr_rc);
                                }
                                break;
                            case RS_RELOP_EQUAL:
                                if ((double)(long)d != d) {
                                    /* always FALSE */
                                    rs_cons_setalwaysfalse(cd, cons);
                                    return (cons->cr_rc);
                                }
                                break;
                            case RS_RELOP_GT:
                            case RS_RELOP_GT_VECTOR:
                                ss_dprintf_4(("%s, %d. Val=%lg.\n",
                                            __FILE__, __LINE__, d));
                                d = floor(d);
                                if (d == (double)RS_INT_MAX) {
                                    rs_cons_setalwaysfalse(cd, cons);
                                    return (cons->cr_rc);
                                } else if (d == (double)(RS_INT_MAX - 1)) {
                                    cons->cr_relop = RS_RELOP_EQUAL;
                                    d = (double)RS_INT_MAX;
                                }
                                break;
                            case RS_RELOP_LE:
                            case RS_RELOP_LE_VECTOR:
                                d = floor(d);
                                if (d == (double)RS_INT_MIN) {
                                    cons->cr_relop = RS_RELOP_EQUAL;
                                }
                                break;
                            default:
                                ss_error;
                        }
                        ss_dprintf_4(("%s, %d. Val=%lg.\n",
                                    __FILE__, __LINE__, d));
                        cons->cr_atype =
                            rs_atype_copy(cd, col_atype);
                        cons->cr_aval =
                            rs_aval_create(cd, cons->cr_atype);
                        succp = rs_aval_setlong_ext(
                                    cd,
                                    cons->cr_atype,
                                    cons->cr_aval,
                                    (long)d,
                                    NULL);
                        if (!succp) {
                            goto error_return;
                        }
                        ss_dprintf_1(("relop=%d, cr_val=%ld.\n",
                                        cons->cr_relop, (long)d));
                        return (cons->cr_rc);
                    default:
                        break;
                }
                break;
            case RSDT_UNICODE:
                switch (to_dt) {
                    case RSDT_CHAR:
                        {
                            va_t* va;
                            void* p_data;
                            va_index_t datalen;

                            va = rs_aval_va(cd, atype, aval);
                            p_data = va_getdata(va, &datalen);
                            ss_dassert(datalen & 1);
                            datalen /= 2;

                            cons->cr_atype = rs_atype_unitochar(cd, atype);
                            cons->cr_aval = rs_aval_create(cd, cons->cr_atype);
                            succp = rs_aval_assign_ext(
                                        cd,
                                        cons->cr_atype,
                                        cons->cr_aval,
                                        atype,
                                        aval,
                                        NULL);
                            if (succp == RSAVR_FAILURE) {
                                /* UNICODE pattern contains char(s)
                                 * with at least one of bits 8-15 nonzero.
                                 * We cannot convert the
                                 * pattern to CHAR.
                                 */
                                rs_aval_free(cd, cons->cr_atype, cons->cr_aval);
                                cons->cr_aval = NULL;
                                rs_atype_free(cd, cons->cr_atype);
                                cons->cr_atype = NULL;
                                if (relop == RS_RELOP_LIKE
                                ||  relop == RS_RELOP_EQUAL
                                ||  relop == RS_RELOP_NOTEQUAL)
                                {
                                    if (relop == RS_RELOP_NOTEQUAL) {
                                        rs_cons_setalwaystrue(cd, cons);
                                    } else {
                                        rs_cons_setalwaysfalse(cd, cons);
                                    }
                                } else {
                                    ss_char2_t c;
                                    ss_debug(uint i = 0);

                                    ss_dassert(datalen != 0);
                                    do {
                                        c = SS_CHAR2_LOAD(p_data);
                                        ss_dassert(i < datalen);
                                        ss_debug(i++);
                                        p_data = (ss_char2_t*)p_data + 1;
                                    } while (c == (ss_char2_t)0x00FF);
                                    /* Check if first char
                                     * after leading chars that
                                     * are equal to UCHAR_MAX
                                     * is greater than UCHAR_MAX.
                                     * In that case the pattern
                                     * is greater than any 8-bit char
                                     * string
                                     */
                                    if (!ss_isw8bit(c)) {
                                        switch (relop) {
                                            case RS_RELOP_GT:
                                            case RS_RELOP_GT_VECTOR:
                                            case RS_RELOP_GE:
                                            case RS_RELOP_GE_VECTOR:
                                                /* always FALSE */
                                                rs_cons_setalwaysfalse(cd, cons);
                                                break;
                                            case RS_RELOP_LT:
                                            case RS_RELOP_LT_VECTOR:
                                            case RS_RELOP_LE:
                                            case RS_RELOP_LE_VECTOR:
                                                /* always TRUE when non-null */
                                                rs_cons_setalwaystrue(cd, cons);
                                                break;
                                            default:
                                                ss_error;
                                        }
                                        return (cons->cr_rc);
                                    }
                                    cons->cr_atype = rs_atype_copy(cd, atype);
                                    cons->cr_aval = rs_aval_copy(cd, cons->cr_atype, aval);
                                    SU_BFLAG_SET(cons->cr_flags, RS_CONSFLAG_UNI4CHARCOL);
                                }
                            } else {
                                /* The constraint is just CHAR, because
                                 * all characters did fit into the
                                 * CHAR range
                                 */
                                if (datalen == 0) {
                                    switch (relop) {
                                        case RS_RELOP_GE:
                                        case RS_RELOP_GE_VECTOR:
                                            /* always TRUE when non-null */
                                            rs_aval_free(cd, cons->cr_atype, cons->cr_aval);
                                            cons->cr_aval = NULL;
                                            rs_atype_free(cd, cons->cr_atype);
                                            cons->cr_atype = NULL;
                                            rs_cons_setalwaystrue(cd, cons);
                                            break;
                                        case RS_RELOP_LT:
                                        case RS_RELOP_LT_VECTOR:
                                            /* always FALSE */
                                            rs_aval_free(cd, cons->cr_atype, cons->cr_aval);
                                            cons->cr_aval = NULL;
                                            rs_atype_free(cd, cons->cr_atype);
                                            cons->cr_atype = NULL;
                                            rs_cons_setalwaysfalse(cd, cons);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            return (cons->cr_rc);
                        }
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        cons->cr_atype = rs_atype_copymax(cd, col_atype);
        cons->cr_aval = rs_aval_create(cd, cons->cr_atype);

        succp = rs_aval_assign_ext(
                    cd,
                    cons->cr_atype,
                    cons->cr_aval,
                    atype,
                    aval,
                    NULL);
        if (succp == RSAVR_FAILURE) {
            if (to_dt == RSDT_DATE && from_dt == RSDT_CHAR) {
                char* s;

                s = rs_aval_getasciiz(cd, atype, aval);
                cons->cr_rc = E_ILLDATECONST_S;
                if (cons->cr_errh != NULL) {
                    rs_error_free(cd, cons->cr_errh);
                    cons->cr_errh = NULL;
                }
                rs_error_create(&cons->cr_errh, cons->cr_rc, s);
            } else {
                goto error_return;
            }
        }
        return (cons->cr_rc);

error_return:;
        cons->cr_rc = E_ILLCONSTR;
        if (cons->cr_errh != NULL) {
            rs_error_free(cd, cons->cr_errh);
            cons->cr_errh = NULL;
        }
        rs_error_create(&cons->cr_errh, cons->cr_rc);
        return (cons->cr_rc);
#else /* SS_NOSQL */
        return(E_ILLCONSTR);
#endif /* SS_NOSQL */
}



/*##**********************************************************************\
 *
 *              rs_cons_init
 *
 * Initializes a constraint object.
 * Given constraint atype can differ from the original atype in relation.
 * In this case proper conversion is performed so that the atype in returned
 * constraint object will match the original atype.
 *
 * Some consistency checking is done. Constraint is in some cases set
 * to "always false" state.
 *
 * See additional parameter specification from tb_relcur_constr
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      relop - in
 *              relational operator
 *
 *      ano - in
 *              constraint attribute index in ttype of relation
 *
 *      atype - in, use
 *              constraint attribute type
 *          If NULL, a dummy atype is created
 *
 *      aval - in, use
 *              constraint attribute value, may be NULL
 *          If NULL, a dummy (SQL NULL) aval is created
 *
 *      sqlcons - in, hold
 *          Original comns from SQL.
 *
 *      flags - in
 *          Control flags.
 *
 *      escchar - in
 *              escape character, RS_CONS_NOESCCHAR if none
 *
 *      col_atype - in
 *              Original attribute type in relation.
 *
 *      p_errh - out, give
 *          if non-null a possible error object is created and *p_errh
 *          is assigned to point at it
 *
 * Return value - give :
 *
 *     Pointer to constraint object or
 *     NULL when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_cons_t* rs_cons_init(
        void*       cd,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int         escchar,
        rs_atype_t* col_atype,
        rs_err_t**  p_errh)
{
        rs_cons_t* cons;

        SS_PUSHNAME("rs_cons_init");
        SS_NOTUSED(cd);

        ss_dprintf_1(("rs_cons_init.\n"));
        cons = SSMEM_NEW(rs_cons_t);

        cons_initbuf(
                cd,
                cons,
                relop,
                ano,
                atype,
                aval,
                sqlcons,
                flags,
                escchar,
                col_atype,
                p_errh);

        SS_POPNAME;

        return(cons);
}


/*##**********************************************************************\
 *
 *              rs_cons_done
 *
 * Frees a constraint object.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, take
 *              constraint pointer
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_cons_done(
        void* cd,
        rs_cons_t* cons)
{
        ss_dprintf_1(("rs_cons_done.\n"));
        CONS_CHECK(cons);

        cons_donebuf(cd, cons);

        SsMemFree(cons);
}

/*##**********************************************************************\
 *
 *              rs_cons_reset
 *
 * resets a constraint object.
 * Given constraint atype can differ from the original atype in relation.
 * In this case proper conversion is performed so that the atype in returned
 * constraint object will match the original atype.
 *
 * Some consistency checking is done. Constraint is in some cases set
 * to "always false" state.
 *
 * See additional parameter specification from tb_relcur_constr
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in out, use
 *              constraint pointer
 *
 *      relop - in
 *              relational operator
 *
 *      ano - in
 *              constraint attribute index in ttype of relation
 *
 *      atype - in, use
 *              constraint attribute type
 *
 *      aval - in, use
 *              constraint attribute value
 *
 *      sqlcons - in, hold
 *          Original comns from SQL.
 *
 *      escchar - in
 *              escape character, RS_CONS_NOESCCHAR if none
 *
 *      col_atype - in
 *              Original attribute type in relation.
 *
 *      p_errh - out, give
 *          if non-null a possible error object is created and *p_errh
 *          is assigned to point at it
 *
 * Return value - give :
 *
 *     Pointer to constraint object or
 *     NULL when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
void rs_cons_reset(
        void*       cd,
        rs_cons_t*  cons,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int         escchar,
        rs_atype_t* col_atype,
        rs_err_t**  p_errh)
{
        ss_dprintf_1(("rs_cons_reset.\n"));
        CONS_CHECK(cons);
        SS_PUSHNAME("rs_cons_reset");

        cons_donebuf(cd, cons);

        cons_initbuf(
                cd,
                cons,
                relop,
                ano,
                atype,
                aval,
                sqlcons,
                flags,
                escchar,
                col_atype,
                p_errh);

        SS_POPNAME;
}

/*##**********************************************************************\
 *
 *              rs_cons_escchar
 *
 * Returns the escape character of a constraint. If there are no
 * escape character, return RS_CONS_NOESCCHAR.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint pointer
 *
 * Return value :
 *
 *      Escape character, or
 *      RS_CONS_NOESCCHAR
 *
 * Limitations  :
 *
 * Globals used :
 */
int rs_cons_escchar(cd, cons)
    void* cd;
    rs_cons_t* cons;
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(cons->cr_escchar);
}

/*##**********************************************************************\
 *
 *              rs_cons_setescchar
 *
 * Changes the escape character of a constraint.
 *
 * Parameters :
 *
 *      cd - in
 *
 *
 *      cons - use
 *
 *
 *      esc - in
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
void rs_cons_setescchar(
        void* cd,
        rs_cons_t* cons,
        int esc)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        cons->cr_escchar = esc;
}

#if defined(SS_DEBUG)

/*##**********************************************************************\
 *
 *              rs_cons_issolved
 *
 * Returns TRUE if the constraint is solved and FALSE otherwise.
 * By default the constraint is not solved.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint pointer
 *
 * Return value :
 *
 *      TRUE    - constraint is solved
 *      FALSE   - constraint not solved
 *
 * Limitations  :
 *
 * Globals used :
 */
bool rs_cons_issolved(cd, cons)
    void* cd;
    rs_cons_t* cons;
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_SOLVED));
}

#endif /* defined(SS_DEBUG) */

#ifndef SS_NOSQL
/*#***********************************************************************\
 *
 *              rs_cons_setalwaystrue
 *
 * Flags constraint condition as always-true when non-null (ie. changes
 * the search condition to RS_RELOP_ISNOTNULL).
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in out, use
 *              constraint object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void rs_cons_setalwaystrue(cd, cons)
    void*       cd;
        rs_cons_t*  cons;
{
        ss_dassert(cons->cr_atype == NULL);
        ss_dassert(cons->cr_aval == NULL);
        cons->cr_relop = RS_RELOP_ISNOTNULL;
        cons->cr_atype = rs_atype_initlong(cd);  /* dummy atype */
        cons->cr_aval = rs_aval_create(cd, cons->cr_atype); /* dummy aval */
}
#endif /* SS_NOSQL */

/*#***********************************************************************\
 *
 *              rs_cons_setalwaysfalse
 *
 * Flags constraint object as always-false
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in out, use
 *              constraint object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void rs_cons_setalwaysfalse(cd, cons)
    void*       cd;
        rs_cons_t*  cons;
{
        SU_BFLAG_SET(cons->cr_flags, RS_CONSFLAG_ALWAYSFALSE);

        if (cons->cr_atype == NULL) {
            cons->cr_atype = rs_atype_init(  /* dummy atype */
                                cd,
                                RSAT_USER_DEFINED,
                                RSDT_INTEGER,
                                RSSQLDT_INTEGER,
                                RS_INT_PREC,
                                RS_INT_SCALE,
                                TRUE);
        }
        if (cons->cr_aval == NULL) {
            cons->cr_aval = rs_aval_create(cd, cons->cr_atype); /* dummy aval */
        }
}



/*##**********************************************************************\
 *
 *              rs_cons_setestimated
 *
 * Sets estimated flag for a constraint.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cons -
 *
 *
 *      estimatedp -
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
void rs_cons_setestimated(
        void* cd,
        rs_cons_t* cons,
        bool estimatedp)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        if (estimatedp) {
            SU_BFLAG_SET(cons->cr_flags, RS_CONSFLAG_ESTIMATED);
        } else {
            SU_BFLAG_CLEAR(cons->cr_flags, RS_CONSFLAG_ESTIMATED);
        }
}

/*##**********************************************************************\
 *
 *              rs_cons_isestimated
 *
 * Gets the estimated flag value from  a constraint.
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      cons -
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
bool rs_cons_isestimated(
        void* cd,
        rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_ESTIMATED));
}

void rs_cons_setvectorno(
        void* cd __attribute__ ((unused)),
        rs_cons_t* cons,
        int vectorno)
{
        CONS_CHECK(cons);

        cons->cr_vectorno = vectorno;
}

int rs_cons_getvectorno(
        void* cd __attribute__ ((unused)),
        rs_cons_t* cons)
{
        CONS_CHECK(cons);

        return(cons->cr_vectorno);
}


/*##**********************************************************************\
 *
 *              rs_cons_isuniforchar
 *
 * Checks if constraint is unicode and the column is char.
 * This case needs special handling in the database engine.
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      cons - in, use
 *              constraint object
 *
 * Return value :
 *      non-FALSE when constraint is UNICODE for CHAR column
 *      FALSE if constraint is regular.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool rs_cons_isuniforchar(
        void* cd,
        rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return (SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_UNI4CHARCOL));
}

/*##**********************************************************************\
 *
 *              rs_cons_likeprefixinfo
 *
 * Gets some information about like patternt characteristics
 *
 * Parameters :
 *
 *      cd - use
 *              client data
 *
 *      cons - in, use
 *              constraint object
 *
 *      p_numfixedchars - out, use
 *              if non-NULL a number of fixed characters in the pattern
 *           is given to *p_numfixedchars
 *
 *      p_numwildcards - out, use
 *              if non-NULL a number of wildcard characters in the pattern
 *          is given to *p_numwildcards.
 *
 * Return value :
 *      number of fixed characters before the first wildcard character
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
size_t rs_cons_likeprefixinfo(
        void* cd,
        rs_cons_t* cons,
        size_t* p_numfixedchars,
        size_t* p_numwildcards)
{
        rs_atype_t* atype;
        rs_aval_t* aval;
        size_t prefixlen = 0;
        va_t* va;
        void* data;
        va_index_t datalen;
        rs_datatype_t dt;

        atype = cons->cr_atype;
        aval = cons->cr_aval;

        ss_dassert(cons->cr_relop == RS_RELOP_LIKE);
        ss_dassert(aval != NULL);
        ss_dassert(!rs_aval_isnull(cd, atype, aval));

        va = rs_aval_va(cd, atype, aval);
        data = va_getdata(va, &datalen);

        switch (dt = rs_atype_datatype(cd, atype)) {
            case RSDT_CHAR:
                datalen--;
                prefixlen =
                    su_slike_prefixinfo(
                        data,
                        datalen,
                        cons->cr_escchar,
                        p_numfixedchars,
                        p_numwildcards);
                break;
            case RSDT_UNICODE:
                ss_dassert(datalen & 1);
                datalen /= sizeof(ss_char2_t);
                prefixlen =
                    su_wlike_prefixinfo(
                        data,
                        datalen,
                        cons->cr_escchar,
                        p_numfixedchars,
                        p_numwildcards,
                        TRUE);
                break;
            default:
                ss_rc_error(dt);
        }
        return (prefixlen);
}


#if defined(SS_DEBUG)

const char* rs_cons_relopname(
        uint relop)
{
        switch (relop) {
            case RS_RELOP_EQUAL:
                return("RS_RELOP_EQUAL");
            case RS_RELOP_NOTEQUAL:
                return("RS_RELOP_NOTEQUAL");
            case RS_RELOP_LT:
                return("RS_RELOP_LT");
            case RS_RELOP_GT:
                return("RS_RELOP_GT");
            case RS_RELOP_LE:
                return("RS_RELOP_LE");
            case RS_RELOP_GE:
                return("RS_RELOP_GE");
            case RS_RELOP_LIKE:
                return("RS_RELOP_LIKE");
            case RS_RELOP_ISNULL:
                return("RS_RELOP_ISNULL");
            case RS_RELOP_ISNOTNULL:
                return("RS_RELOP_ISNOTNULL");
            case RS_RELOP_LT_VECTOR:
                return("RS_RELOP_LT_VECTOR");
            case RS_RELOP_GT_VECTOR:
                return("RS_RELOP_GT_VECTOR");
            case RS_RELOP_LE_VECTOR:
                return("RS_RELOP_LE_VECTOR");
            case RS_RELOP_GE_VECTOR:
                return("RS_RELOP_GE_VECTOR");
            default:
                ss_error;
                return(NULL);
        }
}

#endif /* defined(SS_DEBUG) */
