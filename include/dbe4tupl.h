/*************************************************************************\
**  source       * dbe4tupl.h
**  directory    * dbe
**  description  * Tuple insert, update and delete operations.
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


#ifndef DBE4TUPL_H
#define DBE4TUPL_H

#include <ssc.h>

#include <rs0relh.h>
#include <rs0tval.h>

#include "dbe9type.h"
#include "dbe4srch.h"
#include "dbe0erro.h"
#include "dbe0trx.h"
#include "dbe0tref.h"
#include "dbe0type.h"
#include "dbe0brb.h"

typedef enum {
        DBE_TUPLEINSERT_NORMAL,
        DBE_TUPLEINSERT_REPLICA,
        DBE_TUPLEINSERT_QUICK,
        DBE_TUPLEINSERT_BLOCK
} dbe_tupleinsert_t;

typedef struct dbe_tuple_createindex_st dbe_tuple_createindex_t;
typedef struct dbe_tuple_dropindex_st   dbe_tuple_dropindex_t;

int dbe_tuple_printvtpl(
        void* cd,
        rs_relh_t* relh,
        vtpl_t* vtpl,
        bool trefp,
        bool oldhsb);

dbe_ret_t dbe_tuple_insert_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        rs_tval_t* tval,
        dbe_tupleinsert_t type);

dbe_ret_t dbe_tuple_recovinsert(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxnum_t trxnum,
        dbe_trxid_t stmttrxid,
        rs_relh_t* relh,
        vtpl_t* clustkey_vtpl,
        bool clustkey_isblob);

dbe_ret_t dbe_tuple_delete_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        dbe_tref_t* tref,
        dbe_search_t* search);

dbe_ret_t dbe_tuple_recovdelete(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxnum_t trxnum,
        dbe_trxid_t stmttrxid,
        rs_relh_t* relh,
        vtpl_t* tref_vtpl,
        bool hsbrecov);

dbe_ret_t dbe_tuple_update_disk(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        rs_relh_t* relh,
        dbe_tref_t* old_tref,
        bool* new_attrs,
        rs_tval_t* new_tval,
        dbe_tref_t* new_tref,
        dbe_search_t* search);

dbe_ret_t dbe_tuple_copyblobaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

bool dbe_tuple_isnocheck(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh);

bool trx_uniquecheck_isnull(
        rs_sysi_t* cd,
        rs_key_t* key,
        vtpl_t* key_vtpl);

dbe_tuple_createindex_t* dbe_tuple_createindex_init(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key);

void dbe_tuple_createindex_done(
        dbe_tuple_createindex_t* ci);

dbe_ret_t dbe_tuple_createindex_advance(
        dbe_tuple_createindex_t* ci);

dbe_tuple_dropindex_t* dbe_tuple_dropindex_init(
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_key_t* key,
        bool truncate,
        su_list_t* deferredblobunlinklist);

void dbe_tuple_dropindex_done(
        dbe_tuple_dropindex_t* di);

dbe_ret_t dbe_tuple_dropindex_advance(
        dbe_tuple_dropindex_t* di);

dbe_ret_t dbe_tuple_recovdroprel(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        bool truncate);

dbe_ret_t dbe_tuple_recovcreateindex(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        ulong keyid);

dbe_ret_t dbe_tuple_recovdropindex(
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_trxnum_t committrxnum,
        rs_relh_t* relh,
        ulong keyid,
        bool truncate);

dbe_tuplestate_t *dbe_tuplestate_init(
        void);

void dbe_tuplestate_done(
        dbe_tuplestate_t *ts);

void dbe_tuplestate_setfailed(
        dbe_tuplestate_t *ts);

#endif /* DBE4TUPL_H */
