/*************************************************************************\
**  source       * ss1utf.h
**  directory    * ss
**  description  * UNICODE conversions internal support
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


#ifndef SS1UTF_H
#define SS1UTF_H

#include "ssenv.h"
#include "ssc.h"

#define SS_UTF8_BYTEMARK 0x80U
#define SS_UTF8_BYTEMASK 0xbfU

/* This macro calculates the number of bytes needed to
 * represent a 16-bit unsigned (UCS-2) character as UTF-8
 */
#define SS_UTF8_BYTES(ch) \
        (ss_UTF8_bytesneeded[ss_UTF8_map_bits_11_15[(ch) >> 11U] | ss_UTF8_map_bits_7_10[((ch) >> 7U) & 15U]])

extern const ss_byte_t ss_UTF8_map_bits_11_15[32];
extern const ss_byte_t ss_UTF8_map_bits_7_10[16];
extern const ss_byte_t ss_UTF8_bytesneeded[4];
extern const ss_byte_t ss_UTF8_extrabytes[0x100];
extern const ss_byte_t ss_UTF8_1stbytemark[6];
extern const ss_uint4_t ss_UTF8_offsets[6];

#endif /* SS1UTF_H */

