/*************************************************************************\
**  source       * dbe7hdr.c
**  directory    * dbe
**  description  * Database file header information management
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
#include <ssmem.h>
#include <ssstring.h>
#include <sstime.h>
#include <sswcs.h>
#include <ssutf.h>
#include <su0icvt.h>
#include <su0vers.h>

#ifdef SS_LICENSEINFO_V3
#include <su0li3.h>
#else /* SS_LICENSEINFO_V3 */
#include <su0li2.h>
#endif /* SS_LICENSEINFO_V3 */

#include <su0crc32.h>
#include <su0sdefs.h>
#include <ssfile.h>
#include "dbe7cfg.h"
#include "dbe7hdr.h"
#include "dbe9bhdr.h"

extern bool dbefile_diskless;

#define DBE_HEADER_SIZE    512      /* minimum block size for header */

/* The database header record contents
** Note: minimum database file block size = DBE_HEADER_SIZE bytes
*/

struct dbe_header_st {
        dbe_blocktype_t     hdr_blktype;    /* 1 byte */
        dbe_cpnum_t         hdr_cpnum;      /* 4 bytes */
        dbe_hdr_chknum_t    hdr_chknum1;    /* 4 bytes */
        ss_uint1_t          hdr_dbstate;    /* 1 byte, dbe_dbstate_t */
        dbe_hdr_blocksize_t hdr_blocksize;  /* 4 bytes file block size */
        dbe_startrec_t      hdr_startrec;   /* 256 bytes start record */
        ss_uint2_t          hdr_headervers; /* 2 bytes */
        ss_uint2_t          hdr_solidvers;  /* 2 bytes */
        ss_uint2_t          hdr_dbvers;     /* 2 bytes */
        ss_uint4_t          hdr_creatime;   /* 4 bytes = SsTimeT */
        ss_uint4_t          hdr_creatimcrc; /* 4 bytes = 32 bit CRC */
        ss_char2_t          hdr_defcatalog[
                                SU_MAXDEFCATALOGLEN + 1]; /* 80 bytes */
        ss_uint1_t          hdr_hsbcopy;    /* 1 bytes */
        ss_uint4_t          hdr_hsbtime;    /* 4 bytes = SsTimeT */
        ss_uint1_t          hdr_cryptalg;   /* 1 byte - encyption alg. */
        ss_uint1_t          hdr_cryptkey[32]; /* 32 bytes - encyption key */
        ss_uint1_t          hdr_cryptchk[32]; /* 32 bytes - encyption check record. */
        ss_uint4_t          hdr_flags;        /* 4 bytes */
        char                hdr_reserved[256 - 168]; /* bytes */

        /* physical gap */
        dbe_hdr_chknum_t    hdr_chknum2;    /* 4 bytes */
};

