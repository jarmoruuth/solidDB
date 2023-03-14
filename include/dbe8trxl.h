/*************************************************************************\
**  source       * dbe8trxl.h
**  directory    * dbe
**  description  * This file declares interface to transaction number
**               * list which must be saved upon creation of a checkpoint.
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


#ifndef DBE8TRXL_H
#define DBE8TRXL_H

#include "dbe9type.h"
#include "dbe8cach.h"
#include "dbe8flst.h"
#include "dbe8clst.h"
#include "dbe0type.h"

typedef struct dbe_trxlist_st dbe_trxlist_t;
typedef struct dbe_trxlist_st dbe_trxlist_iter_t;

dbe_trxlist_t *dbe_trxl_init(
        dbe_cache_t *p_cache,
        dbe_freelist_t *p_freelist,
        dbe_cpnum_t cpnum,
        dbe_blocktype_t blocktype);

void dbe_trxl_done(
        dbe_trxlist_t *p_trxl);

su_ret_t dbe_trxl_add(
        dbe_trxlist_t *p_trxl,
        dbe_trxid_t usertrxid,
        dbe_trxnum_t committrxnum);

su_ret_t dbe_trxl_addstmttrx(
        dbe_trxlist_t *p_trxl,
        dbe_trxid_t usertrxid,
        dbe_trxid_t stmttrxid);

su_ret_t dbe_trxl_addrtrx(
        dbe_trxlist_t *p_trxl,
        dbe_trxid_t remotetrxid,
        dbe_trxid_t localtrxid);

su_ret_t dbe_trxl_save(
        dbe_trxlist_t *p_trxl,
        su_daddr_t *p_daddr);

su_ret_t dbe_trxl_deletefromdisk(
        su_daddr_t start_daddr,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_chlist_t* chlist,
        dbe_cpnum_t cpnum);

dbe_trxlist_iter_t *dbe_trxli_init(
        dbe_cache_t *p_cache,
        su_daddr_t daddr,
        dbe_blocktype_t blocktype);

void dbe_trxli_done(
        dbe_trxlist_iter_t *p_trxli);

bool dbe_trxli_getnext(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_usertrxid,
        dbe_trxnum_t* p_committrxnum);

bool dbe_trxli_getnextstmttrx(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_usertrxid,
        dbe_trxid_t* p_stmttrxid);

bool dbe_trxli_getnextrtrx(
        dbe_trxlist_iter_t *p_trxli,
        dbe_trxid_t* p_remotetrxid,
        dbe_trxid_t* p_localtrxid);

#endif /* DBE8TRXL_H */
