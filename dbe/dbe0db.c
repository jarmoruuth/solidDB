/*************************************************************************\
**  source       * dbe0db.c
**  directory    * dbe
**  description  * Database interface.
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

This module implements the database object. The database object contains
global database resources like the index tree, relation buffer, log file,
big objects file and global variables and counters. When the database
object is created, a new database is created or an existing database
is opened.

The database is opened or created by creating the database object. When
the database is created, necessary files are created and the system
relations are created and inserted to the database. When an existing
database is opened, system variables are read from the database. When
the database is closed system variables are writtem back to the database.

Limitations:
-----------


Error handling:
--------------


Objects used:
------------

relation buffer                         rs0rbuf.c

index system (for opening the database) dbe5inde.c
system relation initialization          dbe4srli.c

Preconditions:
-------------


Multithread considerations:
--------------------------


Example:
-------

void main(void)
{
        dbe_db_t* db;
        dbe_openinfo_t* oi;
        dbe_user_t* user;
        dbe_trx_t* trx;
        rs_cd_t* cd;

        /* init openinfo */
        oi = ...;

        /* init cd */
        cd = ...;

        /* open the database */
        db = dbe_db_init(...);

        /* create user */
        user = dbe_user_init(db, cd, "username", "password");

        /* begin transaction */
        trx = dbe_trx_begin(user);

        /* do database operations */
        ...

        /* commit transaction */
        dbe_trx_commit(trx);

        /* kill the user */
        dbe_user_done(user);

        /* close the database */
        dbe_db_done(db);
}

**************************************************************************
#endif /* DOCUMENTATION */

#define DBE0DB_C

#include <ssstddef.h>

#include <ssc.h>

#ifdef SS_MYSQL_PERFCOUNT
#include <sswindow.h>
#endif

#include <math.h>

#include <ssc.h>
#include <ssdebug.h>
#include <ssmem.h>
#include <sssem.h>
#include <ssservic.h>
#include <sssprint.h>
#include <sstime.h>
#include <ssthread.h>
#include <ssfile.h>
#include <ssscan.h>
#include <sschcvt.h>
#include <sspmon.h>
#include <ssfnsplt.h>

#ifdef SS_FFMEM
#include <ssffmem.h>
#endif /* SS_FFMEM */

#include <ssqmem.h>

#include <su0gate.h>
#include <su0inifi.h>
#include <su0vfil.h>
#include <su0vers.h>
#include <su0err.h>
#include <su0icvt.h>
#include <su0cfgst.h>
#include <su0li3.h>
#include <su0param.h>
#include <su0prof.h>

#include <uti0va.h>

#include <ui0msg.h>

#include <rs0sdefs.h>
#include <rs0rbuf.h>
#include <rs0ttype.h>
#include <rs0tval.h>
#include <rs0atype.h>
#include <rs0aval.h>

#include "dbe0db.h"
#include "dbe9type.h"
#include "dbe9bhdr.h"
#include "dbe8trxl.h"
#include "dbe8seql.h"
#include "dbe8srec.h"
#include "dbe8flst.h"
#include "dbe7hdr.h"
#include "dbe7cfg.h"
#include "dbe7ctr.h"
#include "dbe7gtrs.h"
#include "dbe6gobj.h"
#include "dbe6lmgr.h"
#include "dbe6bmgr.h"
#include "dbe6btre.h"
#include "dbe6bkey.h"
#include "dbe6bnod.h"
#include "dbe6blob.h"
#include "dbe5inde.h"
#include "dbe5imrg.h"
#include "dbe4srli.h"
#include "dbe4rfwd.h"
#include "dbe4tupl.h"
#include "dbe2back.h"
#include "dbe1seq.h"
#include "dbe0seq.h"
#include "dbe0type.h"
#include "dbe0erro.h"
#include "dbe0user.h"
#include "dbe0blobg2.h"
#include "dbe0brb.h"
#include "dbe7logf.h"
#include "dbe0hsbg2.h"
#include "dbe0crypt.h"

#ifdef SS_MME
#include "dbe4mme.h"
#endif

bool                sqlsrv_isbackupservermode = FALSE;   /* Start the server without solid.db */


/* Alpha is the number of last nsearch values taken to a average value of
   active searches. The average value of active searches is used to calculate
   the amount of buffer space for one query.
*/

#define DB_ALPHA            100.0
#define DB_ONE_PER_ALPHA    (1.0 / DB_ALPHA)

/* Initial value of average active searches.
*/

#define DB_INIT_AVGNSEARCH  3

/* Minimum time interval between consecutive measurements of log size */
#define DB_MIN_LOGSIZE_MEAS_INTERVAL_IN_SECS    (60L)
/* Logsize (in kilo bytes) after which log file is split by generating a check point */
#define DB_LOGSPLITSIZE     1500000

#define DB_FORCEMERGEINTERVAL   (200 * 1024 * 1024)
#define DB_MERGEFIXEDRATE       1

extern bool dbefile_diskless;
extern long backup_blocksize;
#ifdef DBE_BNODE_MISMATCHARRAY /* This needs more testimg. */
extern bool dbe_bnode_usemismatcharray;
#endif

bool dbexist_diskless = FALSE;  /* for diskless, no physical dbfile */

static void (*dbe_server_sysi_init_functions_fp)(rs_sysi_t* sysi);

#if defined(SS_FFMEM) && !defined(SS_PURIFY)

#define DB_DISKLESSMMEPAGEALLOC(p_ctx, size)                                 \
        ((*(p_ctx) == NULL ?                                                 \
          (*(p_ctx) = SsFFmemCtxInitUseMutexIfNPools(TRUE, SS_FFMEM_NPOOLS)) \
          : NULL),                                                           \
         SsFFmemAllocCtxFor(*(p_ctx),SS_FFMEM_PAGES, (size)))

#define DB_DISKLESSMMEPAGEFREE(ctx, p) \
        SsFFmemFreeCtxFor((ctx), SS_FFMEM_PAGES, (p))

#else /* SS_FFMEM && !SS_PURIFY */

#define DB_DISKLESSMMEPAGEALLOC(p_ctx, size) SsMemAlloc(size)
#define DB_DISKLESSMMEPAGEFREE(ctx, p) SsMemFree(p)

#endif /* SS_FFMEM && !SS_PURIFY */

#ifndef SS_MYSQL
static su_list_t* db_disklessmmepages = NULL;
#endif

static void* db_disklessmmememctx = NULL;

static dbe_ret_t db_createcp(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_blocktype_t type,
        bool displayprogress,
        bool splitlog);

static void db_restorelatestcp(
        dbe_db_t* db);

static void db_mergestart_nomutex(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool idlemerge,
        bool force_full_merge,
        bool usermerge);

static void db_mergestop_nomutex(
        dbe_db_t* db);

static void db_quickmergestop_nomutex(
        dbe_db_t* db);

extern int dbe_debug;
bool dbe_ignorecrashed = FALSE;
bool dbe_nologrecovery = FALSE;
dbe_db_openstate_t dbe_db_openstate = DBE_DB_OPEN_NONE; /* Open state, updated from tab0conn.c */

ss_debug(bool dbe_db_hsbg2enabled;)

/* store database object so that callbacks can access it */
dbe_db_t* h_db = NULL;

/*#***********************************************************************\
 *
 *              db_getinfo
 *
 * Gets existing info from index file header
 *
 * Parameters :
 *
 *      db - in out, use
 *          Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void db_getinfo(dbe_db_t* db)
{
        dbe_header_t* dbheader;
        dbe_startrec_t* sr;

        CHK_DB(db);

        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        sr = dbe_header_getstartrec(dbheader);
        dbe_counter_getinfofromstartrec(db->db_go->go_ctr, sr);
        dbe_type_updateconst(db->db_go->go_ctr);
}

/*#**********************************************************************\
 *
 *              db_updateinfo
 *
 * Writes certain info to database header
 *
 * Input params :
 *
 *      db - in out, use
 *          Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void db_updateinfo(dbe_db_t* db)
{
        dbe_header_t* dbheader;
        dbe_startrec_t* sr;

        CHK_DB(db);

        if (db->db_dbfile != NULL) {
            dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
            sr = dbe_header_getstartrec(dbheader);
            dbe_counter_putinfotostartrec(db->db_go->go_ctr, sr);
        }
}

/*#***********************************************************************\
 *
 *              db_checkheaderflags
 *
 * Checks database header flags.
 *
 * Parameters :
 *
 *      dbheader - in
 *          db header object
 *
 * Return value :
 *
 * Comments :
 *      Stops the program when failure to support the database
 *      format occurs.
 *
 * Globals used :
 *
 * See also :
 */
static void db_checkheaderflags(
        dbe_header_t* dbheader)
{
        ss_uint4_t flags;

        flags = dbe_header_getheaderflags(dbheader);
#ifdef SS_MYSQL
        if ((flags & HEADER_FLAG_MYSQL) == 0) {
            su_informative_exit(__FILE__, __LINE__, DBE_ERR_NOTMYSQLDATABASEFILE);
        }
#endif
#ifdef DBE_BNODE_MISMATCHARRAY
        dbe_bnode_usemismatcharray = TRUE;
        ss_pprintf_1(("db_checkheaderflags:dbe_bnode_usemismatcharray=%d\n", dbe_bnode_usemismatcharray));
#else
        ss_dassert(!(flags & HEADER_FLAG_BNODE_MISMATCHARRAY));
#endif
}

/*#***********************************************************************\
 *
 *              db_checkversion
 *
 * Checks that the database header & file format versions match with
 * the current server version (backward compatibility is preserved
 * whenever possible. So, the server may be newer version than the
 * database)
 *
 * Parameters :
 *
 *      dbheader - in, use
 *          db header object
 *
 *      p_migratetounicode - out
 *          need migration to unicode
 *
 *      p_migratetocatalogsupp - out
 *          need migration to catalog support
 *
 *      p_migratetonlobg2 - out
 *          need migration to BLOB G2 support
 *
 *      p_migrategeneric - out
 *          need some migration. This may also be technically
 *          trivial in which case only this needs to be set
 *
 * Return value :
 *
 * Comments :
 *      Stops the program when failure to support the database
 *      format occurs.
 *
 * Globals used :
 *
 * See also :
 */
static void db_checkversion(
        dbe_header_t* dbheader,
        bool* p_migratetounicode,
        bool* p_migratetocatalogsupp,
        bool* p_migratetoblobg2,
        bool* p_migrategeneric)
{
        ss_uint2_t headervers;
        ss_uint2_t dbvers;
        headervers = dbe_header_getheadervers(dbheader);
        dbvers = dbe_header_getdbvers(dbheader);
        if (headervers > SU_DBHEADER_VERSNUM) {
            su_informative_exit(__FILE__, __LINE__,
                    DBE_ERR_WRONGHEADERVERS_D,
                    (int)headervers);
        }
        if (dbvers > SU_DBFILE_VERSNUM) {
            su_informative_exit(__FILE__, __LINE__,
                    DBE_ERR_DATABASEFILEFORMAT_D,
                    (int)dbvers);
        }

        *p_migratetounicode = FALSE;
        *p_migratetocatalogsupp = FALSE;
#ifdef SS_UNICODE_SQL
        if (dbvers < SU_DBFILE_VERSNUM_UNICODE_START) {
            *p_migratetounicode = TRUE;
        }
#endif /* SS_UNICODE_SQL */
        if (dbvers < SU_DBFILE_VERSNUM_CATALOGSUPP_START) {
            *p_migratetocatalogsupp = TRUE;
        } else {
            char* defcatalog = dbe_header_getdefcatalog(dbheader);
            if (defcatalog[0] == '\0') {
                *p_migratetocatalogsupp = TRUE;
            }
        }
        *p_migratetoblobg2 = (dbvers < SU_DBFILE_VERSNUM_BLOBG2_START);
        *p_migrategeneric = (dbvers < SU_DBFILE_VERSNUM);
}

/*##**********************************************************************\
 *
 *              dbe_db_check_overwrite
 *
 * Checks whether pathname could overwrite any of initialization,
 * index or log files.
 *
 * The check is done only on the base file names (including extension),
 * directories are ignored.
 *
 * Parameters :
 *
 *      db - use
 *              Database object
 *
 *  test_path - use
 *      Path to check
 *
 * Return value :
 *
 *      TRUE    - file is ok
 *      FALSE   - file could overwrite inifile, index or log file
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */

bool dbe_db_check_overwrite(dbe_db_t* db, char* test_path)
{
        su_pa_t *fn_idx;
        uint i;
        char *s, *t;
        dbe_filespec_t* fs;
        bool rv = TRUE;
        char test_file[255], test_dir[255], file[255], dir[255];

        CHK_DB(db);
        ss_dassert(test_path != NULL);

        SsFnSplitPath(test_path, test_dir, sizeof(test_dir) - 1, test_file,
          sizeof(test_file) - 1);

        /* check if the initialization file matches */

        SsFnSplitPath(SU_SOLINI_FILENAME, dir, sizeof(dir) - 1, file,
          sizeof(file) - 1);

        if(SsStrcmp(test_file, file) == 0) {
            return (FALSE);
        }

        /* check if the log file name matches */

        dbe_cfg_getlogfilenametemplate(db->db_go->go_cfg, &s);

        SsFnSplitPath(s, dir, sizeof(dir) - 1, file, sizeof(file) - 1);

        SsMemFree(s);

        s = file;
        t = test_file;

        while(*s && *t && ((*s == '#' && ss_isdigit(*t)) || (*s == *t))) {
            s++;
            t++;
        }
        if(*s == *t) {
            return (FALSE);
        }

        /* check if any of the index file names match */

        fn_idx = su_pa_init(); /* needs locking? */
        dbe_cfg_getidxfilespecs(db->db_go->go_cfg, fn_idx);

        /* loop through and free all index filespecs */
        su_pa_do_get(fn_idx, i, fs) {

            SsFnSplitPath(dbe_filespec_getname(fs),
              dir, sizeof(dir) - 1, file, sizeof(file) - 1);

            if(SsStrcmp(test_file, file) == 0) {
                rv = FALSE;
            }

            dbe_filespec_done(fs);
        }
        su_pa_done(fn_idx);

        return (rv);
}

/*##**********************************************************************\
 *
 *              dbe_db_dbexist
 *
 * Checks if the database file exist.
 *
 * Parameters :
 *
 *      inifile - in
 *              Ini file.
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
bool dbe_db_dbexist(
        su_inifile_t* inifile)
{
        bool exists;
        dbe_cfg_t* cfg;

        ss_dassert(inifile != NULL);

        if (dbefile_diskless) {

            cfg = dbe_cfg_init(inifile);
            dbe_cfg_register_su_params(cfg);
            dbe_cfg_done(cfg);

            if (dbexist_diskless) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
        cfg = dbe_cfg_init(inifile);
        su_param_manager_global_init(inifile);
        dbe_cfg_register_su_params(cfg);
        exists = dbe_file_exist(cfg);

        dbe_cfg_done(cfg);
        su_param_manager_global_done();

        return(exists);
}

/*##**********************************************************************\
 *
 *              dbe_db_dbexistall
 *
 * Checks if the all the database files exist i.e there are no holes in the FileSpec definitions
 *
 * Parameters :
 *
 *      inifile - in
 *              Ini file.
 *
 * Return value :
 *
 *      TRUE    - database files exist
 *      FALSE   - at least one database file does not exist
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_dbexistall(
        su_inifile_t* inifile)
{
        bool exists;
        dbe_cfg_t* cfg;

        ss_dassert(inifile != NULL);

        if (dbefile_diskless) {

            cfg = dbe_cfg_init(inifile);
            dbe_cfg_register_su_params(cfg);
            dbe_cfg_done(cfg);

            if (dbexist_diskless) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
        cfg = dbe_cfg_init(inifile);
        dbe_cfg_register_su_params(cfg);

        exists = dbe_file_existall(cfg);

        dbe_cfg_done(cfg);

        return(exists);
}

/*##**********************************************************************\
 *
 *              dbe_db_iscfgreadonly
 *
 * Checks if the database file is configured (solid.ini) to be read-only
 *
 * Parameters :
 *
 *      inifile -
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
bool dbe_db_iscfgreadonly(
        su_inifile_t* inifile)
{
        bool read_only;
        bool found;

        ss_dassert(inifile != NULL);

        found = su_inifile_getbool(
                    inifile,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_READONLY,
                    &read_only);
        if (!found) {
            read_only = FALSE;
        }

        return(read_only);
}


/*##**********************************************************************\
 *
 *              db_removetrxlists
 *
 * Removes transaction lists from disks. This is now
 * done in a delayed fashion, because all writes to database
 * files are now postponed to the first update moment.
 *
 * Parameters :
 *
 *      db -
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
void db_removetrxlists(dbe_db_t* db)
{
        su_ret_t rc;
        su_daddr_t trxlistaddr;
        su_daddr_t stmttrxlistaddr;
        su_daddr_t sequencelistaddr;
#ifdef DBE_REPLICATION
        su_daddr_t rtrxlistaddr;
#endif /* DBE_REPLICATION */
        dbe_cpnum_t cpnum;
        dbe_header_t* dbheader;

        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);
#ifdef DBE_REPLICATION
        rtrxlistaddr = dbe_header_getrtrxlistaddr(dbheader);
        ss_dprintf_2(("db_removetrxlists:delete rtrxlist, addr=%ld\n", rtrxlistaddr));
        rc = dbe_trxl_deletefromdisk(
                rtrxlistaddr,
                db->db_dbfile->f_indexfile->fd_cache,
                db->db_dbfile->f_indexfile->fd_freelist,
                db->db_dbfile->f_indexfile->fd_chlist,
                cpnum);
        su_rc_assert(rc == SU_SUCCESS, rc);
        dbe_header_setrtrxlistaddr(dbheader, SU_DADDR_NULL);
#endif /* DBE_REPLICATION */
        trxlistaddr = dbe_header_gettrxlistaddr(dbheader);
        ss_dprintf_2(("db_removetrxlists:delete trxlist, addr=%ld\n", trxlistaddr));
        rc = dbe_trxl_deletefromdisk(
                trxlistaddr,
                db->db_dbfile->f_indexfile->fd_cache,
                db->db_dbfile->f_indexfile->fd_freelist,
                db->db_dbfile->f_indexfile->fd_chlist,
                cpnum);
        su_rc_assert(rc == SU_SUCCESS, rc);
        stmttrxlistaddr = dbe_header_getstmttrxlistaddr(dbheader);
        ss_dprintf_2(("db_removetrxlists:delete stmttrxlist, addr=%ld\n", stmttrxlistaddr));
        rc = dbe_trxl_deletefromdisk(
                stmttrxlistaddr,
                db->db_dbfile->f_indexfile->fd_cache,
                db->db_dbfile->f_indexfile->fd_freelist,
                db->db_dbfile->f_indexfile->fd_chlist,
                cpnum);
        su_rc_assert(rc == SU_SUCCESS, rc);
#ifndef SS_NOSEQUENCE
        sequencelistaddr = dbe_header_getsequencelistaddr(dbheader);
        ss_dprintf_2(("db_removetrxlists:delete sequencelist, addr=%ld\n", sequencelistaddr));
        rc = dbe_seql_deletefromdisk(
                sequencelistaddr,
                db->db_dbfile->f_indexfile->fd_cache,
                db->db_dbfile->f_indexfile->fd_freelist,
                db->db_dbfile->f_indexfile->fd_chlist,
                cpnum);
        su_rc_assert(rc == SU_SUCCESS, rc);
#endif /* SS_NOSEQUENCE */
        dbe_header_settrxlistaddr(dbheader, SU_DADDR_NULL);
        dbe_header_setstmttrxlistaddr(dbheader, SU_DADDR_NULL);
#ifndef SS_NOSEQUENCE
        dbe_header_setsequencelistaddr(dbheader, SU_DADDR_NULL);
#endif /* SS_NOSEQUENCE */
}

/*#***********************************************************************\
 *
 *              db_hsbmodechangereset
 *
 * Disable locks in Hot Standby secondary server.
 *
 * Parameters :
 *
 *      db -
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
static void db_hsbmodechangereset(dbe_db_t* db)
{
        CHK_DB(db);

        dbe_lockmgr_setuselocks(
            db->db_lockmgr,
            db->db_hsbmode != DBE_HSB_SECONDARY);
        dbe_index_hsbsetbloblimit_high(
            db->db_index,
            db->db_hsbmode == DBE_HSB_PRIMARY);
}

static void db_initforcemergevalues(dbe_db_t* db)
{
        ulong id;
        ulong num;
        dbe_trxid_t nextmergetrxid;
        dbe_trxnum_t nextmergetrxnum;

        db->db_forcemergetrxid = dbe_counter_gettrxid(db->db_go->go_ctr);
        id = dbe_trxid_getlong(db->db_forcemergetrxid);
        id = id / DB_FORCEMERGEINTERVAL;
        id = (id + 1) * DB_FORCEMERGEINTERVAL;
        nextmergetrxid = dbe_trxid_init(id);
        ss_dassert(dbe_trxid_cmp(nextmergetrxid, db->db_forcemergetrxid) > 0);
        db->db_forcemergetrxid = nextmergetrxid;
        ss_dprintf_3(("db_initforcemergevalues:dbe_counter_gettrxid=%ld, db->db_forcemergetrxid=%ld\n",
            DBE_TRXID_GETLONG(dbe_counter_gettrxid(db->db_go->go_ctr)),
            DBE_TRXID_GETLONG(db->db_forcemergetrxid)));

        db->db_forcemergetrxnum = dbe_counter_getcommittrxnum(db->db_go->go_ctr);
        num = dbe_trxnum_getlong(db->db_forcemergetrxnum);
        num = num / DB_FORCEMERGEINTERVAL;
        num = (num + 1) * DB_FORCEMERGEINTERVAL;
        nextmergetrxnum = dbe_trxnum_init(num);
        ss_dassert(dbe_trxnum_cmp(nextmergetrxnum, db->db_forcemergetrxnum) > 0);
        db->db_forcemergetrxnum = nextmergetrxnum;
        ss_dprintf_3(("db_initforcemergevalues:dbe_counter_getcommittrxnum=%ld, db->db_forcemergetrxnum=%ld\n",
            DBE_TRXNUM_GETLONG(dbe_counter_getcommittrxnum(db->db_go->go_ctr)),
            DBE_TRXNUM_GETLONG(db->db_forcemergetrxnum)));
}

static int db_checkforcemergevalues(dbe_db_t* db)
{
        if (dbe_trxid_cmp(db->db_forcemergetrxid, dbe_counter_gettrxid(db->db_go->go_ctr)) < 0) {
            ss_dprintf_3(("db_checkforcemergevalues:TRUE, forcemergetrxid\n"));
            return(0);
        }
        if (dbe_trxnum_cmp(db->db_forcemergetrxnum, dbe_counter_getcommittrxnum(db->db_go->go_ctr)) < 0) {
            ss_dprintf_3(("db_checkforcemergevalues:TRUE, forcemergetrxnum\n"));
            return(0);
        }
        ss_dprintf_3(("db_checkforcemergevalues:FALSE\n"));
        return(DB_MERGEFIXEDRATE);
}

static void db_setnewforcemergevalues(dbe_db_t* db)
{
        if (db_checkforcemergevalues(db) > 0) {
            db->db_forcemergetrxid = dbe_trxid_sum(db->db_forcemergetrxid, DB_FORCEMERGEINTERVAL);
            db->db_forcemergetrxnum = dbe_trxnum_sum(db->db_forcemergetrxnum, DB_FORCEMERGEINTERVAL);
            ss_dprintf_3(("db_setnewforcemergevalues:db->db_forcemergetrxid=%ld, db->db_forcemergetrxnum=%ld\n",
                DBE_TRXID_GETLONG(db->db_forcemergetrxid),
                DBE_TRXNUM_GETLONG(db->db_forcemergetrxnum)));
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_init
 *
 * Initializes the database object. If the database file(s) specified by
 * inifile does not exist, a new database is created. Otherwise an existing
 * database is opened. After this call the database is operational and
 * can be used.
 *
 * Parameters :
 *
 *      inifile - in, hold
 *          Database init file specifying e.g. file names
 *          and buffer size.
 *
 *      p_dbstate - in out
 *          on input DBSTATE_CRASHED forces even a closed
 *          database to recover.
 *          on output DBSTATE_CRASHED indicates that
 *          the database will run a recovery mechanism
 *
 *      migratetounicode_done - in
 *          TRUE when UNICODE migration is done at this
 *          database open procedure.
 *
 * Return value - give :
 *
 *      Pointer to the database object.
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_db_t* dbe_db_init(
        rs_sysi_t* cd,
        su_inifile_t* inifile,
        dbe_dbstate_t* p_dbstate,
        bool migratetounicode_done,
        bool freelist_globallysorted
#ifdef SS_HSBG2
        , void *hsbg2ctx
#endif /* SS_HSBG2 */
        )
{
        dbe_db_t* db;
        dbe_ret_t rc;
        dbe_cfg_t* cfg;
        uint maxopenfiles;
        dbe_header_t* dbheader;
        su_daddr_t bonsairoot;
        su_daddr_t permroot;
        uint trxbufsize;
        uint relbufsize;
        bool migratetocatalogsupp;
        long freelistreserve;
        bool header_mergecleanup;
        ss_uint4_t header_flags;

#ifndef SS_NOLOCKING
        uint lockhashsize;
#endif /* SS_NOLOCKING */

        ss_dprintf_1(("dbe_db_init\n"));
        ss_dassert(inifile != NULL);

        h_db = NULL;

        dbe_error_init();
        su_param_manager_global_init(inifile);

        cfg = dbe_cfg_init(inifile);

        if (freelist_globallysorted) {
            dbe_cfg_setfreelistgloballysorted(cfg, TRUE);
        }

        /*
         * Register sybsystem ini-parameters.
         */
        dbe_cfg_register_su_params(cfg);
        dbe_cfg_getmaxopenfiles(cfg, &maxopenfiles);

        if (!dbexist_diskless && !su_vfh_globalinit(maxopenfiles)) {
            ui_msg_error_nostop(INI_MSG_FAILED_TO_SET_MAXOPENFILES_DD, maxopenfiles, SS_MAXFILES_DEFAULT);
        }

        db = SsMemCalloc(sizeof(dbe_db_t), 1);

        ss_debug(db->db_chk = DBE_CHK_DB);

        db->db_migratetounicode = FALSE;
        db->db_migratetoblobg2 = FALSE;
        db->db_migrategeneric = FALSE;
        db->db_go = dbe_gobj_init();
        db->db_go->go_syscd = cd;
        db->db_rbuf = rs_rbuf_init(NULL, NULL);
        db->db_go->go_ctr = dbe_counter_init();
        db->db_go->go_cfg = cfg;

        db->db_go->go_dbfile = db->db_dbfile = dbe_file_init(cfg, db);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;

        db->db_go->go_idxfd = db->db_dbfile->f_indexfile;

#ifdef IO_MANAGER
        db->db_go->go_iomgr = dbe_iomgr_init(db->db_dbfile, cfg);
#endif
#ifndef SS_NOBLOB
# ifdef IO_MANAGER
        db->db_go->go_blobmgr = dbe_blobmgr_init(
                                    db->db_go->go_iomgr,
                                    db->db_dbfile,
                                    db->db_go->go_ctr,
                                    SS_UINT4_MAX);
# else /* IO_MANAGER */
        db->db_go->go_blobmgr = dbe_blobmgr_init(
                                    db->db_dbfile,
                                    db->db_go->go_ctr,
                                    SS_UINT4_MAX);
# endif /* IO_MANAGER */
#else /* SS_NOBLOB */
        db->db_go->go_blobmgr = NULL;
#endif /* SS_NOBLOB */

#if defined(DBE_MTFLUSH)
        db->db_flushwakeupfp = (void (*)(void*))NULL;
        db->db_flushwakeupctx = NULL;
        db->db_flusheventresetfp = (void (*)(void*))NULL;
        db->db_flusheventresetctx = NULL;
        db->db_flushbatchwakeupfp = (void (*)(void*))NULL;
        db->db_flushbatchwakeupctx = NULL;
#endif /* DBE_MTFLUSH */
        db->db_cpmgr = dbe_cpmgr_init(db->db_dbfile);
        db->db_cpactive = FALSE;
        db->db_allowcheckpointing = TRUE;
        db->db_checkpointcallback = NULL;
        db->db_actiongate = su_gate_init(
                SS_SEMNUM_DBE_DB_ACTIONGATE,
                FALSE);
        db->db_users = su_pa_init();
        db->db_nsearchsem = SsSemCreateLocal(SS_SEMNUM_DBE_DB_NSEARCH);
        db->db_nsearch = 0;
        db->db_avgnsearch = DB_INIT_AVGNSEARCH;
        db->db_sem = SsSemCreateLocal(SS_SEMNUM_DBE_DB_SEM);
        db->db_mergesem = su_gate_init(SS_SEMNUM_DBE_DB_MERGE, FALSE);
        db->db_backup = NULL;
        db->db_indmerge = NULL;
        db->db_quickmerge = NULL;
        db->db_indmergenumber = 1;
        db->db_ddopactivecnt = 0;
        db->db_isloader = 0;
        db->db_mergeidletime = FALSE;
        db->db_mergedisablecount = 0;
        db->db_changed = FALSE;
        db->db_force_checkpoint = FALSE;
        db->db_final_checkpoint = 0;
        db->db_changedsem = SsSemCreateLocal(SS_SEMNUM_DBE_DB_CHANGED);
        db->db_dropcardinallist = NULL;

        db->db_cplasttime = SsTime(NULL);
        db->db_mergelasttime = SsTime(NULL);
        db->db_quickmergelasttime = SsTime(NULL);

        dbe_cfg_getbackup_blocksize(cfg, &backup_blocksize);
        dbe_cfg_getbackup_stepstoskip(cfg, &db->db_backup_stepstoskip);
        dbe_cfg_getmergeinterval(cfg, &db->db_mergelimit);
        dbe_cfg_getmergemintime(cfg, &db->db_mergemintime);
        dbe_cfg_getcpinterval(cfg, &db->db_cplimit);
        dbe_cfg_getcpmintime(cfg, &db->db_cpmintime);
        dbe_cfg_getidxcachesize(cfg, &db->db_poolsize);
        dbe_cfg_getearlyvld(cfg, &db->db_earlyvld);
        dbe_cfg_getreadonly(cfg, &db->db_readonly);
        dbe_cfg_getdisableidlemerge(cfg, &db->db_disableidlemerge);

        dbe_cfg_getcheckescalatelimit(cfg, &db->db_escalatelimits.esclim_check);
        dbe_cfg_getreadescalatelimit(cfg, &db->db_escalatelimits.esclim_read);
        dbe_cfg_getlockescalatelimit(cfg, &db->db_escalatelimits.esclim_lock);
        dbe_cfg_getallowlockebounce(cfg, &db->db_escalatelimits.esclim_allowlockbounce);
        dbe_cfg_getfreelistreserve(cfg, &freelistreserve);

#ifdef SS_HSBG2
        db->db_hsbg2configured = dbe_cfg_ishsbg2configured(db->db_go->go_cfg);

#ifdef REMOVED_TMP

        if (!db->db_hsbg2configured && ss_migratehsbg2) {
            /* migrating to HSBG2 requires that hsbg2 is enabled */

            su_informative_exit(
                __FILE__,
                __LINE__,
                DBE_ERR_MIGRATEHSB_WITHNOHSB);
        }
#endif

        db->db_hsbshutdown = FALSE;
        db->db_reloaf_rbuf_fun = NULL;
        db->db_reloaf_rbuf_ctx = NULL;

        db->db_hsb_durable_mes = SsMesCreateLocal();
        db->db_hsb_durable_cd = rs_sysi_init();
        db->db_starthsbstate = HSB_STATE_STANDALONE;

#endif

        db->db_go->go_earlyvld = db->db_earlyvld;

        dbe_bkeyinfo_init(
            &db->db_bkeyinfo,
            dbe_bnode_maxkeylen(db->db_dbfile->f_indexfile->fd_blocksize));
        db->db_go->go_bkeyinfo = &db->db_bkeyinfo;
        db->db_go->go_db = db;

#ifndef SS_NOSEQUENCE
        db->db_go->go_seq = db->db_seq = dbe_seq_init();
#endif /* SS_NOSEQUENCE */

        dbe_cfg_getrelbufsize(cfg, &relbufsize);
        rs_rbuf_setmaxbuffered(NULL, db->db_rbuf, relbufsize);

#ifndef SS_NOLOCKING
        dbe_cfg_getpessimistic(cfg, &db->db_pessimistic);
        dbe_cfg_getlocktimeout(
            cfg,
            &db->db_pessimistic_lock_to,
            &db->db_optimistic_lock_to);
        dbe_cfg_getlockhashsize(cfg, &lockhashsize);
        db->db_lockmgr = dbe_lockmgr_init(
                            lockhashsize,
                            db->db_escalatelimits.esclim_lock,
                            NULL);
#endif /* SS_NOLOCKING */

        dbe_cfg_gettablelocktimeout(    /* for SYNC conflict resolution */
            cfg,
            &db->db_table_lock_to);

        db->db_prevlogsizemeastime = (SsTimeT)0L;
        db->db_prevlogsize = 0L;
        db->db_fatalerrcode = SU_SUCCESS;
        db->db_logsplit = FALSE;
        db->db_recoverydone = FALSE;
#ifdef DBE_REPLICATION
        db->db_hsbenabled = FALSE;
        db->db_replicatefun = NULL;
        db->db_commitreadyfun = NULL;
        db->db_commitdonefun = NULL;
        db->db_repctx = NULL;
        db->db_hsbmode = DBE_HSB_STANDALONE;

#ifdef SS_HSBG2

        db->db_hsb_enable_syschanges = FALSE;

        db->db_hsbg2svc = dbe_hsbg2_init(hsbg2ctx, db);
        db->db_hsbstate = NULL;
        db->db_hsbg2_adaptiveif = FALSE;

#endif /* SS_HSBG2 */

        db->db_reptrxidmax = DBE_TRXID_NULL;
        db->db_repopcount = 0;
        db->db_hsbsem = SsSemCreateLocal(SS_SEMNUM_HSB_PRI);
        db->db_ishsb = FALSE;

        db->db_hsbunresolvedtrxs = FALSE;

#endif /* DBE_REPLICATION */
#ifdef SS_MME
        db->db_mmecp_state = MME_CP_INIT;
        db->db_mmecp_page = NULL;
        db->db_mmecp_pagedata = NULL;
        db->db_mmecp_daddr = SU_DADDR_NULL;
        db->db_mmecp_pageaddrpage = NULL;
        db->db_mmecp_pageaddrdata = NULL;
        db->db_mmecp_pageaddrdaddr = SU_DADDR_NULL;
        db->db_mmecp_flushbatch = NULL;
        db->db_mmecp_npages = 0;
#endif /* SS_MME */

        if (p_dbstate != NULL && *p_dbstate == DBSTATE_CRASHED) {
            /* Explicit recovery request:
             * recovery is from offline backup
             */
            ss_dprintf_2(("dbe_db_init:explicit DBSTATE_CRASHED\n"));
            db->db_dbstate = DBSTATE_CRASHED;
        } else {
            ss_dassert(p_dbstate == NULL || *p_dbstate == DBSTATE_CLOSED);
            db->db_dbstate = DBSTATE_CLOSED;
        }
        if (db->db_dbfile->f_indexfile->fd_created) {
            db->db_dbstate = DBSTATE_NEW;
        }

        {
            dbe_cryptoparams_t* cp = rs_sysi_getcryptopar(cd);
            su_cipher_t* cipher = cp == NULL ? NULL: dbe_crypt_getcipher(cp); ;

            if (cipher != NULL && dbe_crypt_getpasswd(cp) == NULL) {
                /* Accelerator creates new encrypted database */
                dbe_filedes_t* filedes = db->db_dbfile->f_indexfile;
                filedes->fd_cipher = cipher;
                su_svf_setcipher(filedes->fd_svfil, filedes->fd_cipher,
                                 dbe_crypt_getencrypt(cp),
                                 dbe_crypt_getdecrypt(cp));
            }
        }

        if (db->db_dbstate == DBSTATE_NEW) {
            DBE_CPMGR_CRASHPOINT(6);
            if (db->db_readonly) {
                su_informative_exit(__FILE__, __LINE__,
                    DBE_ERR_CANTRECOVERREADONLY_SS,
                    SU_DBE_GENERALSECTION,
                    SU_DBE_READONLY
                );
            }
            rc = dbe_file_startencryption(db->db_dbfile, cfg);
            if (rc == DBE_ERR_UNKNOWN_CRYPTOALG) {
                su_informative_exit(
                    __FILE__,
                    __LINE__,
                    rc
                );
            }
            ss_dassert(rc == DBE_RC_SUCC);
            rs_sdefs_setcurrentdefcatalog(RS_AVAL_DEFCATALOG_NEW);
            dbe_header_setdefcatalog(dbheader, RS_AVAL_DEFCATALOG);
            db->db_changed = TRUE;
            db_updateinfo(db);
            dbe_header_setdbstate(dbheader, DBSTATE_CRASHED);

            dbe_file_saveheaders(db->db_dbfile);
            db_checkheaderflags(dbheader);
        } else {
            rc = dbe_file_restoreheaders(db->db_dbfile, cfg);
            dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
            switch (rc) {
                case DBE_RC_SUCC:
                    break;
                case DBE_WARN_HEADERSINCONSISTENT:
                    ui_msg_warning(DBE_WARN_HEADERSINCONSISTENT);
                case DBE_WARN_DATABASECRASHED:
                    ss_dprintf_2(("dbe_db_init:DBSTATE_CRASHED from headers\n"));
                    db->db_dbstate = DBSTATE_CRASHED;
                    break;
                case DBE_ERR_HEADERSCORRUPT:
                    su_emergency2rc_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_DBCORRUPTED,
                        DBE_ERR_HEADERSCORRUPT);

                case DBE_ERR_WRONGBLOCKSIZE:
                case DBE_ERR_NODBPASSWORD:
                case DBE_ERR_WRONGPASSWORD:
                case DBE_ERR_UNKNOWN_CRYPTOALG:
                    su_informative_exit(
                        __FILE__,
                        __LINE__,
                        rc
                    );

                default:
                    su_rc_error(rc);
            }
            if (dbe_header_isbrokenhsbcopy(dbheader)) {
                if (!sqlsrv_isbackupservermode) {
                    *p_dbstate = DBSTATE_BROKENNETCOPY;
                    return(db);
                }
                /* else we know the netcopy has completed!
                   (fix for TPR 452282)
                */
                dbe_header_clearhsbcopybrokenstatus(dbheader);
                dbe_file_saveheaders(db->db_dbfile);
            }
            db_checkversion(dbheader,
                            &db->db_migratetounicode,
                            &migratetocatalogsupp,
                            &db->db_migratetoblobg2,
                            &db->db_migrategeneric);
            db_checkheaderflags(dbheader);
#ifdef SS_UNICODE_SQL
            if (migratetounicode_done) {
                db->db_migratetounicode = FALSE;
            }
#endif /* SS_UNICODE_SQL */

            if (dbe_ignorecrashed) {
                db->db_dbstate = DBSTATE_CLOSED;
            }

            switch (db->db_dbstate) {
                case DBSTATE_CRASHED:
                    if (db->db_readonly) {
                        su_informative_exit(__FILE__, __LINE__,
                            DBE_ERR_CANTRECOVERREADONLY_SS,
                            SU_DBE_GENERALSECTION,
                            SU_DBE_READONLY
                        );
                    }
#ifdef SS_UNICODE_SQL
                    if (db->db_migratetounicode) {
                        su_informative_exit(__FILE__, __LINE__,
                            DBE_ERR_CRASHEDDBNOMIGRATEPOS);
                    }
#endif /* SS_UNICODE_SQL */
                    if (migratetocatalogsupp) {
                        su_informative_exit(__FILE__, __LINE__,
                                            DBE_ERR_CRASHEDDBNOMIGRATEPOS);
                    }
                    if (db->db_migratetoblobg2 && !dbe_nologrecovery) {
                        su_informative_exit(__FILE__, __LINE__,
                                            DBE_ERR_CRASHEDDBNOMIGRATEPOS);
                    }
                    if (db->db_migrategeneric && !dbe_nologrecovery) {
                        ss_uint2_t dbvers;
                        dbvers = dbe_header_getdbvers(dbheader);
                        /* check that dbvers is at least 04.20 i.e 0x404 = 1028 */
                        if (dbvers > SU_DBFILE_VERSNUM || dbvers < 1028) {
                            su_informative_exit(__FILE__, __LINE__,
                                DBE_ERR_DATABASEFILEFORMAT_D,
                                (int)dbvers);
                        }
                    }
                    db->db_changed = TRUE;
                    db_restorelatestcp(db);
                    dbe_header_setdbvers(dbheader, SU_DBFILE_VERSNUM);
                    dbe_header_setdbstate(dbheader, DBSTATE_CRASHED);
                    dbe_file_saveheaders(db->db_dbfile);
                    break;
                case DBSTATE_CLOSED:
                    if (migratetocatalogsupp) {
                        if (RS_AVAL_DEFCATALOG_NEW == NULL) {
                            su_informative_exit(__FILE__, __LINE__,
                                                DBE_ERR_NOBASECATALOGGIVEN);
                        }
                        ss_dassert(RS_AVAL_DEFCATALOG_NEW != NULL);
                        ss_dassert(RS_AVAL_DEFCATALOG == NULL);
                    }
                    break;
                default:
                    ss_rc_error(db->db_dbstate);
            }
            if (!migratetocatalogsupp && !db->db_migratetounicode) {
                char* defcatalog = dbe_header_getdefcatalog(dbheader);
                rs_sdefs_setnewdefcatalog(defcatalog);
                rs_sdefs_setcurrentdefcatalog(defcatalog);
                ss_dassert(RS_AVAL_DEFCATALOG != NULL);
            }
            {
                ss_uint4_t timestamp;

                timestamp = dbe_header_getcreatime(dbheader);
                if (timestamp == 0L) {
                    ss_uint4_t crc_from_startrec;
                    ss_uint4_t ctc;
                    ss_uint4_t creatime;

                    crc_from_startrec =
                        dbe_header_calcstartreccrc(dbheader);
                    ctc = dbe_header_getctc(dbheader);
                    if (crc_from_startrec == ctc) {
                        if (db->db_readonly) {
                            su_informative_exit(
                                    __FILE__,
                                    __LINE__,
                                    DBE_ERR_CANTRECOVERREADONLY_SS,
                                    SU_DBE_GENERALSECTION,
                                    SU_DBE_READONLY
                            );
                        }
                        db->db_changed = TRUE;
                        creatime = (ss_uint4_t)SsTime(NULL);

                        ss_pprintf_1(("dbe_db_init:dbe_header_setcreatime %ld\n", creatime));

                        dbe_header_setcreatime(dbheader, creatime);
                        /* Mark the database so that
                         * we know it has been a demo database
                         * by calculating the ctc in a different
                         * way using su_lxc_calcctc twice.
                         */
                        ctc = su_lxc_calcctc(creatime);
                        ctc = su_lxc_calcctc(ctc);
                        dbe_header_setctc(dbheader, ctc);
                        dbe_header_setdbstate(dbheader, DBSTATE_CRASHED);
                        dbe_file_saveheaders(db->db_dbfile);
                    } else {
                        su_informative_exit(
                                __FILE__,
                                __LINE__,
                                SU_LI_LICENSEVIOLATION);
                    }
                }
            }
            db_getinfo(db);
        }

        header_flags = dbe_header_getheaderflags(dbheader);
        if (header_flags & HEADER_FLAG_MERGECLEANUP) {
            header_mergecleanup = TRUE;
        } else {
            header_mergecleanup = FALSE;
        }
        ss_dprintf_1(("dbe_db_init:header_mergecleanup=%d, dbe_cfg_mergecleanup=%d\n", header_mergecleanup, dbe_cfg_mergecleanup));

        dbe_cfg_gettrxbufsize(cfg, &trxbufsize);

        db->db_go->go_trxbuf = dbe_trxbuf_init(
                                trxbufsize,
                                dbe_counter_gettrxid(db->db_go->go_ctr),
                                header_mergecleanup);