/*##**********************************************************************\
 * 
 *		dbe_header_init
 * 
 * Creates a database header object
 * 
 * Parameters : 
 * 
 *	blocksize - in
 *		database file block size in bytes
 *		
 * Return value - give :
 *      pointer to header object
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_header_t* dbe_header_init(size_t blocksize)
{
        dbe_header_t* dbe_header;

        dbe_header = SSMEM_NEW(dbe_header_t);
        dbe_header->hdr_blktype = DBE_BLOCK_DBHEADER;
        dbe_header->hdr_cpnum = 1L;
        dbe_header->hdr_chknum1 = dbe_header->hdr_chknum2 = 1L;
        dbe_header->hdr_dbstate = DBSTATE_CRASHED;
        dbe_header->hdr_blocksize = blocksize;

        ss_rc_dassert(sizeof(dbe_header->hdr_startrec) == 256, sizeof(dbe_header->hdr_startrec));
        memset(&dbe_header->hdr_startrec, '\0', sizeof(dbe_header->hdr_startrec));
        dbe_header->hdr_startrec.sr_cpnum = dbe_header->hdr_cpnum;
        dbe_header->hdr_startrec.sr_bonsairoot = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_permroot = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_mmiroot = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_freelistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_chlistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_cplistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_trxlistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_filesize = DBE_INDEX_HEADERSIZE;
        dbe_header->hdr_startrec.sr_stmttrxlistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_sequencelistaddr = SU_DADDR_NULL;
        dbe_header->hdr_startrec.sr_rtrxlistaddr = SU_DADDR_NULL;

        memset(dbe_header->hdr_startrec.sr_reserved,
               0,
               sizeof(dbe_header->hdr_startrec.sr_reserved));

        dbe_header->hdr_headervers = SU_DBHEADER_VERSNUM;
        dbe_header->hdr_solidvers = (ss_uint2_t)SS_SERVER_VERSNUM;
        dbe_header->hdr_dbvers = SU_DBFILE_VERSNUM;
        dbe_header->hdr_creatime = SsTime(NULL);

        ss_pprintf_1(("dbe_header_init:dbe_header_setcreatime %ld\n", dbe_header->hdr_creatime));

        dbe_header->hdr_creatimcrc = su_lxc_calcctc(dbe_header->hdr_creatime);
        memset(dbe_header->hdr_defcatalog, 0,
               sizeof(dbe_header->hdr_defcatalog));
        dbe_header->hdr_hsbcopy = 0;
        dbe_header->hdr_hsbtime = 0;

#ifdef SS_MME
        dbe_header->hdr_startrec.sr_firstmmeaddrpage = SU_DADDR_NULL;
#endif

        dbe_header->hdr_cryptalg = SU_CRYPTOALG_NONE;
        memset(dbe_header->hdr_cryptkey, 0, sizeof(dbe_header->hdr_cryptkey));
        memset(dbe_header->hdr_cryptchk, 0, sizeof(dbe_header->hdr_cryptchk));

        dbe_header->hdr_flags = HEADER_FLAG_BNODE_MISMATCHARRAY;
#if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
        dbe_header->hdr_flags |= HEADER_FLAG_MYSQL;
#endif
        if (dbe_cfg_mergecleanup) {
            dbe_header->hdr_flags |= HEADER_FLAG_MERGECLEANUP;
        } else {
            dbe_header->hdr_flags &= ~HEADER_FLAG_MERGECLEANUP;
        }

        memset(
            dbe_header->hdr_reserved,
            0,
            sizeof(dbe_header->hdr_reserved));
        return (dbe_header);
}

/*##**********************************************************************\
 * 
 *		dbe_header_readblocksize
 * 
 * Reads block size from database file
 * 
 * Parameters : 
 * 
 *	filename - in, use
 *		file name
 *		
 *	p_blocksize - out
 *		pointer to variable where blocksize will be stored
 *		
 * Return value :
 *      TRUE if successful
 *      FALSE when failed (maybe file does not exist)
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
bool dbe_header_readblocksize(
        char* filename,
        size_t* p_blocksize)
{
        int nread;
        long loc;
        char buf[sizeof(dbe_hdr_chknum_t)];
        dbe_hdr_chknum_t blocksize;
        SsBFileT* bfile;
        dbe_header_t* dbe_header = NULL;

        if (!SsFExist(filename)) {
            return (FALSE);
        }
        bfile = SsBOpen(
                    filename,
                    SS_BF_EXCLUSIVE | SS_BF_READONLY,
                    512);
        if (bfile == NULL) {
            return (FALSE);
        }
        if (SsBSize(bfile) == 0) {
            return (FALSE);
        }
        ss_dassert(sizeof(dbe_hdr_chknum_t) == 4);
        loc = DBE_BLOCKCPNUMOFFSET + sizeof(dbe_cpnum_t) +
            sizeof(dbe_header->hdr_chknum1) +
            sizeof(dbe_header->hdr_dbstate);
        nread = SsBRead(bfile, loc, buf, sizeof(buf));
        SsBClose(bfile);
        if (nread != sizeof(buf)) {
            return (FALSE);
        }
        blocksize = SS_UINT4_LOADFROMDISK(buf);
        if (blocksize != (size_t)blocksize) {
            return (FALSE);
        }
        switch ((size_t)blocksize) {
            case 2048:
            case 4096:
            case 8192:
            case 16384:
            case 32768:
            case 65536:
                *p_blocksize = (size_t)blocksize;
                break;
            default:
                return (FALSE);
        }
        return (TRUE);
}
/*##**********************************************************************\
 * 
 *		dbe_header_read
 * 
 * Reads the database header information from disk @specified address
 * 
 * Parameters : 
 * 
 *	dbe_header - in out, use
 *		the header object
 *		
 *	cache - in out, use
 *		the file cache
 *		
 *	daddr - in
 *		disk address
 *		
 * Return value :
 *      TRUE when ok or
 *      FALSE when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_header_read(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr)
{
        dbe_cacheslot_t* cacheslot;
        char* diskbuf;
        char* p_buf;
        size_t blocksize;
        bool is_consistent;
        su_svfil_t* svfil;
        su_ret_t rc;
        size_t sizeread;

        ss_dprintf_1(("dbe_header_read:daddr=%ld\n", daddr));
        ss_dassert(dbe_header != NULL);
        ss_dassert(cache != NULL);
        ss_dassert(daddr != SU_DADDR_NULL);

        blocksize = dbe_cache_getblocksize(cache);
        svfil = dbe_cache_getsvfil(cache);
        is_consistent = TRUE;
        if (!dbefile_diskless) { 
            cacheslot = dbe_cache_alloc(cache, &diskbuf);
            rc = su_svf_readlocked(svfil, daddr, diskbuf, blocksize, &sizeread);
            su_rc_assert(rc == SU_SUCCESS, rc);
            ss_dassert(sizeread == blocksize);
        } else {
            cacheslot = dbe_cache_reach(
                            cache,
                            daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            &diskbuf,
                            NULL);
        }
        DBE_BLOCK_GETTYPE(diskbuf, &dbe_header->hdr_blktype);
        if (dbe_header->hdr_blktype != DBE_BLOCK_DBHEADER) {
            is_consistent = FALSE;
        }
        DBE_BLOCK_GETCPNUM(diskbuf, &dbe_header->hdr_cpnum);
        p_buf = diskbuf + DBE_BLOCKCPNUMOFFSET + sizeof(dbe_cpnum_t);
        dbe_header->hdr_chknum1 = SS_UINT4_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_chknum1);
        dbe_header->hdr_dbstate = (ss_uint1_t)*p_buf;
        p_buf += sizeof(dbe_header->hdr_dbstate);
        dbe_header->hdr_blocksize = SS_UINT4_LOADFROMDISK(p_buf);
        if (dbe_header->hdr_blocksize != blocksize) {
            is_consistent = FALSE;
        }
        p_buf += sizeof(dbe_header->hdr_blocksize);
        p_buf = dbe_srec_getfromdisk(&dbe_header->hdr_startrec, p_buf);
        if (dbe_header->hdr_startrec.sr_cpnum != dbe_header->hdr_cpnum) {
            is_consistent = FALSE;
        }
        dbe_header->hdr_headervers = SS_UINT2_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_headervers);
        dbe_header->hdr_solidvers = SS_UINT2_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_solidvers);
        dbe_header->hdr_dbvers = SS_UINT2_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_dbvers);
        dbe_header->hdr_creatime = SS_UINT4_LOADFROMDISK(p_buf);

        ss_pprintf_1(("dbe_header_read:dbe_header_setcreatime %ld\n", dbe_header->hdr_creatime));

        p_buf += sizeof(dbe_header->hdr_creatime);
        dbe_header->hdr_creatimcrc = SS_UINT4_LOADFROMDISK(p_buf);
        if (dbe_header->hdr_creatimcrc == 0L) {
            dbe_header->hdr_creatimcrc =
                su_lxc_calcctc(dbe_header->hdr_creatime);
        }
        p_buf += sizeof(dbe_header->hdr_creatimcrc);
        memcpy(dbe_header->hdr_defcatalog, p_buf, sizeof(dbe_header->hdr_defcatalog));
        
        p_buf += sizeof(dbe_header->hdr_defcatalog);
        dbe_header->hdr_hsbcopy = (ss_uint1_t)*p_buf;
        ss_dprintf_2(("dbe_header_read:dbe_header->hdr_hsbcopy=%d\n", dbe_header->hdr_hsbcopy));
        p_buf += sizeof(dbe_header->hdr_hsbcopy);
        dbe_header->hdr_hsbtime = SS_UINT4_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_hsbtime);

        dbe_header->hdr_cryptalg = (ss_uint1_t)*p_buf;
        ss_dprintf_2(("dbe_header_read:dbe_header->hdr_cryptalg=%d\n", dbe_header->hdr_cryptalg));
        p_buf += sizeof(dbe_header->hdr_cryptalg);

        memcpy(dbe_header->hdr_cryptkey, p_buf, sizeof(dbe_header->hdr_cryptkey));
        p_buf += sizeof(dbe_header->hdr_cryptkey);
 
        memcpy(dbe_header->hdr_cryptchk, p_buf, sizeof(dbe_header->hdr_cryptchk));
        p_buf += sizeof(dbe_header->hdr_cryptchk);

        dbe_header->hdr_flags = SS_UINT4_LOADFROMDISK(p_buf);
        p_buf += sizeof(dbe_header->hdr_flags);

        p_buf = diskbuf + (blocksize - sizeof(dbe_header->hdr_chknum2));
        dbe_header->hdr_chknum2 = SS_UINT4_LOADFROMDISK(p_buf);
        if (dbe_header->hdr_chknum2 != dbe_header->hdr_chknum1) {
            is_consistent = FALSE;
        }
        if (!dbefile_diskless) {
            dbe_cache_free(cache, cacheslot);
        } else {
            dbe_cache_release(
                 cache,
                 cacheslot,
                 DBE_CACHE_CLEAN,
                 NULL);
        }
        return (is_consistent);
}

bool dbe_header_checkkey(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr)
{
        char buf[sizeof(dbe_header->hdr_cryptchk)];
        dbe_cacheslot_t* cacheslot;
        su_svfil_t* svfil;
        su_ret_t rc;
        size_t sizeread;
        su_cipher_t* cipher;
        size_t blocksize;
        char* diskbuf;
        bool ret;

        ss_dprintf_1(("dbe_header_read:daddr=%ld\n", daddr));
        ss_dassert(dbe_header != NULL);
        ss_dassert(cache != NULL);
        ss_dassert(daddr != SU_DADDR_NULL);

        blocksize = dbe_cache_getblocksize(cache);
        svfil = dbe_cache_getsvfil(cache);
        cipher = su_svf_getcipher(svfil);
 
        if (!dbefile_diskless) {
            cacheslot = dbe_cache_alloc(cache, &diskbuf);
            rc = su_svf_readlocked(svfil, daddr, diskbuf, blocksize, &sizeread);
            su_rc_assert(rc == SU_SUCCESS, rc);
            ss_dassert(sizeread == blocksize);
        } else {
            cacheslot = dbe_cache_reach(
                            cache,
                            daddr,
                            DBE_CACHE_READONLY,
                            DBE_INFO_CHECKPOINT,
                            &diskbuf,
                            NULL);
        }
        memcpy(buf, diskbuf+402, sizeof(buf));
        su_cipher_decrypt_page(cipher, buf, sizeof(buf));
        ss_dassert(sizeof(buf) == sizeof(dbe_header->hdr_cryptchk));
        ret = memcmp(buf, diskbuf, sizeof(buf)) == 0;
        if (!dbefile_diskless) {
            dbe_cache_free(cache, cacheslot); 
        } else {
            dbe_cache_release(
                 cache,
                 cacheslot,
                 DBE_CACHE_CLEAN,
                 NULL);
        }
        return ret;
}

/*##**********************************************************************\
 * 
 *		dbe_header_done
 * 
 * Deletes a database header object.
 * 
 * Parameters : 
 * 
 *	dbe_header - in, take
 *		pointer to header object
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_done(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        SsMemFree(dbe_header);
}

/*##**********************************************************************\
 * 
 *		dbe_header_makecopyof
 * 
 * Makes a new copy of the db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value - give : 
 *      pointer to new copy of header
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_header_t* dbe_header_makecopyof(
        dbe_header_t* dbe_header)
{
        dbe_header_t* new_dbe_header;

        new_dbe_header = SSMEM_NEW(dbe_header_t);
        memcpy(new_dbe_header, dbe_header, sizeof(dbe_header_t));
        return (new_dbe_header);
}

static void header_puttodiskbuf(
        dbe_header_t* dbe_header,
        su_cipher_t* cipher,
        char* diskbuf,
        size_t blocksize)
{
        char* p_buf;

        ss_dprintf_3(("header_puttodiskbuf\n"));

        memset(diskbuf, 0, blocksize);

        DBE_BLOCK_SETTYPE(diskbuf, &dbe_header->hdr_blktype);
        DBE_BLOCK_SETCPNUM(diskbuf, &dbe_header->hdr_cpnum);
        p_buf = diskbuf + DBE_BLOCKCPNUMOFFSET + sizeof(dbe_cpnum_t);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_chknum1);
        p_buf += sizeof(dbe_header->hdr_chknum1);
        *p_buf = (char)dbe_header->hdr_dbstate;
        p_buf += sizeof(dbe_header->hdr_dbstate);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_blocksize);
        p_buf += sizeof(dbe_header->hdr_blocksize);
        p_buf = dbe_srec_puttodisk(&dbe_header->hdr_startrec, p_buf);
        SS_UINT2_STORETODISK( p_buf, dbe_header->hdr_headervers);
        p_buf += sizeof(dbe_header->hdr_headervers);
        SS_UINT2_STORETODISK( p_buf, dbe_header->hdr_solidvers);
        p_buf += sizeof(dbe_header->hdr_solidvers);
        SS_UINT2_STORETODISK( p_buf, dbe_header->hdr_dbvers);
        p_buf += sizeof(dbe_header->hdr_dbvers);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_creatime);
        p_buf += sizeof(dbe_header->hdr_creatime);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_creatimcrc);
        p_buf += sizeof(dbe_header->hdr_creatimcrc);
        memcpy(p_buf, dbe_header->hdr_defcatalog, sizeof(dbe_header->hdr_defcatalog));
        p_buf += sizeof(dbe_header->hdr_defcatalog);
        *p_buf = (char)dbe_header->hdr_hsbcopy;
        p_buf += sizeof(dbe_header->hdr_hsbcopy);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_hsbtime);
        p_buf += sizeof(dbe_header->hdr_hsbtime);

        *p_buf = (char)dbe_header->hdr_cryptalg;
        p_buf += sizeof(dbe_header->hdr_cryptalg);
        memcpy(p_buf, dbe_header->hdr_cryptkey, sizeof(dbe_header->hdr_cryptkey));
        p_buf += sizeof(dbe_header->hdr_cryptkey);
        if (dbe_header->hdr_cryptalg == SU_CRYPTOALG_DES) {
            ss_dassert(sizeof(*diskbuf) == 1);
            ss_dassert(diskbuf+sizeof(dbe_header->hdr_cryptchk)<p_buf);
            memcpy(p_buf, diskbuf, sizeof(dbe_header->hdr_cryptchk));
            su_cipher_encrypt_page(cipher, p_buf, sizeof(dbe_header->hdr_cryptchk));
        } else {
            memcpy(p_buf, dbe_header->hdr_cryptchk,
                   sizeof(dbe_header->hdr_cryptchk));
        }
        ss_dassert(p_buf-diskbuf == 402);
        p_buf += sizeof(dbe_header->hdr_cryptchk);
        SS_UINT4_STORETODISK( p_buf, dbe_header->hdr_flags);
        p_buf += sizeof(dbe_header->hdr_flags);

        memcpy(
            p_buf,
            &dbe_header->hdr_reserved,
            sizeof(dbe_header->hdr_reserved));
        p_buf += sizeof(dbe_header->hdr_reserved);
        memset(
            p_buf,
            0,
            blocksize - (p_buf - diskbuf) - sizeof(dbe_header->hdr_chknum2));
        p_buf = diskbuf + (blocksize - sizeof(dbe_header->hdr_chknum2));
        SS_UINT4_STORETODISK(p_buf, dbe_header->hdr_chknum2);
}
/*##**********************************************************************\
 * 
 *		dbe_header_save
 * 
 * Saves the database header to disk @specified address
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *	cache - in out, use
 *		cache object
 *		
 *	daddr - in
 *		disk address
 *		
 * Return value : 
 *      TRUE when ok or
 *      FALSE when failed
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
bool dbe_header_save(
        dbe_header_t* dbe_header,
        dbe_cache_t* cache,
        su_daddr_t daddr)
{
        dbe_cacheslot_t* cacheslot;
        char* diskbuf;
        size_t blocksize;
        su_svfil_t* svfil;
        su_ret_t rc;
        su_cipher_t* cipher;

        ss_dprintf_1(("dbe_header_save\n"));
        ss_dassert(dbe_header != NULL);
        ss_dassert(cache != NULL);
        ss_dassert(daddr != SU_DADDR_NULL);
        ss_assert(DBE_HEADER_SIZE <= dbe_cache_getblocksize(cache));

        svfil = dbe_cache_getsvfil(cache);
        blocksize = dbe_cache_getblocksize(cache);
        cipher = su_svf_getcipher(svfil);
        if (!dbefile_diskless) {
            cacheslot = dbe_cache_alloc(cache, &diskbuf);
            header_puttodiskbuf(dbe_header, cipher, diskbuf, blocksize);
            rc = su_svf_writelocked(svfil, daddr, diskbuf, blocksize);
            su_rc_assert(rc == SU_SUCCESS, rc);
            su_svf_flush(svfil);
            dbe_cache_free(cache, cacheslot);
        } else {
            cacheslot = dbe_cache_reach(
                            cache,
                            daddr,
                            DBE_CACHE_WRITEONLY,
                            DBE_INFO_CHECKPOINT,
                            &diskbuf,
                            NULL);
            header_puttodiskbuf(dbe_header, cipher, diskbuf, blocksize);
            dbe_cache_release(
                 cache,
                 cacheslot,
                 DBE_CACHE_DIRTY,
                 NULL);
        } 
        return (TRUE);
}

/*##**********************************************************************\
 * 
 *		dbe_header_getcpnum
 * 
 * Gets checkpoint # from database header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      the checkoint number
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_cpnum_t dbe_header_getcpnum(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(dbe_header->hdr_cpnum == dbe_header->hdr_startrec.sr_cpnum);
        return (dbe_header->hdr_startrec.sr_cpnum);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setcpnum
 * 
 * Sets the checkpoint number to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	cpnum - in
 *		checkpoint #
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setcpnum(
        dbe_header_t* dbe_header,
        dbe_cpnum_t cpnum)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_cpnum =
            dbe_header->hdr_startrec.sr_cpnum =
                cpnum;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getchknum
 * 
 * Gets the check # from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      check number
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_hdr_chknum_t dbe_header_getchknum(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(dbe_header->hdr_chknum1 == dbe_header->hdr_chknum2);
        return (dbe_header->hdr_chknum1);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setchknum
 * 
 * Sets the check # to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	chknum - in
 *		check #
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setchknum(
        dbe_header_t* dbe_header,
        dbe_hdr_chknum_t chknum)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_chknum1 =
            dbe_header->hdr_chknum2 = chknum;
}

/*##**********************************************************************\
 * 
 *		dbe_header_incchknum
 * 
 * Increments the check number of db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in out, use
 *		pointer to db header
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_incchknum(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_chknum1++;
        dbe_header->hdr_chknum2 = dbe_header->hdr_chknum1;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getdbstate
 * 
 * Gets the db state from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		
 *		
 * Return value :
 *      the db state
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_dbstate_t dbe_header_getdbstate(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return ((dbe_dbstate_t)dbe_header->hdr_dbstate);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setdbstate
 * 
 * Sets the db state to header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	dbstate - in
 *		state to set
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setdbstate(
        dbe_header_t* dbe_header,
        dbe_dbstate_t dbstate)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_dbstate = (ss_uint1_t)dbstate;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getblocksize
 * 
 * Gets db file block size
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      db file block size in bytes
 *         
 * Limitations  : 
 * 
 * Globals used : 
 */
