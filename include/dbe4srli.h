/*************************************************************************\
**  source       * dbe4srli.h
**  directory    * dbe
**  description  * Declarations needed for creating system relations.
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


#ifndef DBE4SRLI_H
#define DBE4SRLI_H

#include <rs0rbuf.h>
#include <rs0sdefs.h>

/* Start value for user's relation id:s. Smaller
** values are reserved for system use !
** The same value should be used also for attribute
** and key id's.
*/
#define DBE_USER_ID_START       RS_USER_ID_START

/* First id for system tables and views generated using SQL.
 */
#define DBE_SYS_SQL_ID_START    RS_SYS_SQL_ID_START

/* 
 * First id for system tables and views (in migration, we want to fill
 * holes in identifiers
 *
 */
#define DBE_MIGRATION_ID_START    RS_MIGRATION_ID_START

void dbe_srli_init(rs_sysi_t* cd, rs_rbuf_t *rbuf, bool unicode_enabled);

#ifdef SS_DEBUG
void dbe_srli_testinit(void);

#endif

#endif /* DBE4SRLI_H */
