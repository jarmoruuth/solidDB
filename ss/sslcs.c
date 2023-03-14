/*************************************************************************\
**  source       * sslcs.c
**  directory    * ss
**  description  * Long character string routines.
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
#include "ssstddef.h"
#include "ssstring.h"

#include "ssc.h"
#include "ssmem.h"
#include "ssdebug.h"
#include "sslimits.h"
#include "sswctype.h"
#include "sslcs.h"
#include "ss1utf.h"
#include "ssutf.h"


/*##**********************************************************************\
 * 
 *		SsLcscat
 * 
 * Wide-char version of strcat()
 * 
 * Parameters : 
 * 
 *	target - in out, use
 *		target string (with SsLcslen(tail) extra space)
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
ss_lchar_t* SsLcscat(ss_lchar_t* target, const ss_lchar_t* tail)
{
        ss_lchar_t* t = target;
        while (*t) {
            t++;
        }
        while ((*t = *tail) != (ss_lchar_t)0) {
            t++;
            tail++;
        }
        return (target);
}

ss_lchar_t* SsLcscatA(ss_lchar_t* target, const char* tail)
{
        ss_lchar_t* t = target;
        while (*t) {
            t++;
        }
        while ((*t = *tail) != (ss_lchar_t)0) {
            t++;
            tail++;
        }
        return (target);
}

/*##**********************************************************************\
 * 
 *		SsLcsncat
 * 
 * Wide-char version of strncat(). Note the difference
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
ss_lchar_t* SsLcsncat(ss_lchar_t* target, const ss_lchar_t* tail, size_t n)
{
        ss_lchar_t* t = target;

        while (*t != (ss_lchar_t)0) {
            t++;
        }
        while (n != 0) {
            if ((*t = *tail) == (ss_lchar_t)0) {
                return (target);
            }
            n--;
            t++;
            tail++;
        }
        *t = (ss_lchar_t)0;
        return (target);
}

ss_lchar_t* SsLcsncatA(ss_lchar_t* target, const char* tail, size_t n)
{
        ss_lchar_t* t = target;

        while (*t != (ss_lchar_t)0) {
            t++;
        }
        while (n != 0) {
            if ((*t = *tail) == (ss_lchar_t)0) {
                return (target);
            }
            n--;
            t++;
            tail++;
        }
        *t = (ss_lchar_t)0;
        return (target);
}

/*##**********************************************************************\
 * 
 *		SsLcsncpy
 * 
 * Wide-char version of strncpy()
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
 *      behavior when n <= SsLcslen(s) the target is left unterminated!
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_lchar_t* SsLcsncpy(ss_lchar_t * dst, const ss_lchar_t *s, size_t n)
{
        ss_lchar_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);

        while (n != 0 && (*d = *s) != (ss_lchar_t)0) {
            n--;
            d++;
            s++;
        }
        return (dst);
}

ss_lchar_t* SsLcsncpyA(ss_lchar_t * dst, const char *s, size_t n)
{
        ss_lchar_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);

        while (n != 0 && (*d = *s) != (ss_lchar_t)0) {
            n--;
            d++;
            s++;
        }
        return (dst);
}

/*##**********************************************************************\
 * 
 *		SsLcsncmp
 * 
 * Wide-char version of strncmp()
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
int SsLcsncmp(const ss_lchar_t* s1, const ss_lchar_t* s2, size_t n)
{
        for (;n != 0; n--, s1++, s2++) {
            ss_lchar_t c1 = *s1;
            if (sizeof(int) <= sizeof(ss_lchar_t)) {
                ss_lchar_t c2 = *s2;
                /* integer is no bigger size than ss_lchar_t */
                if (c1 != c2) {
                    if (c1 < c2) {
                        return (-1);
                    }
                    return (1);
                }
            } else {
                /* integer is bigger type than ss_lchar_t */
                int cmp = (int)c1 - (int)*s2;
                if (cmp != 0) {
                    return (cmp);
                }
            }
            if (c1 == (ss_lchar_t)0) {
                ss_rc_dassert(*s2 == (ss_lchar_t)0, *s2);
                break;
            }
        }
        return (0);
}

