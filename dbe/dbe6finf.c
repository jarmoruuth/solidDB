/*************************************************************************\
**  source       * dbe6finf.c
**  directory    * dbe
**  description  * File info data structure.
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

This module implements a file info object. The file info object is
used to create or open a file, typically a database file. The object
constructor takes file openinfo as parameter. The file specified
in openinfo is opened as split virtual file and cache system and
a free list are also created for the file.

When a new file is created, DBE_INDEX_HEADERSIZE blocks at the
beginning of the file are reserved for internal usage to store
internal variables. These internal variables include Bonsai and
permanent tree root addresses, file buffer size, freelist address
and the file size. Note that stored file size is not necessarily the
same as the physical size, because the free list system keeps the logical
file size, which may be smaller than the physical size, because the file
is not physically truncated eg. after system crash when we return to the
newest checkpoint and all file space allocated after the checkpoint
becomes logically nonexistent.

When an existing file is opened, values of internal variables are
read from the file header blocks.

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
#include <ssmem.h>
#include <sssprint.h>
#include <ssdebug.h>
#include <ssfile.h>
#include <ssstring.h>
#include <sschcvt.h>

#include <su0vfil.h>
#include <su0svfil.h>
#include <su0error.h>
#include <su0cfgst.h>
#include <su0bsrch.h>
#include <su0param.h>

#ifndef SS_MYSQL
#include <su0svfcrypt.h>
#endif /* !SS_MYSQL */

#include <ui0msg.h>

#include "dbe8flst.h"
#include "dbe8cach.h"
#include "dbe6finf.h"
#include "dbe0type.h"
#include "dbe0crypt.h"
#include "dbe0db.h"

extern bool dbefile_diskless; /* for diskless, no physical dbfile */

su_svfil_t*  fd_svfil_diskless = NULL;
dbe_cache_t* fd_cache_diskless = NULL;

static void dbe_filedes_saveheader1(
        dbe_filedes_t* filedes);

static void dbe_filedes_saveheader2(
        dbe_filedes_t* filedes);

#ifndef SS_MYSQL
static su_cryptoalg_t dbe_file_getcryptoalg(
        dbe_filedes_t* filedes);
#endif

typedef struct {
        size_t dn_nelems;
        uint   dn_numarr[1];
} disknoarr_t;

#define DISKNUMARR_SIZE(nelems) \
        ((char*)&((disknoarr_t*)NULL)->dn_numarr[nelems] - (char*)NULL)

static disknoarr_t* disknoarr_init(void)
{
        disknoarr_t* disknoarr;

        disknoarr = SsMemAlloc(DISKNUMARR_SIZE(0));
        disknoarr->dn_nelems = 0;
        return (disknoarr);
}

static void disknoarr_done(disknoarr_t* disknoarr)
{
        SsMemFree(disknoarr);
}

static int disknoarr_cmp(const void* p1, const void* p2)
{
        return ((int)*(const uint*)p1 - (int)*(const uint*)p2);
}

static disknoarr_t* disknoarr_add(disknoarr_t* disknoarr, uint diskno)
{
        size_t dn_idx;
        uint* p_dn;
        bool found;

        if (disknoarr->dn_nelems > 0) {
            found = su_bsearch(
                        &diskno,
                        disknoarr->dn_numarr,
                        disknoarr->dn_nelems,
                        sizeof(disknoarr->dn_numarr[0]),
                        disknoarr_cmp,
                        (void**)&p_dn);
            if (found) {
                return (disknoarr);
            }
        } else {
            p_dn = &disknoarr->dn_numarr[0];
        }
        dn_idx = p_dn - disknoarr->dn_numarr;
        disknoarr =
            SsMemRealloc(
                disknoarr,
                DISKNUMARR_SIZE(disknoarr->dn_nelems + 1));
        p_dn = &disknoarr->dn_numarr[dn_idx];
        if (dn_idx < disknoarr->dn_nelems) {
            memmove(
                p_dn + 1,
                p_dn,
                sizeof(disknoarr->dn_numarr[0])
                * (disknoarr->dn_nelems - dn_idx));
        } else {
            ss_dassert(dn_idx == disknoarr->dn_nelems);
        }
        *p_dn = diskno;
        disknoarr->dn_nelems++;
        ss_dprintf_1(("disknoarr_add: now dn_nelems = %d\n",
                      (int)disknoarr->dn_nelems));
        return (disknoarr);
}

static uint disknoarr_densify(disknoarr_t* disknoarr, uint diskno)
{
        uint* p_dn;
        bool found;

        found = su_bsearch(
                    &diskno,
                    disknoarr->dn_numarr,
                    disknoarr->dn_nelems,
                    sizeof(disknoarr->dn_numarr[0]),
                    disknoarr_cmp,
                    (void**)&p_dn);
        ss_dassert(found);
        return (uint)(p_dn - disknoarr->dn_numarr);
}

dbe_cache_t *dbe_cache_cfg_init(
	dbe_cfg_t *cfg,
	su_svfil_t *svfil,
	uint blocksize)
{
        bool found;
        bool succp;
        size_t cache_size;
        dbe_cache_t *dbcache = NULL;
        ulong maxpagesemcount;
        uint cache_preflushpercent;
        uint cache_lastuseskippercent;

        found = dbe_cfg_getidxcachesize(cfg, &cache_size);
        if (cache_size <= 20 * blocksize) {

            su_informative_exit(__FILE__, __LINE__,
                                DBE_ERR_TOOSMALLCACHE_SSUU,
                                SU_DBE_INDEXSECTION,
                                SU_DBE_CACHESIZE,
                                (ulong)cache_size,
                                (ulong)(21L * blocksize));
        }
        found = dbe_cfg_getidxpreflushperc(cfg, &cache_preflushpercent);
        found = dbe_cfg_getidxlastuseLRUskipperc(cfg, &cache_lastuseskippercent);
        found = dbe_cfg_getidxmaxpagesemcount(cfg, &maxpagesemcount);

        dbcache = dbe_cache_init(
                    svfil,
                    (uint)(cache_size / blocksize),
                    (uint)maxpagesemcount);
        succp = dbe_cache_setpreflushinfo(
                        dbcache,
                        cache_preflushpercent,
                        cache_lastuseskippercent);
        ss_dassert(succp);

        return dbcache;
}

