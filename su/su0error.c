/*************************************************************************\
**  source       * su0error.c
**  directory    * su
**  description  * Error handling module for SOLID database
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

This module contains error code and message handling. Error texts are
stored into error text arrays of type su_rc_text_t. These arrays are
divided into different subsystems. The recognized subsystems are listed
in enum su_rc_subsys_t. By default (almost) all subsystem texts are empty.
Each subsystem must add own error texts to this error handler using
function su_rc_addsubsys. The error and message texts for different
subsystems are listed in files

        su0msgs.c
        dbe0erro.c
        tab1erro.c
        srv0erro.c

For other applications than SOLID (e.g. SOLID CSS), the application name
prefix printed in error text can be changed using function
su_rc_setapplication.

The text for error code can be received in two different formats. Function
su_rc_nameof gives the enum value in text format. It is mainly intended
for debugging and exceptional error situations. Function su_rc_givetext
creates the 'real' error text that can be displayed to the user. It
contains a more descriptive explanations of the error and also possible
parameter values that help to locate the source of error.


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

#include <ssstdio.h>
#include <ssstdlib.h>
#include <ssstdarg.h>
#include <ssstring.h>

#include <sssprint.h>
#include <ssdebug.h>
#include <ssmem.h>

#include "su0error.h"
#include "su0bsrch.h"

#define BUF_SIZE 8192

#ifndef SS_NOERRORTEXT

static bool su_msg_printmsgcode = FALSE;

#ifndef NO_ANSI
static int SS_CLIBCALLBACK rc_cmp(const void *key, const void *datum);
#else /* NO_ANSI */
static int SS_CLIBCALLBACK rc_cmp();
#endif /* NO_ANSI */

#endif /* SS_NOERRORTEXT */

typedef enum {
        SU_EMERGENCY_EXIT = 0,
        SU_INFORMATIVE_EXIT = 1
} su_politeexit_t;

#ifndef SS_MYSQL
static void add_header_text(
        su_politeexit_t exit_type,
        char* p
);
#endif

static void su_rc_vaddpolitetext(
        char* p_buf,
        su_ret_t rc,
        va_list arg_list
);
static void su_rc_vaddpolitetext_noargs(
        char* p_buf,
        su_ret_t rc
);
static void su_polite_exit(
        su_politeexit_t exit_type,
        char* file,
        int line,
        bool is_2rcexit,
        su_ret_t rc,
        su_ret_t opt_rc2,
        va_list ap
);
static char* su_emergency_assertheader = (char *)"Emergency Shutdown";
static char* su_informative_assertheader = (char *)"Automatic Shutdown";

static char* rc_application = (char *)"SOLID";

/*##**********************************************************************\
 *
 *		su_rc_getapplication
 *
 * Gets application name string used in error messages. The application
 * name is printed as the first part of the error error message. The
 * default application name is "SOLID".
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
char* su_rc_getapplication(
        void)
{
        return(rc_application);
}

/*##**********************************************************************\
 *
 *		su_rc_setapplication
 *
 * Sets application name string used in error messages. The application
 * name is printed as the first part of the error error message. The
 * default application name is "SOLID".
 *
 * Parameters :
 *
 *	application - in, hold
 *		Application name.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void su_rc_setapplication(char* application)
{
        ss_dassert(application != NULL);

        rc_application = application;
}

#ifndef SS_NOERRORTEXT

/*##**********************************************************************\
 *
 *		su_rc_addsubsys
 *
 * Adds error texts of a subsystem to the error message table.
 *
 * Parameters :
 *
 *	subsys - in
 *		Enum specifying the subsystem.
 *
 *	texts - in, hold
 *		Table of texts.
 *
 *	size - in
 *		Table size, i.e. number of texts in the table.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void su_rc_addsubsys(
        su_rc_subsys_t subsys,
        su_rc_text_t* texts,
        uint size)
{
        ss_dassert(subsys > SU_RC_SUBSYS_SU && subsys < SU_RC_NSUBSYS);

        if (rc_subsys[subsys].rcss_texts == NULL) {

            /* Ensure that texts are sorted by error code.
             */
            qsort(texts, size, sizeof(su_rc_text_t), rc_cmp);

            /* Add the subsystem texts to the table.
             */
            rc_subsys[subsys].rcss_size = size;
            rc_subsys[subsys].rcss_sorted = TRUE;
            rc_subsys[subsys].rcss_texts = texts;
        }
}

