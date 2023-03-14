/*************************************************************************\
**  source       * tab0cata.h
**  directory    * tab
**  description  * Catalog support functions.
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


#ifndef TAB0CATA_H
#define TAB0CATA_H

#include <rs0error.h>

#include "tab0tran.h"
#include "tab0sche.h"

char* tb_catalog_resolve(
        rs_sysi_t* cd,
        char* catalog);

char* tb_catalog_resolve_withschema(
        rs_sysi_t* cd,
        char* catalog,
        char* schema);

bool tb_catalog_create(
	void*       cd,
	tb_trans_t* trans,
        char*       name,
        void**      cont,
	rs_err_t**  p_errh);

bool tb_catalog_drop(
	void*       cd,
	tb_trans_t* trans,
        char*       name,
        bool        cascade,
        void**      cont,
	rs_err_t**  p_errh);

bool tb_catalog_set(
	void*       cd,
	tb_trans_t* trans,
        char*       name,
        void**      cont,
	rs_err_t**  p_errh);

#define tb_catalog_find(cd, trans, name, p_id) tb_schema_find_catalog(cd, name, p_id)

#endif /* TAB0CATA_H */
