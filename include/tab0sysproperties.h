/*************************************************************************\
**  source       * tab0sysproperties.h
**  directory    * tab
**  description  * Persistent system properties interface
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


#ifndef TAB0SYSPROPERTIS_H
#define TAB0SYSPROPERTIS_H

#include <ssc.h>

typedef char *(*tb_sysproperties_callback_fun_t)(void* ctx);

typedef struct tb_sysproperties_st tb_sysproperties_t;

#include <dbe0hsbstate.h>
#include "tab0conn.h"

void tb_sysproperties_done(tb_sysproperties_t *sp);

void tb_sysproperties_checkpoint(tb_sysproperties_t* sp, rs_sysi_t* cd);

tb_sysproperties_t* tb_sysproperties_init(void);

void tb_sysproperties_start(tb_sysproperties_t* sp, rs_sysi_t* cd);

char *tb_sysproperties_get(tb_sysproperties_t* sp, char *key);

dbe_catchup_logpos_t tb_sysproperties_get_lpid(tb_sysproperties_t* sp, char *key);

void tb_sysproperties_register_callback(
        tb_sysproperties_t* sp, 
        char *key, 
        tb_sysproperties_callback_fun_t callback_fun,
        void *callback_ctx);

void tb_sysproperties_set(
        tb_sysproperties_t* sp, 
        char *key,
        char *value);

void tb_sysproperties_set_lpid(
        tb_sysproperties_t* sp,
        char *key,
        dbe_catchup_logpos_t* value);

void tb_sysproperties_lock(
        tb_sysproperties_t* sp);

void tb_sysproperties_set_nomutex(
        tb_sysproperties_t* sp, 
        char *key,
        char *value);

void tb_sysproperties_unlock(
        tb_sysproperties_t* sp);

dbe_hsbstatelabel_t tb_sysproperties_gethsbstate(
        tb_sysproperties_t* sp);

#endif /* TAB0SYSPROPERTIS_H */