size_t dbe_header_getblocksize(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return ((size_t)dbe_header->hdr_blocksize);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setblocksize
 * 
 * Sets block size to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	blocksize - in
 *		block size in bytes
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setblocksize(
        dbe_header_t* dbe_header,
        size_t blocksize)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_blocksize = (dbe_hdr_blocksize_t)blocksize;
}

size_t dbe_header_getblocksizefrombuf(
        char *dbe_header_copy)
{
     dbe_header_t dbe_header; /* this is use for calculating the offset only */
     uint offset;
     dbe_hdr_chknum_t blocksize;

     offset = sizeof(dbe_header.hdr_blktype) +
              sizeof(dbe_header.hdr_cpnum) +
              sizeof(dbe_header.hdr_chknum1) +
              sizeof(dbe_header.hdr_dbstate);

     blocksize = SS_UINT4_LOADFROMDISK(dbe_header_copy + offset);

     ss_dprintf_1(("dbe_header_getblocksizefrombuf:blocksize=%d\n", blocksize));

     return(blocksize);
}

/*##**********************************************************************\
 * 
 *		dbe_header_getstartrec
 * 
 * Gets pointer to start record part of db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		
 *		
 * Return value - ref : 
 *      pointer to start record
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
dbe_startrec_t* dbe_header_getstartrec(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (&dbe_header->hdr_startrec);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setstartrec
 * 
 * Sets the start record to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	startrec - in, use
 *		pointer to start record
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setstartrec(
        dbe_header_t* dbe_header,
        dbe_startrec_t* startrec)
{
        ss_dassert(dbe_header != NULL);
        memcpy(
            &dbe_header->hdr_startrec,
            startrec,
            sizeof(dbe_header->hdr_startrec));
}

/*##**********************************************************************\
 * 
 *		dbe_header_getbonsairoot
 * 
 * Gets bonsai-tree root disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      bonsai root address
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getbonsairoot(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_bonsairoot);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setbonsairoot
 * 
 * Sets the bonsai-tree root address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		bonsai root address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setbonsairoot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_bonsairoot = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getpermroot
 * 
 * Gets the permanent tree root address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      permanent index tree root address
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getpermroot(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_permroot);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setpermroot
 * 
 * Sets the permanent index tree root address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		perm. tree root address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setpermroot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_permroot = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getmmiroot
 * 
 * Gets the main memory index tree root address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      main memory index index tree root address
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getmmiroot(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_mmiroot);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setmmiroot
 * 
 * Sets the main memory index index tree root address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		main memory index tree root address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setmmiroot(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_mmiroot = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getfreelistaddr
 * 
 * Gets the free list start address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      free list start disk address
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getfreelistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        FAKE_RETURN(FAKE_DBE_REJECTFREELIST, SU_DADDR_NULL);
        return (dbe_header->hdr_startrec.sr_freelistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setfreelistaddr
 * 
 * Sets the free list start disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		free list start address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setfreelistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_freelistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getchlistaddr
 * 
 * Gets change list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      change list disk address
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getchlistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_chlistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setchlistaddr
 * 
 * Sets the change list disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		change list disk address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setchlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_chlistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getcplistaddr
 * 
 * Gets checkpoint list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      the checkpoint list start address
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getcplistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_cplistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setcplistaddr
 * 
 * Sets the checkpoint list start disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		checkpoint list disk address
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setcplistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_cplistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_gettrxlistaddr
 * 
 * Gets transaction info list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      disk address of transaction info list start
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_gettrxlistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_trxlistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_settrxlistaddr
 * 
 * Sets transaction info list disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		disk address of transaction info list
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_settrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_trxlistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getstmttrxlistaddr
 * 
 * Gets stmt transaction info list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      disk address of stmt transaction info list start
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getstmttrxlistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_stmttrxlistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setstmttrxlistaddr
 * 
 * Sets stmt transaction info list disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		disk address of stmt transaction info list
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setstmttrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_stmttrxlistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getsequencelistaddr
 * 
 * Gets sequence list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      disk address of sequence list start
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getsequencelistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_sequencelistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setsequencelistaddr
 * 
 * Sets sequence list disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		disk address of sequence list
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setsequencelistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_sequencelistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getrtrxlistaddr
 * 
 * Gets rtrx list start disk address from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      disk address of rtrx list start
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getrtrxlistaddr(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_rtrxlistaddr);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setrtrxlistaddr
 * 
 * Sets rtrx list disk address to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	daddr - in
 *		disk address of rtrx list
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setrtrxlistaddr(
        dbe_header_t* dbe_header,
        su_daddr_t daddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_rtrxlistaddr = daddr;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getfilesize
 * 
 * Gets filesize from db header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      file size in blocks
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getfilesize(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_filesize);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setfilesize
 * 
 * Sets file size to db header
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	filesize - in
 *		file size in blocks
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setfilesize(
        dbe_header_t* dbe_header,
        su_daddr_t filesize)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_filesize = filesize;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getheadervers
 * 
 * Gets database header record format version number
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      2 - byte unsigned integer indicating header version
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_uint2_t dbe_header_getheadervers(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_headervers);
}

ss_uint4_t dbe_header_getheaderflags(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_flags);
}

void dbe_header_setheaderflags(
        dbe_header_t* dbe_header)
{
        dbe_header->hdr_flags |= HEADER_FLAG_BNODE_MISMATCHARRAY;
        if (dbe_cfg_mergecleanup) {
            dbe_header->hdr_flags |= HEADER_FLAG_MERGECLEANUP;
        } else {
            dbe_header->hdr_flags &= ~HEADER_FLAG_MERGECLEANUP;
        }
}

/*##**********************************************************************\
 * 
 *		dbe_header_setheadervers
 * 
 * 
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *	headervers - in
 *		2 - byte unsigned integer indicating header version
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_header_setheadervers(
        dbe_header_t* dbe_header,
        ss_uint2_t headervers)
{
        dbe_header->hdr_headervers = headervers;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getsolidvers
 * 
 * Gets solid version that has created the database
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      2 - byte unsigned integer indicating solid version
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_uint2_t dbe_header_getsolidvers(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_solidvers);
}

/*##**********************************************************************\
 * 
 *		dbe_header_getdbvers
 * 
 * Gets the database file format version.
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value :
 *      2 - byte unsigned integer indicating the version
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
ss_uint2_t dbe_header_getdbvers(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_dbvers);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setdbvers
 * 
 * Sets db file version # to header
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *	dbvers - in
 *		2 - byte unsigned integer indicating db file version
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
void dbe_header_setdbvers(
        dbe_header_t* dbe_header,
        ss_uint2_t dbvers)
{
        dbe_header->hdr_dbvers = dbvers;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getcreatime
 * 
 * Gets database file creation time
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *		
 * Return value :
 *      creation timestamp
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_uint4_t dbe_header_getcreatime(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_creatime);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setcreatime
 * 
 * Sets database header db creation time record
 * 
 * Parameters : 
 * 
 *	dbe_header - use
 *		db header
 *		
 *	creatime - in
 *		creation time in time_t (secs since 1970-01-01 00:00:00)
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_header_setcreatime(
        dbe_header_t* dbe_header,
        ss_uint4_t creatime)
{
        ss_pprintf_1(("dbe_header_setcreatime from %ld to %ld\n", dbe_header->hdr_creatime, creatime));

        dbe_header->hdr_creatime = creatime;
}

/*##**********************************************************************\
 * 
 *		dbe_header_gethsbtime
 * 
 * Gets database file hsb time
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *		
 * Return value :
 *      hsb timestamp
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_uint4_t dbe_header_gethsbtime(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_hsbtime);
}

/*##**********************************************************************\
 * 
 *		dbe_header_sethsbtime
 * 
 * Sets database header db hsb time record
 * 
 * Parameters : 
 * 
 *	dbe_header - use
 *		db header
 *		
 *	hsbtime - in
 *		hsb time in time_t (secs since 1970-01-01 00:00:00)
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_header_sethsbtime(
        dbe_header_t* dbe_header,
        ss_uint4_t hsbtime)
{
        dbe_header->hdr_hsbtime = hsbtime;
}

/*##**********************************************************************\
 * 
 *		dbe_header_getctc
 * 
 * Gets database file creation time CRC. Needed for paranoid checking
 * agoinst license crackers
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 *		
 * Return value :
 *      creation timestamp CRC
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
ss_uint4_t dbe_header_getctc(
        dbe_header_t* dbe_header)
{
        return (dbe_header->hdr_creatimcrc);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setctc
 * 
 * Sets creation time CRC record to database header record
 * 
 * Parameters : 
 * 
 *	dbe_header - use
 *		db header
 *		
 *	ctc - in
 *		db creation timestamp CRC
 *		
 * Return value : 
 * 
 * Comments : 
 * 
 * Globals used : 
 * 
 * See also : 
 */
