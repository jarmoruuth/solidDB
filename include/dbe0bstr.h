/*************************************************************************\
**  source       * dbe0bstr.h
**  directory    * dbe
**  description  * Blob stream interface of DBE
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


#ifndef DBE0BSTR_H
#define DBE0BSTR_H

#include <su0bstre.h>
#include <rs0sysi.h>
#include <rs0aval.h>
#include "dbe0type.h"
#include "dbe0erro.h"

typedef struct dbe_wblob_st dbe_wblob_t;
typedef su_bstream_t dbe_rblob_t;

dbe_ret_t dbe_blobaval_delete(
                rs_sysinfo_t* cd,
                dbe_db_t* db,
                rs_atype_t* atype,
                rs_aval_t* aval);

dbe_ret_t dbe_blobaval_read(
        rs_sysinfo_t* cd,
        dbe_db_t* db,
        rs_atype_t* atype,
        rs_aval_t* aval);

dbe_blobsize_t dbe_blobaval_size(
        rs_sysinfo_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

dbe_wblob_t* dbe_wblob_init(
                rs_sysinfo_t* cd,
                dbe_db_t* db,
                dbe_trx_t* trx,
                rs_atype_t* atype,
                rs_aval_t* aval,
                dbe_blobsize_t blobsize);

dbe_ret_t dbe_wblob_done(
        dbe_wblob_t* stream);

void dbe_wblob_abort(
        dbe_wblob_t* stream);

dbe_ret_t dbe_wblob_write(
        dbe_wblob_t* stream,
        char* buf,
        size_t bufsize,
        size_t* p_written);

dbe_ret_t dbe_wblob_reach(
        dbe_wblob_t* wb,
        char** pp_buf,
        size_t* p_avail);

void dbe_wblob_release(
        dbe_wblob_t* stream,
        size_t n_written);

dbe_rblob_t* dbe_rblob_init(
                dbe_db_t* db,
                va_t* blobref_va,
                dbe_blobsize_t* p_totalsize);

#define dbe_rblob_read(rb, buf, bufsize, p_nread) \
            su_bstream_read(rb, buf, bufsize, p_nread)

#define dbe_rblob_reach(rb, p_avail) \
            su_bstream_reachforread(rb, p_avail)

#define dbe_rblob_release(rb, n_read) \
            su_bstream_releaseread(rb, n_read)

#define dbe_rblob_abort(rb) \
            su_bstream_abort(rb)

#define dbe_rblob_done(rb) \
            su_bstream_done(rb)

#endif /* DBE0BSTR_H */
