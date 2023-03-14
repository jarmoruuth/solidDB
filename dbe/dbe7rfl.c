/*************************************************************************\
**  source       * dbe7rfl.c
**  directory    * dbe
**  description  * Roll forward log
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
#include <sssem.h>
#include <ssmem.h>
#include <ssdebug.h>
#include <ssstring.h>
#include <sssprint.h>
#include <sslimits.h>
#include <ssfile.h>
#include <ssltoa.h>
#include <sspmon.h>
#if defined(DBE_LAZYLOG_OPT)
#include <ssthread.h>
#endif

#include <su0icvt.h>
#include <su0vfil.h>
#include <su0svfil.h>
#include <su0mbsvf.h>
#include <su0cfgst.h>
#include <su0gate.h>
#include <su0mesl.h>

#include <ui0msg.h>

#ifdef SS_MME
#ifndef SS_MYSQL
#ifdef SS_MMEG2
#include <../mmeg2/mme0rval.h>
#else
#include <../mme/mme0rval.h>
#endif
#endif
#endif

#include "dbe9type.h"
#include "dbe0type.h"
#include "dbe6bnod.h"
#include "dbe6log.h"
#include "dbe7logf.h"
#include "dbe0lb.h"
#include "dbe0crypt.h"
#ifdef SS_HSBG2
#include "dbe0catchup.h"
#endif /* SS_HSBG2 */
#include "dbe7rfl.h"
#include "dbe4mme.h"

#ifndef SS_NOLOGGING

#define BLOBG2DATA_HEADERSIZE   (sizeof(dbe_blobg2id_t) + sizeof(dbe_blobg2size_t))


/* rollforward log file object type */
struct dbe_rflog_st {
        dbe_counter_t*  rfl_counter;
        su_mbsvfil_t*   rfl_mbsvfil;
        su_daddr_t      rfl_fsize;
        size_t          rfl_addressing_size;
        size_t          rfl_currbufsize;
        size_t          rfl_currbufdatasize;
        ss_int8_t       rfl_startpos_bytes; /* ignored when 0 */
        su_daddr_t      rfl_startpos;
        size_t          rfl_blocksize_at_startpos;
        dbe_logbuf_t*   rfl_buffer;
        char*           rfl_nametemplate;
        char*           rfl_logdir;
        char            rfl_digittemplate;

        dbe_logrectype_t rfl_lastlogrectype;
        dbe_logpos_t    rfl_lp;             /* scan position */
        dbe_logpos_t    rfl_lastlogreclp;
        bool            rfl_endreached;
        size_t          rfl_datasize;
        ss_uint4_t      rfl_relid;
        vtpl_t*         rfl_vtpl;
        void*           rfl_editbuf;
        size_t          rfl_editbufsize;

        bool            rfl_binarymode;      /* in binarymode data is
                                                read directly to the
                                                buffer of the calling
                                                function, rather than
                                                through rflog internal
                                                buffers */
#ifdef SS_HSBG2
        bool            rfl_hsblog;
        su_pa_t*        rfl_filespec_to_logfnum; /* map filespec numbers to
                                                    log file numbers */
        su_daddr_t      rfl_logrecpos_daddr;
        size_t          rfl_logrecpos_bufpos;
#endif /* SS_HSBG2 */
#ifdef SS_MME
        uchar*          rfl_rvalp;
#endif
        rs_sysi_t*      rfl_cd;
};


#ifdef SS_DEBUG
#define RFLOG_ADDTOLPADDR(rfl, n) rflog_addtolpaddr(rfl, n, __LINE__)
#else /* SS_DEBUG */
#define RFLOG_ADDTOLPADDR(rfl, n) rflog_addtolpaddr(rfl, n)
#endif /* SS_DEBUG */

static void rflog_addtolpaddr(dbe_rflog_t* rflog, size_t n
#ifdef SS_DEBUG
                              ,int line
#endif /* SS_DEBUG */
                             )
{
        size_t blocksize = rflog->rfl_currbufsize;

        ss_dprintf_3(("rflog_addtolpaddr: called from line %d, "
                      "rflog->rfl_lp.lp_daddr=%ld,n=%ld\n",
                      line, (long)rflog->rfl_lp.lp_daddr, (long)n));
        ss_dassert(n >= 1);
        for (;;) {
            rflog->rfl_lp.lp_daddr += blocksize / rflog->rfl_addressing_size;
            if (--n == 0) {
                break;
            }
            blocksize = su_mbsvf_getblocksize_at_addr(rflog->rfl_mbsvfil,
                                                      rflog->rfl_lp.lp_daddr);
        }
        ss_dprintf_3(("rflog_addtolpaddr: resulting rflog->rfl_lp.lp_daddr=%ld\n",
                      (long)rflog->rfl_lp.lp_daddr));
}

static su_ret_t rflog_write_page(
        dbe_rflog_t* rflog,
        su_daddr_t daddr,
        void* buff)
{
        dbe_ret_t rc;
        void *write_buff = buff;
        dbe_cryptoparams_t* cp = NULL;
        su_cipher_t* cipher;
        size_t size = rflog->rfl_currbufsize;

        if (rflog->rfl_cd != NULL) {
            cp = rs_sysi_getcryptopar(rflog->rfl_cd);
        }
        cipher = cp == NULL ? NULL: dbe_crypt_getcipher(cp);

        if (cipher != NULL) {
            dbe_encrypt_t encrypt = dbe_crypt_getencrypt(cp);

            write_buff = SsMemAlloc(size);
            memcpy(write_buff, buff, size);
            encrypt(cipher, daddr, write_buff, 1, size);
        }

        rc = su_mbsvf_write(rflog->rfl_mbsvfil, daddr, write_buff, size);

        if (write_buff != buff) {
            SsMemFree(write_buff);
        }
        return rc;
}

static su_ret_t rflog_read_pages(
        dbe_rflog_t* rflog,
        su_daddr_t daddr,
        void* buff,
        size_t size)
{
        su_ret_t rc;
        dbe_cryptoparams_t* cp = NULL;
        su_cipher_t* cipher;
        dbe_decrypt_t decrypt;

        if (rflog->rfl_cd != NULL) {
            cp = rs_sysi_getcryptopar(rflog->rfl_cd);
        }

        if (cp != NULL) {
            cipher = dbe_crypt_getcipher(cp);
            decrypt = dbe_crypt_getdecrypt(cp);
        } else {
            cipher = NULL;
            decrypt = NULL;
        }

        rc = su_mbsvf_read(rflog->rfl_mbsvfil, daddr, buff, size);

        if (rc == SU_SUCCESS && cipher != NULL) {
            decrypt(cipher, daddr, buff, 1, size);
        }

        return rc;
}

static su_ret_t rflog_read_rfl(
        dbe_rflog_t* rflog)
{
        su_ret_t rc;
        rc = rflog_read_pages(rflog, rflog->rfl_lp.lp_daddr,
                              rflog->rfl_buffer, rflog->rfl_currbufsize);
        return rc;
}

/*#***********************************************************************\
 *
 *              rflog_createfile
 *
 * Creates the split virtual file by combining together the log files.
 * The log file names are generated from the template and the log file
 * number. The EOF is detected by opening files with incremented log file
 * numbers till an empty (or non-existent file is detected). The first
 * file is allowed to be non-existent, because the registered log file #
 * at checkpoint creation may actually be nonrelevant.
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *      size of log in blocks
 *
 * Limitations  :
 *
 * Globals used :
 */
static void rflog_createfile(
        dbe_rflog_t* rflog,
        dbe_logfnum_t start_logfnum,
        bool hsb_catchup)
{
        dbe_ret_t rc;
        su_svfil_t* tmp_svfil;
        char* fname;
        su_daddr_t fsize;
        int i;
        int filespecno = 1;
        dbe_logfnum_t logfnum;
        ss_int8_t fsizesum_bytes;
        ss_int8_t fsize_bytes;
        dbe_cryptoparams_t* cp = NULL;
        
        
        if (rflog->rfl_cd != NULL) {
            cp = rs_sysi_getcryptopar(rflog->rfl_cd);
        }

        rc = DBE_RC_SUCC;
        if (start_logfnum == 0) {
            logfnum = dbe_counter_getlogfnum(rflog->rfl_counter);
        } else {
            logfnum = start_logfnum;
        }

        rflog->rfl_fsize = 0L;
        SsInt8Set0(&fsizesum_bytes);
        for (i = 0; ; i++, logfnum++)  {
#ifdef SS_HSBG2
            tmp_svfil = su_svf_init(DBE_CFG_MINLOGBLOCKSIZE, SS_BF_SEQUENTIAL);
#else
            tmp_svfil = su_svf_init(DBE_CFG_MINLOGBLOCKSIZE, SS_BF_EXCLUSIVE|SS_BF_SEQUENTIAL);
#endif /* SS_HSBG2 */

            if (cp != NULL) {
                su_svf_setcipher(tmp_svfil, dbe_crypt_getcipher(cp),
                                 dbe_crypt_getencrypt(cp),
                                 dbe_crypt_getdecrypt(cp));
            }

            fname = dbe_logfile_genname(    /* generate file name */
                rflog->rfl_logdir,
                rflog->rfl_nametemplate,
                logfnum,
                rflog->rfl_digittemplate);
            if (fname == NULL) {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_ILLLOGFILETEMPLATE_SSSDD,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGFILETEMPLATE,
                    rflog->rfl_nametemplate,
                    DBE_LOGFILENAME_MINDIGITS,
                    DBE_LOGFILENAME_MAXDIGITS);
            }

            ss_dprintf_1(("rflog_createfile:logfnum %d, fname %s\n", logfnum, fname));

            if (!SsFExist(fname)) {
                fsize = 0L;
            } else {
                fsize_bytes = SsFSizeAsInt8(fname);
                if (!SsInt8Is0(fsize_bytes)) {
                    su_svf_addfile(tmp_svfil, fname, fsize_bytes, FALSE);
                    fsize = su_svf_getsize(tmp_svfil);
                    if (fsize == 0) {
                        /* blocksize is smaller, the value could have been 0,
                           although file is not empty
                        */
                        fsize = 1;
                    }
                } else {
                    fsize = 0;
                }
            }

            if (fsize != 0L) {      /* file exists */
                size_t bytesread;
                uchar* p;
                loghdrdata_t hdrdata;

                rc = su_svf_read(   /* read 1st block to get header record */
                        tmp_svfil,
                        0L,
                        rflog->rfl_buffer,
                        DBE_CFG_MINLOGBLOCKSIZE,
                        &bytesread);
                su_rc_assert(rc == DBE_RC_SUCC, rc);
                ss_dassert(DBE_CFG_MINLOGBLOCKSIZE == bytesread);

                p = DBE_LB_DATA(rflog->rfl_buffer);
                if (*p != DBE_LOGREC_HEADER) {
                    su_emergency_exit( /* HSB:should not exit in HSB catchup */
                        __FILE__,
                        __LINE__,
                        DBE_ERR_LOGFILE_CORRUPT_S,
                        fname);
                    /* NOTREACHED */
                }
                ss_dassert(*p == DBE_LOGREC_HEADER);
                p++;
                hdrdata.lh_logfnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(hdrdata.lh_logfnum);
                hdrdata.lh_cpnum = SS_UINT4_LOADFROMDISK(p);
                p += sizeof(hdrdata.lh_cpnum);
                hdrdata.lh_blocksize = SS_UINT4_LOADFROMDISK(p);
                
                if (0 != ((hdrdata.lh_blocksize - 1) & hdrdata.lh_blocksize)
                ||  hdrdata.lh_blocksize < DBE_CFG_MINLOGBLOCKSIZE)
                {
                    /* Must be a power of two and at least minimum */
                    su_informative_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_ILLBLOCKSIZE_UUSSSU,
                        (ulong)rflog->rfl_currbufsize,
                        (ulong)hdrdata.lh_blocksize,
                        fname,
                        SU_DBE_LOGSECTION,
                        SU_DBE_BLOCKSIZE,
                        (ulong)hdrdata.lh_blocksize
                    );
                }
                p += sizeof(hdrdata.lh_blocksize);
                hdrdata.lh_dbcreatime = SS_UINT4_LOADFROMDISK(p);
                if (hdrdata.lh_cpnum
                    < dbe_counter_getcpnum(rflog->rfl_counter)
                &&  i > 0)
                {
                    /* ss_dassert(start_logfnum == 0); pete removed 2004-03-26 */
                    rflog->rfl_startpos_bytes = fsizesum_bytes;
                }
                su_svf_done(tmp_svfil);

                rc = su_mbsvf_addfile(
                        &rflog->rfl_mbsvfil,
                        fname,
                        fsize_bytes,
                        hdrdata.lh_blocksize);
                if (start_logfnum == logfnum) {
                    rflog->rfl_blocksize_at_startpos = hdrdata.lh_blocksize;
                }
                ss_rc_dassert(rc == SU_SUCCESS, rc);
                SsInt8AddInt8(&fsizesum_bytes, fsizesum_bytes, fsize_bytes);

                ss_dprintf_1(("rflog_createfile:%d:rfl_fsize %ld\n", i, rflog->rfl_fsize));

#ifdef SS_HSBG2
                su_pa_insertat(
                    rflog->rfl_filespec_to_logfnum,
                    filespecno,
                    (void *) logfnum);

                filespecno++;
#endif /* SS_HSBG2 */

            } else {
                su_svf_done(tmp_svfil);
            }
            SsMemFree(fname);
            if (fsize == 0L && i > 0) {
                break;
            }
        }
        if (logfnum > 1) {
            logfnum--;  /* Decrement the log file number to last existing */
        }

        if (!hsb_catchup) {
            ss_debug(
            {   dbe_logfnum_t c_logfnum;
                c_logfnum = dbe_counter_getlogfnum(rflog->rfl_counter);

                ss_dassert(c_logfnum <= logfnum);
                ss_dassert(c_logfnum <= logfnum+1);
            }
            );

            dbe_counter_setlogfnum(rflog->rfl_counter, logfnum);
        }
        if (!SsInt8Is0(fsizesum_bytes)) {
            size_t maxblocksize;
            rflog->rfl_addressing_size = su_mbsvf_getminblocksize(rflog->rfl_mbsvfil);
            maxblocksize = su_mbsvf_getmaxblocksize(rflog->rfl_mbsvfil);
            if (maxblocksize > rflog->rfl_currbufsize) {
                dbe_lb_done(rflog->rfl_buffer);
                rflog->rfl_buffer = dbe_lb_init(maxblocksize);
            }
            rflog->rfl_fsize = su_mbsvf_getsize(rflog->rfl_mbsvfil);
        } else {
            rflog->rfl_fsize = 0;
        }
            
}

