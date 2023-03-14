/*************************************************************************\
**  source       * ssltoa.h
**  directory    * ss
**  description  * long int to asciiz conversion functions
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


#ifndef SSLTOA_H
#define SSLTOA_H

#include "ssstddef.h"
#include "ssc.h"

/* do not use the below constant directly! */
extern const char ss_ltoa_int2ascii_xlat[36];

/* use this macro instead */
#define SS_LTOA_DIGITXLATE(i) \
        ss_ltoa_int2ascii_xlat[i]

size_t SsLongToAscii(
        long l,
        char* buf,
        uint radix,
        size_t width,
        char leftfillch,
        bool is_signed);

char* SsLtoa(
        long l,
        char* buf,
        int radix);

char* SsUltoa(
        ulong l,
        char* buf,
        int radix);

#endif /* SSLTOA_H */