#ifdef DBE_REPLICATION
        db->db_go->go_rtrxbuf = NULL;
#endif
        db->db_go->go_gtrs = dbe_gtrs_init(
                                    db,
                                    db->db_go->go_trxbuf,
                                    db->db_go->go_ctr);

        db->db_go->go_nmergewrites = dbe_counter_getmergectr(db->db_go->go_ctr);
        dbe_cfg_getquickmergeinterval(cfg, &db->db_quickmergelimit);

        dbe_file_start(db->db_dbfile, cfg, db->db_dbstate == DBSTATE_CRASHED);
        dbe_fl_setreservesize(db->db_go->go_idxfd->fd_freelist, (int)freelistreserve);

        if (db->db_dbstate == DBSTATE_CRASHED) {
            dbe_cpnum_t cpnum;
            dbe_cpnum_t prev_cpnum;

            cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);
            dbe_cpmgr_deldeadcheckpoints(db->db_cpmgr);
            prev_cpnum = dbe_cpmgr_prevcheckpoint(
                            db->db_cpmgr,
                            cpnum - 1);
            if (prev_cpnum != 0L) {
                dbe_cpmgr_deletecp(db->db_cpmgr, prev_cpnum);
            }
            dbe_cpmgr_inheritchlist(db->db_cpmgr);
        }
        bonsairoot = dbe_header_getbonsairoot(dbheader);
        permroot = dbe_header_getpermroot(dbheader);

        db->db_index = dbe_index_init(
                                db->db_go,
                                bonsairoot,
                                permroot);

        dbe_index_hsbsetbloblimit_high(
            db->db_index,
            db->db_hsbmode == DBE_HSB_PRIMARY);

#ifdef SS_MME
        db->db_mme = dbe_mme_init(
                db, cfg, dbe_counter_getcpnum(db->db_go->go_ctr),
                db_disklessmmememctx);
        db->db_go->go_mme = db->db_mme;
#endif /* SS_MME */

        dbe_cfg_getdefaultstoreismemory(cfg, &db->db_defaultstoreismemory);
        dbe_cfg_getdurabilitylevel(cfg, &db->db_durabilitylevel);

        db->db_hsbg2safenesslevel_adaptive = FALSE; /* default is 2-safe */

#ifdef SS_UNICODE_SQL
        dbe_srli_init(cd, db->db_rbuf, !(db->db_migratetounicode));
#else /* SS_UNICODE_SQL */
        dbe_srli_init(cd, db->db_rbuf, FALSE);
#endif /* SS_UNICODE_SQL */

        rs_rbuf_setresetcallback(cd, db->db_rbuf, dbe_srli_init, !db->db_migratetounicode);

        if (db->db_dbstate != DBSTATE_NEW) {

            /* Open an existing database.
             */
#ifndef SS_NOMMINDEX
            su_ret_t rc2;
#endif
            su_daddr_t trxlistaddr;
            su_daddr_t stmttrxlistaddr;
            su_daddr_t sequencelistaddr;
#ifdef DBE_REPLICATION
            su_daddr_t rtrxlistaddr;
#endif /* DBE_REPLICATION */

            trxlistaddr = dbe_header_gettrxlistaddr(dbheader);
            stmttrxlistaddr = dbe_header_getstmttrxlistaddr(dbheader);
            dbe_trxbuf_restore(
                db->db_go->go_trxbuf,
                db->db_dbfile->f_indexfile->fd_cache,
                trxlistaddr,
                stmttrxlistaddr);

            db->db_go->go_trxstat.ts_quickmergelimitcnt = dbe_trxbuf_getcount(db->db_go->go_trxbuf);

#ifndef SS_NOSEQUENCE
            sequencelistaddr = dbe_header_getsequencelistaddr(dbheader);
            dbe_seq_restore(
                db->db_seq,
                db->db_dbfile->f_indexfile->fd_cache,
                sequencelistaddr);
#endif /* SS_NOSEQUENCE */

#ifdef DBE_REPLICATION
            rtrxlistaddr = dbe_header_getrtrxlistaddr(dbheader);
            if (rtrxlistaddr == 0L) {
                rtrxlistaddr = SU_DADDR_NULL;
                dbe_header_setrtrxlistaddr(dbheader, rtrxlistaddr);
            }
            if (rtrxlistaddr != SU_DADDR_NULL && db->db_go->go_rtrxbuf == NULL) {
                db->db_go->go_rtrxbuf = dbe_rtrxbuf_init();
            }
#endif /* DBE_REPLICATION */

            if (db->db_dbstate != DBSTATE_CRASHED) {
#ifdef DBE_REPLICATION
                if (rtrxlistaddr != SU_DADDR_NULL) {
                    dbe_rtrxbuf_setsearchby(db->db_go->go_rtrxbuf, DBE_RTRX_SEARCHBYREMOTE);
                    ss_dprintf_2(("dbe_db_init:restore rtrxlist\n"));
                    rc = dbe_rtrxbuf_restore(
                            db->db_go->go_rtrxbuf,
                            db->db_dbfile->f_indexfile->fd_cache,
                            rtrxlistaddr);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                }
#endif /* DBE_REPLICATION */
            } else {
#ifdef DBE_REPLICATION
                if (rtrxlistaddr != SU_DADDR_NULL) {
                    ss_dprintf_2(("dbe_db_init:restore rtrxlist, addr=%ld\n", rtrxlistaddr));
                    dbe_rtrxbuf_setsearchby(
                        db->db_go->go_rtrxbuf,
                        DBE_RTRX_SEARCHBYLOCAL);
                    rc = dbe_rtrxbuf_restore(
                            db->db_go->go_rtrxbuf,
                            db->db_dbfile->f_indexfile->fd_cache,
                            rtrxlistaddr);
                    su_rc_assert(rc == DBE_RC_SUCC, rc);
                }
                dbe_header_setrtrxlistaddr(dbheader, SU_DADDR_NULL);
#endif /* DBE_REPLICATION */
                ss_dprintf_2(("dbe_db_init:clear header addresses\n"));
                dbe_header_settrxlistaddr(dbheader, SU_DADDR_NULL);
                dbe_header_setstmttrxlistaddr(dbheader, SU_DADDR_NULL);
#ifndef SS_NOSEQUENCE
                dbe_header_setsequencelistaddr(dbheader, SU_DADDR_NULL);
#endif /* SS_NOSEQUENCE */
            }
        }

        if (p_dbstate != NULL) {
            ss_dprintf_2(("dbe_db_init:*p_dbstate=%d\n", *p_dbstate));
            *p_dbstate = db->db_dbstate;
        }
        if (db->db_dbstate != DBSTATE_CRASHED) {
            db->db_recoverydone = TRUE;
        }

        ss_debug(dbe_db_hsbg2enabled = dbe_cfg_ishsbg2configured(db->db_go->go_cfg););

        dbe_db_setprimarystarttime(db);
        dbe_db_setsecondarystarttime(db);

        db->db_backupmme_cb = NULL;
        db->db_backupmmectx = NULL;

        db->db_lastmergecleanup = DBE_TRXNUM_NULL;

        db_initforcemergevalues(db);

        ss_dassert(h_db == NULL);
        h_db = db;

        if (!db->db_readonly && header_mergecleanup != dbe_cfg_mergecleanup) {
            rs_sysi_t* cd;

            ss_dprintf_1(("dbe_db_init:run quick merge\n"));
            cd = rs_sysi_init();
            dbe_db_initcd(db, cd);
            dbe_db_quickmergestart(cd, db);
            while (dbe_db_quickmergeadvance(db, cd)) {
                continue;
            }
            dbe_db_quickmergestop(db);

            ss_dprintf_1(("dbe_db_init:call dbe_trxbuf_setuseaborttrxid\n"));
            dbe_trxbuf_setusevisiblealltrxid(
                db->db_go->go_trxbuf,
                dbe_counter_gettrxid(db->db_go->go_ctr),
                dbe_cfg_mergecleanup);

            dbe_cfg_startupforcemerge = TRUE;
        }

        dbe_header_setheaderflags(dbheader);

        return(db);
}

void dbe_db_startupforcemergeif(
        rs_sysi_t* cd,
		dbe_db_t* db)
{

		if (!db->db_readonly && dbe_cfg_startupforcemerge) {
            dbe_db_mergestart_full(cd, db);
            while (dbe_db_mergeadvance(db, cd, 100)) {
                continue;
            }
            dbe_db_mergestop(db);
        }
        dbe_cfg_startupforcemerge = FALSE;
}        

#if defined(DBE_MTFLUSH)

/*##**********************************************************************\
 *
 *              dbe_db_setflushwakeupcallback
 *
 * Sets the multithread flush ended signal callback function and
 * context for it
 *
 * Parameters :
 *
 *      db - use
 *          database object
 *
 *      flushwakeupfp - in, hold
 *          wake up function pointer
 *
 *      flushwakeupctx - in out, hold
 *          parameter for wake up function
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
void dbe_db_setflushwakeupcallback(
        dbe_db_t* db,
        void (*flushwakeupfp)(void*),
        void* flushwakeupctx,
        void  (*flusheventresetfp)(void*),
        void* flusheventresetctx,
        void (*flushbatchwakeupfp)(void*),
        void* flushbatchwakeupctx)
{
        CHK_DB(db);

        db->db_flushwakeupfp = flushwakeupfp;
        db->db_flushwakeupctx = flushwakeupctx;
        db->db_flusheventresetfp = flusheventresetfp;
        db->db_flusheventresetctx = flusheventresetctx;
        db->db_flushbatchwakeupfp = flushbatchwakeupfp;
        db->db_flushbatchwakeupctx = flushbatchwakeupctx;
}

#endif /* DBE_MTFLUSH */

/*##**********************************************************************\
 *
 *              dbe_db_done
 *
 * Closes the database and releases the resources allocated to it.
 *
 * Parameters :
 *
 *      db - in, take
 *          Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_done(dbe_db_t* db)
{
        dbe_ret_t rc;
        dbe_header_t* dbheader = NULL;
        su_daddr_t bonsairoot;
        su_daddr_t permroot;
        su_daddr_t mmiroot;
        su_daddr_t trxbuf_addr;
        su_daddr_t rtrxbuf_addr = SU_DADDR_NULL;
        su_daddr_t stmttrxbuf_addr;
        dbe_cpnum_t cpnum;
        su_daddr_t seq_addr;

        ss_dprintf_1(("dbe_db_done\n"));
        CHK_DB(db);

        h_db = NULL;

        ss_svc_notify_done();

        db_mergestop_nomutex(db);
        db_quickmergestop_nomutex(db);

        if (db->db_backup != NULL) {
            dbe_backup_done(db->db_backup);
            db->db_backup = NULL;
            SS_PMON_SET(SS_PMON_BACKUPACT, 0);
        }

        if (db->db_fatalerrcode != SU_SUCCESS || (db->db_readonly && !db->db_hsbshutdown)) {
            /* Cannot close the database properly. */
            ss_dprintf_2(("dbe_db_done:cannot close database\n"));
            return;
        }

        ss_svc_notify_done();

        ss_svc_notify_done();

        dbe_counter_setmergectr(db->db_go->go_ctr, db->db_go->go_nmergewrites);

        db_updateinfo(db);

        ss_svc_notify_done();

        if (db->db_go->go_blobmgr != NULL) {
            dbe_blobmgr_done(db->db_go->go_blobmgr);
        }

        if (db->db_index != NULL) {
            dbe_index_done(db->db_index, &bonsairoot, &permroot);
            ss_assert(bonsairoot != permroot);
        }
        mmiroot = 0;

        ss_dassert(db->db_mme == NULL);
        DBE_CPMGR_CRASHPOINT(10);

        ss_svc_notify_done();

        if (db->db_dbfile != NULL) {
            dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
            if (db->db_index != NULL) {
                dbe_header_setbonsairoot(dbheader, bonsairoot);
                dbe_header_setpermroot(dbheader, permroot);
                dbe_header_setmmiroot(dbheader, mmiroot);
            }
        }

        cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);

        if (db->db_changed) {

#ifdef DBE_REPLICATION
            if (db->db_go->go_rtrxbuf != NULL) {
                rc = dbe_rtrxbuf_save(
                        db->db_go->go_rtrxbuf,
                        db->db_dbfile->f_indexfile->fd_cache,
                        db->db_dbfile->f_indexfile->fd_freelist,
                        cpnum,
                        &rtrxbuf_addr);
            }
#endif /* DBE_REPLICATION */

            rc = dbe_trxbuf_save(
                    db->db_go->go_trxbuf,
                    db->db_dbfile->f_indexfile->fd_cache,
                    db->db_dbfile->f_indexfile->fd_freelist,
                    cpnum,
                    &trxbuf_addr,
                    &stmttrxbuf_addr);
            su_rc_assert(rc == DBE_RC_SUCC, rc);

            dbe_seq_entermutex(db->db_seq);
            rc = dbe_seq_save_nomutex(
                    db->db_seq,
                    db->db_dbfile->f_indexfile->fd_cache,
                    db->db_dbfile->f_indexfile->fd_freelist,
                    cpnum,
                    &seq_addr);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
            dbe_seq_exitmutex(db->db_seq);
        }

        DBE_CPMGR_CRASHPOINT(11);

        ss_svc_notify_done();

        if (dbheader != NULL) {
            dbe_header_setcpnum(dbheader, cpnum);
        }

        if (db->db_changed) {

#ifdef DBE_REPLICATION
            dbe_header_setrtrxlistaddr(dbheader, rtrxbuf_addr);
#endif /* DBE_REPLICATION */

            dbe_header_settrxlistaddr(dbheader, trxbuf_addr);
            dbe_header_setstmttrxlistaddr(dbheader, stmttrxbuf_addr);

            dbe_header_setsequencelistaddr(dbheader, seq_addr);

            dbe_file_save(db->db_dbfile);
        }
        if (db->db_go->go_iomgr != NULL) {
            dbe_iomgr_done(db->db_go->go_iomgr);
        }
        if (db->db_dbfile != NULL) {
            dbe_file_done(db->db_dbfile);
        }
        dbe_cfg_done(db->db_go->go_cfg);

#ifdef SS_HSBG2
        /*
         * this cannot be shut down before dbe log
         */
        if(db->db_hsbg2svc != NULL) {
            dbe_hsbg2_done(db->db_hsbg2svc);
        }
        if (db->db_hsbstate != NULL) {
            dbe_hsbstate_done(db->db_hsbstate);
        }

        if (db->db_hsb_durable_mes != NULL) {
            SsMesFree(db->db_hsb_durable_mes);
            rs_sysi_done(db->db_hsb_durable_cd);
        }

#endif /* SS_HSBG2 */

        ss_svc_notify_done();
        if (db->db_lockmgr != NULL) {
            dbe_lockmgr_done(db->db_lockmgr);
        }
        if (db->db_seq != NULL) {
            dbe_seq_done(db->db_seq);
        }

#ifdef DBE_REPLICATION
        if (db->db_go->go_rtrxbuf != NULL) {
            dbe_rtrxbuf_done(db->db_go->go_rtrxbuf);
        }
        if (db->db_hsbsem != NULL) {
            SsSemFree(db->db_hsbsem);
        }
#endif /* DBE_REPLICATION */

        if (db->db_go->go_gtrs != NULL) {
            dbe_gtrs_done(db->db_go->go_gtrs);
        }
        if (db->db_go->go_trxbuf != NULL) {
            dbe_trxbuf_done(db->db_go->go_trxbuf);
        }

        if (db->db_rbuf != NULL) {
            rs_rbuf_done(NULL, db->db_rbuf);
        }
        if (db->db_cpmgr != NULL) {
            dbe_cpmgr_done(db->db_cpmgr);
        }
        if (db->db_actiongate != NULL) {
            su_gate_done(db->db_actiongate);
        }
        if (db->db_go->go_ctr != NULL) {
            dbe_counter_done(db->db_go->go_ctr);
        }
        if (db->db_users != NULL) {
            su_pa_done(db->db_users);
        }
        if (db->db_nsearchsem != NULL) {
            SsSemFree(db->db_nsearchsem);
        }
        if (db->db_changedsem != NULL) {
            SsSemFree(db->db_changedsem);
        }
        if (db->db_sem != NULL) {
            SsSemFree(db->db_sem);
        }
        if (db->db_mergesem != NULL) {
            su_gate_done(db->db_mergesem);
        }
        if (db->db_dropcardinallist != NULL) {
            su_list_done(db->db_dropcardinallist);
        }
        dbe_gobj_done(db->db_go);

        su_param_manager_save();
        ss_debug(su_param_manager_setreadonly());
        su_param_manager_global_done();

        FAKE_BLOBID_PRINTPOOL();
        su_vfh_globaldone();

        SsMemFree(db);
}

#ifdef SS_HSBG2

static void db_inithsbstate(dbe_db_t* db)
{
        if (db->db_hsbstate == NULL) {
            if (db->db_hsbg2configured || ss_migratehsbg2) {
                db->db_hsbstate = dbe_hsbstate_init(HSB_STATE_SECONDARY_ALONE, db);
            } else {
                db->db_hsbstate = dbe_hsbstate_init(HSB_STATE_STANDALONE, db);
            }
        }
}

#endif /* SS_HSBG2 */

/*##**********************************************************************\
 *
 *              dbe_db_recover
 *
 * Runs roll-forward recovery to database
 *
 * Parameters :
 *      db - in out, use
 *          Database object
 *
 *      user - in, use
 *          User object
 *
 *      getrelhfun - in, use
 *          pointer to data-dictionary read frunction (gets relation handle)
 *
 *      rbufctx - in, use
 *          context to getrelhfun
 *
 *      p_ncommits - out
 *          pointer to variable where number of recovered transactions
 *          will be stored or NULL when info not needed
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
dbe_ret_t dbe_db_recover(
        dbe_db_t* db,
        dbe_user_t* user,
        dbe_db_recovcallback_t* recovcallback,
        ulong* p_ncommits)
{
        dbe_ret_t rc;
        dbe_rollfwd_t* rf;
        ulong ncommits = 0;
        bool any_replicated;
        SsTimeT dbcreatime;
        SsTimeT cptimestamp;

        ss_dprintf_1(("dbe_db_recover\n"));
        ss_dassert(db->db_dbstate == DBSTATE_CRASHED);
        dbcreatime = dbe_header_getcreatime(
                        db->db_dbfile->f_indexfile->fd_dbheader);
        cptimestamp = dbe_cpmgr_gettimestamp(db->db_cpmgr);

#ifndef SS_NOLOGGING

#ifdef SS_HSBG2
        db_inithsbstate(db);
#endif /* SS_HSBG2 */

# ifdef DBE_REPLICATION
        if (db->db_go->go_rtrxbuf == NULL) {
            db->db_go->go_rtrxbuf = dbe_rtrxbuf_init();
            dbe_rtrxbuf_setsearchby(
                db->db_go->go_rtrxbuf,
                DBE_RTRX_SEARCHBYLOCAL);
        }
        if (db->db_hsbmode != DBE_HSB_SECONDARY) {
            if (dbe_db_ishsbconfigured(db)) {
                dbe_trxbuf_addopentrx(db->db_go->go_trxbuf,
                                    db->db_go->go_rtrxbuf,
                                    dbe_db_ishsbcopy(db) ? FALSE : TRUE);
            }
        }
# endif

        rf = dbe_rollfwd_init(
                db->db_go->go_cfg,
                db->db_go->go_ctr,
                user,
                db->db_go->go_trxbuf,
                db->db_go,
                recovcallback,
                dbcreatime,
                cptimestamp,
# ifdef DBE_REPLICATION
#ifdef SS_HSBG2
                ss_migratehsbg2 ? db->db_hsbmode : dbe_db_gethsbg2mode(db),
#else /* SS_HSBG2 */
                db->db_hsbmode,
#endif /* SS_HSBG2 */
                db->db_reptrxidmax,
                db->db_go->go_rtrxbuf
# else
                DBE_HSB_STANDALONE,
                0L,
                NULL
# endif
#ifdef SS_HSBG2
                , db->db_hsbg2svc
#endif
             );
        any_replicated = FALSE;

		if (!dbe_nologrecovery) {
           rc = dbe_rollfwd_scancommitmarks(rf, &ncommits, &any_replicated, &db->db_starthsbstate);

           /* Jarmo changed if statement, Apr 16, 1996
              if ((rc == DBE_RC_SUCC || rc == DBE_RC_LOGFILE_TAIL_CORRUPT)
              &&  (ncommits > 0 || any_replicated))
           */
           if (rc == DBE_RC_SUCC || rc == DBE_RC_LOGFILE_TAIL_CORRUPT)
           {
               bool saved_splitpurge;

               /* Cannot use splitpurge during recovery because merge level
                * is not correctly set. 
                */
               saved_splitpurge = dbe_cfg_splitpurge;
               dbe_cfg_splitpurge = FALSE;

               ss_dprintf_1(("dbe_db_recover(): %lu transactions to recover\n",
                           ncommits));
               rc = dbe_rollfwd_recover(rf);

               dbe_cfg_splitpurge = saved_splitpurge;
		   }
		}
		else {
           rc = DBE_RC_SUCC;
        }

        db->db_recoverydone = TRUE;

#ifdef SS_MME
        /* End MME recovery */

#ifdef SS_HSBG2
        /* HSB-recovery must not abort open transactions */
        if (!ss_migratehsbg2 && !db->db_hsbg2configured) {
            ss_dprintf_1(("dbe_mme_endrecov:dbe_db_recove:%d,%d\n",
                    ss_migratehsbg2, db->db_hsbg2configured));
            ss_dprintf_1(("dbe_mme_endrecov:dbe_db_recover\n"));
            dbe_mme_endrecov(db->db_mme);
#ifdef MME_CP_FIX_XXX
            ss_debug(dbe_mme_check_integrity(db->db_mme));
#endif
        }
#else /* SS_HSBG2 */
        dbe_mme_endrecov(db->db_mme);
#endif /* SS_HSBG2 */

#endif /* SS_MME */

# ifdef DBE_REPLICATION
#ifdef SS_HSBG2
        if (dbe_db_gethsbg2mode(db) == DBE_HSB_STANDALONE)
#endif /* SS_HSBG2 */
        {
        db->db_hsbmode = dbe_rollfwd_gethsbmode(rf);
        ss_dprintf_2(("dbe_db_recover(): dbe_rollfwd_gethsbmode()=%d\n", (int)db->db_hsbmode));
        db_hsbmodechangereset(db);
        if (dbe_rollfwd_getreptrxidmaxupdated(rf)) {
            /* Overwrite values only if we have found during recovery.
             */
            db->db_reptrxidmax = dbe_rollfwd_getreptrxidmax(rf);
            db->db_repopcount = 0;
        } else {
            ss_dassert(DBE_TRXID_EQUAL(db->db_reptrxidmax, dbe_rollfwd_getreptrxidmax(rf)));
        }
        if (db->db_go->go_rtrxbuf != NULL) {
            dbe_rtrxbuf_removeaborted(
                db->db_go->go_rtrxbuf,
                db->db_go->go_trxbuf);
            dbe_rtrxbuf_setsearchby(
                db->db_go->go_rtrxbuf,
                DBE_RTRX_SEARCHBYREMOTE);
        }
        }
        /* if (db->db_hsbmode != DBE_HSB_SECONDARY) */
# endif
        {
            /*  Moved to dbe_db_open.
                dbe_trxbuf_cleanuncommitted(db->db_go->go_trxbuf); */
        }
        dbe_rollfwd_done(rf);

#endif /* SS_NOLOGGING */

        db->db_go->go_trxstat.ts_quickmergelimitcnt = dbe_trxbuf_getcount(db->db_go->go_trxbuf);

        if (p_ncommits != NULL) {
            *p_ncommits = ncommits;
        }
        if (rc == DBE_ERR_LOGFILE_CORRUPT && db->db_hsbg2configured) {
            /* In case of HSB we accept this error code but does not allow it to work
             * with any other database node. Normal server also starts after this error
             * in next restart because broken part of the log is cleared. So we do not
             * change the behaviour very much.
             */
            rc = DBE_RC_LOGFILE_TAIL_CORRUPT;
            dbe_db_sethsbtime_outofsync(db);
        }

        dbe_db_setprimarystarttime(db);
        dbe_db_setsecondarystarttime(db);

        return (rc);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              dbe_db_sethsbunresolvedtrxs
 *
 * Sets the value of HSB unresolved transactions flag for database.
 *
 * It is pre-requirement that you have entered the dbe_hsbsem mutex before
 * calling this function.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      b - use
 *              Value for HSB unresolved transactions flag
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_sethsbunresolvedtrxs(
        dbe_db_t* db,
        bool b)
{
        CHK_DB(db);

    ss_dassert(SsSemThreadIsEntered(db->db_hsbsem));

        /*
         * The assertion below checks whether unresolved transactions may
         * be nested. If the assertion does not hold then boolean flag
         * is not sufficient to express unresolved transactions.
         *
         * mikko / 2002-02-12
         *
         */
        ss_dassert(b == FALSE || db->db_hsbunresolvedtrxs == FALSE);

        db->db_hsbunresolvedtrxs = b;
}

/*##**********************************************************************\
 *
 *              dbe_db_gethsbunresolvedtrxs
 *
 * Returns the value of HSB unresolved transactions flag for database
 *
 * It is pre-requirement that you have entered the dbe_hsbsem mutex before
 * calling this function.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 * Return value :
 *
 *      TRUE if there are HSB unresolved transactions, FALSE otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_gethsbunresolvedtrxs(
        dbe_db_t* db)
{
        CHK_DB(db);

    ss_dassert(SsSemThreadIsEntered(db->db_hsbsem));

        return (db->db_hsbunresolvedtrxs);
}

#ifdef SS_HSBG2

void dbe_db_hsbenable_syschanges(
        dbe_db_t* db,
        bool enablep)
{
        ss_dprintf_1(("dbe_db_hsbenable_syschanges:enablep=%d\n"));
        CHK_DB(db);

        db->db_hsb_enable_syschanges = enablep;
}


void dbe_db_setstarthsbstate(
        dbe_db_t* db,
        dbe_hsbstatelabel_t hsbstate)
{
        CHK_DB(db);

        db->db_starthsbstate = hsbstate;
}

#ifndef SS_MYSQL
dbe_hsbstate_t* dbe_db_gethsbstate(dbe_db_t* db)
{
        CHK_DB(db);
        ss_assert(db->db_hsbstate != NULL);

        return (db->db_hsbstate);
}
#endif /* !SS_MYSQL */

#ifdef SS_MYSQL_AC

char* dbe_db_gethsbrolestr(dbe_db_t* db)
{
        hsb_role_t role;

        role = dbe_hsbstate_getrole(db->db_hsbstate);
        return(dbe_hsbstate_getrolestring_user(role));
}

char* dbe_db_gethsbstatestr(dbe_db_t* db)
{
        dbe_hsbstatelabel_t state_label;

        state_label = dbe_hsbstate_getlabel(db->db_hsbstate);
        return(dbe_hsbstate_getuserstatestring(state_label));
}

#endif /* SS_MYSQL_AC */

#endif /* SS_HSBG2 */


