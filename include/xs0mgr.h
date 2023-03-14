/*************************************************************************\
**  source       * xs0mgr.h
**  directory    * xs
**  description  * eXternal Sort MaNaGer
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


#ifndef XS0MGR_H
#define XS0MGR_H

#include <ssc.h>
#include <su0inifi.h>
#include <rs0ttype.h>
#include <dbe0db.h>

#include "xs0type.h"

typedef struct xs_mgr_st xs_mgr_t;

xs_mgr_t* xs_mgr_init(
        dbe_db_t* db,
        su_inifile_t* inifile,
        long sortarraysize);

void xs_mgr_done(
        xs_mgr_t* xsmgr);

xs_sorter_t* xs_mgr_sortinit(
        xs_mgr_t* xsmgr,
        rs_ttype_t* ttype,
        ulong lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        void* cd,
        xs_qcomparefp_t comp_fp,
        bool sql,
        bool testonly);


void xs_mgr_addcfgtocfgl(
        xs_mgr_t* xsmgr,
        su_cfgl_t* cfgl);

#endif /* XS0MGR_H */
