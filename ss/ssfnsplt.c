/*************************************************************************\
**  source       * ssfnsplt.c
**  directory    * ss
**  description  * File name split/merge routines.
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

#include "ssstring.h"
#include "ssc.h"
#include "ssstdio.h"
#include "ssstdlib.h"
#include "ssfnsplt.h"
#include "ssdebug.h"

#if defined(DOS) || defined(WINDOWS) || defined(SS_NT) 

#define PATHSEPARATOR   '\\'
#define POSSIBLE_PATHSEPARATORS  "\\/:"
#define ABSPATH(path) absolute_path(path)

static bool absolute_path(char* path)
{
        switch (path[0]) {
            case '\\':
            case '/':
                return (TRUE);
            case '\0':
                break;
            default:
                if (path[1] == ':') {
                    return (TRUE);
                }
                break;
        }
        return (FALSE);
}

static void truncate_pathsep_if_not_root(char* path)
{
        size_t len = strlen(path);

        if (len > 0) {
            switch (path[len-1]) {
                case '\\':
                case '/':
                    if (len == 1) {
                        break;
                    }
                    if (len == 3 && path[1] == ':') {
                        break; /* eg. "C:\" */
                    }
                    path[len - 1] = '\0';;
                    break;
                default:
                    break;
            }
        }
}

#endif /* DOS || WINDOWS */

#ifdef UNIX

#define PATHSEPARATOR   '/'
#define POSSIBLE_PATHSEPARATORS  "/"
#define ABSPATH(path) ((path)[0] == PATHSEPARATOR)

static void truncate_pathsep_if_not_root(char* path)
{
        size_t len;

        len = strlen(path);
        if (len > 0 && path[len - 1] == PATHSEPARATOR) {
            if (len != 1) {
                path[len - 1] = '\0';
            }
        }
}

#endif /* UNIX */

/*##**********************************************************************\
 * 
 *		SsFnSplitPath
 * 
 * Splits a file path to a filename and a directory
 * 
 * Parameters : 
 * 
 *	pathname - in, use
 *		path to be split
 *
 *	dbuf - out, use
 *		directory buffer
 *
 *	dbuf_s - in
 *		directory buffer size
 *
 *	fbuf - out, use
 *		file buffer
 *
 *	fbuf_s - in
 *		file buffer size
 *
 * Return value :
 * 
 *      1 if successful,
 *      0 otherwise (buffers too small)
 * 
 * Limitations  :
 * 
 * Globals used :
 */
int SsFnSplitPath(pathname, dbuf, dbuf_s, fbuf, fbuf_s)
char *pathname, *dbuf;
int dbuf_s;
char *fbuf;
int fbuf_s;
{
        int l = strlen(pathname);
        char *ptr;

        while (l > 0 && strchr(POSSIBLE_PATHSEPARATORS, pathname[l-1]) == NULL) {
            l--;
        }

        if (l == 0) {
            /* There is no separator, assume file. */
            ptr = pathname;
        } else if (l == 1) {
            /* Separator is the first character. */
            ptr = pathname + 1;
        } else {
            ptr = pathname + l;
        }

        if (l >= dbuf_s) {
            return(0);
        }

        if (strlen(ptr) >= (size_t)fbuf_s) {
            return(0);
        }

        strncpy(dbuf, pathname, l);
        dbuf[l] = '\0';
        truncate_pathsep_if_not_root(dbuf);
        
        strcpy(fbuf, ptr);

        return(1);
}

/*##**********************************************************************\
 * 
 *		SsFnIsPath
 * 
 * Return true if filename contains a path
 * 
 * Parameters : 
 * 
 *	filename - in, use
 *
 * Return value :
 * 
 *      TRUE - filename contains a patch
 *      FALSE - otherwise
 * 
 * Limitations  :
 * 
 * Globals used :
 */
bool SsFnIsPath(
        char* pathname)
{
        int l = strlen(pathname);

        while (l > 0 && strchr(POSSIBLE_PATHSEPARATORS, pathname[l-1]) == NULL) {
            l--;
        }

        if (l == 0) {
            return FALSE;
        } else {
            return TRUE;
        }
}

/*##**********************************************************************\
 *           
 *		SsFnMakePath
 * 
 * Joins to a filename and a directory to a file path 
 * 
 * Parameters : 
 * 
 *	dirname - in, use
 *		directory
 *
 *	filename - in, use
 *		file
 *
 *	pbuf - out, use
 *		result buffer
 *
 *	pbuf_s - in
 *		result buffer size
 *
 * 
 * Return value :
 * 
 *      1 if successful,
 *      0 otherwise (buffers too small)
 * 
 * Limitations  :
 * 
 * Globals used :
 */