/*#***********************************************************************\
 *
 *		dbe_idxfiledes_init
 *
 * Creates an index file descriptor. It is not fully operable yet.
 * (see dbe_idxfiledes_start() below)
 *
 * Parameters :
 *
 *	cfg - in, use
 *		pointer to configuration object
 *
 * Return value - give :
 *      pointer to created file descriptor
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_filedes_t* dbe_idxfiledes_init(dbe_cfg_t* cfg)
{
        su_pa_t* filespec_array;
        bool found;
        bool blocksize_found;
        su_ret_t rc;
        uint i;
        dbe_filespec_t* filespec;
        dbe_filedes_t* filedes;
        su_daddr_t filesize;
        int flags;
        disknoarr_t* disknoarr;
        bool readonlyp;
        int writeflushmode;
#ifdef FSYNC_OPT
        bool    syncwrite;
        bool    fileflush;
#else
        bool synchronizedwrite;
#endif
#ifdef IO_OPT
        bool    directio;
#endif

        filedes = SSMEM_NEW(dbe_filedes_t);
        blocksize_found = dbe_cfg_getidxblocksize(cfg, &filedes->fd_blocksize);
#ifdef SS_MT
        flags = 0;
#else
        flags = SS_BF_EXCLUSIVE;
#endif

#ifdef FSYNC_OPT
        dbe_cfg_getindexfileflush(cfg, &fileflush);
        if (!fileflush) {
            flags |= SS_BF_NOFLUSH;
        }

        (void) dbe_cfg_getsyncwrite(cfg, &syncwrite);

        if (syncwrite) {
            flags |= SS_BF_SYNCWRITE;
        }
#else
        (void) dbe_cfg_getsynchronizedwrite(cfg, &synchronizedwrite);
        if (synchronizedwrite) {
            flags |= SS_BF_SYNCWRITE;
        }
#endif
        
#ifdef IO_OPT
        dbe_cfg_getindexdirectio(cfg, &directio);
        if (directio) {
            flags |= SS_BF_DIRECTIO;
        }
#else
        flags |= SS_BF_NOBUFFERING;
#endif
        
        found = dbe_cfg_getwriteflushmode(cfg, &writeflushmode);
        switch (writeflushmode) {
            case SS_BFLUSH_NORMAL:
                break;
            case SS_BFLUSH_BEFOREREAD:
                flags |= SS_BF_FLUSH_BEFOREREAD;
                break;
            case SS_BFLUSH_AFTERWRITE:
                flags |= SS_BF_FLUSH_AFTERWRITE;
                break;
            default:
                ss_rc_derror(writeflushmode);
                break;
        }
        (void)dbe_cfg_getreadonly(cfg, &readonlyp);
        if (readonlyp) {
            flags |= SS_BF_READONLY;
        }

        /* for diskless, do not create the file physically. */
        if (dbefile_diskless) {
            flags |= SS_BF_DISKLESS|SS_BF_DLSIZEONLY;
        }
        filespec_array = su_pa_init();
        found = dbe_cfg_getidxfilespecs(cfg, filespec_array);
        if (su_pa_nelems(filespec_array) > 0) {
            size_t blocksize;
            for (i = 0; ; i++) {
                if (su_pa_indexinuse(filespec_array, i)) {
                    filespec = su_pa_getdata(filespec_array, i);
                    break;
                }
            }
            if (dbe_header_readblocksize(
                        dbe_filespec_getname(filespec),
                        &blocksize))
            {
                ss_dprintf_1(("Got blocksize %d from header\n", blocksize));
                if (filedes->fd_blocksize != blocksize) {
                    if (blocksize_found) {
                        /* not considered fatal any more! */
#ifdef SS_MYSQL
                        ui_msg_warning(DBE_WARN_WRONGBLOCKSIZE_SSD,
                                       SU_MYSQLD_SECTION,
                                       SU_MYSQLD_DBE_BLOCKSIZE,
                                       (long)blocksize);
#else
                        ui_msg_warning(DBE_WARN_WRONGBLOCKSIZE_SSD,
                                       SU_DBE_INDEXSECTION,
                                       SU_DBE_BLOCKSIZE,
                                       (long)blocksize);
#endif
                    }
                    filedes->fd_blocksize = blocksize;
                    dbe_cfg_settmpidxblocksize(cfg, blocksize);
                }
            }
        }

        if (!fd_svfil_diskless) {
            filedes->fd_svfil = su_svf_init(filedes->fd_blocksize, flags);

            disknoarr = disknoarr_init();
            su_pa_do_get(filespec_array, i, filespec) {
                disknoarr = disknoarr_add(
                                disknoarr,
                                dbe_filespec_getdiskno(filespec));
            }
            su_pa_do_get(filespec_array, i, filespec) {
                rc = su_svf_addfile2(
                        filedes->fd_svfil,
                        dbe_filespec_getname(filespec),
                        dbe_filespec_getmaxsize(filespec),
                        (i == 0) ? TRUE : FALSE,
                        disknoarr_densify(
                            disknoarr, dbe_filespec_getdiskno(filespec)));
                if (rc != SU_SUCCESS) {
                    su_informative_exit(__FILE__, __LINE__,
                        DBE_ERR_CANNOTOPENDB_SSD,
                        dbe_filespec_getname(filespec),
                        su_rc_nameof(rc),
                        (int)rc);
                }
                dbe_filespec_done(filespec);
            }
            disknoarr_done(disknoarr);
        } else {
            filedes->fd_svfil = fd_svfil_diskless;
        }
        su_pa_done(filespec_array);
        filespec_array = NULL;

        filesize = su_svf_getsize(filedes->fd_svfil);
        filedes->fd_created = (filesize == 0L);
        if (filedes->fd_created) {
            rc = su_svf_extendsize(filedes->fd_svfil, DBE_INDEX_HEADERSIZE);
            su_rc_assert(rc == SU_SUCCESS, rc);
            su_svf_flush(filedes->fd_svfil);
        }
        rc = su_svf_lockrange(filedes->fd_svfil, 0L, DBE_INDEX_HEADERSIZE);
        if (rc != SU_SUCCESS) {
            su_informative_exit(__FILE__, __LINE__, DBE_ERR_DBALREADYINUSE);
        }

        if (!fd_cache_diskless) {
            filedes->fd_cache = dbe_cache_cfg_init(
					cfg,
					filedes->fd_svfil,
					(uint)filedes->fd_blocksize);
        } else {
            filedes->fd_cache = fd_cache_diskless;
        }
        filedes->fd_dbheader = dbe_header_init(filedes->fd_blocksize);
        if (filedes->fd_created) {
            dbe_header_setfilesize(
                filedes->fd_dbheader,
                DBE_INDEX_HEADERSIZE);
        }
        filedes->fd_olddbheader = NULL;
        filedes->fd_freelist = NULL;
        filedes->fd_chlist = NULL;
        filedes->fd_cplist = NULL;
        filedes->fd_cprec = NULL;
        filedes->fd_fnum = DBE_FNUM_INDEXFILE;
        filedes->fd_cipher = NULL;
        return (filedes);
}

