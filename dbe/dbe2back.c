/*************************************************************************\
**  source       * dbe2back.c
**  directory    * dbe
**  description  * Database backup.
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

#include <errno.h>

#include <ssstdio.h>
#include <ssstdlib.h>
#include <ssstring.h>
#include <sslimits.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <ssthread.h>
#include <ssfnsplt.h>
#include <sstime.h>
#include <ssfile.h>
#include <sscacmem.h>
#include <sschcvt.h>
#include <sspmon.h>

#include <su0svfil.h>
#include <su0vfil.h>
#include <su0cfgst.h>

#include <ui0msg.h>

#include "dbe9type.h"
#include "dbe7ctr.h"
#include "dbe7cfg.h"
#include "dbe6finf.h"
#include "dbe7logf.h"
#include "dbe7rfl.h"
#include "dbe2back.h"
#include "dbe0type.h"

extern bool dbefile_diskless;
long backup_blocksize;

#ifndef SS_NOBACKUP

#define BU_DEFAULT_BUFSIZE   (32UL * 1024UL)
#define BU_DELETE_MAXRETRY          5

typedef enum {
       BUST_INDEX,     /* Index file copying state. */
        BUST_LOG,       /* Log file copying state. */
        BUST_INIFILE,   /* Ini file copying state. */
        BUST_SOLMSGOUT, /* solmsg.out copying state */
        BUST_END        /* End of backup state. */
} backup_state_t;

struct dbe_backup_st {
        backup_state_t  bu_state;           /* Backup advance state. */
        dbe_counter_t*  bu_ctr;             /* dbe counter object */
        dbe_cfg_t*      bu_cfg;             /* Configuration object. */
        char*           bu_dir;             /* Backup directory. */
        bool            bu_hsbcopy;         /* is this for hsb copy/netcopy? */
        bool            bu_hsbcopycomplete; /* is this for hsb copy/netcopy? */
        su_svfil_t*     bu_indexsvfil_src;  /* Source index file. */
        su_svfil_t*     bu_indexsvfil_dest; /* Destination index file. */
        su_daddr_t      bu_indexsvfil_size; /* Index file size. */
        dbe_cache_t*    bu_indexcache;      /* Index cache */
        su_svfil_t*     bu_logsvfil_src;    /* Source log file. */
        su_svfil_t*     bu_logsvfil_dest;   /* Destination log file. */
        su_daddr_t      bu_logsvfil_size;   /* Log file size. */

        /* currently copied other file */
        su_svfil_t*     bu_curr_copyfile_src;
        su_svfil_t*     bu_curr_copyfile_dest;

        su_daddr_t      bu_loc;             /* Current copy location. */
        su_daddr_t      bu_endloc;          /* End copy location. */
        char*           bu_buf;             /* Copy buffer. */
        size_t          bu_bufsize;         /* Copy buffer size. */
        size_t          bu_blocksize;       /* Copy file block size. */
        bool            bu_copylog;         /* If TRUE, log file is copied. */
        bool            bu_deletelog;       /* If TRUE, log files are deleted
                                               after backup. */
        bool            bu_copyinifile;     /* If TRUE, copy solid.ini. */
        bool            bu_copysolmsgout;   /* If TRUE, copy solmsg.out. */
        dbe_logfnum_t   bu_firstlogfnum;    /* First copied log file number. */
        dbe_logfnum_t   bu_lastlogfnum;     /* Last copied log file number. */
        dbe_logfnum_t   bu_firstnotdeletelogfnum; /* Smallest log file number
                                               that should not be deleted.*/
        dbe_cpnum_t     bu_cpnum;           /* Backup checkpoint number. */
        SsCacMemT*      bu_cacmem;
        su_ret_t       (*bu_callbackfp)(   /* Callback function to write */
                            void* ctx,      /* user given stream. */
                            dbe_backupfiletype_t ftype,
                            ss_int4_t finfo,  /* log file number, unused for other files */
                            su_daddr_t daddr, /* position in file */
                            char* fname,   /* path is not relevant! */
                            void* data,
                            size_t len);
        void*           bu_callbackctx;
        dbe_logfnum_t   bu_hsb_start_logfnum;
        rs_sysi_t*      bu_cd;
};



/*#***********************************************************************\
 *
 *              backup_getbufsize
 *
 * Gets read/write buffer allocation size.
 *
 * Parameters :
 *
 *      blocksize - in
 *              File block size.
 *
 * Return value :
 *
 *      Copy buffer size in bytes.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static size_t backup_getbufsize(size_t blocksize)
{
        ulong nblocks;
        ulong bufsize;

        /*bufsize = BU_DEFAULT_BUFSIZE;*/
        bufsize = backup_blocksize;
        nblocks = bufsize / blocksize;
        if (nblocks == 0) {
            nblocks = 1;
        }
        if (nblocks * (ulong)blocksize > (ulong)SS_MAXALLOCSIZE) {
            nblocks = (ulong)SS_MAXALLOCSIZE / blocksize;
        }
        ss_assert(nblocks > 0);
        bufsize = nblocks * (ulong)blocksize;
        return((size_t)bufsize);
}

static void backup_initcopyfileifneeded(dbe_backup_t* backup,
                                        char* src_pathname,
                                        dbe_backupfiletype_t ftype __attribute__ ((unused)))
{
        if (backup->bu_curr_copyfile_src == NULL) {
            bool src_exists;
            ss_dassert(backup->bu_curr_copyfile_dest == NULL);
            backup->bu_blocksize = 1;
            backup->bu_curr_copyfile_src = su_svf_init(backup->bu_blocksize,
                                                       SS_BF_SEQUENTIAL |
                                                       SS_BF_READONLY);
            ss_dassert(backup->bu_curr_copyfile_src != NULL);
            src_exists = SsFExist(src_pathname);
            if (src_exists) {
                ss_int8_t maxsize;
                
                SsInt8SetUint4(&maxsize, SU_VFIL_SIZE_MAX);
                su_svf_addfile(backup->bu_curr_copyfile_src,
                               src_pathname,
                               maxsize,
                               FALSE);
                backup->bu_endloc = su_svf_getsize(backup->bu_curr_copyfile_src);
            } else {
                backup->bu_endloc = 0;
            }
            backup->bu_loc = 0;
            backup->bu_bufsize = backup_getbufsize(backup->bu_blocksize);
            backup->bu_cacmem = SsCacMemInit(backup->bu_bufsize, 1);
            backup->bu_buf = SsCacMemAlloc(backup->bu_cacmem);
            if (backup->bu_callbackfp == NULL) {
                bool succp;
                char* dest_pathname;
                char* fname;
                char* src_dir;
                size_t src_pathname_len_plus_1 = strlen(src_pathname) + 1;
                size_t dest_path_len_plus_1;

                fname = SsMemAlloc(src_pathname_len_plus_1);
                src_dir = SsMemAlloc(src_pathname_len_plus_1);
                succp = SsFnSplitPath(src_pathname,
                                      src_dir, src_pathname_len_plus_1,
                                      fname, src_pathname_len_plus_1);
                ss_dassert(succp);
                dest_path_len_plus_1 =
                    strlen(backup->bu_dir) +
                    strlen(fname) +
                    (1 + 1);
                dest_pathname = SsMemAlloc(dest_path_len_plus_1);
                succp = SsFnMakePath(backup->bu_dir,
                                     fname,
                                     dest_pathname, dest_path_len_plus_1);
                ss_dassert(succp);
                backup->bu_curr_copyfile_dest =
                    su_svf_init(backup->bu_blocksize,
                                SS_BF_SEQUENTIAL |
                                SS_BF_WRITEONLY);
                if (src_exists) {
                    ss_int8_t maxsize;

                    SsInt8SetUint4(&maxsize, SU_VFIL_SIZE_MAX);
                    su_svf_addfile(backup->bu_curr_copyfile_dest,
                                   dest_pathname,
                                   maxsize,
                                   FALSE);
                }
                SsMemFree(dest_pathname);
                SsMemFree(fname);
                SsMemFree(src_dir);
            }
        }
}

