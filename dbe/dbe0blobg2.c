/*************************************************************************\
**  source       * dbe0blobg2.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------

BLOB (Binary Large Object) are represented using a chain of disk blocks.
The most natural solution would be a simple linked list of disk blocks.
But it is inefficient in presence of multithreaded database engine that has
a separate prefetch thread. That is why the BLOBs consist of directory
information kept at a higher level and data blocks that are not directly
linked.

Limitations:
-----------

Now the BLOB size field is 64bits and the earlier 4 GB limit is thus removed

Error handling:
--------------

All errors are reported just as an error code
(currently no more context is available)

Objects used:
------------

File descriptor dbe_filedes_t from dbe6finf.h
Counter dbe_counter_t from dbe7ctr.h
Log from dbe6log.h
cache manager through i/o manager (dbe6iom.h)

Preconditions:
-------------

The referred objects must be initialized (except log which may be NULL).

Multithread considerations:
--------------------------

The data structures are re-entrant, but shared access to same object
is not protected.

Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <ssenv.h>
#include <ssstddef.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <su0cfgst.h>
#include "dbe9type.h"
#include "dbe9bhdr.h"
#include "dbe7binf.h"
#include "dbe0type.h"
#include "dbe6gobj.h"
#include "dbe6bmgr.h"
#include "dbe0blobg2.h"

#define CHK_WB(wb)\
        ss_assert((wb) != NULL); \
        ss_rc_assert((wb)->wb_check == DBE_CHK_WBLOBG2, (wb)->wb_check)

#ifdef SS_DEBUG
static char wblobg2_reachctx[] = "wBLOB G2 reach";
#else
#define wblobg2_reachctx NULL
#endif

/* callback functions that are needed because dbe level routines need
 * to handle BLOBs whose interface has moved to tab
 */

su_ret_t (*dbe_blobg2callback_move_page)(
        rs_sysi_t* cd,
        su_daddr_t old_daddr,
        su_daddr_t new_daddr,
        ss_byte_t* page_data,
        void* page_slot);

su_ret_t (*dbe_blobg2callback_incrementpersistentrefcount_byva)(
        rs_sysi_t* cd,
        va_t* p_va,
        su_err_t** p_errh);
        