/*##**********************************************************************\
 *
 *		dbe_idxfiledes_start
 *
 * Starts index file descriptor into fully operable mode.
 *
 * Parameters :
 *
 *	filedes - in out, use
 *		pointer to file descriptor object
 *
 *	cfg - in, use
 *          pointer to configuration object
 *
 *	crashed - in
 *		TRUE if recovering a crashed database or
 *          FALSE when the database has been closed gracefully
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_idxfiledes_start(
        dbe_file_t* file,
        dbe_filedes_t* filedes,
        dbe_cfg_t* cfg,
        bool crashed)
{
        uint extend_incr;
        uint max_seq_alloc;
        su_daddr_t daddr;
        su_daddr_t physical_filesize;
        su_daddr_t logical_filesize;
        bool fl_globally_sorted;
        su_ret_t rc;

        dbe_cfg_getidxextendincr(cfg, &extend_incr);
        dbe_cfg_getidxmaxseqalloc(cfg, &max_seq_alloc);
        dbe_cfg_getfreelistgloballysorted(cfg, &fl_globally_sorted);
        if (filedes->fd_freelist != NULL) {
            dbe_fl_done(filedes->fd_freelist);
        }
        physical_filesize = su_svf_getsize(filedes->fd_svfil);
        logical_filesize = dbe_header_getfilesize(filedes->fd_dbheader);
        /* TPR 450484, partial fix */
        /*ss_rc_assert(physical_filesize >= logical_filesize, logical_filesize);*/
        if (physical_filesize < logical_filesize) {
           su_informative_exit(__FILE__, __LINE__, DBE_ERR_WRONGSIZE);
        }

        filedes->fd_freelist =
            dbe_fl_init(
                filedes->fd_svfil,
                filedes->fd_cache,
                dbe_header_getfreelistaddr(filedes->fd_dbheader),
                crashed ? physical_filesize : logical_filesize,
                extend_incr,
                max_seq_alloc,
                fl_globally_sorted,
                dbe_header_getcpnum(filedes->fd_dbheader),
                file->f_db);
        if (filedes->fd_chlist != NULL) {
            dbe_cl_done(filedes->fd_chlist);
        }
        filedes->fd_chlist =
            dbe_cl_init(
                filedes->fd_svfil,
                filedes->fd_cache,
                filedes->fd_freelist,
                dbe_header_getcpnum(filedes->fd_dbheader),
                dbe_header_getchlistaddr(filedes->fd_dbheader));
        dbe_fl_setchlist(filedes->fd_freelist, filedes->fd_chlist);
        if (filedes->fd_cplist != NULL) {
            dbe_cpl_done(filedes->fd_cplist);
        }
        filedes->fd_cplist =
            dbe_cpl_init(
                filedes->fd_svfil,
                filedes->fd_cache,
                filedes->fd_freelist,
                filedes->fd_chlist,
                dbe_header_getcplistaddr(filedes->fd_dbheader));
        if (crashed) {
            rc = dbe_cpl_save(
                    filedes->fd_cplist,
                    dbe_header_getcpnum(filedes->fd_dbheader),
                    &daddr);
            su_rc_assert(rc == SU_SUCCESS, rc);
            ss_dprintf_1(("dbe_cpl_save() at %s %d daddr = %ld cpnum=%ld\n",
                          __FILE__, __LINE__, daddr,
                          dbe_header_getcpnum(filedes->fd_dbheader)));
            dbe_header_setcplistaddr(filedes->fd_dbheader, daddr);
            for (daddr = physical_filesize; daddr > logical_filesize; ) {
                daddr--;
                dbe_fl_free(filedes->fd_freelist, daddr);
            }
        }
        DBE_CPMGR_CRASHPOINT(9);
}

/*#***********************************************************************\
 *
 *		dbe_idxfiledes_savestep1
 *
 * Does 1st step of dbe_file_save() for 1 file descriptor
 *
 * Parameters :
 *
 *	filedes - in out, use
 *		pointer to file descriptor object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_idxfiledes_savestep1(
        dbe_filedes_t* filedes)
{
        ss_dassert(filedes != NULL);
        if (filedes->fd_olddbheader != NULL) {
            dbe_header_done(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }
        dbe_header_setdbstate(filedes->fd_dbheader, DBSTATE_CRASHED);
}

/*#***********************************************************************\
 *
 *		dbe_idxfiledes_savestep2
 *
 * Does 2nd step of dbe_file_save() for 1 file descriptor
 *
 * Parameters :
 *
 *	filedes - in out, use
 *		pointer to file descriptor object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_idxfiledes_savestep2(
        dbe_filedes_t* filedes)
{
        dbe_cpnum_t cpnum;
        su_daddr_t cplist_addr;
        su_ret_t rc;
        su_daddr_t freelist_addr;
        su_daddr_t freelist_filesize;
        su_daddr_t chlist_addr;

        cpnum = dbe_header_getcpnum(filedes->fd_dbheader);
        if (filedes->fd_olddbheader != NULL) {
            cplist_addr = dbe_header_getcplistaddr(filedes->fd_olddbheader);
            if (cplist_addr != SU_DADDR_NULL) {
                rc = dbe_cpl_deletefromdisk(
                        filedes->fd_cplist,
                        cpnum,
                        cplist_addr);
                su_rc_assert(rc == SU_SUCCESS, rc);
            }
            dbe_header_done(filedes->fd_olddbheader);
            filedes->fd_olddbheader = NULL;
        }
        rc = dbe_cl_preparetosave(filedes->fd_chlist);
        su_rc_assert(rc == SU_SUCCESS, rc);
        DBE_CPMGR_CRASHPOINT(3);
        rc = dbe_fl_save(
                filedes->fd_freelist,
                cpnum,
                &freelist_addr,
                &freelist_filesize);
        su_rc_assert(rc == SU_SUCCESS, rc);
        DBE_CPMGR_CRASHPOINT(4);
        rc = dbe_cl_save(
                filedes->fd_chlist,
                cpnum,
                &chlist_addr);
        su_rc_assert(rc == SU_SUCCESS, rc);
        dbe_header_setfreelistaddr(filedes->fd_dbheader, freelist_addr);
        dbe_header_setfilesize(filedes->fd_dbheader, freelist_filesize);
        dbe_header_setchlistaddr(filedes->fd_dbheader, chlist_addr);
        dbe_header_setdbstate(filedes->fd_dbheader, DBSTATE_CLOSED);
        DBE_CPMGR_CRASHPOINT(1);
}

/*#***********************************************************************\
 *
 *		dbe_filedes_done
 *
 * Deletes file descriptor object
 *
 * Parameters :
 *
 *	filedes - in, take
 *		pointer to file descriptor object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_filedes_done(dbe_filedes_t* filedes)
{
        ss_dassert(filedes != NULL);
        if (filedes->fd_freelist != NULL) {
            dbe_fl_done(filedes->fd_freelist);
        }
        if (filedes->fd_dbheader != NULL) {
            dbe_header_done(filedes->fd_dbheader);
        }
        if (filedes->fd_olddbheader != NULL) {
            dbe_header_done(filedes->fd_olddbheader);
        }
        if (filedes->fd_cplist != NULL) {
            dbe_cpl_done(filedes->fd_cplist);
        }
        if (filedes->fd_chlist != NULL) {
            dbe_cl_done(filedes->fd_chlist);
        }
        if (filedes->fd_cprec != NULL) {
            SsMemFree(filedes->fd_cprec);
        }
        if (filedes->fd_cache != NULL) {
            dbe_cache_done(filedes->fd_cache);
        }
        if (filedes->fd_svfil != NULL) {
            su_svf_done(filedes->fd_svfil);
        }
        SsMemFree(filedes);
}

/*##**********************************************************************\
 *
 *		dbe_file_exist
 *
 * Checks if the database file exist.
 *
 * Parameters :
 *
 *	cfg - in
 *		Database configuration.
 *
 * Return value :
 *
 *      TRUE    - database file exists
 *      FALSE   - database does not file exists
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_file_exist(
        dbe_cfg_t* cfg)
{
        su_pa_t* filespec_array;
        dbe_filespec_t* filespec;
        uint i;
        bool exists = FALSE;

        ss_dassert(cfg != NULL);

        filespec_array = su_pa_init();
        dbe_cfg_getidxfilespecs(cfg, filespec_array);
        su_pa_do_get(filespec_array, i, filespec) {
            if (SsFExist(dbe_filespec_getname(filespec))) {
                exists = TRUE;
                break;
            }
        }
        su_pa_do_get(filespec_array, i, filespec) {
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);

        return(exists);
}

/*##**********************************************************************\
 *
 *		dbe_file_existall
 *
 * Checks if all the database files exist.
 *
 * Parameters :
 *
 *	cfg - in
 *		Database configuration.
 *
 * Return value :
 *
 *      TRUE    - database files exists
 *      FALSE   - one or more databases do not file exist
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_file_existall(
        dbe_cfg_t* cfg)
{
        su_pa_t* filespec_array;
        dbe_filespec_t* filespec;
        uint i;
        bool exists = FALSE;
        bool firstexistingfound = FALSE;
        bool firstmissingfound = FALSE;

        ss_dassert(cfg != NULL);

        filespec_array = su_pa_init();
        dbe_cfg_getidxfilespecs(cfg, filespec_array);
        su_pa_do_get(filespec_array, i, filespec) {
            if (SsFExist(dbe_filespec_getname(filespec))) {
                if (firstmissingfound) { /* if one or more db files are missing between 2 existing db files, FALSE returned */
                    exists = FALSE;
                    break;
                }
                exists = TRUE;
                firstexistingfound = TRUE; /* FileSpec_1 is found */
            } else if (!firstexistingfound) { /* if FileSpec_1 is missing, FALSE returned */
                exists = FALSE;
                break;
            } else if (firstexistingfound) {
                firstmissingfound = TRUE; /* at least one db file is missing after FileSpec_1 is found */
            }
        }
        su_pa_do_get(filespec_array, i, filespec) {
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);

        return(exists);
}

