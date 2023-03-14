/*************************************************************************\
**  source       * xs2tupl.h
**  directory    * xs
**  description  * Tuple building routines for sorting
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


#ifndef XS2TUPL_H
#define XS2TUPL_H

#include <uti0vtpl.h>
#include <su0list.h>
#include <rs0sysi.h>
#include <rs0types.h>
#include <rs0tval.h>
#include "xs2stre.h"

rs_ano_t* xs_tuple_anomap_init(
        rs_sysinfo_t* cd,
        uint nattrs,
        rs_ano_t sellist[/*nattrs*/],
        su_list_t* orderby_list);

void xs_tuple_anomap_done(
        rs_ano_t* anomap);

uint* xs_tuple_cmpcondarr_init(
        su_list_t* orderby_list);

void xs_tuple_cmpcondarr_done(
        uint* cmpcondarr);

bool xs_tuple_makevtpl(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        uint nattrs,
        rs_ano_t* anomap,
        char* buf,
        size_t bufsize,
        char** p_nextpos);

bool xs_tuple_makevtpl2stream(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        uint nattrs,
        xs_stream_t* stream,
        rs_err_t** p_errh);

bool xs_tuple_filltval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_ano_t* anomap,
        vtpl_t* vtpl,
        rs_tval_t* tval);

#endif /* XS2TUPL_H */