su_ret_t (*dbe_blobg2callback_decrementpersistentrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

tb_wblobg2stream_t* (*dbe_blobg2callback_wblobinit)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

tb_wblobg2stream_t* (*dbe_blobg2callback_wblobinit_for_recovery)(
        rs_sysi_t* cd,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startoffset);

su_ret_t (*dbe_blobg2callback_wblobreach)(
        tb_wblobg2stream_t* wbs,
        ss_byte_t** pp_buf,
        size_t* p_avail,
        su_err_t** p_errh);

su_ret_t (*dbe_blobg2callback_wblobrelease)(
        tb_wblobg2stream_t* wbs,
        size_t nbytes,
        su_err_t** p_errh);

su_ret_t (*dbe_blobg2callback_wblobdone)(
        tb_wblobg2stream_t* wbs,
        su_err_t** p_errh);

void (*dbe_blobg2callback_wblobabort)(
        tb_wblobg2stream_t* wbs);

su_ret_t (*dbe_blobg2callback_delete_unreferenced_blobs_after_recovery)(
        rs_sysi_t* cd,
        size_t* p_nblobs_deleted,
        su_err_t** p_errh);

su_ret_t (*dbe_blobg2callback_copy_old_blob_to_g2)(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        su_err_t** p_errh);

su_ret_t (*dbe_blobg2callback_incrementinmemoryrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

su_ret_t (*dbe_blobg2callback_decrementinmemoryrefcount)(
        rs_sysi_t* cd,
        dbe_blobg2id_t bid,
        su_err_t** p_errh);

typedef enum {
    BSTAT_RELEASED,
    BSTAT_REACHED
} blobg2_status_t;

/* structure for representing a BLOB write stream at DBE level */
struct dbe_wblobg2_st {
        int               wb_check;
        blobg2_status_t   wb_status;
        dbe_cpnum_t       wb_startcpnum; /* checkpoint number when blob write started */
        dbe_iomgr_t*      wb_iom;        /* I/O manager */
        dbe_cache_t*      wb_cache;      /* cache page manager */
        dbe_log_t*        wb_log;        /* transaction log object */
        dbe_blobg2id_t    wb_id;         /* unique blob id */
        dbe_blobg2size_t  wb_startofs;   /* byte offset for page starting from beginning of BLOB */
        dbe_cacheslot_t*  wb_cacheslot;  /* reached cache slot or NULL when n/a*/
        ss_byte_t*        wb_pagedata;   /* NULL when n/a */
        size_t            wb_pagesize;   /* page gross size */
        size_t            wb_writepos;   /* write pos within data portion of page */
        su_ret_t        (*wb_getpageaddrfun)(void* getpageaddr_ctx,
                                             su_daddr_t* p_daddr,
                                             su_err_t** p_errh);
        void*             wb_getpageaddr_ctx;
        su_ret_t          (*wb_releasepageaddrfun)(void* released_pageaddr_ctx,
                                                   su_daddr_t daddr,
                                                   size_t bytes_in_use,
                                                   su_err_t** p_errh);
        void*             wb_releasepageaddr_ctx;
};

#define BLOBG2_PAGETYPE_OFS     0      /* 1 byte page tag */
#define BLOBG2_CPNUM_OFS        1      /* 4 bytes CP num */
#define BLOBG2_NBYTESINUSE_OFS  5      /* 4 bytes allocated bytes count */
#define BLOBG2_BLOBID_OFS       9      /* 8 bytes BLOB id */
#define BLOBG2_STARTOFS_OFS     17     /* 8 bytes page start offset in BLOB */
#define BLOBG2_DATASTART_OFS    25     /* rest of page for data */

#define BLOBG2_NETPAGESIZE(pagesize) \
        ((pagesize) - BLOBG2_DATASTART_OFS)

#define WBLOBG2_NETPAGESIZE(wb) \
        BLOBG2_NETPAGESIZE((wb)->wb_pagesize)

#define WBLOBG2_LOGSIZE(wb) \
        (wb->wb_writepos + (BLOBG2_DATASTART_OFS - BLOBG2_BLOBID_OFS))

#define WBLOBG2_LOGPTR(wb) \
        (wb->wb_pagedata + BLOBG2_BLOBID_OFS)

#define BREFG2_FLAG_ISBLOBG2 (1U << (sizeof(ss_byte_t) * SS_CHAR_BIT - 1U))


/* Note these offsets are relative to (buffer + bufsize)! */
#define BLOBREFVA_OFS_SIZE_HI (-(ss_int4_t)sizeof(ss_uint4_t))
#define BLOBREFVA_OFS_FLAGS   (BLOBREFVA_OFS_SIZE_HI - (ss_int4_t)sizeof(ss_byte_t))
#define BLOBREFVA_OFS_SIZE_LO (BLOBREFVA_OFS_FLAGS - (ss_int4_t)sizeof(ss_uint4_t))
#define BLOBREFVA_OFS_ID_LO   (BLOBREFVA_OFS_SIZE_LO - (ss_int4_t)sizeof(ss_uint4_t))
#define BLOBREFVA_OFS_ID_HI   (BLOBREFVA_OFS_ID_LO - (ss_int4_t)sizeof(ss_uint4_t))


/*##**********************************************************************\
 *
 *      dbe_brefg2_getblobg2id
 *
 * Gets BLOB ID from G2 blob reference record.
 *
 * Parameters:
 *      bref - in, use
 *          BLOB reference record
 *
 * Return value:
 *      BLOB ID
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_blobg2id_t dbe_brefg2_getblobg2id(dbe_vablobg2ref_t* bref)
{
        return (bref->brefg2_id);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_getblobg2size
 *
 * Gets BLOB size from G2 blob reference
 *
 * Parameters:
 *      bref - in, use
 *          BLOB reference record
 *
 * Return value:
 *      BLOB size in bytes
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_blobg2size_t dbe_brefg2_getblobg2size(dbe_vablobg2ref_t* bref)
{
        return (bref->brefg2_size);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_initbuf
 *
 * Initializes context of a BLOB G2 reference
 *
 * Parameters:
 *      brefbuf - out, use
 *          BLOB G2 reference buffer
 *
 *      bid - in
 *          BLOB ID
 *
 *      bs - in
 *          BLOB size
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_initbuf(
        dbe_vablobg2ref_t* brefbuf,
        dbe_blobg2id_t bid,
        dbe_blobg2size_t bs)
{
        brefbuf->brefg2_id = bid;
        brefbuf->brefg2_size = bs;
        brefbuf->brefg2_flags = BREFG2_FLAG_ISBLOBG2;
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_isblobg2check_from_diskbuf
 *
 * Checks whether a (on-disk format) BLOB reference is to G2 BLOB
 *
 * Parameters:
 *      data - in, use
 *          pointer to data which contains BLOB ref. record
 *          at its end
 *
 *      datasize - in
 *          size of data area in byte
 *
 * Return value:
 *      TRUE - data contains a reference to G2 BLOB
 *      FALSE - BLOB reference must be to OLD format BLOB
 *
 * Limitations:
 *
 * Globals used:
 */
bool dbe_brefg2_isblobg2check_from_diskbuf(ss_byte_t* data, size_t datasize)
{
        return ((data[(ss_int4_t)datasize + BLOBREFVA_OFS_FLAGS] & BREFG2_FLAG_ISBLOBG2) != 0);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_isblobg2check_from_va
 *
 * Same as dbe_brefg2_isblobg2check_from_diskbuf but this takes
 * the data as a v-attribute
 *
 * Parameters:
 *      va - in, use
 *          v-attribute containing a BLOB reference
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
bool dbe_brefg2_isblobg2check_from_va(va_t* va)
{
        va_index_t dsize;
        ss_byte_t* data = va_getdata(va, &dsize);
        bool blobg2p = dbe_brefg2_isblobg2check_from_diskbuf(data, dsize);
        return (blobg2p);
}

bool dbe_brefg2_isblobg2check_from_aval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        va_t* va;
        
        if (!rs_aval_isblob(cd, atype, aval)) {
            return (FALSE);
        }
        va = rs_aval_va(cd, atype, aval);
        return (dbe_brefg2_isblobg2check_from_va(va));
}
        
/*##**********************************************************************\
 *
 *      dbe_brefg2_loadfromdiskbuf
 *
 * Loads a BLOB G2 reference structure from disk-format buffer
 *
 * Parameters:
 *      bref - out, use
 *          pointer to BLOB G2 Reference structure
 *
 *      data - in, use
 *          data buffer containing blob reference at its end
 *
 *      datasize - in
 *          data buffer size in bytes
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_loadfromdiskbuf(
        dbe_vablobg2ref_t* bref,
        ss_byte_t* data,
        size_t datasize)
{
        ss_byte_t* data_plus_datasize = data + datasize;
        ss_byte_t* p_lo;
        ss_byte_t* p_hi;
        ss_dassert(dbe_brefg2_isblobg2check_from_diskbuf(data, datasize));
        p_hi = data_plus_datasize + BLOBREFVA_OFS_ID_HI;
        p_lo = data_plus_datasize + BLOBREFVA_OFS_ID_LO;
        DBE_BLOBG2ID_SET2UINT4S(&bref->brefg2_id,
                                SS_UINT4_LOADFROMDISK(p_hi),
                                SS_UINT4_LOADFROMDISK(p_lo));
        p_hi = data_plus_datasize + BLOBREFVA_OFS_SIZE_HI;
        p_lo = data_plus_datasize + BLOBREFVA_OFS_SIZE_LO;
        DBE_BLOBG2SIZE_SET2UINT4S(&bref->brefg2_size,
                                  SS_UINT4_LOADFROMDISK(p_hi),
                                  SS_UINT4_LOADFROMDISK(p_lo));
        bref->brefg2_flags = data_plus_datasize[BLOBREFVA_OFS_FLAGS];
        ss_dassert(bref->brefg2_flags == BREFG2_FLAG_ISBLOBG2);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_loadfromva
 *
 * Same as dbe_brefg2_loadfromdiskbuf but this takes the buffer as
 * a v-attribute
 *
 * Parameters:
 *      bref - out, use
 *          pointer to BLOB G2 Reference structure
 *
 *      blobrefva - in, use
 *          v-attribute
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_loadfromva(dbe_vablobg2ref_t* bref, va_t* blobrefva)
{
        ss_byte_t* vadata;
        va_index_t vadatalen;
        
        vadata = va_getdata(blobrefva, &vadatalen);
        dbe_brefg2_loadfromdiskbuf(bref, vadata, vadatalen);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_loadfromaval
 *
 * same as dbe_brefg2_loadfromva but this one takes the input as aval
 *
 * Parameters:
 *      bref - out, use
 *          pointer to BLOB G2 Reference structure
 *
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type for aval
 *
 *      aval - in, use
 *          aval containig BLOB G2 reference
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_loadfromaval(dbe_vablobg2ref_t* bref,
                             rs_sysi_t* cd,
                             rs_atype_t* atype,
                             rs_aval_t* aval)
{
        va_t* blobrefva;
        ss_dassert(rs_aval_isblob(cd, atype, aval));
        blobrefva = rs_aval_va(cd, atype, aval);
        dbe_brefg2_loadfromva(bref, blobrefva);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_getsizefromaval
 *
 * Gets BLOB size from aval
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type for aval
 *
 *      aval - in, use
 *          aval containing BLOB (G2 or old format) reference
 *
 * Return value:
 *      Size of the BLOB in bytes
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_blobg2size_t dbe_brefg2_getsizefromaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        va_t * va;
        
        ss_dassert(rs_aval_isblob(cd, atype, aval));

        va = rs_aval_va(cd, atype, aval);
        if (dbe_brefg2_isblobg2check_from_va(va)) {
            dbe_vablobg2ref_t bref;
            
            dbe_brefg2_loadfromaval(&bref, cd, atype, aval);
            return (dbe_brefg2_getblobg2size(&bref));
        } else {
            /* it is an old format BLOB */
            dbe_blobg2size_t size;
            dbe_blobsize_t size_in_old_type;
            dbe_vablobref_t old_bref;
            dbe_bref_loadfromva(&old_bref, va);
            size_in_old_type = dbe_bref_getblobsize(&old_bref);
            DBE_BLOBG2SIZE_ASSIGN_SIZE(&size, size_in_old_type);
            return (size);
        }
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_getidfromaval
 *
 * Gets BLOB id from aval
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type for aval
 *
 *      aval - in, use
 *          aval containing BLOB (G2 or old format) reference
 *
 * Return value:
 *      ID of the BLOB
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_blobg2id_t dbe_brefg2_getidfromaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        va_t * va;
        
        ss_dassert(rs_aval_isblob(cd, atype, aval));

        va = rs_aval_va(cd, atype, aval);
        if (dbe_brefg2_isblobg2check_from_va(va)) {
            dbe_vablobg2ref_t bref;
            
            dbe_brefg2_loadfromaval(&bref, cd, atype, aval);
            return (dbe_brefg2_getblobg2id(&bref));
        } else {
            /* it is an old format BLOB */
            dbe_blobg2id_t id;
            dbe_blobid_t id_in_old_type;
            dbe_vablobref_t old_bref;
            dbe_bref_loadfromva(&old_bref, va);
            id_in_old_type = dbe_bref_getblobid(&old_bref);
            DBE_BLOBG2ID_ASSIGN_UINT4(&id, id_in_old_type);
            return (id);
        }
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_storetodiskbuf
 *
 * Stores a G2 BLOB reference structure to end of disk-format buffer.
 *
 * Parameters:
 *      bref - in, use
 *          BLOB G2 reference structure
 *
 *      data - in out, use
 *          pointer to disk-format buffer where to store the
 *          BLOB reference. The recors is stored to end of the
 *          buffer and the other contents is not changed
 *
 *      datasize - in
 *          data buffer size in bytes
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_storetodiskbuf(dbe_vablobg2ref_t* bref, ss_byte_t* data, size_t datasize)
{
        ss_byte_t* data_plus_datasize = data + datasize;
        ss_byte_t* p_lo;
        ss_byte_t* p_hi;
        ss_uint4_t lo;
        ss_uint4_t hi;
        p_hi = data_plus_datasize + BLOBREFVA_OFS_ID_HI;
        p_lo = data_plus_datasize + BLOBREFVA_OFS_ID_LO;
        lo = DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bref->brefg2_id);
        hi = DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bref->brefg2_id);
        SS_UINT4_STORETODISK(p_lo, lo);
        SS_UINT4_STORETODISK(p_hi, hi);
        p_hi = data_plus_datasize + BLOBREFVA_OFS_SIZE_HI;
        p_lo = data_plus_datasize + BLOBREFVA_OFS_SIZE_LO;
        lo = DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(bref->brefg2_size);
        hi = DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(bref->brefg2_size);
        SS_UINT4_STORETODISK(p_lo, lo);
        SS_UINT4_STORETODISK(p_hi, hi);
        data_plus_datasize[BLOBREFVA_OFS_FLAGS] = bref->brefg2_flags;
        ss_dassert(bref->brefg2_flags == BREFG2_FLAG_ISBLOBG2);
}