/*##**********************************************************************\
 *
 *		dbe_file_init
 *
 * Creates a dbe_file_t object (not into fully operable mode, yet)
 *
 * Parameters :
 *
 *	cfg - in, use
 *		pointer to configuration object
 *
 * Return value - give :
 *      pointer to created database file object
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_file_t* dbe_file_init(
        dbe_cfg_t* cfg,
        dbe_db_t* db)
{
        dbe_file_t* dbfile;

        dbfile = SSMEM_NEW(dbe_file_t);
        dbfile->f_indexfile = dbe_idxfiledes_init(cfg);
        dbfile->f_blobfiles = NULL;
        dbfile->f_log = NULL;
        dbfile->f_db = db;
        return (dbfile);
}

/*##**********************************************************************\
 *
 *		dbe_file_start
 *
 * Sets a dbe_file_t object into fully operable mode.
 *
 * Parameters :
 *
 *	dbfile - in out, use
 *		pointer to database file object
 *
 *	cfg - in, use
 *		pointer to database
 *
 *	crashed - in
 *		TRUE if recovering a crashed database or
 *          FALSE when the database has been closed gracefully
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_file_start(
        dbe_file_t* dbfile,
        dbe_cfg_t* cfg,
        bool crashed)
{
        dbe_filedes_t* filedes = dbfile->f_indexfile;
        dbe_idxfiledes_start(dbfile, filedes, cfg, crashed);
}

/*##**********************************************************************\
 *
 *		dbe_file_done
 *
 * Deletes a database file object (does not save!)
 *
 * Parameters :
 *
 *	dbfile - in, take
 *		pointer to database file object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_file_done(
        dbe_file_t* dbfile)
{
        dbe_filedes_t* filedes = dbfile->f_indexfile;

        ss_dassert(dbfile != NULL);
        dbe_filedes_done(filedes);
        ss_dassert(dbfile->f_blobfiles == NULL);
#ifndef SS_NOLOGGING
        if (dbfile->f_log != NULL) {
            dbe_log_done(dbfile->f_log);
        }
#endif /* SS_NOLOGGING */
        SsMemFree(dbfile);
}

/*##**********************************************************************\
 *
 *		dbe_file_save
 *
 * Saves database file to disk gracefully
 *
 * Parameters :
 *
 *	dbfile - in out, use
 *		pointer to database file object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_file_save(
        dbe_file_t* dbfile)
{
        dbe_filedes_t* filedes;

        ss_dassert(dbfile != NULL);

        filedes = dbfile->f_indexfile;
        dbe_idxfiledes_savestep1(filedes);
        /* Future: savestep for BLOBFILES, too !!!!!!!!! */

        dbe_file_saveheaders(dbfile);

        filedes = dbfile->f_indexfile;
        dbe_idxfiledes_savestep2(filedes);
        /* Future: savestep for BLOBFILES, too !!!!!!!!! */

        DBE_CPMGR_CRASHPOINT(12);

        filedes = dbfile->f_indexfile;
        dbe_cache_concurrent_flushinit(filedes->fd_cache);
        while (dbe_cache_concurrent_flushstep(filedes->fd_cache, ULONG_MAX, DBE_INFO_CHECKPOINT)) {
            ;
        }
        /* Future: flush for BLOBFILES, too !!!!!!!!! */

        dbe_file_saveheaders(dbfile);
}

/*##**********************************************************************\
 *
 *		dbe_file_saveheaders
 *
 * Writes two copies of database headers. Each header is written as a
 * separate pass over all random access files in order to make sure
 * at least one whole set of headers is readable in all those files
 *
 * Parameters :
 *
 *	dbfile - in out, use
 *		pointer to database file object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_file_saveheaders(dbe_file_t* dbfile)
{
        dbe_filedes_t* filedes;

        ss_dprintf_1(("dbe_filedes_saveheaders\n"));

        /* Synchronize the whole file system. Needed
         * because eg. Microsoft SMARTDRV does not flush
         * all lazy write buffers of a file even if the file is
         * closed and then reopened.
         */
        SsFGlobalFlush();

        filedes = dbfile->f_indexfile;
        dbe_filedes_saveheader1(filedes);
        DBE_CPMGR_CRASHPOINT(7);
