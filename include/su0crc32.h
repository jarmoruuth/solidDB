/*************************************************************************\
**  source       * su0crc32.h
**  directory    * su
**  description  * 32-bit CRC calculation
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


#ifndef SU0CRC32_H
#define SU0CRC32_H

#include <ssc.h>
#include <ssstddef.h>
#include <sslimits.h>

SS_INLINE void su_crc32(
        char* s,
        size_t len,
        FOUR_BYTE_T* p_crc32);


/*##**********************************************************************\
 * 
 *		SU_CRC32
 * 
 * A macro version of su_crc32()
 * 
 * Parameters : 
 * 
 *	s -   see su_crc32()
 *	len -    - '' -
 *		
 *	crc32val - same as *p_crc32 in su_crc32(). Must be a valid lvalue.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */

#define SU_CRC32(s, len, crc32val) \
{ \
        size_t i; \
        /* update running CRC calculation with contents of a buffer */ \
        for (i = 0;  i < (len);  i++) { \
            int index; \
            index = (int) ((crc32val) ^ (s)[i]) & 0x000000FFL; \
            (crc32val) = crc_32_tab[index] ^ (((crc32val) >> 8) \
                & 0x00FFFFFFL); \
        } \
}

#if defined(SU0CRC32_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 * 
 *		su_crc32
 * 
 * 32 bit CRC calculation
 * 
 * Parameters : 
 * 
 *	s - in, use
 *		pointer to input data
 *		
 *	len - in
 *		input data length
 *		
 *	p_crc32 - in out, use
 *		pointer to 32-bit integer where the CRC is stored. It
 *          should be initialized to 0 before the first call. Explicit
 *          initialization is required because this function can be used
 *          for calculating CRC over several distinct memory buffers.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
SS_INLINE void su_crc32(
        char* s,
        size_t len,
        FOUR_BYTE_T* p_crc32)
{
        FOUR_BYTE_T crc32val;
        size_t i;
        extern const long crc_32_tab[];

        crc32val = *p_crc32;
        /* update running CRC calculation with contents of a buffer */
        for (i = 0;  i < len;  i++) {
            int index;
            index = (int) (crc32val ^ s[i]) & 0x000000FFL;
            crc32val = crc_32_tab[index] ^ ((crc32val >> 8) & 0x00FFFFFFL);
        }
        *p_crc32 = crc32val;
}

#endif /* defined(SU0CRC32_C) || defined(SS_USE_INLINE) */

#endif /* SU0CRC32_H */