/*#***********************************************************************\
 *
 *      dbe_brefg2_storetova
 *
 * stores a BLOB G2 reference record to end of a v-attribute, overwriting
 * the old contents from the end of the va.
 *
 * Parameters:
 *      bref - in, use
 *          BLOB reference record
 *
 *      va - in out, use
 *          pointer to v-attribute which to modify
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
static void dbe_brefg2_storetova(dbe_vablobg2ref_t* bref, va_t* va)
{
        void* data;
        va_index_t dsize;

        data = va_getdata(va, &dsize);
        dbe_brefg2_storetodiskbuf(bref, data, dsize);
}

/*##**********************************************************************\
 *
 *      dbe_brefg2_nullifyblobid_from_va
 *
 * sets the BLOB ID of a refdva -object (internal representation of
 * column value in rs_aval) to DBE_BLOBG2ID_NULL
 *
 * Parameters:
 *      va - in out, use
 *          pointer to va
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_brefg2_nullifyblobid_from_va(va_t* va)
{
        ss_dassert(va_testblob(va));
        if (dbe_brefg2_isblobg2check_from_va(va)) {
            dbe_vablobg2ref_t bref;
            dbe_brefg2_loadfromva(&bref, va);
            bref.brefg2_id = DBE_BLOBG2ID_NULL;
            dbe_brefg2_storetova(&bref, va);
        }
}

/*#***********************************************************************\
 * 
 *      wblobg2_init_continued
 * 
 * Creates a Write blob stream. This version allows continuing from existing
 * BLOB (for recovery)
 * 
 * Parameters :
 *      db - in, use (hold)
 *          database object, some subobjects are referenced later,
 *          that is why "(hold)"
 *
 *      startcpnum - in
 *          checkpoint number where the writing of the BLOB started
 *
 *      id - in
 *          BLOB id
 *
 *      startpos - in
 *          start position (offset) in the blob
 *
 *      getpageaddrfun - in, hold
 *          pointer to function that allocates a new page address.
 *
 *      getpageaddr_ctx - in, hold
 *          context to be used as first parameter for getpageaddrfun.
 *
 *      releasepageaddrfun - in, hold
 *         pointer to function that releases a page address
 *         (tells it is returned for cache manager)
 *
 *      releasepageaddr_ctx - in, hold
 *         context for releasepageaddrfun
 *
 * Return value - give :
 *      pointer to new blob object    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static dbe_wblobg2_t* wblobg2_init_continued(
        dbe_db_t* db,
        dbe_cpnum_t startcpnum,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startpos,
        su_ret_t (*getpageaddrfun)(
                void* getpageaddr_ctx,
                su_daddr_t* p_daddr,
                su_err_t** p_errh),
        void* getpageaddr_ctx,
        su_ret_t  (*releasepageaddrfun)(void* released_pageaddr_ctx,
                                        su_daddr_t daddr,
                                        size_t bytes_in_use,
                                        su_err_t** p_errh),
        void* releasepageaddr_ctx)
{
        dbe_wblobg2_t* wb = SSMEM_NEW(dbe_wblobg2_t);
        dbe_gobj_t* gobjs;

        wb->wb_check = DBE_CHK_WBLOBG2;
        wb->wb_status = BSTAT_RELEASED;
        gobjs = dbe_db_getgobj(db);
        wb->wb_iom = gobjs->go_iomgr;
        wb->wb_cache = gobjs->go_dbfile->f_indexfile->fd_cache;
        wb->wb_log = gobjs->go_dbfile->f_log;
        wb->wb_startcpnum = startcpnum;
        wb->wb_id = id;
        wb->wb_startofs = startpos;
        wb->wb_cacheslot = NULL;
        wb->wb_pagedata = NULL;
        wb->wb_pagesize = gobjs->go_dbfile->f_indexfile->fd_blocksize;
        wb->wb_writepos = 0;
        wb->wb_getpageaddrfun = getpageaddrfun;
        wb->wb_getpageaddr_ctx = getpageaddr_ctx;
        wb->wb_releasepageaddrfun = releasepageaddrfun;
        wb->wb_releasepageaddr_ctx = releasepageaddr_ctx;
        return (wb);
}


/*##**********************************************************************\
 * 
 *      dbe_wblobg2_init_for_recovery
 * 
 * Creates a Write blob stream. This version allows continuing from existing
 * BLOB (for recovery)
 * 
 * Parameters :
 *      db - in, use (hold)
 *          database object, some subobjects are referenced later,
 *          that is why "(hold)"
 *
 *      p_startcpnum_in_out - in out, use
 *          ptr to checkpoint number where the writing of the BLOB started.
 *          On input *p_start_in_out == DBE_CPNUM_NULL means this routine
 *          should allocate one.
 *
 *      id - in
 *          BLOB id
 *
 *      startpos - in
 *          start position (offset) in the blob
 *
 *      getpageaddrfun - in, hold
 *          pointer to function that allocates a new page address.
 *
 *      getpageaddr_ctx - in, hold
 *          context to be used as first parameter for getpageaddrfun.
 *
 *      releasepageaddrfun - in, hold
 *         pointer to function that releases a page address
 *         (tells it is returned for cache manager)
 *
 *      releasepageaddr_ctx - in, hold
 *         context for releasepageaddrfun
 *
 * Return value - give :
 *      pointer to new blob object    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_wblobg2_t* dbe_wblobg2_init_for_recovery(
        dbe_db_t* db,
        dbe_cpnum_t* p_startcpnum_in_out,
        dbe_blobg2id_t id,
        dbe_blobg2size_t startpos,
        su_ret_t (*getpageaddrfun)(
                void* getpageaddr_ctx,
                su_daddr_t* p_daddr,
                su_err_t** p_errh),
        void* getpageaddr_ctx,
        su_ret_t  (*releasepageaddrfun)(void* released_pageaddr_ctx,
                                        su_daddr_t daddr,
                                        size_t bytes_in_use,
                                        su_err_t** p_errh),
        void* releasepageaddr_ctx)
{
        dbe_wblobg2_t* wb;

        if (*p_startcpnum_in_out == DBE_CPNUM_NULL) {
            dbe_gobj_t* gobjs = dbe_db_getgobj(db);
            *p_startcpnum_in_out = dbe_counter_getcpnum(gobjs->go_ctr);
        }
        wb = wblobg2_init_continued(db,
                                    *p_startcpnum_in_out,
                                    id,
                                    startpos,
                                    getpageaddrfun,    getpageaddr_ctx,
                                    releasepageaddrfun,releasepageaddr_ctx);
#ifdef SS_HSBG2
        wb->wb_log = NULL;
#endif
        return (wb);
        
}

/*##**********************************************************************\
 * 
 *      dbe_wblobg2_init
 * 
 * Creates a Write blob stream
 * 
 * Parameters :
 *      db - in, use (hold)
 *          database object, some subobjects are referenced later,
 *          that is why "(hold)"
 *
 *      p_id_out - out, use
 *          pointer to blob id record where the generated new blob id
 *          will be stored.
 *
 *      p_startcpnum_out - out, use
 *          start checkpoint number for this blob
 *
 *      getpageaddrfun - in, hold
 *          pointer to function that allocates a new page address.
 *
 *      getpageaddr_ctx - in, hold
 *          context to be used as first parameter for getpageaddrfun.
 *
 *      releasepageaddrfun - in, hold
 *         pointer to function that releases a page address
 *         (tells it is returned for cache manager)
 *
 *      releasepageaddr_ctx - in, hold
 *         context for releasepageaddrfun
 *
 * Return value - give :
 *      pointer to new blob object    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_wblobg2_t* dbe_wblobg2_init(
        dbe_db_t* db,
        dbe_blobg2id_t* p_id_out,
        dbe_cpnum_t* p_startcpnum_out,
        su_ret_t (*getpageaddrfun)(
                void* getpageaddr_ctx,
                su_daddr_t* p_daddr,
                su_err_t** p_errh),
        void* getpageaddr_ctx,
        su_ret_t  (*releasepageaddrfun)(void* released_pageaddr_ctx,
                                        su_daddr_t daddr,
                                        size_t bytes_in_use,
                                        su_err_t** p_errh),
        void* releasepageaddr_ctx)
{
        dbe_wblobg2_t* wb;
        dbe_blobg2id_t id;
        dbe_blobg2size_t startpos;
        dbe_gobj_t* gobjs;
        dbe_cpnum_t startcpnum;
        
        gobjs = dbe_db_getgobj(db);
        id = dbe_counter_getnewblobg2id(gobjs->go_ctr);
        startcpnum = dbe_counter_getcpnum(gobjs->go_ctr);
        DBE_BLOBG2SIZE_ASSIGN_SIZE(&startpos, 0);
        wb = wblobg2_init_continued(db,
                                    startcpnum,
                                    id,
                                    startpos,
                                    getpageaddrfun,    getpageaddr_ctx,
                                    releasepageaddrfun,releasepageaddr_ctx);
        *p_startcpnum_out = startcpnum;
        *p_id_out = id;
        return (wb);
}

/*#***********************************************************************\
 * 
 *          wblobg2_done
 * 
 * Frees a write BLOB object. It must be either flushed or cancelled.
 * 
 * Parameters : 
 * 
 *      wb - in, take
 *          pointer to blob
 *		
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void wblobg2_done(dbe_wblobg2_t* wb)
{
        ss_debug(CHK_WB(wb));
        ss_dassert(wb->wb_pagedata == NULL);
        wb->wb_check = DBE_CHK_FREEDWBLOBG2;
        SsMemFree(wb);
}

/*#***********************************************************************\
 * 
 *      wblobg2_releasepage
 * 
 * Creates a Write blob stream
 * 
 * Parameters : 
 *      wb - in out, use
 *          write blob object
 *
 *      releasemode - in
 *          release mode (for cache manager)
 *
 * Return value:
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void wblobg2_releasepage(
        dbe_wblobg2_t* wb,
        dbe_cache_releasemode_t releasemode,
        su_daddr_t daddr)
{
        ss_debug(CHK_WB(wb));

        ss_dassert(wb->wb_cacheslot != NULL);
        if (releasemode == DBE_CACHE_IGNORE) {
            ss_rc_dassert(daddr == SU_DADDR_NULL, daddr); 
            dbe_cache_free(wb->wb_cache, wb->wb_cacheslot);
        } else {
            ss_rc_dassert(daddr != SU_DADDR_NULL, daddr);
            dbe_cache_setpageaddress(wb->wb_cache, wb->wb_cacheslot, daddr);
            dbe_iomgr_release(wb->wb_iom,
                              wb->wb_cacheslot,
                              releasemode,
                              wblobg2_reachctx);
        }
        wb->wb_pagedata = NULL;
        wb->wb_cacheslot = NULL;
        wb->wb_writepos = 0;
}

/*#***********************************************************************\
 * 
 *      wblobg2_putdata
 * 
 * Fills in data to cache page
 * 
 * Parameters : 
 *      wb - in out, use
 *          write blob object
 *
 * Return value:
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void wblobg2_putdata(dbe_wblobg2_t* wb)
{
        dbe_blocktype_t blocktype = DBE_BLOCK_BLOBG2PAGE;
        ss_debug(CHK_WB(wb));
        ss_dassert(wb->wb_pagedata != NULL);
        ss_dassert(wb->wb_writepos != 0);
        DBE_BLOCK_SETTYPE(wb->wb_pagedata, &blocktype);
        DBE_BLOCK_SETCPNUM(wb->wb_pagedata, &(wb->wb_startcpnum));
        SS_UINT4_STORETODISK(wb->wb_pagedata + BLOBG2_NBYTESINUSE_OFS, wb->wb_writepos);
        DBE_BLOBG2ID_PUTTODISK(wb->wb_pagedata + BLOBG2_BLOBID_OFS, wb->wb_id);
        DBE_BLOBG2SIZE_PUTTODISK(wb->wb_pagedata + BLOBG2_STARTOFS_OFS, wb->wb_startofs);

}
        
/*#***********************************************************************\
 * 
 *      wblobg2_log
 * 
 * Logs BLOB data to transaction log (if available)
 * 
 * Parameters : 
 *      wb - in out, use
 *          write blob object
 *
 * Return value:
 *      SU_SUCCESS when successful
 *      or error code (disk full?) when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_ret_t wblobg2_log(dbe_wblobg2_t* wb, rs_sysi_t* cd)
{
        su_ret_t rc = SU_SUCCESS;
        
        ss_debug(CHK_WB(wb));
        if (wb->wb_log != NULL) {
            rc = dbe_log_putblobg2data(wb->wb_log,
                                       cd,
                                       WBLOBG2_LOGPTR(wb),
                                       WBLOBG2_LOGSIZE(wb));
        }
        return (rc);
}
            
/*#***********************************************************************\
 * 
 *		wblobg2_logdatacomplete
 * 
 * Logs blob data complete mark.
 * 
 * Parameters : 
 * 
 *	wb - 
 *		
 *		
 *	cd - 
 *		
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static su_ret_t wblobg2_logdatacomplete(dbe_wblobg2_t* wb, rs_sysi_t* cd)
{
        su_ret_t rc = SU_SUCCESS;

        ss_dprintf_3(("wblobg2_logdatacomplete\n"));
        
        ss_debug(CHK_WB(wb));
        if (wb->wb_log != NULL) {
            rc = dbe_log_putblobg2datacomplete(wb->wb_log, cd, wb->wb_id);
        }
        return (rc);
}

/*#***********************************************************************\
 * 
 *      wblobg2_flush
 * 
 * flushes all data that is in reached cache page back to cache.
 * 
 * Parameters : 
 *      wb - in out, use
 *          write blob object
 *
 *      p_errh - out, give
 *          pointer to pointer to error object where to return error
 *          diagnostics when possible error
 *          occured (return value returns the error code in that case)
 *
 * Return value:
 *      SU_SUCCESS when successful
 *      or error code (disk full?) when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static su_ret_t wblobg2_flush(
        dbe_wblobg2_t* wb,
        bool complete,
        rs_sysi_t* cd,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        
        CHK_WB(wb);
        ss_rc_dassert(wb->wb_status == BSTAT_RELEASED, wb->wb_status);
        if (wb->wb_pagedata != NULL) {
            if (wb->wb_writepos == 0) {
                wblobg2_releasepage(wb, DBE_CACHE_IGNORE, SU_DADDR_NULL);
            } else {
                su_daddr_t daddr;
                wblobg2_putdata(wb);
                rc = wblobg2_log(wb, cd);
                if (rc != SU_SUCCESS) {
                    wblobg2_releasepage(wb, DBE_CACHE_IGNORE, SU_DADDR_NULL);
                    return (rc);
                }
                rc = (*wb->wb_getpageaddrfun)(wb->wb_getpageaddr_ctx, &daddr, p_errh);
                if (rc != SU_SUCCESS) {
                    wblobg2_releasepage(wb, DBE_CACHE_IGNORE, SU_DADDR_NULL);
                    return (rc);
                }
                rc = (*wb->wb_releasepageaddrfun)(
                        wb->wb_releasepageaddr_ctx,
                        daddr,
                        wb->wb_writepos,
                        p_errh);
                if (rc != SU_SUCCESS) {
                    wblobg2_releasepage(wb, DBE_CACHE_DIRTYLASTUSE, daddr);
                    return (rc);
                }
                DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&(wb->wb_startofs), wb->wb_writepos);
                wblobg2_releasepage(wb, DBE_CACHE_DIRTYLASTUSE, daddr);
            }
        } /* else do nothing! */
        if (complete && rc == SU_SUCCESS) {
            rc = wblobg2_logdatacomplete(wb, cd);
        }
        return (rc);
}
            
