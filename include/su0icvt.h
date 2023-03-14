/*************************************************************************\
**  source       * su0icvt.h
**  directory    * su
**  description  * Integer to disk image convert routines for SOLID
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


#ifndef SU0ICVT_H
#define SU0ICVT_H

#include <sslimits.h>
#include <ssc.h>

#define SU_4BYTE_COPY_DIRECT(destaddr, srcaddr) \
        (*(FOUR_BYTE_T*)(destaddr) = *(FOUR_BYTE_T*)(srcaddr))

#define SU_2BYTE_COPY_DIRECT(destaddr, srcaddr) \
        (*(TWO_BYTE_T*)(destaddr) = *(TWO_BYTE_T*)(srcaddr))

#  define SU_4BYTE_COPY_BYTEWISE(destaddr, srcaddr) \
        ((((char*)(destaddr))[0] = ((char*)(srcaddr))[0]),\
         (((char*)(destaddr))[1] = ((char*)(srcaddr))[1]),\
         (((char*)(destaddr))[2] = ((char*)(srcaddr))[2]),\
         (((char*)(destaddr))[3] = ((char*)(srcaddr))[3]))

#define SU_2BYTE_COPY_BYTEWISE(destaddr, srcaddr) \
        ((((char*)(destaddr))[0] = ((char*)(srcaddr))[0]),\
         (((char*)(destaddr))[1] = ((char*)(srcaddr))[1]))

#define SU_4BYTE_COPY_REVERSE(destaddr, srcaddr) \
        ((((char*)(destaddr))[3] = ((char*)(srcaddr))[0]),\
         (((char*)(destaddr))[2] = ((char*)(srcaddr))[1]),\
         (((char*)(destaddr))[1] = ((char*)(srcaddr))[2]),\
         (((char*)(destaddr))[0] = ((char*)(srcaddr))[3]))

#define SU_2BYTE_COPY_REVERSE(destaddr, srcaddr) \
        ((((char*)(destaddr))[1] = ((char*)(srcaddr))[0]),\
         (((char*)(destaddr))[0] = ((char*)(srcaddr))[1]))


/*##**********************************************************************\
 * 
 *		SU_COPY_UINT4TODISK
 *
 * DEPRECATED !!!!
 * Copy four byte unsigned integer from memory format to disk format.
 * 
 * Parameters : 
 * 
 *	dest_daddr - out
 *		Pointer to destination.
 *		
 *	src_uladdr - in
 *		Pointer to source.
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SU_COPY_UINT4TODISK(dest_daddr, src_uladdr) \
        SU_COPY_UINT4TODISK is deprecated, use macro SS_UINT4_STORETODISK !

/*##**********************************************************************\
 * 
 *		SU_COPY_DISKTOUINT4
 * 
 * DEPRECATED !!!!
 * Copy four byte unsigned integer from disk format to memory format.
 * 
 * Parameters : 
 * 
 *	dest_uladdr - out
 *		Pointer to destination.
 *		
 *	src_daddr - in
 *		Pointer to source.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SU_COPY_DISKTOUINT4(dest_uladdr, src_daddr) \
        SU_COPY_DISKTOUINT4 is deprecated, use macro SS_UINT4_LOADFROMDISK !

/*##**********************************************************************\
 * 
 *		SU_COPY_UINT2TODISK
 * 
 * DEPRECATED !!!!
 * Copy two byte unsigned integer from memory format to disk format.
 * 
 * Parameters : 
 * 
 *	dest_daddr - out
 *		Pointer to destination.
 *		
 *	src_usaddr - in
 *		Pointer to source.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SU_COPY_UINT2TODISK(dest_daddr, src_usaddr) \
        SU_COPY_UINT2TODISK is deprecated, use macro SS_UINT2_STORETODISK !

/*##**********************************************************************\
 * 
 *		SU_COPY_DISKTOUINT2
 * 
 * DEPRECATED !!!!
 * Copy two byte unsigned integer from disk format to memory format.
 * 
 * Parameters : 
 * 
 *	dest_usaddr - out
 *		Pointer to destination.
 *		
 *	src_daddr - in
 *		Pointer to source.
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#define SU_COPY_DISKTOUINT2(dest_usaddr, src_daddr) \
        SU_COPY_DISKTOUINT2 is deprecated, use macro SS_UINT2_LOADFROMDISK !

#endif /* SU0ICVT_H */
