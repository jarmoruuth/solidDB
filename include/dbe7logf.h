/*************************************************************************\
**  source       * dbe6log.h
**  directory    * dbe
**  description  * Log file interface
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


#ifndef DBE7LOGF_H
#define DBE7LOGF_H

#include <ssc.h>
#include <ssstdlib.h>
#include <sstime.h>
#include <su0types.h>

#include <uti0vtpl.h>

#include <rs0types.h>
#include <rs0entna.h>
#include "dbe0erro.h"
#include "dbe7cfg.h"
#include "dbe7hdr.h"
#include "dbe7ctr.h"
#include "dbe0type.h"

#ifdef SS_HSBG2
#include "dbe0hsbg2.h"
#endif /* SS_HSBG2 */

/* Minimum number of digit positions in logfile name template
*/
#define DBE_LOGFILENAME_MINDIGITS   4
#define DBE_LOGFILENAME_MAXDIGITS   10

typedef enum {
        DBE_LOGFILE_IDLEHSBDURABLE_ON,
        DBE_LOGFILE_IDLEHSBDURABLE_OFF,
        DBE_LOGFILE_IDLEHSBDURABLE_DISABLE
} dbe_logfile_idlehsbdurable_t;

/* logfile header record data */
typedef struct {
        dbe_logfnum_t       lh_logfnum;     /* file # */
        dbe_cpnum_t         lh_cpnum;       /* value of cp number at creat */
        dbe_hdr_blocksize_t lh_blocksize;   /* block size check */
        ss_uint4_t          lh_dbcreatime;  /* database creation time */
} loghdrdata_t;

#define LOGFILE_HEADERSIZE \
        (sizeof(dbe_logfnum_t) + sizeof(dbe_cpnum_t) +\
         sizeof(dbe_hdr_blocksize_t) + sizeof(ss_uint4_t))

/* This structure is declared in header because
 * the clients need to see the size of it in order
 * to allocate storage for a saved scan position
 */
typedef struct {
        su_daddr_t  lp_daddr;
        size_t      lp_bufpos;
} dbe_logpos_t;

dbe_logfile_t* dbe_logfile_init(
#ifdef SS_HSBG2
        dbe_hsbg2_t* hsbsvc,
        dbe_log_instancetype_t instancetype,
#endif /* SS_HSBG2 */
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime,
        su_cipher_t* cipher);

void dbe_logfile_done(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_flush(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_idleflush(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_flushtodisk(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_waitflushmes(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd);

void dbe_logfile_setidlehsbdurable(
        dbe_logfile_t* logfile,
        dbe_logfile_idlehsbdurable_t mode);

void dbe_logfile_set_groupcommit_queue_flush(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_putdata(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten);

dbe_ret_t dbe_logfile_puttrxmark(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid);

dbe_ret_t dbe_logfile_putcommitmark_noflush(
        dbe_logfile_t* logfile,
        dbe_trxid_t trxid);

dbe_ret_t dbe_logfile_putcpmark(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_cpnum_t cpnum,
        SsTimeT ts,
        bool* p_splitlog);

dbe_ret_t dbe_logfile_puttuple(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dynvtpl_t logdata,
        ulong relid);

dbe_ret_t dbe_logfile_putcreatetable(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name,
        rs_ano_t nkeys,
        rs_ano_t nattrs);

dbe_ret_t dbe_logfile_putrenametable(
        dbe_logfile_t* logfile,
        dbe_trxid_t trxid,
        ulong relid,
        rs_entname_t* name);

dbe_ret_t dbe_logfile_putdroptable(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname);

dbe_ret_t dbe_logfile_putaltertable(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        char* relname,
        rs_ano_t nnewattrs);

dbe_ret_t dbe_logfile_putcreateordropidx(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ulong relid,
        ulong keyid,
        char* relname);

dbe_ret_t dbe_logfile_putcreateuser(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype);

dbe_ret_t dbe_logfile_putstmtmark(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        dbe_trxid_t stmttrxid);

dbe_ret_t dbe_logfile_putincsysctr(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_sysctrid_t ctrid);

dbe_ret_t dbe_logfile_puthsbsysctr(
        dbe_logfile_t* logfile,
        char* data);

dbe_ret_t dbe_logfile_putcreatectrorseq(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname);

dbe_ret_t dbe_logfile_putdropctrorseq(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t ctrorseqid,
        char* ctrorseqname);

dbe_ret_t dbe_logfile_putincctr(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid);

dbe_ret_t dbe_logfile_putsetctr(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        ss_uint4_t ctrid,
        rs_tuplenum_t* value);

dbe_ret_t dbe_logfile_putsetseq(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        ss_uint4_t seqid,
        rs_tuplenum_t* value);

dbe_ret_t dbe_logfile_puthotstandbymark(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid);

dbe_ret_t dbe_logfile_putreplicatrxstart(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotetrxid);

dbe_ret_t dbe_logfile_putreplicastmtstart(
        dbe_logfile_t* logfile,
        dbe_logrectype_t logrectype,
        dbe_trxid_t localtrxid,
        dbe_trxid_t remotestmtid,
        dbe_trxid_t localstmtid);

dbe_ret_t dbe_logfile_putauditinfo(
        dbe_logfile_t* logfile,
        dbe_trxid_t trxid,
        long userid,
        char* info);

char* dbe_logfile_genname(
        char* logdir,
        char* nametemplate,
        dbe_logfnum_t logfnum,
        char digittemplate);

ulong dbe_logfile_getsize(
        dbe_logfile_t* logfile);

ulong dbe_logfile_getsize2(
        dbe_logfile_t* logfile);

ulong dbe_logfile_getbufsize(
        dbe_logfile_t* logfile);

void dbe_logfile_seterrorhandler(
        dbe_logfile_t* logfile,
        void (*errorfunc)(void*),
        void* errorctx);

ulong dbe_logfile_getfilewritecnt(
        dbe_logfile_t* logfile);

long dbe_logpos_cmp(
        dbe_logpos_t* lp1,
        dbe_logpos_t* lp2);

dbe_ret_t dbe_logfile_putdata_splitif(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        dbe_logrectype_t logrectype,
        dbe_trxid_t trxid,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalenwritten,
        bool* p_splitlog);

dbe_logfile_t* dbe_logfile_transform_init(
#ifdef SS_HSBG2
        dbe_hsbg2_t *hsbsvc,
        dbe_log_instancetype_t instancetype,
        void* instance_ctx,
#endif /* SS_HSBG2 */
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        bool newdb,
        ulong dbcreatime);

void* dbe_logfile_getdatalenandlen(
        dbe_logrectype_t logrectype,
        void* logdata,
        ss_uint4_t logdatalen_or_relid,
        size_t* p_logdatalen);

dbe_catchup_logpos_t dbe_logfile_getlogpos(
        dbe_logfile_t* logfile);

dbe_ret_t dbe_logfile_put_durable(
        dbe_logfile_t* logfile,
        rs_sysi_t* cd,
        hsb_role_t role,
        dbe_catchup_logpos_t local_durable_logpos);

dbe_ret_t dbe_logfile_encrypt(
        dbe_cfg_t*    cfg,
        dbe_header_t* dbheader,
        su_cipher_t*  cipher,
        su_cipher_t*  old_cipher,
        dbe_encrypt_t encrypt,
        dbe_decrypt_t decript);

#endif /* DBE7LOGF_H */
