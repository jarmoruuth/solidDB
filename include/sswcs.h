/*************************************************************************\
**  source       * sswcs.h
**  directory    * ss
**  description  * Wide character string (wcs) support routines
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


#ifndef SSWCS_H
#define SSWCS_H

#include "ssc.h"
#include "ssstddef.h"

extern ss_char1_t SsWcs2StrDefChar;
#define SsWcs2StrGetDefChar() (SsWcs2StrDefChar)

void SsWcs2StrSetDefChar(ss_char1_t c);

ss_char2_t* SsWcscat(ss_char2_t *target, const ss_char2_t* tail);
ss_char2_t* SsWcschr(const ss_char2_t* s, ss_char2_t c);
int SsWcscmp(const ss_char2_t* s1, const ss_char2_t* s2);
ss_char2_t* SsWcscpy(ss_char2_t* dst, const ss_char2_t* s);
size_t SsWcscspn(const ss_char2_t* s1, const ss_char2_t* s2);
size_t SsWcslen(const ss_char2_t * s);
ss_char2_t* SsWcsncat(ss_char2_t* target, const ss_char2_t* tail, size_t n);
int SsWcsncmp(const ss_char2_t* s1, const ss_char2_t* s2, size_t n);
ss_char2_t* SsWcsncpy(ss_char2_t * dst, const ss_char2_t *s, size_t n);
ss_char2_t* SsWcspbrk(const ss_char2_t* s1, const ss_char2_t* s2);
ss_char2_t* SsWcsrchr(const ss_char2_t* s, ss_char2_t c);
size_t SsWcsspn(const ss_char2_t* s1, const ss_char2_t* s2);
ss_char2_t* SsWcswcs(const ss_char2_t* s, const ss_char2_t* pat);
int SsWcsicmp(const ss_char2_t* s1, const ss_char2_t* s2);
ss_char2_t* SsWcslwr(ss_char2_t* s);
ss_char2_t* SsWcsupr(ss_char2_t* s);

bool SsWbuf2Str(ss_char1_t* dst, ss_char2_t* s, size_t len);
bool SsvaWbuf2Str(ss_char1_t* dst, ss_char2_t* s, size_t len);
void SsSbuf2Wcs(ss_char2_t* dst, ss_char1_t* s, size_t len);
bool SsWcs2Str(ss_char1_t* dst, const ss_char2_t* s);
void SsStr2Wcs(ss_char2_t* d, ss_char1_t* s);
void SsStr2WcsInPlace(void* p);
bool SsWcs2StrInPlace(void* p);

ss_char2_t* SsWbufwbuf_unaligned(
        const ss_char2_t* s, size_t slen,
        const ss_char2_t* pat, size_t patlen);

void SsSbuf2Wcs_msb1st(
        ss_char2_t* dst,
        ss_char1_t* s,
        size_t len);

void SsWbuf2vaWbuf(
        ss_char2_t* dst,
        ss_char2_t* src,
        size_t n);

void SsvaWbuf2Wbuf(
        ss_char2_t* dst,
        ss_char2_t* src,
        size_t n);

ss_char2_t* SsWcsdup(
        ss_char2_t* s);

#endif /* SSWCS_H */
