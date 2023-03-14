/*************************************************************************\
**  source       * su0err.c
**  directory    * su
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
#include <ssmemtrc.h>
#include <sssprint.h>
#include <ssdebug.h>

#include "su1check.h"
#include "su0error.h"
#include "su0err.h"

#define CHK_ERR(err)    ss_dassert(su_err_check(err))

/* type for error handle
 */
struct suerrstruct {
        ss_debug(su_check_t err_chk;)
        su_ret_t            err_code;  /* integer code for the error */
        char*               err_text;  /* descriptive text for the error */
};

/*#***********************************************************************\
 * 
 *		err_alloc
 * 
 * Allocates a new error handle.
 * 
 * Parameters : 
 * 
 *	code - in
 *	    Error code.
 *		
 *	text - in, take
 *		Error text allocated by SsMemAlloc.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static su_err_t* err_alloc(su_ret_t code, char* text)
{
        su_err_t* err;

        err = SSMEM_NEW(su_err_t);

        ss_debug(err->err_chk = SUCHK_ERR);
        err->err_code = code;
        err->err_text = text;

        FAKE_CODE_BLOCK_EQ(FAKE_SU_STOPONRC, code, {ss_error; } );

        return(err);
}

/*##**********************************************************************\
 * 
 *		su_err_init
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
void SS_CDECL su_err_init(
        su_err_t** p_errh,
        su_ret_t   code,
        ...
) {
        va_list ap;

        ss_bassert(code != 0);

        va_start(ap, code);
        su_err_vinit(p_errh, code, ap);
        va_end(ap);
}

/*##**********************************************************************\
 * 
 *		su_err_init_noargs
 * 
 * Same as su_err_init but does not take any extra arguments. This is safe
 * to call even if the explanation text for the rc expects arguments.
 * maybe we should dassert that it won't expect.
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
void su_err_init_noargs(
        su_err_t** p_errh,
        su_ret_t code)
{
        if (p_errh != NULL) {
            char* text = su_rc_givetext_noargs(code);
            *p_errh = err_alloc(code, text);
        }
}
            
/*##**********************************************************************\
 * 
 *		su_err_vinit
 * 
 * Same as su_err_init() but takes va_list instead of '...'
 * 
 * Parameters :
 * 
 *      p_errh - out, give
 * 
 * 
 *      code - in
 *
 *      arg_list
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
void SS_CDECL su_err_vinit(
        su_err_t** p_errh,
        su_ret_t   code,
        va_list    arg_list
) {
        ss_bassert(code != 0);
        ss_dassert(code != DBE_RC_CONT);

        if (p_errh) {
            *p_errh = err_alloc(code, su_rc_vgivetext(code, arg_list));
            ss_pprintf_2(("su_err_vinit:%d: %d, %s\n", (int)*p_errh, code, (*p_errh)->err_text));
        } else {
            ss_pprintf_2(("su_err_vinit:%s\n", su_rc_nameof(code)));
        }
}

/*##**********************************************************************\
 * 
 *		su_err_init_text
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
void su_err_init_text(
        su_err_t** p_errh,
        su_ret_t   code,
        char*      text)
{
        ss_bassert(code != 0);

        if (p_errh) {
            *p_errh = err_alloc(code, SsMemStrdup(text));
            ss_pprintf_2(("su_err_init_text:%d: %d, %s\n", (int)*p_errh, code, text));
        }
}

/*##**********************************************************************\
 * 
 *		su_err_done
 * 
 * 
 * 
 * Parameters : 
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
void su_err_done(
        su_err_t* errh
) {
        if (errh != NULL) {
            CHK_ERR(errh);
            ss_dprintf_4(("su_err_done:%d\n", (int)errh));
            SsMemFree(errh->err_text);
            SsMemFree(errh);
        }
}

/*##**********************************************************************\
 * 
 *		su_err_printinfo
 * 
 * 
 * 
 * Parameters : 
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
void su_err_printinfo(
        su_err_t* errh,
        su_ret_t* p_errcode,
        char**    p_errstr
) {
        CHK_ERR(errh);

        if (p_errcode != NULL) {
            *p_errcode = errh->err_code;
        }
        if (p_errstr != NULL) {
            *p_errstr = SsMemStrdup(errh->err_text);
        }
}

/*##**********************************************************************\
 * 
 *		su_err_geterrstr
 * 
 * Returns a reference to an error string stored in errh 
 * Can in some cases be used instead of su_err_printinfo.
 * 
 * Parameters : 
 * 
 *	errh - in
 *		
 *		
 * Return value - ref : 
 * 
 *      Pointer to error string
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* su_err_geterrstr(
        su_err_t* errh
) {
        CHK_ERR(errh);
        ss_dassert(errh->err_text != NULL);

        return(errh->err_text);
}

/*##**********************************************************************\
 * 
 *		su_err_geterrcode
 * 
 * 
 * 
 * Parameters : 
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
int su_err_geterrcode(
        su_err_t* errh
) {
        CHK_ERR(errh);

        return(errh->err_code);
}

/*##**********************************************************************\
 * 
 *		su_err_copyerrh
 * 
 * Duplicates an error handle
 * 
 * Parameters : 
 * 
 *	p_errh - out, give
 *		output error handle
 *		
 *	errh - in, use
 *		input error handle to be duplicated
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void su_err_copyerrh(
        su_err_t** p_errh,
        su_err_t* errh)
{
        if (p_errh != NULL && errh != NULL) {
            CHK_ERR(errh);
            *p_errh = err_alloc(errh->err_code, SsMemStrdup(errh->err_text));
            ss_pprintf_2(("su_err_copyerrh:%d: %d, %s\n", (int)*p_errh, (*p_errh)->err_code, (*p_errh)->err_text));
        }
}

#ifdef SS_DEBUG

/*##**********************************************************************\
 * 
 *		su_err_check
 * 
 * 
 * 
 * Parameters : 
 * 
 *	err - 
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
bool su_err_check(su_err_t* err)
{
        if (err == NULL) {
            return(FALSE);
        }
        if (err->err_chk != SUCHK_ERR) {
            ss_dprintf_1(("Error, su_err_t (%d) check failed, check %d\n", (int)err, err->err_chk));
            ss_dprintf_1(("Call location:\n"));
            ss_output_1(SsMemTrcPrintCallStk(NULL));
            return(FALSE);
        }
        return(TRUE);
}

#endif /* SS_DEBUG */
