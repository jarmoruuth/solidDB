/*************************************************************************\
**  source       * dbe0bstr.c
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

#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------


Error handling:
--------------


Objects used:
------------


Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------


**************************************************************************
#endif /* DOCUMENTATION */

#include <rs0aval.h>
#include <rs0key.h>
#include "dbe6blob.h"
#include "dbe6bmgr.h"
#include "dbe0db.h"
#include "dbe0trx.h"
#include "dbe0bstr.h"

/* write blob object */
struct dbe_wblob_st {
        rs_sysinfo_t*          wb_cd;
        dbe_blobwritestream_t* wb_stream;
        rs_atype_t*            wb_atype;
        rs_aval_t*             wb_aval;
        dbe_trx_t*             wb_trx;
        dbe_blobsize_t         wb_size;
        dbe_blobsize_t         wb_nwritten;
        su_ret_t               wb_errcode;
};

/*##**********************************************************************\
 * 
 *		dbe_blobaval_delete
 * 
 * Deletes a BLOB identified by BLOB ref. va
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	db - in, use
 *		database object
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobaval_delete(
                rs_sysinfo_t* cd,
                dbe_db_t* db,
                rs_atype_t* atype,
                rs_aval_t* aval)
{
        dbe_ret_t rc;
        va_t* p_va;
        dbe_blobmgr_t* blobmgr;

        dbe_db_enteraction(db, cd);
        if (!dbe_db_setchanged(db, NULL)) {
            rc = DBE_ERR_DBREADONLY;
        } else {
            blobmgr = dbe_db_getblobmgr(db);
            ss_dassert(rs_aval_isblob(cd, atype, aval));
            p_va = rs_aval_va(cd, atype, aval);
            rc = dbe_blobmgr_delete(blobmgr, p_va);
        }
        dbe_db_exitaction(db, cd);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_blobaval_read
 * 
 * Reads actual blob data to (moderate-sized) attribute
 * 
 * Parameters : 
 * 
 *	cd - use
 *		client data
 *		
 *	db - use
 *		db object
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - use
 *		attribute value on input it contains blob reference,
 *          on output it has the whole blob data.
 *		
 * Return value :
 *      DBE_RC_SUCC when successful,
 *      error code when failed (should not fail)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobaval_read(
        rs_sysinfo_t* cd,
        dbe_db_t* db,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        dbe_ret_t rc;
        va_t* p_va;
        dbe_blobmgr_t* blobmgr;

        blobmgr = dbe_db_getblobmgr(db);
        ss_dassert(rs_aval_isblob(cd, atype, aval));
        p_va = rs_aval_va(cd, atype, aval);

        rc = dbe_blobmgr_readtoaval(
                cd,
                blobmgr,
                p_va,
                atype,
                aval);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_blobaval_size
 * 
 * Returns (blob) size of an aval which has a blob reference
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	atype - in, use
 *		attribute type
 *		
 *	aval - in, use
 *		attribute value
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_blobsize_t dbe_blobaval_size(
        rs_sysinfo_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        dbe_blobsize_t blobsize;
        va_t* va;

        ss_dassert(rs_aval_isblob(cd, atype, aval));
        va = rs_aval_va(cd, atype, aval);
        blobsize = dbe_brefva_getblobsize(va);
        return (blobsize);
}

/*##**********************************************************************\
 * 
 *		dbe_wblob_init
 * 
 * Creates a new write BLOB object
 * 
 * Parameters : 
 * 
 *	cd - in, hold
 *		client data
 *		
 *	db - in, use
 *		database object
 *		
 *	trx - in (out), hold
 *		database transaction
 *		
 *	atype - in, hold
 *		attribute type
 *		
 *	aval - in out, hold
 *		attribute value, will be updated to contain the BLOB
 *          ref. va
 *		
 *	blobsize - in
 *		BLOB total size in bytes or DBE_BLOBSIZE_UNKNOWN when not
 *          known
 *		
 * Return value - give :
 *      pointer to created write BLOB object
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_wblob_t* dbe_wblob_init(
                rs_sysinfo_t* cd,
                dbe_db_t* db,
                dbe_trx_t* trx,
                rs_atype_t* atype,
                rs_aval_t* aval,
                dbe_blobsize_t blobsize)
{
        dbe_blobmgr_t* blobmgr;
        dbe_trxid_t trxid;
        dbe_wblob_t* wb;

        ss_dassert(atype != NULL);
        ss_dassert(aval != NULL);
        wb = SSMEM_NEW(dbe_wblob_t);
        wb->wb_cd = cd;
        wb->wb_atype = atype;
        wb->wb_aval = aval;
        wb->wb_trx = trx;
        wb->wb_size = blobsize;
        wb->wb_nwritten = 0L;
        blobmgr = dbe_db_getblobmgr(db);
        trxid = dbe_trx_getstmttrxid(trx);

        if (!dbe_db_setchanged(db, NULL)) {
            wb->wb_errcode = DBE_ERR_DBREADONLY;
            wb->wb_stream = NULL;
        } else {
            wb->wb_errcode = SU_SUCCESS;
            wb->wb_stream = dbe_blobwritestream_init(
                                blobmgr,
                                cd,
                                blobsize,
                                DBE_BLOBVAGROSSLEN(RS_KEY_MAXCMPLEN),
                                trxid);
        }

        return (wb);
}

/*##**********************************************************************\
 * 
 *		dbe_wblob_done
 * 
 * Closes and deletes a write BLOB object and preserves data in database
 * 
 * Parameters : 
 * 
 *	wb - in, take
 *		write BLOB object
 *		
 * Return value :
 *      DBE_RC_SUCC when successful or
 *      DBE_WARN_BLOBSIZE_OVERFLOW when actual size written exceed the
 *          previously specified size or
 *      DBE_WARN_BLOBSIZE_UNDERFLOW when actual size written is smaller
 *          than previously specified size.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_wblob_done(
        dbe_wblob_t* wb)
{
        dbe_ret_t rc;
        va_t* p_va;
        
        ss_dassert(wb != NULL);
        rc = DBE_RC_SUCC;
        if (wb->wb_size != DBE_BLOBSIZE_UNKNOWN) {
            if (wb->wb_nwritten > wb->wb_size) {
                rc = DBE_WARN_BLOBSIZE_OVERFLOW;
            } else if (wb->wb_nwritten < wb->wb_size) {
                rc = DBE_WARN_BLOBSIZE_UNDERFLOW;
            }
        }
        if (wb->wb_stream != NULL) {
            dbe_blobwritestream_close(wb->wb_stream);
            p_va = dbe_blobwritestream_getva(wb->wb_stream);
            if (p_va != NULL) {
                rs_aval_setva(wb->wb_cd, wb->wb_atype, wb->wb_aval, p_va);
            }
            dbe_blobwritestream_done(wb->wb_stream);
        }
        SsMemFree(wb);
        return (rc);
}

/*##**********************************************************************\
 * 
 *		dbe_wblob_abort
 * 
 * Deletes write BLOB object and discard all data written to database
 * through it
 * 
 * Parameters : 
 * 
 *	wb - in, take
 *		write BLOB object
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_wblob_abort(
        dbe_wblob_t* wb)
{
        ss_dassert(wb != NULL);
        if (wb->wb_stream != NULL) {
            dbe_blobwritestream_abort(wb->wb_stream);
        }
        SsMemFree(wb);
}

/*##**********************************************************************\
 * 
 *		dbe_wblob_write
 * 
 * Writes a bufferful of data into a write BLOB object
 * 
 * Parameters : 
 * 
 *	wb - in out, use
 *		write BLOB
 *		
 *	buf - in, use
 *		data buffer
 *		
 *	bufsize - in
 *		# of bytes to write
 *		
 *	p_written - out
 *		pointer to variable where the # of bytes written will be stored
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#if 0 /* Removed by Pete 1995-08-29 */
dbe_ret_t dbe_wblob_write(
        dbe_wblob_t* wb,
        char* buf,
        size_t bufsize,
        size_t* p_written)
{
        dbe_ret_t rc;

        ss_dassert(wb != NULL);
        ss_dassert(buf != NULL);
        ss_dassert(p_written != NULL);

        rc = dbe_blobwritestream_write(wb->wb_stream, buf, bufsize, p_written);
        wb->wb_nwritten += (dbe_blobsize_t)*p_written;
        return (rc);
}
#endif
/*##**********************************************************************\
 * 
 *		dbe_wblob_reach
 * 
 * Reaches a write buffer for the BLOB
 * 
 * Parameters : 
 * 
 *	wb - in out, use
 *		write BLOB
 *
 *      pp_buf - out, ref
 *          pointer pointer to write buffer
 *
 *	p_avail - out
 *		pointer to variable where the number of bytes available
 *          for writing will be stored
 *		
 * Return value :
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_wblob_reach(
        dbe_wblob_t* wb,
        char** pp_buf,
        size_t* p_avail)
{
        dbe_ret_t rc;

        ss_dassert(wb != NULL);
        ss_dassert(p_avail != NULL);
        if (wb->wb_stream == NULL) {
            return(wb->wb_errcode);
        } else {
            rc = dbe_blobwritestream_reach(wb->wb_stream, pp_buf, p_avail);
            return (rc);
        }
}

/*##**********************************************************************\
 * 
 *		dbe_wblob_release
 * 
 * Releases a reched write buffer
 * 
 * Parameters : 
 * 
 *	wb - in out, use
 *		write BLOB
 *		
 *	n_written - in
 *		# of bytes written
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_wblob_release(
        dbe_wblob_t* wb,
        size_t n_written)
{
        ss_dassert(wb != NULL);
        if (wb->wb_stream != NULL) {
            dbe_blobwritestream_release(wb->wb_stream, n_written);
            wb->wb_nwritten += (dbe_blobsize_t)n_written;
        }
}

/*##**********************************************************************\
 * 
 *		dbe_rblob_init
 * 
 * Creates a read blob stream (su_bstream_t)
 * 
 * Parameters : 
 * 
 *	db - in, use
 *		database object
 *		
 *	blobref_va - in, use
 *		blob reference va
 *      
 *      p_totalsize - out
 *          pointer to variable where the total BLOB size will be stored
 *      
 *		
 * Return value - give : 
 *      pointer to su_bstream_t object (aliased as dbe_rblob_t)
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_rblob_t* dbe_rblob_init(
                dbe_db_t* db,
                va_t* blobref_va,
                dbe_blobsize_t* p_totalsize)
{
        dbe_ret_t rc;
        dbe_blobmgr_t* blobmgr;
        su_bstream_t* stream;

        ss_dassert(db != NULL);
        ss_dassert(blobref_va != NULL);
        ss_dassert(va_testblob(blobref_va));
        ss_dassert(p_totalsize != NULL);
        *p_totalsize = dbe_brefva_getblobsize(blobref_va);
        blobmgr = dbe_db_getblobmgr(db);
        rc = dbe_blobmgr_getreadstreamofva(blobmgr, blobref_va, &stream);
        su_rc_assert(rc == DBE_RC_SUCC, rc);
        return (stream);
}


