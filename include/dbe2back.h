/*************************************************************************\
**  source       * dbe2back.h
**  directory    * dbe
**  description  * Database backup.
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


#ifndef DBE2BACK_H
#define DBE2BACK_H

#include <rs0types.h>

#include "dbe7cfg.h"
#include "dbe0erro.h"
#include "dbe0type.h"
#include "dbe0catchup.h"

typedef struct dbe_backup_st dbe_backup_t;

dbe_ret_t dbe_backup_check(
        dbe_cfg_t* cfg,
        char* backupdir,
        rs_err_t** p_errh);

dbe_backup_t* dbe_backup_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_file_t* file,
        dbe_counter_t* ctr,
        char* backupdir,
        bool replicap,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        dbe_ret_t* p_rc,
        rs_err_t** p_errh);

dbe_backup_t* dbe_backup_initwithcallback(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_file_t* file,
        dbe_counter_t* ctr,
        su_ret_t (*callbackfp)(   /* Callback function to write to */
                void* ctx,        /* user given stream. */
                dbe_backupfiletype_t ftype,
                ss_int4_t finfo,  /* log file number, unused for other files */
                su_daddr_t daddr, /* position in file */
                char* fname,      /* file name */
                void* data,       /* file data to write */
                size_t len),
        void* callbackctx,
        bool replicap,
        dbe_backuplogmode_t backuplogmode,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        dbe_ret_t* p_rc,
        rs_err_t** p_errh);

void dbe_backup_done(
        dbe_backup_t* backup);

dbe_ret_t dbe_backup_advance(
        dbe_backup_t* backup,
        rs_err_t** p_errh);

char* dbe_backup_getcurdir(
        dbe_backup_t* backup);

void dbe_backup_getlogfnumrange(
        dbe_backup_t* backup,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end);

void dbe_backup_getlogfnumrange_withoutbackupobject(
        dbe_counter_t* ctr,
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_cpnum_t cpnum,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end);

dbe_ret_t dbe_backup_deletelog_cp(
        dbe_counter_t* ctr,
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        rs_err_t** p_errh);

#endif /* DBE2BACK_H */
