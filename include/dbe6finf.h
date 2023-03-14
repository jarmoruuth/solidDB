/*************************************************************************\
**  source       * dbe0finf.h
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


#ifndef DBE6FINF_H
#define DBE6FINF_H

#include <ssc.h>
#include <ssstdlib.h>
#include <ssint8.h>
#include <su0parr.h>
#include <su0types.h>

#include "dbe6log.h"
#include "dbe8flst.h"
#include "dbe8cach.h"
#include "dbe8clst.h"
#include "dbe8cpls.h"
#include "dbe7cfg.h"
#include "dbe7hdr.h"
#include "dbe6cpm.h"
#include "dbe0type.h"

typedef enum {
        DBE_FNUM_LOGFILE = -1,
        DBE_FNUM_INDEXFILE = 0
} dbe_fnum_t;

/* Structure containing various information that is related to the
   file storage and caching of the index tree.
*/
struct dbe_filedes_st {
        dbe_cache_t*    fd_cache;       /* File cache. */
        dbe_freelist_t* fd_freelist;    /* List of free disk blocks. */
        su_svfil_t*     fd_svfil;       /* Split virtual file. */
        size_t          fd_blocksize;   /* File block size in bytes */
        bool            fd_created;     /* TRUE if created new file when
                                          database was last opened */
        dbe_chlist_t*   fd_chlist;      /* Change list from prev. checkpoint */
        dbe_cplist_t*   fd_cplist;      /* live checkpoint/snapshot list */
        dbe_header_t*   fd_dbheader;    /* database file header */
        dbe_header_t*   fd_olddbheader;
        dbe_cprec_t*    fd_cprec;       /* checkpoint record */
        int             fd_fnum;        /* see dbe_fnum_t above */
        su_cipher_t*    fd_cipher;      /* database cipher */
};

struct dbe_file_st {
        dbe_filedes_t*      f_indexfile;
        su_pa_t*            f_blobfiles;
        dbe_log_t*          f_log;
        dbe_db_t*           f_db;
};

bool dbe_file_exist(
        dbe_cfg_t* cfg);

bool dbe_file_existall(
        dbe_cfg_t* cfg);

dbe_file_t* dbe_file_init(
        dbe_cfg_t* cfg,
        dbe_db_t* db);

void dbe_file_start(
        dbe_file_t* dbfile,
        dbe_cfg_t* cfg,
        bool crashed);

void dbe_file_done(
        dbe_file_t* dbfile);

void dbe_file_save(
        dbe_file_t* dbfile);

void dbe_file_saveheaders(
        dbe_file_t* dbfile);

dbe_ret_t dbe_file_restoreheaders(
        dbe_file_t* dbfile,
        dbe_cfg_t*  cfg);

dbe_filedes_t* dbe_file_getfiledes(
        dbe_file_t* dbfile);

uint dbe_file_getdiskno(
        dbe_file_t* dbfile,
        su_daddr_t daddr);


su_ret_t dbe_file_removelastfilespec(
        su_inifile_t* dbinifile,
        dbe_file_t* dbfile);

su_ret_t dbe_file_addnewfilespec(
        su_inifile_t* dbinifile,
        dbe_file_t* dbfile,
        char* filename,
        ss_int8_t maxsize,
        uint diskno);

void dbe_file_fileusageinfo(
        dbe_file_t* dbfile,
        double* maxsize,
        double* currsize,
        float* totalperc,
        uint nth,
        float* perc);

dbe_cache_t *dbe_cache_cfg_init(
	dbe_cfg_t *cfg, 
	su_svfil_t *svfil, 
	uint blocksize);

ss_int8_t dbe_fildes_getnbyteswritten(
        dbe_filedes_t*  fildes);

void dbe_fildes_zeronbyteswritten(
        dbe_filedes_t*  fildes);

dbe_ret_t dbe_file_startencryption(
        dbe_file_t* dbfile,
        dbe_cfg_t*  cfg);

su_cipher_t* dbe_file_getcipher(
        dbe_file_t* dbfile);

#endif /* DBE6FINF_H */