/*##**********************************************************************\
 * 
 *      dbe_wblobg2_flush
 * 
 * flushes all data that is in reached cache page back to cache.
 * 
 * Parameters : 
 *      wb - in out, use
 *          write blob object
 *
 *      p_errh - out, give
 *          pointer to pointer to error object where to return error
 *          diagnostics when possible error
 *          occured (return value returns the error code in that case)
 *
 * Return value:
 *      SU_SUCCESS when successful
 *      or error code (disk full?) when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_wblobg2_flush(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_WB(wb);

        rc = wblobg2_flush(
                wb,
                FALSE,
                cd,
                p_errh);

        return(rc);
}

/*##**********************************************************************\
 * 
 *          dbe_wblobg2_cancel
 * 
 * Cancels a write BLOB object
 * 
 * Parameters : 
 * 
 *      wb - in, take
 *          pointer to blob
 *		
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_wblobg2_cancel(dbe_wblobg2_t* wb)
{
        CHK_WB(wb);
        if (wb->wb_pagedata != NULL) {
            wblobg2_releasepage(wb, DBE_CACHE_IGNORE, SU_DADDR_NULL);
        }
        wblobg2_done(wb);
}

/*##**********************************************************************\
 * 
 *          dbe_wblobg2_reach
 * 
 * Reaches a write buffer for a blob
 * 
 * Parameters : 
 * 
 *      wb - in out, use
 *          pointer to blob
 *
 *      pp_buf - out, ref
 *          pointer to pointer to write buffer which
 *          points after successful call into a cache page
 *          where the caller can put (read from network?)
 *          the blob data
 *     
 *      p_nbytes - out
 *          pointer to variable where the size of the buffer will
 *          be stored
 *
 *      p_errh - out, give
 *          if non-NULL and
 *          in case of error a pointer to a newly allocated error
 *          handle is given in *p_errh
 *		
 * Return value :
 *      DBE_RC_SUCC when OK or error code when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_wblobg2_reach(
        dbe_wblobg2_t* wb,
        ss_byte_t** pp_buf,
        size_t* p_nbytes,
        su_err_t** p_errh)
{
        su_ret_t rc;

        CHK_WB(wb);
        ss_rc_dassert(wb->wb_status == BSTAT_RELEASED, wb->wb_status);
        rc = SU_SUCCESS;
        if (wb->wb_pagedata == NULL) {
            /* no reached page, need to get a new page */
            ss_dassert(wb->wb_cacheslot == NULL);
            wb->wb_cacheslot = dbe_cache_alloc(wb->wb_cache, (char**)&wb->wb_pagedata);
            if (wb->wb_cacheslot == NULL) {
                rc = DBE_ERR_OUTOFCACHEBLOCKS_SS;
                su_err_init(p_errh,
                            rc,
                            SU_DBE_INDEXSECTION,
                            SU_DBE_CACHESIZE);
                *p_nbytes = 0;
                *pp_buf = NULL;
                return (rc);
            }
            ss_dassert(wb->wb_pagedata != NULL);
        }
        *p_nbytes = WBLOBG2_NETPAGESIZE(wb) - wb->wb_writepos;
        ss_dassert(*p_nbytes > 0);
        *pp_buf =
            (ss_byte_t*)wb->wb_pagedata +
            BLOBG2_DATASTART_OFS +
            wb->wb_writepos;
        wb->wb_status = BSTAT_REACHED;
        return (rc);
}