/*#***********************************************************************\
 *
 *              rflog_checkpingpong1
 *
 * Checks if the 2 last blocks in the end of the log are actually versions
 * of the same logical block (generated by the ping-pong write algorithm).
 * This 1st check only detects the case if the second-last block is
 * corrupt. In that case the last block is copied over the second-last
 * and the last is null-padded (ie. filled with DBE_LOGREC_NOPs).
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when end of readable log is reached
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_checkpingpong1(dbe_rflog_t* rflog,
                                      size_t blocksize_at_daddr_plus_1)
{
        dbe_ret_t rc;
        dbe_logbuf_t* tmp_lb = NULL;
        bool consistent1;

#ifdef SS_HSBG2
        ss_dassert(!rflog->rfl_hsblog);
#endif /* SS_HSBG2 */
        ss_dassert(rflog->rfl_lp.lp_daddr +
                   rflog->rfl_currbufsize / rflog->rfl_addressing_size +
                   blocksize_at_daddr_plus_1 / rflog->rfl_addressing_size
                   == rflog->rfl_fsize);
        /* read the first of the last two blocks */

        rc = rflog_read_rfl(rflog);

        if (rc != SU_SUCCESS) {
            ss_dprintf_3(("rflog_checkpingpong1(line %d): unexpected read error: %ld\n",
                          __LINE__,
                          (long)rc));
            return (rc);
        }
        consistent1 = dbe_lb_isconsistent(rflog->rfl_buffer,
                                          rflog->rfl_currbufsize);
        if (rflog->rfl_currbufsize != blocksize_at_daddr_plus_1) {
            if (consistent1) {
                return (SU_SUCCESS);
            } else {
#ifdef AUTOTEST_RUN
                ss_derror;
#endif
                return (DBE_ERR_LOGFILE_CORRUPT);
            }
        }
        ss_dassert(rflog->rfl_currbufsize == blocksize_at_daddr_plus_1);
        tmp_lb = dbe_lb_init(rflog->rfl_currbufsize);
        rc = rflog_read_pages(rflog, 
                (rflog->rfl_lp.lp_daddr +
                 (blocksize_at_daddr_plus_1 /
                  rflog->rfl_addressing_size)),
                tmp_lb,
                rflog->rfl_currbufsize);
        if (rc != SU_SUCCESS) {
            ss_dprintf_3(("rflog_checkpingpong1(line %d): unexpected read error: %ld\n",
                          __LINE__,
                          (long)rc));
            goto return_rc;
        }
        if (consistent1) {
            /* second-last block was consistent */
            if (dbe_lb_isconsistent(tmp_lb, rflog->rfl_currbufsize)) {
                if (dbe_lb_sameblocknumber(tmp_lb, rflog->rfl_buffer)) {
                    if (dbe_lb_versioncmp(tmp_lb, rflog->rfl_buffer) < 0) {
                        goto truncate_last_off;
                    } else {
                        goto copy_last_over_2ndlast;
                    }
                } else {
                    goto not_pingpong;
                }
            } else {
                /* last block is corrupted! */
                goto truncate_last_off;
            }
            
        } else {
            /*
             * the first block was not consistent, so read the second one
             *
             */
            if (dbe_lb_isconsistent(tmp_lb, rflog->rfl_currbufsize)) {
                /* the last block was consistent, so overwrite the
                   second-last one with the data from the last one */
                goto copy_last_over_2ndlast;
            } else {
                goto both_corrupt;
            }
        }
        /* NOTREACHED */
        ss_derror;
 copy_last_over_2ndlast:;
        dbe_lb_incversion(tmp_lb, rflog->rfl_currbufsize);
        rc = rflog_write_page(rflog, rflog->rfl_lp.lp_daddr, tmp_lb);
        if (rc != SU_SUCCESS) {
            ss_dprintf_3(("rflog_checkpingpong1(line %d): unexpected read error: %ld\n",
                          __LINE__,
                          (long)rc));
            goto return_rc;
        }
        memcpy(rflog->rfl_buffer,
               tmp_lb,
               rflog->rfl_currbufsize);
            
        /* and truncate the log file to avoid later confusion */
 truncate_last_off:;
        rc = su_mbsvf_decreasesize(rflog->rfl_mbsvfil,
                                   rflog->rfl_fsize
                                   - (rflog->rfl_currbufsize
                                      / rflog->rfl_addressing_size));
        if (rc != SU_SUCCESS) {
            ss_dprintf_3(("rflog_checkpingpong1(line %d): unexpected read error: %ld\n",
                          __LINE__,
                          (long)rc));
            goto return_rc;
        }
        rflog->rfl_fsize -= (rflog->rfl_currbufsize
                             / rflog->rfl_addressing_size);
        goto return_rc;
 both_corrupt:;
#ifdef AUTOTEST_RUN
        ss_derror;
#endif
        rc = DBE_ERR_LOGFILE_CORRUPT;
 not_pingpong:;
 return_rc:;
        if (tmp_lb != NULL) {
            dbe_lb_done(tmp_lb);
        }
        return (rc);
}

#ifdef SS_HSBG2

dbe_ret_t dbe_rflog_fill_catchuplogpos(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *logpos)
{
        dbe_ret_t rc;
        int filespecno;
        su_daddr_t physdaddr;
        bool b;

        b = su_mbsvf_getfilespecno_and_physdaddr(
                    rflog->rfl_mbsvfil,
                    rflog->rfl_logrecpos_daddr,
                    &filespecno,
                    &physdaddr);

        ss_dprintf_3(("dbe_rflog_fill_catchuplogpos: rfl_logrecpos_daddr=%ld,"
                      "filespecno=%ld,physaddr=%ld\n",
                      (long)rflog->rfl_logrecpos_daddr,
                      (long)filespecno,
                      (long)physdaddr));
        if (b) {
            rc = SU_SUCCESS;
            DBE_CATCHUP_LOGPOS_SET(
                (*logpos),
                (dbe_logfnum_t) su_pa_getdata(
                                        rflog->rfl_filespec_to_logfnum,
                                        filespecno),
                physdaddr,
                rflog->rfl_logrecpos_bufpos);
        } else {
            /* file not found */
            rc = DBE_RC_NOTFOUND;
            DBE_CATCHUP_LOGPOS_SET_NULL(*logpos);
        }

        return(rc);
}

#endif /* SS_HSBG2 */

/*#***********************************************************************\
 *
 *              rflog_loadblock
 *
 * loads a block at rfl_lp.lp_daddr to rfl_buffer and resets rfl_lp.lp_bufpos
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      resetpos - in
 *          tells whether to reset the rfl_lp.lp_bufpos
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_loadblock(
        dbe_rflog_t* rflog,
        bool resetpos)
{
        dbe_ret_t rc;
        size_t blocksize_at_daddr;
        size_t blocksize_at_daddr_plus_1;

#ifdef SS_HSBG2
        if (rflog->rfl_hsblog) {
            ss_dprintf_3(("rflog_loadblock: line %d returning %d\n",
                          __LINE__,
                          (int)DBE_RC_END));
            return(DBE_RC_END);
        }
#endif /* SS_HSBG2 */
        if (rflog->rfl_lp.lp_daddr >= rflog->rfl_fsize) {
            ss_dprintf_3(("rflog_loadblock: line %d returning %d "
                          "(rflog->rfl_lp.lp_daddr=%ld,"
                          "rflog->rfl_fsize=%ld)\n",
                          __LINE__,
                          (int)DBE_RC_END,
                          (long)rflog->rfl_lp.lp_daddr,
                          (long)rflog->rfl_fsize));
            return (DBE_RC_END);
        }
        
        blocksize_at_daddr =
            su_mbsvf_getblocksize_at_addr(rflog->rfl_mbsvfil,
                                          rflog->rfl_lp.lp_daddr);
        ss_dprintf_3(("rflog_loadblock: daddr=%ld,blocksize_at_daddr=%ld\n",
                      (long)rflog->rfl_lp.lp_daddr,
                      (long)blocksize_at_daddr));
        if (blocksize_at_daddr != rflog->rfl_currbufsize) {
            rflog->rfl_currbufsize = blocksize_at_daddr;
            rflog->rfl_currbufdatasize =
                blocksize_at_daddr - (2 * sizeof(ss_uint2_t));
        }
        blocksize_at_daddr_plus_1 =
            su_mbsvf_getblocksize_at_addr(rflog->rfl_mbsvfil,
                                          rflog->rfl_lp.lp_daddr +
                                          (blocksize_at_daddr /
                                           rflog->rfl_addressing_size));
        ss_dprintf_3(("rflog_loadblock: daddr+1=%ld,blocksize_at_daddr_plus_1=%ld\n",
                      (long)(rflog->rfl_lp.lp_daddr +
                             (blocksize_at_daddr /
                              rflog->rfl_addressing_size)),
                      (long)blocksize_at_daddr_plus_1));
        if (rflog->rfl_lp.lp_daddr +
            (blocksize_at_daddr /
             rflog->rfl_addressing_size) +
            (blocksize_at_daddr_plus_1 /
             rflog->rfl_addressing_size)
            ==  rflog->rfl_fsize)
        {
            rc = rflog_checkpingpong1(rflog, blocksize_at_daddr_plus_1);
            ss_dprintf_3(("rflog_loadblock: rflog_checkpingpong1"
                          " returned %ld\n",
                          (long)rc));
        } else {
            rc = rflog_read_rfl(rflog);
            if (!dbe_lb_isconsistent(rflog->rfl_buffer,
                                     rflog->rfl_currbufsize))
            {
#ifdef AUTOTEST_RUN
                ss_derror;
#endif
                rc = DBE_ERR_LOGFILE_CORRUPT;
            }
        }
        if (resetpos) {
            rflog->rfl_lp.lp_bufpos = 0;
        }
        ss_dprintf_3(("rflog_loadblock: line %d returning %d\n",
                      __LINE__,
                      (int)rc));
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_checkbufpos
 *
 * Checks whether the rfl_lp.lp_bufpos has grown to end of block and in
 * such a case load a new block to rfl_buffer. (a macro)
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static int rflog_checkbufpos(dbe_rflog_t* rflog)
{
        su_ret_t rc;

        ss_pprintf_3(("rflog_checkbufpos:(rflog)->rfl_lp.lp_bufpos=%ld\n",
          (rflog)->rfl_lp.lp_bufpos));

#ifdef SS_HSBG2
        if (rflog->rfl_hsblog) {
            if (rflog->rfl_lp.lp_bufpos > rflog->rfl_currbufdatasize) {
                ss_dprintf_3(("rflog_checkbufpos: line %d returning %d\n",
                              __LINE__,
                              (int)DBE_RC_END));
               
                return(DBE_RC_END);
            } else {
                return(DBE_RC_SUCC);
            }
        }
#endif /* SS_HSBG2 */

        if(rflog->rfl_lp.lp_bufpos >= rflog->rfl_currbufdatasize) {
            RFLOG_ADDTOLPADDR(rflog, 1);
            rc = rflog_loadblock(rflog, TRUE);
        } else {
            rc = DBE_RC_SUCC;
        }
        ss_dprintf_3(("rflog_checkbufpos: line %d returning %d\n",
                      __LINE__,
                      (int)rc));
        return (rc);
}


