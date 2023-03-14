/*************************************************************************\
**  source       * ssfncnv.c
**  directory    * ss
**  description  * File name path conversion routines
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


#include "ssenv.h"
#include "ssc.h"
#include "ssstring.h"
#include "ssstddef.h"
#include "ssfncnv.h"
#include "ssdebug.h"
#include "sssprint.h"


/* General form */
#define GEN_PATH_SEPARATOR  '/'
#define GEN_DRIVE_SEPARATOR ':'


#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT) 
    static bool dos_path(char* buf, char* path);
#endif

#ifdef SS_UNIX
static bool unix_path(char* buf, char* path);
#endif

static void make_general_path(
            char* buf,
            char* dir,
            char* fname,
            int separate_fname);

static void convert_path_separators(char* path);


/*#***********************************************************************\
 * 
 *		convert_path_separators
 * 
 * Convert all possible path separators to '/' characters 
 * 
 * dots between <> and [] are treated as path separators
 * 
 * Parameters : 
 * 
 *	path - in out, use
 *		path to be converted, perform an in-place conversion
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 *      This implementation does not check the validity of given path
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void convert_path_separators(char* path)
{
        char* ptr;

        while ((ptr = strchr(path, '\\')) != NULL) {
            *ptr = GEN_PATH_SEPARATOR;
        }

        /* handle dots between [] */
        if ((ptr = strchr(path, '[')) != NULL) {
            while (*ptr != '\0' && *ptr != ']') {
                if (*ptr == '.') {
                    *ptr = GEN_PATH_SEPARATOR;
                }
                ptr++;
            }
        }
        /* handle dots between <> */
        if ((ptr = strchr(path, '<')) != NULL) {
            while (*ptr != '\0' && *ptr != '>') {
                if (*ptr == '.') {
                    *ptr = GEN_PATH_SEPARATOR;
                }
                ptr++;
            }
        }

        while ((ptr = strchr(path, '[')) != NULL) {
            *ptr = GEN_PATH_SEPARATOR;
        }
        while ((ptr = strchr(path, ']')) != NULL) {
            *ptr = GEN_PATH_SEPARATOR;
        }
        while ((ptr = strchr(path, '<')) != NULL) {
            *ptr = GEN_PATH_SEPARATOR;
        }
        while ((ptr = strchr(path, '>')) != NULL) {
            *ptr = GEN_PATH_SEPARATOR;
        }
}

/*#***********************************************************************\
 * 
 *		make_general_path
 * 
 * 
 * 
 * Parameters : 
 * 
 *	buf - 
 *		
 *		
 *	dir - 
 *		
 *		
 *	fname - 
 *		
 *		
 *	separate_fname - 
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
static void make_general_path(
        char* buf,
        char* dir,
        char* fname,
        bool separate_fname)
{
        char tmpdir[255];
        char tmpfname[255];

        if (separate_fname) {

            /* Forget the possibly existing path part in fname */
            char* tmp;

            strcpy(tmpfname, fname);
            convert_path_separators(tmpfname);

            tmp = strrchr(tmpfname, GEN_PATH_SEPARATOR);
            if (tmp == NULL) {
                tmp = strrchr(tmpfname, GEN_DRIVE_SEPARATOR);
            }
            if (tmp != NULL) {
                fname = tmp + 1;
            } else {
                fname = tmpfname;
            }

        } else {
            /* Check if fname is fully qualified path. */
            if (strchr(fname, GEN_DRIVE_SEPARATOR) || fname[0] == GEN_PATH_SEPARATOR) {
                /* Check later, if this is really valid or not */
                strcpy(buf, fname);
                convert_path_separators(buf);
                return;
            }
            strcpy(tmpfname, fname);
            convert_path_separators(tmpfname);
            fname = tmpfname;
        }

        strcpy(tmpdir, dir);
        convert_path_separators(tmpdir);
        ss_dprintf_4(("tmpdir='%s', last chars: '%c%c'\n",
                    tmpdir,
                    tmpdir[strlen(tmpdir)-2],
                    tmpdir[strlen(tmpdir)-1]));
        if (strlen(tmpdir) > 1 &&
            tmpdir[strlen(tmpdir)-2] == GEN_PATH_SEPARATOR &&
            tmpdir[strlen(tmpdir)-1] == '.') {
            /* dir ends with '/.', forget it */
            tmpdir[strlen(tmpdir)-2] = '\0';               
        }

        /* check the last character of 'dir' */
        switch (tmpdir[strlen(tmpdir) == 0 ? 0 : strlen(tmpdir)-1]) {
            case '\0':
            case GEN_DRIVE_SEPARATOR:
            case GEN_PATH_SEPARATOR:
                /* do not add separator between dir and fname in this case */
                SsSprintf(buf, "%s%s", tmpdir, fname);
                break;
            default:
                /* concatenate dir and fname and put separator between them */
                SsSprintf(buf, "%s%c%s", tmpdir, GEN_PATH_SEPARATOR, fname);
                break;
        }
}

/*#***********************************************************************\
 * 
 *		dos_path
 * 
 * 
 * 
 * Parameters : 
 * 
 *	buf - 
 *		
 *		
 *	fname - 
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
#if defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
static bool dos_path(char* buf, char* fname)
{
        char* p;
        strcpy(buf, fname);
        p = strchr(buf, GEN_DRIVE_SEPARATOR);
        if (p != NULL && (p-buf > 1)) {
            ss_dprintf_1(("Invalid dos drive in '%s'.\n", buf));
            return(FALSE);
        }
        /* Restore the '\' separators */
        while ((p = strchr(buf, GEN_PATH_SEPARATOR)) != NULL) {
            *p = '\\';
        }
        return(TRUE);
}
#endif /* SS_DOS ETC */

/*#***********************************************************************\
 * 
 *		unix_path
 * 
 * 
 * 
 * Parameters : 
 * 
 *	buf - 
 *		
 *		
 *	fname - 
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
#ifdef SS_UNIX
static bool unix_path(char* buf, char* fname)
{
        char* p;
        strcpy(buf, fname);
        p = strchr(buf, GEN_DRIVE_SEPARATOR);
        if (p != NULL) {
            ss_dprintf_1(("Drive letters not allowed in unix '%s'.\n", buf));
            return(FALSE);
        }
        return(TRUE);
}
#endif

/*#***********************************************************************\
 * 
 *		vms_path
 * 
 * 
 *      general form
 *          dka300:/sdrive/solpro/alpha/sse/file.c
 *      becomes
 *          dka300:[sdrive.solpro.alpha.sse]file.c
 * 
 * 
 * Parameters : 
 * 
 *	buf - 
 *		
 *		
 *	fname - 
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
char* SsFnCnvToGeneral(
        char* pathname,
        char* gbuf,
        int   gbufsz)
{
        make_general_path(gbuf, (char *)"", pathname, 0);
        ss_assert(strlen(gbuf) <= (uint)gbufsz);
        return(gbuf);
}

char* SsFnCnvToNative(
        char* pathname,
        char* nbuf,
        int   nbufsz)
{
        char gbuf[255];
        make_general_path(gbuf, (char *)"", pathname, 0);
#if defined(SS_UNIX)
        unix_path(nbuf, gbuf);
#elif defined(SS_DOS) || defined(SS_WIN) || defined(SS_NT)
        dos_path(nbuf, gbuf);
#else
# error Hogihogi! Unknown environment
#endif
        ss_assert(strlen(nbuf) <= (uint)nbufsz);
        return(nbuf);
}
