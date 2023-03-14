/*************************************************************************\
**  source       * ss1utf.c
**  directory    * ss
**  description  * UTF functions.
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


#include "ss1utf.h"
#include "ssstddef.h"

/* for SS_UTF8_BYTES() */
const ss_byte_t ss_UTF8_map_bits_11_15[32] = {
        0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
};
/* for SS_UTF8_BYTES() */
const ss_byte_t ss_UTF8_map_bits_7_10[16] = {
        0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};
/* for SS_UTF8_BYTES() */
const ss_byte_t ss_UTF8_bytesneeded[4] = {
        1,2,3,3
};

const ss_byte_t ss_UTF8_extrabytes[0x100] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* < 11000000 => 0 */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 110xxxxx => 1*/
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* 1110xxxx => 2*/
    3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5  /* 11110xxx => 3, 111110xx => 4, 111111xx => 5*/
};
const ss_byte_t ss_UTF8_1stbytemark[6] = {
    0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC
};

const ss_uint4_t ss_UTF8_offsets[6] = {
    0x00000000UL, /* ss_UTF8_1stbytemark[0] */
    0x00003080UL, /* (ss_UTF8_1stbytemark[1] << 6)
                     + SS_UTF8_BYTEMARK */
    0x000E2080UL, /* (ss_UTF8_1stbytemark[2] << 12)
                     + (SS_UTF8_BYTEMARK << 6)
                     + SS_UTF8_BYTEMARK */
    0x03C82080UL, /* (ss_UTF8_1stbytemark[3] << 18)
                     + (SS_UTF8_BYTEMARK << 12)
                     + (SS_UTF8_BYTEMARK << 6)
                     + SS_UTF8_BYTEMARK */
    0xFA082080UL, /* (ss_UTF8_1stbytemark[4] << 24)
                     + (SS_UTF8_BYTEMARK << 18)
                     + (SS_UTF8_BYTEMARK << 12)
                     + (SS_UTF8_BYTEMARK << 6)
                     + SS_UTF8_BYTEMARK */
    0x82082080UL  /* (ss_UTF8_1stbytemark[5] << 32)
                     + (SS_UTF8_BYTEMARK << 24)
                     + (SS_UTF8_BYTEMARK << 18)
                     + (SS_UTF8_BYTEMARK << 12)
                     + (SS_UTF8_BYTEMARK << 6)
                     + SS_UTF8_BYTEMARK */
};


/*##**********************************************************************\
 * 
 *		SsUTF8CharLen
 * 
 * Measures how many characters an UTF-8 encoded buffer contains
 * 
 * Parameters : 
 * 
 *	src - in, use
 *		UTF-8 encoded buffer
 *		
 *	n - in, use
 *		number of bytes in buffer
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
size_t SsUTF8CharLen(ss_byte_t* src, size_t n)
{
        ss_byte_t* src_end;
        size_t clen;

        src_end = src + n;

        clen = 0;
        while (src < src_end) {
            size_t bytes;

            bytes = ss_UTF8_extrabytes[*src] + 1;
            clen++;
            src += bytes;
        }
        return (clen);
}

