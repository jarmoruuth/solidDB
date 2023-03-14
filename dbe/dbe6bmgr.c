/*************************************************************************\
**  source       * dbe6bmgr.c
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


#ifdef DOCUMENTATION
**************************************************************************

Implementation:
--------------


Limitations:
-----------

We cannot test that input v-attribute is a blob v-attribute. When the
v-attribute is got from the index, the special blob v-attribute length
information is lost during key compression. For that reason, we just have
to trust that the v-attributes are blob v-attributes.

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

#include <ssc.h>
#include <ssstring.h>
#include <ssstdlib.h>
#include <sslimits.h>
#include <ssmem.h>
#include <ssdebug.h>

#include <su0bstre.h>
#include <su0icvt.h>

#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe7binf.h"
#include "dbe6blob.h"
#include "dbe6bmgr.h"

#define BLOB_READAHEAD 5

struct dbe_blobmgr_st {
        dbe_iomgr_t*    bm_iomgr;
        dbe_file_t*     bm_file;
        dbe_counter_t*  bm_counter;
        dbe_blobsize_t  bm_bloblogthreshold;
};

/*##**********************************************************************\
 * 
 *              dbe_bref_loadfromdiskbuf
 * 
 * Loads blob reference from memory buffer that is in disk format
 * 
 * Parameters : 
 * 
 *      bref - out, use
 *              pointer to blob ref.
 *              
 *      diskbuf - in, use
 *              pointer to disk buffer
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_bref_loadfromdiskbuf(
        dbe_vablobref_t* bref,
        char* diskbuf)
{
        ss_dprintf_3(("dbe_bref_loadfromdiskbuf\n"));
        bref->bref_blobid =  SS_UINT4_LOADFROMDISK(diskbuf);
        diskbuf += sizeof(bref->bref_blobid);
        bref->bref_blobsize =  SS_UINT4_LOADFROMDISK(diskbuf);
        diskbuf += sizeof(bref->bref_blobsize);
        bref->bref_fileno = (uchar)*diskbuf;
        diskbuf += sizeof(bref->bref_fileno);
        bref->bref_daddr =  SS_UINT4_LOADFROMDISK(diskbuf);
        ss_dprintf_3(("dbe_bref_loadfromdiskbuf: end\n"));
}

/*##**********************************************************************\
 * 
 *              dbe_bref_storetodiskbuf
 * 
 * Stores a blob reference into a memory buffer in disk format
 * 
 * Parameters : 
 * 
 *      bref - in, use
 *              pointer to blob ref.
 *              
 *      diskbuf - out, use
 *              pointer to disk buffer
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_bref_storetodiskbuf(
        dbe_vablobref_t* bref,
        char* diskbuf)
{
        ss_dprintf_3(("dbe_bref_storetodiskbuf\n"));
        SS_UINT4_STORETODISK(diskbuf, bref->bref_blobid);
        diskbuf += sizeof(bref->bref_blobid);
        SS_UINT4_STORETODISK(diskbuf, bref->bref_blobsize);
        diskbuf += sizeof(bref->bref_blobsize);
        *diskbuf = (char)bref->bref_fileno;
        diskbuf += sizeof(bref->bref_fileno);
        SS_UINT4_STORETODISK(diskbuf, bref->bref_daddr);
        ss_dprintf_3(("dbe_bref_storetodiskbuf: end\n"));
}


/*##**********************************************************************\
 * 
 *              dbe_bref_loadfromva
 * 
 * Loads a blob reference from a va to dbe_vablobref_t
 * 
 * Parameters : 
 * 
 *      bref - out, use
 *              pointer to blobref structure
 *              
 *      p_va - in, use
 *          pointer to va
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
void dbe_bref_loadfromva(
        dbe_vablobref_t* bref,
        va_t* p_va)
{
        char* p;

        ss_dprintf_3(("dbe_bref_loadfromva\n"));
        ss_dassert(DBE_VABLOBREF_SIZE <= va_netlen(p_va));
        p = (char*)p_va + VA_GROSSLEN(p_va) - DBE_VABLOBREF_SIZE;
        dbe_bref_loadfromdiskbuf(bref, p);
        ss_dprintf_3(("dbe_bref_loadfromva: end\n"));
}

/*##**********************************************************************\
 * 
 *              dbe_bref_storetova
 * 
 * Stores a dbe_vablobref_t object to a va
 * 
 * Parameters : 
 * 
 *      bref - in, use
 *              pointer to blobref structure
 *              
 *      p_va - out, use
 *              pointer to va
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_bref_storetova(
        dbe_vablobref_t* bref,
        va_t* p_va)
{
        char* p;

        ss_dprintf_3(("dbe_bref_storetova\n"));
        ss_dassert(DBE_VABLOBREF_SIZE <= va_netlen(p_va));
        p = (char*)p_va + VA_GROSSLEN(p_va) - DBE_VABLOBREF_SIZE;
        dbe_bref_storetodiskbuf(bref, p);
        ss_dprintf_3(("dbe_bref_storetova: end\n"));
}

/*##**********************************************************************\
 * 
 *              dbe_bref_makecopyof
 * 
 * Allocates a new copy of a blob reference structure. It can be
 * freed with SsMemFree()
 * 
 * Parameters : 
 * 
 *      bref - in, use
 *              pointer to blobref
 *              
 * Return value :
 *      pointer to newly allocated blob ref.
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_vablobref_t* dbe_bref_makecopyof(
        dbe_vablobref_t* bref)
{
        dbe_vablobref_t* new_bref;

        ss_dprintf_3(("dbe_bref_makecopyof\n"));
        ss_dassert(bref != NULL);
        new_bref = SSMEM_NEW(dbe_vablobref_t);
        memcpy(new_bref, bref, sizeof(dbe_vablobref_t));
        ss_dprintf_3(("dbe_bref_makecopyof: end\n"));
        return (new_bref);
}

/*##**********************************************************************\
 * 
 *              dbe_brefva_getblobsize
 * 
 * Gets BLOB size out of blob reference va
 * 
 * Parameters : 
 * 
 *      p_va - in, use
 *              pointer to v-attribute
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_blobsize_t dbe_brefva_getblobsize(va_t* p_va)
{
        dbe_vablobref_t bref;

        dbe_bref_loadfromva(&bref, p_va);
        return (dbe_bref_getblobsize(&bref));
}

/*##**********************************************************************\
 * 
 *          dbe_blobmgr_init
 * 
 * Creates the blob manager object
 * 
 * Parameters : 
 * 
 *      iomgr - in, hold
 *          pointer to the iomgr object
 *
 *      file - in, hold
 *          pointer to the db file object
 *
 *      counter - in, hold
 *          pointer to the dbe counter object
 *
 *      brb - in, hold
 *          pointer to blob ref. buffer object
 *
 *      bloblogthreshold - in
 *          threshold size of BLOBs below which BLOBs are entirely
 *          copied 
 *              
 * Return value - give : 
 *      pointer to created blob manager
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_blobmgr_t* dbe_blobmgr_init(
        dbe_iomgr_t* iomgr,
        dbe_file_t* file,
        dbe_counter_t* counter,
        dbe_blobsize_t bloblogthreshold)
{
        dbe_blobmgr_t* blobmgr;

        ss_dassert(file != NULL);
        ss_dassert(counter != NULL);
        blobmgr = SSMEM_NEW(dbe_blobmgr_t);
        blobmgr->bm_iomgr = iomgr;
        blobmgr->bm_file = file;
        blobmgr->bm_counter = counter;
        blobmgr->bm_bloblogthreshold = bloblogthreshold;
        return (blobmgr);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_done
 * 
 * Deletes a blob manager object
 * 
 * Parameters : 
 * 
 *      blobmgr - in, take
 *              pointer to blob manager
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobmgr_done(dbe_blobmgr_t* blobmgr)
{
        ss_dassert(blobmgr != NULL);
        SsMemFree(blobmgr);
}

/*#***********************************************************************\
 * 
 *              blobmgr_selectfiledes
 * 
 * Selects a file descriptor for a blob of given size
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      blobsize - in
 *              size of the blob (or DBE_BLOBSIZE_UNKNOWN)
 *              
 *      p_blobfileidx - out
 *              pointer to variable where the index of the blob
 *          file (=blob file #) will be stored
 *              
 * Return value - ref : 
 *      pointer to selected file descriptor object
 *
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
static dbe_filedes_t* blobmgr_selectfiledes(
        dbe_blobmgr_t* blobmgr,
        dbe_blobsize_t blobsize,
        uint* p_blobfileidx)
{
        uint i;
        dbe_filedes_t* filedes;
        dbe_filedes_t* prev_filedes;
        uint blobfilearraysize;

        ss_dprintf_3(("blobmgr_selectfiledes\n"));
        if (blobmgr->bm_file->f_blobfiles == NULL) {
            if (p_blobfileidx != NULL) {
                *p_blobfileidx = 0;
            }
            ss_dprintf_3(("blobmgr_selectfiledes: end 1\n"));
            return (blobmgr->bm_file->f_indexfile);
        }
#ifdef DBE_FILE_BLOBFILES
        blobfilearraysize = su_pa_nelems(blobmgr->bm_file->f_blobfiles);
        ss_dassert(blobfilearraysize > 1);
        prev_filedes = NULL;
        for (i = 1; i < blobfilearraysize; i++) {
            filedes = su_pa_getdata(blobmgr->bm_file->f_blobfiles, i);
            if (prev_filedes == NULL) {
                prev_filedes = filedes;
            }
            if (blobsize / filedes->fd_blocksize < 10) {
                if (i > 1) {
                    i--;
                }
                break;
            }
            prev_filedes = filedes;
        }
        if (i == blobfilearraysize) {
            i--;
        }
        if (p_blobfileidx != NULL) {
            *p_blobfileidx = i;
        }
        ss_dprintf_3(("blobmgr_selectfiledes: end 2\n"));
        return (prev_filedes);
#else
        ss_error;
        return(0);
#endif
}


/*##**********************************************************************\
 * 
 *              dbe_blobmgr_insertaval
 * 
 * Inserts an aval object into blob file
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      atype - in, use
 *              attribute type
 *              
 *      aval - in out, use
 *              attribute value
 *              
 *      maxvalen - in
 *              maximum gross length of the blob reference va
 *
 *      trxid - in
 *          Transaction ID
 *              
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_insertaval(
        rs_sysinfo_t* cd,
        dbe_blobmgr_t* blobmgr,
        rs_atype_t* atype,
        rs_aval_t* aval,
        va_index_t maxvalen,
        dbe_trxid_t trxid)
{
        su_ret_t rc;
        dbe_blobsize_t blobsize;
        dbe_blobsize_t ncopied;
        dbe_writeblob_t* wblob;
        dbe_vablobref_t blobref;
        dbe_filedes_t* filedes;
        va_t* p_va;
        dynva_t new_va = NULL;
        va_index_t grosslen_of_va;
        va_index_t ndata;
        char* p_new_va_data;

        char* p_data;
        ulong datalen;

        char* buf;
        size_t bufsize;
        size_t ntocopy;
        uint t;

        ss_dprintf_3(("dbe_blobmgr_insertaval\n"));

        rc = DBE_RC_SUCC;
        ss_dassert(!rs_aval_isnull(cd, atype, aval));
        p_va = rs_aval_va(cd, atype, aval);
        grosslen_of_va = va_grosslen(p_va);

        ss_dassert(grosslen_of_va > maxvalen);
        dynva_setblobdata(
            &new_va,
            NULL,
            maxvalen - VA_LENGTHMAXLEN - DBE_VABLOBREF_SIZE,
            NULL,
            DBE_VABLOBREF_SIZE);
        p_new_va_data = va_getdata(new_va, &ndata);
        p_data = rs_aval_getdata(cd, atype, aval, &datalen);
        blobsize = (dbe_blobsize_t)datalen;
        filedes = blobmgr_selectfiledes(
                        blobmgr,
                        blobsize,
                        &t);
        blobref.bref_fileno = (uchar)t;
        wblob = dbe_writeblob_init(
                    cd,
                    blobmgr->bm_iomgr,
                    filedes,
                    blobmgr->bm_counter,
                    blobsize,
                    blobmgr->bm_file->f_log,
                    TRUE,       /* Future - Not always TRUE !!!!!!!!!!!!! */
                    trxid,
                    &blobref.bref_blobid);
        blobref.bref_blobsize = blobsize;
        blobref.bref_daddr = SU_DADDR_NULL;

        /* Add the DBE_LOGREC_BLOBSTART_OLD record to log file!
         */