/*#***********************************************************************\
 *
 *		rc_cmp
 *
 * Comparison callback function for qsort() and bsearch().
 *
 * Input params :
 *
 *	key	    - in, use
 *          pointer to key value in bsearch() or
 *          left value in qsort()
 *
 *	datum	- in, use
 *          pointer to array value in bsearch() or
 *          right value in qsort()
 *
 * Return value :
 *      key->rct_rc - datum->rct_rc
 *
 * Limitations  :
 *
 * Globals used :
 */
static int SS_CLIBCALLBACK rc_cmp(key, datum)
    const void *key;
    const void *datum;
{
        return (int)((const su_rc_text_t*)key)->rct_rc -
               (int)((const su_rc_text_t*)datum)->rct_rc;
}

/*#***********************************************************************\
 *
 *		rc_findrctext
 *
 * Finds subsystem and text objects for a return code.
 *
 * Parameters :
 *
 *	rc - in
 *		Error text searched from error text tables.
 *
 *	p_subsys - out, ref
 *		If p_subsys is non-NULL and rc is found, pointer to the
 *          subsystem entry is stored into *p_subsys
 *
 *	p_rctext - out, ref
 *		If rc is found, pointer to text object is stored into *p_rctext.
 *		Otherwise NULL is stored into *p_rctext.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static bool rc_findrctext(
        su_ret_t rc,
        su_rc_subsysitem_t** p_subsys,
        su_rc_text_t** p_rctext)
{
        uint i;
        su_rc_text_t rctext;

        ss_dassert(p_rctext != NULL);

        for (i = 0; i < SU_RC_NSUBSYS; i++) {
            if (rc_subsys[i].rcss_texts != NULL) {
                if (!rc_subsys[i].rcss_sorted) {
                    qsort(
                        rc_subsys[i].rcss_texts,
                        rc_subsys[i].rcss_size,
                        sizeof(su_rc_text_t),
                        rc_cmp);
                    rc_subsys[i].rcss_sorted = TRUE;
                }
                rctext.rct_rc = rc;
                *p_rctext = bsearch(
                                &rctext,
                                rc_subsys[i].rcss_texts,
                                rc_subsys[i].rcss_size,
                                sizeof(su_rc_text_t),
                                rc_cmp);
                if (*p_rctext != NULL) {
                    /* Text found.
                     */
                    if (p_subsys != NULL) {
                        *p_subsys = &rc_subsys[i];
                    }
                    return(TRUE);
                }
            }
        }
        /* Text not found.
         */
        *p_rctext = NULL;
        return(FALSE);
}