/*##**********************************************************************\
 *
 *              dbe_db_getreptrxidmax
 *
 * Gets max trxid of committed remote (replicated) transactions after
 * recovery
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 * Return value :
 *      max trxid
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_trxid_t dbe_db_getreptrxidmax(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_reptrxidmax);
}

/*##**********************************************************************\
 *
 *              dbe_db_setreptrxidmax
 *
 * Sets max trxid of committed remote (replicated) transactions.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      reptrxidmax - in
 *          max remote trx id
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_setreptrxidmax(dbe_db_t* db, dbe_trxid_t reptrxidmax)
{
        ss_dprintf_1(("dbe_db_setreptrxidmax:reptrxidmax=%ld\n", DBE_TRXID_GETLONG(reptrxidmax)));
        CHK_DB(db);

        db->db_reptrxidmax = reptrxidmax;
}

/*##**********************************************************************\
 *
 *              dbe_db_getrepopcount
 *
 * Gets replication operation count since last replication commit.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 * Return value :
 *      operation count
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
long dbe_db_getrepopcount(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_repopcount);
}

/*##**********************************************************************\
 *
 *              dbe_db_setrepopcount
 *
 * Sets replication operation count since last replication commit.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      repopcount - in
 *          operation count
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_setrepopcount(dbe_db_t* db, long repopcount)
{
        CHK_DB(db);

        db->db_repopcount = repopcount;
}

/*##**********************************************************************\
 *
 *              dbe_db_getcurtrxidmax
 *
 * Returns maximum current transaction id value.
 *
 * Parameters :
 *
 *      db -
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
dbe_trxid_t dbe_db_getcurtrxidmax(dbe_db_t* db)
{
        CHK_DB(db);

        return(dbe_counter_gettrxid(db->db_go->go_ctr));
}

/*##**********************************************************************\
 *
 *              dbe_db_hsbsem_get
 *
 * Returns reference to Hot Standby semaphore.
 *
 * Parameters :
 *
 *      db -
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
SsSemT* dbe_db_hsbsem_get(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_hsbsem);
}

#ifdef SS_DEBUG
/*##**********************************************************************\
 *
 *              dbe_db_hsbsem_isentered
 *
 * Checks if Hot Standby semaphore is already entered. Used only for
 * debug asserts.
 *
 * Parameters :
 *
 *      db -
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
bool dbe_db_hsbsem_isentered(dbe_db_t* db)
{
        CHK_DB(db);

        return(SsSemIsEntered(db->db_hsbsem));
}
#endif /* SS_DEBUG */

void dbe_db_sethsb(dbe_db_t* db)
{
        CHK_DB(db);

        db->db_ishsb = TRUE;
}

void dbe_db_reset_logpos(dbe_db_t* db)
{
        CHK_DB(db);

        if (dbe_db_ishsbcopy(db)) {
            dbe_hsbg2_reset_logpos(db->db_hsbg2svc);
        }
}

bool dbe_db_ishsbcopy(dbe_db_t* db)
{
        CHK_DB(db);

        return(dbe_header_ishsbcopy(db->db_dbfile->f_indexfile->fd_dbheader));
}

/*##**********************************************************************\
 *
 *              dbe_db_ishsbconfigured
 *
 * Returns TRUE if Hot Standby is configured in configuration file.
 *
 * Parameters :
 *
 *      db -
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
bool dbe_db_ishsbconfigured(dbe_db_t* db)
{
        CHK_DB(db);

        return(dbe_cfg_ishsbconfigured(db->db_go->go_cfg));
}

#endif /* DBE_REPLICATION */


/*##**********************************************************************\
 *
 *              dbe_db_ishsbg2configured
 *
 * Returns TRUE if Hot Standby G2 is configured in configuration file.
 *
 * Parameters :
 *
 *      db -
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
bool dbe_db_ishsbg2configured(dbe_db_t* db)
{
        CHK_DB(db);

#ifdef SS_HSBG2
        return(db->db_hsbg2configured);
#else
        return(FALSE);
#endif
}


/*##**********************************************************************\
 *
 *              dbe_db_open
 *
 * Opens database for client access (initializes logging)
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
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
dbe_ret_t dbe_db_open(dbe_db_t* db)
{
#ifndef SS_NOLOGGING
        bool logenabled;
#endif /* !SS_NOLOGGING */

        CHK_DB(db);
        ss_dassert(db->db_dbfile->f_log == NULL);

#ifdef SS_HSBG2
        db_inithsbstate(db);

        ss_dprintf_1(("dbe_db_open: db->db_hsbg2mode=%d, ss_migratehsbg2=%d\n", (int)dbe_db_gethsbg2mode(db), ss_migratehsbg2));
        if (dbe_db_gethsbg2mode(db) != DBE_HSB_SECONDARY) {
            ss_dassert(dbe_db_gethsbg2mode(db) == DBE_HSB_STANDALONE);
            if (!ss_migratehsbg2) {
                ss_dprintf_2(("dbe_trxbuf_cleanuncommitted\n"));
                dbe_trxbuf_cleanuncommitted(db->db_go->go_trxbuf, dbe_counter_getcommittrxnum(db->db_go->go_ctr));
            }
            if (!db->db_migratetoblobg2) {
                size_t n_deleted;
                su_ret_t rc;

                rc = (*dbe_blobg2callback_delete_unreferenced_blobs_after_recovery)(
                        db->db_go->go_syscd,
                        &n_deleted,
                        NULL);
                if (rc == SU_SUCCESS) {
                    ss_dprintf_1(("dbe_blobg2callback_delete_unreferenced_blobs_after_recovery: %ld blobs deleted\n",
                                    (long)n_deleted));
                } else {
                    ss_rc_derror(rc);
                }
            }
        }
#else  /* SS_HSBG2 */
        ss_dprintf_1(("dbe_db_open: db->db_hsbmode=%d\n", (int)db->db_hsbmode));
        if (db->db_hsbmode != DBE_HSB_SECONDARY) {
            if (!dbe_db_ishsbconfigured(db)) {
                size_t n_deleted;
                su_ret_t rc;

                ss_dprintf_2(("dbe_db_open: hsb not configured, call dbe_trxbuf_cleanuncommitted() (hsbmode=%d)\n", (int)db->db_hsbmode));
                dbe_trxbuf_cleanuncommitted(db->db_go->go_trxbuf, dbe_counter_getcommittrxnum(db->db_go->go_ctr));
                if (!db->db_migratetoblobg2) {
                    rc = (*dbe_blobg2callback_delete_unreferenced_blobs_after_recovery)(
                            db->db_go->go_syscd,
                            &n_deleted,
                            NULL);
                    if (rc == SU_SUCCESS) {
                        ss_dprintf_1(("dbe_blobg2callback_delete_unreferenced_blobs_after_recovery: %ld blobs deleted\n",
                                      (long)n_deleted));
                    } else {
                        ss_rc_derror(rc);
                    }
                }
            }
        }
#endif /* SS_HSBG2 */
        if (db->db_dbstate == DBSTATE_NEW) {
            dbe_counter_newdbinit(db->db_go->go_ctr);
        }
#ifndef SS_NOLOGGING
        dbe_cfg_getlogenabled(db->db_go->go_cfg, &logenabled);
        if (!logenabled) {
            ui_msg_message(LOG_MSG_DISABLED);
            if (db->db_hsbg2configured) {
                /* HSB is on, but logging is off -> use virtual logging */
                logenabled = TRUE;
            }
        }
        if (db->db_changed
#ifdef SS_UNICODE_SQL
        &&  !db->db_migratetounicode
#endif /* SS_UNICODE_SQL */
        ) {
            db->db_dbfile->f_log =
                dbe_log_init(
#ifdef DBE_LOGORDERING_FIX
                    db,
#endif /* DBE_LOGORDERING_FIX */
                    db->db_go->go_cfg,
                    db->db_go->go_ctr,
                    (db->db_dbstate == DBSTATE_NEW),
                    dbe_header_getcreatime(
                        db->db_dbfile->f_indexfile->fd_dbheader),
                    (db->db_hsbg2configured || ss_migratehsbg2)
                        ? DBE_LOG_INSTANCE_LOGGING_HSB
                        : DBE_LOG_INSTANCE_LOGGING_STANDALONE
#ifdef SS_HSBG2
                    ,db->db_hsbg2svc
#endif
                    );
        } else {
            db->db_dbfile->f_log = NULL;
        }
#else
        db->db_dbfile->f_log = NULL;
#endif /* SS_NOLOGGING */

        ss_dprintf_1(("dbe_db_open:log %x\n", db->db_dbfile->f_log));

#ifdef SS_MME
        dbe_mme_startservice(db->db_mme);
#endif

        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdbstate
 *
 * Returns the db open status.
 *
 * Parameters :
 *
 *      db -
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
dbe_dbstate_t dbe_db_getdbstate(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_dbstate);
}

/*##**********************************************************************\
 *
 *              dbe_db_setfatalerror
 *
 * Sets the database to a fatal error state. In fatal error state the
 * database cannot be closed properly and checkpoints cannot be made.
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_setfatalerror(dbe_db_t* db, su_ret_t rc)
{
        if (db != NULL) {
            /* The db object may be NULL in some test programs
             * that do not create the db object.
             */
            CHK_DB(db);

            if (db->db_fatalerrcode == SU_SUCCESS) {

                /* for diskless, allow user to connect even if the database
                 * was full. There is a chance that some users rollback and
                 * the disk is not full anymore.
                 */
                if (dbefile_diskless && rc == SU_ERR_FILE_WRITE_CFG_EXCEEDED) {
                    return;
                }

                db->db_fatalerrcode = rc;

                ui_msg_error_nostop(rc);
            }
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_force_checkpoint
 *
 * Sets a flag that checkpoint is forced even though there are no db
 * changes.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      p_errh - out, give
 *          in case of error a pointer to new error handle object
 *          is given
 *
 * Return value :
 *      TRUE - success
 *      FALSE - error (database is read-only)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_force_checkpoint(dbe_db_t* db)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        if (db->db_readonly) {
            rc = DBE_ERR_DBREADONLY;
        } else {
            db->db_force_checkpoint = TRUE;
        }

        return (rc);
}

void dbe_db_setfinalcheckpoint(dbe_db_t* db)
{
        CHK_DB(db);

        db->db_final_checkpoint = 1;
        if (db->db_dbfile->f_log != NULL) {
            /* Set pending final checkpoint to log too. */
            dbe_log_set_final_checkpoint(db->db_dbfile->f_log, 1);
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_setchanged
 *
 * Sets the database object to changed state. i.e. there are write
 * operations done to the database.
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      p_errh - out, give
 *          in case of error a pointer to new error handle object
 *          is given
 *
 * Return value :
 *      TRUE - success
 *      FALSE - error (database is read-only)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_setchanged(dbe_db_t* db, rs_err_t** p_errh)
{
        CHK_DB(db);
        FAKE_CODE_BLOCK(
            FAKE_DBE_SETDBREADONLY,
            {
                if (!db->db_readonly) {
                    SsPrintf("FAKE_DBE_SETDBREADONLY\n");
                    db->db_readonly = TRUE;
                }
            }
        );
        if (db->db_readonly) {
            if (p_errh != NULL) {
                rs_error_create(p_errh, DBE_ERR_DBREADONLY);
            }
            return (FALSE);
        }
        if (!db->db_changed) {
            SsSemEnter(db->db_changedsem);
            if (!db->db_changed) {
                dbe_header_t* dbheader;

                ss_dassert(db->db_dbfile->f_log == NULL);
#ifndef SS_NOLOGGING
                if (!db->db_migratetounicode) {
                    db->db_dbfile->f_log =
                        dbe_log_init(
#ifdef DBE_LOGORDERING_FIX
                            db,
#endif /* DBE_LOGORDERING_FIX */
                            db->db_go->go_cfg,
                            db->db_go->go_ctr,
                            (db->db_dbstate == DBSTATE_NEW),
                            dbe_header_getcreatime(
                                db->db_dbfile->f_indexfile->fd_dbheader),
                            (db->db_hsbg2configured || ss_migratehsbg2)
                                ? DBE_LOG_INSTANCE_LOGGING_HSB
                                : DBE_LOG_INSTANCE_LOGGING_STANDALONE
#ifdef SS_HSBG2
                            ,db->db_hsbg2svc
#endif
                            );

                }
#endif /* !SS_NOLOGGING */
                ss_dprintf_1(("dbe_db_setchanged:log %x\n", db->db_dbfile->f_log));
                db_removetrxlists(db);
                dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
                dbe_header_setheadervers(dbheader, SU_DBHEADER_VERSNUM);
#ifdef SS_UNICODE_SQL
                if (!db->db_migratetounicode)
#endif /* SS_UNICODE_SQL */
                {
                    dbe_header_setdbvers(dbheader, SU_DBFILE_VERSNUM);
                }
                dbe_header_setdbstate(dbheader, DBSTATE_CRASHED);
                dbe_file_saveheaders(db->db_dbfile);
            }
            db->db_changed = TRUE;
            SsSemExit(db->db_changedsem);
        }
        return(TRUE);
}

/*##**********************************************************************\
 *
 *              dbe_db_getinifile
 *
 * Returns inifile object for the database.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 * Return value - ref :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_inifile_t* dbe_db_getinifile(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(dbe_cfg_getinifile(db->db_go->go_cfg));
}

#ifdef SS_MME

void dbe_db_clearmme(
        rs_sysi_t*      cd,
        dbe_db_t*       db)
{
        CHK_DB(db);

        dbe_mme_done(cd, db->db_mme);
        db->db_mme = NULL;
}

void dbe_db_gettemporarytablecardin(
        rs_sysi_t*      cd,
        dbe_db_t*       db,
        rs_relh_t*      relh,
        ss_int8_t*      p_nrows,
        ss_int8_t*      p_nbytes)
{
        dbe_mme_gettemporarytablecardin(
                cd, db->db_mme, relh, p_nrows, p_nbytes);
}

#endif /* SS_MME */

bool dbe_db_ismme(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_mme != NULL);
}

bool dbe_db_getdefaultstoreismemory(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_defaultstoreismemory);
}

void dbe_db_setdefaultstoreismemory(
        dbe_db_t* db, bool val)
{
        CHK_DB(db);

        db->db_defaultstoreismemory = val;
}

void dbe_db_settablelocktimeout(dbe_db_t* db, long val)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        db->db_table_lock_to = val;

        SsSemExit(db->db_sem);
}

void dbe_db_setcpinterval(
        dbe_db_t* db, long val)
{
        CHK_DB(db);

        if (val < 0) {
            ss_derror;
            val = 0;
        }

        db->db_cplimit = val;
}

void dbe_db_setmergeinterval(dbe_db_t* db, long val)
{
        CHK_DB(db);

        if (val <= 0) {
            dbe_cfg_getmergeinterval(db->db_go->go_cfg, &db->db_mergelimit);
        } else {
            db->db_mergelimit = val;
        }
}

void dbe_db_setmergemintime(dbe_db_t* db, long val)
{
        CHK_DB(db);

        if (val < 0) {
            dbe_cfg_getmergemintime(db->db_go->go_cfg, &db->db_mergemintime);
        } else {
            db->db_mergemintime = val;
        }
}

void dbe_db_setcpmintime(dbe_db_t* db, long val)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        db->db_cpmintime = val;

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdurabilitylevel
 *
 * Returns default durability level.
 *
 * Parameters :
 *
 *      db -
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
dbe_durability_t dbe_db_getdurabilitylevel(
        dbe_db_t* db)
{
        CHK_DB(db);

#ifdef SS_HSBG2
        if (dbe_db_gethsbg2mode(db) == DBE_HSB_SECONDARY) {
            return(DBE_DURABILITY_RELAXED);
        }
#endif

        switch (db->db_durabilitylevel) {
            case DBE_DURABILITY_ADAPTIVE:
#ifdef SS_HSBG2
                if (db->db_hsbg2_adaptiveif) {
                    return(DBE_DURABILITY_RELAXED);
                }
#endif
                return(DBE_DURABILITY_STRICT);
                break;

            case DBE_DURABILITY_RELAXED:
            case DBE_DURABILITY_STRICT:
                break;
            default:
                ss_error;

        }
        return(db->db_durabilitylevel);
}

dbe_durability_t dbe_db_getdurabilitylevel_raw(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_durabilitylevel);
}

/*##**********************************************************************\
 *
 *      dbe_db_setdurabilitylevel
 *
 * Set default durabilitylevel for the db.
 *
 * Parameters:
 *      db - in
 *          database to affect
 *
 *      durability - in
 *          new durability level
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_db_setdurabilitylevel(dbe_db_t* db, dbe_durability_t durability)
{
        CHK_DB(db);

        switch (durability) {
            case DBE_DURABILITY_RELAXED:
            case DBE_DURABILITY_STRICT:
            case DBE_DURABILITY_ADAPTIVE:
                db->db_durabilitylevel = durability;
                break;
            default:
                ss_rc_error(durability);
                break;
        }
}

bool dbe_db_hsbg2safenesslevel_adaptive(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_hsbg2safenesslevel_adaptive);
}

void dbe_db_set_hsbg2safenesslevel_adaptive(
        dbe_db_t* db,
        bool adaptive)
{
        CHK_DB(db);

        db->db_hsbg2safenesslevel_adaptive = adaptive;
}

/*##**********************************************************************\
 *
 *              dbe_db_getrbuf
 *
 * Returns the relation buffer of the database object. The relation buffer
 * buffers all relation information to the memory for fast access.
 *
 * Parameters :
 *
 *      db - in, use
 *              Database object.
 *
 * Return value - ref :
 *
 *      Pointer to the relation buffer object.
 *
 * Limitations  :
 *
 * Globals used :
 */
rs_rbuf_t* dbe_db_getrbuf(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_rbuf);
}


/*##**********************************************************************\
 *
 *              dbe_db_setreadonly
 *
 * Sets the database read only flag value.
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      readonlyp -
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
void dbe_db_setreadonly(
        dbe_db_t* db,
        bool readonlyp)
{
        ss_dprintf_1(("dbe_db_setreadonly:readonlyp=%d\n", readonlyp));
        CHK_DB(db);

        db->db_readonly = readonlyp;
}

void dbe_db_starthsbshutdown(
        dbe_db_t* db)
{
        ss_dprintf_1(("dbe_db_starthsbshutdown\n"));
        CHK_DB(db);
        if (db->db_dbfile->f_log != NULL) {
            dbe_log_setidlehsbdurable(db->db_dbfile->f_log, DBE_LOGFILE_IDLEHSBDURABLE_DISABLE);
        }
}

void dbe_db_sethsbshutdown(
        dbe_db_t* db)
{
        ss_dprintf_1(("dbe_db_sethsbshutdown\n"));
        CHK_DB(db);

        db->db_readonly = TRUE;
        db->db_hsbshutdown = TRUE;
        if (db->db_final_checkpoint && dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY_UNCERTAIN) {
            db->db_fatalerrcode = DBE_ERR_HSBPRIMARYUNCERTAIN;
        }
}

#ifndef SS_NOLOCKING
/*##**********************************************************************\
 *
 *              dbe_db_ispessimistic
 *
 * Returns the default concurrency control mode.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 * Return value :
 *
 *      TRUE    - default is to use pessimistic CC
 *      FALSE   - default is to use optimistic CC
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_ispessimistic(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_pessimistic);
}

void dbe_db_setlocktimeout(
        dbe_db_t* db,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to)
{
        CHK_DB(db);

        if (p_pessimistic_lock_to != NULL) {
            db->db_pessimistic_lock_to = *p_pessimistic_lock_to;
        }
        if (p_optimistic_lock_to != NULL) {
            db->db_optimistic_lock_to = *p_optimistic_lock_to;
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_removelastfilespec
 *
 * Removes last physical file from database file configuration. Only empty
 * files can be removed.
 *
 * Parameters :
 *
 *      db -
 *
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
su_ret_t  dbe_db_removelastfilespec(dbe_db_t* db)
{
        su_inifile_t* dbinifile;
        su_ret_t rc;

        CHK_DB(db);
        dbinifile = dbe_db_getinifile(db);
        rc = dbe_file_removelastfilespec(dbinifile, db->db_dbfile);
        return rc;

}

/*##**********************************************************************\
 *
 *              dbe_db_addnewfilespec
 *
 *  Add new physical file to database configuration.
 *
 * Parameters :
 *
 *      db -
 *
 *      filespecs - in, use
 *              current filespecs
 *
 *      filename - in, use
 *          name of the new physical file.
 *
 *      maxsize - in, use
 *          maximum size of the new physical file
 *
 *      diskno - in, use
 *          device number of the new physical file
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
su_ret_t dbe_db_addnewfilespec(
        dbe_db_t* db,
        char* filename,
        ss_int8_t maxsize,
        uint diskno)
{
        su_inifile_t* dbinifile;
        su_ret_t rc;

        CHK_DB(db);
        dbinifile = dbe_db_getinifile(db);
        rc = dbe_file_addnewfilespec(dbinifile,
                                     db->db_dbfile,
                                     filename, maxsize, diskno);
        return rc;
}

/*##**********************************************************************\
 *
 *              dbe_db_fileusageinfo
 *
 * Get usage info of whole database file and single physical file.
 *
 * Parameters :
 *
 *      db -
 *
 *      maxsize - out
 *          maximun size of the database (in megabytes)
 *
 *      currsize - out
 *          current size of the database (in megabytes)
 *
 *      totalperc - out
 *          percentage used (whole database)
 *
 *      nth - in
 *          give percentage info also from nth physical file
 *
 *      perc - out
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
void dbe_db_fileusageinfo(dbe_db_t* db, double* maxsize, double* currsize, float* totalperc, uint nth, float* perc)
{
        CHK_DB(db);
        dbe_file_fileusageinfo(db->db_dbfile, maxsize, currsize, totalperc, nth, perc);
}

#endif /* SS_NOLOCKING */


/*##**********************************************************************\
 *
 *              dbe_db_setcheckpointcallback
 *
 * Sets callback function that is called at the start of atomic section
 * of checkpoint create.
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_setcheckpointcallback(
        dbe_db_t* db,
        su_ret_t (*cpcallbackfun)(rs_sysi_t* cd))
{
        CHK_DB(db);

        db->db_checkpointcallback = cpcallbackfun;
}

/*##**********************************************************************\
 *
 *              dbe_db_createcheckpoint
 *
 * Creates checkpoint
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      db - in out, use
 *              Database object.
 *
 * Return value :
 *      DBE_RC_SUCC
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_db_createcheckpoint(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool displayprogress,
        bool splitlog)
{
        dbe_ret_t rc;

        ss_dprintf_1(("dbe_db_createcheckpoint\n"));

        if (db->db_force_checkpoint) {
            db->db_force_checkpoint = FALSE;
            dbe_db_setchanged(db, NULL);
        }
        if (!db->db_changed) {
            return (DBE_RC_SUCC);
        }
        if (db->db_readonly) {
            ss_dprintf_2(("dbe_db_createcheckpoint:DBE_ERR_DBREADONLY\n"));
            return(DBE_ERR_DBREADONLY);
        }
        rc = db_createcp(cd, db, DBE_BLOCK_CPRECORD, displayprogress, splitlog);
        return (rc);
}

/*##**********************************************************************\
 *
 *              dbe_db_checkcreatecheckpoint
 *
 * Checks is checkpoint creation is possible.
 *
 * Parameters :
 *
 *      db - in
 *              Database object.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_checkcreatecheckpoint(dbe_db_t* db)
{
        dbe_ret_t rc = DBE_RC_SUCC;

        ss_dprintf_1(("dbe_db_checkcreatecheckpoint\n"));

        if (db->db_force_checkpoint) {
            db->db_force_checkpoint = FALSE;
            dbe_db_setchanged(db, NULL);
        }
        if (!db->db_changed) {
            return (DBE_RC_SUCC);
        }
        if (db->db_fatalerrcode != SU_SUCCESS) {
            return(db->db_fatalerrcode);
        }
        if (db->db_readonly) {
            return(DBE_ERR_DBREADONLY);
        }

        SsSemEnter(db->db_sem);

        if (db->db_backup != NULL) {
            rc = DBE_ERR_BACKUPACT;
        } else if (db->db_ddopactivecnt > 0) {
            rc = DBE_ERR_DDOPACT;
        }
        SsSemExit(db->db_sem);

        return(rc);
}

#if 0 /* Removed by Pete 1996-07-05 */

/*##**********************************************************************\
 *
 *              dbe_db_createsnapshot
 *
 * Creates snapshot
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      db - in out, use
 *              Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_db_createsnapshot(rs_sysi_t* cd, dbe_db_t* db)
{
        dbe_ret_t rc;

        rc = db_createcp(cd, db, DBE_BLOCK_SSRECORD, FALSE, FALSE);
        return (rc);
}

#endif /* 0 */

#if defined(DBE_MTFLUSH)

static void db_mtflushwakeupfun(void* msg)
{
        ss_dassert(msg != NULL);
        SsMesSend(msg);
}

static void db_flusheventresetfun(void* msg)
{
        ss_dassert(msg != NULL);
        SsMesReset(msg);
}
#endif /* DBE_MTFLUSH */

#if defined(SS_MME) && !defined(SS_MYSQL)
/*#***********************************************************************\
 *
 *              db_createcp_startmme
 *
 * Starts concurrent creation of MME snapshot/checkpoint
 *
 * Parameters :
 *
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t db_createcp_startmme(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_cpnum_t cpnum,
        su_daddr_t* mmepagedir)
{
        mme_storage_t* storage;
        su_ret_t rc = DBE_RC_SUCC;

        ss_dassert(db != NULL);

        db->db_mmecp_pageaddrdaddr = SU_DADDR_NULL;

        if ((storage = dbe_mme_getstorage(cd, db->db_mme)) != NULL
            && (!dbefile_diskless || db->db_backupmme_cb != NULL)) {

            ss_dassert(cd != NULL);

#if defined(DBE_MTFLUSH)
            /* start a checkpoint */
            rc = mme_storage_startcheckpoint(
                    cd,
                    storage,
                    cpnum,
                    mmepagedir);
            /* XXX. What if an error happens? */
            if (rc == SU_SUCCESS) {
                if (dbefile_diskless) {
                    db->db_mmecp_pagedata =
                        SsMemAlloc(dbe_cache_getblocksize(
                                           db->db_go->go_dbfile->f_indexfile->fd_cache));
                }
                db->db_mmecp_page = NULL;
                db->db_mmecp_pageaddrpage = NULL;
                db->db_mmecp_state = MME_CP_STARTED;
                db->db_mmecp_npages = 0;

                /* Create a flushbatch object */
                db->db_mmecp_flushbatch =
                    dbe_iomgr_flushbatch_init(
                        db->db_go->go_iomgr,
                        db->db_flushwakeupfp,
                        db->db_flushwakeupctx,
                        db->db_flushbatchwakeupfp,
                        db->db_flushbatchwakeupctx);
            }
#else
        rc = DBE_ERR_FAILED;
#endif
        } else {
            db->db_mmecp_state = MME_CP_DONE;
            *mmepagedir = SU_DADDR_NULL;
        }
        return (rc);
}

void dbe_db_resetflushbatch(dbe_db_t* db)
{
        db->db_mmecp_flushbatch = NULL;
}

#endif /* defined(SS_MME) && !defined(SS_MYSQL) */

/*##**********************************************************************\
 *
 *              dbe_db_hsbg2_sendandwaitdurablemark
 *
 * If this is primary servers sends durable mark and waits durable ack
 * for it. Used in primary shutdown to clear waiting requests before final
 * checkpoint.
 *
 * Parameters :
 *
 *              db -
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
void dbe_db_hsbg2_sendandwaitdurablemark(dbe_db_t* db)
{
        dbe_catchup_logpos_t dummy_logpos;
        ss_pprintf_1(("dbe_db_hsbg2_sendandwaitdurablemark\n"));
        CHK_DB(db);

        if (dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY) {
            dbe_ret_t rc;
            ss_debug(dbe_catchup_logpos_t curlogpos;)
            ss_debug(dbe_catchup_logpos_t cplpid;)

            ss_pprintf_2(("dbe_db_hsbg2_sendandwaitdurablemark:db->db_hsbg2mode == DBE_HSB_PRIMARY, put durable mark to log\n"));

#ifndef SS_MYSQL
            SS_PMON_SET(SS_PMON_HSB_CPWAITMES, 1);
#endif /* !SS_MYSQL */

            SsMesReset(db->db_hsb_durable_mes);
            rs_sysi_sethsbwaitmes(db->db_hsb_durable_cd, db->db_hsb_durable_mes);

            ss_debug(curlogpos = dbe_log_getlogpos(db->db_dbfile->f_log));

            DBE_CATCHUP_LOGPOS_SET_NULL(dummy_logpos);

            rc = dbe_log_put_durable(
                    db->db_dbfile->f_log,
                    db->db_hsb_durable_cd,
                    dummy_logpos);

            ss_rc_dassert(rc == DBE_RC_SUCC, rc);

            if (rc == DBE_RC_SUCC && dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY) {
                SsMesRetT mes_rc;

                ss_pprintf_2(("dbe_db_hsbg2_sendandwaitdurablemark:start to wait ack for durable mark\n"));
                mes_rc = SsMesRequest(db->db_hsb_durable_mes, 20*60*1000);
                ss_rc_dassert(mes_rc == SSMES_RC_SUCC, mes_rc);
                ss_pprintf_2(("dbe_db_hsbg2_sendandwaitdurablemark:got mes\n"));

                ss_debug(cplpid = dbe_hsbg2_getcplpid(db->db_hsbg2svc));
                ss_dprintf_3(("dbe_db_hsbg2_sendandwaitdurablemark:curlogpos(%d,%s,%d,%d,%d), cplpid(%d,%s,%d,%d,%d)\n", LOGPOS_DSDDD(curlogpos), LOGPOS_DSDDD(cplpid)));
                ss_dassert(dbe_catchup_logpos_cmp(curlogpos, cplpid) <= 0);
            } else {
                ss_pprintf_2(("dbe_db_hsbg2_sendandwaitdurablemark:log write failed, rc=%d\n", rc));
            }
#ifndef SS_MYSQL
            SS_PMON_SET(SS_PMON_HSB_CPWAITMES, 0);
#endif /* !SS_MYSQL */

        } else {
            ss_pprintf_2(("dbe_db_hsbg2_sendandwaitdurablemark:not primary, do not write durable mark\n"));
        }
}

/*#***********************************************************************\
 *
 *              db_createcp_start
 *
 * Starts concurrent creation of snapshot/checkpoint
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 *      type - in
 *          indicates whether we create a checkpoint or a snapshot:
 *              DBE_BLOCK_CPRECORD for checkpoint or
 *          DBE_BLOCK_SSRECORD for snapshot
 *
 *      splitlog - in
 *          TRUE forces log file split
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *      DBE_RC_WAITFLUSH when need to wait for multithreaded flushing
 *                      to complete
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t db_createcp_start(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_blocktype_t type,
        bool splitlog)
{
        dbe_ret_t rc;
        dbe_cpnum_t cpnum;
        su_daddr_t bonsairoot;
        su_daddr_t permroot;
        su_daddr_t mmiroot;
        SsTimeT cp_timestamp;
#if defined(DBE_MTFLUSH)
        bool anything_to_flush;
        su_daddr_t* flusharray;
        size_t flusharraysize;
#endif /* DBE_MTFLUSH */
#ifdef SS_MME
        su_daddr_t mmepagedir __attribute__ ((unused));
#endif
        su_profile_timer;

        ss_dprintf_1(("db_createcp_start\n"));
        CHK_DB(db);

        if (db->db_fatalerrcode != SU_SUCCESS) {
            ss_dprintf_2(("db_createcp_start:db->db_fatalerrcode=%d\n", db->db_fatalerrcode));
            return(db->db_fatalerrcode);
        }

        /* save param manager here */
        su_param_manager_save();

        if (dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY_UNCERTAIN) {
            ss_dprintf_2(("db_createcp_start:DBE_ERR_HSBPRIMARYUNCERTAIN\n"));
            return(DBE_ERR_HSBPRIMARYUNCERTAIN);
        }

        SS_PUSHNAME("db_createcp_start");

        FAKE_CODE_BLOCK(FAKE_DBE_CHECKPOINT_SPLITLOG, {
            ss_pprintf_1(("FAKE_DBE_CHECKPOINT_SPLITLOG\n"));
            splitlog = TRUE;
        });

        SsSemEnter(db->db_sem);

#ifndef SS_NOBACKUP
        if (db->db_backup != NULL) {
            SsSemExit(db->db_sem);
            SS_POPNAME;
            return(DBE_ERR_BACKUPACT);
        }
#endif
        if (db->db_ddopactivecnt > 0) {
            SsSemExit(db->db_sem);
            SS_POPNAME;
            return(DBE_ERR_DDOPACT);
        }
        if (db->db_cpactive) {
            SsSemExit(db->db_sem);
            SS_POPNAME;
            return(DBE_ERR_CPACT);
        }
        db->db_cpactive = TRUE;
        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 1);
        ss_pprintf_2(("db_createcp_start\n"));

        SsSemExit(db->db_sem);

        dbe_hsbg2_createcp_start(db->db_hsbg2svc, db, DBE_HSBCREATECP_BEGINATOMIC);

        cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);

        DBE_CPMGR_CRASHPOINT(14);

        dbe_log_setidlehsbdurable(db->db_dbfile->f_log, DBE_LOGFILE_IDLEHSBDURABLE_OFF);

        /* Wait for action-consistent state;
         * (ie. that all currently executing insert/update/delete
         * operations have completed
         */
        dbe_db_enteraction_exclusive(db, cd);

        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 2);
        su_profile_start;

        /* Merge cannot be active when checkpoint is created.
         */
        su_gate_enter_exclusive(db->db_mergesem);

        dbe_seq_entermutex(db->db_seq);

#ifdef SS_HSBG2
        ss_pprintf_2(("db_createcp_start:db->db_hsbg2mode=%d\n", dbe_db_gethsbg2mode(db)));
        if (dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY && !db->db_final_checkpoint) {

            ss_pprintf_2(("db_createcp_start:db->db_hsbg2mode == DBE_HSB_PRIMARY, put durable mark to log\n"));

            dbe_db_hsbg2_sendandwaitdurablemark(db);
        }
        dbe_hsbg2_createcp_start(db->db_hsbg2svc, db, DBE_HSBCREATECP_START);
#endif

#ifdef DBE_LOGORDERING_FIX
        if (db->db_dbfile->f_log) {
            dbe_log_lock(db->db_dbfile->f_log);
        }
#endif /* DBE_LOGORDERING_FIX */

        SsSemEnter(db->db_hsbsem);

        if (db->db_checkpointcallback != NULL) {
            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 3);
            rc = (*db->db_checkpointcallback)(cd);
            if (rc == SU_SUCCESS && db->db_readonly) {
                /* Callback did not notice error but during the
                 * call database was set to read only state.
                 */
                rc = DBE_ERR_DBREADONLY;
            } else {
                ss_rc_dassert(rc == SU_SUCCESS || rc == DBE_ERR_DBREADONLY, rc);
            }
        } else {
            rc = DBE_RC_SUCC;
        }

        if (rc == DBE_RC_SUCC) {
            /* Minor preparations that could be done concurrently with other
             * database operations (but are not done right now).
             */
            cp_timestamp = SsTime(NULL);
            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 4);
            rc = dbe_cpmgr_prepare(
                    db->db_cpmgr,
                    cpnum,
                    type,
                    cp_timestamp);
            ss_rc_dassert(rc == SU_SUCCESS, rc);
            su_timer_reset(&db->db_cp_timer);
            su_timer_start(&db->db_cp_timer);
            dbe_fildes_zeronbyteswritten(db->db_dbfile->f_indexfile);
        }
        if (rc != DBE_RC_SUCC) {
            ss_dprintf_2(("db_createcp_start:dbe_cpmgr_prepare failed\n"));
            db->db_cpactive = FALSE;
            dbe_hsbg2_createcp_start(db->db_hsbg2svc, db, DBE_HSBCREATECP_ENDATOMIC);
            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 0);
