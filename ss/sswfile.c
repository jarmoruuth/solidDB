/*************************************************************************\
**  source       * sswfile.c
**  directory    * ss
**  description  * Wide char file I/O routines
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

#include "ssstdio.h"
#include "ssstddef.h"

#include "ssc.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sswcs.h"
#include "ssutf.h"
#include "sswfile.h"
#include "ssfile.h"
#ifdef SS_NT
#include <direct.h>
#endif /* SS_NT */

#define SSWF_BOM ((ss_char2_t)0xFEFF)

#ifdef SS_LSB1ST
#define FPUTWC_1STBYTEOFS 0
#define FPUTWC_2NDBYTEOFS 1
#else /* SS_LSB1ST */
#define FPUTWC_1STBYTEOFS 1
#define FPUTWC_2NDBYTEOFS 0
#endif  /* SS_LSB1ST */

SS_FILE* SsUTF8fopen(ss_char1_t* fname, ss_char1_t* flags)
{
        SS_FILE* fp;
        ss_char2_t* fn;
        ss_char2_t* fl;

        fn = SsUTF8toUCS2Strdup(fname);
        fl = SsUTF8toUCS2Strdup(flags);
        fp = SsWfopen(fn, fl);
        SsMemFree(fn);
        SsMemFree(fl);
        return (fp);
}
        

/*##**********************************************************************\
 * 
 *		SsWfopen
 * 
 * Wide character version of fopen. Fails to open file
 * if the system does not support wide character file names and
 * file name cannot be converted to 8-bit char string.
 *
 * 
 * Parameters : 
 * 
 *	fname - in, use
 *		file name
 *		
 *	flags - in, use
 *		file open flags
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_FILE* SsWfopen(ss_char2_t* fname, ss_char2_t* flags)
{
        SS_FILE* fp;
        size_t fn_len;
        size_t flg_len;
        ss_char1_t* fn;
        ss_char1_t* flg;
        bool succp;

        fn_len = SsWcslen(fname);
        flg_len = SsWcslen(flags);
        fn = SsMemAlloc((fn_len + flg_len + 2) * sizeof(ss_char1_t));
        flg = fn + fn_len + 1;
        succp = SsWcs2Str(fn, fname);
        if (succp) {
            succp = SsWcs2Str(flg, flags);
        }
        if (succp) {
            fp = SsFOpenT(fn, flg);
        } else {
            fp = NULL;
        }
        SsMemFree(fn);
        return (fp);
}

/*##**********************************************************************\
 * 
 *		SsFTypeGet
 * 
 * Gets file type of open file handle (must be opened in binary read mode).
 * The file position is set to the first significant data bytes in
 * the file (i.e. the possible UNICODE BOM is skipped if it exists)
 * after the call for this function
 * 
 * Parameters : 
 * 
 *	fp - use
 *		file stream pointer must be in position 0
 *		
 * Return value :
 *      SSTFT_ASCII - the file is not UNICODE text (probably ASCII)
 *      SSTFT_UNICODE_LSB1ST - the file is UNICODE in LSB First format
 *      SSTFT_UNICODE_MSB1ST - the file is UNICODE in MSB First format
 *      SSTFT_ERROR - the file cannot be checked due to error
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsTextFileTypeT SsFTypeGet(SS_FILE* fp)
{
        int rc;
        int c1;
        int c2;
        SsTextFileTypeT ftype;
        ss_debug(long oldpos;)

        ss_debug(oldpos = SsFTell(fp);)
        ss_rc_dassert(oldpos == 0L, (int)oldpos);

        rc = SsFSeek(fp, 0L, SEEK_SET);
        if (rc == EOF) {
            return (SSTFT_ERROR);
        }
        ftype = SSTFT_ASCII;
        c1 = SsFGetc(fp);
        c2 = SsFGetc(fp);
        switch (c1) {
            case (SSWF_BOM & 0x00FF):
                if (c2 == ((SSWF_BOM >> 8) & 0x00FF)) {
                    ftype = SSTFT_UNICODE_LSB1ST;
                }
                break;
            case ((SSWF_BOM >> 8) & 0x00FF):
                if (c2 == (SSWF_BOM & 0x00FF)) {
                    ftype = SSTFT_UNICODE_MSB1ST;
                }
                break;
            default:
                break;
        }
        if (ftype == SSTFT_ASCII) {
            rc = SsFSeek(fp, 0L, SEEK_SET);
            ss_rc_assert(rc == 0, rc);
        }
#ifdef SS_DEBUG
        else {
            /* Empty in product version! */
            ss_debug(long pos = SsFTell(fp);)
            ss_rc_dassert(pos == 2L, (int)pos);
        }
#endif /* SS_DEBUG */
        return (ftype);
}