static void backup_donecopyfile(dbe_backup_t* backup)
{
        if (backup->bu_curr_copyfile_src != NULL) {
            su_svf_done(backup->bu_curr_copyfile_src);
            backup->bu_curr_copyfile_src = NULL;
        }
        if (backup->bu_curr_copyfile_dest != NULL) {
            su_svf_flush(backup->bu_curr_copyfile_dest);
            su_svf_done(backup->bu_curr_copyfile_dest);
            backup->bu_curr_copyfile_dest = NULL;
        }
        if (backup->bu_cacmem != NULL) {
            ss_dassert(backup->bu_buf != NULL);
            SsCacMemFree(backup->bu_cacmem, backup->bu_buf);
            SsCacMemDone(backup->bu_cacmem);
            backup->bu_cacmem = NULL;
            backup->bu_buf = NULL;
        }
}

#ifndef SS_NOLOGGING

/*#***********************************************************************\
 *
 *              backup_deletedblog
 *
 * Deletes database log files after backup.
 *
 * Parameters :
 *
 *      cfg -
 *
 *
 *      firstlogfnum -
 *
 *
 *      firstnotdeletelogfnum -
 *
 *
 *      p_errh -
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
static dbe_ret_t backup_deletedblog(
        dbe_cfg_t* cfg,
        dbe_logfnum_t firstlogfnum,
        dbe_logfnum_t firstnotdeletelogfnum,
        dbe_logfnum_t hsb_notdeletelogfnum,
        rs_err_t** p_errh)
{
        dbe_logfnum_t logfnum;
        char* logdir;
        char* nametemplate;
        char digittemplate;
        char* fname;
        dbe_ret_t rc = DBE_RC_SUCC;

        ss_dprintf_2(("backup_deletedblog:firstlogfnum=%ld, firstnotdeletelogfnum=%ld, hsb_notdeletelogfnum=%ld\n", 
                       firstlogfnum, firstnotdeletelogfnum, hsb_notdeletelogfnum));

        dbe_cfg_getlogdir(cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(cfg, &nametemplate);
        dbe_cfg_getlogdigittemplate(cfg, &digittemplate);

        if (hsb_notdeletelogfnum > 0 && hsb_notdeletelogfnum < firstnotdeletelogfnum) {
            firstnotdeletelogfnum = hsb_notdeletelogfnum;
            ss_dprintf_2(("backup_deletedblog:new firstnotdeletelogfnum=%ld\n", firstnotdeletelogfnum));
        }

        /* Generate log file names and delete those that can be deleted.
         */
        for (logfnum = firstlogfnum;
             logfnum < firstnotdeletelogfnum && rc == DBE_RC_SUCC;
             logfnum++) {
            int i;
            bool b;

            fname = dbe_logfile_genname(    /* generate file name */
                        logdir,
                        nametemplate,
                        logfnum,
                        digittemplate);

            if (SsFExist(fname)) {
                ss_dprintf_4(("backup_deletedblog:file exists, fname=%s, logfnum=%ld\n", fname, logfnum));
                for (i = 0; i < BU_DELETE_MAXRETRY; i++) {
                    ss_dprintf_4(("backup_deletedblog:SsFRemove(%s)\n", fname));
                    b = SsFRemove(fname);
                    if (b) {
                        break;
                    }
                }
                if (!b) {
                    rc = DBE_ERR_LOGDELFAILED_S;
                    rs_error_create(p_errh, DBE_ERR_LOGDELFAILED_S, fname);
                    ss_rc_derror(errno);
                }
            } else {
                ss_dprintf_4(("backup_deletedblog:file NOT found, fname=%s, logfnum=%ld\n", fname, logfnum));
            }
            SsMemFree(fname);
        }
        SsMemFree(nametemplate);
        SsMemFree(logdir);

        return(rc);
}

/*#***********************************************************************\
 *
 *              backup_deletebackuplog
 *
 * Deletes log files in backup directory.
 *
 * Parameters :
 *
 *      backup -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t backup_deletebackuplog(
        dbe_backup_t* backup,
        rs_err_t** p_errh)
{
        dbe_logfnum_t logfnum;
        char* logdir;
        char* nametemplate;
        char digittemplate;
        char* fname;
        char* dname;
        char* logpathname;
        size_t logpathnamelen;
        char pathname[256];
        dbe_ret_t rc = DBE_RC_SUCC;
        bool b;

        if (backup->bu_firstlogfnum == 0) {
            return(DBE_RC_SUCC);
        }

        dbe_cfg_getlogdir(backup->bu_cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(backup->bu_cfg, &nametemplate);
        dbe_cfg_getlogdigittemplate(backup->bu_cfg, &digittemplate);

        /* Generate log file names and delete those that can be deleted.
         */
        for (logfnum = backup->bu_firstlogfnum - 1; ; logfnum--) {

            logpathname = dbe_logfile_genname(    /* generate file name */
                                logdir,
                                nametemplate,
                                logfnum,
                                digittemplate);
            logpathnamelen = strlen(logpathname);
            fname = SsMemAlloc(logpathnamelen + 1);
            dname = SsMemAlloc(logpathnamelen + 1);

            b = SsFnSplitPath(
                    logpathname,
                    dname,
                    (int)logpathnamelen + 1,
                    fname,
                    (int)logpathnamelen + 1);
            ss_dassert(b);
            b = SsFnMakePath(backup->bu_dir, fname, pathname, sizeof(pathname));
            SsMemFree(fname);
            SsMemFree(dname);
            SsMemFree(logpathname);
            if (!b) {
                rc = SU_ERR_TOO_LONG_FILENAME;
                break;
            }

            if (SsFExist(pathname)) {
                int i;
                bool b;
                for (i = 0; i < BU_DELETE_MAXRETRY; i++) {
                    b = SsFRemove(pathname);
                    ss_dprintf_1(("backup_deletebackuplog:SsFRemove(%s)\n", pathname));
                    if (b) {
                        break;
                    }
                }
                if (!b) {
                    rc = DBE_ERR_LOGDELFAILED_S;
                    rs_error_create(p_errh, DBE_ERR_LOGDELFAILED_S, pathname);
                    break;
                }
            } else {
                break;
            }
        }

        SsMemFree(nametemplate);
        SsMemFree(logdir);

        return(rc);
}