int SsLcsncmpA(const ss_lchar_t* s1, const char* s2, size_t n)
{
        for (;n != 0; n--, s1++, s2++) {
            ss_lchar_t c1 = *s1;
            if (sizeof(int) <= sizeof(ss_lchar_t)) {
                ss_lchar_t c2 = *s2;
                /* integer is no bigger size than ss_lchar_t */
                if (c1 != c2) {
                    if (c1 < c2) {
                        return (-1);
                    }
                    return (1);
                }
            } else {
                /* integer is bigger type than ss_lchar_t */
                int cmp = (int)c1 - (int)*s2;
                if (cmp != 0) {
                    return (cmp);
                }
            }
            if (c1 == (ss_lchar_t)0) {
                ss_rc_dassert(*s2 == (ss_lchar_t)0, *s2);
                break;
            }
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		SsLcscspn
 * 
 * Wide-char version of strcspn()
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
size_t SsLcscspn(const ss_lchar_t* s1, const ss_lchar_t* s2)
{
        size_t pos;

        for (pos = 0; *s1 != (ss_lchar_t)0; s1++, pos++) {
            const ss_lchar_t* p;
            for (p = s2; *p != (ss_lchar_t)0; p++) {
                if (*s1 == *p) {
                    return (pos);
                }
            }
        }
        return pos;
}

size_t SsLcscspnA(const ss_lchar_t* s1, const char* s2)
{
        size_t pos;

        for (pos = 0; *s1 != (ss_lchar_t)0; s1++, pos++) {
            const char* p;
            for (p = s2; *p != (ss_lchar_t)0; p++) {
                if (*s1 == *p) {
                    return (pos);
                }
            }
        }
        return pos;
}

ss_lchar_t* SsLcsupr(ss_lchar_t* s)
{
        ss_lchar_t* p;

        for (p = s; *p != (ss_lchar_t)0; p++) {
            *p = (ss_lchar_t)ss_towupper((ss_char2_t)*p);
        }
        return s;
}

ss_lchar_t* SsLcsdup(ss_lchar_t* s)
{
        size_t size = (SsLcslen(s) + 1) * sizeof(ss_lchar_t);
        ss_lchar_t* d = SsMemAlloc(size);
        memcpy(d, s, size);
        return (d);
}

size_t SsLcslen(ss_lchar_t* s)
{
        size_t len;

        for (len = 0; s[len] != (ss_lchar_t)0; len++) {
        }
        return (len);
}

ss_lchar_t* SsLcscpy(ss_lchar_t* dest, ss_lchar_t* src)
{
        ss_lchar_t* d = dest;

        while ((*d = *src) != (ss_lchar_t)0) {
            src++;
            d++;
        }
        return (dest);
}

ss_lchar_t* SsLcscpyA(ss_lchar_t* dest, char* src)
{
        ss_lchar_t* d = dest;

        while ((*d = *src) != (ss_lchar_t)0) {
            src++;
            d++;
        }
        return (dest);
}

int SsLcscmp(ss_lchar_t* s1, ss_lchar_t* s2)
{
        int cmp = 0;
        ss_lchar_t c1;

        do {
            c1 = *s1;
            if (sizeof(ss_lchar_t) == 2) {
                if (sizeof(ss_lchar_t) < sizeof(int)) {
                    cmp = (int)(ss_uint2_t)c1 - (int)(ss_uint2_t)*s2;
                    if (cmp != 0) {
                        break;
                    }
                } else {
                    ss_uint2_t c2 = (ss_uint2_t)*s2;
                    if ((ss_uint2_t)c1 < c2) {
                        cmp = -1;
                        break;
                    } else if ((ss_uint2_t)c1 > c2) {
                        cmp = 1;
                        break;
                    }
                    ss_dassert((ss_uint2_t)c1 == c2);
                }
            } else {
                ss_uint4_t c2 = (ss_uint4_t)*s2;;
                ss_dassert(sizeof(ss_lchar_t) == 4);
                if ((ss_uint4_t)c1 < c2) {
                    cmp = -1;
                    break;
                } else if ((ss_uint4_t)c1 > c2) {
                    cmp = 1;
                    break;
                }
                ss_dassert((ss_uint4_t)c1 == c2);
            }
            s1++;
            s2++;
        } while (c1 != (ss_lchar_t)0);
        return (cmp);
}

int SsLcscmpA(ss_lchar_t* s1, char* s2)
{
        int cmp = 0;
        ss_lchar_t c1;

        do {
            c1 = *s1;
            if (sizeof(ss_lchar_t) == 2) {
                if (sizeof(ss_lchar_t) < sizeof(int)) {
                    cmp = (int)(ss_uint2_t)c1 - (int)(ss_uint2_t)*s2;
                    if (cmp != 0) {
                        break;
                    }
                } else {
                    ss_uint2_t c2 = (ss_uint2_t)*s2;
                    if ((ss_uint2_t)c1 < c2) {
                        cmp = -1;
                        break;
                    } else if ((ss_uint2_t)c1 > c2) {
                        cmp = 1;
                        break;
                    }
                    ss_dassert((ss_uint2_t)c1 == c2);
                }
            } else {
                ss_uint4_t c2 = (ss_uint4_t)*s2;
                ss_dassert(sizeof(ss_lchar_t) == 4);
                if ((ss_uint4_t)c1 < c2) {
                    cmp = -1;
                    break;
                } else if ((ss_uint4_t)c1 > c2) {
                    cmp = 1;
                    break;
                }
                ss_dassert((ss_uint4_t)c1 == c2);
            }
            s1++;
            s2++;
        } while (c1 != (ss_lchar_t)0);
        return (cmp);
}

int SsLcsicmp(const ss_lchar_t* s1, const ss_lchar_t* s2)
{
        int cmp;

        for (;;s1++, s2++) {
            if (sizeof(int) <= sizeof(ss_lchar_t)) {
                ss_lchar_t c1 = ss_towupper(*s1);
                ss_lchar_t c2 = ss_towupper(*s2);

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
            if (*s1 == (ss_lchar_t)0) {
                break;
            }
        }
        return (0);
}

int SsLcsicmpA(const ss_lchar_t* s1, const char* s2)
{
        int cmp;

        for (;;s1++, s2++) {
            if (sizeof(int) <= sizeof(ss_lchar_t)) {
                ss_lchar_t c1 = ss_towupper(*s1);
                ss_lchar_t c2 = ss_towupper(*s2);

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
            if (*s1 == (ss_lchar_t)0) {
                break;
            }
        }
        return (0);
}

/*##**********************************************************************\
 * 
 *		SsLcschr
 * 
 * Wide-char version of strchr()
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
ss_lchar_t* SsLcschr(const ss_lchar_t* s, ss_lchar_t c)
{
        ss_dassert(s != NULL);
        if (s != NULL) {
            for (;;s++) {
                if (*s == c) {
                    return ((ss_lchar_t*)s);
                }
                if (*s == (ss_lchar_t)0) {
                    break;
                }
            }
        }
        return (NULL);
}

/*##**********************************************************************\
 * 
 *          SsLcstok_r
 *
 * Wide-char version of strtok_r() (which, in turn, is a re-entrant
 * version of strtok()
 * 
 * Parameters : 
 * 
 *      base - in out, use
 *          the string where to scan (note: this function modifies the
 *          contents of the base string)
 *
 *      delim - in, use
 *          character to search for
 *
 *      p_pos = in out, use
 *          a pointer to variable in the user scope to retain re-entrancy.
 *          Note: the caller only needs to supply a pointer to a local
 *          pointer variable; no need to initialize its contents.
 *          It serves just as a scan position placeholder for this function.
 *          The variable pointed to by p_pos must remain in scope and its
 *          contents must not be modified between successive calls to
 *          SsLcstok_r(NULL, delim, &pos)
 *
 * Return value - ref :
 *      pointer to next scanned token or NULL when end-of-string reached
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_lchar_t* SsLcstok_r(ss_lchar_t* base, ss_lchar_t* delim, ss_lchar_t** p_pos)
{
        ss_lchar_t c;
        ss_lchar_t* end;
                
        ss_bassert(p_pos != NULL);
        if (base == NULL) {
            base = *p_pos;
        }
        for (;; base++) {
            c = *base;
            if (c == (ss_lchar_t)0) {
                *p_pos = base;
                return (NULL);
            }
            if (SsLcschr(delim, c) == NULL) {
                break; /* first non-delimiter found */
            }
        }
        for (end = base + 1; ; end++) {
            c = *end;
            if (c == (ss_lchar_t)0) {
                *p_pos = end;
                break;
            }
            if (SsLcschr(delim, c) != NULL) {
                *p_pos = end + 1;
                *end = (ss_lchar_t)0;
                break;
            }
        }
        return (base);
}
                   
void SsLcb2Msb1stWbuf(ss_char2_t* dst, ss_lchar_t* src, size_t nchars)
{
        while (nchars) {
            ss_char2_t ch = (ss_char2_t)*src;
            nchars--;
            src++;
            SS_CHAR2_STORE_MSB1ST(dst, ch);
            dst++;
        }
}

void SsMsb1stWbuf2Lcb(ss_lchar_t* dst, ss_char2_t* src, size_t nchars)
{
        if (sizeof(ss_lchar_t) > sizeof(ss_char2_t) &&
            (ss_byte_t*)dst == (ss_byte_t*)src)
        {
            /* convert in-place: copy right to left */
            src += nchars;
            dst += nchars;
            while (nchars) {
                src--;
                dst--;
                nchars--;
                *dst = (ss_lchar_t)SS_CHAR2_LOAD_MSB1ST(src);
            }
        } else {
            while (nchars) {
                *dst = (ss_lchar_t)SS_CHAR2_LOAD_MSB1ST(src);
                nchars--;
                src++;
                dst++;
            }
        }
}

bool SsMsb1stWbuf2Str(ss_char1_t* dst, ss_char2_t* s, size_t len)
{
        bool succp = TRUE;
        ss_char1_t* d = dst;

        ss_dassert(d != NULL);
        ss_dassert(s != NULL);
        for (;len ; len--, d++, s++) {
            ss_char2_t c = SS_CHAR2_LOAD_MSB1ST(s);
            if (c & (ss_char2_t)~0x00ff) {
                succp = FALSE;
                *d = 0xFF;
            } else {
                *d = (ss_char1_t)c;
            }
            ss_dassert(c != (ss_char2_t)0 || len == 1);
        }
        return (succp);
}

/*##**********************************************************************\
 * 
 *      SsLcbByteLenAsUTF8
 * 
 * Measures how many bytes an UCS-2 encoded buffer would require if it
 * were encoded in UTF-8
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		wchar_t (=ss_lchar_t) encoded buffer
 *		
 *	n - in
 *		number of characters contained in src
 *		
 * Return value :
 *      number of bytes needed for UTF-8 encoding the same data
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsLcbByteLenAsUTF8(ss_lchar_t* src, size_t n)
{
        size_t blen;
        ss_lchar_t* src_end;

        for (src_end = src + n, blen = 0; src < src_end; src++) {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = (ss_char2_t)*src;
            dst_bytes = SS_UTF8_BYTES(ch);
#ifdef SS_DEBUG
            if (ch < 0x80U) {
                ss_assert(dst_bytes == 1);
            } else if (ch < 0x800U) {
                ss_assert(dst_bytes == 2);
            } else {
                ss_assert(dst_bytes == 3);
            }
#endif /* SS_DEBUG */
            blen += dst_bytes;
        }
        return (blen);
}