/*#***********************************************************************\
 *
 *		rc_typename
 *
 * Returns the type name of an enum type value.
 *
 * Parameters :
 *
 *	type -
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
static char* rc_typename(su_rctype_t type)
{
        const char* typename;

        switch (type) {
            case SU_RCTYPE_RETCODE:
                typename = "Return Code";
                break;
            case SU_RCTYPE_WARNING:
                typename = "Warning";
                break;
            case SU_RCTYPE_ERROR:
                typename = "Error";
                break;
            case SU_RCTYPE_MSG:
                typename = "Message";
                break;
            case SU_RCTYPE_FATAL:
                typename = "Fatal Error";
                break;
            default:
                ss_rc_derror(type);
                typename = "Unknown type";
                break;
        }
        return((char *)typename);
}

/*#***********************************************************************\
 *
 *		allmessagelist_donefun
 *
 * Done function for all messages list.
 *
 * Parameters :
 *
 *		data -
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
static void allmessagelist_donefun(void* data)
{
        SsMemFree(data);
}

/*##**********************************************************************\
 *
 *		su_rc_getallmessages
 *
 * Returns all message codes, text and other info as a list. One list node
 * contains one message code with fields separated by comma.
 *
 * Parameters :
 *
 * Return value - give :
 *
 *      List of message info.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_list_t* su_rc_getallmessages(void)
{
        uint i;
        uint j;
        su_rc_text_t* rctexts;
        su_list_t* list;
        char* str;

        list = su_list_init(allmessagelist_donefun);

        su_list_insertlast(list, SsMemStrdup((char *)"Code,Class,Type,Text"));

        for (i = 0; i < SU_RC_NSUBSYS; i++) {
            if (rc_subsys[i].rcss_texts != NULL) {
                if (!rc_subsys[i].rcss_sorted) {
                    qsort(
                        rc_subsys[i].rcss_texts,
                        rc_subsys[i].rcss_size,
                        sizeof(su_rc_text_t),
                        rc_cmp);
                    rc_subsys[i].rcss_sorted = TRUE;
                }
                rctexts = rc_subsys[i].rcss_texts;
                for (j = 0; j < rc_subsys[i].rcss_size; j++) {
                    str = SsMemAlloc(
                            10 + 1 +
                            strlen(rc_subsys[i].rcss_subsysname) + 1 +
                            strlen(rc_typename(rctexts[j].rct_type)) + 1 +
                            (rctexts[j].rct_text == NULL ? 0 : strlen(rctexts[j].rct_text)) +
                            1);
                    SsSprintf(
                        str,
                        "%d,%s,%s,%s",
                        rctexts[j].rct_rc,
                        rc_subsys[i].rcss_subsysname,
                        rc_typename(rctexts[j].rct_type),
                        rctexts[j].rct_text == NULL ? "" : rctexts[j].rct_text);
                    su_list_insertlast(list, str);
                }
            }
        }
        return(list);
}


/*##**********************************************************************\
 *
 *		su_rc_classof
 *
 * Returns the class of an error code, e.g. "Utility"
 *
 * Parameters :
 *
 *	rc -
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
char* su_rc_classof(rc)
    su_ret_t rc;
{
        su_rc_subsysitem_t* subsys;
        su_rc_text_t* rctext;

        if (rc_findrctext(rc, &subsys, &rctext)) {
            return((char *)subsys->rcss_subsysname);
        } else {
            return((char *)"Unknown message number");
        }
}

/*##**********************************************************************\
 *
 *		su_rc_textof
 *
 * Returns the text of an error code, e.g. "File open failure".
 *
 * Note that the returned text may contain embedded printf-style
 * format characters like %s.
 *
 * Parameters :
 *
 *	rc -
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
char* su_rc_textof(rc)
    su_ret_t rc;
{
        su_rc_text_t* rctext;

        if (rc_findrctext(rc, NULL, &rctext)) {
            return((char *)rctext->rct_text);
        } else {
            ss_info_dassert(0, ("Unknown message number: %d", (int) rc));
            return((char *)"Unknown message number");
        }
}

/*##**********************************************************************\
 *
 *		su_rc_typeof
 *
 * Returns the type of an error code, e.g. "Error".
 *
 * Parameters :
 *
 *	rc -
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
char* su_rc_typeof(rc)
    su_ret_t rc;
{
        su_rc_text_t* rctext;

        if (rc_findrctext(rc, NULL, &rctext)) {
            return(rc_typename(rctext->rct_type));
        } else {
            return((char *)"Unknown message number");
        }
}

#else /* SS_NOERRORTEXT */

#define rc_findrctext(rc, p1, p2) (TRUE)

#endif /* SS_NOERRORTEXT */

/*##**********************************************************************\
 *
 *		su_rc_nameof
 *
 * Gets name for return code value. For example error code
 * DBE_ERR_LOSTUPDATE returns string "DBE_ERR_LOSTUPDATE".
 *
 * Parameters :
 *
 *	rc	- in
 *          return code
 *
 * Return value - ref :
 *      pointer to name or constant error text if name not found
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_rc_nameof(rc)
    su_ret_t rc;
{
#ifndef SS_NOERRORTEXT
        su_rc_text_t* rctext;

        if (rc_findrctext(rc, NULL, &rctext)) {
            return((char *)rctext->rct_enumname);
        } else {
            return((char *)"Unknown message number");
        }
#else /* SS_NOERRORTEXT */
        return("UNKNOWN_ERROR_TEXT");
#endif /* SS_NOERRORTEXT */
}


