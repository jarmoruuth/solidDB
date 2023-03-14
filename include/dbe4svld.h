/*************************************************************************\
**  source       * dbe4svld.h
**  directory    * dbe
**  description  * Search validation routines for transaction
**               * validation.
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


#ifndef DBE4SVLD_H
#define DBE4SVLD_H

#include <ssc.h>

#include <rs0relh.h>
#include <rs0tval.h>
#include <rs0pla.h>

#include "dbe0type.h"
#include "dbe0erro.h"

typedef struct dbe_seavld_st dbe_seavld_t;

dbe_seavld_t* dbe_seavld_init(
        dbe_user_t* user,
        dbe_trx_t* trx,
        rs_pla_t* plan,
        dbe_searchrange_t* search_range,
        dbe_trxnum_t maxtrxnum,
        dbe_trxnum_t mintrxnum,
        bool earlyvld);

void dbe_seavld_done(
        dbe_seavld_t* seavld);

dbe_ret_t dbe_seavld_next(
        dbe_seavld_t* seavld,
        dbe_trxid_t* p_tuple_trxid);

#endif /* DBE4SVLD_H */
