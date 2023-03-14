/*************************************************************************\
**  source       * tab0blob.c
**  directory    * tab
**  description  * Blob routines.
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

#include <ssc.h>
#include <ssdebug.h>

#include <dbe0db.h>
#include <dbe0bstr.h>

#include "tab0blob.h"

/*##**********************************************************************\
 * 
 *		tb_blob_readsmallblobstotvalwithinfo
 * 
 * Reads small BLOBs to tval as normal avals, the big
 * ones are preserved as a reference.
 * 
 * Parameters : 
 * 
 *      cd - in, use
 *          client data
 *		
 *      ttype - in, use
 *          tuple type
 *		
 *      tval - in out, use
 *          tuple value
 *		
 *      smallblobsizemax - in
 *          max size of blob that will be read to aval
 *		
 *      p_nblobs_read - out, use
 *          pointer to variable telling the number of blobs
 *          read in (ie. count of blobs the size of which is
 *          below or equal to smallblobsizemax)
 *
 *      p_nblobs_total - out, use
 *          pointer t variable telling the number of blob attributes
 *  
 * Return value :
 *      SU_SUCCESS when successful
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_ret_t tb_blob_readsmallblobstotvalwithinfo(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        dbe_blobsize_t smallblobsizemax,
        uint* p_nblobs_read,
        uint* p_nblobs_total)
{
#ifndef SS_NOBLOB
        dbe_ret_t rc;
        int i;
        dbe_blobsize_t blobsize;
        rs_aval_t* aval;
        rs_atype_t* atype;

        ss_dprintf_1(("tb_blob_readsmallblobstotvalwithinfo\n"));

        *p_nblobs_read = *p_nblobs_total = 0;
        i = -1; /* Initial value for scanning. */
        while (rs_tval_scanblobs(cd, ttype, tval, &i)) {
            atype = rs_ttype_atype(cd, ttype, i);
            aval = rs_tval_aval(cd, ttype, tval, i);
            ss_dassert(rs_aval_isblob(cd, atype, aval));
            blobsize = dbe_blobaval_size(cd, atype, aval);
            ss_dprintf_2(("tb_blob_readsmallblobstotvalwithinfo:blob at id %d, size %ld\n", i, (long)blobsize));

            (*p_nblobs_total)++;
            if (blobsize <= smallblobsizemax) {
                ss_dprintf_2(("tb_blob_readsmallblobstotvalwithinfo:read blob\n"));
                (*p_nblobs_read)++;
                rc = dbe_blobaval_read(
                        cd,
                        rs_sysi_db(cd),
                        atype,
                        aval);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
            }
            /* else only reference to blob is sent */
        }
        return (SU_SUCCESS);
#else /* SS_NOBLOB */
        return(DBE_ERR_FAILED);
#endif /* SS_NOBLOB */
}

/*##**********************************************************************\
 * 
 *		tb_blob_readsmallblobstotval
 * 
 * Reads small BLOBs to tval as normal avals, the big
 * ones are preserved as a reference.
 * 
 * Parameters : 
 * 
 *	cd - in, use
 *		client data
 *		
 *	ttype - in, use
 *		tuple type
 *		
 *	tval - in out, use
 *		tuple value
 *		
 *	smallblobsizemax - in
 *		max size of blob that will be read to aval
 *		
 * Return value :
 *      SU_SUCCESS when successful
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
su_ret_t tb_blob_readsmallblobstotval(
        rs_sysinfo_t* cd,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        dbe_blobsize_t smallblobsizemax)
{
#ifndef SS_NOBLOB
        su_ret_t rc;
        uint nblobs_read;
        uint nblobs_total;

        rc = tb_blob_readsmallblobstotvalwithinfo(
                cd, 
                ttype, tval, 
                smallblobsizemax, &nblobs_read, &nblobs_total);
        return (rc);
#else /* SS_NOBLOB */
        return(DBE_ERR_FAILED);
#endif /* SS_NOBLOB */
}

bool tb_blob_loadblobtoaval(
        void* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        size_t sizelimit)
{
#ifndef SS_NOBLOB
        dbe_blobsize_t blobsize;

        ss_dassert(rs_aval_isblob(cd, atype, aval));
        blobsize = dbe_blobaval_size(cd, atype, aval);
        if (blobsize <= sizelimit) {
            dbe_ret_t rc;
            dbe_db_t* db = rs_sysi_db(cd);
            rc = dbe_blobaval_read(cd, db, atype, aval);
            ss_rc_dassert(rc == DBE_RC_SUCC, rc);
            return (rc == DBE_RC_SUCC);
        }
#endif /* SS_NOBLOB */
        return (FALSE);
}