/*#***********************************************************************\
 *
 *              backup_getlogfnumrange
 *
 * Gets a range of numbers of existing log file.
 *
 * Parameters :
 *
 *      ctr -
 *
 *
 *      cfg -
 *
 *
 *      p_lastlogfnum -
 *
 *
 *      p_firstlogfnum -
 *
 *
 *      p_firstnotdeletelogfnum -
 *
 *
 *      p_errh -
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
static dbe_ret_t backup_getlogfnumrange(
        dbe_counter_t* ctr,
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_cpnum_t cpnum,
        dbe_logfnum_t* p_lastlogfnum,
        dbe_logfnum_t* p_firstlogfnum,
        dbe_logfnum_t* p_firstnotdeletelogfnum,
        rs_err_t** p_errh)
{
        dbe_logfnum_t logfnum;
        char* logdir;
        char* nametemplate;
        char digittemplate;
        char* fname;
        size_t blocksize;
        dbe_ret_t rc;

        *p_lastlogfnum = dbe_counter_getlogfnum(ctr) - 1;

        dbe_cfg_getlogdir(cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(cfg, &nametemplate);
        dbe_cfg_getlogdigittemplate(cfg, &digittemplate);

        /* Get the first log file number. */
        for (logfnum = *p_lastlogfnum; ; ) {
            bool exist;

            logfnum--;
            fname = dbe_logfile_genname(    /* generate file name */
                        logdir,
                        nametemplate,
                        logfnum,
                        digittemplate);

            exist = SsFExist(fname);
            SsMemFree(fname);

            if (!exist) {
                /* File not found, the first log file num is one bigger  */
                logfnum++;
                break;
            }
        }

        *p_firstlogfnum = logfnum;

        dbe_cfg_getlogblocksize(cfg, &blocksize);

        rc = dbe_rflog_getfilenumstart(
                cd,
                logdir,
                nametemplate,
                digittemplate,
                blocksize,
                cpnum,
                *p_firstlogfnum,
                p_firstnotdeletelogfnum);
        if (*p_lastlogfnum < *p_firstnotdeletelogfnum) {
            *p_firstnotdeletelogfnum = *p_lastlogfnum;
        }
        SsMemFree(nametemplate);
        SsMemFree(logdir);

        ss_dprintf_3(("backup_getlogfnumrange:p_firstlogfnum=%ld, p_lastlogfnum=%ld, p_firstnotdeletelogfnum=%ld\n", *p_firstlogfnum, *p_lastlogfnum, *p_firstnotdeletelogfnum));

        if (rc != DBE_RC_SUCC) {
            rs_error_create(p_errh, rc);
        }
        return(rc);
}

/*#***********************************************************************\
 *
 *              backup_logsvfil
 *
 * Gets a split virtual file that contains all log files that should
 * be backed up.
 *
 * Parameters :
 *
 *      backup -
 *
 *
 *      ctr -
 *
 *
 *      p_rc -
 *
 *
 * Return value - give :
 *
 *      Split virtual file that contains all log files that
 *      should be backed up.
 *
 * Limitations  :
 *
 * Globals used :
 */
