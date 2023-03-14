/*************************************************************************\
**  source       * xs1sort.h
**  directory    * xs
**  description  * Sorter object for solid sort utility
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


#ifndef XS1SORT_H
#define XS1SORT_H

#include <uti0vtpl.h>

#include <su0list.h>

#include <rs0sysi.h>
#include <rs0ttype.h>

#include "xs2mem.h"
#include "xs2tfmgr.h"
#include "xs0type.h"

xs_sorter_t* xs_sorter_init(
        rs_sysinfo_t* cd,
        xs_mem_t* memmgr,
        xs_tfmgr_t* tfmgr,
        rs_ttype_t* ttype,
        su_list_t* orderby_list,
        uint maxnbuf,
        uint maxfiles,
        ulong stepsizebytes,
        uint stepsizerows,
        xs_qcomparefp_t comp_fp);

void xs_sorter_done(
        xs_sorter_t* sorter);

bool xs_sorter_addtuple(
        xs_sorter_t* sorter,
        rs_tval_t* tval,
        rs_err_t** p_errh);

xs_ret_t xs_sorter_merge(
        xs_sorter_t* sorter,
        bool* p_emptyset,
        rs_err_t** p_errh);

bool xs_sorter_fetchnext(
        xs_sorter_t* sorter,
        rs_tval_t** p_tval);

bool xs_sorter_fetchprev(
        xs_sorter_t* sorter,
        rs_tval_t** p_tval);

bool xs_sorter_cursortobegin(
        xs_sorter_t* sorter);

bool xs_sorter_cursortoend(
        xs_sorter_t* sorter);

#endif /* XS1SORT_H */
