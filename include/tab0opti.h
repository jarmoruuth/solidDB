/*************************************************************************\
**  source       * tab0opti.h
**  directory    * tab
**  description  * Utilities for SQL optimizing
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


#ifndef TAB0OPTI_H
#define TAB0OPTI_H


#include <ssc.h>
#include <rs0types.h>

uint tb_opti_sortarraysize(
        void*       cd,
        tb_trans_t* trans,
        rs_ttype_t* ttype
);

uint tb_opti_unionsfromors(
        void*       cd,
        tb_trans_t* trans
);

rs_estcost_t tb_opti_sortestimate(
        void*        cd,
        rs_ttype_t*  ttype,
        rs_estcost_t lines,
        bool         external);

#endif /* TAB0OPTI_H */
