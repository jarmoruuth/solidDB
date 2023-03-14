/*************************************************************************\
**  source       * su0ctc.c
**  directory    * su
**  description  * CRC calculation routine for database creation
**               * timestamp
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


Limitations:
-----------


Error handling:
--------------


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

#include <ssc.h>
#include "su0crc32.h"
#include "su0icvt.h"
#ifdef SS_LICENSEINFO_V3
#include "su0li3.h"
#else /* SS_LICENSEINFO_V3 */
#include "su0li2.h"
#endif /* SS_LICENSEINFO_V3 */
/*##**********************************************************************\
 * 
 *		su_lxc_calcctc
 * 
 * Calculates a 32 bit CRC for database creation timestamp
 * 
 * Parameters : 
 * 
 *	creatime - in
 *		SsTimeT timestamp
 *		
 * Return value :
 *      CRC of the creatime
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_uint4_t su_lxc_calcctc(ss_uint4_t creatime)
{
        ss_byte_t creatimebuf[sizeof(creatime)];
        ss_uint4_t crc = 0L;

        SS_UINT4_STORETODISK(creatimebuf, creatime);
        su_crc32((char*)creatimebuf, sizeof(creatimebuf), &crc);
        return (crc);
}


