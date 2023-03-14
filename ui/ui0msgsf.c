/*************************************************************************\
**  source       * ui0msgsf.c
**  directory    * ui
**  description  * Stub message functions.
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

#include <ssenv.h>

#define SS_WINNT_NEEDSGDI
#include <sswindow.h>

#include <ssstdlib.h>
#include <ssstdio.h>
#include <ssconio.h>
#include <ssstring.h>
#include <ssctype.h>
#include <sschcvt.h>

#include <ssc.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <sssprint.h>
#include <sstime.h>
#include <sswinint.h>
#include <ssthread.h>
#include <ssservic.h>
#include <ssutf.h>

#include <su0sdefs.h>
#include <su0types.h>
#include <su0error.h>

#include "ui1msg.h"
#include "ui0msg.h"

#ifdef SS_UNIX

#include <signal.h>

#include <termios.h>

#if defined(SS_LINUX) && !defined(SS_MT) /* == SS_LUX */ 
#include <unistd.h>
#endif

#endif /* SS_UNIX */

/* If non-null, username and password given in ui_msg_setdba. */
static char* msg_dbauname = NULL;
static char* msg_dbapassw = NULL;

static char* msg_defcatalog = NULL;

static void (*ui_msg_fp)(ui_msgtype_t type, su_msgret_t msgcode, char* msg, bool newline);

#ifdef SS_UNICODE_SQL

#define TMP_INPUTBUFFER_SIZE(bufsize) ((bufsize) / 2)

#ifndef SS_MYSQL
static void pwd_or_usrname_ncpy(ss_char1_t*p_dest, ss_char1_t*p_src, size_t n)
{
        size_t i;
        SsUtfRetT utfrc;
        
        ss_byte_t* p_dest_tmp;
        ss_char1_t* p_src_tmp;

        for (i = 0; ; i++) {
            ss_rc_dassert(i < n, i);
            if (p_src[i] == '\0') {
                break;
            }
        }
        p_dest_tmp = (ss_byte_t*)p_dest;
        p_src_tmp = p_src;
        utfrc = SsASCII8toUTF8(&p_dest_tmp, p_dest_tmp + n,
                               &p_src_tmp, p_src_tmp + i + 1);
        ss_rc_dassert(utfrc == SS_UTF_OK || utfrc == SS_UTF_NOCHANGE, utfrc);
}
#endif /* !SS_MYSQL */

#else /* SS_UNICODE_SQL */

#define TMP_INPUTBUFFER_SIZE(bufsize) (bufsize)
    
#define  pwd_or_usrname_ncpy strncpy

#endif /* SS_UNICODE_SQL */

        
/*##**********************************************************************\
 * 
 *		ui_msg_setdba
 * 
 * Sets username and password used in database create.
 * Used only during testing. If this function is called,
 * ui_msg_getdba returns username and password given here.
 * 
 * Parameters : 
 * 
 *	username - 
 *		
 *		
 *	password - 
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
void ui_msg_setdba(char* username, char* password)
{
        msg_dbauname = username;
        msg_dbapassw = password;
}

void ui_msg_getdefdba(
        char** p_username,
        char** p_password)
{
        ss_dassert(p_username != NULL);
        ss_dassert(p_password != NULL);

        *p_username = msg_dbauname;
        *p_password = msg_dbapassw;
}

void ui_msg_setdefcatalog(char* catalog)
{
        ss_dprintf_1(("ui_msg_setdefcatalog: %s\n", catalog != NULL ? catalog : "NULL"))
        msg_defcatalog = catalog;
}

char* ui_msg_getdefcatalog(void)
{
        ss_dprintf_1(("ui_msg_getdefcatalog: %s\n", msg_defcatalog != NULL ? msg_defcatalog : "NULL"))
        return(msg_defcatalog);
}

/*##**********************************************************************\
 * 
 *		ui_msg_setmessagefp
 * 
 * 
 * 
 * Parameters :
 * 
 *      message_fp - in, hold
 *          Function pointer used for message output.
 * 
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void ui_msg_setmessagefp(void (*message_fp)(ui_msgtype_t type, su_msgret_t msgcode, char* msg, bool newline))
{
        ui_msg_fp = message_fp;
}


/* ================================== */
#if defined(SS_NT) || defined(SS_DOS)
/* ================================== */

