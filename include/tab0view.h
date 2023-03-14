/*************************************************************************\
**  source       * tab0view.h
**  directory    * tab
**  description  * Relation level interface to view handles
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


#ifndef TAB0VIEW_H
#define TAB0VIEW_H

#include <rs0types.h>
#include <rs0viewh.h>
#include <rs0error.h>

tb_viewh_t* tb_viewh_create(
	void*       cd,
        tb_trans_t* trans,
	char*       name,
	char*       authid,
	char*       catalog,
        uint        throughview,
	char*       viewname,
	char*       viewauthid,
	char*       viewcatalog,
        tb_viewh_t* parentviewh,
	rs_err_t**  p_errh);

tb_viewh_t* tb_viewh_sql_create(
	void*       cd,
        tb_trans_t* trans,
	char*       name,
	char*       authid,
	char*       catalog,
        uint        throughview,
	char*       viewname,
	char*       viewauthid,
	char*       viewcatalog,
        tb_viewh_t* parentviewh,
	rs_err_t**  p_errh);

void tb_viewh_free(
	void*       cd,
	tb_viewh_t* tbviewh);

uint tb_viewh_issame(
	void*       cd,
        tb_trans_t* trans,
	tb_viewh_t* tbviewh,
        char*       name,
        char*       schema,
        char*       catalog,
        char*       extrainfo,
        uint        throughview,
        char*       viewname,
        char*       viewschema,
        char*       viewcatalog);

rs_viewh_t* tb_viewh_rsviewh(
	void*       cd,
	tb_viewh_t* tbviewh);

char* tb_viewh_def(
	void*       cd,
	tb_viewh_t* tbviewh);

#endif /* TAB0VIEW_H */