#ifdef DBE_FILE_BLOBFILES
        if (dbfile->f_blobfiles != NULL) {
            su_pa_do_get(dbfile->f_blobfiles, i, filedes) {
                dbe_filedes_saveheader1(filedes);
            }
        }
#endif /* DBE_FILE_BLOBFILES */
        SsFGlobalFlush();   /* Synchronize the whole file system. */

        filedes = dbfile->f_indexfile;
        dbe_filedes_saveheader2(filedes);
        DBE_CPMGR_CRASHPOINT(8);
#ifdef DBE_FILE_BLOBFILES
        if (dbfile->f_blobfiles != NULL) {
            su_pa_do_get(dbfile->f_blobfiles, i, filedes) {
                dbe_filedes_saveheader2(filedes);
            }
        }
#endif /* DBE_FILE_BLOBFILES */
        SsFGlobalFlush();   /* Synchronize the whole file system. */
}

#ifndef SS_MYSQL
static su_cryptoalg_t dbe_file_getcryptoalg(
        dbe_filedes_t* filedes)
{
        ss_uint1_t key[SU_CRYPTOKEY_SIZE];
        su_cryptoalg_t cipher_alg;

        cipher_alg = dbe_header_getcryptokey(filedes->fd_dbheader, key);
        return cipher_alg;
}
#endif /* !SS_MYSQL */

dbe_ret_t dbe_file_startencryption(
        dbe_file_t* dbfile,
        dbe_cfg_t*  cfg __attribute__ ((unused)))
{
        dbe_filedes_t* filedes = dbfile->f_indexfile;
        su_cryptoalg_t cipher_alg;
        ss_uint1_t key[SU_CRYPTOKEY_SIZE];
        char *password;
        rs_sysi_t* cd = dbe_db_getsyscd(dbfile->f_db);
        dbe_cryptoparams_t* cp = rs_sysi_getcryptopar(cd);
        su_ret_t rc __attribute__ ((unused));
        su_cipher_t* old_cipher __attribute__ ((unused));

        if (cp == NULL) {
            /* Needed for some tests. */
            return DBE_RC_SUCC;
        }

        password = dbe_crypt_getpasswd(cp);
        cipher_alg = dbe_header_getcryptokey(filedes->fd_dbheader, key);

        switch (dbe_crypt_getmode(cp)) {
        case CPAR_NONE:
            if (dbe_crypt_getcipher(cp) != NULL) {
                if (cipher_alg != SU_CRYPTOALG_APP &&
                    cipher_alg != SU_CRYPTOALG_NONE)
                {
                    return DBE_ERR_UNKNOWN_CRYPTOALG;
                }
                memset(key, 0, sizeof(key));
                dbe_header_setcryptokey(filedes->fd_dbheader, SU_CRYPTOALG_APP, key);
            } else if (cipher_alg == SU_CRYPTOALG_DES) {
                if (password == NULL) {
                    return DBE_ERR_NODBPASSWORD;
                }
            } else {
                return DBE_RC_SUCC;
            }
            break;

#ifndef SS_MYSQL

        case CPAR_ENCRYPTED:
            if (cipher_alg != SU_CRYPTOALG_DES) {
                return DBE_ERR_UNKNOWN_CRYPTOALG;
            }
            filedes->fd_cipher = su_cipher_init(cipher_alg, key, password);
            su_svf_setcipher(filedes->fd_svfil, filedes->fd_cipher,
                             svfc_encrypt_dbfile, svfc_decrypt_dbfile);
            break;


        case CPAR_ENCRYPT:
            if (cipher_alg != SU_CRYPTOALG_NONE) {
                return DBE_ERR_NODBPASSWORD;
            }

            su_cipher_generate(key);
            dbe_header_setcryptokey(filedes->fd_dbheader, SU_CRYPTOALG_DES, key);
            filedes->fd_cipher = su_cipher_init(SU_CRYPTOALG_DES, key, password);
            su_svf_setcipher(filedes->fd_svfil, filedes->fd_cipher,
                             svfc_encrypt_dbfile, svfc_decrypt_dbfile);
            rc = su_svf_encryptall(filedes->fd_svfil);
            su_rc_assert(rc == SU_SUCCESS, rc);
            dbe_filedes_saveheader1(filedes);
            dbe_filedes_saveheader2(filedes);
            rc = dbe_logfile_encrypt(cfg, filedes->fd_dbheader,
                                     filedes->fd_cipher, NULL,
                                     svfc_encrypt_page, svfc_decrypt_page);
            su_rc_assert(rc == SU_SUCCESS, rc);
            break;

        case CPAR_DECRYPT:
            if (cipher_alg != SU_CRYPTOALG_DES) {
                return DBE_ERR_UNKNOWN_CRYPTOALG;
            }
            dbe_header_setcryptokey(filedes->fd_dbheader,
                                    SU_CRYPTOALG_NONE, key);
            su_svf_setcipher(filedes->fd_svfil, NULL,
                             svfc_encrypt_dbfile, svfc_decrypt_dbfile);
            old_cipher = su_cipher_init(cipher_alg, key, password);
            memset(key, 0, sizeof(key));
            rc = su_svf_decryptall(filedes->fd_svfil, old_cipher);
            su_rc_assert(rc == SU_SUCCESS, rc);
            rc = dbe_logfile_encrypt(cfg, filedes->fd_dbheader,
                                     NULL, old_cipher,
                                     svfc_encrypt_page, svfc_decrypt_page);
            su_rc_assert(rc == SU_SUCCESS, rc);
            su_cipher_done(old_cipher);
            dbe_filedes_saveheader1(filedes);
            dbe_filedes_saveheader2(filedes);
            dbe_cache_flush(filedes->fd_cache);
            break;

        case CPAR_CHANGEPASSWORD:
            {
                char* old_password = dbe_crypt_getoldpasswd(cp);
                filedes->fd_cipher = su_cipher_init(SU_CRYPTOALG_DES, key, old_password);
                su_svf_setcipher(filedes->fd_svfil, filedes->fd_cipher,
                                 svfc_encrypt_dbfile, svfc_decrypt_dbfile);
                su_cipher_change_password(key, password, old_password);
            }
            dbe_header_setcryptokey(filedes->fd_dbheader, SU_CRYPTOALG_DES, key);
            dbe_filedes_saveheader2(filedes);
            dbe_filedes_saveheader1(filedes);
            break;
#endif /* !SS_MYSQL */

        default:
            ss_derror;
        }

        if (filedes->fd_cipher != NULL) {
            dbe_cache_flush(filedes->fd_cache);
        }
        dbe_crypt_setcipher(cp, filedes->fd_cipher);
        return DBE_RC_SUCC;
}