#ifndef SS_NOLOGGING
        if (blobmgr->bm_file->f_log != NULL) {
            /* ignore return value */
            dbe_log_putblobstart(
                blobmgr->bm_file->f_log,
                cd,
                trxid,
                blobref);
        }
#endif /* SS_NOLOGGING */
        ss_dassert(grosslen_of_va > maxvalen);
        ncopied = 0L;
        while (ncopied < blobsize) {
            rc = dbe_writeblob_reach(wblob, &buf, &bufsize);
            if (rc != DBE_RC_SUCC) {
                dynva_free(&new_va);
                dbe_writeblob_abort(wblob);
                return (rc);
            }
            ss_dassert(bufsize != 0);
            ntocopy = (size_t)(blobsize - ncopied);
            if (ntocopy > bufsize) {
                ntocopy = bufsize;
            }
            memcpy(buf, p_data + ncopied, ntocopy);
            ncopied += ntocopy;
            dbe_writeblob_release(wblob, ntocopy);
        }
        ss_dassert(ncopied == blobsize);
        memcpy(
            p_new_va_data,
            p_data,
            maxvalen - VA_LENGTHMAXLEN - DBE_VABLOBREF_SIZE);
        dbe_writeblob_close(wblob);
        blobref.bref_daddr = dbe_writeblob_getstartaddr(wblob);
        ss_dassert(blobref.bref_blobsize == ncopied);
        ss_dassert(dbe_writeblob_getsize(wblob) == ncopied);
        blobref.bref_blobsize = ncopied;