/*##**********************************************************************\
 * 
 *          dbe_wblobg2_release
 * 
 * Releases a write buffer from reach
 * 
 * Parameters : 
 * 
 *      wb - in out, use
 *          pointer to write blob
 *		
 *      nbytes - in
 *          number of bytes written
 *
 *      p_errh - out, give
 *          if failed, a newly alocated error diag record is given in *p_errh
 *
 * Return value : 
 *      SU_SUCCESS when successful, error code when failed
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_wblobg2_release(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        size_t nbytes,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;

        CHK_WB(wb);
        ss_rc_dassert(wb->wb_status == BSTAT_REACHED, wb->wb_status);
        ss_dassert(wb->wb_pagedata != NULL);
        wb->wb_status = BSTAT_RELEASED;
        if (nbytes == 0) {
            return (rc);
        }
        ss_dassert(nbytes <= WBLOBG2_NETPAGESIZE(wb) - wb->wb_writepos);
        wb->wb_writepos += nbytes;
        if (wb->wb_writepos >= WBLOBG2_NETPAGESIZE(wb)) {
            ss_rc_dassert(wb->wb_writepos == WBLOBG2_NETPAGESIZE(wb), wb->wb_writepos);
            rc = wblobg2_flush(wb, FALSE, cd, p_errh);
        }
        return (rc);
}

/*##**********************************************************************\
 * 
 *          dbe_wblobg2_close
 * 
 * Closes a write BLOB object (successfully complete)
 * 
 * Parameters : 
 * 
 *      wb - in, take
 *          pointer to blob
 *
 *      p_errh - out, give
 *          possible error diag record is given if failed
 *
 * Return value :
 *      SU_ERROR or error code on failure
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_wblobg2_close(
        dbe_wblobg2_t* wb,
        rs_sysi_t* cd,
        su_err_t** p_errh)
{
        su_ret_t rc;
        
        ss_debug(CHK_WB(wb));
        rc = wblobg2_flush(wb, TRUE, cd, p_errh);
        wblobg2_done(wb);
        return (rc);
}


#ifdef SS_DEBUG
static char rblobg2_reachctx[] = "rBLOB G2 reach";
#else
#define rblobg2_reachctx NULL
#endif

#define CHK_RB(rb)\
        ss_assert((rb) != NULL); \
        ss_rc_assert((rb)->rb_check == DBE_CHK_RBLOBG2, (rb)->rb_check)


/* read blob object (dbe level functionality, see tab0blobg2.c) */
struct dbe_rblobg2_st {
        int               rb_check;
        blobg2_status_t   rb_status;
        dbe_iomgr_t*      rb_iom;
        dbe_blobg2id_t    rb_id;
        dbe_blobg2size_t  rb_startofs; /* byte offset for page starting from beginning of BLOB */
        su_daddr_t        rb_addr;     /* disk address of current page */
        dbe_cacheslot_t*  rb_cacheslot;/* cache slot for current page */
        ss_byte_t*        rb_pagedata; /* data pointer for current page */
        size_t            rb_pagesize; /* page (gross) size */
        size_t            rb_pagedatasize; /* page data size in use */
        size_t            rb_readpos;
        su_daddr_t      (*rb_getpageaddrfun)(void* getpageaddr_ctx, dbe_blobg2size_t offset);
        void*             rb_getpageaddr_ctx;
};

