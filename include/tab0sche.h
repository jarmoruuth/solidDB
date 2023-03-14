/*************************************************************************\
**  source       * tab0sche.h
**  directory    * tab
**  description  * Schema support functions.
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


#ifndef TAB0SCHE_H
#define TAB0SCHE_H

#include <rs0error.h>

#include "tab0tran.h"

typedef struct tb_schema_st tb_schema_t;

typedef enum {
        TB_SCHEMA_DROP_USER,
        TB_SCHEMA_DROP_SCHEMA,
        TB_SCHEMA_DROP_CATALOG
} tb_schema_drop_t;

bool tb_schema_create_ex(
	void*       cd,
	tb_trans_t* trans,
	char*       schema,
        char*       catalog,
	char*       authid,
        bool        usercreate,
        long        uid,
	rs_err_t**  p_errh);

bool tb_schema_drop_ex(
	void*            cd,
	tb_trans_t*      trans,
	char*            schema,
        char*            catalog,
        bool             cascade,
        tb_schema_drop_t droptype,
	rs_err_t**       p_errh);

bool tb_schema_drop_int(
        void*            cd,
        tb_trans_t*      trans,
        char*            schema,
        char*            catalog,
        bool             cascade,
        tb_schema_drop_t droptype,
        rs_err_t**       p_errh);

void tb_schema_dropcatalog(
	void*       cd,
	tb_trans_t* trans,
        char*       catalog);
                           
bool tb_schema_create(
	void*       cd,
	tb_trans_t* trans,
	char*       schema,
        char*       catalog,
	char*       authid,
        void**      cont,
	rs_err_t**  p_errh);

bool tb_schema_drop(
	void*       cd,
	tb_trans_t* trans,
	char*       schema,
        char*       catalog,
        bool        cascade,
        void**      cont,
	rs_err_t**  p_errh);

bool tb_schema_find(
	void*       cd,
	tb_trans_t* trans,
	char*       schema,
        char*       catalog);

bool tb_schema_maptouser(
	void*       cd,
	tb_trans_t* trans,
	char*       schema,
        char*       catalog,
        long*       p_userid,
        char**      p_username);

bool tb_schema_allowuserdrop(
	void*       cd,
	tb_trans_t* trans,
	char*       username,
        rs_err_t**  p_errh);

bool tb_schema_allowcatalogdrop(
	void*       cd,
	tb_trans_t* trans,
        char*       catalog,
        rs_err_t**  p_errh);

bool tb_schema_isvalidsetschemaname(
        void* cd,
        tb_trans_t* trans,
        char* username);

bool tb_schema_insert_catalog(
        rs_sysi_t* cd,
        char* catalog,
        long id);

bool tb_schema_remove_catalog(
        rs_sysi_t* cd,
        char* catalog);

bool tb_schema_find_catalog(
        rs_sysi_t* cd,
        char* catalog,
        long* p_id);

bool tb_schema_find_catalog_mode(
        rs_sysi_t* cd,
        char* catalog,
        long* p_id,
        rs_sysi_t** p_cd,
        bool *p_ismaster,
        bool *p_isreplica);

bool tb_schema_catalog_setmaster(
        rs_sysi_t* cd,
        char* catalog,
        bool ismaster);

bool tb_schema_catalog_setreplica(
        rs_sysi_t* cd,
        char* catalog,
        bool isreplica);

bool tb_schema_catalog_setmode(
        rs_sysi_t* cd,
        char* catalog,
        bool mode);

void tb_schema_catalog_clearmode(
        rs_sysi_t* cd);

tb_schema_t* tb_schema_globalinit(
        rs_sysi_t* cd);

void tb_schema_globaldone(
        tb_schema_t* sc);

bool tb_schema_reload(
        rs_sysi_t* cd, 
        tb_schema_t* sc);

bool tb_schema_dropreferencekeys(
        void*            cd,
        tb_trans_t*      trans,
        char*            schema,
        char*            catalog,
        rs_err_t**       p_errh);

#endif /* TAB0SCHE_H */
