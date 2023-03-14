/*************************************************************************\
**  source       * dbe9blst.c
**  directory    * dbe
**  description  * Block list header routines (for block lists)
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

This is the common part of block lists used in several places
inside the database engine. A block list is a disk-based linked list
with several data items in each node.

Limitations:
-----------

none

Error handling:
--------------

none

Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssdebug.h>
#include "dbe9blst.h"

/*##**********************************************************************\
 * 
 *		dbe_blh_init
 * 
 * Initializes a block list header to
 * contain no data and no successors
 * 
 * Parameters : 
 * 
 *	p_hdr - in out, use
 *		pointer to block list header object
 *
 *	type - in
 *		type of list block
 *
 *	cpnum - in
 *		checkpoint number to put into block
 *
 * Return value : pointer to the header = p_hdr
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_blheader_t *dbe_blh_init(
	dbe_blheader_t *p_hdr,
	dbe_blocktype_t type,
	dbe_cpnum_t cpnum)
{
        ss_dassert(p_hdr != NULL);
        p_hdr->bl_type = type;
        p_hdr->bl_cpnum = cpnum;
        p_hdr->bl_nblocks = 0;
        p_hdr->bl_next = SU_DADDR_NULL;
        return (p_hdr);
}

/*##**********************************************************************\
 * 
 *		dbe_blh_get
 * 
 * Gets header information from node disk image.
 * 
 * Parameters : 
 * 
 *	p_hdr - in out, use
 *		pointer to header object
 *
 *	diskbuf - in, use
 *		disk image buffer
 *
 * 
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void  dbe_blh_get(p_hdr, diskbuf)
	dbe_blheader_t *p_hdr;
	void *diskbuf;
{
        ss_dassert(p_hdr != NULL);
        ss_dassert(diskbuf != NULL);

        DBE_BLOCK_GETTYPE(diskbuf, &p_hdr->bl_type);
        DBE_BLOCK_GETCPNUM(diskbuf, &p_hdr->bl_cpnum);
        p_hdr->bl_nblocks = SS_UINT2_LOADFROMDISK(
                            (char*)diskbuf + DBE_BLIST_NBLOCKSOFFSET);
        p_hdr->bl_next = SS_UINT4_LOADFROMDISK(
                            (char*)diskbuf + DBE_BLIST_NEXTOFFSET);

}

/*##**********************************************************************\
 * 
 *		dbe_blh_put
 * 
 * Puts header information to disk image
 * 
 * Parameters : 
 * 
 *	p_hdr - in, use
 *		pointer to header object
 *
 *	diskbuf - out, use
 *		disk image buffer
 *
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_blh_put(p_hdr, diskbuf)
	dbe_blheader_t *p_hdr;
	void *diskbuf;
{
        ss_dassert(p_hdr != NULL);
        ss_dassert(diskbuf != NULL);

        DBE_BLOCK_SETTYPE(diskbuf, &p_hdr->bl_type);
        DBE_BLOCK_SETCPNUM(diskbuf, &p_hdr->bl_cpnum);
        SS_UINT2_STORETODISK((char*)diskbuf + DBE_BLIST_NBLOCKSOFFSET,
                            p_hdr->bl_nblocks);
        SS_UINT4_STORETODISK((char*)diskbuf + DBE_BLIST_NEXTOFFSET,
                            p_hdr->bl_next);
}