/*#***********************************************************************\
 *
 *              rflog_getrelidordatasize
 *
 * Gets relation id from log
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relidordatasize - out
 *              pointer to 4-byte integer where relation id or data size
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t rflog_getrelidordatasize(dbe_rflog_t* rflog, ss_uint4_t* p_relidordatasize)
{
        uchar* p;
        ss_uint4_t relidordatasize_field;
        dbe_ret_t rc;

        rc = DBE_RC_SUCC;
        p = DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos;
        if (rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos
            >= sizeof(relidordatasize_field))
        {
            relidordatasize_field = SS_UINT4_LOADFROMDISK(p);
            rflog->rfl_lp.lp_bufpos += sizeof(relidordatasize_field);
        } else {
            uchar* dp;
            size_t tmp;

            dp = rflog->rfl_editbuf;
            tmp = rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
            memcpy(
                dp,
                p,
                tmp);
            dp += tmp;
            RFLOG_ADDTOLPADDR(rflog, 1);
            rc = rflog_loadblock(rflog, TRUE);
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            p = DBE_LB_DATA(rflog->rfl_buffer);
            ss_dassert(rflog->rfl_lp.lp_bufpos == 0);
            tmp = sizeof(relidordatasize_field) - tmp;
            memcpy(dp, p, tmp);
            relidordatasize_field = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            rflog->rfl_lp.lp_bufpos += tmp;
        }
        ss_dassert(p_relidordatasize != NULL);
        *p_relidordatasize = relidordatasize_field;
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_getdatasize
 *
 * Gets the value of data size field
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_datasize - out, use
 *              pointer to var where the data size will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_getdatasize(dbe_rflog_t* rflog, size_t* p_datasize)
{
        ss_uint4_t length_field;
        dbe_ret_t rc;

        ss_dassert(p_datasize != NULL);
        rc = rflog_getrelidordatasize(rflog, &length_field);
        *p_datasize = (size_t)length_field;
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_getrelid
 *
 * Gets relation id (a macro)
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relid - out
 *              pointer to variable where the relation id will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
#define rflog_getrelid(rflog, p_relid) \
        rflog_getrelidordatasize(rflog, p_relid)


/*#***********************************************************************\
 *
 *              rflog_gettrxid
 *
 * Gets the trxid field from a log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_trxid - out, use
 *              pointer to var where the trxid will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_gettrxid(dbe_rflog_t* rflog, dbe_trxid_t* p_trxid)
{
        uchar* p;
        ss_uint4_t trxid_field;
        dbe_ret_t rc;

        rc = DBE_RC_SUCC;
        p = DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos;
        if (rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos
            >= sizeof(trxid_field))
        {
            trxid_field = SS_UINT4_LOADFROMDISK(p);
            rflog->rfl_lp.lp_bufpos += sizeof(trxid_field);
        } else {
            uchar* dp;
            size_t tmp;

            dp = rflog->rfl_editbuf;
            tmp = rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
            memcpy(
                dp,
                p,
                tmp);
            dp += tmp;
            RFLOG_ADDTOLPADDR(rflog, 1);
            rc = rflog_loadblock(rflog, TRUE);
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            p = DBE_LB_DATA(rflog->rfl_buffer);
            ss_dassert(rflog->rfl_lp.lp_bufpos == 0);
            tmp = sizeof(trxid_field) - tmp;
            memcpy(dp, p, tmp);
            trxid_field = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            rflog->rfl_lp.lp_bufpos += tmp;
        }
        ss_dassert(p_trxid != NULL);
        *p_trxid = DBE_TRXID_INIT(trxid_field);
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_getvtplsize
 *
 * Gets size of vtuple field of a log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_vtplsize - out, use
 *              pointer to variable where the vtuple size will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_getvtplsize(dbe_rflog_t* rflog, size_t* p_vtplsize)
{
        uchar* p;
        size_t length;
        dbe_ret_t rc;
        size_t lenfldlen;

        rc = DBE_RC_SUCC;
        p = DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos;
        lenfldlen = VA_LENLEN((va_t*)p);
        if (rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos >= lenfldlen) {
            length = VA_NETLEN((va_t*)p);
            if (length + lenfldlen <=
                rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos)
            {
                rflog->rfl_vtpl = (vtpl_t*)p;
            } else {
                memcpy(rflog->rfl_editbuf, p, lenfldlen);
                rflog->rfl_vtpl = (vtpl_t*)rflog->rfl_editbuf;
            }
            rflog->rfl_lp.lp_bufpos += lenfldlen;
        } else {
            uchar* dp;
            size_t tmp;

            dp = rflog->rfl_editbuf;
            tmp = rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
            memcpy(
                dp,
                p,
                tmp);
            dp += tmp;
            RFLOG_ADDTOLPADDR(rflog, 1);
            rc = rflog_loadblock(rflog, TRUE);
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            p = DBE_LB_DATA(rflog->rfl_buffer);
            ss_dassert(rflog->rfl_lp.lp_bufpos == 0);
            tmp = lenfldlen - tmp;
            memcpy(dp, p, tmp);
            length = VA_NETLEN((va_t*)rflog->rfl_editbuf);
            rflog->rfl_lp.lp_bufpos += tmp;
            rflog->rfl_vtpl = (vtpl_t*)rflog->rfl_editbuf;
        }
        ss_dassert(p_vtplsize != NULL);
        *p_vtplsize = length;
        return (rc);
}

#ifdef SS_MME
/*#***********************************************************************\
 *
 *              rflog_getrvalsize
 *
 * Gets size of mme_rowval_t field of a log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_rvalsize - out, use
 *              pointer to variable where the mme_rval_t size will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code when failure
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t rflog_getrvalsize(dbe_rflog_t* rflog, size_t* p_rvalsize)
{
        uchar* p;
        ss_uint2_t length;
        ss_uint2_t lenfldlen;
        dbe_ret_t rc;

        rc = DBE_RC_SUCC;
        p = DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos;
        lenfldlen = sizeof(ss_uint2_t);
        if (rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos >= lenfldlen) {
            length = SS_UINT2_LOADFROMDISK(p);
            if (length + lenfldlen <=
                rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos)
            {
                p += lenfldlen;
                rflog->rfl_rvalp = p; /* point directly to diskbuf */
            } else {
                rflog->rfl_rvalp = rflog->rfl_editbuf;
            }
            rflog->rfl_lp.lp_bufpos += lenfldlen;
        } else {
            uchar* dp;
            size_t tmp;

            dp = rflog->rfl_editbuf;
            tmp = rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
            memcpy(
                dp,
                p,
                tmp);
            dp += tmp;
            RFLOG_ADDTOLPADDR(rflog, 1);
            rc = rflog_loadblock(rflog, TRUE);
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            p = DBE_LB_DATA(rflog->rfl_buffer);
            ss_dassert(rflog->rfl_lp.lp_bufpos == 0);
            tmp = lenfldlen - tmp;
            memcpy(dp, p, tmp);

            length = SS_UINT2_LOADFROMDISK(rflog->rfl_editbuf);
            rflog->rfl_lp.lp_bufpos += tmp;
            rflog->rfl_rvalp = rflog->rfl_editbuf;
        }
        ss_dassert(p_rvalsize != NULL);
        *p_rvalsize = (size_t)length;
        return (rc);
}
#endif /* SS_MME */

/*##**********************************************************************\
 *
 *              dbe_rflog_seek
 *
 * Seek to a certain position in the file
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static bool dbe_rflog_seek(
        dbe_rflog_t* rflog,
        dbe_logpos_t pos)
{
        rflog->rfl_lp.lp_daddr = pos.lp_daddr;
        rflog->rfl_lp.lp_bufpos = pos.lp_bufpos;
        rflog->rfl_datasize = 0;
        rflog->rfl_lastlogrectype = DBE_LOGREC_NOP;
        if (rflog->rfl_lp.lp_daddr < rflog->rfl_fsize ||
            (rflog->rfl_fsize == 0 && rflog->rfl_lp.lp_daddr == 0)) /* egge: Bugzilla #398 */
        {
            dbe_ret_t rc;

            rc = rflog_loadblock(rflog, FALSE);
            if (rc != DBE_RC_SUCC) {
#ifdef AUTOTEST_RUN
                ss_derror;
#endif
                return (FALSE);
            }
            return(TRUE);
        } else {
            FAKE_CODE_BLOCK_GT(
                FAKE_HSBG2_PRI_CP_DELETE_LOG_FILES, 0,
                {
                    return(FALSE);
                }
            );
            ss_derror;
            return(FALSE);
        }
}

/*#***********************************************************************\
 *
 *              rflog_init
 *
 * Inits rflog object.
 *
 * Parameters :
 *
 *              cfg -
 *
 *
 *              counter -
 *
 *
 *              p_catchup_startpos -
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
static dbe_rflog_t* rflog_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter,
        dbe_catchup_logpos_t* p_catchup_startpos)
{
        dbe_rflog_t* rflog;
        size_t idxblocksize;

        ss_dprintf_3(("rflog_init:%s\n", p_catchup_startpos != NULL ? "HSB catchup" : "recovery"));

        ss_debug(
            if (p_catchup_startpos != NULL) {
                ss_dprintf_3(("rflog_init:p_catchup_startpos->logfnum=%ld,"
                              "daddr=%ld,bufpos=%ld\n",
                              (long)p_catchup_startpos->lp_logfnum,
                              (long)p_catchup_startpos->lp_daddr,
                              (long)p_catchup_startpos->lp_bufpos));
            });
        rflog = SSMEM_NEW(dbe_rflog_t);

        rflog->rfl_filespec_to_logfnum = su_pa_init();
        rflog->rfl_cd = cd;
        dbe_cfg_getlogblocksize(cfg, &rflog->rfl_currbufsize);
        dbe_cfg_getidxblocksize(cfg, &idxblocksize);
        rflog->rfl_currbufdatasize = rflog->rfl_currbufsize -
            (2 * sizeof(ss_uint2_t));
        if (p_catchup_startpos != NULL) {
            rflog->rfl_startpos = p_catchup_startpos->lp_daddr;
        } else {
            rflog->rfl_startpos = 0L;
        }

        rflog->rfl_buffer = dbe_lb_init(rflog->rfl_currbufsize);
        dbe_cfg_getlogfilenametemplate(
                    cfg,
                    &rflog->rfl_nametemplate);
        dbe_cfg_getlogdir(
                    cfg,
                    &rflog->rfl_logdir);
        dbe_cfg_getlogdigittemplate(
                    cfg,
                    &rflog->rfl_digittemplate);
        rflog->rfl_counter = counter;
        rflog->rfl_blocksize_at_startpos = 0;
        SsInt8Set0(&rflog->rfl_startpos_bytes);

        rflog->rfl_mbsvfil = su_mbsvf_init();
        if (p_catchup_startpos != NULL) {
            ss_dprintf_3(("rflog_init:p_catchup_startpos->lp_daddr %ld, logfnum %ld\n", p_catchup_startpos->lp_daddr, p_catchup_startpos->lp_logfnum));
            ss_dassert(p_catchup_startpos->lp_logfnum > 0);
            ss_dassert(rflog->rfl_mbsvfil != NULL);

            rflog_createfile(rflog, p_catchup_startpos->lp_logfnum, TRUE);

            /* patch startpos:
             */
            rflog->rfl_startpos = p_catchup_startpos->lp_daddr;
            ss_dprintf_3(("rflog_init: rfl_blocksize_at_startpos=%ld\n",
                          (long)rflog->rfl_blocksize_at_startpos));
            if (rflog->rfl_blocksize_at_startpos != 0) {
                if (rflog->rfl_blocksize_at_startpos != rflog->rfl_addressing_size) {
                    ss_dassert(rflog->rfl_addressing_size < rflog->rfl_blocksize_at_startpos);
                    rflog->rfl_startpos *=
                        rflog->rfl_blocksize_at_startpos / rflog->rfl_addressing_size;
                    ss_dprintf_3(("rflog_init: rfl_startpos=%ld\n",
                                  (long)rflog->rfl_startpos));
                }
            }
        } else {
            ss_dassert(rflog->rfl_mbsvfil != NULL);

            rflog_createfile(rflog, 0, FALSE);
            if (!SsInt8Is0(rflog->rfl_startpos_bytes)) {
                ss_int8_t tmp;

                SsInt8SetUint4(&tmp, rflog->rfl_addressing_size);
                SsInt8DivideByInt8(&tmp, rflog->rfl_startpos_bytes, tmp);
                rflog->rfl_startpos = SsInt8GetLeastSignificantUint4(tmp);
            }
        }

        /* This isn't accurate, but in the ballpark anyway. */
        rflog->rfl_editbufsize = idxblocksize;
        rflog->rfl_editbuf = SsMemAlloc(rflog->rfl_editbufsize);
        rflog->rfl_vtpl = NULL;
        rflog->rfl_binarymode = FALSE;
        rflog->rfl_relid = 0L;
        rflog->rfl_lastlogreclp.lp_daddr = SU_DADDR_NULL;
        rflog->rfl_lastlogreclp.lp_bufpos = 0;
        rflog->rfl_lp.lp_daddr = 0L;
        rflog->rfl_lp.lp_bufpos = 0;
        rflog->rfl_endreached = FALSE;
        rflog->rfl_datasize = 0;
        rflog->rfl_lastlogrectype = DBE_LOGREC_NOP;
        rflog->rfl_hsblog = FALSE;
        rflog->rfl_rvalp = NULL;

        if (p_catchup_startpos != NULL) {
            dbe_logpos_t lp;

            if(p_catchup_startpos->lp_daddr != 0) {
                ss_dassert(rflog->rfl_startpos != 0);
                if (rflog->rfl_startpos >= rflog->rfl_fsize) {
                    dbe_rflog_done(rflog);
                    return (NULL);
                }
            }
            lp.lp_daddr = rflog->rfl_startpos;
            lp.lp_bufpos = p_catchup_startpos->lp_bufpos;
            if (!dbe_rflog_seek(rflog, lp)) {
                dbe_rflog_done(rflog);
                return(NULL);
            }
        } else if (rflog->rfl_fsize != 0L) {
            dbe_rflog_resetscan(rflog);
        }
        return (rflog);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_init
 *
 * Creates a roll-forward log object
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 * Return value - give :
 *      pointer to roll-forward log
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_rflog_t* dbe_rflog_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter)
{
        ss_dprintf_1(("dbe_rflog_init\n"));

        return(rflog_init(cfg, cd, counter, NULL));
}

#ifdef SS_HSBG2

/*##**********************************************************************\
 *
 *              dbe_rflog_catchup_init
 *
 * Creates a roll-forward log object for use in HSBG2 disk-based catchup
 *
 * Parameters :
 *
 *      cfg - in, use
 *              pointer to configuration object
 *
 *      counter - in out, hold
 *              pointer to counter object
 *
 * Return value - give :
 *      pointer to roll-forward log
 *
 * Limitations  :
 *
 * Globals used :
 */

dbe_rflog_t* dbe_rflog_catchup_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_counter_t* counter,
        dbe_catchup_logpos_t startpos)
{
        ss_dprintf_1(("dbe_rflog_catchup_init\n"));

        return(rflog_init(cfg, cd, counter, &startpos));
}

