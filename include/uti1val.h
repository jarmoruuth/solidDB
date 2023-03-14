/*************************************************************************\
**  source       * uti1val.h
**  directory    * uti
**  description  * v-attribute and v-tuple length macros
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

#include <sslimits.h>
#include <uti0va.h>

#ifndef UTI1VAL_H
#define UTI1VAL_H

/* constants **************************************************/


/* the bias of the v-attribute integer representation */
#define VA_INT_BIAS (0x80)
/* the maximum size of integer to be stored in a v-attribute */
#define VA_INT_MAX_BYTES 8
/* the biases for multi-byte integer representations */
#define VA_POS_BIAS (VA_INT_BIAS + VA_INT_MAX_BYTES - 0x100)
#define VA_NEG_BIAS (VA_INT_BIAS - VA_INT_MAX_BYTES)


/* macros **************************************************/


/*#**********************************************************************\
 *
 *		FOUR_BYTE_LEN_LONG
 *
 * Returns the length of the data area of a large v-attribute.
 *
 * Parameters :
 *
 *	va - in, use
 *		pointer to the v-attribute
 *
 * Return value :
 *
 *      the length of the data area
 *
 * Limitations  :
 *
 *      The length of the v-attribute must be in the four-byte
 *      format.
 *
 * Globals used :
 */
#if defined(UNALIGNED_LOAD) && defined(SS_LSB1ST)
#  define FOUR_BYTE_LEN_LONG(va) (*((FOUR_BYTE_T*)&(va)->c[1]))
#else
#  define FOUR_BYTE_LEN_LONG(va) \
        (((((((FOUR_BYTE_T)(va)->c[4] << 8) | (va)->c[3]) << 8) | \
            (va)->c[2]) << 8) | (va)->c[1])
#endif





#define VA_GETDATA_SHORT(p_data, p_va, len) \
        (len) = (p_va)->c[0]; \
        (p_data) = (void*)&(p_va)->c[1];

#define VA_SET_LEN_SHORT(p_data, p_va, len) \
        (p_va)->c[0] = (ss_byte_t)(len); \
        (p_data) = (ss_byte_t*)&(p_va)->c[1];

#define VTPL_SET_LEN_SHORT(p_data, p_vtpl, len) \
        VA_SET_LEN_SHORT(p_data, p_vtpl, len)

#endif /* UTI1VAL_H */
