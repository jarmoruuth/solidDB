/*************************************************************************\
**  source       * dbe4mme.h
**  directory    * dbe
**  description  * Prototype MME interface.
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


#ifndef DBE4MME_H
#define DBE4MME_H

#ifdef SS_MME

#if defined(SS_MMEG2) /* && !defined(DBE_NOMME) */

#include "dbe4mmeg2.h"

#else

#include <rs0vbuf.h>

#ifndef SS_MYSQL
#include <mme0stor.h>
#endif

#include "dbe0type.h"
#include "dbe0tref.h"
#include "dbe7cfg.h"
#include "dbe7trxb.h"

typedef struct dbe_mme_createindex_st dbe_mme_createindex_t;
typedef struct dbe_mme_dropindex_st dbe_mme_dropindex_t;

/* COMMON SERVICES --------------------------------------------------------- */
dbe_mme_t* dbe_mme_init(
        dbe_db_t*       db,
        dbe_cfg_t*      cfg,
        dbe_cpnum_t     cpnum,
        void*           memctx);

void dbe_mme_done(
        rs_sysi_t*      cd,
        dbe_mme_t*      mme);

void dbe_mme_setreadonly(
        dbe_mme_t*      mme);

void dbe_mme_startservice(
        dbe_mme_t* mme);

mme_storage_t* dbe_mme_getstorage(
        rs_sysi_t* cd,
        dbe_mme_t* mme);

void dbe_mme_removeuser(
        rs_sysi_t*  cd,
        dbe_mme_t*  mme);

void dbe_mme_gettemporarytablecardin(
        rs_sysi_t*      cd,
        dbe_mme_t*      mme,
        rs_relh_t*      relh,
        ss_int8_t*      p_nrows,
        ss_int8_t*      p_nbytes);

/* TRANSACTIONAL SERVICES ------------------------------------------------- */
dbe_ret_t dbe_mme_insert(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        rs_tval_t* tval);

dbe_ret_t dbe_mme_delete(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* tref,
        dbe_mme_search_t* search);

dbe_ret_t dbe_mme_update(
        rs_sysi_t* cd,
        dbe_trx_t* trx,
        rs_relh_t* relh,
        dbe_tref_t* old_tref,
        bool* new_attrs,
        rs_tval_t* new_tval,
        dbe_tref_t* new_tref,
        dbe_mme_search_t* search);

dbe_mme_search_t* dbe_mme_search_init(
        rs_sysi_t*          cd,
        dbe_user_t*         user,
        dbe_trx_t*          trx,
        dbe_trxid_t         usertrxid,
        rs_ano_t*           sellist,
        rs_pla_t*           plan,
        rs_relh_t*          relh,
        rs_key_t*           key,
        dbe_cursor_type_t   cursor_type,
        bool*               p_newplan);

void dbe_mme_search_done(
        dbe_mme_search_t*   search);

void dbe_mme_search_reset(
        dbe_mme_search_t*   search,
        dbe_trx_t*          trx,
        rs_pla_t*           plan);

rs_aval_t* dbe_mme_search_getaval(
        dbe_mme_search_t*   search,
        rs_tval_t*          tval,
        rs_atype_t*         atype,
        uint                kpno);

bool dbe_mme_search_getsearchinfo(
        dbe_mme_search_t*   search,
        rs_pla_t**          p_plan);

void dbe_mme_search_invalidate(
        dbe_mme_search_t* search,
        dbe_trxid_t usertrxid);

void dbe_mme_search_restart(
        dbe_mme_search_t* search,
        dbe_trx_t* trx);

void dbe_mme_search_abortrelid(
        dbe_mme_search_t* search,
        ulong relid);

void dbe_mme_search_abortkeyid(
        dbe_mme_search_t* search,
        ulong keyid);

bool dbe_mme_search_isactive(
        dbe_mme_search_t* search);

dbe_ret_t dbe_mme_search_relock(
        dbe_mme_search_t*   search,
        dbe_trx_t*          trx);

dbe_ret_t dbe_mme_search_nextorprev_n(
        dbe_mme_search_t*   search,
        dbe_trx_t*          trx,
        bool                nextp,
        rs_vbuf_t*          vb);

dbe_ret_t dbe_mme_search_next(
        dbe_mme_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t** p_tval);

dbe_ret_t dbe_mme_search_prev(
        dbe_mme_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t** p_tval);

dbe_ret_t dbe_mme_search_gotoend(
        dbe_mme_search_t* search,
        dbe_trx_t* trx);

dbe_ret_t dbe_mme_search_setposition(
        dbe_mme_search_t* search,
        dbe_trx_t* trx,
        rs_tval_t* tval);

dbe_tref_t* dbe_mme_search_gettref(
        dbe_mme_search_t*   search,
        rs_tval_t*          tval);

void dbe_mme_search_setoptinfo(
        dbe_mme_search_t* search,
        ulong tuplecount);

void dbe_mme_search_newplan(
        dbe_mme_search_t*   search,
        ulong               relid);

void dbe_mme_search_printinfo(
        void* fp,
        dbe_mme_search_t* search);

dbe_mme_locklist_t* dbe_mme_locklist_init(
        rs_sysi_t*          cd,
        dbe_mme_t*          mme);

