/*************************************************************************\
**  source       * xs0sqli.h
**  directory    * xs
**  description  * eXternal Sorter SQL Interface
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


#ifndef XS0SQLI_H
#define XS0SQLI_H

#include "xs0type.h"

xs_sorter_t* xs_sorter_cmpinit(
        void* cd,
        rs_ttype_t* ttype,
        ulong lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        xs_qcomparefp_t comp_fp);

xs_sorter_t* xs_sorter_sqlinit(
        void* cd,
        rs_ttype_t* ttype,
        rs_estcost_t lines,
        bool exact,
        uint order_c,
        uint order_cols[/*order_c*/],
        bool descarr[/*order_c*/],
        bool testonly);

bool xs_sorter_sqladd(
        void* cd,
        xs_sorter_t* sorter,
        rs_tval_t* tval,
        rs_err_t** p_errh);

xs_ret_t xs_sorter_sqlrunstep(
        void* cd,
        xs_sorter_t* sorter,
        rs_err_t** p_errh);

rs_tval_t* xs_sorter_sqlfetchnext(
        void* cd,
        xs_sorter_t* sorter,
        bool* p_finished);

rs_tval_t* xs_sorter_sqlfetchprev(
        void* cd,
        xs_sorter_t* sorter,
        bool* p_finished);

bool xs_sorter_sqlcursortobegin(
        void* cd,
        xs_sorter_t* sorter);

bool xs_sorter_sqlcursortoend(
        void* cd,
        xs_sorter_t* sorter);

void xs_sorter_sqldone(
        void* cd,
        xs_sorter_t* sorter);

#endif /* XS0SQLI_H */
