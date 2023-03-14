/*************************************************************************\
**  source       * xs0sqli.c
**  directory    * xs
**  description  * eXternal Sorter SQL Interface
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

#include <ssc.h>
#include <ssdebug.h>
#include <ssstddef.h>
#include "xs2cmp.h"
#include "xs1sort.h"
#include "xs0mgr.h"

static xs_sorter_t* sorter_cmpinit(
        void* cd,
        rs_ttype_t* ttype,
        ulong lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        xs_qcomparefp_t comp_fp,
        bool sql,
        bool testonly)
{
        xs_mgr_t* xsmgr;
        xs_sorter_t* sorter;

        ss_dprintf_1(("sorter_cmpinit: order_c = %u\n", order_c));
        ss_output_1({
            uint i;
            SsDbgPrintf("order_cols:");
            for (i = 0; i < order_c; i++) {
                SsDbgPrintf(" %u", order_cols[i]);
            }
            SsDbgPrintf("\ndescarray:");
            for (i = 0; i < order_c; i++) {
                SsDbgPrintf(" %d", descarr[i]);
            }
            SsDbgPrintf("\n");
        })
        ss_output_1(rs_ttype_print(cd, ttype));
        xsmgr = rs_sysi_xsmgr(cd);
        if (xsmgr == NULL) {
            return (NULL);
        }
        sorter = xs_mgr_sortinit(
                    xsmgr,
                    ttype,
                    lines,
                    exact,
                    order_c,
                    order_cols,
                    descarr,
                    cd,
                    (xs_qcomparefp_t)comp_fp,
                    sql,
                    testonly);
        return (sorter);
}

xs_sorter_t* xs_sorter_cmpinit(
        void* cd,
        rs_ttype_t* ttype,
        ulong lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        xs_qcomparefp_t comp_fp)
{
        xs_sorter_t* sorter =
            sorter_cmpinit(cd,
                           ttype,
                           lines,
                           exact,
                           order_c,
                           order_cols,
                           descarr,
                           comp_fp,
                           FALSE,
                           FALSE);
        return (sorter);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlinit
 *
 * Creates an eXternal Sorter object. Part of SQL function block
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	ttype - in, hold
 *		tuple type
 *
 *	lines - in
 *		estimated # of lines to be sorted
 *
 *	exact - in
 *		TRUE if lines is known to be exact; FALSE otherwise
 *
 *	order_c - in
 *		number of ORDER BY columns
 *
 *	order_cols - in
 *		array of column numbers in ORDER BY
 *
 *	descarr - in
 *		array of booleans telling whether
 *          the corresponding ORDER BY column has
 *          DESC specification
 *
 *  testonly - in
 *      if 1, a sorter is not created but a non-NULL
 *      dummy pointer is returned if external sorter
 *      would be used
 *
 * Return value - give :
 *      sorter object or NULL when
 *      external sorter should not be used
 *      for some reason (lines is small or
 *      not enough resources for external sorting)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
xs_sorter_t* xs_sorter_sqlinit(
        void* cd,
        rs_ttype_t* ttype,
        rs_estcost_t lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        bool testonly)
{

        xs_sorter_t* sorter =
            sorter_cmpinit(
                    cd,
                    ttype,
                    lines >= (rs_estcost_t)LONG_MAX ? LONG_MAX : (ulong)lines,
                    exact,
                    order_c,
                    order_cols,
                    descarr,
                    (xs_qcomparefp_t)xs_qsort_cmp,
                    TRUE,
                    testonly);
        return (sorter);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqladd
 *
 * Adds a row to sorter
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter object
 *
 *	tval - in, use
 *		tuple value to be added to sorter
 *
 *	p_errh - out, give
 *		if return is FALSE: pointer to error handle object
 *          or NULL when error handle not required
 *
 * Return value :
 *      TRUE when successful, FALSE when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool xs_sorter_sqladd(
        void* cd,
        xs_sorter_t* sorter,
        rs_tval_t* tval,
        rs_err_t** p_errh)
{
        bool succp;

        SS_NOTUSED(cd);

        succp = xs_sorter_addtuple(sorter, tval, p_errh);
        return (succp);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlrunstep
 *
 * Runs sort step
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter
 *
 *	p_errh - out, give
 *		if return is XS_RC_ERROR: pointer to error handle object
 *          or NULL when error handle not required
 *
 *
 * Return value :
 *      XS_RC_CONT (0) when not completed
 *      XS_RC_SUCC (1) when completed successfully
 *      XS_RC_ERROR (2) when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
xs_ret_t xs_sorter_sqlrunstep(
        void* cd,
        xs_sorter_t* sorter,
        rs_err_t** p_errh)
{
        xs_ret_t rc;
        bool emptyset;

        SS_NOTUSED(cd);

        rc = xs_sorter_merge(sorter, &emptyset, p_errh);
        return (rc);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlfetchnext
 *
 * Fetches next value from sorted sorter object
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter object
 *
 *	p_finished - out
 *		pointer to boolean telling whether fetch operation finished
 *          (always put to TRUE)
 *
 * Return value - ref :
 *      tval object or NULL when end reached
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* xs_sorter_sqlfetchnext(
        void* cd,
        xs_sorter_t* sorter,
        bool* p_finished)
{
        bool succp;
        rs_tval_t* tval;

        SS_NOTUSED(cd);

        ss_dassert(p_finished != NULL);
        *p_finished = TRUE;
        succp = xs_sorter_fetchnext(sorter, &tval);
        ss_dassert(succp ? (tval != NULL) : (tval == NULL));
        return (tval);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlfetchprev
 *
 * Fetches previous value
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter object
 *
 *	p_finished - out
 *		pointer to boolean telling whether fetch operation finished
 *          (always put to TRUE)
 *
 *
 * Return value - ref :
 *      tval object or NULL when beginning reached
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
rs_tval_t* xs_sorter_sqlfetchprev(
        void* cd,
        xs_sorter_t* sorter,
        bool* p_finished)
{
        bool succp;
        rs_tval_t* tval;

        SS_NOTUSED(cd);

        ss_dassert(p_finished != NULL);
        *p_finished = TRUE;
        succp = xs_sorter_fetchprev(sorter, &tval);
        ss_dassert(succp ? (tval != NULL) : (tval == NULL));
        return (tval);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlcursortobegin
 *
 * Moves the cursor to begin of sorted result set
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter object
 *
 * Return value :
 *      TRUE when successful (should never fail)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool xs_sorter_sqlcursortobegin(
        void* cd,
        xs_sorter_t* sorter)
{
        bool succp;

        SS_NOTUSED(cd);

        succp = xs_sorter_cursortobegin(sorter);
        ss_dassert(succp);
        return (TRUE);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqlcursortoend
 *
 * Moves the cursor to end of sorted result set
 *
 * Parameters :
 *
 *	cd - in, use
 *		client data
 *
 *	sorter - use
 *		sorter object
 *
 * Return value :
 *      TRUE when successful (should never fail)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool xs_sorter_sqlcursortoend(
        void* cd,
        xs_sorter_t* sorter)
{
        bool succp;

        SS_NOTUSED(cd);

        succp = xs_sorter_cursortoend(sorter);
        ss_dassert(succp);
        return (TRUE);
}

/*##**********************************************************************\
 *
 *		xs_sorter_sqldone
 *
 * Deletes a sorter object
 *
 * Parameters :
 *
 *	cd -
 *
 *
 *	sorter -
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
void xs_sorter_sqldone(
        void* cd,
        xs_sorter_t* sorter)
{
        SS_NOTUSED(cd);

        xs_sorter_done(sorter);
}