#if defined(GCC)
#define putch(c) { putchar(c); SsFFlush(SsStdout); }
#endif

#define console_putch(_ch)  putch(_ch)


#endif /* defined(SS_NT) || defined(SS_DOS) */


#if defined(SS_NTGUI)

/*##**********************************************************************\
 * 
 *		ui_msg_message
 * 
 * 
 * 
 * Parameters : 
 * 
 *	msg - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SS_CDECL ui_msg_message(int msgcode, ...)
{
        va_list ap;
        char* msg;
        
        if(msgcode != 0){
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number"));
            va_start(ap, msgcode);
            msg = su_rc_vgivetext(msgcode, ap);
            va_end(ap);
        } else {
            va_start(ap, msgcode);
            msg = va_arg(ap, char*);
            va_end(ap);
        }
        if (ui_msg_fp != NULL) {
            (*ui_msg_fp)(UI_MSG_MESSAGE, msgcode, msg, TRUE);
        } else {
            SsPrintf("%s\n", msg);
        }
        if(msgcode != 0) {
            SsMemFree(msg);
        }
}

#endif

void SS_CDECL ui_msg_message_status(int msgcode, ...)
{
        SS_NOTUSED(msgcode);
}


/*##**********************************************************************\
 * 
 *		ui_msg_messagebox
 * 
 * 
 * 
 * Parameters : 
 * 
 *	header - 
 *		
 *		
 *	msg - 
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
void SS_CDECL ui_msg_messagebox(char* header, int msgcode, ...)
{
        SS_NOTUSED(header);
        SS_NOTUSED(msgcode);
}

/*##**********************************************************************\
 * 
 *		ui_msg_warning
 * 
 * 
 * 
 * Parameters : 
 * 
 *	msg - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SS_CDECL ui_msg_warning(int msgcode, ...)
{
        va_list ap;
        char* msg;
        
        if(msgcode != 0){
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number"));
            va_start(ap, msgcode);
            msg = su_rc_vgivetext(msgcode, ap);
            va_end(ap);
        } else {
            va_start(ap, msgcode);
            msg = va_arg(ap, char*);
            va_end(ap);
        }

        ss_dprintf_1(("ui_msg_warning\n"));

        if (ui_msg_fp != NULL) {
            (*ui_msg_fp)(UI_MSG_WARNING, msgcode, msg, TRUE);
        }
        if(msgcode != 0) {
            SsMemFree(msg);
        }
}

/*##**********************************************************************\
 * 
 *		ui_msg_error
 * 
 * 
 * 
 * Parameters : 
 * 
 *	msg - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SS_CDECL ui_msg_error(int msgcode, ...)
{
        va_list ap;
        char* msg;
        
        if(msgcode != 0){
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number"));
            va_start(ap, msgcode);
            msg = su_rc_vgiveerrtext(msgcode, ap, UI_MSG_ERROR);
            va_end(ap);
        } else {
            va_start(ap, msgcode);
            msg = va_arg(ap, char*);
            va_end(ap);
        }
        SsLogErrorMessage(msg);

        SS_NOTUSED(msgcode);
        if(msgcode != 0){
            SsMemFree(msg);
        }
}

/*##**********************************************************************\
 * 
 *		ui_msg_error_nostop
 * 
 * Error message function that does not stop the server.
 * 
 * Parameters : 
 * 
 *	msg - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void SS_CDECL ui_msg_error_nostop(int msgcode, ...)
{
        va_list ap;
        char* msg;
        
        if(msgcode != 0){
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number"));
            va_start(ap, msgcode);
            msg = su_rc_vgivetext(msgcode, ap);
            va_end(ap);
        } else {
            va_start(ap, msgcode);
            msg = va_arg(ap, char*);
            va_end(ap);
        }
        ui_msg_error(0, msg);
        if(msgcode != 0){
            SsMemFree(msg);
        }
}


bool ui_msg_confirmshutdown(
        char* username __attribute__ ((unused)),
        size_t username_size __attribute__ ((unused)),
        char* password __attribute__ ((unused)),
        size_t password_size __attribute__ ((unused)))
{
        return(FALSE);
}

/*##**********************************************************************\
 * 
 *		ui_msg_getuser
 * 
 * Asks username and password. Used e.g. from remote console.
 * 
 * Parameters : 
 * 
 *	username - 
 *		
 *		
 *	username_size - 
 *		
 *		
 *	password - 
 *		
 *		
 *	password_size - 
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
void ui_msg_getuser(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size)
{
        SS_NOTUSED(username);
        SS_NOTUSED(username_size);
        SS_NOTUSED(password);
        SS_NOTUSED(password_size);
}

#ifdef SS_UNICODE_CLI

void ui_msg_getuser_UTF8(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size)
{
        SS_NOTUSED(username);
        SS_NOTUSED(username_size);
        SS_NOTUSED(password);
        SS_NOTUSED(password_size);
}

#endif /* SS_UNICODE_CLI */

