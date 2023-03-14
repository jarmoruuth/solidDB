/*************************************************************************\
**  source       * dbe7rtrx.h
**  directory    * dbe
**  description  * Replication Transaction translation
**               * routines
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


#ifndef DBE7RTRX_H
#define DBE7RTRX_H

#include "dbe8cach.h"
#include "dbe8flst.h"
#include "dbe7trxb.h"

typedef enum {
        DBE_RTRX_SEARCHBYNONE,
        DBE_RTRX_SEARCHBYLOCAL,
        DBE_RTRX_SEARCHBYREMOTE
} dbe_rtrxsearchby_t;


dbe_rtrxbuf_t* dbe_rtrxbuf_init(
        void);

void dbe_rtrxbuf_done(
        dbe_rtrxbuf_t* rtrxbuf);

void dbe_rtrxbuf_setsearchby(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_rtrxsearchby_t searchby);

dbe_ret_t dbe_rtrxbuf_add(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid,
        dbe_trxid_t localtrxid,
        void* trxdata,
        bool isdummy);

dbe_trxid_t dbe_rtrxbuf_localbyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid);

void* dbe_rtrxbuf_localtrxbyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid);

bool dbe_rtrxbuf_isdummybyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid);

void* dbe_rtrxbuf_localtrxbylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid);

dbe_trxid_t dbe_rtrxbuf_remotebylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid);

dbe_ret_t dbe_rtrxbuf_deletebylocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t localtrxid);

dbe_ret_t dbe_rtrxbuf_deletebyremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxid_t remotetrxid);

dbe_ret_t dbe_rtrxbuf_save(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_cache_t* cache,
        dbe_freelist_t* freelist,
        dbe_cpnum_t cpnum,
        su_daddr_t* p_rtrxlistdaddr);

dbe_ret_t dbe_rtrxbuf_restore(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_cache_t* cache,
        su_daddr_t rtrxlistdaddr);

bool dbe_rtrxbuf_iterate(
        dbe_rtrxbuf_t* rtrxbuf,
        void** p_iter);

void* dbe_rtrxbuf_getitertrxdata(
        dbe_rtrxbuf_t* rtrxbuf,
        void* iter);

dbe_trxid_t dbe_rtrxbuf_getiterlocaltrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        void* iter);

dbe_trxid_t dbe_rtrxbuf_getiterremotetrxid(
        dbe_rtrxbuf_t* rtrxbuf,
        void* iter);

bool dbe_rtrxbuf_getiterisdummymapping(
        dbe_rtrxbuf_t* rtrxbuf,
        void* iter);

void dbe_rtrxbuf_setitertrxdata(
        dbe_rtrxbuf_t* rtrxbuf,
        void* iter,
        void* trxdata);

void dbe_rtrxbuf_deleteall(
        dbe_rtrxbuf_t* rtrxbuf);

void dbe_rtrxbuf_removeaborted(
        dbe_rtrxbuf_t* rtrxbuf,
        dbe_trxbuf_t* trxbuf);

#endif /* DBE7RTRX_H */
