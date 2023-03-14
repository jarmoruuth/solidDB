/*************************************************************************\
**  source       * ssint4.h
**  directory    * ss
**  description  * utilities to load/store ss_int4_t values
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


#ifndef SSINT4_H
#define SSINT4_H

#include "ssenv.h"
#include "ssstddef.h"
#include "ssc.h"
#include "sslimits.h"

/* internally used definitions, do not use outside ssint4.c !! */
#define I4_MINBYTES(i4) \
(((i4) >= (ss_int4_t)0xFFFFFF80L) ?\
 (((i4) < (ss_int4_t)0x00000080L) ?\
  1 :\
  (((i4) < (ss_int4_t)0x00008000L) ?\
   2 :\
   (((i4) < (ss_int4_t)0x00800000L) ?\
    3 : 4))) :\
 (((i4) >= (ss_int4_t)0xFFFF8000L) ?\
  2 :\
  (((i4) >= (ss_int4_t)0xFF800000L) ?\
   3 : 4)))


/* external definitions/declarations */
SS_INLINE size_t SsInt4MinBytes(ss_int4_t i4);
SS_INLINE void SsInt4StorePackedMSB1st(
        ss_int4_t i4, ss_byte_t* buf, size_t size);
SS_INLINE void SsInt4LoadPackedMSB1st(ss_int4_t* p_result, ss_byte_t* buf, size_t size);

#if defined(SS_USE_INLINE) || defined(SSINT4_C)

SS_INLINE size_t SsInt4MinBytes(ss_int4_t i4)
{
        size_t minbytes = I4_MINBYTES(i4);
        return (minbytes);
}

SS_INLINE void SsInt4StorePackedMSB1st(
        ss_int4_t i4, ss_byte_t* buf, size_t size)
{
        /* removed (assertion is valid, but include hierarchy problematic):
           ss_dassert(1 <= size && size <= 4);
        */
        switch (size) {
            case 4:
                *buf++ = (ss_byte_t)((ss_uint4_t)i4 >> (3 * SS_CHAR_BIT));
                /* FALLTHROUGH */
            case 3:
                *buf++ = (ss_byte_t)((ss_uint4_t)i4 >> (2 * SS_CHAR_BIT));
                /* FALLTHROUGH */
            case 2:
                *buf++ = (ss_byte_t)((ss_uint4_t)i4 >> (1 * SS_CHAR_BIT));
                /* FALLTHROUGH */
            default: /* 1 */
                *buf = (ss_byte_t)i4;
                break;
        }
}

SS_INLINE void SsInt4LoadPackedMSB1st(
        ss_int4_t* p_result, ss_byte_t* buf, size_t size)
{
        ss_uint4_t u4;

        /* note: sign extension forced with cast to ss_int1_t ! */
        u4 = (ss_uint4_t)(ss_int4_t)(ss_int1_t)*buf++;
        
        switch (size) {
            case 4:
                u4 = (u4 << SS_CHAR_BIT) | *buf++;
                /* FALLTHROUGH */
            case 3:
                u4 = (u4 << SS_CHAR_BIT) | *buf++;
                /* FALLTHROUGH */
            case 2:
                u4 = (u4 << SS_CHAR_BIT) | *buf;
                /* FALLTHROUGH */
            default: /* 1 */
                break;
        }
        *p_result = (ss_int4_t)u4;
}

#endif /* SS_USE_INLINE || SSINT4_C */

#endif /* SSINT4_H */