void dbe_header_setctc(
        dbe_header_t* dbe_header,
        ss_uint4_t ctc)
{
        dbe_header->hdr_creatimcrc = ctc;
}

/*##**********************************************************************\
 * 
 *		dbe_header_calcstartreccrc
 * 
 * Calculates a 32-bit CRC from start record portion of db header
 * 
 * Parameters : 
 * 
 *	dbe_header - 
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
ss_uint4_t dbe_header_calcstartreccrc(
        dbe_header_t* dbe_header)
{
        char startrec_buf[DBE_STARTREC_SIZE];
        FOUR_BYTE_T crc = 0L;

        ss_dprintf_1(("dbe_header_calcstartreccrc\n"));

        dbe_srec_puttodisk(&(dbe_header->hdr_startrec), startrec_buf);
        su_crc32(startrec_buf, sizeof(startrec_buf), &crc);
        return ((ss_uint4_t)crc);
}

char* dbe_header_getdefcatalog(
        dbe_header_t* dbe_header)
{
        static ss_byte_t utf8_buf[SU_MAXDEFCATALOGLEN * 3 + 1];
        ss_byte_t* dst = utf8_buf;
        ss_char2_t* src = dbe_header->hdr_defcatalog;
        size_t src_len = SsWcslen(dbe_header->hdr_defcatalog);
        ss_debug(SsUtfRetT utfrc =)
            SsUCS2vatoUTF8(&dst, dst + sizeof(utf8_buf),
                         &src, src + src_len + 1);
        ss_rc_dassert(utfrc == SS_UTF_OK, utfrc);
        ss_dassert(src_len <= SU_MAXDEFCATALOGLEN);
        return ((char*)utf8_buf);
}
        
    
void dbe_header_setdefcatalog(
        dbe_header_t* dbe_header,
        char* defcatalog)
{
        size_t bytelen;
        ss_char2_t* dst = dbe_header->hdr_defcatalog;
        ss_byte_t* src = (ss_byte_t*)defcatalog;
        SsUtfRetT utfrc;

        ss_assert(defcatalog != NULL);
        bytelen = strlen(defcatalog);

        utfrc = SsUTF8toUCS2va(&dst,
                               dst +
                               (sizeof(dbe_header->hdr_defcatalog)
                                / sizeof(dbe_header->hdr_defcatalog[0])),
                               &src, src + bytelen + 1);
        ss_rc_dassert(utfrc == SS_UTF_OK || utfrc == SS_UTF_TRUNCATION, utfrc);
        dbe_header->hdr_defcatalog[(sizeof(dbe_header->hdr_defcatalog)
                                    / sizeof(dbe_header->hdr_defcatalog[0]))
                                  - 1] =
            (ss_char2_t)0;
}
        
/*##**********************************************************************\
 *
 *              dbe_header_ishsbcopy
 *
 * Get from db header if this database is from hsb copy/netcopy
 *
 * Parameters :
 *
 *      dbe_header - in, use
 *
 *
 * Return value :
 *      if this is hsb copy
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_header_ishsbcopy(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dprintf_1(("dbe_header_ishsbcopy:dbe_header->hdr_hsbcopy=%d\n", dbe_header->hdr_hsbcopy));
        return (dbe_header->hdr_hsbcopy ? TRUE : FALSE);
}

/*##**********************************************************************\
 *
 *              dbe_header_isbrokenhsbcopy
 *
 * Get from db header if this database is from broken hsb copy/netcopy
 *
 * Parameters :
 *
 *      dbe_header - in, use
 *
 *
 * Return value :
 *      if this is hsb copy
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_header_isbrokenhsbcopy(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dprintf_1(("dbe_header_isbrokenhsbcopy:dbe_header->hdr_hsbcopy=%d\n", dbe_header->hdr_hsbcopy));
        return (dbe_header->hdr_hsbcopy == 1);
}

void dbe_header_clearhsbcopybrokenstatus(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(dbe_header->hdr_hsbcopy == 1);
        if (dbe_header->hdr_hsbcopy == 1) {
            dbe_header->hdr_hsbcopy = 2;
        }
}
/*##**********************************************************************\
 *
 *              dbe_header_clearhsbcopy
 *
 * Clear hsb netcopy/copy bit from the database header
 *
 * Parameters :
 *
 *      dbe_header - in, use
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_header_clearhsbcopy(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dprintf_1(("dbe_header_clearhsbcopy\n"));
        dbe_header->hdr_hsbcopy = 0;
}

/*##**********************************************************************\
 *
 *              dbe_header_sethsbcopy
 *
 * set hsb netcopy/copy bit in the in-memory copy of the database header
 *
 * Parameters :
 *
 *      dbe_header_copy - in, use
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_header_sethsbcopy(
        char *dbe_header_copy,
        bool complete)
{
     dbe_header_t dbe_header; /* this is use for calculating the offset only */
     uint offset;

     ss_dprintf_1(("dbe_header_sethsbcopy:complete=%d\n", complete));

     offset = sizeof(dbe_header.hdr_blktype) +
              sizeof(dbe_header.hdr_cpnum) +
              sizeof(dbe_header.hdr_chknum1) +
              sizeof(dbe_header.hdr_dbstate) +
              sizeof(dbe_header.hdr_blocksize) +
              sizeof(dbe_header.hdr_startrec) +
              sizeof(dbe_header.hdr_headervers) +
              sizeof(dbe_header.hdr_solidvers) +
              sizeof(dbe_header.hdr_dbvers) +
              sizeof(dbe_header.hdr_creatime) +
              sizeof(dbe_header.hdr_creatimcrc) +
              sizeof(dbe_header.hdr_defcatalog);

     dbe_header_copy[offset] = complete ? 2 : 1;
}

