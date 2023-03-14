/*************************************************************************\
**  source       * dbe8srec.c
**  directory    * dbe
**  description  * Start record for database file
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

#include <ssstring.h>

#include <ssdebug.h>

#include <su0icvt.h>
#include "dbe8srec.h"

/*##**********************************************************************\
 * 
 *		dbe_srec_puttodisk
 * 
 * Puts start record to disk image buffer
 * 
 * Parameters : 
 * 
 *	sr - in, use
 *		pointer to start record object
 *		
 *	dbufpos - out, use
 *		pointer to correct position in disk buffer
 *		
 * Return value : 
 *      pointer to next position after start record in disk buffer 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
char* dbe_srec_puttodisk(dbe_startrec_t* sr, char* dbufpos)
{
        ss_uint4_t blobg2idlo;
        ss_uint4_t blobg2idhi;
        FOUR_BYTE_T tnum_tmp;
        ss_dprintf_1(("dbe_srec_puttodisk\n"));

        SS_UINT4_STORETODISK(dbufpos, sr->sr_cpnum);
        dbufpos += sizeof(sr->sr_cpnum);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_bonsairoot);
        dbufpos += sizeof(sr->sr_bonsairoot);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_permroot);
        dbufpos += sizeof(sr->sr_permroot);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_freelistaddr);
        dbufpos += sizeof(sr->sr_freelistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_chlistaddr);
        dbufpos += sizeof(sr->sr_chlistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_cplistaddr);
        dbufpos += sizeof(sr->sr_cplistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_trxlistaddr);
        dbufpos += sizeof(sr->sr_trxlistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_filesize);
        dbufpos += sizeof(sr->sr_filesize);

        SS_UINT4_STORETODISK(dbufpos, (sr->sr_maxtrxnum));
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_maxtrxnum_res);
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, (sr->sr_committrxnum));
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_committrxnum_res);
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, (sr->sr_mergetrxnum));
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_mergetrxnum_res);
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, (sr->sr_trxid));
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_trxid_res);
        dbufpos += sizeof(ss_uint4_t);
        tnum_tmp = rs_tuplenum_getlsl(&sr->sr_tuplenum);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
        tnum_tmp = rs_tuplenum_getmsl(&sr->sr_tuplenum);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_attrid);
        dbufpos += sizeof(sr->sr_attrid);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_keyid);
        dbufpos += sizeof(sr->sr_keyid);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_userid);
        dbufpos += sizeof(sr->sr_userid);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_logfnum);
        dbufpos += sizeof(sr->sr_logfnum);
        blobg2idlo = DBE_BLOBG2ID_GETLEASTSIGNIFICANTUINT4(sr->sr_blobg2id);
        blobg2idhi = DBE_BLOBG2ID_GETMOSTSIGNIFICANTUINT4(sr->sr_blobg2id);
        SS_UINT4_STORETODISK(dbufpos, blobg2idlo);
        dbufpos += sizeof(blobg2idlo);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_mergectr);
        dbufpos += sizeof(sr->sr_mergectr);
        tnum_tmp = rs_tuplenum_getlsl(&sr->sr_tupleversion);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
        tnum_tmp = rs_tuplenum_getmsl(&sr->sr_tupleversion);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_stmttrxlistaddr);
        dbufpos += sizeof(sr->sr_stmttrxlistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_sequencelistaddr);
        dbufpos += sizeof(sr->sr_sequencelistaddr);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_mmiroot);
        dbufpos += sizeof(sr->sr_mmiroot);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_rtrxlistaddr);
        dbufpos += sizeof(sr->sr_rtrxlistaddr);
#ifdef SS_SYNC
        SS_UINT4_STORETODISK(dbufpos, sr->sr_syncmsgid);
        dbufpos += sizeof(sr->sr_syncmsgid);
        tnum_tmp = rs_tuplenum_getlsl(&sr->sr_synctupleversion);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
        tnum_tmp = rs_tuplenum_getmsl(&sr->sr_synctupleversion);
        SS_UINT4_STORETODISK(dbufpos, tnum_tmp);
        dbufpos += sizeof(tnum_tmp);
#endif /* SS_SYNC */
        SS_UINT4_STORETODISK(dbufpos, blobg2idhi);
        dbufpos += sizeof(blobg2idhi);