ss_lchar_t* SsUTF8toLcsdup(ss_char1_t* UTF8_str)
{
        ss_byte_t* src_tmp;
        ss_lchar_t* dst_tmp;
        ss_lchar_t* Lcstring;
        size_t src_size;
        size_t dst_nchars;
        ss_debug(SsUtfRetT utfrc;)

        src_size = strlen(UTF8_str) + 1;
        dst_nchars = SsUTF8CharLen((ss_byte_t*)UTF8_str, src_size);
        Lcstring = dst_tmp = SsMemAlloc(dst_nchars * sizeof(ss_lchar_t));
        src_tmp = (ss_byte_t*)UTF8_str;
        ss_debug(utfrc =)
        SsUTF8toLcb(&dst_tmp, dst_tmp + dst_nchars,
                     &src_tmp, src_tmp + src_size);
        ss_rc_dassert(utfrc == SS_UTF_OK, utfrc);
        return (Lcstring);
}

/*##**********************************************************************\
 * 
 *		SsUTF8toLcb
 * 
 * Converts UTF-8 encoded byte buffer to UNICODE encoded lchar buffer
 * 
 * Parameters : 
 * 
 *	p_dst - in out, use
 *		pointer to pointer to beginning of destination buffer
 *          on return, pointer to the first lchar beyond the conversion
 *          result.
 *		
 *	dst_end - in, use
 *		pointer to first lchar beyond the destination buffer
 *		
 *	p_src - in out, use
 *		pointer to pointer to beginning of input buffer.
 *          on return pointer to first byte that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in, use
 *		pointer to first lchar beyond the input data
 *		
 * Return value : 
 *      SS_UTF_ERROR - the input buffer was not legal UTF-8
 *      SS_UTF_OK - whole buffer converted
 *      SS_UTF_TRUNCATION - the destination buffer was too small
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsUtfRetT SsUTF8toLcb(
        ss_lchar_t** p_dst,
        ss_lchar_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end)
{
        SsUtfRetT rc = SS_UTF_OK;
        ss_lchar_t* dst;
        ss_byte_t* src;

        dst = *p_dst;
        src = *p_src;

        while (src < src_end) {
            ss_uint4_t ch = 0;
            size_t extrabytes;

            extrabytes = ss_UTF8_extrabytes[*src];
            if (src + extrabytes >= src_end) {
                rc = SS_UTF_ERROR;
                break;
            }
            if (dst >= dst_end) {
                rc = SS_UTF_TRUNCATION;
                break;
            }
            switch (extrabytes) {
                case 2:
                    ch += *src++;
                    ch <<= 6;
                    /* FALLTHROUGH */
                case 1:
                    ch += *src++;
                    ch <<= 6;
                    /* FALLTHROUGH */
                case 0:
                    ch += *src++;
                    break;
                default:
                    ss_rc_derror(extrabytes);
                    rc = SS_UTF_ERROR;
                    goto loop_exit;
            }
            ch -= ss_UTF8_offsets[extrabytes];
            ss_rc_dassert(ch <= 0xFFFFUL, ch);
            *dst++ = (ss_lchar_t)ch;
        }
