/*************************************************************************\
**  source       * tab0proc.h
**  directory    * tab
**  description  * Procedure support function
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


#ifndef TAB0PROC_H
#define TAB0PROC_H

#include <ssc.h>

#include <rs0types.h>
#include <rs0error.h>

#include "tab0tran.h"

bool tb_proc_create(
        void* cd,
        tb_trans_t* trans,
        char* procname,
        char* procschema,
        char* proccatalog,
        char* procstr,
        rs_ttype_t* inpttype,
        rs_ttype_t* outpttype,
        bool isfunction,
        rs_err_t** p_errh);

bool tb_proc_drop(
        void* cd,
        tb_trans_t* trans,
        char* procname,
        char* procschema,
        char* proccatalog,
        rs_err_t** p_errh);

bool tb_proc_find(
        void* cd,
        tb_trans_t* trans,
        char* procname,
        char* procschema,
        char* proccatalog,
        char** p_schema,
        char** p_catalog,
        char** p_procstr,
        long* p_id,
        int* p_type,
        bool isfunction,
        rs_err_t** p_errh);

bool tb_proc_createproccolumninfo(
        void* cd,
        tb_trans_t* trans,
        char* procstr,
        long  procid,
        rs_entname_t* en,
        rs_ttype_t* inpttype,
        rs_ttype_t* outpttype,
        bool converting_database,
        rs_err_t** p_errh);

#ifdef STORED_SQL_FUNCTIONS
uint tb_func_describefun(
        void* cd,
        char* fname,
        void** phandle,
        bool* p_pure,
        rs_atype_t** p_defpartype,
        uint parno
);

void tb_func_releasefunhandle(
    void* cd,
    void* fhandle);

bool tb_func_callfun_sql(
        void*        cd,
        char*        fname,
        void*        fhandle,
        rs_atype_t*  atypearray[],
        rs_aval_t*   avalarray[],
        rs_atype_t** p_res_atype,
        rs_aval_t**  p_res_aval,
        void**       cont,
        rs_err_t**   errhandle
);

bool tb_func_callfun(
        void*        cd,
        char*        fname,
        void*        fhandle,
        rs_atype_t*  atypearray[],
        rs_aval_t*   avalarray[],
        rs_atype_t** p_res_atype,
        rs_aval_t**  p_res_aval,
        bool         usestmtgroup,
        void**       cont,
        rs_err_t**   errhandle
);
#endif

#endif /* TAB0PROC_H */