#ifdef SS_MME
        SS_UINT4_STORETODISK(dbufpos, sr->sr_firstmmeaddrpage);
        dbufpos += sizeof(sr->sr_firstmmeaddrpage);
#endif
        SS_UINT4_STORETODISK(dbufpos, (sr->sr_storagetrxnum));
        dbufpos += sizeof(ss_uint4_t);
        SS_UINT4_STORETODISK(dbufpos, sr->sr_storagetrxnum_res);
        dbufpos += sizeof(ss_uint4_t);

        memset(dbufpos, 0, sizeof(sr->sr_reserved));
        dbufpos += sizeof(sr->sr_reserved);

        ss_dprintf_2(("sr->sr_cpnum = %ld\n", sr->sr_cpnum));
        ss_dprintf_2(("sr->sr_bonsairoot = %ld\n", sr->sr_bonsairoot));
        ss_dprintf_2(("sr->sr_permroot = %ld\n", sr->sr_permroot));
        ss_dprintf_2(("sr->sr_freelistaddr = %ld\n", sr->sr_freelistaddr));
        ss_dprintf_2(("sr->sr_chlistaddr = %ld\n", sr->sr_chlistaddr));
        ss_dprintf_2(("sr->sr_cplistaddr = %ld\n", sr->sr_cplistaddr));
        ss_dprintf_2(("sr->sr_trxlistaddr = %ld\n", sr->sr_trxlistaddr));
        ss_dprintf_2(("sr->sr_filesize = %ld\n", sr->sr_filesize));
        ss_dprintf_2(("sr->sr_maxtrxnum = %ld,%ld\n", sr->sr_maxtrxnum, sr->sr_maxtrxnum_res));
        ss_dprintf_2(("sr->sr_committrxnum = %ld,%ld\n", sr->sr_committrxnum, sr->sr_committrxnum_res));
        ss_dprintf_2(("sr->sr_mergetrxnum = %ld,%ld\n", sr->sr_mergetrxnum, sr->sr_mergetrxnum_res));
        ss_dprintf_2(("sr->sr_storagetrxnum = %ld,%ld\n", sr->sr_storagetrxnum, sr->sr_storagetrxnum_res));
        ss_dprintf_2(("sr->sr_trxid = %ld,%ld\n", sr->sr_trxid, sr->sr_trxid_res));
        ss_dprintf_2(("sr->sr_tuplenum = 0x%08lX%08lX\n",
            rs_tuplenum_getmsl(&sr->sr_tuplenum),
            rs_tuplenum_getlsl(&sr->sr_tuplenum)));
        ss_dprintf_2(("sr->sr_attrid = %ld\n", sr->sr_attrid));
        ss_dprintf_2(("sr->sr_keyid = %ld\n", sr->sr_keyid));
        ss_dprintf_2(("sr->sr_userid = %ld\n", sr->sr_userid));
        ss_dprintf_2(("sr->sr_logfnum = %ld\n", sr->sr_logfnum));
        ss_dprintf_2(("sr->sr_blobg2id = 0x%08lX%08lX\n",
                      (long)blobg2idhi, (long)blobg2idlo));
        ss_dprintf_2(("sr->sr_mergectr = %ld\n", sr->sr_mergectr));
        ss_dprintf_2(("sr->sr_tupleversion = 0x%08lX%08lX\n",
            rs_tuplenum_getmsl(&sr->sr_tupleversion),
            rs_tuplenum_getlsl(&sr->sr_tupleversion)));
        ss_dprintf_2(("sr->sr_stmttrxlistaddr = %ld\n", sr->sr_stmttrxlistaddr));
        ss_dprintf_2(("sr->sr_sequencelistaddr = %ld\n", sr->sr_sequencelistaddr));
        ss_dprintf_2(("sr->sr_mmiroot = %ld\n", sr->sr_mmiroot));
        ss_dprintf_2(("sr->sr_rtrxlistaddr = %ld\n", sr->sr_rtrxlistaddr));
#ifdef SS_MME
        ss_dprintf_2(("sr->sr_firstmmeaddrpage = %ld\n", sr->sr_firstmmeaddrpage));
