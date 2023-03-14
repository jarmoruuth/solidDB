/*************************************************************************\
**  source       * dbe0blobg2.h
**  directory    * dbe
**  description  * Blob stream G2 interface of DBE
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

#ifndef DBE0BLOBG2_H
#define DBE0BLOBG2_H

#include <su0error.h>
#include <su0types.h>
#include <rs0aval.h>
#include <su0list.h>
#include "dbe0db.h"
#include "dbe0type.h"

/* these funny forward tb-level declarations are here
 * for strong type checking of BLOB callback routines.
 */
typedef struct tb_wblobg2stream_st tb_wblobg2stream_t;
typedef struct tb_blobg2mgr_st tb_blobg2mgr_t;
typedef struct tb_rblobg2stream_st tb_rblobg2stream_t;

typedef struct dbe_wblobg2_st dbe_wblobg2_t;
typedef struct dbe_rblobg2_st dbe_rblobg2_t;

typedef struct {
        dbe_blobg2id_t brefg2_id;
        dbe_blobg2size_t brefg2_size;
        ss_byte_t brefg2_flags;
} dbe_vablobg2ref_t;


void dbe_brefg2_initbuf(
        dbe_vablobg2ref_t* brefbuf,
        dbe_blobg2id_t bid,
        dbe_blobg2size_t bs);
dbe_blobg2id_t dbe_brefg2_getblobg2id(dbe_vablobg2ref_t* bref);
dbe_blobg2size_t dbe_brefg2_getblobg2size(dbe_vablobg2ref_t* bref);

bool dbe_brefg2_isblobg2check_from_diskbuf(ss_byte_t* data, size_t datasize);
bool dbe_brefg2_isblobg2check_from_va(va_t* va);
bool dbe_brefg2_isblobg2check_from_aval(rs_sysi_t* cd, rs_atype_t* atype, rs_aval_t* aval);
void dbe_brefg2_loadfromdiskbuf(dbe_vablobg2ref_t* bref, ss_byte_t* data, size_t datasize);
void dbe_brefg2_storetodiskbuf(dbe_vablobg2ref_t* bref, ss_byte_t* data, size_t datasize);
void dbe_brefg2_loadfromva(
        dbe_vablobg2ref_t* bref,
        va_t* blobrefva);
void dbe_brefg2_loadfromaval(
        dbe_vablobg2ref_t* bref,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

dbe_blobg2size_t dbe_brefg2_getsizefromaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

dbe_blobg2id_t dbe_brefg2_getidfromaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

void dbe_brefg2_nullifyblobid_from_va(
        va_t* va);

dbe_wblobg2_t* dbe_wblobg2_init(
        dbe_db_t* db,
        dbe_blobg2id_t* p_id_out,
        dbe_cpnum_t* p_startcpnum_out,
        su_ret_t (*getpageaddrfun)(
                void* getpageaddr_ctx,
                su_daddr_t* p_daddr,
                su_err_t** p_errh),
        void* getpageaddr_ctx,
        su_ret_t  (*releasepageaddrfun)(
                void* released_pageaddr_ctx,
                su_daddr_t daddr,
                size_t bytes_in_use,
                su_err_t** p_errh),
        void* releasepageaddr_ctx);

dbe_wblobg2_t* dbe_wblobg2_init_for_recovery(
        dbe_db_t* db,
        dbe_cpnum_t* p_startcpnum_in_out,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startpos,
        su_ret_t (*getpageaddrfun)(
                void* getpageaddr_ctx,
                su_daddr_t* p_daddr,
                su_err_t** p_errh),
        void* getpageaddr_ctx,
        su_ret_t  (*releasepageaddrfun)(void* released_pageaddr_ctx,
                                        su_daddr_t daddr,
                                        size_t bytes_in_use,
                                        su_err_t** p_errh),
        void* releasepageaddr_ctx);

su_ret_t dbe_wblobg2_flush(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        su_err_t** p_errh);

void dbe_wblobg2_cancel(
        dbe_wblobg2_t* wb);

su_ret_t dbe_wblobg2_reach(
        dbe_wblobg2_t* wb,
        ss_byte_t** pp_buf,
        size_t* p_nbytes,
        su_err_t** p_errh);

su_ret_t dbe_wblobg2_release(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        size_t nbytes,
        su_err_t** p_errh);

su_ret_t dbe_wblobg2_close(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        su_err_t** p_errh);

dbe_rblobg2_t* dbe_rblobg2_init(
        dbe_db_t* db,
        dbe_blobg2id_t id,
        su_daddr_t (*getpageaddrfun)(void* getpageaddr_ctx, dbe_blobg2size_t offset),
        void* getpageaddr_ctx);

void dbe_rblobg2_done(
        dbe_rblobg2_t* rb);

su_ret_t dbe_rblobg2_reach(
        dbe_rblobg2_t* rb,
        ss_byte_t** pp_buf,
        size_t* p_nbytes);

void dbe_rblobg2_release(
        dbe_rblobg2_t* rb,
        size_t nbytes);

su_ret_t dbe_blobg2_unlink_by_vtpl(
        rs_sysi_t* cd,
        vtpl_t* vtpl,
        su_err_t** p_errh);

void dbe_blobg2_append_blobids_of_vtpl_to_list(
        rs_sysi_t* cd,
        su_list_t* list,
        vtpl_t* vtpl);

su_ret_t dbe_blobg2_unlink_list_of_blobids(
        rs_sysi_t* cd,
        su_list_t* blobid_list,
        su_err_t** p_errh);

su_ret_t dbe_blobg2_insertaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t maxvalen,
        su_err_t** p_errh);