/*##**********************************************************************\
 *
 *		su_rc_buildtext_bycomponent
 *
 * Builds error text from subcomponentstrings.
 *
 * Parameters :
 *
 *	application -
 *
 *
 *	subsysname -
 *
 *
 *	typename -
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
char* su_rc_buildtext_bycomponent(
        su_rctype_t type,
        char* application,
        char* subsysname,
        char* type_name,
        int code,
        char* text)
{
        char* buf;

        if(type != SU_RCTYPE_MSG){
            buf = SsMemAlloc(
                    strlen(application) + 1 +
                    strlen(subsysname) + 1 +
                    strlen(type_name) + 1 +
                    10 + 2 +
                    strlen(text) +
                    1);

            SsSprintf(buf, "%s %s %s %d: %s",
                application, subsysname, type_name, code, text);
        } else if(su_msg_printmsgcode) {
            buf = SsMemAlloc(
                    strlen(subsysname) + 5 +
                    1 + strlen(text) +
                    1+2);

            SsSprintf(buf, "<%s%d> %s",
                 subsysname, code, text);
        } else {
            buf = SsMemAlloc(
                    strlen(text) +
                    1);
            SsSprintf(buf, "%s", text);
        }
        return(buf);
}

/*##**********************************************************************\
 *
 *		su_rc_buildtext
 *
 * Builds error text from subcomponents.
 *
 * Parameters :
 *
 *	subsys - in
 *		Enum specifying the subsystem.
 *
 *	type - in
 *		Enum specifying text type.
 *
 *	code - in
 *		Error or warning code.
 *
 *	text - in
 *		Actual error or warning text.
 *
 * Return value - give :
 *
 *      Returns error text allocated using SsMemAlloc. The user is
 *      responsible for releasing is using SsMemFree.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_rc_buildtext(
        su_rc_subsys_t subsys,
        su_rctype_t type,
        int code,
        char* text)
{
#ifndef SS_NOERRORTEXT
        char* buf;
        char* subsysname;
        char* typename;

        ss_dassert((int)subsys >= SU_RC_SUBSYS_SU && subsys < SU_RC_NSUBSYS);
        ss_dassert(text != NULL);

        subsysname = (char *)rc_subsys[subsys].rcss_subsysname;

        typename = rc_typename(type);

        buf = su_rc_buildtext_bycomponent(
                    type,
                    rc_application,
                    subsysname,
                    typename,
                    code,
                    text);
        return(buf);

#else /* SS_NOERRORTEXT */
        char* buf;

        buf = SsMemAlloc(30);
        SsSprintf(buf, "Solid error %d", code);
        return(buf);
#endif /* SS_NOERRORTEXT */
}

/*##**********************************************************************\
 *
 *		su_rc_vgivetext
 *
 * Allocates and generates an error string. For example error code
 * E_ILLCONST_S returns string
 *
 *      "SOLID Table Error 13035: Illegal constant xyz"
 *
 * Parameters :
 *
 *	rc	- in
 *          return code
 *
 *	arg_list - in
 *		Pointer to variable number of arguments to the error string.
 *
 * Return value - give :
 *
 *      Pointer to a string that contains the error text. The string
 *      is allocated using SsMemAlloc, and the user is responsible for
 *      releasing the string with SsMemFree.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_rc_vgivetext(
        su_ret_t rc,
        va_list arg_list)
{
        return su_rc_vgiveerrtext(rc, arg_list, -1);
}

char* su_rc_vgiveerrtext(
        su_ret_t rc,
        va_list arg_list,
        int msgtype)
{
        char buf[BUF_SIZE];
        char* retbuf;
        su_rc_subsysitem_t* subsys;
        su_rc_text_t* rctext;

        buf[BUF_SIZE - 1] = '\0';
        if (!rc_findrctext(rc, &subsys, &rctext)) {
            SsSprintf(buf, "%s Internal Error: Unknown error code %d", rc_application, rc);
            retbuf = SsMemStrdup(buf);
        } else {
            SsVsprintf(buf, (char *)rctext->rct_text, arg_list);
            ss_dassert(buf[BUF_SIZE - 1] == '\0');

            retbuf = su_rc_buildtext(
                        subsys->rcss_subsyscode,
                        msgtype==-1 ? (int)rctext->rct_type: msgtype,
                        rc,
                        buf);
        }

        return(retbuf);
}

/*##**********************************************************************\
 *
 *		su_rc_givetext_noargs
 *
 * Gives text by rc only without any %[sdfx] arguments expanded
 *
 * Parameters :
 *
 *	rc - in
 *		return code
 *
 * Return value - give:
 *
 *      Pointer to a string that contains the error text. The string
 *      is allocated using SsMemAlloc, and the user is responsible for
 *      releasing the string with SsMemFree.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_rc_givetext_noargs(su_ret_t rc)
{
        char buf[BUF_SIZE];
        char* retbuf;
        su_rc_subsysitem_t* subsys;
        su_rc_text_t* rctext;

        buf[BUF_SIZE - 1] = '\0';

        if (!rc_findrctext(rc, &subsys, &rctext)) {

            SsSprintf(buf, "%s Internal Error: Unknown error code %d", rc_application, rc);
            ss_dassert(buf[BUF_SIZE - 1] == '\0');
            retbuf = SsMemStrdup(buf);

        } else {
            retbuf = su_rc_buildtext(
                        subsys->rcss_subsyscode,
                        rctext->rct_type,
                        rc,
                        (char *)rctext->rct_text);
        }

        return(retbuf);

}
/*##**********************************************************************\
 *
 *		su_rc_givetext
 *
 *
 *
 * Parameters :
 *
 *      rc - in
 *
 *
 *      ... - in
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
char* SS_CDECL su_rc_givetext(
        su_ret_t rc,
        ...)
{
        char* p;
        va_list ap;

        va_start(ap, rc);

        p = su_rc_vgivetext(rc, ap);

        va_end(ap);

        return(p);
}

/*##**********************************************************************\
 *
 *              su_rc_skipheader
 *
 * Skip to the start of the error message text.
 *
 * Parameters :
 *
 *      text - text from su_rc_buildtext
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* su_rc_skipheader(char* text)
{
     char *str = text;

     while (*str != '\0' && *str != ':') {
         str++;
     }

     /* no colon found, return the original string */
     if (*str == '\0') {
         return text;
     }

     /* skip the colon */
     str++;

     /* skip the space before the colon too */
     if (*str == ' ') {
         str++;
     }
     return str;
}

