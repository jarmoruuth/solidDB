/*************************************************************************\
**  source       * rs0sdefs.c
**  directory    * res
**  description  * Utilities for system definitions.
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

#include <ssstdio.h>
#include <ssstring.h>
#include <ssctype.h>

#include <ssc.h>
#include <ssmem.h>
#include <sssprint.h>
#include <su0chcvt.h>

#include "rs0sdefs.h"

static char* sdefs_new_defcatalog = NULL;
static char* sdefs_current_defcatalog = NULL;

char* rs_sdefs_getnewdefcatalog(void)
{
        return (sdefs_new_defcatalog);
}

char* rs_sdefs_getcurrentdefcatalog(void)
{
        return (sdefs_current_defcatalog);
}

static char* sdefs_uprdup(char* src)
{
        char* dst;
        size_t len;
        size_t i;

        if (src == NULL) {
            return (NULL);
        }
        len = strlen(src);
        dst = SsMemAlloc(len + 1);
        for (i = 0; i <= len; i++) {
            ss_byte_t c = (ss_byte_t)src[i];
            if (SU_SQLISLOWER(c)) {
                c -= 'a' - 'A';
            }
            dst[i] = (char)c;
        }
        return (dst);
}

void rs_sdefs_setnewdefcatalog(char* defcatalog)
{
        ss_dassert(defcatalog != NULL);

        if (sdefs_new_defcatalog != NULL) {
            SsMemFree(sdefs_new_defcatalog);
        }
        sdefs_new_defcatalog = sdefs_uprdup(defcatalog);
}

void rs_sdefs_setcurrentdefcatalog(char* defcatalog)
{
        if (sdefs_current_defcatalog != NULL) {
            SsMemFree(sdefs_current_defcatalog);
        }
        sdefs_current_defcatalog = sdefs_uprdup(defcatalog);
}

void rs_sdefs_globaldone(void)
{
        if (sdefs_current_defcatalog != NULL) {
            SsMemFree(sdefs_current_defcatalog);
            sdefs_current_defcatalog = NULL;
        }
        if (sdefs_new_defcatalog != NULL) {
            SsMemFree(sdefs_new_defcatalog);
            sdefs_new_defcatalog = NULL;
        }
}

/*##**********************************************************************\
 *
 *              rs_sdefs_sysaname
 *
 *
 *
 * Parameters :
 *
 *      aname -
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
bool rs_sdefs_sysaname(char* aname)
{
        int i;
        static const char* sysanames[] = {
            RS_ANAME_TUPLE_ID,
            RS_ANAME_TUPLE_VERSION,
#ifndef SS_MYSQL
            RS_PNAME_ROWID,
            RS_PNAME_ROWVER,
            RS_PNAME_ROWFLAGS,
#endif /* SS_MYSQL */
            NULL
        };


        for (i = 0; sysanames[i] != NULL; i++) {
            if (strcmp(sysanames[i], aname) == 0) {
                return(TRUE);
            }
        }
        return(FALSE);
}

bool rs_sdefs_sysparam(char* pname)
{
#ifndef SS_MYSQL
        int i;
        static const char* syspnames[] = {
            RS_BBOARD_PAR_NODE_NAME,
            RS_BBOARD_PAR_TRANID,
            RS_BBOARD_PAR_TRANUSERID,
            RS_BBOARD_PAR_ISPROPAGATE,
            RS_BBOARD_PAR_ISSUBSCRIBE,
            RS_BBOARD_PAR_SYNC_OPERATION_TYPE,
            RS_BBOARD_PAR_SYNC_RESULTSET_TYPE,
            RS_BBOARD_PAR_SYNCMASTER,
            RS_BBOARD_PAR_SYNCREPLICA,
            NULL
        };

        for (i = 0; syspnames[i] != NULL; i++) {
            if (strcmp(syspnames[i], pname) == 0) {
                return(TRUE);
            }
        }
#endif /* SS_MYSQL */
        return(FALSE);
}


#ifdef SS_SYNC

/*##**********************************************************************\
 *
 *              rs_sdefs_buildsynchistorytablename
 *
 * Builds sync history table name from base table name.
 *
 * Parameters :
 *
 *      relname - in
 *              Base table name
 *
 * Return value - give :
 *
 *      Sync history table name.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* rs_sdefs_buildsynchistorytablename(
        char* relname)
{
        char* p;

        p = SsMemAlloc(strlen(RSK_SYNCHIST_TABLENAMESTR) + strlen(relname) + 1);
        SsSprintf(p, RSK_SYNCHIST_TABLENAMESTR, relname);
        return(p);
}

#endif /* SS_SYNC */