/*##**********************************************************************\
 * 
 *		ui_msg_sqlwarning
 * 
 * 
 * 
 * Parameters : 
 * 
 *	code - 
 *		
 *		
 *	str - 
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
void ui_msg_sqlwarning(uint code, char* str)
{
        SS_NOTUSED(code);
        SS_NOTUSED(str);
}


/*##**********************************************************************\
 * 
 *		ui_msg_message_nogui
 * 
 * 
 * 
 * Parameters : 
 * 
 *	msg - 
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
void SS_CDECL ui_msg_message_nogui(int msgcode, ...)
{
        va_list ap;
        char* msg;

        if(msgcode != 0){
            ss_dassert(strcmp(su_rc_typeof(msgcode), "Unknown message number"));
            va_start(ap, msgcode);
            msg = su_rc_vgivetext(msgcode, ap);
            va_end(ap);
        } else {
            va_start(ap, msgcode);
            msg = va_arg(ap, char*);
            va_end(ap);
        }
        if (ui_msg_fp != NULL) {
            (*ui_msg_fp)(UI_MSG_MESSAGE, msgcode, msg, TRUE);
        } else {
            if (msg[strlen(msg)-1] == '\n') {
                SsPrintf("%s", msg);
            } else {
                SsPrintf("%s\n", msg);
            }
        }
        if(msgcode != 0) {
            SsMemFree(msg);
        }
}

/* buffers for dialog's data. inputfields are max 40 chars currently
 */
#ifndef SS_MYSQL
static char ui_CatalogName[SU_MAXNAMELEN+1];
static char ui_UserName[SU_MAXNAMELEN+1];
static char ui_PassWord[SU_MAXNAMELEN+1];
static char ui_RetypedPassWord[SU_MAXNAMELEN+1];
#endif

/*##**********************************************************************\
 * 
 *		ui_msg_getdba
 * 
 * 
 * 
 * Parameters : 
 * 
 *	username - 
 *		
 *		
 *	username_size - 
 *		
 *		
 *	password - 
 *		
 *		
 *	password_size - 
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
bool ui_msg_getdba(
        char* username,
        size_t username_size,
        char* password,
        size_t password_size, 
        char* catalog,
        size_t catalog_size
    )
{
        ss_dprintf_1(("ui_msg_getdba\n"));

        if (msg_dbauname != NULL && msg_dbapassw != NULL &&  msg_defcatalog != NULL) {
            
            /* ui_msg_setdba called */
            strncpy(username, msg_dbauname, username_size);
            strncpy(password, msg_dbapassw, password_size);
            strncpy(catalog,  msg_defcatalog, catalog_size);
        } else {
                return(FALSE);
        }
        return(TRUE);
}

/*##**********************************************************************\
 * 
 *		ui_msg_fgetc
 * 
 * 
 * 
 * Parameters : 
 * 
 *	fp - 
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
int ui_msg_fgetc(SS_FILE* fp)
{
        return(SsFGetc(fp));
}

/*##**********************************************************************\
 * 
 *		ui_msg_generate_error
 * 
 * 
 * 
 * Parameters : 
 * 
 *	msg - 
 *		
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void ui_msg_generate_error(const char* msg)
{
        ui_msg_error(0, msg);
}


