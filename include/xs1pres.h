/*************************************************************************\
**  source       * xs1pres.h
**  directory    * xs
**  description  * Presorter for Solid sort utility
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


#ifndef XS1PRES_H
#define XS1PRES_H

#include <uti0vtpl.h>
#include <rs0sysi.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <su0list.h>

#include "xs2stre.h"

typedef struct xs_presorter_st xs_presorter_t;

xs_presorter_t* xs_presorter_init(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        xs_streamarr_t* streamarr,
        rs_ano_t* anomap,
        xs_qcomparefp_t comp_fp,
        uint* cmpcondarr,
        uint nbuf,
        xs_mem_t* memmgr);

void xs_presorter_done(
        xs_presorter_t* presorter);

bool xs_presorter_flush(
        xs_presorter_t* presorter,
        rs_err_t** p_errh);

bool xs_presorter_addtuple(
        xs_presorter_t* presorter,
        rs_tval_t* tval,
        rs_err_t** p_errh);

#endif /* XS1PRES_H */