#ifdef SS_DEBUG
int dbe_header_gethsbcopy(
        char *dbe_header_copy)
{
     dbe_header_t dbe_header; /* this is use for calculating the offset only */
     uint offset;

     offset = sizeof(dbe_header.hdr_blktype) +
              sizeof(dbe_header.hdr_cpnum) +
              sizeof(dbe_header.hdr_chknum1) +
              sizeof(dbe_header.hdr_dbstate) +
              sizeof(dbe_header.hdr_blocksize) +
              sizeof(dbe_header.hdr_startrec) +
              sizeof(dbe_header.hdr_headervers) +
              sizeof(dbe_header.hdr_solidvers) +
              sizeof(dbe_header.hdr_dbvers) +
              sizeof(dbe_header.hdr_creatime) +
              sizeof(dbe_header.hdr_creatimcrc) +
              sizeof(dbe_header.hdr_defcatalog);

    ss_dprintf_1(("dbe_header_gethsbcopy:dbe_header_copy[offset]=%d\n", dbe_header_copy[offset]));

     return(dbe_header_copy[offset]);
}
#endif /* SS_DEBUG */

#ifdef SS_MME
/*##**********************************************************************\
 * 
 *		dbe_header_getfirstmmeaddrpage
 * 
 * Gets the address of the first mme address page. I.e. address
 * of a page that contains the addresses of valid MME pages.
 * 
 * Parameters : 
 * 
 *	dbe_header - in, use
 *		pointer to db header
 *		
 * Return value : 
 *      address of the first mme address page
 *
 * Limitations  : 
 * 
 * Globals used : 
 */