/*##**********************************************************************\
 *
 *		su_rc_assertionfailure
 *
 * Assertion message print and program abort function.
 *
 * Parameters :
 *
 *	file - in, use
 *		file name
 *
 *	line - in
 *		line number
 *
 *	expr - in
 *		evaluated expr as string or NULL if not available
 *
 *	rc - in
 *		return code
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int su_rc_assertionfailure(file, line, expr, rc)
    char *file;
    int line;
    char *expr;
    su_ret_t rc;
{
        char buf[BUF_SIZE];

        SS_NOTUSED(expr);

        buf[BUF_SIZE - 1] = '\0';

        SsSprintf(
            buf,
"Status: %d@%s\n\
Code: %d (%s)\n",
            line,
            file,
            rc,
            su_rc_nameof(rc));

        ss_dassert(buf[BUF_SIZE - 1] == '\0');
        SsAssertionFailureText(buf, NULL, 0);

        return(0);
}


/*##**********************************************************************\
 *
 *		su_fatal_error_v
 *
 * Prints a message about fatal system error and aborts program.
 *
 * Parameters :
 *
 *	msg - in
 *		Format string for error message
 *
 *	ap -
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
void su_fatal_error_v(char* msg, va_list ap)
{
        char buf[BUF_SIZE];
        char* p_buf;

        buf[BUF_SIZE - 1] = '\0';

        p_buf = buf;

        SsSprintf(
            p_buf,
            "Fatal system error, program aborted.\nProduct: %s\nVersion: %s\nOperating system: %s\nMessage: ",
            SS_SERVER_NAME,
            SS_SERVER_VERSION,
            SsEnvNameCurr());

        ss_dassert(buf[BUF_SIZE - 1] == '\0');
        p_buf += strlen(p_buf);

        SsVsprintf(p_buf, msg, ap);
        va_end(ap);

        ss_dassert(buf[BUF_SIZE - 1] == '\0');
        if (buf[strlen(buf) - 1] != '\n') {
            strcat(buf, "\n");
        }
        ss_dassert(buf[BUF_SIZE - 1] == '\0');
        SsAssertionExit(buf);
}

/*##**********************************************************************\
 *
 *		su_rc_fatal_error_v
 *
 * Prints a message about fatal system error and aborts program.
 *
 * Parameters :
 *
 *	rc - in
 *		fatal error code
 *
 *	ap - in, use
 *		pointer to argument list
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void su_rc_fatal_error_v(su_ret_t rc, va_list ap)
{
        char* errtext;

        errtext = su_rc_vgivetext(rc, ap);
        SsLogErrorMessage(errtext);
        errtext = su_rc_givetext(SRV_FATAL_ABORT);
        errtext = SsMemRealloc(errtext, strlen(errtext)+2);
        strcat(errtext, "\n");
        SsAssertionExit(errtext);
}

/*##**********************************************************************\
 *
 *		su_rc_fatal_error
 *
 * Prints a message about fatal system error and aborts program.
 *
 * Parameters :
 *
 *      rc - in
 *          error code
 *
 *      ... - in
 *          possible arguments for the error
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_rc_fatal_error(su_ret_t rc, ...)
{
        va_list ap;

        va_start(ap, rc);
        su_rc_fatal_error_v(rc, ap);
}

/*##**********************************************************************\
 *
 *		su_fatal_error
 *
 * Prints a message about fatal system error and aborts program.
 *
 * Parameters :
 *      msg - in
 *          Format string for error message as with printf()
 *
 *      ... - in
 *          0 or more arguments matching the msg format
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_fatal_error(char* msg, ...)
{
        va_list ap;

        va_start(ap, msg);
        su_fatal_error_v(msg, ap);
}

#ifndef SS_MYSQL
static void add_header_text(su_politeexit_t exit_type, char* p)
{
        switch (exit_type) {

            case SU_EMERGENCY_EXIT:
                strcat(p,
"To prevent any data loss Solid Database Engine process executed an emergency shutdown.\n");
                break;

            case SU_INFORMATIVE_EXIT:
                strcat(p,
"Solid Database Engine process executed an emergency shutdown.\n");
                break;

            default:
                ss_derror;
                break;
        }
}
#endif /* !SS_MYSQL */

