/*************************************************************************\
**  source       * ssutf.h
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


#ifndef SSUTF_H
#define SSUTF_H

#include "ssc.h"
#include "ssstddef.h"

typedef enum {
        SS_UTF_ERROR,
        SS_UTF_OK,
        SS_UTF_TRUNCATION,
        SS_UTF_NOCHANGE
} SsUtfRetT;

/* how many bytes per UTF8 char we support; later possibly upped to 6 */
#define SS_UTF8CHAR_BYTES 3
/* get the maximum number of bytes a UTF8 string of len n may need */
#define SS_MAXSTRLEN_UTF8(n)       ((n) * SS_UTF8CHAR_BYTES)
#define SS_MAXSTRLEN_UTF8_WNULL(n) (SS_MAXSTRLEN_UTF8(n) + 1)

SsUtfRetT SsUCS2toUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char2_t** p_src,
        ss_char2_t* src_end);

SsUtfRetT SsUCS2vatoUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char2_t** p_src,
        ss_char2_t* src_end);

SsUtfRetT SsUTF8toUCS2(
        ss_char2_t** p_dst,
        ss_char2_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end);

SsUtfRetT SsUTF8toUCS2va(
        ss_char2_t** p_dst,
        ss_char2_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end);

SsUtfRetT SsASCII8toUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_char1_t** p_src,
        ss_char1_t* src_end);

SsUtfRetT SsUTF8toASCII8(
        ss_char1_t** p_dst,
        ss_char1_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end);

bool SsUTF8isASCII8(
        ss_byte_t* src,
        size_t n,
        size_t* p_clen);

size_t SsUTF8CharLen(
        ss_byte_t* src,
        size_t n);

size_t SsUCS2ByteLenAsUTF8(
        ss_char2_t* src,
        size_t n);

size_t SsUCS2vaByteLenAsUTF8(
        ss_char2_t* src,
        size_t n);

size_t SsASCII8ByteLenAsUTF8(
        ss_char1_t* src,
        size_t n);

ss_char2_t* SsUTF8toUCS2Strdup(
        ss_char1_t* UTF8_str);

ss_char1_t* SsUCS2toUTF8Strdup(
        ss_char2_t* UCS2_str);

ss_char1_t* SsASCII8toUTF8Strdup(
        ss_char1_t* s);

void SsUTF8Strupr(
        ss_char1_t* s);

int SsUTF8Stricmp(
        ss_char1_t* s1,
        ss_char1_t* s2);

ss_char1_t* SsUTF8toASCII8Strdup(
        ss_char1_t* str);

#endif /* SSUTF_H */