su_daddr_t dbe_header_getfirstmmeaddrpage(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return (dbe_header->hdr_startrec.sr_firstmmeaddrpage);
}

/*##**********************************************************************\
 * 
 *		dbe_header_setfirstmmeaddrpage
 * 
 * Sets the address of the first mme address page. I.e. address
 * of a page that contains the addresses of valid MME pages.
 * 
 * Parameters : 
 * 
 *	dbe_header - out, use
 *		pointer to db header
 *		
 *	firstmmepageaddr - in
 *          address of the first mme address page
 *		
 * Return value : 
 * 
 * Limitations  : 
 * 
 * Globals used : 
 */
void dbe_header_setfirstmmeaddrpage(
        dbe_header_t* dbe_header,
        su_daddr_t firstmmepageaddr)
{
        ss_dassert(dbe_header != NULL);
        dbe_header->hdr_startrec.sr_firstmmeaddrpage = firstmmepageaddr;
}

#endif

/*##**********************************************************************\
 *
 *      dbe_header_getcryptoalg
 * 
 * Gets the cryptographic algorithm number used for database encryption.
 * Default is 0 meaning no encryption.
 *
 * Parameters :
 *
 *  dbe_header - in, use
 *      pointer to db header
 *
 * Return value :
 *      address of the first mme address page
 *
 * Limitations  :
 *
 * Globals used :
 */