static void su_polite_exit(
            su_politeexit_t exit_type,
            char* file,
            int line,
            bool is_2rcexit,
            su_ret_t rc,
            su_ret_t rc2,
            va_list ap
)
{
        char buf[2048];
        char dt[20];
        char* p_buf;

        strcpy(buf, "");
        p_buf = buf;

        su_rc_vaddpolitetext(p_buf, rc, ap);
        p_buf += strlen(p_buf);
        strcat(buf, "\n");
        p_buf++;
        va_end(ap);

        if (is_2rcexit) {
            su_rc_vaddpolitetext_noargs(p_buf, rc2);
            strcat(buf, "\n");
            p_buf++;
        }

        if (buf[strlen(buf) - 1] != '\n') {
            strcat(buf, "\n");
        }
        p_buf += strlen(p_buf);

        SsPrintDateTime(dt, 20, SsTime(NULL));

        switch (exit_type) {
            case SU_INFORMATIVE_EXIT:
                SsSprintf(p_buf, "\nDate: %s\nProduct: %s\nVersion: %s\nOperating system: %s\n",
                    dt,
                    SS_SERVER_NAME,
                    SS_SERVER_VERSION,
                    SsEnvNameCurr());
                break;

            case SU_EMERGENCY_EXIT:
                SsSprintf(p_buf, "\nDate: %s\nProduct: %s\nVersion: %s\nOperating system: %s\nStatus: %d@%s\n",
                    dt,
                    SS_SERVER_NAME,
                    SS_SERVER_VERSION,
                    SsEnvNameCurr(),
                    line,
                    file);
                break;

            default:
                ss_derror;
                break;
        }
        p_buf += strlen(p_buf);

        switch (exit_type) {
            case SU_INFORMATIVE_EXIT:
                    SsSetAssertMessageHeader(su_informative_assertheader);
                break;

            case SU_EMERGENCY_EXIT:
                    SsSetAssertMessageHeader(su_emergency_assertheader);
                break;

            default:
                ss_derror;
                break;
        }
        strcat(buf, "\n");
        p_buf += strlen(p_buf);
        SsPrintDateTime(p_buf, 20, SsTime(NULL));
        strcat(p_buf, " ");
        p_buf += strlen(p_buf);
        su_rc_vaddpolitetext_noargs(p_buf, SRV_FATAL_ABORT);
        strcat(p_buf, "\n");
        SsAssertionExit(buf);
}

static void su_rc_vaddpolitetext(
        char* p_buf,
        su_ret_t rc,
        va_list arg_list)
{
        su_rc_subsysitem_t* subsys;
        su_rc_text_t* rctext;
        char *txt;

        if (!rc_findrctext(rc, &subsys, &rctext)) {
            SsSprintf(p_buf, "Exit message not available\n");
        } else {
            SsVsprintf(p_buf, (char *)rctext->rct_text, arg_list);
        }

        txt = su_rc_buildtext_bycomponent(
                SU_RCTYPE_MSG, (char *)"", (char *)"SRV", (char *)"Fatal error", rc, p_buf
        );
        strcpy(p_buf, txt);
        SsMemFree(txt);
}

