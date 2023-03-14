/*************************************************************************\
**  source       * su0sdefs.h
**  directory    * su
**  description  * System definitions.
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


#ifndef SU0SDEFS_H
#define SU0SDEFS_H

#include <ssc.h>

#define SU_MINUSERNAMELEN   2
#define SU_MAXUSERNAMELEN   80
#define SU_MINPASSWORDLEN   3
#define SU_MAXPASSWORDLEN   SU_MAXUSERNAMELEN
#define SU_MINCATALOGLEN    1
#define SU_MAXCATALOGLEN    SU_MAXUSERNAMELEN

/* note this dictates the space allocated from db header
 * see dbe7hdr.c
 */
#define SU_MAXDEFCATALOGLEN 39

bool su_sdefs_isvalidusername(char* name);
bool su_sdefs_isvalidpassword(char* name);
bool su_sdefs_isvalidcatalog(char* name);

#endif /* SU0SDEFS_H */
