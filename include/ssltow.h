/*************************************************************************\
**  source       * ssltow.h
**  directory    * ss
**  description  * Wide char versions for long integer to string
**               * conversion
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


#ifndef SSLTOW_H
#define SSLTOW_H

#include "ssc.h"
#include "ssstddef.h"

size_t SsLongToWcs(
        long l,
        ss_char2_t* buf,
        uint radix,
        size_t width,
        ss_char2_t leftfillch,
        bool is_signed);

ss_char2_t* SsLtow(
        long l,
        ss_char2_t* buf,
        int radix);

ss_char2_t* SsUltow(
        ulong l,
        ss_char2_t* buf,
        int radix);

#endif /* SSLTOW_H */