static su_svfil_t* backup_logsvfil(
        dbe_backup_t* backup,
        dbe_counter_t* ctr,
        su_ret_t* p_rc)
{
        dbe_logfnum_t logfnum;
        su_svfil_t* svfil;
        size_t blocksize;
        char* logdir;
        char* nametemplate;
        char digittemplate;
        char* fname;
        ss_int8_t maxsize;
        su_daddr_t size_sum;
        su_ret_t rc;

        SS_NOTUSED(ctr);

        *p_rc = SU_SUCCESS;

        dbe_cfg_getlogdir(backup->bu_cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(backup->bu_cfg, &nametemplate);
        dbe_cfg_getlogdigittemplate(backup->bu_cfg, &digittemplate);

        blocksize = DBE_CFG_MINLOGBLOCKSIZE;

        svfil = su_svf_init(
                    blocksize,
                    SS_BF_SEQUENTIAL|SS_BF_NOBUFFERING|SS_BF_READONLY);

        ss_dprintf_4(("backup->bu_firstlogfnum = %ld,  backup->bu_lastlogfnum = %ld\n",
                      (long)backup->bu_firstlogfnum,
                      (long)backup->bu_lastlogfnum));
        /* Add logfiles to svfil.
         */
        size_sum = 0;
        for (logfnum = backup->bu_firstlogfnum;
             logfnum <= backup->bu_lastlogfnum;
             logfnum++) {

            fname = dbe_logfile_genname(    /* generate file name */
                        logdir,
                        nametemplate,
                        logfnum,
                        digittemplate);

            if (logfnum < backup->bu_lastlogfnum) {
                /* Not the last file. */
                ss_int8_t tmp_i8;

                maxsize = SsFSizeAsInt8(fname);
                if (SsInt8Is0(maxsize)) {
                    SsInt8SetUint4(&maxsize, SU_VFIL_SIZE_MAX);
                }
                SsInt8SetUint4(&tmp_i8, blocksize);
                SsInt8DivideByInt8(&tmp_i8, maxsize, tmp_i8);
                size_sum += SsInt8GetLeastSignificantUint4(tmp_i8);
            } else {
                /* Last file. */
                ss_int8_t tmp_i8;
                su_daddr_t maxsize_blocks = SU_DADDR_MAX - size_sum;

                SsInt8SetUint4(&maxsize, maxsize_blocks);
                SsInt8SetUint4(&tmp_i8, blocksize);
                SsInt8MultiplyByInt8(&maxsize, maxsize, tmp_i8);
            }

            ss_dprintf_1(("backup_logsvfil: adding file %s\n",
                          fname));
            rc = su_svf_addfile(svfil, fname, maxsize, FALSE);
            if (rc != SU_SUCCESS) {
                *p_rc = rc;
            }
            SsMemFree(fname);
        }

        SsMemFree(nametemplate);
        SsMemFree(logdir);

        return(svfil);
}

#endif /* SS_NOLOGGING */

/*#***********************************************************************\
 *
 *              backup_checkdir
 *
 *
 *
 * Parameters :
 *
 *      cfg - in
 *
 *
 *      backupdir - in
 *
 *
 *      p_errh - out, give
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
static dbe_ret_t backup_checkdir(
        dbe_cfg_t* cfg,
        char* backupdir,
        rs_err_t** p_errh)
{
        bool found;
        su_pa_t* filespec_array;
        dbe_filespec_t* filespec;
        uint i;
        char dbuf[255];
        char fbuf[255];
        char pbuf[255];
        char testpbuf[255];
        char* tmpfname = (char *)"solbakZZ.tmp";
        char* txt = (char *)"Solid backup test file\n";
        SS_FILE* fp;
        char* logfname;
        bool succp = TRUE;
        bool b;

        if (dbefile_diskless) {
            rs_error_create(p_errh, SRV_ERR_DISKLESSNOTSUPP);
            return(SRV_ERR_DISKLESSNOTSUPP);
        }

        b = SsFnMakePath(backupdir, tmpfname, testpbuf, sizeof(testpbuf));
        if (!b) {
            rs_error_create(p_errh, DBE_ERR_ILLBACKUPDIR_S, backupdir);
            return(DBE_ERR_ILLBACKUPDIR_S);
        }

        fp = SsFOpenB(testpbuf, (char *)"r");
        if (fp != NULL) {
            SsFClose(fp);
            rs_error_create(p_errh, DBE_ERR_ILLBACKUPDIR_S, backupdir);
            return(DBE_ERR_ILLBACKUPDIR_S);
        }

        fp = SsFOpenB(testpbuf, (char *)"w");
        if (fp == NULL) {
            rs_error_create(p_errh, DBE_ERR_BACKUPDIRNOTEXIST_S, backupdir);
            return(DBE_ERR_BACKUPDIRNOTEXIST_S);
        }
        SsFWrite(txt, strlen(txt), 1, fp);
        SsFClose(fp);

        /* Check that backup and database directories are not the same.
         */
        filespec_array = su_pa_init();
        dbe_cfg_getidxfilespecs(cfg, filespec_array);
        su_pa_do_get(filespec_array, i, filespec) {
            b = SsFnSplitPath(
                    dbe_filespec_getname(filespec),
                    dbuf,
                    sizeof(dbuf),
                    fbuf,
                    sizeof(fbuf));
            ss_dassert(b);
            if (b) {
                b = SsFnMakePath(dbuf, tmpfname, pbuf, sizeof(pbuf));
                ss_dassert(b);
            }
            if (b) {
                fp = SsFOpenB(pbuf, (char *)"r");
                if (fp != NULL) {
                    /* Database and backup directories are the same. */
                    SsFClose(fp);
                    succp = FALSE;
                    break;
                }
            }
        }
        su_pa_do_get(filespec_array, i, filespec) {
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);

        if (!succp) {
            SsFRemove(testpbuf);
            rs_error_create(p_errh, DBE_ERR_ILLBACKUPDIR_S, backupdir);
            return(DBE_ERR_ILLBACKUPDIR_S);
        }

        /* Check that log and backup directories are not the same.
         */
        found = dbe_cfg_getlogfilenametemplate(cfg, &logfname);
        if (found) {
            b = SsFnSplitPath(
                    logfname,
                    dbuf,
                    sizeof(dbuf),
                    fbuf,
                    sizeof(fbuf));
            ss_dassert(b);
            if (b) {
                b = SsFnMakePath(dbuf, tmpfname, pbuf, sizeof(pbuf));
                ss_dassert(b);
            }
            if (b) {
                fp = SsFOpenB(pbuf, (char *)"r");
                if (fp != NULL) {
                    /* Log and backup directories are the same. */
                    SsFClose(fp);
                    succp = FALSE;
                }
            }
        }
        SsMemFree(logfname);

        if (!succp) {
            SsFRemove(testpbuf);
            rs_error_create(p_errh, DBE_ERR_ILLBACKUPDIR_S, backupdir);
            return(DBE_ERR_ILLBACKUPDIR_S);
        }

        SsFRemove(testpbuf);

        return(DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_backup_check
 *
 * Checks that backup directory is ok.
 *
 * Parameters :
 *
 *      cfg -
 *
 *
 *      backupdir -
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
dbe_ret_t dbe_backup_check(
        dbe_cfg_t* cfg,
        char* backupdir,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        if (backupdir == NULL) {
            dbe_cfg_getbackupdir(cfg, &backupdir);
            rc = backup_checkdir(cfg, backupdir, p_errh);
            SsMemFree(backupdir);
        } else {
            rc = backup_checkdir(cfg, backupdir, p_errh);
        }
        return(rc);
}
/*#***********************************************************************\
 *
 *          backup_init
 *
 * Initializes a backup.
 *
 * Parameters :
 *
 *      cfg - in, hold
 *          Configuration object.
 *
 *      file - in, hold
 *          Database files.
 *
 *      ctr - in, use
 *          Counter object.
 *
 *      backupdir - in, use
 *          If NULL, the backup directory is taken from the configuration
 *          object.
 *
 *      callbackfp - in, hold
 *          pointer to callback function that outputs the data,
 *          backupdir parameter should be NULL when callbackfun != NULL
 *
 *      callbackctx - in, hold
 *          context given as 1st argument to callbackfun
 *
 *      replicap - in
 *          If TRUE, replication backup.
 *
 *      p_rc - out
 *          Error code is returned in *p_rc, if function return code
 *          is NULL.
 *
 *      p_errh - out
 *          Error info.
 *
 * Return value - give :
 *      pointer to created backup object
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_backup_t* backup_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_file_t* file,
        dbe_counter_t* ctr,
        char* backupdir,
        su_ret_t (*callbackfp)(   /* Callback function to write */
                void* ctx,      /* user given stream. */
                dbe_backupfiletype_t ftype,
                ss_int4_t finfo,  /* log file number, unused for other files */
                su_daddr_t daddr, /* position in file */
                char* fname,   /* without path! */
                void* data,
                size_t len),
        void* callbackctx,
        bool replicap,
        dbe_backuplogmode_t backuplogmode,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        dbe_ret_t* p_rc,
        rs_err_t** p_errh)
{
        dbe_backup_t* backup;
        bool netp;

        if (callbackfp == NULL) {
            netp = FALSE;
            if (backupdir == NULL) {
                dbe_cfg_getbackupdir(cfg, &backupdir);
            } else {
                backupdir = SsMemStrdup(backupdir);
            }
            *p_rc = backup_checkdir(cfg, backupdir, p_errh);
            if (*p_rc != DBE_RC_SUCC) {
                SsMemFree(backupdir);
                return(NULL);
            }
        } else {
            ss_dassert(backupdir == NULL);
            netp = TRUE;
            *p_rc = DBE_RC_SUCC;
            backupdir = NULL;
        }

        backup = SSMEM_NEW(dbe_backup_t);

        backup->bu_cfg = cfg;
        backup->bu_ctr = ctr;

        backup->bu_hsb_start_logfnum = 0;
        backup->bu_cd = cd;

        if (replicap) {
            backup->bu_hsbcopy = TRUE;
            backup->bu_hsbcopycomplete = FALSE;
            backup->bu_copylog = FALSE;
            backup->bu_deletelog = FALSE;
            backup->bu_copyinifile = FALSE;
            backup->bu_copysolmsgout = FALSE;
        } else {
            bool found;
            bool cpdeletelog;

            backup->bu_hsbcopy = FALSE;
            backup->bu_hsbcopycomplete = FALSE;
            if (netp) {
                found = dbe_cfg_getnetbackupcopylog(cfg, &backup->bu_copylog);
            } else {
                found = dbe_cfg_getbackupcopylog(cfg, &backup->bu_copylog);
            }
            
            dbe_cfg_getcpdeletelog(cfg, &cpdeletelog);
            if (cpdeletelog && backup->bu_copylog) {
                backup->bu_copylog = FALSE;
                if (found) {
                    ui_msg_warning(LOG_MSG_CONFLICTING_PARAMETERS);
                }
            }
            switch (backuplogmode) {
                case DBE_BACKUPLM_DELETELOGS:
                    backup->bu_deletelog = TRUE;
                    break;
                case DBE_BACKUPLM_KEEPLOGS:
                    backup->bu_deletelog = FALSE;
                    break;
                default:
                    ss_rc_derror(backuplogmode);
                    /* FALLTHROUGH in product compilation */
                case DBE_BACKUPLM_DEFAULT:
                    if (netp) {
                        dbe_cfg_getnetbackupdeletelog(cfg, &backup->bu_deletelog);
                    } else {
                        dbe_cfg_getbackupdeletelog(cfg, &backup->bu_deletelog);
                    }
                    break;
            }
            if (netp) {
                dbe_cfg_getnetbackupcopyinifile(cfg, &backup->bu_copyinifile);
            } else {
                dbe_cfg_getbackupcopyinifile(cfg, &backup->bu_copyinifile);
            }
            if (netp) {
                dbe_cfg_getnetbackupcopysolmsgout(
                        cfg, &backup->bu_copysolmsgout);
            } else {
                dbe_cfg_getbackupcopysolmsgout(
                        cfg, &backup->bu_copysolmsgout);
            }
        }
        backup->bu_cpnum = dbe_counter_getcpnum(ctr);

        if (backup->bu_copylog || backup->bu_deletelog) {
            *p_rc = backup_getlogfnumrange(
                        ctr,
                        backup->bu_cfg,
                        cd,
                        backup->bu_cpnum,
                        &backup->bu_lastlogfnum,
                        &backup->bu_firstlogfnum,
                        &backup->bu_firstnotdeletelogfnum,
                        p_errh);

#ifdef SS_HSBG2
            backup->bu_hsb_start_logfnum = lp.lp_logfnum;
            if(hsb_enabled && backup->bu_hsb_start_logfnum < backup->bu_firstnotdeletelogfnum)  {
                backup->bu_firstnotdeletelogfnum = backup->bu_hsb_start_logfnum;
            }
#endif /* SS_HSBG2 */

            if (*p_rc != DBE_RC_SUCC) {
                SsMemFreeIfNotNULL(backupdir);
                SsMemFree(backup);
                return(NULL);
            }
        }

        backup->bu_indexsvfil_src = file->f_indexfile->fd_svfil;
        if (callbackfp == NULL) {
            backup->bu_callbackfp = NULL;
            backup->bu_callbackctx = NULL;
            backup->bu_indexsvfil_dest = su_svf_initcopy(
                                            backupdir,
                                            file->f_indexfile->fd_svfil,
                                            &backup->bu_indexsvfil_size,
                                            SS_BF_SEQUENTIAL|
                                            SS_BF_NOBUFFERING|
                                            SS_BF_WRITEONLY,
                                            p_rc);
            if (backup->bu_indexsvfil_dest == NULL) {
                ss_dassert(*p_rc != DBE_RC_SUCC);
                rs_error_create(p_errh, *p_rc);
                SsMemFree(backupdir);
                SsMemFree(backup);
                return(NULL);
            }
        } else {
            backup->bu_callbackfp = callbackfp;
            backup->bu_callbackctx = callbackctx;
            backup->bu_indexsvfil_dest = NULL;
            backup->bu_indexsvfil_size = su_svf_getsize(backup->bu_indexsvfil_src);
            if (dbefile_diskless) {
                backup->bu_indexcache = file->f_indexfile->fd_cache;
            }
        }

        backup->bu_logsvfil_src = NULL;
        backup->bu_logsvfil_dest = NULL;
        backup->bu_curr_copyfile_src = NULL;
        backup->bu_curr_copyfile_dest = NULL;

#ifndef SS_NOLOGGING
        if (backup->bu_copylog) {
            backup->bu_logsvfil_src = backup_logsvfil(backup, ctr, p_rc);
            backup->bu_logsvfil_dest = NULL;
            if (callbackfp == NULL) {
                if (*p_rc == SU_SUCCESS) {
                    backup->bu_logsvfil_dest = su_svf_initcopy(
                                                    backupdir,
                                                    backup->bu_logsvfil_src,
                                                    &backup->bu_logsvfil_size,
                                                    SS_BF_SEQUENTIAL|
                                                    SS_BF_NOBUFFERING|
                                                    SS_BF_WRITEONLY,
                                                    p_rc);
                }
                if (backup->bu_logsvfil_dest == NULL) {
                    ss_dassert(*p_rc != DBE_RC_SUCC);
                    rs_error_create(p_errh, *p_rc);
                    SsMemFree(backupdir);
                    su_svf_done(backup->bu_indexsvfil_dest);
                    su_svf_done(backup->bu_logsvfil_src);
                    SsMemFree(backup);
                    return(NULL);
                }
            } else {
                backup->bu_logsvfil_size =
                    su_svf_getsize(backup->bu_logsvfil_src);
            }
        }
#endif /* SS_NOLOGGING */

        backup->bu_dir = backupdir;

        if (backup->bu_deletelog) {
            *p_rc = backup_deletebackuplog(backup, p_errh);
            if (*p_rc != DBE_RC_SUCC) {
                if (backup->bu_indexsvfil_dest != NULL) {
                    su_svf_done(backup->bu_indexsvfil_dest);
                }
#ifndef SS_NOLOGGING
                if (backup->bu_copylog) {
                    su_svf_done(backup->bu_logsvfil_src);
                    if (backup->bu_logsvfil_dest != NULL) {
                        su_svf_done(backup->bu_logsvfil_dest);
                    }
                }
#endif /* SS_NOLOGGING */
                SsMemFreeIfNotNULL(backup->bu_dir);
                SsMemFree(backup);
                return(NULL);
            }
        }

        backup->bu_state = BUST_INDEX;
        backup->bu_blocksize = su_svf_getblocksize(backup->bu_indexsvfil_src);
        backup->bu_bufsize = backup_getbufsize(backup->bu_blocksize);
        backup->bu_cacmem = SsCacMemInit(backup->bu_bufsize, 1);
        backup->bu_buf = SsCacMemAlloc(backup->bu_cacmem);
        backup->bu_loc = 0;
        backup->bu_endloc = backup->bu_indexsvfil_size;

        return(backup);
}

