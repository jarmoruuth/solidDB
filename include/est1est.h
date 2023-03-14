/*************************************************************************\
**  source       * tab1est.h
**  directory    * est
**  description  * The header for query cost estimation and
**               * key selection module
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


#ifndef TAB1EST_H
#define TAB1EST_H

#include "tab1defs.h"

#include <rs0types.h>
#include <rs0sysi.h>

#include <su0list.h>

typedef struct est_struct tb_est_t;

/* The following function chooses the best key and finds the
 * estimate for it. The information is stored into the returned structure
 * from where you can extract the information.
 */
tb_est_t* tb_est_create_estimate(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        rs_relh_t*  table,
        su_list_t*  constraint_list,
        int*        select_list,
        su_list_t*  order_by_list,
        int         infolevel,
        rs_key_t*   indexhintkey);

/* Use the following function to get the selected best key.
 */
SS_INLINE rs_key_t* tb_est_get_key(
        rs_sysi_t*  cd,
        tb_est_t*   estimate);

/* Use the following function to get the estimated delay time parameters the
 * SQL interpreter wants. The times are given in units of 0.1 milliseconds.
 * Note that estimated time before the first row arrives is
 * delay_at_start + average_delay_per_row. The total
 * time is delay_at_start + n_rows * average_delay_per_row.
 */
void tb_est_get_delays(
        rs_sysi_t*    cd,
        tb_est_t*     estimate,
        rs_estcost_t* delay_at_start,
        rs_estcost_t* average_delay_per_row);

/* Use the following function to get the estimated number of rows
 * the query returns.
 * The return value:
 *      0 = NO ESTIMATE,
 *      1 = APPROXIMATE,
 *      2 = EXACT.
 */
uint tb_est_get_n_rows(
        rs_sysi_t*    cd,
        tb_est_t*     estimate,
        rs_estcost_t* n_rows);

/* Use the following function to ask the number of matching
 * order bys for the query.
 */
uint tb_est_get_n_order_bys(
        rs_sysi_t*  cd,
        tb_est_t*   estimate);

/* Use the following function to ask if the table is accessed
 * using a full table scan.
 */
bool tb_est_get_full_scan(
        rs_sysi_t*  cd,
        tb_est_t*   estimate);

/* Use the following function to ask if the data must be retrieved
 * from the clustering key.
 */
bool tb_est_get_must_retrieve(
        rs_sysi_t*  cd,
        tb_est_t*   estimate);

/* Use the following function to ask if all rows are unique.
 */
bool tb_est_get_unique_value(
        rs_sysi_t* cd,
        tb_est_t* estimate);

bool tb_est_get_single_row(
        rs_sysi_t* cd,
        tb_est_t* estimate);

/* Use the following function to get list of constraints for each ano.
 */
SS_INLINE su_pa_t* tb_est_get_cons_byano(
        rs_sysi_t* cd,
        tb_est_t* estimate);

rs_estcost_t tb_est_getdiffrowcount(
	rs_sysi_t*   cd,
        tb_est_t*    estimate,
        rs_relh_t*   relh,
        uint         n,
        uint*        sql_cols);

/* Use the following to free the space allocated for the estimate
 * object.
 */
void tb_est_free_estimate(
        rs_sysi_t*  cd,
        tb_est_t*   estimate);

#ifdef SS_DEBUG

bool tb_est_check(
        tb_est_t* estimate);

#endif /* SS_DEBUG */

/* Below are functions for testing.
 */

void tb_est_initialize_test(
        rs_sysi_t*  sysi,
        long        n_rows);

void tb_est_init_selectivity_test(
        void);

tb_est_t* tb_est_get_est(
        void);

void tb_est_ensureselectivityinfo(
        rs_sysi_t* cd,
        rs_relh_t* relh);

void tb_est_updateselectivityinfo(
        rs_sysi_t* cd,
        rs_relh_t* relh);

rs_estcost_t tb_est_sortestimate(
        rs_sysi_t*   cd,
        rs_ttype_t*  ttype,
        rs_estcost_t lines,
        bool         external);

/* The estimate object */
struct est_struct {
        ss_debug(tb_check_t e_chk;)
        rs_key_t*    e_key;              /* chosen best key for search */
        rs_estcost_t e_delay_at_start;   /* delay at start */
        rs_estcost_t e_delay_per_row;    /* additional average delay per row */
        long         e_n_rows;           /* estimated number of result rows */
        uint         e_n_order_bys;      /* number of matching order bys */
        bool         e_full_scan;        /* if TRUE, a full table scan is used */
        bool         e_must_retrieve;    /* if TRUE, data must be retrieved
                                            from clustering key */
        rs_estcost_t* e_rowcounts;        /* Row counts for each column. */
        bool         e_unique_value;     /* Are all rows unique. */
        bool         e_single_row;       /* One row in result set. */
        su_pa_t*     e_cons_byano;       /* Lists of constraints by each ano. */
};

#define CHK_EST(e) ss_dassert(SS_CHKPTR(e) && (e)->e_chk == TBCHK_EST)

#if defined(TAB1EST_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *		tb_est_get_key
 *
 * Gets the chosen best key.
 *
 * Parameters :
 *
 *	estimate - in, use
 *		the estimate object
 *
 *
 * Output params:
 *
 * Return value - ref :
 *      the best key
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE rs_key_t* tb_est_get_key(
        rs_sysi_t* cd,
        tb_est_t* estimate)
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_key);
}

/*##**********************************************************************\
 * 
 *		tb_est_get_cons_byano
 * 
 * Returns a supa that contains a lists of constraints for each ano.
 * 
 * Parameters : 
 * 
 *		cd - 
 *			
 *			
 *		estimate - 
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
SS_INLINE su_pa_t* tb_est_get_cons_byano(
        rs_sysi_t* cd,
        tb_est_t* estimate)
{
        SS_NOTUSED(cd);
        ss_dassert(estimate);
        CHK_EST(estimate);

        return(estimate->e_cons_byano);
}

#endif /* defined(TAB1EST_C) || defined(SS_USE_INLINE) */

#endif /* TAB1EST_H */