su_cryptoalg_t dbe_header_getcryptoalg(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(dbe_header->hdr_cryptalg <= SU_CRYPTOALG_MAX);
        return (su_cryptoalg_t)dbe_header->hdr_cryptalg;
}

/*##**********************************************************************\
 *
 *      dbe_header_setcryptoalg
 *
 * Sets the cryptographic algorithm to be used for database file
 * encryption.
 *
 * Parameters :
 *
 *  dbe_header - out, use
 *      pointer to db header
 *
 *  alg - in
 *          address of the first mme address page
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_header_setcryptokey(
        dbe_header_t* dbe_header,
        su_cryptoalg_t alg,
        ss_uint1_t *key)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(alg <= SU_CRYPTOALG_MAX);
        ss_dassert(sizeof(dbe_header->hdr_cryptkey) == SU_CRYPTOKEY_SIZE);
        dbe_header->hdr_cryptalg = alg;
        memcpy(dbe_header->hdr_cryptkey, key, SU_CRYPTOKEY_SIZE);
}

/*##**********************************************************************\
 *
 *      dbe_header_getcryptokey
 *
 * Gets the cryptographic algorithm number used for database encryption.
 * Default is 0 meaning no encryption.
 *
 * Parameters :
 *
 *  dbe_header - in, use
 *      pointer to db header
 *
 * Return value :
 *      address of the first mme address page
 *
 * Limitations  :
 *
 * Globals used :
 */
su_cryptoalg_t dbe_header_getcryptokey(
        dbe_header_t* dbe_header,
        ss_uint1_t *key)
{
        ss_dassert(dbe_header != NULL);
        ss_dassert(SU_CRYPTOKEY_SIZE==sizeof(dbe_header->hdr_cryptkey));
        if (dbe_header->hdr_cryptalg != SU_CRYPTOALG_NONE) {
            memcpy(key, dbe_header->hdr_cryptkey, SU_CRYPTOKEY_SIZE);
        }
        return (su_cryptoalg_t)dbe_header->hdr_cryptalg;
}

ss_uint1_t *dbe_header_getcryptochk(
        dbe_header_t* dbe_header)
{
        ss_dassert(dbe_header != NULL);
        return dbe_header->hdr_cryptchk;
}