#ifdef DBE_LOGORDERING_FIX
            dbe_log_unlock(db->db_dbfile->f_log);
#endif /* DBE_LOGORDERING_FIX */
            SsSemExit(db->db_hsbsem);
            dbe_seq_exitmutex(db->db_seq);
            su_gate_exit(db->db_mergesem);
            dbe_db_exitaction(db, cd);
            su_profile_stop("db_createcp_start");
            dbe_log_setidlehsbdurable(db->db_dbfile->f_log, DBE_LOGFILE_IDLEHSBDURABLE_ON);
            SS_POPNAME;
            return(rc);
        }
        DBE_CPMGR_CRASHPOINT(15);

        ss_dassert(db->db_dbfile->f_indexfile->fd_cprec != NULL);

#if defined(SS_MME) && !defined(SS_MYSQL)
        /* Start MME checkpoint */
        if (db_createcp_startmme(cd, db, cpnum+1, &mmepagedir) == DBE_RC_SUCC) {
            /* Save the MME page directory address to the file header */
            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 5);
            dbe_header_setfirstmmeaddrpage(
                db->db_dbfile->f_indexfile->fd_dbheader,
                mmepagedir);
            dbe_cpmgr_setfirstmmeaddr(
                db->db_cpmgr,
                mmepagedir);
        }
#endif /* defined(SS_MME) && !defined(SS_MYSQL) */

        /* Flush all free list sequence allocations. */
        dbe_fl_seq_flushall(db->db_go->go_idxfd->fd_freelist);

        dbe_type_updateconst(db->db_go->go_ctr);

        /* Save counter status */
        dbe_counter_putinfotostartrec(
            db->db_go->go_ctr,
            dbe_cprec_getstartrec(db->db_dbfile->f_indexfile->fd_cprec));
        DBE_CPMGR_CRASHPOINT(16);

        mmiroot = 0;

        /* Get index root addresses */
        dbe_index_getrootaddrs(db->db_index, &bonsairoot, &permroot);

        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 6);

        /* and create checkpoint */
        dbe_cpmgr_createcp(
            db->db_cpmgr,
            db->db_go->go_trxbuf,
#ifdef DBE_REPLICATION
            db->db_go->go_rtrxbuf,
#else
            NULL,
#endif
            db->db_seq,
            bonsairoot,
            permroot,
            mmiroot);
        DBE_CPMGR_CRASHPOINT(17);

        /* increment checkpoint number */
        cpnum++;
        dbe_counter_setcpnum(db->db_go->go_ctr, cpnum);
        ss_dprintf_1(("db_createcp: new cpnum=%ld\n", cpnum));

        dbe_counter_setmergectr(db->db_go->go_ctr, db->db_go->go_nmergewrites);

        /* Put checkpoint mark to log
         */
        if (db->db_dbfile->f_log != NULL) {
            dbe_logrectype_t logrectype = DBE_LOGREC_CHECKPOINT_NEW;

            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 7);

            if (type == DBE_BLOCK_CPRECORD) {
                logrectype = DBE_LOGREC_CHECKPOINT_NEW;
            } else if (type == DBE_BLOCK_SSRECORD) {
                logrectype = DBE_LOGREC_SNAPSHOT_NEW;
            } else {
                ss_rc_error(type);
            }
#ifndef SS_NOLOGGING
            rc = dbe_log_putcpmark(
                    db->db_dbfile->f_log,
                    logrectype,
                    cpnum - 1,
                    cp_timestamp,
                    db->db_final_checkpoint,
                    &splitlog);
            if (db->db_final_checkpoint) {
                dbe_mme_setreadonly(db->db_mme);
                db->db_final_checkpoint = 2;
            }
            db->db_logsplit = splitlog;
            SS_NOTUSED(rc);
            FAKE_IF(FAKE_DBE_CRASHAFTERCPMARK) {
                su_rc_fatal_error(DBE_FATAL_CRASHAFTERCPMARK);
            }
#endif /* SS_NOLOGGING */
        } else {
            db->db_logsplit = TRUE;
        }
        db->db_go->go_nlogwrites = 0;

        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 8);

#if defined(DBE_MTFLUSH)
        ss_dassert(db->db_flushwakeupfp != NULL)
        anything_to_flush =
            dbe_iomgr_flushallcaches_init(
                db->db_go->go_iomgr,
                &flusharray,
                &flusharraysize);
#endif /* DBE_MTFLUSH */

        SsSemExit(db->db_hsbsem);

#ifdef DBE_LOGORDERING_FIX
        if (db->db_dbfile->f_log) {
            dbe_log_unlock(db->db_dbfile->f_log);
        }
#endif /* DBE_LOGORDERING_FIX */

        dbe_seq_exitmutex(db->db_seq);

        dbe_hsbg2_createcp_start(db->db_hsbg2svc, db, DBE_HSBCREATECP_ENDATOMIC);

        su_gate_exit(db->db_mergesem);

        SS_PMON_ADD(SS_PMON_CHECKPOINTCOUNT);

        /* Re-allow database operations
         */
        dbe_db_exitaction(db, cd);
        su_profile_stop("db_createcp_start");
        dbe_log_setidlehsbdurable(db->db_dbfile->f_log, DBE_LOGFILE_IDLEHSBDURABLE_ON);

#if defined(DBE_MTFLUSH)
        if (anything_to_flush) {
            SS_PMON_SET(SS_PMON_CHECKPOINTACT, 9);
            dbe_iomgr_flushallcaches_exec(
                db->db_go->go_iomgr,
                db->db_flushwakeupfp,
                db->db_flushwakeupctx,
                flusharray,
                flusharraysize,
                DBE_INFO_CHECKPOINT);
            SS_POPNAME;
            return (DBE_RC_WAITFLUSH);
        }
#endif /* DBE_MTFLUSH */

        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 10);
        SS_POPNAME;
        return (DBE_RC_SUCC);
}

/*##**********************************************************************\
 *
 *              dbe_db_createcheckpoint_start
 *
 * Creates checkpoint
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      db - in out, use
 *              Database object.
 *
 *      splitlog - in
 *          TRUE forces the log file to split
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_db_createcheckpoint_start(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool splitlog)
{
        dbe_ret_t rc;

        if (db->db_force_checkpoint) {
            db->db_force_checkpoint = FALSE;
            dbe_db_setchanged(db, NULL);
        }
        if (!db->db_changed) {
            return (DBE_RC_SUCC);
        }

        if (db->db_readonly) {
            return(DBE_ERR_DBREADONLY);
        }

        ss_dprintf_1(("dbe_db_createcheckpoint_start\n"));
        SS_PUSHNAME("dbe_db_createcheckpoint_start");

        rc = db_createcp_start(cd, db, DBE_BLOCK_CPRECORD, splitlog);

        SS_POPNAME;

        return (rc);
}

bool dbe_db_last_checkpoint_split_log(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_logsplit);
}


#if 0 /* Removed by Pete 1996-07-05 */
/*##**********************************************************************\
 *
 *              dbe_db_createsnapshot_start
 *
 * Starts concurrent snapshot creation
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      db - in out, use
 *              Database object.
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_db_createsnapshot_start(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        dbe_ret_t rc;

        rc = db_createcp_start(cd, db, DBE_BLOCK_SSRECORD, FALSE);
        return (rc);
}
#endif /* 0 */

#if defined(SS_MME) && !defined(SS_MYSQL)
/*##**********************************************************************\
 *
 *              db_createcp_mmestep
 *
 * Runs one step of concurrent MME checkpoint/snapshot creation
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *      DBE_RC_CONT when operation needs to be repeated or
 *      DBE_RC_END when this phase has ended (and the next function
 *      to run is dbe_db_createcp_end()).
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t db_createcp_mmestep(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        dbe_iomgr_t* iomgr = NULL;
        dbe_cache_t* cache = NULL;
        ss_debug(static int ndatapages;)
        ss_debug(static int naddrpages;)

        SS_PUSHNAME("db_createcp_mmestep");

        ss_dassert(db != NULL);

#if defined(DBE_MTFLUSH)

        iomgr = db->db_go->go_iomgr;
        cache = db->db_go->go_dbfile->f_indexfile->fd_cache;

        switch(db->db_mmecp_state) {
            case MME_CP_STARTED:
                ss_debug(ndatapages = 0;)
                ss_debug(naddrpages = 0;)
                db->db_mmecp_npages = 0;
            /* FALLTHROUGH */
            case MME_CP_GETPAGE:
                ss_bassert(cd != NULL);
                /* MME data page */
                if (!dbefile_diskless) {
                    if (db->db_mmecp_page == NULL) {
                        db->db_mmecp_page = dbe_cache_alloc(
                                cache,
                                &db->db_mmecp_pagedata);
                        db->db_mmecp_daddr = SU_DADDR_NULL;
                    }
                    /* Page for MME-page addresses */
                    if (db->db_mmecp_pageaddrpage == NULL) {
                        db->db_mmecp_pageaddrpage = dbe_cache_alloc(
                                cache,
                                &db->db_mmecp_pageaddrdata);
                        db->db_mmecp_pageaddrdaddr = SU_DADDR_NULL;
                    }
                }

                /* MME fills the page contents */
                ss_dprintf_1(("db_createcp_mmestep(): Get page forcheckpoint\n"));
                if (dbefile_diskless) {
                    if (db->db_backupmme_cb != NULL) {
                        db->db_mmecp_rc = mme_storage_getpageforcheckpoint(
                                cd,
                                dbe_mme_getstorage(cd, db->db_mme),
                                dbe_cache_getblocksize(cache),
                                db->db_mmecp_pagedata,
                                NULL,
                                NULL,
                                NULL);
                        if (db->db_mmecp_rc == SU_SUCCESS) {
                            (*db->db_backupmme_cb)(
                                    db->db_backupmmectx,
                                    DBE_BACKUPFILE_MME,
                                    db->db_mmecp_pagedata,
                                    dbe_cache_getblocksize(cache));
                            db->db_mmecp_state = MME_CP_GETPAGE;
                        } else if (db->db_mmecp_rc == MME_RC_END) {
                            db->db_mmecp_state = MME_CP_DONE;
                            db->db_mmecp_rc = DBE_RC_END;
                        }
                    } else {
                        ss_error;
                        db->db_mmecp_state = MME_CP_DONE;
                        db->db_mmecp_rc = DBE_RC_END;
                    }

                    if (db->db_mmecp_rc == MME_RC_CONT
                        || db->db_mmecp_rc == SU_SUCCESS) {
                        db->db_mmecp_rc = DBE_RC_CONT;
                    }
                    SS_POPNAME;
                    return db->db_mmecp_rc;
                }
                db->db_mmecp_rc = mme_storage_getpageforcheckpoint(
                        cd,
                        dbe_mme_getstorage(cd, db->db_mme),
                        dbe_cache_getblocksize(cache),
                        db->db_mmecp_pagedata,
                        &db->db_mmecp_daddr,
                        db->db_mmecp_pageaddrdata,
                        &db->db_mmecp_pageaddrdaddr);
                if (db->db_mmecp_rc == MME_RC_END) {
                    db->db_mmecp_rc = SU_SUCCESS;
                } else if (db->db_mmecp_rc == SU_SUCCESS) {
                    db->db_mmecp_rc = MME_RC_CONT;
                }
                /* FALLTHROUGH */
            case MME_CP_FLUSH_DATAPAGE:
                /* MME data page */
                if (db->db_mmecp_daddr != SU_DADDR_NULL) {
                    ss_dprintf_1(("db_createcp_mmestep():Flush addr: %d\n", db->db_mmecp_daddr));
                    if (!dbe_iomgr_addtoflushbatch(
                            iomgr,
                            db->db_mmecp_flushbatch,
                            db->db_mmecp_page,
                            db->db_mmecp_daddr,
                            DBE_INFO_CHECKPOINT)) {
                        /* There is no room left, wait for it */
                        db->db_mmecp_state = MME_CP_FLUSH_DATAPAGE;
                        SS_POPNAME;
                        return (DBE_RC_WAITFLUSHBATCH);
                    }
                    ss_debug(ndatapages++;)
                    (db->db_mmecp_npages)++;
                    db->db_mmecp_page = NULL;
                }

            /* FALLTHROUGH */
            case MME_CP_FLUSH_ADDRESSPAGE:
                /* Page for MME-page addresses */
                if (db->db_mmecp_pageaddrdaddr != SU_DADDR_NULL) {
                    if (!dbe_iomgr_addtoflushbatch(
                            iomgr,
                            db->db_mmecp_flushbatch,
                            db->db_mmecp_pageaddrpage,
                            db->db_mmecp_pageaddrdaddr,
                            DBE_INFO_CHECKPOINT)) {
                        /* There is no room left, wait for it */
                        db->db_mmecp_state = MME_CP_FLUSH_ADDRESSPAGE;
                        SS_POPNAME;
                        return (DBE_RC_WAITFLUSHBATCH);
                    }
                    ss_debug(naddrpages++;)
                    (db->db_mmecp_npages)++;
                    db->db_mmecp_pageaddrpage = NULL;
                }
                break;
            case MME_CP_COMPLETED:
                FAKE_CODE_BLOCK(
                    FAKE_DBE_FASTIOMGR,
                    ss_debug(if (ndatapages + naddrpages > 0)) {
                        SsPrintf("Sleeping 20 seconds to make sure I/O manager has completed\n");
                        SsThrSleep(20000);
                    }
                );
                if (db->db_mmecp_flushbatch != NULL
                &&  dbe_iomgr_flushbatch_nleft(
                        iomgr, db->db_mmecp_flushbatch) > 0)
                {
                    /* IO-manager is still flushing. */
                    SS_POPNAME;
                    return(DBE_RC_WAITFLUSH);
                } else {
                    /* reset the event to avoid later false wakeup */
                    ss_dassert(db->db_flusheventresetfp
                               != (void(*)(void*))NULL);
                    (*db->db_flusheventresetfp)(
                            db->db_flusheventresetctx);
                    /* This was a temporary assertion to prove bugzilla
                       bug 278
                    if (ndatapages + naddrpages > 0) {
                        ss_derror;
                    }
                    */
                }

            /* FALLTHROUGH */
            case MME_CP_INIT: /* hasn't even been started */
            case MME_CP_DONE:
                ss_dprintf_1(("db_createcp_mmestep():Wrote ndatapages: %d\n", ndatapages));
                ss_dprintf_1(("db_createcp_mmestep():Wrote naddrpages: %d\n", naddrpages));
                db->db_mmecp_state = MME_CP_DONE;
                SS_POPNAME;
                return(DBE_RC_END);
            default:
                ss_rc_error(db->db_mmecp_state);
                break;
        }
        ss_rc_bassert(db->db_mmecp_rc == SU_SUCCESS || db->db_mmecp_rc == MME_RC_CONT, db->db_mmecp_rc);
        if (db->db_mmecp_rc != MME_RC_CONT) {
            /* All the pages gone through */
            if (db->db_mmecp_page != NULL &&
                    db->db_mmecp_daddr == SU_DADDR_NULL) {
                dbe_cache_free(cache, db->db_mmecp_page);
            }
            if (db->db_mmecp_pageaddrpage != NULL &&
                    db->db_mmecp_pageaddrdaddr == SU_DADDR_NULL) {
                dbe_cache_free(cache, db->db_mmecp_pageaddrpage);
            }
            db->db_mmecp_page = NULL;
            db->db_mmecp_pageaddrpage = NULL;
            db->db_mmecp_state = MME_CP_COMPLETED;
        } else {
            db->db_mmecp_state = MME_CP_GETPAGE;
        }
#else
        db->db_mmecp_state = MME_CP_DONE;
        SS_POPNAME;
        return(DBE_RC_END);
#endif /* DBE_MTFLUSH*/

        SS_POPNAME;
        return(DBE_RC_CONT);
}

#ifdef SS_DEBUG
static char mme_reachctx[] = "MME reach";
#else
#define mme_reachctx NULL
#endif

/*##**********************************************************************\
 *
 *              db_addpagetomme
 *
 *  Adds an page (and rows) from checkpoint to the MME engine.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static su_ret_t db_addpagetomme(
        rs_sysi_t*  cd,
        dbe_db_t*   db,
        dbe_mme_t*  mme,
        mme_storage_t* storage,
        su_daddr_t  daddr,
        ss_byte_t*  mmepagedata,
        size_t      page_size,
        rs_relh_t*  (*getrelhfunbyname)(
                        void* ctx,
                        rs_entname_t* relname,
                        void* p_priv),
        rs_relh_t* (*getrelhfunbyid_trxinfo)(
                void* ctx,
                ulong relid,
                dbe_trxinfo_t* trxinfo,
                dbe_trxid_t readtrxid),
        void*       recovctx)
{
        mme_pagescan_t pagescan;
        mme_page_t* mme_page;
        mme_rval_t* rval;
        dbe_trxid_t trxid;
        dbe_trxid_t stmtid;
        rs_entname_t* relentname = NULL;
        rs_relh_t* relh;
        ss_uint4_t relid;
        bool tentative;
        bool succp;
        ss_debug(long npages = 0;)

        /* Start page scan */
        mme_page = mme_storage_initreadpage(
                    cd,
                    storage,
                    daddr,
                    mmepagedata,
                    page_size,
                    &pagescan);
        ss_dassert(mme_page != NULL);
        ss_dassert(daddr == mme_page_getdiskaddr(mme_page));
        ss_dprintf_1(("\tdbe_mme_addpage: daddr: %ld relid:%ld\n", mme_page_getdiskaddr(mme_page), mme_page_getrelid(mme_page)));

        /* Get relation handle */
        relid = mme_page_getrelid(mme_page);

        succp = rs_rbuf_relnamebyid(cd, db->db_rbuf, relid, &relentname);
        if (succp) {
            relh = getrelhfunbyname(recovctx, relentname, NULL);
            if (relh == NULL) {
                su_informative_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_RELNAMENOTFOUND_S,
                        rs_entname_getname(relentname));
            }
        } else {
            relh = NULL;
        }
        rval = mme_page_scanrval(
                        cd,
                        mme_page,
                        &pagescan,
                        &tentative,
                        &trxid,
                        &stmtid);
        ss_bassert(rval != NULL);
        if (rval != NULL) {
            if (relh == NULL) {
                if (tentative) {
                    dbe_trxinfo_t* trxinfo;
                    trxinfo = dbe_trxbuf_gettrxinfo(db->db_go->go_trxbuf,
                                                    trxid);
                    ss_dassert(trxinfo != NULL);
                    relh = getrelhfunbyid_trxinfo(recovctx,
                                                  relid,
                                                  trxinfo,
                                                  stmtid);
                    ss_dassert(relh != NULL);
                } else {
                    /* temporarily NULL relh allowed to circumvent
                       [bug 49] (recorded in bugzilla)
                    */
                }
#if 0 /* pete removed 2002-12-09, see above */
                if (relh == NULL) {
                    su_informative_exit(
                            __FILE__,
                            __LINE__,
                            DBE_ERR_RELIDNOTFOUND_D,
                            relid);
                }
#endif /* 0 */
            }
            if (relh != NULL) {
#ifdef MME_CP_FIX
                if (!tentative) {
                    trxid = DBE_TRXID_NULL;
                    stmtid = DBE_TRXID_NULL;
                }
                /* Place the page correctly into the storage. */
                dbe_mme_recovplacepage(
                        cd,
                        mme,
                        relh,
                        mme_page,
                        rval,
                        trxid,
                        stmtid);
#endif
                /* Scan the rows from the page and feed them to the MME */
                do {
                    if (!tentative) {
                        trxid = DBE_TRXID_NULL;
                        stmtid = DBE_TRXID_NULL;
                    }
                    ss_dprintf_2(("\t\tdbe_mme_addpage:Recov insert: rval: %p relid:%ld daddr: %ld\n", rval, relid, daddr));
                    dbe_mme_recovinsert(cd,
                                        mme,
                                        db->db_go->go_trxbuf,
                                        relh,
                                        mme_page,
                                        rval,
                                        trxid,
                                        stmtid);
                    ss_debug(npages++);
#ifndef SS_FORCE_NONVERSCOUNTED2SSMEMALLOC
                    mme_setmemctxtocd(cd, mme);
#endif
                    rval = mme_page_scanrval(
                            cd,
                            mme_page,
                            &pagescan,
                            &tentative,
                            &trxid,
                            &stmtid);
                } while (rval != NULL);
            }
        }
        if (relh == NULL) {
            ss_dprintf_1((
                    "create table t;insert into t ..;drop table t;commit\n"
                    "sequence bug (=garbage row(s)) detected. freeing page.\n"));
            mme_storage_remove_page(cd, storage, mme_page);
        } else {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
            if (relentname != NULL) {
                rs_entname_done(relentname);
            }
        }
        ss_dprintf_1(("\t\tdbe_mme_addpage:Added %ld rows\n", npages));

        return(SU_SUCCESS);
}


void dbe_db_hsbg2_mme_newdb(
        dbe_db_t*   db)
{
        /* if (!dbefile_diskless && db->db_hsbg2configured) { */
        if (db->db_hsbg2configured) {
            dbe_mme_beginrecov(db->db_mme);
        }
}


/*##**********************************************************************\
 *
 *              dbe_db_loadmme
 *
 *  Loads the MME tables from the database file.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_loadmme(
        rs_sysi_t*  cd,
        dbe_db_t*   db,
        rs_relh_t* (*getrelhfunbyname)(
                void* ctx,
                rs_entname_t* relname,
                void* p_priv),
        rs_relh_t* (*getrelhfunbyid_trxinfo)(
                void* ctx,
                ulong relid,
                dbe_trxinfo_t* trxinfo,
                dbe_trxid_t readtrxid),
        void* recovctx)
{
        dbe_cacheslot_t* mmeslot;
        dbe_cacheslot_t* slot;
        dbe_cache_t* cache;
        dbe_iomgr_t* iomgr;
        dbe_cpnum_t cpnum;
        mme_storage_t* storage;
        su_daddr_t* daddr_array_1;
        su_daddr_t* daddr_array_2;
        su_daddr_t* prefetchp;
        su_daddr_t* reachp;
        su_daddr_t daddrpage;
        su_daddr_t daddr;
        su_ret_t rc;
        ss_byte_t* addrpagedata;
        ss_byte_t* mmepagedata;
        bool endloop;
        bool firstbatch;
        uint pagesize;
        size_t array_size;
        size_t array_size_orig;
        size_t* nprefetchp;
        size_t* nreachp;
        size_t nread_1, nread_2;
        size_t npos;
        int i;
        enum {
            FIRSTBATCH,
            EVENBATCH,
            ODDBATCH
        } state;
        int npages = 0;
        int naddrpages = 0;

        SS_PUSHNAME("db_load_mme");
        if (db->db_mme == NULL) {
            /* MME not available at this binary.
             */
            SS_POPNAME;
            return(SU_SUCCESS);
        }

        iomgr = db->db_go->go_iomgr;
        cache = db->db_go->go_dbfile->f_indexfile->fd_cache;

        pagesize = dbe_cache_getblocksize(cache);

        /* Begin recovery */
        ss_dprintf_1(("dbe_mme_beginrecov:loadmme\n"));
        dbe_mme_beginrecov(db->db_mme);
        storage = dbe_mme_getstorage(cd, db->db_mme);

        /* Get the start address of the "MME page directory" page
           from the header */
        daddrpage = dbe_header_getfirstmmeaddrpage(
                            db->db_dbfile->f_indexfile->fd_dbheader);
        /* zero means an old database version */
        if (daddrpage == SU_DADDR_NULL || daddrpage == 0) {
            /* Empty database, I guess... */
            ss_bprintf_1(("db_load_mme: No MME pages in the database.\n"));
#if 0
            if (db->db_dbstate != DBSTATE_CRASHED) {
#ifdef SS_HSBG2
                if (!ss_migratehsbg2 && !db->db_hsbg2configured) {
                    ss_dprintf_1(("dbe_mme_endrecov:dbe_load_mme:%d,%d\n",
                        ss_migratehsbg2, db->db_hsbg2configured));
                    dbe_mme_endrecov(db->db_mme);
                }
#else /* SS_HSBG2 */
                dbe_mme_endrecov(db->db_mme);
#endif /* SS_HSBG2 */
            }
#endif
            rc = SU_SUCCESS;
            goto no_disk_pages;
        }
        /* We  don't want to use the whole cache for MME-pages */
        /* XXX - Fix the array_size! */
        array_size = (int) (0.5 * dbe_cache_getnslot(cache));
        /* Make it even */
        array_size % 2 ? array_size++ : 1;
        array_size_orig = array_size; /* save the size */

        daddr_array_1 = SsMemAlloc(array_size * sizeof(su_daddr_t));
        daddr_array_2 = SsMemAlloc((array_size/2) * sizeof(su_daddr_t));

        while (daddrpage != SU_DADDR_NULL) {

            ss_dprintf_1(("db_load_mme:Reach Address page: %d\n", daddrpage));
            naddrpages++;
            /* Get addresses */
            slot = dbe_iomgr_reach(
                    iomgr,
                    daddrpage,
                    DBE_CACHE_READONLY,
                    0,
                    (char**)&addrpagedata,
                    mme_reachctx);
            ss_bassert(slot != NULL);
            daddr = daddrpage;
            daddrpage = SU_DADDR_NULL;

            /* Extract the checkpoint number of the page */
            cpnum = mme_storage_getcpnumfromdirpage(addrpagedata);

            /* Get the addresses of the MME pages
               from the MME page directory page. */
            npos = 0;
            endloop = FALSE;
            rc = MME_RC_CONT;
            state = FIRSTBATCH;
            array_size = array_size_orig; /* restore the size */
            while (!endloop) {

                switch (state) {
                    case FIRSTBATCH:
                        firstbatch = TRUE;
                        prefetchp = daddr_array_1;
                        nprefetchp = &nread_1;
                        reachp = daddr_array_1;
                        nreachp = &nread_1;
                        state = EVENBATCH;
                        break;
                    case EVENBATCH:
                        prefetchp = daddr_array_1;
                        nprefetchp = &nread_1;
                        reachp = daddr_array_2;
                        nreachp = &nread_2;
                        state = ODDBATCH;
                        break;
                    case ODDBATCH:
                        prefetchp = daddr_array_2;
                        nprefetchp = &nread_2;
                        reachp = daddr_array_1;
                        nreachp = &nread_1;
                        state = EVENBATCH;
                        break;
                }

                if (rc == MME_RC_CONT) {
                    ss_dassert(cd != NULL);
                    /* MME extracts the addresses from the page */
                    rc = mme_storage_getaddressesfromdirpage(
                            cd,
                            dbe_mme_getstorage(cd, db->db_mme),
                            daddr,
                            addrpagedata,
                            pagesize,
                            prefetchp,
                            array_size,
                            nprefetchp,
                            &npos,
                            &daddrpage);
                    ss_dassert(*nprefetchp <= array_size);
                    ss_dprintf_1(("db_load_mme:mme_storage_getaddressesfromdirpage: %d npos: %d\n", rc, npos));
                    ss_dprintf_1(("\tdb_load_mme:Read %d pages\n", *nprefetchp));

                    /* Add prefetch requests for the pages */
                    if (*nprefetchp > 0) {
                        dbe_iomgr_prefetch(
                            iomgr,
                            prefetchp,
                            *nprefetchp,
                            0);
                    }else {
                        ss_rc_bassert(rc != MME_RC_CONT, rc);
                    }

                    ss_debug({
                            int ii = 0;
                            for (ii=0; ii<*nprefetchp; ii++) {
                                ss_dprintf_1(("\tdb_load_mme:Prefetch %d\n", prefetchp[ii]));
                            }
                        })
                    /* Prefetch also the next directory page */
                    if (daddrpage != SU_DADDR_NULL) {
                        ss_dassert(rc == SU_SUCCESS);
                        dbe_iomgr_prefetch(
                            iomgr,
                            &daddrpage,
                            1,
                            0);
                    }

                    /* Divide first batch to two */
                    if (firstbatch) {
                        if (rc == MME_RC_CONT) {
                            int split;
                            array_size /= 2;
                            split = array_size;
                            nread_2 = *nprefetchp-split;
                            memcpy(daddr_array_2,
                                &daddr_array_1[split],
                                nread_2*sizeof(su_daddr_t));
                            nread_1 = split;
                        } else {
                            endloop = TRUE;
                        }
                        firstbatch = FALSE;
                    }
                } else {
                    endloop = TRUE;
                }

                /* Read the pages */
                for (i=0; i<*nreachp; i++) {
                    npages++;
                    ss_dprintf_1(("db_load_mme:Reach page from address: %d\n", reachp[i]));

                    if ((npages % 100) == 0) {
                        ui_msg_message_status(DBE_MSG_LOADING_MME_D, npages);
                    }
                    mmeslot = dbe_iomgr_reach(
                                iomgr,
                                reachp[i],
                                DBE_CACHE_READONLY,
                                0,
                                &mmepagedata,
                                mme_reachctx);
                    ss_bassert(mmeslot != NULL);

                    /* Give the page to MME */
                    db_addpagetomme(
                        cd,
                        db,
                        db->db_mme,
                        storage,
                        reachp[i],
                        (ss_byte_t*)mmepagedata,
                        pagesize,
                        getrelhfunbyname,
                        getrelhfunbyid_trxinfo,
                        recovctx);

                    /* Release it back to the cache */
                    dbe_iomgr_release(
                            iomgr,
                            mmeslot,
                            DBE_CACHE_CLEANLASTUSE,
                            NULL);
                }
            }

            dbe_iomgr_release(
                    iomgr,
                    slot,
                    DBE_CACHE_CLEANLASTUSE,
                    NULL);

            if (db->db_dbstate == DBSTATE_CRASHED) {
                /* Directory page can be freed, because the original freeing
                   done during checkpoint creation is undone when server
                   is reverted back to latest chackpoint
                */
                ss_dprintf_1(("db_load_mme:Free %ld cpnum %ld\n",
                              (long)daddr, (long)cpnum));
                dbe_db_free_n_pages(db, 1, &daddr, cpnum, TRUE);
            }
            /* else
               must not be freed, because done at checkpoint creation
            */
        }
        if (npages > 0) {
            ui_msg_message(DBE_MSG_LOADING_MME_FIN_D, npages+naddrpages);
        }
        ss_dprintf_1(("db_load_mme:Read %d addresspages\n", naddrpages));
        ss_dprintf_1(("db_load_mme:Read %d datapages\n", npages));

        SsMemFree(daddr_array_1);
        SsMemFree(daddr_array_2);

 no_disk_pages:
        if (db_disklessmmepages != NULL) {
            ss_dprintf_2(("db_load_mme: handling diskless pages\n"));
            while (NULL !=
                   (mmepagedata = su_list_removefirst(db_disklessmmepages)))
            {
                db_addpagetomme(
                        cd,
                        db,
                        db->db_mme,
                        storage,
                        SU_DADDR_NULL,
                        mmepagedata,
                        pagesize,
                        getrelhfunbyname,
                        getrelhfunbyid_trxinfo,
                        recovctx);
                DB_DISKLESSMMEPAGEFREE(db_disklessmmememctx, mmepagedata);
            }
            su_list_done(db_disklessmmepages);
            db_disklessmmepages = NULL;
            db_disklessmmememctx = NULL;
        } else {
            ss_dprintf_2(("db_load_mme: no diskless pages\n"));
        }

        /* End recovery */
        if (db->db_dbstate != DBSTATE_CRASHED) {

#ifdef SS_HSBG2
            if (!ss_migratehsbg2 && !db->db_hsbg2configured) {
                ss_dprintf_1(("dbe_mme_endrecov:db_load_mme:END:%d,%d\n",
                    ss_migratehsbg2, db->db_hsbg2configured));
                dbe_mme_endrecov(db->db_mme);
            }

#else /* SS_HSBG2 */

            dbe_mme_endrecov(db->db_mme);

#endif /* SS_HSBG2 */

#ifdef MME_CP_FIX_XXX
            ss_debug(dbe_mme_check_integrity(db->db_mme));
#endif
        }

        SS_POPNAME;
        return(SU_SUCCESS);
}

#endif /* defined(SS_MME) && !defined(SS_MYSQL) */

/*##**********************************************************************\
 *
 *              dbe_db_createcp_step
 *
 * Runs one step of concurrent checkpoint/snampshot creation
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *      DBE_RC_CONT when operation needs to be repeated or
 *      DBE_RC_END when this phase has ended (and the next function
 *      to run is dbe_db_createcp_end()).
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_createcp_step(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool dislpayprogress __attribute__ ((unused)))
{
        dbe_ret_t ret;

        SS_NOTUSED(cd);

        if (db->db_force_checkpoint) {
            db->db_force_checkpoint = FALSE;
            dbe_db_setchanged(db, NULL);
        }
        if (!db->db_changed) {
            return (DBE_RC_END);
        }

        SS_PUSHNAME("dbe_db_createcp_step");

        /* Do concurrent cache flush !
         */
#if !defined(DBE_MTFLUSH)
        if (dbe_cpmgr_flushstep(db->db_cpmgr)) {
            SS_POPNAME;
            return (DBE_RC_CONT);
        }
#endif /* !DBE_MTFLUSH */

#if defined(SS_MME) && !defined(SS_MYSQL)
        ret = db_createcp_mmestep(cd, db);
        if (dislpayprogress) {
            if (db->db_mmecp_npages > 0 && (db->db_mmecp_npages % 100) == 0) {
                ui_msg_message_status(DBE_MSG_WRITING_MMEPAGES_D, db->db_mmecp_npages);
            }
            if (ret == DBE_RC_END) {
                if (db->db_mmecp_npages > 0) {
                    ui_msg_message(DBE_MSG_WRITING_MMEPAGES_FIN_D, db->db_mmecp_npages);
                }
            }
        }
#else
        ret = DBE_RC_END;
#endif /* defined(SS_MME) && !defined(SS_MYSQL) */

        SS_POPNAME;
        return (ret);
}

#if defined(SS_MME) && !defined(SS_MYSQL)
/*##**********************************************************************\
 *
 *              db_createcp_endmme
 *
 * Makes the final operations of MME checkpoint/snapshot creation.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static dbe_ret_t db_createcp_endmme(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        mme_storage_t* storage;

        SS_PUSHNAME("dbe_db_createcp_endmme");
        ss_dassert(db != NULL);

#if defined(DBE_MTFLUSH)
        if (db->db_mmecp_state == MME_CP_INIT) {
            SS_POPNAME;
            return (DBE_RC_SUCC);
        }
        if ((storage = dbe_mme_getstorage(cd, db->db_mme)) != NULL
            && (!dbefile_diskless || db->db_backupmme_cb != NULL)) {
            ss_rc_dassert(db->db_mmecp_state == MME_CP_DONE, db->db_mmecp_state);
            ss_dassert(cd != NULL);

            db->db_mmecp_page = NULL;
            db->db_mmecp_daddr = SU_DADDR_NULL;
            db->db_mmecp_pageaddrpage = NULL;

            if (db->db_mmecp_flushbatch != NULL) {
                dbe_iomgr_flushbatch_done(
                        db->db_go->go_iomgr,
                        db->db_mmecp_flushbatch);
            }
            db->db_mmecp_flushbatch = NULL;

            /* Notify MME */
            mme_storage_endcheckpoint(cd, storage);

            db->db_backupmme_cb = NULL;
            if (dbefile_diskless) {
                SsMemFree(db->db_mmecp_pagedata);
            }
        }
        ss_dassert(db->db_mmecp_flushbatch == NULL);