/*##**********************************************************************\
 *
 *              dbe_backup_init
 *
 * Initializes a backup.
 *
 * Parameters :
 *
 *      cfg - in, hold
 *              Configuration object.
 *
 *      file - in, hold
 *              Database files.
 *
 *      ctr - in, use
 *              Counter object.
 *
 *      backupdir - in, use
 *              If NULL, the backup directory is taken from the configuration
 *              object.
 *
 *      replicap - in
 *              If TRUE, replication backup.
 *
 *      p_rc - out
 *              Error code is returned in *p_rc, if function return code
 *              is NULL.
 *
 *      p_errh - out
 *              Error info.
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_backup_t* dbe_backup_init(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_file_t* file,
        dbe_counter_t* ctr,
        char* backupdir,
        bool replicap,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        dbe_ret_t* p_rc,
        rs_err_t** p_errh)
{
        dbe_backup_t* backup;

        ss_dprintf_1(("dbe_backup_init:backupdir = %s\n",
            backupdir != NULL ? backupdir : "<default backup directory>"));
        backup = backup_init(cfg,
                             cd,
                             file,
                             ctr,
                             backupdir,
                             NULL,
                             NULL,
                             replicap,
                             DBE_BACKUPLM_DEFAULT,
#ifdef SS_HSBG2
                             hsb_enabled,
                             lp,
#endif /* SS_HSBG2 */
                             p_rc,
                             p_errh);
        return (backup);
}

