/*************************************************************************\
**  source       * tab0trig.h
**  directory    * tab
**  description  * Trigger support functions
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


#ifndef TAB0TRIG_H
#define TAB0TRIG_H

#include <rs0relh.h>

#include "tab0tli.h"

typedef enum {
        TB_TRIG_NONE = 0,
        TB_TRIG_BEFOREINSERT,
        TB_TRIG_AFTERINSERT,
        TB_TRIG_BEFOREUPDATE,
        TB_TRIG_AFTERUPDATE,
        TB_TRIG_BEFOREDELETE,
        TB_TRIG_AFTERDELETE
} tb_trigtype_t;

bool tb_trig_create(
        void* cd,
        tb_trans_t* trans,
        char* trigname,
        char* trigauthid,
        char* trigcatalog,
        char* trigstr,
        tb_relh_t* trigrelh,
        tb_trigtype_t trigtype,
        rs_err_t** p_errh);

bool tb_trig_drop(
        void* cd,
        tb_trans_t* trans,
        char* trigname,
        char* trigauthid,
        char* trigcatalog,
        rs_err_t** p_errh);

bool tb_trig_alter(
        void* cd,
        tb_trans_t* trans,
        char* trigname,
        char* trigauthid,
        char* trigcatalog,
        bool enablep,
        rs_err_t** p_errh);

bool tb_trig_find(
        void* cd,
        tb_trans_t* trans,
        char* trigname,
        char* trigauthid,
        char* trigcatalog,
        char** p_trigauthid,
        char** p_trigcatalog,
        rs_err_t** p_errh);

void tb_trig_findall(
        void* cd,
        tb_trans_t* trans,
        rs_relh_t* relh);

void tb_trig_droprelh(
        TliConnectT* tcon,
        rs_relh_t* relh);

#endif /* TAB0TRIG_H */