loop_exit:;
        ss_dassert(src_end >= src);
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsLcbtoUTF8
 * 
 * Converts UNICODE encoded (ss_lchar_t) buffer to UTF-8 encoded
 * (ss_byte_t or ss_char1_t) buffer.
 * 
 * 
 * Parameters : 
 * 
 *	p_dst - in out, use
 *		pointer to pointer to beginning of destination buffer
 *          on return, pointer to the first byte beyond the conversion
 *          result.
 *		
 *	dst_end - in, use
 *		pointer to first byte beyond the buffer.
 *		
 *	p_src - in out, use
 *		pointer to pointer to beginning of input buffer.
 *          on return pointer to first lchar that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in
 *		pointer to first lchar beyond the input data
 *		
 * Return value :
 *      SS_UTF_OK - the whole buffer converted
 *      SS_UTF_TRUNCATION - the destination buffer was too small,
 *      the *p_dst == dst_end
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsUtfRetT SsLcbtoUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_lchar_t** p_src,
        ss_lchar_t* src_end)
{
        ss_byte_t* dst;
        ss_lchar_t* src;
        SsUtfRetT rc = SS_UTF_OK;

        dst = *p_dst;
        src = *p_src;

        while (src < src_end) {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = (ss_char2_t)*src;
            dst_bytes = SS_UTF8_BYTES(ch);
#ifdef SS_DEBUG
            if (ch < 0x80U) {
                ss_assert(dst_bytes == 1);
            } else if (ch < 0x800U) {
                ss_assert(dst_bytes == 2);
            } else {
                ss_assert(dst_bytes == 3);
            }
#endif /* SS_DEBUG */
            dst += dst_bytes;
            src++;
            if (dst > dst_end) {
                dst -= dst_bytes;
                rc = SS_UTF_TRUNCATION;
                break;
            }
            switch (dst_bytes) {
                case 3:
                    dst--;
                    *dst = (ss_byte_t)((ch | SS_UTF8_BYTEMARK) & SS_UTF8_BYTEMASK);
                    ch >>= 6;
                    /* FALLTHROUGH */
                case 2:
                    dst--;
                    *dst = (ss_byte_t)((ch | SS_UTF8_BYTEMARK) & SS_UTF8_BYTEMASK);
                    ch >>= 6;
                    /* FALLTHROUGH */
                case 1:
                    dst--;
                    *dst = (ss_byte_t)ch | (ss_UTF8_1stbytemark-1)[dst_bytes];
                    break;
                ss_debug(
                default: ss_rc_error(dst_bytes);)
            }
            dst += dst_bytes;
        }
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

ss_byte_t* SsLcstoUTF8dup(
        ss_lchar_t* src)
{
        ss_byte_t*  p_dst;
        ss_byte_t*  dst;
        ss_lchar_t* p_src;
        size_t      srclen;
        size_t      size_as_UTF8;

        ss_debug(SsUtfRetT utfrc;)

        srclen = SsLcslen(src);
        size_as_UTF8 = SsLcbByteLenAsUTF8(src, srclen);
        dst = (ss_byte_t*)SsMemAlloc((size_as_UTF8+1)*sizeof(ss_byte_t));

        p_dst = dst; p_src = src;
        ss_debug(utfrc =)
        SsLcbtoUTF8(&p_dst, p_dst + size_as_UTF8, &p_src, p_src + srclen);

        ss_rc_dassert(utfrc == SS_UTF_OK, utfrc);
        dst[size_as_UTF8] = (ss_byte_t)0;

        return dst;
}

void SsSbuf2Lcb(ss_lchar_t* d, ss_char1_t* s, size_t n)
{
        size_t i;
        
        for (i = 0; i < n; i++) {
            d[i] = (ss_lchar_t)(ss_byte_t)s[i];
        }
}

void SsSbuf2Msb1stWbuf(ss_char2_t* d, ss_char1_t* s, size_t n)
{
        if ((ss_byte_t*)d == (ss_byte_t*)s) {
            for (s +=n, d += n; n != 0; ) {
                ss_char2_t c;
                
                s--;
                d--;
                n--;
                c = (ss_char2_t)(ss_byte_t)*s;
                SS_CHAR2_STORE_MSB1ST(d, c);
            }
        } else {
            for (; n != 0; n--, d++, s++) {
                ss_char2_t c = (ss_char2_t)(ss_byte_t)*s;
                SS_CHAR2_STORE_MSB1ST(d, c);
            }
        }
}