#ifdef SS_DEBUG
        {
            dbe_blobid_t blobid;
            su_daddr_t blobdaddr;

            blobid = dbe_writeblob_getid(wblob);
            ss_dassert(dbe_writeblob_getsize(wblob) == blobsize);
            blobdaddr = dbe_writeblob_getstartaddr(wblob);
            ss_dprintf_1(("dbe_blobmgr_insertaval(): blobid=%lu,blobsize=%lu,daddr=%lu\n",
                (ulong)blobid, (ulong)blobsize, (ulong)blobdaddr));
        }        
#endif
        dbe_writeblob_done(wblob);
        dbe_bref_storetova(&blobref, new_va);
        rs_aval_setva(cd, atype, aval, new_va);
        dynva_free(&new_va);
        ss_dprintf_3(("dbe_blobmgr_insertaval: end, rc = %d\n", rc));
        return (rc);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_getreadstreamofva
 * 
 * Gets (and creates) a bstream object of a given va that
 * contains a blob referece
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      va - in, use
 *              pointer to va object
 *              
 *      p_bstream - out, give
 *              pointer to su_bstream_t* variable
 *              
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_getreadstreamofva(
        dbe_blobmgr_t* blobmgr,
        va_t* va,
        su_bstream_t** p_bstream)
{
        dbe_ret_t rc;
        dbe_vablobref_t blobref;

        ss_dprintf_3(("dbe_blobmgr_getreadstreamofva\n"));
        ss_dassert(blobmgr != NULL);
        ss_dassert(va != NULL);
        ss_dassert(p_bstream != NULL);

        dbe_bref_loadfromva(&blobref, va);
        rc = dbe_blobmgr_getreadstreamofbref(blobmgr, &blobref, p_bstream);
        ss_dprintf_3(("dbe_blobmgr_getreadstreamofva: end, rc = %d\n", rc));
        return (rc);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_getreadstreamofbref
 * 
 * Gets (and creates) a bstream object of a given blob reference
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      blobref - in, use
 *              pointer to blob reference
 *              
 *      p_bstream - out, give
 *              pointer to su_bstream_t* variable
 *              
 * Return value : 
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_getreadstreamofbref(
        dbe_blobmgr_t* blobmgr,
        dbe_vablobref_t* blobref,
        su_bstream_t** p_bstream)
{
        dbe_readblob_t* rblob;
        dbe_filedes_t* filedes;

        ss_dprintf_3(("dbe_blobmgr_getreadstreamofbref\n"));
        ss_dassert(blobmgr != NULL);
        ss_dassert(blobref != NULL);
        ss_dassert(p_bstream != NULL);
        ss_dassert(blobref->bref_fileno == DBE_FNUM_INDEXFILE);

        filedes = dbe_file_getfiledes(blobmgr->bm_file);
        ss_dassert(filedes != NULL);
        ss_dassert(blobref->bref_daddr != SU_DADDR_NULL);
        ss_dassert(blobref->bref_blobsize != DBE_BLOBSIZE_UNKNOWN);
        rblob = dbe_readblob_init(
                blobmgr->bm_iomgr,
                filedes,
                blobref->bref_daddr,
                BLOB_READAHEAD);
        (*p_bstream) = su_bstream_initread(
                            (su_bstream_iofp_t)dbe_readblob_read,
                            (su_bstream_reachfp_t)dbe_readblob_reach,
                            (su_bstream_releasefp_t)dbe_readblob_release,
                            (su_bstream_closefp_t)dbe_readblob_done,
                            (su_bstream_abortfp_t)NULL,
                            (su_bstream_suerrfp_t)NULL,
                            rblob);
        ss_dprintf_3(("dbe_blobmgr_getreadstreamofbref: end, rc = DBE_RC_SUCC\n"));
        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_delete
 * 
 * Deletes a blob identified by a blob reference va
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      va - in, use
 *              pointer to blob reference va
 *              
 * Return value : 
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_delete(
        dbe_blobmgr_t* blobmgr,
        va_t* va)
{
        dbe_vablobref_t blobref;
        dbe_filedes_t* filedes;

        ss_dprintf_3(("dbe_blobmgr_delete\n"));
        ss_dassert(blobmgr != NULL);
        ss_dassert(va != NULL);

        dbe_bref_loadfromva(&blobref, va);
        if (blobmgr->bm_file->f_blobfiles == NULL) {
            ss_dassert(blobref.bref_fileno == 0);
            filedes = blobmgr->bm_file->f_indexfile;
        } else {
#ifdef DBE_FILE_BLOBFILES
            ss_dassert(
                su_pa_indexinuse(
                    blobmgr->bm_file->f_blobfiles,
                    (uint)blobref.bref_fileno));
            filedes = su_pa_getdata(
                            blobmgr->bm_file->f_blobfiles,
                            (uint)blobref.bref_fileno);
#else
            ss_error;
#endif
        }
        ss_dprintf_1(("dbe_blobmgr_delete: blobid = %lu\n", (ulong)blobref.bref_blobid));
        FAKE_BLOBID_REMOVE(blobref.bref_blobid);
        dbe_blob_delete(
            blobmgr->bm_iomgr,
            filedes,
            blobmgr->bm_counter,
            blobref.bref_daddr);
        ss_dprintf_3(("dbe_blobmgr_delete:end, rc == DBE_RC_SUCC\n"));
        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_copy
 * 
 * Copies a blob using fastest available method: cache slot relocation
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      va - in out, use
 *              pointer to the blob reference va
 *
 *      trxid - in
 *          transaction ID
 *              
 * Return value : 
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_copy(
        dbe_blobmgr_t* blobmgr,
        rs_sysi_t* cd,
        va_t* va,
        dbe_trxid_t trxid)
{
        dbe_ret_t rc;
        dbe_vablobref_t blobref;
        dbe_filedes_t* filedes;
        dbe_copyblob_t* copyblob;
        ss_debug(dbe_blobid_t old_blobid;)

        ss_dprintf_3(("dbe_blobmgr_copy\n"));
        ss_dassert(blobmgr != NULL);
        ss_dassert(va != NULL);

        dbe_bref_loadfromva(&blobref, va);
        if (blobmgr->bm_file->f_blobfiles == NULL) {
            ss_dassert(blobref.bref_fileno == 0);
            filedes = blobmgr->bm_file->f_indexfile;
        } else {
#ifdef DBE_FILE_BLOBFILES
            ss_dassert(
                su_pa_indexinuse(
                    blobmgr->bm_file->f_blobfiles,
                    (uint)blobref.bref_fileno));
            filedes = su_pa_getdata(
                            blobmgr->bm_file->f_blobfiles,
                            (uint)blobref.bref_fileno);
#else
            ss_error;
#endif
        }
        ss_debug(old_blobid = blobref.bref_blobid);
        copyblob = dbe_copyblob_init(
                        blobmgr->bm_iomgr,
                        filedes,
                        blobmgr->bm_counter,
                        blobref.bref_daddr,
                        blobmgr->bm_file->f_log,
                        TRUE,   /* Future - Not always TRUE !!!!!!!!!!!!! */
                        trxid,
                        &blobref.bref_blobid,
                        &blobref.bref_blobsize,
                        BLOB_READAHEAD);
        ss_dprintf_1(("dbe_blobmgr_copy: old blobid = %lu, new = %lu\n",
                      old_blobid, blobref.bref_blobid));
#ifndef SS_NOLOGGING
        if (blobmgr->bm_file->f_log != NULL) {
            /* ignore return value */
            dbe_log_putblobstart(blobmgr->bm_file->f_log, cd, trxid, blobref);
        }
#endif /* SS_NOLOGGING */
        rc = dbe_copyblob_copy(copyblob, &blobref.bref_daddr);
        ss_dprintf_1(("dbe_blobmgr_copy(): blobid=%lu,blobsize=%lu,daddr=%lu\n",
            blobref.bref_blobid,
            blobref.bref_blobsize,
            blobref.bref_daddr));
        dbe_copyblob_done(copyblob);
        if (rc == DBE_RC_SUCC) {
            dbe_bref_storetova(&blobref, va);
        }
        ss_dprintf_3(("dbe_blobmgr_copy: end, rc = %d\n", rc));
        return (rc);
}

/*##**********************************************************************\
 * 
 *              dbe_blobmgr_readtoaval
 * 
 * Reads a (small) BLOB to aval
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *              client data
 *              
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      va - in, use
 *              blob reference va
 *              
 *      atype - in, use
 *              attribute type
 *              
 *      aval - in out, use
 *              attribute value, where the blob is to be stored
 *              
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      error code when failure
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobmgr_readtoaval(
        rs_sysinfo_t* cd,
        dbe_blobmgr_t* blobmgr,
        va_t* va,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        dbe_readblob_t* rblob;
        dbe_vablobref_t blobref;
        dbe_filedes_t* filedes;
        size_t s;
        bool succp;
        char* p_data;
        ulong datasize_available;

        ss_dassert(blobmgr != NULL);
        ss_dassert(va != NULL);

        dbe_bref_loadfromva(&blobref, va);
        filedes = dbe_file_getfiledes(blobmgr->bm_file);
        ss_dassert(filedes != NULL);
        ss_dassert(blobref.bref_daddr != SU_DADDR_NULL);
        ss_dassert(blobref.bref_blobsize != DBE_BLOBSIZE_UNKNOWN);
        rblob = dbe_readblob_init(
                    blobmgr->bm_iomgr,
                    filedes,
                    blobref.bref_daddr,
                    BLOB_READAHEAD);
        ss_dprintf_1(("dbe_blobmgr_readtoaval:blobid=%d, blobsize=%ld, rblobsize=%ld\n",
            blobref.bref_blobid, blobref.bref_blobsize, dbe_readblob_getsize(rblob)));
        ss_dassert(dbe_readblob_getsize(rblob) == blobref.bref_blobsize);
#ifdef SS_UNICODE_DATA
        succp = rs_aval_setbdata_ext(cd, atype, aval, NULL, (uint)blobref.bref_blobsize, NULL);
#else /* SS_UNICODE_DATA */
        succp = rs_aval_setdata(cd, atype, aval, NULL, (uint)blobref.bref_blobsize);
#endif /*  */
        ss_dassert(succp);
        p_data = rs_aval_getdata(cd, atype, aval, &datasize_available);
        ss_dassert(datasize_available == blobref.bref_blobsize
                   || (datasize_available < blobref.bref_blobsize
                       && succp == RSAVR_TRUNCATION));
        s = dbe_readblob_read(
                rblob,
                p_data,
                (size_t)datasize_available);
        ss_dassert(s == datasize_available);
        dbe_readblob_done(rblob);
        ss_dprintf_3(("dbe_blobmgr_readtoaval: end, rc == DBE_RC_SUCC\n"));
        return (DBE_RC_SUCC);
}


struct dbe_blobwritestream_st {
        dbe_writeblob_t*    bws_blob;
        dynva_t             bws_dynva;
        va_index_t          bws_vadatanetlen;
        char*               bws_reachbuf;
        uint                bws_fileidx;
        dbe_blobsize_t      bws_blobsize;
        dbe_trxid_t         bws_trxid;
        dbe_vablobref_t     bws_blobref;
};

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_init
 * 
 * Creates a BLOB Write Stream
 * 
 * Parameters : 
 * 
 *      blobmgr - in, use
 *              pointer to the blob manager object
 *              
 *      blobsize - in
 *              blob size or DBE_BLOBSIZE_UNKNOWN
 *              
 *      maxvalen - in
 *              max. gross length of Blob reference va
 *              
 *      trxid - in
 *              Trx id of transactions that writes the blob
 *              
 * Return value - give :
 *      pointer to created BLOB write stream
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_blobwritestream_t* dbe_blobwritestream_init(
        dbe_blobmgr_t* blobmgr,
        rs_sysi_t* cd,
        dbe_blobsize_t blobsize,
        va_index_t maxvalen,
        dbe_trxid_t trxid)
{
        dbe_filedes_t* filedes;
        dbe_blobwritestream_t* stream;
        dbe_blobid_t blobid;

        stream = SSMEM_NEW(dbe_blobwritestream_t);
        filedes = blobmgr_selectfiledes(
                        blobmgr,
                        blobsize,
                        &stream->bws_fileidx);
        stream->bws_dynva = NULL;
        stream->bws_vadatanetlen = maxvalen - DBE_BLOBVAHEADERSIZE;
        stream->bws_reachbuf = NULL;
        stream->bws_blobsize = blobsize;
        stream->bws_trxid = trxid;
        stream->bws_blob =
            dbe_writeblob_init(
                cd,
                blobmgr->bm_iomgr,
                filedes,
                blobmgr->bm_counter,
                blobsize,
                blobmgr->bm_file->f_log,
                TRUE,   /* Future - Not always TRUE !!!!!!!!!!!!! */
                trxid,
                &blobid);
        dbe_bref_setfileno(&stream->bws_blobref, (ss_byte_t)stream->bws_fileidx);
        dbe_bref_setblobid(&stream->bws_blobref, blobid);
        dbe_bref_setblobsize(&stream->bws_blobref, blobsize);
        dbe_bref_setdaddr(&stream->bws_blobref, SU_DADDR_NULL);

#ifndef SS_NOLOGGING
        /* Add the DBE_LOGREC_BLOBSTART_OLD record to log file!
         */
        if (blobmgr->bm_file->f_log!= NULL) {
            /* ignore return value */
            dbe_log_putblobstart(
                blobmgr->bm_file->f_log, 
                cd,
                trxid, 
                stream->bws_blobref);
        }
#endif /* SS_NOLOGGING */
        return (stream);
}

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_done
 * 
 * Closes and deletes BLOB write stream object. All data written to it
 * is preserved, ie. this is the succesful completion of blob write.
 * 
 * Parameters : 
 * 
 *      stream - in, take
 *              pointer to BLOB write stream object
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobwritestream_done(
        dbe_blobwritestream_t* stream)
{
        ss_dassert(stream != NULL);
        dbe_writeblob_done(stream->bws_blob);
        if (stream->bws_dynva != NULL) {
            dynva_free(&stream->bws_dynva);
        }
        SsMemFree(stream);
}

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_abort
 * 
 * Discards all data written into a BLOB write stream and deletes the stream
 * object
 * 
 * Parameters : 
 * 
 *      stream - in, take
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
void dbe_blobwritestream_abort(
        dbe_blobwritestream_t* stream)
{
        ss_dassert(stream != NULL);
        dbe_writeblob_abort(stream->bws_blob);
        if (stream->bws_dynva != NULL) {
            dynva_free(&stream->bws_dynva);
        }
        SsMemFree(stream);
}

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_write
 * 
 * Writes a bufferful of data into BLOB write stream
 * 
 * Parameters : 
 * 
 *      stream - in out, use
 *              pointer to BLOB write stream object
 *              
 *      buf - in, use
 *              pointer to data buffer
 *              
 *      bufsize - in
 *              # of bytes to write from buf
 *              
 *      p_written - out
 *              pointer to variable where the # of bytes actually written
 *          will be stored
 *              
 * Return value :
 *      RC_SU_BSTREAM_SUCC when succesful or
 *      ERR_SU_BSTREAM_ERROR when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
#if 0 /* Removed by Pete 1995-08-29 */
dbe_ret_t dbe_blobwritestream_write(
        dbe_blobwritestream_t* stream,
        char* buf,
        size_t bufsize,
        size_t* p_written)
{
        size_t written;

        if (stream->bws_dynva == NULL) {
            ss_dassert(bufsize >= stream->bws_vadatanetlen);
            dynva_setblobdata(
                &stream->bws_dynva,
                buf, stream->bws_vadatanetlen,
                NULL, DBE_VABLOBREF_SIZE);
        }
        written = dbe_writeblob_write(stream->bws_blob, buf, bufsize);
        if (bufsize != written) {
            return (ERR_SU_BSTREAM_ERROR);
        }
        if (p_written != NULL) {
            *p_written = written;
        }
        return (RC_SU_BSTREAM_SUCC);
}
#endif

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_reach
 * 
 * Reaches a buffer for writing data into the BLOB write stream
 * 
 * Parameters : 
 * 
 *      stream - in out, use
 *              pointer to BLOB write stream object
 *
 *      pp_buf - out, ref
 *          pointer to pointer to write buffer
 *
 *      p_avail - out
 *              pointer to variable where the # of bytes available for
 *          writing will be stored
 *              
 * Return value :
 *      DBE_RC_SUCC when OK or error code when failed
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
dbe_ret_t dbe_blobwritestream_reach(
        dbe_blobwritestream_t* stream,
        char** pp_buf,
        size_t* p_avail)
{
        dbe_ret_t rc;

        ss_dassert(stream != NULL);
        ss_dassert(p_avail != NULL);
        rc = dbe_writeblob_reach(stream->bws_blob, pp_buf, p_avail);
        stream->bws_reachbuf = *pp_buf;
        ss_debug(
            if (rc == DBE_RC_SUCC) {
                ss_assert(stream->bws_dynva != NULL
                        || *p_avail >= stream->bws_vadatanetlen);
            }
        );
        return (rc);
}

/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_release
 * 
 * Releases a write buffer gotten with dbe_blobwritestream_reach().
 * 
 * Parameters : 
 * 
 *      stream - in out, use
 *              pointer to BLOB write stream object
 *              
 *      n_written - in
 *              # of bytes written into the buffer
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobwritestream_release(
        dbe_blobwritestream_t* stream,
        size_t n_written)
{
        ss_dassert(stream != NULL);
        if (stream->bws_dynva == NULL) {
            ss_dassert(stream->bws_vadatanetlen <= n_written)
            dynva_setblobdata(
                &stream->bws_dynva,
                stream->bws_reachbuf, stream->bws_vadatanetlen,
                NULL, DBE_VABLOBREF_SIZE);
        }
        dbe_writeblob_release(stream->bws_blob, n_written);
}


/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_close
 * 
 * Closes BLOB write stream
 * 
 * Parameters : 
 * 
 *      stream - in out, use
 *              pointer to BLOB write stream object
 *              
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_blobwritestream_close(
        dbe_blobwritestream_t* stream)
{
        dbe_blobsize_t blobsize;
        su_daddr_t blobdaddr;
        ss_debug(dbe_blobid_t blobid);

        ss_dassert(stream != NULL);
        dbe_writeblob_close(stream->bws_blob);
        if (stream->bws_dynva != NULL) {
            blobsize = dbe_writeblob_getsize(stream->bws_blob);
            blobdaddr = dbe_writeblob_getstartaddr(stream->bws_blob);
            ss_debug(blobid = dbe_writeblob_getid(stream->bws_blob));
            ss_dprintf_1(("dbe_blobwritestream_close(): blobid=%lu,blobsize=%lu,daddr=%lu\n",
                (ulong)blobid, (ulong)blobsize, (ulong)blobdaddr));
            dbe_bref_setblobsize(&stream->bws_blobref, blobsize);
            dbe_bref_setdaddr(&stream->bws_blobref, blobdaddr);
            dbe_bref_storetova(&stream->bws_blobref, stream->bws_dynva);
        }
}


/*##**********************************************************************\
 * 
 *              dbe_blobwritestream_getva
 * 
 * Returns reference to the BLOB reference va of the BLOB write stream.
 * 
 * Parameters : 
 * 
 *      stream - in, use
 *              
 *              
 * Return value - ref :
 *      pointer to va object
 * 
 * Comments :
 *      The va is NOT complete until the BLOB write stream has been
 *      closed. The pointer reference, however, remains valid if it
 *      is non-null, ie. the va contents will be updated when the
 *      write stream is closed.
 * 
 * Globals used : 
 * 
 * See also : 
 */
va_t* dbe_blobwritestream_getva(
        dbe_blobwritestream_t* stream)
{
        ss_dassert(stream != NULL);
        return (stream->bws_dynva);
}

