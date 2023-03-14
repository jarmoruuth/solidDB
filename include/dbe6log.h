/*************************************************************************\
**  source       * dbe6log.h
**  directory    * dbe
**  description  * Logical log interface
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


#ifndef DBE6LOG_H
#define DBE6LOG_H

#include <ssc.h>
#include <ssstdlib.h>
#include <sstime.h>
#include <su0types.h>

#include <uti0vtpl.h>

#include <rs0types.h>
#include <rs0entna.h>

#ifndef SS_MYSQL
#ifdef SS_MME
#include <mme0rval.h>
#endif
#endif

#include "dbe0logi.h"
#include "dbe0erro.h"
#include "dbe7cfg.h"
#include "dbe7logf.h"
#include "dbe0type.h"
#include "dbe0logi.h"
#include "dbe6blob.h"
#include "dbe6bmgr.h"
#include "dbe0hsbstate.h"

#ifdef SS_HSBG2
dbe_log_t* dbe_log_transform_init(
        dbe_db_t* db,
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        dbe_hsbg2_t* hsbsvc,
        void* instance_ctx,
        dbe_log_instancetype_t instancetype
);
#endif /* SS_HSBG2 */

dbe_log_t* dbe_log_init(
#ifdef DBE_LOGORDERING_FIX
        dbe_db_t* db,
#endif /* DBE_LOGORDERING_FIX */
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        dbe_log_instancetype_t instancetype
#ifdef SS_HSBG2
        , dbe_hsbg2_t* hsbsvc
#endif /* SS_HSBG2 */
);

void dbe_log_done(
        dbe_log_t* log);

#ifdef SS_HSBG2

dbe_ret_t dbe_log_put_durable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos);

dbe_ret_t dbe_log_put_durable_standalone(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos);

dbe_ret_t dbe_log_put_remote_durable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos);

dbe_ret_t dbe_log_put_remote_durable_ack(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos);

dbe_ret_t dbe_log_putabortall(
        dbe_log_t* log);

dbe_ret_t dbe_log_puthsbnewstate(
        dbe_log_t* log,
        dbe_hsbstatelabel_t state);

dbe_ret_t dbe_log_putsavelogpos(
        dbe_log_t* log,
        dbe_catchup_savedlogpos_t* savedpos);

#endif /* SS_HSBG2 */

dbe_ret_t dbe_log_puthsbcommitmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logi_commitinfo_t info,
        dbe_trxid_t trxid 
#ifdef DBE_LOGORDERING_FIX
        , rep_params_t *rp,
        bool *p_replicated);
#else /* DBE_LOGORDERING_FIX */
        );
#endif /* DBE_LOGORDERING_FIX */

dbe_ret_t dbe_log_puttrxmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_logi_commitinfo_t info,
        dbe_trxid_t trxid,
        dbe_hsbmode_t trxhsbmode);

dbe_ret_t dbe_log_putcpmark(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        dbe_cpnum_t cpnum,
        SsTimeT ts,
        bool final_checkpoint,
        bool* p_splitlog);

dbe_ret_t dbe_log_puttuple(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dynvtpl_t logdata,
        ulong relid
#ifdef DBE_LOGORDERING_FIX
        , rep_params_t *rp,
        bool *p_replicated);
#else /* DBE_LOGORDERING_FIX */
        );
#endif /* DBE_LOGORDERING_FIX */

dbe_ret_t dbe_log_putcreatetable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name,
        rs_ano_t nkeys,
        rs_ano_t nattrs);

dbe_ret_t dbe_log_putrenametable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name);

dbe_ret_t dbe_log_putdroptable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname);

dbe_ret_t dbe_log_putaltertable(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname,
        rs_ano_t nnewattrs);

dbe_ret_t dbe_log_putcreateordropidx(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        ulong keyid,
        char* relname);

dbe_ret_t dbe_log_putcreateuser(
        dbe_log_t* log,
        dbe_logrectype_t logrectype);

dbe_ret_t dbe_log_putstmtmark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dbe_trxid_t stmttrxid);

dbe_ret_t dbe_log_putincsysctr(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        dbe_sysctrid_t ctrid);

dbe_ret_t dbe_log_puthsbsysctr(
        dbe_log_t* log,
        char* data);

dbe_ret_t dbe_log_putcreatectrorseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname);

dbe_ret_t dbe_log_putdropctrorseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname);

dbe_ret_t dbe_log_putincctr(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid);

dbe_ret_t dbe_log_putsetctr(
        dbe_log_t* log,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid,
        rs_tuplenum_t* value);

dbe_ret_t dbe_log_putsetseq(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t seqid,
        rs_tuplenum_t* value);

dbe_ret_t dbe_log_puthotstandbymark(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid);

dbe_ret_t dbe_log_putreplicatrxstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotetrxid);

dbe_ret_t dbe_log_putreplicastmtstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotestmtid,
        dbe_trxid_t localstmtid);

dbe_ret_t dbe_log_putauditinfo(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        long userid,
        char* info);

dbe_ret_t dbe_log_putblobstart(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_trxid_t trxid,
        dbe_vablobref_t blobref);

dbe_ret_t dbe_log_putblobdata(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        char *data,
        size_t datasize);

dbe_ret_t dbe_log_putblobg2data(
        dbe_log_t* log,
        rs_sysi_t* cd,
        ss_byte_t* data, 
        size_t datasize);

dbe_ret_t dbe_log_putblobg2dropmemoryref(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_blobg2id_t blobid);

dbe_ret_t dbe_log_putblobg2datacomplete(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_blobg2id_t blobid);

dbe_ret_t dbe_log_idleflush(
        dbe_log_t* log);

dbe_ret_t dbe_log_flushtodisk(
        dbe_log_t* log);

dbe_ret_t dbe_log_waitflushmes(
        dbe_log_t* log,
        rs_sysi_t* cd);

void dbe_log_setidlehsbdurable(
        dbe_log_t* log,
        dbe_logfile_idlehsbdurable_t mode);

ulong dbe_log_getsize(dbe_log_t* log, bool lastonly);

void dbe_log_seterrorhandler(
        dbe_log_t* log,
        void (*errorfunc)(void*),
        void* errorctx);

ulong dbe_log_getfilewritecnt(
        dbe_log_t* log);


#ifdef DBE_LOGORDERING_FIX

void dbe_log_lock(dbe_log_t *log);

void dbe_log_unlock(dbe_log_t *log);

#endif /* DBE_LOGORDERING_FIX */

#ifdef SS_MME

dbe_ret_t dbe_log_putmmetuple(
        dbe_log_t* log,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        mme_rval_t* logdata,
        ulong relid,
        rep_params_t *rp,
        bool *p_replicated);

#endif /* SS_MME */

dbe_catchup_logpos_t dbe_log_getlogpos(
        dbe_log_t* log);

#ifdef SS_HSBG2

dbe_catchup_logpos_t dbe_log_getcatchuplogpos(dbe_log_t* log);

dbe_ret_t dbe_log_put_hsb_new_primary(
        dbe_log_t* log,
        long originator_nodeid,
        long primary_nodeid);

void dbe_log_set_final_checkpoint(
        dbe_log_t* log,
        int final_checkpoint);

#endif /* SS_HSBG2 */

dbe_ret_t dbe_log_put_comment(
        dbe_log_t* log,
        char* data,
        size_t datafize);

#endif /* DBE6LOG_H */