/* begin db shrink functionality */
void dbe_blobg2_get_id_and_endpos_from_page(
        rs_sysi_t* cd,
        ss_byte_t* pagedata,
        dbe_blobg2id_t* p_bid,
        dbe_blobg2size_t* p_startpos,
        dbe_blobg2size_t* p_endpos);

su_ret_t dbe_blobg2_relocate_page(
        rs_sysi_t* cd,
        ss_byte_t* page_data,
        void* page_slot,
        su_daddr_t new_daddr);

extern su_ret_t (*dbe_blobg2callback_move_page)(
        rs_sysi_t* cd,
        su_daddr_t old_daddr,
        su_daddr_t new_daddr,
        ss_byte_t* page_data,
        void* page_slot);

/* end db shrink functionality */

extern su_ret_t (*dbe_blobg2callback_incrementpersistentrefcount_byva)(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh);
        
extern su_ret_t (*dbe_blobg2callback_decrementpersistentrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

extern su_ret_t (*dbe_blobg2callback_copy_old_blob_to_g2)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_err_t** p_errh);

extern tb_wblobg2stream_t* (*dbe_blobg2callback_wblobinit)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

extern tb_wblobg2stream_t* (*dbe_blobg2callback_wblobinit_for_recovery)(
        rs_sysi_t* cd,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset);

extern su_ret_t (*dbe_blobg2callback_wblobreach)(
        tb_wblobg2stream_t* wbs,
        ss_byte_t** pp_buf,
        size_t* p_avail,
        su_err_t** p_errh);

extern su_ret_t (*dbe_blobg2callback_wblobrelease)(
        tb_wblobg2stream_t* wbs,
        size_t nbytes,
        su_err_t** p_errh);

extern su_ret_t (*dbe_blobg2callback_wblobdone)(
        tb_wblobg2stream_t* wbs,
        su_err_t** p_errh);

extern void (*dbe_blobg2callback_wblobabort)(
        tb_wblobg2stream_t* wbs);

extern su_ret_t (*dbe_blobg2callback_delete_unreferenced_blobs_after_recovery)(
        rs_sysi_t* cd,
        size_t* p_nblobs_deleted,
        su_err_t** p_errh);

extern su_ret_t (*dbe_blobg2callback_incrementinmemoryrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

extern su_ret_t (*dbe_blobg2callback_decrementinmemoryrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

                         
                 
#endif /* DBE0BLOBG2_H */