static void su_rc_vaddpolitetext_noargs(char* p_buf, su_ret_t rc)
{
        su_rc_subsysitem_t* subsys;
        su_rc_text_t* rctext;
        char *txt;

        if (!rc_findrctext(rc, &subsys, &rctext)) {
            SsSprintf(p_buf, "Exit message not available\n");
        } else {
            SsSprintf(p_buf, rctext->rct_text);
        }
        txt = su_rc_buildtext_bycomponent(
                SU_RCTYPE_MSG, (char *)"", (char *)"SRV", (char *)"Fatal error", rc, p_buf
        );
        strcpy(p_buf, txt);
        SsMemFree(txt);
}

void SS_CDECL su_rc_adderrortext(const char* buf, su_ret_t rc, ...)
{
        va_list ap;
        va_start(ap, rc);
        su_rc_vaddpolitetext((char *)buf, rc, ap);
        va_end(ap);
}

/*##**********************************************************************\
 *
 *		su_emergency_exit
 *
 * Prints 'emergency exit' error message and aborts program.
 *
 * Parameters :
 *
 *      file - in
 *
 *      line - in
 *
 *	rc - in
 *		fatal error code
 *
 *      ... - in
 *          0 or more arguments matching the msg format
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_emergency_exit(const char* file, int line, su_ret_t rc, ...)
{
        va_list ap;

        va_start(ap, rc);

        su_polite_exit(SU_EMERGENCY_EXIT, (char *)file, line, FALSE, rc, 0, ap);
}

/*##**********************************************************************\
 *
 *		su_informative_exit
 *
 * Prints 'informative exit' message and aborts program.
 *
 *
 * Parameters :
 *
 *      file - in
 *
 *      line - in
 *
 *	rc - in
 *		fatal error code
 *
 *      ... - in
 *          0 or more arguments matching the msg format
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_informative_exit(const char* file, int line, su_ret_t rc, ...)
{
        va_list ap;

        va_start(ap, rc);

        su_polite_exit(SU_INFORMATIVE_EXIT, (char *)file, line, FALSE, rc, 0, ap);
}

/*##**********************************************************************\
 *
 *		su_emergency2rc_exit
 *
 * Prints 'emergency exit' error message and aborts program. Message
 * is constructed from two parts: general header error message (rc)
 * and more accurate second error message. For example general error
 * message could be 'Roll forward recovery failed...' and more precise
 * message 'File open error' or 'Logfiles corrupted' etc.
 *
 *
 * Parameters :
 *
 *      file - in
 *
 *      line - in
 *
 *	rc - in
 *		general error code
 *
 *	rc2 - in
 *		more precise error code. NOTE: rc2 must NOT take
 *          any arguments.
 *
 *      ... - in
 *          0 or more arguments matching rc's error format
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_emergency2rc_exit(
                const char* file,
                int line,
                su_ret_t rc,
                su_ret_t rc2,
                ...
)
{
        va_list ap;

        va_start(ap, rc2);

        su_polite_exit(SU_EMERGENCY_EXIT, (char *)file, line, TRUE, rc, rc2, ap);
}

/*##**********************************************************************\
 *
 *		su_informative2rc_exit
 *
 * Prints 'informative exit' error message and aborts program. Message
 * is constructed from two parts: general header error message (rc)
 * and more accurate second error message. For example general error
 * message could be 'Roll forward recovery failed' and more precise
 * message 'File open error' or 'Logfiles corrupted' etc.
 *
 *
 * Parameters :
 *
 *      file - in
 *
 *      line - in
 *
 *	rc - in
 *		general error code
 *
 *	rc2 - in
 *		more precise error code. NOTE: rc2 must NOT take
 *          any arguments.
 *
 *      ... - in
 *          0 or more arguments matching rc's error format
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void SS_CDECL su_informative2rc_exit(
                const char* file,
                int line,
                su_ret_t rc,
                su_ret_t rc2,
                ...
)
{
        va_list ap;

        va_start(ap, rc2);

        su_polite_exit(SU_INFORMATIVE_EXIT, (char *)file, line, TRUE, rc, rc2, ap);
}

void su_setprintmsgcode(bool p)
{
        su_msg_printmsgcode = p;
}