/*##**********************************************************************\
 *
 *              dbe_rflog_hsbinit
 *
 * Init rflog for HSB secondary executor. Executes one memory buffer.
 *
 * Parameters :
 *
 *              lb -
 *
 *
 *              bufsize -
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
dbe_rflog_t* dbe_rflog_hsbinit(
        dbe_logbuf_t* lb,
        size_t bufsize)
{
        dbe_rflog_t* rflog;

        ss_pprintf_1(("dbe_rflog_hsbinit:bufsize %ld\n", bufsize));

        rflog = SSMEM_NEW(dbe_rflog_t);

        rflog->rfl_filespec_to_logfnum = su_pa_init();
        rflog->rfl_currbufsize = bufsize;

        rflog->rfl_currbufdatasize = rflog->rfl_currbufsize - (2 * sizeof(ss_uint2_t));

        ss_pprintf_2(("dbe_rflog_hsbinit:rflog->rfl_currbufsize=%ld, rflog->rfl_currbufdatasize=%ld\n",
            rflog->rfl_currbufsize, rflog->rfl_currbufdatasize));
        rflog->rfl_startpos = 0L;
        rflog->rfl_buffer = lb;
        rflog->rfl_counter = NULL;
        rflog->rfl_mbsvfil = NULL;
        rflog->rfl_fsize = rflog->rfl_currbufsize;

        rflog->rfl_editbufsize = 200;
        rflog->rfl_editbuf = SsMemAlloc(rflog->rfl_editbufsize);
        rflog->rfl_vtpl = NULL;
        rflog->rfl_binarymode = FALSE;
        rflog->rfl_relid = 0L;
        rflog->rfl_lastlogreclp.lp_daddr = SU_DADDR_NULL;
        rflog->rfl_lastlogreclp.lp_bufpos = 0;
        rflog->rfl_lp.lp_daddr = 0L;
        rflog->rfl_lp.lp_bufpos = 0;
        rflog->rfl_endreached = FALSE;
        rflog->rfl_datasize = 0;
        rflog->rfl_lastlogrectype = DBE_LOGREC_NOP;
        rflog->rfl_hsblog = TRUE;
#ifdef SS_MME
        rflog->rfl_rvalp = NULL;
#endif /* SS_MME */

        return (rflog);
}
#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_rflog_done
 *
 * Deletes a foll-forward log object
 *
 * Parameters :
 *
 *      rflog - in, take
 *              pointer to roll-forward log
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_rflog_done(
        dbe_rflog_t* rflog)
{
        ss_dprintf_1(("dbe_rflog_done\n"));

#ifdef SS_HSBG2
        if (!rflog->rfl_hsblog)
#endif /* SS_HSBG2 */
        {
            dbe_lb_done(rflog->rfl_buffer);
            SsMemFree(rflog->rfl_nametemplate);
            SsMemFree(rflog->rfl_logdir);
            su_mbsvf_done(rflog->rfl_mbsvfil);
        }

#ifdef SS_HSBG2
        if(rflog->rfl_filespec_to_logfnum != NULL) {
            su_pa_done(rflog->rfl_filespec_to_logfnum);
        }
#endif /* SS_HSBG2 */

        SsMemFree(rflog->rfl_editbuf);
        SsMemFree(rflog);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getremainingbufsize
 *
 * Returns num,ner of byutes remainign in current data buffer. In HSB it
 * return real number of bytes left. During recovery data is reads from
 * the disk so the value does not tell the number of bytes left in recovery,
 * just number of bytes before next block is read from disk.
 *
 * Parameters :
 *
 *      rflog -
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
size_t dbe_rflog_getremainingbufsize(
        dbe_rflog_t* rflog)
{
        return(rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_resetscan
 *
 * Resets scan to beginning
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_rflog_resetscan(
        dbe_rflog_t* rflog)
{
        dbe_ret_t rc;

        rc = DBE_RC_SUCC;
        rflog->rfl_lp.lp_daddr = rflog->rfl_startpos;
        rflog->rfl_lp.lp_bufpos = 0;
        rflog->rfl_datasize = 0;
        rflog->rfl_endreached = FALSE;
        rflog->rfl_lastlogrectype = DBE_LOGREC_NOP;
        rc = rflog_loadblock(rflog, TRUE);
        su_rc_dassert(rc == DBE_RC_SUCC, rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_skip_unscanned_data
 *
 * Skips unscanned data from roll forward log
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_skip_unscanned_data(dbe_rflog_t *rflog)
{
        su_daddr_t old_scanaddr;
        bool prevdataskipped = FALSE;
        dbe_ret_t rc = DBE_RC_SUCC;

        ss_pprintf_1(("dbe_rflog_skip_unscanned_data\n"));
        SS_PUSHNAME("dbe_rflog_skip_unscanned_data");

        old_scanaddr = rflog->rfl_lp.lp_daddr;

        while (rflog->rfl_datasize != 0) {
            ss_pprintf_2(("dbe_rflog_skip_unscanned_data:(rflog)->rfl_lp.lp_bufpos=%ld\n", (rflog)->rfl_lp.lp_bufpos));
            prevdataskipped = TRUE;
            if (!rflog->rfl_binarymode) {
                if (rflog->rfl_datasize >=
                    rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos)
                {
                    rflog->rfl_datasize -=
                        rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
                    rflog->rfl_lp.lp_bufpos = 0L;
                    if (rflog->rfl_datasize >= rflog->rfl_currbufsize) {
                        rflog->rfl_binarymode = TRUE;
                    }
                    RFLOG_ADDTOLPADDR(rflog,1);
                } else {
                    rflog->rfl_lp.lp_bufpos += rflog->rfl_datasize;
                    rflog->rfl_datasize = 0;
                }
            } else {
                size_t s;

                if (rflog->rfl_lp.lp_bufpos != 0) {
                    s = rflog->rfl_currbufsize - rflog->rfl_lp.lp_bufpos;
                    rflog->rfl_datasize -= s;
                    rflog->rfl_lp.lp_bufpos = 0;
                    RFLOG_ADDTOLPADDR(rflog,1);
                }
                s = rflog->rfl_datasize / rflog->rfl_currbufsize;
                RFLOG_ADDTOLPADDR(rflog, s);
                s *= rflog->rfl_currbufsize;
                rflog->rfl_datasize -= s;
                rflog->rfl_lp.lp_bufpos = 0L;
                rflog->rfl_binarymode = FALSE;
            }
        }

        ss_dassert(rflog->rfl_datasize == 0);

        if (rflog->rfl_lp.lp_daddr != old_scanaddr) {
            ss_dprintf_3(("rflog_skip_unscanned_data: "
                          "rflog->rfl_lp.lp_daddr=%ld,"
                          "old_scanaddr=%ld,"
                          "rflog->rfl_lp.lp_bufpos=%ld\n",
                          (long)rflog->rfl_lp.lp_daddr,
                          (long)old_scanaddr,
                          (long)rflog->rfl_lp.lp_bufpos));
                          
            rc = rflog_loadblock(rflog, FALSE);
        } else {
            rc = rflog_checkbufpos(rflog);
        }

        if (rc == DBE_RC_SUCC || (!prevdataskipped &&
           (rc == DBE_ERR_LOGFILE_CORRUPT || rc == DBE_RC_END))) {
            rflog->rfl_lastlogreclp = rflog->rfl_lp;
        }

        SS_POPNAME;

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getnextrecheader
 *
 * Scans next record header
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_rectype - out, use
 *              pointer to variable where the record type will be put
 *
 *      p_trxid - out, use
 *              pointer to variable where the trx id will be put
 *
 *      p_datasize - out, use
 *              pointer to variable where the data size will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_END when EOF
 *      error code on failure
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_getnextrecheader(
        dbe_rflog_t* rflog,
        dbe_logrectype_t* p_rectype,
        dbe_trxid_t* p_trxid,
        size_t* p_datasize)
{
        dbe_ret_t rc;
        uchar* p;

        if (rflog->rfl_fsize == 0) {
            return (DBE_RC_END);
        }

        ss_pprintf_1(("dbe_rflog_getnextrecheader:(rflog)->rfl_lp.lp_bufpos=%ld\n", (rflog)->rfl_lp.lp_bufpos));

        if (rflog->rfl_endreached) {
            return (DBE_RC_END);
        }
        /*
         * skip over all unscanned data in the log file
         * from previous log record
         *
         */
        rc = dbe_rflog_skip_unscanned_data(rflog);

        if(rc != DBE_RC_SUCC) {
            ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                          __LINE__, (int)rc));
            return (rc);
        }

        ss_dassert(rflog->rfl_datasize == 0);

        /*
         * ping pong flushing writes last block full of NOP log records,
         * this will skip them
         *
         */

#ifdef SS_HSBG2
        rflog->rfl_logrecpos_daddr = rflog->rfl_lp.lp_daddr;
        rflog->rfl_logrecpos_bufpos = rflog->rfl_lp.lp_bufpos;
#endif /* SS_HSBG2 */

        p = DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos;
        *p_rectype = (dbe_logrectype_t)*p;
        rflog->rfl_lastlogrectype = *p_rectype;
        rflog->rfl_lp.lp_bufpos++;
        rc = rflog_checkbufpos(rflog);
        if (rc != DBE_RC_SUCC) {
            ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                          __LINE__, (int)rc));
            return (rc);
        }

        *p_trxid = DBE_TRXID_NULL;
        switch (*p_rectype) {
            case DBE_LOGREC_NOP:
            case DBE_LOGREC_CREATEUSER:
            case DBE_LOGREC_HSBG2_ABORTALL:
                rflog->rfl_datasize = 0;
                *p_datasize = 0;
                break;
            case DBE_LOGREC_HSBG2_NEWSTATE:
                rflog->rfl_datasize = 1;
                *p_datasize = 1;
                break;
            case DBE_LOGREC_CHECKPOINT_OLD:
            case DBE_LOGREC_SNAPSHOT_OLD:
            case DBE_LOGREC_DELSNAPSHOT:
                *p_datasize = rflog->rfl_datasize = sizeof(ss_uint4_t);
                break;
            case DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT:
            case DBE_LOGREC_CHECKPOINT_NEW:
            case DBE_LOGREC_SNAPSHOT_NEW:
                *p_datasize = rflog->rfl_datasize = 2 * sizeof(ss_uint4_t);
                break;
            case DBE_LOGREC_HEADER:
                rflog->rfl_datasize =
                    *p_datasize =
                        LOGFILE_HEADERSIZE;
                break;
            case DBE_LOGREC_INSTUPLE:
#ifndef SS_NOBLOB
            case DBE_LOGREC_INSTUPLEWITHBLOBS:
#endif /* SS_NOBLOB */
            case DBE_LOGREC_INSTUPLENOBLOBS:
            case DBE_LOGREC_DELTUPLE:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                          __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_getrelid(rflog, &rflog->rfl_relid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_getvtplsize(rflog, &rflog->rfl_datasize);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = rflog->rfl_datasize +
                    (size_t)VA_LENLEN((va_t*)rflog->rfl_vtpl);
                break;
#ifdef SS_MME
            case DBE_LOGREC_MME_INSTUPLEWITHBLOBS:
            case DBE_LOGREC_MME_INSTUPLENOBLOBS:
            case DBE_LOGREC_MME_DELTUPLE:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_getrelid(rflog, &rflog->rfl_relid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_getrvalsize(rflog, &rflog->rfl_datasize);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                /* length of the mme-tuple length field is 2 bytes */
                *p_datasize = rflog->rfl_datasize + sizeof(ss_uint2_t);
                ss_bprintf_1(("dbe_rflog_getnextrecheader: rvalsize=%ld\n", (rflog)->rfl_datasize));
                break;
#endif
            case DBE_LOGREC_COMMITTRX_OLD:
            case DBE_LOGREC_COMMITTRX_HSB_OLD:
            case DBE_LOGREC_ABORTTRX_OLD:
            case DBE_LOGREC_ABORTSTMT:
            case DBE_LOGREC_PREPARETRX:
            case DBE_LOGREC_SWITCHTOPRIMARY :
            case DBE_LOGREC_SWITCHTOSECONDARY  :
            case DBE_LOGREC_SWITCHTOSECONDARY_NORESET  :
            case DBE_LOGREC_HSBCOMMITMARK_OLD:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = 0;
                rflog->rfl_datasize = 0;
                break;

            case DBE_LOGREC_COMMITTRX_INFO:
            case DBE_LOGREC_ABORTTRX_INFO:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = 1;
                rflog->rfl_datasize = 1;
                break;

#ifdef SS_HSBG2
            case DBE_LOGREC_HSBG2_DURABLE:
                *p_datasize = rflog->rfl_datasize = DBE_LOGPOS_BINSIZE;
                break;

            case DBE_LOGREC_HSBG2_REMOTE_DURABLE:
            case DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK:
                *p_datasize = rflog->rfl_datasize = 2 * DBE_LOGPOS_BINSIZE;
                break;

            case DBE_LOGREC_HSBG2_NEW_PRIMARY:
                *p_datasize = rflog->rfl_datasize = 2 * sizeof(ss_uint4_t);
                ss_dassert(*p_datasize == 8);
                break;
#endif

            case DBE_LOGREC_CLEANUPMAPPING  :
                *p_datasize = 0;
                rflog->rfl_datasize = 0;
                break;
            case DBE_LOGREC_COMMITSTMT:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = sizeof(ss_uint4_t);
                rflog->rfl_datasize = sizeof(ss_uint4_t);
                break;
#ifndef SS_NOBLOB
            case DBE_LOGREC_BLOBSTART_OLD:
            case DBE_LOGREC_BLOBALLOCLIST_OLD:
            case DBE_LOGREC_BLOBALLOCLIST_CONT_OLD:
            case DBE_LOGREC_BLOBDATA_OLD:
            case DBE_LOGREC_BLOBDATA_CONT_OLD:
#endif /* SS_NOBLOB */
            case DBE_LOGREC_CREATETABLE:
            case DBE_LOGREC_CREATETABLE_NEW:
            case DBE_LOGREC_CREATEINDEX:
            case DBE_LOGREC_DROPTABLE:
            case DBE_LOGREC_TRUNCATETABLE:
            case DBE_LOGREC_TRUNCATECARDIN:
            case DBE_LOGREC_DROPINDEX:
            case DBE_LOGREC_CREATEVIEW:
            case DBE_LOGREC_CREATEVIEW_NEW:
            case DBE_LOGREC_DROPVIEW:
            case DBE_LOGREC_ALTERTABLE:
            case DBE_LOGREC_CREATECTR:
            case DBE_LOGREC_CREATESEQ:
            case DBE_LOGREC_DROPCTR:
            case DBE_LOGREC_DROPSEQ:
            case DBE_LOGREC_RENAMETABLE:
            case DBE_LOGREC_AUDITINFO:
            case DBE_LOGREC_CREATETABLE_FULLYQUALIFIED:
            case DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED:
            case DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                /* FALLTHROUGH */
            case DBE_LOGREC_BLOBG2DATA:
                rc = rflog_getdatasize(rflog, &rflog->rfl_datasize);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = rflog->rfl_datasize;
                break;
            case DBE_LOGREC_BLOBG2DATACOMPLETE:
            case DBE_LOGREC_BLOBG2DROPMEMORYREF:
                rflog->rfl_datasize =
                    *p_datasize =
                        sizeof(ss_int8_t);
                ss_dassert(*p_datasize == 8);
                break;
            case DBE_LOGREC_INCSYSCTR:
                rflog->rfl_datasize =
                    *p_datasize =
                        sizeof(uchar);
                ss_dassert(*p_datasize == 1);
                break;
            case DBE_LOGREC_SETHSBSYSCTR:
                rflog->rfl_datasize =
                    *p_datasize =
                        DBE_HSBSYSCTR_SIZE;
                ss_dassert(*p_datasize == DBE_HSBSYSCTR_SIZE);
                break;
            case DBE_LOGREC_INCCTR:
                rflog->rfl_datasize =
                    *p_datasize =
                        sizeof(ss_uint4_t);
                ss_dassert(*p_datasize == 4);
                break;
            case DBE_LOGREC_SETCTR:
                rflog->rfl_datasize =
                    *p_datasize =
                        sizeof(ss_uint4_t) * 3;
                ss_dassert(*p_datasize == 12);
                break;
            case DBE_LOGREC_SETSEQ:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rc = rflog_checkbufpos(rflog);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                rflog->rfl_datasize =
                    *p_datasize =
                        sizeof(ss_uint4_t) * 3;
                ss_dassert(*p_datasize == 12);
                break;

            case DBE_LOGREC_REPLICATRXSTART:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize = sizeof(ss_uint4_t);
                rflog->rfl_datasize = sizeof(ss_uint4_t);
                break;
            case DBE_LOGREC_REPLICASTMTSTART:
                rc = rflog_gettrxid(rflog, p_trxid);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
                *p_datasize =
                    rflog->rfl_datasize =
                        2 * sizeof(ss_uint4_t);
                break;

            case DBE_LOGREC_COMMENT:
                *p_datasize = 0;
                rflog->rfl_datasize = 0;
                break;

            default:

#ifdef SS_HSBG2
                if (rflog->rfl_hsblog) {
                    ss_rc_error(*p_rectype);
                }
#endif
                ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d, logrec %d\n",
                              __LINE__, (int)DBE_ERR_LOGFILE_CORRUPT, *p_rectype));
#ifdef AUTOTEST_RUN
                ss_rc_derror(*p_rectype);
#endif
                return (DBE_ERR_LOGFILE_CORRUPT);
        }

        if (rflog->rfl_lp.lp_bufpos >= rflog->rfl_currbufdatasize
        &&  rflog->rfl_datasize > 0)
        {
            ss_dassert(rflog->rfl_lp.lp_bufpos == rflog->rfl_currbufdatasize);
            RFLOG_ADDTOLPADDR(rflog, 1);
            if (rflog->rfl_datasize >= rflog->rfl_currbufsize) {
                rflog->rfl_binarymode = TRUE;
                rflog->rfl_lp.lp_bufpos = 0;
            } else {
                rc = rflog_loadblock(rflog, TRUE);
                if (rc != DBE_RC_SUCC) {
                    ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                                  __LINE__, (int)rc));
                    return (rc);
                }
            }
        }

        ss_dprintf_3(("dbe_rflog_getnextrecheader: line %d returning %d\n",
                      __LINE__, (int)rc));
        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_rflog_readdata
 *
 * Reads data of a log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      buffer - out, use
 *              pointer to buffer where the data will be read
 *
 *      bytes_desired - in
 *              # of bytes required
 *
 *      p_bytes_read - out
 *              pointer to variable where the amount of bytes actually read
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_CONT when bytes_desired < datasize
 *      DBE_RC_END when EOF or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_readdata(
        dbe_rflog_t* rflog,
        void* buffer,
        size_t bytes_desired,
        size_t* p_bytes_read)
{
        dbe_ret_t rc;
        size_t ntoread;
        size_t bytesinblock;
        size_t nblocks;

        ss_dprintf_1(("dbe_rflog_readdata:bytes_desired=%d, rflog->rfl_datasize=%d\n", bytes_desired, rflog->rfl_datasize));
        rc = DBE_RC_SUCC;
        *p_bytes_read = 0;
        while (bytes_desired && rflog->rfl_datasize) {
            if (!rflog->rfl_binarymode) {
                ntoread = rflog->rfl_datasize;
                ss_dprintf_2(("dbe_rflog_readdata:ntoread=%d\n", ntoread));
                if (bytes_desired < rflog->rfl_datasize) {
                    ss_dprintf_2(("dbe_rflog_readdata:ntoread=%d\n", ntoread));
                    ntoread = bytes_desired;
                }
                bytesinblock = rflog->rfl_currbufdatasize - rflog->rfl_lp.lp_bufpos;
                ss_dprintf_2(("dbe_rflog_readdata:bytesinblock=%d\n", bytesinblock));
                if (bytesinblock > ntoread) {
                    ss_dprintf_2(("dbe_rflog_readdata:bytesinblock=%d\n", bytesinblock));
                    bytesinblock = ntoread;
                }
                memcpy(
                    buffer,
                    DBE_LB_DATA(rflog->rfl_buffer) + rflog->rfl_lp.lp_bufpos,
                    bytesinblock);
                bytes_desired -= bytesinblock;
                rflog->rfl_datasize -= bytesinblock;
                *p_bytes_read += bytesinblock;
                buffer = (uchar*)buffer + bytesinblock;
                rflog->rfl_lp.lp_bufpos += bytesinblock;

                ss_dprintf_2(("dbe_rflog_readdata:rflog->rfl_lp.lp_bufpos=%ld, rflog->rfl_currbufdatasize=%ld\n",
                    (long)rflog->rfl_lp.lp_bufpos, (long)rflog->rfl_currbufdatasize));

                if (rflog->rfl_lp.lp_bufpos >= rflog->rfl_currbufdatasize) {
#ifdef SS_HSBG2
                    if (rflog->rfl_hsblog) {
                        ss_dprintf_2(("dbe_rflog_readdata:DBE_RC_END\n", bytesinblock));
                        if (rflog->rfl_datasize == 0) {
                            ss_dassert(bytes_desired == 0);
                            rc = DBE_RC_SUCC;
                        } else {
                            rc = DBE_RC_END;
                        }
                        break;
                    }
#endif /* SS_HSBG2 */
                    ss_dassert(rflog->rfl_lp.lp_bufpos == rflog->rfl_currbufdatasize);
                    RFLOG_ADDTOLPADDR(rflog, 1);
                    if (rflog->rfl_lp.lp_daddr >= rflog->rfl_fsize) {
                        ss_dassert(rflog->rfl_lp.lp_daddr == rflog->rfl_fsize);
                        rflog->rfl_endreached = TRUE;
                        if (rflog->rfl_datasize != 0) {
                            rc = DBE_RC_END;
                        } else {
                            ss_dassert(bytes_desired == 0);
                        }
                        break;
                    }
                    if (rflog->rfl_datasize >= rflog->rfl_currbufsize) {
                        rflog->rfl_lp.lp_bufpos = 0;
                        rflog->rfl_binarymode = TRUE;
                    } else {
                        rc = rflog_loadblock(rflog, TRUE);
                        if (rc != DBE_RC_SUCC) {
                            if (rc == DBE_RC_END
                                &&  rflog->rfl_datasize == 0)
                            {
                                ss_dassert(bytes_desired == 0);
                                rflog->rfl_endreached = TRUE;
                                rc = DBE_RC_SUCC;
                            }
                            break;
                        }
                    }
                }
                continue;
            } else {    /* we are in binary read mode (more data
                           than size of single buffer requested,
                           data is read directly to the buffer) */

                ss_dassert(rflog->rfl_datasize >=
                    rflog->rfl_currbufsize - rflog->rfl_lp.lp_bufpos);
                if (rflog->rfl_lp.lp_bufpos == 0) {

                    /*
                     * nblocks -- how many blocks we need to read in
                     *            order to satisfy the caller of this function
                     *
                     * nblocks2 -- how many blocks we need to read in
                     *             in total in order to read the complete
                     *             log record from disk
                     *
                     * in here we need to read anyway, so we actually read
                     * one more block than those requested
                     *
                     */

                    size_t nblocks2;
                    ntoread = (size_t)rflog->rfl_datasize;
                    nblocks2 = ntoread / rflog->rfl_currbufsize;
                    if (bytes_desired < ntoread) {
                        ntoread = bytes_desired;
                    }
                    nblocks = ntoread / rflog->rfl_currbufsize;
                    if (nblocks != 0) {
                        bytesinblock = nblocks * rflog->rfl_currbufsize;
                        rc = rflog_read_pages(rflog, rflog->rfl_lp.lp_daddr,
                                              buffer, bytesinblock);
                        if (rc != SU_SUCCESS) {
                            break;
                        }

                        rflog->rfl_lp.lp_bufpos = 0;
                        rflog->rfl_lp.lp_daddr += bytesinblock / rflog->rfl_addressing_size;
                        if (nblocks2 == nblocks) {
                            /*
                             * we have reached the last block in the
                             * binary mode, return to normal mode
                             *
                             */

                            rflog->rfl_binarymode = FALSE;
                            rc = rflog_loadblock(rflog, TRUE);
                        }
                    } else {
                        bytesinblock = ntoread;
                        rc = rflog_read_rfl(rflog);
                        if (rc != SU_SUCCESS) {
                            break;
                        }
                        memcpy(buffer, rflog->rfl_buffer, bytesinblock);
                        rflog->rfl_lp.lp_bufpos += bytesinblock;
                    }
                    buffer = (uchar*)buffer + bytesinblock;
                    rflog->rfl_datasize -= bytesinblock;
                    bytes_desired -= bytesinblock;
                    *p_bytes_read += bytesinblock;
                } else { /* we are in the middle of a buffer */
                    ntoread = rflog->rfl_currbufsize - rflog->rfl_lp.lp_bufpos;
                    if (bytes_desired < ntoread) {
                        ntoread = bytes_desired;
                    }
                    bytesinblock = ntoread;
                    ss_dassert(rflog->rfl_datasize >= ntoread);
                    memcpy(
                        buffer,
                        (uchar*)rflog->rfl_buffer + rflog->rfl_lp.lp_bufpos,
                        bytesinblock);
                    bytes_desired -= bytesinblock;
                    rflog->rfl_datasize -= bytesinblock;
                    *p_bytes_read += bytesinblock;
                    buffer = (uchar*)buffer + bytesinblock;
                    rflog->rfl_lp.lp_bufpos += bytesinblock;
                    if (rflog->rfl_lp.lp_bufpos >= rflog->rfl_currbufsize) {
                        ss_dassert(rflog->rfl_lp.lp_bufpos ==
                            rflog->rfl_currbufsize);
                        RFLOG_ADDTOLPADDR(rflog, 1);
                        if (rflog->rfl_lp.lp_daddr >= rflog->rfl_fsize) {
                            rc = DBE_RC_END;
                            break;
                        }
                        if (rflog->rfl_datasize >= rflog->rfl_currbufsize) {
                            rflog->rfl_lp.lp_bufpos = 0;
                        } else {
                            rflog->rfl_binarymode = FALSE;
                            rc = rflog_loadblock(rflog, TRUE);
                            if (rc != DBE_RC_SUCC) {
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (rc == DBE_RC_SUCC
        && bytes_desired == 0
        && rflog->rfl_datasize != 0)
        {
            rc = DBE_RC_CONT;
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getvtupleref
 *
 * Gets reference to vtuple
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      pp_vtpl - out, ref
 *              pointer to pointer var where the pointer to vtuple will be put
 *
 *      p_relid - out
 *          pointer to variable where the relid of the tuple will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_getvtupleref(
        dbe_rflog_t* rflog,
        vtpl_t** pp_vtpl,
        ulong* p_relid)
{
        uchar* p;
        vtpl_t* p_vtpl;
        dbe_ret_t rc;
        size_t bytesread;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_INSTUPLE
                || rflog->rfl_lastlogrectype == DBE_LOGREC_INSTUPLEWITHBLOBS
                || rflog->rfl_lastlogrectype == DBE_LOGREC_INSTUPLENOBLOBS
                || rflog->rfl_lastlogrectype == DBE_LOGREC_DELTUPLE);
        rc = DBE_RC_SUCC;
        *p_relid = rflog->rfl_relid;
        p = (uchar*)rflog->rfl_vtpl;
        p_vtpl = rflog->rfl_vtpl;
        p += VA_LENLEN((va_t*)p);
        if ((void*)rflog->rfl_vtpl == rflog->rfl_editbuf) {
            /* the vtuple must be copied into the editbuf, because
             * it crosses block boundar(y|ies).
             */
            rc = dbe_rflog_readdata(
                    rflog,
                    p,
                    rflog->rfl_datasize,
                    &bytesread);

        } else {
            /* the whole vtuple resides in one logfile disk block.
             * We can avoid the extra copying and give the reference
             * directly into the disk buffer
             */
            rflog->rfl_lp.lp_bufpos += rflog->rfl_datasize;
            rflog->rfl_datasize = 0;
        }
        *pp_vtpl = p_vtpl;

        return (rc);
}

#ifdef SS_MME
/*##**********************************************************************\
 *
 *              dbe_rflog_getrval
 *
 * Gets a MME row value object in GIVE mode.
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      pp_rval - out, give
 *              pointer to pointer var where the pointer to mme_rval_t will be put
 *
 *      p_relid - out
 *          pointer to variable where the relid of the tuple will be put
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF or
 *      error code
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_getrval(
        rs_sysi_t*      cd,
        dbe_rflog_t*    rflog,
        mme_rval_t**    pp_rval,
        ulong*          p_relid)
{
        uchar* p;
        dbe_ret_t rc;
        size_t bytesread;
        size_t length;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_MME_INSTUPLEWITHBLOBS
                || rflog->rfl_lastlogrectype == DBE_LOGREC_MME_INSTUPLENOBLOBS
                || rflog->rfl_lastlogrectype == DBE_LOGREC_MME_DELTUPLE);
        rc = DBE_RC_SUCC;
        *p_relid = rflog->rfl_relid;
        p = (uchar*)rflog->rfl_rvalp;
        length = rflog->rfl_datasize;
        if ((void*)rflog->rfl_rvalp == rflog->rfl_editbuf) {
            /* the rval must be copied into the editbuf, because
             * it crosses block boundar(y|ies).
             */
            ss_dprintf_1(("dbe_rflog_getrval: copy to editbuf: (rflog)->rfl_lp.lp_bufpos=%ld, datasize: %ld\n", (rflog)->rfl_lp.lp_bufpos, rflog->rfl_datasize));
            ss_rc_bassert(rflog->rfl_datasize <= rflog->rfl_editbufsize, rflog->rfl_datasize);
            rc = dbe_rflog_readdata(
                    rflog,
                    p,
                    rflog->rfl_datasize,
                    &bytesread);
            if (rc != DBE_RC_SUCC) {
                ss_debug(*pp_rval = (void*)(long)0xDeadBeef);
                return (rc);
            }
        } else {
            /* the whole row value resides in one logfile disk block.
             */
            ss_dprintf_1(("dbe_rflog_getrval: in one block: (rflog)->rfl_lp.lp_bufpos=%ld, datasize: %ld\n", (rflog)->rfl_lp.lp_bufpos, rflog->rfl_datasize));
            rflog->rfl_lp.lp_bufpos += rflog->rfl_datasize;
            rflog->rfl_datasize = 0;
        }
        *pp_rval = dbe_mme_rval_init_from_diskbuf(
                    cd, p, length, NULL, NULL, MME_RVAL_NORMAL);

        return (rc);
}
#endif /* SS_MME */

/*##**********************************************************************\
 *
 *              dbe_rflog_getlogheaderdata
 *
 * Gets log file header data
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_logfnum - out, use
 *              pointer to var where to store log file #
 *
 *      p_cpnum - out, use
 *              pointer to var where to store cpnum
 *
 *      p_blocksize - out, use
 *              pointer to var where to store log file block size
 *
 *      p_dbcreatime - out, use
 *          pointer to variable where to store the database creation time
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_getlogheaderdata(
        dbe_rflog_t* rflog,
        dbe_logfnum_t* p_logfnum,
        dbe_cpnum_t* p_cpnum,
        dbe_hdr_blocksize_t* p_blocksize,
        ss_uint4_t* p_dbcreatime)
{
        dbe_ret_t rc;
        char hdrdatabuf[LOGFILE_HEADERSIZE];
        char* p;
        size_t bytesread;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_HEADER);
        p = hdrdatabuf;
        rc = dbe_rflog_readdata(
                rflog,
                p,
                sizeof(hdrdatabuf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(hdrdatabuf));
            *p_logfnum = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(*p_logfnum);
            *p_cpnum = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(*p_cpnum);
            *p_blocksize = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(*p_blocksize);
            *p_dbcreatime = SS_UINT4_LOADFROMDISK(p);
        }
        return (rc);
}


/*##**********************************************************************\
 *
 *              dbe_rflog_getcpmarkdata_old
 *
 * Gets data (checkpoint/snapshot number) about global mark record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_cpnum - out, use
 *              pointer to variable where the cpnum will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_rflog_getcpmarkdata_old(
        dbe_rflog_t* rflog,
        dbe_cpnum_t* p_cpnum)
{
        dbe_ret_t rc;
        char cpnumbuf[sizeof(ss_uint4_t)];
        ss_uint4_t i4;
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_CHECKPOINT_OLD ||
            rflog->rfl_lastlogrectype == DBE_LOGREC_SNAPSHOT_OLD ||
            rflog->rfl_lastlogrectype == DBE_LOGREC_DELSNAPSHOT);
        rc = dbe_rflog_readdata(
                rflog,
                cpnumbuf,
                sizeof(cpnumbuf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(cpnumbuf));
            i4 = SS_UINT4_LOADFROMDISK(cpnumbuf);
            *p_cpnum = (dbe_cpnum_t)i4;
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getcpmarkdata_new
 *
 * Gets a Checkpoint/snapshot mark record with timestamp
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_cpnum - out
 *              pointer to checkpointer number variable
 *
 *      p_ts - out
 *              pointer to timestamp variable
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getcpmarkdata_new(
        dbe_rflog_t* rflog,
        dbe_cpnum_t* p_cpnum,
        SsTimeT* p_ts)
{
        dbe_ret_t rc;
        char buf[2 * sizeof(ss_uint4_t)];
        size_t bytesread;
        ss_uint4_t i4;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_CHECKPOINT_NEW ||
            rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_REMOTE_CHECKPOINT ||
            rflog->rfl_lastlogrectype == DBE_LOGREC_SNAPSHOT_NEW);
        ss_dassert(p_cpnum != NULL);
        ss_dassert(p_ts != NULL);
        rc = dbe_rflog_readdata(
                rflog,
                buf,
                sizeof(buf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(buf));
            i4 = SS_UINT4_LOADFROMDISK(buf);
            *p_cpnum = (dbe_cpnum_t)i4;
            i4 = SS_UINT4_LOADFROMDISK(buf + sizeof(ss_uint4_t));
            *p_ts = (SsTimeT)i4;
        }
        return (rc);
}




/*##**********************************************************************\
 *
 *              dbe_rflog_getcreatetable
 *
 * Gets info about create table/view log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relid - out, use
 *              pointer to variable where the relation id will be stored
 *
 *      p_relschema - out, ref use
 *              if non-NULL, pointer to variable where the reference to
 *          relation schema will be stored
 *
 *      p_relname - out, ref use
 *              pointer to variable where the reference to relation name
 *          will be stored
 *
 *      p_nkeys - out
 *          pointer to variable where number of indexes created
 *          including clustering key will be stored
 *
 *      p_nattrs - out
 *          pointer to variable where number of attributes
 *          (including hidden attributes) will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getcreatetable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        rs_entname_t* name,
        rs_ano_t* p_nkeys,
        rs_ano_t* p_nattrs)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint2_t i2;
        ss_uint4_t i4;
        char* relschema = NULL;
        char* relname;
        size_t relname_len_plus_1 = 1;
        size_t relschema_len_plus_1 = 1;
        char* relcatalog = NULL;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_CREATETABLE ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_CREATEVIEW ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_CREATETABLE_NEW ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_CREATEVIEW_NEW ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_CREATETABLE_FULLYQUALIFIED ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED);
        ss_dassert(p_relid != NULL);
        ss_dassert(name != NULL);
        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_relid = (ulong)i4;
            i2 = SS_UINT2_LOADFROMDISK(
                    (uchar*)rflog->rfl_editbuf + sizeof(ss_uint4_t));
            *p_nkeys = i2;
            i2 = SS_UINT2_LOADFROMDISK(
                (uchar*)rflog->rfl_editbuf + sizeof(ss_uint4_t) + sizeof(i2));
            *p_nattrs = i2;
            relname = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t) +
                         sizeof(i2) * 2;
            if (rflog->rfl_lastlogrectype != DBE_LOGREC_CREATETABLE &&
                rflog->rfl_lastlogrectype != DBE_LOGREC_CREATEVIEW)
            {
                /* old format did not contain schema */
                relname_len_plus_1 = strlen(relname) + 1;
                relschema = (char*)rflog->rfl_editbuf +
                    sizeof(ss_uint4_t) +
                    sizeof(i2) * 2 +
                    relname_len_plus_1;
            }
            if (rflog->rfl_lastlogrectype == DBE_LOGREC_CREATETABLE_FULLYQUALIFIED
             || rflog->rfl_lastlogrectype == DBE_LOGREC_CREATEVIEW_FULLYQUALIFIED)
            {
                relschema_len_plus_1 = strlen(relschema) + 1;
                relcatalog = (char*)rflog->rfl_editbuf +
                    sizeof(ss_uint4_t) +
                    sizeof(i2) * 2 +
                    relname_len_plus_1 +
                    relschema_len_plus_1;
            }
            rs_entname_initbuf(name, relcatalog, relschema, relname);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getrenametable
 *
 *
 *
 * Parameters :
 *
 *      rflog -
 *
 *
 *      p_relid -
 *
 *
 *      p_newschema -
 *
 *
 *      p_newname -
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
dbe_ret_t dbe_rflog_getrenametable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        rs_entname_t* name)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint4_t i4;
        char* newschema;
        char* newname;
        size_t newname_len_plus_1;
        size_t newschema_len_plus_1;
        char* newcatalog = NULL;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_RENAMETABLE
          || rflog->rfl_lastlogrectype == DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED);
        ss_dassert(p_relid != NULL);
        ss_dassert(name != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_relid = (ulong)i4;
            newname = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t);
            newname_len_plus_1 = strlen(newname) + 1;
            newschema = (char*)rflog->rfl_editbuf +
                            sizeof(ss_uint4_t) +
                            newname_len_plus_1;
            if (rflog->rfl_lastlogrectype ==
                DBE_LOGREC_RENAMETABLE_FULLYQUALIFIED)
            {
                newschema_len_plus_1 = strlen(newschema) + 1;
                newcatalog = (char*)rflog->rfl_editbuf +
                    sizeof(ss_uint4_t) +
                    newname_len_plus_1 +
                    newschema_len_plus_1;
            }
            rs_entname_initbuf(name, newcatalog, newschema, newname);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getdroptable
 *
 * Gets info from drop table/view record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relid - out, use
 *              pointer to variable where the relation id will be stored
 *
 *      p_relname - out, ref use
 *              pointer to variable where the reference to relation name
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getdroptable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        char** p_relname)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint4_t i4;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_DROPTABLE ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_DROPVIEW ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_TRUNCATECARDIN ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_TRUNCATETABLE);
        ss_dassert(p_relid != NULL);
        ss_dassert(p_relname != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_relid = (ulong)i4;
            *p_relname = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getaltertable
 *
 * Gets info from ALTER TABLE log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relid - out, use
 *              pointer to variable where the relation id will be stored
 *
 *      p_relname - out, ref use
 *              pointer to variable where the reference to relation name
 *          will be stored
 *
 *      p_nnewattrs - out, use
 *              pointer to variable where the # of new attributes will be
 *          stored.
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getaltertable(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        char** p_relname,
        rs_ano_t* p_nnewattrs)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint2_t i2;
        ss_uint4_t i4;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_ALTERTABLE);
        ss_dassert(p_relid != NULL);
        ss_dassert(p_relname != NULL);
        ss_dassert(p_nnewattrs != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_relid = (ulong)i4;
            i2 = SS_UINT2_LOADFROMDISK(
                (uchar*)rflog->rfl_editbuf + sizeof(ss_uint4_t));
            *p_nnewattrs = i2;
            *p_relname =
                (char*)rflog->rfl_editbuf +
                sizeof(ss_uint4_t) +
                sizeof(i2);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getcreateordropindex
 *
 * Gets info about create/drop index log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_relid - out, use
 *              pointer to variable where the relation id will be stored
 *
 *      p_keyid - out, use
 *              pointer to variable where the key id will be stored
 *
 *      p_relname - out, ref use
 *              pointer to variable where the reference to relation name
 *          will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getcreateordropindex(
        dbe_rflog_t* rflog,
        ulong* p_relid,
        ulong* p_keyid,
        char** p_relname)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint4_t i4;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_CREATEINDEX ||
                   rflog->rfl_lastlogrectype == DBE_LOGREC_DROPINDEX);
        ss_dassert(p_relid != NULL);
        ss_dassert(p_keyid != NULL);
        ss_dassert(p_relname != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_relid = (ulong)i4;
            i4 = SS_UINT4_LOADFROMDISK(
                (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t));
            *p_keyid = (ulong)i4;
            *p_relname = (char*)rflog->rfl_editbuf + 2 * sizeof(ss_uint4_t);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getcommitstmt
 *
 * Gets statement trx id from Commit Statement -record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_stmttrxid - out
 *              pointer to variable where the stmt trx id will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getcommitstmt(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_stmttrxid)
{
        dbe_ret_t rc;
        char stmttrxidbuf[sizeof(ss_uint4_t)];
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_COMMITSTMT)
        rc = dbe_rflog_readdata(
                rflog,
                stmttrxidbuf,
                sizeof(stmttrxidbuf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(stmttrxidbuf));
            *p_stmttrxid = DBE_TRXID_INIT(SS_UINT4_LOADFROMDISK(stmttrxidbuf));
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getcommitinfo
 *
 * Returns commit info bits.
 *
 * Parameters :
 *
 *              rflog -
 *
 *
 *              p_info -
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
dbe_ret_t dbe_rflog_getcommitinfo(
        dbe_rflog_t* rflog,
        dbe_logi_commitinfo_t* p_info)
{
        dbe_ret_t rc;
        ss_byte_t infobuf[1];
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_COMMITTRX_INFO ||
            rflog->rfl_lastlogrectype == DBE_LOGREC_ABORTTRX_INFO)
        rc = dbe_rflog_readdata(
                rflog,
                infobuf,
                1,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == 1);
            *p_info = (dbe_logi_commitinfo_t)infobuf[0];
            ss_rc_dassert(DBE_LOGI_ISVALIDCOMMITINFO(*p_info), *p_info);
        }
        return (rc);
}

#ifdef SS_HSBG2
/*##**********************************************************************\
 *
 *              dbe_rflog_get_durable
 *
 * Gets durable log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      local_durable_logpos - out
 *              pointer to variable where the local logpos will be written
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_get_durable(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *p_local_durable_logpos)
{
        dbe_ret_t rc;
        ss_byte_t buf[DBE_LOGPOS_BINSIZE];
        size_t bytesread;

        DBE_CATCHUP_LOGPOS_SET_NULL(*p_local_durable_logpos);

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_DURABLE);

        rc = dbe_rflog_readdata(
                rflog,
                buf,
                sizeof(buf),
                &bytesread);

        ss_rc_dassert(rc == DBE_RC_SUCC, rc);

        if (rc == DBE_RC_SUCC) {
            char* p;
            ss_dassert(bytesread == sizeof(buf));
            p = (char *)buf;
#ifdef HSB_LPID
            p_local_durable_logpos->lp_id = LPID_LOADFROMDISK(p);
            p += sizeof(dbe_hsb_lpid_t);
            p_local_durable_logpos->lp_role = *p;
            p += 1;
#endif
            p_local_durable_logpos->lp_logfnum = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_local_durable_logpos->lp_daddr = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_local_durable_logpos->lp_bufpos = SS_UINT4_LOADFROMDISK(p);

            ss_dassert(!DBE_CATCHUP_LOGPOS_ISNULL(*p_local_durable_logpos));

        }

        dbe_catchup_logpos_check(*p_local_durable_logpos);

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_get_remote_durable
 *
 * Gets remote durable log record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_local_durable_logpos - out
 *              pointer to variable where the local logpos will be written
 *
 *      p_remote_durable_logpos - out
 *              pointer to variable where the remote logpos will be written
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_get_remote_durable(
        dbe_rflog_t* rflog,
        dbe_catchup_logpos_t *p_local_durable_logpos,
        dbe_catchup_logpos_t *p_remote_durable_logpos)
{
        dbe_ret_t rc;
        char buf[2 * DBE_LOGPOS_BINSIZE];
        size_t bytesread;

        DBE_CATCHUP_LOGPOS_SET_NULL(*p_local_durable_logpos);
        DBE_CATCHUP_LOGPOS_SET_NULL(*p_remote_durable_logpos);

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE
                  || rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_REMOTE_DURABLE_ACK);

        rc = dbe_rflog_readdata(
                rflog,
                buf,
                sizeof(buf),
                &bytesread);

        if (rc == DBE_RC_SUCC) {
            char* p;
            ss_dassert(bytesread == sizeof(buf));

            /* read local durable logpos */

            p = buf;
#ifdef HSB_LPID
            p_local_durable_logpos->lp_id = LPID_LOADFROMDISK(p);
            p += sizeof(dbe_hsb_lpid_t);
            p_local_durable_logpos->lp_role = *p;
            p += 1;
#endif
            p_local_durable_logpos->lp_logfnum = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_local_durable_logpos->lp_daddr = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_local_durable_logpos->lp_bufpos = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);

            /* read remote durable logpos */