su_cipher_t* dbe_file_getcipher(
        dbe_file_t* dbfile)
{
        return dbfile->f_indexfile->fd_cipher;
}

/*##**********************************************************************\
 *
 *		dbe_file_restoreheaders
 *
 * Restores database header information from db file.
 *
 * Parameters :
 *
 *	dbfile - in out, use
 *          pointer to database file object
 *
 * Return value :
 *      DBE_RC_SUCC when database closed OK
 *      DBE_WARN_DATABASECRASHED when database was not closed correctly
 *      DBE_WARN_HEADERSINCONSISTENT when all copies of header were not
 *          either not readable or were incompatible in version numbers
 *      DBE_ERR_HEADERSCORRUPT when no consistent set of headers was
 '          readable.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_file_restoreheaders(
        dbe_file_t* dbfile,
        dbe_cfg_t*  cfg)
{
        dbe_filedes_t* filedes;
        bool ok;
        bool pass1_ok;
        bool pass2_ok;
        dbe_hdr_chknum_t chknum1 = 0;
        dbe_hdr_chknum_t chknum2 = 0;
        dbe_header_t* tmp_dbheader = NULL;
        dbe_ret_t rc;
        size_t blocksize;

        rc = DBE_RC_SUCC;
        /* Read 1st copy of headers from indexfile & all BLOB files */
        filedes = dbfile->f_indexfile;

        ok = dbe_header_read(
            filedes->fd_dbheader,
            filedes->fd_cache,
            DBE_HEADER_1_ADDR);
        blocksize = dbe_header_getblocksize(filedes->fd_dbheader);
        if (blocksize == 0) {
            blocksize = filedes->fd_blocksize;
        } else if (ok && blocksize != filedes->fd_blocksize) {
            su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_WRONGBLOCKSIZE_SSD,
                SU_DBE_INDEXSECTION,
                SU_DBE_BLOCKSIZE,
                (long)blocksize
            );
        }

        if (dbfile->f_db != NULL) {
            /* dbfile->f_db is NULL in some tests. */
            rc = dbe_file_startencryption(dbfile, cfg);
            if (rc != DBE_RC_SUCC) {
                ok = FALSE;
                return rc;
            }
        }

        if (ok) {
            chknum1 = dbe_header_getchknum(filedes->fd_dbheader);
#ifdef DBE_FILE_BLOBFILES
            if (dbfile->f_blobfiles != NULL) {
                su_pa_do_get(dbfile->f_blobfiles, i, filedes) {
                    ok = dbe_header_read(
                            filedes->fd_dbheader,
                            filedes->fd_cache,
                            DBE_HEADER_1_ADDR);
                    blocksize = dbe_header_getblocksize(filedes->fd_dbheader);
                    if (blocksize != filedes->fd_blocksize) {
                        su_informative_exit(
                            __FILE__,
                            __LINE__,
                            DBE_ERR_WRONGBLOCKSIZE_SSD,
                            SU_DBE_INDEXSECTION,
                            SU_DBE_BLOCKSIZE,
                            blocksize
                        );

                    }
                    if (!ok) {
                        break;
                    }
                    ok = (dbe_header_getchknum(filedes->fd_dbheader) == chknum1);
                    if (!ok) {
                        break;
                    }
                }
            }
#endif /* DBE_FILE_BLOBFILES */
        }
        pass1_ok = ok;

        /* Read 2nd copy of headers from indexfile & all BLOB files */
        filedes = dbfile->f_indexfile;
        if (pass1_ok) {
            tmp_dbheader = dbe_header_makecopyof(filedes->fd_dbheader);
        } else {
            tmp_dbheader = filedes->fd_dbheader;
        }
        ok = dbe_header_read(
                tmp_dbheader,
                filedes->fd_cache,
                DBE_HEADER_2_ADDR);
        if (ok) {
            chknum2 = dbe_header_getchknum(tmp_dbheader);
        }

        if (filedes->fd_cipher != NULL) {
            ss_uint1_t key1[SU_CRYPTOKEY_SIZE];
            ss_uint1_t key2[SU_CRYPTOKEY_SIZE];
            su_cryptoalg_t alg1;
            su_cryptoalg_t alg2;

            alg1 = dbe_header_getcryptokey(filedes->fd_dbheader, key1);
            alg2 = dbe_header_getcryptokey(tmp_dbheader, key2);
            if (alg2 != SU_CRYPTOALG_NONE
             && memcmp(key1, key2, SU_CRYPTOKEY_SIZE) != 0)
            {
                rc = DBE_WARN_HEADERSINCONSISTENT;
            } else if (alg2 == SU_CRYPTOALG_DES &&
                       !dbe_header_checkkey(filedes->fd_dbheader,
                                     filedes->fd_cache,
                                     DBE_HEADER_1_ADDR))
            {
                ok = FALSE;
                rc = DBE_ERR_WRONGPASSWORD;
            }
        }

        if (tmp_dbheader != filedes->fd_dbheader) {
            dbe_header_done(tmp_dbheader);
            tmp_dbheader = NULL;
        }
#ifdef DBE_FILE_BLOBFILES

        if (ok && dbfile->f_blobfiles != NULL) {
            su_pa_do_get(dbfile->f_blobfiles, i, filedes) {
                if (pass1_ok) {
                    tmp_dbheader =
                        dbe_header_makecopyof(filedes->fd_dbheader);
                } else {
                    tmp_dbheader = filedes->fd_dbheader;
                }
                ok = dbe_header_read(
                        tmp_dbheader,
                        filedes->fd_cache,
                        DBE_HEADER_2_ADDR);
                if (ok) {
                    ok = (dbe_header_getchknum(tmp_dbheader) == chknum2);
                }
                if (tmp_dbheader != filedes->fd_dbheader) {
                    dbe_header_done(tmp_dbheader);
                    tmp_dbheader = NULL;
                }
                if (!ok) {
                    break;
                }
            }
        }
