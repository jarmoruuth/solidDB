/*************************************************************************\
**  source       * tab0minisql.h
**  directory    * tab
**  description  * Table level MINI SQL functions.
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


#ifndef TAB0MINISQL_H
#define TAB0MINISQL_H

#include <rs0sysi.h>

#include <su0err.h>

#include <rs0types.h>
#include <rs0sysi.h>

#include "tab0type.h"
#include "tab0tran.h"

typedef void (*case_convertion_func_t)(char *);

bool tb_minisql_execdirect(
        void* cd,
        sqltrans_t* sqltrans,
        rs_relh_t* relh,
        char* sqlstr,
        rs_err_t** p_errh);

#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)

bool tb_minisql_prepare_drop_forkeys(
        rs_sysi_t* cd,
        char* sqlstr,
        su_list_t** fkey_list,
        rs_err_t** p_errh);

bool tb_minisql_prepare_mysql_forkeys(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        char* sqlstr,
        case_convertion_func_t case_convertion_func,
        case_convertion_func_t field_name_convertion_func,
        su_list_t** fkey_list,
        rs_err_t** p_errh);

bool tb_minisql_is_drop_index(
        rs_sysi_t* cd,
        const char* sqlstr);

bool tb_minisql_get_table_name(
        rs_sysi_t* cd,
        char* sqlstr,
        case_convertion_func_t case_convertion_func,
        rs_entname_t* alter_table_name);

bool tb_minisql_is_drop_table_cascade(
        rs_sysi_t* cd,
        const char* sqlstr);

#endif /* SS_MYSQL || SS_MYSQL_AC */

#endif /* TAB0MINISQL_H */