/*#***********************************************************************\
 * 
 *          rblobg2_releasepage
 * 
 * releases reached cache page and resets related structure members
 * 
 * Parameters : 
 * 
 *      rb - use
 *          pointer to blob
 *		
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
static void rblobg2_releasepage(dbe_rblobg2_t* rb)
{
        ss_debug(CHK_RB(rb));

        ss_dassert(rb->rb_cacheslot != NULL);
        ss_dassert(rb->rb_pagedata != NULL);
        dbe_iomgr_release(rb->rb_iom,
                          rb->rb_cacheslot,
                          DBE_CACHE_CLEANLASTUSE,
                          rblobg2_reachctx);
        rb->rb_cacheslot = NULL;
        rb->rb_readpos = 0;
        rb->rb_pagedata = NULL;
        rb->rb_pagedatasize = 0;
        rb->rb_addr = SU_DADDR_NULL;
}

/*##**********************************************************************\
 * 
 *      dbe_rblobg2_init
 * 
 * Creates a Read blob stream
 * 
 * Parameters :
 *      db - in, use (hold)
 *          database object, some subobjects are referenced later,
 *          that is why "(hold)"
 *
 *      id - in
 *          blob id
 *
 *      getpageaddrfun - in, hold
 *          pointer to function that gives the next page address.
 *
 *      getpageaddr_ctx - in, hold
 *          context to be used as first parameter for getpageaddrfun.
 *
 * Return value - give :
 *      pointer to new blob object    
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_rblobg2_t* dbe_rblobg2_init(
        dbe_db_t* db,
        dbe_blobg2id_t id,
        su_daddr_t (*getpageaddrfun)(void* getpageaddr_ctx, dbe_blobg2size_t offset),
        void* getpageaddr_ctx)
{
        dbe_rblobg2_t* rb = SSMEM_NEW(dbe_rblobg2_t);
        dbe_gobj_t* gobjs;

        rb->rb_check = DBE_CHK_RBLOBG2;
        rb->rb_status = BSTAT_RELEASED;
        gobjs = dbe_db_getgobj(db);
        rb->rb_iom = gobjs->go_iomgr;
        rb->rb_id = id;
        DBE_BLOBG2SIZE_ASSIGN_SIZE(&(rb->rb_startofs), 0);
        rb->rb_addr = SU_DADDR_NULL;
        rb->rb_cacheslot = NULL;
        rb->rb_pagedata = NULL;
        rb->rb_pagesize = gobjs->go_dbfile->f_indexfile->fd_blocksize;
        rb->rb_pagedatasize = 0;
        rb->rb_readpos = 0;
        rb->rb_getpageaddrfun = getpageaddrfun;
        rb->rb_getpageaddr_ctx = getpageaddr_ctx;
        ss_debug(CHK_RB(rb));
        return (rb);
}

/*##**********************************************************************\
 * 
 *      dbe_rblobg2_done
 * 
 * Frees a Read blob stream
 * 
 * Parameters :
 *      rb - in, take
 *          read blob
 *
 * Return value - give :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_rblobg2_done(
        dbe_rblobg2_t* rb)
{
        CHK_RB(rb);

        if (rb->rb_status == BSTAT_REACHED) {
            dbe_rblobg2_release(rb, 0);
        }
        ss_rc_dassert(rb->rb_status == BSTAT_RELEASED, rb->rb_status);
        if (rb->rb_pagedata != NULL) {
            rblobg2_releasepage(rb);
        }
        rb->rb_check = DBE_CHK_FREEDRBLOBG2;
        SsMemFree(rb);
}

/*##**********************************************************************\
 * 
 *          dbe_rblobg2_reach
 * 
 * Reaches a read buffer for a blob
 * 
 * Parameters : 
 * 
 *      rb - in out, use
 *          pointer to blob
 *
 *      pp_buf - out, ref
 *          pointer to pointer to read buffer which
 *          points after successful call into a cache page
 *          where the caller can read the blob data
 *     
 *      p_nbytes - out
 *          pointer to variable where the size of the buffer will
 *          be stored
 *		
 * Return value :
 *      DBE_RC_SUCC when OK or error code when failed (should never fail!)
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_ret_t dbe_rblobg2_reach(
        dbe_rblobg2_t* rb,
        ss_byte_t** pp_buf,
        size_t* p_nbytes)
{
        CHK_RB(rb);
        ss_rc_dassert(rb->rb_status == BSTAT_RELEASED, rb->rb_status);
        if (rb->rb_pagedata == NULL) {
            /* no reached page, need to get a new page */
            ss_dassert(rb->rb_cacheslot == NULL);
            rb->rb_addr = (*rb->rb_getpageaddrfun)(rb->rb_getpageaddr_ctx, rb->rb_startofs);
            if (rb->rb_addr == SU_DADDR_NULL) {
                /* no next page! */
                *pp_buf = NULL;
                *p_nbytes = 0;
                return (DBE_RC_END);
            }
            ss_dprintf_1(("dbe_rblobg2_reach:addr=%ld\n", rb->rb_addr));
            rb->rb_cacheslot = dbe_iomgr_reach(rb->rb_iom,
                                               rb->rb_addr,
                                               DBE_CACHE_READONLY,
                                               0,
                                               (char**)&rb->rb_pagedata,
                                               rblobg2_reachctx);
            ss_dassert(rb->rb_pagedata != NULL);
            ss_rc_dassert(rb->rb_readpos == 0, rb->rb_readpos);
            {
                dbe_blocktype_t blocktype;
                dbe_blobg2id_t blobid;
                dbe_blobg2size_t page_startpos;
                bool issame;
                
                DBE_BLOCK_GETTYPE(rb->rb_pagedata, &blocktype);
                ss_rc_assert(blocktype == DBE_BLOCK_BLOBG2PAGE, blocktype);
                blobid = DBE_BLOBG2ID_GETFROMDISK(rb->rb_pagedata + BLOBG2_BLOBID_OFS);
                issame = DBE_BLOBG2ID_EQUAL(blobid, rb->rb_id);
                ss_assert(issame);
                page_startpos = DBE_BLOBG2SIZE_GETFROMDISK(rb->rb_pagedata + BLOBG2_STARTOFS_OFS);
                issame = DBE_BLOBG2SIZE_EQUAL(page_startpos, rb->rb_startofs);
                ss_info_assert(issame,
                ((char *)"page_startpos=0x%08lX%08lX,rb->rb_startofs=0x%08lX%08lX,daddr=%lu bid=0x%08lX%08lX\n",
                 (ulong)DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(page_startpos),
                 (ulong)DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(page_startpos),
                 (ulong)DBE_BLOBG2SIZE_GETMOSTSIGNIFICANTUINT4(rb->rb_startofs),
                 (ulong)DBE_BLOBG2SIZE_GETLEASTSIGNIFICANTUINT4(rb->rb_startofs),
                 (ulong)rb->rb_addr,
                 (ulong)DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(rb->rb_id),
                 (ulong)DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(rb->rb_id)));
                rb->rb_pagedatasize =
                    SS_UINT4_LOADFROMDISK(rb->rb_pagedata + BLOBG2_NBYTESINUSE_OFS);
                ss_dassert(rb->rb_pagedatasize <= BLOBG2_NETPAGESIZE(rb->rb_pagesize)
                    &&     rb->rb_pagedatasize != 0);
            }
                
        }
        *p_nbytes = rb->rb_pagedatasize - rb->rb_readpos;
        ss_dassert(*p_nbytes != 0);
        *pp_buf =
            (ss_byte_t*)rb->rb_pagedata +
            BLOBG2_DATASTART_OFS +
            rb->rb_readpos;
        rb->rb_status = BSTAT_REACHED;
        return (SU_SUCCESS);
}
        
