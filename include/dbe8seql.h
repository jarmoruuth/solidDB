/*************************************************************************\
**  source       * dbe8seql.h
**  directory    * dbe
**  description  * Sequence list object. Used to save sequence values
**               * to the database during checkpoint.
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


#ifndef DBE8SEQL_H
#define DBE8SEQL_H

#include <sslimits.h>
#include <rs0tnum.h>

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe8flst.h"
#include "dbe8clst.h"
#include "dbe0type.h"

typedef struct dbe_seqlist_st dbe_seqlist_t;
typedef struct dbe_seqlist_st dbe_seqlist_iter_t;

dbe_seqlist_t* dbe_seql_init(
	dbe_cache_t* p_cache,
	dbe_freelist_t* p_freelist,
	dbe_cpnum_t cpnum);

void dbe_seql_done(
	dbe_seqlist_t* p_seql);

su_ret_t dbe_seql_add(
	dbe_seqlist_t* p_seql,
	FOUR_BYTE_T id,
        rs_tuplenum_t* value);

su_ret_t dbe_seql_save(
	dbe_seqlist_t* p_seql,
	su_daddr_t* p_daddr);

su_ret_t dbe_seql_deletefromdisk(
        su_daddr_t start_daddr,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_chlist_t* chlist,
        dbe_cpnum_t cpnum);

dbe_seqlist_iter_t* dbe_seqli_init(
	dbe_cache_t* p_cache,
	su_daddr_t daddr);

void dbe_seqli_done(
	dbe_seqlist_iter_t* p_seqli);

bool dbe_seqli_getnext(
        dbe_seqlist_iter_t *p_seqli,
	FOUR_BYTE_T* p_id,
        rs_tuplenum_t* p_value);

#endif /* DBE8SEQL_H */
