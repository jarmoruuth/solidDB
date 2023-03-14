/*************************************************************************\
**  source       * tab0sqls.h
**  directory    * tab
**  description  * SQL system functions
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


#ifndef TAB0SQLS_H
#define TAB0SQLS_H

#include <rs0types.h>

sqlsystem_t* tb_sqls_init(
        void* cd);

sqlsystem_t* tb_sqls_init_hurc(
        void* cd);

sqlsystem_t* tb_sqls_init_syncpublinsert(
        void* cd);

void tb_sqls_done(
        void* cd,
        sqlsystem_t* sqls);

void tb_sqls_builderrh(
        void* cd,
        sqlsystem_t* sqls,
        rs_err_t** p_errh);

void* tb_sqls_memalloc(size_t size);
void* tb_sqls_memrealloc(void *ptr, size_t size);
void  tb_sqls_memfree(void *ptr);
void  tb_sqls_internalerror(void* cd, char* file, uint line);

#ifdef SS_DEBUG
void  tab_sqls_rs_error_printinfo(void* cd, rs_err_t* errh, uint* errcode,
                                  char** errstr);
char* tab_sqls_rs_aval_print(void* cd, rs_atype_t* atype, rs_aval_t* aval);
char* tab_sqls_tb_info_option(void* cd, tb_trans_t* trans, char* option);
#endif /* SS_DEBUG */
#endif /* TAB0SQLS_H */
