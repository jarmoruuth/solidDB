/*************************************************************************\
**  source       * rs0cons.h
**  directory    * res
**  description  * Constraint services
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


#ifndef RS0CONS_H
#define RS0CONS_H

#include <ssc.h>
#include <su0slike.h>
#include <su0error.h>
#include <su0bflag.h>

#include "rs0types.h"
#include "rs0aval.h"

/* relational operators */
#define RS_RELOP_EQUAL     0
#define RS_RELOP_NOTEQUAL  1
#define RS_RELOP_LT        2
#define RS_RELOP_GT        3
#define RS_RELOP_LE        4
#define RS_RELOP_GE        5
#define RS_RELOP_LIKE      6
#define RS_RELOP_ISNULL    7
#define RS_RELOP_ISNOTNULL 8

#define RS_RELOP_LT_VECTOR 102  /* Not used. */
#define RS_RELOP_GT_VECTOR 103
#define RS_RELOP_LE_VECTOR 104  /* Not used. */
#define RS_RELOP_GE_VECTOR 105

#define RS_CONSFLAG_COPYSQLCONS      SU_BFLAG_BIT(1)
#define RS_CONSFLAG_FORCECONVERT     SU_BFLAG_BIT(2)
#define RS_CONSFLAG_UNI4CHARCOL      SU_BFLAG_BIT(3)     /* UNICODE contraint for CHAR column */
#define RS_CONSFLAG_NEEDS_CONVERSION SU_BFLAG_BIT(4)
#define RS_CONSFLAG_SOLVED           SU_BFLAG_BIT(6)
#define RS_CONSFLAG_ALWAYSFALSE      SU_BFLAG_BIT(7)
#define RS_CONSFLAG_ESTIMATED        SU_BFLAG_BIT(8)

#define RS_CONS_NOESCCHAR  SU_SLIKE_NOESCCHAR

typedef struct {
        uint        sc_relop;
        int         sc_attrn;
        rs_atype_t* sc_atype;
        rs_aval_t*  sc_aval;
        rs_atype_t* sc_escatype;
        rs_aval_t*  sc_escaval;
        bool        sc_alias;
        bool        sc_tabcons;
} rs_sqlcons_t;

/* Structure for a constraint.
 */
typedef struct tbconstr_st {

        ss_debug(rs_check_t cr_check;)  /* check field */
        uint        cr_relop;       /* relational operator */
        rs_ano_t    cr_ano;         /* attribute number in ttype */
        rs_atype_t* cr_atype;       /* constraint attribute type */
        rs_aval_t*  cr_aval;        /* constraint attribute value */
        int         cr_escchar;     /* escape character, RS_CONS_NOESCCHAR if
                                       none */
        int         cr_vectorno;    /* vector constraint number */
        su_bflag_t  cr_flags;       /* flags for constraints */
        su_ret_t    cr_rc;          /* Return code */
        rs_err_t*   cr_errh;        /* Possible error object */
        rs_sqlcons_t cr_sqlcons;    /* hold: orig cons from SQL */
        rs_atype_t* cr_columnatype; /* hold: orig esc atype from table */
} rs_cons_t;

rs_cons_t* rs_cons_init(
        void*       cd,
        uint        relop,
        rs_ano_t    ano,
        rs_atype_t* atype,
        rs_aval_t*  aval,
        su_bflag_t    flags,
        rs_sqlcons_t* sqlcons,
        int         escchar,
        rs_atype_t* org_atype,
        rs_err_t**  p_errh
);

void rs_cons_done(
        void*      cd,
        rs_cons_t* cons
);

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
        rs_atype_t* org_atype,
        rs_err_t**  p_errh
);

SS_INLINE rs_sqlcons_t* rs_cons_getsqlcons(
        rs_sysi_t* cd,
        rs_cons_t* cons);

SS_INLINE uint rs_cons_relop(
        void* cd,
        rs_cons_t* cons);

SS_INLINE rs_ano_t rs_cons_ano(
        void* cd,
        rs_cons_t* cons);

SS_INLINE rs_atype_t* rs_cons_atype(
        void* cd,
        rs_cons_t* cons);

SS_INLINE rs_aval_t* rs_cons_aval(
        void* cd,
        rs_cons_t* cons);

int rs_cons_escchar(
        void* cd,
        rs_cons_t* cons);

void rs_cons_setescchar(
        void* cd,
        rs_cons_t* cons,
        int esc);

SS_INLINE void rs_cons_setsolved(
        void* cd,
        rs_cons_t* cons,
        bool solved);

SS_INLINE bool rs_cons_isalwaysfalse(
        void* cd,
        rs_cons_t* cons);

SS_INLINE bool rs_cons_isalwaysfalse_once(
        void* cd,
        rs_cons_t* cons);

void rs_cons_setestimated(
        void* cd,
        rs_cons_t* cons,
        bool estimatedp);

bool rs_cons_isestimated(
        void* cd,
        rs_cons_t* cons);

void rs_cons_setvectorno(
        void* cd,
        rs_cons_t* cons,
        int vectorno);

int rs_cons_getvectorno(
        void* cd,
        rs_cons_t* cons);

bool rs_cons_value_aliased(
        rs_sysi_t* cd,
        rs_cons_t* cons);

bool rs_cons_isuniforchar(
        void* cd,
        rs_cons_t* cons);

size_t rs_cons_likeprefixinfo(
        void* cd,
        rs_cons_t* cons,
        size_t* p_numfixedchars,
        size_t* p_numwildcards);

#if defined(SS_DEBUG)

const char* rs_cons_relopname(
        uint relop);

bool rs_cons_issolved(
        void* cd,
        rs_cons_t* cons);

#else /* defined(SS_DEBUG) */

