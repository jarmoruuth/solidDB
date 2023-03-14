/*************************************************************************\
**  source       * sslcs.h
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


#ifndef SSLCS_H
#define SSLCS_H

#include "ssenv.h"
#include "ssstddef.h"
#include "ssc.h"
#include "ssutf.h"

size_t SsLcslen(
        ss_lchar_t* s);

ss_lchar_t* SsLcscpy(
        ss_lchar_t* dest,
        ss_lchar_t* src);

ss_lchar_t* SsLcscpyA(
        ss_lchar_t* dest,
        char* src);

int SsLcscmp(
        ss_lchar_t* s1,
        ss_lchar_t* s2);

int SsLcscmpA(
        ss_lchar_t* s1,
        char* s2);

int SsLcsicmp(
        const ss_lchar_t* s1,
        const ss_lchar_t* s2);

int SsLcsicmpA(
        const ss_lchar_t* s1,
        const char* s2);

ss_lchar_t* SsLcscat(
        ss_lchar_t* target,
        const ss_lchar_t* tail);

ss_lchar_t* SsLcscatA(
        ss_lchar_t* target,
        const char* tail);

ss_lchar_t* SsLcsncat(
        ss_lchar_t* target,
        const ss_lchar_t* tail,
        size_t n);

ss_lchar_t* SsLcsncatA(
        ss_lchar_t* target,
        const char* tail,
        size_t n);

ss_lchar_t* SsLcsncpy(
        ss_lchar_t * dst,
        const ss_lchar_t *s,
        size_t n);

ss_lchar_t* SsLcsncpyA(
        ss_lchar_t * dst,
        const char *s,
        size_t n);

int SsLcsncmp(
        const ss_lchar_t* s1,
        const ss_lchar_t* s2,
        size_t n);

int SsLcsncmpA(
        const ss_lchar_t* s1,
        const char* s2, size_t n);

size_t SsLcscspn(
        const ss_lchar_t* s1,
        const ss_lchar_t* s2);

size_t SsLcscspnA(
        const ss_lchar_t* s1,
        const char* s2);

ss_lchar_t* SsLcschr(
        const ss_lchar_t* s,
        ss_lchar_t c);

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
ss_lchar_t* SsLcstok_r(
        ss_lchar_t* base,
        ss_lchar_t* delim,
        ss_lchar_t** p_pos);

ss_lchar_t* SsLcsupr(
        ss_lchar_t* s);

ss_lchar_t* SsLcsdup(
        ss_lchar_t* s);

void SsLcb2Msb1stWbuf(
        ss_char2_t* dst,
        ss_lchar_t* src,
        size_t nchars);

void SsMsb1stWbuf2Lcb(
        ss_lchar_t* dst,
        ss_char2_t* src,
        size_t nchars);

bool SsMsb1stWbuf2Str(
        ss_char1_t* dst,
        ss_char2_t* s,
        size_t len);

size_t SsLcbByteLenAsUTF8(
        ss_lchar_t* src,
        size_t n);

ss_lchar_t* SsUTF8toLcsdup(
        ss_char1_t* UTF8_str);

SsUtfRetT SsUTF8toLcb(
        ss_lchar_t** p_dst,
        ss_lchar_t* dst_end,
        ss_byte_t** p_src,
        ss_byte_t* src_end);

SsUtfRetT SsLcbtoUTF8(
        ss_byte_t** p_dst,
        ss_byte_t* dst_end,
        ss_lchar_t** p_src,
        ss_lchar_t* src_end);

ss_byte_t* SsLcstoUTF8dup(
        ss_lchar_t* src);

void SsSbuf2Lcb(
        ss_lchar_t* d,
        ss_char1_t* s,
        size_t n);

void SsSbuf2Msb1stWbuf(
        ss_char2_t* d,
        ss_char1_t* s,
        size_t n);

#endif /* SSLCS_H */