#endif /* DBE_FILE_BLOBFILES */
        pass2_ok = ok;

        /* Check the header read status conditions: return if the
        ** 1st copy of headers is OK, or the situation is hopeless.
        */
        if (pass1_ok) {
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            if (pass2_ok && chknum1 == chknum2) {
                if (dbe_header_getdbstate(dbfile->f_indexfile->fd_dbheader)
                    == DBSTATE_CLOSED)
                {
                    return (DBE_RC_SUCC);
                }
                return (DBE_WARN_DATABASECRASHED);
            }
            if (pass2_ok && chknum1 < chknum2) {
                return (DBE_WARN_HEADERSINCONSISTENT);
            }
            if (!pass2_ok) {
                return (DBE_WARN_HEADERSINCONSISTENT);
            }
        }
        if (!pass2_ok) {    /* Fatal error! */
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            return (DBE_ERR_HEADERSCORRUPT);
        }
        /* if none of the above return conditions was satisfied
           we have to revert to backup (2nd) copy of database header
        */
        filedes = dbfile->f_indexfile;

        tmp_dbheader = dbe_header_makecopyof(filedes->fd_dbheader);
        ok = dbe_header_read(
                tmp_dbheader,
                filedes->fd_cache,
                DBE_HEADER_2_ADDR);
        if (ok) {
            chknum2 = dbe_header_getchknum(tmp_dbheader);
#ifdef DBE_FILE_BLOBFILES
            if (dbfile->f_blobfiles != NULL) {
                su_pa_do_get(dbfile->f_blobfiles, i, filedes) {
                    ok = dbe_header_read(
                            tmp_dbheader,
                            filedes->fd_cache,
                            DBE_HEADER_2_ADDR);
                    if (!ok) {
                        break;
                    }
                    ok = (dbe_header_getchknum(tmp_dbheader) ==
                        chknum2);
                    if (!ok) {
                        break;
                    }
                }
            }
#endif /* DBE_FILE_BLOBFILES */
        }
        dbe_header_done(tmp_dbheader);

        pass2_ok = ok;
        if (!pass2_ok) {
            if (rc != DBE_RC_SUCC) {
                return (rc);
            }
            return (DBE_ERR_HEADERSCORRUPT);
        }
        return (DBE_WARN_HEADERSINCONSISTENT);
}

/*#***********************************************************************\
 *
 *		dbe_filedes_saveheader1
 *
 * Saves 1st copy of db header to one file
 *
 * Parameters :
 *
 *	filedes - in out, use
 *		pointer to file descriptor
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_filedes_saveheader1(filedes)
        dbe_filedes_t* filedes;
{
        bool succp;

        ss_dprintf_3(("dbe_filedes_saveheader1\n"));

        /* incrementing chknum is important for assuring the
        ** version consistence !
        */
        dbe_header_incchknum(filedes->fd_dbheader);
        succp = dbe_header_save(
                    filedes->fd_dbheader,
                    filedes->fd_cache,
                    DBE_HEADER_1_ADDR);
        ss_assert(succp);
}