#endif /* DBE_MTFLUSH */
        SS_POPNAME;
        return (DBE_RC_SUCC);
}
#endif /* defined(SS_MME) && !defined(SS_MYSQL) */

/*##**********************************************************************\
 *
 *              dbe_db_createcp_end
 *
 * Makes the final operations of concurrent checkpoint/snapshot creation.
 *
 * Parameters :
 *
 *      cd - in, use
 *              client data
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *      DBE_RC_SUCC when OK
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_createcp_end(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        dbe_ret_t rc;
        dbe_cpnum_t cpnum;
        dbe_cpnum_t prev_cpnum;
#ifdef SS_HSBG2
        dbe_catchup_logpos_t lp;
#endif /* SS_HSBG2 */

        if (db->db_force_checkpoint) {
            db->db_force_checkpoint = FALSE;
            dbe_db_setchanged(db, NULL);
        }
        if (!db->db_changed) {
            ss_dassert(!db->db_cpactive);
            return (DBE_RC_SUCC);
        }

        SS_PUSHNAME("dbe_db_createcp_end");

        cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);

        /* Update database header info to contain current checkpoint info
         */
        dbe_cpmgr_updateheaders(db->db_cpmgr);
        dbe_header_clearhsbcopy(db->db_dbfile->f_indexfile->fd_dbheader);
        DBE_CPMGR_CRASHPOINT(18);
        dbe_file_saveheaders(db->db_dbfile);
        DBE_CPMGR_CRASHPOINT(19);

        /* Delete earlier checkpoints
         */
        prev_cpnum = dbe_cpmgr_prevcheckpoint(db->db_cpmgr, cpnum - 1);
        if (prev_cpnum != 0L) {
            dbe_cpmgr_deletecp(db->db_cpmgr, prev_cpnum);
            DBE_CPMGR_CRASHPOINT(21);
        }

        /* Inherit valid entries from checkpoint change list
         */
        dbe_cpmgr_inheritchlist(db->db_cpmgr);
        DBE_CPMGR_CRASHPOINT(22);

#ifndef SS_NOBACKUP
        if (db->db_dbfile->f_log != NULL) {
#ifdef SS_HSBG2
            lp = dbe_catchup_logpos_getfirstusedlogpos(db);
#endif /* SS_HSBG2 */

            rc = dbe_backup_deletelog_cp(
                    db->db_go->go_ctr,
                    db->db_go->go_cfg,
                    cd,
#ifdef SS_HSBG2
                    dbe_db_gethsbg2mode(db) != DBE_HSB_STANDALONE,
                    lp,
#endif /* SS_HSBG2 */
                    NULL);
            su_rc_dassert(rc == DBE_RC_SUCC, rc);
        }
#endif
#if defined(SS_MME) && !defined(SS_MYSQL)
        /* We cannot be in dbe-mutex, because this
           function goes to mme-mutex which has to
           be entered first */
        db_createcp_endmme(cd, db);
#endif /* SS_MME */

        /* End checkpoint */
        SsSemEnter(db->db_sem);
        db->db_cpactive = FALSE;
        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 0);
        ss_pprintf_2(("dbe_db_createcp_end\n"));

        {
            ss_int8_t   nbytes_written;
            ss_int8_t   time_taken;
            ss_int8_t   thruput;
            ss_int4_t   thruput_l;

            su_timer_stop(&db->db_cp_timer);
            nbytes_written =
                dbe_fildes_getnbyteswritten(db->db_dbfile->f_indexfile);
            SsInt8SetUint4(&time_taken, su_timer_read(&db->db_cp_timer));
            SsInt8DivideByInt8(&thruput, nbytes_written, time_taken);
            SsInt8ConvertToInt4(&thruput_l, thruput);
            db->db_cp_thruput =
                ((float) thruput_l * 1000.0f)
                / ((float) (1024 * 1024));
#if 0
            long        nbytes_written_l;
            double      time_taken_f;
            SsInt8ConvertToInt4(&nbytes_written_l, nbytes_written);
            time_taken_f = su_timer_readf(&db->db_cp_timer);
            SsPrintf("Checkpoint wrote %d bytes in %f seconds.\n",
                     nbytes_written_l, time_taken_f);
            SsPrintf("Checkpoint completed at %fMB/s\n",
                     db->db_cp_thruput);
#endif
        }
        SsSemExit(db->db_sem);

        SS_POPNAME;

        return (DBE_RC_SUCC);
}


#if 0 /* Removed by Pete 1996-07-05 */
/*##**********************************************************************\
 *
 *              dbe_db_deletesnapshot
 *
 * Deletes a snapshot
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
 *
 *      cpnum - in
 *              # of the snapshot to delete
 *
 * Return value :
 *      DBE_RC_SUCC when ok,
 *      DBE_ERR_SNAPSHOTNOTEXISTS if snapshot with given number does not
 *      exist, or
 *      DBE_ERR_SNAPSHOTISNEWEST if the specified snapshot is the newest
 *      in the database
 *
 * Limitations  :
 *
 * Globals used :
 */
dbe_ret_t dbe_db_deletesnapshot(dbe_db_t *db, dbe_cpnum_t cpnum)
{
        dbe_ret_t rc;

        rc = DBE_RC_SUCC;
        su_gate_enter_exclusive(db->db_admingate);
        if (!dbe_cpmgr_isalive(db->db_cpmgr, cpnum)) {
            rc = DBE_ERR_SNAPSHOTNOTEXISTS;
        } else if (dbe_cpmgr_newest(db->db_cpmgr) == cpnum) {
            rc = DBE_ERR_SNAPSHOTISNEWEST;
        } else {
            dbe_cpmgr_deletecp(db->db_cpmgr, cpnum);
        }
        su_gate_exit(db->db_admingate);
        return (rc);
}
#endif /* 0 */
/*##**********************************************************************\
 *
 *              dbe_db_setddopactive
 *
 * Sets or unsets an active data dictionary operation. Used in lengthy
 * operations that require that a checkpoint is not created while
 * operation is not completed. (Now only CREATE INDEX uses this.)
 *
 * Parameters :
 *
 *      db - use
 *
 *
 *      startp - in
 *              If TRUE, dd operation is started, otherwise it is stopped.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_setddopactive(dbe_db_t *db, bool startp)
{
        ss_dassert(db != NULL);

        SsSemEnter(db->db_sem);

        if (startp) {
            ss_rc_dassert(db->db_ddopactivecnt >= 0, db->db_ddopactivecnt);
            db->db_ddopactivecnt++;
        } else {
            ss_rc_dassert(db->db_ddopactivecnt > 0, db->db_ddopactivecnt);
            db->db_ddopactivecnt--;
        }

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_enteraction
 *
 * Enter database action (ie. insert/update/delete)
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
 *
 *      cd - in
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_enteraction(
        dbe_db_t* db,
        rs_sysi_t* cd)
{
#ifdef TEST_ACTIONGATE_ALLEXCLUSIVE
        dbe_db_enteraction_exclusive(db, cd);
#else
        su_profile_timer;

        CHK_DB(db);
        ss_dassert(!rs_sysi_isstaticcd(cd));

        su_profile_start;

        if (cd == NULL || rs_sysi_enterdbaction(cd)) {
            ss_dprintf_4(("dbe_db_enteraction:cd=%ld:ENTER\n", (long)cd));
            su_gate_enter_shared(db->db_actiongate);
            ss_debug(if (cd != NULL) rs_sysi_setdbactionshared(cd, TRUE));
        } else {
            ss_dprintf_4(("dbe_db_enteraction:cd=%ld:dbactioncounter=%d\n", (long)cd, cd->si_dbactioncounter));
        }

        su_profile_stop("dbe_db_enteraction");
#endif /* TEST_ACTIONGATE_ALLEXCLUSIVE */
}

void dbe_db_enteraction_exclusive(
        dbe_db_t* db,
        rs_sysi_t* cd)
{
        su_profile_timer;

        CHK_DB(db);
        ss_dassert(!rs_sysi_isstaticcd(cd));

        su_profile_start;

        if (cd == NULL || rs_sysi_enterdbaction(cd)) {
            ss_dprintf_4(("dbe_db_enteraction_exclusive:cd=%ld:ENTER\n", (long)cd));
            su_gate_enter_exclusive(db->db_actiongate);
            ss_debug(if (cd != NULL) rs_sysi_setdbactionshared(cd, FALSE));
        } else {
            ss_dprintf_4(("dbe_db_enteraction_exclusive:cd=%ld:dbactioncounter=%d\n", (long)cd, cd->si_dbactioncounter));
        }
        ss_dassert(cd == NULL || !rs_sysi_isdbactionshared(cd));

        su_profile_stop("dbe_db_enteraction_exclusive");
}

/*##**********************************************************************\
 *
 *              dbe_db_exitaction
 *
 * Exit database action (ie. Insert/update/delete)
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
 *
 *      cd - in
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_exitaction(
        dbe_db_t* db,
        rs_sysi_t* cd)
{
        CHK_DB(db);
        ss_dassert(!rs_sysi_isstaticcd(cd));

        if (cd == NULL || rs_sysi_exitdbaction(cd)) {
            ss_dprintf_4(("dbe_db_exitaction:cd=%ld:EXIT\n", (long)cd));
            su_gate_exit(db->db_actiongate);
        } else {
            ss_dprintf_4(("dbe_db_exitaction:cd=%ld:dbactioncounter=%d\n", (long)cd, cd->si_dbactioncounter));
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_enteraction_hsb
 *
 * Enter database action for hsb logging.
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_enteraction_hsb(dbe_db_t* db __attribute__ ((unused)))
{
        CHK_DB(db);

#ifdef DBE_LOG_INSIDEACTIONGATE

#ifdef TEST_ACTIONGATE_ALLEXCLUSIVE
        su_gate_enter_exclusive(db->db_actiongate);
#else
        su_gate_enter_shared(db->db_actiongate);
#endif /* TEST_ACTIONGATE_ALLEXCLUSIVE */

#endif
}

/*##**********************************************************************\
 *
 *              dbe_db_exitaction_hsb
 *
 * Exit database action for hsb logging
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_exitaction_hsb(dbe_db_t* db __attribute__ ((unused)))
{
        CHK_DB(db);

#ifdef DBE_LOG_INSIDEACTIONGATE
        su_gate_exit(db->db_actiongate);
#endif
}

/*#***********************************************************************\
 *
 *              db_createcp
 *
 * Creates either snapshot or checkpoint at one step
 *
 * Parameters :
 *
 *      cd - in, use
 *          client data
 *
 *      db - in out, use
 *              Database object
 *
 *      type - in
 *          indicates whether we create a checkpoint or a snapshot:
 *              DBE_BLOCK_CPRECORD for checkpoint or
 *          DBE_BLOCK_SSRECORD for snapshot
 *
 * Return value :
 *      DBE_RC_SUCC when succesful
 *
 * Limitations  :
 *
 * Globals used :
 */
static dbe_ret_t db_createcp(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_blocktype_t type,
        bool displayprogress,
        bool splitlog)
{
        dbe_ret_t rc;

#if defined(DBE_MTFLUSH)
        void (*saved_wakeupfp)(void*);
        void* saved_wakeupctx;
#ifdef SS_MME
        void (*saved_fbwakeupfp)(void*);
        void* saved_fbwakeupctx;
        void (*saved_flusheventresetfp)(void*);
        void* saved_flusheventresetctx;
#endif
        saved_wakeupfp = db->db_flushwakeupfp;
        saved_wakeupctx = db->db_flushwakeupctx;
        db->db_flushwakeupfp = db_mtflushwakeupfun;
        db->db_flushwakeupctx = SsMesCreateLocal();

#ifdef SS_MME
        saved_flusheventresetfp = db->db_flusheventresetfp;
        saved_flusheventresetctx = db->db_flusheventresetctx;
        saved_fbwakeupfp = db->db_flushbatchwakeupfp;
        saved_fbwakeupctx = db->db_flushbatchwakeupctx;
        db->db_flushbatchwakeupfp = db_mtflushwakeupfun;
        db->db_flushbatchwakeupctx = db->db_flushwakeupctx;
        db->db_flusheventresetfp = db_flusheventresetfun;
        db->db_flusheventresetctx= db->db_flushwakeupctx;
#endif

#endif /* DBE_MTFLUSH */
        rc = db_createcp_start(cd, db, type, splitlog);
#if defined(DBE_MTFLUSH)
        if (rc == DBE_RC_WAITFLUSH) {
            ss_dprintf_1(("db_createcp(): waiting for flush\n"));
            SsMesWait(db->db_flushwakeupctx);
            ss_dprintf_1(("db_createcp(): wait for flush ended\n"));
            rc = DBE_RC_SUCC;
        }
        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 11);
#ifndef SS_MME
        SsMesFree(db->db_flushwakeupctx);
        db->db_flushwakeupfp  = saved_wakeupfp;
        db->db_flushwakeupctx = saved_wakeupctx;
#endif
#endif /* DBE_MTFLUSH */
        if (rc != DBE_RC_SUCC) {
            return(rc);
        }
#if defined(DBE_MTFLUSH) && defined(SS_MME)
        while ((rc = dbe_db_createcp_step(cd, db, displayprogress)) != DBE_RC_END) {
            if (rc == DBE_RC_WAITFLUSH) {
                ss_dprintf_1(("db_createcp(): waiting for flush\n"));
                SS_PMON_SET(SS_PMON_CHECKPOINTACT, 12);
                SsMesWait(db->db_flushwakeupctx);
                ss_dprintf_1(("db_createcp(): wait for flush ended\n"));
            }
            if (rc == DBE_RC_WAITFLUSHBATCH) {
                ss_dprintf_1(("db_createcp(): waiting for flush batch\n"));
                SS_PMON_SET(SS_PMON_CHECKPOINTACT, 13);
                SsMesWait(db->db_flushbatchwakeupctx);
                ss_dprintf_1(("db_createcp(): wait for flush batch ended\n"));
            }
        }
        SS_PMON_SET(SS_PMON_CHECKPOINTACT, 14);
        SsMesFree(db->db_flushwakeupctx);
        db->db_flushwakeupfp  = saved_wakeupfp;
        db->db_flushwakeupctx = saved_wakeupctx;
        db->db_flusheventresetfp = saved_flusheventresetfp;
        db->db_flusheventresetctx = saved_flusheventresetctx;
        db->db_flushbatchwakeupfp = saved_fbwakeupfp;
        db->db_flushbatchwakeupctx = saved_fbwakeupctx;
#else
        while ((rc = dbe_db_createcp_step(cd, db, displayprogress)) == DBE_RC_CONT) {
            /* no loop body */
            ;
        }
#endif

        su_rc_assert(rc == DBE_RC_END, rc);
        rc = dbe_db_createcp_end(cd, db);
        su_rc_assert(rc == DBE_RC_SUCC, rc);
        return (rc);
}

/*#***********************************************************************\
 *
 *              db_restorelatestcp
 *
 * Restores the latest checkpoint situation to db engine. This does
 * not, however, complete the revert-to-checkpoint operation.
 *
 * Parameters :
 *
 *      db - in out, use
 *              Database object.
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void db_restorelatestcp(dbe_db_t* db)
{
        dbe_ret_t rc;

        rc = dbe_cpmgr_restorestartrec(db->db_cpmgr);
        switch (rc) {
            case DBE_RC_SUCC:
                return;
            case DBE_ERR_NOCHECKPOINT:
                if (!dbe_debug) {
                    su_informative_exit(
                        __FILE__,
                        __LINE__,
                        DBE_ERR_NOCHECKPOINT);
                }
                break;
            default:
                su_rc_error(rc);
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_adduser
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      user -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
int dbe_db_adduser(dbe_db_t* db, dbe_user_t* user)
{
        int id;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        id = su_pa_insert(db->db_users, user);
        SsSemExit(db->db_sem);

        return(id);
}

/*##**********************************************************************\
 *
 *              dbe_db_removeuser
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      userid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_removeuser(dbe_db_t* db, uint userid)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);
        ss_dassert(su_pa_indexinuse(db->db_users, userid));
        su_pa_remove(db->db_users, userid);
        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *		dbe_db_getusercount
 *
 * Returns number of users in db user list.
 *
 * Parameters :
 *
 *		db -
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
int dbe_db_getusercount(dbe_db_t* db)
{
        int count;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        count = su_pa_nelems(db->db_users);
        SsSemExit(db->db_sem);

        return(count);
}

/*##**********************************************************************\
 *
 *      dbe_db_getuserbyid
 *
 * <function description>
 *
 * Parameters:
 *      db - <usage>
 *          <description>
 *
 *      userid - <usage>
 *          <description>
 *
 * Return value - <usage>:
 *       <description>
 *
 * Limitations:
 *
 * Globals used:
 */
dbe_user_t* dbe_db_getuserbyid(dbe_db_t* db, uint userid)
{
        dbe_user_t*     user;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        ss_dassert(su_pa_indexinuse(db->db_users, userid));
        user = su_pa_getdata(db->db_users, userid);
        SsSemExit(db->db_sem);

        return user;
}

/*##**********************************************************************\
 *
 *              dbe_db_abortsearchesrelid
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      relid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_abortsearchesrelid(dbe_db_t* db, ulong relid)
{
        uint i;
        dbe_user_t* user;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        su_pa_do_get(db->db_users, i, user) {
            dbe_user_abortsearchesrelid(user, relid);
        }
        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_abortsearcheskeyid
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      keyid -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_abortsearcheskeyid(dbe_db_t* db, ulong keyid)
{
        uint i;
        dbe_user_t* user;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        su_pa_do_get(db->db_users, i, user) {
            dbe_user_abortsearcheskeyid(user, keyid);
        }
        SsSemExit(db->db_sem);
}

void dbe_db_newplan(dbe_db_t* db, ulong relid)
{
        uint i;
        dbe_user_t* user;

        CHK_DB(db);

        SsSemEnter(db->db_sem);
        su_pa_do_get(db->db_users, i, user) {
            dbe_user_newplan(user, relid);
        }
        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *		dbe_db_signaltouser
 *
 * Sends signal to all users in db users list. Sending a signal means
 * setting a flag in cd in rs_signalinfo_t.
 *
 * Parameters :
 *
 *		db -
 *
 *
 *		userid -
 *
 *
 *		signaltype -
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
void dbe_db_signaltouser(dbe_db_t* db, int userid, rs_signaltype_t signaltype)
{
        uint i;
        dbe_user_t* user;
        rs_sysi_t* cd;
        rs_signalinfo_t* rsi;

        CHK_DB(db);

        SsSemEnter(db->db_sem);

        switch (signaltype) {
            case RS_SIGNAL_DDCHANGE:
                /* Signal to all cliemts.
                 */
                ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_DDCHANGE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    cd = dbe_user_getcd(user);
                    rsi = rs_sysi_getsignalinfo(cd);
                    if (rsi != NULL) {
                        rsi->rsi_flushproccache = TRUE;
                        rsi->rsi_flushstmtcache = TRUE;
                    }
                }
                break;
            case RS_SIGNAL_HSB_STATE_CHANGE_START:
                /* Signal HSB role change to all the TF- clients */
                ss_dprintf_4(("dbe_db_signaltouser:RS_HSB_ROLE_CHANGE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    cd = dbe_user_getcd(user);
                    rsi = rs_sysi_getsignalinfo(cd);
                    if (rsi != NULL) {
                        rsi->rsi_tf_rolechange_ctr++;
                        rsi->rsi_tf_rolechange_progress = TRUE;
                    }
                }
                break;
            case RS_SIGNAL_HSB_STATE_CHANGE_END:
                /* Signal HSB role change to all the TF- clients */
                ss_dprintf_4(("dbe_db_signaltouser:RS_HSB_ROLE_CHANGE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    cd = dbe_user_getcd(user);
                    rsi = rs_sysi_getsignalinfo(cd);
                    if (rsi != NULL) {
                        rsi->rsi_tf_rolechange_ctr++;
                        rsi->rsi_tf_rolechange_progress = FALSE;
                    }
                }
                break;
            case RS_SIGNAL_FLUSHSQLCACHE:
                /* Signal only to one user.
                 */
                ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_FLUSHSQLCACHE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    if (dbe_user_getid(user) == userid) {
                        /* Found user.
                         */
                        ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_DDCHANGE, found userid %d\n", userid));
                        cd = dbe_user_getcd(user);
                        rsi = rs_sysi_getsignalinfo(cd);
                        if (rsi != NULL) {
                            rsi->rsi_flushproccache = TRUE;
                            rsi->rsi_flushstmtcache = TRUE;
                        }
                        break;
                    }
                }
                break;
            case RS_SIGNAL_DEFSCHEMACHANGE:
                /* Signal only to one user.
                 */
                ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_DEFSCHEMACHANGE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    if (dbe_user_getid(user) == userid) {
                        /* Found user.
                         */
                        ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNA_DEFSCHEMACHANGE, found userid %d\n", userid));
                        cd = dbe_user_getcd(user);
                        rsi = rs_sysi_getsignalinfo(cd);
                        if (rsi != NULL) {
                            rsi->rsi_defschemachanged = TRUE;
                        }
                        break;
                    }
                }
                break;
            case RS_SIGNAL_DEFCATALOGCHANGE:
                /* Signal only to one user.
                 */
                ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_DEFCATALOGCHANGE\n"));
                su_pa_do_get(db->db_users, i, user) {
                    if (dbe_user_getid(user) == userid) {
                        /* Found user.
                         */
                        ss_dprintf_4(("dbe_db_signaltouser:RS_SIGNAL_DEFCATALOGCHANGE, found userid %d\n", userid));
                        cd = dbe_user_getcd(user);
                        rsi = rs_sysi_getsignalinfo(cd);
                        if (rsi != NULL) {
                            rsi->rsi_defcatalogchanged = TRUE;
                        }
                        break;
                    }
                }
                break;
            default:
                ss_rc_error(signaltype);
        }
        SsSemExit(db->db_sem);
}

static void db_updateavgnsearch(dbe_db_t* db)
{
        db->db_avgnsearch = DB_ONE_PER_ALPHA * (db->db_nsearch + 1) +
                            (1.0 - DB_ONE_PER_ALPHA) * db->db_avgnsearch;
}

/*##**********************************************************************\
 *
 *              dbe_db_searchstarted
 *
 * Notifies that a search is started.
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_searchstarted(dbe_db_t* db)
{
        CHK_DB(db);

        SsSemEnter(db->db_nsearchsem);
        db->db_nsearch++;
        db_updateavgnsearch(db);
        SsSemExit(db->db_nsearchsem);
}

/*##**********************************************************************\
 *
 *              dbe_db_searchstopped
 *
 *
 * Notifies that a search is started.
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_searchstopped(dbe_db_t* db)
{
        CHK_DB(db);

        SsSemEnter(db->db_nsearchsem);
        db->db_nsearch--;
        db_updateavgnsearch(db);
        SsSemExit(db->db_nsearchsem);
}

/*##**********************************************************************\
 *
 *              dbe_db_getnewrelid_log
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_db_getnewrelid_log(dbe_db_t* db)
{
        return(dbe_db_getnewkeyid_log(db));
}

/*##**********************************************************************\
 *
 *              dbe_db_getnewattrid_log
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_db_getnewattrid_log(dbe_db_t* db)
{
        ulong val;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);

        val = dbe_counter_getnewattrid(db->db_go->go_ctr);
        if (db->db_dbfile->f_log != NULL) {
            dbe_ret_t rc;

            rc = dbe_log_putincsysctr(
                    db->db_dbfile->f_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_ATTRID);
            ss_rc_dassert(rc == DBE_RC_SUCC || rc == SRV_ERR_HSBCONNBROKEN, rc);
        }

        dbe_db_exitaction(db, NULL);

        return (val);
}

/*##**********************************************************************\
 *
 *              dbe_db_getnewkeyid_log
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_db_getnewkeyid_log(dbe_db_t* db)
{
        ulong val;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);

        val = dbe_counter_getnewkeyid(db->db_go->go_ctr);
        if (db->db_dbfile->f_log != NULL) {
            dbe_ret_t rc;

            rc = dbe_log_putincsysctr(
                    db->db_dbfile->f_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_KEYID);
            ss_rc_dassert(rc == DBE_RC_SUCC || rc == SRV_ERR_HSBCONNBROKEN, rc);
        }

        dbe_db_exitaction(db, NULL);

        return (val);
}

/*##**********************************************************************\
 *
 *              dbe_db_getnewuserid_log
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
ulong dbe_db_getnewuserid_log(dbe_db_t* db)
{
        ulong val;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);

        val = dbe_counter_getnewuserid(db->db_go->go_ctr);
        if (db->db_dbfile->f_log != NULL) {
            dbe_ret_t rc;

            rc = dbe_log_putincsysctr(
                    db->db_dbfile->f_log,
                    DBE_LOGREC_INCSYSCTR,
                    DBE_CTR_USERID);
            ss_rc_dassert(rc == DBE_RC_SUCC || rc == SRV_ERR_HSBCONNBROKEN, rc);
        }

        dbe_db_exitaction(db, NULL);

        return(val);
}

/*##**********************************************************************\
 *
 *              dbe_db_inctuplenum
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_inctuplenum(dbe_db_t* db)
{
        CHK_DB(db);

        (void)dbe_counter_getnewtuplenum(db->db_go->go_ctr);
}

/*##**********************************************************************\
 *
 *              dbe_db_inctupleversion
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_inctupleversion(dbe_db_t* db)
{
        CHK_DB(db);

        (void)dbe_counter_getnewtupleversion(db->db_go->go_ctr);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              dbe_db_getreplicacounters
 *
 * Returns data area containing counter values that should be in sync
 * during replication. The returned data area is allocated by SsMemAlloc
 * and must be released by the caller.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 *      p_data - out, give
 *
 *
 *      p_size - out
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
void dbe_db_getreplicacounters(
        dbe_db_t* db,
        bool hsbg2,
        char** p_data,
        int* p_size)
{
        CHK_DB(db);

        dbe_counter_getreplicacounters(
            db->db_go->go_ctr,
            hsbg2,
            p_data,
            p_size);
}

/*##**********************************************************************\
 *
 *              dbe_db_setreplicacounters
 *
 * Sets counter values that should be in sync during replication from
 * a given data area.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 *      data - in
 *
 *
 *      size - in
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
void dbe_db_setreplicacounters(
        dbe_db_t* db,
        bool hsbg2,
        char* data,
        int size __attribute__ ((unused)))
{
        bool changes;

        CHK_DB(db);
        ss_dassert((hsbg2 && size == DBE_HSBSYSCTR_SIZE)
                || (!hsbg2 && size == DBE_HSBSYSCTR_OLDSIZE));

        dbe_db_enteraction_hsb(db);

        changes = dbe_counter_setreplicacounters(
                    db->db_go->go_ctr,
                    hsbg2,
                    data);
        if (changes) {
            dbe_db_setchanged(db, NULL);
            if (db->db_dbfile->f_log) {
                dbe_log_puthsbsysctr(db->db_dbfile->f_log, data);
            }
        }

        dbe_db_exitaction_hsb(db);
}

/*##**********************************************************************\
 *
 *              dbe_db_sethsbmode
 *
 * Sets initial hot standby replication mode.
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      hsbmode -
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
void dbe_db_sethsbmode(
        dbe_db_t* db,
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_hsbmode_t hsbmode)
{
        CHK_DB(db);

#ifdef SS_HSBG2
        if (!ss_migratehsbg2) {
            ss_dprintf_1(("dbe_db_sethsbmode:hsbmode=%d, IGNORE WITH HSBG2\n", (int)hsbmode));
            return;
        }
#endif /* SS_HSBG2 */

        ss_dprintf_1(("dbe_db_sethsbmode:hsbmode=%d\n", (int)hsbmode));
        ss_rc_dassert(hsbmode == DBE_HSB_STANDALONE ||
                      hsbmode == DBE_HSB_PRIMARY ||
                      hsbmode == DBE_HSB_SECONDARY,
                      (int)hsbmode);
        if (hsbmode == DBE_HSB_STANDALONE) {
            /* JarmoR Jan 14, 2001. Fixed a recovery bug where we cleaned
             * uncommitted transactions before recovery because this
             * function is called before recovery. Added check that
             * clean is not done before recovery is complete (db_recoverydone).
             */
            if (db->db_recoverydone
            &&  (db->db_hsbmode == DBE_HSB_SECONDARY
                 || !dbe_db_ishsbconfigured(db)))
            {
                ss_dprintf_2(("dbe_db_sethsbmode: call dbe_trxbuf_cleanuncommitted() (hsbmode=%d)\n", (int)db->db_hsbmode));
                dbe_trxbuf_cleanuncommitted(db->db_go->go_trxbuf, dbe_counter_getcommittrxnum(db->db_go->go_ctr));
            }
            if (db->db_hsbmode != DBE_HSB_STANDALONE) {
                bool cfg_readonly;
                dbe_cfg_getreadonly(db->db_go->go_cfg,
                                    &cfg_readonly);
                db->db_readonly = cfg_readonly;
            }
        }
        db->db_hsbmode = hsbmode;
        db_hsbmodechangereset(db);
}

#ifdef SS_HSBG2
/* durability */
void dbe_db_sethsbg2_adaptive_loggingif(
        dbe_db_t* db,
        bool adaptive_loggingif)
{
        CHK_DB(db);
        ss_dprintf_1(("dbe_db_sethsbg2_adaptive_loggingif:adaptive_logging=%d\n", adaptive_loggingif));
        db->db_hsbg2_adaptiveif = adaptive_loggingif;
}
#endif

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 *
 *              dbe_db_addindexwrites
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nindexwrites -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_addindexwrites(
        dbe_db_t* db,
        rs_sysi_t* cd,
        long nindexwrites)
{
        CHK_DB(db);

        dbe_gobj_addindexwrites(db->db_go, cd, nindexwrites);
}

/*##**********************************************************************\
 *
 *              dbe_db_addmergewrites
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nmergewrites -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_addmergewrites(
        dbe_db_t* db,
        long nmergewrites)
{
        CHK_DB(db);

        dbe_gobj_addmergewrites(db->db_go, nmergewrites);
}

/*##**********************************************************************\
 *
 *		dbe_db_getmergewrites
 *
 * Returns current merge writes value.
 *
 * Parameters :
 *
 *		db -
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
long dbe_db_getmergewrites(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_go->go_nmergewrites);
}

/*##**********************************************************************\
 *
 *		dbe_db_getmergelimit
 *
 * Returns current merge limit value.
 *
 * Parameters :
 *
 *		db -
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
long dbe_db_getmergelimit(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_mergelimit);
}

/*##**********************************************************************\
 *
 *              dbe_db_addlogwrites
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nlogwrites -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
void dbe_db_addlogwrites(dbe_db_t* db, long nlogwrites)
{
        CHK_DB(db);

        dbe_gobj_addlogwrites(db->db_go, nlogwrites);
}

/*##**********************************************************************\
 *
 *              dbe_db_addtrxstat
 *
 * Adds transaction statistics to the database object.
 *
 * Parameters :
 *
 *      db - use
 *              Database object
 *
 *      trxtype - in
 *              Transaction type, one of commit, rollback or abort.
 *
 *      read_only - in
 *              TRUE if transaction was read only.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_addtrxstat(
        dbe_db_t* db,
        dbe_db_trxtype_t trxtype,
        bool read_only,
        long stmtcnt)
{
        CHK_DB(db);

        dbe_gobj_addtrxstat(
            db->db_go,
            NULL,
            trxtype,
            FALSE,
            read_only,
            stmtcnt,
            0,
            0);
}

/*##**********************************************************************\
 *
 *              dbe_db_startloader
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_startloader(
        dbe_db_t* db)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        ss_dassert(db->db_isloader >= 0);

        db->db_isloader++;

        if (db->db_isloader == 1) {
            db->db_tmpcplimit = db->db_cplimit;
            db->db_cplimit = LONG_MAX;
        }

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_stoploader
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_stoploader(
        dbe_db_t* db)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        ss_dassert(db->db_isloader > 0);

        db->db_isloader--;

        if (db->db_isloader == 0) {
            db->db_cplimit = db->db_tmpcplimit;
        }

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_cpchecklimit
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
bool dbe_db_cpchecklimit(
        dbe_db_t* db)
{
        dbe_log_t* log;
        bool make_cp = FALSE;

        CHK_DB(db);

        if (dbefile_diskless) {
            return FALSE;
        }
        if (db->db_readonly) {
            return(FALSE);
        }

        SsSemEnter(db->db_sem);

        if (db->db_go->go_nlogwrites > db->db_cplimit) {
            make_cp = TRUE;
        }

        if ((log = dbe_db_getlog(db)) != NULL) {
            if (dbe_log_getsize(log, TRUE) > DB_LOGSPLITSIZE) {
                make_cp = TRUE;
            }
        }
        if (db->db_cpmintime != 0) {
            make_cp = make_cp &&
                      (long)SsTime(NULL) - db->db_cplasttime > db->db_cpmintime;
        }

        if (make_cp) {
            db->db_cplasttime = SsTime(NULL);
        }

        SsSemExit(db->db_sem);

        return(make_cp);
}

bool dbe_db_mergecleanup(dbe_db_t* db)
{
        int docleanup;
        dbe_trxnum_t mergetrxnum;

        CHK_DB(db);

        if (!dbe_cfg_mergecleanup) {
            ss_pprintf_2(("dbe_db_mergecleanup:!dbe_cfg_mergecleanup, return 0\n"));
            return(FALSE);
        }
        if (!db->db_changed) {
            ss_pprintf_2(("dbe_db_mergecleanup:!db->db_changed, return 0\n"));
            return(FALSE);
        }
        if (db->db_readonly) {
            ss_pprintf_2(("dbe_db_mergecleanup:db->db_readonly, return 0\n"));
            return(FALSE);
        }

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);
        dbe_db_exitaction(db, NULL);

        su_gate_enter_shared(db->db_mergesem);
        SsSemEnter(db->db_sem);

        ss_pprintf_1(("dbe_db_mergecleanup:nmergewrites=%ld, mergelimit=%ld, quickmergecnt=%ld, quickmergelimit=%ld\n",
            db->db_go->go_nmergewrites, db->db_mergelimit,
            db->db_go->go_trxstat.ts_quickmergelimitcnt, db->db_quickmergelimit));

        mergetrxnum = dbe_counter_getmergetrxnum(db->db_go->go_ctr);

        if (DBE_TRXNUM_ISNULL(db->db_lastmergecleanup)) {
            ss_pprintf_2(("dbe_db_mergecleanup:lastmergecleanup = DBE_TRXNUM_NULL\n"));
            docleanup = TRUE;
        } else {
            dbe_trxnum_t nextmergecleanup;
            nextmergecleanup = DBE_TRXNUM_SUM(db->db_lastmergecleanup, 10000);
            ss_pprintf_2(("dbe_db_mergecleanup:nextmergecleanup=%ld, mergetrxnum=%ld\n", DBE_TRXNUM_GETLONG(nextmergecleanup), DBE_TRXNUM_GETLONG(mergetrxnum)));
            if (DBE_TRXNUM_CMP_EX(mergetrxnum, nextmergecleanup) >= 0) {
                docleanup = TRUE;
            } else {
                docleanup = FALSE;
            }
        }

        if (docleanup) {
            SS_PMON_ADD(SS_PMON_MERGECLEANUP);
            if (DBE_TRXNUM_CMP_EX(db->db_lastmergecleanup, mergetrxnum) < 0) {
                ss_pprintf_2(("dbe_db_mergecleanup:new lastmergecleanup=%ld\n", DBE_TRXNUM_GETLONG(mergetrxnum)));
                db->db_lastmergecleanup = mergetrxnum;
            }
        }

        SsSemExit(db->db_sem);

        if (docleanup) {
            long removecount;

            ss_pprintf_2(("dbe_db_mergecleanup:call dbe_trxbuf_clean\n"));

            removecount = dbe_trxbuf_clean(
                                db->db_go->go_trxbuf,
                                mergetrxnum,
                                DBE_TRXID_NULL,
                                NULL);

            dbe_gobj_quickmergeupdate(db->db_go, removecount);

            ss_pprintf_2(("dbe_db_mergecleanup:removecount=%ld\n", removecount));
        }

        su_gate_exit(db->db_mergesem);

        ss_pprintf_2(("dbe_db_mergecleanup:return\n"));

        return(docleanup);
}

/*#***********************************************************************\
 *
 *              db_mergechecklimit
 *
 *
 *
 * Parameters :
 *
 *      db - in
 *
 *
 *      loaderp - in
 *              Is the caller a loader program.
 *
 * Return value :
 *
 *      Integer value telling how many times the merge counter has
 *      gone over merge limit. Return value zero means no need to start
 *      merge. The value is calculated by doing integer division
 *
 *          merge counter / merge limit
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
static int db_mergechecklimit(
        dbe_db_t* db,
        bool loaderp)
{
        int check;

        CHK_DB(db);

        if (db->db_mergedisablecount > 0) {
            return(0);
        }
        if (!db->db_changed) {
            ss_dprintf_2(("db_mergechecklimit:!db->db_changed, return 0\n"));
             return(0);
        }
        if (db->db_readonly) {
            ss_dprintf_2(("db_mergechecklimit:db->db_readonly, return 0\n"));
            return(0);
        }

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);
        dbe_db_exitaction(db, NULL);

        su_gate_enter_shared(db->db_mergesem);
        SsSemEnter(db->db_sem);

        ss_pprintf_1(("db_mergechecklimit:nmergewrites=%ld, mergelimit=%ld, quickmergecnt=%ld, quickmergelimit=%ld, db->db_isloader=%d, loaderp=%d\n",
            db->db_go->go_nmergewrites, db->db_mergelimit,
            db->db_go->go_trxstat.ts_quickmergelimitcnt, db->db_quickmergelimit,
            db->db_isloader, loaderp));

        check = db->db_go->go_nmergewrites / db->db_mergelimit;
        if (!loaderp && !dbe_cfg_mergecleanup) {
            int qmrgcheck;
            qmrgcheck =
                (long) db->db_go->go_trxstat.ts_quickmergelimitcnt / db->db_quickmergelimit;
            if (qmrgcheck) {
                if (check) {
                    check += qmrgcheck;
                } else {
                    check = 1;
                }
            }
        }
        if (check && db->db_mergemintime != 0 && db->db_indmerge == NULL) {
            bool startp;
            startp = (long) SsTime(NULL) - db->db_mergelasttime > db->db_mergemintime;
            ss_dprintf_2(("db_mergechecklimit:db_mergemintime startp=%d\n", check));
            if (startp) {
                db->db_mergelasttime = SsTime(NULL);
            } else {
                check = 0;
            }
        }
        if (check == 0) {
            check = db_checkforcemergevalues(db);
        }
        if (check && dbe_cfg_mergecleanup) {
            /* Return always a fixed value so server does not accelerate merge. */
            check = DB_MERGEFIXEDRATE;
        }

        SsSemExit(db->db_sem);
        su_gate_exit(db->db_mergesem);

        ss_dprintf_2(("db_mergechecklimit:check=%d\n", check));

        return(check);
}

