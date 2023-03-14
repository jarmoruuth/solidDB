/*************************************************************************\
**  source       * tab0publ.h
**  directory    * tab
**  description  * CREATE and DROP PUBLICATION through SQL.
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


#ifndef TAB0PUBL_H
#define TAB0PUBL_H

void tb_publ_set_drop_callback(
        bool (*drop_fp)(void* cd,
                        tb_trans_t* trans,
                        char* name,
                        char* schema,
                        char* catalog,
                        rs_err_t** p_errh));

bool tb_publ_create(
        void* cd,
        tb_trans_t* trans,
        char* pubname,
        char* schema,
        char* catalog,
        char* extrainfo,
        rs_ttype_t* pardefs,
        sqllist_t* ressetqueries,
        sqllist_t* ressettables,
        sqllist_t* ressetschemas,
        sqllist_t* ressetcatalogs,
        sqllist_t* ressetextrainfos,
        void** cont,
        su_err_t** err);

bool tb_publ_drop(
        void* cd,
        tb_trans_t* trans,
        char* pubname,
        char* schema,
        char* catalog,
        char* extrainfo,
        void** cont,
        su_err_t** err);

#endif /* TAB0PUBL_H */