/*#***********************************************************************\
 *
 *		dbe_filedes_saveheader2
 *
 * Saves 2nd copy of db header to one file
 *
 * Parameters :
 *
 *	filedes - in out, use
 *		pointer to file descriptor
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void dbe_filedes_saveheader2(dbe_filedes_t* filedes)
{
        bool succp;

        ss_dprintf_3(("dbe_filedes_saveheader2\n"));

        succp = dbe_header_save(
                    filedes->fd_dbheader,
                    filedes->fd_cache,
                    DBE_HEADER_2_ADDR);
        ss_assert(succp);
}

/*##**********************************************************************\
 *
 *		dbe_file_getfiledes
 *
 * Gets a filedes with a gine file number (needed for finding correct
 * blob file)
 *
 * Parameters :
 *
 *	dbfile - in, use
 *		pointer to db file
 *
 * Return value - ref :
 *      pointer to filedes
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_filedes_t* dbe_file_getfiledes(
        dbe_file_t* dbfile)
{
        return (dbfile->f_indexfile);
}

/*##**********************************************************************\
 *
 *		dbe_file_getdiskno
 *
 * Gets physical disk number for file # and disk address
 *
 * Parameters :
 *
 *	dbfile - in, use
 *		database file object
 *
 *	daddr - in
 *		disk block address in file
 *
 * Return value :
 *      dense number from zero upward describing the disk device
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
uint dbe_file_getdiskno(
        dbe_file_t* dbfile,
        su_daddr_t daddr)
{
        dbe_filedes_t* filedes;
        int diskno;

        filedes = dbfile->f_indexfile;

        diskno = (uint)su_svf_getdiskno(filedes->fd_svfil, daddr);
        ss_dassert(diskno != -1);
        return (diskno);
}


/*##**********************************************************************\
 *
 *		dbe_file_removelastfilespec
 *
 * Removes last physical file from database file configuration. Only empty
 * files can be removed.
 *
 * Parameters :
 *
 *	dbfile -
 *
 * Return value :
 *
 *    SU_SUCCESS - last physical file removed
 *    DBE_ERR_CANNOTREMOVEFILESPEC - last physical file not empty
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t dbe_file_removelastfilespec(su_inifile_t* dbinifile, dbe_file_t* dbfile)
{
        dbe_filespec_t* filespec;
        su_pa_t* filespec_array;
        dbe_filedes_t* filedes;
        su_svfil_t* svfil;
        dbe_cfg_t* cfg;
        su_ret_t rc;
        char* key;
        bool b;
        uint i;

        filedes = dbfile->f_indexfile;
        ss_dassert(filedes != NULL);

        svfil = filedes->fd_svfil;
        ss_dassert(svfil != NULL);

        rc = su_svf_removelastfile(svfil);
        if (rc != SU_SUCCESS) {
            return DBE_ERR_CANNOTREMOVEFILESPEC;
        }
        filespec_array = su_pa_init();
        cfg = dbe_cfg_init(dbinifile);
        b = dbe_cfg_getidxfilespecs(cfg, filespec_array);
        ss_dassert(b == TRUE);
        i = su_pa_nelems(filespec_array);
        key = SsMemAlloc(strlen(SU_DBE_FILESPEC) + 16);
        SsSprintf(key, SU_DBE_FILESPEC, i);
        b = su_param_remove((char *)SU_DBE_INDEXSECTION, key);

        dbe_cfg_done(cfg);
        su_pa_do_get(filespec_array, i, filespec) {
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);
        SsMemFree(key);

        if (b) {
            return SU_SUCCESS;
        } else {
            return DBE_ERR_CANNOTREMOVEFILESPEC;
        }
}

/*##**********************************************************************\
 *
 *		dbe_file_addnewfilespec
 *
 *  Add new physical file to database file.
 *
 *
 * Parameters :
 *
 *	dbfile -
 *
 *
 *	filespecs - in, use
 *		current filespecs
 *
 *	filename - in, use
 *          name of the new physical file.
 *
 *	maxsize - in, use
 *          maximum size of the new physical file
 *
 *	diskno - in, use
 *	    device number of the new physical file
 *
 * Return value :
 *
 *      SU_SUCCESS
 *      SU_ERR_FILE_OPEN_FAILURE
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_ret_t dbe_file_addnewfilespec(
        su_inifile_t* dbinifile,
        dbe_file_t* dbfile,
        char* filename,
        ss_int8_t maxsize,
        uint diskno)
{
        dbe_filespec_t* filespec;
        su_pa_t* filespec_array;
        disknoarr_t* disknoarr;
        su_svfil_t* svfil;
        su_err_t* err = NULL;
        dbe_cfg_t* cfg;
        su_ret_t rc;
        uint i;
        char* newfspec;
        char* newkey;
        bool b, succ, first = FALSE;
        bool changed = FALSE;
        char maxsize_str[21];


        /*
         * Check if physical filename already in use
         */
        svfil = dbfile->f_indexfile->fd_svfil;

        rc = su_svf_filenameinuse(svfil, filename);
        if (rc != SU_SUCCESS) {
            return rc;
        }
        filespec_array = su_pa_init();
        cfg = dbe_cfg_init(dbinifile);
        b = dbe_cfg_getidxfilespecs(cfg, filespec_array);
        if (b) {
            /*
             * Add new filespec to ini-file
             */
            i = su_pa_nelems(filespec_array);
            SsInt8ToAscii(maxsize, maxsize_str, 10, 0, ' ', FALSE);
            newkey = SsMemAlloc(strlen(SU_DBE_FILESPEC) + 16);
            SsSprintf(newkey, SU_DBE_FILESPEC, i + 1);
            newfspec = SsMemAlloc(strlen(filename) + 64);
            if (diskno == 0) {
                SsSprintf(newfspec, "%s %s", filename, maxsize_str);
            } else {
                SsSprintf(newfspec, "%s %s %u", filename, maxsize_str, diskno);
            }
            ss_dprintf_1(("dbe_file_addnewfilespec: filespec_array size = %d\n",
                          i));
            succ = su_param_register(
                        SU_DBE_INDEXSECTION,
                        newkey,
                        NULL,
                        NULL,
                        newfspec,
                        "Filespec_n describes the location and the maximum size of the database file",
                        NULL,
                        NULL,
                        SU_PARAM_TYPE_STR,
                        SU_PARAM_AM_RW
            );
            ss_dassert(succ);

            rc = su_param_set_values(
                        SU_DBE_INDEXSECTION,
                        newkey,
                        newfspec,
                        FALSE,
                        FALSE,
                        FALSE,
                        &err,
                        &changed);

            su_param_switchtoreadonly((char *)SU_DBE_INDEXSECTION, newkey);
        } else {
            first = TRUE;
            newkey = SsMemAlloc(strlen(SU_DBE_FILESPEC) + 16);

            /*
             * Add default filespec to ini-file
             */
            SsSprintf(newkey, SU_DBE_FILESPEC, 1);
            newfspec = SsMemAlloc(strlen("solid.db") + 64);
            SsSprintf(newfspec, "%s %ld", "solid.db", (long)SU_VFIL_SIZE_MAX);
            ss_dprintf_1(("dbe_file_addnewfilespec: No old filespecs found\n"));

            succ = su_param_register(
                        SU_DBE_INDEXSECTION,
                        "FileSpec_1",
                        NULL,
                        NULL,
                        newfspec,
                        "Filespec_n describes the location and the maximum size of the database file",
                        NULL,
                        NULL,
                        SU_PARAM_TYPE_STR,
                        SU_PARAM_AM_RW
            );
            ss_dassert(succ);
            rc = su_param_set_values(
                        SU_DBE_INDEXSECTION,
                        "FileSpec_1",
                        newfspec,
                        FALSE,
                        FALSE,
                        FALSE,
                        &err,
                        &changed);

            su_param_switchtoreadonly((char *)SU_DBE_INDEXSECTION, (char *)"FileSpec_1");

            SsMemFree(newfspec);

            /*
             * Add new filespec
             */
            SsSprintf(newkey, SU_DBE_FILESPEC, 2);
            newfspec = SsMemAlloc(strlen(filename) + 64);
            SsInt8ToAscii(maxsize, maxsize_str, 10, 0, ' ', FALSE);

            if (diskno == 0) {
                SsSprintf(newfspec, "%s %s", filename, maxsize_str);
            } else {
                SsSprintf(newfspec, "%s %s %u", filename, maxsize_str, diskno);
            }
            succ = su_param_register(
                        SU_DBE_INDEXSECTION,
                        newkey,
                        NULL,
                        NULL,
                        newfspec,
                        "Filespec_n describes the location and the maximum size of the database file",
                        NULL,
                        NULL,
                        SU_PARAM_TYPE_STR,
                        SU_PARAM_AM_RW
            );
            ss_dassert(succ);

            rc = su_param_set_values(
                        SU_DBE_INDEXSECTION,
                        newkey,
                        newfspec,
                        FALSE,
                        FALSE,
                        FALSE,
                        &err,
                        &changed);

            su_param_switchtoreadonly((char *)SU_DBE_INDEXSECTION, newkey);
        }
        su_pa_do_get(filespec_array, i, filespec) {
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);

        /*
         * Refresh disknoarr
         */
        filespec_array = su_pa_init();
        b = dbe_cfg_getidxfilespecs(cfg, filespec_array);
        ss_dassert(b == TRUE);
        disknoarr = disknoarr_init();
        su_pa_do_get(filespec_array, i, filespec) {
            disknoarr = disknoarr_add(disknoarr, dbe_filespec_getdiskno(filespec));
            dbe_filespec_done(filespec);
        }
        su_pa_done(filespec_array);
        dbe_cfg_done(cfg);

        /*
         * Add new file to svfil
         */
        svfil = dbfile->f_indexfile->fd_svfil;

        rc = su_svf_addfile2(
                svfil,
                filename,
                maxsize,
                FALSE,
                disknoarr_densify(disknoarr, diskno)
        );
        if (rc != SU_SUCCESS) {
            b = su_param_remove((char *)SU_DBE_INDEXSECTION, newkey);
            ss_dassert(b);
            if (first) {
                b = su_param_remove((char *)SU_DBE_INDEXSECTION, (char *)"FileSpec_1");
                ss_dassert(b);
            }
        }
        disknoarr_done(disknoarr);
        SsMemFree(newkey);
        SsMemFree(newfspec);
        return rc;
}

/*##**********************************************************************\
 *
 *		dbe_file_fileusageinfo
 *
 * Get usage info of database file and single physical file.
 *
 *
 * Parameters :
 *
 *	dbfile -
 *
 *	maxsize - out
 *          maximun size of the database (in megabytes)
 *
 *	currsize - out
 *          current size of the database (in megabytes)
 *
 *	totalperc - out
 *          percentage used (whole database)
 *
 *	nth - in
 *          give percentage info also from nth physical file
 *
 *	perc - out
 *          percentage used (Nth physical file)
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
void dbe_file_fileusageinfo(dbe_file_t* dbfile, double* maxsize, double* currsize, float* totalperc, uint nth, float* perc)
{
        dbe_filedes_t* filedes;
        su_svfil_t* svfil;

        filedes = dbfile->f_indexfile;
        svfil = filedes->fd_svfil;

        su_svf_fileusageinfo(svfil, maxsize, currsize, totalperc, nth, perc);
}

ss_int8_t dbe_fildes_getnbyteswritten(
        dbe_filedes_t*  fildes)
{
        return su_svf_getnbyteswritten(fildes->fd_svfil);
}

void dbe_fildes_zeronbyteswritten(
        dbe_filedes_t*  fildes)
{
        su_svf_zeronbyteswritten(fildes->fd_svfil);
}