/*##**********************************************************************\
 *
 *              dbe_db_quickmergechecklimit
 *
 *
 *
 * Parameters :
 *
 *      db - in
 *
 *
 * Return value :
 *
 *      Integer value telling how many times the merge counter has
 *      gone over quick merge limit. Return value zero means no need to start
 *      merge.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int dbe_db_quickmergechecklimit(dbe_db_t* db)
{
        int check;

        CHK_DB(db);

        if (!dbe_cfg_mergecleanup) {
            ss_pprintf_2(("dbe_db_quickmergechecklimit:!dbe_cfg_mergecleanup, return 0\n"));
            return(0);
        }
        if (db->db_mergedisablecount > 0) {
            return(0);
        }
        if (!db->db_changed) {
            ss_dprintf_2(("db_mergechecklimit:!db->db_changed, return 0\n"));
             return(0);
        }
        if (db->db_readonly) {
            ss_dprintf_2(("db_mergechecklimit:db->db_readonly, return 0\n"));
            return(0);
        }

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);
        dbe_db_exitaction(db, NULL);

        su_gate_enter_shared(db->db_mergesem);
        SsSemEnter(db->db_sem);

        ss_pprintf_1(("db_quickmergechecklimit:quickmergecnt=%ld, quickmergelimit=%ld\n",
            db->db_go->go_trxstat.ts_quickmergelimitcnt, db->db_quickmergelimit));

        check = (long) db->db_go->go_trxstat.ts_quickmergelimitcnt / db->db_quickmergelimit;

        if (check && db->db_mergemintime != 0 && db->db_quickmerge == NULL) {
            bool startp;
            startp = (long) SsTime(NULL) - db->db_quickmergelasttime > db->db_mergemintime;
            ss_dprintf_2(("db_quickmergechecklimit:db_mergemintime startp=%d\n", check));
            if (startp) {
                db->db_quickmergelasttime = SsTime(NULL);
            } else {
                check = 0;
            }
        }
        if (check) {
            /* Return always a fixed value so server does not accelerate merge. */
            check = DB_MERGEFIXEDRATE;
        }

        SsSemExit(db->db_sem);
        su_gate_exit(db->db_mergesem);

        ss_dprintf_2(("db_quickmergechecklimit:check=%d\n", check));

        return(check);
}

/*##**********************************************************************\
 *
 *              dbe_db_mergechecklimit_loader
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 *      Integer value telling how many times the merge counter has
 *      gone over merge limit. Return value zero means no need to start
 *      merge. The value is calculated by doing integer division
 *
 *          merge counter / merge limit
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int dbe_db_mergechecklimit_loader(
        dbe_db_t* db)
{
        CHK_DB(db);
        ss_dassert(db->db_isloader > 0);

        return(db_mergechecklimit(db, TRUE));
}

/*##**********************************************************************\
 *
 *              dbe_db_mergechecklimit
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value :
 *
 *      Integer value telling how many times the merge counter has
 *      gone over merge limit. Return value zero means no need to start
 *      merge. The value is calculated by doing integer division
 *
 *          merge counter / merge limit
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
int dbe_db_mergechecklimit(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db_mergechecklimit(db, FALSE));
}

/*#**********************************************************************\
 *
 *              db_mergestart_nomutex
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      db -
 *
 *
 *      idlemerge -
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
static void db_mergestart_nomutex(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool idlemerge,
        bool force_full_merge,
        bool usermerge)
{
        if (!dbe_db_setchanged(db, NULL)) {
             return;
        }
        if (db->db_readonly) {
            return;
        }
        if (db->db_mergedisablecount > 0) {
            return;
        }

        SS_PUSHNAME("db_mergestart_nomutex");

        if (db->db_indmerge == NULL) {
            dbe_trxnum_t mergetrxnum;
            dbe_trxnum_t patchtrxnum;
            ulong maxpoolblocks;
            bool full_merge;
            bool print_merge_message;

            if (db_checkforcemergevalues(db)) {
                force_full_merge = TRUE;
            }
            if (dbe_cfg_mergecleanup) {
                force_full_merge = TRUE;
            }

            ss_pprintf_1(("db_mergestart_nomutex:start merge, force_full_merge=%d, idlemerge=%d, db->db_go->go_nmergewrite=%ld\n", force_full_merge, idlemerge, db->db_go->go_nmergewrites));

            mergetrxnum = dbe_counter_getmergetrxnum(db->db_go->go_ctr);
            patchtrxnum = dbe_counter_getmaxtrxnum(db->db_go->go_ctr);
            maxpoolblocks = (ulong)dbe_db_poolsizeforquery(db) /
                            (ulong)dbe_db_blocksize(db);

            if (force_full_merge
                || (db->db_go->go_nmergewrites / db->db_mergelimit) > 0)
            {
                full_merge = TRUE;
            } else {
                int qmrgctr;
                qmrgctr = db->db_go->go_trxstat.ts_quickmergelimitcnt / db->db_quickmergelimit;
                if (qmrgctr < 2
                    && db->db_go->go_nmergewrites > (db->db_mergelimit / 2))
                {
                    /* There are quite many merge writes and trxbuf not too much
                     * over limit. Start a full merge.
                     */
                    full_merge = TRUE;
                } else {
                    full_merge = FALSE;
                }
            }

            full_merge = force_full_merge ||
                         db->db_go->go_nmergewrites / db->db_mergelimit > 0;

            dbe_gobj_mergestart(db->db_go, mergetrxnum, full_merge);

            db->db_indmergenumber++;
            if (db->db_indmergenumber == 0) {
                db->db_indmergenumber++;
            }

            if (full_merge) {
                /* Start a full merge.
                 */
                db->db_indmerge = dbe_indmerge_init_ex(
                                    cd,
                                    db,
                                    db->db_index,
                                    mergetrxnum,
                                    patchtrxnum,
                                    maxpoolblocks,
                                    FALSE);
            } else {
                /* Start a quick merge.
                 */
                db->db_indmerge = dbe_indmerge_init_ex(
                                    cd,
                                    db,
                                    db->db_index,
                                    mergetrxnum,
                                    patchtrxnum,
                                    maxpoolblocks,
                                    TRUE);
                db->db_go->go_trxstat.ts_quickmergelimitcnt = 0;
            }
            SS_PMON_SET(SS_PMON_MERGEACT, 1);
#ifdef SS_DEBUG
            print_merge_message = TRUE;
#else
            print_merge_message = dbe_cfg_startupforcemerge;
#endif /* SS_DEBUG */
            if (print_merge_message) {
                if (full_merge) {
                    if (idlemerge) {
                        ui_msg_message_nogui(DBE_MSG_IDLE_MERGE_STARTED_D, db->db_go->go_nmergewrites);
                    } else {
                        ui_msg_message_nogui(DBE_MSG_MERGE_STARTED_SD, usermerge ? " by user": "", db->db_go->go_nmergewrites);
                    }
                } else {
                    ss_dassert(!usermerge);
                    if (idlemerge) {
                        ui_msg_message_nogui(DBE_MSG_IDLE_QUICK_MERGE_STARTED);
                    } else {
                        ui_msg_message_nogui(DBE_MSG_QUICK_MERGE_STARTED);
                    }
                }
            }
        }
        SS_POPNAME;
}

static bool db_quickmergestart_nomutex(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        if (db->db_mergedisablecount > 0) {
            return(FALSE);
        }
        if (!dbe_db_setchanged(db, NULL)) {
             return(FALSE);
        }
        if (db->db_readonly) {
            return(FALSE);
        }

        if (db->db_quickmerge == NULL) {
            dbe_trxnum_t mergetrxnum;
            dbe_trxnum_t patchtrxnum;
/*
*  Unused?
*            ulong maxpoolblocks;
*            bool full_merge;
*/
            bool print_merge_message;

            ss_pprintf_1(("db_quickmergestart_nomutex:start quick merge\n"));

            mergetrxnum = dbe_counter_getmergetrxnum(db->db_go->go_ctr);
            patchtrxnum = dbe_counter_getmaxtrxnum(db->db_go->go_ctr);

            dbe_gobj_mergestart(db->db_go, mergetrxnum, FALSE);

            db->db_quickmerge = dbe_indmerge_init_ex(
                                    cd,
                                    db,
                                    db->db_index,
                                    mergetrxnum,
                                    patchtrxnum,
                                    2,
                                    TRUE);

            db->db_go->go_trxstat.ts_quickmergelimitcnt = 0;

#ifdef SS_DEBUG
            print_merge_message = TRUE;
#else
            print_merge_message = dbe_cfg_startupforcemerge;
#endif /* SS_DEBUG */
            if (print_merge_message) {
                ui_msg_message_nogui(DBE_MSG_QUICK_MERGE_STARTED);
            }
            return(TRUE);
        } else {
            return(FALSE);
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_mergestart
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      db -
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
uint dbe_db_mergestart(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        uint mergenumber;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);

        su_gate_enter_exclusive(db->db_mergesem);

        db_mergestart_nomutex(cd, db, FALSE, FALSE, FALSE);
        db->db_mergeidletime = FALSE;

        mergenumber = db->db_indmergenumber;

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, NULL);

        return(mergenumber);
}

uint dbe_db_mergestart_full(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        uint mergenumber;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);

        su_gate_enter_exclusive(db->db_mergesem);

        db_mergestart_nomutex(cd, db, FALSE, TRUE, TRUE);
        db->db_mergeidletime = FALSE;

        mergenumber = db->db_indmergenumber;

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, NULL);

        return(mergenumber);
}

bool dbe_db_quickmergestart(
        rs_sysi_t* cd,
        dbe_db_t* db)
{
        bool startp;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);

        su_gate_enter_exclusive(db->db_mergesem);

        startp = db_quickmergestart_nomutex(cd, db);

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, NULL);

        return(startp);
}

/*#***********************************************************************\
 *
 *              db_mergestop_nomutex
 *
 *
 *
 * Parameters :
 *
 *      db -
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
static void db_mergestop_nomutex(dbe_db_t* db)
{
        long nindexwrites;

        CHK_DB(db);

        if (db->db_indmerge != NULL) {
            bool print_merge_message;
            dbe_trxnum_t mergelevel;

            SsSemEnter(db->db_sem);
            mergelevel = dbe_indmerge_getmergelevel(db->db_indmerge);
            if (DBE_TRXNUM_CMP_EX(db->db_lastmergecleanup, mergelevel) < 0) {
                ss_dprintf_2(("db_mergestop_nomutex:new lastmergecleanup=%ld\n", DBE_TRXNUM_GETLONG(mergelevel)));
                db->db_lastmergecleanup = mergelevel;
            }
            SsSemExit(db->db_sem);

            dbe_indmerge_done_ex(db->db_indmerge, &nindexwrites);

            if (dbe_db_gethsbg2mode(db) == DBE_HSB_SECONDARY
                && nindexwrites <= db->db_mergelimit / 100)
            {
                /* Avoid busylooping of merge in secondary and set
                 * mergewrites to zero.
                 */
                ss_pprintf_2(("db_mergestop_nomutex:DBE_HSB_SECONDARY and nindexwrites=%ld, clean merge and index writes\n", nindexwrites));
                nindexwrites = LONG_MAX;
                dbe_gobj_mergeupdate(db->db_go, LONG_MAX, LONG_MAX);
            }

            dbe_gobj_mergestop(db->db_go);

            db->db_indmerge = NULL;
            SS_PMON_SET(SS_PMON_MERGEACT, 0);

#ifdef SS_DEBUG
            print_merge_message = TRUE;
#else
            print_merge_message = dbe_cfg_startupforcemerge;
#endif /* SS_DEBUG */
            if (print_merge_message) {
                if (nindexwrites == LONG_MAX) {
                    ui_msg_message_nogui(DBE_MSG_MERGE_STOPPED_ALL_MERGED);
                } else {
                    ui_msg_message_nogui(DBE_MSG_MERGE_STOPPED_N_KEYS_MERGED_D, nindexwrites);
                }
            }

            ss_pprintf_1(("db_mergestop_nomutex:nindexwrites=%ld, db->db_go->go_nmergewrites=%ld, db->db_mergelimit=%ld\n",
                nindexwrites, db->db_go->go_nmergewrites, db->db_mergelimit));
        }
}

/*#***********************************************************************\
 *
 *              db_quickmergestop_nomutex
 *
 *
 *
 * Parameters :
 *
 *      db -
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
static void db_quickmergestop_nomutex(dbe_db_t* db)
{
        long nindexwrites;

        CHK_DB(db);

        if (db->db_quickmerge != NULL) {
            bool print_merge_message;
            dbe_trxnum_t mergelevel;

            SsSemEnter(db->db_sem);
            mergelevel = dbe_indmerge_getmergelevel(db->db_quickmerge);
            if (DBE_TRXNUM_CMP_EX(db->db_lastmergecleanup, mergelevel) < 0) {
                ss_dprintf_2(("db_quickmergestop_nomutex:new lastmergecleanup=%ld\n", DBE_TRXNUM_GETLONG(mergelevel)));
                db->db_lastmergecleanup = mergelevel;
            }
            SsSemExit(db->db_sem);

            dbe_indmerge_done_ex(db->db_quickmerge, &nindexwrites);

            db->db_quickmerge = NULL;

#ifdef SS_DEBUG
            print_merge_message = TRUE;
#else
            print_merge_message = dbe_cfg_startupforcemerge;
#endif /* SS_DEBUG */
            if (print_merge_message) {
                ui_msg_message_nogui(DBE_MSG_QUICK_MERGE_STOPPED);
            }
            ss_pprintf_1(("db_quickmergestop_nomutex:\n"));
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_mergestop
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_mergestop(dbe_db_t* db)
{
        CHK_DB(db);

        su_gate_enter_exclusive(db->db_mergesem);

        db_mergestop_nomutex(db);

        su_gate_exit(db->db_mergesem);
}

/*##**********************************************************************\
 *
 *              dbe_db_quickmergestop
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_quickmergestop(dbe_db_t* db)
{
        CHK_DB(db);

        su_gate_enter_exclusive(db->db_mergesem);

        db_quickmergestop_nomutex(db);

        su_gate_exit(db->db_mergesem);
}

/*##**********************************************************************\
 *
 *              dbe_db_setnmergetasks
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_setnmergetasks(dbe_db_t* db, int nmergetasks)
{
        CHK_DB(db);

        su_gate_enter_shared(db->db_mergesem);

        ss_dassert(nmergetasks > 0);

        if (db->db_indmerge != NULL) {
            dbe_indmerge_setnmergetasks(db->db_indmerge, nmergetasks);
        }

        su_gate_exit(db->db_mergesem);
}

/*##**********************************************************************\
 *
 *              dbe_db_mergeadvance_ex
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nstep -
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
dbe_mergeadvance_t dbe_db_mergeadvance_ex(
        dbe_db_t* db,
        rs_sysi_t* cd,
        uint nstep,
        bool mergetask,
        uint* p_mergenumber)
{
        dbe_mergeadvance_t ret;
        su_list_t* deferred_blob_unlink_list = NULL;

        CHK_DB(db);
        ss_dassert(cd != NULL);
        ss_dprintf_1(("dbe_db_mergeadvance_ex\n"));

        if (db->db_readonly) {
            if (p_mergenumber != NULL) {
                *p_mergenumber = db->db_indmergenumber;
            }
            return(DBE_MERGEADVANCE_END);
        }

        dbe_db_enteraction(db, cd);
        su_gate_enter_shared(db->db_mergesem);

        if (db->db_indmerge == NULL) {
            ret = DBE_MERGEADVANCE_END;
        } else {
#ifdef SS_MYSQL_PERFCOUNT
            __int64 startcount;
            __int64 endcount;

            if (mysql_enable_perfcount > 1) {
                QueryPerformanceCounter((LARGE_INTEGER*)&startcount);
            }
#endif /* SS_MYSQL_PERFCOUNT */
            ret = dbe_indmerge_advance(
                        db->db_indmerge,
                        cd,
                        nstep,
                        mergetask,
                        &deferred_blob_unlink_list);
            if (ret == DBE_MERGEADVANCE_END && !db->db_quickmerge) {
                db_setnewforcemergevalues(db);
            }

#ifdef SS_MYSQL_PERFCOUNT
            if (mysql_enable_perfcount > 1) {
                QueryPerformanceCounter((LARGE_INTEGER*)&endcount);
                dbe_indmerge_perfcount += endcount - startcount;
                dbe_indmerge_callcount++;
            }
#endif /* SS_MYSQL_PERFCOUNT */

        }
        if (p_mergenumber != NULL) {
            *p_mergenumber = db->db_indmergenumber;
        }

        if (deferred_blob_unlink_list != NULL) {
            dbe_indmerge_unlinkblobs(cd, deferred_blob_unlink_list);
        }

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, cd);

        return(ret);
}

/*##**********************************************************************\
 *
 *              dbe_db_mergeadvance
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nstep -
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
dbe_mergeadvance_t dbe_db_mergeadvance(dbe_db_t* db, rs_sysi_t* cd, uint nstep)
{
        return(dbe_db_mergeadvance_ex(db, cd, nstep, TRUE, NULL));
}

/*##**********************************************************************\
 *
 *              dbe_db_quickmergeadvance
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      nstep -
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
dbe_mergeadvance_t dbe_db_quickmergeadvance(
        dbe_db_t* db,
        rs_sysi_t* cd)
{
        dbe_mergeadvance_t ret;
        su_list_t* deferred_blob_unlink_list = NULL;

        CHK_DB(db);
        ss_dassert(cd != NULL);
        ss_dprintf_1(("dbe_db_quickmergeadvance\n"));

        if (db->db_readonly) {
            return(DBE_MERGEADVANCE_END);
        }

        dbe_db_enteraction(db, cd);
        su_gate_enter_shared(db->db_mergesem);

        if (db->db_quickmerge == NULL) {
            ret = DBE_MERGEADVANCE_END;
        } else {
            ret = dbe_indmerge_advance(
                        db->db_quickmerge,
                        cd,
                        10,
                        TRUE,
                        &deferred_blob_unlink_list);
        }
        if (deferred_blob_unlink_list != NULL) {
            dbe_indmerge_unlinkblobs(cd, deferred_blob_unlink_list);
        }

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, cd);

        return(ret);
}

/*##**********************************************************************\
 *
 *              dbe_db_mergeidletimebegin
 *
 *
 *
 * Parameters :
 *
 *      cd -
 *
 *
 *      db -
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
bool dbe_db_mergeidletimebegin(
        rs_sysi_t* cd,
        dbe_db_t* db,
        uint* p_mergenumber)
{
        bool merge_started = FALSE;

        CHK_DB(db);
        ss_dassert(p_mergenumber != NULL);
        ss_dprintf_1(("dbe_db_mergeidletimebegin: idlemerge %ld, change %ld\n", db->db_disableidlemerge, db->db_changed));

        if (db->db_disableidlemerge) {
            if (!db->db_changed) {
                return(FALSE);
            }
        }

        if (db->db_readonly) {
            return(FALSE);
        }
        if (!dbe_db_setchanged(db, NULL)) {
             return(FALSE);
        }

        dbe_db_enteraction(db, NULL);
        dbe_gtrs_releasereadlevels(db->db_go->go_gtrs);

        su_gate_enter_exclusive(db->db_mergesem);

        if (db->db_indmerge == NULL) {
            db_mergestart_nomutex(cd, db, TRUE, TRUE, FALSE);
            merge_started = TRUE;
            db->db_mergeidletime = TRUE;
            *p_mergenumber = db->db_indmergenumber;
        }

        su_gate_exit(db->db_mergesem);
        dbe_db_exitaction(db, NULL);

        return(merge_started);
}

/*##**********************************************************************\
 *
 *              dbe_db_mergeidletimeend
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_mergeidletimeend(dbe_db_t* db)
{
        CHK_DB(db);

        su_gate_enter_exclusive(db->db_mergesem);

        if (db->db_indmerge != NULL && db->db_mergeidletime) {
            db_mergestop_nomutex(db);
            db->db_mergeidletime = FALSE;
        }

        su_gate_exit(db->db_mergesem);
}

/*##**********************************************************************\
 *
 *              dbe_db_setmergedisabled
 *
 * Disables merge.
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_setmergedisabled(dbe_db_t* db, bool disabled)
{
        CHK_DB(db);

        su_gate_enter_exclusive(db->db_mergesem);

        if (disabled) {

            SS_RTCOVERAGE_INC(SS_RTCOV_DB_MERGE_DISABLED);

            if (db->db_indmerge != NULL) {
                db_mergestop_nomutex(db);
                ss_dassert(db->db_indmerge == NULL);
            }
            if (db->db_quickmerge != NULL) {
                db_mergestop_nomutex(db);
                db_quickmergestop_nomutex(db);
                ss_dassert(db->db_indmerge == NULL);
                ss_dassert(db->db_quickmerge == NULL);
            }

            db->db_mergedisablecount++;

        } else {
            ss_dassert(db->db_mergedisablecount > 0);
            db->db_mergedisablecount--;
        }

        ss_dprintf_1(("Merge %s (%d)\n", db->db_mergedisablecount ? "disabled" : "enabled", db->db_mergedisablecount));

        su_gate_exit(db->db_mergesem);
}

/*##**********************************************************************\
 *
 *              dbe_db_logidleflush
 *
 * Flushes log file when the log file write mode is lazy write and
 * there are unwritten commit marks pending.
 *
 * Parameters :
 *
 *      db - in out, use
 *              database object
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_logidleflush(dbe_db_t* db)
{
        dbe_ret_t rc;

        CHK_DB(db);
#ifndef SS_NOLOGGING
        if (db->db_dbfile->f_log != NULL) {
            rc = dbe_log_idleflush(db->db_dbfile->f_log);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }
#endif /* SS_NOLOGGING */
}

void dbe_db_logflushtodisk(dbe_db_t* db)
{
        dbe_ret_t rc;

        CHK_DB(db);
        if (db->db_dbfile->f_log != NULL) {
            rc = dbe_log_flushtodisk(db->db_dbfile->f_log);
            su_rc_assert(rc == DBE_RC_SUCC, rc);
        }
}

#ifndef SS_NOBACKUP

/*##**********************************************************************\
 *
 *              dbe_db_backupcheck
 *
 * Checks that backup directory is ok.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 *      backupdir - in
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
dbe_ret_t dbe_db_backupcheck(
        dbe_db_t* db,
        char* backupdir,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        if (db->db_readonly) {
            su_err_init(p_errh, DBE_ERR_DBREADONLY);
            return(DBE_ERR_DBREADONLY);
        }

        SsSemEnter(db->db_sem);

        rc = dbe_backup_check(
                db->db_go->go_cfg,
                backupdir,
                p_errh);

        SsSemExit(db->db_sem);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_db_backupstart
 *
 *
 *
 * Parameters :
 *
 *      db - use
 *
 *
 *      backupdir - in, use
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
dbe_ret_t dbe_db_backupstart(
        dbe_db_t* db,
        char* backupdir,
        bool replicap,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;
#ifdef SS_HSBG2
        dbe_catchup_logpos_t lp;
#endif /* SS_HSBG2 */

        CHK_DB(db);

        SsSemEnter(db->db_sem);

        if (db->db_cpactive) {
            SsSemExit(db->db_sem);
            return(DBE_ERR_CPACT);
        }

        if (db->db_backup != NULL) {
            SsSemExit(db->db_sem);
            return(DBE_ERR_BACKUPACT);
        }
        su_param_manager_save();

#ifdef SS_HSBG2
        lp = dbe_catchup_logpos_getfirstusedlogpos(db);
#endif /* SS_HSBG2 */

        db->db_backup = dbe_backup_init(
                            db->db_go->go_cfg,
                            db->db_go->go_syscd,
                            db->db_dbfile,
                            db->db_go->go_ctr,
                            backupdir,
                            replicap,
#ifdef SS_HSBG2
                            dbe_hsbg2_logging_enabled(db->db_hsbg2svc),
                            lp,
#endif /* SS_HSBG2 */
                            &rc,
                            p_errh);

        if (db->db_backup != NULL) {
            /* Backup started succesfully. Otherwise dbe_backup_init
             * returned error code in rc.
             */
            rc = DBE_RC_SUCC;
        }

        SS_PMON_SET(SS_PMON_BACKUPACT, 1);

        SsSemExit(db->db_sem);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_db_backupstartwithcallback
 *
 * Starts backup which uses a callback function do do the actual
 * writing of the data (for RPC backup)
 *
 * Parameters :
 *
 *      db - use
 *          database object
 *
 *      callbackfp - in, hold
 *          pointer to function that outputs the backup data
 *
 *      callbackctx - in out, hold
 *          pointer to callback context to be used as the first argument
 *          for callbackfp
 *
 *      replicap - in
 *          TRUE if this is a HSB copy
 *          FALSE if ordinary backup
 *
 *      p_errh - out, give
 *          pointer to error handle object where possible error
 *          status will be put
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
dbe_ret_t dbe_db_backupstartwithcallback(
        dbe_db_t* db,
        su_ret_t (*callbackfp)(   /* Callback function to write */
                void* ctx,      /* user given stream. */
                dbe_backupfiletype_t ftype,
                ss_int4_t finfo,  /* log file number, unused for other files */
                su_daddr_t daddr, /* position in file */
                char* fname,   /* without path! */
                void* data,
                size_t len),
        void*   callbackctx,
        bool replicap,
        dbe_backuplogmode_t backuplogmode,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;
#ifdef SS_HSBG2
        dbe_catchup_logpos_t lp;
#endif /* SS_HSBG2 */

        CHK_DB(db);

        SsSemEnter(db->db_sem);

        if (db->db_cpactive) {
            SsSemExit(db->db_sem);
            return(DBE_ERR_CPACT);
        }

        if (db->db_backup != NULL) {
            SsSemExit(db->db_sem);
            return(DBE_ERR_BACKUPACT);
        }

#ifdef SS_HSBG2
        lp = dbe_catchup_logpos_getfirstusedlogpos(db);
#endif /* SS_HSBG2 */

        db->db_backup = dbe_backup_initwithcallback(
                            db->db_go->go_cfg,
                            db->db_go->go_syscd,
                            db->db_dbfile,
                            db->db_go->go_ctr,
                            callbackfp,
                            callbackctx,
                            replicap,
                            backuplogmode,
#ifdef SS_HSBG2
                            dbe_hsbg2_logging_enabled(db->db_hsbg2svc),
                            lp,
#endif /* SS_HSBG2 */
                            &rc,
                            p_errh);

        if (db->db_backup != NULL) {
            /* Backup started succesfully. Otherwise dbe_backup_init
             * returned error code in rc.
             */
            rc = DBE_RC_SUCC;
        }

        SS_PMON_SET(SS_PMON_BACKUPACT, 1);

        SsSemExit(db->db_sem);

        return(rc);
}

dbe_ret_t dbe_db_backupsetmmecallback(
        dbe_db_t*       db,
        su_ret_t        (*callbackfp)(
                void*                   ctx,
                dbe_backupfiletype_t    ftype,
                void*                   data,
                size_t                  len),
        void*           callbackctx)
{
        CHK_DB(db);

        if (callbackfp == NULL) {
            db->db_backupmme_cb = NULL;
            db->db_backupmmectx = NULL;
        } else {
            ss_dassert(db->db_backupmme_cb == NULL);

            db->db_backupmme_cb = callbackfp;
            db->db_backupmmectx = callbackctx;
        }

        return DBE_RC_SUCC;
}

void dbe_db_backuplogfnumrange(
        dbe_db_t* db,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end)
{
#ifdef SS_HSBG2
        dbe_catchup_logpos_t lp;
#endif /* SS_HSBG2 */

        CHK_DB(db);
        ss_dassert(db->db_backup != NULL);

#ifdef SS_HSBG2
        lp = dbe_catchup_logpos_getfirstusedlogpos(db);
#endif /* SS_HSBG2 */

        dbe_backup_getlogfnumrange(
                db->db_backup,
                p_logfnum_start,
                p_logfnum_end);

#ifdef SS_HSBG2
        if(lp.lp_logfnum < *p_logfnum_start) {
            *p_logfnum_start = lp.lp_logfnum;
        }
#endif /* SS_HSBG2 */
}

/*##**********************************************************************\
 *
 *              dbe_db_backupstop
 *
 *
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_backupstop(dbe_db_t* db)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        ss_dassert(db->db_backup != NULL);
        ss_dassert(!db->db_cpactive);

        dbe_backup_done(db->db_backup);
        db->db_backup = NULL;
        db->db_prevlogsizemeastime = (SsTimeT)0L;

        SS_PMON_SET(SS_PMON_BACKUPACT, 0);

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_backupadvance
 *
 *
 *
 * Parameters :
 *
 *      db -
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
dbe_ret_t dbe_db_backupadvance(
        dbe_db_t* db,
        rs_err_t** p_errh)
{
        dbe_ret_t rc;

        CHK_DB(db);
        ss_dassert(db->db_backup != NULL);
        ss_dassert(!db->db_cpactive);

        rc = dbe_backup_advance(db->db_backup, p_errh);

        return(rc);
}

/*##**********************************************************************\
 *
 *              dbe_db_getcurbackupdir
 *
 * Returns the current backup directpry of an active backup.
 *
 * Parameters :
 *
 *      db - in
 *              Database object.
 *
 * Return value - ref :
 *
 *      backup directory of currently active backup.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* dbe_db_getcurbackupdir(dbe_db_t* db)
{
        char* dir;

        CHK_DB(db);
        ss_dassert(db->db_backup != NULL);
        ss_dassert(!db->db_cpactive);

        dir = dbe_backup_getcurdir(db->db_backup);

        return(dir);
}

/*##**********************************************************************\
 *
 *              dbe_db_givedefbackupdir
 *
 * Returns default backup directory.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 * Return value - give :
 *
 *      Backup directory allocated by SsMemAlloc.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
char* dbe_db_givedefbackupdir(dbe_db_t* db)
{
        char* dir;

        CHK_DB(db);

        dbe_cfg_getbackupdir(
            db->db_go->go_cfg,
            &dir);

        return(dir);
}

#endif /* SS_NOBACKUP */

/*##**********************************************************************\
 *
 *              dbe_db_blocksize
 *
 * The block size in bytes
 *
 * Parameters :
 *
 *      db - in, use
 *
 *
 * Return value :
 *
 *      block size
 *
 * Limitations  :
 *
 * Globals used :
 */
uint dbe_db_blocksize(dbe_db_t* db)
{
        uint blocksize;
        CHK_DB(db);

        blocksize = (uint)db->db_dbfile->f_indexfile->fd_blocksize;

        return(blocksize);
}

/*##**********************************************************************\
 *
 *              dbe_db_poolsize
 *
 * Gets size of buffer pool for index file
 *
 * Parameters :
 *
 *      db - in, use
 *              db object
 *
 * Return value :
 *      index cache size in bytes
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
size_t dbe_db_poolsize(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_poolsize);
}

/*##**********************************************************************\
 *
 *              dbe_db_poolsizeforquery
 *
 * The buffer pool size in bytes for a single query.
 *
 * Parameters :
 *
 *      db - in, use
 *
 *
 * Return value :
 *
 *      the pool size
 *
 * Limitations  :
 *
 * Globals used :
 */
long dbe_db_poolsizeforquery(dbe_db_t* db)
{
        double avgnsearch;
        double dbsizekb;
        double poolsizekb;
        double poolsizekbforquery;

        CHK_DB(db);

        SsSemEnter(db->db_nsearchsem);
        avgnsearch = db->db_avgnsearch;
        SsSemExit(db->db_nsearchsem);

        dbsizekb = (double)dbe_db_getdbsize(db);
        poolsizekb = (double)db->db_poolsize / 1024.0;

        if (poolsizekb >= dbsizekb
            || (1 - (poolsizekb / dbsizekb)) < 0.1
            || avgnsearch < 0.1)
        {
            poolsizekbforquery = poolsizekb;
        } else {
            poolsizekbforquery = (poolsizekb / (avgnsearch + 1.0))
                                 * sqrt(avgnsearch * (1 - (poolsizekb / dbsizekb)));
            if (poolsizekbforquery > poolsizekb) {
                poolsizekbforquery = poolsizekb;
            }
        }

        ss_dprintf_4(("dbe_db_poolsizeforquery:poolsize=%ld, avgnsearch=%.1lf\n", (long)(poolsizekbforquery * 1024.0), avgnsearch));

        return((long)(poolsizekbforquery * 1024.0));
}

