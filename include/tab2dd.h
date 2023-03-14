/*************************************************************************\
**  source       * tab2dd.h
**  directory    * tab
**  description  * Data dictionary maintenance routines.
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


#ifndef TAB2DD_H
#define TAB2DD_H

#include <tab1dd.h>

extern char create_view_sync_failed_master_messages[];
extern char create_view_sync_failed_messages[];
extern char create_view_sync_active_master_messages[];
extern char create_view_sync_active_messages[];

dd_updatestmt_t* tb_dd_syncproceduredefs_init();
void tb_dd_syncproceduredefs_done( dd_updatestmt_t* defs );

dd_updatestmt_t* tb_dd_updatestmts_be();

dd_updatestmt_t* tb_dd_syncsequencedefs();

long tb_dd_getlatestsyncprocedureversion(void);
bool tb_dd_getsyncproceduredropstmts(
        rs_sysinfo_t* cd,
        rs_rbuf_t* rbuf,
        char** dropstmts);

bool tb_dd_convert_sync_trxid_int2bin(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        sqlsystem_t* sys);


#endif /* TAB2DD_H */

