/*************************************************************************\
**  source       * tab0srv.h
**  directory    * tab
**  description  * Server routines to table level is normal server code
**               * is not used.
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


#ifndef TAB0SRV_H
#define TAB0SRV_H

#include <ssc.h>
#include "tab0conn.h"

/* MySQL interface for solidDB */
typedef struct {

    int (*frm_from_disk_to_db)(tb_connect_t* tbcon, rs_sysi_t* cd, tb_trans_t* trans, rs_entname_t *en, const char *name, int deletep);
    int (*frm_from_db_to_disk)(tb_connect_t* tbcon, rs_sysi_t* cd, tb_trans_t* trans, rs_entname_t *en, int deletep);
    void (*con_throwout)(void* con);

} mysql_funblock_t;

void tb_srv_set_mysql_funblock(mysql_funblock_t* fb);

bool tb_srv_init(
        su_inifile_t* inifile,
        dbe_cryptoparams_t* cp,
        mysql_funblock_t* mysql_funblock);

void tb_srv_done(
        void);

bool tb_srv_start(
        tb_database_t* tdb);

void tb_srv_stop(
        void);

void tb_srv_shutdown(
        void);

tb_connect_t* tb_srv_connect_local(
        void* ctx,
        int loginid,
        char* username,
        char* password,
        int* p_id);

void tb_srv_disconnect(
        tb_connect_t* tbcon,
        void* ctx,
        int id);

void tb_srv_initcd(
        rs_sysi_t* cd);

void tb_srv_donecd(
        rs_sysi_t* cd);

bool tb_srv_frm_to_disk(
        tb_connect_t* tcon, 
        rs_sysi_t* cd,
        vtpl_t* vtpl,
        bool deletep);

bool tb_srv_pmondiff_start(
        rs_sysi_t* cd,
        uint interval,
        char* filename,
        bool append,
        bool raw,
        char* comment,
        su_err_t** p_errh);

bool tb_srv_pmondiff_stop(
        rs_sysi_t* cd,
        su_err_t** p_errh);

void tb_srv_pmondiff_clear(
        rs_sysi_t* cd);

#endif /* TAB0SRV_H */

