/*************************************************************************\
**  source       * su0bmap.h
**  directory    * su
**  description  * Bit map routines
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


#ifndef SU0BMAP_H
#define SU0BMAP_H

#include <ssc.h>
#include <sslimits.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssstring.h>

#define su_bmap_t   uchar


/* type used as an array element:
** unsigned int would be fastest. but because machine independent
** representation is required it is uchar. Thus the bitmaps can
** safely be transmitted over network or stored to disk.
*/
typedef uchar su_bmelem_t;   

/*##**********************************************************************\
 * 
 *		SU_BMAP_SET
 * 
 * Sets a a numbered bit from bitmap to specified value
 * 
 * Input params : 
 * 
 *	buf	- pointer to bitmap
 *	bit	- bit number
 *	val	- (bool) TRUE or FALSE
 * 
 * Output params: none
 * 
 * Return value : none
 * 
 * Limitations  : bit number must be in the 'unsigned int' range
 * 
 * Globals used : none
 */
#define SU_BMAP_SET(buf,bit,val)\
do {\
uint b;\
su_bmelem_t *p;\
ss_dassert(((val)&~1)==0);\
b = (uint)(bit) % (sizeof(su_bmelem_t)*CHAR_BIT);\
p = (su_bmelem_t*)(buf) +\
(uint)(bit) / (sizeof(su_bmelem_t)*CHAR_BIT);\
*p = (su_bmelem_t)((*p & (~(1 << b))) | ((val) << b));\
} while (0)

/*##**********************************************************************\
 * 
 *		SU_BMAP_GET
 * 
 * Gets a bool value from bitmap
 * 
 * Input params : 
 * 
 *	buf	- pointer to bitmap
 *	bit	- bit number
 * 
 * Output params: none
 * 
 * Return value : TRUE or FALSE
 * 
 * Limitations  : bit number must be in the 'unsigned int' range
 * 
 * Globals used : none
 */
#define SU_BMAP_GET(buf,bit) \
((((su_bmelem_t*)(buf))[(uint)(bit) / (sizeof(su_bmelem_t)*CHAR_BIT)] >>\
((uint)(bit) % (sizeof(su_bmelem_t)*CHAR_BIT))) & 1)

/*##**********************************************************************\
 * 
 *		SU_BMAP_INIT
 * 
 * Creates a new bitmap and initializes it to TRUE or FALSE
 * 
 * Input params : 
 * 
 *	size	- number of bits
 *	val	    - initial value TRUE/FALSE
 * 
 * Output params: none
 * 
 * Return value : pointer to new bitmap array
 * 
 * Limitations  : size must be in the 'unsigned int' range
 * 
 * Globals used : none
 */
#define SU_BMAP_INIT(size, val) \
(su_bmap_t *)memset(SsMemAlloc((((size) + sizeof(su_bmelem_t)*CHAR_BIT - 1)/(sizeof(su_bmelem_t)*CHAR_BIT))*sizeof(su_bmelem_t)),\
       (val)? ~0 : 0, (((size) + sizeof(su_bmelem_t)*CHAR_BIT - 1)/(sizeof(su_bmelem_t)*CHAR_BIT))*sizeof(su_bmelem_t))

/*##**********************************************************************\
 * 
 *		SU_BMAP_DONE
 * 
 * Deletes a bitmap allocated with SU_BMAP_INIT
 * 
 * Input params : 
 * 
 *	p	- pointer to bitmap
 * 
 * Output params: none
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define SU_BMAP_DONE(p) SsMemFree(p)

/*##**********************************************************************\
 * 
 *		SU_BMAP_SETALL
 * 
 * Set all members of bitmap to TRUE or FALSE
 * 
 * Input params : 
 * 
 *	bmap	- pointer to bitmap
 *	size	- number of bits
 *	val	    - TRUE or FALSE
 * 
 * Output params: none
 * 
 * Return value : none
 * 
 * Limitations  : size must be in the 'unsigned int' range
 * 
 * Globals used : none
 */
#define SU_BMAP_SETALL(bmap,size, val) \
memset(bmap, (val)? ~0 : 0,\
(((size) + sizeof(su_bmelem_t)*CHAR_BIT - 1)\
/(sizeof(su_bmelem_t)*CHAR_BIT))*sizeof(su_bmelem_t))

#define SU_BMAP_BYTESIZE(size) \
        (((size) + ((sizeof(su_bmelem_t)*CHAR_BIT) - 1)) / (sizeof(su_bmelem_t)*CHAR_BIT))

long su_bmap_find1st(su_bmap_t* bmap, ulong bmsize, bool val);
long su_bmap_findnext(su_bmap_t* bmap, ulong bmsize, bool val, ulong startpos);
long su_bmap_findprev(su_bmap_t* bmap, ulong bmsize, bool val, ulong startpos);
long su_bmap_findlast(su_bmap_t* bmap, ulong bmsize, bool val);

#define SU_BMAPRC_NOTFOUND (-1L)

#endif /* SU0BMAP_H */



