/*************************************************************************\
**  source       * dbe1trdd.h
**  directory    * dbe
**  description  * Transaction data dictionary operations.
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


#ifndef DBE1TRDD_H
#define DBE1TRDD_H

#include <ssenv.h>

#include <ssc.h>

#include <rs0types.h>
#include <rs0entna.h>

#include "dbe0type.h"
#include "dbe0erro.h"

typedef struct dbe_trdd_st dbe_trdd_t;

dbe_trdd_t* dbe_trdd_init(
        void* cd,
        dbe_db_t* db,
        dbe_trx_t* trx,
        dbe_trxid_t usertrxid,
        dbe_trxid_t stmttrxid,
        dbe_log_t* log);

dbe_ret_t dbe_trdd_done(
        dbe_trdd_t* trdd,
        bool trxsuccp);

dbe_ret_t dbe_trdd_cleanup(
        dbe_trdd_t* trdd,
        bool trxsuccp);

void dbe_trdd_unlinkblobs(
        dbe_trdd_t* trdd);

void dbe_trdd_startcommit(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        dbe_trdd_t* trdd);

long dbe_trdd_getnindexwrites(
        dbe_trdd_t* trdd);

void dbe_trdd_stmt_begin(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid);

void dbe_trdd_stmt_commit(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid);

void dbe_trdd_stmt_rollback(
        dbe_trdd_t* trdd,
        dbe_trxid_t stmttrxid);

dbe_ret_t dbe_trdd_commit_advance(
        dbe_trdd_t* trdd,
        bool* p_ddopisactive);

dbe_ret_t dbe_trdd_rollback(
        dbe_trdd_t* trdd);

bool dbe_trdd_relinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* relname,
        rs_relh_t** p_relh);

bool dbe_trdd_reldeleted(
        dbe_trdd_t* trdd,
        rs_entname_t* relname);

bool dbe_trdd_indexinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* indexname,
        rs_relh_t** p_relh,
        rs_key_t** p_key);

bool dbe_trdd_viewinserted(
        dbe_trdd_t* trdd,
        rs_entname_t* viewname,
        rs_viewh_t** p_viewh);

bool dbe_trdd_viewdeleted(
        dbe_trdd_t* trdd,
        rs_entname_t* viewname);

dbe_ret_t dbe_trdd_insertrel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh);

dbe_ret_t dbe_trdd_deleterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh);

dbe_ret_t dbe_trdd_truncaterel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        bool physdelete);

dbe_ret_t dbe_trdd_alterrel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        int ncolumns);

int dbe_trdd_newcolumncount(
        dbe_trdd_t* trdd,
        rs_relh_t* relh);

dbe_ret_t dbe_trdd_renamerel(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_entname_t* newname);

dbe_ret_t dbe_trdd_insertindex(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_key_t* key);

dbe_ret_t dbe_trdd_deleteindex(
        dbe_trdd_t* trdd,
        rs_relh_t* relh,
        rs_key_t* key);

dbe_ret_t dbe_trdd_insertseq(
        dbe_trdd_t* trdd,
        rs_entname_t* seq_name,
        long seq_id,
        bool densep);

dbe_ret_t dbe_trdd_deleteseq(
        dbe_trdd_t* trdd,
        rs_entname_t* seq_name,
        long seq_id,
        bool densep);

dbe_ret_t dbe_trdd_insertname(
        dbe_trdd_t* trdd,
        rs_entname_t* name);

dbe_ret_t dbe_trdd_deletename(
        dbe_trdd_t* trdd,
        rs_entname_t* name);

bool dbe_trdd_namedeleted(
        dbe_trdd_t* trdd,
        rs_entname_t* name);

dbe_ret_t dbe_trdd_insertevent(
        dbe_trdd_t* trdd,
        rs_event_t* event);

dbe_ret_t dbe_trdd_deleteevent(
        dbe_trdd_t* trdd,
        rs_entname_t* name);

dbe_ret_t dbe_trdd_insertview(
        dbe_trdd_t* trdd,
        rs_viewh_t* viewh);

dbe_ret_t dbe_trdd_deleteview(
        dbe_trdd_t* trdd,
        rs_viewh_t* viewh);

dbe_ret_t dbe_trdd_setrelhchanged(
        dbe_trdd_t* trdd,
        rs_relh_t* relh);

void dbe_trdd_setlog(
        dbe_trdd_t* trdd,
        dbe_log_t* log);

#ifdef SS_DEBUG
int dbe_trdd_listlen(
        dbe_trdd_t* trdd);
#endif /* SS_DEBUG */

#endif /* DBE1TRDD_H */