/*##**********************************************************************\
 * 
 *          dbe_rblobg2_release
 * 
 * Releases a read buffer for a blob (the read blob must be in
 * reached state!)
 * 
 * Parameters : 
 * 
 *      rb - in out, use
 *          pointer to blob
 *
 *      nbytes - out
 *          number of bytes the read has advanced
 *          (nbytes >= 0 && nbytes <= reached buffer size)
 *		
 * Return value :
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_rblobg2_release(
        dbe_rblobg2_t* rb,
        size_t nbytes)
{
        CHK_RB(rb);
        ss_rc_dassert(rb->rb_status == BSTAT_REACHED, rb->rb_status);
        ss_dassert(rb->rb_pagedata != NULL);
        rb->rb_status = BSTAT_RELEASED;
        ss_dassert(nbytes <= rb->rb_pagedatasize - rb->rb_readpos);
        rb->rb_readpos += nbytes;
        if (rb->rb_readpos >= rb->rb_pagedatasize) {
            ss_rc_dassert(rb->rb_readpos == rb->rb_pagedatasize, rb->rb_readpos);
            DBE_BLOBG2SIZE_ADDASSIGN_SIZE(&(rb->rb_startofs), rb->rb_readpos);
            rblobg2_releasepage(rb);
        }
}

#if 0 /* removed by pete 2002-1-10, not needed! */

/*##**********************************************************************\
 * 
 *          dbe_blobg2_unlink_by_vtpl
 * 
 * unlinks all blob v-attributes of a v-tuple. The v-tuple must be
 * a blob v-tuple in the index tree format. In index tree format there
 * is an extra v-attribute at the end of the v-tuple that tells the
 * blob attribute positions inside the v-tuple. 
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *          client data that is used to find correct blob manager context
 *		
 *      vtpl - in, use
 *          pointer to index blob vtpl
 *
 *      p_errh - out, give
 *          in case of error and p_errh != NULL a newly allocated error
 *          object is given in *p_errh
 *
 * Return value : 
 * 
 *      SU_SUCCESS when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_ret_t dbe_blobg2_unlink_by_vtpl(
        rs_sysi_t* cd,
        vtpl_t* vtpl,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        bool* blobattrs;
        int nattrs;
        int i;
        va_t* va;

        ss_dprintf_3(("dbe_blobg2_unlink_by_vtpl\n"));
        blobattrs = dbe_blobinfo_getattrs(vtpl, 0, &nattrs);

        va = VTPL_GETVA_AT0(vtpl);
        for (i = 0; i < nattrs; i++, va = VTPL_SKIPVA(va)) {
            if (blobattrs[i]) {
                su_ret_t rc2 =
                    (*dbe_blobg2callback_decrementpersistentrefcount_byva)(
                            cd, va, p_errh);
                if (rc2 != SU_SUCCESS) {
                    /* only report 1st error */
                    p_errh = NULL;
                    if (rc == SU_SUCCESS) {
                        rc = rc2;
                    }
                }
            }
        }
        SsMemFree(blobattrs);
        ss_dprintf_3(("dbe_blobg2_unlink_by_vtpl:end, rc == %d\n", (int)rc));
        return (rc);
}
#endif /* pete removed */


