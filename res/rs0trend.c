/*************************************************************************\
**  source       * rs0trend.c
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

#define RS0TREND_C

#include <ssenv.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include <su0list.h>

#include "rs0trend.h"

static void trend_freefun(
        trend_functx_t* tf);


/*##**********************************************************************\
 * 
 *		rs_trend_init
 * 
 * 
 * 
 * Parameters : 	 - none
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
rs_trend_t* rs_trend_init(void)
{
        rs_trend_t* trend;

        trend = SSMEM_NEW(rs_trend_t);

        ss_debug(trend->tre_chk = RSCHK_TREND);
        trend->tre_list = su_list_init((void(*)(void*))trend_freefun);
        trend->tre_n = NULL;

        return(trend);
}

/*##**********************************************************************\
 * 
 *		rs_trend_done
 * 
 * 
 * 
 * Parameters : 
 * 
 *	trend - 
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
void rs_trend_done(
        rs_trend_t* trend)
{
        CHK_TREND(trend);

        su_list_done(trend->tre_list);
        SsMemFree(trend);
}

static void trend_addfun(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx,
        bool firstp)
{
        trend_functx_t* tf;

        CHK_TREND(trend);

        tf = SSMEM_NEW(trend_functx_t);

        tf->tf_fun = fun;
        tf->tf_ctx = ctx;
        tf->tf_trans = trans;

        if (firstp) {
            su_list_insertfirst(trend->tre_list, tf);
        } else {
            su_list_insertlast(trend->tre_list, tf);
        }
}

/*##**********************************************************************\
 * 
 *		rs_trend_addfun
 * 
 * Adds a new function to transaction end list.
 * 
 * Parameters : 
 * 
 *	trend - in, use
 *		
 *		
 *	fun - in, hold
 *		User function.
 *		
 *	ctx - in, hold
 *		Optional context to user function. It must be cleaned
 *		by user function when trop is RS_TROP_DONE.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_trend_addfun(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx)
{
        trend_addfun(
            trend,
            trans,
            fun,
            ctx,
            FALSE);
}

/*##**********************************************************************\
 * 
 *		rs_trend_addfun_first
 * 
 * Adds a new function to transaction end list as the first item.
 * 
 * Parameters : 
 * 
 *	trend - in, use
 *		
 *		
 *	fun - in, hold
 *		User function.
 *		
 *	ctx - in, hold
 *		Optional context to user function. It must be cleaned
 *		by user function when trop is RS_TROP_DONE.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_trend_addfun_first(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx)
{
        trend_addfun(
            trend,
            trans,
            fun,
            ctx,
            TRUE);
}

void rs_trend_addstmtfun(
        rs_trend_t* trend,
        sqltrans_t* trans,
        rs_trendfun_t fun,
        void* ctx)
{
        trend_functx_t* tf;

        CHK_TREND(trend);

        tf = SSMEM_NEW(trend_functx_t);

        tf->tf_fun = fun;
        tf->tf_ctx = ctx;
        tf->tf_trans = trans;

        su_list_insertlast(trend->tre_list, tf);
}


int rs_trend_removefun(
        rs_trend_t* trend,
        rs_trendfun_t fun,
        void* ctx)
{
        int count = 0;
        trend_functx_t* tf;
        su_list_node_t* n;

        CHK_TREND(trend);

        n = su_list_first(trend->tre_list);
        while (n != NULL) {
            tf = su_listnode_getdata(n);
            if (tf->tf_fun == fun && tf->tf_ctx == ctx) {
                n = su_list_removeandnext(trend->tre_list, n);
                count++;
            } else {
                n = su_list_next(trend->tre_list, n);
            }
        }
        return(count);
}

/*#***********************************************************************\
 * 
 *		trend_freefun
 * 
 * 
 * 
 * Parameters : 
 * 
 *	tf - 
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
static void trend_freefun(
        trend_functx_t* tf)
{
        SsMemFree(tf);
}

void rs_trend_move(
        rs_trend_t* target_trend,
        rs_trend_t* source_trend)
{
        su_list_node_t* n;
        
        CHK_TREND(target_trend);
        CHK_TREND(source_trend);

        while ((n = su_list_first(source_trend->tre_list)) != NULL) {
            su_list_insertlast(
                target_trend->tre_list,
                su_listnode_getdata(n));
            su_list_remove_nodatadel(source_trend->tre_list, n);
        }
}

/*##**********************************************************************\
 * 
 *		rs_trend_isfunset
 * 
 * Returns TRUE if the given funtions is allready set
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
bool rs_trend_isfunset(
        rs_trend_t* trend,
        rs_trendfun_t fun,
        void* ctx)
{
        trend_functx_t* tf;
        su_list_node_t* n;

        CHK_TREND(trend);

        n = su_list_first(trend->tre_list);
        while (n != NULL) {
            tf = su_listnode_getdata(n);
            if (tf->tf_fun == fun && tf->tf_ctx == ctx) {
                return(TRUE);
            } else {
                n = su_list_next(trend->tre_list, n);
            }
        }
        return(FALSE);
}