void dbe_mme_locklist_done(
        dbe_mme_locklist_t* mmll);

void dbe_mme_locklist_replicafree(
        dbe_mme_locklist_t* mmll);

void dbe_mme_locklist_rollback_searches(
        dbe_mme_locklist_t* mmll);

void dbe_mme_locklist_commit(
        dbe_mme_locklist_t* mmll);

void dbe_mme_locklist_rollback(
        dbe_mme_locklist_t* mmll);

dbe_ret_t dbe_mme_locklist_stmt_commit(
        dbe_mme_locklist_t* mmll,
        dbe_trxid_t         stmttrxid,
        bool                groupstmtp);

void dbe_mme_locklist_stmt_rollback(
        dbe_mme_locklist_t* mmll,
        dbe_trxid_t         stmttrxid);

#ifdef SS_MIXED_REFERENTIAL_INTEGRITY
dbe_ret_t dbe_mme_refkeycheck(
        rs_sysi_t*          cd,
        rs_key_t*           refkey,
        rs_relh_t*          refrelh,
        dbe_trx_t*          trx,
        dbe_trxid_t         stmtid,
        dynvtpl_t           value);
#endif

/* CREATE/DROP INDEX ------------------------------------------------------- */
dbe_mme_createindex_t* dbe_mme_createindex_init(
        rs_sysi_t*              cd,
        dbe_trx_t*              trx,
        rs_relh_t*              relh,
        rs_key_t*               key,
        dbe_trxid_t             usertrxid,
        dbe_trxnum_t            committrxnum,
        bool                    commitp);

void dbe_mme_createindex_done(
        dbe_mme_createindex_t*  ci);

dbe_ret_t dbe_mme_createindex_advance(
        dbe_mme_createindex_t*  ci);

dbe_mme_dropindex_t* dbe_mme_dropindex_init(
        rs_sysi_t*              cd,
        dbe_trx_t*              trx,
        rs_relh_t*              relh,
        rs_key_t*               key,
        dbe_trxid_t             usertrxid,
        dbe_trxnum_t            committrxnum,
        bool                    commitp,
        bool                    isclearing,
        bool                    allusers);

void dbe_mme_dropindex_done(
        dbe_mme_dropindex_t*  di);

dbe_ret_t dbe_mme_dropindex_advance(
        dbe_mme_dropindex_t*  di);

/* RECOVERY SERVICES ------------------------------------------------------- */
void dbe_mme_beginrecov(
        dbe_mme_t*          mme);

void dbe_mme_recovinsert(
        rs_sysi_t*          cd,
        dbe_mme_t*          mme,
        dbe_trxbuf_t*       tb,
        rs_relh_t*          relh,
        mme_page_t*         page,
        mme_rval_t*         rval,
        dbe_trxid_t         trxid,
        dbe_trxid_t         stmtid);

void dbe_mme_recovplacepage(
        rs_sysi_t*              cd,
        dbe_mme_t*              mme,
        rs_relh_t*              relh,
        mme_page_t*             page,
        mme_rval_t*             rval,
        dbe_trxid_t             trxid,
        dbe_trxid_t             stmtid);

void dbe_mme_recovstmtcommit(
        dbe_mme_t*          mme,
        dbe_trxid_t         trxid,
        dbe_trxid_t         stmtid);

void dbe_mme_recovstmtrollback(
        dbe_mme_t*          mme,
        dbe_trxid_t         trxid,
        dbe_trxid_t         stmtid);

void dbe_mme_recovcommit(
        dbe_mme_t*          mme,
        dbe_trxid_t         trxid);

void dbe_mme_recovrollback(
        dbe_mme_t*          mme,
        dbe_trxid_t         trxid);

int dbe_mme_abortall(
        dbe_mme_t*              mme);

void dbe_mme_endrecov(
        dbe_mme_t*              mme);

mme_rval_t* dbe_mme_rval_init_from_diskbuf(
        rs_sysi_t* cd,
        ss_byte_t* diskbuf,
        size_t diskbuf_size,
        mme_rval_t* prev,
        void* owner,
        mme_rval_type_t type);

void dbe_mme_rval_done(
        rs_sysi_t* cd,
        mme_rval_t* rval,
        mme_rval_type_t type);

#if !defined(SS_PURIFY) && defined(SS_FFMEM)
void* dbe_mme_getmemctx(dbe_mme_t* mme);
#endif /* !SS_PURIFY && SS_FFMEM */

void dbe_mme_gettvalsamples_nomutex(
        rs_sysi_t* cd,
        dbe_mme_t* mme,
        rs_relh_t* relh,
        rs_tval_t** sample_tvalarr,
        size_t sample_size);

void dbe_mme_lock_mutex(
        rs_sysi_t* cd,
        dbe_mme_t* mme);

void dbe_mme_unlock_mutex(
        rs_sysi_t* cd,
        dbe_mme_t* mme);

#ifdef SS_DEBUG
void dbe_mme_check_integrity(
        dbe_mme_t*      mme);
#endif

#endif /* !SS_MMEG2 */
#endif /* SS_MME */
#endif /* DBE4MME_H */

