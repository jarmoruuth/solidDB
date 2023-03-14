/*************************************************************************\
**  source       * dbe7rfl.h
**  directory    * dbe
**  description  * Roll forward log interface
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


#ifndef DBE7RFL_H
#define DBE7RFL_H

#include <ssc.h>
#include <ssstdlib.h>
#include <sstime.h>
#include <su0types.h>

#include <uti0vtpl.h>

#include <rs0types.h>
#include <rs0entna.h>

#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe7cfg.h"
#include "dbe7ctr.h"
#include "dbe6log.h"
#include "dbe7logf.h"

#ifdef SS_HSBG2
#include "dbe0catchup.h"
#include "dbe0ld.h"
#include "dbe0hsbstate.h"
#endif /* SS_HSBG2 */

/* Roll-forward log methods start here
 */

dbe_rflog_t* dbe_rflog_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter);

#ifdef SS_HSBG2
dbe_rflog_t* dbe_rflog_catchup_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter,
        dbe_catchup_logpos_t startpos);

dbe_rflog_t* dbe_rflog_hsbinit(
        dbe_logbuf_t* lb,
        size_t bufsize);

su_ret_t dbe_rflog_next_logdata(
        dbe_rflog_t* rflog,
        dbe_logdata_t** ld);

dbe_logdata_t* dbe_rflog_getlogdata(dbe_rflog_t* rfl);

void dbe_rflog_set_collect_logdata(
        dbe_rflog_t* rflog,
        bool b);

dbe_ret_t dbe_rflog_get_hsb_new_primary(
        dbe_rflog_t* rflog,
        long *p_originator_nodeid,
        long *p_primary_nodeid);

#endif /* SS_HSBG2 */

void dbe_rflog_done(
        dbe_rflog_t* rflog);

void dbe_rflog_resetscan(
        dbe_rflog_t* rflog);

size_t dbe_rflog_getremainingbufsize(
        dbe_rflog_t* rflog);
                                
dbe_ret_t dbe_rflog_getnextrecheader(
        dbe_rflog_t* rflog,
        dbe_logrectype_t* p_rectype,
        dbe_trxid_t* p_trxid,
        size_t* p_datasize);

dbe_ret_t dbe_rflog_skip_unscanned_data(
        dbe_rflog_t *rflog);

dbe_ret_t dbe_rflog_readdata(
        dbe_rflog_t* rflog,
        void* buffer,
        size_t bytes_desired,
        size_t* p_bytes_read);

dbe_ret_t dbe_rflog_getvtupleref(
        dbe_rflog_t* rflog,
        vtpl_t** pp_vtpl,
        ulong* p_relid);

dbe_ret_t dbe_rflog_getlogheaderdata(
        dbe_rflog_t* rflog,
        dbe_logfnum_t* p_logfnum,
        dbe_cpnum_t* p_cpnum,
        dbe_hdr_blocksize_t* p_blocksize,
        ss_uint4_t* p_dbcreatime);

dbe_ret_t dbe_rflog_getcpmarkdata_old(
        dbe_rflog_t* rflog,
        dbe_cpnum_t* p_cpnum);

dbe_ret_t dbe_rflog_getcpmarkdata_new(
        dbe_rflog_t* rflog,
        dbe_cpnum_t* p_cpnum,
        SsTimeT* p_ts);

dbe_ret_t dbe_rflog_getcreatetable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        rs_entname_t* name,
        rs_ano_t* p_nkeys,
        rs_ano_t* p_nattrs);

dbe_ret_t dbe_rflog_getrenametable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        rs_entname_t* name);

dbe_ret_t dbe_rflog_getdroptable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        char** p_relname);

dbe_ret_t dbe_rflog_getaltertable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        char** p_relname,
        rs_ano_t* p_nnewattrs);

dbe_ret_t dbe_rflog_getcreateordropindex(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        ulong* p_keyid,
        char** p_relname);

dbe_ret_t dbe_rflog_getcommitinfo(
        dbe_rflog_t* rflog,
        dbe_logi_commitinfo_t* p_info);

dbe_ret_t dbe_rflog_getcommitstmt(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_stmttrxid);

dbe_ret_t dbe_rflog_getincsysctr(
        dbe_rflog_t* rflog,
        dbe_sysctrid_t* p_ctrid);

dbe_ret_t dbe_rflog_gethsbsysctr(
        dbe_rflog_t* rflog,
        char* data);

dbe_ret_t dbe_rflog_getcreatectrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        char** p_ctrorseqname);

dbe_ret_t dbe_rflog_getdropctrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        char** p_ctrorseqname);

dbe_ret_t dbe_rflog_getincctr(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrid);

dbe_ret_t dbe_rflog_getsetctr(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrid,
        rs_tuplenum_t* p_value);

dbe_ret_t dbe_rflog_getsetseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_seqid,
        rs_tuplenum_t* p_value);

dbe_ret_t dbe_rflog_getreplicatrxstart(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_remotetrxid);

dbe_ret_t dbe_rflog_getreplicastmtstart(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_remotestmtid,
        dbe_trxid_t* p_localstmtid);

dbe_ret_t dbe_rflog_getauditinfo(
        dbe_rflog_t* rflog,
        long* p_userid,
        char** p_info);

dbe_ret_t dbe_rflog_getblobg2idandoffset(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id,
        dbe_blobg2size_t* p_offset,
        size_t* p_remaining_datasize);

dbe_ret_t dbe_rflog_getblobg2databuffer(
        dbe_rflog_t* rflog,
        ss_byte_t** p_data, 
        size_t* p_datasize,
        dbe_blobg2id_t* p_id,
        dbe_blobg2size_t* p_offset,
        ss_byte_t** p_remaining_data,
        size_t* p_remaining_datasize);

dbe_ret_t dbe_rflog_getblobg2dropmemoryref(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id);

dbe_ret_t dbe_rflog_getblobg2datacomplete(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id);

void dbe_rflog_saverecordpos(
        dbe_rflog_t* rflog,
        dbe_logpos_t* pos_buf);

void dbe_rflog_restorerecordpos(
        dbe_rflog_t* rflog,
        dbe_logpos_t* pos_buf);

void dbe_rflog_cleartoeof(
        dbe_rflog_t* rflog,
        dbe_logpos_t* logpos);

dbe_ret_t dbe_rflog_getfilenumstart(
        rs_sysi_t *cd,
        char* logdir,
        char* nametemplate,
        char digittemplate,
        size_t bufsize,
        dbe_cpnum_t cpnum,
        dbe_logfnum_t logfnum,
        dbe_logfnum_t* p_startlogfnum);

char* dbe_rflog_getphysicalfname(
        dbe_rflog_t* rflog);

#ifdef SS_MME
dbe_ret_t dbe_rflog_getrval(
        rs_sysi_t* cd,
        dbe_rflog_t* rflog,
        mme_rval_t** pp_rval,
        ulong* p_relid);
#endif

#ifdef SS_HSBG2
dbe_ret_t dbe_rflog_fill_catchuplogpos(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *logpos);

dbe_ret_t dbe_rflog_get_durable(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *p_local_durable_logpos);

dbe_ret_t dbe_rflog_get_remote_durable(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *p_local_durable_logpos,
        dbe_catchup_logpos_t *p_remote_durable_logpos);

dbe_ret_t dbe_rflog_get_hsbnewstate(
        dbe_rflog_t* rflog,
        dbe_hsbstatelabel_t* p_state);

#endif /* SS_HSBG2 */

#endif /* DBE7RFL_H */
