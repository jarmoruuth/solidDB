/*************************************************************************\
**  source       * dbe1seq.h
**  directory    * dbe
**  description  * Sequence routines internal to dbe.
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


#ifndef DBE1SEQ_H
#define DBE1SEQ_H

#include "dbe0seq.h"

typedef struct dbe_seqvalue_st dbe_seqvalue_t;

long dbe_seqvalue_getid(
        dbe_seqvalue_t* sv);

dbe_seq_t* dbe_seq_init(
        void);

void dbe_seq_done(
        dbe_seq_t* seq);

dbe_ret_t dbe_seq_create(
        dbe_seq_t* seq,
        long seq_id,
        rs_err_t** p_errh);

dbe_ret_t dbe_seq_drop(
        dbe_seq_t* seq,
        long seq_id,
        rs_err_t** p_errh);

dbe_seqvalue_t* dbe_seq_find(
        dbe_seq_t* seq,
        long seq_id);

dbe_ret_t dbe_seq_markdropped(
        dbe_seq_t* seq,
        long seq_id);

dbe_ret_t dbe_seq_unmarkdropped(
        dbe_seq_t* seq,
        long seq_id);

dbe_ret_t dbe_seq_commit(
        dbe_seq_t* seq,
        dbe_trx_t* trx,
        dbe_seqvalue_t* sv);

void dbe_seq_transend(
        dbe_seq_t* seq,
        dbe_seqvalue_t* sv,
        bool commitp);

dbe_ret_t dbe_seq_setvalue(
        dbe_seq_t* seq,
        long seq_id,
        rs_tuplenum_t* value);

dbe_ret_t dbe_seq_inc(
        dbe_seq_t* seq,
        long seq_id);

void dbe_seq_entermutex(
        dbe_seq_t* seq);

void dbe_seq_exitmutex(
        dbe_seq_t* seq);

dbe_ret_t dbe_seq_save_nomutex(
        dbe_seq_t* seq,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_seqlistdaddr);

dbe_ret_t dbe_seq_restore(
        dbe_seq_t* seq,
        dbe_cache_t* cache,
        su_daddr_t seqlistdaddr);

#endif /* DBE1SEQ_H */
