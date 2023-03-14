/*************************************************************************\
**  source       * rs0trend.h
**  directory    * res
**  description  * Transaction end routines.
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


#ifndef RS0TREND_H
#define RS0TREND_H

#include "su0error.h"
#include "rs0types.h"

#define CHK_TREND(trend)    ss_dassert(SS_CHKPTR(trend) && (trend)->tre_chk == RSCHK_TREND)

/* Different transaction end states where functions can be called.
 */
typedef enum {
        RS_TROP_BEFORECOMMIT,
        RS_TROP_AFTERCOMMIT,
        RS_TROP_BEFOREROLLBACK,
        RS_TROP_AFTERROLLBACK,
        RS_TROP_AFTERSTMTCOMMIT,
        RS_TROP_AFTERSTMTROLLBACK,
        RS_TROP_DONE
} rs_trop_t;

/* Possible return codes from user function.
 */
typedef enum {
        RS_TRFUNRET_KEEP,       /* Keep function in list. */
        RS_TRFUNRET_REMOVE,     /* Remove function from list. */
        RS_TRFUNRET_CONT        /* Continue, only for RS_TROP_AFTERCOMMIT. */
} rs_trfunret_t;

typedef rs_trfunret_t (*rs_trendfun_t)(
            rs_sysi_t* cd,
            sqltrans_t* trans,
            rs_trop_t trop,
            void* ctx);

typedef struct rs_trend_st rs_trend_t;

struct rs_trend_st {
        ss_debug(rs_check_t tre_chk;)
        su_list_t*      tre_list;
        su_list_node_t* tre_n;
};

rs_trend_t* rs_trend_init(
        void);

void rs_trend_done(
        rs_trend_t* trend);

void rs_trend_addfun(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx);

void rs_trend_addfun_first(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx);

void rs_trend_addstmtfun(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx);

int rs_trend_removefun(
        rs_trend_t* trend,
        rs_trendfun_t fun,
        void* ctx);

SS_INLINE su_ret_t rs_trend_transend(
        rs_trend_t* trend,
        rs_sysi_t* cd,
        rs_trop_t trop);

SS_INLINE void rs_trend_stmttransend(
        rs_trend_t* trend,
        rs_sysi_t* cd,
        rs_trop_t trop);

void rs_trend_move(
        rs_trend_t* target_trend,
        rs_trend_t* source_trend);

bool rs_trend_isfunset(
        rs_trend_t* trend,
        rs_trendfun_t fun,
        void* ctx);

SS_INLINE bool rs_trend_isfun(
        rs_trend_t* trend);

typedef struct {
        rs_trendfun_t   tf_fun;
        void*           tf_ctx;
        sqltrans_t*     tf_trans;
} trend_functx_t;

#if defined(RS0TREND_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		rs_trend_transend
 * 
 * Calls all transaction end functions.
 * 
 * Parameters : 
 * 
 *	trend - in
 *		
 *		
 *	cd - in
 *		
 *		
 *	trans - in
 *		
 *		
 *	trop - in
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
SS_INLINE su_ret_t rs_trend_transend(
        rs_trend_t* trend,
        rs_sysi_t* cd,
        rs_trop_t trop)
{
        su_ret_t rc;
        trend_functx_t* tf;
        rs_trfunret_t funret;

        CHK_TREND(trend);

        rc = DBE_RC_SUCC;
        if (trend->tre_n == NULL) {
            trend->tre_n = su_list_first(trend->tre_list);
        }
        while (trend->tre_n != NULL && rc == DBE_RC_SUCC) {
            tf = (trend_functx_t*)su_listnode_getdata(trend->tre_n);
            funret = (*tf->tf_fun)(cd, tf->tf_trans, trop, tf->tf_ctx);
            ss_dprintf_2(("rs_trend_transend:trop %d funret=%d\n", trop,funret));
            switch (funret) {
                case RS_TRFUNRET_KEEP:
                    trend->tre_n = (su_list_node_t*)su_list_next(trend->tre_list, trend->tre_n);
                    break;
                case RS_TRFUNRET_REMOVE:
                    trend->tre_n = (su_list_node_t*)su_list_removeandnext(trend->tre_list, trend->tre_n);
                    break;
                case RS_TRFUNRET_CONT:
                    ss_dassert(trop == RS_TROP_AFTERCOMMIT);
                    rc = DBE_RC_CONT;
                    break;
                default:
                    ss_rc_error(funret);
            }
        }
        return(rc);
}

SS_INLINE void rs_trend_stmttransend(
        rs_trend_t* trend,
        rs_sysi_t* cd,
        rs_trop_t trop)
{
        trend_functx_t* tf;
        su_list_node_t* n;
        rs_trfunret_t funret;

        CHK_TREND(trend);

        n = su_list_first(trend->tre_list);
        while (n != NULL) {
            tf = (trend_functx_t*)su_listnode_getdata(n);
            funret = (*tf->tf_fun)(cd, tf->tf_trans, trop, tf->tf_ctx);
            ss_dprintf_2(("rs_trend_stmttransend:trop %d funret=%d\n", trop,funret));
            switch (funret) {
                case RS_TRFUNRET_KEEP:
                    n = su_list_next(trend->tre_list, n);
                    break;
                case RS_TRFUNRET_REMOVE:
                    n = su_list_removeandnext(trend->tre_list, n);
                    break;
                default:
                    ss_rc_error(funret);
            }
        }
}

/*##**********************************************************************\
 * 
 *		rs_trend_isfun
 * 
 * Returns TRUE if there is at least one trend function
 * 
 * Parameters : 
 * 
 *	trend - in, use
 *		
 *		
 *	fun - in, use
 *		User function.
 *		
 *	ctx - in, use
 *		Optional context to user function.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE bool rs_trend_isfun(
        rs_trend_t* trend)
{
        CHK_TREND(trend);

        return(su_list_length(trend->tre_list) > 0);
}

#endif /* defined(RS0TREND_C) || defined(SS_USE_INLINE) */

#endif /* RS0TREND_H */
