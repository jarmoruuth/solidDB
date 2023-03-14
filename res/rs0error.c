/*************************************************************************\
**  source       * rs0error.c
**  directory    * res
**  description  * Error generation
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

#include <ssstdarg.h>

#include <ssc.h>
#include <ssmem.h>
#include <sssprint.h>
#include <ssdebug.h>
#include <ssstdarg.h>

#include <su0error.h>
#include <su0err.h>

#include "rs0types.h"
#include "rs0error.h"
#include "rs0sysi.h"
#include "rs0key.h"

/*##**********************************************************************\
 * 
 *		rs_error_create
 * 
 * 
 * 
 * Parameters :
 * 
 *      p_errh - out, give
 * 
 * 
 *      code - in
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
void SS_CDECL rs_error_create(
        rs_err_t** p_errh,
        uint       code,
        ...
) {
        va_list arg_list;
     
        va_start(arg_list, code);
        su_err_vinit((su_err_t**)p_errh, code, arg_list);
        va_end(arg_list);
}

/*##**********************************************************************\
 * 
 *		rs_error_create_key
 * 
 * 
 * 
 * Parameters :
 * 
 *      p_errh - out, give
 * 
 * 
 *      code - in
 *
 *      key - in 
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SS_CDECL rs_error_create_key(
        rs_err_t** p_errh,
        uint       code,
        rs_key_t*  key
) {
        char keyname[SU_MAXNAMELEN+4];
        char *kn = NULL;

        ss_dassert(key != NULL);
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        if (key != NULL) {
            kn = rs_key_name(NULL, key);
        }
#else /* SS_MYSQL */
        kn = rs_key_name(NULL, key);
#endif /* SS_MYSQL */

        if (kn != NULL &&
           (code == DBE_ERR_UNIQUE_S || code == E_CANNOTDROPUNQCOL_S))
        {
            int len = strlen(RSK_NEW_UNQKEYCONSTRSTR)-2;
            if (strncmp(kn, RSK_NEW_UNQKEYCONSTRSTR, len)==0) {
                kn += len;
            }
        }
        if (kn == NULL || kn[0] == '$' || kn[0] == 0) {
            strcpy(keyname, "");
        } else {
            SsSprintf(keyname, "(%s) ", kn);
        }
        rs_error_create(p_errh, code, keyname);
}

/*##**********************************************************************\
 * 
 *		rs_error_create_text
 * 
 * Initializes error object with error code and properly formatted
 * error text.
 * 
 * Parameters : 
 * 
 *	p_errh - 
 *		
 *		
 *	code - 
 *		
 *		
 *	text - 
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
void rs_error_create_text(
        rs_err_t** p_errh,
        uint       code,
        char*      text
) {
        su_err_init_text((su_err_t**)p_errh, code, text);
}

/*##**********************************************************************\
 * 
 *		rs_error_free
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	errh - in, take
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
void rs_error_free(
        void*     cd,
        rs_err_t* errh
) {
        SS_NOTUSED(cd);

        su_err_done((su_err_t*)errh);
}

/*##**********************************************************************\
 * 
 *		rs_error_printinfo
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	errh - in
 *		
 *		
 *	errcode - out
 *		
 *		
 *	errstr - out, give
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
void rs_error_printinfo(
        void*     cd,
        rs_err_t* errh,
        uint*     errcode,
        char**    errstr
) {
        su_ret_t rc;

        SS_NOTUSED(cd);

        su_err_printinfo((su_err_t*)errh, &rc, errstr);

        if (errcode != NULL) {
            *errcode = (uint)rc;
        }
}

/*##**********************************************************************\
 * 
 *		rs_error_geterrstr
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - in
 *		
 *		
 *	errh - in
 *		
 *		
 * Return value - ref : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* rs_error_geterrstr(
        void*     cd,
        rs_err_t* errh
) {
        SS_NOTUSED(cd);

        return(su_err_geterrstr((su_err_t*)errh));
}

/*##**********************************************************************\
 * 
 *		rs_error_geterrcode
 * 
 * 
 * 
 * Parameters : 
 * 
 *	cd - 
 *		
 *		
 *	errh - 
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
int rs_error_geterrcode(
        void*     cd,
        rs_err_t* errh
) {
        SS_NOTUSED(cd);

        return(su_err_geterrcode((su_err_t*)errh));
}

/*##**********************************************************************\
 * 
 *		rs_error_copyerrh
 * 
 * Creates a duplicate of error handle
 * 
 * Parameters : 
 * 
 *	p_errh - out, give
 *		
 *		
 *	errh - in, use
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
void rs_error_copyerrh(
        rs_err_t** p_errh,
        rs_err_t* errh)
{
        su_err_copyerrh((su_err_t**)p_errh, (su_err_t*)errh);
}