#define rs_cons_relopname(r)        ""
#define rs_cons_issolved(cd, cons)  SU_BFLAG_TEST((cons)->cr_flags, RS_CONSFLAG_SOLVED)

#endif /* defined(SS_DEBUG) */

/* Check macro */
#define CONS_CHECK(c) ss_dassert(SS_CHKPTR(c) && (c)->cr_check == RSCHK_CONSTR)

#if defined(RS0CONS_C) || defined(SS_USE_INLINE)

SS_INLINE rs_sqlcons_t* rs_cons_getsqlcons(
        rs_sysi_t* cd __attribute__ ((unused)),
        rs_cons_t* cons)
{
        CONS_CHECK(cons);

        return &cons->cr_sqlcons;
}

/*##**********************************************************************\
 *
 *              rs_cons_relop
 *
 * Returns the relational operator of a constraint.
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
 *      relational operator of a constraint
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE uint rs_cons_relop(
    void* cd,
    rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(cons->cr_relop);
}

/*##**********************************************************************\
 *
 *              rs_cons_ano
 *
 * Returns the attribute index of a constraint.
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
 *      attribute index of a constraint
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_ano_t rs_cons_ano(
    void* cd,
    rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(cons->cr_ano);
}

/*##**********************************************************************\
 *
 *              rs_cons_atype
 *
 * Returns the attribute type of a constraint.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint pointer
 *
 * Return value - ref :
 *
 *      attribute type of a constraint
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_atype_t* rs_cons_atype(
    void* cd,
    rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(cons->cr_atype);
}

/*##**********************************************************************\
 *
 *              rs_cons_aval
 *
 * Returns the attribute value of a constraint.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint pointer
 *
 * Return value - ref:
 *
 *      attribute value of a constraint
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_aval_t* rs_cons_aval(
    void* cd,
    rs_cons_t* cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        if (cons->cr_sqlcons.sc_aval == NULL) {
            ss_dassert(cons->cr_relop == RS_RELOP_ISNULL ||
                       cons->cr_relop == RS_RELOP_ISNOTNULL);
            ss_dprintf_1(("rs_cons_aval:cons->cr_sqlcons.sc_aval == NULL:aval=%ld, relop=%s\n",
                (long)cons->cr_aval,
                rs_cons_relopname(cons->cr_relop)));
            return(cons->cr_aval);
        }
        if (rs_aval_isunknown(cd, cons->cr_sqlcons.sc_atype, cons->cr_sqlcons.sc_aval)) {
            ss_dprintf_1(("rs_cons_aval:unknownvalue=%d:aval=NULL, relop=%s\n",
                rs_aval_isunknown(cd, cons->cr_sqlcons.sc_atype, cons->cr_sqlcons.sc_aval),
                rs_cons_relopname(cons->cr_relop)));
            return (NULL);
        }
        ss_dprintf_1(("rs_cons_aval:normal return:aval=%ld, relop=%s\n",
            (long)cons->cr_aval,
            rs_cons_relopname(cons->cr_relop)));
        return(cons->cr_aval);
}

/*##**********************************************************************\
 *
 *              rs_cons_setsolved
 *
 * Sets the solved state of a constraint. By default the constraint is
 * not solved.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint pointer
 *
 *      solved - in
 *              new solved state of a constraint
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE void rs_cons_setsolved(
        void* cd,
        rs_cons_t* cons,
        bool solved)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        if (solved) {
            SU_BFLAG_SET(cons->cr_flags, RS_CONSFLAG_SOLVED);
        } else {
            SU_BFLAG_CLEAR(cons->cr_flags, RS_CONSFLAG_SOLVED);
        }
}

/*##**********************************************************************\
 *
 *              rs_cons_isalwaysfalse
 *
 * Checks whether the constraint condition is never satified.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint object
 *
 * Return value :
 *      TRUE if condition always false
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool rs_cons_isalwaysfalse(
        void*       cd,
        rs_cons_t*  cons)
{
        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        return(SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_ALWAYSFALSE));
}

/*##**********************************************************************\
 *
 *              rs_cons_isalwaysfalse_once
 *
 * Checks whether the constraint condition is never satified.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      cons - in, use
 *              constraint object
 *
 * Return value :
 *      TRUE if condition always false
 *      FALSE otherwise
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE bool rs_cons_isalwaysfalse_once(
        void*       cd,
        rs_cons_t*  cons)
{
        bool alwaysfalse_once = FALSE;

        SS_NOTUSED(cd);
        CONS_CHECK(cons);

        if (SU_BFLAG_TEST(cons->cr_flags, RS_CONSFLAG_ALWAYSFALSE)) {
            alwaysfalse_once = TRUE;
        }  else if (cons->cr_atype != NULL) {
            if (cons->cr_aval != NULL && rs_aval_isnull(cd, cons->cr_atype, cons->cr_aval)) {
                switch (cons->cr_relop) {
                    case RS_RELOP_EQUAL:
                    case RS_RELOP_NOTEQUAL:
                    case RS_RELOP_LT:
                    case RS_RELOP_LT_VECTOR:
                    case RS_RELOP_GT:
                    case RS_RELOP_GT_VECTOR:
                    case RS_RELOP_LE:
                    case RS_RELOP_LE_VECTOR:
                    case RS_RELOP_GE:
                    case RS_RELOP_GE_VECTOR:
                    case RS_RELOP_LIKE:
                        alwaysfalse_once = TRUE;
                        break;
                    case RS_RELOP_ISNULL:
                    case RS_RELOP_ISNOTNULL:
                        break;
                    default:
                        ss_error;
                }
            }
        }
        return(alwaysfalse_once);
}

#endif /* defined(RS0CONS_C) || defined(SS_USE_INLINE) */


#endif /* RS0CONS_H */
