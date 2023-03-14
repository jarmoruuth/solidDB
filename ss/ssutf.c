/*************************************************************************\
**  source       * ssutf.c
**  directory    * ss
**  description  * UNICODE conversions from UCS-2 to UTF-8 and vice versa
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

#include "ssstring.h"
#include "ssdebug.h"
#include "ssmem.h"
#include "sswcs.h"
#include "sslimits.h"
#include "ssutf.h"
#include "ss1utf.h"

/*##**********************************************************************\
 * 
 *		SsUCS2toUTF8
 * 
 * Converts UCS-2 encoded (ss_char2_t) buffer to UTF-8 encoded
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
 *          on return pointer to first char2 that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in
 *		pointer to first char2 beyond the input data
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
SsUtfRetT SsUCS2toUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char2_t** p_src,
        ss_char2_t* src_end)
{
        ss_byte_t* dst;
        ss_char2_t* src;
        SsUtfRetT rc = SS_UTF_OK;

        dst = *p_dst;
        src = *p_src;

        while (src < src_end) {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = *src;
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
                ss_debug(default: ss_rc_error(dst_bytes);)
            }
            dst += dst_bytes;
        }
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsUCS2vatoUTF8
 * 
 * Same as SsUCS2toUTF8 but this one assumes the UCS2 buffer is
 * in MSB 1st byte order and possibly misaligned.
 * 
 * Parameters : 
 * 
 *	p_dst - 
 *		
 *		
 *	dst_end - 
 *		
 *		
 *	p_src - 
 *		
 *		
 *	src_end - 
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
SsUtfRetT SsUCS2vatoUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char2_t** p_src,
        ss_char2_t* src_end)
{
        ss_byte_t* dst;
        ss_char2_t* src;
        SsUtfRetT rc = SS_UTF_OK;

        dst = *p_dst;
        src = *p_src;

        while ((ss_byte_t*)src < (ss_byte_t*)src_end) {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = SS_CHAR2_LOAD_MSB1ST(src);
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
                ss_debug(default: ss_rc_error(dst_bytes);)
            }
            dst += dst_bytes;
        }
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsUTF8toUCS2
 * 
 * Converts UTF-8 encoded byte buffer to UCS-2 encoded char2 buffer
 * 
 * Parameters : 
 * 
 *	p_dst - in out, use
 *		pointer to pointer to beginning of destination buffer
 *          on return, pointer to the first char2 beyond the conversion
 *          result.
 *		
 *	dst_end - in, use
 *		pointer to first char2 beyond the destination buffer
 *		
 *	p_src - in out, use
 *		pointer to pointer to beginning of input buffer.
 *          on return pointer to first byte that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in, use
 *		pointer to first char2 beyond the input data
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
SsUtfRetT SsUTF8toUCS2(
        ss_char2_t** p_dst,
        ss_char2_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end)
{
        SsUtfRetT rc = SS_UTF_OK;
        ss_char2_t* dst;
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
            *dst++ = (ss_char2_t)ch;
        }
loop_exit:;
        ss_dassert(src_end >= src);
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsUTF8toUCS2va
 * 
 * Same as SsUTF8toUCS2 but this one assumes the UCS2 buffer is in
 * MSB 1st byte order and possibly misaligned.
 * 
 * Parameters : 
 * 
 *	p_dst - 
 *		
 *		
 *	dst_end - 
 *		
 *		
 *	p_src - 
 *		
 *		
 *	src_end - 
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
SsUtfRetT SsUTF8toUCS2va(
        ss_char2_t** p_dst,
        ss_char2_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end)
{
        SsUtfRetT rc = SS_UTF_OK;
        ss_char2_t* dst;
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
            if ((ss_byte_t*)dst >= (ss_byte_t*)dst_end) {
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
            SS_CHAR2_STORE_MSB1ST(dst, ch);
            dst++;
        }
loop_exit:;
        ss_dassert(src_end >= src);
        *p_src = src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsASCII8toUTF8
 * 
 * Converts and ASCII-8 (extended ASCII) buffer to UTF-8 encoded
 * buffer
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
 *          on return pointer to first char1 that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in, use
 *		pointer to pointer to first byte beyond input buffer.
 *		
 *		
 * Return value : 
 *      SS_UTF_OK - the whole buffer converted
 *      SS_UTF_TRUNCATION - the destination buffer was too small,
 *                          the *p_dst == dst_end.
 *      SS_UTF_NOCHANGE - OK, and the destination buffer is exactly
 *                        equal to the source buffer
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsUtfRetT SsASCII8toUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char1_t** p_src,
        ss_char1_t* src_end)
{
        ss_byte_t* dst;
        ss_byte_t* src;
        SsUtfRetT rc = SS_UTF_OK;
        size_t change = 0;

        dst = *p_dst;
        src = (ss_byte_t*)*p_src;

        while (src < (ss_byte_t*)src_end) {
            ss_char2_t ch;
            size_t dst_bytes;
            size_t bit7;

            ch = *src++;
            bit7 = ch >> 7;
            dst_bytes = 1 + bit7;
            change |= bit7;
            dst += dst_bytes;
            if (dst > dst_end) {
                dst -= dst_bytes;
                rc = SS_UTF_TRUNCATION;
                break;
            }
            switch (dst_bytes) {
                case 2:
                    dst--;
                    *dst = (ss_byte_t)((ch | SS_UTF8_BYTEMARK) & SS_UTF8_BYTEMASK);
                    ch >>= 6;
                    /* FALLTHROUGH */
                case 1:
                    dst--;
                    *dst = (ss_byte_t)ch | (ss_UTF8_1stbytemark-1)[dst_bytes];
                    break;
                ss_debug(default: ss_rc_error(dst_bytes);)
            }
            dst += dst_bytes;
        }
        if (rc == SS_UTF_OK && !change) {
            rc = SS_UTF_NOCHANGE;
        }
        *p_src = (ss_char1_t*)src;
        *p_dst = dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsUTF8toASCII8
 * 
 * Converts an UTF-8 encoded buffer to ASCII-8 buffer
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
 *          on return pointer to first char1 that was not converted
 *          (same as src_end if all converted)
 *		
 *	src_end - in, use
 *		pointer to pointer to beginning of input buffer.
 *          on return pointer to first char1 that was not converted
 *          (same as src_end if all converted)
 *		
 *		
 * Return value :
 *      SS_UTF_ERROR - the input buffer was not legal UTF-8 or
 *                     at least one of the characters were more
 *                     than 0xff in value
 *      SS_UTF_OK - the whole buffer converted
 *      SS_UTF_TRUNCATION - the destination buffer was too small,
 *                          the *p_dst == dst_end.
 *      SS_UTF_NOCHANGE - OK, and the destination buffer is exactly
 *                        equal to the source buffer
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SsUtfRetT SsUTF8toASCII8(
        ss_char1_t** p_dst,
        ss_char1_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end)
{
        SsUtfRetT rc = SS_UTF_OK;
        ss_byte_t* dst;
        ss_byte_t* src;
        bool change = FALSE;

        dst = (ss_byte_t*)*p_dst;
        src = *p_src;

        while (src < src_end) {
            ss_uint4_t ch = 0;
            size_t extrabytes;

            extrabytes = ss_UTF8_extrabytes[*src];
            if (src + extrabytes >= src_end) {
                rc = SS_UTF_ERROR;
                break;
            }
            if (dst >= (ss_byte_t*)dst_end) {
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
                    change = TRUE;
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
            if (ch > 0x00FFU) {
                src -= extrabytes + 1;
                rc = SS_UTF_ERROR;
                break;
            }
            ss_rc_dassert(ch <= 0xFFFFUL, ch);
            *dst++ = (ss_byte_t)ch;
        }
loop_exit:;
        ss_dassert(src_end >= src);
        if (rc == SS_UTF_OK && !change) {
            rc = SS_UTF_NOCHANGE;
        }
        *p_src = src;
        *p_dst = (ss_char1_t*)dst;
        return (rc);
}

/*##**********************************************************************\
 * 
 *		SsUTF8isASCII8
 * 
 * Checks if a String buffer encoded in UTF-8 can be represented
 * as ASCII-8 as well
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		UTF-8 buffer
 *		
 *	n - in
 *		number of bytes in the buffer
 *
 *      p_clen - out, use
 *          pointer to number of characters or NULL
 *          if that information is not needed
 *		
 * Return value :
 *      TRUE all characters are <= 0xFF
 *      FALSE at least one char > 0xFF
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool SsUTF8isASCII8(ss_byte_t* src, size_t n, size_t* p_clen)
{
        ss_byte_t* src_end;
        size_t clen;
        size_t doesnotfitto8bits;

        clen = 0;
        src_end = src + n;
        doesnotfitto8bits = 0;

        
        while (src < src_end) {
            ss_uint4_t ch = 0;
            size_t extrabytes;

            extrabytes = ss_UTF8_extrabytes[*src];
            if (src + extrabytes >= src_end) {
                ss_derror;
                return (FALSE);
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
                    doesnotfitto8bits = 1;
                    goto loop_exit;
            }
            ch -= ss_UTF8_offsets[extrabytes];
            doesnotfitto8bits |= ch & ~0x00FFU;
            clen++;
            ss_rc_dassert(ch <= 0xFFFFUL, ch);
        }
loop_exit:;
        if (p_clen != NULL) {
            *p_clen = clen;
        }
        return (doesnotfitto8bits == 0);
}


/*##**********************************************************************\
 * 
 *		SsUCS2ByteLenAsUTF8
 * 
 * Measures how many bytes an UCS-2 encoded buffer would require if it
 * were encoded in UTF-8
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		UCS-2 encoded buffer
 *		
 *	n - in
 *		number of char2's contained in src
 *		
 * Return value :
 *      number of byte needed for UTF-8 encoding the same data
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsUCS2ByteLenAsUTF8(ss_char2_t* src, size_t n)
{
        size_t blen;
        ss_char2_t* src_end;

        for (src_end = src + n, blen = 0; src < src_end; src++) {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = *src;
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

/*##**********************************************************************\
 * 
 *		SsUCS2vaByteLenAsUTF8
 * 
 * Same as SsUCS2ByteLenAsUTF8 but this takes the UCS2 buffer
 * in MSB 1st format and possibly misaligned.
 * 
 * Parameters : 
 * 
 *	src - 
 *		
 *		
 *	n - 
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
size_t SsUCS2vaByteLenAsUTF8(ss_char2_t* src, size_t n)
{
        size_t blen;
        ss_char2_t* src_end;

        for (src_end = src + n, blen = 0;
             (ss_byte_t*)src < (ss_byte_t*)src_end;
             src++)
        {
            ss_char2_t ch;
            size_t dst_bytes;

            ch = SS_CHAR2_LOAD_MSB1ST(src);
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

/*##**********************************************************************\
 * 
 *		SsASCII8ByteLenAsUTF8
 * 
 * Measures how many bytes an ASCII-8 encoded buffer would require if it
 * were encoded in UTF-8
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		ASCII-8 encoded buffer
 *		
 *	n - in
 *		number of char1's contained in src
 *		
 * Return value :
 *      number of byte needed for UTF-8 encoding the same data
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsASCII8ByteLenAsUTF8(ss_char1_t* src, size_t n)
{
        size_t blen;
        ss_char1_t* src_end;

        for (src_end = src + n, blen = 0; src < src_end; src++) {
            blen += 1 + (((size_t)*(ss_byte_t*)src) >> 7U);
        }
        return (blen);
}

ss_char2_t* SsUTF8toUCS2Strdup(ss_char1_t* UTF8_str)
{
        ss_byte_t* src_tmp;
        ss_char2_t* dst_tmp;
        ss_char2_t* UCS2_str;
        size_t src_size;
        size_t dst_nchars;
        ss_debug(SsUtfRetT utfrc;)

        src_size = strlen(UTF8_str) + 1;
        dst_nchars = SsUTF8CharLen((ss_byte_t*)UTF8_str, src_size);
        UCS2_str = dst_tmp = SsMemAlloc(dst_nchars * sizeof(ss_char2_t));
        src_tmp = (ss_byte_t*)UTF8_str;
        ss_debug(utfrc =)
        SsUTF8toUCS2(&dst_tmp, dst_tmp + dst_nchars,
                     &src_tmp, src_tmp + src_size);
        ss_rc_dassert(utfrc == SS_UTF_OK, utfrc);
        return (UCS2_str);
}

ss_char1_t* SsUCS2toUTF8Strdup(ss_char2_t* UCS2_str)
{
        size_t src_len;
        size_t dst_size;
        ss_char1_t* UTF8_str;
        ss_byte_t* dst_tmp;
        ss_char2_t* src_tmp;
        ss_debug(SsUtfRetT utfrc;)

        src_len = SsWcslen(UCS2_str) + 1;
        dst_size = SsUCS2ByteLenAsUTF8(UCS2_str, src_len);
        UTF8_str = SsMemAlloc(dst_size);
        dst_tmp = (ss_byte_t*)UTF8_str;
        src_tmp = UCS2_str;
        ss_debug(utfrc =)
        SsUCS2toUTF8(
                &dst_tmp, dst_tmp + dst_size,
                &src_tmp, UCS2_str + src_len);
        ss_rc_dassert(utfrc == SS_UTF_OK, utfrc);
        return (UTF8_str);
}

ss_char1_t* SsASCII8toUTF8Strdup(ss_char1_t* s)
{
        size_t s_size;
        size_t d_size;
        ss_char1_t* s_tmp = s;
        ss_byte_t* d;
        ss_byte_t* d_tmp;
        ss_debug(SsUtfRetT utfrc;)

        s_size = strlen(s) + 1;
        d_size = SsASCII8ByteLenAsUTF8(s, s_size);
        d = d_tmp = SsMemAlloc(d_size);
        ss_debug(utfrc =)
        SsASCII8toUTF8(&d_tmp, d + d_size, &s_tmp, s + s_size);
        ss_rc_dassert(utfrc == SS_UTF_OK || utfrc == SS_UTF_NOCHANGE, utfrc);
        ss_dassert(d_tmp == d + d_size)
        return ((ss_char1_t*)d);
}

/*##**********************************************************************\
 * 
 *          SsUTF8Strupr
 * 
 * Uppercases ASCII characters from UTF-8 encoded string
 * and leaves all non-ASCII characters untouched.
 * 
 * 
 * Parameters : 
 * 
 *	s - use
 *		UTF-8 encoded nul-terminated string
 *		
 * Return value :
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void SsUTF8Strupr(ss_char1_t* s)
{
        ss_byte_t* p;
        uint c;

        for (p = (ss_byte_t*)s; ; p++) {
            c = (uint)*p;
            if ((uint)(c - 1U) >= 0x007FU) {
                if (c == 0U) {
                    return;
                }
            } else {
                *p = ss_toupper(c);
            }
        }
}

/*##**********************************************************************\
 * 
 *		SsUTF8Stricmp
 * 
 * Compare two UTF-8 strings case insensitive. Only ASCII (7-bit) characters
 * are actually treated case insensitive.
 * 
 * Parameters : 
 * 
 *	s1 - use
 *		UTF-8 encoded nul-terminated string
 *		
 *	s2 - use
 *		UTF-8 encoded nul-terminated string
 *		
 * Return value : 
 * 
 *      -1, 0 or 1
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
int SsUTF8Stricmp(ss_char1_t* s1, ss_char1_t* s2)
{
        ss_byte_t* p1;
        ss_byte_t* p2;
        uint c1;
        uint c2;

        p1 = (ss_byte_t*)s1;
        p2 = (ss_byte_t*)s2;

        for (; ; p1++, p2++) {
            c1 = (uint)*p1;
            c2 = (uint)*p2;
            if ((uint)(c1 - 1U) < 0x007FU) {
                c1 = ss_toupper(c1);
            }
            if ((uint)(c2 - 1U) < 0x007FU) {
                c2 = ss_toupper(c2);
            }
            if (c1 == 0 || c1 != c2) {
                break;
            }
        }
        if (c1 < c2) {
            return(-1);
        } else if (c1 > c2) {
            return(1);
        } else {
            return(0);
        }
}

/*##**********************************************************************\
 * 
 *		SsUTF8toASCII8Strdup
 * 
 * Copies UTF-8 encoded input buffer to a new buffer and returns pointer to it
 * 
 * Parameters : 
 * 
 *	str - in, use
 *		pointer to UTF-8 buffer that has to be copied
 *
 *	utf8 - in, use
 *		if TRUE, defines that output string has to be UTF-8
 *      otherwise output string is converted to ASCII8
 *		
 * Return value :
 *		pointer to a new string if copy (and conversion) was successful
 *      otherwise NULL
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_char1_t* SsUTF8toASCII8Strdup(ss_char1_t* str)
{
        ss_char1_t* str_out;

        SsUtfRetT utfrc;
        size_t orig_size = strlen(str) + 1;
        size_t dest_size = SsUTF8CharLen((ss_byte_t*)str, orig_size);
        ss_byte_t* src_tmp = (ss_byte_t*)str;
        ss_char1_t* dest_tmp;

        str_out = SsMemAlloc(dest_size);
        dest_tmp = str_out;
        utfrc = SsUTF8toASCII8(&dest_tmp, dest_tmp + dest_size,
                               &src_tmp, src_tmp + orig_size);
        if (utfrc != SS_UTF_OK && utfrc != SS_UTF_NOCHANGE) {
            SsMemFree(str_out);
            return (NULL);
        }
        return (str_out);
}
