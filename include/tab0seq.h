/*************************************************************************\
**  source       * tab0seq.h
**  directory    * tab
**  description  * Sequence support function
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


#ifndef TAB0SEQ_H
#define TAB0SEQ_H

#include <ssc.h>
#include <rs0types.h>
#include <rs0error.h>
#include <dbe0erro.h>
#include "tab0tran.h"
#include "tab0tran.h"

bool tb_seq_find(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* name,
        char* authid,
        char* catalog,
        char** p_authid,
        char** p_catalog,
        long* p_id,
        bool* p_isdense,
        rs_err_t** p_errh);

bool tb_seq_findbyid(
        rs_sysi_t* cd,
        long id,
        char** p_name,
        char** p_authid,
        char** p_catalog);

bool tb_seq_next(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh);

bool tb_seq_current(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh);

bool tb_seq_set(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        bool densep,
        rs_atype_t* atype,
        rs_aval_t* aval,
        bool* p_finishedp,
        rs_err_t** p_errh);

dbe_ret_t tb_seq_lock(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id,
        rs_err_t** p_errh);

void tb_seq_unlock(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        long seq_id);

sqlseq_t *tb_seq_sql_create(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char *name,
        char *schema,
        char *catalog,
        uint throughview,
        char *viewname,
        char *viewschema,
        char *viewcatalog,
        sqlview_t *viewhandle,
        sqlerr_t **err);

void tb_seq_sql_free(
        rs_sysi_t* cd,
        sqlseq_t *seq);

sqlftype_t *tb_seq_sql_oper(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        sqlseq_t *seq,
        char *operstr,
        sqlfinst_t **resval,
        bool *sideeffects,
        sqlerr_t **err);

bool tb_createseq(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo,
        bool dense,
        void** cont,
        sqlerr_t **err);

bool tb_createseqandid(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo,
        bool dense,
        void** cont,
        sqlerr_t **err,
        long* seq_id);

bool tb_dropseq(
        rs_sysi_t* cd,
        sqltrans_t *trans,
        char *seqname,
        char *schema,
        char *catalog,
        char* extrainfo,
        bool cascade,
        void** cont,
        sqlerr_t **err);

#endif /* TAB0SEQ_H */
