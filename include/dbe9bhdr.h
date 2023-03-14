/*************************************************************************\
**  source       * dbe9bhdr.h
**  directory    * dbe
**  description  * Database Block header declarations and some helpful
**               * macros
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


#ifndef DBE9BHDR_H
#define DBE9BHDR_H

#include <su0icvt.h>    /* integer conversions */
#include "dbe9type.h"   /* DBE internal type defs. */
#include "dbe0type.h"   /* DBE public type defs */

#define DBE_BLOCKTYPEOFFSET  0  /* block type field    - 1 byte  */
#define DBE_BLOCKCPNUMOFFSET 1  /* block checkpoint #  - 4 bytes */

/*##**********************************************************************\
 * 
 *		DBE_BLOCK_GETTYPE
 * 
 * Macro for getting database block (in memory) type field
 * 
 * Parameters : 
 * 
 *	p_block - in, use
 *		pointer to database block
 * 
 *	p_type - out, use
 *		pointer to type variable
 *
 * 
 * Return value : type field value
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */

#define DBE_BLOCK_GETTYPE(p_block, p_type) \
(*(p_type) = (*(dbe_blocktype_t*)((uchar*)(p_block)+DBE_BLOCKTYPEOFFSET)))


/*##**********************************************************************\
 * 
 *		DBE_BLOCK_GETCPNUM
 * 
 * Macro for getting database block (in memory) checkpoint number field
 * 
 * Parameters : 
 * 
 *	p_block - in, use
 *		pointer to database block
 * 
 *	p_cpnum - out, use
 *		pointer to checkpoint number variable
 *
 * 
 * Return value : checkpoint number value
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_BLOCK_GETCPNUM(p_block, p_cpnum) \
*p_cpnum = SS_UINT4_LOADFROMDISK(((uchar*)(p_block) + DBE_BLOCKCPNUMOFFSET))


/*##**********************************************************************\
 * 
 *		DBE_BLOCK_SETTYPE
 * 
 * Macro for setting database block (in memory) type field
 * 
 * Parameters : 
 * 
 *	p_block - out, use
 *		pointer to start of block
 *
 *	p_type - in, use
 *		pointer to variable containing the type value
 *
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_BLOCK_SETTYPE(p_block, p_type) \
(*(dbe_blocktype_t*)((uchar*)(p_block)+DBE_BLOCKTYPEOFFSET) = *(p_type))

/*##**********************************************************************\
 * 
 *		DBE_BLOCK_SETCPNUM
 * 
 * Macro for setting database block (in memory) checkpoint number field
 * 
 * Parameters : 
 * 
 *	p_block - out, use
 *		pointer to start of block
 *
 *	p_cpnum - in, use
 *		pointer to checkpoint number variable
 * 
 * Return value : none
 * 
 * Limitations  : none
 * 
 * Globals used : none
 */
#define DBE_BLOCK_SETCPNUM(p_block, p_cpnum) \
SS_UINT4_STORETODISK(((uchar*)(p_block)+DBE_BLOCKCPNUMOFFSET), *p_cpnum)

                      
#endif /* DBE9BHDR_H */
