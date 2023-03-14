/*************************************************************************\
**  source       * dbe4rfwd.h
**  directory    * dbe
**  description  * Roll-forward recovery
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


#ifndef DBE4RFWD_H
#define DBE4RFWD_H

#include <rs0sysi.h>
#include <rs0relh.h>
#include <su0error.h>
#include <su0list.h>
#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe7cfg.h"
#include "dbe7ctr.h"
#include "dbe7trxb.h"
#include "dbe7rtrx.h"
#include "dbe0trx.h"
#include "dbe0db.h"

typedef struct dbe_rollfwd_st dbe_rollfwd_t;

dbe_rollfwd_t* dbe_rollfwd_init(
        dbe_cfg_t* cfg,
        dbe_counter_t* counter,
        dbe_user_t* user,
        dbe_trxbuf_t* trxbuf,
        dbe_gobj_t* gobjs,
        dbe_db_recovcallback_t* recovcallback,
        SsTimeT dbcreatime,
        SsTimeT cptimestamp,
        dbe_hsbmode_t hsbmode,
        dbe_trxid_t reptrxidmax,
        dbe_rtrxbuf_t* rtrxbuf
#ifdef SS_HSBG2
        , dbe_hsbg2_t* hsbsvc
#endif /* SS_HSBG2 */
        );

void dbe_rollfwd_done(
        dbe_rollfwd_t* rf);

void dbe_rollfwd_deferred_rfinfo_clean(
        su_list_t* deferred_rfinfolist);

dbe_ret_t dbe_rollfwd_scancommitmarks(
        dbe_rollfwd_t* rf,
        ulong* p_ncommits,
        bool* p_rtrxfound,
        dbe_hsbstatelabel_t* p_starthsbstate);

dbe_ret_t dbe_rollfwd_recover(
        dbe_rollfwd_t* rf);

dbe_hsbmode_t dbe_rollfwd_gethsbmode(
        dbe_rollfwd_t* rf);

dbe_trxid_t dbe_rollfwd_getreptrxidmax(
        dbe_rollfwd_t* rf);

bool dbe_rollfwd_getreptrxidmaxupdated(
        dbe_rollfwd_t* rf);

#endif /* DBE4RFWD_H */
