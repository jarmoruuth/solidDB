/*************************************************************************\
**  source       * tab0info.h
**  directory    * tab
**  description  * Query processing information production
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


#ifndef TAB0INFO_H
#define TAB0INFO_H

#include <ssc.h>
#include <rs0types.h>

uint tb_info_level(
        rs_sysi_t*  cd,
        tb_trans_t* trans
);

char* tb_info_option(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        char*       option);

void tb_info_print(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        uint        level,
        char*       str
);

void tb_info_printwarning(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        uint        code,
        char*       str
);

sql_nullcoll_t tb_info_defaultnullcoll(
        rs_sysi_t*  cd,
        tb_trans_t* trans);

#endif /* TAB0INFO_H */
