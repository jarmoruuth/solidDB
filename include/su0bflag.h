/*************************************************************************\
**  source       * su0bflag.h
**  directory    * su
**  description  * Bit flag test and set routines.
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


#ifndef SU0BFLAG_H
#define SU0BFLAG_H

#include <ssc.h>
typedef ss_uint4_t su_bflag_t;

#define SU_BFLAG_BIT(n)             ((su_bflag_t)1 << (n))


#define SU_BFLAG_SET(flg, val)      ((flg) |= (val))
#define SU_BFLAG_CLEAR(flg, val)    ((flg) &= ~(val))
#define SU_BFLAG_TEST(flg, val)     (((flg) & (val)) != 0)


/*##**********************************************************************\
 * 
 *		SU_BFLAG_COPYWITHMASK
 * 
 * Copies bits which are 1 in mask from srcflg to destflg.
 * other bits in destflg remain unchanged
 * 
 * Parameters : 
 * 
 *	destflg - in out
 *		destination flags
 *		
 *	srcflg - in
 *		source flags
 *		
 *	mask - in
 *		mask flags, bit value 1 enables copying of that bit
 *		
 * Return value :
 *      value of destflags
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SU_BFLAG_COPYWITHMASK(destflg, srcflg, mask) \
        ((destflg) = ((destflg) & ~(mask)) | ((srcflg) & (mask))

#endif /* SU0BFLAG_H */