dbe_backup_t* dbe_backup_initwithcallback(
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_file_t* file,
        dbe_counter_t* ctr,
        su_ret_t (*callbackfp)(   /* Callback function to write to */
                void* ctx,        /* user given stream. */
                dbe_backupfiletype_t ftype,
                ss_int4_t finfo,  /* log file number, unused for other files */
                su_daddr_t daddr, /* position in file */
                char* fname,      /* file name */
                void* data,       /* file data to write */
                size_t len),
        void* callbackctx,
        bool replicap,
        dbe_backuplogmode_t backuplogmode,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        dbe_ret_t* p_rc,
        rs_err_t** p_errh)
{
        dbe_backup_t* backup;

        ss_dprintf_1(("dbe_backup_init_rpc\n"));
        backup = backup_init(cfg,
                             cd,
                             file,
                             ctr,
                             NULL,
                             callbackfp,
                             callbackctx,
                             replicap,
                             backuplogmode,
#ifdef SS_HSBG2
                             hsb_enabled,
                             lp,
#endif /* SS_HSBG2 */
                             p_rc,
                             p_errh);
        return (backup);
}

/*##**********************************************************************\
 *
 *              dbe_backup_done
 *
 * Releases resources of backup object.
 *
 * Parameters :
 *
 *      backup - in, take
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_backup_done(dbe_backup_t* backup)
{
        backup_donecopyfile(backup);
        if (backup->bu_indexsvfil_dest != NULL) {
            su_svf_done(backup->bu_indexsvfil_dest);
        }
#ifndef SS_NOLOGGING
        if (backup->bu_logsvfil_src != NULL) {
            su_svf_done(backup->bu_logsvfil_src);
        }
        if (backup->bu_logsvfil_dest != NULL) {
            su_svf_done(backup->bu_logsvfil_dest);
        }
#endif /* SS_NOLOGGING */
        SsMemFreeIfNotNULL(backup->bu_dir);
        SsMemFree(backup);
}