#endif
        return (dbufpos);
}

/*##**********************************************************************\
 * 
 *		dbe_srec_getfromdisk
 * 
 * Gets the start record from disk image format
 * 
 * Parameters : 
 * 
 *	sr - out, use
 *		pointer to start record
 *		
 *	dbufpos - in, use
 *		pointer to start record position in disk buffer
 *		
 * Return value :
 *      pointer to next position after start record in disk buffer 
 *                  
 * Limitations  : 
 * 
 * Globals used : 
 */
char *dbe_srec_getfromdisk(dbe_startrec_t* sr, char* dbufpos)
{
        ss_uint4_t blobg2idlo;
        ss_uint4_t blobg2idhi;
        FOUR_BYTE_T tnum_lsl;
        FOUR_BYTE_T tnum_msl;
        ss_debug(char* orig_dbufpos = dbufpos;)
        ss_dprintf_1(("dbe_srec_getfromdisk\n"));

        ss_dassert(sizeof(dbe_startrec_t) == 256);

        sr->sr_cpnum = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_cpnum);
        sr->sr_bonsairoot = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_bonsairoot);
        sr->sr_permroot = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_permroot);
        sr->sr_freelistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_freelistaddr);
        sr->sr_chlistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_chlistaddr);
        sr->sr_cplistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_cplistaddr);
        sr->sr_trxlistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_trxlistaddr);
        sr->sr_filesize = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_filesize);

        sr->sr_maxtrxnum = (SS_UINT4_LOADFROMDISK(dbufpos));
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_maxtrxnum_res = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_committrxnum = (SS_UINT4_LOADFROMDISK(dbufpos));
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_committrxnum_res = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_mergetrxnum = (SS_UINT4_LOADFROMDISK(dbufpos));
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_mergetrxnum_res = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_trxid = (SS_UINT4_LOADFROMDISK(dbufpos));
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_trxid_res = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(ss_uint4_t);
        tnum_lsl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_lsl);
        tnum_msl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_msl);
        rs_tuplenum_ulonginit(&sr->sr_tuplenum, tnum_msl, tnum_lsl);
        sr->sr_attrid = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_attrid);
        sr->sr_keyid = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_keyid);
        sr->sr_userid = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_userid);
        sr->sr_logfnum = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_logfnum);
        blobg2idlo = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(blobg2idlo);
        sr->sr_mergectr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_mergectr);
        tnum_lsl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_lsl);
        tnum_msl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_msl);
        rs_tuplenum_ulonginit(&sr->sr_tupleversion, tnum_msl, tnum_lsl);
        sr->sr_stmttrxlistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_stmttrxlistaddr);
        sr->sr_sequencelistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_sequencelistaddr);
        sr->sr_mmiroot = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_mmiroot);
        sr->sr_rtrxlistaddr = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_rtrxlistaddr);
#ifdef SS_SYNC
        sr->sr_syncmsgid = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_syncmsgid);
        tnum_lsl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_lsl);
        tnum_msl = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(tnum_msl);
        rs_tuplenum_ulonginit(&sr->sr_synctupleversion, tnum_msl, tnum_lsl);
#endif
        blobg2idhi = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(blobg2idhi);
        DBE_BLOBG2ID_SET2UINT4S(&(sr->sr_blobg2id), blobg2idhi, blobg2idlo);

#ifdef SS_MME
        sr->sr_firstmmeaddrpage = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(sr->sr_firstmmeaddrpage);
