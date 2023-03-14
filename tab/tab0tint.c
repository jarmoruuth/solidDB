/*************************************************************************\
**  source       * tab0tint.c
**  directory    * tab
**  description  * Table level funblock interface for relcur and relh
**               * functions.
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
#endif 

#include "tab0tint.h"
#include "tab0relh.h"
#include "tab0relc.h"
#include "tab0tran.h"

#ifdef SS_NT
#pragma warning(disable:4028)
#endif

static tb_tint_t tbfunblock = {
    
        tb_relcur_create,
        tb_relcur_disableinfo,
        tb_relcur_free,
        tb_relcur_orderby,
        tb_relcur_tabconstr,
        tb_relcur_project,
        tb_relcur_endofconstr,
        tb_relcur_ordered,
        tb_relcur_tabopen,
        tb_relcur_ttype,
        tb_relcur_next,
        tb_relcur_prev,
        tb_relcur_update,
        tb_relcur_delete,
        tb_relcur_begin,
        tb_relcur_end,
        tb_relcur_copytref,
        tb_relcur_saupdate,
        tb_relcur_sadelete,
        tb_relcur_setposition,
        
        tb_relh_sainsert
};

#ifdef SS_NT
#pragma warning(default:4028)
#endif

/*##**********************************************************************\
 * 
 *		tb_tint_init
 * 
 * 
 * 
 * Parameters : 
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
tb_tint_t* tb_tint_init(){
        return( &tbfunblock );
}

/*##**********************************************************************\
 * 
 *		tb_tint_done
 * 
 * 
 * 
 * Parameters :
 *
 *     interface - 
 *
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void tb_tint_done(tb_tint_t* interface __attribute__ ((unused))){
}








