/*************************************************************************\
**  source       * ssfnsrch.c
**  directory    * ss
**  description  * File name search routine (from a path
**               * specification and file name)
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
#include "ssstdio.h"
#include "ssstring.h"
#include "ssgetenv.h"

#include "ssc.h"
#include "ssmem.h"
#include "ssfile.h"
#include "ssfnsplt.h"
#include "ssdebug.h"
#include "sslimits.h"
#include "sschcvt.h"
#include "ssfnsrch.h"


#if defined(SS_WIN) || defined(SS_NT)

# define SSFN_PATHSEP ";"
# if defined(SS_WIN)
#  define SS_DPATH SsGetEnv("windir")
# else 
#  define SS_DPATH SsGetEnv("DPATH")
# endif

#elif defined(UNIX)

# define SSFN_PATHSEP ":"
# define SS_DPATH NULL

#elif defined(SS_DOS) 

# define SSFN_PATHSEP ""
# define SS_DPATH NULL

#else

#error undefined environment!

#endif

/*#***********************************************************************\
 * 
 *		make_sepmap
 * 
 * Creates a separator map for fast scanning of separator chars
 * 
 * Parameters : 
 * 
 *	sepmap - out, use
 *		pointer to separator map
 *		
 *	seps - in, use
 *		string containing possible separators
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void make_sepmap(uchar* sepmap, char* seps)
{
        memset(sepmap, 0, 1 << CHAR_BIT);
        for ( ; ; seps++) {
            sepmap[(uchar)*seps] = (uchar)1;
            if (*seps == '\0') {
                break;
            }
        }

}

/*#***********************************************************************\
 * 
 *		sep_scan
 * 
 * Scans next separator from buffer
 * 
 * Parameters : 
 * 
 *	buf - in, use
 *		buffer
 *		
 *	sepmap - in, use
 *		character map where index entries at separator chars are
 *          nonzero, others are zero
 *		
 * Return value - ref :
 *      pointer to buffer position where the 1st found separator is
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static char* sep_scan(char* buf, uchar* sepmap)
{
        for ( ; !sepmap[(uchar)*buf]; buf++) {
            /* no loop body ! */
        }
        return (buf);
}

/*#***********************************************************************\
 * 
 *		copy_dir
 * 
 * Copies dir to buffer
 * 
 * Parameters : 
 * 
 *	dir_start - in, use
 *		directory name start pointer with possible leading spaces
 *		
 *	dir_end - in, use
 *		directory name end pointer with possible trailing spaces
 *		
 *	dir_buf - out, use
 *		pointer to dir buffer where trimmed dirname will be copied
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static void copy_dir(char* dir_start, char* dir_end, char* dir_buf)
{
        ss_dassert(dir_start <= dir_end);
        while (dir_start < dir_end && ss_isspace((uchar)*dir_start)) {
            ++dir_start;
        }
        if (dir_start < dir_end) {
            while (ss_isspace((uchar)*dir_end)) {
                --dir_end;
            }
        }
        if (dir_start < dir_end) {
            memcpy(dir_buf, dir_start, dir_end - dir_start);
        }
        dir_buf[dir_end - dir_start] = '\0';
}

/*##**********************************************************************\
 * 
 *		SsFnSearch
 * 
 * Tries to find a file name from a path specification separated by
 * system dependent path characters
 * 
 * Parameters : 
 * 
 *	fname - in, use
 *		file name
 *		
 *	path_string - in, use
 *		path string
 *		
 * Return value - give :
 *      pointer to complete file path of existing file or
 *      NULL if no matching file is found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* SsFnSearch(char* fname, char* path_string)
{
        char* retval;
        size_t retval_length;
        char* dir_buf;
        char* dir_start;
        char* dir_end;
        char* path_string_end;
        uchar sepmap[1 << CHAR_BIT];
        bool empty_tried;
        size_t fname_length;
        size_t path_string_length;
        bool succp;

        empty_tried = FALSE;
        make_sepmap(sepmap, (char *)SSFN_PATHSEP);
        fname_length = strlen(fname);
        path_string_length = strlen(path_string);
        path_string_end = path_string + path_string_length;
        dir_buf = SsMemAlloc(path_string_length + 1);
        dir_start = path_string;

        for (retval = NULL;
             dir_start <= path_string_end;
             dir_start = dir_end + 1)
        {
            dir_end = sep_scan(dir_start, sepmap);
            copy_dir(dir_start, dir_end, dir_buf);
            if (*dir_buf == '\0') {
                if (empty_tried) {
                    continue;
                }
                empty_tried = TRUE;
            }
            retval_length = strlen(dir_buf) + fname_length + 2;
            retval = SsMemAlloc(retval_length);
            succp = SsFnMakePath(dir_buf, fname, retval, retval_length);
            ss_dassert(succp);
            if (SsFExist(retval)) {
                break;
            } 
            SsMemFree(retval);
            retval = NULL;
        }
        SsMemFree(dir_buf);
        return (retval);
}

/*##**********************************************************************\
 * 
 *		SsDataFileSearch
 * 
 * Tries to find a file from system dependent data search path
 * 
 * Parameters : 
 * 
 *	fname - in, use
 *		file name
 *		
 * Return value - give :
 *      pointer to full file name buffer or
 *      NULL when not found
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
char* SsDataFileSearch(char* fname)
{
        char* dpath;
        char* filepath;

        dpath = SS_DPATH;
        if (dpath != NULL) {
            filepath = SsFnSearch(fname, dpath);
        } else {
            filepath = NULL;
        }
        return (filepath);
}