/*#***********************************************************************\
 *
 *              backup_copy
 *
 * Copies one block from src to dest.
 *
 * Parameters :
 *
 *      backup - in out, use
 *
 *
 *      dest -
 *
 *
 *      src -
 *
 *
 *      read_locked - in
 *              If TRUE, the read from src is done in locked mode. Locked
 *              mode read is required for locked parts of the file like
 *              first DBE_INDEX_HEADERSIZE blocks of the index file.
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
static su_ret_t backup_copy(
        dbe_backup_t* backup,
        su_svfil_t* dest,
        su_svfil_t* src,
        bool read_locked,
        dbe_backupfiletype_t ftype)
{
        su_ret_t rc;
        size_t sizeread;
        size_t bufsize;
        size_t nblock;

        char* fname = NULL;
        su_daddr_t nblocks_left_at_physical_file;
        int filespecno;

        bufsize = backup->bu_bufsize;
        nblock = bufsize / backup->bu_blocksize;

        FAKE_CODE_BLOCK(FAKE_DBE_SLOWBACKUP,
            SsDbgPrintf("FAKE_DBE_SLOWBACKUP\n");
            SsThrSleep(1000L);
        );

        if (backup->bu_loc + nblock > backup->bu_endloc) {
            nblock = backup->bu_endloc - backup->bu_loc;
        }
        if (backup->bu_callbackfp != NULL) {
            fname = su_svf_getphysfilenamewithrange(
                        src,
                        backup->bu_loc,
                        &filespecno,
                        &nblocks_left_at_physical_file);
            ss_dassert(fname != NULL);
            if (nblock > nblocks_left_at_physical_file) {
                nblock = nblocks_left_at_physical_file;
            }
        }
        bufsize = nblock * backup->bu_blocksize;
        ss_dassert(bufsize <= backup->bu_bufsize);
        ss_dassert(backup->bu_loc + nblock <= backup->bu_endloc);

        ss_dprintf_1(("about to read from address %ld endloc=%ld physfilename=%s\n",
                      backup->bu_loc,
                      backup->bu_endloc,
                      (fname != NULL ? fname : "(no name)")));
        if (dbefile_diskless) {
            char* tmpbuf = backup->bu_buf;
            uint   i;

            sizeread = 0;
            rc = SU_SUCCESS;

            for (i = 0; i < nblock; i++) {
                 dbe_cacheslot_t* cacheslot;
                 char* dbuf;

                 cacheslot = dbe_cache_reach(
                                 backup->bu_indexcache,
                                 backup->bu_loc + i,
                                 DBE_CACHE_READONLY,
                                 0,
                                 &dbuf,
                                 NULL);
                 ss_dassert(cacheslot != NULL);
                 memcpy(tmpbuf, dbuf, backup->bu_blocksize);
                 dbe_cache_release(
                      backup->bu_indexcache,
                      cacheslot,
                      DBE_CACHE_CLEAN,
                      NULL);
                 tmpbuf += backup->bu_blocksize;
                 sizeread += backup->bu_blocksize;
            }

        } else if (read_locked) {
            rc = su_svf_readlocked_raw(
                    src,
                    backup->bu_loc,
                    backup->bu_buf,
                    bufsize,
                    &sizeread);
        } else {
            rc = su_svf_read_raw(
                    src,
                    backup->bu_loc,
                    backup->bu_buf,
                    bufsize,
                    &sizeread);
        }
        if (rc != SU_SUCCESS) {
            return(rc);
        }
        if (backup->bu_hsbcopy && (backup->bu_loc == 0 || backup->bu_loc == 1)) {
            dbe_header_sethsbcopy(backup->bu_buf, backup->bu_hsbcopycomplete);
            if (backup->bu_loc == 0 && sizeread > backup->bu_blocksize) {
                dbe_header_sethsbcopy(backup->bu_buf + backup->bu_blocksize, backup->bu_hsbcopycomplete);
            }
        }
        if (sizeread != bufsize) {
            ss_dassert(sizeread < bufsize);
            backup->bu_endloc = backup->bu_loc +
                (sizeread / backup->bu_blocksize);
        }
        if (backup->bu_callbackfp == NULL) {
            ss_dassert(dest != NULL);
            rc = su_svf_write(
                    dest,
                    backup->bu_loc,
                    backup->bu_buf,
                    sizeread);
        } else {
            ss_int4_t finfo;

            ss_dassert(dest == NULL);
            ss_dassert(fname != NULL);

            if (ftype == DBE_BACKUPFILE_LOG) {
                finfo = backup->bu_firstlogfnum + filespecno - 1;
            } else {
                finfo = -1;
            }
            rc = (*backup->bu_callbackfp)(
                    backup->bu_callbackctx,
                    ftype,
                    finfo,
                    backup->bu_loc,
                    fname,
                    backup->bu_buf,
                    sizeread);
        }
        if (rc != SU_SUCCESS) {
            return(rc);
        }
        backup->bu_loc += sizeread / backup->bu_blocksize;

        FAKE_CODE_BLOCK(
            FAKE_DBE_PRI_NETCOPY_CRASH, 
            {
                SsDbgPrintf("FAKE_DBE_PRI_NETCOPY_CRASH\n");
                ss_skipatexit = TRUE;
                SsExit(0);
            }
        );

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_backup_advance
 *
 * Advances backup one atomic step.
 *
 * Parameters :
 *
 *      backup - in out, use
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_backup_advance(
        dbe_backup_t* backup,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        SS_PMON_ADD(SS_PMON_BACKUPSTEP);

        switch (backup->bu_state) {

            case BUST_INDEX:
                if (backup->bu_loc < backup->bu_endloc) {
                    rc = backup_copy(
                            backup,
                            backup->bu_indexsvfil_dest,
                            backup->bu_indexsvfil_src,
                            backup->bu_loc < DBE_INDEX_HEADERSIZE,
                            DBE_BACKUPFILE_DB);
                    if (rc != SU_SUCCESS) {
                        rs_error_create(p_errh, rc);
                        return(rc);
                    }
                } else if (backup->bu_hsbcopy && !backup->bu_hsbcopycomplete) {
                    /* Resend first block with backup complete info set. */
                    backup->bu_hsbcopycomplete = TRUE;
                    backup->bu_loc = 0;
                    rc = backup_copy(
                            backup,
                            backup->bu_indexsvfil_dest,
                            backup->bu_indexsvfil_src,
                            backup->bu_loc < DBE_INDEX_HEADERSIZE,
                            DBE_BACKUPFILE_DB);
                    if (rc != SU_SUCCESS) {
                        rs_error_create(p_errh, rc);
                        return(rc);
                    }
                    backup->bu_loc = backup->bu_endloc;
                } else {
                    if (backup->bu_callbackfp == NULL) {
                        ss_dassert(backup->bu_indexsvfil_dest != NULL);
                        su_svf_flush(backup->bu_indexsvfil_dest);
                        su_svf_done(backup->bu_indexsvfil_dest);
                        backup->bu_indexsvfil_dest = NULL;
                    } else {
                        ss_dassert(backup->bu_indexsvfil_dest == NULL);
                        (*backup->bu_callbackfp)(
                                backup->bu_callbackctx,
                                DBE_BACKUPFILE_DB,
                                -1,
                                SU_DADDR_NULL,
                                NULL,
                                NULL,
                                0);
                    }
                    SsCacMemFree(backup->bu_cacmem, backup->bu_buf);
                    SsCacMemDone(backup->bu_cacmem);
                    backup->bu_buf = NULL;
                    backup->bu_cacmem = NULL;
                    backup->bu_state = BUST_INIFILE;
#ifndef SS_NOLOGGING
                    if (backup->bu_copylog) {
                        backup->bu_state = BUST_LOG;
                        backup->bu_blocksize = su_svf_getblocksize(backup->bu_logsvfil_src);
                        backup->bu_bufsize = backup_getbufsize(backup->bu_blocksize);
                        backup->bu_cacmem = SsCacMemInit(backup->bu_bufsize, 1);
                        backup->bu_buf = SsCacMemAlloc(backup->bu_cacmem);
                        backup->bu_loc = 0;
                        backup->bu_endloc = backup->bu_logsvfil_size;
                    }
#endif /* SS_NOLOGGING */
                }
                return(DBE_RC_CONT);

#ifndef SS_NOLOGGING
            case BUST_LOG:
                if (backup->bu_loc < backup->bu_endloc) {
                    rc = backup_copy(
                            backup,
                            backup->bu_logsvfil_dest,
                            backup->bu_logsvfil_src,
                            FALSE,
                            DBE_BACKUPFILE_LOG);
                    if (rc != SU_SUCCESS) {
                        rs_error_create(p_errh, rc);
                        return(rc);
                    }
                } else {
                    if (backup->bu_callbackfp == NULL) {
                        ss_dassert(backup->bu_logsvfil_dest != NULL);
                        su_svf_flush(backup->bu_logsvfil_dest);
                        su_svf_done(backup->bu_logsvfil_dest);
                        backup->bu_logsvfil_dest = NULL;
                    } else {
                        ss_dassert(backup->bu_logsvfil_dest == NULL);
                        (*backup->bu_callbackfp)(
                                backup->bu_callbackctx,
                                DBE_BACKUPFILE_LOG,
                                -1,
                                SU_DADDR_NULL,
                                NULL,
                                NULL,
                                0);
                    }
                    su_svf_done(backup->bu_logsvfil_src);
                    backup->bu_logsvfil_src = NULL;
                    SsCacMemFree(backup->bu_cacmem, backup->bu_buf);
                    SsCacMemDone(backup->bu_cacmem);
                    backup->bu_buf = NULL;
                    backup->bu_cacmem = NULL;
                    backup->bu_state = BUST_INIFILE;
                }
                return(DBE_RC_CONT);
