/*************************************************************************\
**  source       * tab0blobg2.h
**  directory    * tab
**  description  * Blob stream G2 interface of TAB
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


#ifndef TAB0BLOBG2_H
#define TAB0BLOBG2_H

#include <rs0sysi.h>
#include <rs0ttype.h>
#include <rs0tval.h>

#include <dbe0blobg2.h>

extern char tb_blobg2_sysblobs_create_stmts[];

tb_blobg2mgr_t* tb_blobg2mgr_init(
        dbe_db_t* db);

void tb_blobg2mgr_done(
        tb_blobg2mgr_t* bm);

void tb_blobg2mgr_sethsb(
        tb_blobg2mgr_t* bm,
        bool hsb);

su_ret_t tb_blobg2mgr_blobdeletebyid(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

/* reference count services */
su_ret_t tb_blobg2mgr_incrementinmemoryrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_decrementinmemoryrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_incrementpersistentrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_decrementpersistentrefcount(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_decrementpersistentrefcount_hsb(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_decrementpersistentrefcount_callback(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_incrementinmemoryrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_decrementinmemoryrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_incrementpersistentrefcount_byva(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_flushallwblobs(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_delete_unreferenced_blobs_after_recovery(
        rs_sysi_t* cd,
        size_t* p_nblobs_deleted,
        su_err_t** p_errh);

su_ret_t tb_blobg2mgr_copy_old_blob_to_g2(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_err_t** p_errh);

tb_wblobg2stream_t*  tb_blobg2mgr_initwblobstream(
        rs_sysi_t* cd,
        tb_blobg2mgr_t* bm,
        rs_atype_t* atype,
        rs_aval_t* aval);

su_ret_t tb_wblobg2stream_done(
        tb_wblobg2stream_t* wbs,
        su_err_t** p_errh);

void tb_wblobg2stream_abort(
        tb_wblobg2stream_t* wbs);

su_ret_t tb_wblobg2stream_reach(
        tb_wblobg2stream_t* wbs,
        ss_byte_t** pp_buf,
        size_t* p_avail,
        su_err_t** p_errh);

su_ret_t tb_wblobg2stream_release(
        tb_wblobg2stream_t* wbs,
        size_t nbytes,
        su_err_t** p_errh);

tb_wblobg2stream_t*  tb_blobg2mgr_initwblobstream_bycd(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

tb_wblobg2stream_t* tb_blobg2mgr_initwblobstream_for_recovery_bycd(
        rs_sysi_t* cd,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset);

tb_rblobg2stream_t* tb_rblobg2stream_init(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        dbe_blobg2size_t* p_totalsize);

su_ret_t tb_rblobg2stream_reach(
        tb_rblobg2stream_t* rbs,
        ss_byte_t** pp_buf,
        size_t* p_nbytes,
        su_err_t** p_errh);

su_ret_t tb_rblobg2stream_release(
        tb_rblobg2stream_t* rbs,
        size_t nbytes,
        su_err_t** p_errh);

void tb_rblobg2stream_done(
        tb_rblobg2stream_t* rbs);

bool tb_blobg2_loadblobtoaval_limit(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t sizelimit_or_0);

su_ret_t tb_blobg2_readsmallblobstotvalwithinfo(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        size_t smallblobsizemax_size_t,
        uint* p_nblobs_read,
        uint* p_nblobs_total);

su_ret_t tb_blobg2_readsmallblobstotval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        size_t smallblobsizemax);

/* for db shrink */
su_ret_t tb_blobg2mgr_move_page(
        rs_sysi_t* cd,
        su_daddr_t old_daddr,
        su_daddr_t new_daddr,
        ss_byte_t* page_data,
        void* page_slot);

#endif /* TAB0BLOBG2_H */

