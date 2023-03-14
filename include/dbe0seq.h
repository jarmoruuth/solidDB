/*************************************************************************\
**  source       * dbe0seq.h
**  directory    * dbe
**  description  * Routines for fast sequence handling.
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


#ifndef DBE0SEQ_H
#define DBE0SEQ_H

#include <rs0atype.h>
#include <rs0aval.h>

#include "dbe0type.h"
#include "dbe0trx.h"

/*
        External user functions.
*/

dbe_ret_t dbe_seq_lock(
        dbe_trx_t* trx,
        long seq_id,
        rs_err_t** p_errh);

void dbe_seq_unlock(
        dbe_trx_t* trx,
        long seq_id);

dbe_ret_t dbe_seq_next(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh);

dbe_ret_t dbe_seq_current(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh);

dbe_ret_t dbe_seq_set(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        rs_err_t** p_errh);

dbe_ret_t dbe_seq_setreplica(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        long seq_id,
        bool densep,
        va_t* va,
        rs_err_t** p_errh);

/*
        Internal administrator functions in file dbe1seq.h.
*/

#endif /* DBE0SEQ_H */