#ifdef HSB_LPID
            p_remote_durable_logpos->lp_id = LPID_LOADFROMDISK(p);
            p += sizeof(dbe_hsb_lpid_t);
            p_remote_durable_logpos->lp_role = *p;
            p += 1;
#endif
            p_remote_durable_logpos->lp_logfnum = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_remote_durable_logpos->lp_daddr = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            p_remote_durable_logpos->lp_bufpos = SS_UINT4_LOADFROMDISK(p);

            dbe_catchup_logpos_check(*p_local_durable_logpos);
            dbe_catchup_logpos_check(*p_remote_durable_logpos);
        }

        return (rc);
}

dbe_ret_t dbe_rflog_get_hsbnewstate(
        dbe_rflog_t* rflog,
        dbe_hsbstatelabel_t* p_state)
{
        dbe_ret_t rc;
        ss_byte_t buf[1];
        size_t bytesread;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_NEWSTATE);

        rc = dbe_rflog_readdata(
                rflog,
                buf,
                sizeof(buf),
                &bytesread);

        ss_rc_dassert(rc == DBE_RC_SUCC, rc);

        if (rc == DBE_RC_SUCC) {
            *p_state = (dbe_hsbstatelabel_t)buf[0];
            ss_dassert(*p_state < HSB_STATE_MAX);
        }

        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_get_hsb_new_primary
 *
 * Gets node ids from new primary logrec.
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_originator_nodeid - out
 *              pointer to variable where the originator node id will be stored
 *
 *      p_primary_nodeid - out
 *              pointer to variable where the primary node id will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_get_hsb_new_primary(
        dbe_rflog_t* rflog,
        long *p_originator_nodeid,
        long *p_primary_nodeid)
{
        dbe_ret_t rc;
        char buf[2 * sizeof(ss_uint4_t)];
        size_t bytesread;

        *p_originator_nodeid = 0;
        *p_primary_nodeid = 0;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_HSBG2_NEW_PRIMARY);
        rc = dbe_rflog_readdata(
                rflog,
                buf,
                sizeof(buf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            void *p;
            ss_dassert(bytesread == sizeof(buf));
            p = buf;
            *p_originator_nodeid = SS_UINT4_LOADFROMDISK(p);
            p = buf + sizeof(ss_uint4_t);
            *p_primary_nodeid = SS_UINT4_LOADFROMDISK(p);
        }

        return (rc);
}
#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_rflog_getreplicatrxstart
 *
 * Gets statement trx id from Commit Statement -record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_remotetrxid - out
 *              pointer to variable where the stmt trx id will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getreplicatrxstart(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_remotetrxid)
{
        dbe_ret_t rc;
        char remotetrxidbuf[sizeof(ss_uint4_t)];
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_REPLICATRXSTART);
        rc = dbe_rflog_readdata(
                rflog,
                remotetrxidbuf,
                sizeof(remotetrxidbuf),
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(remotetrxidbuf));
            *p_remotetrxid =  DBE_TRXID_INIT(SS_UINT4_LOADFROMDISK(remotetrxidbuf));
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getreplicastmtstart
 *
 * Gets statement stmt id from Commit Statement -record
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      p_remotestmtid - out
 *              pointer to variable where the remote stmt trx id will be stored
 *
 *      p_localstmtid - out
 *              pointer to variable where the local stmt trx id will be stored
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getreplicastmtstart(
        dbe_rflog_t* rflog,
        dbe_trxid_t* p_remotestmtid,
        dbe_trxid_t* p_localstmtid)
{
        dbe_ret_t rc;
        char stmtidbuf[2 * sizeof(ss_uint4_t)];
        size_t bytesread;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_REPLICASTMTSTART);
        rc = dbe_rflog_readdata(
                rflog,
                stmtidbuf,
                sizeof(stmtidbuf),
                &bytesread);

        if (rc == DBE_RC_SUCC) {
            ss_dassert(bytesread == sizeof(stmtidbuf));
            *p_remotestmtid = DBE_TRXID_INIT(SS_UINT4_LOADFROMDISK(&stmtidbuf[0]));
            *p_localstmtid = DBE_TRXID_INIT(SS_UINT4_LOADFROMDISK(&stmtidbuf[sizeof(ss_uint4_t)]));
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getauditinfo
 *
 *
 *
 * Parameters :
 *
 *      rflog -
 *
 *
 *      p_userid -
 *
 *
 *      p_info -
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
dbe_ret_t dbe_rflog_getauditinfo(
        dbe_rflog_t* rflog,
        long* p_userid,
        char** p_info)
{
        dbe_ret_t rc;
        size_t bytesread;
        ss_uint4_t i4;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_AUDITINFO);
        ss_dassert(p_info != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            i4 = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_userid = (long)i4;
            *p_info = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getincsysctr
 *
 * Gets system counter ID
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log object
 *
 *      p_ctrid - out
 *              pointer to counter id object
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getincsysctr(
        dbe_rflog_t* rflog,
        dbe_sysctrid_t* p_ctrid)
{
        dbe_ret_t rc;
        uchar ctridbuf;
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_INCSYSCTR);

        rc = dbe_rflog_readdata(
                rflog,
                &ctridbuf,
                sizeof(ctridbuf),
                &bytesread);
        *p_ctrid = (dbe_sysctrid_t)ctridbuf;
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_gethsbsysctr
 *
 * Gets system counter set
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log object
 *
 *      data - out
 *              pointer to counter value object
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_gethsbsysctr(
        dbe_rflog_t* rflog,
        char* data)
{
        dbe_ret_t rc;
        size_t bytesread;

        ss_dassert(
            rflog->rfl_lastlogrectype == DBE_LOGREC_SETHSBSYSCTR);

        rc = dbe_rflog_readdata(
                rflog,
                data,
                DBE_HSBSYSCTR_SIZE,
                &bytesread);
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_getcreateordropctrorseq
 *
 * Gets create/drop counter/sequence record from log
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrorseqid - out
 *              pointer to counter/sequence ID
 *
 *      p_ctrorseqname - out, ref
 *              pointer to pointer to counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t rflog_getcreateordropctrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        char** p_ctrorseqname)
{
        dbe_ret_t rc;
        size_t bytesread;

        ss_dassert(p_ctrorseqid != NULL);
        ss_dassert(p_ctrorseqname != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            *p_ctrorseqid = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            *p_ctrorseqname = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getcreatectrorseq
 *
 * Gets create counter/sequence log record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrorseqid - out
 *              pointer to counter/sequence ID
 *
 *      p_ctrorseqname - out, ref
 *              pointer to pointer to counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getcreatectrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        char** p_ctrorseqname)
{
        dbe_ret_t rc;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_CREATECTR
            ||     rflog->rfl_lastlogrectype == DBE_LOGREC_CREATESEQ);
        rc = rflog_getcreateordropctrorseq(
                rflog,
                p_ctrorseqid,
                p_ctrorseqname);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getdropctrorseq
 *
 * Gets drop counter/sequence log record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrorseqid - out
 *              pointer to counter/sequence ID
 *
 *      p_ctrorseqname - out, ref
 *              pointer to pointer to counter/sequence name
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getdropctrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        char** p_ctrorseqname)
{
        dbe_ret_t rc;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_DROPCTR
            ||     rflog->rfl_lastlogrectype == DBE_LOGREC_DROPSEQ);
        rc = rflog_getcreateordropctrorseq(
                rflog,
                p_ctrorseqid,
                p_ctrorseqname);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getincctr
 *
 * Gets an increment counter record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrid - out
 *              pointer to counter ID
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getincctr(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrid)
{
        dbe_ret_t rc;
        ss_uint4_t ctridbuf;
        size_t bytesread;

        ss_dassert(rflog != NULL);
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_INCCTR);
        ss_dassert(rflog->rfl_datasize == sizeof(ctridbuf));

        rc = dbe_rflog_readdata(
                rflog,
                &ctridbuf,
                rflog->rfl_datasize,
                &bytesread);
        *p_ctrid = SS_UINT4_LOADFROMDISK(&ctridbuf);
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflog_getsetctrorseq
 *
 * Gets set counter/sequence record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrorseqid - out
 *              pointer to counter/sequence ID
 *
 *      p_value - out
 *              pointer to rs_tuplenum_t counter
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t rflog_getsetctrorseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrorseqid,
        rs_tuplenum_t* p_value)
{
        dbe_ret_t rc;
        char* p;
        ss_uint4_t lsl;
        ss_uint4_t msl;
        size_t bytesread;

        ss_dassert(p_ctrorseqid != NULL);
        ss_dassert(p_value != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            *p_ctrorseqid = SS_UINT4_LOADFROMDISK(rflog->rfl_editbuf);
            p = (char*)rflog->rfl_editbuf + sizeof(ss_uint4_t);
            lsl = SS_UINT4_LOADFROMDISK(p);
            p += sizeof(ss_uint4_t);
            msl = SS_UINT4_LOADFROMDISK(p);
            rs_tuplenum_ulonginit(p_value, msl, lsl);
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getsetctr
 *
 * Gets a set counter record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_ctrid - out
 *              pointer to counter ID
 *
 *      p_value - out
 *              pointer to counter
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getsetctr(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_ctrid,
        rs_tuplenum_t* p_value)
{
        dbe_ret_t rc;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_SETCTR);
        rc = rflog_getsetctrorseq(rflog, p_ctrid, p_value);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getsetseq
 *
 * Gets a set sequence record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_seqid - out
 *              pointer to sequence ID
 *
 *      p_value - out
 *              pointer to counter
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getsetseq(
        dbe_rflog_t* rflog,
        ss_uint4_t* p_seqid,
        rs_tuplenum_t* p_value)
{
        dbe_ret_t rc;

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_SETSEQ);
        rc = rflog_getsetctrorseq(rflog, p_seqid, p_value);
        return (rc);
}

/*#***********************************************************************\
 *
 *              rflogbuffer_getblobg2idandoffset
 *
 * Gets blob id and remaning data size from log record data.
 *
 * Parameters :
 *
 *              buf - in
 *
 *
 *              datasize - in
 *
 *
 *          p_id - out
 *                  pointer to BLOB ID
 *
 *          p_offset - out
 *                  pointer to BLOB offset (bytes from its beginning)
 *
 *          p_remaining_datasize - out
 *                  blob data area size
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static void rflogbuffer_getblobg2idandoffset(
        ss_byte_t* buf,
        size_t datasize,
        dbe_blobg2id_t* p_id,
        dbe_blobg2size_t* p_offset,
        size_t* p_remaining_datasize)
{
        ss_dprintf_3(("rflogbuffer_getblobg2idandoffset\n"));

        *p_id = DBE_BLOBG2ID_GETFROMDISK(&buf[0]);
        *p_offset = DBE_BLOBG2SIZE_GETFROMDISK(&buf[sizeof(dbe_blobg2id_t)]);
        *p_remaining_datasize = datasize - BLOBG2DATA_HEADERSIZE;
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getblobg2idandoffset
 *
 * Gets BLOB G2 id and offset from DBE_LOGREC_BLOBG2DATA record
 *
 * Parameters :
 *
 *      rflog - use
 *              roll-forward log
 *
 *      p_id - out
 *              pointer to BLOB ID
 *
 *      p_offset - out
 *              pointer to BLOB offset (bytes from its beginning)
 *
 *      p_remaining_datasize - out
 *              blob data area size
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getblobg2idandoffset(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id,
        dbe_blobg2size_t* p_offset,
        size_t* p_remaining_datasize)
{
        dbe_ret_t rc;
        size_t bytes_gotten;
        size_t bytes_avail = rflog->rfl_datasize;
        ss_byte_t id_and_offset_data[BLOBG2DATA_HEADERSIZE];

        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_BLOBG2DATA);
        rc = dbe_rflog_readdata(rflog,
                                id_and_offset_data, sizeof(id_and_offset_data),
                                &bytes_gotten);
        ss_dassert((rc != DBE_RC_CONT && rc != DBE_RC_SUCC)
                   || bytes_gotten == sizeof(id_and_offset_data));
        if (rc == DBE_RC_SUCC || rc == DBE_RC_CONT) {
            ss_rc_dassert(rc == DBE_RC_CONT, rc);
            ss_dassert(bytes_gotten == sizeof(id_and_offset_data));

            rflogbuffer_getblobg2idandoffset(
                id_and_offset_data,
                bytes_avail,
                p_id,
                p_offset,
                p_remaining_datasize);

            ss_dassert(rflog->rfl_datasize == *p_remaining_datasize);
            rc = DBE_RC_SUCC;
        }
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getblobg2databuffer
 *
 * Gets BLOB G2 data buffer from DBE_LOGREC_BLOBG2DATA record
 *
 * Parameters :
 *
 *          rflog - use
 *                  roll-forward log
 *
 *          p_data - out
 *                  pointer to newly allocated BLOB data
 *
 *          p_datasize - out
 *                  data length
 *
 *          p_id - out
 *                  pointer to BLOB ID
 *
 *          p_offset - out
 *                  pointer to BLOB offset (bytes from its beginning)
 *
 *          p_remaining_datasize - out
 *                  blob data area size
 *
 * Return value :
 *      DBE_RC_SUCC when OK or
 *      DBE_RC_END when EOF reached or
 *      an error code when corrupt file or read failure
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getblobg2databuffer(
        dbe_rflog_t* rflog,
        ss_byte_t** p_data,
        size_t* p_datasize,
        dbe_blobg2id_t* p_id,
        dbe_blobg2size_t* p_offset,
        ss_byte_t** p_remaining_data,
        size_t* p_remaining_datasize)
{
        dbe_ret_t rc;
        size_t bytes_gotten;
        size_t remaining_datasize;
        size_t bytes_avail = rflog->rfl_datasize;
        ss_byte_t* datapos;

        ss_dprintf_1(("dbe_rflog_getblobg2databuffer:bytes_avail=%d\n", bytes_avail));
        ss_dassert(rflog->rfl_lastlogrectype == DBE_LOGREC_BLOBG2DATA);

        *p_data = SsMemAlloc(bytes_avail);
        *p_datasize = bytes_avail;

        datapos = *p_data;
        remaining_datasize = bytes_avail;

        do {
            rc = dbe_rflog_readdata(rflog,
                                    datapos,
                                    remaining_datasize,
                                    &bytes_gotten);
            ss_dprintf_2(("dbe_rflog_getblobg2databuffer:rc=%d, bytes_gotten=%d\n", rc, bytes_gotten));
            if (datapos == *p_data) {
                /* First read. */
                ss_dassert(bytes_gotten >= BLOBG2DATA_HEADERSIZE);
                rflogbuffer_getblobg2idandoffset(
                    datapos,
                    bytes_avail,
                    p_id,
                    p_offset,
                    p_remaining_datasize);
                *p_remaining_data = *p_data + BLOBG2DATA_HEADERSIZE;
            }
            datapos += bytes_gotten;
            remaining_datasize -= bytes_gotten;
        } while(rc == DBE_RC_CONT);

        ss_dassert(remaining_datasize == 0);

        return (rc);
}

