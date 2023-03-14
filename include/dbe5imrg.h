/*************************************************************************\
**  source       * dbe5imrg.h
**  directory    * dbe
**  description  * Index merge routines.
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


#ifndef DBE5IMRG_H
#define DBE5IMRG_H

#include <su0list.h>
#include <rs0sysi.h>
#include "dbe9type.h"
#include "dbe5inde.h"
#include "dbe0type.h"
#include "dbe0erro.h"

typedef struct dbe_indmerge_st dbe_indmerge_t;

dbe_indmerge_t* dbe_indmerge_init_ex(
        void* cd,
        dbe_db_t* db,
        dbe_index_t* index,
        dbe_trxnum_t mergetrxnum,
        dbe_trxnum_t patchtrxnum,
        ulong maxpoolblocks,
        bool quickmerge);

dbe_indmerge_t* dbe_indmerge_init(
        void* cd,
        dbe_db_t* db,
        dbe_index_t* index,
        dbe_trxnum_t mergetrxnum,
        dbe_trxnum_t patchtrxnum,
        ulong maxpoolblocks);

void dbe_indmerge_getnmerged(
        dbe_indmerge_t* merge,
        long* p_nindexwrites);

void dbe_indmerge_done_ex(
        dbe_indmerge_t* merge,
        long* p_nindexwrites);

void dbe_indmerge_done(
        dbe_indmerge_t* merge);

void dbe_indmerge_setnmergetasks(
        dbe_indmerge_t* merge,
        int nmergetasks);

dbe_trxnum_t dbe_indmerge_getmergelevel(
        dbe_indmerge_t* merge);

dbe_mergeadvance_t dbe_indmerge_advance(
        dbe_indmerge_t* merge,
        rs_sysi_t* cd,
        uint nstep,
        bool mergestep,
        su_list_t** p_deferred_blob_unlink_list);

void dbe_indmerge_unlinkblobs(
        rs_sysi_t* cd,
        su_list_t* deferred_blob_unlink_list);

#endif /* DBE5IMRG_H */