/*##**********************************************************************\
 *
 *      dbe_blobg2_append_blobids_of_vtpl_to_list
 *
 * appends all BLOB IDs referred to one vtuple into the list of BLOB
 * IDs. This is needed because merge cannot call the BLOB unlink
 * from within the merge gate.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      list - in out, use
 *          list where to add the BLOB IDs
 *
 *      vtpl - in, use
 *          v-tuple containing 1 or more BLOb references
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_blobg2_append_blobids_of_vtpl_to_list(
        rs_sysi_t* cd,
        su_list_t* list,
        vtpl_t* vtpl)
{
        bool* blobattrs;
        int nattrs;
        int i;
        va_t* va;

        ss_dprintf_3(("dbe_blobg2_append_blobids_of_vtpl_to_list\n"));
        blobattrs = dbe_blobinfo_getattrs(vtpl, 0, &nattrs);
        va = VTPL_GETVA_AT0(vtpl);
        for (i = 0; i < nattrs; i++, va = VTPL_SKIPVA(va)) {
            if (blobattrs[i]) {
                if (dbe_brefg2_isblobg2check_from_va(va)) {
                    dbe_vablobg2ref_t bref;
                    dbe_blobg2id_t* p_bid = SSMEM_NEW(dbe_blobg2id_t);

                    dbe_brefg2_loadfromva(&bref, va);
                    *p_bid = dbe_brefg2_getblobg2id(&bref);
                    su_list_insertlast(list, p_bid);
                } else {
                    /* old blob, delete immediately! */
                    dbe_blobmgr_t* blobmgr =
                        dbe_db_getblobmgr(rs_sysi_db(cd));
                    dbe_blobmgr_delete(blobmgr, va);
                }                    
            }
        }
        SsMemFree(blobattrs);
        ss_dprintf_3(("dbe_blobg2_append_blobids_of_vtpl_to_list:end\n"));
}

/*##**********************************************************************\
 *
 *      dbe_blobg2_unlink_list_of_blobids
 *
 * unlinks (decrements persistent reference count) for a list of BLOB IDs
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      blobref_list - in out, use
 *          list of BLOB references to unlink
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t dbe_blobg2_unlink_list_of_blobids(
        rs_sysi_t* cd,
        su_list_t* blobref_list,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        dbe_blobg2id_t* bid;

        ss_dprintf_3(("dbe_blobg2_unlink_list_of_blobrefs\n"));

        for (;;su_list_removefirst(blobref_list)) {
            su_ret_t rc2;
            su_list_node_t* ln = su_list_first(blobref_list);
            if (ln == NULL) {
                break;
            }
            bid = su_listnode_getdata(ln);
            rc2 = (*dbe_blobg2callback_decrementpersistentrefcount)(
                    cd,
                    *bid,
                    p_errh);
            SsMemFree(bid);
            if (rc2 != SU_SUCCESS) {
                /* only report 1st error */
                p_errh = NULL;
                if (rc == SU_SUCCESS) {
                    rc = rc2;
                }
            }
        }
        ss_dprintf_3(("dbe_blobg2_unlink_list_of_blobrefs:end, rc == %d\n", (int)rc));
        return (rc);
}

/*##**********************************************************************\
 *
 *      dbe_blobg2_insertaval
 *
 * inserts a (long) attribute value to BLOB storage so that the
 * original aval will be changed to point to the BLOB.
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      atype - in, use
 *          type for aval
 *
 *      aval - in out, use
 *          aval containing long value on input
 *          and BLOB reference on output
 *
 *      maxvalen - in
 *          currently not used
 *
 *      p_errh - out, give
 *          for error handle output
 *
 * Return value:
 *      SU_SUCCESS or error code
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t dbe_blobg2_insertaval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t maxvalen __attribute__ ((unused)),
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        ss_byte_t* data;
        ulong datalen_tmp;
        size_t datalen;
        size_t ntocopy;
        size_t ncopied;
        size_t bufsize;
        ss_byte_t* writebuf;
        tb_wblobg2stream_t* wbs;
        rs_aval_t* new_aval = rs_aval_create(cd, atype);

        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        ss_dassert(!rs_aval_isblob(cd, atype, aval));
        data = rs_aval_getdata(cd, atype, aval, &datalen_tmp);
        datalen = (size_t)datalen_tmp;
        wbs = dbe_blobg2callback_wblobinit(cd, atype, new_aval);
        for (ncopied = 0; ncopied < datalen; ncopied += ntocopy) {
            rc = (*dbe_blobg2callback_wblobreach)(
                    wbs,
                    &writebuf, &bufsize,
                    p_errh);
            if (rc != SU_SUCCESS) {
                goto failure;
            }
            ntocopy = datalen - ncopied;
            if (bufsize < ntocopy) {
                ntocopy = bufsize;
            }
            memcpy(writebuf, data + ncopied, ntocopy);
            rc = (*dbe_blobg2callback_wblobrelease)(
                    wbs,
                    ntocopy,
                    p_errh);
            if (rc != SU_SUCCESS) {
                goto failure;
            }
        }
        rc = (*dbe_blobg2callback_wblobdone)(wbs, p_errh);
        if (rc == SU_SUCCESS) {
            ss_debug(RS_AVALRET_T avalrc =)
                rs_aval_assign_ext(cd, atype, aval, atype, new_aval, p_errh);
            /* cannot fail! */
            ss_rc_dassert(avalrc == RSAVR_SUCCESS, avalrc);
        }
 return_rc:;
        rs_aval_free(cd, atype, new_aval);
        return (rc);
 failure:;
        (*dbe_blobg2callback_wblobabort)(wbs);
        goto return_rc;
}

void dbe_blobg2_get_id_and_endpos_from_page(
        rs_sysi_t* cd __attribute__ ((unused)),
        ss_byte_t* pagedata __attribute__ ((unused)),
        dbe_blobg2id_t* p_bid,
        dbe_blobg2size_t* p_startpos,
        dbe_blobg2size_t* p_endpos)
{

        ss_uint4_t nbytesinuse;

        ss_rc_dassert(pagedata[0] == DBE_BLOCK_BLOBG2PAGE, pagedata[0]);
        *p_bid = DBE_BLOBG2ID_GETFROMDISK(pagedata + BLOBG2_BLOBID_OFS);
        *p_endpos = *p_startpos =
            DBE_BLOBG2SIZE_GETFROMDISK(pagedata + BLOBG2_STARTOFS_OFS);
        nbytesinuse = SS_UINT4_LOADFROMDISK(pagedata + BLOBG2_NBYTESINUSE_OFS);
        
        DBE_BLOBG2SIZE_ADDASSIGN_SIZE(p_endpos, nbytesinuse);
}

/*##**********************************************************************\
 *
 *      dbe_blobg2_relocate_page
 *
 * Relacates and releases the given page
 *
 * Parameters:
 *      cd - in, use
 *          client context
 *
 *      page_data - in, take
 *          pointer to page data area, currently not used.
 *
 *      page_slot - in, take
 *          cache page
 *
 *      new_daddr - in
 *          new disk address for the page
 *
 * Return value:
 *      SU_SUCCESS
 *
 * Limitations:
 *
 * Globals used:
 */
su_ret_t dbe_blobg2_relocate_page(
        rs_sysi_t* cd,
        ss_byte_t* page_data __attribute__ ((unused)),
        void* page_slot,
        su_daddr_t new_daddr)
{
        dbe_db_t* db;
        dbe_gobj_t* gobjs;
        dbe_cache_t* cache;
        dbe_iomgr_t* iomgr;

        db = rs_sysi_db(cd);
        gobjs = dbe_db_getgobj(db);
        cache = gobjs->go_dbfile->f_indexfile->fd_cache;
        iomgr = gobjs->go_iomgr;
        page_slot = dbe_cache_relocate(cache, page_slot, new_daddr, NULL, 0);
        dbe_iomgr_release(iomgr,
                          page_slot,
                          DBE_CACHE_DIRTYLASTUSE,
                          wblobg2_reachctx);
        return (SU_SUCCESS);
}