int SsFnMakePath(
        char* dirname,
        char* filename,
        char* pbuf,
        int pbuf_s)
{
        size_t dirname_len = 0;
        size_t filename_len = 0;
        size_t l;

        if (dirname != NULL) {
            dirname_len = strlen(dirname);
        }
        if (filename != NULL) {
            filename_len = strlen(filename);
        }
        if (dirname_len + filename_len + 2 > (uint)pbuf_s) {
            return(0);
        }
        if (filename_len != 0 && ABSPATH(filename)) {
            strcpy(pbuf, filename);
            return(1);
        }
        l = dirname_len;
        if (dirname_len != 0) {
            memcpy(pbuf, dirname, dirname_len);
            if (strchr(POSSIBLE_PATHSEPARATORS, dirname[dirname_len - 1])
                == NULL &&
                filename_len != 0)
            {
                /* Not empty directory name and last char not path separator. */
                pbuf[l] = PATHSEPARATOR;
                l++;
            }
        }
        if (filename_len != 0) {
            memcpy(pbuf + l, filename, filename_len + 1);
        } else {
            pbuf[l] = '\0';
        }
        return(1);
}



/*##**********************************************************************\
 * 
 *		SsFnSplitPathExt
 * 
 * Splits a file path to its parts directory, base filename and extension
 * 
 * Parameters : 
 * 
 *	pathname - 
 *		
 *		
 *	dbuf - 
 *		
 *		
 *	dbuf_s - 
 *		
 *		
 *	fbuf - 
 *		
 *		
 *	fbuf_s - 
 *		
 *		
 *	ebuf - 
 *		
 *		
 *	ebuf_s - 
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
int SsFnSplitPathExt(
        char*   pathname,
        char*   dbuf,
        int     dbuf_s, 
        char*   fbuf,
        int     fbuf_s,
        char*   ebuf,
        int     ebuf_s)
{
        char* dot;
        char* ext;

        if (SsFnSplitPath(pathname, dbuf, dbuf_s, fbuf, fbuf_s) == 0) {
            return(0);
        }

        dot = strchr(fbuf, '.');
        if (dot == NULL) {
            /* No extension */
            ss_assert(ebuf_s > 0);
            ebuf[0] = '\0';
            return(1);
        }
        *dot = '\0';
        ext  = dot + 1;

        if (strlen(ext) + 1  > (size_t)ebuf_s) {
            /* ebuf too small */
            return(0);
        } else {
            /* Ok, copy the extension */
            strcpy(ebuf, ext);
            return(1);
        }
}

int SsFnMakePathExt(
        char*   dirname,
        char*   basename,
        char*   extension,
        char*   pbuf,
        int     pbuf_s)
{

        if (SsFnMakePath(dirname, basename, pbuf, pbuf_s) == 0) {
            return(0);
        }
        
        if (strlen(pbuf) + strlen(extension) + 1 > (size_t)pbuf_s) {
            return(0);
        }
        if (strlen(extension) > 0) {
            /* Add only real extensions */
            strcat(pbuf, ".");
            strcat(pbuf, extension);
        }
        return(1);
}

bool SsFnPathIsAbsolute(
        char* pathname)
{
        return (ABSPATH(pathname));
}

#if 0 /* not needed */
bool SsFnMakePath3Way(
        char* basedirname,
        char* dirname,
        char* fname,
        char*   pbuf,
        int     pbuf_s)
{
        size_t len;
        size_t fname_len;
        size_t basedirname_len = 0;
        size_t dirname_len = 0;
        ss_dassert(fname != NULL);

        fname_len = strlen(fname);
        if (ABSPATH(fname) ||
            ((basedirname == NULL || basedirname[0] == '\0') &&
             (dirname == NULL || dirname[0] == '\0')))
        {
            if (fname_len >= pbuf_s) {
                return (FALSE);
            }
            memcpy(pbuf, fname, fname_len + 1);
            return (TRUE);
        }
        if (dirname != NULL && dirname[0] != '\0') {
            dirname_len = strlen(dirname);
            if (ABSPATH(dirname) ||
                (basedirname == NULL || basedirname[0] == '\0'))
            {
                return (SsFnMakePath(dirname, fname, pbuf, pbuf_s));
            }
            basedirname_len = strlen(basedirname);
            len = basedirname_len;
            if (len >= pbuf_s) {
                return (FALSE);
            }
            memcpy(pbuf, basedirname, basedirname_len + 1);
            if (strchr(POSSIBLE_PATHSEPARATORS,
                       basedirname[basedirname_len - 1]) == NULL)
            {
                if (len + 1 >= pbuf_s) {
                    return (FALSE);
                }
                pbuf[len++] = PATHSEPARATOR;
                pbuf[len] = '\0';
            }
            if (len + dirname_len >= pbuf_s) {
                return (FALSE);
            }
            memcpy(pbuf + len, dirname, dirname_len + 1);
            len += dirname_len;
            if (strchr(POSSIBLE_PATHSEPARATORS,
                       dirname[dirname_len - 1]) == NULL)
            {
                if (len + 1 >= pbuf_s) {
                    return (FALSE);
                }
                pbuf[len++] = PATHSEPARATOR;
                pbuf[len] = '\0';
            }
            if  (len + fname_len >= pbuf_s) {
                return (FALSE);
            }
            memcpy(pbuf + len, fname, fname_len + 1);
            return (TRUE);
        }
        ss_dassert(basedirname != NULL && basedirname[0] != '\0');
        return (SsFnMakePath(basedirname, fname, pbuf, pbuf_s));
}
#endif /* 0, not needed */