#endif
        sr->sr_storagetrxnum = (SS_UINT4_LOADFROMDISK(dbufpos));
        dbufpos += sizeof(ss_uint4_t);
        sr->sr_storagetrxnum_res = SS_UINT4_LOADFROMDISK(dbufpos);
        dbufpos += sizeof(ss_uint4_t);

        memset(sr->sr_reserved, 0, sizeof(sr->sr_reserved));
        dbufpos += sizeof(sr->sr_reserved);

        ss_rc_dassert(dbufpos - orig_dbufpos == 256, (int)(dbufpos - orig_dbufpos));

        FAKE_CODE_BLOCK(
            FAKE_DBE_INCCOUNTERS,
            {
                ulong tmp1;
                ulong tmp2;
                sr->sr_trxid = (sr->sr_trxid + 5000);
                tmp1 = rs_tuplenum_getmsl(&sr->sr_tuplenum);
                tmp2 = rs_tuplenum_getlsl(&sr->sr_tuplenum);
                tmp2 += 5000;
                rs_tuplenum_ulonginit(&sr->sr_tuplenum, tmp1, tmp2);
                sr->sr_attrid += 5000;
                sr->sr_keyid += 5000;
                sr->sr_userid += 5000;
                DBE_BLOBG2ID_ADDASSIGN_UINT2(&(sr->sr_blobg2id), (ss_uint2_t)5000);
                tmp1 = rs_tuplenum_getmsl(&sr->sr_tupleversion);
                tmp2 = rs_tuplenum_getlsl(&sr->sr_tupleversion);
                tmp2 += 5000;
                rs_tuplenum_ulonginit(&sr->sr_tupleversion, tmp1, tmp2);
                
            }
        );

        ss_dprintf_2(("sr->sr_cpnum = %ld\n", sr->sr_cpnum));
        ss_dprintf_2(("sr->sr_bonsairoot = %ld\n", sr->sr_bonsairoot));
        ss_dprintf_2(("sr->sr_permroot = %ld\n", sr->sr_permroot));
        ss_dprintf_2(("sr->sr_freelistaddr = %ld\n", sr->sr_freelistaddr));
        ss_dprintf_2(("sr->sr_chlistaddr = %ld\n", sr->sr_chlistaddr));
        ss_dprintf_2(("sr->sr_cplistaddr = %ld\n", sr->sr_cplistaddr));
        ss_dprintf_2(("sr->sr_trxlistaddr = %ld\n", sr->sr_trxlistaddr));
        ss_dprintf_2(("sr->sr_filesize = %ld\n", sr->sr_filesize));
        ss_dprintf_2(("sr->sr_maxtrxnum = %ld\n", sr->sr_maxtrxnum));
        ss_dprintf_2(("sr->sr_committrxnum = %ld\n", sr->sr_committrxnum));
        ss_dprintf_2(("sr->sr_mergetrxnum = %ld\n", sr->sr_mergetrxnum));
        ss_dprintf_2(("sr->sr_storagetrxnum = %ld\n", sr->sr_storagetrxnum));
        ss_dprintf_2(("sr->sr_trxid = %ld\n", sr->sr_trxid));
        ss_dprintf_2(("sr->sr_tuplenum = 0x%08lX%08lX\n",
            rs_tuplenum_getmsl(&sr->sr_tuplenum),
            rs_tuplenum_getlsl(&sr->sr_tuplenum)));
        ss_dprintf_2(("sr->sr_attrid = %ld\n", sr->sr_attrid));
        ss_dprintf_2(("sr->sr_keyid = %ld\n", sr->sr_keyid));
        ss_dprintf_2(("sr->sr_userid = %ld\n", sr->sr_userid));
        ss_dprintf_2(("sr->sr_logfnum = %ld\n", sr->sr_logfnum));
        ss_dprintf_2(("sr->sr_blobg2id = 0x%08lX%08lX\n",
                      (long)blobg2idhi, (long)blobg2idlo));
        ss_dprintf_2(("sr->sr_mergectr = %ld\n", sr->sr_mergectr));
        ss_dprintf_2(("sr->sr_tupleversion = 0x%08lX%08lX\n",
            rs_tuplenum_getmsl(&sr->sr_tupleversion),
            rs_tuplenum_getlsl(&sr->sr_tupleversion)));
        ss_dprintf_2(("sr->sr_stmttrxlistaddr = %ld\n", sr->sr_stmttrxlistaddr));
        ss_dprintf_2(("sr->sr_sequencelistaddr = %ld\n", sr->sr_sequencelistaddr));
        ss_dprintf_2(("sr->sr_mmiroot = %ld\n", sr->sr_mmiroot));
        ss_dprintf_2(("sr->sr_rtrxlistaddr = %ld\n", sr->sr_rtrxlistaddr));
#ifdef SS_MME
        ss_dprintf_2(("sr->sr_firstmmeaddrpage = %ld\n", sr->sr_firstmmeaddrpage));
#endif
                                          
        return (dbufpos);
}
