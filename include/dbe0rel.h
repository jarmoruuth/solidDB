/*************************************************************************\
**  source       * dbe0rel.h
**  directory    * dbe
**  description  * Relation insert, update and delete operations.
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


#ifndef DBE0REL_H
#define DBE0REL_H

#include <ssc.h>

#include <rs0error.h>
#include <rs0relh.h>
#include <rs0tval.h>

#include "dbe0erro.h"
#include "dbe0tref.h"
#include "dbe0trx.h"

dbe_ret_t dbe_rel_insert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh);

dbe_ret_t dbe_rel_replicainsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh);

dbe_ret_t dbe_rel_quickinsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        rs_err_t** p_errh);

#ifdef SS_BLOCKINSERT
dbe_ret_t dbe_rel_blockinsert(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval,
        int blockindex,
        rs_err_t** p_errh);
#endif /* SS_BLOCKINSERT */

dbe_ret_t dbe_rel_delete(
        dbe_trx_t*  trx,
        rs_relh_t*  relh,
        dbe_tref_t* tref,
        rs_err_t**  p_errh);

dbe_ret_t dbe_rel_update(
        dbe_trx_t*  trx,
        rs_relh_t*  relh,
        dbe_tref_t* old_tref,
        bool*       upd_attrs,
        rs_tval_t*  upd_tval,
        dbe_tref_t* new_tref,
        rs_err_t**  p_errh);

#endif /* DBE0REL_H */
