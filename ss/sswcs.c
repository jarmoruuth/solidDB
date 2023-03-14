/*************************************************************************\
**  source       * sswcs.c
**  directory    * ss
**  description  * Wide character string (wcs) support routines
**               * they are the strxxx compatibles for 2-byte
**               * (UNICODE) characters
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


#include "ssc.h"
#include "ssstddef.h"
#include "ssstring.h"
#include "ssmem.h"
#include "ssdebug.h"
#include "sslimits.h"
#include "sswctype.h"
#include "sswcs.h"


ss_char1_t SsWcs2StrDefChar = (ss_char1_t)0xFF;

void SsWcs2StrSetDefChar(ss_char1_t c)
{
        SsWcs2StrDefChar = c;
}

/*##**********************************************************************\
 * 
 *		SsWcscat
 * 
 * Two-byte version of strcat()
 * 
 * Parameters : 
 * 
 *	target - in out, use
 *		target string (with SsWcslen(tail) extra space)
 *		
 *	tail - in, use
 *		new tail to be appended to target
 *		
 * Return value :
 *      target
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcscat(ss_char2_t *target, const ss_char2_t* tail)
{
        ss_char2_t* t = target;
        while (*t) {
            t++;
        }
        while ((*t = *tail) != (ss_char2_t)0) {
            t++;
            tail++;
        }
        return (target);
}

/*##**********************************************************************\
 * 
 *		SsWcschr
 * 
 * Two-byte version of strchr()
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		the string
 *		
 *	c - in
 *		character to search for
 *		
 * Return value :
 *      pointer to first occurence of c in s or NULL
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcschr(const ss_char2_t* s, ss_char2_t c)
{
        ss_dassert(s != NULL);
        if (s != NULL) {
            for (;;s++) {
                if (*s == c) {
                    return ((ss_char2_t*)s);
                }
                if (*s == (ss_char2_t)0) {
                    break;
                }
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *		SsWcscmp
 * 
 * Two-byte version of strcmp()
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
 *		
 *		
 * Return value :
 *      as with strcmp()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsWcscmp(const ss_char2_t* s1, const ss_char2_t* s2)
{
        int cmp;

        for (;;s1++, s2++) {
            if (sizeof(int) <= sizeof(ss_char2_t)) {
                if (*s1 != *s2) {
                    if (*s1 < *s2) {
                        return (-1);
                    }
                    return (1);
                }
            } else {
                cmp = (int)*s1 - (int)*s2;
                if (cmp != 0) {
                    return (cmp);
                }
            }
            if (*s1 == (ss_char2_t)0) {
                break;
            }
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		SsWcscpy
 * 
 * Two-byte version of strcpy
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	s - in, use
 *		source string
 *		
 * Return value :
 *      dst
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcscpy(ss_char2_t* dst, const ss_char2_t* s)
{
        ss_char2_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        while ((*d = *s) != (ss_char2_t)0) {
            s++;
            d++;
        }
        return (dst);
}

/*##**********************************************************************\
 * 
 *		SsWcscspn
 * 
 * Two-byte version of strcspn()
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
 *		
 *		
 * Return value :
 *      see documentation on ANSI routine strcspn()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsWcscspn(const ss_char2_t* s1, const ss_char2_t* s2)
{
        size_t pos;

        for (pos = 0; *s1 != (ss_char2_t)0; s1++, pos++) {
            const ss_char2_t* p;
            for (p = s2; *p != (ss_char2_t)0; p++) {
                if (*s1 == *p) {
                    return (pos);
                }
            }
        }
        return pos;
}

/*##**********************************************************************\
 * 
 *		SsWcslen
 * 
 * Two-byte version of strlen
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		string
 *		
 * Return value :
 *      length of s (excluding the NUL termination)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsWcslen(const ss_char2_t * s)
{
        size_t len = 0;
        while (*s != (ss_char2_t)0) {
            s++;
            len++;
        }
        return (len);
}

/*##**********************************************************************\
 * 
 *		SsWcsncat
 * 
 * Two byte version of strncat(). Note the difference
 * in NUL-termination between strncpy() and strncat()
 * 
 * Parameters : 
 * 
 *	target - in out, use
 *		target string (with extra space allocated)
 *		
 *	tail - in, use
 *		tail to be appended
 *		
 *	n - in
 *		max number of chars to append from tail
 *		
 * Return value :
 *      target
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcsncat(ss_char2_t* target, const ss_char2_t* tail, size_t n)
{
        ss_char2_t* t = target;

        while (*t != (ss_char2_t)0) {
            t++;
        }
        while (n != 0) {
            if ((*t = *tail) == (ss_char2_t)0) {
                return (target);
            }
            n--;
            t++;
            tail++;
        }
        *t = (ss_char2_t)0;
        return (target);
}

/*##**********************************************************************\
 * 
 *		SsWcsncmp
 * 
 * Two-byte version of strncmp()
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
 *		
 *		
 *	n - in
 *		
 *		
 * Return value : 
 *      See docs on ANSI function strncmp()
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsWcsncmp(const ss_char2_t* s1, const ss_char2_t* s2, size_t n)
{
        int cmp;

        for (;n != 0; n--, s1++, s2++) {
            if (sizeof(int) <= sizeof(ss_char2_t)) {
                /* 16-bit int */
                if (*s1 != *s2) {
                    if (*s1 < *s2) {
                        return (-1);
                    }
                    return (1);
                }
            } else {
                /* 32-bit int */
                cmp = (int)*s1 - (int)*s2;
                if (cmp != 0) {
                    return (cmp);
                }
            }
            if (*s1 == (ss_char2_t)0) {
                ss_rc_dassert(*s2 == (ss_char2_t)0, *s2);
                break;
            }
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		SsWcsncpy
 * 
 * Two-byte version of strncpy()
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	s - in
 *		source string
 *		
 *	n - in
 *		max number of chars to copy
 *		
 * Return value :
 *      dst
 * 
 * Comments :
 *      Note: as in ANSI function strncpy() there is a weird
 *      behavior when n <= SsWcslen(s) the target is left unterminated!
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcsncpy(ss_char2_t * dst, const ss_char2_t *s, size_t n)
{
        ss_char2_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);

        while (n != 0 && (*d = *s) != (ss_char2_t)0) {
            n--;
            d++;
            s++;
        }
        return (dst);
}

