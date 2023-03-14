/*************************************************************************\
**  source       * rs0order.c
**  directory    * res
**  description  * Order by services.
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
This module contains the implementation of order by search constarint

Limitations:
-----------
None

Error handling:
--------------
None

Objects used:
------------
None

Preconditions:
-------------
None

Multithread considerations:
--------------------------
Code is fully re-entrant.
The same ob object can not be used simultaneously from many threads.


Example:
-------
See torder.c

**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstdio.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "rs0types.h"
#include "rs0order.h"
#include "rs0sysi.h"

/* Check macro */
#define OB_CHECK(ob) ss_dassert(SS_CHKPTR(ob) && (ob)->ob_check == RSCHK_ORDERBY)

/* Structure for an orderby object.
*/

struct tborderbystruct {
        
        rs_check_t  ob_check;   /* check field */
        rs_ano_t    ob_ano;     /* attribute index */
        bool        ob_asc;     /* if TRUE, ascending order */
        bool        ob_solved;  /* if TRUE, order by is solved, used by
                                   query plan generator */

}; /*  rs_ob_t */

/*##**********************************************************************\
 * 
 *		rs_ob_init
 * 
 * Creates a new orderby object.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ano - in
 *		attribute index
 *
 *	asc - in
 *		if TRUE, order by is in ascending order
 * 
 * Return value - give : 
 * 
 *      pointer to the newly allocated orderby structure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ob_t* rs_ob_init(cd, ano, asc)
	void*    cd;
	rs_ano_t ano;
	bool     asc;
{
        rs_ob_t* ob;

        SS_NOTUSED(cd);

        ob = SSMEM_NEW(rs_ob_t);

        ob->ob_check = RSCHK_ORDERBY;
        ob->ob_ano = ano;
        ob->ob_asc = asc;
        ob->ob_solved = FALSE;

        return(ob);
}

/*##**********************************************************************\
 * 
 *		rs_ob_done
 * 
 * Frees an orderby object.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ob - in, take
 *		pointer to orderby
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_ob_done(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        SsMemFree(ob);
}

/*##**********************************************************************\
 * 
 *		rs_ob_aindex
 * 
 * Returns attribute index of an orderby.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ob - in, use
 *		pointer to orderby
 *
 * Return value : 
 * 
 *      attribute index
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
rs_ano_t rs_ob_ano(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        return(ob->ob_ano);
}

/*##**********************************************************************\
 * 
 *		rs_ob_asc
 * 
 * Returns TRUE if orderby is in ascending order, otherwise returns
 * FALSE.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ob - in, use
 *		pointer to orderby
 * 
 * Return value : 
 * 
 *      TRUE    - orderby in ascending order
 *      FALSE   - orderby in descending order
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_ob_asc(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        return(ob->ob_asc);
}

/*##**********************************************************************\
 * 
 *		rs_ob_setasc
 * 
 * Changes the order by criteria from descending order to ascending
 * order. The current order by criteria must be in descending order
 * when this function is called.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		client data
 *		
 *	ob - use
 *		pointer to orderby
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_ob_setasc(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        ss_dassert(!ob->ob_asc);
        ob->ob_asc = TRUE;
}

/*##**********************************************************************\
 * 
 *		rs_ob_setdesc
 * 
 * Changes the order by criteria from ascending order to descending
 * order. The current order by criteria must be in ascending order
 * when this function is called.
 * 
 * Parameters : 
 * 
 *	cd - in
 *		client data
 *		
 *	ob - use
 *		pointer to orderby
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void rs_ob_setdesc(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        ss_dassert(ob->ob_asc);
        ob->ob_asc = FALSE;
}

/*##**********************************************************************\
 * 
 *		rs_ob_setsolved
 * 
 * Sets the solved state of an orderby. By default the orderby is not
 * solved.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ob - in out, use
 *		pointer to orderby
 *
 *	solved - in
 *		new orderby solved state
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void rs_ob_setsolved(cd, ob, solved)
	void*    cd;
	rs_ob_t* ob;
	bool solved;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        ob->ob_solved = solved;
}

/*##**********************************************************************\
 * 
 *		rs_ob_issolved
 * 
 * Returns the orderby solved state. By default the orderby is not
 * solved.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *
 *	ob - in, use
 *		pointer to orderby
 *
 * Return value : 
 * 
 *      TRUE    - orderby solved
 *      FALSE   - orderby not solved
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool rs_ob_issolved(cd, ob)
	void*    cd;
	rs_ob_t* ob;
{
        SS_NOTUSED(cd);
        OB_CHECK(ob);

        return(ob->ob_solved);
}