dbe_ret_t dbe_rflog_getblobg2dropmemoryref(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id)
{
        dbe_ret_t rc;
        size_t bytesread;

        ss_dassert(p_id != NULL);

        if (rflog->rfl_datasize > rflog->rfl_editbufsize) {
            rflog->rfl_editbufsize = rflog->rfl_datasize;
            rflog->rfl_editbuf =
                SsMemRealloc(rflog->rfl_editbuf, rflog->rfl_editbufsize);
        }
        rc = dbe_rflog_readdata(
                rflog,
                rflog->rfl_editbuf,
                rflog->rfl_datasize,
                &bytesread);
        if (rc == DBE_RC_SUCC) {
            *p_id = DBE_BLOBG2ID_GETFROMDISK(rflog->rfl_editbuf);
        }
        return (rc);
}

dbe_ret_t dbe_rflog_getblobg2datacomplete(
        dbe_rflog_t* rflog,
        dbe_blobg2id_t* p_id)
{
        return(dbe_rflog_getblobg2dropmemoryref(rflog, p_id));
}

/*##**********************************************************************\
 *
 *              dbe_rflog_saverecordpos
 *
 * Saves the postion of the current log record
 *
 * Parameters :
 *
 *      rflog - in, use
 *              pointer to roll-forward log
 *
 *      pos_buf - out, use
 *              pointer to buffer where the position will be saved
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_rflog_saverecordpos(
        dbe_rflog_t* rflog,
        dbe_logpos_t* pos_buf)
{
        ss_dassert(rflog != NULL);
        ss_dassert(pos_buf);
        *pos_buf = rflog->rfl_lastlogreclp;
}

/*##**********************************************************************\
 *
 *              dbe_rflog_restorerecordpos
 *
 * Restores position to saved log record position
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      pos_buf - in, use
 *              pointer to buffer that contains a saved position
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_rflog_restorerecordpos(
        dbe_rflog_t* rflog,
        dbe_logpos_t* pos_buf)
{
        su_ret_t rc;
        size_t blocksize;

        ss_dassert(rflog != NULL);
        ss_dassert(pos_buf);
        rflog->rfl_lp = *pos_buf;
        rflog->rfl_datasize = 0;
        rflog->rfl_endreached = FALSE;
        rflog->rfl_lastlogrectype = DBE_LOGREC_NOP;
        blocksize = su_mbsvf_getblocksize_at_addr(rflog->rfl_mbsvfil,
                                                  rflog->rfl_lp.lp_daddr);
        if (blocksize != rflog->rfl_currbufsize) {
            rflog->rfl_currbufsize = blocksize;
            rflog->rfl_currbufdatasize =
                blocksize - (2 * sizeof(ss_uint2_t));
        }
        rc = rflog_read_rfl(rflog);
        su_rc_assert(rc == SU_SUCCESS, rc);
}

/*##**********************************************************************\
 *
 *              dbe_rflog_cleartoeof
 *
 * Clears the end of the roll-forward log from logpos (including it) to
 * the end of file. This is needed to enable scanning corrupt part of
 * logfile in later recovery.
 *
 *
 * Parameters :
 *
 *      rflog - in out, use
 *              pointer to roll-forward log
 *
 *      logpos - in, use
 *              saved position of first corrupt record
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_rflog_cleartoeof(
        dbe_rflog_t* rflog,
        dbe_logpos_t* logpos)
{
        dbe_ret_t rc;
        dbe_logbuf_t* tmp_lb;
        size_t blocksize;
        size_t blockdatasize;
        su_daddr_t daddr;

        rc = DBE_RC_SUCC;
        daddr = logpos->lp_daddr;
        blocksize = su_mbsvf_getblocksize_at_addr(rflog->rfl_mbsvfil,
                                                  daddr);
        blockdatasize = blocksize - (2 * sizeof(ss_uint2_t));
        tmp_lb = dbe_lb_init(blocksize);
        if (daddr != SU_DADDR_NULL &&  daddr < rflog->rfl_fsize) {
             
            rc = rflog_read_pages(rflog, daddr, tmp_lb, blocksize);
            su_rc_assert(rc == SU_SUCCESS, rc);

            if (logpos->lp_bufpos > 0
            &&  logpos->lp_bufpos < rflog->rfl_currbufdatasize)
            {
                dbe_lb_incblock(tmp_lb, rflog->rfl_currbufsize);
                memset(DBE_LB_DATA(tmp_lb) + logpos->lp_bufpos,
                       DBE_LOGREC_NOP,
                       blockdatasize - logpos->lp_bufpos);
                rc = rflog_write_page(rflog, daddr, tmp_lb);
                su_rc_assert(rc == SU_SUCCESS, rc);
                daddr += blocksize / rflog->rfl_addressing_size;
            }
            rc = su_mbsvf_decreasesize(rflog->rfl_mbsvfil, daddr);
        }
        dbe_lb_done(tmp_lb);
        su_rc_assert(rc == SU_SUCCESS, rc);
}


/*##**********************************************************************\
 *
 *              dbe_rflog_getphysicalfname
 *
 * Get physical filename of the current roll-forward log position.
 *
 * Parameters :
 *
 *      rflog - in, use
 *
 *
 * Return value - ref:
 *      pointer to a file name string or NULL when daddr is not valid.
 *
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* dbe_rflog_getphysicalfname(dbe_rflog_t* rflog)
{
        su_daddr_t lastpos;
        char* fname;

        lastpos = rflog->rfl_lastlogreclp.lp_daddr;
        fname = su_mbsvf_getphysfilename(rflog->rfl_mbsvfil, lastpos);

        return fname;
}

/*##**********************************************************************\
 *
 *              dbe_rflog_getfilenumstart
 *
 * Gets log file number of first file to be included in roll-forward
 * from specified cpnum & logfnum
 *
 * Parameters :
 *
 *      nametemplate - in, use
 *              log file name tamplate
 *
 *      digittemplate - in
 *              digit placeholder char in previous
 *
 *      bufsize - in
 *              log file buffer size
 *
 *      cpnum - in
 *              checkpoint #
 *
 *      logfnum - in
 *              log file number
 *
 *      p_startlogfnum - out
 *              pointer to log file number variable where to start roll-forward
 *
 * Return value :
 *      DBE_RC_SUCC when ok or error code
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_rflog_getfilenumstart(
        rs_sysi_t *cd,
        char* logdir,
        char* nametemplate,
        char digittemplate,
        size_t bufsize,
        dbe_cpnum_t cpnum,
        dbe_logfnum_t logfnum,
        dbe_logfnum_t* p_startlogfnum)
{
        dbe_ret_t rc;
        su_svfil_t* tmp_svfil;
        char* fname;
        su_daddr_t fsize;
        su_daddr_t fsize_sum;
        int i;
        dbe_logbuf_t* tmp_lb;

        bufsize = DBE_CFG_MINLOGBLOCKSIZE;
        rc = DBE_RC_SUCC;
        *p_startlogfnum = logfnum;
        fsize_sum = 0L;
        tmp_lb = NULL;
        for (i = 0; ; i++, logfnum++)  {
            fname = dbe_logfile_genname(    /* generate file name */
                logdir,
                nametemplate,
                logfnum,
                digittemplate);
            if (fname == NULL) {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    DBE_ERR_ILLLOGFILETEMPLATE_SSSDD,
                    SU_DBE_LOGSECTION,
                    SU_DBE_LOGFILETEMPLATE,
                    nametemplate,
                    DBE_LOGFILENAME_MINDIGITS,
                    DBE_LOGFILENAME_MAXDIGITS);
            }
            if (!SsFExist(fname)) {
                fsize = 0L;
            } else {
                ss_int8_t addressspace;
                ss_int8_t tmp_i8;
                dbe_cryptoparams_t* cp = NULL;
                
                if (cd != NULL) {
                    cp = rs_sysi_getcryptopar(cd);
                }
                
                if (tmp_lb == NULL) {
                    tmp_lb = dbe_lb_init(bufsize);
                }
                tmp_svfil = su_svf_init(bufsize, SS_BF_SEQUENTIAL);

                if (cp != NULL) {
                    su_svf_setcipher(tmp_svfil, dbe_crypt_getcipher(cp),
                                     dbe_crypt_getencrypt(cp),
                                     dbe_crypt_getdecrypt(cp));
                }

                SsInt8SetUint4(&addressspace, SU_DADDR_MAX);
                SsInt8SetUint4(&tmp_i8, (ss_uint4_t)bufsize);
                SsInt8MultiplyByInt8(&addressspace, addressspace, tmp_i8);
                su_svf_addfile(tmp_svfil, fname, addressspace, FALSE);

                fsize = su_svf_getsize(tmp_svfil);
                if (fsize != 0L) {      /* file exists and size != 0 */
                    size_t bytesread;
                    uchar* p;
                    loghdrdata_t hdrdata;

                    rc = su_svf_read(   /* read 1st block to get header record */
                            tmp_svfil,
                            0L,
                            tmp_lb,
                            bufsize,
                            &bytesread);
                    if (rc == DBE_RC_SUCC) {
                        su_rc_dassert(rc == DBE_RC_SUCC, rc);
                        ss_dassert(bufsize == bytesread);

                        p = DBE_LB_DATA(tmp_lb);
                        if (*p != DBE_LOGREC_HEADER) {
                            su_emergency_exit(
                                __FILE__,
                                __LINE__,
                                DBE_ERR_LOGFILE_CORRUPT_S,
                                fname);
                            /* NOTREACHED */
                        }
                        ss_dassert(*p == DBE_LOGREC_HEADER);
                        p++;
                        hdrdata.lh_logfnum = SS_UINT4_LOADFROMDISK(p);
                        p += sizeof(hdrdata.lh_logfnum);
                        hdrdata.lh_cpnum = SS_UINT4_LOADFROMDISK(p);
                        p += sizeof(hdrdata.lh_cpnum);
                        hdrdata.lh_blocksize = SS_UINT4_LOADFROMDISK(p);
#if 0 /* removed by pete 2004-06-11, multi-blocksize recovery is possible */
                        if (hdrdata.lh_blocksize != bufsize) {
                            rc = DBE_ERR_WRONGLOGBLOCKSIZEATBACKUP;
                        }
#endif /* 0, removed by pete */
                        if (hdrdata.lh_cpnum < cpnum && i > 0) {
                            *p_startlogfnum = logfnum;
                        }
                        fsize_sum += fsize;
                    }
                }
                su_svf_done(tmp_svfil);
            }
            SsMemFree(fname);
            if (rc != DBE_RC_SUCC || (fsize == 0L && i > 0)) {
                break;
            }
        }
        if (tmp_lb != NULL) {
            dbe_lb_done(tmp_lb);
        }
        return (rc);
}

#endif /* SS_NOLOGGING */
