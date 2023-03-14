/*************************************************************************\
**  source       * dbe6blob.h
**  directory    * dbe
**  description  * File stream BLOB support
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


#ifndef DBE6BLOB_H
#define DBE6BLOB_H

typedef struct dbe_blob_st dbe_readblob_t;
typedef struct dbe_blob_st dbe_writeblob_t;
typedef struct dbe_blob_st dbe_copyblob_t;

#include "dbe6log.h"
#include "dbe9type.h"
#include "dbe7ctr.h"
#include "dbe6finf.h"
#include "dbe6iom.h"
#include "dbe0type.h"

extern long dbe_blob_nblock;

dbe_readblob_t* dbe_readblob_init(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        su_daddr_t daddr,
        int prefetchsize);

void dbe_readblob_done(
        dbe_readblob_t* blob);

char* dbe_readblob_reach(
        dbe_readblob_t* blob,
        size_t* p_nbytes);

void dbe_readblob_release(
        dbe_readblob_t* blob,
        size_t nbytes);

size_t dbe_readblob_read(
        dbe_readblob_t* blob,
        char* buf,
        size_t bufsize);

dbe_blobid_t dbe_readblob_getid(
        dbe_readblob_t* blob);

dbe_blobsize_t dbe_readblob_getsize(
        dbe_readblob_t* blob);

dbe_writeblob_t* dbe_writeblob_init(
        rs_sysi_t* cd,
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        dbe_blobsize_t size,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid,
        dbe_blobid_t* p_blobid);

void dbe_writeblob_done(
        dbe_writeblob_t* blob);

void dbe_writeblob_close(
        dbe_writeblob_t* blob);

void dbe_writeblob_abort(
        dbe_writeblob_t* blob);

dbe_ret_t dbe_writeblob_reach(
        dbe_writeblob_t* blob,
        char** pp_buf,
        size_t* p_nbytes);

void dbe_writeblob_release(
        dbe_writeblob_t* blob,
        size_t nbytes);

dbe_ret_t dbe_writeblob_write(
        dbe_writeblob_t* blob,
        char* buf,
        size_t bufsize);

dbe_blobsize_t dbe_writeblob_getsize(
        dbe_writeblob_t* blob);

su_daddr_t dbe_writeblob_getstartaddr(
        dbe_writeblob_t* blob);

dbe_blobid_t dbe_writeblob_getid(
        dbe_writeblob_t* blob);

void dbe_blob_delete(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        su_daddr_t daddr);

dbe_copyblob_t* dbe_copyblob_init(
        dbe_iomgr_t* iomgr,
        dbe_filedes_t* filedes,
        dbe_counter_t* counter,
        su_daddr_t daddr,
        dbe_log_t* log,
        bool logdataflag,
        dbe_trxid_t trxid,
        dbe_blobid_t* p_blobid,
        dbe_blobsize_t* p_blobsize,
        int prefetchsize);

void dbe_copyblob_done(
        dbe_copyblob_t* blob);

dbe_ret_t dbe_copyblob_copy(
        dbe_copyblob_t* blob,
        su_daddr_t* p_daddr);

#ifdef SS_DEBUG

void dbe_blob_printalloclist(
        void);

#endif
#ifdef SS_FAKE
/* debugging of alive blob ids */
void dbe_blob_debugpool_add(dbe_blobid_t blobid);
void dbe_blob_debugpool_remove(dbe_blobid_t blobid);
void dbe_blob_debugpool_print(void);

#endif /* SS_FAKE */

#define FAKE_BLOBID_ADD(blobid) \
FAKE_CODE_BLOCK(FAKE_DBE_BLOBDEBUGPOOL, { dbe_blob_debugpool_add(blobid); })

#define FAKE_BLOBID_REMOVE(blobid) \
FAKE_CODE_BLOCK(FAKE_DBE_BLOBDEBUGPOOL, { dbe_blob_debugpool_remove(blobid); })

#define FAKE_BLOBID_PRINTPOOL() \
FAKE_CODE_BLOCK(FAKE_DBE_BLOBDEBUGPOOL, { dbe_blob_debugpool_print(); })

#endif /* DBE6BLOB_H */
