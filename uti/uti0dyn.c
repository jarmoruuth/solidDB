/*************************************************************************\
**  source       * uti0dyn.c
**  directory    * uti
**  description  * Dynamic strings
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

See nstdyn.doc.

Limitations:
-----------

None.

Error handling:
--------------

None.

Objects used:
------------

None.

Preconditions:
-------------

None.

Multithread considerations:
--------------------------

None.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssstring.h>
#include <ssmem.h>
#include <ssdebug.h>
#include "uti0dyn.h"


/* functions ***********************************************/


/*##**********************************************************************\
 * 
 *		dstr_free
 * 
 * Releases resources allocated for a dynstr.
 * 
 * Parameters : 
 * 
 *	p_ds - in out, take
 *		pointer to a dynstr variable, *p_ds set to NULL
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
void dstr_free(p_ds)
	dstr_t* p_ds;
{
        if (*p_ds != NULL) {
            SsMemFree(*p_ds);
            *p_ds = NULL;
        }
}

/*##**********************************************************************\
 * 
 *		dstr_set
 * 
 * Set a dynstr from an ordinary asciiz string.
 * 
 * Parameters : 
 * 
 *	p_ds - in out, give
 *		pointer to a dynstr variable
 *
 *	str - in, use
 *		an asciiz string
 *
 * Return value - ref : 
 * 
 *      the new value of *p_ds
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
dstr_t dstr_set(p_ds, str)
	dstr_t* p_ds;
	const char* str;
{
        return(dstr_setdata(p_ds, (char *)str, strlen(str)));
}


/*##**********************************************************************\
 * 
 *		dstr_app
 * 
 * Append an ordinary asciiz string to a dynstr.
 * 
 * Parameters : 
 * 
 *	p_ds - in out, give
 *		pointer to a dynstr variable
 *
 *	str - in, use
 *		an asciiz string
 *
 * Return value - ref : 
 * 
 *      the new value of *p_ds
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
dstr_t dstr_app(p_ds, str)
	dstr_t* p_ds;
	const char* str;
{
        return(dstr_appdata(p_ds, (char *)str, strlen(str)));
}


/*##**********************************************************************\
 * 
 *		dstr_setdata
 * 
 * Set a dynstr from a data area.
 * 
 * Parameters : 
 * 
 *	p_ds - out, give
 *		pointer to a dynstr variable
 *
 *	data - in, use
 *		pointer to a data area
 *
 *      datalen - in
 *		length of the data
 *
 * Return value : 
 * 
 *      the new value of *p_ds
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
dstr_t dstr_setdata(p_ds, data, datalen)
	dstr_t* p_ds;
	void* data;
	size_t datalen;
{
        if (*p_ds == NULL) *p_ds = (dstr_t)SsMemAlloc(datalen + 1);
        else *p_ds = (dstr_t)SsMemRealloc(*p_ds, datalen + 1);
        (*p_ds)[datalen] = '\0';
        return(memcpy(*p_ds, data, datalen));
}


/*##**********************************************************************\
 * 
 *		dstr_appdata
 * 
 * Appends data from a data area to a dynstr.
 * 
 * Parameters : 
 * 
 *	p_ds - in out, give
 *		pointer to a dynstr variable
 *
 *	data - in, use
 *		pointer to a data area
 *
 *      datalen - in
 *		length of the data
 *
 * Return value : 
 * 
 *      the new value of *p_ds
 * 
 * Limitations  : 
 * 
 * Globals used :
 */
dstr_t dstr_appdata(p_ds, data, datalen)
	dstr_t* p_ds;
	void* data;
	size_t datalen;
{
        size_t size_ds;
        register dstr_t new_dynstr;

        ss_assert(p_ds != NULL);
        if (*p_ds == NULL) {
            return (dstr_setdata(p_ds, data, datalen));
        }
        size_ds = strlen(*p_ds);
        *p_ds = new_dynstr = (dstr_t)SsMemRealloc(*p_ds, size_ds + datalen + 1);
        memcpy(new_dynstr + size_ds, data, datalen);
        new_dynstr[size_ds + datalen] = '\0';
        return(new_dynstr);
}
