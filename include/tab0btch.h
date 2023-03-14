/*************************************************************************\
**  source       * tab0btch.h
**  directory    * tab
**  description  * Table level SQL "batch executor".
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


#ifndef TAB0BTCH_H
#define TAB0BTCH_H

#include <rs0sysi.h>

#include <su0err.h>

#include <rs0types.h>
#include <rs0sysi.h>
#include <rs0defno.h>

#include "tab0type.h"
#include "tab0tran.h"
#include "tab0conn.h"
#include "tab0sqls.h"

typedef struct tb_batch_st tb_batch_t;

tb_batch_t* tb_batch_init(
        tb_database_t* tdb,
        char* name,
        su_list_t* sqllist,
        su_list_t* inpttypes,
        su_list_t* inptvals,
        long uid,
        void (*task_wait_tmofunp)(void*, long),
        void (*task_wait_funp)(void*),
        void (*task_wakeup_funp)(void*));

void tb_batch_done(tb_batch_t* bt);

bool tb_batch_exec_task(
        tb_batch_t* bt,
        void* task,
        tb_connect_t* tbc,
        bool* finishedp,
        bool abortonerror);

rs_err_t* tb_batch_geterrh(
        tb_batch_t* bt);

void tb_batch_setdefnode(
        tb_batch_t* bt,
        rs_defnode_t* defnode);

void tb_batch_listitemdelete(
        void* data);

bool tb_batch_didpreparefail(
        tb_batch_t* bt);

bool tb_batch_cancel_task(
        tb_batch_t* bt,
        void* task);

#endif /* TAB0BTCH_H */