/*##**********************************************************************\
 *
 *              dbe_db_addcfgtocfgl
 *
 * Adds configuration parameter into to configuration list.
 *
 * Parameters :
 *
 *      db - in
 *
 *
 *      cfgl - use
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
void dbe_db_addcfgtocfgl(
        dbe_db_t* db,
        su_cfgl_t* cfgl)
{
        CHK_DB(db);

        dbe_cfg_addtocfgl(db->db_go->go_cfg, cfgl);
}

#ifndef SS_LIGHT
/*##**********************************************************************\
 *
 *              dbe_db_printinfo
 *
 *
 *
 * Parameters :
 *
 *      fp -
 *
 *
 *      db -
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
void dbe_db_printinfo(void* fp, dbe_db_t* db)
{
        uint i;
        dbe_user_t* user;

        CHK_DB(db);

        SsFprintf(fp, "Database create timestamp %ld\n",
            dbe_header_getcreatime(db->db_dbfile->f_indexfile->fd_dbheader));
        SsFprintf(fp, "%-4s %-7s %-5s %-8s %-6s %-3s %-4s %-3s %-5s %-9s %-7s %-7s\n",
            "NSea",
            "AvgNSea",
            "BlkSz",
            "PoolSz",
            "ErlVld",
            "Mrg",
            "Qmrg",
            "Bak",
            "RdOnl",
            "DbSize",
            "LogSize",
            "NBlbBlk");
        SsFprintf(fp, "%-4d %-7.1lf %-5d %-8ld %-6d %-3d %-4d %-3d %-5d %-9ld %-7ld %-7ld\n",
            db->db_nsearch,
            db->db_avgnsearch,
            dbe_db_blocksize(db),
            db->db_poolsize/1024L,
            db->db_earlyvld,
            db->db_indmerge != NULL,
            db->db_quickmerge != NULL,
            db->db_backup != NULL,
            db->db_readonly,
            dbe_db_getdbsize(db),
            dbe_db_getlogsize(db),
            dbe_blob_nblock);
        SsFprintf(fp, "%-8s %-6s %-6s %-6s %-8s %-6s %-5s %-7s\n",
            "TotIdxWr",
            "NIdxWr",
            "NMrgWr",
            "MrgLim",
            "TotLogWr",
            "NLogWr",
            "CpLim",
            "QMrgLim");
        SsFprintf(fp, "%-8ld %-6ld %-6ld %-6ld %-8ld %-6ld %-5ld %-7ld\n",
            db->db_go->go_ntotindexwrites,
            db->db_go->go_nindexwrites,
            db->db_go->go_nmergewrites,
            db->db_mergelimit,
            db->db_go->go_ntotlogwrites,
            db->db_go->go_nlogwrites,
            db->db_cplimit,
            db->db_quickmergelimit);
        SsFprintf(fp, "%-9s %-9s %-9s %-9s %-9s\n",
            "CommitCnt",
            "AbortCnt",
            "RollbCnt",
            "StmtCnt",
            "RdonlyCnt");
        SsFprintf(fp, "%-9ld %-9ld %-9ld %-9ld %-9ld\n",
            db->db_go->go_trxstat.ts_commitcnt,
            db->db_go->go_trxstat.ts_abortcnt,
            db->db_go->go_trxstat.ts_rollbackcnt,
            db->db_go->go_trxstat.ts_stmtcnt,
            db->db_go->go_trxstat.ts_readonlycnt);
        SsFprintf(fp, "MERGE INFO:\n");
        SsFprintf(fp, "MergeRounds %ld QuickMergeRounds %ld\n",
            db->db_go->go_quickmergerounds,
            db->db_go->go_mergerounds);
        SsFprintf(fp, "MergeWrites %ld MergeLimit %ld QuickMergeLimit %ld\n",
            db->db_go->go_nmergewrites,
            db->db_mergelimit,
            db->db_quickmergelimit);
        SsFprintf(fp, "MergeLevel %ld\n",
            DBE_TRXNUM_GETLONG(dbe_counter_getmergetrxnum(db->db_go->go_ctr)));
        SsFprintf(fp, "%-8s %-13s\n",
            "SplitCnt", "SplitAvoidCnt");
        SsFprintf(fp, "%-8ld %-13ld\n",
            db->db_go->go_splitcount,
            db->db_go->go_splitavoidcount);
        SsFprintf(fp, "USERS:\n");
        SsSemEnter(db->db_sem);
        su_pa_do_get(db->db_users, i, user) {
            dbe_user_printinfo(fp, user);
        }
        SsSemExit(db->db_sem);
        SsFprintf(fp, "RELATION BUFFER:\n");
        rs_rbuf_printinfo(fp, db->db_rbuf);
        SsFprintf(fp, "INDEX:\n");
        dbe_index_printinfo(fp, db->db_index);
        SsFprintf(fp, "COUNTERS:\n");
        dbe_counter_printinfo(fp, db->db_go->go_ctr);
        SsFprintf(fp, "GLOBAL TRANSACTION STATE (gtrs):\n");
        dbe_gtrs_printinfo(fp, db->db_go->go_gtrs);
        SsFprintf(fp, "LOCK MANAGER:\n");
        dbe_lockmgr_printinfo(fp, db->db_lockmgr);
#ifdef SS_BETA
        SsFprintf(fp, "LOG RECORD INFO:\n");
        dbe_logi_printfinfo(fp);
#endif /* BETA */
}

/*##**********************************************************************\
 *
 *              dbe_db_getlockcount
 *
 *
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getlockcount(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(dbe_lockmgr_getlockcount(db->db_lockmgr));
}

/*##**********************************************************************\
 *
 *              dbe_db_setlockdisablerowspermessage
 *
 * Enables or disables rows per message optimization when a row is
 * locked. By default rows per message is enabled.
 *
 * Parameters :
 *
 *      db - in, use
 *              Database object.
 *
 *      lockdisablerowspermessage - in
 *              If TRUE, rows per message is disabled. If FALSE, it is enabled.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_setlockdisablerowspermessage(
        dbe_db_t* db,
        bool lockdisablerowspermessage)
{
        CHK_DB(db);

        dbe_lockmgr_setlockdisablerowspermessage(
            db->db_lockmgr,
            lockdisablerowspermessage);
}

/*##**********************************************************************\
 *
 *              dbe_db_getmergecount
 *
 *
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getmergecount(
        dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_go->go_mergerounds + db->db_go->go_quickmergerounds);
}


/*##**********************************************************************\
 *
 *              dbe_db_printtree
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      values -
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
void dbe_db_printtree(
        dbe_db_t* db,
        bool values)
{
        CHK_DB(db);

        dbe_index_print(db->db_index, values);
}

void dbe_db_errorprinttree(
        dbe_db_t* db,
        bool values)
{
        CHK_DB(db);

        dbe_db_setreadonly(db, TRUE);
        dbe_db_setmergedisabled(db, TRUE);
        SsDbgSet("/UNL/NOTASK/NOSQL/LEV:0");
        SsThrSleep(10);
        SsDbgSet("/ASSERTSTOP");
        SsDbgPrintf("dbe_db_errorprinttree\n");
        dbe_index_print(db->db_index, values);
}

/*##**********************************************************************\
 *
 *              dbe_db_checkindex
 *
 *
 *
 * Parameters :
 *
 *      db - in
 *              Database object.
 *
 *      full_check - in
 *              If TRUE, check also index leaf content.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_checkindex(
        dbe_db_t* db,
        bool silentp,
        bool full_check)
{
        bool succp;

        CHK_DB(db);

        dbe_db_enteraction(db, NULL);

        dbe_debug = TRUE;
        if (silentp) {
            succp = dbe_index_check(db->db_index, full_check);
        } else {
            succp = dbe_index_printfp((void*)-1L, db->db_index, full_check);
        }
        dbe_debug = FALSE;

        dbe_db_exitaction(db, NULL);

        return(succp);
}

/*#***********************************************************************\
 *
 *              print_buf
 *
 *
 *
 * Parameters :
 *
 *      buf -
 *
 *
 *      len -
 *
 *
 * Return value :
 *
 * Limitations  :
 *
 * Globals used :
 */
static void print_buf(char* buf, int len)
{
        int i;
        char tmp[80];
        char str[255];

        str[0] = '\0';

        for (i = 0; i < len; i++) {
            SsSprintf(tmp, "%02x", buf[i] & 0xff);
            strcat(str, tmp);
        }
        strcat(str, " ");
        for (i = 0; i < len; i++) {
            SsSprintf(tmp, "%c",
                ((ss_byte_t)buf[i] > 127 || ss_isalnum(buf[i])) ? buf[i] : '.');
            strcat(str, tmp);
        }
        SsPrintf("%s", str);
}

/*#***********************************************************************\
 *
 *              blocktypename
 *
 *
 *
 * Parameters :
 *
 *      blocktype -
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
static const char* blocktypename(dbe_blocktype_t blocktype)
{
        const char* bltype;

        switch (blocktype) {
            case DBE_BLOCK_FREE:
                bltype = "free block (for debug purposes)";
                break;
            case DBE_BLOCK_FREELIST:
                bltype = "free list block";
                break;
            case DBE_BLOCK_CHANGELIST:
                bltype = "change list from checkpoint";
                break;
            case DBE_BLOCK_BADLIST:
                bltype = "bad block list";
                break;
            case DBE_BLOCK_CPLIST:
                bltype = "live checkpoint list";
                break;
            case DBE_BLOCK_TRXLIST:
                bltype = "transaction list";
                break;
            case DBE_BLOCK_CPRECORD:
                bltype = "checkpoint record";
                break;
            case DBE_BLOCK_SSRECORD:
                bltype = "snapshot record";
                break;
            case DBE_BLOCK_DBHEADER:
                bltype = "database header record";
                break;
            case DBE_BLOCK_BLOBLIST:
                bltype = "blob allocation list block";
                break;
            case DBE_BLOCK_BLOBDATA:
                bltype = "blob data block";
                break;
            case DBE_BLOCK_TREENODE:
                bltype = "B+-tree node";
                break;
            case DBE_BLOCK_BLOB1ST:
                bltype = "blob 1st block";
                break;
            case DBE_BLOCK_STMTTRXLIST:
                bltype = "transaction statement list";
                break;
            case DBE_BLOCK_SEQLIST:
                bltype = "sequence counter list";
                break;
            case DBE_BLOCK_RTRXLIST:
                bltype = "replication transaction list";
                break;
            case DBE_BLOCK_FREECACHEPAGE:
                bltype = "free cache page (for debug purposes)";
                break;
            case DBE_BLOCK_BLOBG2PAGE:
                bltype = "BLOB (G2) page";
                break;
            case DBE_BLOCK_MMESTORAGE:
                bltype = "MME storage page";
                break;
            case DBE_BLOCK_MMEPAGEDIR:
                bltype = "MME page directory";
                break;
            default:
                bltype = NULL;
                break;
        }
        return(bltype);
}

/*##**********************************************************************\
 *
 *              dbe_db_checkdbfile
 *
 *
 *
 * Parameters :
 *
 *      filename -
 *
 *
 *      blocksize -
 *
 *
 *      startblock -
 *
 *
 *      data -
 *
 *
 *      report_only -
 *
 *
 *      content -
 *
 *
 *      only_one_block -
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
bool dbe_db_checkdbfile(
        long startblock,
        bool data,
        bool report_only,
        bool content,
        bool only_one_block)
{
        char* buf;
        su_daddr_t addr;
        dbe_cpnum_t cpnum;
        dbe_blocktype_t blocktype;
        dbe_blocktype_t dbblocktype;
        char* bltype;
        uint i;
        int j;
        int sizeread;
        SS_FILE* fp;
        long totalblocks = 0;
        long nblocks[DBE_BLOCK_LASTNOTUSED + 1];
        long totalleaflength = 0;
        long leaflength = 0;
        su_inifile_t* inifile;
        bool succp;
        size_t blocksize;
        su_pa_t* files;
        dbe_cfg_t* cfg;
        char* dbfilename;
        dbe_filespec_t* filespec;
        uint fileindex;

#       define OUTPUT_BYTES    20

        inifile = su_inifile_init(SU_SOLINI_FILENAME, &succp);
        cfg = dbe_cfg_init(inifile);
        dbe_cfg_register_su_params(cfg);
        files = su_pa_init();

        dbe_cfg_getidxfilespecs(cfg, files);
        dbe_cfg_getidxblocksize(cfg, &blocksize);

        filespec = su_pa_getdata(files, 0);
        dbfilename = dbe_filespec_getname(filespec);
        dbe_header_readblocksize(dbfilename, &blocksize);

        SsDbgMessage("Checking database blocks:\n");

        for (i = 0; i < sizeof(nblocks) / sizeof(nblocks[0]); i++) {
             nblocks[i] = 0;
        }

        succp = TRUE;
        buf = SsMemAlloc(blocksize);

        su_pa_do_get(files, fileindex, filespec) {
            dbfilename = dbe_filespec_getname(filespec);
            fp = SsFOpenB(dbfilename, (char *)"r");
            if (fp == NULL) {
                continue;
            }

            if (SsFSeek(fp, startblock * blocksize, SEEK_SET) != 0) {
                SsDbgMessage("SOLID Fatal Error: Failed to seek to address %ld in file '%s'\n",
                    startblock, dbfilename);
                return(FALSE);
            }

            for (addr = startblock; ; addr++) {
                SsThrSwitch();  /* Needed for Novell-version */
                sizeread = SsFRead(buf, 1, blocksize, fp);
                if (sizeread < (int) blocksize) {
                    break;
                }

                DBE_BLOCK_GETTYPE(buf, &blocktype);
                DBE_BLOCK_GETCPNUM(buf, &cpnum);
                dbblocktype = blocktype;
                bltype = (char *)blocktypename(blocktype);
                if (bltype == NULL) {
                    succp = FALSE;
                    blocktype = DBE_BLOCK_LASTNOTUSED;
                    bltype = (char *)"BAD BLOCK";
                }
                totalblocks++;
                nblocks[blocktype]++;
                if (blocktype == DBE_BLOCK_TREENODE) {
                    leaflength = dbe_bnode_getlength(buf);
                    totalleaflength += leaflength;
                }

                if (!report_only || blocktype == DBE_BLOCK_LASTNOTUSED) {
                    char buf[80];
                    SsSprintf(buf, "Block: %4ld, cpnum: %4ld, type: %s (typenum: %d)",
                        (long)addr, (long)cpnum, bltype, (int)dbblocktype);
                    if (blocktype == DBE_BLOCK_LASTNOTUSED) {
                        /* Bad block */
                        SsDbgMessage("%s", buf);
                    } else {
                        SsPrintf("%s", buf);
                    }
                    if (blocktype == DBE_BLOCK_TREENODE) {
                        SsPrintf(", len: %ld", leaflength);
                    }
                    SsPrintf("\n");
                }

                if (data) {
                    SsPrintf("\n");
                    for (j = 0; j < (int) blocksize; j += OUTPUT_BYTES) {
                        SsPrintf("%5ld %5lx ",
                            addr * blocksize + j,
                            addr * blocksize + j);
                        print_buf(
                            buf + j,
                            (int)(j > (int) blocksize - OUTPUT_BYTES
                                ? blocksize - j
                                : OUTPUT_BYTES));
                        SsPrintf("\n");
                    }
                    SsPrintf("\n");
                }
                if (content) {
                    switch (blocktype) {
                        case DBE_BLOCK_TREENODE:
                            if (!dbe_bnode_print(NULL, buf, blocksize)) {
                                nblocks[DBE_BLOCK_LASTNOTUSED]++;
                            }
                            break;
                        default:
                            break;
                    }
                    SsPrintf("\n");
                }
                if (only_one_block) {
                    break;
                }
            }
            SsFClose(fp);
            dbe_filespec_done(filespec);
        }
        SsMemFree(buf);
        dbe_cfg_done(cfg);
        su_inifile_done(inifile);
        su_pa_done(files);

        if (totalblocks == 0) {
            succp = FALSE;
        }

        SsDbgMessage("\
Report of database block types:\n\
%%     Count  Type:\n\
------ ------ ----\n");

        for (i = 0; i < sizeof(nblocks) / sizeof(nblocks[0]); i++) {
            if (i == DBE_BLOCK_LASTNOTUSED) {
                bltype = (char *)"BAD BLOCK";
            } else {
                bltype = (char *)blocktypename((dbe_blocktype_t)i);
            }
            ss_dassert(bltype != NULL);

            SsDbgMessage("%5.1lf%% %6ld %s",
                nblocks[i] * 100.0 / totalblocks,
                nblocks[i],
                bltype);
            if (i == DBE_BLOCK_TREENODE) {
                double perc;
                if (nblocks[i] == 0) {
                    ss_dassert(totalleaflength == 0);
                    perc = 0.0;
                } else {
                    perc = ((double)totalleaflength * 100.0)
                           / ((double)nblocks[i] * (double)blocksize);
                }
                SsDbgMessage(" (filled %.1lf%%)", perc);
            }
            SsDbgMessage("\n");
        }
        SsDbgMessage("\
------ ------ ----\n\
%5.1lf%% %6ld  %s\n",
                100.0, totalblocks, "Total");

        return(succp);
}

/*##**********************************************************************\
 *
 *              dbe_db_filldbblock
 *
 * Fills a database block with empty information. Block type and
 * address are given in params string.
 *
 * Parameters :
 *
 *      params - in
 *              Block information in format <type>,<address>
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_filldbblock(
        char* params)
{
        bool succp;
        su_daddr_t addr;
        su_daddr_t org_addr;
        dbe_blocktype_t blocktype;
        size_t blocksize;
        su_inifile_t* inifile;
        su_pa_t* files;
        dbe_cfg_t* cfg;
        char* dbfilename = NULL;
        dbe_filespec_t* filespec;
        uint fileindex;
        SS_FILE* fp;
        char* buf;

        ss_dprintf_1(("dbe_db_filldbblock:params = '%ld'\n", params));

        switch (ss_toupper(params[0])) {
            case 'I':
                blocktype = DBE_BLOCK_TREENODE;
                break;
            default:
                SsDbgMessage("SOLID Fatal Error: Unsupported type '%c'\n",
                    params[0]);
                return(FALSE);
        }

        while (*params != '\0' && !ss_isdigit(*params)) {
            params++;
        }
        if (*params == '\0') {
            SsDbgMessage("SOLID Fatal Error: No address specified\n");
            return(FALSE);
        }
        succp = SsStrScanLong(params, (long*)&addr, &params);
        if (!succp) {
            SsDbgMessage("SOLID Fatal Error: No valid address specified\n");
            return(FALSE);
        }
        org_addr = addr;

        inifile = su_inifile_init(SU_SOLINI_FILENAME, &succp);
        cfg = dbe_cfg_init(inifile);
        dbe_cfg_register_su_params(cfg);
        files = su_pa_init();

        dbe_cfg_getidxfilespecs(cfg, files);
        dbe_cfg_getidxblocksize(cfg, &blocksize);

        filespec = su_pa_getdata(files, 0);
        dbfilename = dbe_filespec_getname(filespec);
        dbe_header_readblocksize(dbfilename, &blocksize);

        /* Find the correct file.
         */
        succp = FALSE;
        su_pa_do_get(files, fileindex, filespec) {
            ulong filesize;
            dbfilename = dbe_filespec_getname(filespec);
            filesize = SsFSize(dbfilename);
            ss_dprintf_2(("dbe_db_filldbblock:dbfilename='%s', filesize=%ld, addr=%ld\n",
                dbfilename, filesize, addr));
            if (filesize <= 0) {
                SsDbgMessage("SOLID Fatal Error: Address %ld not found\n", org_addr);
                return(FALSE);
            }
            if (addr * blocksize < filesize) {
                succp = TRUE;
                break;
            }
            addr -= (filesize / blocksize);
            dbe_filespec_done(filespec);
        }
        su_pa_done(files);

        if (!succp) {
            SsDbgMessage("SOLID Fatal Error: Address %ld not found\n", org_addr);
            return(FALSE);
        }

        fp = SsFOpenB(dbfilename, (char *)"r+");
        if (fp == NULL) {
            SsDbgMessage("SOLID Fatal Error: Failed to open file '%s'\n",
                dbfilename);
            return(FALSE);
        }

        if (SsFSeek(fp, addr * blocksize, SEEK_SET) != 0) {
            SsDbgMessage("SOLID Fatal Error: Failed to seek to address %ld in file '%s'\n",
                addr, dbfilename);
            return(FALSE);
        }

        buf = SsMemCalloc(blocksize, 1);
        succp = FALSE;

        switch (blocktype) {
            case DBE_BLOCK_TREENODE:
                dbe_bnode_initempty(buf);
                succp = TRUE;
                break;
            default:
                ss_derror;
        }

        if (succp) {
            SsFWrite(buf, 1, blocksize, fp);
        }

        succp = !ferror(fp);

        SsFClose(fp);
        SsMemFree(buf);
        dbe_cfg_done(cfg);
        su_inifile_done(inifile);

        return(succp);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdbsize
 *
 * Returns database size in kilobytes.
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getdbsize(
        dbe_db_t* db)
{
        double block_size_in_kbytes;
        double dbsize;

        CHK_DB(db);

        block_size_in_kbytes = dbe_db_blocksize(db) / (double)1024;
        dbsize = su_svf_getsize(db->db_dbfile->f_indexfile->fd_svfil) *
                 block_size_in_kbytes;

        return((ulong)dbsize);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdbfreeblocks
 *
 * Returns number of free blocks in database.
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getdbfreeblocks(
        dbe_db_t* db)
{
        ulong nfreeblocks;

        CHK_DB(db);

        ss_dprintf_1(("dbe_db_getdbfreeblocks\n"));

        nfreeblocks = dbe_fl_getfreeblocks(
                                db->db_dbfile->f_indexfile->fd_freelist);
        ss_dprintf_2(("dbe_db_getdbfreeblocks:nfreeblocks = %lu\n", nfreeblocks));

        return(nfreeblocks);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdbfreesize
 *
 * Returns free area size in database.
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getdbfreesize(
        dbe_db_t* db)
{
        ulong ret;
        double nfreeblocks;
        double freesize;

        CHK_DB(db);

        ss_dprintf_1(("dbe_db_getdbfreesize\n"));

        nfreeblocks = (double)dbe_db_getdbfreeblocks(db);

        ss_dprintf_2(("dbe_db_getdbfreesize:nfreeblocks = %.2lf\n", nfreeblocks));

        freesize = nfreeblocks * dbe_db_blocksize(db) / (double)1024;
        ss_dprintf_2(("dbe_db_getdbfreesize:freesize = %.2lf\n", freesize));

        ret = (ulong)freesize;
        ss_dprintf_2(("dbe_db_getdbfreesize:return = %lu\n", ret));

        return(ret);
}

/*##**********************************************************************\
 *
 *              dbe_db_getlogsize
 *
 * Returns log size in kilobytes.
 *
 * Parameters :
 *
 *      db -
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
ulong dbe_db_getlogsize(
        dbe_db_t* db)
{
        SsTimeT t;
        CHK_DB(db);

        if (db->db_dbfile->f_log == NULL) {
            db->db_prevlogsize = 0L;
        } else {
#ifndef SS_NOLOGGING
            t = SsTime(NULL);
            if (t - db->db_prevlogsizemeastime >=
                DB_MIN_LOGSIZE_MEAS_INTERVAL_IN_SECS)
            {
                db->db_prevlogsizemeastime = t;
                db->db_prevlogsize = dbe_log_getsize(db->db_dbfile->f_log, FALSE);
            }
#endif /* SS_NOLOGGING */
        }
        return (db->db_prevlogsize);
}

/*##**********************************************************************\
 *
 *              dbe_db_getstat
 *
 * Stores database statistics into *p_dbst.
 *
 * Parameters :
 *
 *      db - in
 *              Database object.
 *
 *      p_dbst - out
 *              Statistics information is stored into *p_dbst.
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_getstat(
        dbe_db_t* db,
        dbe_dbstat_t* p_dbst)
{
        dbe_cache_info_t cache_info;

        CHK_DB(db);
        ss_dassert(p_dbst != NULL);

        dbe_cache_getinfo(
            db->db_dbfile->f_indexfile->fd_cache,
            &cache_info);

        SsSemEnter(db->db_sem);

        p_dbst->dbst_trx_commitcnt = db->db_go->go_trxstat.ts_commitcnt;
        p_dbst->dbst_trx_abortcnt = db->db_go->go_trxstat.ts_abortcnt;
        p_dbst->dbst_trx_rollbackcnt = db->db_go->go_trxstat.ts_rollbackcnt;
        p_dbst->dbst_trx_readonlycnt = db->db_go->go_trxstat.ts_readonlycnt;
        p_dbst->dbst_trx_bufcnt = dbe_trxbuf_getcount(db->db_go->go_trxbuf);
        dbe_gtrs_getcount(
            db->db_go->go_gtrs,
            &p_dbst->dbst_trx_activecnt,
            &p_dbst->dbst_trx_validatecnt);

        p_dbst->dbst_cac_findcnt = cache_info.cachei_nfind;
        p_dbst->dbst_cac_readcnt = cache_info.cachei_nread;
        p_dbst->dbst_cac_writecnt = cache_info.cachei_nwrite;
        p_dbst->dbst_cac_prefetchcnt = cache_info.cachei_nprefetch;
        p_dbst->dbst_cac_preflushcnt = cache_info.cachei_npreflush;

        p_dbst->dbst_ind_writecnt = db->db_go->go_ntotindexwrites;
        p_dbst->dbst_ind_writecntsincemerge = db->db_go->go_nindexwrites;
        p_dbst->dbst_ind_mergewrites = db->db_go->go_nmergewrites;
        p_dbst->dbst_ind_mergeact = (ulong)(db->db_indmerge != NULL || db->db_quickmerge != NULL);
        p_dbst->dbst_ind_filesize = dbe_db_getdbsize(db);
        p_dbst->dbst_ind_freesize = dbe_db_getdbfreesize(db);

        if (db->db_dbfile->f_log == NULL) {
            /* Log is not used. */
            p_dbst->dbst_log_writecnt = 0;
            p_dbst->dbst_log_writecntsincecp = 0;
            p_dbst->dbst_log_filewritecnt = 0;
        } else {
            p_dbst->dbst_log_writecnt = db->db_go->go_ntotlogwrites;
            p_dbst->dbst_log_writecntsincecp = db->db_go->go_nlogwrites;
            p_dbst->dbst_log_filewritecnt = dbe_log_getfilewritecnt(db->db_dbfile->f_log);
        }
        p_dbst->dbst_log_filesize = dbe_db_getlogsize(db);

        p_dbst->dbst_sea_activecnt = db->db_nsearch;
        p_dbst->dbst_sea_activeavg = (ulong)db->db_avgnsearch;

        p_dbst->dbst_cp_thruput = db->db_cp_thruput;

        SsSemExit(db->db_sem);
}

#endif /* SS_LIGHT */

#ifndef SS_NOLOGGING
/*##**********************************************************************\
 *
 *              dbe_dbe_setlogerrorhandler
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      errorfunc -
 *
 *
 *      errorctx -
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
void dbe_db_setlogerrorhandler(
        dbe_db_t* db,
        void (*errorfunc)(void*),
        void* errorctx)
{
        CHK_DB(db);
        if (db->db_dbfile->f_log != NULL) {
            dbe_log_seterrorhandler(
                db->db_dbfile->f_log,
                errorfunc,
                errorctx);
        }
}
#endif /* SS_NOLOGGING */

#ifndef SS_NOESTSAMPLES

/*##**********************************************************************\
 *
 *              dbe_db_getkeysamples
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      range_min -
 *
 *
 *      range_max -
 *
 *
 *      sample_vtpl -
 *
 *
 *      sample_size -
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
void dbe_db_getkeysamples(
        dbe_db_t* db,
        rs_sysi_t* cd,
        rs_relh_t* relh,
        vtpl_t* range_min,
        vtpl_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size)
{
        int i;
        dbe_btree_t* b;

        CHK_DB(db);
        SS_PUSHNAME("dbe_db_getkeysamples");

        SS_PMON_ADD(SS_PMON_ESTSAMPLESREAD);

        if (rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY) {
#ifdef SS_MME
            /* XXX - MME key samples. */
#else
            ;
#endif
        } else {

            b = dbe_index_getpermtree(db->db_index);

            dbe_btree_getkeysamples(
                b,
                range_min,
                range_max,
                sample_vtpl,
                sample_size,
                FALSE);

            /* Check if any samples were found. */
            for (i = 0; i < sample_size; i++) {
                if (sample_vtpl[i] != NULL) {
                    break;
                }
            }

            if (i == sample_size) {
                /* No samples found, try from the Bonsai-tree.
                */
                b = dbe_index_getbonsaitree(db->db_index);

                dbe_btree_getkeysamples(
                    b,
                    range_min,
                    range_max,
                    sample_vtpl,
                    sample_size,
                    FALSE);

            }
        }

        SS_POPNAME;
}

void dbe_db_getmergekeysamples(
        dbe_db_t* db,
        dynvtpl_t* sample_vtpl,
        int sample_size)
{
        dbe_btree_t* b;
        char buf_min[80];
        char buf_max[80];
        vtpl_t* range_min;
        vtpl_t* range_max;

        CHK_DB(db);
        SS_PUSHNAME("dbe_db_getmergekeysamples");

        range_min = (vtpl_t*)buf_min;
        range_max = (vtpl_t*)buf_max;

        vtpl_setvtpl(range_min, VTPL_EMPTY);
        vtpl_appva(range_min, &va_minint);
        vtpl_setvtpl(range_max, VTPL_EMPTY);
        vtpl_appva(range_max, &va_maxint);

        b = dbe_index_getbonsaitree(db->db_index);

        dbe_btree_getkeysamples(
            b,
            range_min,
            range_max,
            sample_vtpl,
            sample_size,
            TRUE);

        SS_POPNAME;
}

void dbe_db_gettvalsamples(
        dbe_db_t* db,
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_tval_t** sample_tvalarr,
        size_t sample_size)
{
        CHK_DB(db);
        SS_PUSHNAME("dbe_db_gettvalsamples");
        ss_bassert(rs_relh_reltype(cd, relh) == RS_RELTYPE_MAINMEMORY);
        if (rs_relh_istransient(cd, relh) ||
            rs_relh_isglobaltemporary(cd, relh)) {
            SS_POPNAME;
            return;
        }
        SS_PMON_ADD(SS_PMON_ESTSAMPLESREAD);

        dbe_mme_gettvalsamples_nomutex(cd,
                                       db->db_mme,
                                       relh,
                                       sample_tvalarr,
                                       sample_size);
        /* dbe_db_newplan(db, rs_relh_relid(cd, relh)); */

        SS_POPNAME;
}

#endif /* SS_NOESTSAMPLES */

int dbe_db_getequalrowestimate(
        rs_sysi_t* cd,
        dbe_db_t* db,
        vtpl_t* range_begin,
        vtpl_t* range_end)
{
        int estimate;

        estimate = dbe_btrsea_getequalrowestimate(
                        cd,
                        dbe_index_getpermtree(db->db_index),
                        range_begin,
                        range_end);

        ss_dprintf_1(("dbe_db_getequalrowestimate:estimate=%d\n", estimate));

        return(estimate);
}

/*##**********************************************************************\
 *
 *              dbe_db_getcreatime
 *
 * Gets database file creation timestamp as 4-byte integer (SsTimeT)
 *
 * Parameters :
 *
 *      db - in, use
 *              database object
 *
 * Return value :
 *      timestamp value (seconds since Jan 1,1970 00:00:00 GMT)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SsTimeT dbe_db_getcreatime(
        dbe_db_t* db)
{
        SsTimeT t;
        dbe_header_t* dbheader;

        CHK_DB(db);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        t = (SsTimeT)dbe_header_getcreatime(dbheader);
        return (t);
}


/*##**********************************************************************\
 *
 *		dbe_db_gethsbtime
 *
 * Gets hsb time stamt. This is last time when database was detected as
 * out of sync.
 *
 * Parameters :
 *
 *		db -
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
SsTimeT dbe_db_gethsbtime(
        dbe_db_t* db)
{
        SsTimeT t;
        dbe_header_t* dbheader;

        CHK_DB(db);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        t = (SsTimeT)dbe_header_gethsbtime(dbheader);
        return (t);
}

/*##**********************************************************************\
 *
 *		dbe_db_sethsbtime_outofsync
 *
 * Sets database hsb time which means this database is out of sync and
 * netcopy is only way to continue hsb.
 *
 * Parameters :
 *
 *		db -
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
void dbe_db_sethsbtime_outofsync(
        dbe_db_t* db)
{
        ss_uint4_t oldhsbtime;
        ss_uint4_t newhsbtime;
        dbe_header_t* dbheader;

        CHK_DB(db);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        newhsbtime = (ss_uint4_t)SsTime(NULL);
        oldhsbtime = dbe_header_gethsbtime(dbheader);
        if (newhsbtime == oldhsbtime) {
            /* If clock is not working properly we may get the same time.
             * Ensure that new time is different that the old one.
             */
            newhsbtime++;
        }
        dbe_header_sethsbtime(dbheader, newhsbtime);
        dbe_file_saveheaders(db->db_dbfile);
}

