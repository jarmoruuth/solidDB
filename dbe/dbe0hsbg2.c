/*************************************************************************\
**  source       * dbe0hsbg2.c
**  directory    * dbe
**  description  * HSB interface.
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


#include <ssc.h>
#include <ssdebug.h>

#include "dbe0hsbg2.h"

static dbe_hsbg2_funblock_t* hsb_fb = NULL;

dbe_hsbg2_t* dbe_hsbg2_init(void* ctx, dbe_db_t* db)
{
        dbe_hsbg2_t *hsb = NULL;

        if(hsb_fb != NULL && hsb_fb->hsb_init != NULL) {
            hsb = (*hsb_fb->hsb_init)(ctx, db);
        }

        return (hsb);
}

void dbe_hsbg2_done(dbe_hsbg2_t *hsb)
{
        if(hsb_fb != NULL && hsb_fb->hsb_done != NULL) {
            (*hsb_fb->hsb_done)(hsb);
        }
}

void dbe_hsbg2_getspace(
        dbe_hsbg2_t* hsb, 
        int size)
{
        if(hsb_fb != NULL && hsb_fb->hsb_getspace != NULL) {
            (*hsb_fb->hsb_getspace)(hsb, size);
        }
}


dbe_catchup_logpos_t dbe_hsbg2_getfirstusedlogpos(dbe_hsbg2_t* hsb)
{
        dbe_catchup_logpos_t lp;

        DBE_CATCHUP_LOGPOS_SET_NULL(lp);

        if(hsb_fb != NULL && hsb_fb->hsb_getfirstusedlogpos != NULL) {
            lp = (*hsb_fb->hsb_getfirstusedlogpos)(hsb);
        }

        return (lp);
}

dbe_catchup_logpos_t dbe_hsbg2_getcplpid(dbe_hsbg2_t *hsb)
{
        dbe_catchup_logpos_t pos;

        DBE_CATCHUP_LOGPOS_SET_NULL(pos);

        if(hsb_fb != NULL && hsb_fb->hsb_getcplpid != NULL) {
            pos = (*hsb_fb->hsb_getcplpid)(hsb);
        }

        return (pos);
}


void dbe_hsbg2_logdata_take(
        dbe_hsbg2_t* hsb,
        dbe_logdata_t* ld,
        rs_sysi_t *cd,
        dbe_log_instancetype_t loginstancetype,
        void* instance_ctx)
{
        if(hsb_fb != NULL && hsb_fb->hsb_logdata_take != NULL) {
            (*hsb_fb->hsb_logdata_take)(hsb, ld, cd, loginstancetype, instance_ctx);
        } else {
            dbe_logdata_done(ld);
        }
}


bool dbe_hsbg2_need_durable_logrec(dbe_hsbg2_t* hsb, int datalen)
{
        bool b = FALSE;

        if(hsb_fb != NULL && hsb_fb->hsb_need_durable_logrec != NULL) {
            b = (*hsb_fb->hsb_need_durable_logrec)(hsb, datalen);
        }

        return (b);
}

void dbe_hsbg2_log_written_up_to(
        dbe_hsbg2_t*            hsb,
        dbe_catchup_logpos_t    logpos,
        bool                    flushed)
{
        if(hsb_fb != NULL && hsb_fb->hsb_log_written_up_to != NULL) {
            (*hsb_fb->hsb_log_written_up_to)(hsb, logpos, flushed);
        }
}

dbe_ret_t dbe_hsbg2_sec_opscan_recovery(
        void* recovctx,
        dbe_rflog_t* rflog,
        long durablecount,
        dbe_cpnum_t lastcpnum,
        int flags,
        dbe_catchup_logpos_t local_durable_logpos,
        dbe_catchup_logpos_t remote_durable_logpos,
        su_list_t* savedlogposlist)
{
        if(hsb_fb != NULL && hsb_fb->hsb_sec_opscan_recovery != NULL) {
            return (*hsb_fb->hsb_sec_opscan_recovery)(
                        recovctx, 
                        rflog, 
                        durablecount, 
                        lastcpnum, 
                        flags, 
                        local_durable_logpos, 
                        remote_durable_logpos,
                        savedlogposlist);
        } else {
            ss_derror;
            return(DBE_RC_SUCC);
        }
}

dbe_ret_t dbe_hsbg2_accept_logdata(dbe_hsbg2_t* hsb)
{
        if(hsb_fb != NULL && hsb_fb->hsb_svc_accept_logdata != NULL) {
            return (*hsb_fb->hsb_svc_accept_logdata)(hsb);
        }
        return(DBE_RC_SUCC);
}

bool dbe_hsbg2_logging_enabled(dbe_hsbg2_t* hsb)
{
        if(hsb_fb != NULL && hsb_fb->hsb_svc_logging_enabled != NULL) {
            return (*hsb_fb->hsb_svc_logging_enabled)(hsb);
        }

        return (FALSE);
}

void dbe_hsbg2_createcp_start(dbe_hsbg2_t* hsb, dbe_db_t* db, dbe_hsbcreatecp_t hsbcreatecp)
{
        if(hsb_fb != NULL && hsb_fb->hsb_svc_createcp_start != NULL) {
            (*hsb_fb->hsb_svc_createcp_start)(hsb, db, hsbcreatecp);
        }
}

void dbe_hsbg2_reset_logpos(dbe_hsbg2_t* hsb)
{
        if(hsb_fb != NULL && hsb_fb->hsb_svc_reset_logpos != NULL) {
            (*hsb_fb->hsb_svc_reset_logpos)(hsb);
        }
}

void dbe_hsbg2_set_funblock(dbe_hsbg2_funblock_t* fb)
{
        /* if (fb != NULL) { */
            hsb_fb = fb;
        /* } */
}