#endif /* SS_NOLOGGING */

            case BUST_INIFILE:
                if (backup->bu_copyinifile) {
                    su_inifile_t* inifile = dbe_cfg_getinifile(backup->bu_cfg);
                    char* inifile_name = su_inifile_getname(inifile);

                    backup_initcopyfileifneeded(backup,
                                                inifile_name,
                                                DBE_BACKUPFILE_INIFILE);
                    if (backup->bu_loc < backup->bu_endloc) {
                        rc = backup_copy(backup,
                                         backup->bu_curr_copyfile_dest,
                                         backup->bu_curr_copyfile_src,
                                         FALSE,
                                         DBE_BACKUPFILE_INIFILE);
                        if (rc != SU_SUCCESS) {
                            rs_error_create(p_errh, rc);
                            return(rc);
                        }
                    } else {
                        if (backup->bu_callbackfp != NULL) {
                            (*backup->bu_callbackfp)(backup->bu_callbackctx,
                                                     DBE_BACKUPFILE_INIFILE,
                                                     -1,
                                                     SU_DADDR_NULL,
                                                     NULL,
                                                     NULL,
                                                     0);
                        }
                        backup_donecopyfile(backup);
                        backup->bu_state = BUST_SOLMSGOUT;
                    }
                } else {
                    backup->bu_state = BUST_SOLMSGOUT;
                }
                return(DBE_RC_CONT);
            case BUST_SOLMSGOUT:
                if (backup->bu_copysolmsgout) {
                    backup_initcopyfileifneeded(backup,
                                                (char *)SU_SOLID_MSGFILENAME,
                                                DBE_BACKUPFILE_INIFILE);
                    if (backup->bu_loc < backup->bu_endloc) {
                        rc = backup_copy(backup,
                                         backup->bu_curr_copyfile_dest,
                                         backup->bu_curr_copyfile_src,
                                         FALSE,
                                         DBE_BACKUPFILE_SOLMSGOUT);
                        if (rc != SU_SUCCESS) {
                            rs_error_create(p_errh, rc);
                            return(rc);
                        }
                    } else {
                        if (backup->bu_callbackfp != NULL) {
                            (*backup->bu_callbackfp)(backup->bu_callbackctx,
                                                     DBE_BACKUPFILE_INIFILE,
                                                     -1,
                                                     SU_DADDR_NULL,
                                                     NULL,
                                                     NULL,
                                                     0);
                        }
                        backup_donecopyfile(backup);
                        backup->bu_state = BUST_END;
                    }
                } else {
                    backup->bu_state = BUST_END;
                }
                return(DBE_RC_CONT);
            case BUST_END:
                if (backup->bu_deletelog) {
                    rc = backup_deletedblog(
                            backup->bu_cfg,
                            backup->bu_firstlogfnum,
                            backup->bu_firstnotdeletelogfnum,
                            backup->bu_hsb_start_logfnum,
                            p_errh);
                    backup->bu_deletelog = FALSE;
                    if (rc == DBE_RC_SUCC) {
                        return(DBE_RC_END);
                    } else {
                        rs_error_create(p_errh, rc);
                        return(rc);
                    }
                } else {
                    return(DBE_RC_END);
                }
            default:
                ss_error;
                return(DBE_RC_END);
        }
}

/*##**********************************************************************\
 *
 *              dbe_backup_getcurdir
 *
 * Returns currectly used backup directory.
 *
 * Parameters :
 *
 *      backup - in
 *              Active backup object.
 *
 * Return value - ref :
 *
 *      Backup directory.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* dbe_backup_getcurdir(dbe_backup_t* backup)
{
        return(backup->bu_dir);
}

void dbe_backup_getlogfnumrange(
        dbe_backup_t* backup,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end)
{
        dbe_logfnum_t dummy;

        backup_getlogfnumrange(
                        backup->bu_ctr,
                        backup->bu_cfg,
                        backup->bu_cd,
                        backup->bu_cpnum,
                        p_logfnum_end,
                        p_logfnum_start,
                        &dummy,
                        NULL);
}

void dbe_backup_getlogfnumrange_withoutbackupobject(
        dbe_counter_t* ctr,
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
        dbe_cpnum_t cpnum,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end)
{
        dbe_logfnum_t dummy;
        backup_getlogfnumrange(
                ctr,
                cfg,
                cd,
                cpnum,
                p_logfnum_end,
                p_logfnum_start,
                &dummy,
                NULL);
}
/*##**********************************************************************\
 *
 *              dbe_backup_deletelog_cp
 *
 * Deletes log files that can be removed after checkpoint
 * creation if full backup recovery is not desired.
 * This option is for saving disk space.
 *
 * Parameters :
 *
 *      ctr - use
 *              counter
 *
 *      cfg - in
 *              config
 *
 *      p_errh - out, give
 *              in case of error this is a pointer to created
 *          error handle
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      error code when failed
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_backup_deletelog_cp(
        dbe_counter_t* ctr,
        dbe_cfg_t* cfg,
        rs_sysi_t* cd,
#ifdef SS_HSBG2
        bool hsb_enabled,
        dbe_catchup_logpos_t lp,
#endif /* SS_HSBG2 */
        rs_err_t** p_errh)
{
        dbe_ret_t rc;
        dbe_cpnum_t cpnum;
        dbe_logfnum_t lastlogfnum;
        dbe_logfnum_t firstlogfnum;
        dbe_logfnum_t firstnotdeletelogfnum;
        bool deletelog;

        ss_dprintf_1(("dbe_backup_deletelog_cp:hsb_enabled=%d, hsb logpos (%d,%s,%d,%d,%d)\n", hsb_enabled, LOGPOS_DSDDD(lp)));

        dbe_cfg_getcpdeletelog(cfg, &deletelog);
        FAKE_CODE_BLOCK_GT(
            FAKE_HSBG2_PRI_CP_DELETE_LOG_FILES, 0,
            {
                SsDbgPrintf("FAKE_HSBG2_PRI_CP_DELETE_LOG_FILES\n");
                hsb_enabled = FALSE;
                deletelog = TRUE;
            }
        );
        if (!deletelog) {
            /* Nothing to do! */
            return (DBE_RC_SUCC);
        }

#ifdef SS_HSBG2
        if(hsb_enabled && DBE_CATCHUP_LOGPOS_ISNULL(lp)) {
            /* Not allowed to delete anything */
            return (DBE_RC_SUCC);
        }
#endif /* SS_HSBG2 */

        cpnum = dbe_counter_getcpnum(ctr);
        rc = backup_getlogfnumrange(
                    ctr,
                    cfg,
                    cd,
                    cpnum,
                    &lastlogfnum,
                    &firstlogfnum,
                    &firstnotdeletelogfnum,
                    p_errh);

#ifdef SS_HSBG2
        if(hsb_enabled
        && !DBE_CATCHUP_LOGPOS_ISNULL(lp)
        && lp.lp_logfnum < firstnotdeletelogfnum) {
            firstnotdeletelogfnum = lp.lp_logfnum;
            if (firstnotdeletelogfnum < firstlogfnum) {
                ss_dprintf_2(("dbe_backup_deletelog_cp:new firstlogfnum=%ld\n", firstnotdeletelogfnum));
                firstlogfnum = firstnotdeletelogfnum;
            }
        }
#endif /* SS_HSBG2 */

        if (rc == DBE_RC_SUCC) {
            ss_dassert(firstnotdeletelogfnum <= lastlogfnum);
            ss_dassert(firstlogfnum  <= firstnotdeletelogfnum);

            rc = backup_deletedblog(
                    cfg,
                    firstlogfnum,
                    firstnotdeletelogfnum,
                    0,
                    p_errh);
        }
        return (rc);
}

#endif /* SS_NOBACKUP */