/*##**********************************************************************\
 *
 *              dbe_db_getctc
 *
 * Gets Creation Timestamp check value (for paranoid license check)
 *
 * Parameters :
 *
 *      db - in, use
 *              database object
 *
 * Return value :
 *      CRC of creation timestamp
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
ss_uint4_t dbe_db_getctc(
        dbe_db_t* db)
{
        ss_uint4_t crc;
        dbe_header_t* dbheader;

        CHK_DB(db);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        crc = dbe_header_getctc(dbheader);
        return (crc);
}

/*##**********************************************************************\
 *
 *              dbe_db_getbufpool
 *
 * Gets a reference to database buffer pool
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 * Return value - ref :
 *      pointer to buffer pool
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_bufpool_t* dbe_db_getbufpool(dbe_db_t* db)
{
        CHK_DB(db);
        ss_dassert(db->db_dbfile != NULL);
        ss_dassert(db->db_dbfile->f_indexfile != NULL);
        ss_dassert(db->db_dbfile->f_indexfile->fd_cache != NULL);
        return ((dbe_bufpool_t*)db->db_dbfile->f_indexfile->fd_cache);
}

/*##**********************************************************************\
 *
 *              dbe_db_releasebufpool
 *
 * Tells that the bufpool is no longer needed by the caller
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 *      bufpool - in, take
 *              buffer pool
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_db_releasebufpool(dbe_db_t* db, dbe_bufpool_t* bufpool)
{
        CHK_DB(db);
        ss_dassert(bufpool != NULL);
        SS_NOTUSED(db);
        SS_NOTUSED(bufpool);
}

/*##**********************************************************************\
 *
 *              dbe_bufpool_alloc
 *
 * Allocates a buffer from buffer pool
 *
 * Parameters :
 *
 *      bufpool - use
 *              buffer pool
 *
 *      pp_buf - out
 *              pointer to pointer where the buffer start address
 *          will be stored
 *
 * Return value - give :
 *      buffer handle (not a writable memory address)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_hbuf_t* dbe_bufpool_alloc(dbe_bufpool_t* bufpool, void* pp_buf)
{
        dbe_hbuf_t* hbuf;

        ss_dassert(bufpool != NULL);
        hbuf = (dbe_hbuf_t*)dbe_cache_alloc((dbe_cache_t*)bufpool,
                                (char**)pp_buf);
        return (hbuf);
}

/*##**********************************************************************\
 *
 *              dbe_bufpool_free
 *
 * Frees a buffer handle
 *
 * Parameters :
 *
 *      bufpool - use
 *              buffer pool object
 *
 *      hbuf - in, take
 *              buffer handle
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void dbe_bufpool_free(dbe_bufpool_t* bufpool, dbe_hbuf_t* hbuf)
{
        ss_dassert(bufpool != NULL);
        ss_dassert(hbuf != NULL);
        dbe_cache_free((dbe_cache_t*)bufpool, (dbe_cacheslot_t*)hbuf);
}

/*##**********************************************************************\
 *
 *              dbe_hbuf_getbuf
 *
 * Gets a pointer to buffer from a buffer handle
 *
 * Parameters :
 *
 *      hbuf - use
 *              buffer handle
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
void* dbe_hbuf_getbuf(dbe_hbuf_t* hbuf)
{
        void* p;

        ss_dassert(hbuf != NULL);
        p = dbe_cacheslot_getdata((dbe_cacheslot_t*)hbuf);
        return (p);
}

#ifdef DBE_REPLICATION

/*##**********************************************************************\
 *
 *              dbe_db_setreplication
 *
 * Sets callback function use for replication.
 *
 * Parameters :
 *
 *      db - use
 *
 *      enable - in
 *          TRUE means enable HSB, FALSE means not
 *
 *      replicatefun - in, hold
 *          pointer to callback for replication
 *
 *      rep_ctx - in, hold
 *          context for callback function
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
void dbe_db_setreplication(
        dbe_db_t* db,
        bool enable,
        dbe_ret_t (*replicatefun)(
                        void* rep_ctx,
                        rep_type_t type,
                        rep_params_t* rp),
        dbe_ret_t (*commitreadyfun)(dbe_trxid_t trxid),
        void (*commitdonefun)(dbe_trxid_t trxid, bool commit),
        void* rep_ctx)
{
        CHK_DB(db);

        db->db_hsbenabled = enable;
        db->db_replicatefun = replicatefun;
        db->db_commitreadyfun = commitreadyfun;
        db->db_commitdonefun = commitdonefun;
        db->db_repctx = rep_ctx;
}

/*##**********************************************************************\
 *
 *              dbe_db_setrepctx
 *
 * Sets replication context.
 *
 * Parameters :
 *
 *      db - use
 *
 *      rep_ctx - in, hold
 *          replication context
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
void dbe_db_setrepctx(
        dbe_db_t* db,
        void* rep_ctx)
{
        CHK_DB(db);

        db->db_repctx = rep_ctx;
}

/*##**********************************************************************\
 *
 *              dbe_db_replicate
 *
 * Executes one replication operation.
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      type -
 *
 *
 *      rp -
 *
 *
 * Return value :
 *
 *      Replication return code or DB_RC_NOHSB if replication is
 *      not active.
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
dbe_ret_t dbe_db_replicate(
        dbe_db_t* db,
        rep_type_t type,
        rep_params_t* rp)
{
        dbe_ret_t rc;

        CHK_DB(db);

#ifdef SS_DEBUG
    if(rp) {
        SsSemEnter(ss_lib_sem);
            rp->rp_count++;
        ss_dassert(rp->rp_count == 1);
        SsSemExit(ss_lib_sem);
    }
#endif /* SS_DEBUG */

        if (db->db_hsbenabled) {
            ss_debug(int memtrccallstkheight;)
            ss_debug(memtrccallstkheight = (int) SsMemTrcGetCallStackHeight(NULL));

#ifdef DBE_LOGORDERING_FIX
            /*
             * This is solution to fix problem where REP_INSERT and
             * REP_DELETE  may cause HSB flush to replica and thus return
             * DBE_RC_CONT which is not allowed because we need to hold
             * logical log mutex until the stuff is written into both trx
             * and hsb logs.
             *
             * Final solution would mean that this call will just add the
             * stuff into the HSB oplist (or something similar) and
             * return immediately (never returning DBE_RC_CONT, even when
             * we commit), and separate call handles the flushing.
             *
             */

            if(rp->rp_type == REP_INSERT || rp->rp_type == REP_DELETE) {
                rp->rp_flushallowed = FALSE;
            }
#endif /* DBE_LOGORDERING_FIX */

            rc = (*db->db_replicatefun)(db->db_repctx, type, rp);

            ss_dassert(memtrccallstkheight == (int) SsMemTrcGetCallStackHeight(NULL));
        } else {
            if (db->db_replicatefun != NULL) {
                (*db->db_replicatefun)(db->db_repctx, type, rp);
            }
            rc = DB_RC_NOHSB;
        }

#ifdef SS_DEBUG
    if(rp) {
        SsSemEnter(ss_lib_sem);
            rp->rp_count--;
        SsSemExit(ss_lib_sem);
    }
#endif /* SS_DEBUG */
        return(rc);
}

dbe_ret_t dbe_db_commitready(dbe_db_t* db, dbe_trxid_t trxid)
{
        CHK_DB(db);

        if (db->db_hsbenabled) {
            return (*db->db_commitreadyfun)(trxid);
        } else {
            if (db->db_commitreadyfun != NULL) {
                return (*db->db_commitreadyfun)(trxid);
            }
        }
        return DBE_RC_SUCC;
}

void dbe_db_commitdone(dbe_db_t* db, dbe_trxid_t trxid, bool commit)
{
        CHK_DB(db);

        if (db->db_hsbenabled) {
            (*db->db_commitdonefun)(trxid, commit);
        } else {
            if (db->db_commitdonefun != NULL) {
                (*db->db_commitdonefun)(trxid, commit);
            }
        }
}

#endif /* DBE_REPLICATION */

/*##**********************************************************************\
 *
 *              dbe_db_adddropcardinal
 *
 * Adds relation id to a list of relations where cardinal entry should
 * be dropped.
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      relid -
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
void dbe_db_adddropcardinal(dbe_db_t* db, long relid)
{
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        if (db->db_dropcardinallist == NULL) {
            db->db_dropcardinallist = su_list_init(NULL);
            ss_autotest(su_list_setmaxlen(db->db_dropcardinallist, 80000));
        }
        su_list_insertlast(db->db_dropcardinallist, (void*)relid);

        SsSemExit(db->db_sem);
}

/*##**********************************************************************\
 *
 *              dbe_db_givedropcardinallist
 *
 * Returns a list of relation id for which the cardinal should be dropped.
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value - give :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
su_list_t* dbe_db_givedropcardinallist(dbe_db_t* db)
{
        su_list_t* list;

        CHK_DB(db);

        SsSemEnter(db->db_sem);

        list = db->db_dropcardinallist;
        if (list != NULL) {
            db->db_dropcardinallist = NULL;
        }

        SsSemExit(db->db_sem);

        return(list);
}


#ifdef SS_UNICODE_SQL
/*##**********************************************************************\
 *
 *              dbe_db_migratetounicode
 *
 * Tells whether the database object has detected a need to migrate
 * from ISO to UNICODE data dictionary.
 *
 * Parameters :
 *
 *      db - in, use
 *              database object
 *
 * Return value :
 *      TRUE - need to migrate to UNICODE
 *      FALSE - no need to migrate (already have UNICODE data dictionary)
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
bool dbe_db_migratetounicode(dbe_db_t* db)
{
        CHK_DB(db);
        return (db->db_migratetounicode);
}

/*##**********************************************************************\
 *
 *      dbe_db_migratetoblobg2
 *
 * checks whether there is need to convert the database to new BLOB G2
 * format
 *
 * Parameters:
 *      db - in, use
 *          database object
 *
 * Return value:
 *      TRUE - database is old format, need to convert
 *      FALSE - database is already converted
 *
 * Limitations:
 *
 * Globals used:
 */
bool dbe_db_migratetoblobg2(dbe_db_t* db)
{
        CHK_DB(db);
        return (db->db_migratetoblobg2);
}

/*##**********************************************************************\
 *
 *      dbe_db_migratetoblobg2
 *
 * checks whether there is need to convert the database to new format.
 *
 * Parameters:
 *      db - in, use
 *          database object
 *
 * Return value:
 *      TRUE - database is old format, need to convert
 *      FALSE - database is already converted
 *
 * Limitations:
 *
 * Globals used:
 */
bool dbe_db_migrateneeded(dbe_db_t* db)
{
        CHK_DB(db);
        return (db->db_migrategeneric);
}

/*##**********************************************************************\
 *
 *              dbe_db_migratetounicodecompleted
 *
 * Tells the db object that migration to UNICODE has completed,
 * so the logging can be turned on again.
 *
 * Parameters :
 *
 *      db -
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
void dbe_db_migratetounicodecompleted(dbe_db_t* db)
{
        ss_dassert(db->db_migratetounicode);
        db->db_migratetounicode = FALSE;
        rs_rbuf_setresetcallback(db->db_go->go_syscd, db->db_rbuf, dbe_srli_init, !db->db_migratetounicode);
        ss_dassert(db->db_dbfile->f_log == NULL);
        db->db_dbfile->f_log =
            dbe_log_init(
#ifdef DBE_LOGORDERING_FIX
                db,
#endif /* DBE_LOGORDERING_FIX */
                db->db_go->go_cfg,
                db->db_go->go_ctr,
                FALSE,
                dbe_header_getcreatime(
                    db->db_dbfile->f_indexfile->fd_dbheader),
                db->db_hsbg2configured
                    ? DBE_LOG_INSTANCE_LOGGING_HSB
                    : DBE_LOG_INSTANCE_LOGGING_STANDALONE
#ifdef SS_HSBG2
                ,db->db_hsbg2svc
#endif
                );

        ss_dprintf_1(("dbe_db_migratetounicodecompleted:log %x\n", db->db_dbfile->f_log));

}

void dbe_db_migratetounicodemarkheader(dbe_db_t* db)
{
        dbe_header_t* dbheader;

        CHK_DB(db);
        ss_dassert(db->db_changed);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        dbe_header_setdbvers(dbheader, SU_DBFILE_VERSNUM);
}

#endif /* SS_UNICODE_SQL */


/* marks header to contain the default catalog */

void dbe_db_migratetocatalogsuppmarkheader(dbe_db_t* db)
{
        dbe_header_t* dbheader;

        CHK_DB(db);
        ss_dassert(db->db_changed);
        dbheader = db->db_dbfile->f_indexfile->fd_dbheader;
        dbe_header_setdefcatalog(dbheader, RS_AVAL_DEFCATALOG_NEW);
}


/*##**********************************************************************\
 *
 *              dbe_db_initcd
 *
 * Init db object stuff to sysinfo object.
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      cd -
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
void dbe_db_initcd(dbe_db_t* db, rs_sysi_t* cd)
{
        CHK_DB(db);

        rs_sysi_insertdb(cd, db);
        rs_sysi_insertrbuf(cd, db->db_rbuf);
        rs_sysi_sethsbconfigured(cd, db->db_hsbg2configured);
}

void dbe_db_donecd(dbe_db_t* db, rs_sysi_t* cd)
{
        CHK_DB(db);
}

rs_sysinfo_t* dbe_db_getsyscd(
        dbe_db_t* db)
{
        CHK_DB(db);
        return db->db_go->go_syscd;
}

void dbe_db_setinitconnectcdfun(
        dbe_db_t* db,
        void* ctx,
        rs_sysi_t* (*connect_initcd)(void* ctx),
        void (*connect_donecd)(rs_sysi_t* cd))
{
        CHK_DB(db);
        db->db_tbconnect_initcd_cb = connect_initcd;
        db->db_tbconnect_donecd_cb = connect_donecd;
        db->db_tbconnect_ctx = ctx;
}

rs_sysi_t* dbe_db_inittbconcd(
        dbe_db_t* db)
{
        rs_sysi_t* cd;

        CHK_DB(db);

        if (db->db_tbconnect_initcd_cb != NULL) {
            cd = (*db->db_tbconnect_initcd_cb)(db->db_tbconnect_ctx);
            ss_dassert(cd != NULL);
            ss_autotest_or_debug(rs_sysi_setthrid(cd));
        } else {
            cd = NULL;
        }
        return(cd);
}

void dbe_db_donetbconcd(
        dbe_db_t* db,
        rs_sysi_t* cd)
{
        CHK_DB(db);

        if (db->db_tbconnect_donecd_cb != NULL) {
            (*db->db_tbconnect_donecd_cb)(cd);
        }
}

su_ret_t dbe_db_alloc_n_pages(
        dbe_db_t* db,
        su_daddr_t* p_daddrarr_out,
        size_t numpages_requested,
        size_t* p_numpages_gotten,
        su_daddr_t prev_daddr_for_optimization,
        bool mmepage __attribute__ ((unused)))
{
        dbe_freelist_t* fl;
        size_t i;
        su_ret_t rc = SU_SUCCESS;
        dbe_info_t info;

        CHK_DB(db);
        ss_dassert(!mmepage || db->db_final_checkpoint != 2);
        dbe_info_init(info, 0);
        fl = db->db_go->go_idxfd->fd_freelist;
        ss_dassert(numpages_requested > 0);
        i = 0;
        if (numpages_requested > 0) {
            if (prev_daddr_for_optimization == SU_DADDR_NULL) {
                FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                    FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
                    FAKE_CODE(SsPrintf("FAKE_DBE_FLSTALLOC_BLOB\n"));
                } else {
                    rc = dbe_fl_alloc(fl, &prev_daddr_for_optimization, &info);
                }
                if (rc != SU_SUCCESS) {
                    goto failed;
                }
                p_daddrarr_out[i] = prev_daddr_for_optimization;
                i++;
            }
            for (; i < numpages_requested; i++) {
                /* note should be (when it will be available):
                rc = dbe_fl_seq_alloc(fl,
                        prev_daddr_for_optimization,
                        &prev_daddr_for_optimization);
                */
                FAKE_IF(FAKE_DBE_FLSTALLOC_BLOB) {
                    FAKE_CODE(rc = SU_ERR_FILE_WRITE_DISK_FULL);
                    FAKE_CODE(SsPrintf("FAKE_DBE_FLSTALLOC_BLOB\n"));
                } else {
                    rc = dbe_fl_alloc(fl, &prev_daddr_for_optimization, &info);
                }
                if (rc != SU_SUCCESS) {
                    goto failed;
                }
                p_daddrarr_out[i] = prev_daddr_for_optimization;
            }
        }
 exitcode:;
        *p_numpages_gotten = i;
        return (rc);
 failed:;
        while (i > 0) {
            i--;
            dbe_fl_free(fl, p_daddrarr_out[i]);
        }
        ss_rc_dassert(i == 0, (int)i);
        goto exitcode;
}

su_ret_t dbe_db_free_n_pages(
        dbe_db_t* db,
        size_t numpages,
        su_daddr_t* daddr_array,
        dbe_cpnum_t page_cpnum,
        bool mmepage __attribute__ ((unused)))
{
        su_ret_t rc = SU_SUCCESS;

        CHK_DB(db);
        ss_dassert(!mmepage || db->db_final_checkpoint != 2);
        if (page_cpnum < dbe_counter_getcpnum(db->db_go->go_ctr)) {
            size_t i;
            for (i = 0; i < numpages; i++) {
                su_ret_t rc2 = dbe_cl_add(db->db_go->go_idxfd->fd_chlist,
                                          page_cpnum,
                                          daddr_array[i]);
                ss_dprintf_1((
                "dbe_db_free_n_pages:changelist:daddr=%lu,cpnum=%lu,ctr_cpnum=%lu\n",
                (ulong)daddr_array[i],
                (ulong)page_cpnum,
                (ulong)dbe_counter_getcpnum(db->db_go->go_ctr)));

                if (rc == SU_SUCCESS) {
                    rc = rc2;
                }
            }
        } else {
            size_t i;
            for (i = 0; i < numpages; i++) {
                su_ret_t rc2 = dbe_fl_free(db->db_go->go_idxfd->fd_freelist,
                                           daddr_array[i]);
                ss_dprintf_1((
                "dbe_db_free_n_pages:freelist:daddr=%lu,cpnum=%lu\n",
                (ulong)daddr_array[i],
                (ulong)page_cpnum));
                if (rc == SU_SUCCESS) {
                    rc = rc2;
                }
            }
        }
        return (rc);
}

void dbe_db_prefetch_n_pages(
        dbe_db_t* db,
        size_t numpages,
        su_daddr_t* daddr_array)
{
        CHK_DB(db);
        ss_dassert(numpages > 0);
        ss_dprintf_1(("dbe_db_prefetch_n_pages: n=%ld\n", (long)numpages));
        ss_debug({
            size_t i;
            for (i = 0; i < numpages; i++) {
                ss_dprintf_1(("dbe_db_prefetch_n_pages: daddr=%ld\n",
                              (long)daddr_array[i]));
            }
        });
#ifdef IO_MANAGER
        dbe_iomgr_prefetch(
                db->db_go->go_iomgr,
                daddr_array,
                (int)numpages,
                0);
#endif
}


long dbe_db_getstepstoskip(dbe_db_t* db)
{
        CHK_DB(db);
        return (db->db_backup_stepstoskip);
}

/*##**********************************************************************\
 *
 *              dbe_server_setsysifuns
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 *************************************************************************/

void dbe_server_setsysifuns(
        void (*server_sysi_init)(rs_sysi_t* sysi) __attribute__((unused)),
        void (*server_sysi_done)(rs_sysi_t* sysi) __attribute__((unused)),
        void (*server_sysi_init_functions)(rs_sysi_t* sysi))
{
    dbe_server_sysi_init_functions_fp = server_sysi_init_functions;
}

/*##**********************************************************************\
 *
 *              dbe_server_sysi_init_functions
 *
 *
 *
 * Parameters :          - none
 *
 * Return value :
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 *************************************************************************/

void dbe_server_sysi_init_functions(
        rs_sysi_t* cd)
{
    ss_dassert(dbe_server_sysi_init_functions_fp != NULL);
    (*dbe_server_sysi_init_functions_fp)(cd);
}

#ifdef SS_FAKE
void dbe_db_dummycommit(dbe_db_t* db, dbe_trxid_t* p_trxid)
{
        dbe_ret_t rc;
        bool dummy;

        CHK_DB(db);

        *p_trxid = dbe_counter_getnewtrxid(dbe_db_getcounter(db));

        rc = dbe_log_puthsbcommitmark(
                    dbe_db_getlog(db),
                    NULL,
                    DBE_LOGI_COMMIT_HSBPRIPHASE1|DBE_LOGI_COMMIT_LOCAL,
                    *p_trxid,
                    (void*)-1,
                    &dummy);
        ss_assert(rc == SU_SUCCESS);

        rc = dbe_log_puttrxmark(
                    dbe_db_getlog(db),
                    NULL,
                    DBE_LOGREC_COMMITTRX_INFO,
                    DBE_LOGI_COMMIT_HSBPRIPHASE2|DBE_LOGI_COMMIT_LOCAL,
                    *p_trxid,
                    DBE_HSB_NOT_KNOWN);
        ss_assert(rc == SU_SUCCESS);
}
#endif /* SS_FAKE */

void dbe_db_setcheckpointing(dbe_db_t* db, bool setting)
{
        CHK_DB(db);
        SsSemEnter(db->db_sem);
        db->db_allowcheckpointing = setting;
        SsSemExit(db->db_sem);
}

bool dbe_db_getcheckpointing(dbe_db_t* db)
{
        CHK_DB(db);
        return(db->db_allowcheckpointing);
}

uint dbe_db_getreadaheadsize(dbe_db_t* db)
{
        uint readahead; /* in # of pages */

        CHK_DB(db);
        dbe_cfg_getreadaheadsize(db->db_go->go_cfg, &readahead);
        return (readahead);
}
#if defined(SS_MME) && !defined(SS_PURIFY) && defined(SS_FFMEM)
void dbe_db_getmeminfo_mme(dbe_db_t* db, SsQmemStatT* p_mmememstat)
{
        void* mme_memctx;

        CHK_DB(db);
        mme_memctx = dbe_mme_getmemctx(db->db_mme);
        if (mme_memctx != NULL) {
            SsFFmemGetInfo(mme_memctx, p_mmememstat);
        } else {
            memset(p_mmememstat, '\0', sizeof(SsQmemStatT));
        }
}
#endif /* MME && !SS_PURIFY && SS_FFMEM */

void dbe_db_getmeminfo(dbe_db_t* db __attribute__ ((unused)),
                       SsQmemStatT* p_memstat)
{
        CHK_DB(db);

        SsQmemGetInfo(p_memstat);
#if defined(SS_MME) && !defined(SS_PURIFY) && defined(SS_FFMEM)
        {
            SsQmemStatT tmpstat;
            dbe_db_getmeminfo_mme(db, &tmpstat);
            p_memstat->qms_sysptrcount += tmpstat.qms_sysptrcount;
            p_memstat->qms_sysbytecount += tmpstat.qms_sysbytecount;
            p_memstat->qms_slotptrcount += tmpstat.qms_slotptrcount;
            p_memstat->qms_slotbytecount += tmpstat.qms_slotbytecount;
            if (tmpstat.qms_ptrmin != 0) {
                if (p_memstat->qms_ptrmin == 0) {
                    p_memstat->qms_ptrmin = tmpstat.qms_ptrmin;
                } else {
                    p_memstat->qms_ptrmin = SS_MIN(p_memstat->qms_ptrmin,
                                                   tmpstat.qms_ptrmin);
                }
            }
            if (tmpstat.qms_ptrmax != 0) {
                if (p_memstat->qms_ptrmax == 0) {
                    p_memstat->qms_ptrmax = tmpstat.qms_ptrmax;
                } else {
                    p_memstat->qms_ptrmax = SS_MAX(p_memstat->qms_ptrmax,
                                                   tmpstat.qms_ptrmax);
                }
            }
        }
#endif /* MME && !SS_PURIFY && SS_FFMEM */
}

#ifdef SS_MME

void dbe_db_lockmmemutex(rs_sysi_t* cd, dbe_db_t* db)
{
        CHK_DB(db);
        dbe_mme_lock_mutex(cd, db->db_mme);
}

void dbe_db_unlockmmemutex(rs_sysi_t* cd, dbe_db_t* db)
{
        CHK_DB(db);
        dbe_mme_unlock_mutex(cd, db->db_mme);
}
#endif /* SS_MME */

void dbe_db_setoutofdiskspace(dbe_db_t* db, su_ret_t rc)
{
        ss_dprintf_1(("dbe_db_setoutofdiskspace\n"));
        CHK_DB(db);

        if (!db->db_readonly) {
            db->db_readonly = TRUE;
            ui_msg_error_nostop(rc);
        }
}

dbe_ret_t dbe_db_logblobg2dropmemoryref(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_blobg2id_t blobid)
{
        bool writetolog;

        ss_dprintf_1(("dbe_db_logblobg2dropmemoryref\n"));
        CHK_DB(db);

        writetolog = (db->db_dbfile->f_log != NULL);
#ifdef SS_HSBG2
        if (dbe_db_gethsbg2mode(db) == DBE_HSB_SECONDARY
        ||  dbe_db_gethsbg2mode(db) == DBE_HSB_PRIMARY_UNCERTAIN)
        {
            writetolog = FALSE;
        }
#endif

        if (writetolog) {
            dbe_ret_t rc;
            dbe_db_enteraction(db, cd);
            rc = dbe_log_putblobg2dropmemoryref(db->db_dbfile->f_log, cd, blobid);
            dbe_db_exitaction(db, cd);
            return(rc);
        } else {
            return(DBE_RC_SUCC);
        }
}

#ifdef SS_HSBG2

void dbe_db_setreloadrbuffun(
        dbe_db_t* db,
        void (*reloaf_rbuf_fun)(void* ctx),
        void* ctx)
{
        CHK_DB(db);

        db->db_reloaf_rbuf_fun = reloaf_rbuf_fun;
        db->db_reloaf_rbuf_ctx = ctx;
}

void dbe_db_callreloadrbuffun(dbe_db_t* db)
{
        CHK_DB(db);

        ss_dassert(db->db_reloaf_rbuf_fun != NULL);
        (*db->db_reloaf_rbuf_fun)(db->db_reloaf_rbuf_ctx);
}

int dbe_db_abortall(
        dbe_db_t*db,
        bool* p_reload_rbuf)
{
        int naborts;
        ss_dprintf_1(("dbe_db_abortall\n"));
        CHK_DB(db);

        naborts = 0;

        if (dbe_db_setchanged(db, NULL)) {
            dbe_db_enteraction_hsb(db);
            naborts = dbe_mme_abortall(db->db_mme);
            dbe_gtrs_entertrxgate(db->db_go->go_gtrs);
            naborts = naborts + dbe_trxbuf_abortall(
                                    db->db_go->go_trxbuf,
                                    dbe_counter_getcommittrxnum(db->db_go->go_ctr),
                                    dbe_db_getlog(db),
                                    p_reload_rbuf);
            dbe_gtrs_exittrxgate(db->db_go->go_gtrs);
            dbe_db_exitaction_hsb(db);
        }

        return(naborts);
}

void dbe_db_abortallactive(
        dbe_db_t* db)
{
        su_list_t* list;
        su_list_node_t* n;
        dbe_trx_t* trx;
        bool aborted;

        ss_dprintf_1(("dbe_db_abortallactive\n"));
        CHK_DB(db);

        dbe_db_enteraction_exclusive(db, NULL);

        list = dbe_gtrs_getactivetrxlist(db->db_go->go_gtrs);

        su_list_do_get(list, n, trx) {
            aborted = dbe_trx_setfailed(trx, DBE_ERR_HSBABORTED, FALSE);
            ss_dprintf_1(("dbe_db_abortallactive:aborted %d, trxid %ld\n",
                        aborted,
                        DBE_TRXID_GETLONG(dbe_trx_getusertrxid(trx))));
        }

        dbe_db_exitaction(db, NULL);

        su_list_done(list);
}

void dbe_db_hsbabortalluncertain(
        dbe_db_t* db)
{
        su_list_t* list;
        su_list_node_t* n;
        dbe_trx_t* trx;

        ss_dprintf_1(("dbe_db_hsbabortalluncertain\n"));
        CHK_DB(db);
        /* NOTE! db->db_actiongate must be entered in exclusive mode */

        list = dbe_gtrs_getuncertaintrxlist(db->db_go->go_gtrs);

        su_list_do_get(list, n, trx) {
            dbe_trx_hsbenduncertain(trx, DBE_TRXID_NULL, DBE_ERR_HSBABORTED);
        }

        su_list_done(list);
}

void dbe_db_hsbcommituncertain(
        dbe_db_t* db,
        dbe_trxid_t trxid)
{
        su_list_t* list;
        su_list_node_t* n;
        dbe_trx_t* trx;
        bool foundp;

        ss_dprintf_1(("dbe_db_hsbcommituncertain\n"));
        CHK_DB(db);
        /* NOTE! db->db_actiongate must be entered in exclusive mode */

        list = dbe_gtrs_getuncertaintrxlist(db->db_go->go_gtrs);

        su_list_do_get(list, n, trx) {
            foundp = dbe_trx_hsbenduncertain(trx, trxid, SU_SUCCESS);
            if (foundp) {
                break;
            }
        }

        su_list_done(list);
}


bool dbe_db_getlogenabled(
        dbe_db_t* db)
{
        bool logenabled;

        dbe_cfg_getlogenabled(db->db_go->go_cfg, &logenabled);
        return(logenabled);
}


#endif /* SS_HSBG2 */

void dbe_db_convert_set(
        dbe_db_t* db,
        bool b)
{
        dbe_counter_convert_set(db->db_go->go_ctr, b);
}

void dbe_db_convert_init(
        dbe_db_t* db,
        bool *attrid_used,
        bool *keyid_used)
{
        dbe_counter_convert_init(db->db_go->go_ctr, attrid_used, keyid_used);
}

void dbe_db_convert_done(
        dbe_db_t* db)
{
        dbe_counter_convert_done(db->db_go->go_ctr);
}

void dbe_db_endhsbg2migrate(dbe_db_t* db)
{
        db->db_hsbg2configured = dbe_cfg_ishsbg2configured(db->db_go->go_cfg);
        ss_debug(dbe_db_hsbg2enabled = dbe_cfg_ishsbg2configured(db->db_go->go_cfg);)
}

void dbe_db_logfnumrange(
        dbe_db_t* db,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end)
{
        char* logfnametemplate;
        char logfnamedigittemplate;
        dbe_cpnum_t cpnum;

        CHK_DB(db);

        dbe_cfg_getlogfilenametemplate(db->db_go->go_cfg, &logfnametemplate);
        dbe_cfg_getlogdigittemplate(db->db_go->go_cfg, &logfnamedigittemplate);
        cpnum = dbe_counter_getcpnum(db->db_go->go_ctr);
        dbe_backup_getlogfnumrange_withoutbackupobject(
                db->db_go->go_ctr,
                db->db_go->go_cfg,
                db->db_go->go_syscd,
                cpnum,
                p_logfnum_start,
                p_logfnum_end);
        SsMemFree(logfnametemplate);
}


static void db_getlogfilenames(
        rs_sysi_t* cd __attribute__ ((unused)),
        dbe_db_t* db,
        su_list_t* fname_list)
{
        dbe_logfnum_t i;
        dbe_logfnum_t logfnum_start;
        dbe_logfnum_t logfnum_end;
        char* logdir;
        char* logfnametemplate;
        char logfnamedigittemplate;

        CHK_DB(db);

        dbe_db_logfnumrange(
            db,
            &logfnum_start,
            &logfnum_end);

        dbe_cfg_getlogdir(db->db_go->go_cfg, &logdir);
        dbe_cfg_getlogfilenametemplate(db->db_go->go_cfg, &logfnametemplate);
        dbe_cfg_getlogdigittemplate(db->db_go->go_cfg, &logfnamedigittemplate);

        for (i = logfnum_start - 1; i > 0; i--) {
            char* logfname;

            logfname = dbe_logfile_genname(logdir, logfnametemplate, i, logfnamedigittemplate);
            if (SsFExist(logfname)) {
                su_list_insertfirst(fname_list, logfname);
            } else {
                SsMemFree(logfname);
                break;
            }
        }
        for (i = logfnum_start; ; i++) {
            char* logfname;

            ss_dassert(i < SS_UINT2_MAX);
            logfname = dbe_logfile_genname(logdir, logfnametemplate, i, logfnamedigittemplate);
            su_list_insertlast(fname_list, logfname);
            if (!SsFExist(logfname)) {
                if (i > logfnum_end) {
                    break;
                }
            }
        }
        SsMemFree(logfnametemplate);
        SsMemFree(logdir);
}

static void fname_destructor(void* fname)
{
        SsMemFree(fname);
}

void dbe_db_getdbandlogfilenames(
        dbe_db_t* db,
        su_list_t** fname_list)
{
        su_pa_t* idxfilespec_array;
        dbe_filespec_t* filespec;
        uint i;
        bool succp;

        succp = TRUE;
        *fname_list = su_list_init(fname_destructor);
        db_getlogfilenames(db->db_go->go_syscd, db, *fname_list);
        idxfilespec_array = su_pa_init();
        dbe_cfg_getidxfilespecs(db->db_go->go_cfg, idxfilespec_array);
        su_pa_do_get(idxfilespec_array, i, filespec) {
            char* fname = dbe_filespec_getname(filespec);
            if (SsFExist(fname)) {
                su_list_insertlast(*fname_list, SsMemStrdup(fname));
            }
            dbe_filespec_done(filespec);
        }
        su_pa_done(idxfilespec_array);
}

bool dbe_db_deletedbbyfnamelist(su_list_t* fname_list)
{
        bool succp = TRUE;

        su_list_node_t* ln;
        char* fname;
        su_list_do_get(fname_list, ln, fname) {
            if (SsFExist(fname)) {
                succp &= SsFRemove(fname);
            }
        }
        su_list_done(fname_list);
        return (succp);
}

/*##**********************************************************************\
 *
 *      dbe_db_getprimarystarttime
 *
 * Returns the time this db last went into PRIMARY or STANDALONE role.
 *
 * Parameters:
 *      db - <usage>
 *          <description>
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
SsTimeT dbe_db_getprimarystarttime(dbe_db_t* db)
{
        SsTimeT t = 0;

        dbe_hsbstate_entermutex(db->db_hsbstate);

        switch (dbe_hsbstate_getrole(db->db_hsbstate)) {
            case HSB_ROLE_STANDALONE:
            case HSB_ROLE_PRIMARY:
                t = db->db_primarystarttime;
                break;

            default:
                /* t = 0; */
                break;
        }

        dbe_hsbstate_exitmutex(db->db_hsbstate);

        return t;
}

/*##**********************************************************************\
 *
 *      dbe_db_setprimarystarttime
 *
 * Sets the time this db last went into PRIMARY or STANDALONE role as
 * now.
 *
 * Parameters:
 *      db - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_db_setprimarystarttime(dbe_db_t* db)
{
        db->db_primarystarttime = SsTime(NULL);
}

/*##**********************************************************************\
 *
 *      dbe_db_getsecondarystarttime
 *
 * Returns the time this db last went into SECONDARY role.
 *
 * Parameters:
 *      db - <usage>
 *          <description>
 *
 * Return value:
 *      <description>
 *
 * Limitations:
 *
 * Globals used:
 */
SsTimeT dbe_db_getsecondarystarttime(dbe_db_t* db)
{
        SsTimeT t = 0;

        dbe_hsbstate_entermutex(db->db_hsbstate);

        switch (dbe_hsbstate_getrole(db->db_hsbstate)) {
            case HSB_ROLE_SECONDARY:
                t = db->db_secondarystarttime;
                break;

            default:
                /* t = 0; */
                break;
        }

        dbe_hsbstate_exitmutex(db->db_hsbstate);

        return t;
}

/*##**********************************************************************\
 *
 *      dbe_db_setsecondarystarttime
 *
 * Sets the time this db last went into SECONDARY role as now.
 *
 * Parameters:
 *      db - <usage>
 *          <description>
 *
 * Return value:
 *
 * Limitations:
 *
 * Globals used:
 */
void dbe_db_setsecondarystarttime(dbe_db_t* db)
{
        db->db_secondarystarttime = SsTime(NULL);
}

#ifndef SS_MYSQL
void dbe_db_writedisklessmmepage(
        dbe_backupfiletype_t    ftype,
        ss_byte_t*              data,
        size_t                  len)
{
        ss_byte_t*              copy;

        ss_dprintf_1(("dbe_db_writemme\n"));

        copy = DB_DISKLESSMMEPAGEALLOC(&db_disklessmmememctx, len);
        memcpy(copy, data, len);
        if (db_disklessmmepages == NULL) {
            db_disklessmmepages = su_list_init(NULL);
        }
        su_list_insertlast(db_disklessmmepages, copy);
}

void dbe_db_cleardisklessmmepages(void)
{
        if (db_disklessmmepages != NULL) {
            ss_byte_t* mmepagedata;

            while (NULL !=
                   (mmepagedata = su_list_removefirst(db_disklessmmepages)))
            {
                DB_DISKLESSMMEPAGEFREE(db_disklessmmememctx, mmepagedata);
            }
            su_list_done(db_disklessmmepages);
            db_disklessmmepages = NULL;
            db_disklessmmememctx = NULL;
        }
}
#endif /* !SS_MYSQL */


int dbe_db_setencryption_level(
        dbe_db_t* db,
        dbe_cryptoparams_t* db_cryptoparams)
{
        dbe_filedes_t* filedes = db->db_dbfile->f_indexfile;
        int level = 0;
        char *passwd;

        switch (dbe_header_getcryptoalg(filedes->fd_dbheader)) {
        case SU_CRYPTOALG_DES:
            ss_dassert(db_cryptoparams != NULL);
            passwd = dbe_crypt_getpasswd(db_cryptoparams);
            if (passwd == NULL || strlen(passwd) == 0) {
                level = 1;
            } else {
                level = 2;
            }
        case SU_CRYPTOALG_APP:
            level = 3;

        case SU_CRYPTOALG_NONE:
        default:
            level = 0;
        }

        rs_sysi_set_encryption_level(db->db_go->go_syscd, level);

        return level;
}
