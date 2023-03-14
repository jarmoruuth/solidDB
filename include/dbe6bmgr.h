/*************************************************************************\
**  source       * dbe6bmgr.h
**  directory    * dbe
**  description  * Blob storage manager
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


#ifndef DBE6BMGR_H
#define DBE6BMGR_H

#include <uti0vtpl.h>
#include <rs0sysi.h>
#include <rs0aval.h>

#include "dbe6blob.h"

typedef struct dbe_vablobref_st {
        dbe_blobid_t    bref_blobid;
        dbe_blobsize_t  bref_blobsize;
        uchar           bref_fileno;
        su_daddr_t      bref_daddr;
} dbe_vablobref_t;

#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0brb.h"
#include "dbe6iom.h"

#define dbe_bref_getblobid(bref)    ((bref)->bref_blobid)
#define dbe_bref_getblobsize(bref)  ((bref)->bref_blobsize)
#define dbe_bref_getfileno(bref)    ((bref)->bref_fileno)
#define dbe_bref_getdaddr(bref)     ((bref)->bref_daddr)

#define dbe_bref_setblobid(bref,bid)        do {(bref)->bref_blobid = (bid);} while (0)
#define dbe_bref_setblobsize(bref,bs)       do {(bref)->bref_blobsize = (bs);} while (0)
#define dbe_bref_setfileno(bref,fn)         do {(bref)->bref_fileno = (fn);} while (0)
#define dbe_bref_setdaddr(bref,daddr)       do {(bref)->bref_daddr = (daddr);} while (0)

void dbe_bref_loadfromdiskbuf(
        dbe_vablobref_t* bref,
        char* diskbuf);

void dbe_bref_storetodiskbuf(
        dbe_vablobref_t* bref,
        char* diskbuf);

void dbe_bref_loadfromva(
        dbe_vablobref_t* bref,
        va_t* p_va);

void dbe_bref_storetova(
        dbe_vablobref_t* bref,
        va_t* p_va);

dbe_vablobref_t* dbe_bref_makecopyof(
        dbe_vablobref_t* bref);

dbe_blobsize_t dbe_brefva_getblobsize(
        va_t* p_va);

dbe_blobmgr_t* dbe_blobmgr_init(
        dbe_iomgr_t* iomgr,
        dbe_file_t* file,
        dbe_counter_t* counter,
        dbe_blobsize_t bloblogthreshold);

void dbe_blobmgr_done(
        dbe_blobmgr_t* blobmgr);

dbe_ret_t dbe_blobmgr_insertaval(
        rs_sysinfo_t* cd,
        dbe_blobmgr_t* blobmgr,
        rs_atype_t* atype,
        rs_aval_t* aval,
        va_index_t maxvalen,
        dbe_trxid_t trxid);

dbe_ret_t dbe_blobmgr_getreadstreamofva(
        dbe_blobmgr_t* blobmgr,
        va_t* va,
        su_bstream_t** p_bstream);

dbe_ret_t dbe_blobmgr_getreadstreamofbref(
        dbe_blobmgr_t* blobmgr,
        dbe_vablobref_t* blobref,
        su_bstream_t** p_bstream);

dbe_ret_t dbe_blobmgr_delete(
        dbe_blobmgr_t* blobmgr,
        va_t* va);

dbe_ret_t dbe_blobmgr_deletevtpl_maybedeferred(
        dbe_blobmgr_t* blobmgr,
        vtpl_t* vtpl);

dbe_ret_t dbe_blobmgr_copy(
        dbe_blobmgr_t* blobmgr,
        rs_sysi_t* cd,
        va_t* va,
        dbe_trxid_t trxid);

dbe_ret_t dbe_blobmgr_readtoaval(
        rs_sysinfo_t* cd,
        dbe_blobmgr_t* blobmgr,
        va_t* va,
        rs_atype_t* atype,
        rs_aval_t* aval);

dbe_blobwritestream_t* dbe_blobwritestream_init(
        dbe_blobmgr_t* blobmgr,
        rs_sysi_t* cd,
        dbe_blobsize_t blobsize,
        va_index_t maxvalen,
        dbe_trxid_t trxid);

void dbe_blobwritestream_done(
        dbe_blobwritestream_t* stream);

void dbe_blobwritestream_abort(
        dbe_blobwritestream_t* stream);

dbe_ret_t dbe_blobwritestream_write(
        dbe_blobwritestream_t* stream,
        char* buf,
        size_t bufsize,
        size_t* p_written);

dbe_ret_t dbe_blobwritestream_reach(
        dbe_blobwritestream_t* stream,
        char** pp_buf,
        size_t* p_avail);

void dbe_blobwritestream_release(
        dbe_blobwritestream_t* stream,
        size_t n_written);

void dbe_blobwritestream_close(
        dbe_blobwritestream_t* stream);

va_t* dbe_blobwritestream_getva(
        dbe_blobwritestream_t* stream);

/* Size of the header part of the blob va, contains both the va lenlen and
 * blob reference size.
 */
#define DBE_BLOBVAHEADERSIZE    (VA_LENGTHMAXLEN + DBE_VABLOBREF_SIZE)

/* Calculates gross length of blob ref. va from the
 * net len of the comparing part
 */
#define DBE_BLOBVAGROSSLEN(netlen) \
        (VA_LENGTHMAXLEN + DBE_VABLOBREF_SIZE + (netlen))


#endif /* DBE6BMGR_H */