/*##**********************************************************************\
 * 
 *		SsWcspbrk
 * 
 * Two-byte version of strpbrk()
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
 *		
 *		
 * Return value :
 *      see docs on ANSI function strpbrk()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcspbrk(const ss_char2_t* s1, const ss_char2_t* s2)
{
        for (; *s1 != (ss_char2_t)0; s1++) {
            const ss_char2_t* p;
            for (p = s2; *p != (ss_char2_t)0; p++) {
                if (*s1 == *p) {
                    return ((ss_char2_t*)s1);
                }
            }
        }
        return NULL;
}

/*##**********************************************************************\
 * 
 *		SsWcsrchr
 * 
 * Two-byte version of strrchr()
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		string
 *		
 *	c - in
 *		char
 *		
 * Return value :
 *      last occurence of c in s or NULL
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcsrchr(const ss_char2_t* s, ss_char2_t c)
{
        const ss_char2_t* last = NULL;

        while (*s != (ss_char2_t)0) {
            if (*s == c) {
                last = s;
            }
        }
        return ((ss_char2_t*)last);
}

/*##**********************************************************************\
 * 
 *		SsWcsspn
 * 
 * Two-byte version of strspn
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
 *		
 *		
 * Return value :
 *      See docs on ANSI function strspn()
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsWcsspn(const ss_char2_t* s1, const ss_char2_t* s2)
{
        size_t pos;

        for (pos = 0; *s1 != (ss_char2_t)0; s1++, pos++) {
            const ss_char2_t* p;
            for (p = s2; ; p++) {
                if (*p == (ss_char2_t)0) {
                    return (pos);
                }
                if (*s1 == *p) {
                    break;
                }
            }
        }
        return pos;
}

/*##**********************************************************************\
 * 
 *		SsWcswcs
 * 
 * Two-byte version of strstr()
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		source string to search from
 *		
 *	pat - in, use
 *		string pattern to search for
 *		
 * Return value :
 *      first occurence of pat in s or NULL
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcswcs(const ss_char2_t* s, const ss_char2_t* pat)
{
        const ss_char2_t* t;
        do {
            const ss_char2_t* p = pat;

            t = s;
            while (*t == *p && *t != (ss_char2_t)0) {
                t++;
                p++;
            }
            if (*p == (ss_char2_t)0) {
                return ((ss_char2_t*)s);
            }
            s++;
        } while (*t != (ss_char2_t)0);
        return (NULL);
}

/*##**********************************************************************\
 * 
 *		SsWcsicmp
 * 
 * Case insensitive wide character string comparison
 * 
 * Parameters : 
 * 
 *	s1 - in, use
 *		
 *		
 *	s2 - in, use
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
int SsWcsicmp(const ss_char2_t* s1, const ss_char2_t* s2)
{
        int cmp;

        for (;;s1++, s2++) {
            if (sizeof(int) <= sizeof(ss_char2_t)) {
                ss_char2_t c1 = ss_towupper(*s1);
                ss_char2_t c2 = ss_towupper(*s2);

                if (c1 != c2) {
                    if (c1 < c2) {
                        return (-1);
                    }
                    return (1);
                }
            } else {
                cmp = (int)ss_towupper(*s1) - (int)ss_towupper(*s2);
                if (cmp != 0) {
                    return (cmp);
                }
            }
            if (*s1 == (ss_char2_t)0) {
                break;
            }
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		SsWcslwr
 * 
 * Two-byte version of strlwr()
 * 
 * Parameters : 
 * 
 *	s - use
 *		
 *		
 * Return value : 
 *      s
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcslwr(ss_char2_t* s)
{
        ss_char2_t* p;

        for (p = s; *p != (ss_char2_t)0; p++) {
            *p = ss_towlower(*p);
        }
        return s;
}

/*##**********************************************************************\
 * 
 *		SsWcsupr
 * 
 * Two-byte version of strupr()
 * 
 * Parameters : 
 * 
 *	s - use
 *		
 *		
 * Return value :
 *      s
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWcsupr(ss_char2_t* s)
{
        ss_char2_t* p;

        for (p = s; *p != (ss_char2_t)0; p++) {
            *p = ss_towupper(*p);
        }
        return s;
}

/*##**********************************************************************\
 * 
 *		SsWcs2Str
 * 
 * Copies Wide character string to character string
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination string buffer
 *		
 *	s - in, use
 *		source wide char string
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - source contains characters that do not fit to 8 bits
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsWcs2Str(ss_char1_t* dst, const ss_char2_t* s)
{
        ss_char1_t* d = dst;
        bool succp = TRUE;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        for (;; d++, s++) {
            ss_char2_t c;

            *d = (ss_char1_t)(c = *s);

            if (c == (ss_char2_t)0) {
                break;
            }
            if (c & (ss_char2_t)~0x00ff) {
                succp = FALSE;
                *d = SsWcs2StrGetDefChar();
            }
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		SsWbuf2Str
 * 
 * Copies a possibly nul-terminated buffer of wide characters to string.
 * Does not append extra nul termination character
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination string buffer
 *		
 *	s - in, use
 *		source buffer
 *		
 *	len - in
 *		string length in s
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsWbuf2Str(ss_char1_t* dst, ss_char2_t* s, size_t len)
{
        bool succp = TRUE;
        ss_char1_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        for (;len; len--, d++, s++) {
            ss_char2_t c = *s;

            if (c & (ss_char2_t)~0x00ff) {
                succp = FALSE;
                *d = SsWcs2StrGetDefChar();
            } else {
                *d = (ss_char1_t)c;
            }
            ss_dassert(c != (ss_char2_t)0 || len == 1);
        }
        return (succp);
}
/*##**********************************************************************\
 * 
 *		SsvaWbuf2Str
 * 
 * Copies a possibly nul-terminated buffer of wide characters to string.
 * Does not append extra nul termination character. Wide characters are
 * supposed to be in (possibly unaligned) MSB 1st buffer.
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination string buffer
 *		
 *	s - in, use
 *		source buffer
 *		
 *	len - in
 *		string length in s
 *		
 * Return value :
 *      TRUE - success
 *      FALSE - failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsvaWbuf2Str(ss_char1_t* dst, ss_char2_t* s, size_t len)
{
        bool succp = TRUE;
        ss_char1_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        for (;len ; len--, d++, s++) {
            ss_char2_t c = SS_CHAR2_LOAD_MSB1ST(s);
            if (c & (ss_char2_t)~0x00ff) {
                succp = FALSE;
                *d = SsWcs2StrGetDefChar();
            } else {
                *d = (ss_char1_t)c;
            }
            ss_dassert(c != (ss_char2_t)0 || len == 1);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *		SsSbuf2Wcs
 * 
 * Copies a buffer of characters to wide character buffer.
 * the source must not contain nul-characters except as
 * the last character the destination will not be nul-terminated
 * unless the last character of source buffer is zero
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	s - in, use
 *		source buffer
 *		
 *	len - in
 *		number of characters to copy
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsSbuf2Wcs(ss_char2_t* dst, ss_char1_t* s, size_t len)
{
        dst += len;
        s += len;
        while (len) {
            s--;
            dst--;
            len--;
            *dst = (ss_char2_t)(ss_byte_t)*s;
        }
}

/*##**********************************************************************\
 * 
 *		SsSbuf2Wcs_msb1st
 * 
 * Same as SsSbuf2Wcs, but the dst buffer is always stored in MSB
 * First byte order and the dst may be misaligned
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	s - in, use
 *		source buffer
 *		
 *	len - in
 *		source buffer length in characters
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsSbuf2Wcs_msb1st(ss_char2_t* dst, ss_char1_t* s, size_t len)
{
        dst += len;
        s += len;
        while (len) {
            ss_char2_t c;

            s--;
            dst--;
            len--;
            c = (ss_char2_t)(ss_byte_t)*s;
            SS_CHAR2_STORE_MSB1ST(dst, c);
        }
}

/*##**********************************************************************\
 * 
 *		SsWbuf2vaWbuf
 * 
 * Copies a native byte order wide char buffer to possibly misaligned
 * MSB First byte order buffer.
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	src - in, use
 *		source buffer
 *		
 *	n - in
 *		number of wide chars to copy
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsWbuf2vaWbuf(ss_char2_t* dst, ss_char2_t* src, size_t n)
{
        while (n) {
            ss_char2_t c;
            n--;
            c = *src;
            src++;
            SS_CHAR2_STORE_MSB1ST(dst, c);
            dst++;
        }
}

/*##**********************************************************************\
 * 
 *		SsvaWbuf2Wbuf
 * 
 * Copies a possibly misaligned MSB First format wide char buffer
 * to native byte order buffer.
 * 
 * Parameters : 
 * 
 *	dst - out, use
 *		destination buffer
 *		
 *	src - in, use
 *		source buffer
 *		
 *	n - in
 *		number of wide chars to copy
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsvaWbuf2Wbuf(ss_char2_t* dst, ss_char2_t* src, size_t n)
{
        while (n) {
            n--;
            *dst = SS_CHAR2_LOAD_MSB1ST(src);
            src++;
            dst++;
        }
}

/*##**********************************************************************\
 * 
 *		SsStr2Wcs
 * 
 * String to wide character string copy
 * 
 * Parameters : 
 * 
 *	d - out, use
 *		output string buffer
 *		
 *	s - in, use
 *		input string
 *		
 * Return value : 
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsStr2Wcs(ss_char2_t* d, ss_char1_t* s)
{
        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        while ((*d = (ss_char2_t)(ss_byte_t)*s) != (ss_char2_t)0) {
            d++;
            s++;
        }
}

/*##**********************************************************************\
 * 
 *		SsStr2WcsInPlace
 * 
 * In-place char to wide-char string conversion
 * 
 * Parameters : 
 * 
 *	p - use
 *		the string to be converted
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsStr2WcsInPlace(void* p)
{
        ss_char2_t* d;
        ss_byte_t* s = p;

        while (*s != '\0') {
            s++;
        }
        d = (ss_char2_t*)p + (s - (ss_byte_t*)p);
        for (;; d--, s--) {
            *d = (ss_char2_t)*s;
            if (s == (ss_byte_t*)p) {
                ss_dassert(s == (ss_byte_t*)d);
                break;
            }
        }
}

/*##**********************************************************************\
 * 
 *		SsWcs2StrInPlace
 * 
 * In-place wide char to char conversion
 * 
 * Parameters : 
 * 
 *	p - use
 *		the string to be converted
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsWcs2StrInPlace(void* p)
{
        bool succp;

        succp = SsWcs2Str(p, p);
        return (succp);
}


/*##**********************************************************************\
 * 
 *		SsWbufwbuf_unaligned
 * 
 * Two-byte version of strstr() with non-null terminated strings
 * and possibly unaligned members
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		source string to search from
 *
 *      slen - in
 *          input buffer length in 2-byte chars
 *		
 *	pat - in, use
 *		string pattern to search for
 *		
 *      patlen - in
 *          pattern length in 2-byte chars
 *		
 * Return value :
 *      first occurence of pat in s or NULL
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char2_t* SsWbufwbuf_unaligned(
        const ss_char2_t* s, size_t slen,
        const ss_char2_t* pat, size_t patlen)
{
        size_t s_i;
        size_t t_i;
        size_t p_i;

        s_i = 0;
        if (patlen == 0) {
            return ((ss_char2_t*)s);
        }
        for (;;) {
            t_i = s_i;
            p_i = 0;
            for (;;) {
                if (t_i >= slen) {
                    ss_dassert(t_i == slen);
                    return (NULL);
                }
                {
                    const ss_char2_t* t1;
                    const ss_char2_t* p1;

                    t1 = s + t_i;
                    p1 = pat + p_i;
                    if (SS_CHAR2_LOAD(t1) != SS_CHAR2_LOAD(p1)) {
                        break;
                    }
                }
                p_i++;
                t_i++;
                if (p_i >= patlen) {
                    ss_dassert(p_i == patlen);
                    return ((ss_char2_t*)s + s_i);
                }
            }
            s_i++;
        }
        ss_derror;
        return (NULL);
}

ss_char2_t* SsWcsdup(ss_char2_t* s)
{
        size_t len;
        ss_char2_t* d;

        len = SsWcslen(s);
        d = SsMemAlloc((len + 1) * sizeof(ss_char2_t));
        memcpy(d, s, (len + 1) * sizeof(ss_char2_t));
        return (d);
}
        
