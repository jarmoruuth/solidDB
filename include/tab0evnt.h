/*************************************************************************\
**  source       * tab0evnt.h
**  directory    * tab
**  description  * Event support function
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


#ifndef TAB0EVNT_H
#define TAB0EVNT_H

#include <ssc.h>

#include <rs0error.h>

#include "tab0tran.h"

bool tb_event_create(
        void* cd,
        tb_trans_t* trans,
        char* eventname,
        char* eventschema,
        char* eventcatalog,
        char* eventtext,
        int* paramtypes,
        rs_err_t** p_errh);

bool tb_event_drop(
        void* cd,
        tb_trans_t* trans,
        char* eventname,
        char* eventschema,
        char* eventcatalog,
        rs_err_t** p_errh);

bool tb_event_find(
        void* cd,
        tb_trans_t* trans,
        char* eventname,
        char* eventschema,
        char* eventcatalog,
        char** p_schema,
        char** p_catalog,
        long* p_eventid,
        int* p_paramcount,
        int** p_paramtypes,
        rs_err_t** p_errh);

#endif /* TAB0EVNT_H */