/*##**********************************************************************\
 * 
 *		SsFputwc
 * 
 * Puts a wide character to file stream.
 * 
 * Parameters : 
 * 
 *	c - in
 *		character to put
 *		
 *	fp - use
 *		file stream pointer
 *		
 * Return value :
 *      c when successful, EOF on failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsFputwc(ss_char2_t c, SS_FILE* fp)
{
        int r;
        ss_char1_t c1;
        ss_char1_t c2;

        if (c == '\n') {
            r = SsFPutc('\r', fp);
            if (r == EOF) {
                return (EOF);
            }
            r = SsFPutc('\0', fp);
            if (r == EOF) {
                return (EOF);
            }
        }
        c1 = ((ss_char1_t*)&c)[FPUTWC_1STBYTEOFS];
        c2 = ((ss_char1_t*)&c)[FPUTWC_2NDBYTEOFS];
        r = SsFPutc(c1, fp);
        if (r == EOF) {
            return (EOF);
        }
        r = SsFPutc(c2, fp);
        if (r == EOF) {
            return (EOF);
        }
        return ((int)c);
}

/*##**********************************************************************\
 * 
 *		SsFPutWBuf
 * 
 * Puts a wide char string buffer to file
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		character string buffer
 *		
 *	n - in
 *		number of wide characters to put
 *		
 *	fp - use
 *		file stream pointer
 *		
 * Return value :
 *      n when successful, EOF when failed to write all characters
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsFPutWBuf(ss_char2_t* s, size_t n, SS_FILE* fp)
{
        int r;
        size_t i;

        if (n != 0) {
            if (s[n-1] == '\0') {
                n--;
            }
            for (i = n; i > 0; i--, s++) {
                r = SsFputwc(*s, fp);
                if (r == EOF) {
                    return (EOF);
                }
            }
        }
        return (n);
}

/*##**********************************************************************\
 * 
 *		SsFputws
 * 
 * Wide character replacement for library routine fputs()
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		wide character string to put
 *		
 *	fp - use
 *		file stream pointer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsFputws(ss_char2_t* s, SS_FILE* fp)
{
        int r;
        int i;

        for (i = 0; *s != '\0' ; s++, i++) {
            r = SsFputwc(*s, fp);
            if (r == EOF) {
                return (EOF);
            }
        }
        return (i);
}

/*##**********************************************************************\
 * 
 *		SsWfputBOM
 * 
 * Puts byte order mark (BOM) to file
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
bool SsWfputBOM(SS_FILE* fp)
{
        int r;
        ss_debug(long pos = SsFTell(fp);)
        ss_rc_dassert(pos == 0L, (int)pos);
        r = SsFputwc(SSWF_BOM, fp);
        return (r == SSWF_BOM);
}


/*##**********************************************************************\
 * 
 *		SsFgetwc
 * 
 * Gets a wide character from file stream,
 * CR-LF is translated to LF on input
 * 
 * Parameters : 
 * 
 *	fp - use
 *		file stream pointer
 *		
 *	ftype - in
 *		file type indicator
 *          SSTFT_ASCII = one byte in file is converted to wide char
 *          SSTFT_UNICODE_LSB1ST = file is in UNICODE LSB first format
 *          SSTFT_UNICODE_MSB1ST = file is in UNICODE MSB first format
 *		
 * Return value :
 *      the read-in character or EOF when end of file or error
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsFgetwc(SS_FILE* fp, SsTextFileTypeT ftype)
{
        int c1;
        int c2;
        bool restarted_for_lf = FALSE;

restart_for_lf:
        c1 = SsFGetc(fp);
        if (ftype != SSTFT_ASCII) {
            if (c1 == EOF) {
                return (EOF);
            }
            c2 = SsFGetc(fp);
            if (c1 == EOF) {
                return (EOF);
            }
            if (ftype == SSTFT_UNICODE_LSB1ST) {
                c1 = (c2 << 8) | c1;
            } else {
                ss_rc_dassert(ftype == SSTFT_UNICODE_MSB1ST, (int)ftype);
                c1 = (c1 << 8) | c2;
            }
        }
        if (restarted_for_lf) {
            if (c1 != '\n') {
                return (EOF);
            }
        } else if (c1 == '\r') {
            restarted_for_lf = TRUE;
            goto restart_for_lf;
        }
        return (c1);
}

/*##**********************************************************************\
 * 
 *		SsFgetws
 * 
 * Gets a wide character string from file stream
 * 
 * Parameters : 
 * 
 *	buf - out, use
 *		string buffer
 *		
 *	n - in
 *		number of that can be put to buf (terminating '\0' included)
 *		
 *	fp - use
 *		file stream pointer
 *		
 *	ftype - in
 *		file type indicator
 *          SSTFT_ASCII = one byte in file is converted to wide char
 *          SSTFT_UNICODE_LSB1ST = file is in UNICODE LSB first format
 *          SSTFT_UNICODE_MSB1ST = file is in UNICODE MSB first format
 *		
 * Return value :
 *      buf - success
 *      NULL - failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsFgetws(ss_char2_t* buf, size_t n, SS_FILE* fp, SsTextFileTypeT ftype)
{
        ss_char2_t *p;

        if (n == 0) {
            return (NULL);
        }
        for (p = buf; n > 1; n--, p++) {
            int c;

            c = SsFgetwc(fp, ftype);
            switch (c) {
                case EOF:
                    break;
                case '\n':
                    *p = (ss_char2_t)c;
                    p++;
                    break;
                default:
                    *p = (ss_char2_t)c;
                    continue;
            }
        }
        *p = '\0';
        return (buf);
}

bool SsUTF8chdir(ss_char1_t* dirname)
{
        bool succp;
        ss_char2_t* dn;

        dn = SsUTF8toUCS2Strdup(dirname);
        succp = SsWchdir(dn);
        SsMemFree(dn);
        return (succp);
}

bool SsWchdir(ss_char2_t* dirname)
{
        bool succp;

        size_t dn_size = SsWcslen(dirname) + 1;
        ss_char1_t* dn = SsMemAlloc(dn_size);

        succp = SsWbuf2Str(dn, dirname, dn_size);
        if (succp) {
            succp = SsChdir(dn);
        }
        SsMemFree(dn);
        return (succp);
}
