/*************************************************************************\
**  source       * dbe0hsbg2.h
**  directory    * dbe
**  description  * HSB interface for DBE.
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


#ifndef DBE0HSBG2_H
#define DBE0HSBG2_H

#include "dbe0ld.h"
#include "dbe0catchup.h"
#include "dbe0type.h"

typedef void dbe_hsbg2_t;

dbe_hsbg2_t* dbe_hsbg2_init(void *, dbe_db_t*);

void dbe_hsbg2_done(dbe_hsbg2_t*);

void dbe_hsbg2_getspace(dbe_hsbg2_t*, int);

dbe_catchup_logpos_t dbe_hsbg2_getcatchuplogpos(dbe_hsbg2_t*);

dbe_catchup_logpos_t dbe_hsbg2_getfirstusedlogpos(dbe_hsbg2_t*);

dbe_catchup_logpos_t dbe_hsbg2_getlastlogpos(dbe_hsbg2_t*);

dbe_catchup_logpos_t dbe_hsbg2_getcplpid(dbe_hsbg2_t*);

void dbe_hsbg2_setprimary(dbe_hsbg2_t*, long);

void dbe_hsbg2_logdata_take(
        dbe_hsbg2_t*, 
        dbe_logdata_t*, 
        rs_sysi_t *, 
        dbe_log_instancetype_t loginstancetype,
        void* instance_ctx);

bool dbe_hsbg2_need_durable_logrec(dbe_hsbg2_t*, int);

void dbe_hsbg2_log_written_up_to(
        dbe_hsbg2_t*            hsb,
        dbe_catchup_logpos_t    logpos,
        bool                    flushed);

dbe_ret_t dbe_hsbg2_accept_logdata(dbe_hsbg2_t*);

bool dbe_hsbg2_logging_enabled(dbe_hsbg2_t*);

void dbe_hsbg2_reset_logpos(dbe_hsbg2_t*);

void dbe_hsbg2_createcp_start(dbe_hsbg2_t*, dbe_db_t* db, dbe_hsbcreatecp_t);

dbe_ret_t dbe_hsbg2_sec_opscan_recovery(
        void* recovctx,
        dbe_rflog_t* rflog,
        long durablecount,
        dbe_cpnum_t lastcpnum,
        int flags,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos,
        su_list_t* savedlogposlist);

/* HSB G2 interface */

typedef struct {
    dbe_hsbg2_t* (*hsb_init)(void *, dbe_db_t*);
    void (*hsb_done)(dbe_hsbg2_t*);
    dbe_catchup_logpos_t (*hsb_getcatchuplogpos)(dbe_hsbg2_t*);
    dbe_catchup_logpos_t (*hsb_getfirstusedlogpos)(dbe_hsbg2_t*);
    dbe_catchup_logpos_t (*hsb_getlastlogpos)(dbe_hsbg2_t*);
    dbe_catchup_logpos_t (*hsb_getcplpid)(dbe_hsbg2_t*);
    void (*hsb_setprimary)(dbe_hsbg2_t*, long);
    void (*hsb_logdata_take)(dbe_hsbg2_t*, dbe_logdata_t*, rs_sysi_t *, dbe_log_instancetype_t it, void* instance_ctx);
    bool (*hsb_mustflush_and_startwaitif)(dbe_hsbg2_t*, rs_sysi_t*);
    bool (*hsb_need_durable_logrec)(dbe_hsbg2_t*, int);
    void (*hsb_log_written_up_to)(dbe_hsbg2_t*, dbe_catchup_logpos_t, bool);
    dbe_ret_t (*hsb_sec_opscan_recovery)(
        void* recovctx,
        dbe_rflog_t* rflog,
        long durablecount,
        dbe_cpnum_t lastcpnum,
        bool uselocallogpos,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos,
        su_list_t* savedlogposlist);
    dbe_ret_t (*hsb_svc_accept_logdata)(dbe_hsbg2_t*);
    bool (*hsb_svc_logging_enabled)(dbe_hsbg2_t*);
    void (*hsb_svc_createcp_start)(dbe_hsbg2_t*, dbe_db_t*, dbe_hsbcreatecp_t);
    void (*hsb_svc_reset_logpos)(dbe_hsbg2_t*);

    void (*hsb_getspace)(dbe_hsbg2_t*, int size);

} dbe_hsbg2_funblock_t;

void dbe_hsbg2_set_funblock(dbe_hsbg2_funblock_t* fb);

#endif /* DBE0HSBG2_H */
