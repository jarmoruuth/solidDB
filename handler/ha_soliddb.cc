/* Copyright (C) 2006-2007 MySQL AB & Solid Information Technology Ltd


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

/*
  ha_soliddb is a storage engine interface for solidDB.
  You can enable it in your build by doing the following during your
  build process:
  ./configure --with-soliddb-storage-engine

  Once this is done mysql will let you create tables with:
  CREATE TABLE A (...) ENGINE=SOLIDDB;

  Please read the object definition in ha_soliddb.h before reading the rest
  if this file.

  To get an idea of what occurs here is an example select that would do a
  scan of an entire table:
  ha_soliddb::store_lock
  ha_soliddb::external_lock
  ha_soliddb::info
  ha_soliddb::rnd_init
  ha_soliddb::extra
  ENUM HA_EXTRA_CACHE   Cash record in HA_rrnd()
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::rnd_next
  ha_soliddb::extra
  ENUM HA_EXTRA_NO_CACHE   End cacheing of records (def)
  ha_soliddb::external_lock
  ha_soliddb::extra
  ENUM HA_EXTRA_RESET   Reset database to after open

  In the above example has 9 row called before rnd_next signalled that it was
  at the end of its data. In the above example the table was already opened
  (or you would have seen a call to ha_soliddb::open(). Calls to
  ha_soliddb::extra() are hints as to what will be occuring to the request.

*/

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif


#define HAVE_SOLIDDB_BACKUP_NEW

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation        // gcc: Class implementation
#endif

#define INSIDE_HA_SOLIDDB_CC

#define _WIN32_WINNT 0x0403 /* enable TryEnterCriticalSection etc. */

#include <sql_table.h>  
                       

#include <sql_acl.h>    // PROCESS_ACL
#include <m_ctype.h>
#include <m_string.h>
#include <strfunc.h>
#include <mysys_err.h>
#include <mysql/plugin.h>
#include <mysql/psi/psi.h>
#include <sql_parse.h>
#include <hash.h>

#include <sql_priv.h>
#include <mysqld_error.h>
#include <my_sys.h>
#include <my_base.h>
#include <ssmyenv.h>

#include "ha_soliddb.h"
#include "ha_soliddb_is.h"

#ifndef VOID
#define VOID void
#endif


#define SS_HA_BUG62FIX
#define DDL_LOCK_TIMEOUT 3600000 /* in miliseconds */
#define DDL_LOCK_INSTANTLY 0 /* in miliseconds */
#define UNKNOWN_NUMBER_OF_TUPLES_IN_TABLE 0.0
/* #define SOLID_TRUNCATE */
#define EST_MIN_SELECTIVITY     (1E-20)
#define WRONG_KEY_NO UINT_MAX32

#define ss_printf       printf
#ifdef SS_NT
#define snprintf    _snprintf
#endif

#if defined(SS_NT) && defined(SS_MYSQL_PERFCOUNT)
#define ss_win_perf(c)              c
#define ss_win_perf_start           if (mysql_enable_perfcount) QueryPerformanceCounter((LARGE_INTEGER*)&startcount)
#define ss_win_perf_stop(pc, cc)    if (mysql_enable_perfcount) {\
    QueryPerformanceCounter((LARGE_INTEGER*)&endcount);\
    (pc) += endcount - startcount;\
    (cc)++;\
}
#else
#define ss_win_perf(c)
#define ss_win_perf_start
#define ss_win_perf_stop(pc, cc)
#endif

#ifndef DBUG_OFF
#define SDB_DBUG_ENTER(fname) DBUG_ENTER(fname); SsMemTrcEnterFunction((char *)__FILE__, (char *)fname); ss_pprintf_1((fname "\n")); char* _soliddb_func_ = (char*)fname;
#define SDB_DBUG_RETURN(rc) do { SsMemTrcExitFunction((char *)__FILE__, 0); ss_pprintf_1(( "%s:return %d\n", _soliddb_func_, rc)); DBUG_RETURN(rc); } while(0)
#define SDB_DBUG_VOID_RETURN do { SsMemTrcExitFunction((char *)__FILE__, 0); ss_pprintf_1(( "%s:return\n", _soliddb_func_)); DBUG_VOID_RETURN; } while(0)
#else
#define SDB_DBUG_ENTER(fname)
#define SDB_DBUG_RETURN(rc) return(rc)
#define SDB_DBUG_VOID_RETURN return
#endif

void solid_split_myrelname( const char* mysqlname, char** schema, char** relname );
inline rs_entname_t* solid_split_myrelname( const char* mysqlname, rs_entname_t *solrel )
{
    solid_split_myrelname(mysqlname, &solrel->en_schema, &solrel->en_name);
    return solrel;
}

void solid_relname( rs_entname_t *solrel, const char* mysqlname);
void solid_relname( rs_entname_t *solrel, const char* mysqldb, const char* mysqlrel);

static void solid_my_caseup_str(char *name);
static void solid_my_caseup_str_if_lower_case_table_names(char *name);

bool is_soliddb_table( const char* dbname, const char* tablename );
bool is_soliddb_table( File &frm_file );

static inline bool is_DDL_command(MYSQL_THD thd );
static inline bool operation_requires_xtable_lock(MYSQL_THD thd );

/******************************************************************//**
Strip dir name from a full path name and return only the file name
@return file name or "null" if no file name */
const char* 
soliddb_basename(
/*==============*/
        const char*     path_name)      /*!< in: full path name */
{
        const char*     name = base_name(path_name);

        return((name) ? name : "null");
}


/* MySQL related utility functions. */
bool copy_file( File &src, const char* dstdir, const char* fname, const char* ext );

#define SQL_WHITESPACES " \t\n\r"
#define BACKUP_LIST_FILE_NAME "solid_restore.cnf"
#define BACKUP_CONFIG_FILE_NAME "my.cnf"

/* Table to map special MySQL table names to Solid system tables. You meed to
 * create these tables in MySQL with a set of columns that are compatible
 * with solidDB system tables.
 */

static solid_sysrel_map_t solid_sysrel_map[] =
{
        { (char *)"SOLIDDB_SYS_TABLES",         (char *)RS_RELNAME_TABLES },
        { (char *)"SOLIDDB_SYS_COLUMNS",        (char *)RS_RELNAME_COLUMNS },
        { (char *)"SOLIDDB_SYS_COLUMNS_AUX",    (char *)RS_RELNAME_COLUMNS_AUX },
        { (char *)"SOLIDDB_SYS_KEYS",           (char *)RS_RELNAME_KEYS },
        { (char *)"SOLIDDB_SYS_SCHEMAS",        (char *)RS_RELNAME_SCHEMAS },
        { (char *)"SOLIDDB_SYS_KEYPARTS",       (char *)RS_RELNAME_KEYPARTS },
        { (char *)"SOLIDDB_SYS_KEYPARTS_AUX",   (char *)RS_RELNAME_KEYPARTS_AUX },
        { (char *)"SOLIDDB_SYS_FORKEYS",        (char *)RS_RELNAME_FORKEYS },
        { (char *)"SOLIDDB_SYS_FORKEYPARTS",    (char *)RS_RELNAME_FORKEYPARTS },
        { (char *)"SOLIDDB_SYS_SEQUENCES",      (char *)RS_RELNAME_SEQUENCES },
        { (char *)"SOLIDDB_SYS_BLOBS",          (char *)RS_RELNAME_BLOBS },
        { (char *)"SOLIDDB_SYS_CARDINAL",       (char *)RS_RELNAME_CARDINAL },
        { (char *)"SOLIDDB_SYS_INFO",           (char *)RS_RELNAME_INFO },
        { (char *)"SOLIDDB_SYS_TABLEMODES",     (char *)RS_RELNAME_TABLEMODES },
        { (char *)"SOLIDDB_SYS_COLLATIONS",     (char *)RS_RELNAME_COLLATIONS },
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        { (char *)"SOLIDDB_SYS_FORKEYS_UNRESOLVED",     (char *)RS_RELNAME_FORKEYS_UNRESOLVED },
        { (char *)"SOLIDDB_SYS_FORKEYPARTS_UNRESOLVED", (char *)RS_RELNAME_FORKEYPARTS_UNRESOLVED },
#endif /*FOREIGN_KEY_CHECKS_SUPPORTED*/
        { NULL, NULL }
};

/* System tables */
static const char* soliddb_sysrel_names[] =
{
    RS_RELNAME_TABLES,
    RS_RELNAME_COLUMNS,
    RS_RELNAME_COLUMNS_AUX,
    RS_RELNAME_USERS,
    RS_RELNAME_UROLE,
    RS_RELNAME_RELAUTH,
    RS_RELNAME_ATTAUTH,
    RS_RELNAME_VIEWS,
    RS_RELNAME_KEYPARTS,
    RS_RELNAME_KEYPARTS_AUX,
    RS_RELNAME_KEYS,
    RS_RELNAME_CARDINAL,
    RS_RELNAME_INFO,
    RS_RELNAME_SYNONYM,
    RS_RELNAME_TYPES,
    RS_RELNAME_SQL_LANG,
    RS_RELNAME_PROCEDURES,
    RS_RELNAME_TRIGGERS,
    RS_RELNAME_EVENTS,
    RS_RELNAME_TABLEMODES,
    RS_RELNAME_FORKEYS,
    RS_RELNAME_FORKEYPARTS,
    RS_RELNAME_SEQUENCES,
    RS_RELNAME_CATALOGS,
    RS_RELNAME_SCHEMAS,
    RS_RELNAME_PROCEDURE_COL_INFO,
    RS_RELNAME_SYSPROPERTIES,
    RS_RELNAME_HOTSTANDBY,
    RS_RELNAME_BLOBS,
    RS_RELNAME_COLLATIONS,
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
    RS_RELNAME_FORKEYS_UNRESOLVED,
    RS_RELNAME_FORKEYPARTS_UNRESOLVED,
#endif /*FOREIGN_KEY_CHECKS_SUPPORTED*/
    NULL
};

/* solidDB for MySQL status variables */

typedef struct export_vars_st {
        longlong soliddb_memory_size;
        longlong soliddb_transaction_commits;
        longlong soliddb_transaction_rollbacks;
        longlong soliddb_transaction_aborts;
        longlong soliddb_transaction_total;
        longlong soliddb_transaction_readonly;
        longlong soliddb_transaction_trxbuf;
        longlong soliddb_transaction_active;
        longlong soliddb_transaction_validate;
        long soliddb_cache_hitrate;
        longlong soliddb_cache_find;
        longlong soliddb_cache_read;
        longlong soliddb_cache_write;
        bool soliddb_indexmerge_active;
        longlong soliddb_index_writes;
        longlong soliddb_index_writesincemerge;
        longlong soliddb_log_writes;
        longlong soliddb_log_writessincecp;
        longlong soliddb_active;
        longlong soliddb_activeavg;
        longlong soliddb_filesize;
        longlong soliddb_logsize;
        bool soliddb_cp_active;
        bool soliddb_backup_active;
        const char *soliddb_version;
#ifdef SS_MYSQL_AC
        char* soliddb_hsb_role;
        char* soliddb_hsb_state;
        char* soliddb_hsb_safeness;
#endif /* SS_MYSQL_AC */

/* 
 #ifdef HAVE_PSI_INTERFACE

static PSI_cond_info    all_soliddb_cache_conds[] = {
	{&soliddb_cachefind_key, "soliddb_cachefind", 0}
	{&soliddb_cachefileread_key, "soliddb_cachefileread", 0}
	{&soliddb_cachefilewrite_key, "soliddb_cachefilewrite", 0}
	{&soliddb_cacheprefetch_key, "soliddb_cacheprefetch", 0}
	{&soliddb_cacheprefetchwait_key, "soliddb_cacheprefetchwait", 0}
	{&soliddb_cachepreflush_key, "soliddb_cachepreflush", 0}
	{&soliddb_cachelruwrite_key, "soliddb_cachelruwrite", 0}
	{&soliddb_cacheslotwait_key, "soliddb_cacheslotwait", 0}
};

 #endif */ /* HAVE_PSI_INTERFACE */




/* Performance monitoring */
        longlong soliddb_pmon_fileopen;
        longlong soliddb_pmon_fileread;
        longlong soliddb_pmon_filewrite;
        longlong soliddb_pmon_fileappend;
        longlong soliddb_pmon_fileflush;
        longlong soliddb_pmon_filelock;
        longlong soliddb_pmon_cachefind;
        longlong soliddb_pmon_cachefileread;
        longlong soliddb_pmon_cachefilewrite;
        longlong soliddb_pmon_cacheprefetch;
        longlong soliddb_pmon_cacheprefetchwait;
        longlong soliddb_pmon_cachepreflush;
        longlong soliddb_pmon_cachelruwrite;
        longlong soliddb_pmon_cacheslotwait;
        longlong soliddb_pmon_sqlprepare;
        longlong soliddb_pmon_sqlexecute;
        longlong soliddb_pmon_sqlfetch;
        longlong soliddb_pmon_dbeinsert;
        longlong soliddb_pmon_dbedelete;
        longlong soliddb_pmon_dbeupdate;
        longlong soliddb_pmon_dbefetch;
        longlong soliddb_pmon_dbefetchuniquefound;
        longlong soliddb_pmon_dbefetchuniquenotfound;
        longlong soliddb_pmon_transcommit;
        longlong soliddb_pmon_transabort;
        longlong soliddb_pmon_transrdonly;
        longlong soliddb_pmon_transbufcnt;
        longlong soliddb_pmon_transbufclean;
        longlong soliddb_pmon_transbufcleanlevel;
        longlong soliddb_pmon_transbufabortlevel;
        longlong soliddb_pmon_transbufadded;
        longlong soliddb_pmon_transbufremoved;
        longlong soliddb_pmon_transvldcnt;
        longlong soliddb_pmon_transactcnt;
        longlong soliddb_pmon_transreadlevel;
        longlong soliddb_pmon_indwrite;
        longlong soliddb_pmon_indwritesaftermerge;
        longlong soliddb_pmon_logwrites;
        longlong soliddb_pmon_logfilewrite;
        longlong soliddb_pmon_logwritesaftercp;
        longlong soliddb_pmon_logsize;
        longlong soliddb_pmon_srchnactive;
        longlong soliddb_pmon_dbsize;
        longlong soliddb_pmon_dbfreesize;
        longlong soliddb_pmon_memsize;
        longlong soliddb_pmon_mergequickstep;
        longlong soliddb_pmon_mergestep;
        longlong soliddb_pmon_mergepurgestep;
        longlong soliddb_pmon_mergeuserstep;
        longlong soliddb_pmon_mergeoper;
        longlong soliddb_pmon_mergecleanup;
        longlong soliddb_pmon_mergeact;
        longlong soliddb_pmon_mergewrites;
        longlong soliddb_pmon_mergefilewrite;
        longlong soliddb_pmon_mergefileread;
        longlong soliddb_pmon_mergelevel;
        longlong soliddb_pmon_backupstep;
        longlong soliddb_pmon_backupact;
        longlong soliddb_pmon_checkpointact;
        longlong soliddb_pmon_checkpointcount;
        longlong soliddb_pmon_checkpointfilewrite;
        longlong soliddb_pmon_checkpointfileread;
        longlong soliddb_pmon_estsamplesread;
        longlong soliddb_pmon_logflushes_logical;
        longlong soliddb_pmon_logflushes_physical;
        longlong soliddb_pmon_loggroupcommit_wakeups;
        longlong soliddb_pmon_logflushes_fullpages;
        longlong soliddb_pmon_logwaitflush;
        longlong soliddb_pmon_logmaxwritequeuerecords;
        longlong soliddb_pmon_logmaxwritequeuebytes;
        longlong soliddb_pmon_ss_threadcount;
        longlong soliddb_pmon_waitreadlevel_count;
        longlong soliddb_pmon_dbe_lock_ok;
        longlong soliddb_pmon_dbe_lock_timeout;
        longlong soliddb_pmon_dbe_lock_deadlock;
        longlong soliddb_pmon_dbe_lock_wait;
        longlong soliddb_pmon_mysql_rnd_init;
        longlong soliddb_pmon_mysql_index_read;
        longlong soliddb_pmon_mysql_fetch_next;
        longlong soliddb_pmon_mysql_cursor_create;
        longlong soliddb_pmon_mysql_cursor_reset_full;
        longlong soliddb_pmon_mysql_cursor_reset_simple;
        longlong soliddb_pmon_mysql_cursor_reset_fetch;
        longlong soliddb_pmon_mysql_cursor_cache_find;
        longlong soliddb_pmon_mysql_cursor_cache_hit;
        longlong soliddb_pmon_mysql_connect;
        longlong soliddb_pmon_mysql_commit;
        longlong soliddb_pmon_mysql_rollback;
/* End of pmon values */
} export_vars_t;

static export_vars_t solid_export_vars;

#if MYSQL_VERSION_ID >= 50100
static SHOW_VAR soliddb_status_variables[] = {
#else
struct show_var_st soliddb_status_variables[] = {
#endif
    {"memory_size", (char *) &solid_export_vars.soliddb_memory_size, SHOW_LONGLONG},
    {"transaction_commits", (char *) &solid_export_vars.soliddb_transaction_commits, SHOW_LONGLONG},
    {"transaction_rollbacks", (char *) &solid_export_vars.soliddb_transaction_rollbacks, SHOW_LONGLONG},
    {"transaction_aborts", (char *) &solid_export_vars.soliddb_transaction_aborts, SHOW_LONGLONG},
    {"transaction_total", (char *) &solid_export_vars.soliddb_transaction_total, SHOW_LONGLONG},
    {"transaction_readonly", (char *) &solid_export_vars.soliddb_transaction_readonly, SHOW_LONGLONG},
    {"transaction_trxbuf", (char *) &solid_export_vars.soliddb_transaction_trxbuf, SHOW_LONGLONG},
    {"transaction_activecnt", (char *) &solid_export_vars.soliddb_transaction_active, SHOW_LONGLONG},
    {"transaction_validatecnt", (char *) &solid_export_vars.soliddb_transaction_validate, SHOW_LONGLONG},
    {"cache_hit_rate", (char *) &solid_export_vars.soliddb_cache_hitrate, SHOW_LONG},
    {"cache_finds", (char *) &solid_export_vars.soliddb_cache_find, SHOW_LONGLONG},
    {"cache_reads", (char *) &solid_export_vars.soliddb_cache_read, SHOW_LONGLONG},
    {"cache_writes", (char *) &solid_export_vars.soliddb_cache_write, SHOW_LONGLONG},
    {"indexmerge_active", (char *) &solid_export_vars.soliddb_indexmerge_active, SHOW_BOOL},
    {"index_writes", (char *) &solid_export_vars.soliddb_index_writes, SHOW_LONGLONG},
    {"index_writes_after_merge", (char *) &solid_export_vars.soliddb_index_writesincemerge, SHOW_LONGLONG},
    {"log_writes", (char *) &solid_export_vars.soliddb_log_writes, SHOW_LONGLONG},
    {"log_writes_after_cp", (char *)&solid_export_vars.soliddb_log_writessincecp, SHOW_LONGLONG},
    {"active_searches", (char *)&solid_export_vars.soliddb_active, SHOW_LONGLONG},
    {"average_searches", (char *)&solid_export_vars.soliddb_activeavg, SHOW_LONGLONG},
    {"database_size", (char *)&solid_export_vars.soliddb_filesize, SHOW_LONGLONG},
    {"log_size", (char *)&solid_export_vars.soliddb_logsize, SHOW_LONGLONG},
    {"checkpoint_active", (char *)&solid_export_vars.soliddb_cp_active, SHOW_BOOL},
    {"backup_active", (char *)&solid_export_vars.soliddb_backup_active, SHOW_BOOL},
    {"version", (char*)&solid_export_vars.soliddb_version, SHOW_CHAR_PTR},
#ifdef SS_MYSQL_AC
    {"hsb_role",      (char *)&solid_export_vars.soliddb_hsb_role,     SHOW_CHAR_PTR},
    {"hsb_state",     (char *)&solid_export_vars.soliddb_hsb_state,    SHOW_CHAR_PTR},
    {"hsb_safeness",  (char *)&solid_export_vars.soliddb_hsb_safeness, SHOW_CHAR_PTR},
#endif /* SS_MYSQL_AC */

/*
{"pmon_fileopen", (char *)&solid_export_vars.soliddb_pmon_fileopen, SHOW_LONGLONG },
{"pmon_fileread", (char *)&solid_export_vars.soliddb_pmon_fileread, SHOW_LONGLONG },
{"pmon_filewrite", (char *)&solid_export_vars.soliddb_pmon_filewrite, SHOW_LONGLONG },
{"pmon_fileappend", (char *)&solid_export_vars.soliddb_pmon_fileappend, SHOW_LONGLONG },
{"pmon_fileflush", (char *)&solid_export_vars.soliddb_pmon_fileflush, SHOW_LONGLONG },
{"pmon_filelock", (char *)&solid_export_vars.soliddb_pmon_filelock, SHOW_LONGLONG },
{"pmon_cachefind", (char *)&solid_export_vars.soliddb_pmon_cachefind, SHOW_LONGLONG },
{"pmon_cachefileread", (char *)&solid_export_vars.soliddb_pmon_cachefileread, SHOW_LONGLONG },
{"pmon_cachefilewrite", (char *)&solid_export_vars.soliddb_pmon_cachefilewrite, SHOW_LONGLONG },
{"pmon_cacheprefetch", (char *)&solid_export_vars.soliddb_pmon_cacheprefetch, SHOW_LONGLONG },
{"pmon_cacheprefetchwait", (char *)&solid_export_vars.soliddb_pmon_cacheprefetchwait, SHOW_LONGLONG },
{"pmon_cachepreflush", (char *)&solid_export_vars.soliddb_pmon_cachepreflush, SHOW_LONGLONG },
{"pmon_cachelruwrite", (char *)&solid_export_vars.soliddb_pmon_cachelruwrite, SHOW_LONGLONG },
{"pmon_cacheslotwait", (char *)&solid_export_vars.soliddb_pmon_cacheslotwait, SHOW_LONGLONG },
{"pmon_sqlprepare", (char *)&solid_export_vars.soliddb_pmon_sqlprepare, SHOW_LONGLONG },
{"pmon_sqlexecute", (char *)&solid_export_vars.soliddb_pmon_sqlexecute, SHOW_LONGLONG },
{"pmon_sqlfetch", (char *)&solid_export_vars.soliddb_pmon_sqlfetch, SHOW_LONGLONG },
{"pmon_dbeinsert", (char *)&solid_export_vars.soliddb_pmon_dbeinsert, SHOW_LONGLONG },
{"pmon_dbedelete", (char *)&solid_export_vars.soliddb_pmon_dbedelete, SHOW_LONGLONG },
{"pmon_dbeupdate", (char *)&solid_export_vars.soliddb_pmon_dbeupdate, SHOW_LONGLONG },
{"pmon_dbefetch", (char *)&solid_export_vars.soliddb_pmon_dbefetch, SHOW_LONGLONG },
{"pmon_dbefetchuniquefound", (char *)&solid_export_vars.soliddb_pmon_dbefetchuniquefound, SHOW_LONGLONG },
{"pmon_dbefetchuniquenotfound", (char *)&solid_export_vars.soliddb_pmon_dbefetchuniquenotfound, SHOW_LONGLONG },
{"pmon_transcommit", (char *)&solid_export_vars.soliddb_pmon_transcommit, SHOW_LONGLONG },
{"pmon_transabort", (char *)&solid_export_vars.soliddb_pmon_transabort, SHOW_LONGLONG },
{"pmon_transrdonly", (char *)&solid_export_vars.soliddb_pmon_transrdonly, SHOW_LONGLONG },
{"pmon_transbufcnt", (char *)&solid_export_vars.soliddb_pmon_transbufcnt, SHOW_LONGLONG },
{"pmon_transbufclean", (char *)&solid_export_vars.soliddb_pmon_transbufclean, SHOW_LONGLONG },
{"pmon_transbufcleanlevel", (char *)&solid_export_vars.soliddb_pmon_transbufcleanlevel, SHOW_LONGLONG },
{"pmon_transbufabortlevel", (char *)&solid_export_vars.soliddb_pmon_transbufabortlevel, SHOW_LONGLONG },
{"pmon_transbufadded", (char *)&solid_export_vars.soliddb_pmon_transbufadded, SHOW_LONGLONG },
{"pmon_transbufremoved", (char *)&solid_export_vars.soliddb_pmon_transbufremoved, SHOW_LONGLONG },
{"pmon_transvldcnt", (char *)&solid_export_vars.soliddb_pmon_transvldcnt, SHOW_LONGLONG },
{"pmon_transactcnt", (char *)&solid_export_vars.soliddb_pmon_transactcnt, SHOW_LONGLONG },
{"pmon_transreadlevel", (char *)&solid_export_vars.soliddb_pmon_transreadlevel, SHOW_LONGLONG },
{"pmon_indwrite", (char *)&solid_export_vars.soliddb_pmon_indwrite, SHOW_LONGLONG },
{"pmon_indwritesaftermerge", (char *)&solid_export_vars.soliddb_pmon_indwritesaftermerge, SHOW_LONGLONG },
{"pmon_logwrites", (char *)&solid_export_vars.soliddb_pmon_logwrites, SHOW_LONGLONG },
{"pmon_logfilewrite", (char *)&solid_export_vars.soliddb_pmon_logfilewrite, SHOW_LONGLONG },
{"pmon_logwritesaftercp", (char *)&solid_export_vars.soliddb_pmon_logwritesaftercp, SHOW_LONGLONG },
{"pmon_logsize", (char *)&solid_export_vars.soliddb_pmon_logsize, SHOW_LONGLONG },
{"pmon_srchnactive", (char *)&solid_export_vars.soliddb_pmon_srchnactive, SHOW_LONGLONG },
{"pmon_dbsize", (char *)&solid_export_vars.soliddb_pmon_dbsize, SHOW_LONGLONG },
{"pmon_dbfreesize", (char *)&solid_export_vars.soliddb_pmon_dbfreesize, SHOW_LONGLONG },
{"pmon_memsize", (char *)&solid_export_vars.soliddb_pmon_memsize, SHOW_LONGLONG },
{"pmon_mergequickstep", (char *)&solid_export_vars.soliddb_pmon_mergequickstep, SHOW_LONGLONG },
{"pmon_mergestep", (char *)&solid_export_vars.soliddb_pmon_mergestep, SHOW_LONGLONG },
{"pmon_mergepurgestep", (char *)&solid_export_vars.soliddb_pmon_mergepurgestep, SHOW_LONGLONG },
{"pmon_mergeuserstep", (char *)&solid_export_vars.soliddb_pmon_mergeuserstep, SHOW_LONGLONG },
{"pmon_mergeoper", (char *)&solid_export_vars.soliddb_pmon_mergeoper, SHOW_LONGLONG },
{"pmon_mergecleanup", (char *)&solid_export_vars.soliddb_pmon_mergecleanup, SHOW_LONGLONG },
{"pmon_mergeact", (char *)&solid_export_vars.soliddb_pmon_mergeact, SHOW_LONGLONG },
{"pmon_mergewrites", (char *)&solid_export_vars.soliddb_pmon_mergewrites, SHOW_LONGLONG },
{"pmon_mergefilewrite", (char *)&solid_export_vars.soliddb_pmon_mergefilewrite, SHOW_LONGLONG },
{"pmon_mergefileread", (char *)&solid_export_vars.soliddb_pmon_mergefileread, SHOW_LONGLONG },
{"pmon_mergelevel", (char *)&solid_export_vars.soliddb_pmon_mergelevel, SHOW_LONGLONG },
{"pmon_backupstep", (char *)&solid_export_vars.soliddb_pmon_backupstep, SHOW_LONGLONG },
{"pmon_backupact", (char *)&solid_export_vars.soliddb_pmon_backupact, SHOW_LONGLONG },
{"pmon_checkpointact", (char *)&solid_export_vars.soliddb_pmon_checkpointact, SHOW_LONGLONG },
{"pmon_checkpointcount", (char *)&solid_export_vars.soliddb_pmon_checkpointcount, SHOW_LONGLONG },
{"pmon_checkpointfilewrite", (char *)&solid_export_vars.soliddb_pmon_checkpointfilewrite, SHOW_LONGLONG },
{"pmon_checkpointfileread", (char *)&solid_export_vars.soliddb_pmon_checkpointfileread, SHOW_LONGLONG },
{"pmon_estsamplesread", (char *)&solid_export_vars.soliddb_pmon_estsamplesread, SHOW_LONGLONG },
{"pmon_logflushes_logical", (char *)&solid_export_vars.soliddb_pmon_logflushes_logical, SHOW_LONGLONG },
{"pmon_logflushes_physical", (char *)&solid_export_vars.soliddb_pmon_logflushes_physical, SHOW_LONGLONG },
{"pmon_loggroupcommit_wakeups", (char *)&solid_export_vars.soliddb_pmon_loggroupcommit_wakeups, SHOW_LONGLONG },
{"pmon_logflushes_fullpages", (char *)&solid_export_vars.soliddb_pmon_logflushes_fullpages, SHOW_LONGLONG },
{"pmon_logwaitflush", (char *)&solid_export_vars.soliddb_pmon_logwaitflush, SHOW_LONGLONG },
{"pmon_logmaxwritequeuerecords", (char *)&solid_export_vars.soliddb_pmon_logmaxwritequeuerecords, SHOW_LONGLONG },
{"pmon_logmaxwritequeuebytes", (char *)&solid_export_vars.soliddb_pmon_logmaxwritequeuebytes, SHOW_LONGLONG },
{"pmon_ss_threadcount", (char *)&solid_export_vars.soliddb_pmon_ss_threadcount, SHOW_LONGLONG },
{"pmon_waitreadlevel_count", (char *)&solid_export_vars.soliddb_pmon_waitreadlevel_count, SHOW_LONGLONG },
{"pmon_dbe_lock_ok", (char *)&solid_export_vars.soliddb_pmon_dbe_lock_ok, SHOW_LONGLONG },
{"pmon_dbe_lock_timeout", (char *)&solid_export_vars.soliddb_pmon_dbe_lock_timeout, SHOW_LONGLONG },
{"pmon_dbe_lock_deadlock", (char *)&solid_export_vars.soliddb_pmon_dbe_lock_deadlock, SHOW_LONGLONG },
{"pmon_dbe_lock_wait", (char *)&solid_export_vars.soliddb_pmon_dbe_lock_wait, SHOW_LONGLONG },
{"pmon_mysql_rnd_init", (char *)&solid_export_vars.soliddb_pmon_mysql_rnd_init, SHOW_LONGLONG },
{"pmon_mysql_index_read", (char *)&solid_export_vars.soliddb_pmon_mysql_index_read, SHOW_LONGLONG },
{"pmon_mysql_fetch_next", (char *)&solid_export_vars.soliddb_pmon_mysql_fetch_next, SHOW_LONGLONG },
{"pmon_mysql_cursor_create", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_create, SHOW_LONGLONG },
{"pmon_mysql_cursor_reset_full", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_reset_full, SHOW_LONGLONG },
{"pmon_mysql_cursor_reset_simple", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_reset_simple, SHOW_LONGLONG },
{"pmon_mysql_cursor_reset_fetch", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_reset_fetch, SHOW_LONGLONG },
{"pmon_mysql_cursor_cache_find", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_cache_find, SHOW_LONGLONG },
{"pmon_mysql_cursor_cache_hit", (char *)&solid_export_vars.soliddb_pmon_mysql_cursor_cache_hit, SHOW_LONGLONG },
{"pmon_mysql_connect", (char *)&solid_export_vars.soliddb_pmon_mysql_connect, SHOW_LONGLONG },
{"pmon_mysql_commit", (char *)&solid_export_vars.soliddb_pmon_mysql_commit, SHOW_LONGLONG },
{"pmon_mysql_rollback", (char *)&solid_export_vars.soliddb_pmon_mysql_rollback, SHOW_LONGLONG },
*/

    {NullS, NullS, SHOW_LONG}
};


static void solidcon_throwout(void* ctx);

/* Prototypes for admin functions */

/*
#ifndef MYSQL_DYNAMIC_PLUGIN
static su_ret_t do_backup( rs_sysi_t* cd, char* parameters, su_err_t** p_errh );
#endif
*/
static su_ret_t do_backup( rs_sysi_t* cd, char* parameters, su_err_t** p_errh );
static su_ret_t do_performance_monitor(rs_sysi_t* cd, char* parameters, su_err_t** p_errh);
static su_ret_t do_checkpoint(rs_sysi_t* cd, char* parameters, su_err_t** p_errh);
static su_ret_t do_ssdebug(rs_sysi_t* cd, char* parameters, su_err_t** p_errh);
static su_ret_t do_command(rs_sysi_t* cd, tb_trans_t* trans, const char* cmd, const char* parameters, su_err_t** p_errh);

#if !defined(MYSQL_DYNAMIC_PLUGIN) && defined(SS_DEBUG)
static su_ret_t do_unittests( rs_sysi_t* cd, char* parameters, su_err_t** p_errh );
#endif

/* Structure to hold admin command strings and their functions */
typedef struct solid_admin_commands_st {
        const char* command_str;
        su_ret_t(*command_func)(rs_sysi_t*, char *, su_err_t**);
} solid_admin_commands_t;

/* Init admin commands table */
static solid_admin_commands_t solid_admin_commands [] =
{
/*
#ifndef MYSQL_DYNAMIC_PLUGIN
    {"backup", do_backup},
#endif
*/
    {"backup", do_backup},
    {"pmon", do_performance_monitor},
    {"checkpoint", do_checkpoint},
    {"ssdebug", do_ssdebug},

#if !defined(MYSQL_DYNAMIC_PLUGIN) && defined(SS_DEBUG)
    {"unittest", do_unittests},
#endif
    {NULL, NULL }
};

static bool solid_handler_init(void);
static int solid_close_connection(MYSQL_THD thd);

static int solid_commit(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD thd,
        bool all);

static int solid_rollback(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD thd,
        bool all);

#if MYSQL_VERSION_ID >= 50100
static uint solid_alter_table_flags(uint flags);

bool soliddb_show_status(handlerton *hton, MYSQL_THD thd, stat_print_fn*, enum ha_stat_type);
#endif /* MYSQL_VERSION_ID >= 50100 I.E. 5.1 */

static const char solid_hton_name[]= "solidDB";

static const char solid_hton_comment[]=
"Fully transactional disk-based engine with multiversion optimistic/pessimistic concurrency control";

struct handlerton *legacy_soliddb_hton; 

static int solid_started = 0;
static bool solid_printmsg = TRUE;
static SsSemT* solid_mutex = NULL;
static SsSemT* solid_collation_mutex = NULL;
static SsMsgLogT* solid_msglog;
static su_inifile_t* solid_inifile = NULL;

/* Variables for example share methods */
static HASH soliddb_open_tables; // Hash used to track open tables
pthread_mutex_t soliddb_mutex;   // This is the mutex we use to init the hash
static int soliddb_init= 0;      // Variable for checking the init state of hash

/* Configuration parameters for MySQL/soliDB storage engine. Note that these
 should include default values! */

#define SS_SEMNUM_SOLIDDB_BACKUP_DIR (SS_SEMNUM_SS_LIB)

char* soliddb_logdir = NULL;
char  soliddb_backupdir_buf[FN_REFLEN] = "";
char *soliddb_backupdir = soliddb_backupdir_buf;
static SsFlatMutexT soliddb_backupdir_mutex;

#ifdef HAVE_SOLIDDB_BACKUP_NEW
my_bool soliddb_backup_emptydir = false;  // By default backup directory should not be emptied
my_bool soliddb_backup_myisam   = false;  // By default MyISAM tables should not be backed up
my_bool soliddb_backup_system   = false;  // By default system tables should not be backed up
my_bool soliddb_backup_config   = false;  // By default config file should not be backed up
#endif

char* soliddb_filespec = NULL;
char* soliddb_admin_command = NULL;
longlong soliddb_cache_size = DBE_DEFAULT_INDEXCACHESIZE;
ulong soliddb_durability_level = DBE_DURABILITY_STRICT;
my_bool soliddb_checkpoint_deletelog = TRUE; /* By default solidDB should delete log files
                                                only after backup. */
ulong soliddb_lock_wait_timeout = DBE_DEFAULT_PESSIMISTIC_LOCK_TO;
ulong soliddb_optimistic_lock_wait_timeout = DBE_DEFAULT_OPTIMISTIC_LOCK_TO;
ulong soliddb_log_block_size = DBE_DEFAULT_LOGBLOCKSIZE;
ulong soliddb_db_block_size = DBE_DEFAULT_INDEXBLOCKSIZE;
ulong soliddb_backup_block_size = DBE_DEFAULT_BACKUP_BLOCKSIZE;
longlong soliddb_checkpoint_interval = DBE_DEFAULT_CPINTERVAL;
ulong soliddb_io_threads = 0;        /* use system default */
ulong soliddb_write_threads = 0;     /* use system default */
longlong soliddb_lockhash_size = DBE_DEFAULT_LOCKHASHSIZE;
ulong soliddb_checkpoint_time = 0;   /* use system default */
my_bool soliddb_log_enabled = DBE_DEFAULT_LOGENABLED;
my_bool soliddb_pessimistic = TRUE;
ulong soliddb_extend_increment = DBE_DEFAULT_INDEXEXTENDINCR;
ulong soliddb_readahead = DBE_DEFAULT_READAHEADSIZE;

static su_pa_t* soliddb_collations;

static solid_bool soliddb_collation_general_initbuf(
        su_collation_t* coll,
        char* collation_name, /* UTF8 encoding! */
        void* data);

static su_collation_t* soliddb_collation_init(
        char*      collation_name,
        uint       collation_id);

static void soliddb_collations_init(
        tb_database_t* tdb);

static char* soliddb_aval_print_externaldatatype(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval);

static bool soliddb_atype_can_have_collation(rs_sysi_t* cd, rs_atype_t* atype);

static bool internal_solid_end_trans_and_stmt(
    rs_sysi_t* cd,
    tb_trans_t* trans,
    bool commitp,
    su_err_t** p_errh);

static int ha_solid_mysql_error(MYSQL_THD thd, su_err_t* errh, int rc);

static int handle_error(su_err_t*& errh);

static char* get_foreign_key_str(SOLID_CONN* con, rs_relh_t* relh, rs_key_t* key);
static int solid_key_to_mysqlkeyidx(rs_sysi_t* cd,rs_relh_t* relh,ulong solidkeyid);

/************************************************************************/
/* structure to support ALTER TABLE ... DISABLE/ENABLE KEYS.            */
/************************************************************************/
#define SS_SEMNUM_SOLIDDB_DISABLED_KEYS (SS_SEMNUM_ANONYMOUS_SEM)

/*
 * This structure is needed to keep index definition which is given from
 * point of view of SQL user (not an internal index structure).
 */
typedef struct index_def
{
     char*               indexname; /* name of the index */
     solid_bool          unique;    /* if this index is unique */
     uint                attr_c;    /* number of attributes */
     char**              attrs;     /* array of attribute names of size `attr_c` (allocated dynamically) */
     solid_bool*         desc;      /* array of desc/asc signs for each attribute. 1 = descending, 0 - ascending. (allocated dynamically)*/
#ifdef SS_COLLATION
     size_t*             prefix_lengths; /* prefix lengths for each attribute of index (allocated dynamically) */
#endif /* SS_COLLATION */
} index_def_t;

/* this structure is uses to keep index definitions for a table */
typedef struct table_indexes
{
     rs_entname_t table_name;
     size_t n_indexes;
     index_def_t* indexes;
} table_indexes_t;

/* List of disabled keys for all Solid tables and a mutex to protect it. */
static su_pa_t* solid_disabled_keys = NULL;
static SsFlatMutexT solid_disabled_keys_mutex;

static void init_disabled_keys()
{
        solid_disabled_keys = su_pa_init();
        SsFlatMutexInit(&solid_disabled_keys_mutex, SS_SEMNUM_SOLIDDB_DISABLED_KEYS);
}

static void free_table_disabled_keys(table_indexes_t* p)
{
        rs_entname_done_buf(&p->table_name);
        for (size_t k=0; k < p->n_indexes; ++k) {
            if (p->indexes[k].indexname) {
                SsMemFree(p->indexes[k].indexname);
            }
            for (size_t a=0; a < p->indexes[k].attr_c; ++a) {
                SsMemFree(p->indexes[k].attrs[a]);
            }
            SsMemFree(p->indexes[k].attrs);
            SsMemFree(p->indexes[k].desc);
            #ifdef SS_COLLATION
                SsMemFree(p->indexes[k].prefix_lengths);
            #endif
        }
        SsMemFree(p->indexes);
        SsMemFree(p);
}

static table_indexes_t* add_table_disabled_keys(rs_entname_t* entname,
                                                size_t n_indexes)
{
        table_indexes_t* p = (table_indexes_t*)SsMemAlloc(sizeof(table_indexes_t));
        rs_entname_copybuf(&p->table_name,entname);
        p->indexes = (index_def_t*)SsMemAlloc(sizeof(index_def_t)*n_indexes);
        memset(p->indexes,0,sizeof(index_def_t)*n_indexes);
        p->n_indexes = n_indexes;
        SsFlatMutexLock(solid_disabled_keys_mutex);
        su_pa_insert(solid_disabled_keys,p);
        SsFlatMutexUnlock(solid_disabled_keys_mutex);
        return p;
}

static void add_table_disabled_key(table_indexes_t* p,
                                   size_t k,
                                   char* indexname,
                                   solid_bool unique,
                                   uint attr_c)
{
        p->indexes[k].indexname = (char*)SsMemStrdup(indexname);
        p->indexes[k].unique = unique;
        p->indexes[k].attr_c = attr_c;
        p->indexes[k].attrs = (char**)SsMemAlloc(sizeof(char  *)*  attr_c);
        memset(p->indexes[k].attrs,0,attr_c*sizeof(char *));
        p->indexes[k].desc = (solid_bool*)SsMemAlloc(attr_c*sizeof(solid_bool));
        memset(p->indexes[k].desc,0,attr_c*sizeof(solid_bool));
        #ifdef SS_COLLATION
            p->indexes[k].prefix_lengths = (size_t*)SsMemAlloc(attr_c*sizeof(size_t));
            memset(p->indexes[k].prefix_lengths,0,attr_c*sizeof(size_t));
        #endif
}

static void add_table_disabled_key_part(table_indexes_t* p,
                                        size_t k,
                                        size_t kp,
                                        char* aname,
                                        bool desc,
#ifdef SS_COLLATION
                                        size_t prefix_length
#endif /* SS_COLLATION */
                                        )
{
        p->indexes[k].attrs[kp] = (char*)SsMemStrdup(aname);
        p->indexes[k].desc[kp] = desc ? TRUE : FALSE;
        #ifdef SS_COLLATION
            p->indexes[k].prefix_lengths[kp] = prefix_length;
        #endif
}

static table_indexes_t* get_table_disabled_keys(rs_entname_t* table)
{
        int i;
        void* pp;
        table_indexes_t* found = NULL;

        SsFlatMutexLock(solid_disabled_keys_mutex);
        su_pa_do_get(solid_disabled_keys, i, pp) {
            table_indexes_t* p = (table_indexes_t*)pp;
            if ( ! rs_entname_compare(&p->table_name,table) ) {
                found = p;
                break;
            }
        }
        SsFlatMutexUnlock(solid_disabled_keys_mutex);
        return found;
}

static void drop_table_disabled_keys(table_indexes_t* ti)
{
        int i;
        void* pp;

        SsFlatMutexLock(solid_disabled_keys_mutex);
        su_pa_do_get(solid_disabled_keys, i, pp) {
            table_indexes_t* p = (table_indexes_t*)pp;
            if (p == ti) {
                su_pa_remove(solid_disabled_keys,i);
                free_table_disabled_keys(p);
                break;
            }
        }
        SsFlatMutexUnlock(solid_disabled_keys_mutex);
}

static void free_disabled_keys()
{
        int i;
        void* pp;

        su_pa_do_get(solid_disabled_keys, i, pp) {
            table_indexes_t* p = (table_indexes_t*)pp;
            free_table_disabled_keys(p);
        }
        su_pa_done(solid_disabled_keys);
        solid_disabled_keys = NULL;
        SsFlatMutexDone(solid_disabled_keys_mutex);
}

#if defined(SS_DEBUG)

    static void print_index_def(const index_def_t* id)
    {
        int i;
        SsDbgPrintf("\tINDEX \'%s\' %s %d attributes\n",
                                      id->indexname,
                                      id->unique ? "UNIQUE" : "PLURAL",
                                      id->attr_c);
        SsDbgPrintf("\t\t (\n");
        for (i=0; i<id->attr_c; ++i) {
            SsDbgPrintf("\t\t\t%s"
#ifdef SS_COLLATION
                        "(%d)"
#endif
                        " %s\n",
                        id->attrs[i],
#ifdef SS_COLLATION
                        id->prefix_lengths[i],
#endif
                        id->desc[i],
                        i == id->attr_c-1 ? "" : ",");
        }
        SsDbgPrintf("\t\t )\n");
    }

    static void print_disabled_keys()
    {
            int i;
            int j;
            void* pp;

            SsFlatMutexLock(solid_disabled_keys_mutex);
            su_pa_do_get(solid_disabled_keys, i, pp) {
                table_indexes_t* p = (table_indexes_t*)pp;
                char *tofree;
                SsDbgPrintf("%d) TABLE %s, %d index(es)", i,
                            (tofree = rs_entname_printname(&p->table_name)),
                            p->n_indexes);
                for (j=0; j<p->n_indexes; ++p) {
                    print_index_def(p->indexes + j);
                }
                SsMemFree(tofree);
            }
            SsFlatMutexUnlock(solid_disabled_keys_mutex);
    }
#endif
/************************************************************************/
/* structure to support DISABLE/ENABLE of foreign KEYS.                 */
/************************************************************************/
#define SS_SEMNUM_SOLIDDB_DISABLED_FOREIGNKEYS (SS_SEMNUM_ANONYMOUS_SEM)

/* Keeps definitions of table foreign keys that have been disabled. */
typedef struct table_forkeys
{
     rs_entname_t table_name;
     size_t n_forkeys;
     tb_sqlforkey_t* forkeys;
} table_forkeys_t;

/* List of tables with disabled foreign keys and a mutex to protect it. */
static su_pa_t* solid_disabled_forkeys = NULL;
static SsFlatMutexT solid_disabled_forkeys_mutex;

static void init_disabled_forkeys()
{
        solid_disabled_forkeys = su_pa_init();
        SsFlatMutexInit(&solid_disabled_forkeys_mutex, SS_SEMNUM_SOLIDDB_DISABLED_FOREIGNKEYS);
}

static table_forkeys_t* add_table_disabled_forkeys(rs_entname_t* entname,
                                                   size_t n_forkeys)
{
        table_forkeys_t* p = (table_forkeys_t*)SsMemAlloc(sizeof(table_forkeys_t));
        rs_entname_copybuf(&p->table_name,entname);
        p->forkeys = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t)*n_forkeys);
        memset(p->forkeys,0,sizeof(tb_sqlforkey_t)*n_forkeys);
        p->n_forkeys = n_forkeys;
        SsFlatMutexLock(solid_disabled_forkeys_mutex);
        su_pa_insert(solid_disabled_forkeys,p);
        SsFlatMutexUnlock(solid_disabled_forkeys_mutex);
        return p;
}

static table_forkeys_t* get_table_disabled_forkeys(rs_entname_t* table)
{
        int i;
        void* pp;
        table_forkeys_t* found = NULL;

        SsFlatMutexLock(solid_disabled_forkeys_mutex);
        su_pa_do_get (solid_disabled_forkeys, i, pp) {
            table_forkeys_t* p = (table_forkeys_t*)pp;
            if (!rs_entname_compare(&p->table_name,table)) {
                found = p;
                break;
            }
        }
        SsFlatMutexUnlock(solid_disabled_forkeys_mutex);
        return found;
}


static void free_table_disabled_forkeys(table_forkeys_t* p)
{
        rs_entname_done_buf(&p->table_name);
        for (size_t k=0; k < p->n_forkeys; ++k) {
            tb_forkey_done_buf(&p->forkeys[k]);
        }
        SsMemFree(p->forkeys);
        SsMemFree(p);
}

static void drop_table_disabled_forkeys(table_forkeys_t* tfk)
{
        int i;
        void* pp;

        SsFlatMutexLock(solid_disabled_forkeys_mutex);
        su_pa_do_get(solid_disabled_forkeys, i, pp) {
            table_forkeys_t* p = (table_forkeys_t*)pp;
            if (p == tfk) {
                su_pa_remove(solid_disabled_forkeys,i);
                free_table_disabled_forkeys(p);
                break;
            }
        }
        SsFlatMutexUnlock(solid_disabled_forkeys_mutex);
}

static void free_disabled_forkeys()
{
        int i;
        void* p;

        su_pa_do_get(solid_disabled_forkeys, i, p) {
            free_table_disabled_forkeys((table_forkeys_t*)p);
        }
        su_pa_done(solid_disabled_forkeys);
        solid_disabled_forkeys = NULL;
        SsFlatMutexDone(solid_disabled_forkeys_mutex);
}

static rs_key_t* rs_relh_basekeyforforgnkey(void* cd,
                                            tb_trans_t* trans,
                                            rs_relh_t* relh,
                                            rs_key_t* key)
{
        rs_relh_t *referenced_relh;
        ulong base_key_id;
        rs_key_t* base_key;

        ss_dassert(cd != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(key != NULL);
        ss_dassert(rs_relh_hasrefkey(cd,relh,rs_key_name(cd,key)));

        referenced_relh = tb_dd_getrelhbyid((rs_sysi_t*)cd, trans,
                                            rs_key_refrelid((rs_sysi_t*)cd, key),
                                            NULL,NULL);

        ss_dassert(referenced_relh != NULL);

        /* tb_dd_resolverefkeys(cd, referenced_relh); */ /* TODO: is this needed ? */

        base_key_id = va_getlong(rs_keyp_constvalue(cd, key, 0));
        base_key = rs_relh_keybyid(cd, referenced_relh, base_key_id);

        rs_relh_done(cd, referenced_relh);

        return base_key;
}

#if defined(SS_DEBUG)
    static void print_disabled_forkeys(void* cd)
    {
            int i;
            size_t k;
            void* pp;

            SsFlatMutexLock(solid_disabled_forkeys_mutex);
            su_pa_do_get(solid_disabled_forkeys, i, pp) {

                table_forkeys_t* p = (table_forkeys_t*)pp;

                for (k=0; k<p->n_forkeys; ++k) {
                    tb_forkey_print(rs_entname_getname(&p->table_name), NULL, NULL, p->forkeys+k);
                }
            }
            SsFlatMutexUnlock(solid_disabled_forkeys_mutex);
    }
#endif /*defined(SS_DEBUG)*/

/************************************************************************/

typedef struct ddl_monitoring
{
    unsigned int number_of_backups;
    unsigned int number_of_ddls;
    SsFlatMutexT ddl_monitoring_mutex;
    SsMesT* ddl_finish_event;
} ddl_monitoring_t;

#define SS_SEMNUM_SOLIDDB_DDL_MONITOR (SS_SEMNUM_SS_LIB)

#ifdef HAVE_SOLIDDB_BACKUP_NEW
typedef struct bkp_ctx_st BKP_CTX;
static BKP_CTX*  bkp_ctx = NULL;
BKP_CTX* create_bkp_ctx();
void free_bkp_ctx(BKP_CTX* ctx);

#define SS_SEMNUM_SOLIDDB_BACKUP (SS_SEMNUM_SS_LIB)
static SsFlatMutexT soliddb_backup_mutex;

/*#***********************************************************************\
*              soliddb_query
*
* Get MySQL query string
*
* Parameters :
*
*   MYSQL_THD*  thd, in ,use, MySQL thread
*
* Return value :
*
*   const char*
*
* Globals used : -
*
*/
static inline const char *soliddb_query(
        MYSQL_THD thd)
{
        ss_dassert(thd != NULL);

#if MYSQL_VERSION_ID >= 50100
        return (const char *)*(thd_query(thd));
#else
        return (const char *)thd->query;
#endif
}

/*#***********************************************************************\
*              soliddb_drop_mode
*
* Get drop table mode from MySQL
*
* Parameters :
*
*   rs_sysi_t*  cd, in, use, solidDB system information
*   MYSQL_THD*  thd, in ,use, MySQL thread
*
* Return value :
*
*   int
*
* Globals used : -
*
*/
static inline bool soliddb_drop_mode(
        rs_sysi_t* cd,
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (tb_minisql_is_drop_table_cascade(cd, soliddb_query(thd)));
#else
        return (thd->lex->drop_mode == DROP_CASCADE);
#endif
}

/*#***********************************************************************\
*              soliddb_charset
*
* Get MySQL thread charset
*
* Parameters :
*
*   MYSQL_THD*  thd, in ,use, MySQL thread
*
* Return value :
*
* struct charset_info_st*
*
* Globals used : -
*
*/
static inline struct charset_info_st* soliddb_charset(
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (thd_charset(thd));
#else
        return (thd->charset());
#endif 
}

/*#***********************************************************************\
 *
 *              soliddb_tx_isolation
 *
 * Return transaction isolation mode
 *
 * Parameters :
 *
 *   MYSQL_THD   thd, in, use, MySQL thread
 *
 * Return value : int
 *
 * Globals used :
 */
static inline int soliddb_tx_isolation(
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (thd_tx_isolation(thd));
#else
        return (thd->variables.tx_isolation);
#endif
}

/*#***********************************************************************\
 *
 *              soliddb_in_lock_tables
 *
 * Return TRUE if we are in lock tables
 *
 * Parameters :
 *
 *   MYSQL_THD   thd, in, use, MySQL thread
 *
 * Return value : TRUE or FALSE
 *
 * Globals used :
 */
static inline int soliddb_in_lock_tables(
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (thd_in_lock_tables(thd));
#else
        return (thd->in_lock_tables);
#endif
}

/*#***********************************************************************\
 *
 *              soliddb_tablespace_op
 *
 * Return TRUE if this is tablespace operation
 *
 * Parameters :
 *
 *   MYSQL_THD   thd, in, use, MySQL thread
 *
 * Return value : TRUE or FALSE
 *
 * Globals used :
 */
static inline int soliddb_tablespace_op(
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (thd_tablespace_op(thd));
#else
        return (thd->tablespace_op);
#endif
}

/*#***********************************************************************\
*              init_backup
*
* Initialize backup
*
* Parameters :    -
* Return value :  -
* Globals used :
*
*         static BKP_CTX*   bkp_ctx
*         SsFlatMutexT      soliddb_backup_mutex
*/
static inline void init_backup()
{
        SsFlatMutexInit(&soliddb_backup_mutex, SS_SEMNUM_SOLIDDB_BACKUP);
        bkp_ctx = create_bkp_ctx();
}

/*#***********************************************************************\
*              free_backup
*
* Free Backup
*
* Parameters :    -
* Return value :  -
* Globals used :
*
*         static BKP_CTX*   bkp_ctx
*         SsFlatMutexT      soliddb_backup_mutex
*/
static inline void free_backup()
{
        free_bkp_ctx(bkp_ctx);
        SsFlatMutexDone(soliddb_backup_mutex);
}

#endif /*HAVE_SOLIDDB_BACKUP_NEW*/


/*#***********************************************************************\
*              soliddb_sql_command
*
* Get MySQL SQL-command type
*
* Parameters :
*
*   MYSQL_THD*  thd, in ,use, MySQL thread
*
* Return value :
*
*   int
*
* Globals used : -
*
*/
static inline int soliddb_sql_command(
        MYSQL_THD thd)
{
#if MYSQL_VERSION_ID >= 50100
        return (thd_sql_command(thd));
#else
        return (thd->lex->sql_command);
#endif
}

/*#***********************************************************************\
*              is_alter_table
*
* Tells if the currently executed command in the MySQL thread is an ALTER TABLE
* command.
*
* Parameters :
*
*    THD* thd, in, MySQL current thread/process
*
* Return value : true or false
*
* Globals used : -
*/
inline bool ha_soliddb::is_alter_table(
        MYSQL_THD thd)
{
        int sql_command = soliddb_sql_command(thd);

        return (sql_command == SQLCOM_ALTER_TABLE
#if MYSQL_VERSION_ID < 50100
            || sql_command == SQLCOM_DROP_INDEX
            || sql_command == SQLCOM_CREATE_INDEX
#endif
            );
}

/*#***********************************************************************\
*              is_soliddb_system_relation
*
* Check whether the table user has used is solidDB for MySQL system table

* Parameters :
*
*    const char* sol_schema, in, use, schema name
*    const char* sol_relname, in, use, table name
*
* Return value : true or false
*
* Globals used : -
*/
static inline bool is_soliddb_system_relation (
        const char *sol_schema,
        const char *sol_relname)
{
    bool systab = false;

    if ( !strcmp(sol_schema, SNC_SCHEMA) ) {
        uint i;

        for (i = 0; soliddb_sysrel_names[i]; ++i) {
            if (!strcmp(soliddb_sysrel_names[i], sol_relname)) {
                systab = true;
                break;
            }
        }
    }

    return systab;
}

/*#***********************************************************************\
*              is_soliddb_system_relation
*
* Check whether the table user has used is solidDB for MySQL system table

* Parameters :
*
*    cont char* full_myrelname, in, use, schema and table name
*
* Return value : true or false
*
* Globals used : -
*/
static inline bool is_soliddb_system_relation (
        const char *full_myrelname)
{
    rs_entname_t en;
    bool systab;

    solid_relname(&en, full_myrelname);
    systab = is_soliddb_system_relation(en.en_schema,en.en_name);
    rs_entname_done_buf(&en);

    return (systab);
}

/*#***********************************************************************\
*              is_soliddb_system_relation
*
* Check whether the table user has used is solidDB for MySQL system table

* Parameters :
*
*    cont rs_entname_t& solrel, in, use, solid relation
*
* Return value : true or false
*
* Globals used : -
*/
static inline bool is_soliddb_system_relation (
        const rs_entname_t &solrel)
{
    return (is_soliddb_system_relation(solrel.en_schema, solrel.en_name));
}

/*#***********************************************************************\
*              init_ddl_monitoring
*
* Initialize DDL monitoring
*
* Parameters :
*
*    ddl_monitoring_t* ddl_monitor, in, use, DDL monitoring structure
*
* Return value : -
*
* Globals used : -
*/
static inline void init_ddl_monitoring(
        ddl_monitoring_t *ddl_monitor)
{
        ddl_monitor->number_of_backups = ddl_monitor->number_of_ddls = 0;
        SsFlatMutexInit( &ddl_monitor->ddl_monitoring_mutex, SS_SEMNUM_SOLIDDB_DDL_MONITOR );
        ddl_monitor->ddl_finish_event = SsMesCreateLocal();
        SsMesSend( ddl_monitor->ddl_finish_event );
}

/*#***********************************************************************\
*              destroy_ddl_monitoring
*
* Free DDL monitoring
*
* Parameters :
*
*    ddl_monitoring_t* ddl_monitor, in, use, DDL monitoring structure
*
* Return value : true or false
*
* Globals used : -
*/
static inline void destroy_ddl_monitoring(
        ddl_monitoring_t *ddl_monitor)
{
        SsFlatMutexDone( ddl_monitor->ddl_monitoring_mutex );
        SsMesFree( ddl_monitor->ddl_finish_event );
}

static ddl_monitoring_t solid_ddl_monitoring;

/*#***********************************************************************\
*              block_solid_DDL
*
* Wait until no DDL commands are active
*
* Parameters :
*
* Return value : -
*
* Globals used : -
*/
static inline void block_solid_DDL()
{
        bool is_ddl_going = false;

        SsFlatMutexLock( solid_ddl_monitoring.ddl_monitoring_mutex );

        if (solid_ddl_monitoring.number_of_backups < UINT_MAX) {
            solid_ddl_monitoring.number_of_backups++;
        }

        is_ddl_going = solid_ddl_monitoring.number_of_ddls > 0;

        SsFlatMutexUnlock( solid_ddl_monitoring.ddl_monitoring_mutex );

        if ( is_ddl_going ) {
            SsMesWait( solid_ddl_monitoring.ddl_finish_event );
        }
}

/*#***********************************************************************\
*              unlock_solid_DDL
*
* Remove one backup from DDL monitoring
*
* Parameters :
*
* Return value : -
*
* Globals used : -
*/
static inline void unblock_solid_DDL()
{
        SsFlatMutexLock( solid_ddl_monitoring.ddl_monitoring_mutex );

        if (solid_ddl_monitoring.number_of_backups) {
            solid_ddl_monitoring.number_of_backups--;
        }

        SsFlatMutexUnlock( solid_ddl_monitoring.ddl_monitoring_mutex );
}

/*#***********************************************************************\
*              unlock_solid_DDL
*
* Check is the backup active
*
* Parameters :
*
* Return value : true if backup is active, false otherwise
*
* Globals used : -
*/
static inline bool is_solid_DDL_blocked() /* is not actually going to be used */
{
        bool is_solid_DDL_disabled = false;

        SsFlatMutexLock( solid_ddl_monitoring.ddl_monitoring_mutex );
        is_solid_DDL_disabled = solid_ddl_monitoring.number_of_backups > 0;
        SsFlatMutexUnlock( solid_ddl_monitoring.ddl_monitoring_mutex );

        return (is_solid_DDL_disabled);
}

/*#***********************************************************************\
*              start_solid_DDL
*
*
*
* Parameters :
*
* Return value : true if backup is active, false otherwise
*
* Globals used : -
*/
static inline bool start_solid_DDL( int &rc )
{
        bool is_solid_DDL_disabled = false;

        SsFlatMutexLock( solid_ddl_monitoring.ddl_monitoring_mutex );

        is_solid_DDL_disabled = solid_ddl_monitoring.number_of_backups > 0;

        if ( !is_solid_DDL_disabled ) {
            if (solid_ddl_monitoring.number_of_ddls < UINT_MAX) {
                solid_ddl_monitoring.number_of_ddls++;
            }

            SsMesReset( solid_ddl_monitoring.ddl_finish_event );
        }

        SsFlatMutexUnlock( solid_ddl_monitoring.ddl_monitoring_mutex );

        if( is_solid_DDL_disabled ) {
            rc = HA_ERR_WRONG_COMMAND;
            sql_print_error( "DDL commands are temporarily disabled, "
                             "possibly because on-line backup is in progress. "
                             "Try again later.\n" );

            /* ui_msg_message(BACKUP_MSG_FAILED_S, su_err_geterrstr(*p_errh)); */

            ss_pprintf(("DDL commands are temporarily disabled, "
                        "possibly because on-line backup is in progress. "
                        "Try again later.\n"));
        }

        return (!is_solid_DDL_disabled);
}

/*#***********************************************************************\
*              finish_solid_DDL
*
*
*
* Parameters :
*
* Return value : -
*
* Globals used : -
*/
static inline void finish_solid_DDL()
{
        unsigned int prev_number_of_ddls = 0;

        SsFlatMutexLock( solid_ddl_monitoring.ddl_monitoring_mutex );
        prev_number_of_ddls = solid_ddl_monitoring.number_of_ddls;

        if (solid_ddl_monitoring.number_of_ddls) {
            solid_ddl_monitoring.number_of_ddls--;
        }

        if ( prev_number_of_ddls && !solid_ddl_monitoring.number_of_ddls ) {
            /* this DDL was the last one, now there is no DDL active */
            SsMesSend( solid_ddl_monitoring.ddl_finish_event );
        }

        SsFlatMutexUnlock( solid_ddl_monitoring.ddl_monitoring_mutex );
}

/*#***********************************************************************\
*              lock_table
*
* Lock solidDB relation
*
* Parameters :
*
*    SOLID_CONN*  con, in, use, solidDB connection
*    rs_relh_t*   tableh, in, use, solidDB table relation
*    bool         exlusive, in, use, should we use an exclusive lock
*    long         timeout, in, use, lock timeout
*
* Return value : -
*
* Globals used : -
*/
static inline su_err_t* lock_table(
        SOLID_CONN* con,
        rs_relh_t* tableh,
        bool exclusive,
        long timeout)
{
        su_err_t* errh = NULL;
        solid_bool finished = FALSE;
        bool succp = TRUE;

        SDB_DBUG_ENTER("solid::lock_table");
        ss_dassert(con != NULL);
        ss_dassert(tableh != NULL);

        do {

            succp = tb_trans_lockrelh(con->sc_cd,
                                      con->sc_trans,
                                      tableh,
                                      exclusive,
                                      timeout,
                                      &finished,
                                      &errh);

        } while (rs_sysi_lockwait(con->sc_cd) || !finished);

        SDB_DBUG_RETURN(errh);
}

/*#***********************************************************************\
*              lock_table
*
*
*
* Parameters :
*
*    SOLID_CONN*    con, in, use, solidDB connection
*    rs_entname_t*  en, in, use, solidDB table relation name
*    bool           exlusive, in, use, should we use an exclusive lock
*    long           timeout, in, use, lock timeout
*
* Return value : su_err_t error structure
*
* Globals used : -
*/
static inline su_err_t* lock_table(
        SOLID_CONN* con,
        rs_entname_t* en,
        bool exclusive,
        long timeout)
{
        su_err_t* errh = NULL;
        rs_relh_t* tableh = NULL;

        SDB_DBUG_ENTER("solid::lock_table");
        ss_dassert(con != NULL);
        ss_dassert(en != NULL);

        tableh = tb_dd_getrelh(con->sc_cd, con->sc_trans, en, NULL, &errh);

        if (tableh && !errh) {
            errh = lock_table(con, tableh, exclusive, timeout);
            SS_MEM_SETUNLINK(tableh);
            rs_relh_done(con->sc_cd, tableh);
        }

        SDB_DBUG_RETURN(errh);
}

/*#***********************************************************************\
 *
 *              solid_create_handler
 *
 * Create solidDB handler
 *
 * Parameters :
 *
 *      TABLE_SHARE* table, in out, use
 *      MEM_ROOT*    mem_root, in out, use
 *
 * Return value : Initialized handler interface
 *
 * Limitations: MySQL 5.1.x
 *
 * Globals used :
 */

#if MYSQL_VERSION_ID >= 50100
static handler *solid_create_handler(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root)
{
  return new (mem_root) ha_soliddb(hton, table);
}
#endif

/*#***********************************************************************\
 *
 *              ha_map_solid_mysql_fkaction
 *
 * Maps a solid foreign key referential action code to MySQL foreign key
 * referential action. Prints referential action string to buffer if a
 * buffer is given.
 *
 * Parameters :
 *
 *     sqlrefact_t solid_ref_action, in, solid foreign key referential action
 *     char*       buffer, in, buffer where to print action string or NULL
 *
 * Return value : uint, MySQL foreign key referential action code
 *
 * Globals used : -
 */
static inline uint ha_map_solid_mysql_fkaction(
        sqlrefact_t solid_ref_action,
        char *buffer)
{
        uint mysql_ref_action;

        switch (solid_ref_action) {
            case SQL_REFACT_RESTRICT:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_RESTRICT;
#else
                mysql_ref_action = foreign_key::FK_OPTION_RESTRICT;
#endif
                if (buffer) {
                    strcat(buffer, "RESTRICT");
                }
                break;
            case SQL_REFACT_CASCADE:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_CASCADE;
#else
                mysql_ref_action = foreign_key::FK_OPTION_CASCADE;
#endif

                if (buffer) {
                    strcat(buffer, "CASCADE");
                }
                break;
            case SQL_REFACT_SETNULL:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_SET_NULL;
#else
                mysql_ref_action = foreign_key::FK_OPTION_SET_NULL;
#endif

                if (buffer) {
                    strcat(buffer, "SET NULL");
                }
                break;
            case SQL_REFACT_NOACTION:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_NO_ACTION;
#else
                mysql_ref_action = foreign_key::FK_OPTION_NO_ACTION;
#endif

                if (buffer) {
                    strcat(buffer, "NO ACTION");
                }
                break;
            case SQL_REFACT_SETDEFAULT:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_DEFAULT;
#else
                mysql_ref_action = foreign_key::FK_OPTION_DEFAULT;
#endif

                if (buffer) {
                    strcat(buffer, "SET DEFAULT");
                }
                break;
            default:
#if MYSQL_VERSION_ID >= 50120
                mysql_ref_action = Foreign_key::FK_OPTION_UNDEF;
#else
                mysql_ref_action = foreign_key::FK_OPTION_UNDEF;
#endif
                break;
        }

        return (mysql_ref_action);
}

/*#***********************************************************************\
 *
 *              keyname_from_mysql_to_solid
 *
 * Converts MySQL keyname to solidDB keyname
 *
 * Parameters :
 *
 *      rs_sysi_t*  cd, in, use
 *      rs_relh_t*  relh, in, use, solidDB relation
 *      char*       mysql_keyname, in, use
 *      char*       solid_keyname_buffer,in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static inline void keyname_from_mysql_to_solid(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        char* mysql_keyname,
        char* solid_keyname_buffer)
{
        long relid;

        relid = rs_relh_relid(cd, relh);

        SsSprintf(solid_keyname_buffer, "%s_%ld", mysql_keyname, relid);

        solid_my_caseup_str(solid_keyname_buffer);
}

 /*#***********************************************************************\
 *
 *              foreignkey_name_from_mysql_to_solid
 *
 * Converts MySQL foreign key name to solidDB key name
 *
 * Parameters :
 *
 *      rs_sysi_t*  cd, in, use
 *      rs_relh_t*  relh, in, use, solidDB relation
 *      char*       mysql_keyname, in, use
 *
 * Return value : - char* - solid_keyname_buffer
 *
 * Globals used :
 */
static inline char* foreignkey_name_from_mysql_to_solid(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        char* mysql_keyname)
{
        static const size_t LONG_MAX_DECIMAL_DIGITS = 64;
        long relid = rs_relh_relid(cd, relh);
        char* solid_keyname_buffer = (char *)SsMemAlloc(strlen(mysql_keyname)+LONG_MAX_DECIMAL_DIGITS);
        SsSprintf(solid_keyname_buffer, "%s_%ld", mysql_keyname, relid);

        return (solid_keyname_buffer);
}

/*#***********************************************************************\
 *
 *              keyname_from_solid_to_mysql
 *
 * Converts solidDB keyname to MySQL keyname
 *
 * Parameters :
 *
 *      char*       solid_keyname,in, use
 *      char*       mysql_keyname_buffer, in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static inline void keyname_from_solid_to_mysql(
        char* solid_keyname,
        char* mysql_keyname_buffer)
{
        uint pos;
        char *tmp;

        ss_dassert(solid_keyname);
        ss_dassert(mysql_keyname_buffer);

        pos = strlen(solid_keyname)-1;
        strcpy(mysql_keyname_buffer, solid_keyname);

        tmp = mysql_keyname_buffer + pos;
        ss_dassert(tmp);

        while(tmp != mysql_keyname_buffer) {
            if (*tmp == '_' ) {
                *tmp = '\0';
                break;
            }

            tmp--;
        }
}

#ifdef SS_MYSQL_AC

#define FRM_CATALOG RS_AVAL_DEFCATALOG
#define FRM_SCHEMA "_SYSTEM"
#define FRM_RELNAME "SYS_MYFILES"
#define RS_ANAME_FRM_TYPE "TYPE"
#define RS_ANAME_SCHEMA "SSCHEMA"
#define RS_ANAME_NAME "SNAME"
#define RS_ANAME_FRM_NAME "FNAME"
#define RS_ANAME_FRM_DATA "DATA"
#define FRM_MAXSIZE (32*1024)

#define MYFILE_TYPE_FRM 1

/* give this (and next) function to solidDB when starting database.
 * See tb_srv_xxx for details
 */

/*#***********************************************************************\
*              frm_from_dist_to_db
*
*
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static int frm_from_disk_to_db(
        tb_connect_t* tbcon,
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        rs_entname_t *en,
        const char   *name,
        int           deletep)
{
        uint length = 0;
        char *val_sschema;
        char *val_sname;
        char *val_name;
        const void *data;
        char *val_data;
        size_t val_length;
        long val_type;
        TliConnectT* tli_con;
        TliCursorT* tcur;
        TliRetT trc;

        ss_pprintf_1(("frm_from_disk_to_db:name [%s]\n", name));

        if (!deletep) {
            if (readfrm(name, &data, &length)) {
                ss_pprintf_1(("frm_from_disk_to_db:readfrm FAILED\n"));
                return(FALSE);
            }
            ss_pprintf_1(("frm_from_disk_to_db:frm length %d\n", length));
        }

        tli_con = TliConnectInitByTrans(cd, trans);
        ss_dassert(tli_con != NULL);

        tcur = TliCursorCreate(tli_con, FRM_CATALOG, FRM_SCHEMA, FRM_RELNAME);
        ss_dassert(tcur != NULL);

        TliCursorSetMaxBlobSize(tcur, FRM_MAXSIZE);

        trc = TliCursorColLong(tcur, RS_ANAME_FRM_TYPE, &val_type);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMA, &val_sschema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_NAME, &val_sname);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorColStr(tcur, RS_ANAME_FRM_NAME, &val_name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColData(tcur, RS_ANAME_FRM_DATA, &val_data, &val_length);
        ss_dassert(trc == TLI_RC_SUCC);

        val_name = (char*) name;
        if (en != NULL) {
            //trc = TliCursorConstrStr(tcur, RS_ANAME_FRM_NAME, TLI_RELOP_EQUAL, val_name);
            val_sschema = en->en_schema;
            val_sname   = en->en_name;

            trc = TliCursorConstrStr(tcur, RS_ANAME_SCHEMA, TLI_RELOP_EQUAL, val_sschema);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorConstrStr(tcur, RS_ANAME_NAME, TLI_RELOP_EQUAL, val_sname);
            ss_dassert(trc == TLI_RC_SUCC);
        }

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_SUCC) {
            ss_pprintf_1(("deletefrm:%s\n", name));
            trc = TliCursorDelete(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorNext(tcur);
        }
        ss_dassert(trc == TLI_RC_END);

        if (!deletep) {
            ss_dassert(en != NULL);
            val_type = MYFILE_TYPE_FRM;
            val_sschema = en->en_schema;
            val_sname   = en->en_name;
            val_name = (char*) name;
            val_data = (char*) data;
            val_length = (size_t)length;
            trc = TliCursorInsert(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
        }
        TliCursorFree(tcur);
        TliConnectDone(tli_con);

        if (!deletep) {
            my_free((char*)data);
        }
        return(TRUE);
}

/*#***********************************************************************\
*              frm_from_db_to_db_rename
*
*
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static int frm_from_db_to_db_rename(
        tb_connect_t* tbcon,
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        rs_entname_t *old_en,
        const char   *old_name,
        rs_entname_t *new_en,
        const char   *new_name)
{
        uint length = 0;
        char *val_sschema;
        char *val_sname;
        char *val_name;
        const void *data;
        char *val_data;
        size_t val_length;
        long val_type;
        TliConnectT* tli_con;
        TliCursorT* tcur;
        TliRetT trc;

        ss_pprintf_1(("frm_from_db_to_db_rename:[%s] --> [%s]\n", old_name, new_name));

        //if (readfrm(name, &data, &length)) {
        //    ss_pprintf_1(("frm_from_disk_to_db:readfrm FAILED\n"));
        //    return(FALSE);
        //}
        //ss_pprintf_1(("frm_from_disk_to_db:frm length %d\n", length));

        tli_con = TliConnectInitByTrans(cd, trans);
        ss_dassert(tli_con != NULL);

        tcur = TliCursorCreate(tli_con, FRM_CATALOG, FRM_SCHEMA, FRM_RELNAME);
        ss_dassert(tcur != NULL);

        TliCursorSetMaxBlobSize(tcur, FRM_MAXSIZE);

        trc = TliCursorColLong(tcur, RS_ANAME_FRM_TYPE, &val_type);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMA, &val_sschema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_NAME, &val_sname);
        ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorColStr(tcur, RS_ANAME_FRM_NAME, &val_name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColData(tcur, RS_ANAME_FRM_DATA, &val_data, &val_length);
        ss_dassert(trc == TLI_RC_SUCC);

        val_name = (char*) old_name;
        ss_dassert(old_en != NULL);
        ss_dassert(new_en != NULL);

        val_sschema = old_en->en_schema;
        val_sname   = old_en->en_name;

        trc = TliCursorConstrStr(tcur, RS_ANAME_SCHEMA, TLI_RELOP_EQUAL, val_sschema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorConstrStr(tcur, RS_ANAME_NAME, TLI_RELOP_EQUAL, val_sname);
        ss_dassert(trc == TLI_RC_SUCC);

        //trc = TliCursorConstrStr(tcur, RS_ANAME_FRM_NAME, TLI_RELOP_EQUAL, val_name);
        //ss_dassert(trc == TLI_RC_SUCC);

        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        if (trc == TLI_RC_SUCC) {
            ss_pprintf_1(("rename to:%s\n", new_name));
            val_sschema = new_en->en_schema;
            val_sname   = new_en->en_name;
            val_name = (char*) new_name;
            trc = TliCursorUpdate(tcur);
            ss_dassert(trc == TLI_RC_SUCC);
            trc = TliCursorNext(tcur);
        }
        ss_dassert(trc == TLI_RC_END);

        //if (!deletep) {
        //    val_type = MYFILE_TYPE_FRM;
        //    val_name = (char*) name;
        //    val_data = (char*) data;
        //    val_length = (size_t)length;
        //    trc = TliCursorInsert(tcur);
        //    ss_dassert(trc == TLI_RC_SUCC);
        //}

        TliCursorFree(tcur);
        TliConnectDone(tli_con);

        //my_free((char*)data, MYF(0));

        return(TRUE);
}

/*#***********************************************************************\
*              frm_from_db_to_disk
*
*
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static int frm_from_db_to_disk(
        tb_connect_t* tbcon,
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        rs_entname_t *en,
        int           deletep)
{
        int succp = TRUE;
        const void *frmdata;
        uint len;

        uint length = 0;
        const char   *name = NULL;
        char *val_sschema;
        char *val_sname;
        char *val_name;
        const void *data;
        char *val_data;
        size_t val_length;
        long val_type;
        TliConnectT* tli_con;
        TliCursorT* tcur;
        TliRetT trc;

        ss_pprintf_1(("frm_from_db_to_disk:name [%s]\n", en != NULL ? en->en_name : "all"));

        tli_con = TliConnectInitByTrans(cd, trans);
        ss_dassert(tli_con != NULL);

        tcur = TliCursorCreate(tli_con, RS_AVAL_DEFCATALOG, FRM_SCHEMA, FRM_RELNAME);
        ss_dassert(tcur != NULL);

        TliCursorSetMaxBlobSize(tcur, FRM_MAXSIZE);
        trc = TliCursorColLong(tcur, RS_ANAME_FRM_TYPE, &val_type);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_SCHEMA, &val_sschema);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_NAME, &val_sname);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColStr(tcur, RS_ANAME_FRM_NAME, &val_name);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorColData(tcur, RS_ANAME_FRM_DATA, &val_data, &val_length);
        ss_dassert(trc == TLI_RC_SUCC);

        val_name = (char*) name;
        if (name != NULL) {
            trc = TliCursorConstrStr(tcur, RS_ANAME_FRM_NAME, TLI_RELOP_EQUAL, val_name);
            ss_dassert(trc == TLI_RC_SUCC);
        }
        trc = TliCursorOpen(tcur);
        ss_dassert(trc == TLI_RC_SUCC);
        trc = TliCursorNext(tcur);

        while (trc == TLI_RC_SUCC && succp) {
            ss_pprintf_1(("frm_from_db_to_disk:frm length %d\n", val_length));
            ss_pprintf_1(("frm_from_db_to_disk:frm length %d\n", val_length));

            switch (val_type) {
              case MYFILE_TYPE_FRM:
                length = val_length;
                if (writefrm(val_name, val_data, length)) {
                  succp = FALSE;
                }
                break;
              default:
                ss_rc_error(val_type);
                break;
            }
            if (name == NULL) {
                trc = TliCursorNext(tcur);
            } else {
                break;
            }
        }
        ss_rc_dassert(trc == TLI_RC_END || trc == TLI_RC_SUCC, trc);

        TliCursorFree(tcur);
        TliConnectDone(tli_con);

        return(succp);
}

static mysql_funblock_t mysql_funblock = {
        frm_from_disk_to_db,
        frm_from_db_to_disk,
        solidcon_throwout
};

#else /* SS_MYSQL_AC */
static mysql_funblock_t mysql_funblock = {
        NULL,
        NULL,
        NULL
};
#endif /* SS_MYSQL_AC */

/*#***********************************************************************\
 *
 *              solid_get_error_message
 *
 * Get error key, table and message
 *
 * Parameters :
 *
 *      handlerton*, hton, in, use, MySQL handlerton structure on 5.1
 *      THD*     thd, in, use, MySQL thread
 *      TABLE**  table, out, MySQL table where error occurred
 *      int      error_num, in, not used, error value
 *      String*  error_string, inout, not used, error message
 *
 * Return value : key number
 *
 * Globals used :
 */
int solid_get_error_message(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD thd,
        TABLE ** table __attribute__ ((unused)) /* table - unused */,
        int error_num,
        String* error_string)
{
        SOLID_CONN* con;
        int key_num;
        su_err_t* errh = NULL;

        ss_dprintf_1(("solid_get_error_message\n"));
        SS_PUSHNAME("solid_get_error_message");

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection((handlerton *)hton, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        key_num = WRONG_KEY_NO;

        if (con->sc_err_tableid && con->sc_errkey) {

            rs_relh_t* relh;

            relh = tb_dd_getrelhbyid(con->sc_cd, con->sc_trans,
                                     con->sc_err_tableid, NULL, &errh);

            if (errh) {
                sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
                ss_derror;
                su_err_done(errh);
            }

            if (relh) {
                rs_keytype_t keytype;

                keytype = rs_key_type(con->sc_cd, con->sc_errkey);
                key_num = solid_key_to_mysqlkeyidx(con->sc_cd, relh, rs_key_id(con->sc_cd,con->sc_errkey));


                if(key_num > -1 && key_num < WRONG_KEY_NO) {
                    if (error_num == HA_ERR_FOUND_DUPP_KEY) {
                        char* uniquerrorkeyvalue =
                            tb_trans_getuniquerrorkeyvalue(con->sc_cd, con->sc_trans);

                        if (uniquerrorkeyvalue) {
                            error_string->append(uniquerrorkeyvalue);
                        } else {
                            ss_derror;
                            error_string->append("...");
                        }
                    } else {
                        char *foreign_str = NULL;

                        if (error_num == HA_ERR_ROW_IS_REFERENCED ||
                            error_num == HA_ERR_NO_REFERENCED_ROW) {

                            if ((keytype == RS_KEY_PRIMKEYCHK ||
                                 keytype == RS_KEY_FORKEYCHK) &&
                                relh != NULL) {
                                foreign_str = get_foreign_key_str(con, relh, con->sc_errkey);
                            }
                        }

                        if (foreign_str != NULL) {
                            error_string->append(foreign_str);
                            my_free(foreign_str);
                        } else {
                            error_string->append(rs_key_name(con->sc_cd, con->sc_errkey));
                        }
                    }
                }
            }

            con->sc_err_tableid = 0;
            con->sc_errkey = NULL;

            if (relh != NULL) {
                SS_MEM_SETUNLINK(relh);
                rs_relh_done(con->sc_cd, relh);
            }
        }

        SS_POPNAME;

        return (key_num);
}

/*#***********************************************************************\
 *
 *              solid_get_key
 *
 * Create our own hash to get a key
 *
 * Parameters :
 *
 *      SOLIDDB_SHARE* share, in out, use
 *
 * Return value : byle*  table name
 *
 * Globals used :
 */
static SS_MYSQL_ROW* soliddb_get_key(
        SOLIDDB_SHARE *share,
#if MYSQL_VERSION_ID >= 50100
        size_t *length,
#else
        uint   *length,
#endif
        my_bool not_used __attribute__((unused)))
{
#if MYSQL_VERSION_ID >= 50100
        *length=(size_t)share->table_name_length;
#else
        *length=(uint)share->table_name_length;
#endif

        return (SS_MYSQL_ROW*) share->table_name;
}

/*#***********************************************************************\
 *
 *              check_solid_sysrel
 *
 *
 *
 * Parameters :
 *
 *      char* relname, in, use
 *
 * Return value : Index to system relation or -1
 *
 * Globals used :
 */
static int check_solid_sysrel(char* relname)
{
        int i;

        for (i = 0; solid_sysrel_map[i].srm_mysqlname != NULL; i++) {
            if (strcmp(solid_sysrel_map[i].srm_mysqlname, relname) == 0) {
                return(i);
            }
        }
        return(-1);
}

/*#***********************************************************************\
 *
 *              get_share
 *
 * Example of simple lock controls. The "share" it creates is structure we will
 * pass to each example handler. Do you have to have one of these? Well, you have
 * pieces that are used for locking, and they are needed to function.
 *
 * Parameters :
 *
 *      const char* table_name, in, use
 *
 * Return value : SOLIDDB_SHARE *
 *
 * Globals used :
 */
static SOLIDDB_SHARE *get_share(
        const char *table_name)
{
        SOLIDDB_SHARE *share;
        uint length;

        if (!soliddb_init)
        {
            soliddb_init++;
            VOID(pthread_mutex_init(&soliddb_mutex, MY_MUTEX_INIT_FAST));

            (void) my_hash_init(&soliddb_open_tables, system_charset_info, 32, 0, 0,
                             (my_hash_get_key) soliddb_get_key, 0, 0);
        }

        pthread_mutex_lock(&soliddb_mutex);
        length=(uint) strlen(table_name);

        if (!(share= ((SOLIDDB_SHARE*)my_hash_search(&soliddb_open_tables,
                                                  (SS_MYSQL_ROW *) table_name,
                                                  length)))) {
            share=(SOLIDDB_SHARE *) my_malloc(sizeof(*share)+length+1,
                                              MYF(MY_FAE | MY_ZEROFILL));
            share->table_name_length=length;
            share->table_name=(char *)(share+1);
            strmov(share->table_name, table_name);

            if (my_hash_insert(&soliddb_open_tables, (SS_MYSQL_ROW*) share)) {
                pthread_mutex_unlock(&soliddb_mutex);
                my_free((char *) share);

                return 0;
            }

            thr_lock_init(&share->lock);
            pthread_mutex_init(&share->mutex, MY_MUTEX_INIT_FAST);
        }

        share->use_count++;
        pthread_mutex_unlock(&soliddb_mutex);

        return share;
}

/*#***********************************************************************\
 *
 *              free_share
 *
 * Free lock controls. We call this whenever we close a table. If the table had
 * the last reference to the share then we free memory associated with it.
 *
 * Parameters :
 *
 *     SOLIDDB_SHARE* share
 *
 * Return value : -
 *
 * Globals used :
 */
static void free_share(SOLIDDB_SHARE *share)
{
        pthread_mutex_lock(&soliddb_mutex);

        if (!--share->use_count)
        {
            my_hash_delete(&soliddb_open_tables, (SS_MYSQL_ROW*) share);
            thr_lock_delete(&share->lock);
            pthread_mutex_destroy(&share->mutex);
            my_free((char *) share);
        }

        pthread_mutex_unlock(&soliddb_mutex);
}

/*#***********************************************************************\
 *
 *              solid_message_fp
 *
 *
 *
 * Parameters :
 *
 *     ui_msgtype_t type
 *     char*        msg
 *     int          newline
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_message_fp(ui_msgtype_t type, su_msgret_t msgcode, char* msg, solid_bool newline)
{
        if (newline && msg[strlen(msg)-1] == '\n') {
            newline = FALSE;
        }

        if (solid_printmsg) {
            if (newline) {
                printf("%s\n", msg);
            } else {
                printf("%s", msg);
            }
        }

        if (solid_msglog != NULL) {
            char *format;

            format = (char *)SsMemAlloc(SS_MSGLOG_BUFSIZE-100+1);

            if (newline) {
                SsSprintf(format, (char *)"%%.%ds\n", SS_MSGLOG_BUFSIZE-100);
            } else {
                SsSprintf(format, (char *)"%%.%ds", SS_MSGLOG_BUFSIZE-100);
            }
            SsMsgLogPrintfWithTime(solid_msglog, format, msg);
            SsMsgLogFlush(solid_msglog);

            SsMemFree(format);
        }
}

/*#***********************************************************************\
 *
 *              assert_message_fp
 *
 *
 *
 * Parameters :
 *
 *     char*        msg
 *
 * Return value : -
 *
 * Globals used :
 */
static void assert_message_fp(const char* msg)
{
        solid_printmsg = TRUE;
        solid_message_fp(UI_MSG_ERROR, SRV_MSG_ASSERTION, (char *)msg, TRUE);
}

/*#***********************************************************************\
 *
 *              solid_set_logdir
 *
 * Set up directory for log files.
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use
 *     const char*   soliddb_logdir, in, use, directory name
 *
 * Return value : TRUE if successfull
 *                FALSE otherwice
 *
 * Globals used :
 */
static bool solid_set_logdir(
        su_inifile_t* inifile,
        const char *  soliddb_logdir)
{
        ss_pprintf_1(("LogDir=%s\n", soliddb_logdir));

        su_inifile_putstring(inifile, (char *)SU_DBE_LOGSECTION,
                            (char *)SU_DBE_LOGDIR, soliddb_logdir);

        return (TRUE);
}

/*#***********************************************************************\
 *
 *              solid_set_backupdir
 *
 * Set up directory for backup files.
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use, solid configuration
 *     const char*   soliddb_backupdir, in, use, backup directory
 *
 * Return value : TRUE
 *
 * Globals used :
 */
static bool solid_set_backupdir(
        su_inifile_t* inifile,
        const char *  soliddb_backupdir)
{
        ss_pprintf_1(("BackupDir=%s\n", soliddb_backupdir));

        su_inifile_putstring(inifile, (char *)SU_DBE_GENERALSECTION,
                            (char *)SU_DBE_BACKUPDIR, soliddb_backupdir);

        return (TRUE);
}

/*#***********************************************************************\
 *
 *              soliddb_check_backupdir
 *
 * Check directory for backup files.
 *
 * Parameters :
 *
 *     THD*      thd, in, use
 *     set_var*  var, in, use, variable value from MySQL
 *
 * Return value : 0
 *
 * Globals used :
 */
int soliddb_check_backupdir(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        struct st_mysql_value*          value,          /*!< out: where the
                                                        formal string goes */
        void*                           save)           /*!< in: immediate result
                                                        from check function */
{
        const char *str;
#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        length= sizeof(buff);
        str= value->val_str(value, buff, &length);
#else
        str = var ? var->value->str_value.c_ptr_quick() : NULL;
#endif

        ss_pprintf_1(("BackupDir= [%s]\n", str ? str : "NULL"));

        return str ? 0 : -1;
}

/*#***********************************************************************\
 *
 *              solid_update_backupdir
 *
 * Update directory for backup files.
 *
 * Parameters :
 *
 *     THD*      thd, in, use
 *     set_var*  var, in, use
 *
 * Return value : TRUE if successfull
 *                FALSE otherwice
 *
 * Globals used :
 */

void soliddb_update_backupdir(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           value,          /*!< out: where the
                                                        formal string goes */
        const void*                     save)           /*!< in: immediate result
                                                        from check function */
{
        char* new_value;
#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        length= sizeof(buff);
        new_value = (char *)value;

        if (!new_value) {
            new_value = (char *)"";
        }
#else

        new_value = (char *)var->value->str_value.c_ptr_quick();

        if (var->value->null_value || !new_value) {
            new_value = (char *)"";
        }
#endif

        DBUG_ENTER("soliddb_update_backupdir");
        ss_pprintf_1(("BackupDir=%s\n", new_value));

        new_value = SsStrTrimLeft(new_value);
        SsStrTrimRight(new_value);

        SsFlatMutexLock(soliddb_backupdir_mutex);
        strcpy(soliddb_backupdir_buf, new_value);
        SsFlatMutexUnlock(soliddb_backupdir_mutex);

#if MYSQL_VERSION_ID >= 50100
        DBUG_VOID_RETURN;
#else
        DBUG_RETURN(0);
#endif
}

/*#***********************************************************************\
 *
 *              solid_parseandset_filespec
 *
 * Set up paths for database files given on a MySQL configuration file
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use, solid configuration
 *     const char *  soliddb_filespec, in, use, database file specification
 *
 * Return value : TRUE in case of success
 *                FALSE on error
 *
 * Globals used :
 */
static bool solid_parseandset_filespec(
        su_inifile_t* inifile,
        const char*   soliddb_filespec)
{
        char* FileSpec = NULL;
        char* FileSize = NULL;
        char* FileName = NULL;
        char* InputString = NULL;
        char* saveptr = NULL;
        char* saveptr2 = NULL;
        char* NewFileSpec = NULL;
        char* OriginalPtr = NULL;
        int   n_filespec = 1;
        int   len;

        ss_dassert(soliddb_filespec != NULL);
        ss_dassert(inifile != NULL);

        SS_PUSHNAME("solid_parseandset_filespec");

        len = strlen(soliddb_filespec);
        InputString = (char *)SsMemAlloc(len+2);
        OriginalPtr = InputString;
        memcpy(InputString, soliddb_filespec,len+1);

        FileSpec = strtok_r(InputString, "," , &saveptr);

        if (FileSpec == NULL) {
            SS_POPNAME;
            return (FALSE);
        }

        while (FileSpec) {
            FileName = strtok_r(FileSpec, " ", &saveptr2);
            FileSize = strtok_r(NULL, ",", &saveptr2);

            if (FileName && FileSize) {
                char* KeyName;

                NewFileSpec = (char *)SsMemAlloc(strlen(FileSpec) + strlen(FileSize) + 2);
                sprintf(NewFileSpec, "%s %s",FileSpec, FileSize);
                ss_pprintf_1(((char *)"FileSpec_%u=%s\n", n_filespec, NewFileSpec));
                KeyName = (char *)SsMemAlloc(strlen(SU_DBE_FILESPEC) + 32);
                sprintf(KeyName, SU_DBE_FILESPEC, n_filespec);
                ss_pprintf_1(((char *)"KeyName=%s\n", KeyName));
                su_inifile_putstring(inifile, (char *)SU_DBE_INDEXSECTION, KeyName, NewFileSpec);
                n_filespec++;

                SsMemFree(KeyName);
                SsMemFree(NewFileSpec);

                FileSpec = strtok_r(NULL, ",", &saveptr);
            } else {
                SsMemFree(OriginalPtr);
                SS_POPNAME;
                return (FALSE);
            }
        }

        SsMemFree(OriginalPtr);

        SS_POPNAME;
        return (TRUE);
}

/*#***********************************************************************\
 *
 *              solid_set_longlong
 *
 * Set up longlong parameter given in a MySQL configuration file.
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use
 *     longlong      value, in, use, configuration parameter value
 *     const char*   section, in, use, section on inifile
 *     const char*   param, in, use, parameter in inifile
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_set_longlong(
        su_inifile_t* inifile,
        longlong      value,
        const char*   section,
        const char*   param)
{
        char* string;

        string = (char *)SsMemAlloc(64);
        sprintf(string, "%lld", value);
        ss_pprintf(("%s=%s\n", param, string));

        su_inifile_putstring(inifile, section, param, string);

        SsMemFree(string);
}

/*#***********************************************************************\
 *
 *              solid_set_bool
 *
 * Set up bool parameter given in a MySQL configuration file
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use
 *     bool          value, in, use, configuration parameter value
 *     const char*   section, in, use, section on inifile
 *     const char*   param, in, use, parameter in inifile
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_set_bool(
        su_inifile_t* inifile,
        bool          value,
        const char*   section,
        const char*   param)
{
        char* string;

        string = (char *)SsMemAlloc(4);

        if (value) {
            sprintf(string, "yes");
        } else {
            sprintf(string, "no");
        }

        ss_pprintf(("%s=%s\n", param, string));
        su_inifile_putstring(inifile, section, param, string);

        SsMemFree(string);
}

/*#***********************************************************************\
 *
 *              solid_set_long
 *
 * Set up long parameter given in a MySQL configuration file
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use
 *     long          value, in, use, configuration parameter value
 *     const char*   section, in, use, section on inifile
 *     const char*   param, in, use, parameter in inifile
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_set_long(
        su_inifile_t* inifile,
        long          value,
        const char*   section,
        const char*   param)
{
        char* string;

        string = (char *)SsMemAlloc(64);

        sprintf(string, "%ld", value);
        ss_pprintf(("%s=%s\n", param, string));

        su_inifile_putstring(inifile, section, param, string);

        SsMemFree(string);
}

/*#***********************************************************************\
 *
 *              solid_set_block_size
 *
 * Set up log block size parameter given in a MySQL configuration file
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use
 *     long          block_size, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_set_block_size(
        su_inifile_t* inifile,
        long          block_size)
{
        char* blocksize_string;

        blocksize_string = (char *)SsMemAlloc(64);
        sprintf(blocksize_string, "%ld", block_size);
        ss_pprintf(("BlockSize=%s\n", blocksize_string));

        su_inifile_putstring(inifile, SU_DBE_LOGSECTION, SU_DBE_BLOCKSIZE,
                             blocksize_string);

        SsMemFree(blocksize_string);
}

/*#***********************************************************************\
 *
 *              solid_inifile_saveas
 *
 * Save solidDB configuration to a file
 *
 * Parameters :
 *
 *     su_inifile_t* inifile, in out, use, solid configuration,
 *     char*         fname, in, use, file name
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_inifile_saveas(
        su_inifile_t* inifile,
        char*         fname)
{
        su_vfile_t *vfp = NULL;
        SS_FILE* fp = NULL;

        vfp = su_vfp_init_txt(fname, (char *)"w");
        fp = su_vfp_access(vfp);
        su_inifile_savefp(inifile, fp);
        su_vfp_done(vfp);
}

/*#***********************************************************************\
 *
 *              solid_startedif
 *
 * Start solidDB server if it has not been started.
 *
 * Parameters : -
 *
 * Return value : TRUE if solidDB started
 *                FALSE if solidDB can't be started
 *
 * Globals used :
 */
static bool solid_startedif(void)
{
        bool success = TRUE;

        if (!solid_started) {
            su_inifile_t* inifile;
            bool succp = TRUE;

            tb_dd_collation_init = soliddb_collation_init;

            rs_aval_print_externaldatatype = soliddb_aval_print_externaldatatype;

            tb_globalinit();

            solid_collation_mutex = SsSemCreateLocal(SS_SEMNUM_HA_COLLATION);

            SS_PUSHNAME("solid_startedif");

            SsSetAssertMessageHeader((char *)"solidDB Error");
            SsSetAssertMessageFunction(assert_message_fp);

            ui_msg_setdba((char *)"mysql", (char *)"mysql");
            ui_msg_setdefcatalog((char *)"dba");

            solid_printmsg = TRUE;
            ui_msg_setmessagefp(solid_message_fp);
            solid_msglog = SsMsgLogInit((char *)SU_SOLID_MSGFILENAME, 1024*1024);

            ui_msg_message(SRV_MSG_VERSION_S, SsGetVersionstring(FALSE));
            ui_msg_message(0, "This is a solidDB for MySQL build.");
            ui_msg_message(SRV_MSG_COPYRIGHT_S, ss_copyright);

            /* Create a inifile and read the contents of the inifile */
            inifile = su_inifile_init(SU_SOLINI_FILENAME, NULL);
            ss_pprintf_1(("solid_startedif:tb_init\n"));

            /* Now set MySQL/solidDB startup options to inifile. This means
               that MySQL startup options overdrive Solid configuration
               options. */

#ifdef SS_MME
            ui_msg_message(0, "Main Memory Engine is in use");
#endif

            /* Set up CacheSize */
            solid_set_longlong(inifile, soliddb_cache_size,
                               SU_DBE_INDEXSECTION, SU_DBE_CACHESIZE);

            /* Set up CheckPointDeleteLog */
            solid_set_bool(inifile, soliddb_checkpoint_deletelog,
                           SU_DBE_GENERALSECTION, SU_DBE_CPDELETELOG);

            /* Set up LogEnabled */
            solid_set_bool(inifile, soliddb_log_enabled,
                           SU_DBE_LOGSECTION, SU_DBE_LOGENABLED);

            /* Set up Pessimistic */
            solid_set_bool(inifile, soliddb_pessimistic,
                           SU_DBE_GENERALSECTION, SU_DBE_PESSIMISTIC);

            /* Set DirectIO for indexfile and logfile */
#ifdef IO_OPT
            solid_set_bool(inifile, TRUE, SU_DBE_INDEXSECTION, SU_DBE_DIRECTIO);
            solid_set_bool(inifile, TRUE, SU_DBE_LOGSECTION, SU_DBE_DIRECTIO);
            fprintf(stderr, " SolidDB: Direct IO is being used\n");

            ui_msg_message(0, "Direct IO is being used.");
#endif

            /* Set up LockWaitTimeout */
            solid_set_long(inifile, soliddb_lock_wait_timeout,
                           SU_DBE_GENERALSECTION, SU_DBE_PESSIMISTIC_LOCK_TO);

            solid_set_long(inifile, soliddb_optimistic_lock_wait_timeout,
                           SU_DBE_GENERALSECTION, SU_DBE_OPTIMISTIC_LOCK_TO);

            /* Set up DurabilityLevel */
            solid_set_long(inifile, soliddb_durability_level,
                           SU_DBE_LOGSECTION, SU_DBE_DURABILITYLEVEL);
            fprintf(stderr, " SolidDB: Durability Level set to %ld\n",soliddb_durability_level);

            /* Set up BlockSize */
            if (soliddb_log_block_size != 0) {
                solid_set_long(inifile, soliddb_log_block_size,
                               SU_DBE_LOGSECTION, SU_DBE_BLOCKSIZE);
            }

            if (soliddb_db_block_size != 0) {
                solid_set_long(inifile, soliddb_db_block_size,
                               SU_DBE_INDEXSECTION, SU_DBE_BLOCKSIZE);
            }

            if (soliddb_backup_block_size != 0) {
                solid_set_long(inifile, soliddb_backup_block_size,
                               SU_DBE_GENERALSECTION, SU_DBE_BACKUP_BLOCKSIZE);
            }

            /* Set up CheckPointInterval */
            solid_set_longlong(inifile, soliddb_checkpoint_interval,
                               SU_DBE_GENERALSECTION, SU_DBE_CPINTERVAL);

            /* Set up LockHashSize */
            solid_set_long(inifile, soliddb_lockhash_size,
                               SU_DBE_GENERALSECTION, SU_DBE_LOCKHASHSIZE);

            if (soliddb_checkpoint_time != 0) {
                /* Set up MinCheckpointTime */
                solid_set_long(inifile, soliddb_checkpoint_time,
                               SU_DBE_GENERALSECTION, SU_DBE_CPMINTIME);
            } else {
                soliddb_checkpoint_time = DBE_DEFAULT_CPMINTIME;
            }

            if (soliddb_io_threads != 0) {
                /* Set up I/O threads */
                solid_set_long(inifile, soliddb_io_threads,
                               SU_DBE_GENERALSECTION, SU_DBE_NUMIOTHREADS);
            } else {
                soliddb_io_threads = DBE_DEFAULT_NUMIOTHREADS;
            }
            fprintf(stderr, " SolidDB: IO Threads: %ld\n", soliddb_io_threads);

            if (soliddb_write_threads != 0) {
                /* Set up WriteThreads */
                solid_set_long(inifile, soliddb_write_threads,
                               SU_DBE_GENERALSECTION, SU_DBE_NUMWRITERIOTHREADS);
            } else {
                soliddb_write_threads = DBE_DEFAULT_NUMWRITERIOTHREADS;
            }
            fprintf(stderr, " SolidDB: Write Threads: %ld\n", soliddb_write_threads);

            /* Set up ExtendIncrement */
            solid_set_long(inifile, soliddb_extend_increment,
                           SU_DBE_INDEXSECTION, SU_DBE_EXTENDINCR);

            /* Set up ReadAhead */
            solid_set_long(inifile, soliddb_readahead,
                           SU_DBE_LOGSECTION, SU_DBE_READAHEADSIZE);

            /* Set up LogDir. */
            if (soliddb_logdir != NULL) {
                success = solid_set_logdir(inifile, soliddb_logdir);
            }

            /* Set up Backup directory. */
            if (soliddb_backupdir != soliddb_backupdir_buf) {
                if (soliddb_backupdir != NULL && soliddb_backupdir[0]) {
                    strncpy(soliddb_backupdir_buf,soliddb_backupdir,sizeof(soliddb_backupdir_buf)-1);
                }

                soliddb_backupdir = soliddb_backupdir_buf;
                success = solid_set_backupdir(inifile, soliddb_backupdir);
            }


            /* Parse and set up FileSpec */
            fprintf(stderr, " SolidDB: Initializing Filespec\n");
            if (success && soliddb_filespec != NULL) {
                success = solid_parseandset_filespec(inifile, soliddb_filespec);
            }

            if (success) {
                SsFlatMutexInit( &soliddb_backupdir_mutex, SS_SEMNUM_SOLIDDB_BACKUP_DIR );
/*
#if defined(HAVE_SOLIDDB_BACKUP_NEW) && !defined(MYSQL_DYNAMIC_PLUGIN)
                init_backup();
#endif
*/
                init_backup();

                init_ddl_monitoring( &solid_ddl_monitoring );
                init_disabled_keys();
                init_disabled_forkeys();
                solid_started = tb_srv_init(inifile, NULL, &mysql_funblock);
                solid_inifile = inifile;
            }

#ifdef SS_DEBUG
            solid_inifile_saveas(inifile, (char *)"mysql_soliddb_inifile.out");
#endif

            if (solid_started) {
                /* Create a global mutex for relation cursor list access.
                 * TODO: Semnum is not exactly correct but works ok here.
                 */
                solid_mutex = SsSemCreateLocal(SS_SEMNUM_SSE_LOCSRV);
                succp = tb_srv_start(tb_getlocal());
                ss_dassert(succp);
                ui_msg_message(SRV_MSG_DB_STARTED);
                solid_printmsg = FALSE;
                su_collation_init();
                soliddb_collations_init(tb_getlocal());
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
                tb_dd_drop_fkeys_unresolved(tb_getlocal());
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */                
                fprintf(stderr, " SolidDB: Initialization is complete\n");
                SS_POPNAME;
            } else {
                SsMsgLogT* msglog;
                SS_POPNAME;
                solid_printmsg = TRUE;
                msglog = solid_msglog;
                solid_msglog = NULL;
                SsMsgLogDone(msglog);
                tb_globaldone();
                su_inifile_done(inifile);
                fprintf(stderr, " SolidDB: Initialization failed\n");
            }
        }

        return(solid_started);
}

/*#***********************************************************************\
 *
 *              solid_end
 *
 * Shutdown solidDB server
 *
 * Parameters : unused
 *
 * Return value : 0
 *
 * Globals used :
 */
#if MYSQL_VERSION_ID >= 50100
static int solid_end(
        handlerton *hton,
        ha_panic_function type)
#else
       int solid_end(void)
#endif
{
        if (solid_collation_mutex != NULL) {
            SsSemFree(solid_collation_mutex);
            solid_collation_mutex = NULL;
        }

        if (solid_started) {
            SsMsgLogT* msglog = solid_msglog;
            ss_pprintf_1(("Stop solid server...\n"));
            solid_printmsg = TRUE;
            tb_srv_stop();
            ui_msg_message(SRV_MSG_SHUTDOWN_S, SS_SERVER_NAME);
            SsSemFree(solid_mutex);
            solid_mutex = NULL;
            SsFlatMutexDone( soliddb_backupdir_mutex );

/*
#if defined(HAVE_SOLIDDB_BACKUP_NEW) && !defined(MYSQL_DYNAMIC_PLUGIN)
            free_backup();
#endif
*/
            free_backup();

            destroy_ddl_monitoring( &solid_ddl_monitoring );
            free_disabled_keys();
            free_disabled_forkeys();
            solid_msglog = NULL;
            SsMsgLogDone(msglog);

            if (soliddb_collations != NULL) {
                int i;
                su_collation_t* collation;

                su_pa_do(soliddb_collations, i) {
                    collation = (su_collation_t*)su_pa_getdata(soliddb_collations, i);
                    ss_dassert(collation != NULL);
                    SsMemFree(collation);
                }

                su_pa_done(soliddb_collations);
                soliddb_collations = NULL;
            }

            su_collation_done();

            /* it seems all deinitialization should not appear
             * after following lines as the solid's heap is destroyed
             * in these lines. To this point all solid's dynamically
             * allocated objects must be freed.
             */
            tb_srv_done();
            ss_debug(SsMemChkPrintList());

#ifdef SS_DEBUG_CHECKMEMLEAKS
            ss_dassert(!memchk_nptr);
#endif

            tb_globaldone();
            solid_started = 0;
        }

        return (0);
}

/*#***********************************************************************\
 *
 *              solid_handler_init
 *
 * Initialize solidDB server
 *
 * Parameters : -
 *
 * Return value : 0 if success
 *                1 on error
 *
 * Globals used :
 */
static bool solid_handler_init(void)
{
        ss_pprintf_1(("solid_handler_init\n"));

#if MYSQL_VERSION_ID < 50100
        if (have_soliddb != SHOW_OPTION_YES) {
            return(1); /* solidDB disabled */
        }
#endif

        solid_startedif();

        if (solid_started) {
            return(0);
        }

        return(1);
}

/*#***********************************************************************\
 *
 *              solid_link_connection
 *
 * Link to connection
 *
 * Parameters :
 *
 *     SOLID_CONN* con, in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_link_connection(SOLID_CONN* con)
{
        CHK_CONN(con);
        ss_dassert(con->sc_nlink >= 1);
        SsSemEnter(solid_mutex);
        con->sc_nlink++;
        SsSemExit(solid_mutex);
}

/*#***********************************************************************\
*              free_constr_list
*
* Free constraint list
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static void free_constr_list(rs_sysi_t* cd, su_list_t* list)
{
        su_list_node_t* listnode;
        rs_cons_t* cons;

        ss_dprintf_3(("free_constr_list\n"));

        su_list_do(list, listnode) {
            cons = (rs_cons_t*)su_listnode_getdata(listnode);
            rs_cons_done(cd, cons);
        }

        su_list_done(list);
}

/*#***********************************************************************\
*              free_end_of_constr_list
*
*
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static void free_end_of_constr_list(rs_sysi_t* cd, su_list_t* list, su_list_node_t* last_node)
{
        su_list_node_t* listnode;
        rs_cons_t* cons;

        ss_dprintf_3(("free_end_of_constr_list\n"));
        ss_dassert(last_node != NULL);

        for (;;) {
            listnode = su_list_last(list);
            ss_assert(listnode != NULL);
            cons = (rs_cons_t*)su_listnode_getdata(listnode);
            rs_cons_done(cd, cons);
            su_list_remove(list, listnode);

            if (listnode == last_node) {
                break;
            }
        }
}

/*#***********************************************************************\
*              frm_casc_states
*
* Free foreign key action states
*
* Parameters :
*
* Return value :
*
* Globals used : -
*
*/
static void free_casc_states(rs_sysi_t* cd, su_pa_t*& list)
{
        if (list == NULL) {
            return;
        }

        uint i;

        su_pa_do(list, i) {
            if (su_pa_indexinuse(list, i)) {
                tb_trans_keyaction_state_t *state =
                    (tb_trans_keyaction_state_t*)su_pa_getdata(list, i);
                tb_ref_keyaction_free(cd, &state);
            }
        }

        su_pa_done(list);
        list = NULL;
}

/*#***********************************************************************\
 *
 *              solid_relcur_free_cursor
 *
 * Free solidDB relation cursor
 *
 * Parameters :
 *
 *     rs_sysi_t*      cd, in out, use
 *     solid_relcur_t* solid_relcur, in out, use
 *     SOLID_CONN*     con, in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_relcur_free_cursor(
        rs_sysi_t* cd,
        solid_relcur_t* solid_relcur,
        SOLID_CONN* con,
        bool enterp)
{
        ss_dassert(con != NULL);
        CHK_CONN(con);

        SS_PUSHNAME("solid_relcur_free_cursor");

        if (enterp) {
            SsSemEnter(con->sc_mutex);
        }

        CHK_CONN(con);

        if (solid_relcur->sr_relcurlist != NULL) {
            su_list_remove(solid_relcur->sr_relcurlist, solid_relcur->sr_relcurlistnode);
            solid_relcur->sr_relcurlist = NULL;
            solid_relcur->sr_relcurlistnode = NULL;
        } else {
            ss_dassert(solid_relcur->sr_relcurlistnode == NULL);
        }

        free_casc_states(cd, solid_relcur->sr_casc_states);

        if (solid_relcur->sr_fetchtval != NULL) {
            ss_debug(rs_tval_resetexternalflatva(cd, solid_relcur->sr_ttype, solid_relcur->sr_fetchtval));

            if (!solid_relcur->sr_mainmem) {
                rs_tval_free(cd, solid_relcur->sr_ttype, solid_relcur->sr_fetchtval);
            }

            solid_relcur->sr_fetchtval = NULL;
        }
        if (solid_relcur->sr_bkeybuf != NULL) {
            dbe_bkey_done_ex(cd, solid_relcur->sr_bkeybuf);
            solid_relcur->sr_bkeybuf = NULL;
        }

        if (solid_relcur->sr_relcur != NULL) {
            ss_pprintf_2(("solid_relcur_free_cursor:close cursor\n"));

            if (solid_relcur->sr_constraints != NULL) {
                free_constr_list(con->sc_cd, solid_relcur->sr_constraints);
                solid_relcur->sr_constraints = NULL;
            }

            if (solid_relcur->sr_constval != NULL) {
                rs_tval_free(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_constval);
                solid_relcur->sr_constval = NULL;
            }

            if (solid_relcur->sr_postval != NULL) {
                rs_tval_free(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_postval);
                solid_relcur->sr_postval = NULL;
            }

            if (solid_relcur->sr_vbuf != NULL) {
                rs_vbuf_done(con->sc_cd, solid_relcur->sr_vbuf);
                solid_relcur->sr_vbuf = NULL;
            }

            SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_CLOSE);
            dbe_cursor_done(solid_relcur->sr_relcur, NULL);
            solid_relcur->sr_relcur = NULL;
        }

        if (solid_relcur->sr_pla != NULL) {
            rs_pla_done(cd, solid_relcur->sr_pla);
            solid_relcur->sr_pla = NULL;
            solid_relcur->sr_plakey = NULL;
        }

        if (enterp) {
            SsSemExit(con->sc_mutex);
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_relcur_free
 *
 * Free solidDB relation
 *
 * Parameters :
 *
 *     rs_sysi_t*      cd, in out, use
 *     solid_relcur_t* solid_relcur, in out, use
 *     SOLID_CONN*     con, in out, use
 *
 * Return value: -
 *
 * Globals used :
 */
static void solid_relcur_free(
        rs_sysi_t* cd,
        solid_relcur_t* solid_relcur,
        SOLID_CONN* con,
        bool add_to_donelist,
        bool enterp)
{
        ss_pprintf_1(("solid_relcur_free:bool add_to_donelist=%d\n", add_to_donelist));

        SS_PUSHNAME("solid_relcur_free");

        if (solid_relcur != NULL) {
            ss_pprintf_2(("solid_relcur_free:free cursor %ld\n", (long)solid_relcur));
            CHK_RELCUR(solid_relcur);

            if (add_to_donelist) {
                int donelistid;

                ss_dassert(enterp);
                SsSemEnter(con->sc_mutex);
                donelistid = rs_relh_relid(cd, solid_relcur->sr_rsrelh) % MAX_RELCURDONELIST;
                if (con->sc_relcurdonelist[donelistid] == NULL) {
                    con->sc_relcurdonelist[donelistid] = su_list_init(NULL);
                    su_list_startrecycle(con->sc_relcurdonelist[donelistid]);
                }
                su_list_insertfirst(con->sc_relcurdonelist[donelistid], solid_relcur);
                SsSemExit(con->sc_mutex);
            } else {
                solid_relcur_free_cursor(cd, solid_relcur, con, enterp);
                solid_relcur->sr_chk = 0;

                if (enterp) {
                    SsSemEnter(con->sc_mutex);
                }

                rs_relh_done(cd, solid_relcur->sr_rsrelh);
                solid_relcur->sr_rsrelh = NULL;
                SsMemFree(solid_relcur->sr_selectlist);
                solid_relcur->sr_selectlist = NULL;
                SsMemFree(solid_relcur->sr_usedfields);
                solid_relcur->sr_usedfields = NULL;
                ss_dassert(solid_relcur->sr_pla == NULL);
                SsMemFree(solid_relcur);

                if (enterp) {
                    SsSemExit(con->sc_mutex);
                }
            }
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *      solid_relcur_clearrelcurdonelist
 *
 * Clears all relation cursors that are added to donelist.
 *
 * Parameters :
 *
 *      con -
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
static void solid_relcur_clearrelcurdonelist(SOLID_CONN* con, int maxlen, bool enterp)
{
        solid_relcur_t* solid_relcur;
        int i;

        ss_pprintf_3(("solid_relcur_clearrelcurdonelist\n"));
        SS_PUSHNAME("solid_relcur_clearrelcurdonelist");

        if (!con->sc_isvalid) {
            ss_dassert(con->sc_tbcon == NULL);
            SS_POPNAME;
            return;
        }

        for (i = 0; i < MAX_RELCURDONELIST; i++) {
            if (con->sc_relcurdonelist[i] != NULL) {
                while (su_list_length(con->sc_relcurdonelist[i]) > (uint)maxlen) {
                    if (enterp) {
                        SsSemEnter(con->sc_mutex);
                    }

                    solid_relcur = (solid_relcur_t*)su_list_removelast(con->sc_relcurdonelist[i]);

                    if (enterp) {
                        SsSemExit(con->sc_mutex);
                    }

                    if (solid_relcur != NULL) {
                        solid_relcur_free(con->sc_cd, solid_relcur, con, FALSE, enterp);
                    } else {
                        break;
                    }
                }
            }
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_free_connection_nomutex
 *
 * Free solid connection
 *
 * Parameters :
 *
 *     SOLID_CONN* con, in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_free_connection_nomutex(SOLID_CONN* con)
{
        solid_relcur_t* solid_relcur;

        ss_pprintf_1(("solid_free_connection_nomutex:con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
        CHK_CONN(con);
        SS_PUSHNAME("solid_free_connecton_nomutex");

        if (con->sc_relcurlist != NULL) {
            while ((solid_relcur = (solid_relcur_t*)su_list_removefirst(con->sc_relcurlist)) != NULL) {
                ss_pprintf_2(("solid_free_connection_nomutex:close cursor, con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
                CHK_RELCUR(solid_relcur);
                ss_dassert(solid_relcur->sr_relcurlist != NULL);
                solid_relcur->sr_relcurlist = NULL;
                solid_relcur->sr_relcurlistnode = NULL;

                SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_CLOSE);

                free_casc_states(con->sc_cd, solid_relcur->sr_casc_states);

                if (solid_relcur->sr_fetchtval != NULL) {
                    ss_debug(rs_tval_resetexternalflatva(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_fetchtval));

                    if (!solid_relcur->sr_mainmem) {
                        rs_tval_free(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_fetchtval);
                    }
                    solid_relcur->sr_fetchtval = NULL;
                }

                if (solid_relcur->sr_bkeybuf != NULL) {
                    dbe_bkey_done_ex(con->sc_cd, solid_relcur->sr_bkeybuf);
                    solid_relcur->sr_bkeybuf = NULL;
                }

                if (solid_relcur->sr_relcur != NULL) {
                    if (solid_relcur->sr_constraints != NULL) {
                        free_constr_list(con->sc_cd, solid_relcur->sr_constraints);
                        solid_relcur->sr_constraints = NULL;
                    }

                    if (solid_relcur->sr_constval != NULL) {
                        rs_tval_free(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_constval);
                        solid_relcur->sr_constval = NULL;
                    }

                    if (solid_relcur->sr_postval != NULL) {
                        rs_tval_free(con->sc_cd, solid_relcur->sr_ttype, solid_relcur->sr_postval);
                        solid_relcur->sr_postval = NULL;
                    }

                    if (solid_relcur->sr_vbuf != NULL) {
                        rs_vbuf_done(con->sc_cd, solid_relcur->sr_vbuf);
                        solid_relcur->sr_vbuf = NULL;
                    }

                    dbe_cursor_done(solid_relcur->sr_relcur, NULL);
                    solid_relcur->sr_relcur = NULL;
                    rs_pla_done(con->sc_cd, solid_relcur->sr_pla);
                    solid_relcur->sr_pla = NULL;
                    solid_relcur->sr_plakey = NULL;
                }
            }
            su_list_done(con->sc_relcurlist);
            con->sc_relcurlist = NULL;
        }

        if (con->sc_tbcon != NULL) {
            if (con->sc_tval != NULL) {
                rs_tval_free(con->sc_cd, con->sc_ttype, con->sc_tval);
                rs_ttype_free(con->sc_cd, con->sc_ttype);
                con->sc_tval = NULL;
            }
            ss_pprintf_1(("solid_free_connection_nomutex:donecd:con=%ld, cd=%ld\n", (long)con, (long)con->sc_cd));
            solid_relcur_clearrelcurdonelist(con, 0, FALSE);
            tb_srv_donecd(con->sc_cd);
            tb_srv_disconnect(con->sc_tbcon, (void*)con, con->sc_userid);
            con->sc_tbcon = NULL;
        }

        ss_dassert(con->sc_tval == NULL);

        if (con->sc_errh != NULL) {
            su_err_done(con->sc_errh);
            con->sc_errh = NULL;
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_unlink_connection
 *
 * Unlink from connection
 *
 * Parameters :
 *
 *     SOLID_CONN* con, in out, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_unlink_connection(SOLID_CONN* con, bool freeif)
{
        ss_pprintf_1(("solid_unlink_connection:con=%ld, trans=%ld, link %d\n", (long)con, (long)con->sc_trans, con->sc_nlink));
        CHK_CONN(con);
        SS_PUSHNAME("solid_unlink_connection");

        SsSemEnter(solid_mutex);
        ss_dassert(con->sc_nlink > 0);

        if (freeif) {
            con->sc_nlink--;
            ss_pprintf_1(("solid_unlink_connection:link %d\n", con->sc_nlink));
        }

        if (con->sc_nlink == 0) {
            int i;

            ss_pprintf_1(("solid_unlink_connection:physically free connection, con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
            CHK_CONN(con);
            solid_relcur_clearrelcurdonelist(con, 0, TRUE);

            for (i = 0; i < MAX_RELCURDONELIST; i++) {
                if (con->sc_relcurdonelist[i] != NULL) {
                    su_list_done(con->sc_relcurdonelist[i]);
                    con->sc_relcurdonelist[i] = NULL;
                }
            }

            SsSemEnter(con->sc_mutex);
            solid_free_connection_nomutex(con);
            ss_dassert(con->con_n_tables == 0);

            con->sc_errkey = NULL;

            SsSemExit(con->sc_mutex);
            SsSemFree(con->sc_mutex);

            if (freeif) {
                con->sc_chk = 0;
                SsMemFree(con);
            }
        }

        SsSemExit(solid_mutex);
        SS_POPNAME;
}

/*#***********************************************************************\
*              solid_table_create
*
* Create solid_table structure
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static solid_table_t* solid_table_create(rs_sysi_t* cd, rs_relh_t* rsrelh)
{
        solid_table_t* solid_table;

        ss_dprintf_3(("solid_table_create\n"));
        SS_PUSHNAME("solid_table_create");

        solid_table = SSMEM_NEW(solid_table_t);

        solid_table->st_rsrelh = rsrelh;
        solid_table->st_ttype = rs_relh_ttype(cd, rsrelh);
        solid_table->st_tval = rs_tval_create(cd, solid_table->st_ttype);
        solid_table->st_relopsize1 = 0;
        solid_table->st_relopsize2 = 0;
        SS_POPNAME;

        return(solid_table);
}

/*#***********************************************************************\
*              solid_table_free
*
* Free solid_table structure
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static void solid_table_free(rs_sysi_t* cd, solid_table_t* solid_table)
{
        ss_dprintf_3(("solid_table_free\n"));
        SS_PUSHNAME("solid_table_free");

        if (solid_table != NULL) {
            if (solid_table->st_relopsize1 > 0) {
                SsMemFree(solid_table->st_relops1);
                SsMemFree(solid_table->st_anos1);
                rs_tval_free(cd, solid_table->st_ttype, solid_table->st_limtval1);
            }

            if (solid_table->st_relopsize2 > 0) {
                SsMemFree(solid_table->st_relops2);
                SsMemFree(solid_table->st_anos2);
                rs_tval_free(cd, solid_table->st_ttype, solid_table->st_limtval2);
            }

            rs_tval_free(cd, solid_table->st_ttype, solid_table->st_tval);
            SS_MEM_SETUNLINK(solid_table->st_rsrelh);
            rs_relh_done(cd, solid_table->st_rsrelh);
            SsMemFree(solid_table);
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_clear_connection
 *
 * Clear connection
 *
 * Parameters :
 *
 *     SOLID_CONN*     con, in, use
 *     rs_relh_t*      rsrelh, in, use
 *     solid_relcur_t* solid_relcur, in, use
 *
 * Return value : 0
 *
 * Globals used :
 */
static void solid_clear_connection(
        SOLID_CONN* con,
        solid_table_t* solid_table,
        solid_relcur_t* solid_relcur)
{
        ss_pprintf_1(("solid_clear_connection:con->sc_isvalid=%d\n", con->sc_isvalid));
        SS_PUSHNAME("solid_clear_connection");
        CHK_CONN(con);

        if (con->sc_isvalid) {
            solid_relcur_free(con->sc_cd, solid_relcur, con, TRUE, TRUE);
            solid_table_free(con->sc_cd, solid_table);
        }

        solid_unlink_connection(con, TRUE);
        SS_POPNAME;
}

/*#***********************************************************************\
*              solidcon_throwout
*
* Trow out solid connection
*
* Parameters :
*
*
* Return value :
*
*
* Globals used : -
*
*/
static void solidcon_throwout(void* ctx)
{
        SOLID_CONN* con = (SOLID_CONN*)ctx;

        SS_PUSHNAME("solidcon_throwout");

        if (con != NULL) {
            CHK_CONN(con);

            ss_pprintf_1(("solidcon_throwout:tb_disconnect, con=%ld, trans=%ld, link %d\n", (long)con, (long)con->sc_trans, con->sc_nlink));
            SsSemEnter(con->sc_mutex);
            CHK_CONN(con);
            SS_PMON_ADD(SS_PMON_MYSQL_ROLLBACK);
            tb_trans_rollback_onestep(con->sc_cd, con->sc_trans, FALSE, NULL);
            solid_free_connection_nomutex(con);
            SsSemExit(con->sc_mutex);
            solid_unlink_connection(con, FALSE);
            con->sc_isvalid = FALSE;
            ss_pprintf_1(("solidcon_throwout:con->sc_isvalid=%d\n", con->sc_isvalid));

            //SsThrUnregister();
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              get_solid_ha_data_connection
 *
 * Gets a connection from thd if exists or generates a new connection.
 *
 * Parameters :
 *
 *     THD* thd, in, use
 *
 * Return value :
 *
 *     SOLID_CONN* con if connection established or NULL
 *
 * Globals used :
*/
SOLID_CONN* get_solid_ha_data_connection(
        handlerton *hton,
        MYSQL_THD thd)
{
        SOLID_CONN* con;
#if MYSQL_VERSION_ID < 50100
        uint solid_ha_data_slot = hton->slot;
#endif

        SS_PUSHNAME("get_solid_ha_data_connection");

        ss_pprintf_1(("get_solid_ha_data_connection\n"));

        if (thd == NULL) {
            ss_pprintf_2(("get_solid_ha_data_connection:thd == NULL\n"));
            SS_POPNAME;
            return(NULL);
        }

#if MYSQL_VERSION_ID >= 50100
        con = (SOLID_CONN*) thd_get_ha_data(thd, hton);
#else
        con = (SOLID_CONN*)thd->ha_data[solid_ha_data_slot];
#endif

        if (con != NULL) {
            ss_pprintf_2(("get_solid_ha_data_connection:slot:con=%ld, isvalid=%d\n", con, con->sc_isvalid));
            if (!con->sc_isvalid) {
                //SsMemFree(con);
                con = NULL; /* */
            }
        }

        if (con == NULL) {
            /* No connection yet in ha_data, generate a new connection.
             */

            SsThrRegister();

            ss_pprintf_1(("get_solid_ha_data_connection:tb_connect_local\n"));
            con = (SOLID_CONN*)SsMemCalloc(1, sizeof(struct st_solid_connection));

            con->con_n_tables = 0;
            con->sc_isvalid = TRUE;
            ss_pprintf_1(("get_solid_ha_data_connection (new):con->sc_isvalid=%d\n", con->sc_isvalid));
            con->sc_nlink = 1;
            con->sc_errh = NULL;
            con->sc_chk = CHKVAL_CONN;
            con->sc_err_tableid = 0;
            con->sc_errkey = NULL;
            con->sc_seq_id = 0;

            con->sc_tbcon = tb_srv_connect_local((void*)con, -1, (char *)"mysql", (char *)"mysql", &con->sc_userid);
            ss_dassert(con->sc_tbcon != NULL);

            con->sc_cd = tb_getclientdata(con->sc_tbcon);
            ss_dassert(con->sc_cd != NULL);
            ss_dassert(rs_sysi_getconnecttype(con->sc_cd) == RS_SYSI_CONNECT_USER);

            ss_pprintf_1(("get_solid_ha_data_connection:initcd:con=%ld, cd=%ld\n", (long)con, (long)con->sc_cd));

            tb_srv_initcd(con->sc_cd);

            rs_sysi_setflag(con->sc_cd, RS_SYSI_FLAG_MYSQL);
            rs_sysi_setallowduplicatedelete(con->sc_cd, TRUE);

            con->sc_trans = tb_getsqltrans(con->sc_tbcon);
            ss_dassert(con->sc_trans != NULL);

            con->sc_relcurlist = su_list_init(NULL);
            su_list_startrecycle(con->sc_relcurlist);

            con->sc_mutex = SsSemCreateLocal(SS_SEMNUM_SSE_CONNECT);

            con->sc_tval = NULL;

#if MYSQL_VERSION_ID >= 50100
            thd_set_ha_data(thd, hton, (void *)con);
#else
            thd->ha_data[solid_ha_data_slot] = con;
#endif

            SS_PMON_ADD(SS_PMON_MYSQL_CONNECT);
        }

        CHK_CONN(con);
        ss_pprintf_1(("get_solid_ha_data_connection:con=%ld, con->sc_isvalid=%d\n", con, con->sc_isvalid));
        SS_POPNAME;

        return(con);
}

/*#***********************************************************************\
 *
 *              ::get_solid_connection
 *
 * Get connection to solidDB
 *
 * Parameters :
 *
 *     THD*        thd, in, use
 *     const char* table_name, in, use
 *
 * Return value : 0
 *
 * Globals used :
 */
SOLID_CONN* ha_soliddb::get_solid_connection(
        MYSQL_THD thd,
        const char* table_name)
{
        SOLID_CONN* con;
        SOLID_CONN* old_con = NULL;
        solid_table_t* old_solid_table = NULL;
        solid_relcur_t* old_relcur = NULL;
        bool autocommit = FALSE;

        ss_pprintf_1(("ha_solid::get_solid_connection\n"));
        SS_PUSHNAME("ha_soliddb::get_solid_connection");

        if (thd == NULL) {
            ss_pprintf_1(("ha_solid::get_solid_connection:thd == NULL\n"));
            ss_dassert(solid_conn == NULL);
            SS_POPNAME;
            return(NULL);
        }

#if MYSQL_VERSION_ID >= 50100
        con = (SOLID_CONN*) thd_get_ha_data(thd, this->ht);
#else
        con = (SOLID_CONN*)thd->ha_data[this->ht->slot];
#endif

        if (con != solid_conn && solid_conn != NULL) {
            ss_pprintf_1(("ha_solid::get_solid_connection:con != solid_conn:con=%ld, solid_conn=%ld\n", con, solid_conn));
            ss_dassert(solid_conn != NULL);
            CHK_CONN(solid_conn);
            solid_clear_connection(solid_conn, solid_table, solid_relcur);
            solid_conn = NULL;
            solid_table = NULL;
            solid_relcur = NULL;
        }

        con = solid_conn;

        if (con != NULL && !con->sc_isvalid) {
            /* We have a connection in handler that is no
             * longer valid.
             */
            ss_pprintf_1(("ha_solid::get_solid_connection:invalid connection in handler\n"));
            CHK_CONN(solid_conn);
            old_con = solid_conn;
            old_solid_table = solid_table;
            old_relcur = solid_relcur;

            solid_conn = NULL;
            solid_table = NULL;
            solid_relcur = NULL;
            con = NULL;
        }

        if (con == NULL) {
            /* We do not have a valid connection in handler.
             */
            ss_pprintf_1(("ha_solid::get_solid_connection:no valid connection in handler\n"));

            con = get_solid_ha_data_connection((handlerton *)this->ht, thd);
            CHK_CONN(con);

            /* Link connection to this handler. */
            solid_link_connection(con);
            solid_conn = con;
        }

        if (old_con != NULL) {
            /* Clear old connection.
             */
            ss_pprintf_1(("ha_solid::get_solid_connection:clear old connection\n"));
            solid_clear_connection(old_con, old_solid_table, old_relcur);
        }

        if (solid_table != NULL && rs_relh_isaborted(NULL, solid_table->st_rsrelh)) {
            solid_table_free(NULL, solid_table);
            solid_table = NULL;
        }

        if (solid_table == NULL && table_name != NULL) {

            /* Get a new table handle. */
            rs_relh_t* new_rsrelh;
            rs_entname_t en;
            int sysrelid;

            solid_relname(&en, table_name);

            ss_pprintf_1(("ha_solid::get_solid_connection:get new table handle: %s->%s.%s\n",
                          table_name, en.en_schema, en.en_name));

#if MYSQL_VERSION_ID >= 50100
            autocommit = !(thd_test_options(thd, OPTION_NOT_AUTOCOMMIT));
#else
            autocommit = !(thd->options & OPTION_NOT_AUTOCOMMIT);
#endif

            ss_pprintf_2(("ha_solid::get_solid_connection:con->con_n_tables=%d, tb_trans_isactive=%d, tb_trans_iswrites=%d, autocommit=%s\n",
                          (int)con->con_n_tables,
                          tb_trans_isactive(con->sc_cd, con->sc_trans),
                          tb_trans_iswrites(con->sc_cd, con->sc_trans),
                          autocommit ? "ON" : "OFF"));

            if (con->con_n_tables == 0 &&
                autocommit &&
                tb_trans_isactive(con->sc_cd, con->sc_trans) &&
                !tb_trans_iswrites(con->sc_cd, con->sc_trans)) {
                /* Fix for SHOW commands, they do not call external_lock so we cannot
                 * do autocommit in the normal way. If we do not commit here we might be
                 * reading old data.
                 */
                ss_pprintf_2(("ha_solid::get_solid_connection:no open tables, no writes, autommit -> restart trans\n"));
                tb_trans_rollback_onestep(con->sc_cd, con->sc_trans, FALSE, NULL);
                tb_trans_beginif(con->sc_cd, con->sc_trans);
            }

            /* TODO : add schema processing to check_solid_sysrel() */
            if ((sysrelid = check_solid_sysrel(en.en_name)) != -1) { 
                rs_entname_done_buf(&en);
                rs_entname_initbuf(&en, /*catalog*/ NULL,
                                   RS_AVAL_SYSNAME,
                                   solid_sysrel_map[sysrelid].srm_solidname);
           }

            /* TODO: - how quoted identifiers (case sensitive) are handled? */

            new_rsrelh = tb_dd_getrelh(
                            con->sc_cd,
                            con->sc_trans,
                            &en,
                            NULL,
                            NULL);      // rs_err_t** p_errh ? TODO: add errh processing ?

            if (new_rsrelh != NULL) {
                solid_table = solid_table_create(con->sc_cd, new_rsrelh);
                mainmemory = (rs_relh_reltype(con->sc_cd, solid_table->st_rsrelh) == RS_RELTYPE_MAINMEMORY);
            } else {
                ss_dassert(solid_table == NULL);
            }

            if (sysrelid == -1) {
                rs_entname_done_buf(&en);
            }
        }

        CHK_CONN(con);
        SS_POPNAME;

        return(con);
}

/*#***********************************************************************\
 *
 *              solid_close_connection
 *
 * Frees a possible connection object associated with the current THD
 *
 * Parameters :
 *
 *      THD* thd, in, use
 *
 * Return value :
 *
 *      0 or error number
 *
 * Globals used :
 */
static int solid_close_connection(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD   thd)
{
        SOLID_CONN* con;

#if MYSQL_VERSION_ID >= 50100
        con = (SOLID_CONN*) thd_get_ha_data(thd, hton);
#else
        con = (SOLID_CONN*)thd->ha_data[solid_hton.slot];
#endif

        if (con != NULL) {
            CHK_CONN(con);

#if MYSQL_VERSION_ID >= 50100
            thd_set_ha_data(thd, hton, (void *) NULL);
#else
            thd->ha_data[solid_hton.slot] = NULL;
#endif

            ss_pprintf_1(("solid_close_connection:tb_disconnect, con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
            SsSemEnter(con->sc_mutex);
            CHK_CONN(con);
            ss_pprintf_1(("solid_close_connection:con->sc_isvalid=%d\n", con->sc_isvalid));

            if (con->sc_isvalid) {
                SS_PMON_ADD(SS_PMON_MYSQL_ROLLBACK);
                tb_trans_rollback_onestep(con->sc_cd, con->sc_trans, FALSE, NULL);
            }

            solid_free_connection_nomutex(con);
            con->sc_isvalid = FALSE;
            SsSemExit(con->sc_mutex);
            solid_unlink_connection(con, TRUE);

            SsThrUnregister();
        }

        return(0);
}

/*#***********************************************************************\
 *
 *              ::get_auto_increment
 *
 * Get sequence value associated to auto increment field
 *
 * Parameters : -
 *
 * Return value :
 *
 *      auto increment value
 *
 * Globals used : current_thd
 */
ulonglong ha_soliddb::get_auto_increment()
{
        ulonglong nr;
        ss_int8_t i8;
        long seq_id=0;
        rs_auth_t* auth;
        rs_atype_t* atype;
        rs_aval_t*  aval;
        bool succp;
        solid_bool finished;
        SOLID_CONN* con;
        bool lock_seq_id;
        rs_relh_t* s_relh;

        DBUG_ENTER("ha_soliddb::get_auto_increment()");
        SS_PUSHNAME("ha_soliddb::get_auto_increment()");
        ss_pprintf_1(("ha_soliddb::get_auto_increment()\n"));

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(current_thd, table->s->path.str);
#else
        con = get_solid_connection(current_thd, table->s->path);
#endif

        s_relh = solid_table->st_rsrelh;
        ss_dassert(s_relh != NULL);
        auth = rs_sysi_auth(con->sc_cd);
        ss_dassert(auth != NULL);
        rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

        seq_id = rs_relh_readautoincrement_seqid(con->sc_cd, s_relh);

        if (seq_id == 0) {
            char *table_name = (char *)SsMemAlloc(strlen(rs_relh_name(con->sc_cd, s_relh))+1);
            keyname_from_solid_to_mysql(rs_relh_name(con->sc_cd, s_relh), table_name);
            sql_print_error("SolidDB error: auto increment sequence not found from table %s\n",
                            table_name);
            SsMemFree(table_name);
            SS_POPNAME;
            DBUG_RETURN(~(ulonglong)0);
        }

#if MYSQL_VERSION_ID >= 50100
        lock_seq_id = rs_relh_reltype(con->sc_cd, s_relh) != RS_RELTYPE_OPTIMISTIC
                      && thd_test_options(current_thd, OPTION_BIN_LOG);
#else
        lock_seq_id = rs_relh_reltype(con->sc_cd, s_relh) != RS_RELTYPE_OPTIMISTIC
                      && mysql_bin_log.is_open();
#endif

        /* For pessimistic table we should take a lock to sequence to
           ensure that replication and roll-forward of the binlog
           to work correctly.
           Sequence is kept locked until end of statement.
         */

        rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

        if (lock_seq_id) {
            su_ret_t suret;

            do {
                suret = tb_seq_lock(con->sc_cd, con->sc_trans, seq_id, NULL);
            } while(rs_sysi_lockwait(con->sc_cd) || suret == DBE_RC_WAITLOCK);
        }

        atype = rs_atype_initbigint(con->sc_cd);
        aval = rs_aval_create(con->sc_cd, atype);

        finished = FALSE;

        do {
            succp = tb_seq_next(con->sc_cd, con->sc_trans, seq_id,
                                FALSE, atype, aval, &finished, NULL);

        } while (rs_sysi_lockwait(con->sc_cd) || !finished);

        i8 = rs_aval_getint8(con->sc_cd, atype, aval);
        nr = SsInt8GetNativeUint8(i8);

        rs_auth_setsystempriv(con->sc_cd, auth, FALSE);

        rs_aval_free(con->sc_cd, atype, aval);
        rs_atype_free(con->sc_cd, atype);

        SS_POPNAME;
        DBUG_RETURN((ulonglong) nr);
}

/*#***********************************************************************\
 *
 *              ::reset_auto_increment
 *
 *  Reset the auto-increment counter to the given value, i.e. the next row
 *   inserted will get the given value. This is called e.g. after TRUNCATE
 *   is emulated by doing a 'DELETE FROM t'.
 *
 * Parameters :
 *
 *      ulonglong value, in, use, value to be reseted
 *
 * Return value :
 *
 *      0 or error number
 *
 * Globals used : current_thd§
 */
int ha_soliddb::reset_auto_increment(ulonglong value)
{
        rs_atype_t* valatype;
        rs_aval_t*  aval;
        ss_int8_t   i8;
        rs_auth_t*  auth;
        SOLID_CONN* con;
        long seq_id;
        bool lock_seq_id;
        rs_relh_t* s_relh;
        solid_bool finished;
        bool succp;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(current_thd, table->s->path.str);
#else
        con = get_solid_connection(current_thd, table->s->path);
#endif

        DBUG_ENTER("ha_soliddb::reset_auto_increment");
        SS_PUSHNAME("ha_soliddb::reset_auto_increment()");
        ss_pprintf(("ha_soliddb::reset_auto_increment()"));

        s_relh = solid_table->st_rsrelh;
        ss_dassert(s_relh != NULL);

        if ((seq_id = rs_relh_readautoincrement_seqid(con->sc_cd, s_relh))==0) {
            SS_POPNAME;
            DBUG_RETURN(0);
        }

        auth = rs_sysi_auth(con->sc_cd);
        ss_dassert(auth != NULL);
        rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

#if MYSQL_VERSION_ID >= 50100
        lock_seq_id = rs_relh_reltype(con->sc_cd, s_relh) != RS_RELTYPE_OPTIMISTIC
                      && thd_test_options(current_thd, OPTION_BIN_LOG);
#else
        lock_seq_id = rs_relh_reltype(con->sc_cd, s_relh) != RS_RELTYPE_OPTIMISTIC
                      && mysql_bin_log.is_open();
#endif

        /* For pessimistic table we should take a lock to sequence to
           ensure that replication and roll-forward of the binlog
           to work correctly.
           Sequence is kept locked until end of statement.
         */

        rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

        if (lock_seq_id) {
            su_ret_t suret;

            do {
                suret = tb_seq_lock(con->sc_cd, con->sc_trans, seq_id, NULL);
            } while(rs_sysi_lockwait(con->sc_cd) || suret == DBE_RC_WAITLOCK);
        }

        valatype = rs_atype_initbigint(con->sc_cd);
        aval = rs_aval_create(con->sc_cd, valatype);
        SsInt8SetNativeUint8(&i8, value);
        rs_aval_setint8_ext(con->sc_cd, valatype, aval, i8, NULL);

        finished = FALSE;

        do {
            succp = tb_seq_set(con->sc_cd, con->sc_trans, seq_id, FALSE, valatype, aval,
                               &finished, NULL);
        } while (succp && (rs_sysi_lockwait(con->sc_cd) || !finished));

        rs_auth_setsystempriv(con->sc_cd, auth, FALSE);

        rs_aval_free(con->sc_cd, valatype, aval);
        rs_atype_free(con->sc_cd, valatype);

        SS_POPNAME;
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ha_solid_mysql_error
 *
 * Map solidDB error code to MySQL error code
 *
 * Parameters :
 *
 *      THD* thd, in, use, MySQL thread structure
 *      su_err_t* errh, in, use, solidDB error structure
 *
 * Return value :
 *
 *      MySQL error code
 *
 * Globals used :
 */
static int ha_solid_mysql_error(
        MYSQL_THD thd,
        su_err_t* errh,
        int rc)
{
        int solrc;
        int myrc;

        ss_pprintf_1(("ha_solid_mysql_error:%s, rc=%d\n", errh != NULL ? su_err_geterrstr(errh) : "NULL", rc));

        if (errh == NULL) {
            if (rc == 0) {
                rc = -1;
            }

            solrc = rc;
        } else {
            solrc = su_err_geterrcode(errh);
        }

        switch (solrc) {
            case E_ATTRNOTEXISTONREL_SS:
            case E_FORKINCOMPATDTYPE_S:
            case E_NULLNOTALLOWED_S:
            case E_CONSTRAINT_NOT_FOUND_S:
            case E_CANNOTDROPFORKEYCOL_S:
            case E_CONSTRCHECKFAIL_S:
            case E_INDEX_IS_USED_S:
            case E_FORKEY_LOOPDEP:
            case E_FORKEY_SELFREF:
            case E_AMBIGUOUS_S:
            case E_PRIMKEY_NOTDEF_S:
            case E_FORKNOTUNQK:
            case E_UNRESFKEYS_S:
                myrc = HA_ERR_CANNOT_ADD_FOREIGN;
                break;
            case E_FORKEYREFEXIST_S:
                myrc = HA_ERR_ROW_IS_REFERENCED;
                break;
            case E_KEYNAMEEXIST_S:
                myrc = HA_ERR_WRONG_COMMAND;
                break;
            case DBE_ERR_UNIQUE_S:
            case DBE_ERR_PRIMUNIQUE_S:
                myrc = HA_ERR_FOUND_DUPP_KEY;
                break;
            case DBE_ERR_NOTFOUND:
            case DBE_ERR_NOACTSEARCH:
            case E_UPDNOCUR:
            case E_DELNOCUR:
                myrc = HA_ERR_NO_ACTIVE_RECORD;
                break;
            case DBE_ERR_LOCKED:
            case DBE_ERR_DDOPACT:
            case DBE_ERR_LOSTUPDATE:
            case DBE_ERR_NOTSERIALIZABLE:
            case DBE_ERR_TRXTIMEOUT:    // Transaction timed out
                myrc = HA_ERR_LOCK_WAIT_TIMEOUT;
                break;
            case DBE_ERR_DEADLOCK:
                /* Roll back the whole transaction */

                ss_debug(sql_print_error("DBE_ERR_DEADLOCK, call solid_rollback, thrid=%d", SsThrGetid()));

#if MYSQL_VERSION_ID >= 50100
                thd_mark_transaction_to_rollback(thd, TRUE);
#else
                solid_rollback(thd, TRUE);
#endif

                myrc = HA_ERR_LOCK_WAIT_TIMEOUT;

#ifdef SS_MYSQL_REAL_DEADLOCK
                myrc = HA_ERR_LOCK_DEADLOCK;
#endif /* SS_MYSQL_REAL_DEADLOCK */
                break;
            case DBE_ERR_PARENTNOTEXIST_S:
                myrc = HA_ERR_NO_REFERENCED_ROW;
                break;
            case DBE_ERR_CHILDEXIST_S:
                myrc = HA_ERR_ROW_IS_REFERENCED;
                break;
            case SU_ERR_FILE_WRITE_DISK_FULL:
            case SU_ERR_FILE_WRITE_CFG_EXCEEDED:
                myrc = HA_ERR_RECORD_FILE_FULL;
                break;
            case DBE_ERR_TOOLONGKEY:
            case E_TOOLONGCONSTR:
                myrc = HA_ERR_TO_BIG_ROW;
                break;
            case DBE_ERR_TRXREADONLY:
            case DBE_ERR_DBREADONLY:
                myrc = HA_ERR_READ_ONLY_TRANSACTION;
                break;
            case E_RELNOTEXIST_S:
                myrc = HA_ERR_NO_SUCH_TABLE;
                break;
            case E_RELEXIST_S:
            case E_DDOP:
                myrc = HA_ERR_TABLE_EXIST;
                break;
            default:
                myrc = -1;
                break;
        }

        ss_pprintf_1(("ha_solid_mysql_error:%d->%d\n", solrc, myrc));

        return(myrc);
}
/*#***********************************************************************\
 *
 *              handle_error
 *
 * Handler errors in solidDB for MySQL
 *
 * Parameters :
 *
 *    su_err_t* errh, in, use, solidDB error handler
 *
 * Return value : MySQL error code
 *
 * Globals used : current_thd
 */
static inline int handle_error(
        su_err_t*& errh)
{
    int rc = 0;
    MYSQL_THD thd = current_thd;

    if (errh) {
        rc = ha_solid_mysql_error(thd, errh, rc);
    }

    if (errh) {
        sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
        su_err_done(errh);
        errh = NULL;
    }

    return rc;
}

/*#***********************************************************************\
 *
 *              ha_solid_map_mysql_solid_isolation
 *
 * Maps a MySQL trx isolation level code to the solidDB (tab-level)
 * isolation level code.
 *
 * NOTE: needed MySQL isolation is in thd->variables.tx_isolation or
 * in thd->session_tx_isolation
 *
 * Parameters :
 *
 *      enum_tx_isolation iso, in, use
 *
 * Return value :
 *
 *      solidDB isolation code
 *
 * Globals used :
 */
static inline int ha_solid_map_mysql_solid_isolation(
        enum_tx_isolation iso)
{
        switch(iso) {
            case ISO_REPEATABLE_READ:
                return(TB_TRANSOPT_ISOLATION_REPEATABLEREAD);
            case ISO_READ_COMMITTED:
                return(TB_TRANSOPT_ISOLATION_READCOMMITTED);
            case ISO_SERIALIZABLE:
                return(TB_TRANSOPT_ISOLATION_SERIALIZABLE);
            case ISO_READ_UNCOMMITTED:
                return(TB_TRANSOPT_ISOLATION_READCOMMITTED);
            default:
                ss_error;
                return(0);
        }
}

#if MYSQL_VERSION_ID < 50100
/*#***********************************************************************\
 *
 *              solid_update_duplicates
 *
 * Check what should be done in case of duplicates
 *
 * Parameters :
 *
 *      THD*  thd, in, use, MySQL thread
 *
 * Return value : -
 *
 * Globals used : -
 */
inline void ha_soliddb::solid_update_duplicates(
        MYSQL_THD thd)
{
        extra_ignore_duplicate = FALSE;
        extra_replace_duplicate = FALSE;
        extra_update_duplicate = FALSE;

        switch(thd->lex->duplicates) {
            case DUP_REPLACE:
                extra_replace_duplicate = TRUE;
                break;
            case DUP_UPDATE:
                extra_update_duplicate = TRUE;
                break;
            default:
                break;
        }

        if (thd->lex->ignore) {
            extra_ignore_duplicate = TRUE;
        }
}
#endif /* MYSQL_VERSION_ID < 50100 */

/*#***********************************************************************\
 *
 *              solid_ignore_duplicate
 *
 * Check whether we should ignore duplicate key error
 *
 * Parameters :
 *
 *      THD*  thd, in, use, MySQL thread
 *
 * Return value :
 *
 *      TRUE, if we should update duplicate key
 *      FALSE, otherwise
 *
 * Globals used :
 */
inline bool ha_soliddb::solid_ignore_duplicate(
        MYSQL_THD thd)
{
        int sql_command = soliddb_sql_command(thd);

#if MYSQL_VERSION_ID < 50100
        solid_update_duplicates(thd);
#endif

        /* REPLACE ... [SELECT] || LOAD REPLACE */
        if (sql_command == SQLCOM_REPLACE ||
            sql_command == SQLCOM_REPLACE_SELECT ||
            (sql_command == SQLCOM_LOAD &&
             extra_replace_duplicate)) {
            return(TRUE);
        }

        /* INSERT ... [SELECT] ... ON DUPLICATE KEY ... */
        if ((sql_command == SQLCOM_INSERT ||
             sql_command == SQLCOM_INSERT_SELECT) &&
             extra_update_duplicate) {
            return(TRUE);
        }

        /* {INSERT|UPDATE} IGNORE [SELECT] */
        if ((sql_command == SQLCOM_INSERT ||
             sql_command == SQLCOM_INSERT_SELECT ||
             sql_command == SQLCOM_UPDATE ||
             sql_command == SQLCOM_UPDATE_MULTI) &&
            extra_ignore_duplicate){
            return(TRUE);
        }

        return(FALSE);
}

/*#***********************************************************************\
 *
 *              solid_ignore_duplicate_update
 *
 * Check whether we should ignore duplicate key error on ::update_row
 *
 * Parameters :
 *
 *      THD*  thd, in, use, MySQL thread
 *
 * Return value :
 *
 *      TRUE, if we should update duplicate key
 *      FALSE, otherwise
 *
 * Globals used :
 */
inline bool ha_soliddb::solid_ignore_duplicate_update(
        MYSQL_THD thd)
{
        int sql_command = soliddb_sql_command(thd);

#if MYSQL_VERSION_ID < 50100
        solid_update_duplicates(thd);
#endif

        /* {INSERT|UPDATE} IGNORE [SELECT] */
        if ((sql_command == SQLCOM_INSERT ||
             sql_command == SQLCOM_INSERT_SELECT ||
             sql_command == SQLCOM_UPDATE ||
             sql_command == SQLCOM_UPDATE_MULTI) &&
            extra_ignore_duplicate  == TRUE &&
            extra_update_duplicate == FALSE){
            return(TRUE);
        }

        return(FALSE);
}

/* The flag trx->active_trans is set to 1 in
 *
 * 1. ::external_lock(),
 * 2. ::start_stmt(),
 * 3. innobase_query_caching_of_table_permitted(),
 * 4. innobase_savepoint(),
 * 5. ::init_table_handle_for_HANDLER(),
 * 6. innobase_start_trx_and_assign_read_view(),
 * 7. ::transactional_table_lock()

 * and it is only set to 0 in a commit or a rollback. If it is 0 we know
 * there cannot be resources to be freed and we could return immediately.
 * For the time being, we play safe and do the cleanup though there should
 * be nothing to clean up.
 */

/*#***********************************************************************\
 *
 *              solid_commit
 *
 *  Commits a transaction in an solid database or marks an SQL statement
 *  ended.
 *
 * Parameters :
 *
 *      THD*  thd, in, use
 *      bool  all, in, use, TRUE - commit transaction
 *                          FALSE - commit statement
 *
 * Return value : 0
 *
 * Globals used :
 */
static int solid_commit(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD    thd,
        bool    all)
{
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   solid_trans;
        bool          succp=TRUE;
        int           rc = 0;
        su_err_t*     errh = NULL;
        bool          autocommit = FALSE;
        solid_bool    finished = FALSE;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)

        ss_win_perf_start;

        DBUG_ENTER("solid_commit");
        DBUG_PRINT("trans", ("ending transaction"));

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection((handlerton *)hton, thd);
        autocommit = !(thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
        autocommit = !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#endif

        ss_pprintf_1(("solid_commit:all=%d AUTOCOMMIT = %s\n", (int)all, autocommit ? "ON" : "OFF" ));

        CHK_CONN(con);

        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        solid_trans = con->sc_trans;

        ss_assert(solid_trans != NULL);

        if (con->sc_seq_id != 0) {
            tb_seq_unlock(cd, solid_trans, con->sc_seq_id);
            con->sc_seq_id = 0;
        }

        if (all || autocommit) {

            if (tb_trans_isstmtactive(cd, solid_trans)) {
                do {
                    succp = tb_trans_stmt_commit(cd, solid_trans, &finished, &errh);
                } while (rs_sysi_lockwait(cd) || !finished);
            }

            if (!succp) {

                /* Here we take error key from dbe transaction to the connection */
                con->sc_errkey = tb_trans_geterrkey(cd, solid_trans);
                tb_trans_rollback_onestep(cd, solid_trans, FALSE, NULL);

            } else {
                /* We were instructed to commit the whole transaction, or
                 * this is an SQL statement end and AUTOCOMMIT is ON.
                 */
                ss_pprintf_2(("solid_commit:commit transaction, con=%ld, trans=%ld\n", (long)con, (long)solid_trans));
                SS_PMON_ADD(SS_PMON_MYSQL_COMMIT);

                tb_trans_setusertrxcleanup(cd, solid_trans, TRUE);

                do {
                    succp = tb_trans_commit(cd, solid_trans, &finished, &errh);
                } while (rs_sysi_lockwait(cd) || !finished);
            }

        } else {
            ss_pprintf_2(("solid_commit:commit statement, con=%ld, trans=%ld\n", (long)con, (long)solid_trans));

            do {
                succp = tb_trans_stmt_commit(cd, solid_trans, &finished, &errh);
            } while (rs_sysi_lockwait(cd) || !finished);

            if (!succp) {

                /* Here we take error key from dbe transaction to the connection */
                con->sc_errkey = tb_trans_geterrkey(cd, solid_trans);
            }
        }

        if (!succp) {

#ifndef MYSQL_DYNAMIC_PLUGIN
            THD_TRANS *trans= all ? &thd->transaction.all : &thd->transaction.stmt;

            /* If more than one handler participate to this transaction
               issue a warning that whole transaction might not been rolled back. */

            if (trans->nht > 1) {
                push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
            }
#endif /* MYSQL_DYNAMIC_PLUGIN */

            rc = ha_solid_mysql_error(thd, errh, rc);

            if (errh) {
                su_err_done(errh);
                errh = NULL;
            }
#ifdef MYSQL_DYNAMIC_PLUGIN
            {
                int error = 0;

                /* Implicit error handling for solidDB for MySQL 5.1 dynamic plugin.
                   Idea here is to push a real error string to MySQL error stack before
                   MySQL pushes a wrong error message there. */

                switch (rc) {
                    case HA_ERR_ROW_IS_REFERENCED:
                    {
                        String str;

                        int key_nr = solid_get_error_message(hton, thd, NULL, rc, &str);

                        my_error(ER_ROW_IS_REFERENCED_2, MYF(0), str.c_ptr_safe());
                        error = 1;
                        break;
                    }
                    case HA_ERR_NO_REFERENCED_ROW:
                    {
                        String str;

                        int key_nr = solid_get_error_message(hton, thd, NULL, rc, &str);
                        my_error(ER_NO_REFERENCED_ROW_2, MYF(0), str.c_ptr_safe());
                        error = 1;
                        break;
                    }
                    case HA_ERR_WRONG_COMMAND:
                        rc = ER_ILLEGAL_HA;
                        break;
                    case HA_ERR_FOUND_DUPP_KEY:
                    {
                        String str;

                        /* handlerton must return -1 in the case it does not know violated key. */
                        uint key_nr = solid_get_error_message(hton, thd, NULL, rc, &str);

                        if ((int) key_nr >= 0) {
                            /* Write the duplicated key in the error message */
                            if (str.length() > NAME_LEN) {
                                /* maximum length of key value in error message string for ER_DUP_ENTRY */

                                str.length(NAME_LEN-3);
                                str.append(STRING_WITH_LEN("..."));
                            }

                            my_error(ER_DUP_ENTRY, MYF(0), str.c_ptr(), key_nr+1);
                        } else {
                            my_error(ER_DUP_KEY, MYF(0), "<UNKNOWN TABLE>");
                        }

                        error = 1;
                        break;
                    }
                    case HA_ERR_LOCK_WAIT_TIMEOUT:
                        rc = ER_LOCK_WAIT_TIMEOUT;
                        break;
                    case HA_ERR_LOCK_DEADLOCK:
                        rc = ER_LOCK_DEADLOCK;
                        break;
                    case HA_ERR_READ_ONLY_TRANSACTION:
                        rc = ER_READ_ONLY_TRANSACTION;
                        break;
                    default:
                        rc = ER_ERROR_DURING_COMMIT;
                        break;
                }

                if (!error) {
                    my_error(rc, MYF(0), rc);
                }
            }
#endif
        } else {
            /* No errors found thus cleanup table reference */
            ss_dprintf_1(("set err table to null\n"));
            con->sc_err_tableid = 0;
        }

        if (solid_trans && tb_trans_isusertrxcleanup(cd, solid_trans)) {
            tb_trans_setusertrxcleanup(cd, solid_trans, FALSE);

            if(tb_trans_dbtrx(cd, solid_trans)) {
                tb_trans_commit_cleanup(cd, solid_trans, tb_trans_getdberet(cd, solid_trans), &errh);

                if (errh) {
                    su_err_done(errh);
                }
            }
        }

        ss_win_perf_stop(commit_perfcount, commit_callcount);

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              internal_solid_end_trans_and_stmt
 *
 *
 *
 * Parameters :
 *
 *      rs_sysi_t*   cd,
 *      tb_trans_t*  trans,
 *      bool         commitp,
 *      su_err_t**   p_errh
 *
 * Return value :
 *
 *      TRUE
 *      FALSE
 *
 * Globals used :
 */
static bool internal_solid_end_trans_and_stmt(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        bool commitp,
        su_err_t** p_errh)
{
        bool succp = true;
        solid_bool finished;

        ss_pprintf_1(("ha_solid::internal_solid_end_trans_and_stmt:%s, trans=%ld\n",
                       commitp ? "commit" : "rollback", (long)trans));

        if (commitp) {
            succp = tb_trans_stmt_commit_onestep(cd, trans, p_errh);

            if (!succp) {
                ss_pprintf_2(("ha_solid::internal_solid_end_trans_and_stmt:stmt commit failed, %s\n",
                               su_err_geterrstr(*p_errh)));
                commitp = FALSE;
            }
        } else {
            tb_trans_stmt_rollback_onestep(cd, trans, NULL);
        }

        if (commitp) {
            SS_PMON_ADD(SS_PMON_MYSQL_COMMIT);

            do {
                succp = tb_trans_commit(cd, trans, &finished, p_errh);
            } while (rs_sysi_lockwait(cd) || !finished);

            if (!succp) {
                ss_pprintf_2(("ha_solid::internal_solid_end_trans_and_stmt:trans commit failed, %s\n",
                               su_err_geterrstr(*p_errh)));
            }
        } else {
            SS_PMON_ADD(SS_PMON_MYSQL_ROLLBACK);
            tb_trans_rollback_onestep(cd, trans, FALSE, NULL);
        }

        return(succp);
}

/*#***********************************************************************\
 *
 *              solid_init
 *
 * Initialize solidDB handlerton
 *
 * Parameters : -
 *
 * Return value : 0
 *
 * Globals used :
 */
#if MYSQL_VERSION_ID >= 50100

int solid_init(
        void *p)
{
        DBUG_ENTER("solid_init");

        handlerton *soliddb_hton = (handlerton *)p;
        legacy_soliddb_hton = soliddb_hton;

        soliddb_hton->state = SHOW_OPTION_YES;
        soliddb_hton->db_type = DB_TYPE_SOLID;
        soliddb_hton->savepoint_offset = 0;
        soliddb_hton->close_connection = solid_close_connection;
        soliddb_hton->savepoint_set = NULL;     /* TODO: Savepoints */
        soliddb_hton->savepoint_rollback = NULL;
        soliddb_hton->savepoint_release = NULL;
        soliddb_hton->commit = solid_commit;
        soliddb_hton->rollback = solid_rollback;
        soliddb_hton->prepare = NULL; /* TODO: X/Open XA */
        soliddb_hton->recover = NULL;
        soliddb_hton->commit_by_xid = NULL;
        soliddb_hton->rollback_by_xid = NULL;
        soliddb_hton->create_cursor_read_view = NULL; /* TODO: Cursors */
        soliddb_hton->set_cursor_read_view = NULL;
        soliddb_hton->close_cursor_read_view = NULL;
        soliddb_hton->create = solid_create_handler;
        soliddb_hton->drop_database = solid_drop_database;
        soliddb_hton->panic = NULL;
        soliddb_hton->start_consistent_snapshot = NULL; /* TODO: Consistent snapshots */
        soliddb_hton->flush_logs = solid_flush_logs;
        soliddb_hton->show_status = soliddb_show_status;
        soliddb_hton->flags = HTON_NO_FLAGS;
        soliddb_hton->release_temporary_latches = NULL; /* TODO: needed ? */
        soliddb_hton->alter_table_flags = solid_alter_table_flags;

/*
 #ifdef HAVE_PSI_INTERFACE */
        /* Register keys with MySQL performance schema */
/*
        if (PSI_server) {
                int     count;

                count = array_elements(all_soliddb_cache_conds);
                PSI_server->register_cond("soliddb",
                                           all_soliddb_cache_conds, count);
	}


 #endif */ /* HAVE_PSI_INTERFACE */


#ifdef MYSQL_DYNAMIC_PLUGIN
        if (soliddb_hton != p) {
                soliddb_hton = reinterpret_cast<handlerton*>(p);
                *soliddb_hton = *legacy_soliddb_hton;
        }
#endif /* MYSQL_DYNAMIC_PLUGIN */

        solid_handler_init();

        DBUG_RETURN(0);

}

/*#***********************************************************************\
 *
 *              solid_deinit
 *
 * Deinit solidDB
 *
 * Parameters : void *
 *
 * Return value : 0
 *
 * Globals used :
 */
int solid_deinit(
        void *p)
{
        int rc = 0;

        rc = solid_end(NULL, (ha_panic_function)0);

        solid_started = 0;

        return rc;
}
#endif /* MYSQL_VERSION_ID >= 50100 */

/*#***********************************************************************\
 *
 *              solid_rollback
 *
 *  Rollback a transaction in an solid database or marks an SQL statement
 *  ended
 *
 * Parameters :
 *
 *      THD*  thd, in, use
 *      bool  all, in, use, TRUE - rollback transaction
 *                          FALSE - rollback statement
 *
 * Return value : 0
 *
 * Globals used :
 */
static int solid_rollback(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        MYSQL_THD   thd,
        bool        all)
{
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        su_err_t*     errh = NULL;
        bool          succp=TRUE;
        int           rc;
        solid_bool    finished = FALSE;
        bool          autocommit = FALSE;

        DBUG_ENTER("solid_rollback");
        DBUG_PRINT("trans", ("ending transaction"));

        ss_pprintf_1(("solid_rollback\n"));

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection((handlerton *)hton, thd);
        autocommit = !(thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
        autocommit = !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#endif

        CHK_CONN(con);

        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

        if (all || autocommit) {
            /* We were instructed to commit the whole transaction, or
               this is an SQL statement end and autocommit is on */
            ss_pprintf_2(("solid_rollback:rollback transaction, con=%ld, trans=%ld\n", (long)con, (long)trans));
            SS_PMON_ADD(SS_PMON_MYSQL_ROLLBACK);
            do {
                succp = tb_trans_rollback(cd, trans, NULL, &finished, TRUE, &errh);
            } while (!finished);
        } else {
            ss_pprintf_2(("solid_rollback:rollback statement, con=%ld, trans=%ld\n", (long)con, (long)trans));
            do {
                succp = tb_trans_stmt_rollback(cd, trans, &finished, &errh);
            } while (!finished);

        }

        ss_dprintf_1(("set err table to null\n"));
        con->sc_err_tableid = 0;

        if (!succp) {
            rc = ha_solid_mysql_error(thd, errh, 0);
            su_err_done(errh);
        } else {
            rc = 0;
        }

        DBUG_RETURN(rc);
}

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
/*#***********************************************************************\
 *
 *              solid_set_foreign_key_checks
 *
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
 static void solid_set_foreign_key_checks(
        MYSQL_THD   thd,
        tb_trans_t* trans)
 {
    bool foreign_key_checks;
 
    ss_dprintf_3(("solid_set_foreign_key_checks\n"));
    
    ss_dassert(thd != NULL);
    ss_dassert(trans != NULL);
    
    SS_PUSHNAME("solid_set_foreign_key_checks");
    
#if MYSQL_VERSION_ID >= 50100
    foreign_key_checks = thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS) ? FALSE : TRUE;
#else
    foreign_key_checks = thd->options & OPTION_NO_FOREIGN_KEY_CHECKS ? FALSE : TRUE;
#endif /* MYSQL_VERSION_ID */

    tb_trans_set_foreign_key_checks(trans, foreign_key_checks);
    
    SS_POPNAME;
 }
 #endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

/*#***********************************************************************\
 *
 *              solid_trans_beginif
 *
 * Start a new transaction or statement if it has not been already started
 *
 * Parameters :
 *
 *      THD*  thd, in, use
 *      bool  start_stmt, in, use, TRUE - start a statement
 *                                 FALSE - start a transaction
 *
 * Return value : -
 *
 * Globals used :
 */
static void solid_trans_beginif(
        handlerton* hton,
        MYSQL_THD   thd,
        bool        start_stmt)
{
        SOLID_CONN* con;
        bool new_transaction_started=FALSE;
        bool autocommit = FALSE;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(hton, thd);
        autocommit = !(thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
#else
        con = get_solid_ha_data_connection(hton, thd);
        autocommit = !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#endif

        ss_pprintf_1(("solid_trans_beginif:autocommit %d, start_stmt = %d.\n", (int)autocommit, (int)start_stmt));

        if (!autocommit) {
            ss_pprintf_1(("solid_trans_beginif:OPTION_NOT_AUTOCOMMIT\n"));
        }

        if (con == NULL) {
            ss_error;
            ss_pprintf_1(("solid_trans_beginif:con == NULL\n"));
            return;
        }

        CHK_CONN(con);

        if (start_stmt) {
            ss_dprintf_2(("solid_trans_beginif:tb_trans_begintransandstmt\n"));
            new_transaction_started = tb_trans_begintransandstmt(con->sc_cd, con->sc_trans);
        } else {
            ss_dprintf_2(("solid_trans_beginif:tb_trans_beginif\n"));
            new_transaction_started = tb_trans_beginif(con->sc_cd, con->sc_trans);
        }
        
        if (new_transaction_started) {
            ss_dassert(con->sc_seq_id == 0);

            if (!autocommit) {
                trans_register_ha(thd, TRUE, (handlerton *)hton);
            }

            trans_register_ha(thd, FALSE, (handlerton *)hton);

            ss_pprintf_1(("solid_trans_beginif:new transaction registered, con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
        } else {
            ss_pprintf_1(("solid_trans_beginif:old transaction, NOT registered, con=%ld, trans=%ld\n", (long)con, (long)con->sc_trans));
        }

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        solid_set_foreign_key_checks(thd, con->sc_trans);
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        
}

/*#***********************************************************************\
 *
 *              ha_solid_mysql_atype_setmysqldtatype
 *
 * Set MySQL data type based on field->typ()
 *
 * Parameters :
 *
 *   rs_sysi_t*   cd, in, use, solidDB system information
 *   rs_atype_t*  atype, inout, use, solidDB attribute type
 *   Field*       field, in, use, MySQL field
 *
 * Return value : -
 *
 * Globals used : -
 */
static inline void ha_solid_mysql_atype_setmysqldatatype(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        Field* field)
{
        rs_mysqldatatype_t mysqldatatype = RS_MYSQLTYPE_NONE;

        CHK_SYSI(cd);
        CHECK_ATYPE(atype);
        ss_dassert(field != NULL);

        switch (field->type()) {
            case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
                mysqldatatype = RS_MYSQLTYPE_VAR_STRING;
                break;
            case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
                mysqldatatype = RS_MYSQLTYPE_VARCHAR;
                break;
            case MYSQL_TYPE_STRING:
                mysqldatatype = RS_MYSQLTYPE_STRING;
                break;
            case FIELD_TYPE_NEWDECIMAL:
                mysqldatatype = RS_MYSQLTYPE_NEWDECIMAL;
                break;
            case FIELD_TYPE_LONG:
                mysqldatatype = RS_MYSQLTYPE_LONG;
                break;
            case FIELD_TYPE_LONGLONG:
                mysqldatatype = RS_MYSQLTYPE_LONGLONG;
                break;
            case FIELD_TYPE_DATE:
                mysqldatatype = RS_MYSQLTYPE_DATE;
                break;
            case FIELD_TYPE_YEAR:
                mysqldatatype = RS_MYSQLTYPE_YEAR;
                break;
            case FIELD_TYPE_NEWDATE:
                mysqldatatype = RS_MYSQLTYPE_NEWDATE;
                break;
            case FIELD_TYPE_TIME:
                mysqldatatype = RS_MYSQLTYPE_TIME;
                break;
            case FIELD_TYPE_TIMESTAMP:
                mysqldatatype = RS_MYSQLTYPE_TIMESTAMP;
                break;
            case FIELD_TYPE_DATETIME:
                mysqldatatype = RS_MYSQLTYPE_DATETIME;
                break;
            case FIELD_TYPE_TINY:
                mysqldatatype = RS_MYSQLTYPE_TINY;
                break;
            case FIELD_TYPE_SHORT:
                mysqldatatype = RS_MYSQLTYPE_SHORT;
                break;
            case FIELD_TYPE_INT24:
                mysqldatatype = RS_MYSQLTYPE_INT24;
                break;
            case FIELD_TYPE_FLOAT:
                mysqldatatype = RS_MYSQLTYPE_FLOAT;
                break;
            case FIELD_TYPE_DOUBLE:
                mysqldatatype = RS_MYSQLTYPE_DOUBLE;
                break;
            case FIELD_TYPE_DECIMAL:
                mysqldatatype = RS_MYSQLTYPE_DECIMAL;
                break;
            case FIELD_TYPE_GEOMETRY:
                mysqldatatype = RS_MYSQLTYPE_GEOMETRY;
                break;
            case FIELD_TYPE_TINY_BLOB:
                mysqldatatype = RS_MYSQLTYPE_TINY_BLOB;
                break;
            case FIELD_TYPE_MEDIUM_BLOB:
                mysqldatatype = RS_MYSQLTYPE_MEDIUM_BLOB;
                break;
            case FIELD_TYPE_BLOB:
                mysqldatatype = RS_MYSQLTYPE_BLOB;
                break;
            case FIELD_TYPE_LONG_BLOB:
                mysqldatatype = RS_MYSQLTYPE_LONG_BLOB;
                break;
            case MYSQL_TYPE_BIT:
                mysqldatatype = RS_MYSQLTYPE_BIT;
                break;
            default:
                ss_error;
        }

        rs_atype_setmysqldatatype(cd, atype, mysqldatatype);
}

/*#***********************************************************************\
 *
 *              ha_solid_mysql_atype
 *
 * Map MySQL field type to solidDB attribute type
 *
 * Parameters :
 *
 *      rs_sysi_t* cd, in, use
 *      Field*     field, in, use
 *
 * Return value :
 *
 *      rs_atype_t*
 *
 * Globals used :
 */
static rs_atype_t* ha_solid_mysql_atype(rs_sysi_t* cd, Field* field)
{
        rs_atype_t* atype;
        rs_sqldatatype_t sqldatatype = RSSQLDT_INTEGER;
        ulong length;
        long scale;

        length = field->pack_length();
        scale = field->decimals();

        switch (field->type()) {
            case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
            case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
                if (field->type() == MYSQL_TYPE_VARCHAR) {
                    length -= ((Field_varstring*)field)->length_bytes;
                }
                scale = -1;
                if (field->binary()) {
                    sqldatatype = RSSQLDT_LONGVARBINARY;
                } else {
                    sqldatatype = RSSQLDT_LONGVARCHAR;
                }
                break;
            case MYSQL_TYPE_STRING:
                sqldatatype = RSSQLDT_LONGVARCHAR;
                break;
            case FIELD_TYPE_NEWDECIMAL:
                // sqldatatype = RSSQLDT_VARCHAR;
                sqldatatype = RSSQLDT_LONGVARBINARY;
                break;
            case FIELD_TYPE_LONG:
            case FIELD_TYPE_LONGLONG:
                sqldatatype = RSSQLDT_BIGINT;
                break;
            case FIELD_TYPE_DATE:
            case FIELD_TYPE_NEWDATE:
                sqldatatype = RSSQLDT_DATE;
                break;
            case FIELD_TYPE_TIME:
                sqldatatype = RSSQLDT_TIME;
                break;
            case FIELD_TYPE_TIMESTAMP:
            case FIELD_TYPE_DATETIME:
                sqldatatype = RSSQLDT_TIMESTAMP;
                break;
            case FIELD_TYPE_YEAR:
            case FIELD_TYPE_TINY:
            case FIELD_TYPE_SHORT:
            case FIELD_TYPE_INT24:
                sqldatatype = RSSQLDT_INTEGER;
                break;
            case FIELD_TYPE_FLOAT:
            case FIELD_TYPE_DOUBLE:
                sqldatatype = RSSQLDT_DOUBLE;
                break;
            case FIELD_TYPE_DECIMAL:
                // sqldatatype = RSSQLDT_DECIMAL;
                sqldatatype = RSSQLDT_LONGVARBINARY;
                break;
            case FIELD_TYPE_GEOMETRY:
            case FIELD_TYPE_TINY_BLOB:
            case FIELD_TYPE_MEDIUM_BLOB:
            case FIELD_TYPE_BLOB:
            case FIELD_TYPE_LONG_BLOB:
                length = field->field_length;
                scale = -1;
                if (field->binary()) {
                    sqldatatype = RSSQLDT_LONGVARBINARY;
                } else {
                    sqldatatype = RSSQLDT_LONGVARCHAR;
                }
                break;

            case MYSQL_TYPE_BIT:
                length = field->field_length;
                scale = 0;
                sqldatatype = RSSQLDT_LONGVARBINARY;
                break;

            default:
                ss_error;
        }

        atype = rs_atype_initbysqldt(
                    cd,
                    sqldatatype,
                    length,
                    scale);

        rs_atype_setautoinc(cd, atype, (field->flags & AUTO_INCREMENT_FLAG), 0);
        ha_solid_mysql_atype_setmysqldatatype(cd, atype, field);

        ss_pprintf_3(("%s(%d,%d)\n", rs_atype_name(cd, atype), length, scale));

        return(atype);
}

/*#***********************************************************************\
 *
 *              is_mysql_primary_key
 *
 * Return TRUE if given key is a primary key
 *
 * Parameters :
 *
 *      st_key* key, in, use, MySQL key definition
 *
 * Return value : TRUE if key is primary key, FALSE otherwise
 *
 *
 * Globals used :
 */
static inline bool is_mysql_primary_key(st_key* key)
{
        return(strcmp(key->name, "PRIMARY") == 0);
}

/*#***********************************************************************\
 *
 *              ha_mysql_solid_create_unique_init
 *
 * Create a unique key for solidDB
 *
 * Parameters :
 *
 *      rs_sysi_t* cd, in, use
 *      st_key*  key, in, use
 *      uint     nkey, in, use
 *
 * Return value :
 *
 *      tb_sqlunique_t*
 *
 * Globals used :
 */
static tb_sqlunique_t* ha_mysql_solid_create_unique_init(
        rs_sysi_t* cd,
        st_key* key,
        uint nkey)
{
        tb_sqlunique_t* skey = NULL;
        uint kp;
        size_t skey_size;
        KEY_PART_INFO* key_part;
        Field* key_part_field;
        TABLE* table;
        Field* field;

        if (nkey < 1) {
            ss_pprintf_1(("ha_mysql_solid_create_unique:NO PRIMARY KEY:nkey\n", nkey));
            return(NULL);
        }

        if (key == NULL) {
            ss_pprintf_1(("ha_mysql_solid_create_unique:NO PRIMARY KEY:st_key* == NULL\n"));
            return(NULL);
        }

        if (key->key_parts < 1) {
            ss_pprintf_1(("ha_mysql_solid_create_unique:NO PRIMARY KEY:key->key_parts %d\n", key->key_parts));
            return(NULL);
        }

        skey_size = sizeof(tb_sqlunique_t);
        ss_pprintf_1(("ha_mysql_solid_create_unique:PRIMARY KEY:key->key_parts %d, skey_size %d\n", key->key_parts, skey_size));

        ss_dassert(strcmp(key->name, "PRIMARY") == 0);

        skey = (tb_sqlunique_t*)SsMemAlloc(skey_size);
        skey->name = NULL;
        skey->len  = key->key_parts;
        skey->fields = (uint*)SsMemAlloc(skey->len * sizeof(uint));
        skey->prefixes = NULL;

        for (kp = 0; kp < key->key_parts; ++kp) {

            rs_atype_t*     atype;

            ss_pprintf_1(("ha_mysql_solid_create_unique:ano %d\n", key->key_part[kp].fieldnr - 1));
            skey->fields[kp] = key->key_part[kp].fieldnr - 1;

            key_part = &(key->key_part[kp]);
            key_part_field = key->key_part[kp].field;
            table = key->table;

            uint fnum;
            for (fnum = 0; fnum < table->s->fields; ++fnum) {
                field = table->field[fnum];

                if (0 == my_strcasecmp(system_charset_info, field->field_name,
                                       key_part->field->field_name)) {
                    /* Found the corresponding column */
                    key_part_field = field;
                    break;
                }
            }

            atype = ha_solid_mysql_atype(cd, key_part_field);
            if (soliddb_atype_can_have_collation(cd, atype)) {

                uint key_charlen = key_part->length;

                if (key_part_field->charset() != NULL) {
                    if (key_part_field->charset()->mbmaxlen > 1) {
                        key_charlen = key_charlen / key_part_field->charset()->mbmaxlen;
                    }
                }

                ss_pprintf_1(("Key part char_lengt = %lu field->char_length() = %lu\n",
                             key_charlen, key_part_field->char_length()));

                if (key_charlen < key_part_field->char_length()) {
                    CHARSET_INFO* kp_charset;
                    uint kp_charlen;

                    /* calculate char length from maximum byte length
                       and maximum bytes/char
                    */
                    kp_charlen = key_part->length;
                    kp_charset = key_part_field->charset();
                    kp_charlen /= kp_charset->mbmaxlen;

                    if (skey->prefixes == NULL) {
                        skey->prefixes = (uint*)SsMemCalloc(key->key_parts, sizeof(uint));
                    }

                    skey->prefixes[kp] = kp_charlen;
                }
            }
        }

        return(skey);
}

/*#***********************************************************************\
 *
 *              ha_mysql_solid_create_unique_done
 *
 * Free unique index definition structure
 *
 * Parameters :
 *
 *     tb_sqlunique_t* skey, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
static void ha_mysql_solid_create_unique_done(tb_sqlunique_t* skey)
{
        if (skey != NULL) {
            ss_pprintf_1(("ha_mysql_solid_create_unique_done:\n"));
            ss_dassert(skey->len > 0);
            ss_dassert(skey->fields != NULL);

            if (skey->prefixes) {
                SsMemFree(skey->prefixes);
            }

            SsMemFree(skey->fields);
            SsMemFree(skey);
        }
}

/*#***********************************************************************\
 *
 *              ha_mysql_solid_foreign_done
 *
 * Free foreign key definition structure
 *
 * Parameters :
 *
 *     tb_sqlforkey_t* fkey, in, use, foreign key definitions
 *     uint         n_forkeys, in, use, # of foreign key definitions
 *
 * Return value : -
 *
 * Globals used :
 */
static void ha_mysql_solid_foreign_done(
        tb_sqlforkey_t* fkey,
        uint         n_forkeys)
{
        uint n_forkey;

        if (fkey != NULL) {
            ss_pprintf_1(("ha_mysql_solid_foreign_done:\n"));

            for (n_forkey=0; n_forkey < n_forkeys; n_forkey++) {

                if (fkey[n_forkey].fields != NULL) {
                    SsMemFree(fkey[n_forkey].fields);
                }

                if (fkey[n_forkey].name != NULL) {
                    SsMemFree(fkey[n_forkey].name);
                }

                if (fkey[n_forkey].mysqlname != NULL) {
                    SsMemFree(fkey[n_forkey].mysqlname);
                }

                if (fkey[n_forkey].reftable != NULL) {
                    SsMemFree(fkey[n_forkey].reftable);
                }

                if (fkey[n_forkey].refschema != NULL) {
                    SsMemFree(fkey[n_forkey].refschema);
                }

                if (fkey[n_forkey].refcatalog != NULL) {
                    SsMemFree(fkey[n_forkey].refcatalog);
                }

                if (fkey[n_forkey].reffields != NULL) {
                    uint n_cols;
                    uint n_col;

                    n_cols = fkey[n_forkey].len;

                    for (n_col=0; n_col < n_cols; n_col++) {
                        if (fkey[n_forkey].reffields[n_col]) {
                            SsMemFree(fkey[n_forkey].reffields[n_col]);
                        }
                    }

                    SsMemFree(fkey[n_forkey].reffields);
                }
            }

            SsMemFree(fkey);
        }
}

/*#***********************************************************************\
 *
 *              ha_solid_createindex
 *
 * Create a index for a solidDB table
 *
 * Parameters :
 *
 *     rs_relh_t*    relh, in, use, solidDB relation
 *     st_key*       key,in, use, MySQL key definition
 *     tb_connect_t* tbcon, in, use, solidDB table connection
 *     rs_sysi_t*    cd, in, use
 *     tb_trans_t*   trans, in, use, solidDB transaction
 *     char*         authid, in, use, index name
 *     char*         catalog, in, use
 *     char*         extrainfo, in, NULL,
 *     su_err_t*     p_errh, out, error status or NULL
 *
 * Return value : TRUE if success or FALSE in case of error
 *
 * Globals used :
 */
static bool ha_solid_createindex(
        rs_relh_t* relh,
        st_key* key,
        char* tabname,
        tb_connect_t* tbcon,
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char*  authid,
        char*  catalog,
        char*  extrainfo,
        su_err_t**  p_errh)
{
        uint   kp;
        bool   succp=TRUE;
        char*  indexname;
        char*  tauthid;
        char*  tcatalog;
        char*  textrainfo;
        bool   unique;
        uint   attr_c;
        char** attrs;
        solid_bool*  desc;
        void*  cont;
        KEY_PART_INFO* key_part;
        Field* key_part_field;
        TABLE* table;
        Field* field;
        size_t* prefixlengths = NULL;

        ss_dassert(key != NULL);

        ss_pprintf_1(("ha_solid_createindex:%s, table %s (flags)\n", key->name, tabname, key->flags));
        indexname = (char *)SsMemAlloc(strlen(key->name)+64);
        keyname_from_mysql_to_solid(cd, relh, key->name, indexname);

        tauthid = authid;
        tcatalog = catalog;
        textrainfo = extrainfo;
        unique = (key->flags & HA_NOSAME);
        cont = NULL;

        ss_dassert(key->key_parts > 0);
        attr_c = key->key_parts;
        attrs = (char**)SsMemAlloc(attr_c*sizeof(char*));
        desc = (solid_bool*)SsMemAlloc(attr_c*sizeof(solid_bool));

        for (kp=0;kp<attr_c;kp++) {
            rs_atype_t*     atype;
            uint fnum;

            ss_pprintf_1(("ha_solid_createindex:col %s\n" ,key->key_part[kp].field->field_name));

            attrs[kp] = (char*)SsMemStrdup((char*)key->key_part[kp].field->field_name);

            solid_my_caseup_str(attrs[kp]);

            key_part = &(key->key_part[kp]);
            key_part_field = key->key_part[kp].field;

            if (key_part->key_part_flag & HA_REVERSE_SORT) {
                desc[kp] = TRUE;
            } else {
                desc[kp] = FALSE;
            }

            table = key->table;

            for (fnum = 0; fnum < table->s->fields; ++fnum) {

                field = table->field[fnum];

                if (0 == my_strcasecmp(system_charset_info, field->field_name,
                                       key_part->field->field_name)) {
                    /* Found the corresponding column */
                    key_part_field = field;
                    break;
                }
            }

            atype = ha_solid_mysql_atype(cd, key_part_field);

            if (soliddb_atype_can_have_collation(cd, atype)) {

                uint key_charlen = key_part->length;

                if (key_part_field->charset() != NULL) {
                    if (key_part_field->charset()->mbmaxlen > 1) {
                        key_charlen = key_charlen / key_part_field->charset()->mbmaxlen;
                    }
                }

                ss_pprintf_1(("Key part char_lengt = %lu field->char_length() = %lu\n",
                             key_charlen, key_part_field->char_length()));

                if (key_charlen < key_part_field->char_length()) {
                    CHARSET_INFO* kp_charset;
                    size_t kp_charlen;

                    /* calculate char length from maximum byte length
                       and maximum bytes/char
                    */
                    kp_charlen = key_part->length;
                    kp_charset = key_part_field->charset();
                    kp_charlen /= kp_charset->mbmaxlen;

                    if (prefixlengths == NULL) {
                        prefixlengths = (size_t*)SsMemCalloc(attr_c, sizeof(size_t));
                    }

                    prefixlengths[kp] = kp_charlen;
                }
            }
        }

        succp = tb_createindex_prefix(
                     cd,
                     trans,
                     indexname,
                     authid,
                     catalog,
                     extrainfo,
                     tabname,
                     tauthid,
                     tcatalog,
                     textrainfo,
                     unique,
                     attr_c,
                     attrs,
                     desc,
                     prefixlengths,
                     &cont,
                     p_errh);

        ss_dassert(cont == NULL);

        for (kp=0;kp<attr_c;kp++) {
            SsMemFree(attrs[kp]);
        }

        SsMemFree(attrs);
        SsMemFree(desc);
        SsMemFree(indexname);
        SsMemFreeIfNotNULL(prefixlengths);

        return(succp);
}

/*#***********************************************************************\
 *
 *              ::ha_soliddb
 *
 * Constructor
 *
 * TODO: Check that we really can support these handler flags !
 *
 * Parameters : see below
 *
 * Return value : -
 *
 * Globals used :
 */
ha_soliddb::ha_soliddb(
        #if MYSQL_VERSION_ID >= 50100
             handlerton *hton, TABLE_SHARE *table_arg
        #else
             TABLE *table_arg
        #endif
     ) : handler(
        #if MYSQL_VERSION_ID >= 50100
             hton
        #else
             &solid_hton
        #endif
        , table_arg )
{
        DBUG_ENTER("ha_solid::ha_solid");
        ss_pprintf_1(("ha_solid::ha_solid #%p.\n", this));
        MYSQL_THD thd = current_thd;
        user_thd = thd;

        int_table_flags = HA_TABLE_SCAN_ON_INDEX |
            HA_NULL_IN_KEY |
            HA_CAN_INDEX_BLOBS |
            HA_CAN_GEOMETRY | /*TODO: GEOMETRY INDEX SUPPORT*/
            HA_AUTO_PART_KEY |
            HA_CAN_SQL_HANDLER |
            HA_PRIMARY_KEY_IN_READ_INDEX |
            HA_REC_NOT_IN_SEQ |

/* EXACT count a test */
#if MYSQL_VERSION_ID >= 50100
            HA_BINLOG_ROW_CAPABLE |
            HA_BINLOG_STMT_CAPABLE |			/* Testing: Beta code */
            HA_PARTIAL_COLUMN_READ |
            HA_PRIMARY_KEY_REQUIRED_FOR_POSITION |
	    HA_STATS_RECORDS_IS_EXACT;

#else
            HA_NOT_EXACT_COUNT;
#endif

        num_write_rows = 0;
        solid_conn = NULL;
        solid_table = NULL;
        solid_relcur = NULL;
        rnd_mustinit = FALSE;
        for_update = FALSE;
        mainmemory = FALSE;

        extra_ignore_duplicate = FALSE;
        extra_replace_duplicate = FALSE;
        extra_update_duplicate = FALSE;

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              ::~ha_soliddb
 *
 * Destructor
 *
 * Parameters : -
 *
 * Return value : -
 *
 * Globals used :
 */
ha_soliddb::~ha_soliddb()
{
        ss_pprintf_1(("ha_solid::~ha_solid #%p.\n", this));

        if (solid_conn != NULL) {

            solid_clear_connection(
                    solid_conn,
                    solid_table,
                    solid_relcur);

            solid_conn = NULL;
            solid_relcur = NULL;
            solid_table = NULL;
        }

        ss_dassert(solid_conn == NULL);
        ss_dassert(solid_relcur == NULL);
        ss_dassert(solid_table == NULL);
}

/*#***********************************************************************\
 *
 *              ::get_error_message
 *
 * This function is called from handler::print_error() to get solidDB
 * specific error message (see handler::print_error() for details)
 *
 * Parameters :
 *
 *     error  in, use   - error code (returned by ha_solid_mysql_error())
 *     buf    out, use  - corresponding error message
 *
 * Return value : 1 - this is a temporary error (error will be reported
 *                    to client as a temporary one)
 *                0 - otherwise
 *                    (see handler::print_error() for details)
 *
 * Globals used :
 */
bool ha_soliddb::get_error_message(
        int error,
        String* buf)
{
        switch (error) {
            case HA_ERR_TO_BIG_ROW:
                buf->append(STRING_WITH_LEN("Too big row"));
                break;
            case HA_ERR_ROW_IS_REFERENCED:
            case HA_ERR_NO_REFERENCED_ROW:
            {
                SOLID_CONN *con;

#if MYSQL_VERSION_ID >= 50100
                con = get_solid_connection(current_thd, table->s->path.str);
#else
                con = get_solid_connection(current_thd, table->s->path);
#endif

                CHK_CONN(con);


                if (con->sc_err_tableid == 0 && solid_table != NULL) {
                    con->sc_err_tableid = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);
                }

                solid_get_error_message(
#if MYSQL_VERSION_ID >= 50100
                        this->ht,
#endif
                        current_thd,
                        (TABLE**)NULL,
                        error,
                        buf);
                break;
            }
            default:
                break;
        }

        return FALSE;
}

/*#***********************************************************************\
 *
 *              ::start_stmt
 *
 * MySQL calls this function at the start of each SQL statement inside LOCK
 * TABLES. Inside LOCK TABLES the ::external_lock method does not work to
 * mark SQL statement borders. Note also a special case: if a temporary table
 * is created inside LOCK TABLES, MySQL does not call external_lock() at all
 * on that table.
 * MySQL-5.0 also calls this before each statement in an execution of a stored
 * procedure. To make the execution more deterministic for binlogging, MySQL-5.0
 * locks all tables involved in a stored procedure with full explicit table
 * locks (thd->in_lock_tables is true in ::store_lock()) before executing the
 * procedure.
 *
 * Registers that solid takes part in an SQL statement, so that MySQL knows to
 * roll back the statement if the statement results in an error. This MUST be
 * called for every SQL statement that may be rolled back by MySQL. Calling this
 * several times to register the same statement is allowed, too.
 *
 * Registers an solid transaction in MySQL, so that the MySQL XA code knows
 * to call the solid prepare and commit, or rollback for the transaction. This
 * MUST be called for every transaction for which the user may call commit or
 * rollback. Calling this several times to register the same transaction is
 * allowed, too.
 *
 * This function also registers the current SQL statement.
 *
 * Parameters :
 *
 *     THD*          thd, in, use
 *     thr_lock_type lock_type, in, use
 *
 * Return value : 0
 *
 * Globals used :
 */
int ha_soliddb::start_stmt(
        MYSQL_THD       thd,
        thr_lock_type   lock_type)
{
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        int           error = 0;
        bool          autocommit = FALSE;

        DBUG_ENTER("ha_solid::start_stmt");

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
        autocommit = !(thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
#else
        con = get_solid_connection(thd, table->s->path);
        autocommit = !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#endif
        CHK_CONN(con);

        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

        ss_pprintf_1(("ha_solid::start_stmt, con=%ld, trans=%ld, lock_type=%d, query=%s\n",
                      (long)con, (long)trans, lock_type, soliddb_query(thd)));

        extra_keyread = FALSE;
        extra_retrieve_primary_key = FALSE;
        extra_retrieve_all_cols = FALSE;

        /* Register the statement */

        trans_register_ha(thd, FALSE, (handlerton *)ht);

        ss_pprintf_1(("Statement registered\n"));

        if (tb_trans_isstmtactive(cd, trans)) {
#if MYSQL_VERSION_ID >= 50100
            error = solid_commit(ht, thd, FALSE);
#else
            error = solid_commit(thd, FALSE);
#endif
        }

        if (error == 0) {
            tb_trans_begintransandstmt(cd, trans);

            if (!autocommit) {
                /* No AUTOCOMMIT mode, register for a transaction */

                trans_register_ha(thd, FALSE, (handlerton *)ht);
                ss_pprintf_1(("Transaction registered\n"));
            }
        }

        DBUG_RETURN(error);
}

/*#***********************************************************************\
 *
 *              ::bas_ext
 *
 * If frm_error() is called then we will use this to to find out what file extentions
 * exist for the storage engine. This is also used by the default rename_table and
 * delete_table method in handler.cc.
 *
 * Parameters : -
 *
 * Return value :
 *
 *     char**
 *
 * Globals used :
 */
/*
*/
static const char *ha_soliddb_exts[] = {
        NullS
};

const char **ha_soliddb::bas_ext() const
{
        ss_pprintf_1(("ha_solid::bas_ext\n"));
        return ha_soliddb_exts;
}

/*#***********************************************************************\
 *
 *              ::table_caching_type
 *
 * Return table caching type
 *
 * #define HA_CACHE_TBL_NONTRANSACT 0
 * #define HA_CACHE_TBL_NOCACHE     1
 * #define HA_CACHE_TBL_ASKTRANSACT 2
 * #define HA_CACHE_TBL_TRANSACT    4
 *
 * Parameters : -
 *
 * Return value : uint8
 *
 * Globals used :
 */
uint8 ha_soliddb::table_cache_type()
{
        ss_pprintf_1(("ha_solid::table_cache_type:HA_CACHE_TBL_TRANSACT\n"));
        return HA_CACHE_TBL_TRANSACT; /* Use transactional query cache */
}

/*#***********************************************************************\
 *
 *              get_ref_length
 *
 * Return length of the clustered key
 *
 * Parameters :
 *
 *     rs_sysi_t* cd, in, use
 *     rs_relh_t* rsrelh, in, use, solidDB relation
 *
 * Return value : int
 *
 * Globals used :
 */
static int get_ref_length(rs_sysi_t* cd, rs_relh_t* rsrelh)
{
        rs_key_t* key;
        rs_ano_t nrefparts;
        rs_ano_t i;
        rs_ttype_t* ttype;
        rs_ano_t ano;
        rs_atype_t* atype;
        long atype_maxlen;
        int ref_length;

        DBUG_ENTER("ha_soliddb::get_ref_length");
        ss_pprintf_1(("ha_solid::get_ref_length\n"));

        key = rs_relh_clusterkey(cd, rsrelh);
        nrefparts = rs_key_nrefparts(cd, key);
        ttype = rs_relh_ttype(cd, rsrelh);

        ref_length = VA_LENGTHMAXLEN;

        for (i = 0; i < nrefparts; i++) {
            ano = rs_keyp_ano(cd, key, i);

            if (ano == RS_ANO_NULL) {
                /* Not a real column. */
                atype = rs_keyp_constatype(cd, key, i);
                ss_dassert(atype != NULL);
            } else {
                atype = rs_ttype_atype(cd, ttype, ano);
            }

            atype_maxlen = rs_atype_maxstoragelength(cd, atype);

            ref_length = ref_length + atype_maxlen;
        }

        if (rs_keyp_parttype(cd, key, i) == RSAT_TUPLE_VERSION) {
            ano = rs_keyp_ano(cd, key, i);
            atype = rs_ttype_atype(cd, ttype, ano);
            atype_maxlen = rs_atype_maxstoragelength(cd, atype);

            ref_length = ref_length + atype_maxlen;
        }

        DBUG_RETURN(ref_length);
}

/*#***********************************************************************\
 *
 *              ::open
 *
 * Used for opening tables. The name will be the name of the file.
 * A table is opened when it needs to be opened. For instance
 * when a request comes in for a select on the table (tables are not
 * open and closed for each request, they are cached).
 *
 * Called from handler.cc by handler::ha_open(). The server opens all tables by
 * calling ha_open() which then calls the handler specific open().
 *
 * Parameters :
 *
 *     const char* name, in, use, table name
 *     int         mode, in, not used
 *     uint        test_if_locked, in, not used
 *
 * Return value : 0
 *
 * Globals used : current_thd
 */
int ha_soliddb::open(
        const char *name,
        int mode,
        uint test_if_locked)
{
        SOLID_CONN* con;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::open");
        ss_pprintf_1(("ha_solid::open\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, name);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        if (solid_table != NULL) {
            ref_length = get_ref_length(con->sc_cd,
                                        solid_table->st_rsrelh);

            if (ref_length > MAX_REF_LENGTH) { /* TODO: Need a better check. */
                ref_length = MAX_REF_LENGTH;
            }
        } else {
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (!(share = get_share(name))) {
            DBUG_RETURN(1);
        }

        thr_lock_data_init(&share->lock, &lock, NULL);

        info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::close
 *
 * Closes a table. We call the free_share() function to free any resources
 * that we have allocated in the "shared" structure.
 *
 * Called from sql_base.cc, sql_select.cc, and table.cc.
 * In sql_select.cc it is only used to close up temporary tables or during
 * the process where a temporary table is converted over to being a
 * myisam table.
 *
 * For sql_base.cc look at close_data_tables().
 *
 *
 * Parameters : -
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::close(void)
{
        DBUG_ENTER("ha_soliddb::close");
        ss_pprintf_1(("ha_solid::close a table\n"));
        free_share(share);
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ha_print_aval
 *
 * Print attribute value
 *
 * Parameters :
 *
 *     rs_sysi_t*   cd, in, use
 *     rs_atype_t*  atype, in, use
 *     rs_aval_t*   aval, in, use
 *     char*        infostr, in
 *
 * Return value : -
 *
 * Globals used :
 */
static void ha_print_aval(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        char* infostr)
{
        char* tmp;
        tmp = rs_aval_print(cd, atype, aval);
        ss_pprintf_2(("%s:aval='%.255s'\n", infostr, tmp));
        SsMemFree(tmp);
}

/*#***********************************************************************\
 *
 *              set_mysql_field_to_aval_or_dynva
 *
 * Set mysql field value to solidDB attribute value or dynamic value
 *
 * Parameters :
 *
 *     THD*        thd, in, use, MySQL thread
 *     rs_sysi_t*  cd, in, use, solid system info
 *     rs_atype_t* atype, in out, use, attribute type
 *     rs_aval_t*  aval, in out, use, attribute value
 *     dynva_t*    dva, in out use,
 *     Field*      field, in, use, MySQL attribute
 *     const SS_MYSQL_ROW* ptr, in, MySQL value
 *     bool        iskeyptr, in, use,
 *     su_err_t**  p_errh, in, use, solidDB error structure
 *
 * Return value : -
 *
 * Globals used :
 */
static bool set_mysql_field_to_aval_or_dynva(
        MYSQL_THD thd,
        TABLE* table,
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval,
        va_t* dva,
        Field* field,
        SS_MYSQL_ROW* ptr,
        bool iskeyptr,
        su_err_t** p_errh)
{
        RS_AVALRET_T avret;
        int32 i4 = 0;
        int mysqltype;
        Field_blob* field_blob = NULL;
        bool succp = TRUE;
#if MYSQL_VERSION_ID >= 50100
        my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
#endif

        mysqltype = field->type();

        switch (rs_atype_datatype(cd, atype)) {
            case RSDT_CHAR:
                {
                    bool ismysqlblob;
                    uint length;

                    ismysqlblob =
                    ismysqlblob = (mysqltype == FIELD_TYPE_TINY_BLOB   ||
                                  mysqltype == FIELD_TYPE_MEDIUM_BLOB ||
                                  mysqltype == FIELD_TYPE_BLOB        ||
                                  mysqltype == FIELD_TYPE_LONG_BLOB);

                    if (ismysqlblob) {
#if MYSQL_VERSION_ID >= 50120
                        uchar* buf;
#else
                        char* buf;
#endif
                        uint length;

                        ss_dassert(mysqltype == FIELD_TYPE_TINY_BLOB   ||
                                  mysqltype == FIELD_TYPE_MEDIUM_BLOB ||
                                  mysqltype == FIELD_TYPE_BLOB        ||
                                  mysqltype == FIELD_TYPE_LONG_BLOB);

                        if (iskeyptr) {
                            short j;
                            shortget(j, ptr);
                            length = j;
#if MYSQL_VERSION_ID >= 50120
                            buf = (uchar*)ptr + 2;
#else
                            buf = (char*)ptr + 2;
#endif
                        } else {
                            field_blob = (Field_blob*)field;
                            ss_pprintf_1(("BLOB:type %d, real_type %d, field_length %d, field->binary() == %d\n",
                                      mysqltype, field->real_type(), field->field_length, field->binary()));

                            length = field_blob->get_length();
                            field_blob->get_ptr(&buf);
                        }

                        ss_pprintf_1(("FIELD_BLOB:CHAR:iskeyptr %d, field_length %d value=(%.20s)\n", iskeyptr, length, buf));

                        if (dva != NULL) {
                            va_setdataandnull(dva, buf, length);
                        } else {
                            avret = rs_aval_set8bitcdata_ext(cd, atype, aval, (char *)buf, length, p_errh);
                            succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                        }
                    } else {

                        ss_dassert(mysqltype == FIELD_TYPE_STRING ||
                                  mysqltype == MYSQL_TYPE_VAR_STRING ||
                                  mysqltype == MYSQL_TYPE_VARCHAR);

                        /* Logic here is copied from another ha_ file (which one??)
                         * The slightly different behaviour is done because lenght bytes
                         * are different if the value is given as a row or as a constraint.
                         * Also I could not find a method that would give the out from
                         * a key constraint data buffer.
                         */
                        switch (field->real_type()) {
                        case MYSQL_TYPE_SET:
                        case MYSQL_TYPE_ENUM:
                            length = field->pack_length();
                            break;
                        case MYSQL_TYPE_VARCHAR:
                            /* varstring */
                            if (iskeyptr) {
                                length = uint2korr(ptr);
                                ptr = ptr + 2;
                            } else {
                                uint length_bytes = field->pack_length() - field->key_length();
                                length = length_bytes == 1 ? (uint) (uchar) *ptr : uint2korr(ptr);
                                ptr = ptr + length_bytes;
                            }
                            break;
                        case MYSQL_TYPE_STRING:
                        {
                            /* just to be sure it is CHAR sql type*/
                            /* we have to trim trailing spaces from CHAR strings */
                            su_collation_t* at_collation = NULL;
                            char*           buf          = (char*)ptr;
                            length = field->field_length;

                            at_collation = rs_atype_collation(cd, atype);
                            if (at_collation) {
                                CHARSET_INFO* csinfo = (CHARSET_INFO*)at_collation->coll_data;
                                if (csinfo->mbmaxlen == 1) {
                                    while (length && buf[length - 1] == 0x20) {
                                        --length;
                                    }
                                } else {
                                    while (length && (buf[length - 1] == 0x20 || buf[length - 1] == 0x00)) {
                                        --length;
                                    }
                                }
                            } else {
                                /* consider we have 8 bit encoded string */
                                while (length && buf[length - 1] == 0x20) {
                                    --length;
                                }
                            }
                            break;
                        }
                        default:
                            /* string */
                            length = field->field_length;
                            break;
                        }

                        if (dva != NULL) {
                            va_setdataandnull(dva, (void*)ptr, length);
                        } else {
                            avret = rs_aval_set8bitcdata_ext(cd, atype, aval, (char*)ptr, length, p_errh);
                            succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                        }
                    }
                }
                break;
            case RSDT_INTEGER: {

                switch(mysqltype) {
                   case FIELD_TYPE_YEAR:
                       i4 = field->val_int();
                       break;
                   case FIELD_TYPE_LONG:
                        /* Logic copied to here from field.cpp because I could not
                         * find a method to read key constraint value data. So we
                         * just as well always use this direct read method.
                         */
#ifdef WORDS_BIGENDIAN
                        if (table->s->db_low_byte_first) {
                            i4 = sint4korr(ptr);
                        } else
#endif
                        {
                            if (((Field_num*)field)->unsigned_flag) {
                                ulongget(i4, ptr);
                            } else {
                                longget(i4, ptr);
                            }
                        }
                        break;
                    case FIELD_TYPE_TINY:
                        /* Copied from field.cpp
                         */
                        i4 = ((Field_num*)field)->unsigned_flag
                                ? (int32) ((uchar*) ptr)[0] :
                                (int32) ((signed char*) ptr)[0];
                        break;
                    case FIELD_TYPE_SHORT:
                        {
                            /* Copied from field.cpp
                             */
                            short j;
#ifdef WORDS_BIGENDIAN
                            if (table->s->db_low_byte_first) {
                                j = sint2korr(ptr);
                            } else
#endif
                            {
                                shortget(j, ptr);
                            }
                            i4 = ((Field_num*)field)->unsigned_flag
                                    ? (int32) (unsigned short) j
                                    : (int32) j;
                        }
                        break;
                    case FIELD_TYPE_INT24:
                        /* Copied from field.cpp
                         */
                        i4 = ((Field_num*)field)->unsigned_flag
                                ? (int32) uint3korr(ptr)
                                : sint3korr(ptr);
                        break;
                    default:
                       ss_error;
                }

                if (dva != NULL) {
                    va_setlong(dva, i4);
                } else {
                    avret = rs_aval_setlong_ext(cd, atype, aval, i4, p_errh);
                    succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                }

                break;
            }
            case RSDT_FLOAT:
            case RSDT_DOUBLE:
                {
                    /* Copied from field.cpp
                     */
                    if (mysqltype == FIELD_TYPE_FLOAT) {
                        float j;

#ifdef WORDS_BIGENDIAN
                        if (table->s->db_low_byte_first) {
                            float4get(j, ptr);
                        } else
#endif
                        {
                            memcpy((SS_MYSQL_ROW*)&j, ptr, sizeof(j));
                        }
                        if (dva != NULL) {
                            va_setdouble(dva, (double)j);
                        } else {
                            avret = rs_aval_setdouble_ext(cd, atype, aval, (double)j, p_errh);
                            succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                        }
                    } else if (mysqltype == FIELD_TYPE_DOUBLE) {

                        double j;

#ifdef WORDS_BIGENDIAN
                        if (table->s->db_low_byte_first) {
                            float8get(j,ptr);
                        } else
#endif
                        {
                            doubleget(j,ptr);
                        }
                        if (dva != NULL) {
                            va_setdouble(dva, j);
                        } else {
                            avret = rs_aval_setdouble_ext(cd, atype, aval, j, p_errh);
                            succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                        }
                    } else {
                        ss_error;
                    }
                }
                break;
            case RSDT_DATE: {
#if MYSQL_VERSION_ID < 50045
                TIME ltime;
#else
                MYSQL_TIME ltime;
#endif
                dt_date_t solid_date;
                dt_datesqltype_t datesqltype = DT_DATE_SQLDATE;

                /* See field.cc for more documentation about these types */

                memset(&ltime, 0, sizeof(ltime));


                switch(field->real_type()) {

                    case FIELD_TYPE_NEWDATE:
                       {
#if MYSQL_VERSION_ID < 50100
                         Field_newdate tmp((char*)ptr, 0, 0, field->unireg_check, 0, table, field->charset());
#elif MYSQL_VERSION_ID < 50120
                         Field_newdate tmp((char *)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#else
                         Field_newdate tmp((uchar *)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#endif
                         tmp.get_date(&ltime, 0);
                         datesqltype = DT_DATE_SQLDATE;
                         break;
                       }
                    case FIELD_TYPE_DATE:
                       {
#if MYSQL_VERSION_ID < 50100
                         Field_date tmp((char*)ptr, 0, 0, field->unireg_check, 0, table, field->charset());
#elif MYSQL_VERSION_ID < 50120
                         Field_date tmp((char*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#else
                         Field_date tmp((uchar*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#endif
                         tmp.get_date(&ltime, 0);
                         datesqltype = DT_DATE_SQLDATE;
                         break;
                       }
                    case FIELD_TYPE_TIME:
                       {
#if MYSQL_VERSION_ID < 50100
                         Field_time tmp((char*)ptr,0,0,field->unireg_check,0, table,field->charset());
#elif MYSQL_VERSION_ID < 50120
                         Field_time tmp((char*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#else
                         Field_time tmp((uchar*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#endif

                         tmp.get_time(&ltime);
                         datesqltype = DT_DATE_SQLTIME;
                         break;
                       }
                    case FIELD_TYPE_TIMESTAMP:
                       {
#if MYSQL_VERSION_ID < 50100
                         Field_timestamp tmp((char*)ptr, 0, 0, 0, field->unireg_check, 0, table, field->charset());
#elif MYSQL_VERSION_ID < 50120
                         Field_timestamp tmp((char*)ptr, (uint32)0, (uchar*)NULL, (uchar)0,
                                             field->unireg_check, (const char *)NULL, table->s, field->charset());
#else
                         Field_timestamp tmp((uchar*)ptr, (uint32)0, (uchar*)NULL, (uchar)0,
                                             field->unireg_check, (const char *)NULL, table->s, field->charset());
#endif
                         tmp.get_date(&ltime, 0);
                         datesqltype = DT_DATE_SQLTIMESTAMP;
                         break;
                       }
                    case FIELD_TYPE_DATETIME:
                       {
#if MYSQL_VERSION_ID < 50100
                         Field_datetime tmp((char*)ptr, 0, 0, field->unireg_check, 0, table, field->charset());
#elif MYSQL_VERSION_ID < 50120
                         Field_datetime tmp((char*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#else
                         Field_datetime tmp((uchar*)ptr, NULL, (uchar)0, field->unireg_check, NULL, field->charset());
#endif
                         tmp.get_date(&ltime, 0);
                         datesqltype = DT_DATE_SQLTIMESTAMP;
                         break;
                       }
                   default:
                        ss_error;
                        break;
                }

                succp = dt_date_setdata(&solid_date,
                                    ltime.year,  /* year  */
                                    ltime.month, /* month */
                                    ltime.day,   /* day   */
                                    ltime.hour,  /* hour  */
                                    ltime.minute,/* minute*/
                                    ltime.second,/* second*/
                                    0);

                if (!succp) {
                    if (dva != NULL) {
                        dt_date_datetova(&solid_date, dva);
                    } else {
                        rs_aval_setnull(cd, atype, aval);
                        succp = TRUE;
                    }
                } else {

                    ss_pprintf_2(("Stored date: %d-%d-%d %d:%d:%d\n",
                                  dt_date_year(&solid_date), dt_date_month(&solid_date),
                                  dt_date_mday(&solid_date), dt_date_hour(&solid_date),
                                  dt_date_min(&solid_date), dt_date_sec(&solid_date)));

                    if (dva != NULL) {
                        dt_date_datetova(&solid_date, dva);
                    } else {

                        avret = rs_aval_setdate_ext(cd, atype, aval, &solid_date, datesqltype, p_errh);
                    succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                    }
                }

                break;
            }
            case RSDT_DFLOAT:
                ss_error;
                break;
            case RSDT_BINARY:
                switch (mysqltype) {
                    case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
                    case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
                        {
                            uint length;
                            ss_dassert(field->binary());
                            if (field->real_type() != MYSQL_TYPE_VARCHAR) {
                                /* string */
                                length = field->field_length;
                            } else {
                                /* varstring */
                                if (iskeyptr) {
                                    length = uint2korr(ptr);
                                    ptr = ptr + 2;
                                } else {
                                    uint length_bytes = field->pack_length() - field->key_length();
                                    length = length_bytes == 1 ? (uint) (uchar) *ptr : uint2korr(ptr);
                                    ptr = ptr + length_bytes;
                                }
                            }
                            if (dva != NULL) {
                                va_setdataandnull(dva, (void*)ptr, length);
                            } else {
                                avret = rs_aval_setbdata_ext(cd, atype, aval, (char*)ptr, length, p_errh);
                                succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                            }
                        }
                        break;
                    case FIELD_TYPE_NEWDECIMAL:
                        rs_atype_setextlenscale(
                            cd,
                            atype,
                            ((Field_new_decimal*)field)->precision,
                            field->decimals());
                        /* FALLTHROUGH */
                    case FIELD_TYPE_DECIMAL:
                    case MYSQL_TYPE_BIT:
                        if (dva != NULL) {
                            va_setdataandnull(dva, (void*)ptr, field->pack_length());
                        } else {
                            avret = rs_aval_setbdata_ext(
                                        cd,
                                        atype,
                                        aval,
                                        (char*)ptr,
                                        field->pack_length(),
                                        p_errh);
                            succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                        }
                        break;
                    case FIELD_TYPE_GEOMETRY:
                    case FIELD_TYPE_TINY_BLOB:
                    case FIELD_TYPE_MEDIUM_BLOB:
                    case FIELD_TYPE_BLOB:
                    case FIELD_TYPE_LONG_BLOB:
                        {
#if MYSQL_VERSION_ID >= 50120
                            uchar *buf;
#else
                            char* buf;
#endif
                            uint length;
                            if (iskeyptr) {
                                short j;
                                shortget(j, ptr);
                                length = j;
#if MYSQL_VERSION_ID >= 50120
                                buf = (uchar*)ptr + 2;
#else
                                buf = (char*)ptr + 2;
#endif
                            } else {
                                field_blob = (Field_blob*)field;
                                ss_pprintf_1(("BLOB:type %d, real_type %d, field_length %d, field->binary() == %d\n",
                                          mysqltype, field->real_type(), field->field_length, field->binary()));

                                length = field_blob->get_length();
                                field_blob->get_ptr(&buf);
                            }

                            ss_pprintf_1(("FIELD_BLOB:BINARY:iskeyptr %d, field_length %d value=(%.20s)\n", iskeyptr, length, buf));

                            if (dva != NULL) {
                                va_setdataandnull(dva, buf, length);
                            } else {
                                avret = rs_aval_setbdata_ext(cd, atype, aval, buf, length, p_errh);
                                succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                            }
                        }
                        break;
                    default:
                        ss_error;
                }
                break;
            case RSDT_UNICODE:
                ss_error;
                break;
            case RSDT_BIGINT:
                {
                    ulonglong j;
                    ss_int8_t i8;

                    switch(field->real_type()) {
                        case FIELD_TYPE_LONG: {
#ifdef WORDS_BIGENDIAN
                            if (table->s->db_low_byte_first) {
                                ulong k;
                                k = sint4korr(ptr);
                                j = (ulonglong)k;
                            } else
#endif
                            {
                                if (((Field_num*)field)->unsigned_flag) {
                                    ulong k;

                                    ulongget(k, ptr);
                                    j = (ulonglong)k;
                                } else {
                                    long k;

                                    longget(k, ptr);
                                    j = (ulonglong)k;
                                }
                            }

                            break;
                        }
                        default: {
#ifdef WORDS_BIGENDIAN
                            if (table->s->db_low_byte_first) {
                                j = sint8korr(ptr);
                            } else
#endif
                            {
                                longlongget(j, ptr);
                            }
                            break;
                        }
                    }

                    SsInt8SetNativeUint8(&i8, j);

                    if (dva != NULL) {
                        va_setint8(dva, i8);
                    } else {
                        avret = rs_aval_setint8_ext(cd, atype, aval, i8, p_errh);
                        succp = (avret == RSAVR_SUCCESS || avret == RSAVR_TRUNCATION);
                    }
                }
                break;
            default:
                ss_error;
        }

        if (!succp && p_errh && *p_errh) {
            sql_print_error("%s (%s)", su_err_geterrstr(*p_errh), soliddb_query(thd));
        }

#ifdef SS_DEBUG
        if (aval) {
            char* tmp;
            tmp = rs_aval_print(cd, atype, aval);
            ss_pprintf_1(("Field %s stored value '%s'\n", field->field_name, tmp));
            SsMemFree(tmp);
        }
#endif

#if MYSQL_VERSION_ID >= 50100
        dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

        return(succp);
}

/*#***********************************************************************\
 *
 *              do_checkpoint
 *
 * This function flushes the logs and does a checkpoint for the MySQL/solidDB
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     char* parameters,  in, use, parameters
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB return code
 *
 * Globals used: current_thd
 */
static su_ret_t do_checkpoint(
        rs_sysi_t* cd,
        char* parameters __attribute__((unused)),
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;
        tb_connect_t* tc = NULL;
        dbe_db_t* soliddb = NULL;

        DBUG_ENTER("solid:do_checkpoint");
        ss_pprintf_1(("do_checkpoint\n"));

        ss_dassert(cd != NULL);

        soliddb = (dbe_db_t*)rs_sysi_db(cd);

        dbe_db_logflushtodisk(soliddb);

        tc = tb_sysconnect_init((tb_database_t*)rs_sysi_tabdb(cd));

        /* adding a checkpoint record to the end of log */

        rc = tb_createcheckpoint(tc,/*splitlog = */ TRUE);

        if (rc != DBE_RC_SUCC) {
            su_err_init(p_errh, rc);
        }

        tb_sysconnect_done(tc);

        DBUG_RETURN(rc);
}


/*#***********************************************************************\
 *
 *              do_ssdebug
 *
 * This function sets ssdebug information.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     char* parameters,  in, use, parameters
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB return code
 *
 * Globals used: current_thd
 */
static su_ret_t do_ssdebug(
        rs_sysi_t* cd,
        char* parameters,
        su_err_t** p_errh)
{
        su_ret_t rc = SU_SUCCESS;

        DBUG_ENTER("solid:do_ssdebug");
        ss_pprintf_1(("do_ssdebug\n"));

        if (parameters != NULL) {
            SsDbgSet(parameters);
        }

        DBUG_RETURN(rc);
}


/*#***********************************************************************\
 *
 *              solid_clear_dir
 *
 * This function removes files from the directory
 *
 * Parameters :
 *
 *     const char* path, in, use, directory name
 *
 * Return value : 0  for SUCCESS
 *                -1 in case of error
 *
 * Globals used: -
 */
static int solid_clear_dir(
        const char* path )
{
    SDB_DBUG_ENTER("solid_clear_dir");

    MY_DIR *dirp = my_dir( path, MYF(MY_DONT_SORT|MY_WANT_STAT) );

    if( !dirp ) {
        goto ok;
    }

    for ( uint i = 0; i < dirp->number_off_files; ++i ) {
        FILEINFO *file = dirp->dir_entry + i;
        char filePath[FN_REFLEN];

        /* skipping this directory and parent directory references. */
        if ( MY_S_ISDIR(file->mystat->st_mode) &&
            (!strcmp( file->name, "." ) || !strcmp( file->name, ".." )) ) {
            continue;
        }

        strxmov( filePath, path, "/", file->name, NullS );

        if ( MY_S_ISDIR(file->mystat->st_mode) ?
                      solid_clear_dir( filePath ) || rmdir( filePath ) : /* for dirs call itself recursively */
                      my_delete_with_symlink( filePath, MYF(MY_WME) ) /* otherwise just remove it */
            ) {
            goto err;
        }
    }

ok:
    my_dirend(dirp);
    SDB_DBUG_RETURN(0);

err:
    my_dirend( dirp );
    SDB_DBUG_RETURN(-1);
}

/*#***********************************************************************\
*              handle_backslashes
*
* Parameters :
*
* Return value :
*
* Globals used : -
*
*/
static void handle_backslashes(const char* src, char* dst)
{
    while (*src)
    {
        if (*src == '\\')
            *dst++ = '\\';
        *dst++ = *src++;
    }
    *dst = 0;
}

/*#***********************************************************************\
*              handle_backslashes
*
* Parameters :
*
* Return value :
*
* Globals used : -
*
*/
static bool fwrite_string(FILE *file, const char *str)
{
    uint cnt = strlen(str);
    return my_fwrite(file, (SS_MYSQL_ROW*) str, cnt, MYF(0)) == cnt;
}

/* #ifndef MYSQL_DYNAMIC_PLUGIN */

/*#***********************************************************************\
*              handle_backslashes
*
* Parameters :
*
* Return value :
*
* Globals used : -
*
*/
static int backup_config(const char *backup_dir)
{
    char cnf_file_name[FN_REFLEN];
    FILE *file;
    char buf[255];
    char valbuf[255];
    int errcode = -1;

    strxmov( cnf_file_name, backup_dir, "/", BACKUP_CONFIG_FILE_NAME, NULL );

    if ( !(file = my_fopen(cnf_file_name, O_CREAT|O_WRONLY, MYF(0))) ) {
        return -1;
    }

    if (!fwrite_string(file, "[mysqld]\n")) {
        goto FINISH;
    }

    for (struct my_option *opt = my_long_options; opt->name; opt++) {
        void *pvalue = (void *)opt->value;

        if (!pvalue) {
            continue;
        }

        switch (opt->var_type & GET_TYPE_MASK)
        {
            case GET_NO_ARG:
            case GET_BOOL:
            {
                my_bool value = *(my_bool*) pvalue;
                if (value == (my_bool) opt->def_value)
                    continue;
                if (value)
                    sprintf(buf, "%s\n", opt->name);
                else
                    sprintf(buf, "skip-%s\n", opt->name);
                break;
            }
            case GET_INT:
            {
                int value = *(int*) pvalue;
                if (value == (int) opt->def_value)
                    continue;
                sprintf(buf, "%s=%d\n", opt->name, value);
                break;
            }
            case GET_UINT:
            {
                uint value = *(uint*) pvalue;
                if (value == (uint) opt->def_value)
                    continue;
                sprintf(buf, "%s=%u\n", opt->name, value);
                break;
            }
            case GET_LONG:
            {
                long value = *(long*) pvalue;
                if (value == (long) opt->def_value)
                    continue;
                sprintf(buf, "%s=%ld\n", opt->name, value);
                break;
            }
            case GET_ULONG:
            {
                ulong value = *(ulong*) pvalue;
                if (value == (ulong) opt->def_value)
                    continue;
                sprintf(buf, "%s=%lu\n", opt->name, value);
                break;
            }
            case GET_LL:
            {
                longlong value = *(longlong*) pvalue;
                if (value == (longlong) opt->def_value)
                    continue;
                longlong2str(value, valbuf, -10);
                sprintf(buf, "%s=%s\n", opt->name, valbuf);
                break;
            }
            case GET_ULL:
            {
                ulonglong value = *(ulonglong*) pvalue;
                if (value == (ulonglong) opt->def_value)
                    continue;
                longlong2str(value, valbuf, 10);
                sprintf(buf, "%s=%s\n", opt->name, valbuf);
                break;
            }
            case GET_STR:
            case GET_STR_ALLOC:
            {
                char* value = *(char**) pvalue;
                if (!value)
                    continue;

                if (!SsStricmp(opt->name, "datadir"))
                    value = mysql_real_data_home;

                if (opt->def_value && *(char**) opt->def_value && !SsStricmp(value, *(char**) opt->def_value))
                    continue;

                if ((!SsStricmp(opt->name, "myisam-recover") || !SsStricmp(opt->name, "sql-mode")) &&
                    !SsStricmp(value, "off"))
                    continue;
                handle_backslashes(value, valbuf);
                sprintf(buf, "%s=%s\n", opt->name, valbuf);
                break;
            }
            default:
                continue;
        }

        if (!fwrite_string(file, buf)) {
            goto FINISH;
        }
    }

    errcode = 0;

FINISH:
    my_fclose(file, MYF(0));
    return errcode;
}
/* #endif   -- MYSQL_DYNAMIC_PLUGIN */

/*#***********************************************************************\
 *
 *              copy_mysql_files_related_to_solid
 *
 * Performs backup of Solid database.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     char* parameters,  in, use
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB return code
 */
static int copy_mysql_files_related_to_solid( const char* backup_dir )
{
        SDB_DBUG_ENTER("copy_mysql_files_related_to_solid");

        FILE *backup_cnf; /* file where the list of all backed up tables is written.*/
        {
            char backup_list_path[FN_REFLEN];
            strxmov( backup_list_path, backup_dir, "/", BACKUP_LIST_FILE_NAME, NULL );
            backup_cnf = my_fopen( backup_list_path, O_WRONLY|O_CREAT|O_TRUNC, MYF(0) );
        }

        MY_DIR *databases = my_dir( mysql_data_home, MYF(MY_DONT_SORT|MY_WANT_STAT) );
        if ( !databases ) {
            return 1;
        }

        /* fill list of all databases. */
        for ( uint d = 0; d < databases->number_off_files; ++d ) {

            /* skipping not directories, this directory and parent directory references. */
            if ( !MY_S_ISDIR(databases->dir_entry[d].mystat->st_mode) ||
                 MY_S_ISDIR(databases->dir_entry[d].mystat->st_mode) &&
                (!strcmp( databases->dir_entry[d].name, "." ) ||
                 !strcmp( databases->dir_entry[d].name, ".." )) ) {
                continue;
            }

            const char * const db = databases->dir_entry[d].name;

            ss_pprintf_1(("\t : database is %s.\n", db));

            char db_path[FN_REFLEN];
            strxmov( db_path, mysql_data_home, "/", db, NULL );

            MY_DIR *dirp = my_dir( db_path, MYF(MY_DONT_SORT|MY_WANT_STAT) );
            if ( !dirp ) {
                continue;
            }

            //List_iterator_fast<char> frmi(frm_files);
            //while((frm = frmi++)) {
            for ( uint i = 0; i < dirp->number_off_files; ++i ) {

                /* skipping this directory and parent directory references. */
                if ( MY_S_ISDIR(dirp->dir_entry[i].mystat->st_mode) &&
                    (!strcmp( dirp->dir_entry[i].name, "." ) ||
                     !strcmp( dirp->dir_entry[i].name, ".." )) ) {
                    continue;
                }

                char *afile = dirp->dir_entry[i].name;

                ss_pprintf_1(("\t : file is %s.\n", afile));

                char path[FN_REFLEN];
                strxmov( path, db_path, "/", afile, NULL );

                /* TODO check open mode: it is better to restrict other access. */
                File frm_file = my_open( path, O_RDONLY | O_SHARE, MYF(0) );

                if ( frm_file >= 0 ) {
                    ss_pprintf_1(("\t : file %s has been opened.\n", afile));

                    if ( true || is_soliddb_table( frm_file ) ) {

                        ss_pprintf_1(("\t : file %s is a SolidDB table and will be copied.\n", afile));

                        /* copying FRM file to the backup directory. */
                        char dstdir[FN_REFLEN];

                        strxmov( dstdir, backup_dir, "/", db, NULL );
                        ss_pprintf_2(("copy_mysql_files_related_to_solid: SolidDB FRM: %s\n", path));

                        if ( !copy_file( frm_file, dstdir, afile, "" ) ) {
                            my_close( frm_file, MYF(MY_WME) );
                            my_fclose( backup_cnf, MYF(MY_WME) );
                            SDB_DBUG_RETURN(1);
                        }

                        fprintf( backup_cnf, "%s.%s\n", db, afile );
                    }
                    my_close( frm_file, MYF(MY_WME) );
                }
            }
        }

        my_fclose( backup_cnf, MYF(MY_WME) );
        SDB_DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              do_backup
 *
 * Performs backup of Solid database.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     char* parameters,  in, use
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB return code
 */
#ifndef HAVE_SOLIDDB_BACKUP_NEW
static su_ret_t do_backup_old(
#else
static su_ret_t do_backup(
#endif
        rs_sysi_t* cd,
        char* parameters,
        su_err_t** p_errh )
{
        su_ret_t rc = SU_SUCCESS;
        dbe_db_t* soliddb = NULL;
        char* backup_dir = parameters;
        tb_connect_t* tc = NULL;
        char backup_dir_buf[FN_REFLEN];

        SDB_DBUG_ENTER("solid:do_backup");
        fprintf(stderr, " SolidDB: Backup Initiated\n");

        block_solid_DDL();
        fprintf(stderr, " SolidDB: All data changes currently blocked\n");

        soliddb = (dbe_db_t*)rs_sysi_db(cd);
        ss_assert( soliddb );

        if (!backup_dir || !*backup_dir) {
            SsFlatMutexLock(soliddb_backupdir_mutex);
            strncpy(backup_dir_buf, soliddb_backupdir, sizeof(backup_dir_buf) - 1);
            backup_dir = backup_dir_buf;
            SsFlatMutexUnlock(soliddb_backupdir_mutex);
        }

        if (!*backup_dir) {
            fprintf(stderr, " SolidDB: Error: Backup directory is not defined\n");
            rc = DBE_ERR_FAILED;
            goto epilogue;
        }

        if( (rc = dbe_db_backupcheck( soliddb, backup_dir, p_errh ) ) != DBE_RC_SUCC ) {
            ui_msg_message(BACKUP_MSG_FAILED_S, su_err_geterrstr(*p_errh));
            fprintf(stderr, " SolidDB: Error: Backup failed! Please check log for details\n");
            rc = DBE_ERR_FAILED;
            goto epilogue;
        }

        if( solid_clear_dir( backup_dir ) ) {
            goto epilogue;
        }

/*
#ifndef MYSQL_DYNAMIC_PLUGIN
        if( backup_config( backup_dir ) ) {
            goto epilogue;
        }
#endif
*/
        if( backup_config( backup_dir ) ) {
            goto epilogue;
        }

        /* adding a checkpoint record to the end of log */
        tc = tb_sysconnect_init( (tb_database_t*)rs_sysi_tabdb(cd) );

        if ( (rc = tb_createcheckpoint( tc, /*splitlog = */ TRUE )) != DBE_RC_SUCC ) {
            su_err_init( p_errh, rc );
            fprintf(stderr, " SolidDB: Checkpoint failed\n");
        }
        tb_sysconnect_done(tc);

        if ( rc != DBE_RC_SUCC ) {
            goto epilogue;
        }
        fprintf(stderr, " SolidDB: Checkpoint added to log\n");

        if ( (rc = dbe_db_backupstart(soliddb, backup_dir, FALSE, p_errh)) != DBE_RC_SUCC) {
            goto epilogue;
        }

        ui_msg_message(BACKUP_MSG_STARTED_SS, "directory", backup_dir );

        do {
            rc = dbe_db_backupadvance(soliddb, p_errh);
        } while(rc == DBE_RC_CONT);

        if (rc == DBE_RC_END) {
            rc = SU_SUCCESS;
        } else {
            ui_msg_message(BACKUP_MSG_FAILED_S, su_err_geterrstr(*p_errh));
            fprintf(stderr, " SolidDB: Backup failed\n");
            rc = DBE_ERR_FAILED;
        }

        dbe_db_backupstop(soliddb);

        if( rc != SU_SUCCESS ) {
            goto epilogue;
        }

        if( copy_mysql_files_related_to_solid( backup_dir ) ) {
            rc = DBE_ERR_FAILED;
            goto epilogue;
        }
        fprintf(stderr, " SolidDB: Backup of MySQL system files completed\n");

epilogue:
        unblock_solid_DDL();
        if ( rc == SU_SUCCESS || rc == DBE_RC_SUCC ) {
            ui_msg_message(BACKUP_MSG_COMPLETED);
        } else {
            if (*p_errh == NULL) {
                su_err_init(p_errh, rc);
            }

            ui_msg_message(BACKUP_MSG_START_FAILED_WITH_MSG_S, su_err_geterrstr(*p_errh));
        }
        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              do_performance_monitor
 *
 * Performs performance monitor of the database.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     char* parameters,  in, use
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB return code
 */
static su_ret_t do_performance_monitor(
        rs_sysi_t* cd,
        char* parameters,
        su_err_t** p_errh)
{
        uint interval;
        bool append = FALSE;
        bool raw = FALSE;
        char filename[256];
        char comment[256];
        bool pmon_clear = FALSE;
        bool pmon_start = FALSE;
        bool pmon_stop = FALSE;
        bool succp = FALSE;

        if (parameters == NULL) {
            return (DBE_ERR_FAILED);
        }

        su_pars_match_t m;
        su_pars_match_init( &m, parameters );

        if (su_pars_match_const(&m, (char *)"diff")) {
            if (su_pars_match_const(&m, (char *)"clear")) {
                pmon_clear = TRUE;
                succp = TRUE;
            } else if (su_pars_match_const(&m, (char *)"stop")) {
                pmon_stop = TRUE;
                succp = TRUE;
            } else if (su_pars_match_const(&m, (char *)"start")) {


                if (su_pars_get_filename(&m, filename, sizeof(filename))) {
                } else {
                    strcpy(filename, (char *)"pmondiff.out");
                }

                if (!su_pars_get_uint(&m, &interval)) {
                    interval = 1000;
                }

                if (su_pars_match_const(&m, (char *)"append")) {
                    append = TRUE;
                }

                raw = FALSE;

                if (su_pars_match_const(&m, (char *)"raw")) {
                    raw = TRUE;
                }

                if (!su_pars_get_stringliteral(&m, comment, sizeof(comment))) {
                    comment[0] = 0;
                }

                pmon_start = TRUE;
                succp = TRUE;
            }
        }

        if (su_pars_match_const(&m, (char *)"") && succp) {

            if (pmon_clear) {
                ss_pprintf_3(("do_performance_monitor:pmon clear\n"));
                tb_srv_pmondiff_clear(cd);

            } else if (pmon_stop) {
                ss_pprintf_3(("do_performance_monitor:pmon stop\n"));

                if (!tb_srv_pmondiff_stop(cd, p_errh)) {
                    return(DBE_ERR_FAILED);
                }

            } else if (pmon_start) {
                ss_pprintf_3(("do_performance_monitor:pmon start\n"));

                if (!tb_srv_pmondiff_start(cd, interval, filename, append, raw, comment, p_errh)) {
                    return(DBE_ERR_FAILED);
                }
            } else {
                return(DBE_ERR_FAILED);
            }

            return SU_SUCCESS;
        } else {
            return(DBE_ERR_FAILED);
        }
}
/*#***********************************************************************\
 *
 *              do_command
 *
 * Finds an implementation function for administrative command and executes it.
 * From the command parameters are extracted. Parameters are believed to follow
 * the command. Leading whitespaces are removed from the beginning of the parameters.
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, system info data type
 *     const char* cmd, in, use, admin command
 *     const char* param, in, use, parameters for admin command or NULL
 *     su_err_t**  p_errh, in out, NULL, error structure
 *
 * Return value : solidDB return code
 *
 * Globals used : current_thd
 */
static su_ret_t do_command(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        const char* cmd,
        const char* parameters,
        su_err_t**  p_errh)
{
        uint n_command = 0;
        const char* command = NULL;
        unsigned long len = 0;

        SDB_DBUG_ENTER("solid:do_command");

        len = strlen(cmd);

        for( n_command = 0; solid_admin_commands[n_command].command_str != NULL; ++n_command ) {

            const char * const this_command = solid_admin_commands[n_command].command_str;

            command = SsStristr(cmd, this_command);

            if ( command && command == cmd ) {

                if ( !parameters && len > strlen(this_command) ) {
                    parameters = cmd + strlen(this_command);
                    /* shoot whitespaces */
                    for( ; *parameters && strchr(SQL_WHITESPACES, *parameters); ++parameters );

                    if ( !*parameters ) {
                        parameters = NULL;
                    }
                }

                /* now really do the command */
                su_ret_t rc = solid_admin_commands[n_command].command_func(cd, (char *)parameters, p_errh);

                SDB_DBUG_RETURN(rc);
            }
        }

#if defined(SS_MYSQL_AC) && !defined(MYSQL_DYNAMIC_PLUGIN)
        {

            MYSQL_THD   thd = current_thd;
            Protocol*   protocol = (Protocol *)thd->protocol;
            List<Item>  field_list;

            sqlsystem_t* sqls = tb_sqls_init(cd);
            tb_sql_t*    sc;
            tb_sql_ret_t rc;
            rs_tval_t    *tval=NULL;
            bool succp;
            long value_rc;
            char* buf;
            size_t bufsize = 1024;
            char* sqlstr;
            char* templ = "ADMIN COMMAND '%s'";

            sqlstr = (char*)SsMemAlloc(strlen(cmd) + strlen(templ) + 2);
            SsSprintf(sqlstr, templ, cmd);

            //printf("%s\n", sqlstr);

            sc = tb_sql_init(cd, sqls, trans, sqlstr);
            ss_bassert(sc != NULL);
            succp = tb_sql_prepare(sc, p_errh);

            if (!succp) {
                /* For example if the user does not have suficient priviligies */
                tb_sql_done(sc);
                tb_sqls_done(cd, sqls);
                SsMemFree(sqlstr);
                SDB_DBUG_RETURN(DBE_ERR_FAILED);
            }

            ss_dassert(succp);
            rc = TB_SQL_CONT;

            while (rs_sysi_lockwait(cd) || (rc = tb_sql_execute_cont(sc, p_errh)) == TB_SQL_CONT) {
                continue;
            }

            succp = (rc == TB_SQL_SUCC);

            field_list.push_back(new Item_uint("RC", 21));
            field_list.push_back(new Item_empty_string("TEXT", 180));
            protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

            buf = (char*)SsMemAlloc(bufsize);

            do {
                tval = NULL;

                if (succp) {
                    rc = tb_sql_fetch_cont(sc, TRUE, &tval, NULL); /* maybe errh should be added to resultset in case of failure */
                }

                if (tval != NULL) {
                    succp = tb_sql_getcollong(sc, 0, &value_rc, p_errh);

                    if (succp) {
                        succp = tb_sql_getcolstr(sc, 1, buf, bufsize, p_errh);
                    }

                    if (succp) {
                        //printf("%ld, %s\n", value_rc, buf);
                        protocol->prepare_for_resend();
                        protocol->store((ulonglong)value_rc);
                        protocol->store(buf, system_charset_info);
                        protocol->write();
                    } else {
                        //printf("FAILED!\n");
                    }
                } else {
                    break;
                }
            } while (succp && (rs_sysi_lockwait(cd) || rc != TB_SQL_END));

            send_eof(thd);

            tb_sql_done(sc);
            tb_sqls_done(cd, sqls);
            SsMemFree(buf);
            SsMemFree(sqlstr);

            SDB_DBUG_RETURN(SU_SUCCESS);
        }
#else /* SS_MYSQL_AC */
        SDB_DBUG_RETURN(DBE_ERR_FAILED);
#endif
}

/*#***********************************************************************\
 *
 *              check_admin_command
 *
 * Check wheather user has issued a admin command e.g. backup
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use, system info data type
 *     tb_trans_t* trans, in, use, transaction
 *     rs_ttype_t* ttype, in, use, tuple type
 *     rs_tval_t*  tval, in, use, tuple value
 *     su_err_t**  p_errh, in out, NULL, error structure
 *
 * Return value : solidDB return code
 *
 * Globals used :
 */
static su_ret_t check_admin_command(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_ttype_t* ttype,
        rs_tval_t* tval,
        su_err_t** p_errh)
{
        char* cmd;
        su_ret_t rc = SU_SUCCESS;
        rs_aval_t* cmd_val;
        rs_aval_t* params_val;
        char *params;
        rs_atype_t* cmd_type = rs_ttype_sql_atype(cd, ttype, 0);
        rs_atype_t* params_type = rs_ttype_sql_atype( cd, ttype, 1 );
        rs_datatype_t pdatatype;

        ss_pprintf_3(("check_admin_command\n"));

        if ( (pdatatype = rs_atype_datatype(cd, cmd_type)) != RSDT_CHAR ) {
            ss_pprintf_3(("check_admin_command: datatype of 1st field is wrong: %d\n", pdatatype));
            goto admin_command_error;
        }

        if ( (pdatatype = rs_atype_datatype( cd, params_type )) != RSDT_CHAR) {
            ss_pprintf_3(("check_admin_command: datatype of 2nd field is wrong: %d\n", pdatatype));
            goto admin_command_error;
        }

        cmd_val = rs_tval_sql_aval(cd, ttype, tval, 0);

        if ( rs_aval_isnull(cd, cmd_type, cmd_val) ) {
            ss_pprintf_3(("check_admin_command: COMMAND should not be null\n"));
            goto admin_command_error;
        }

        cmd = rs_aval_getasciiz(cd, cmd_type, cmd_val);

        params_val = rs_tval_sql_aval( cd, ttype, tval, 1 );
        params = NULL;

        if ( !rs_aval_isnull( cd, params_type, params_val ) ) {
            params = rs_aval_getasciiz( cd, params_type, params_val );
        }

        //ss_pprintf_100(("check_admin_command: backup parameters are: [%s]\n", params));

        ss_pprintf_3(("ADMIN COMMAND = %s\n", cmd));

        rc = do_command(cd, trans, cmd, params, p_errh);

        if (rc == SU_SUCCESS || p_errh != NULL) {
            return (rc);
        }

admin_command_error:
        ss_pprintf_3(("check_admin_command:wrong command\n"));
        su_err_init(p_errh, DBE_ERR_FAILED);

        return DBE_ERR_FAILED;
}

/*#***********************************************************************\
 *
 *              soliddb_check_admin_command
 *
 * Check admin command given by the user
 *
 * Parameters :
 *
 *     THD*      thd, in, use, thread data
 *     set_var*  var, in, use, variable value from MySQL
 *
 * Return value : 0 or -1 (error)
 *
 * Globals used :
 */
int soliddb_check_admin_command(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           save,	        /*!< in: immediate result
                                                        from check function */
        struct st_mysql_value*          value)          /*!< out: where the
                                                        formal string goes */
{
        su_ret_t rc = SU_SUCCESS;
        su_err_t* errh = NULL;
        SOLID_CONN* con = NULL;
#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        length= sizeof(buff);
        soliddb_admin_command = (char *)value->val_str(value, buff, &length);
#else
        soliddb_admin_command = (char *)var->value->name;
#endif

        DBUG_ENTER("soliddb_check_admin_command");

/*
#if MYSQL_VERSION_ID >= 50100
*/
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
/*
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif */

        CHK_CONN(con);

        ss_pprintf_1(("Admin command = %s\n", soliddb_admin_command ? soliddb_admin_command: "NULL" ));
        fprintf(stderr, " SolidDB: Admin command = %s\n",soliddb_admin_command);

        if (soliddb_admin_command) {
            rc = do_command(con->sc_cd, con->sc_trans, soliddb_admin_command, NULL, &errh);
        } else {
            rc = DBE_ERR_FAILED;
        }

        if (rc != SU_SUCCESS) {
            if (errh) {
                sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
                su_err_done(errh);
                errh = NULL;
            }
            fprintf(stderr, " SolidDB: Admin command '%s' failed\n", soliddb_admin_command);

            DBUG_RETURN(-1);
        }
        fprintf(stderr, " SolidDB: Admin command '%s' complete\n",soliddb_admin_command);

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_update_admin_command
 *
 * Update admin command string given by the user
 *
 * Parameters :
 *
 *     THD*      thd, in, use, thread data
 *     set_var*  var, in, use, variable value from MySQL
 *
 * Return value : 0
 *
 * Globals used :
 */
void soliddb_update_admin_command(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           value,          /*!< out: where the
                                                        formal string goes */
        const void*                     save)           /*!< in: immediate result
                                                        from check function */
{
#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        DBUG_ENTER("soliddb_update_admin_command");
        length= sizeof(buff);
        soliddb_admin_command = (char *)value;
        DBUG_VOID_RETURN;
#else
        DBUG_ENTER("soliddb_update_admin_command");
        soliddb_admin_command = (char *)var->value->name;
        DBUG_RETURN(0);
#endif

}

/*#***********************************************************************\
 *
 *              soliddb_set_auto_increment
 *
 * Set auto increment value given from MySQL to auto increment sequence
 * if the given value is bigger than the old value
 *
 * Parameters :
 *
 *    rs_sysi_t*  cd, in, use, system information
 *    tb_trans_t* trans, in, use, transaction
 *    su_err_t**  p_errh, in, use, error structure
 *    long        seq_id, in, use, auto increment sequence number
 *    ulonglong   mysql_value, in, use, MySQL value
 *
 * Return value : TRUE if successfull, FALSE othervize
 *
 * Globals used : -
 */
static inline bool solid_set_auto_increment(
        rs_sysi_t*  cd,
        tb_trans_t* trans,
        su_err_t**  p_errh,
        long        seq_id,
        ulonglong   mysql_value)
{
        rs_atype_t* atype;
        rs_aval_t*  aval;
        ulonglong nr;
        ss_int8_t i8;
        bool succp = TRUE;
        solid_bool finished=FALSE;

        SS_PUSHNAME("solid_set_auto_increment");
        atype = rs_atype_initbigint(cd);
        aval = rs_aval_create(cd, atype);

        succp = tb_seq_current(cd, trans, seq_id,
                               FALSE, atype, aval, &finished, p_errh);

        ss_dassert(finished == TRUE);

        if (succp) {
            i8 = rs_aval_getint8(cd, atype, aval);
            nr = SsInt8GetNativeUint8(i8);

            if (mysql_value > nr) {
                SsInt8SetNativeUint8(&i8, mysql_value);
                rs_aval_setint8_ext(cd, atype, aval, i8, NULL);

                succp = tb_seq_set(cd, trans, seq_id, FALSE, atype, aval,
                                   &finished, p_errh);

                ss_dassert(finished == TRUE);
            }
        }

        rs_aval_free(cd, atype, aval);
        rs_atype_free(cd, atype);

        SS_POPNAME;

        return (succp);
}

/*#***********************************************************************\
 *
 *              ::write_row
 *
 * write_row() inserts a row. No extra() hint is given currently if a bulk load
 * is happeneding. buf() is a byte array of data. You can use the field
 * information to extract the data from the native byte array type.
 * Example of this would be:
 * for (Field **field=table->field ; *field ; field++)
 * {
 *   ...
 * }
 *
 * See ha_tina.cc for an example of extracting all of the data as strings.
 * ha_berekly.cc has an example of how to store it intact by "packing" it
 * for ha_berkeley's own native storage type.
 *
 * See the note for update_row() on auto_increments and timestamps. This
 * case also applied to write_row().
 *
 * Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
 * sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.
 *
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*   buf, in, use, values
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* thd
 */
int ha_soliddb::write_row(SS_MYSQL_ROW * buf)
{
        int rc;
        bool succp = TRUE;
        Field** fieldptr;
        Field* field;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_atype_t* atype;
        rs_aval_t* aval;
        int ano;
        int nattrs;
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        MYSQL_THD thd;
        su_err_t* errh = NULL;
        solid_bool finished = FALSE;
        bool committed = FALSE;
        bool auto_inc_used = FALSE;
        su_ret_t suret = DBE_RC_SUCC;
        int sql_command;

        DBUG_ENTER("ha_solid::write_row");
        ss_pprintf_1(("ha_solid::write_row\n"));
        SS_PUSHNAME("ha_soliddb::write_row");

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

        if (solid_table == NULL) {
            rc = ha_solid_mysql_error(thd, con->sc_errh, 0);
            ss_pprintf_1(("ha_solid::table not found\n"));
            SS_POPNAME;
            DBUG_RETURN(rc);
        }

        if (rs_relh_issysrel(cd, solid_table->st_rsrelh)) {
            SS_POPNAME;
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (rs_relh_isaborted(cd, solid_table->st_rsrelh)) {
            SS_POPNAME;
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }
        if (tb_trans_dbtrx(cd, trans) == NULL) {
            int rc;
            ss_dassert(tb_trans_getdberet(cd, trans) != SU_SUCCESS);
            rc = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
            SS_POPNAME;
            DBUG_RETURN(rc);
        }

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_write_count);
#else
        statistic_increment(thd->status_var.ha_write_count, &LOCK_status);
#endif

        if (con->sc_err_tableid == 0) {
#if MYSQL_VERSION_ID >= 50100
            ss_dprintf_1(("set err table to %s\n",table->s->path.str));
#else
            ss_dprintf_1(("set err table to %s\n",table->s->path));
#endif
            con->sc_err_tableid = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);
        }

        ttype = solid_table->st_ttype;
        tval = solid_table->st_tval;
        nattrs = rs_ttype_sql_nattrs(cd, ttype);

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        ss_pprintf_2(("ha_solid::write_row, table %s\n",
                                         rs_relh_name(cd, solid_table->st_rsrelh)));

        if (table->next_number_field && buf == table->record[0]) {
            /* This is the case where the table has an
            auto-increment column */

            if (update_auto_increment()) {
                succp = FALSE;
                rs_error_create(&errh, DBE_ERR_TRXTIMEOUT);
            }

            auto_inc_used = TRUE;
       }

        /* If we have a timestamp column, update it to the current time */
        if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT) {
            table->timestamp_field->set_time();
        }

        if (succp) {
            for (fieldptr = table->field, ano = 0; *fieldptr != NULL; fieldptr++, ano++) {
                int phys_ano;

                field = *fieldptr;
                phys_ano = rs_ttype_quicksqlanotophys(cd, ttype, ano);
                ss_dassert(phys_ano != RS_ANO_NULL);
                atype = rs_ttype_atype(cd, ttype, phys_ano);
                aval = rs_tval_aval(cd, ttype, tval, phys_ano);

                if (field->null_ptr != NULL && (field->null_bit & *field->null_ptr)) {
                    ss_pprintf_1(("ha_solid::write_row:NULL\n"));
                    rs_aval_setnull(cd, atype, aval);
                } else {
                    succp = set_mysql_field_to_aval_or_dynva(
                                thd,
                                table,
                                cd,
                                atype,
                                aval,
                                NULL,
                                field,
#if MYSQL_VERSION_ID >= 50100
                                (SS_MYSQL_ROW *)(buf + field->offset(field->table->record[0])),
#else
                                (SS_MYSQL_ROW *)(buf + field->offset()),
#endif
                                FALSE,
                                &errh);

                    if (!succp) {
                        break;
                    }
                }

                ss_poutput_2(ha_print_aval(cd, atype, aval, (char *)"ha_solid::write_row"));
            }

            ss_poutput_2(rs_tval_print(cd, ttype, tval));
        }

        if (succp) {
            if (strcmp(rs_relh_name(cd, solid_table->st_rsrelh), "SOLIDDB_ADMIN_COMMANDS") == 0) {
                suret = check_admin_command(cd, trans, ttype, tval, &errh);
            } else {
                do {
                    suret = dbe_rel_insert(
                                tb_trans_dbtrx(cd, trans),
                                solid_table->st_rsrelh,
                                tval,
                                &errh);
                } while (rs_sysi_lockwait(cd) || suret == DBE_RC_WAITLOCK);
            }

            succp = (suret == DBE_RC_SUCC);
        }

        if (succp && auto_inc_used && table->next_number_field->val_int() > 0) {
            succp = solid_set_auto_increment(cd, trans, &errh,
                                             rs_relh_readautoincrement_seqid(cd, solid_table->st_rsrelh),
                                             table->next_number_field->val_int());
        }

        sql_command = soliddb_sql_command(thd);

        /* ALTER TABLE is committed at every 10000 rows.*/
        if (succp &&
            rs_relh_reltype(cd, solid_table->st_rsrelh) != RS_RELTYPE_MAINMEMORY &&
            (sql_command == SQLCOM_ALTER_TABLE ||
             sql_command == SQLCOM_OPTIMIZE ||
             sql_command == SQLCOM_CREATE_INDEX ||
             sql_command == SQLCOM_DROP_INDEX)
            && num_write_rows >= 10000) {

            num_write_rows = 0;

            tb_trans_setcanremovereadlevel_withwrites(cd, trans, TRUE);

            if (solid_relcur && solid_relcur->sr_relcur) {
                dbe_cursor_setisolation_transparent(solid_relcur->sr_relcur, TRUE);
            }

            /* Transaction commit */

            do {
                succp = tb_trans_commit(cd, trans, &finished, &errh);
            } while (rs_sysi_lockwait(cd) || !finished);

            /* Start a new transaction */
            if (succp) {
                tb_trans_beginif(con->sc_cd, con->sc_trans);
                rnd_mustinit = TRUE;
            }

            committed = TRUE;
        }

        num_write_rows++;
	stats.records++;

        if (succp && suret == DBE_RC_SUCC) {
            succp = TRUE;
        } else {
            succp = FALSE;
            ss_dassert(errh != NULL);
        }

        /* If we should ignore duplicate keys we need to do a
           statement commit to get those errors. */

        if (succp && !committed && solid_ignore_duplicate(thd)) {
            if (tb_trans_isstmtactive(cd, trans)) {
                do {
                    succp = tb_trans_stmt_commit(cd, trans, &finished, &errh);
                } while (rs_sysi_lockwait(cd) || !finished);
                /* Here we take error key from dbe transaction to the connection */
                con->sc_errkey = tb_trans_geterrkey(cd, trans);
            }
        }

        if (!succp &&
            (sql_command == SQLCOM_REPLACE || sql_command == SQLCOM_REPLACE_SELECT)) {
            /* For REPLACE we can store created tval to connection for update */
            if (con->sc_tval != NULL) {
                /* Clear old values that were not used. */
                rs_tval_free(cd, con->sc_ttype, con->sc_tval);
                rs_ttype_free(cd, con->sc_ttype);
            }

            con->sc_ttype = rs_ttype_copy(cd, ttype);
            con->sc_tval = rs_tval_copy(cd, ttype, tval);
        }

        rs_tval_reset(cd, ttype, tval);

        if (!succp) {
            rc = ha_solid_mysql_error(thd, errh, 0);
            su_err_done(errh);

        } else {
            rc = 0;
        }

        ss_pprintf_1(("ha_solid::write_row:return %d\n", rc));
        SS_POPNAME;

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ha_solid_cascade
 *
 * Do a foreign key cascade operations for a update or delete
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     tb_trans_t* trans, in, use
 *     rs_relh_t*  solid_table->st_rsrelh, in, use
 *     solid_bool* flags, in, use or NULL in delete
 *     rs_tval_t*  old_tval, in, use
 *     rs_tval_t*  new_tval, in, use or NULL in delete
 *     rs_ttype_t* ttype, in, use
 *     su_err_t**  p_errh, in out, NULL
 *
 * Return value : solidDB error code or DBE_RC_SUCC
 *
 * Globals used :
 */
static su_ret_t ha_solid_cascade_cascading(
        SOLID_CONN* con,
        solid_relcur_t* cur,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        solid_bool* flags,
        rs_tval_t*  old_tval,
        rs_tval_t*  new_tval,
        rs_ttype_t* ttype,
        bool cascading,
        su_err_t**  errh)
{
        uint i = 0;
        rs_sysi_t* cd = NULL;
        su_pa_t *keys = NULL;

        CHK_CONN(con);

        cd = con->sc_cd;
        ss_dassert(cd != NULL);

        ss_dprintf_3(("ha_solid_cascade\n"));
        SS_PUSHNAME("ha_solid_cascade");

        keys = rs_relh_refkeys(cd, relh);

        if (cur->sr_casc_states == NULL) {
            ss_dprintf_4(("ha_solid_cascade:cur->sr_casc_states == NULL\n"));
            cur->sr_casc_states = su_pa_init();
        }

        su_pa_do(keys, i) {
           uint rc = 0;
           rs_key_t *key = NULL;
           tb_trans_keyaction_state_t *casc_state = NULL;

           if (su_pa_indexinuse(cur->sr_casc_states, i)) {
               casc_state = (tb_trans_keyaction_state_t *)su_pa_getdata(cur->sr_casc_states, i);
           }

           key = (rs_key_t *)su_pa_getdata(keys, i);

           if ((rs_key_delete_action(cd, key) == SQL_REFACT_CASCADE) != cascading) {
               continue;
           }

           if (rs_key_type(cd, key) != RS_KEY_PRIMKEYCHK) {
               continue;
           }

           ss_dprintf_4(("ha_solid_cascade:tb_ref_keyaction, i=%d\n", i));

           do {
               rc = tb_ref_keyaction(cd, trans,
                                     key, ttype,
                                     flags,
                                     old_tval, new_tval,
                                     &casc_state,
                                     errh);
           } while (rc == TB_CHANGE_CONT);

           if (rc != TB_CHANGE_SUCC) {
               con->sc_errkey = key;

               if (*errh == NULL) {
                   rs_error_create_key(errh, DBE_ERR_CHILDEXIST_S, key);
               }

               rc = DBE_ERR_CHILDEXIST_S;
           }

           if (casc_state != NULL && !su_pa_indexinuse(cur->sr_casc_states, i)) {
               su_pa_insertat(cur->sr_casc_states, i, casc_state);
           }

           if (rc == DBE_ERR_CHILDEXIST_S) {
               SS_POPNAME;
               return DBE_ERR_CHILDEXIST_S;
           }
       }

       SS_POPNAME;
       return DBE_RC_SUCC;
}

/*#***********************************************************************\
*              ha_solid_cascade
*
* Do a foreign key cascade operations for a update or delete, in order:
* cascading deletes go before the cascading updates (set null/default).
*
* Parameters :
*
* Return value :
*
* Globals used : -
*
*/
static su_ret_t ha_solid_cascade(
        SOLID_CONN* con,
        solid_relcur_t* cur,
        tb_trans_t* trans,
        rs_relh_t*  relh,
        solid_bool* flags,
        rs_tval_t*  old_tval,
        rs_tval_t*  new_tval,
        rs_ttype_t* ttype,
        su_err_t**  errh)
{
        su_ret_t rc;
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        if (!tb_trans_get_foreign_key_checks(trans)) {
            return DBE_RC_SUCC;
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        

        rc = ha_solid_cascade_cascading(con, cur, trans, relh, flags,
                                        old_tval, new_tval, ttype, true, errh);
        if (rc != DBE_RC_SUCC) {
            return rc;
        }

        rc = ha_solid_cascade_cascading(con, cur, trans, relh, flags,
                                        old_tval, new_tval, ttype, false, errh);
        return rc;
}

/*#***********************************************************************\
 *
 *              ::update_row
 *
 * Yes, update_row() does what you expect, it updates a row. old_data will have
 * the previous row record in it, while new_data will have the newest data in
 * it.
 *
 * Keep in mind that the server can do updates based on ordering if an ORDER BY
 * clause was used. Consecutive ordering is not guaranteed.
 * Currently new_data will not have an updated auto_increament record, or
 * and updated timestamp field. You can do these for example by doing these:
 * if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
 *   table->timestamp_field->set_time();
 * if (table->next_number_field && record == table->record[0])
 *   update_auto_increment();
 *
 * Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
 *
 * Parameters :
 *
 *     const SS_MYSQL_ROW*  old_data, in, use, old data
 *     SS_MYSQL_ROW*        new_data, in, use, new data
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* thd
 */
int ha_soliddb::update_row(
        const SS_MYSQL_ROW* old_data,
        SS_MYSQL_ROW* new_data)
{
        int rc;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        rs_atype_t* atype;
        rs_aval_t* aval;
        Field** fieldptr;
        Field* field;
        int ano;
        solid_bool* selflags;
        int nattrs;
        int physnattrs;
        MYSQL_THD thd;
        SOLID_CONN*   con;
        bool succp = TRUE;
        su_err_t* errh = NULL;
        solid_bool finished = FALSE;
        su_ret_t suret = SU_SUCCESS;
        bool free_tval = FALSE;
        int sql_command;

        DBUG_ENTER("ha_soliddb::update_row");
        ss_pprintf_1(("ha_solid::update_row\n"));
        SS_PUSHNAME("ha_soliddb::update_row");

        thd = current_thd;

        ss_dassert(solid_relcur != NULL);
        ss_dassert(solid_relcur->sr_relcur != NULL);
        CHK_RELCUR(solid_relcur);

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);
        cd = solid_relcur->sr_cd;
        ss_dassert(cd != NULL);
        ss_dassert(solid_table != NULL);

        if (rs_relh_issysrel(cd, solid_table->st_rsrelh)) {
            SS_POPNAME;
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (rs_relh_isaborted(cd, solid_table->st_rsrelh)) {
            SS_POPNAME;
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (con->sc_err_tableid == 0) {
#if MYSQL_VERSION_ID >= 50100
            ss_dprintf_1(("set err table to %s\n",table->s->path.str));
#else
            ss_dprintf_1(("set err table to %s\n",table->s->path));
#endif
            con->sc_err_tableid = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);
        }

        trans = solid_relcur->sr_trans;
        ttype = solid_relcur->sr_ttype;

        ss_dassert(ttype != NULL);

        if (tb_trans_dbtrx(cd, trans) == NULL) {
            int rc;
            ss_dassert(tb_trans_getdberet(cd, trans) != SU_SUCCESS);
            rc = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
            SS_POPNAME;
            DBUG_RETURN(rc);
        }

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_update_count);
#else
        statistic_increment(thd->status_var.ha_update_count, &LOCK_status);
#endif
        sql_command = soliddb_sql_command(thd);

        if (sql_command != SQLCOM_REPLACE && sql_command != SQLCOM_REPLACE_SELECT) {
            ss_dassert(con->sc_tval == NULL);
            tval = solid_table->st_tval;
            free_tval = FALSE;
        } else {
            tval = con->sc_tval;
            ss_dassert(tval != NULL);
            ss_dassert(rs_ttype_issame(cd, ttype, con->sc_ttype));
            free_tval = TRUE;
        }

        physnattrs = rs_ttype_nattrs(cd, ttype);
        selflags = (solid_bool*)SsMemCalloc(physnattrs, sizeof(selflags[0]));
        nattrs = rs_ttype_sql_nattrs(cd, ttype);

        ss_pprintf_2(("ha_solid::update_row, table %s\n", rs_relh_name(cd, solid_table->st_rsrelh)));

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

#if MYSQL_VERSION_ID >= 50100
        my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
#endif

        if (sql_command != SQLCOM_REPLACE && sql_command != SQLCOM_REPLACE_SELECT) {
            /* If we have a timestamp column, update it to the current time */
            if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE) {
                table->timestamp_field->set_time();
            }

            for (fieldptr = table->field, ano = 0; *fieldptr != NULL; fieldptr++, ano++) {
                field = *fieldptr;

                if (succp && solid_relcur->sr_usedfields[ano]) {
                    int phys_ano;
                    phys_ano = rs_ttype_quicksqlanotophys(cd, ttype, ano);
                    atype = rs_ttype_atype(cd, ttype, phys_ano);
                    aval = rs_tval_aval(cd, ttype, tval, phys_ano);
                    selflags[phys_ano] = TRUE;
                    ss_dprintf_2(("ha_solid::update_row, ano=%d, aname=%s\n", phys_ano, rs_ttype_aname(cd, ttype, phys_ano)));

                    if (field->null_ptr != NULL && (field->null_bit & *field->null_ptr)) {
                        ss_pprintf_1(("ha_solid::update_row:NULL\n"));
                        rs_aval_setnull(cd, atype, aval);
                    } else {
                        succp = set_mysql_field_to_aval_or_dynva(
                                thd,
                                table,
                                cd,
                                atype,
                                aval,
                                NULL,
                                field,
#if MYSQL_VERSION_ID >= 50100
                                (SS_MYSQL_ROW *)(new_data + field->offset(field->table->record[0])),
#else
                                (SS_MYSQL_ROW *)(new_data + field->offset()),
#endif
                                FALSE,
                                &errh);

                        if (!succp) {
                            break;
                        }
                    }
                    ss_poutput_2(ha_print_aval(cd, atype, aval, (char *)"ha_solid::update_row"));
                }
            }
        } else {
            for (fieldptr = table->field, ano = 0; *fieldptr != NULL; fieldptr++, ano++) {
                field = *fieldptr;

                if (solid_relcur->sr_usedfields[ano]) {
                    int phys_ano;
                    phys_ano = rs_ttype_quicksqlanotophys(cd, ttype, ano);
                    selflags[phys_ano] = TRUE;
                }
            }
        }

        ss_poutput_2(rs_tval_print(cd, ttype, tval));

        if (succp) {
            do {
                suret = dbe_cursor_update(
                            solid_relcur->sr_relcur,
                            solid_relcur->sr_fetchtval,
                            tb_trans_dbtrx(cd, trans),
                            solid_table->st_rsrelh,
                            selflags,
                            tval,
                            NULL,
                            &errh);
            } while (rs_sysi_lockwait(cd) || suret == DBE_RC_WAITLOCK);

            succp = (suret == DBE_RC_SUCC);
        }

        if (succp && table->found_next_number_field && table->found_next_number_field->val_int() > 0) {
            succp = solid_set_auto_increment(cd, trans, &errh,
                                             rs_relh_readautoincrement_seqid(cd, solid_table->st_rsrelh),
                                             table->found_next_number_field->val_int());
        }

        if (succp) {
            suret = ha_solid_cascade(con, solid_relcur, trans, solid_table->st_rsrelh,
                                         selflags, solid_relcur->sr_fetchtval,
                                         tval, ttype, &errh);
        }

#if MYSQL_VERSION_ID >= 50100
        dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

        SsMemFree(selflags);

        if (free_tval) {
            ss_dassert(tval == con->sc_tval);
            rs_tval_free(cd, ttype, tval);
            rs_ttype_free(cd, con->sc_ttype);
            con->sc_tval = NULL;
        } else {
            ss_dassert(con->sc_tval == NULL);
            rs_tval_reset(cd, ttype, tval);
        }

        if (succp && suret == DBE_RC_SUCC) {
            succp = TRUE;
        } else {
            succp = FALSE;
        }

        /* If we should ignore duplicate keys we need to do a
           statement commit to get those errors and pass them to
           MySQL. Note that in replace we should not ignore duplicate
           key errors at this stage. */

        if (succp && solid_ignore_duplicate_update(thd)) {

            do {
                succp = tb_trans_stmt_commit(cd, trans, &finished, &errh);
            } while (rs_sysi_lockwait(cd) || !finished);

            /* Ignore errors */
            if (!succp) {
                succp = TRUE;
                su_err_done(errh);
            }
        }

        if (succp) {
            rc = 0;
        } else {
            ss_dassert(errh != NULL);
            rc = ha_solid_mysql_error(thd, errh, 0);
            su_err_done(errh);
        }

        ss_pprintf_2(("ha_solid::update_row:rc=%d\n", rc));
        SS_POPNAME;

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::delete_row
 *
 * This will delete a row. buf will contain a copy of the row to be deleted.
 * The server will call this right after the current row has been called (from
 * either a previous rnd_nexT() or index call).
 * If you keep a pointer to the last row or can access a primary key it will
 * make doing the deletion quite a bit easier.
 * Keep in mind that the server does no guarentee consecutive deletions. ORDER BY
 * clauses can be used.
 *
 * Called in sql_acl.cc and sql_udf.cc to manage internal table information.
 * Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select it is
 * used for removing duplicates while in insert it is used for REPLACE calls.
 *
 *
 * Parameters :
 *
 *     const SS_MYSQL_ROW* buf, in, use, deleted row
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* thd
 */
int ha_soliddb::delete_row(const SS_MYSQL_ROW* buf)
{
        int rc;
        su_ret_t suret;
        rs_sysi_t* cd;
        tb_trans_t* trans;
        su_err_t* errh = NULL;
        MYSQL_THD thd;
        SOLID_CONN* con;

        DBUG_ENTER("ha_soliddb::delete_row");
        ss_pprintf_1(("ha_solid::delete_row\n"));
        ss_dassert(solid_relcur != NULL);
        ss_dassert(solid_relcur->sr_relcur != NULL);
        CHK_RELCUR(solid_relcur);

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        cd = solid_relcur->sr_cd;
        trans = solid_relcur->sr_trans;

        if (rs_relh_issysrel(cd, solid_table->st_rsrelh)) {
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (rs_relh_isaborted(cd, solid_table->st_rsrelh)) {
            DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (tb_trans_dbtrx(cd, trans) == NULL) {
            int rc;
            ss_dassert(tb_trans_getdberet(cd, trans) != SU_SUCCESS);
            rc = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
            DBUG_RETURN(rc);
        }

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_delete_count);
#else
        statistic_increment(thd->status_var.ha_delete_count, &LOCK_status);
#endif

        if (con->sc_err_tableid == 0) {
#if MYSQL_VERSION_ID >= 50100
            ss_dprintf_1(("set err table to %s\n",table->s->path.str));
#else
            ss_dprintf_1(("set err table to %s\n",table->s->path));
#endif
            con->sc_err_tableid = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);
        }

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        ss_pprintf_2(("ha_solid::delete_row, table %s\n", rs_relh_name(cd, solid_table->st_rsrelh)));

        do {
            suret = dbe_cursor_delete(
                        solid_relcur->sr_relcur,
                        solid_relcur->sr_fetchtval,
                        tb_trans_dbtrx(cd, trans),
                        solid_table->st_rsrelh,
                        &errh);
        } while (rs_sysi_lockwait(cd) || suret == DBE_RC_WAITLOCK);

        if (suret == DBE_RC_SUCC) {
            suret = ha_solid_cascade(con, solid_relcur, trans, solid_table->st_rsrelh,
                                     NULL, solid_relcur->sr_fetchtval,
                                     NULL, solid_relcur->sr_ttype, &errh);
        }

        if (suret == DBE_RC_SUCC || su_err_geterrcode(errh) == DBE_ERR_NOTFOUND) {
            rc = 0;
        } else {
            rc = ha_solid_mysql_error(thd, errh, 0);
        }

        if (errh != NULL) {
            su_err_done(errh);
        }

	stats.records--;

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              solid_resolve_key
 *
 * Resolve solidDB key from MySQL key
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     rs_relh_t*  rsrelh, in, use, solidDB table relation
 *     KEY*        key_info, in, use, MySQL key definition
 *
 * Return value :
 *
 *     rs_key_t*, solidDB key definition or NULL
 *
 * Globals used :
 */
static rs_key_t* solid_resolve_key(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        KEY* key_info)
{
        rs_key_t* key;
        rs_entname_t en;

        CHECK_RELH(relh);

        if (key_info == NULL) {
            ss_dprintf_3(("solid_resolve_key:key_info == NULL, use clusterkey\n"));
            key = rs_relh_clusterkey(cd, relh);
            ss_dassert(key != NULL);
        } else {
            char* keyname;

            keyname = foreignkey_name_from_mysql_to_solid(cd, relh, key_info->name);

            if (!rs_relh_hasrefkey(cd, relh, keyname)) {
                SsMemFree(keyname);

            keyname = (char *)SsMemAlloc(strlen(key_info->name)+32);
            keyname_from_mysql_to_solid(cd, relh, key_info->name, keyname);
            }

            rs_entname_initbuf(&en, NULL, rs_relh_schema(cd, relh), keyname);
            key = rs_relh_keybyname(cd, relh, &en);

            if (key == NULL) {
                key = rs_relh_clusterkey(cd, relh);
            }

            ss_dassert(key != NULL);
            ss_dprintf_3(("solid_resolve_key:key %s\n", keyname));
            SsMemFree(keyname);
        }

        return(key);
}

/*#***********************************************************************\
 *
 *              solid_key_to_mysqlkeyidx
 *
 * Resolve solidDB key from MySQL key
 *
 * Parameters :
 *
 *     rs_sysi_t*  cd, in, use
 *     rs_relh_t*  rsrelh, in, use, solidDB table relation
 *     rs_key_t*   key, in, use, Solid key definition
 *
 * Return value :
 *
 *     int, MySQL key index or -1
 *
 * Globals used :
 */
static int solid_key_to_mysqlkeyidx(
        rs_sysi_t* cd,
        rs_relh_t* relh,
        ulong solidkeyid)
{
        rs_ano_t i;
        void* data;
        void* kkey;
        rs_key_t* key;
        int offset = 0;

        CHECK_RELH(relh);

        su_pa_t* keys = rs_relh_keys(cd, relh);
        su_pa_do_get(keys, i, kkey) { // relh->rh_key_pa
            key = (rs_key_t*)kkey;

            if (rs_key_id(cd, (rs_key_t*)key) == solidkeyid) {
                ss_dassert(i + offset >= 0);
                return i + offset;
            } else if(rs_key_refkeys(cd, key)) {
                rs_ano_t j = 0;
                rs_key_t* refkey;

                su_pa_do_get(rs_key_refkeys(cd, key), j, data) {

                    refkey = (rs_key_t*)data;

                    if (rs_key_id(cd, refkey) == solidkeyid) {
                        ss_dassert(i + offset >= 0);
                        return (i + offset);
                    }
                }
            } else if (rs_key_issyskey(cd, key) &&
                    !rs_key_isprimary(cd,key)) {
                --offset;
            }
        }

        return -1;
}

/*#***********************************************************************\
 *
 *              relcur_checkreuse
 *
 * Check if we can reuse existing relcur.
 *
 * Parameters :
 *
 *     SOLID_CONN*       con, in, use, solid connection
 *     solid_relcur_t*   old_relcur, in, use, existing relation cursor
 *     rs_relh_t*        rsrelh, in, use, relation
 *     uint              index, in, use, index
 *     dbe_cursor_type_t cursor_type, in, use, new cursor type
 *
 * Return value :
 *
 *     solid_new_cursor_t, new cursor type
 *
 * Globals used :
 */
static inline solid_new_cursor_t relcur_checkreuse(
        SOLID_CONN* con,
        solid_relcur_t* old_relcur,
        rs_relh_t* rsrelh,
        uint index,
        dbe_cursor_type_t cursor_type,
        bool extra_keyread)
{
        solid_new_cursor_t new_cursor_type;

        if (old_relcur == NULL
            || old_relcur->sr_rsrelh != rsrelh
            || cursor_type != old_relcur->sr_cursor_type
            || old_relcur->sr_extra_keyread != extra_keyread
            || (extra_keyread && old_relcur->sr_index != index)) {
            ss_pprintf_2(("relcur_checkreuse:SOLID_NEW_CURSOR_CREATE\n"));
            new_cursor_type = SOLID_NEW_CURSOR_CREATE;
        } else if (old_relcur->sr_index != index) {
            ss_pprintf_2(("relcur_checkreuse:SOLID_NEW_CURSOR_REUSE\n"));
            ss_dassert(old_relcur->sr_con == con);
            new_cursor_type = SOLID_NEW_CURSOR_REUSE;
        } else {
            ss_pprintf_2(("relcur_checkreuse:SOLID_NEW_CURSOR_USEOLD\n"));
            ss_dassert(old_relcur->sr_con == con);
            new_cursor_type = SOLID_NEW_CURSOR_USEOLD;
        }
        return(new_cursor_type);
}

/*#***********************************************************************\
 *
 *              ::soliddb_cursor_type()
 *
 * Set up solidDB cursor type
 *
 * Parameters :
 *
 *     THD*             thd, in, use
 *     bool             for_update, in, use
 *
 * Return values :
 *
 *     DBE_CURSOR_UPDATE
 *     DBE_CURSOR_DELETE
 *     DBE_CURSOR_FORUPDATE
 *     DBE_CURSOR_SELECT
 *
 * Globals used :
 */
static inline dbe_cursor_type_t soliddb_cursor_type(
        MYSQL_THD thd,
        bool      for_update)
{
        switch (soliddb_sql_command(thd)) {

            case SQLCOM_UPDATE:
            case SQLCOM_UPDATE_MULTI:
            case SQLCOM_INSERT:
            case SQLCOM_REPLACE:
            case SQLCOM_REPLACE_SELECT:
                ss_dprintf_3(("ha_solid_cursor_type:thd->lex->sql_command=%d, return DBE_CURSOR_UPDATE\n", soliddb_sql_command(thd)));
                return (DBE_CURSOR_UPDATE);

            case SQLCOM_DELETE:
            case SQLCOM_DELETE_MULTI:
            case SQLCOM_TRUNCATE:
                ss_dprintf_3(("ha_solid_cursor_type:thd->lex->sql_command=%d, return DBE_CURSOR_DELETE\n", soliddb_sql_command(thd)));
                return (DBE_CURSOR_DELETE);

            default:
                if (for_update) {
                    ss_dprintf_3(("ha_solid_cursor_type:thd->lex->sql_command=%d, return DBE_CURSOR_FORUPDATE\n", soliddb_sql_command(thd)));
                    return (DBE_CURSOR_FORUPDATE);
                } else {
                    ss_dprintf_3(("ha_solid_cursor_type:thd->lex->sql_command=%d, return DBE_CURSOR_SELECT\n", soliddb_sql_command(thd)));
                    return (DBE_CURSOR_SELECT);
                }
        }
}

/*#***********************************************************************\
 *
 *              ::solid_relcur_create
 *
 * Create solidDB relation cursor
 *
 * Parameters :
 *
 *     SOLID_CONN*      con, in, use, solidDB connection
 *     solid_relcur_t*  old_relcur, in, use, old relation cursor or NULL
 *     rs_relh_t*       rsrelh, in, use, table relation
 *     TABLE*           table, in, use, MySQL table
 *     THD*             thd, in, use
 *     int              index, in, use, index number
 *     KEY*             key_info, in, use, MySQL key definition
 *
 * Return value :
 *
 *     solid_relcur_t*  relation cursor
 *
 * Globals used :
 */
solid_relcur_t* ha_soliddb::solid_relcur_create(
        SOLID_CONN* con,
        solid_relcur_t* old_relcur,
        rs_relh_t* rsrelh,
        TABLE* table,
        MYSQL_THD thd,
        int index,
        KEY* key_info)
{
        solid_relcur_t* new_relcur = NULL;
        int nattrs;
        int ano;
        int physano;
        int i;
        rs_sysi_t* cd;
        tb_trans_t* trans;
        solid_new_cursor_t new_cursor_type;
        Field** fieldptr;
        Field* field;
        rs_key_t* key;
        rs_key_t* primkey;
        int donelistid;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)

        ss_win_perf_start;
        cd = con->sc_cd;
        ss_pprintf_1(("ha_solid::solid_relcur_create:table %.30s, query %.250s\n",
                      rs_relh_name(cd, rsrelh), soliddb_query(thd)));
        SS_PUSHNAME("ha_soliddb::solid_relcur_create");
        SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_CACHE_FIND);

        trans = con->sc_trans;

        new_cursor_type = relcur_checkreuse(con, old_relcur, rsrelh, index,
                                            soliddb_cursor_type(thd, for_update),
                                            extra_keyread);

        if (old_relcur != NULL && new_cursor_type == SOLID_NEW_CURSOR_CREATE) {
            ss_pprintf_2(("ha_solid::solid_relcur_create, free old cursor\n"));
            solid_relcur_free(cd, old_relcur, con, FALSE, TRUE);
            old_relcur = NULL;
        }

        /* Could not use old cursor, try to find one from con->sc_relcurdonelist
         */
        donelistid = rs_relh_relid(cd, rsrelh) % MAX_RELCURDONELIST;
        SsSemEnter(con->sc_mutex);

        if (old_relcur == NULL && con->sc_relcurdonelist[donelistid] != NULL) {
            su_list_node_t* n;
            solid_relcur_t* best_old_relcur = NULL;
            su_list_node_t* best_n = NULL;
            solid_new_cursor_t best_new_cursor_type = SOLID_NEW_CURSOR_CREATE;
            void* p;
            su_list_t* donelist;

            ss_pprintf_2(("ha_solid::solid_relcur_create, could not use old cursor, try to find one from con->sc_relcurdonelist\n"));
            donelist = con->sc_relcurdonelist[donelistid];

            su_list_do_get(donelist, n, p) {
                old_relcur = (solid_relcur_t*)p;
                new_cursor_type = relcur_checkreuse(con, old_relcur, rsrelh, index,
                                                    soliddb_cursor_type(thd, for_update),
                                                    extra_keyread);

                if (new_cursor_type == SOLID_NEW_CURSOR_USEOLD) {
                    best_old_relcur = old_relcur;
                    best_n = n;
                    best_new_cursor_type = new_cursor_type;
                    break;
                }

                if (new_cursor_type == SOLID_NEW_CURSOR_REUSE
                &&  best_old_relcur == NULL)
                {
                    best_old_relcur = old_relcur;
                    best_n = n;
                    best_new_cursor_type = new_cursor_type;
                }
            }

            old_relcur = best_old_relcur;
            n = best_n;
            new_cursor_type = best_new_cursor_type;

            if (new_cursor_type != SOLID_NEW_CURSOR_CREATE) {
                /* Found a cursor that can be used.
                 */
                ss_pprintf_2(("ha_solid::solid_relcur_create, found a cursor that can be used\n"));
                su_list_remove(donelist, n);
            } else {
                ss_pprintf_2(("ha_solid::solid_relcur_create, no cursor found from relcurdonelist\n"));
                ss_dassert(old_relcur == NULL);
            }
        }

        SsSemExit(con->sc_mutex);

        if (old_relcur != NULL) {
            CHK_RELCUR(old_relcur);

            if (new_cursor_type == SOLID_NEW_CURSOR_CREATE) {
                ss_pprintf_2(("ha_solid::solid_relcur_create, free old cursor\n"));
                solid_relcur_free(cd, old_relcur, con, FALSE, TRUE);
                new_relcur = NULL;
            } else {
                if (new_cursor_type == SOLID_NEW_CURSOR_REUSE) {
                    ss_pprintf_2(("ha_solid::solid_relcur_create, free old dbe cursor\n"));
                    solid_relcur_free_cursor(cd, old_relcur, con, TRUE);
                } else {
                    ss_dassert(new_cursor_type == SOLID_NEW_CURSOR_USEOLD);
                }

                new_relcur = old_relcur;
            }
        }

        if (new_cursor_type == SOLID_NEW_CURSOR_CREATE) {
            ss_pprintf_2(("ha_solid::solid_relcur_create, create a new cursor, for_update=%d\n", for_update));
            SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_CREATE);

            new_relcur = SSMEM_NEW(solid_relcur_t);

            new_relcur->sr_chk = CHKVAL_RELCUR;
            new_relcur->sr_con = con;
            new_relcur->sr_mainmem = (rs_relh_reltype(cd, rsrelh) == RS_RELTYPE_MAINMEMORY);
            new_relcur->sr_ttype = rs_relh_ttype(cd, rsrelh);

            if (new_relcur->sr_mainmem) {
                new_relcur->sr_fetchtval = NULL;
            } else {
                new_relcur->sr_fetchtval = rs_tval_create(cd, new_relcur->sr_ttype);
            }

            new_relcur->sr_bkeybuf = NULL;
            new_relcur->sr_full_scan = TRUE;
            new_relcur->sr_key = solid_resolve_key(cd, rsrelh, key_info);
            new_relcur->sr_index = index;
            new_relcur->sr_cd = cd;
            new_relcur->sr_trans = trans;
            new_relcur->sr_rsrelh = rsrelh;
            new_relcur->sr_for_update = for_update;
            new_relcur->sr_prevnextp = 2;
            new_relcur->sr_unique_row = FALSE;

            new_relcur->sr_cursor_type = soliddb_cursor_type(thd, for_update);

            new_relcur->sr_vbuf = NULL;

            new_relcur->sr_extra_keyread = extra_keyread;                     /* HA_EXTRA_KEYREAD */
            new_relcur->sr_extra_retrieve_primary_key = extra_retrieve_primary_key;        /* HA_EXTRA_RETRIEVE_PRIMARY_KEY */
            new_relcur->sr_extra_retrieve_all_cols = extra_retrieve_all_cols;           /* HA_EXTRA_RETRIEVE_ALL_COLS */
#if MYSQL_VERSION_ID >= 50100
            new_relcur->sr_query_id = 0;
#else
            new_relcur->sr_query_id = thd->query_id;
#endif
            new_relcur->sr_pla = NULL;
            new_relcur->sr_plakey = NULL;
            rs_relh_link(cd, rsrelh);
            SS_MEM_SETLINK(rsrelh);
            SsSemEnter(con->sc_mutex);
            CHK_CONN(con);
            new_relcur->sr_relcurlist = con->sc_relcurlist;
            new_relcur->sr_relcurlistnode = su_list_insertlast(con->sc_relcurlist, (void*)new_relcur);
            SsSemExit(con->sc_mutex);

            nattrs = rs_ttype_nattrs(cd, new_relcur->sr_ttype);
            new_relcur->sr_selectlist = (int*)SsMemAlloc((nattrs + 1) * sizeof(new_relcur->sr_selectlist[0]));
            new_relcur->sr_plaselectlist = NULL;
            new_relcur->sr_constraints = NULL;
            new_relcur->sr_constval = NULL;
            new_relcur->sr_postval = NULL;
            new_relcur->sr_usedfields = (int*)SsMemAlloc(nattrs * sizeof(new_relcur->sr_usedfields[0]));
            new_relcur->sr_casc_states = NULL;
            new_relcur->sr_relcur = NULL;
        } else if (new_cursor_type == SOLID_NEW_CURSOR_REUSE) {
            ss_dassert(new_relcur->sr_relcur == NULL);
            new_relcur->sr_key = solid_resolve_key(cd, rsrelh, key_info);
            new_relcur->sr_cd = cd;
            new_relcur->sr_trans = trans;
            new_relcur->sr_for_update = for_update;
            new_relcur->sr_cursor_type = soliddb_cursor_type(thd, for_update);
            new_relcur->sr_extra_keyread = extra_keyread;                     /* HA_EXTRA_KEYREAD */
            new_relcur->sr_extra_retrieve_primary_key = extra_retrieve_primary_key;        /* HA_EXTRA_RETRIEVE_PRIMARY_KEY */
            new_relcur->sr_extra_retrieve_all_cols = extra_retrieve_all_cols;           /* HA_EXTRA_RETRIEVE_ALL_COLS */
#if MYSQL_VERSION_ID >= 50100
            new_relcur->sr_query_id = 0;
#else
            new_relcur->sr_query_id = thd->query_id;
#endif
            SsSemEnter(con->sc_mutex);
            CHK_CONN(con);
            new_relcur->sr_relcurlist = con->sc_relcurlist;
            new_relcur->sr_relcurlistnode = su_list_insertlast(con->sc_relcurlist, (void*)new_relcur);
            SsSemExit(con->sc_mutex);
        } else {
            ss_dassert(new_cursor_type == SOLID_NEW_CURSOR_USEOLD);
#if MYSQL_VERSION_ID >= 50100
            new_relcur->sr_query_id = 0;
#else
            new_relcur->sr_query_id = thd->query_id;
#endif
            SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_CACHE_HIT);
        }

        new_relcur->sr_force_dereference = FALSE;

        /* Create select list for the cursor. We do not use cached version because it may change between
         * different calls.
         */
        nattrs = rs_ttype_nattrs(cd, new_relcur->sr_ttype);
        memset(new_relcur->sr_usedfields, '\0', nattrs * sizeof(new_relcur->sr_usedfields[0]));

        key = new_relcur->sr_key;
        primkey = rs_relh_clusterkey(cd, rsrelh);
        ss_dassert(primkey != NULL);

        for (fieldptr = table->field, ano = 0, i = 0; *fieldptr != NULL; fieldptr++, ano++) {
            bool add_to_selectlist;

            physano = rs_ttype_sqlanotophys(cd, new_relcur->sr_ttype, ano);
            field = *fieldptr;
            add_to_selectlist = FALSE;

            if (extra_keyread) {
                if (rs_key_searchkpno_data(cd, key, physano) == RS_ANO_NULL) {
                    if (rs_key_searchkpno_anytype(cd, key, physano) == RS_ANO_NULL) {
                        ss_pprintf_2(("ha_solid::solid_relcur_create, skip column, extra_keyread, not in key, ano=%d\n", ano));
                        continue;
                    }
                    new_relcur->sr_force_dereference = TRUE;
                }
            }

            if (extra_retrieve_all_cols) {
                add_to_selectlist = TRUE;
            }

            if (!add_to_selectlist && extra_retrieve_primary_key) {
                if (rs_key_searchkpno_data(cd, primkey, physano) != RS_ANO_NULL) {
                    add_to_selectlist = TRUE;
                }
            }
#if MYSQL_VERSION_ID >= 50100
            if (bitmap_is_set(table->read_set, ano) ||
                bitmap_is_set(table->write_set, ano)) {
                add_to_selectlist = TRUE;
            }
#else
            if (!add_to_selectlist && new_relcur->sr_query_id == field->query_id) {
                add_to_selectlist = TRUE;
            }
#endif
            if (add_to_selectlist) {
                ss_pprintf_2(("ha_solid::solid_relcur_create, add to select list, ano=%d, physano=%d\n", ano, physano));
                new_relcur->sr_selectlist[i++] = physano;
                new_relcur->sr_usedfields[ano] = TRUE;
            } else {
                ss_pprintf_2(("ha_solid::solid_relcur_create, skip column, ano=%d\n", ano));
            }
        }
        new_relcur->sr_selectlist[i++] = -1;
        ss_dassert(new_relcur->sr_relcurlist != NULL);
        ss_dassert(new_relcur->sr_relcurlistnode != NULL);

        ss_win_perf_stop(relcur_create_perfcount, relcur_create_callcount);
        CHK_RELCUR(new_relcur);
        ss_pprintf_2(("ha_solid::solid_relcur_create, return cursor %ld\n", (long)new_relcur));
        SS_POPNAME;

        return(new_relcur);
}

/*#***********************************************************************\
 *
 *              mysql_record_is_null
 *
 * Check if a field in a record is NULL.
 *
 * Parameters:
 *
 *     TABLE*           table, in, use, table
 *     Field*           field, in, use, field
 *     char*            record, in, use, a row in MySQL format
 *
 * Return value: 1 if record is NULL, 0 otherwise
 *
 * Globals used :
 */
static inline int mysql_record_is_null(
        TABLE* table,
        Field* field,
        char * record)
{
        uint null_offset;

        if (!field->null_ptr) {
            return (0);
        } else {
            null_offset = (uint) ((char *)field->null_ptr
                                  - (char *) table->record[0]);

            if (record[null_offset] & field->null_bit) {
                return (1);
            } else {
                return (0);
            }
        }
}

/*#***********************************************************************\
 *
 *              solid_relcur_setconstr
 *
 * Set constraints for relation cursor
 *
 * Parameters :
 *
 *     rs_sysi_t*       cd, in, use
 *     tb_trans_t*      trans, in, use, transaction
 *     rs_relh_t*       rsrelh, in, use, table relation
 *     solid_relcur_t*  old_relcur, in, use, old relation cursor or NULL
 *     int              keyindex, in, use, index number
 *     KEY*             key_info, in, use, MySQL key definition
 *     const SS_MYSQL_ROW*      keyconstr_ptr, in, use, key constraint
 *     uint             keyconstr_len, in, use, key constraint length
 *     enum ha_rkey_function find_flag, in, use
 *
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int solid_relcur_setconstr(
        MYSQL_THD thd,
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* rsrelh,
        solid_relcur_t* solid_relcur,
        uint keyindex,
        KEY* key_info,
        TABLE* table,
        const SS_MYSQL_ROW* keyconstr_ptr,
        uint keyconstr_len,
        enum ha_rkey_function find_flag)
{
        KEY_PART_INFO* key_part;
        const SS_MYSQL_ROW* ptr;
        uint key_len;
        rs_key_t* primkey;
        rs_key_t* key;
        vtpl_t* range_start;
        vtpl_t* range_end;
        va_t* range_start_last_va;
        va_t* range_end_last_va;
        su_list_t* tuple_reference;
        rs_ttype_t* ttype;
        bool newindex;
        bool newplan;
        int cons_len;
        bool mainmem;
        rs_cons_t* cons;
        bool dereference;
        solid_bool dereference2;
        rs_aval_t avalbuf;
        va_t* va;
        int nequal = 0;
        uint relop = RS_RELOP_EQUAL;
        su_list_node_t* cons_n = NULL;
        bool replan = FALSE;
        rs_atype_t* prev_atype = NULL; /* needed for freeing avalbuf */
        ss_byte_t vabuf[BUFVA_MAXBUFSIZE];
        int retval = 0;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)

        ss_win_perf_start;
        DBUG_ENTER("ha_solid::solid_relcur_setconstr");

        ss_pprintf_1(("ha_solid::solid_relcur_setconstr:key %s, keyindex %u, cursor %ld, find_flag %d\n",
            key_info == NULL ? "" : key_info->name, keyindex, (long)solid_relcur, (int)find_flag));

        CHK_RELCUR(solid_relcur);
        SS_PUSHNAME("solid_relcur_setconstr");

        bufva_init(vabuf, sizeof(vabuf));
        mainmem = solid_relcur->sr_mainmem;

        ttype = rs_relh_ttype(cd, rsrelh);
        primkey = rs_relh_clusterkey(cd, rsrelh);

        if (solid_relcur->sr_key != NULL && solid_relcur->sr_index == keyindex) {
            key = solid_relcur->sr_key;
        } else {
            /* Resolve key. */
            key = solid_resolve_key(cd, rsrelh, key_info);
        }

        if (solid_relcur->sr_plakey == NULL || solid_relcur->sr_plakey != key) {
            newindex = TRUE;
        } else {
            newindex = FALSE;
        }

        solid_relcur->sr_index = keyindex;
        solid_relcur->sr_key = key;
        solid_relcur->sr_plakey = key;
        solid_relcur->sr_full_scan = FALSE;

        if (mainmem) {
            range_start = (vtpl_t*)NULL;
            range_end = (vtpl_t*)NULL;
        } else {
            /* Calculate max constraint lenght. Assume all fields can be long
             * v-attributes and assume that we end null byte at the end of
             * each value.
             */
            cons_len = keyconstr_len
                       + (rs_key_nparts(cd, key) * sizeof(va_t))
                       + (rs_key_nparts(cd, key) + 1) * VA_LENGTHMAXLEN;
            range_start = (vtpl_t*)SsMemAlloc(cons_len + 1);
            range_end = (vtpl_t*)SsMemAlloc(cons_len + 1);
            /* Set upper and lower limit for the key. */
            vtpl_setvtpl(range_start, VTPL_EMPTY);
            vtpl_setvtpl(range_end, VTPL_EMPTY);
        }
        nequal++;

        ss_dassert(rs_keyp_isconstvalue(cd, key, 0));

        if (mainmem) {
            range_start_last_va = NULL;
            range_end_last_va = NULL;
            if (solid_relcur->sr_constraints == NULL) {
                solid_relcur->sr_constraints = su_list_init(NULL);
                solid_relcur->sr_constval = rs_tval_create(cd, ttype);
            }
            cons_n = su_list_first(solid_relcur->sr_constraints);
        } else {
            range_start_last_va = range_end_last_va = rs_keyp_constvalue(cd, key, 0);
        }

        if (keyconstr_ptr != NULL) {

            ptr = keyconstr_ptr;
            key_len = keyconstr_len;
            int kpno;

            /* Get MySQL constraints.
             *
             * Some of the code here is copied from ha_federated.cpp.
             * TODO: Need to recheck this.
             */
            for (key_part = key_info->key_part, kpno=1; ; key_part++, kpno++) {
                rs_ano_t ano;
                rs_atype_t* atype;
                rs_aval_t* aval;
                Field* field;
                bool isnull;
                bool null_value;
                int offset;
                rs_attrtype_t kp_type;
                solid_bool kp_ascending;
                su_collation_t* kp_collation = NULL;
                int kp_prefixlen = 0;

                kp_type = rs_keyp_parttype(cd, key, kpno);

                kp_ascending = rs_keyp_isascending(cd, key, kpno);

                if (kp_type == RSAT_COLLATION_KEY) {
                    kp_collation = rs_keyp_collation(cd, key, kpno);
                    kp_prefixlen = rs_keyp_getprefixlength(cd, key, kpno);
                } else if (kp_type != RSAT_USER_DEFINED) {
                    ss_pprintf_2(("ha_solid::solid_relcur_setconstr:kpno=%d, not RSAT_USER_DEFINED, break\n", kpno));
                    break;
                }

                field = key_part->field;
                enum_field_types  mysqltype = field->type();

                ano = rs_keyp_ano(cd, key, kpno);
                ss_dassert(ano != RS_ANO_NULL);

                ss_pprintf_2(("ha_solid::solid_relcur_setconstr:kpno=%d, ano=%d\n", kpno, ano));

                atype = rs_ttype_atype(cd, ttype, ano);

                offset = 0;

                isnull = mysql_record_is_null(table, field, (char *)ptr);

                if (key_part->null_bit && (*ptr)) {
                    /* Constraint is IS NULL or IS NOT NULL */

                    null_value = TRUE;

                    switch (find_flag) {
                        case HA_READ_KEY_EXACT:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_EXACT:RS_RELOP_ISNULL\n"));
                            if (!mainmem) {
                                if (range_start_last_va != NULL) {
                                    vtpl_appva(range_start, range_start_last_va);
                                }
                                range_start_last_va = VA_NULL;
                                if (range_end_last_va != NULL) {
                                    vtpl_appva(range_end, range_end_last_va);
                                }
                                range_end_last_va = VA_NULL;
                            }
                            break;
                        case HA_READ_KEY_OR_NEXT:
                        case HA_READ_PREFIX:
                        case HA_READ_PREFIX_LAST:
                        case HA_READ_PREFIX_LAST_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_OR_NEXT...:RS_RELOP_GE NULL\n"));
                            if (!mainmem) {
                                if (range_start_last_va != NULL) {
                                    vtpl_appva(range_start, range_start_last_va);
                                }
                                range_start_last_va = VA_NULL;
                            }
                            relop = RS_RELOP_GE;
                            break;
                        case HA_READ_BEFORE_KEY:
                        case HA_READ_KEY_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_BEFORE_KEY...:RS_RELOP_LE NOTNULL\n"));
                            if (!mainmem) {
                                if (range_end_last_va != NULL) {
                                    vtpl_appva(range_end, range_end_last_va);
                                }
                                range_end_last_va = VA_NULL;
                            }
                            relop = RS_RELOP_LE;
                            break;
                        case HA_READ_AFTER_KEY:
                            if (isnull) {
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_AFTER_KEY:RS_RELOP_ISNULL\n"));
                                relop = RS_RELOP_GE;
                            } else {
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_AFTER_KEY:RS_RELOP_NE NOTNULL\n"));
                                relop = RS_RELOP_GT;
                            }
                            if (!mainmem) {
                                if (range_start_last_va != NULL) {
                                    vtpl_appva(range_start, range_start_last_va);
                                }
                                range_start_last_va = VA_NULL;
                            }
                            break;
                        default:
                            ss_pprintf_1(("ha_solid::solid_relcur_setconstr:**UNKNOWN** find_flag=%d\n",
                                          find_flag));

                            if (range_start) {
                                dynvtpl_free(&range_start);
                            }

                            if (range_end) {
                                dynvtpl_free(&range_end);
                            }

                            ss_error;        /* We should not come here */
                            goto return_1;
                    }

                } else { /* Constraint has a value */

                    null_value = FALSE;

                    if (key_part->null_bit) {
                        offset = 1;
                    }

                    /* TODO: LIKE */
                    switch (find_flag) {
                        case HA_READ_KEY_EXACT:
                        #ifdef SS_HA_BUG62FIX
                            if (key_part->field->result_type() == STRING_RESULT &&
                                ( mysqltype == MYSQL_TYPE_VAR_STRING ||
                                  mysqltype == MYSQL_TYPE_STRING ||
                                  mysqltype == MYSQL_TYPE_VARCHAR ) ) {
                                relop = key_part->store_length > key_len ? RS_RELOP_GE : RS_RELOP_EQUAL;
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr: HA_READ_KEY_EXACT(STRING) => RS_RELOP_%s\n",
                                              key_part->store_length > key_len ? "GE" : "EQUAL"));
                            } else {  /* not a string field. */
                                relop = RS_RELOP_EQUAL;
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_EXACT => RS_RELOP_EQUAL\n"));

                                //if (key_part->store_length >= key_len || key_part->type == HA_KEYTYPE_BIT) {
                                //    ss_pprintf_3(("ha_solid::solid_relcur_setconstr: HA_READ_KEY_EXACT => RS_RELOP_EQUAL\n"));
                                //    relop = RS_RELOP_EQUAL;
                                //} else {
                                //    ss_pprintf_3(("ha_solid::solid_relcur_setconstr: HA_READ_KEY_EXACT => RS_RELOP_GE\n"));
                                //    relop = RS_RELOP_GE;
                                //}
                            }
                        #else
                            if (key_part->store_length >= key_len
                                || key_part->type == HA_KEYTYPE_BIT
                                || field->result_type() != STRING_RESULT)
                            {
                                int mysqltype = field->type();

                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_EXACT => RS_RELOP_EQUAL\n"));
                                if (field->result_type() == STRING_RESULT &&
                                    (mysqltype == MYSQL_TYPE_VAR_STRING ||
                                     mysqltype == FIELD_TYPE_STRING ||
                                     mysqltype == MYSQL_TYPE_VARCHAR)) {
                                    ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_EXACT(STRING) => RS_RELOP_GE\n"));
                                    relop = RS_RELOP_GE;
                                }
                            } else {
                                /* TODO: LIKE */
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:LIKE => RS_RELOP_GE\n"));
                                relop = RS_RELOP_GE;
                            }
                        #endif
                            break;
                        case HA_READ_AFTER_KEY:
                            if (key_part->store_length >= key_len) {
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_AFTER_KEY => RS_RELOP_GT\n"));
                                relop = RS_RELOP_GT;
                                break;
                            }
                            /* FALLTHROUGH */
                        case HA_READ_KEY_OR_NEXT:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_OR_NEXT => RS_RELOP_GE\n"));
                            relop = RS_RELOP_GE;
                            break;
                        case HA_READ_BEFORE_KEY:
                            if (key_part->store_length >= key_len) {
                                ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_BEFORE_KEY => RS_RELOP_LT\n"));
                                relop = RS_RELOP_LT;
                                break;
                            }
                            /* FALLTHROUGH */
                        case HA_READ_KEY_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_KEY_OR_PREV => RS_RELOP_LE\n"));
                            relop = RS_RELOP_LE;
                            break;

                        case HA_READ_PREFIX:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_PREVIX => RS_RELOP_GE\n"));
                            relop = RS_RELOP_GE;
                            break;

                        case HA_READ_PREFIX_LAST:
                        case HA_READ_PREFIX_LAST_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_relcur_setconstr:HA_READ_PREFIX_LAST => RS_RELOP_LE\n"));
                            relop = RS_RELOP_LE;
                            break;

                        default:
                            ss_pprintf_1(("ha_solid::solid_relcur_setconstr:**UNKNOWN** find_flag=%d\n",
                                          find_flag));

                            bufva_done(vabuf, sizeof(vabuf));

                            if (range_start) {
                                dynvtpl_free(&range_start);
                            }

                            if (range_end) {
                                dynvtpl_free(&range_end);
                            }

                            ss_error;        /* We should not come here */
                            goto return_1;
                    }

                    switch (relop) {
                        case RS_RELOP_EQUAL:
                            if (range_start_last_va != NULL) {
                                vtpl_appva(range_start, range_start_last_va);
                                range_start_last_va = NULL;
                            }
                            if (range_end_last_va != NULL) {
                                vtpl_appva(range_end, range_end_last_va);
                                range_end_last_va = NULL;
                            }
                            break;
                        case RS_RELOP_GE:
                        case RS_RELOP_GT:
                            if (range_start_last_va != NULL) {
                                vtpl_appva(range_start, range_start_last_va);
                                range_start_last_va = NULL;
                            }
                            break;
                        case RS_RELOP_LT:
                        case RS_RELOP_LE:
                            if (range_end_last_va != NULL) {
                                vtpl_appva(range_end, range_end_last_va);
                                range_end_last_va = NULL;
                            }
                           break;
                        default:

                            if (range_start) {
                                dynvtpl_free(&range_start);
                            }

                            if (range_end) {
                                dynvtpl_free(&range_end);
                            }

                            ss_derror;
                            ss_pprintf_1(("ha_solid::solid_relcur_setconstr:**UNKNOWN** relop=%d\n", relop));
                            goto return_1;
                    }

                }

                if (mainmem) {
                    bool new_cons;

                    if (cons_n == NULL) {
                        ss_pprintf_2(("ha_solid::solid_relcur_setconstr:no old cons\n"));
                        new_cons = TRUE;
                    } else {
                        cons = (rs_cons_t*)su_listnode_getdata(cons_n);
                        aval = rs_tval_aval(cd, ttype, solid_relcur->sr_constval, ano);
                        ss_dassert(rs_cons_ano(cd, cons) == ano);
                        ss_dassert(rs_cons_atype(cd, cons) == atype);
                        ss_dassert(rs_cons_aval(cd, cons) == aval);
                        if (rs_cons_relop(cd, cons) != relop) {
                            ss_pprintf_2(("ha_solid::solid_relcur_setconstr:relop in old cons is different\n"));
                            new_cons = TRUE;
                            free_end_of_constr_list(cd, solid_relcur->sr_constraints, cons_n);
                            cons_n = NULL;
                        } else {
                            ss_pprintf_2(("ha_solid::solid_relcur_setconstr:use old cons\n"));
                            new_cons = FALSE;
                            cons_n = su_list_next(solid_relcur->sr_constraints, cons_n);
                        }
                    }
                    if (new_cons) {
                        rs_sqlcons_t sqlcons;

                        replan = TRUE;

                        /* Get a reference to aval inside tval */
                        aval = rs_tval_aval(cd, ttype, solid_relcur->sr_constval, ano);

                        sqlcons.sc_relop = relop;
                        sqlcons.sc_attrn = ano;
                        sqlcons.sc_atype = atype;
                        sqlcons.sc_aval = aval;
                        sqlcons.sc_escatype = NULL;
                        sqlcons.sc_escaval = NULL;
                        sqlcons.sc_alias = TRUE;
                        sqlcons.sc_tabcons = FALSE;

                        cons = rs_cons_init(
                                    cd,
                                    relop,
                                    ano,
                                    atype,
                                    aval,
                                    0,          /* su_bflag_t flags */
                                    &sqlcons,   /* rs_sqlcons_t* sqlcons */
                                    0,          /* int escchar */
                                    atype,      /* rs_atype_t* org_atype */
                                    NULL);      /* rs_err_t** p_errh */
                        ss_assert(cons != NULL);
                        su_list_insertlast(solid_relcur->sr_constraints, cons);
                    }
                    if (null_value) {
                        rs_aval_setnull(cd, atype, aval);
                    } else {
                        set_mysql_field_to_aval_or_dynva(thd, table, cd, atype, aval, NULL, field,
                                                         (SS_MYSQL_ROW*)(ptr + offset), TRUE, NULL);
                    }
                    ss_debug(ha_print_aval(cd, atype, aval, (char*)"solid_relcur_setconstr:mainmem"));

                } else if (!null_value) {

                    ss_dassert(!mainmem);

                    if (prev_atype != NULL) {
                        /* avalbuf has previous data, free it */
                        ss_dprintf_1(("freeing avalbuf (0x%08lX)\n",
                                      (long)(ss_ptr_as_scalar_t)&avalbuf));
                        rs_aval_freebuf(cd, prev_atype, &avalbuf);
                    }

                    ss_dprintf_1(("initializing avalbuf (0x%08lX)\n",
                                  (long)(ss_ptr_as_scalar_t)&avalbuf));
                    rs_aval_createbuf(cd, atype, &avalbuf);
                    prev_atype = atype;

                    set_mysql_field_to_aval_or_dynva(thd, table, cd, atype, &avalbuf, NULL, field,
                                                     (SS_MYSQL_ROW*)(ptr + offset), TRUE, NULL);

                    va = rs_aval_getkeyva(cd, atype, &avalbuf,
                                          kp_collation,
                                          kp_type,
                                          kp_ascending,
                                          vabuf,
                                          sizeof(vabuf),
                                          kp_prefixlen);

                    ss_dassert(va != NULL);

                    ss_dprintf_1(("rs_aval_getkeyva(0x%08lX) gave 0x%08lX\n",
                                  (long)(ss_ptr_as_scalar_t)&avalbuf,
                                  (long)(ss_ptr_as_scalar_t)va));


                    switch (relop) {
                        case RS_RELOP_EQUAL:
                            range_start_last_va = va;
                            range_end_last_va = va;
                            nequal++;
                            break;
                        case RS_RELOP_GE:
                        case RS_RELOP_GT:
                            range_start_last_va = va;
                            break;
                        case RS_RELOP_LT:
                        case RS_RELOP_LE:
                            range_end_last_va = va;
                            break;
                        default:

                            if (range_start) {
                                dynvtpl_free(&range_start);
                            }

                            if (range_end) {
                                dynvtpl_free(&range_end);
                            }

                            ss_derror;
                            ss_pprintf_1(("ha_solid::solid_relcur_setconstr:**UNKNOWN** relop=%d\n", relop));

                            goto return_1;
                    }
                }

                if (key_part->store_length >= key_len) {
                    break;
                }

                key_len -= key_part->store_length;
                ptr += key_part->store_length;
            }
        }

        if (mainmem) {
            if (cons_n != NULL) {
                /* Old list has more constraints than the new one, free extra
                 * constrains.
                 */
                free_end_of_constr_list(cd, solid_relcur->sr_constraints, cons_n);
                replan = TRUE;
            }
            ss_dassert(range_start == NULL);
            ss_dassert(range_end == NULL);
        } else {
            if (range_start_last_va != NULL) {
                if (relop ==  RS_RELOP_GT) {
                    vtpl_appvawithincrement(range_start, range_start_last_va);
                } else {
                    vtpl_appva(range_start, range_start_last_va);
                }
            }
            if (range_end_last_va != NULL) {
                if (relop ==  RS_RELOP_LT) {
                    vtpl_appva(range_end, range_end_last_va);
                } else {
                    vtpl_appvawithincrement(range_end, range_end_last_va);
                }
            }

            ss_output_1((vtpl_dprintvtpl(1, range_start)));
            ss_output_1((vtpl_dprintvtpl(1, range_end)));
        }

        if (rs_key_isprimary(cd, key)) {
            dereference = FALSE;
        } else {
            dereference = !solid_relcur->sr_extra_keyread || solid_relcur->sr_force_dereference;
        }

        if (solid_relcur->sr_pla == NULL) {
            ss_dassert(newindex);
            solid_relcur->sr_pla = rs_pla_alloc(cd);
            solid_relcur->sr_plaselectlist = (int*)SsMemAlloc(
                                                        rs_key_nparts(cd, primkey) *
                                                        sizeof(solid_relcur->sr_plaselectlist[0]));
            newplan = TRUE;
        } else {
            newplan = FALSE;
        }

        tb_pla_form_select_list_buf(
            cd,
            primkey,
            key,
            solid_relcur->sr_selectlist,
            solid_relcur->sr_plaselectlist,
            dereference,
            &dereference2);
        ss_dassert(dereference == dereference2);

        if (newindex) {
            rs_pla_clear_tuple_reference_buf(cd, solid_relcur->sr_pla);

            tuple_reference = rs_pla_form_tuple_reference(
                                cd,
                                primkey,
                                rs_pla_get_tuple_reference_buf(cd, solid_relcur->sr_pla),
                                key);
        } else {
            tuple_reference = rs_pla_get_tuple_reference_buf(cd, solid_relcur->sr_pla);
        }
        ss_dassert(su_list_length(tuple_reference) > 0);

        if (newplan) {
            ss_pprintf_2(("ha_solid::solid_relcur_setconstr:rs_pla_initbuf\n"));

            rs_pla_initbuf(
                cd,
                solid_relcur->sr_pla,
                rsrelh,
                key,
                TRUE,
                range_start,
                TRUE,
                range_end,
                TRUE,
                NULL,
                NULL,
                solid_relcur->sr_constraints,
                tuple_reference,
                solid_relcur->sr_plaselectlist,
                dereference,
                0,
                0,
                0,
                TRUE);

        } else {
            ss_pprintf_2(("ha_solid::solid_relcur_setconstr:rs_pla_reset\n"));

            ss_debug(rs_pla_check_reset(
                cd,
                solid_relcur->sr_pla,
                rsrelh,
                key,
                TRUE,
                range_start,
                TRUE,
                range_end,
                TRUE,
                NULL,
                NULL,
                solid_relcur->sr_constraints,
                tuple_reference,
                solid_relcur->sr_plaselectlist,
                dereference,
                0,
                0,
                0));

            rs_pla_reset(
                cd,
                solid_relcur->sr_pla,
                TRUE,
                range_start,
                range_end);

            if (mainmem) {
                rs_pla_setreplan(cd, solid_relcur->sr_pla, replan);
            }
        }

        solid_relcur->sr_open_cursor = TRUE;

        if (!solid_relcur->sr_mainmem) {
            solid_relcur->sr_unique_row = rs_key_isunique(cd, key) &&
                                          nequal == (rs_key_lastordering(cd, key) + 1) &&
                                          solid_relcur->sr_cursor_type == DBE_CURSOR_SELECT &&
                                          soliddb_sql_command(thd) == SQLCOM_SELECT;
        }

        ss_pprintf_2(("ha_solid::solid_relcur_setconstr:sr_unique_row=%d, nequal=%d, key lastordering=%d, key unique=%d\n",
            solid_relcur->sr_unique_row, nequal, rs_key_lastordering(cd, key),
            rs_key_isunique(cd, key)));

        ss_win_perf_stop(relcur_setconstr_perfcount, relcur_setconstr_callcount);

 ret_cleanup:;
        if (prev_atype != NULL) {
            rs_aval_freebuf(cd, prev_atype, &avalbuf);
        }

        bufva_done(vabuf, sizeof(vabuf));

        SS_POPNAME;

        DBUG_RETURN(retval);
return_1:
        retval = 1;
        goto ret_cleanup;
}

/*#***********************************************************************\
 *
 *              ha_solid_time_mysql
 *
 * Converts solidDB time structure to time_t
 *
 *
 * Parameters :
 *
 *  dt_date_t* solid_time, in, use, time in solid structure
 *
 * Return value : time_t structure
 *
 * Globals used :
 */
static inline time_t ha_solid_time_mysql(
        dt_date_t* solid_time)
{
        SsTmT ss_time;
        time_t mysql_ttime;

        memset(&ss_time, 0, sizeof(SsTmT));

        ss_time.tm_sec = dt_date_sec(solid_time);
        ss_time.tm_min = dt_date_min(solid_time);
        ss_time.tm_hour = dt_date_hour(solid_time);
        ss_time.tm_mday = dt_date_mday(solid_time);
        ss_time.tm_mon = dt_date_month(solid_time);
        ss_time.tm_year = dt_date_year(solid_time) - 1900;
        ss_time.tm_yday = 0;
        ss_time.tm_isdst = -1;

        mysql_ttime = (time_t)SsMktime(&ss_time);

        return (mysql_ttime);
}

/*#***********************************************************************\
 *
 *              ::solid_fetch
 *
 *   Fetch the next row in a result set. This is a separate routine mainly
 *   to have serarate statictics_increment call for different external
 *   fetch calls.
 *
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*  rowbuf, in out, row buffer
 *     bool   nextp, in, use,
 *     THD*   thd, in, use, MySQL thread
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::solid_fetch(
        SS_MYSQL_ROW* rowbuf,
        bool          nextp,
        MYSQL_THD     thd,
        uint          command)
{
        rs_ttype_t* ttype;
        rs_atype_t* atype;
        rs_aval_t* aval;
        int ano;
        ss_debug(int steps;)
        Field** fieldptr;
        Field* field;
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        rs_relh_t* rsrelh;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)
        dbe_ret_t rc = DBE_RC_SUCC;
        bool isfetch = FALSE;
        int sret = 0;
        su_err_t* errh = NULL;
        size_t smallblobsizemax = INT_MAX; //TODO: what is max blob length in mysql and how long can constraint be for blobs

        ss_win_perf_start;
        DBUG_ENTER("ha_soliddb::solid_fetch");
        ss_dassert(solid_relcur != NULL);
        CHK_RELCUR(solid_relcur);
        SS_PUSHNAME("ha_soliddb::solid_fetch");

#if MYSQL_VERSION_ID >= 50100
        solid_conn = get_solid_ha_data_connection(this->ht, thd);
#else
        solid_conn = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        con = solid_conn;
        ss_dassert(con != NULL);
        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;
        rsrelh = solid_table->st_rsrelh;

        ss_pprintf_1(("ha_solid::solid_fetch, table=%s, nextp=%d\n", rs_relh_name(cd, rsrelh), nextp));

        if (tb_trans_dbtrx(cd, trans) == NULL) {

            if (tb_trans_getdberet(cd, trans) == SU_SUCCESS) {
                tb_trans_beginif(con->sc_cd, con->sc_trans);
            } else {
                int retcode;

                retcode = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
                SS_POPNAME;
                DBUG_RETURN(retcode);
            }
        }

        rs_sysi_setflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);

        if (solid_relcur->sr_open_cursor) {
            bool unique_search = FALSE;

            ss_win_perf(__int64 startcount;)
            ss_win_perf(__int64 endcount;)

            ss_win_perf_start;

            solid_relcur->sr_open_cursor = FALSE;

            if (0 && solid_relcur->sr_unique_row && nextp) {

                if (solid_relcur->sr_fetchtval == NULL) {
                    ss_dassert(!solid_relcur->sr_mainmem);
                    solid_relcur->sr_fetchtval = rs_tval_create(cd,
                                                                solid_relcur->sr_ttype);
                }

                ss_dassert(solid_relcur->sr_ttype != NULL);

                rc = dbe_cursor_getunique(
                        tb_trans_dbtrx(cd, trans),
                        solid_relcur->sr_ttype,
                        solid_relcur->sr_selectlist,
                        solid_relcur->sr_pla,
                        solid_relcur->sr_fetchtval,
                        &solid_relcur->sr_bkeybuf,
                        &errh);

                unique_search = (rc != DBE_RC_NOTFOUND);
                isfetch = unique_search;
                ss_dassert(rc != DBE_RC_WAITLOCK);
            } else {
                unique_search = FALSE;
            }

            if (!unique_search) {
                if (solid_relcur->sr_relcur == NULL) {
                    ss_pprintf_2(("ha_solid::solid_fetch:dbe_cursor_init\n"));
                    SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_RESET_FULL);

                    solid_relcur->sr_relcur = dbe_cursor_init(
                                                tb_trans_dbtrx(cd, trans),
                                                solid_relcur->sr_ttype,
                                                solid_relcur->sr_selectlist,
                                                solid_relcur->sr_pla,
                                                solid_relcur->sr_cursor_type,
                                                NULL,
                                                &errh);

                    ss_dassert(solid_relcur->sr_relcur != NULL);

                    if (solid_relcur->sr_mainmem) {
                        ss_dassert(solid_relcur->sr_vbuf == NULL);
                        if (solid_relcur->sr_cursor_type == DBE_CURSOR_SELECT) {
                            solid_relcur->sr_vbuf = rs_vbuf_init(cd, solid_relcur->sr_ttype, 10);
                        } else {
                            solid_relcur->sr_vbuf = rs_vbuf_init(cd, solid_relcur->sr_ttype, 2);
                        }
                    }

                } else {
                    ss_pprintf_2(("ha_solid::solid_fetch:dbe_cursor_reset\n"));

                    if (nextp && rowbuf != NULL && !solid_relcur->sr_mainmem) {
                        SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_RESET_FETCH);

                        rc = dbe_cursor_reset_fetch(
                                solid_relcur->sr_relcur,
                                tb_trans_dbtrx(cd, trans),
                                solid_relcur->sr_ttype,
                                solid_relcur->sr_selectlist,
                                solid_relcur->sr_pla,
                                &solid_relcur->sr_fetchtval,
                                &errh);

                        isfetch = (rc == DBE_RC_FOUND || rc == DBE_RC_END);
                        ss_dassert(rc != DBE_RC_WAITLOCK);

                    } else {
                        SS_PMON_ADD(SS_PMON_MYSQL_CURSOR_RESET_SIMPLE);

                        if (solid_relcur->sr_vbuf != NULL) {
                            rs_vbuf_reset(cd, solid_relcur->sr_vbuf);
                        }

                        dbe_cursor_reset(
                            solid_relcur->sr_relcur,
                            tb_trans_dbtrx(cd, trans),
                            solid_relcur->sr_ttype,
                            solid_relcur->sr_selectlist,
                            solid_relcur->sr_pla);

                        isfetch = FALSE;
                    }
                }
            }
            ss_win_perf_stop(relcur_open_perfcount, relcur_open_callcount);

            if (rs_relh_isaborted(cd, rsrelh)) {
                ss_pprintf_2(("ha_solid::solid_fetch, rs_relh_isaborted\n"));
                rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
                ss_dassert(errh == NULL);
                rs_error_create(&errh, E_DDOP);

                sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));

                rc = (dbe_ret_t) ha_solid_mysql_error(thd, errh, (int)rc);
                su_err_done(errh);
                SS_POPNAME;
                DBUG_RETURN((int)rc);
            }

            if (rowbuf == NULL) {
                ss_pprintf_2(("ha_solid::solid_fetch, rowbuf == NULL, return without fetching\n"));
                rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
                SS_POPNAME;
                DBUG_RETURN(0);
            }
        }

        if (!isfetch) {
            ss_win_perf(__int64 startcount);
            ss_win_perf(__int64 endcount);

            if (solid_relcur->sr_relcur == NULL) {
                rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
                SS_POPNAME;
                DBUG_RETURN(HA_ERR_END_OF_FILE);
            }

            ss_dassert(solid_relcur->sr_relcur != NULL);

            ss_win_perf_start;
            SS_PMON_ADD(SS_PMON_MYSQL_FETCH_NEXT);
            ss_debug(steps = 0;)

            if (solid_relcur->sr_mainmem) {
                bool waitlock;
                waitlock = FALSE;

                do {
                    if (waitlock) {
                        rc = dbe_cursor_relock(
                                solid_relcur->sr_relcur,
                                tb_trans_dbtrx(cd, trans),
                                &errh);
                        if (rc != SU_SUCCESS) {
                            break;
                        }
                        waitlock = FALSE;
                    }

                    /* Reset the vbuf if we've changed directions. */
                    if (solid_relcur->sr_prevnextp != nextp) {
                        rs_vbuf_rewind(cd, solid_relcur->sr_vbuf);
                        solid_relcur->sr_prevnextp = nextp;
                    }

                    if (!rs_vbuf_hasdata(cd, solid_relcur->sr_vbuf)) {
                        rc = dbe_cursor_nextorprev_n(
                                solid_relcur->sr_relcur,
                                nextp,
                                tb_trans_dbtrx(cd, trans),
                                solid_relcur->sr_vbuf,
                                &errh);

                        ss_debug(if (steps == 10000000) SsDbgSet("/LEV:4/FIL:ha,mme0mme,su0err/LOG/NOD/THR/TIM/LIM:500000000");)
                        ss_rc_dassert(steps < 11000000, steps);
                        ss_debug(steps++;)
                    } else {
                        rc = DBE_RC_FOUND;
                    }

                    if (rc == DBE_RC_FOUND) {
                        solid_relcur->sr_fetchtval = rs_vbuf_readtval(cd, solid_relcur->sr_vbuf);
                        if (solid_relcur->sr_fetchtval != NULL) {
                            rc = DBE_RC_FOUND;
                        } else {
                            rc = DBE_RC_END;
                        }
                    }

                    if (rc == DBE_RC_WAITLOCK) {
                        waitlock = TRUE;
                    }
                } while (rs_sysi_lockwait(cd) || rc == DBE_RC_WAITLOCK);

                ss_dassert(rc != DBE_RC_NOTFOUND);
            } else {
                do {
                    if(command == SQLCOM_ALTER_TABLE ||
                       command == SQLCOM_OPTIMIZE ||
                       command == SQLCOM_CREATE_INDEX ||
                       command == SQLCOM_DROP_INDEX) {
                        dbe_cursor_setisolation_transparent(solid_relcur->sr_relcur, TRUE);
                    }

                    do {
                        rc = dbe_cursor_nextorprev(
                                solid_relcur->sr_relcur,
                                nextp,
                                tb_trans_dbtrx(cd, trans),
                                &solid_relcur->sr_fetchtval,
                                &errh);

                        ss_debug(if (steps == 10000000) SsDbgSet("/LEV:4/FIL:ha,dbe4srch,dbe5isea,dbe6bsea,su0err/LOG/NOD/THR/TIM/LIM:500000000");)
                        ss_rc_dassert(steps < 11000000, steps);
                        ss_debug(steps++;)
                    } while (rs_sysi_lockwait(cd) || rc == DBE_RC_WAITLOCK);
                } while (rc == DBE_RC_NOTFOUND);
            }

            ss_win_perf_stop(relcur_next_perfcount, relcur_next_callcount);
        }

        if (rc != DBE_RC_FOUND && rc != DBE_RC_END) {
            rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
            ss_dassert(errh != NULL);
            sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
            rc = (dbe_ret_t)ha_solid_mysql_error(thd, errh, (int)rc);
            su_err_done(errh);
            SS_POPNAME;
            DBUG_RETURN((int)rc);
        }

        if (rc == DBE_RC_END) {
            ss_pprintf_1(("ha_solid::solid_fetch:end of set\n"));

            if (solid_relcur && solid_relcur->sr_relcur) {
                dbe_cursor_setisolation_transparent(solid_relcur->sr_relcur, FALSE);
            }

            rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
            table->status = STATUS_NOT_FOUND;
            ss_win_perf_stop(fetch_perfcount, fetch_callcount);
            SS_POPNAME;
            DBUG_RETURN(HA_ERR_END_OF_FILE);

        } else if (soliddb_sql_command(thd) != SQLCOM_REPLACE &&
                   soliddb_sql_command(thd) != SQLCOM_REPLACE_SELECT) {

#if MYSQL_VERSION_ID >= 50100
            my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
#endif

            ss_pprintf_1(("ha_solid::solid_fetch:found row\n"));
            ss_dassert(rc == DBE_RC_FOUND);

            ttype = solid_relcur->sr_ttype;
            memset(rowbuf, 0, table->s->null_bytes);
            table->status = 0;

            ss_poutput_2(rs_tval_print(cd, ttype, solid_relcur->sr_fetchtval));

            for (fieldptr = table->field, ano = 0; *fieldptr != NULL; fieldptr++, ano++) {
                int phys_ano;
                field = *fieldptr;

                if (!solid_relcur->sr_usedfields[ano]) {
                    ss_pprintf_1(("ha_solid::solid_fetch:skip field\n"));
                    continue;
                }
                phys_ano = rs_ttype_quicksqlanotophys(cd, ttype, ano);
                atype = rs_ttype_atype(cd, ttype, phys_ano);
                aval = rs_tval_aval(cd, ttype, solid_relcur->sr_fetchtval, ano);

                if (rs_aval_isnull(cd, atype, aval)) {
                    ss_pprintf_1(("ha_solid::solid_fetch:NULL\n"));
                    field->set_null();
                } else {
                    char* str;
                    ulong length;
#if MYSQL_VERSION_ID >= 50120
                    uchar* tmp_ptr;
#else
                    char* tmp_ptr;
#endif
                    ulong aval_length;

                    field->set_notnull();
                    tmp_ptr = field->ptr;

#if MYSQL_VERSION_ID >= 50120
                    field->ptr = (uchar*)rowbuf + field->offset(field->table->record[0]);
#elif MYSQL_VERSION_ID >= 50100
                    field->ptr = (char*)rowbuf + field->offset(field->table->record[0]);
#else
                    field->ptr = (char*)rowbuf + field->offset();
#endif

                    switch (rs_atype_datatype(cd, atype)) {
                        case RSDT_CHAR:
                            if (rs_aval_isblob(cd, atype, aval)) {
                                tb_blobg2_loadblobtoaval_limit(cd, atype, aval, smallblobsizemax);
                            }

                            str = (char*)rs_aval_getdata(cd, atype, aval, &aval_length);

                            switch (field->real_type()) {
                                case MYSQL_TYPE_SET:
                                case MYSQL_TYPE_ENUM:
                                    length = field->pack_length();
                                    memcpy(field->ptr, str, length);
                                    break;
                                default:

                                    sret = field->store(
                                                str,
                                                aval_length,
                                                field->charset());
                                    break;
                            }
                            break;
                        case RSDT_INTEGER:

                            if (((Field_num*)field)->unsigned_flag) {
                                sret = field->store((longlong)(unsigned long)
                                                    rs_aval_getlong(cd, atype, aval), TRUE);
                            } else {
                                sret = field->store((longlong)rs_aval_getlong(cd, atype, aval), FALSE);

                            }

                            break;
                        case RSDT_FLOAT:
                        case RSDT_DOUBLE:

                            sret = field->store(rs_aval_getdouble(cd, atype, aval));
                            break;
                        case RSDT_DATE: {
                            dt_date_t *solid_date;

                            solid_date = rs_aval_getdate(cd, atype, aval);
                            ss_dassert(solid_date != NULL);

                            ss_pprintf_2(("Fetched date: %d-%d-%d %d:%d:%d\n",
                                          dt_date_year(solid_date), dt_date_month(solid_date),
                                          dt_date_mday(solid_date), dt_date_hour(solid_date),
                                          dt_date_min(solid_date), dt_date_sec(solid_date)));

                            /* See field.cc for documentation */
                            switch(field->real_type()) {
                                case FIELD_TYPE_DATE: {
                                    uint32 tmp = 0;

                                    tmp = (uint32) dt_date_year(solid_date) *10000L +
                                        (uint32) (dt_date_month(solid_date)+dt_date_mday(solid_date));

                                    sret = field->store(tmp, (field->flags & UNSIGNED_FLAG));
                                    break;
                                }
                                case FIELD_TYPE_TIME:
                                case FIELD_TYPE_NEWDATE:
                                case FIELD_TYPE_DATETIME: {
#if MYSQL_VERSION_ID < 50045
                                    TIME ltime;
#else
                                    MYSQL_TIME ltime;
#endif

                                    ltime.year   = dt_date_year(solid_date);
                                    ltime.month  = dt_date_month(solid_date);
                                    ltime.day    = dt_date_mday(solid_date);
                                    ltime.hour   = dt_date_hour(solid_date);
                                    ltime.minute = dt_date_min(solid_date);
                                    ltime.second = dt_date_sec(solid_date);
                                    ltime.second_part = 0;
                                    ltime.neg    = 0;

                                    sret = field->store_time(&ltime, MYSQL_TIMESTAMP_DATE);
                                    break;
                                }
                                case FIELD_TYPE_TIMESTAMP: {
                                    longlong ts = 0;

                                    ts = dt_date_year(solid_date) * LL(10000000000) +
                                        dt_date_month(solid_date) * LL(100000000) +
                                        dt_date_mday(solid_date) * 1000000L +
                                        dt_date_hour(solid_date) * 10000L +
                                        dt_date_min(solid_date) * 100 +
                                        dt_date_sec(solid_date);

                                    sret = field->store(ts, (field->flags & UNSIGNED_FLAG));

                                    break;
                                }
                                default:
                                    ss_error;
                                    break;
                            }

                            break;

                        }
                        case RSDT_DFLOAT:
                            ss_error;
                            break;
                        case RSDT_BINARY:
                            {
                                void* data;
                                ulong length;
                                int mysqltype = field->type();

                                if (rs_aval_isblob(cd, atype, aval)) {
                                    tb_blobg2_loadblobtoaval_limit(cd, atype, aval, smallblobsizemax);
                                }

                                data = rs_aval_getdata(cd, atype, aval, &length);

                                switch (field->type()) {
                                    case MYSQL_TYPE_GEOMETRY:
                                    case MYSQL_TYPE_VAR_STRING: /* old <= 4.1 VARCHAR */
                                    case MYSQL_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
                                    case FIELD_TYPE_TINY_BLOB:
                                    case FIELD_TYPE_MEDIUM_BLOB:
                                    case FIELD_TYPE_BLOB:
                                    case FIELD_TYPE_LONG_BLOB:
                                        ss_pprintf_1(("ha_solid::solid_fetch:mysqltype %d, length %ld\n", mysqltype, (long)length));

                                        sret = field->store(
                                                    (const char *)data,
                                                    (uint)length,
                                                    field->charset());
                                        break;
                                    default:
                                        ss_dassert(field->pack_length() == length);
                                        memcpy(field->ptr, data, length);
                                        break;
                                }
                            }
                            break;
                        case RSDT_UNICODE:
                            if (rs_aval_isblob(cd, atype, aval)) {
                                tb_blobg2_loadblobtoaval_limit(cd, atype, aval, smallblobsizemax);
                            }

                            str = (char*)rs_aval_getdata(cd, atype, aval, &aval_length);

                            switch (field->real_type()) {
                                case MYSQL_TYPE_SET:
                                case MYSQL_TYPE_ENUM:
                                    ss_error;
                                    break;
                                default:
                                    sret = field->store(
                                                str,
                                                aval_length,
                                                field->charset());
                                    break;
                            }
                            break;
                        case RSDT_BIGINT:
                            {
                                ss_int8_t i8;

                                i8 = rs_aval_getint8(cd, atype, aval);

                                switch(field->real_type()) {
                                    case FIELD_TYPE_LONG:
                                    {
                                        longlong ll;

                                        if (field->flags & UNSIGNED_FLAG) {
                                            ll = (ulong) SsInt8GetNativeUint8(i8);
                                        } else {
                                            ll = (long) SsInt8GetNativeUint8(i8);
                                        }

                                        sret = field->store(ll, (field->flags & UNSIGNED_FLAG));
                                        break;
                                    }
                                    default:
                                    {
                                        if (field->flags & UNSIGNED_FLAG) {
                                            ulonglong ll;

                                            ll = SsInt8GetNativeUint8(i8);
                                            sret = field->store(ll, TRUE);
                                        } else {
                                            longlong ll;

                                            ll = (longlong) SsInt8GetNativeUint8(i8);
                                            sret = field->store(ll, FALSE);
                                        }
                                        break;
                                    }
                                }
                            }
                            break;
                        default:
                            ss_error;
                    }
                    field->ptr = tmp_ptr;
                    ss_poutput_2(ha_print_aval(cd, atype, aval, (char *)"ha_solid::solid_fetch:"));
                }
            }

            ss_win_perf_stop(fetch_perfcount, fetch_callcount);

#if MYSQL_VERSION_ID >= 50100
            dbug_tmp_restore_column_map(table->write_set, old_map);
#endif

            rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
            SS_POPNAME;
            DBUG_RETURN(0);
        } else {
            rs_sysi_clearflag(cd, RS_SYSI_FLAG_SEARCH_SIMPLETVAL);
            SS_POPNAME;
            DBUG_RETURN(0);
        }
}

/*#***********************************************************************\
 *
 *              ::index_init
 *
 *   Initialize a handle to use an index.
 *
 *
 * Parameters :
 *
 *     uint   keynr, in, use, key number
 *     bool   sorted, not used
 *
 * Return value : 0
 *
 * Globals used :
 */

int ha_soliddb::index_init(
        uint keynr
#if MYSQL_VERSION_ID >= 50100
        ,bool sorted __attribute__(unused)
#endif
    )

{
        DBUG_ENTER("index_init");

        active_index = keynr;

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::solid_index_read_idx
 *
 * Index read
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*          buf, in out, row buffer
 *     uint                   index, in, index number
 *     const SS_MYSQL_ROW*    key, in, use
 *     uint                   key_len, in, use, key length
 *     enum ha_rkey:function  find_flag, in, use
 *     bool                   firstp, in, use
 *     THD*                   thd, in, use
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::solid_index_read_idx(
        SS_MYSQL_ROW* buf,
        uint index,
        const SS_MYSQL_ROW* key,
        uint key_len,
        enum ha_rkey_function find_flag,
        bool firstp,
        MYSQL_THD thd)
{
        int           rc;
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)

        ss_win_perf_start;
        DBUG_ENTER("ha_soliddb::solid_index_read_idx");
        ss_pprintf_1(("ha_solid::solid_index_read\n"));

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);
        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

        if (solid_table == NULL) {
            int rc;
            rc = ha_solid_mysql_error(thd, con->sc_errh, 0);
            ss_pprintf_1(("ha_solid::table not found\n"));
            DBUG_RETURN(rc);
        }

        if (tb_trans_dbtrx(cd, trans) == NULL) {
            int rc;
            ss_dassert(tb_trans_getdberet(cd, trans) != SU_SUCCESS);
            rc = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
            DBUG_RETURN(rc);
        }

        SS_SETSQLSTR((char *)soliddb_query(thd));

        solid_trans_beginif((handlerton *)ht, thd, FALSE);

        solid_relcur = solid_relcur_create(
                            con,
                            solid_relcur,
                            solid_table->st_rsrelh,
                            table,
                            thd,
                            index,
                            &table->key_info[index]);

        rc = solid_relcur_setconstr(
                thd,
                cd,
                trans,
                solid_table->st_rsrelh,
                solid_relcur,
                index,
                &table->key_info[index],
                table,
                key,
                key_len,
                find_flag);

        if (rc == 0) {
            if (!firstp) {
                dbe_ret_t dberc;
                su_err_t* errh = NULL;

                /* Create dbe cursor with NULL buf in fetch. */
                rc = solid_fetch(NULL, firstp, thd, 0);

                if (rc == 0) {
                    ss_pprintf_2(("ha_solid::solid_index_read, dbe_cursor_gotoend\n"));
                    do {
                        dberc = dbe_cursor_gotoend(
                                    solid_relcur->sr_relcur,
                                    tb_trans_dbtrx(cd, trans),
                                    &errh);
                    } while (rs_sysi_lockwait(cd) ||dberc == DBE_RC_WAITLOCK);

                    su_rc_assert(dberc == DBE_RC_SUCC, dberc);
                }
            }

            if (rc == 0) {
                rc = solid_fetch(buf, firstp, thd, 0);
            }
        }

        SS_CLEARSQLSTR;
        ss_win_perf_stop(index_read_idx_perfcount, index_read_idx_callcount);

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_read
 *
 * Positions an index cursor to the index specified in the handle. Fetches the
 * row if available. If the key value is null, begin at the first key of the
 * index.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*         buf, in out, row buffer
 *     const SS_MYSQL_ROW*   key, in, use,
 *     uint key_len          key_len, in, use
 *     enum ha_rkey_function find_flag, in
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::index_read(
        SS_MYSQL_ROW* buf,
        const SS_MYSQL_ROW* key,
        uint key_len,
        enum ha_rkey_function find_flag)
{
        int rc;
        MYSQL_THD thd = current_thd;

        DBUG_ENTER("ha_soliddb::index_read");
        ss_pprintf_1(("ha_solid::index_read\n"));

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_key_count);
#else
        statistic_increment(thd->status_var.ha_read_key_count, &LOCK_status);
#endif

        if (find_flag == HA_READ_KEY_OR_PREV ||
            find_flag == HA_READ_PREFIX_LAST_OR_PREV ||
            find_flag == HA_READ_BEFORE_KEY) {
            rc = solid_index_read_idx(buf, active_index, key, key_len, find_flag, FALSE, thd);
        } else {
            rc = solid_index_read_idx(buf, active_index, key, key_len, find_flag, TRUE, thd);
        }

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_read_last
 *
 * Position to the last row in the index with the current key value or prefix.
 *
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*         buf, in out, row buffer
 *     const SS_MYSQL_ROW*   key, in, use,
 *     uint key_len          key_len, in, use
 *  *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::index_read_last(
        SS_MYSQL_ROW* buf,
        const SS_MYSQL_ROW* key,
        uint key_len)
{
        int rc;
        MYSQL_THD thd = current_thd;

        DBUG_ENTER("ha_soliddb::index_read_last");
        ss_pprintf_1(("ha_solid::index_read_last\n"));

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_key_count);
#else
        statistic_increment(thd->status_var.ha_read_key_count, &LOCK_status);
#endif

        rc = solid_index_read_idx(buf, active_index, key, key_len, HA_READ_PREFIX_LAST_OR_PREV, FALSE, thd);

        DBUG_RETURN(rc);
}

#if MYSQL_VERSION_ID < 50100
/*#***********************************************************************\
 *
 *              ::index_read_idx
 *
 * Positions an index cursor to the index specified in key. Fetches the
 * row if any.  This is only used to read whole keys.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*          buf, in out, row buffer
 *     uint                   index, in, index number
 *     const SS_MYSQL_ROW*    key, in, use
 *     uint                   key_len, in, use, key length
 *     enum ha_rkey_function  find_flag, in, use
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_read_idx(
        SS_MYSQL_ROW* buf,
        uint index,
        const SS_MYSQL_ROW* key,
        uint key_len,
        enum ha_rkey_function find_flag)
{
        int rc = 0;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::index_read_idx");
        ss_pprintf_1(("ha_solid::index_read_idx\n"));
        SS_PMON_ADD(SS_PMON_MYSQL_INDEX_READ);

        thd = current_thd;

        statistic_increment(thd->status_var.ha_read_key_count, &LOCK_status);

        rc = solid_index_read_idx(buf, index, key, key_len, find_flag, TRUE, thd);

        DBUG_RETURN(rc);
}
#endif /* MYSQL_VERSION_ID < 50100 */

/*#***********************************************************************\
 *
 *              ::index_next
 *
 * Used to read forward through the index.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*  buf, in out, row buffer
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_next(SS_MYSQL_ROW* buf)
{
        int rc;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::index_next");
        ss_pprintf_1(("ha_solid::index_next\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_next_count);
#else
        statistic_increment(thd->status_var.ha_read_next_count, &LOCK_status);
#endif

        rc = solid_fetch(buf, TRUE, thd, 0);

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_next_same
 *
 * Read the next row matching to the key value given.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*         buf, in out, row buffer
 *     const SS_MYSQL_ROW*   key, in, use, key value
 *     uint                  keylen, in, use, key value length
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_next_same(
        SS_MYSQL_ROW*        buf,
        const SS_MYSQL_ROW*  key,
        uint                 keylen)
{
        int rc;
        MYSQL_THD thd;

        SDB_DBUG_ENTER("ha_soliddb::index_next_same");

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_next_count);
#else
        statistic_increment(thd->status_var.ha_read_next_count, &LOCK_status);
#endif

        rc = solid_fetch(buf, TRUE, thd, 0);

        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_prev
 *
 * Used to read backwards through the index.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*  buf, in out, row buffer
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_prev(SS_MYSQL_ROW* buf)
{
        int rc;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::index_prev");
        ss_pprintf_1(("ha_solid::index_prev\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_prev_count);
#else
        statistic_increment(thd->status_var.ha_read_prev_count,  &LOCK_status);
#endif

        rc = solid_fetch(buf, FALSE, thd, 0);

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_first
 *
 * index_first() asks for the first key in the index.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*   buf, in out, row buffer
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_first(SS_MYSQL_ROW* buf)
{
      int rc;
      MYSQL_THD thd;

      DBUG_ENTER("ha_soliddb::index_first");
      ss_pprintf_1(("ha_solid::index_first\n"));

      thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_first_count);
#else
      statistic_increment(thd->status_var.ha_read_first_count, &LOCK_status);
#endif

      rc = solid_index_read_idx(buf, active_index, NULL, 0, HA_READ_AFTER_KEY, TRUE, thd);

      DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::index_last
 *
 * index_last() asks for the last key in the index.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*   buf, in out, row buffer
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::index_last(SS_MYSQL_ROW* buf)
{
        int rc;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::index_last");
        ss_pprintf_1(("ha_solid::index_last\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        ha_statistic_increment(&SSV::ha_read_last_count);
#else
        statistic_increment(thd->status_var.ha_read_last_count, &LOCK_status);
#endif

        rc = solid_index_read_idx(buf, active_index, NULL, 0, HA_READ_KEY_OR_PREV, FALSE, thd);

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::rnd_init
 *
 * rnd_init() is called when the system wants the storage engine to do a table
 * scan.
 *
 * See the example in the introduction at the top of this file to see when
 * rnd_init() is called.
 *
 * Parameters :
 *
 *     bool  scan, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::rnd_init(bool scan)
{
        DBUG_ENTER("ha_soliddb::rnd_init");
        ss_pprintf_1(("ha_solid::rnd_init\n"));
        SS_PMON_ADD(SS_PMON_MYSQL_RND_INIT);

        rnd_mustinit = TRUE;
        active_index = 0;
	stats.records = 0;

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::rnd_end
 *
 * rnd_init() is called when the system wants the storage engine to do stop
 * a table scan.
 *
 * Parameters : -
 *
 * Return value : 0
 *
 * Globals used :
 */
int ha_soliddb::rnd_end()
{
        DBUG_ENTER("ha_soliddb::rnd_end");
        ss_pprintf_1(("ha_solid::rnd_end\n"));

        active_index = 0;

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::rnd_next
 *
 * This is called for each row of the table scan. When you run out of records
 * you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
 * The Field structure for the table is the key to getting data into buf
 * in a manner that will allow the server to understand it.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*   buf, in out, row buffer
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::rnd_next(SS_MYSQL_ROW* buf)
{
        int rc;
        MYSQL_THD thd;
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;

        DBUG_ENTER("ha_soliddb::rnd_next");
        ss_pprintf_1(("ha_solid::rnd_next\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
        ha_statistic_increment(&SSV::ha_read_rnd_next_count);
#else
        con = get_solid_connection(thd, table->s->path);
        statistic_increment(thd->status_var.ha_read_rnd_next_count, &LOCK_status);
#endif

        CHK_CONN(con);

        if (solid_table == NULL) {
            rc = HA_ERR_NO_SUCH_TABLE;
            DBUG_RETURN(rc);
        }

        ss_dassert(solid_table != NULL);
        ss_dassert(con != NULL);
        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

        if (rnd_mustinit) {

            SS_SETSQLSTR((char *)soliddb_query(thd));

            solid_trans_beginif((handlerton *)ht, thd, FALSE);

            solid_relcur = solid_relcur_create(
                                con,
                                solid_relcur,
                                solid_table->st_rsrelh,
                                table,
                                thd,
                                (unsigned int)TABLE_SCAN_INDEX,
                                NULL);

            solid_relcur->sr_full_scan = TRUE;
            solid_relcur->sr_key = NULL;
            solid_relcur->sr_index = TABLE_SCAN_INDEX;

            rc = solid_relcur_setconstr(
                    thd,
                    cd,
                    trans,
                    solid_table->st_rsrelh,
                    solid_relcur,
                    0,
                    NULL,
                    NULL,
                    NULL,
                    0,
                    HA_READ_AFTER_KEY);

            rnd_mustinit = FALSE;

            SS_CLEARSQLSTR;
        }

        rc = solid_fetch(buf, TRUE, thd, soliddb_sql_command(thd));
	stats.records++;

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              print_ref
 *
 * TODO: add description of this function
 *
 * Parameters :
 *
 *     SOLID_CONN*      con, in, use, solidDB connection
 *     solid_relcur_t*  solid_relcur, in, use, relation cursor
 *     SS_MYSQL_ROW*            pos, in ,use
 *
 * Return value : -
 *
 * Globals used :
 */
static void print_ref(SOLID_CONN* con, solid_relcur_t* solid_relcur, SS_MYSQL_ROW* pos)
{
        vtpl_t* vtpl;
        rs_relh_t* rsrelh;
        rs_key_t* key;
        rs_ano_t nrefparts;
        rs_ano_t i;
        rs_sysi_t* cd;
        rs_ttype_t* ttype;
        rs_tval_t* tval;

        cd = con->sc_cd;
        vtpl = (vtpl_t*)pos;
        rsrelh = solid_relcur->sr_rsrelh;
        key = rs_relh_clusterkey(cd, rsrelh);
        nrefparts = rs_key_nrefparts(cd, key);
        ttype = solid_relcur->sr_ttype;
        tval = rs_tval_create(cd, ttype);

        for (i = 0; i < nrefparts; i++) {
            rs_ano_t ano;

            ano = rs_keyp_ano(cd, key, i);
            if (ano != RS_ANO_NULL) {
                va_t* va;

                va = vtpl_getva_at(vtpl, i);
                ss_dassert(va != NULL);
                rs_tval_setva(cd, ttype, tval, ano, va);
            }
        }

        rs_tval_print(cd, ttype, tval);

        rs_tval_free(cd, ttype, tval);
}

/*#***********************************************************************\
 *
 *              ::position
 *
 * position() is called after each call to rnd_next() if the data needs
 * to be ordered. You can do something like the following to store
 * the position:
 * my_store_ptr(ref, ref_length, current_position);
 *
 * The server uses ref to store data. ref_length in the above case is
 * the size needed to store current_position. ref is just a byte array
 * that the server will maintain. If you are using offsets to mark rows, then
 * current_position should be the offset. If it is a primary key like in
 * BDB, then it needs to be a primary key.
 *
 * Parameters :
 *
 *     const SS_MYSQL_ROW*  record, in, use
 *
 * Return value : -
 *
 * Globals used : THD* current_thd
 */
void ha_soliddb::position(const SS_MYSQL_ROW* record)
{
        SOLID_CONN* con;
        rs_sysi_t* cd;
        void* tref;
        vtpl_t* vtpl;
        size_t data_len;
        MYSQL_THD thd;

        DBUG_ENTER("ha_soliddb::position");
        ss_pprintf_1(("ha_solid::position\n"));
        CHK_RELCUR(solid_relcur);
        ss_dassert(solid_relcur->sr_relcur != NULL);

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);
        cd = con->sc_cd;

        tref = dbe_cursor_gettref(solid_relcur->sr_relcur, solid_relcur->sr_fetchtval);

        if (solid_relcur->sr_mainmem) {
            void* refdata;
            refdata = dbe_tref_getrefrvaldata((dbe_tref_t*)tref, &data_len);

            /* Copy tuple reference to buffer. */
            memcpy(ref, refdata, data_len);
        } else {
            vtpl = dbe_tref_getvtpl((dbe_tref_t*)tref);
            data_len = vtpl_grosslen(vtpl);
            ss_dassert(data_len <= ref_length);

            /* Copy tuple reference v-tuple to buffer. */
            memcpy(ref, vtpl, data_len);
        }

        /* Clear rest of buffer because MySQL may call memcmp
         * for ref_length length.
         */
        memset((char*)ref + data_len, '\0', ref_length - data_len);

        ss_poutput_1(if (!solid_relcur->sr_mainmem) print_ref(con, solid_relcur, (SS_MYSQL_ROW*)ref));

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              ::rnd_pos
 *
 * This is like rnd_next, but you are given a position to use
 * to determine the row. The position will be of the type that you stored in
 * ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
 * or position you saved when position() was called.
 *
 * Parameters :
 *
 *     SS_MYSQL_ROW*  buf, in out, row buffer
 *     SS_MYSQL_ROW*  pos, in, use, position
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::rnd_pos(SS_MYSQL_ROW* buf, SS_MYSQL_ROW* pos)
{
        int rc;
        rs_relh_t* rsrelh;
        rs_ano_t nrefparts;
        rs_ano_t i;
        rs_sysi_t* cd;
        tb_trans_t* trans;
        rs_ttype_t* ttype;
        rs_tval_t* tval;
        SOLID_CONN* con;
        MYSQL_THD thd;
        su_ret_t suret;
        rs_key_t* key;
        uint old_key;

        DBUG_ENTER("ha_soliddb::rnd_pos");
        ss_pprintf_1(("ha_solid::rnd_pos\n"));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
        ha_statistic_increment(&SSV::ha_read_rnd_count);
#else
        con = get_solid_connection(thd, table->s->path);
        statistic_increment(thd->status_var.ha_read_rnd_count, &LOCK_status);
#endif

        CHK_CONN(con);
        cd = con->sc_cd;
        trans = con->sc_trans;

        old_key = active_index;
        active_index = 0;

        if (solid_table == NULL) {
            int rc=0;
            rc = ha_solid_mysql_error(thd, con->sc_errh, rc);
            ss_pprintf_1(("ha_solid::table not found\n"));
            DBUG_RETURN(rc);
        }
        if (tb_trans_dbtrx(cd, trans) == NULL) {
            int rc;
            ss_dassert(tb_trans_getdberet(cd, trans) != SU_SUCCESS);
            rc = ha_solid_mysql_error(thd, NULL, tb_trans_getdberet(cd, trans));
            DBUG_RETURN(rc);
        }

        if (rnd_mustinit) {

            SS_SETSQLSTR((char *)soliddb_query(thd));

            solid_trans_beginif((handlerton *)ht, thd, FALSE);

            solid_relcur = solid_relcur_create(
                                con,
                                solid_relcur,
                                solid_table->st_rsrelh,
                                table,
                                thd,
                                TABLE_SCAN_INDEX,
                                NULL);

            solid_relcur->sr_full_scan = TRUE;
            solid_relcur->sr_key = NULL;
            solid_relcur->sr_index = TABLE_SCAN_INDEX;

            rc = solid_relcur_setconstr(
                    thd,
                    cd,
                    trans,
                    solid_table->st_rsrelh,
                    solid_relcur,
                    (unsigned int)TABLE_SCAN_INDEX,
                    NULL,
                    NULL,
                    NULL,
                    0,
                    HA_READ_AFTER_KEY);

            rnd_mustinit = FALSE;

            if (rc == 0) {
                /* Just reset the cursor, buf == NULL. */
                rc = solid_fetch(NULL, TRUE, thd, 0);
            }

            SS_CLEARSQLSTR;
        }

        CHK_RELCUR(solid_relcur);
        ss_dassert(solid_relcur->sr_relcur != NULL);

        ss_poutput_1(if (!solid_relcur->sr_mainmem) print_ref(con, solid_relcur, pos));

        rsrelh = solid_table->st_rsrelh;
        key = rs_relh_clusterkey(cd, rsrelh);
        ttype = solid_relcur->sr_ttype;
        if (solid_relcur->sr_postval == NULL) {
            solid_relcur->sr_postval = rs_tval_create(cd, ttype);
        }
        tval = solid_relcur->sr_postval;

        if (solid_relcur->sr_mainmem) {
            dbe_tref_projectrvaltotval(
                cd,
                (ss_byte_t*)pos,
                ref_length,
                ttype,
                tval,
                key);
        } else {
            vtpl_t* vtpl;

            vtpl = (vtpl_t*)pos;
            nrefparts = rs_key_nrefparts(cd, key);

            for (i = 0; i < nrefparts; i++) {
                rs_ano_t ano;

                ano = rs_keyp_ano(cd, key, i);
                if (ano != RS_ANO_NULL) {
                    va_t* va;

                    va = vtpl_getva_at(vtpl, i);
                    ss_dassert(va != NULL);
                    rs_tval_setvaref_flat(cd, ttype, tval, ano, va);
                }
            }
        }

        do {
            suret = dbe_cursor_setposition(
                        solid_relcur->sr_relcur,
                        tb_trans_dbtrx(cd, trans),
                        tval,
                        NULL);
        } while (rs_sysi_lockwait(cd) || suret == DBE_RC_WAITLOCK);
        ss_rc_assert(suret == DBE_RC_SUCC, suret);

        rc = solid_fetch(buf, TRUE, thd, 0);

        active_index = old_key;

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::info
 *
 * ::info() is used to return information to the optimizer.
 * Currently this table handler doesn't implement most of the fields
 * really needed. SHOW also makes use of this data
 * Another note, you will probably want to have the following in your
 * code:
 * if (records < 2)
 *   records = 2;
 * The reason is that the server will optimize for cases of only a single
 * record. If in a table scan you don't know the number of records
 * it will probably be better to set records to two so you can return
 * as many records as you need.
 * Along with records a few more variables you may wish to set are:
 *   records
 *   deleted
 *   data_file_length
 *   index_file_length
 *   delete_length
 *   check_time
 *
 * Parameters :
 *
 *     uint  flag, in, use, status variable
 *
 * Return value : -
 *
 * Globals used : THD* current_thd
 */
#if MYSQL_VERSION_ID >= 50030
  int
#else
  void
#endif
ha_soliddb::info(uint flag)
{
        ss_int8_t     i8;
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        double        ntuples;
        double        nbytes;
        double        nindexbytes;
        uint          i;
        uint          j;
        su_pa_t*      keys;
        rs_key_t*     key;
        MYSQL_THD thd;
        rs_relh_t*    s_relh = NULL;
        bool          succp= TRUE;

        DBUG_ENTER("ha_soliddb::info");
        ss_pprintf_1(("ha_solid::info, %s, flag=%d\n", table->s->path, flag));
        SS_PUSHNAME("ha_soliddb::info");

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        if (solid_table == NULL) {
            int rc=0;
            rc = ha_solid_mysql_error(thd, con->sc_errh, rc);
            ss_pprintf_1(("ha_solid::info:table not found\n"));
            SS_POPNAME;

#if MYSQL_VERSION_ID >= 50030
            DBUG_RETURN(0);
#else
            DBUG_VOID_RETURN;
#endif
        }

        cd = con->sc_cd;
        s_relh = solid_table->st_rsrelh;
        ss_dassert(s_relh != NULL);

        if (flag & HA_STATUS_TIME) {
            dt_date_t     relh_createtime;
            dt_date_t     relh_updatetime;

            /* Get the create_time and update_time of the table */

            tb_dd_get_createtime(cd, con->sc_trans, s_relh, &relh_createtime);

            /* Update time is normally updated only on checkpoint. If no update
               time can be found for this table use create time. */

            if (!(tb_dd_get_updatetime(cd, con->sc_trans, s_relh, &relh_updatetime))) {
                memcpy(&relh_updatetime, &relh_createtime, sizeof(dt_date_t));
            }

#if MYSQL_VERSION_ID >= 50100
            stats.create_time = ha_solid_time_mysql(&relh_createtime);
            stats.update_time = ha_solid_time_mysql(&relh_updatetime);
#else
            create_time = ha_solid_time_mysql(&relh_createtime);
            update_time = ha_solid_time_mysql(&relh_updatetime);
#endif
        }

        if (flag & HA_STATUS_VARIABLE) {
            double avg_rowlen;

            ss_pprintf_1(("ha_solid::info, HA_STATUS_VARIABLE\n"));

            i8 = rs_relh_ntuples(cd, s_relh);
            succp = SsInt8ConvertToDouble(&ntuples, i8);
            ss_dassert(succp);

            if (ntuples <= 2.0) {
                ntuples = 2.0;
            }

#if MYSQL_VERSION_ID >= 50100
            stats.records = (ha_rows)ntuples;
            stats.deleted = 0;
#else
            records = (ha_rows)ntuples;
            deleted = 0;
#endif

            ss_pprintf_2(("ha_solid::info, HA_STATUS_VARIABLE, records=%.1lf\n", ntuples));


            i8 = rs_relh_nbytes(cd, s_relh);
            succp = SsInt8ConvertToDouble(&nbytes, i8);
            ss_dassert(succp);

            if (nbytes <= 20.0) {
                nbytes = 20.0;
            }

            keys = rs_relh_keys(cd, s_relh);

#if MYSQL_VERSION_ID >= 50100
            stats.data_file_length = (ulonglong)nbytes;
#else
            data_file_length = (ulonglong)nbytes;
#endif

            ss_pprintf_2(("ha_solid::info, HA_STATUS_VARIABLE, data_file_length=%.1lf\n", nbytes));

            nindexbytes = 0.0;
            avg_rowlen = nbytes / ntuples;

            su_pa_do(keys, i) {
                double maxstoragelen;
                key = (rs_key_t*)su_pa_getdata(keys, i);

                if (rs_key_isclustering(cd, key)) {
                    continue;
                }

                maxstoragelen = (double)rs_key_maxstoragelen(cd, key);

                if (maxstoragelen > avg_rowlen * 0.8) {
                    maxstoragelen = avg_rowlen * 0.8;
                }

                nindexbytes = nindexbytes + maxstoragelen * ntuples;
            }

            if (nindexbytes > 0.9 * nbytes) {
                nindexbytes = 0.9 * nbytes;
            }

#if MYSQL_VERSION_ID >= 50100
            stats.delete_length = 0;
            stats.check_time = 0;
            stats.mean_rec_length = (ulong)avg_rowlen;
            stats.index_file_length = (ulonglong)nindexbytes;
#else
            delete_length = 0;
            check_time = 0;
            mean_rec_length = (ulong)avg_rowlen;
            index_file_length = (ulonglong)nindexbytes;
#endif

            ss_pprintf_2(("ha_solid::info, HA_STATUS_VARIABLE, index_file_length=%.1lf\n", nindexbytes));
            ss_pprintf_2(("ha_solid::info, HA_STATUS_VARIABLE, mean_rec_length=%.1lf\n", avg_rowlen));
        }

        if (flag & HA_STATUS_CONST) {
            int kpno;
            int ano;
            long ano_rowcount;

            ss_pprintf_1(("ha_solid::info, HA_STATUS_CONST\n"));

            i8 = rs_relh_ntuples(cd, s_relh);
            succp = SsInt8ConvertToDouble(&ntuples, i8);
            ss_dassert(succp);

            if (ntuples <= 2.0) {
                ntuples = 2.0;
            }

            if (ntuples > LONG_MAX) {
                ano_rowcount = LONG_MAX;
            } else {
                ano_rowcount = (long)ntuples;
            }

            if (rs_sysi_simpleoptimizerrules(cd, ntuples)) {

                ss_pprintf_2(("ha_solid::info, HA_STATUS_CONST, simple rules, ntuples=%.1ld, set all to 1\n", ntuples));

                for (i = 0; i < table->s->keys; i++) {
                    for (j = 0; j < table->key_info[i].key_parts; j++) {
                        table->key_info[i].rec_per_key[j] = (ha_rows)1;
                    }
                }
            } else {

                tb_est_ensureselectivityinfo(cd, s_relh);

                for (i = 0; i < table->s->keys; i++) {
                    long diffrowcount=0;

                    key = solid_resolve_key(
                                cd,
                                s_relh,
                                &table->key_info[i]);

                    /* Skip all key parts that are not user defined. */
                    for (kpno = 0; ; kpno++) {
                        rs_attrtype_t kptype = rs_keyp_parttype(cd, key, kpno);

                        if (kptype == RSAT_USER_DEFINED ||
                            kptype == RSAT_COLLATION_KEY) {
                            break;
                        }
                    }

                    for (j = 0; j < table->key_info[i].key_parts; j++, kpno++) {
                        ano = rs_keyp_ano(cd, key, kpno);
                        ss_dassert(ano != RS_ANO_NULL);

                        diffrowcount = rs_relh_getdiffrowcount(
                                            cd,
                                            s_relh,
                                            ano,
                                            ano_rowcount);

                        if (diffrowcount > 0) {
                            table->key_info[i].rec_per_key[j] = (ha_rows)diffrowcount / 2;
                        } else {
                            table->key_info[i].rec_per_key[j] = 1;
                        }

                        if (table->key_info[i].rec_per_key[j] == 0) {
                            table->key_info[i].rec_per_key[j] = 1;
                        }

                        ss_pprintf_2(("ha_solid::info, HA_STATUS_CONST, key=%s, ano=%d, diffrowcount=%ld, set all to 1\n",
                            rs_key_name(cd, key), ano, diffrowcount));
                    }
                }
            }
        }

        /* Fetch auto increment value of the table */
        if (flag & HA_STATUS_AUTO && table->found_next_number_field) {
            ss_int8_t i8;
            long seq_id=0;
            rs_auth_t* auth;
            rs_atype_t* atype;
            rs_aval_t*  aval;
            bool succp;
            solid_bool finishedp;

            auth = rs_sysi_auth(con->sc_cd);
            ss_dassert(auth != NULL);
            rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

            seq_id = rs_relh_readautoincrement_seqid(con->sc_cd, s_relh);

            atype = rs_atype_initbigint(con->sc_cd);
            aval = rs_aval_create(con->sc_cd, atype);

            succp = tb_seq_current(con->sc_cd, con->sc_trans, seq_id,
                                       FALSE, atype, aval, &finishedp, NULL);

            i8 = rs_aval_getint8(con->sc_cd, atype, aval);

            /* Note that sequence contains last used value. Therefore,
               increase value by one before returning. */

#if MYSQL_VERSION_ID >= 50100
            stats.auto_increment_value = SsInt8GetNativeUint8(i8) + 1;
#else
            auto_increment_value = SsInt8GetNativeUint8(i8) + 1;
#endif

            rs_aval_free(con->sc_cd, atype, aval);
            rs_atype_free(con->sc_cd, atype);
            rs_auth_setsystempriv(con->sc_cd, auth, FALSE);
        }

        if (flag & HA_STATUS_ERRKEY && con->sc_errkey && con->sc_err_tableid) {

            rs_relh_t* relh =
                tb_dd_getrelhbyid(con->sc_cd, con->sc_trans,
                                  con->sc_err_tableid, NULL, NULL);
            errkey = solid_key_to_mysqlkeyidx(con->sc_cd, relh, rs_key_id(con->sc_cd,con->sc_errkey));
            rs_relh_done(con->sc_cd, relh);
        }

        SS_POPNAME;

#if MYSQL_VERSION_ID >= 50030
        DBUG_RETURN(0);
#else
        DBUG_VOID_RETURN;
#endif
}

/*#***********************************************************************\
 *
 *              ::extra
 *
 * extra() is called whenever the server wishes to send a hint to
 * the storage engine.
 *
 * Parameters :
 *
 *     enum ha_extra_function  operation, in
 *
 * Return value : 0 (or error code)
 *
 * Globals used :
 */
int ha_soliddb::extra(
        enum ha_extra_function operation)
{
        DBUG_ENTER("ha_soliddb::extra");
        ss_pprintf_1(("ha_solid::extra:operation=%d\n", (int)operation));

        switch (operation) {
            case HA_EXTRA_RESET_STATE:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_RESET_STATE\n"));
                extra_keyread = FALSE;
                extra_retrieve_primary_key = FALSE;
                extra_retrieve_all_cols = FALSE;
                extra_ignore_duplicate = FALSE;
                extra_replace_duplicate = FALSE;
                extra_update_duplicate = FALSE;
                break;
            case HA_EXTRA_NO_KEYREAD:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_NO_KEYREAD\n"));
                extra_keyread = FALSE;
                extra_retrieve_all_cols = TRUE;
                break;
#if MYSQL_VERSION_ID >= 50100
            case HA_EXTRA_INSERT_WITH_UPDATE:
                extra_update_duplicate = TRUE;
                break;
            case HA_EXTRA_IGNORE_DUP_KEY:
                extra_ignore_duplicate = TRUE;
                break;
            case HA_EXTRA_WRITE_CAN_REPLACE:
                extra_replace_duplicate = TRUE;
                break;
            case HA_EXTRA_WRITE_CANNOT_REPLACE:
                extra_replace_duplicate = FALSE;
                break;
            case HA_EXTRA_NO_IGNORE_DUP_KEY:
                extra_ignore_duplicate = FALSE;
                extra_update_duplicate = FALSE;
                break;
#else
            case HA_EXTRA_RESET:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_RESET\n"));
                extra_keyread = FALSE;
                extra_retrieve_primary_key = FALSE;
                extra_retrieve_all_cols = FALSE;
                extra_ignore_duplicate = FALSE;
                extra_replace_duplicate = FALSE;
                extra_update_duplicate = FALSE;
                break;
            case HA_EXTRA_RETRIEVE_ALL_COLS:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_RETRIEVE_ALL_COLS\n"));
                extra_retrieve_all_cols = TRUE;
                extra_keyread = FALSE;
                break;
            case HA_EXTRA_RETRIEVE_PRIMARY_KEY:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_RETRIEVE_PRIMARY_KEY\n"));
                extra_retrieve_primary_key = TRUE;
                break;
#endif
            case HA_EXTRA_KEYREAD:
                ss_pprintf_2(("ha_solid::extra:HA_EXTRA_KEYREAD\n"));
                extra_keyread = TRUE;
                extra_retrieve_all_cols = FALSE;
                break;
            default:
                break;
        }

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::scan_time
 *
 * Called in test_quick_select to determine if indexes should be used.
 * At the moment returns number or tuples on a relation.
 *
 * Parameters : -
 *
 * Return value : double
 *
 * Globals used : THD* current_thd
 */
double ha_soliddb::scan_time()
{
        ss_int8_t     i8;
        SOLID_CONN*   con;
        double        t;
        bool          succp=TRUE;
        MYSQL_THD thd;

        ss_pprintf_1(("ha_solid::scan_time, %s\n", table->s->path));

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        if (solid_table == NULL) {
            int rc = 0;
            ha_solid_mysql_error(thd, con->sc_errh, rc);
            ss_pprintf_1(("ha_solid::table not found\n"));
            return UNKNOWN_NUMBER_OF_TUPLES_IN_TABLE;
        }

        i8 = rs_relh_nbytes(con->sc_cd,solid_table->st_rsrelh);

        succp = SsInt8ConvertToDouble(&t, i8);
        ss_dassert(succp);

        ss_pprintf_2(("ha_solid::scan_time, nbytes=%.1lf\n", t));

        if (soliddb_db_block_size != 0) {
            t = ((double)t / (double)soliddb_db_block_size) / (double)2.0;
        } else {
            t = ((double)t / (double)DBE_DEFAULT_INDEXBLOCKSIZE) / (double)2.0;
        }

        if (t <= 1.0) {
            t = 1.0;
        }

        ss_pprintf_2(("ha_solid::scan_time, t=%.1lf\n", t));

        return t;
}

/*#***********************************************************************\
 *
 *              ::read_time
 *
 * The next method will never be called if you do not implement indexes.
 *
 * Parameters :
 *
 *     uint     index, in, index number
 *     uint     ranges, in
 *     ha_rows  rows, in
 *
 * Return value : double, read time
 *
 * Globals used : THD* current_thd
 */
double ha_soliddb::read_time(
        uint index,
        uint ranges,
        ha_rows rows)
{
        double t;
        MYSQL_THD thd;

        if (index != table->s->primary_key) {
            /* Not clustered */
            return(handler::read_time(index, ranges, rows));
        }

        if (rows <= 2) {

            return((double) rows);
        }

        ss_pprintf_1(("ha_solid::read_time, %s, index=%d\n", table->s->path, index));

        thd = current_thd;

        t = scan_time();

        ss_pprintf_2(("ha_solid::read_time, clustering key, t=%.1lf\n", t));

        /* TODO: return something real here ! */
        return (ranges + (double) rows / (double)t + t);
}

/*#***********************************************************************\
 *
 *              solid_get_key_limits
 *
 * TODO: Provide description of this function
 *
 * Parameters : -
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int solid_get_key_limits(
        MYSQL_THD thd,
        rs_sysi_t* cd,
        rs_relh_t* rsrelh,
        TABLE* table,
        KEY* key_info,
        rs_key_t* key,
        const SS_MYSQL_ROW* keyconstr_ptr,
        uint keyconstr_len,
        enum ha_rkey_function find_flag,
        rs_tval_t** tval,
        int* relopno,
        int* relopsize,
        int** relops,
        int** anos)
{
        KEY_PART_INFO* key_part;
        int kpno;
        int start_kpno;
        int nkeyparts;
        rs_ttype_t* ttype;
        const SS_MYSQL_ROW* ptr;
        uint key_len;
        bool succp=TRUE;
        su_err_t* errh = NULL;
        int new_relopsize;

        DBUG_ENTER("ha_solid::solid_get_key_limits");
        ss_pprintf_1(("ha_solid::solid_get_key_limits:key %s, find_flag %d\n", key_info->name, (int)find_flag));

        key_part = key_info->key_part;

        ttype = rs_relh_ttype(cd, rsrelh);

        new_relopsize = rs_key_nparts(cd, key) * sizeof((*relops)[0]);

        if (*relopsize == 0) {
            *tval = rs_tval_create(cd, ttype);
            *relops = (int *)SsMemAlloc(new_relopsize);
            *anos = (int *)SsMemAlloc(new_relopsize);
        } else if (new_relopsize > *relopsize) {
            /* We need to create bigger buffars. */
            SsMemFree(*relops);
            SsMemFree(*anos);
            *relops = (int *)SsMemAlloc(new_relopsize);
            *anos = (int *)SsMemAlloc(new_relopsize);
        }

        *relopsize = new_relopsize;

        /* Init with illegal relop values.
         */
        ss_debug(memset(*relops, '\xff', new_relopsize));

        nkeyparts = 0;

        /* Skip all key parts that are not user defined. */
        for (kpno = 0;; kpno++) {
            rs_attrtype_t kptype = rs_keyp_parttype(cd, key, kpno);

            if (kptype == RSAT_USER_DEFINED ||
                kptype == RSAT_COLLATION_KEY) {
                break;
            }
        }
        start_kpno = kpno;
        *relopno = 0;

        ptr = keyconstr_ptr;
        key_len = keyconstr_len;
        kpno = start_kpno;

        /* Get MySQL constraints.
         *
         * Some of the code here is copied from ha_federated.cpp.
         * TODO: Need to recheck this.
         */
        for (key_part = key_info->key_part; ; key_part++, kpno++) {
            int relop;
            rs_ano_t ano;
            rs_atype_t* atype;
            rs_aval_t* aval;
            Field* field;
            uint store_length;
            bool isnull;
            bool mysql_isnull;
            int offset;

            nkeyparts++;

            field = key_part->field;
            enum_field_types  mysqltype = field->type();
            store_length = key_part->store_length;

            ss_dassert(rs_keyp_parttype(cd, key, kpno) == RSAT_USER_DEFINED ||
                       rs_keyp_parttype(cd, key, kpno) == RSAT_COLLATION_KEY);

            ano = rs_keyp_ano(cd, key, kpno);
            ss_dassert(ano != RS_ANO_NULL);
            (*anos)[*relopno] = ano;

            ss_pprintf_2(("ha_solid::solid_get_key_limits:kpno=%d, ano=%d\n", kpno, ano));

            atype = rs_ttype_atype(cd, ttype, ano);
            aval = rs_tval_aval(cd, ttype, *tval, ano);

            isnull = FALSE;
            offset = 0;

            if (key_part->null_bit) {
                if (*ptr) {
                    ss_pprintf_3(("ha_solid::solid_get_key_limits:NULL\n"));
                    rs_aval_setnull(
                        cd,
                        atype,
                        aval);
                    isnull = TRUE;
                    mysql_isnull = mysql_record_is_null(table, field, (char *)ptr);
                    switch (find_flag) {
                        case HA_READ_KEY_EXACT:
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_KEY_EXACT->RS_RELOP_EQUAL\n"));
                            relop = RS_RELOP_EQUAL;
                            break;
                        case HA_READ_KEY_OR_NEXT:
                        case HA_READ_PREFIX:
                        case HA_READ_PREFIX_LAST:
                        case HA_READ_PREFIX_LAST_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_KEY_OR_NEXT...->RS_RELOP_GE\n"));
                            relop = RS_RELOP_GE;
                            break;
                        case HA_READ_BEFORE_KEY:
                        case HA_READ_KEY_OR_PREV:
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_BEFORE_KEY...->RS_RELOP_GE\n"));
                            relop = RS_RELOP_LE;
                            break;
                        case HA_READ_AFTER_KEY:
                            if (mysql_isnull) {
                                ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_AFTER_KEY...->RS_RELOP_GE\n"));
                                relop = RS_RELOP_GE;
                            } else {
                                ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_AFTER_KEY...->RS_RELOP_GT\n"));
                                relop = RS_RELOP_GT;
                            }
                            break;
                        default:
                            ss_pprintf_1(("ha_solid::solid_get_key_limits:**UNKNOWN** find_flag=%d\n", find_flag));
                            ss_error;
                            DBUG_RETURN(1);
                    }
                }
                offset = 1;
            }
            if (!isnull) {
                switch (find_flag) {
                    case HA_READ_KEY_EXACT:
                    #ifdef SS_HA_BUG62FIX
                        if (key_part->field->result_type() == STRING_RESULT &&
                            ( mysqltype == MYSQL_TYPE_VAR_STRING ||
                              mysqltype == MYSQL_TYPE_STRING ||
                              mysqltype == MYSQL_TYPE_VARCHAR ) ) {
                            relop = key_part->store_length > key_len ? RS_RELOP_GE : RS_RELOP_EQUAL;
                            ss_pprintf_3(("ha_solid::solid_get_key_limits: HA_READ_KEY_EXACT(STRING) => RS_RELOP_%s\n",
                                          key_part->store_length > key_len ? "GE" : "EQUAL"));
                        } else {  // not a string field.
                            relop = RS_RELOP_EQUAL;
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:HA_READ_KEY_EXACT => RS_RELOP_EQUAL\n"));

                            //if (store_length >= key_len || key_part->type == HA_KEYTYPE_BIT) {
                            //    ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_EQUA\n"));
                            //    relop = RS_RELOP_EQUAL;
                            //} else {
                            //    ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_GE\n"));
                            //    relop = RS_RELOP_GE;
                            //}
                        }
                    #else
                        if (store_length >= key_len
                            || key_part->type == HA_KEYTYPE_BIT
                            || field->result_type() != STRING_RESULT)
                        {
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_EQUAL\n"));
                            relop = RS_RELOP_EQUAL;
                        } else {
                            /* TODO: LIKE */
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_GE\n"));
                            relop = RS_RELOP_GE;
                        }
                    #endif
                        break;
                    case HA_READ_AFTER_KEY:
                        if (store_length >= key_len) {
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_GT\n"));
                            relop = RS_RELOP_GT;
                            break;
                        }
                        /* FALLTHROUGH */
                    case HA_READ_KEY_OR_NEXT:
                        ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_GE\n"));
                        relop = RS_RELOP_GE;
                        break;
                    case HA_READ_BEFORE_KEY:
                        if (store_length >= key_len) {
                            ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_LT\n"));
                            relop = RS_RELOP_LT;
                            break;
                        }
                        /* FALLTHROUGH */
                    case HA_READ_KEY_OR_PREV:
                        ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_LE\n"));
                        relop = RS_RELOP_LE;
                        break;

                    case HA_READ_PREFIX:
                        ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_GE\n"));
                        relop = RS_RELOP_GE;
                        break;

                    case HA_READ_PREFIX_LAST:
                    case HA_READ_PREFIX_LAST_OR_PREV:
                        ss_pprintf_3(("ha_solid::solid_get_key_limits:RS_RELOP_LE\n"));
                        relop = RS_RELOP_LE;
                        break;

                    default:
                        ss_pprintf_1(("ha_solid::solid_get_key_limits:**UNKNOWN** find_flag=%d\n", find_flag));
                        ss_error;
                        DBUG_RETURN(1);
                }

                succp = set_mysql_field_to_aval_or_dynva(thd, table, cd, atype, aval, NULL, field,
                                                         (SS_MYSQL_ROW *)(ptr + offset), TRUE, &errh);

                if (!succp) {
                    sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
                    su_err_done(errh);
                    DBUG_RETURN(1);
                }
            }

            ss_poutput_2(ha_print_aval(cd, atype, aval, (char *)"ha_solid::solid_get_key_limits:"));

            (*relops)[*relopno] = relop;
            (*relopno)++;

            if (store_length >= key_len) {
                break;
            }
            key_len -= store_length;
            ptr += store_length;
        }
        ss_poutput_2(rs_tval_print(cd, ttype, *tval));
        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              mysql_flag_to_range
 *
 * TODO: Provide description of this function
 *
 * Parameters : -
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static enum ha_rkey_function mysql_flag_to_range(
        enum ha_rkey_function find_flag,
        int upper)
{
        enum ha_rkey_function new_flag = find_flag;

        switch (find_flag) {
            case HA_READ_KEY_EXACT:
                break;
            case HA_READ_AFTER_KEY:
                if (upper) {
                    new_flag = HA_READ_BEFORE_KEY;
                }
                break;
            case HA_READ_BEFORE_KEY:
                if (!upper) {
                    new_flag = HA_READ_AFTER_KEY;
                }
                break;
            case HA_READ_KEY_OR_NEXT:
                if (upper) {
                    new_flag = HA_READ_KEY_OR_PREV;
                }
                break;
            case HA_READ_KEY_OR_PREV:
                if (!upper) {
                    new_flag = HA_READ_KEY_OR_NEXT;
                }
                break;
            case HA_READ_PREFIX:
                if (upper) {
                    new_flag = HA_READ_PREFIX_LAST;
                }
                break;
            case HA_READ_PREFIX_LAST:
                if (!upper) {
                    new_flag = HA_READ_PREFIX;
                }
                break;
            case HA_READ_PREFIX_LAST_OR_PREV:
                if (!upper) {
                    new_flag = HA_READ_PREFIX;
                }
                break;
            default:
                ss_derror;
                break;

        }
        return(new_flag);
}

/*#***********************************************************************\
 *
 *              ::records_in_range
 *
 * Given a starting key, and an ending key estimate the number of rows that
 * will exist between the two. end_key may be empty which in case determine
 * if start_key matches any rows.
 *
 * Parameters :
 *
 *     uint        in, index number
 *     key_range*  min_key, in, use
 *     key_range*  max_key, in, use
 *
 * Return value : ha_rows
 *
 * Globals used : THD* current_thd
 */
ha_rows ha_soliddb::records_in_range(uint inx,
                                     key_range *min_key,
                                     key_range *max_key)
{
        SOLID_CONN*   con;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        double        rowcount;
        double        table_rowcount;
        MYSQL_THD thd;
        int i;
        rs_ttype_t* ttype;
        rs_relh_t* rsrelh;
        ss_int8_t i8;
        double new_rowcount;
        int nvalues1 = 0;
        int nvalues2 = 0;
        int nrange = 0;
        int rc = 0;
        bool succp = TRUE;
        enum ha_rkey_function min_flag;
        enum ha_rkey_function max_flag;
        bool revert_range = FALSE;
        ss_win_perf(__int64 startcount;)
        ss_win_perf(__int64 endcount;)

        ss_win_perf_start;

        DBUG_ENTER("ha_soliddb::records_in_range");
        ss_pprintf_1(("ha_solid::records_in_range, %s, index=%d, min_key=%ld, max_key=%ld, key=%s\n",
            table->s->path, inx, (long)min_key, (long)max_key,
            table->key_info[inx].name));
        SS_PMON_ADD(SS_PMON_MYSQL_RECORDS_IN_RANGE);

        thd = current_thd;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif
        CHK_CONN(con);

        if (solid_table == NULL) {
            ha_solid_mysql_error(thd, con->sc_errh, rc);
            ss_pprintf_1(("ha_solid::table not found\n"));
            DBUG_RETURN((ha_rows)UNKNOWN_NUMBER_OF_TUPLES_IN_TABLE);
        }

        cd    = con->sc_cd;
        trans = con->sc_trans;
        rsrelh = solid_table->st_rsrelh;
        ttype = rs_relh_ttype(cd, rsrelh);

        SsInt8ConvertToDouble(&table_rowcount, rs_relh_ntuples(cd, rsrelh));

        ss_pprintf_2(("ha_solid::records_in_range, table ntuples=%.1lf\n", table_rowcount));
        rowcount = table_rowcount;

        solid_trans_beginif((handlerton *)ht, thd, FALSE);

        if (min_key == NULL) {
            min_key = max_key;
            max_key = NULL;
            revert_range = TRUE;
        }

        if (    min_key == NULL && max_key == NULL
            ||  rs_sysi_simpleoptimizerrules(cd, rowcount) &&
                (min_key->flag == HA_READ_KEY_EXACT || max_key == NULL)) {

            double selectivity = 1.0;
            enum ha_rkey_function find_flag;

            ss_pprintf_2(("ha_solid::records_in_range, simple rules\n"));

            if (min_key) {
                find_flag = min_key->flag;
            } else {
                find_flag = HA_READ_KEY_EXACT;
            }

            switch (find_flag) {
                case HA_READ_KEY_EXACT:
                    selectivity = rs_sqli_equal_selectivity(rs_sysi_sqlinfo(cd));
                    break;
                case HA_READ_PREFIX:
                case HA_READ_PREFIX_LAST:
                case HA_READ_PREFIX_LAST_OR_PREV:
                case HA_READ_AFTER_KEY:
                case HA_READ_KEY_OR_NEXT:
                case HA_READ_BEFORE_KEY:
                case HA_READ_KEY_OR_PREV:
                    selectivity = rs_sqli_compare_selectivity(rs_sysi_sqlinfo(cd));
                    break;
                default:
                    ss_error;
            }

            ss_pprintf_2(("ha_solid::records_in_range, selectivity=%.1lf, rowcount=%.1lf\n\n", selectivity, rowcount));

            rowcount = selectivity * rowcount;
        } else {
            rs_key_t* key;
            KEY* key_info;
            double selectivity;
            double total_selectivity;
            double min_selectivity;
            double selectivity_drop_limit;
            rs_sqlinfo_t* sqli;

            sqli = rs_sysi_sqlinfo(cd);
            selectivity_drop_limit = rs_sqli_selectivity_drop_limit(sqli);
            ss_pprintf_2(("ha_solid::records_in_range, selectivity_drop_limit=%.1lff\n", selectivity_drop_limit));

            if (min_key != NULL && max_key != NULL) {
                min_flag = HA_READ_KEY_OR_NEXT;
                max_flag = HA_READ_KEY_OR_PREV;
            } else {
                min_flag = mysql_flag_to_range(min_key->flag, revert_range);
                ss_dassert(max_key == NULL);
            }

            key_info = &table->key_info[inx];

            key = solid_resolve_key(cd, solid_table->st_rsrelh, key_info);

            tb_est_ensureselectivityinfo(cd, rsrelh);

            if (min_key->flag == HA_READ_KEY_EXACT &&
                min_key->length == max_key->length &&
                memcmp(min_key->key, max_key->key, min_key->length)==0) {

                rs_ano_t kp;
                bool stat_ok = TRUE;

                for(kp = rs_key_first_datapart(con->sc_cd, key);
                    kp <= rs_key_lastordering(con->sc_cd, key); ++kp) {

                    long nvalues = 1;
                    stat_ok = stat_ok && rs_relh_getequalvalues(cd,
                                                                solid_table->st_rsrelh,
                                                                rs_keyp_ano(cd, key, kp),
                                                                &nvalues);

                    ss_dprintf_2(("ha_solid::records_in_range, equalvalues, ano=%d, rowcount=%ld\n",
                                  rs_keyp_ano(cd, key, kp), nvalues));

                    if (kp == rs_key_first_datapart(con->sc_cd, key)) {
                        rowcount = nvalues;
                    } else {
                        rowcount = rowcount * ( (double)nvalues / table_rowcount );
                    }
                }

                ss_dprintf_2(("ha_solid::records_in_range, equalvalues, rowcount=%ld\n", (long)rowcount));

                if (stat_ok) {
                    /* As we return an integer to MySQL, returning something < 1 causes it being converted to 0
                       and "Impossible WHERE noticed after reading const tables" appearing.
                       Returning always at least one. */
                    rowcount = SS_MAX(rowcount, 1.0);
                    ss_pprintf_2(("ha_solid::records_in_range:rowcount=%ld\n", (long)rowcount));
                    ss_win_perf_stop(records_in_range_perfcount, records_in_range_callcount);
                    DBUG_RETURN((ha_rows)rowcount);
                }
            }

            rc = solid_get_key_limits(
                    thd,
                    cd,
                    solid_table->st_rsrelh,
                    table,
                    key_info,
                    key,
                    min_key->key,
                    min_key->length,
                    min_flag,
                    &solid_table->st_limtval1,
                    &nvalues1,
                    &solid_table->st_relopsize1,
                    &solid_table->st_relops1,
                    &solid_table->st_anos1);

            if (rc) {
                DBUG_RETURN((ha_rows)0);
            }

            if (max_key != NULL) {

                rc = solid_get_key_limits(
                        thd,
                        cd,
                        solid_table->st_rsrelh,
                        table,
                        key_info,
                        key,
                        max_key->key,
                        max_key->length,
                        max_flag,
                        &solid_table->st_limtval2,
                        &nvalues2,
                        &solid_table->st_relopsize2,
                        &solid_table->st_relops2,
                        &solid_table->st_anos2);

                if (rc) {
                    DBUG_RETURN((ha_rows)0);
                }

                nrange = SS_MIN(nvalues1, nvalues2);
            }

            total_selectivity = 1.0;

            for (i = 0; i < nrange; i++) {

                succp = rs_relh_getrangeselectivity(
                            cd,
                            rsrelh,
                            solid_table->st_anos1[i],
                            rs_tval_va(cd, ttype, solid_table->st_limtval1, solid_table->st_anos1[i]),
                            TRUE,
                            rs_tval_va(cd, ttype, solid_table->st_limtval2, solid_table->st_anos1[i]),
                            TRUE,
                            FALSE,
                            &i8);

                if (succp) {
                    succp = SsInt8ConvertToDouble(&new_rowcount, i8);
                    ss_dassert(succp);
                    rowcount = SS_MIN(rowcount, new_rowcount);
                    selectivity = new_rowcount / table_rowcount;
                    min_selectivity = SS_MIN(selectivity, total_selectivity);
                    total_selectivity = total_selectivity * selectivity;
                    if (total_selectivity < selectivity_drop_limit * min_selectivity) {
                        /* Do not let combined selectivity drop too fast.
                         */
                        total_selectivity = selectivity_drop_limit * min_selectivity;
                    }
                    if (total_selectivity < EST_MIN_SELECTIVITY) {
                        total_selectivity = EST_MIN_SELECTIVITY;
                    }
                    ss_pprintf_2(("ha_solid::records_in_range, rs_relh_getrangeselectivity, rowcount=%.1lf, selectivity=%.5lf, total_selectivity=%.5lf\n", new_rowcount, selectivity, total_selectivity));
                }
            }

            for (; i < nvalues1; i++) {

                int relop = solid_table->st_relops1[i];

                /* At the moment you can't get selectivity for ISNULL and ISNOTNULL
                   relational operators */

                if (relop != RS_RELOP_ISNULL && relop != RS_RELOP_ISNOTNULL) {

                    succp = rs_relh_getrelopselectivity(
                            cd,
                            rsrelh,
                            solid_table->st_anos1[i],
                            relop,
                            rs_tval_va(cd, ttype, solid_table->st_limtval1, solid_table->st_anos1[i]),
                            '\\',
                            &i8);

                    if (succp) {
                        succp = SsInt8ConvertToDouble(&new_rowcount, i8);
                        ss_dassert(succp);
                        rowcount = SS_MIN(rowcount, new_rowcount);
                        selectivity = new_rowcount / table_rowcount;
                        min_selectivity = SS_MIN(selectivity, total_selectivity);
                        total_selectivity = total_selectivity * selectivity;
                        if (total_selectivity < selectivity_drop_limit * min_selectivity) {
                            /* Do not let combined selectivity drop too fast.
                             */
                            total_selectivity = selectivity_drop_limit * min_selectivity;
                        }
                        if (total_selectivity < EST_MIN_SELECTIVITY) {
                            total_selectivity = EST_MIN_SELECTIVITY;
                        }
                        ss_pprintf_2(("ha_solid::records_in_range, rs_relh_getrelopselectivity, rowcount=%.1lf, selectivity=%.5lf, total_selectivity=%.5lf\n", new_rowcount, selectivity, total_selectivity));
                    }
                }
            }
            rowcount = total_selectivity * table_rowcount;
        }

        if (rowcount <= 1) {
            /* Return always at least one. */
            rowcount = 1;
        }

        ss_pprintf_2(("ha_solid::records_in_range:rowcount=%ld\n", (long)rowcount));

        ss_win_perf_stop(records_in_range_perfcount, records_in_range_callcount);

        DBUG_RETURN((ha_rows)rowcount);
}

int ha_soliddb::analyze(THD* thd,
                    HA_CHECK_OPT* check_opt)
{
        SOLID_CONN* con;
        SDB_DBUG_ENTER("ha_soliddb::analyze");

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(current_thd, table->s->path.str);
#else
        con = get_solid_connection(current_thd, table->s->path);
#endif

        /* transaction is needed to raise read level of the server process not to see old row versions */
        solid_trans_beginif((handlerton *)ht, thd, FALSE);
        tb_est_updateselectivityinfo(con->sc_cd, solid_table->st_rsrelh);

#ifdef SS_DEBUG
        rs_relh_printsamples(con->sc_cd, solid_table->st_rsrelh, 0);
#endif

        SDB_DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::delete_all_rows
 *
 * Used to delete all rows in a table. Both for cases of truncate and
 * for cases where the optimizer realizes that all rows will be
 * removed as a result of a SQL statement.
 *
 * Parameters : -
 *
 * Return value : 0 or error code
 *
 * Globals used : current_thd
int ha_soliddb::delete_all_rows()
 */
int ha_soliddb::truncate()
{
        MYSQL_THD    thd;
        SOLID_CONN*  con;
        const char*  name;
        rs_entname_t en;
        int          rc       = 0;
        su_err_t*    errh     = NULL;
        void*        cont     = NULL;
        bool         succp    = FALSE;
        solid_bool   finished = FALSE;

        SDB_DBUG_ENTER("ha_soliddb::delete_all_rows");

        thd = current_thd;

        if (soliddb_sql_command(thd) != SQLCOM_TRUNCATE) {
            SDB_DBUG_RETURN(HA_ERR_WRONG_COMMAND);
        }

#if MYSQL_VERSION_ID >= 50100
        name = table->s->path.str;
#else
        name = table->s->path;
#endif
        con = get_solid_connection(thd, name);

        CHK_CONN(con);

        if (rs_relh_issysrel(con->sc_cd, solid_table->st_rsrelh)) {
            SDB_DBUG_RETURN(HA_ERR_WRONG_COMMAND);
        }

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        solid_relname(&en, name);

        ss_dprintf_2(("ha_soliddb::delete_all_rows call tb_truncaterelation\n"));

        /* Lock table to avoid concurrent truncates */
        do {

            succp = tb_trans_lockrelh(con->sc_cd,
                                      con->sc_trans,
                                      solid_table->st_rsrelh,
                                      TRUE, /* exclusive */
                                      soliddb_lock_wait_timeout * 100,    /* timeout ms*/
                                      &finished,
                                      &errh);

        } while (rs_sysi_lockwait(con->sc_cd) || !finished);

        if (succp) {
            succp = tb_truncaterelation(con->sc_cd, con->sc_trans, en.en_name, en.en_schema,
                                        NULL, NULL, &cont, &errh);
        }
        
        /* In truncate we should reset auto increment value */
        if (succp) {
            reset_auto_increment(0);
        }

        /* Error handling: if we have foreign key get referenced table and key */
        if (!succp && su_err_geterrcode(errh) == E_FORKEYREFEXIST_S) {
            ulong relid;

#if MYSQL_VERSION_ID >= 50100
            ss_dprintf_1(("set err table to %s\n",table->s->path.str));
#else
            ss_dprintf_1(("set err table to %s\n",table->s->path));
#endif
            con->sc_err_tableid = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);

            con->sc_errkey = rs_relh_search_clusterkey(con->sc_cd, solid_table->st_rsrelh);

            relid = tb_dd_get_refrelid_by_relid(con->sc_cd, con->sc_trans,
                                                rs_relh_relid(con->sc_cd, solid_table->st_rsrelh));

            if (relid) {
                rs_relh_t* reftab = NULL;

                con->sc_err_tableid = relid;

                /* Now we need to find out referencing key */
                reftab = tb_dd_getrelhbyid(con->sc_cd, con->sc_trans, relid, NULL, NULL);

                if (reftab) {
                    su_pa_t* keys;
                    uint     i=0;

                    keys = rs_relh_refkeys(con->sc_cd, reftab);

                    su_pa_do(keys, i) {
                        rs_key_t *key;

                        key = (rs_key_t *)su_pa_getdata(keys, i);

                        if (rs_key_refrelid(con->sc_cd, key) == rs_relh_relid(con->sc_cd, solid_table->st_rsrelh)) {
                            con->sc_errkey = key; /* This it is */
                            break;
                        }
                    }

                    rs_relh_done(con->sc_cd, reftab);
                }
            }
        }

        if (!succp) {
            sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
            rc = ha_solid_mysql_error(thd, errh, rc);
            su_err_done(errh);
        }


        rs_entname_done_buf(&en);
        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::external_lock
 *
 * First you should go read the section "locking functions for mysql" in
 * lock.cc to understand this.
 * This creates a lock on the table. If you are implementing a storage engine
 * that can handle transacations look at ha_berkely.cc to see how you will
 * want to goo about doing this. Otherwise you should consider calling flock()
 * here.
 *
 * Parameters :
 *
 *     THD*  thd, in, use
 *     int   lock_type, in, use, MySQL lock type
 *
 * Return value : 0 or derror code
 *
 * Globals used :
 */
int ha_soliddb::external_lock(
        MYSQL_THD thd,
        int       lock_type)
{
        SOLID_CONN* con;
        su_err_t*   errh = NULL;
        int rc = 0;

        DBUG_ENTER("ha_soliddb::external_lock");
        ss_pprintf_1(("ha_solid::external_lock:lock_type=%d\n", lock_type));

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        CHK_CONN(con);

        for_update = FALSE;
        extra_keyread = FALSE;
        extra_retrieve_primary_key = FALSE;
        extra_retrieve_all_cols = FALSE;

        user_thd = thd;

        solid_relcur_clearrelcurdonelist(con, 5, TRUE);

        if (lock_type != F_UNLCK) {
            ss_pprintf_2(("ha_solid::external_lock:LOCK\n"));
            con->con_n_tables++;
            bool trans_register;

            /*
              Set up transaction isolation level at the start of the transaction
              i.e. when first table is accessed. Note that you can't change
              isolation level in the middle of the transaction.
            */

            if(con->con_n_tables == 1) {
                ss_pprintf_1(("ha_solid::external_lock:isolation_type=%d\n",
                             (int) soliddb_tx_isolation(thd)));

                tb_trans_settransoption(con->sc_cd, con->sc_trans,
                    (tb_transopt_t)ha_solid_map_mysql_solid_isolation(
                            (enum_tx_isolation)soliddb_tx_isolation(thd)));

                ss_pprintf_1(("ha_solid::external_lock:trans_isolation=%d\n",
                             (int)tb_trans_getisolation(con->sc_cd, con->sc_trans)));
            }

#if MYSQL_VERSION_ID >= 50100
            trans_register = thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#else
            trans_register = thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN);
#endif

            if (trans_register) {
                trans_register_ha(thd, TRUE, (handlerton *)ht);
            }

            trans_register_ha(thd, FALSE, (handlerton *)ht);

            solid_trans_beginif((handlerton *)ht, thd, FALSE);

            tb_trans_startnewsearch(con->sc_cd, con->sc_trans);

            if (lock_type == F_WRLCK) {
                ss_pprintf_2(("ha_solid::external_lock:WRITE LOCK\n"));

                if (soliddb_sql_command(thd) == SQLCOM_SELECT ||
                    soliddb_sql_command(thd) == SQLCOM_INSERT_SELECT) {
                    for_update = TRUE;
                }
            }
        } else {
            /*
             * When last table is unlocked in autocommit we execute commit.
             * This is needed because it seems that MySql does not call commit
             * for selects in autocommit mode.
             */
            ss_pprintf_2(("ha_solid::external_lock:UNLOCK\n"));
            con->con_n_tables--;

            if (con->con_n_tables == 0) {
                bool autocommit;

#if MYSQL_VERSION_ID >= 50100
                autocommit = !(thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)));
#else
                autocommit = !(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN));
#endif

                if (autocommit) {
                    /* In case of autocommit we commit transaction on last unlock in this session */
                    bool succp;

                    succp = internal_solid_end_trans_and_stmt(con->sc_cd, con->sc_trans, TRUE, &errh);

                    if (!succp) {
                        rc = ha_solid_mysql_error(thd, errh, rc);
                        su_err_done(errh);
                    } else {
                        rc = 0;
                    }

                }
            }

        }
        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::store_lock
 *
 * The idea with handler::store_lock() is the following:
 *
 * The statement decided which locks we should need for the table
 * for updates/deletes/inserts we get WRITE locks, for SELECT... we get
 * read locks.
 *
 * Before adding the lock into the table lock handler (see thr_lock.c)
 * mysqld calls store lock with the requested locks.  Store lock can now
 * modify a write lock to a read lock (or some other lock), ignore the
 * lock (if we don't want to use MySQL table locks at all) or add locks
 * for many tables (like we do when we are using a MERGE handler).
 *
 * Berkeley DB for example  changes all WRITE locks to TL_WRITE_ALLOW_WRITE
 * (which signals that we are doing WRITES, but we are still allowing other
 * reader's and writer's.
 *
 * When releasing locks, store_lock() are also called. In this case one
 * usually doesn't have to do anything.
 *
 * In some exceptional cases MySQL may send a request for a TL_IGNORE;
 * This means that we are requesting the same lock as last time and this
 * should also be ignored. (This may happen when someone does a flush
 * table when we have opened a part of the tables, in which case mysqld
 * closes and reopens the tables and tries to get the same locks at last
 * time).  In the future we will probably try to remove this.
 *
 * Parameters :
 *
 *    THD*                thd, in, use
 *    THR_LOCK_DATA**     to, in, use
 *    enum thr_lock_type  lock_type, in
 *
 * Return value : THD_LOCK_DATA**
 *
 * Globals used :
 */
THR_LOCK_DATA **ha_soliddb::store_lock(
        MYSQL_THD thd,
        THR_LOCK_DATA **to,
        enum thr_lock_type lock_type)
{
        ss_pprintf_1(("ha_solid::store_lock:lock_type=%d\n", (int)lock_type));

        /* The following code is copied from ha_archive.cc. The same
         * code seem to be also in ha_ndbcluster.cc.
         * TODO check this logic here, some other handlers have more complex logic.
         */
        if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
        {
          /*
            Here is where we get into the guts of a row level lock.
            If TL_UNLOCK is set
            If we are not doing a LOCK TABLE or DISCARD/IMPORT
            TABLESPACE, then allow multiple writers
          */

          if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
               lock_type <= TL_WRITE) && !soliddb_in_lock_tables(thd)
              && !soliddb_tablespace_op(thd)) {
              lock_type = TL_WRITE_ALLOW_WRITE;
          }
          /*
            In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
            MySQL would use the lock TL_READ_NO_INSERT on t2, and that
            would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
            to t2. Convert the lock to a normal read lock to allow
            concurrent inserts to t2.
          */

          if (lock_type == TL_READ_NO_INSERT && !soliddb_in_lock_tables(thd)) {
              lock_type = TL_READ;
          }

          lock.type=lock_type;
        }

        *to++= &lock;

        return to;
}

/*#***********************************************************************\
*              ::drop_table_autoincrements
* Parameters :
* rs_relh_t* relh, relation to drop autoincrements from. IN
* SOLID_CONN* con - solid connection structure. IN
*
* Return value : true or false on error.
*
*/
static bool drop_table_autoincrements(
        rs_relh_t* relh,
        SOLID_CONN* con)
{
    bool succp = TRUE;
    su_err_t* errh = NULL;
    char* seq_name = NULL;
    char* authid = NULL;
    char* catalog = NULL;
    rs_auth_t* auth = rs_sysi_auth(con->sc_cd);

    rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

    if (tb_seq_findbyid(con->sc_cd,
        rs_relh_readautoincrement_seqid(con->sc_cd, relh),
        &seq_name, &authid, &catalog)) {
            void* dummy = NULL;

            succp = tb_dropseq(con->sc_cd, con->sc_trans, seq_name, authid, catalog,
                               NULL, FALSE, &dummy, &errh);

            ss_pprintf_1(("SEQ NAME %s dropped succ = %d\n", seq_name, (int)succp));
    }

    if (seq_name) {
        SsMemFree(seq_name);
    }

    if (authid) {
        SsMemFree(authid);
    }

    if (catalog) {
        SsMemFree(catalog);
    }

    seq_name = authid = catalog = NULL;

    if (errh) {
        sql_print_error("%s", su_err_geterrstr(errh));
        su_err_done(errh);
        errh = NULL;
    }

    rs_auth_setsystempriv(con->sc_cd, auth, FALSE);

    return succp;
}

/*#***********************************************************************\
 *
 *              ::delete_table
 *
 * Used to delete a table. By the time delete_table() has been called all
 * opened references to this table will have been closed (and your globally
 * shared references released. The variable name will just be the name of
 * the table. You will need to remove any files you have created at this point.
 *
 * If you do not implement this, the default delete_table() is called from
 * handler.cc and it will delete all files with the file extentions returned
 * by bas_ext().
 *
 * Called from handler.cc by delete_table and  ha_create_table(). Only used
 * during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
 * the storage engine.
 *
 * Parameters :
 *
 *     const char* name, in, MySQL table name
 *
 * Return value : 0 or error code
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::delete_table(const char *name)
{
        SDB_DBUG_ENTER("ha_solid::delete_table");
        bool succp = true;
        int rc = 0;
        su_err_t* errh = NULL;
        SOLID_CONN* con = NULL;
        rs_entname_t en;
        MYSQL_THD thd = current_thd;
        solid_bool checkforkeys = TRUE;
        solid_bool cascade = FALSE;
        bool commit_succ_p = true;
        table_indexes_t *ti = NULL;
        table_forkeys_t *tfk = NULL;
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        long rel_id;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
        solid_relname(&en, name);

        ss_pprintf_1(("ha_solid::delete_table:'%s'->'%s.%s'\n", name, en.en_schema, en.en_name));

        if (is_soliddb_system_relation(en)) {
            ss_pprintf_1(("Trying to drop SolidDB system table %s\n", name));
            rs_entname_done_buf(&en);
            SDB_DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        if (is_DDL_command( thd ) && !start_solid_DDL(rc)) {
            rs_entname_done_buf(&en);
            SDB_DBUG_RETURN(rc);
        }

        con = get_solid_connection(thd, name);
        CHK_CONN(con);

        if (solid_table == NULL) {
            ss_pprintf_1(("Trying to drop unknown SolidDB table %s\n", name));
            rs_entname_done_buf(&en);
            SDB_DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        if (operation_requires_xtable_lock(thd) &&
            (errh = ::lock_table(con, &en, TRUE, DDL_LOCK_INSTANTLY))) {
            succp = FALSE;
            goto epilogue;
        }

        ti = get_table_disabled_keys(&en);
        tfk = get_table_disabled_forkeys(&en);

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        rel_id = rs_relh_relid(con->sc_cd, solid_table->st_rsrelh);
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

        if (rs_relh_isautoincinited(con->sc_cd, solid_table->st_rsrelh)) {
            succp = drop_table_autoincrements(solid_table->st_rsrelh, con);

            if (!succp) {
                sql_print_error("Can't drop table auto incremented fields.");
                rc = HA_ERR_NO_SUCH_TABLE;
                goto epilogue;
            }
        }

        /* In drop database allow dropping tables with foreign key definitions */
        if (soliddb_sql_command(thd) == SQLCOM_DROP_DB) {
            cascade      = TRUE;
            checkforkeys = FALSE;
        } else if (soliddb_sql_command(thd) == SQLCOM_ALTER_TABLE) {
            cascade = FALSE;
            checkforkeys = FALSE;
        } else {
            /* In drop table allow dropping table with foreign key only
               if user has provided CASCADE options */
            checkforkeys = TRUE;
            cascade = FALSE;

            /* On same cases MySQL will call this function with sql_command == SQLCOM_END
               and thd->query == NULL */
            if (soliddb_query(thd)) {
#ifdef MYSQL_DYNAMIC_PLUGIN
                cascade = soliddb_drop_mode(con->sc_cd, thd);
#else
                cascade = soliddb_drop_mode(con->sc_cd, thd);
#endif
        }
        }

        succp = tb_admi_droprelation( con->sc_cd, con->sc_trans,
                                      en.en_name, en.en_schema,
                                      NULL, NULL, cascade,
                                      checkforkeys, &errh);

        solid_table_free(con->sc_cd, solid_table);
        solid_table = NULL;

        if (succp && ti) {
            drop_table_disabled_keys(ti);
        }
        if (succp && tfk) {
            drop_table_disabled_forkeys(tfk);
        }
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        if (succp) {
            tb_dd_delete_from_fkeys_unresolved(con->sc_cd, con->sc_trans, rel_id);
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        

#ifdef SS_MYSQL_AC
        frm_from_disk_to_db(con->sc_tbcon, con->sc_cd, con->sc_trans, &en, name, TRUE);
#endif /* SS_MYSQL_AC */

epilogue:
        if (!succp) {
            rc = handle_error(errh);
        }

        /* Transaction is committed or rolled back depending on success predicate */
        commit_succ_p = internal_solid_end_trans_and_stmt(con->sc_cd, con->sc_trans, succp, &errh);

        if (!commit_succ_p) {
            rc = handle_error(errh);
        }

        rs_entname_done_buf(&en);
        finish_solid_DDL();
        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::rename_table
 *
 * Renames a table from one name to another from alter table call.
 *
 * If you do not implement this, the default rename_table() is called from
 * handler.cc and it will delete all files with the file extentions returned
 * by bas_ext().
 *
 * Parameters :
 *
 *     const char*  from, in, old MySQL table name
 *     const char*  to, in, use, new MySQL table name
 *
 * Return value : 0 or return code
 *
 * Globals used : THD* current_the
 */
int ha_soliddb::rename_table(const char* from, const char* to)
{
        bool succp = true;
        int rc = 0;
        su_err_t* errh = NULL;
        SOLID_CONN* con = NULL;
        tb_relh_t* tbrelh = NULL;
        MYSQL_THD thd = current_thd;
        rs_entname_t from_en;
        rs_entname_t to_en;
        bool commit_succ_p = true;

        SDB_DBUG_ENTER("ha_solid::rename_table");

        if( is_DDL_command( thd ) && !start_solid_DDL(rc) ) {
            SDB_DBUG_RETURN(rc);
        }

        solid_relname(&from_en, from);
        solid_relname(&to_en, to);

        ss_pprintf_1(("ha_solid::rename_table:'%s.%s'(%s)->'%s.%s'(%s)\n",
            from_en.en_schema, from_en.en_name, from, to_en.en_schema, to_en.en_name, to));

        if (is_soliddb_system_relation(from_en)) {
            ss_pprintf_1(("Trying to rename SolidDB system table %s\n", from));
            rs_entname_done_buf(&from_en);
            rs_entname_done_buf(&to_en);
            SDB_DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
        }

        con = get_solid_connection(thd, from);

        tb_trans_begintransandstmt(con->sc_cd, con->sc_trans);

        if (operation_requires_xtable_lock(thd) &&
            (errh = ::lock_table(con, &from_en, TRUE, DDL_LOCK_INSTANTLY))) {
            rs_entname_done_buf(&from_en);
            rs_entname_done_buf(&to_en);
            internal_solid_end_trans_and_stmt(con->sc_cd, con->sc_trans, FALSE, NULL);
            rc = handle_error(errh);
            SDB_DBUG_RETURN(rc);
        }


#ifdef SS_MYSQL_AC
        {
            //char old_frm[FN_REFLEN];
            //char new_frm[FN_REFLEN];
            ////SsSprintf(frm_name, ".\%s\%s.frm", old_name.en_schema, old_name.en_name);
            //strxmov(old_frm, mysql_data_home, "/", from_en.en_schema, "/", from_en.en_name, reg_ext, NullS);
            //strxmov(new_frm, mysql_data_home, "/", to_en.en_schema, "/", to_en.en_name, reg_ext, NullS);

            frm_from_db_to_db_rename(con->sc_tbcon, con->sc_cd, con->sc_trans, &from_en, from, &to_en, to);
        }
#endif /* SS_MYSQL_AC */

        tbrelh = tb_relh_create(con->sc_cd, con->sc_trans,
                                from_en.en_name, from_en.en_schema,
                                /* catalog=*/ NULL, &errh);

        succp = (tbrelh != NULL);

        if (!succp) {
            rc = handle_error(errh);
            errh = NULL;
        }

        /* Creating new schema if it does not exist. */
        if (succp && !tb_schema_find(con->sc_cd, con->sc_trans, to_en.en_schema, NULL)) {
            void* cont = NULL;

            ss_pprintf_2(("ha_solid::schema %s not found, creating it.\n", to_en.en_schema));

            succp = tb_schema_create( con->sc_cd, con->sc_trans,
                                      to_en.en_schema, NULL, NULL, &cont, &errh);
            ss_dassert(cont == NULL);

            if (!succp) {
                rc = handle_error(errh);
                errh = NULL;
            }
        }

        /* Renaming the table */
        if (succp) {
            succp = tb_dd_renametable_ex(con->sc_cd, con->sc_trans, tbrelh,
                                         to_en.en_name, to_en.en_schema, &errh);

            if (!succp) {
                rc = handle_error(errh);
                errh = NULL;
            }
        }

        if (tbrelh) {
            tb_relh_free(con->sc_cd, tbrelh);
            tbrelh = NULL;
        }

        commit_succ_p = internal_solid_end_trans_and_stmt(con->sc_cd, con->sc_trans, succp, &errh);

        if (succp && !commit_succ_p) {
            rc = handle_error(errh);
        }

        rs_entname_done_buf(&from_en);
        rs_entname_done_buf(&to_en);
        finish_solid_DDL();
        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              solid_drop_database
 *
 * Drop database for solidDB
 *
 * Parameters :
 *
 *      char* path, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
void solid_drop_database(
#if MYSQL_VERSION_ID >= 50100
        handlerton* hton,
#endif
        char* path)
{
        SOLID_CONN* con;
        MYSQL_THD thd;
        char* schema = NULL;
        char* tmp = NULL;
        void* cont = NULL;
        size_t len = 0;

        DBUG_ENTER("solid_drop_database");
        ss_pprintf_1(("solid_drop_database:path = %s\n", path));

        while (*path == '.' || *path == '/' || *path == '\\') {
            path++;
        }

        tmp = (char*) SsMemStrdup(path);

        /* SsStrupr(tmp); */
        len = strlen(tmp);

        /* Remove trailing directory character */
        if (len > 1 && (tmp[len-1] == '.' || tmp[len-1] == '/' || tmp[len-1] == '\\')) {
                tmp[len-1]='\0';
        }

        schema = (char *) SsMemStrdup(tmp);
        SsMemFree(tmp);

        ss_pprintf_2(("solid_drop_database:schema = %s\n", schema));

        thd = current_thd;
        ss_dassert(thd != NULL);

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection((handlerton *)hton, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        if (con) {
            bool succp = TRUE;
            rs_auth_t* auth = rs_sysi_auth(con->sc_cd);
            su_err_t*  errh = NULL;

            tb_trans_begintransandstmt(con->sc_cd, con->sc_trans);

            rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

            succp = tb_schema_drop(
                        con->sc_cd,
                        con->sc_trans,
                        schema,
                        NULL,       // catalog
                        TRUE,       // cascade
                        &cont,
                        &errh);

            if (errh != NULL) {
                sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
                su_err_done(errh);
                errh = NULL;
            }

            rs_auth_setsystempriv(con->sc_cd, auth, FALSE);

            succp = internal_solid_end_trans_and_stmt(con->sc_cd, con->sc_trans, TRUE, NULL);
        }

        SsMemFree(schema);

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              check_solid_sysrel_create
 *
 * TODO: provide description to this function
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
static bool check_solid_sysrel_create(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        char* relname,
        rs_ttype_t* ttype)
{
        int i;
        bool foundp;
        rs_relh_t* sysrelh;
        rs_ttype_t* systtype;
        uint nattrs;
        rs_entname_t en;

        SS_PUSHNAME("check_solid_sysrel_create");

        i = check_solid_sysrel(relname);

        if (i == -1) {
            SS_POPNAME;
            return(FALSE);
        }

        rs_entname_initbuf(
            &en,
            NULL,       // catalog,
            (char *)RS_AVAL_SYSNAME,
            solid_sysrel_map[i].srm_solidname);

        sysrelh = tb_dd_getrelh(cd,
                                trans,
                                &en,
                                NULL,
                                NULL);

        if (sysrelh == NULL) {
            ss_derror;
            SS_POPNAME;
            return(FALSE);
        }

        /* Check that columns definitions are same.
         */
        foundp = TRUE;
        nattrs = rs_ttype_sql_nattrs(cd, ttype);
        systtype = rs_relh_ttype(cd, sysrelh);

        if (nattrs == rs_ttype_sql_nattrs(cd, systtype)) {
            uint j;

            for (j = 0; j < nattrs; j++) {
                rs_atype_t* atype;
                rs_atype_t* sysatype;

                atype = rs_ttype_sql_atype(cd, ttype, j);
                sysatype = rs_ttype_sql_atype(cd, systtype, j);

                if (!rs_atype_issame(cd, atype, sysatype)) {
                    foundp = FALSE;
                    break;
                }
            }
        } else {
            foundp = FALSE;
        }

        SS_MEM_SETUNLINK(sysrelh);
        rs_relh_done(cd, sysrelh);

        SS_POPNAME;

        return(foundp);
}


/*#***********************************************************************\
 *
 *              ha_map_mysql_solid_fkaction
 *
 * Maps a MySQL foreign key referential action code to solidDB foreign key
 * referential action.
 *
 * Parameters :
 *
 *     foreign_key* foreign_key, in, use, MySQL foreign key definition
 *
 * Return value : sqlrefact_t
 *
 * Globals used :
 */
static inline sqlrefact_t ha_map_mysql_solid_fkaction(
        uint fk_ref_action)
{
        sqlrefact_t solid_ref_action;

        switch (fk_ref_action) {
#if MYSQL_VERSION_ID >= 50120
            case Foreign_key::FK_OPTION_RESTRICT:
#else
            case foreign_key::FK_OPTION_RESTRICT:
#endif
                solid_ref_action = SQL_REFACT_RESTRICT;
                ss_pprintf_1((" RESTRICT\n"));
                break;
#if MYSQL_VERSION_ID >= 50120
            case Foreign_key::FK_OPTION_CASCADE:
#else
            case foreign_key::FK_OPTION_CASCADE:
#endif
                solid_ref_action = SQL_REFACT_CASCADE;
                ss_pprintf_1((" CASCADE\n"));
                break;
#if MYSQL_VERSION_ID >= 50120
            case Foreign_key::FK_OPTION_SET_NULL:
#else
            case foreign_key::FK_OPTION_SET_NULL:
#endif
                solid_ref_action = SQL_REFACT_SETNULL;
                ss_pprintf_1((" SET NULL\n"));
                break;
#if MYSQL_VERSION_ID >= 50120
            case Foreign_key::FK_OPTION_NO_ACTION:
#else
            case foreign_key::FK_OPTION_NO_ACTION:
#endif
                solid_ref_action = SQL_REFACT_NOACTION;
                ss_pprintf_1((" NO ACTION\n"));
                break;
#if MYSQL_VERSION_ID >= 50120
            case Foreign_key::FK_OPTION_DEFAULT:
#else
            case foreign_key::FK_OPTION_DEFAULT:
#endif
                solid_ref_action = SQL_REFACT_SETDEFAULT;
                ss_pprintf_1((" SET DEFAULT\n"));
                break;
            default:
                /* If no foreign key action have been defined
                   use RESTRICT action */
                solid_ref_action = SQL_REFACT_RESTRICT;
                ss_pprintf_1((" RESTRICT\n"));
                break;
        }

        return (solid_ref_action);
}

/*#***********************************************************************\
 *
 *              solid_find_key - UNUSED
 *
 * Find a key_info for this foreign key definition.
 *
 * Parameters :
 *
 *    Key*   key, in, use
 *    uint   n_keys, in, use, number of keys
 *    st_key key_info, in, use
 *
 * Return value :  MySQL key information
 *
 * Globals used :
 */
static st_key *solid_find_key(
        Key*    key,
        uint    n_keys,
        st_key* key_info)
{
        uint n_key;
        st_key* keyi = NULL;

        for(n_key = 0; n_key < n_keys; n_key++) {
#if MYSQL_VERSION_ID >= 50120
            List_iterator<Key_part_spec> col_iterator(key->columns);
            Key_part_spec* key_part;
#else
            List_iterator<key_part_spec> col_iterator(key->columns);
            key_part_spec* key_part;
#endif

            keyi = &(key_info[n_key]);

            ss_pprintf_1(("Key name %s\n", keyi->name));
            ss_pprintf_1(("Key elements %ld == %ld\n", keyi->key_parts, key->columns.elements));

            if (keyi->key_parts == key->columns.elements) {
                uint n_col;

                n_col = 0;
                while((key_part = col_iterator++)) {
                    ss_pprintf_1(("Key col name '%s' == key_part->field_name '%s'\n", key_part->field_name,
                              keyi->key_part[n_col].field->field_name));

                    if ((strcmp(key_part->field_name.str, keyi->key_part[n_col].field->field_name))) {
                        break;
                    }

                    n_col++;
                }

                if (key_part == NULL) {
                    break;
                }
            }
        }

        if (n_key == n_keys) {
            keyi = NULL;
        }

        return (keyi);
}

/*#***********************************************************************\
 *
 *              solid_check_charset
 *
 * Check field charset
 *
 * Parameters :
 *
 *    Field*       field, in, use, MySQL field
 *    const char*  table_name, in, use, table name
 *
 * Return value : 0 or HA_ERR_UNKNOWN_CHARSET
 *
 * Globals used : -
 */
static inline int solid_check_charset(
        Field*      field,
        const char* table_name)
{
        CHARSET_INFO *charset = field->charset();

        ss_dassert(charset != NULL);

        if (!su_collation_supported(charset->number)) {

            sql_print_error("This version of MySQL/solidDB does not support character "
                            "set %s or collation %s used in table %s field %s",
                            charset->csname,
                            charset->name,
                            table_name, field->field_name);

            return (HA_ERR_UNKNOWN_CHARSET);
        }

        return (0);
}

/*#***********************************************************************\
 *
 *              solid_set_foreign_keys
 *
 * This function sets foreign key definitions from the list to array
 *
 * Parameters :
 *
 *   su_list_t*        fkeys, in, use, foreign key definitions in list
 *   rs_entname_t*     table_name, in, use, table name
 *   tb_sqlforkey_t**  forkeys, in out, use, foreign key definitions in array
 *   uint*             n_fkeys, in out, number of foreign key definitions
 *
 * Return value : -
 *
 * Globals used : -
 */
static void solid_set_foreign_keys(
        su_list_t*       fkeys,
        rs_entname_t*    table_name,
        tb_sqlforkey_t** forkeys,
        uint*            n_fkeys)
{
        tb_sqlforkey_t* new_forkeys = NULL;
        uint len = 0;

        ss_dprintf_3(("solid_set_foreign_keys"));
        SS_PUSHNAME("solid_set_foreign_keys");

        if (fkeys) {
            len = su_list_length(fkeys);
        }

        if (len > 0 ) {
            uint item = 0;
            su_list_node_t* node = NULL;

            new_forkeys = (tb_sqlforkey_t*)SsMemCalloc(1,(sizeof(tb_sqlforkey_t) * len));

            su_list_do(fkeys, node) {
                tb_sqlforkey_t* fkey;

                ss_dassert(node != NULL);
                fkey = (tb_sqlforkey_t*)su_listnode_getdata(node);
                ss_dassert(fkey != NULL);

                if (fkey->refschema == NULL) {
                    fkey->refschema = (char *)SsMemStrdup(table_name->en_schema);

                    solid_my_caseup_str_if_lower_case_table_names(fkey->refschema);
                }

                new_forkeys[item] = *fkey; /* Contents copy */
                SsMemFree(fkey); /* Here only the list item is freed because the contents
                                    and pointers of the item are copied above to an array item */
                item++;
            }

            su_list_done(fkeys);
            fkeys = NULL;

            *forkeys = new_forkeys;
        } else {
            if (fkeys) {
                su_list_done(fkeys);
                fkeys = NULL;
            }
        }

        *n_fkeys = len;

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_fkeys_done
 *
 * Delete all foreign key definitions, this should be used in error
 * handling
 *
 * Parameters :
 *
 *   su_list_t*        fkey_list, in, use, foreign key definitions in list
 *
 * Return value : -
 *
 * Globals used : -
 */
static void solid_fkeys_done(
        su_list_t* fkey_list)
{
        SS_PUSHNAME("solid_fkeys_done");

        if (fkey_list) {
            su_list_node_t* node;

            su_list_do(fkey_list, node) {
                tb_sqlforkey_t* fkey = NULL;
                ss_dassert(node != NULL);
                fkey = (tb_sqlforkey_t*) su_listnode_getdata(node);
                ss_dassert(fkey != NULL);

                ha_mysql_solid_foreign_done(fkey, 1);
            }

            su_list_done(fkey_list);
        }

        SS_POPNAME;
}

/*#***********************************************************************\
 *
 *              solid_check_foreign_keys
 *
 * Check that foreign keys are sensible
 *
 * Parameters :
 *
 *    rs_sysi_t*      cd, in, use, system info client data
 *    rs_ttype_t*        ttype, in, use, table type
 *    tb_trans_t*        trans, in, use, transaction
 *    rs_entname_t*      table_name, inout, table name in create table
 *    su_list_t*         fkey_list, in, use, foreign key list
 *    rs_err_t**      p_errh, in out, error structure
 *
 * Return value : TRUE or FALSE
 *
 * Globals used :
 */
static bool solid_check_foreign_keys(
        rs_sysi_t*    cd,
        rs_ttype_t*   ttype,
        tb_trans_t*   trans,
        rs_entname_t* table_name,
        su_list_t*    fkey_list,
        rs_err_t**    p_errh)
{
        su_list_node_t* node;
        bool         succp = TRUE;
        uint n_col;
        int  error = 0;

        SS_PUSHNAME("solid_check_foreign_keys");
        ss_dassert(fkey_list != NULL);

        /* Here we should do sanity checks for foreign keys */

        su_list_do(fkey_list, node) {
            tb_sqlforkey_t* fkey = (tb_sqlforkey_t*) su_listnode_getdata(node);
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            fkey->unresolved = FALSE;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
            
            /* This version of solidDB for MySQL does not support
               SET DEFAULT. This is because in alter table user can
               change default value of the attribute but this change
               is not notified to storage engine */
            if (fkey->delrefact == SQL_REFACT_SETDEFAULT ||
                fkey->updrefact == SQL_REFACT_SETDEFAULT) {

                sql_print_error("This version of MySQL/solidDB does not support "
                                "SET_DEFAULT option on referential actions.");

                rs_error_create(p_errh, E_CONSTRCHECKFAIL_S, fkey->name);
                succp = FALSE;
                break;
            }

            /* Check that referenced field and referencing field has the
               same charset and collation */

            for(n_col = 0; n_col < fkey->len; n_col++) {

                /* Need only to check if not self referencing foreign key */
                if((!fkey->refschema ||
                    !strlen(fkey->refschema) ||
                    !strlen(table_name->en_schema) ||
                    strcmp(fkey->refschema, table_name->en_schema)
                    ) &&
                   strcmp(fkey->reftable, table_name->en_name)) {

                    rs_relh_t*   ref_relh;
                    rs_entname_t new_en;

                    if (fkey->refschema) {
                        rs_entname_initbuf(&new_en, NULL, fkey->refschema, fkey->reftable);
                    } else {
                        rs_entname_initbuf(&new_en, NULL, table_name->en_schema, fkey->reftable);
                    }

                    ref_relh = tb_dd_getrelh(cd, trans, &new_en, NULL, p_errh);

                    if (ref_relh) {
                        rs_atype_t* ref_atype;
                        rs_atype_t* atype;
                        rs_ttype_t* ref_ttype;
                        su_collation_t* col;
                        su_collation_t* ref_col;
                        char* ref_aname;
                        rs_ano_t ref_ano;

                        ref_ttype = rs_relh_ttype(cd, ref_relh);

                        if (fkey->reffields == NULL) {
                            ref_aname = (char *)rs_ttype_sql_aname(cd, ref_ttype, n_col);
                        } else {
                            ss_dassert(fkey->reffields[n_col]);
                            ref_aname = (char *)fkey->reffields[n_col];
                        }

                        ref_ano = rs_ttype_sql_anobyname(cd, ref_ttype, ref_aname);

                        if (ref_ano != RS_ANO_NULL) {
                            ref_atype = rs_ttype_sql_atype(cd, ref_ttype, ref_ano);

                            ss_dassert((int)fkey->fields[n_col] >= 0);
                            atype = rs_ttype_sql_atype(cd, ttype, fkey->fields[n_col]);

                            col = rs_atype_collation(cd, ref_atype);
                            ref_col = rs_atype_collation(cd, atype);

                            if (col != ref_col) {
                                rs_error_create(p_errh, E_FORKINCOMPATDTYPE_S, rs_ttype_sql_aname(cd, ttype, fkey->fields[n_col]));
                                rs_relh_done(cd, ref_relh);
                                SS_POPNAME;
                                return(FALSE);
                            }
                        } else {
                            rs_error_create(p_errh, E_ATTRNOTEXISTONREL_SS, ref_aname, table_name->en_name);
                            rs_relh_done(cd, ref_relh);
                            SS_POPNAME;
                            return(FALSE);
                        }

                        rs_relh_done(cd, ref_relh);
                    } else {
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED                    
                        if (*p_errh) {
                            error = su_err_geterrcode(*p_errh);
                        }
                        if (error == E_RELNOTEXIST_S) {
                            fkey->unresolved = TRUE;
                            
                            rs_error_create(p_errh, E_UNRESFKEYS_S, fkey->name);
                            succp = FALSE;
                        }
                        else
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */                        
                        {
                            SS_POPNAME;
                            return(FALSE);
                        }
                    }
                }
            }
        }

        SS_POPNAME;
        return (succp);
}

/*#***********************************************************************\
 *
 *              create_foreign_key_definitions
 *
 * Create solid foreign key definitions based on MySQL query string
 *
 * Parameters :
 *
 *    rs_sysi_t*         cd, in, use, system info client data
 *    tb_trans_t*        trans, in, use, transaction
 *    rs_ttype_t*        ttype, in, use, table type
 *    rs_entname_t*      new_table_name, inout, table name in create table
 *    char *             query, in, use, MySQL query string
 *    uint*              n_forkey, out, number of foreign keys
 *    tb_sqlforkey_t**   forkeys, out, foreign key definitions
 *    rs_err_t**         p_errh, in out, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int create_foreign_key_definitions(
        rs_sysi_t*          cd,
        tb_trans_t*         trans,
        rs_ttype_t*         ttype,
        rs_entname_t*       new_table_name,
        const char*         query,
        uint*               n_forkey,
        tb_sqlforkey_t**    forkeys,
        rs_err_t**          p_errh)
{
        uint         n_fkeys = 0;
        int          error = 0;
        rs_err_t*    errh = NULL;
        su_list_t*   fkey_list = NULL;
        bool         succp = TRUE;

        SS_PUSHNAME("create_foreign_key_definitions");

        /* Full foreign key parser for MySQL 5.0 & 5.1 */

        succp = tb_minisql_prepare_mysql_forkeys(cd, ttype,(char *)query,
                                                 solid_my_caseup_str_if_lower_case_table_names,
                                                 solid_my_caseup_str,
                                                 &fkey_list, &errh);

        if (succp) {
            if (fkey_list) {
                n_fkeys += su_list_length(fkey_list);
            }

            succp = solid_check_foreign_keys(cd, ttype, trans, new_table_name, fkey_list, &errh);
        }
        
        if (!succp || n_fkeys == 0) {
        
            if (errh) {
                error = su_err_geterrcode(errh);
            }
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            if (error == E_UNRESFKEYS_S && !tb_trans_get_foreign_key_checks(trans) &&
                n_fkeys > 0) {
                
                if (*p_errh) {
                    su_err_copyerrh(p_errh, errh);
                }
                
                solid_set_foreign_keys(fkey_list, new_table_name, forkeys, &n_fkeys);
                *n_forkey = n_fkeys;
                
                SS_POPNAME;
                return (error);
            }
            else
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
            {
                solid_fkeys_done(fkey_list);
    
                if (*p_errh) {
                    su_err_copyerrh(p_errh, errh);
                }
    
                *n_forkey = 0;
                *forkeys = NULL;
                SS_POPNAME;
                return (error);
            }
        }

        ss_dassert(errh == NULL);
        ss_pprintf_1(("Number of foreign key definitions %lu\n", n_fkeys));

        /* Set foreign keys to the create array */
        solid_set_foreign_keys(fkey_list, new_table_name, forkeys, &n_fkeys);

        *n_forkey = n_fkeys;

        SS_POPNAME;

        return (error);
}

/*#***********************************************************************\
 *
 *              solid_add_foreign_keys
 *
 * This function will parse MySQL query and create solidDB foreign key
 * definitions for all foreign keys added by
 *
 *  alter table x add foreign key ...
 *
 * Parameters :
 *
 *    rs_sysi_t*        cd, in, use, solidDB system information
 *    rs_ttype_t*       ttype,in, use, table type
 *    tb_trans_t*       trans, in, use, transaction
 *    rs_entname_t*     new_table_name, in, use, table name in create table
 *    rs_entname_t*     old_table_name, inout, table name in alter table
 *    char*             query, in, use, MySQL SQL-query
 *    tb_sqlforkey_t**  forkeys,inout, solidDB foreign key definitions
 *    uint*             n_fkeys, inout, number of new foreign keys
 *    rs_err_t**        p_errh, inout, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int solid_add_foreign_keys(
        rs_sysi_t* cd,
        rs_ttype_t* ttype,
        tb_trans_t* trans,
        rs_entname_t* new_table_name,
        rs_entname_t* old_table_name,
        const char* query,
        tb_sqlforkey_t** forkeys,
        uint* n_fkeys,
        rs_err_t** p_errh)
{
        int             error = 0;
        rs_err_t*       errh = NULL;
        su_list_t*      fkeys = NULL;
        uint            nfkeys;
        bool            succp = TRUE;
        tb_sqlforkey_t* fk = NULL;

        ss_dprintf_3(("solid_add_foreign_keys"));
        SS_PUSHNAME("solid_add_foreign_keys");


        succp = tb_minisql_prepare_mysql_forkeys(cd, ttype, (char *) query,
                                                 solid_my_caseup_str_if_lower_case_table_names,
                                                 solid_my_caseup_str,
                                                 &fkeys, &errh);

        if (succp) {
            if (fkeys) {
                nfkeys = su_list_length(fkeys);
            }
            
            succp = solid_check_foreign_keys(cd, ttype, trans, new_table_name, fkeys, &errh);
        }

        if (!succp) {
        
            if (errh) {
                error = su_err_geterrcode(errh);
            }
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            if (error == E_UNRESFKEYS_S && !tb_trans_get_foreign_key_checks(trans) &&
                nfkeys > 0) {
                
                if (*p_errh) {
                    su_err_copyerrh(p_errh, errh);
                }
                
                solid_set_foreign_keys(fkeys, new_table_name, forkeys, &nfkeys);
                *n_fkeys = nfkeys;
                
                SS_POPNAME;
                return (error);
            }
            else
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */
            {
                solid_fkeys_done(fkeys);
    
                if (*p_errh) {
                    su_err_copyerrh(p_errh, errh);
                }
    
                *n_fkeys = 0;
                *forkeys = NULL;
                SS_POPNAME;
                return (error);
            }
        }

        solid_set_foreign_keys(fkeys, new_table_name, forkeys, n_fkeys);

        /* When referencing the table being
             * altered, should reference new temporary table which later will
             * take name of the table being altered.
             */

        fk = *forkeys;

        for(unsigned k = 0; k < *n_fkeys; ++k) {
            /* to the moment all identifiers should already
             * be converted to internal representation so we
             * can use binary comparison instead of system charset.
             */
            if((!fk[k].refschema ||
                !strlen(fk[k].refschema) ||
                !strlen(old_table_name->en_schema) ||
                !strcmp(fk[k].refschema, old_table_name->en_schema)
                   ) &&
               !strcmp(fk[k].reftable, old_table_name->en_name)) {

                if(fk[k].refschema) {
                    SsMemFree(fk[k].refschema);
                }

                SsMemFree(fk[k].reftable);

                fk[k].refschema = (char*)SsMemStrdup(new_table_name->en_schema);
                fk[k].reftable = (char*)SsMemStrdup(new_table_name->en_name);
            }
        }

        if (!succp && errh) {
            error = su_err_geterrcode(errh);
        }

        SS_POPNAME;
        return (error);
}

/*#***********************************************************************\
 *
 *              solid_drop_foreign_keys
 *
 * This function will parse MySQL query and create a list of foreign keys
 * to be dropped. After this these foreign keys are tried to drop.
 *
 *  alter table x drop foreign key ...
 *
 * Parameters :
 *
 *    rs_sysi_t*        cd, in, use, solidDB system information
 *    tb_trans_t*       trans,in ,use, transaction
 *    rs_relh_t*        relh, in, use, relation where foreign keys should be deleted
 *    rs_entname_t*     en, in, use, relation name
 *    char*             query, in, use, MySQL SQL-query
 *    rs_err_t**        p_errh, inout, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int solid_drop_foreign_keys(
        rs_sysi_t* cd,
        tb_trans_t* trans,
        rs_relh_t* relh,
        rs_entname_t* en,
        const char *query,
        rs_err_t** p_errh)
{
        int error = 0;
        su_list_t *drop_fkeys = NULL;

        ss_dprintf_3(("solid_drop_foreign_keys"));
        SS_PUSHNAME("solid_drop_foreign_keys");

        bool succp = TRUE;

        succp = tb_minisql_prepare_drop_forkeys(cd, (char *)query,
                                                &drop_fkeys, p_errh);

        if (succp && drop_fkeys) {
            su_list_node_t* node = NULL;

            su_list_do(drop_fkeys, node) {
                char *fkey_name;
                char *solid_name;
                void *cont = NULL;

                ss_dassert(node != NULL);
                fkey_name = (char *)su_listnode_getdata(node);
                ss_dassert(fkey_name != NULL);

                solid_name = foreignkey_name_from_mysql_to_solid(cd, relh, fkey_name);

                succp = tb_dropconstraint(cd, trans, relh, en->en_schema, NULL, solid_name, &cont, p_errh);

                ss_dassert(cont == NULL);

                SsMemFree(solid_name);

                if (!succp) {

                    error = su_err_geterrcode(*p_errh);

                    if (error == E_CONSTRAINT_NOT_FOUND_S) {
                        su_err_done(*p_errh);

                        rs_error_create(p_errh, E_CONSTRAINT_NOT_FOUND_S, fkey_name);
                    }

                    break;
                }
            }
        }

        if (drop_fkeys) {
            su_list_done(drop_fkeys);
            drop_fkeys = NULL;
        }

        SS_POPNAME;
        return (error);
}

/*#***********************************************************************\
 *
 *              copy_foreign_key_definitions
 *
 * Copy solid foreign key definitions from another table.
 *
 * Parameters :
 *
 *    rs_sysi_t*      cd, in, use, system info client data
 *    tb_trans_t*        trans, in, use, transaction
 *    rs_ttype_t*        new_ttype, in, use, table type
 *    rs_entname_t*      en, new table name
 *    rs_entname_t*      old_en, old table name
 *    const char*        query, in, use, MySQL query string
 *    uint*           n_forkey, out, number of foreign keys
 *    tb_sqlforkey_t**   forkeys, out, foreign key definitions
 *    rs_err_t**      p_errh, inout, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int copy_foreign_key_definitions(
        rs_sysi_t*          cd,
        tb_trans_t*         trans,
        rs_ttype_t*         new_ttype,
        rs_entname_t*       en,
        rs_entname_t*       old_en,
        const char*         query,
        uint*               n_forkey,
        tb_sqlforkey_t**    forkeys,
        rs_err_t**          p_errh)
{
        uint            n_fkeys;
        rs_relh_t*      old_relh;
        su_pa_t*        refkeys;
        int             i;
        int             k;
        uint            fki;
        tb_sqlforkey_t* foreign_keys = NULL;
        int             error = 0;

        ss_dprintf_3(("copy_foreign_key_definitions\n"));
        SS_PUSHNAME("copy_foreign_key_definitions");

        old_relh = tb_dd_getrelh(cd, trans, (rs_entname_t*)(old_en), NULL, NULL);

        if (old_relh == NULL) {
            SS_POPNAME;
            /* This is ok when you alter e.g. myisam table to soliddb */
            return(0);
        }

        n_fkeys = 0;
        i = 0;

        error = solid_add_foreign_keys(cd, new_ttype, trans, en, old_en, query, &foreign_keys, &n_fkeys, p_errh);

        if (error) {
            SS_MEM_SETUNLINK(old_relh);
            rs_relh_done(cd, old_relh);
            *forkeys = foreign_keys;
            *n_forkey = n_fkeys;
            SS_POPNAME;
            return(error);
        }

        error = solid_drop_foreign_keys(cd, trans, old_relh, old_en, query, p_errh);

        if (error) {
            SS_MEM_SETUNLINK(old_relh);
            rs_relh_done(cd, old_relh);
            *forkeys = foreign_keys;
            *n_forkey = n_fkeys;
            SS_POPNAME;
            return (error);
        }

        refkeys = rs_relh_refkeys(cd, old_relh);

        su_pa_do(refkeys, i) {
            rs_key_t* key;

            key = (rs_key_t*)su_pa_getdata(refkeys, i);

            if (rs_key_type(cd, key) == RS_KEY_FORKEYCHK) {
                rs_ano_t j;
                rs_ano_t n_parts;
                rs_relh_t *refrelh;
                rs_ano_t k_part;
                rs_err_t* errh = NULL;
                su_pa_t* forrefkeys = NULL;
                rs_key_t* forkey = NULL;
                tb_sqlforkey_t* newforkey;
                rs_ttype_t* old_ttype;
                rs_ttype_t* ref_ttype;

                n_fkeys++;

                old_ttype = rs_relh_ttype(cd, old_relh);

                if (foreign_keys == NULL) {
                    foreign_keys = (tb_sqlforkey_t*)SsMemAlloc(sizeof(tb_sqlforkey_t));
                } else {
                    foreign_keys = (tb_sqlforkey_t*)SsMemRealloc(foreign_keys, sizeof(tb_sqlforkey_t) * n_fkeys);
                }

                newforkey = &foreign_keys[n_fkeys - 1];

                /* Clear content so that we release only allocated fields if need to return
                 * in the middle. */
                memset(newforkey, '\0', sizeof(tb_sqlforkey_t));

                n_parts = rs_key_nparts(cd, key);

                newforkey->mysqlname = (char*)SsMemStrdup(rs_key_name(cd, key));
                keyname_from_solid_to_mysql(rs_key_name(cd, key), newforkey->mysqlname);
                newforkey->name = NULL;

                newforkey->fields = (uint*)SsMemAlloc(n_parts * sizeof(newforkey->fields));

                ss_dprintf_4(("copy_foreign_key_definitions:name=%s\n", newforkey->name));

                for (k_part = 0, fki = 0; k_part < n_parts; k_part++) {
                    rs_attrtype_t kptype = rs_keyp_parttype(cd, key, k_part);

                    if (kptype == RSAT_USER_DEFINED ||
                        kptype == RSAT_COLLATION_KEY) {
                        char* aname;
                        rs_ano_t kp_ano;
                        rs_ano_t new_ano;

                        kp_ano = rs_keyp_ano(cd, key, k_part);
                        aname = rs_ttype_aname(cd, old_ttype, kp_ano);
                        ss_dassert(aname != NULL);
                        new_ano = rs_ttype_anobyname(cd, new_ttype, aname);

                        if (new_ano == RS_ANO_NULL) {
                            rs_error_create(p_errh, E_CANNOTDROPFORKEYCOL_S, key);
                            error = su_err_geterrcode((su_err_t*)*p_errh);
                            newforkey->len = fki;
                            goto copy_err;
                        }

                        ss_dprintf_4(("copy_foreign_key_definitions:add field %d, index %d\n", new_ano, fki));

                        newforkey->fields[fki++] = new_ano;
                    }
                }

                newforkey->len = fki;

                refrelh = tb_dd_getrelhbyid_ex(cd,
                                               trans,
                                               rs_key_refrelid(cd, key),
                                               &errh);

                if (errh) {
                    /* Someone could have dropped the referenced table. */
                    ss_dassert(refrelh == NULL);
                    sql_print_error("%s (%s)", su_err_geterrstr(errh), query);
                    su_err_done(errh);
                    errh = NULL;
                    error = -1;
                    break;
                }

                ss_dassert(refrelh != NULL);

                if (rs_relh_relid(cd, old_relh) == rs_key_refrelid(cd, key)) {
                    /* Self referencing foreign key */
                    newforkey->reftable = (char*)SsMemStrdup(en->en_name);
                    newforkey->refschema = (char *)SsMemStrdup(en->en_schema);

                    if(en->en_catalog) {
                        newforkey->refcatalog = (char *)SsMemStrdup(en->en_catalog);
                    } else {
                        newforkey->refcatalog = NULL;
                    }
                } else {
                    newforkey->reftable = (char*)SsMemStrdup(rs_relh_name(cd, refrelh));
                    newforkey->refschema =(char*) SsMemStrdup(rs_relh_schema(cd, refrelh));
                    newforkey->refcatalog = (char*)SsMemStrdup(rs_relh_catalog(cd, refrelh));
                }

                newforkey->reffields = (char**)NULL;

                tb_dd_resolverefkeys(cd, refrelh);
                forrefkeys = rs_relh_refkeys(cd, refrelh);
                ref_ttype = rs_relh_ttype(cd, refrelh);
                k = 0;

                newforkey->delrefact = (sqlrefact_t)rs_key_delete_action(cd, key);
                newforkey->updrefact = (sqlrefact_t)rs_key_update_action(cd, key);

                su_pa_do(forrefkeys, k) {
                    forkey = (rs_key_t*)su_pa_getdata(forrefkeys, k);
                    n_parts = rs_key_nparts(cd, forkey);

                    if (rs_key_type(cd, forkey) != RS_KEY_FORKEYCHK
                        && rs_key_refrelid(cd, forkey) == rs_relh_relid(cd, old_relh)
                        && (!(strcmp(rs_key_name(cd, forkey), rs_key_name(cd, key))))) {
                        ss_dassert(newforkey->reffields == NULL);

                        newforkey->reffields = (char**)SsMemAlloc(n_parts * sizeof(newforkey->reffields));

                        for (j = 0, fki = 0; j < n_parts; j++) {
                            rs_ano_t key_ano;
                            rs_attrtype_t kptype;

                            key_ano = rs_keyp_ano(cd, forkey, j);

                            kptype = rs_keyp_parttype(cd, forkey, j);

                            if (kptype == RSAT_USER_DEFINED ||
                                kptype == RSAT_COLLATION_KEY) {
                                char* aname;

                                aname = rs_ttype_aname(cd, ref_ttype, key_ano);
                                ss_dprintf_4(("copy_foreign_key_definitions:add reffield %s, index %d\n", aname, fki));
                                newforkey->reffields[fki++] = (char*)SsMemStrdup(aname);
                            }
                        }

                        ss_dassert(newforkey->len == fki);
                    }
                }

                SS_MEM_SETUNLINK(refrelh);
                rs_relh_done(cd, refrelh);
            }
        }

copy_err:

        SS_MEM_SETUNLINK(old_relh);
        rs_relh_done(cd, old_relh);

        *n_forkey = n_fkeys;
        *forkeys  = foreign_keys;

        SS_POPNAME;

        return (error);
}

/*#***********************************************************************\
 *
 *              solid_parse_relmode
 *
 * Parse comment field from MySQL create table and set relation mode
 * based on that.
 *
 * Parameters :
 *
 *     LEX_STRING* comment, in, use, comment string
 *
 * Return value : One of the below
 *
 * TB_RELMODE_SYSDEFAULT    if parse error or none given
 * TB_RELMODE_OPTIMISTIC    if MODE=OPTIMISTIC
 * TB_RELMODE_PESSIMISTIC   if MODE=PESSIMISTIC
 * TB_RELMODE_MAINMEMORY    if MODE=MAINMEMORY
 *
 * Limitations:
 *
 * Globals used :
 */
static tb_relmode_t solid_parse_relmode(
        LEX_STRING* comment_string)
{
        tb_relmode_t solid_relmode = TB_RELMODE_SYSDEFAULT;
        char* comment = NULL;
        const char* type_beg;
        size_t idst=0;

        if (comment_string == NULL ||
            comment_string->str == NULL ||
            comment_string->length == 0) {

            return TB_RELMODE_SYSDEFAULT;
        }

        ss_pprintf(("Create table comment = %s\n", comment_string));

        /* size of comment_string is unknown a priori - we have to allocate dyn. buffer */
        comment = (char*)SsMemAlloc(comment_string->length+1);
        ss_dassert(comment != NULL);

        /* copying the comment string, converting it to upper case and
         * removing white spaces.
         */

        for( size_t i = 0; i < comment_string->length; ++i ) {
            if ( !strchr( SQL_WHITESPACES, comment_string->str[i] ) ) {
              comment[idst++] = toupper( comment_string->str[i] );
            }
        }

        if (comment) {
            comment[idst]=0;

            if ((type_beg = strstr(comment, "MODE="))) {
                type_beg += sizeof("MODE=")-1; /* -1 - without '\0' */

                if(strncmp( type_beg, "PESSIMISTIC", sizeof("PESSIMISTIC")-1) == 0) {
                    solid_relmode = TB_RELMODE_PESSIMISTIC;
                } else if (strncmp( type_beg, "OPTIMISTIC", sizeof("OPTIMISTIC")-1 ) == 0) {
                    solid_relmode = TB_RELMODE_OPTIMISTIC;
                } else if (strncmp( type_beg, "MAINMEMORY", sizeof("MAINMEMORY")-1 ) == 0) {
                    solid_relmode = TB_RELMODE_MAINMEMORY;
                }
            }
        }

        if (comment) {
            SsMemFree(comment);
        }

        return solid_relmode;
}

/*#***********************************************************************\
 *
 *              solid_check_keys
 *
 * Check that fields used in the index contains supported charset and collation and
 * that if this index is a prefix index the field type is correct. Additionally,
 * check that we really at the moment can support this kind of prefix index.
 *
 * Parameters :
 *
 *     TABLE*      table, in, use, information on table columns and indexes
 *     THD*        thd, in, use, MySQL thread
 *     const char* table_name, in, use, table_name
 *
 * Return value : One of the below
 *
 *     TRUE   success
 *     FALSE  failure
 *
 * Limitations:
 *
 * Globals used :
 */
static bool solid_check_keys(
        TABLE*      table,
        MYSQL_THD   thd,
        const char* table_name)
{
        uint        n_fields;
        Field*      field = NULL;
        uint        n_keys = table->s->keys;
        uint        n_key;
        uint        n_field;
        bool        blob = FALSE;

        n_key = 0;

        while(n_key < n_keys) {
            KEY*        key = &(table->key_info[n_key]);
            n_fields = key->key_parts;

            for (n_field = 0; n_field < n_fields; n_field++) {
                KEY_PART_INFO*  key_part;
                uint j;

                key_part = key->key_part + n_field;

                for (j = 0; j < table->s->fields; j++) {

                    field = table->field[j];

                    if (0 == my_strcasecmp(system_charset_info, field->field_name,
                                           key_part->field->field_name)) {
                        /* Found the corresponding column */

                        break;
                    }
                }

                if (field->has_charset()) {

                    if(solid_check_charset(field, table_name)) {
                        return (FALSE);
                    }
                }

                ss_dassert(j < table->s->fields);

                switch(field->type()) {
                    case FIELD_TYPE_GEOMETRY:
                    case FIELD_TYPE_TINY_BLOB:
                    case FIELD_TYPE_MEDIUM_BLOB:
                    case FIELD_TYPE_BLOB:
                    case FIELD_TYPE_LONG_BLOB:
                        blob = TRUE;
                        break;
                    default:
                        blob = FALSE;
                        break;
                }

                if (blob
                    || (field->type() != MYSQL_TYPE_VARCHAR &&
                        key_part->length < field->pack_length())
                    || (field->type() == MYSQL_TYPE_VARCHAR
                        && key_part->length < field->pack_length()
                        -((Field_varstring*)field)->length_bytes)) {

                    switch(field->type()) {
                        case FIELD_TYPE_LONG:
                        case FIELD_TYPE_LONGLONG:
                        case FIELD_TYPE_TINY:
                        case FIELD_TYPE_SHORT:
                        case FIELD_TYPE_INT24:
                        case FIELD_TYPE_DATE:
                        case FIELD_TYPE_DATETIME:
                        case FIELD_TYPE_YEAR:
                        case FIELD_TYPE_NEWDATE:
                        case FIELD_TYPE_TIME:
                        case FIELD_TYPE_TIMESTAMP:
                        case FIELD_TYPE_FLOAT:
                        case FIELD_TYPE_DOUBLE:
                        case FIELD_TYPE_DECIMAL:
                            sql_print_error("Invalid column type for prefix index field %s table %s index %s",
                                            field->field_name, table_name, key->name);
                            return (FALSE);
                        default: {
                            break;
                        }
                    }
                }
            }

            n_key++;
        }

        return (TRUE);
}

/*#***********************************************************************\
*
*              solid_check_table_charset
*
* Check that used charset and collation for the table are supported
* in solidDB for MySQL
*
* Parameters :
*
* HA_CREATE_INFO*  create_info,in, use, table create information
* const char*      create_sql, in, use, create table SQL-query
* const char*      table_name, in, use, table name
*
* Return value : TRUE if charset and collation are supported
*                FALSE otherwise
*
* Globals used : -
*/
static inline bool solid_check_table_charset(
        HA_CREATE_INFO* create_info,
        const char*     create_sql,
        const char*     table_name)
{
    ss_dassert(create_info != NULL);
    ss_dassert(create_sql != NULL);

    /* If no table option COLLATE were given and default collation is not
       supported we change the collation to supported collation and
       give a warning unknown collation to user. */

    if (create_info->used_fields | HA_CREATE_USED_DEFAULT_CHARSET &&
        create_info->default_table_charset &&
        !su_collation_supported(create_info->default_table_charset->number)) {

        push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_UNKNOWN_COLLATION, ER(ER_UNKNOWN_COLLATION),
                            create_info->default_table_charset->name);

        if (su_collation_supported_charset(create_info->default_table_charset->csname)) {
            return TRUE;
        } else {
            sql_print_error("This version of MySQL/solidDB does not support character "
                            "set %s or collation %s used in table %s",
                            create_info->default_table_charset->csname,
                            create_info->default_table_charset->name,
                            table_name);

            return FALSE;
        }
    }

    if (create_info->table_charset &&
        !su_collation_supported(create_info->table_charset->number)) {

        sql_print_error("This version of MySQL/solidDB does not support character "
                        "set %s or collation %s used in table %s",
                        create_info->table_charset->csname,
                        create_info->table_charset->name,
                        table_name);

        return FALSE;
    }

    return TRUE;
}

/*#***********************************************************************\
 *
 *              check_foreign_key_indexes
 *
 * If foreign keys have been defined for this table check that
 * no index associated to the foreign key is dropped.
 *
 * Parameters :
 *
 *    TABLE*          table, in, use, MySQL table definition
 *    rs_sysi_t*      cd, in, use, system info client data
 *    rs_ttype_t*     ttype, in, use, table type
 *    rs_entname_t*   en, in, use, solidDB table name
 *    char *          query, in, use, MySQL query string
 *    uint            n_forkeys, in, use, number of foreign keys
 *    tb_sqlforkey_t*   forkeys, in, use, foreign key definitions
 *    rs_err_t**      p_errh, inout, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
static int check_foreign_key_indexes(
        TABLE*          table,
        rs_sysi_t*      cd,
        rs_ttype_t*     ttype,
        rs_entname_t*   en,
        const char*     query,
        uint            n_forkeys,
        tb_sqlforkey_t* forkeys,
        rs_err_t**      p_errh)

{
        uint n_forkey;
        uint n_keys;

        if (!(tb_minisql_is_drop_index(cd, query))) {
            return (0);  /* Query does not drop any indexes */
        }

        if (table->s->keys == 0) {
            rs_error_create(p_errh, E_INDEX_IS_USED_S, (char *)""); /* No indexes found */
            return (su_err_geterrcode(*p_errh));
        }

        SS_PUSHNAME("check_foreign_key_indexes");
        n_keys = table->s->keys;

        for(n_forkey = 0; n_forkey < n_forkeys; n_forkey++) {
            uint n_key;
            st_key* find_key=NULL;

            for(n_key = 0; n_key < n_keys; n_key++) {
                uint key_parts;
                uint key_part;
                st_key* mkey = &(table->key_info[n_key]);

                key_parts = mkey->key_parts;

                for(key_part = 0; key_part < key_parts; key_part++) {
                    rs_ano_t col_ano;
                    char *mfield_name = (char *)SsMemStrdup((char *)mkey->key_part[key_part].field->field_name);
                    char *sfield_name;

                    solid_my_caseup_str(mfield_name);

                    col_ano = forkeys[n_forkey].fields[key_part];
                    sfield_name = rs_ttype_sql_aname(cd, ttype, col_ano);

                    if (strcmp(mfield_name, sfield_name)) {
                        SsMemFree(mfield_name);
                        break; /* Not found */
                    }

                    SsMemFree(mfield_name);
                }

                /* Found if reached the end of key parts */
                if (key_part == key_parts) {
                    find_key = &(table->key_info[n_key]);
                }
            }

            /* Create error if index for the foreign key is not found */
            if (!find_key) {
                SS_POPNAME;
                rs_error_create(p_errh, E_INDEX_IS_USED_S, forkeys[n_forkey].mysqlname);
                return (su_err_geterrcode(*p_errh));
            }
        }

        SS_POPNAME;
        return (0);
}

/*#***********************************************************************\
 *
 *              update_child_foreign_keys
 *
 * If the old table is referenced by foreign key we need to update
 * relation_id in these foreign keys because the new table has a new
 * relation_id.
 *
 * Parameters :
 *
 *    THD*            thd, in, use, MySQL thread data
 *    rs_sysi_t*      cd, in, use, system info client data
 *    tb_trans_t*     trans, in, use, solid transaction
 *    rs_relh_t*      relh, in, use, new solidDB relation
 *    rs_ttype_t*     ttype, in, use, table type
 *    rs_entname_t*   old_name, in, use, solidDB old table name
 *    rs_entname_t*   new_name, in, use, solidDB new table name
 *    rs_err_t**      p_errh, inout, error structure
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */

static int update_child_foreign_keys(
        MYSQL_THD     thd,
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        rs_relh_t*    relh,
        rs_ttype_t*   ttype,
        rs_entname_t* old_name,
        rs_entname_t* new_name,
        rs_err_t**    p_errh)
{
        rs_relh_t*    old_relh = NULL;
        int           rc       = 0;

        SS_PUSHNAME("update_child_foreign_keys");

        ss_dassert(cd != NULL);
        ss_dassert(thd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(relh != NULL);
        ss_dassert(ttype != NULL);
        ss_dassert(old_name != NULL);
        ss_dassert(new_name != NULL);
        ss_dassert(p_errh != NULL);

        old_relh = tb_dd_getrelh(cd, trans, old_name, NULL, NULL);

        if (old_relh) {
            if (tb_dd_isforkeyref(cd, trans, old_relh, p_errh)) {
                /* Now we know that the old table is really a parent table of some
                   child table containing a foreign key constraint referencing to this
                   table. Update relation id in these foreign keys in the system tables.
                */

                ss_dprintf_1(("update_child_foreign_keys old_relid = %lu new_relid=%lu\n",
                              rs_relh_relid(cd, old_relh), rs_relh_relid(cd, relh)));

                tb_dd_update_forkeys(cd, trans, relh, rs_relh_relid(cd, old_relh),
                                     rs_relh_relid(cd, relh), p_errh);

                tb_trans_setrelhchanged(cd, trans, old_relh, p_errh);
            }

            SS_MEM_SETUNLINK(old_relh);
            rs_relh_done(cd, old_relh);
        }

        if (p_errh && *p_errh) {
            rc = su_err_geterrcode(*p_errh);
        }

        SS_POPNAME;
        return (rc);
}

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
/*#***********************************************************************\
 *
 *              solid_create_unresolved_foreign_keys
 *
 * Create unresolved foreign keys which are previously were defined to reference
 * particular relation.
 *
 * Parameters :
 *
 *    rs_sysi_t*         cd, in, use, system info client data
 *    tb_trans_t*        trans, in, use, transaction
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int solid_create_unresolved_foreign_keys(
        rs_sysi_t*    cd,
        tb_trans_t*   trans,
        rs_entname_t* ref_relh_entname)
{
        int                   error = 0;
        rs_err_t*             errh  = NULL;
        bool                  bret  = FALSE;
        long                  n_forkeys;
        tb_sqlforkey_unres_t* forkeys_unres;
        int                   i;
        
        ss_dassert(cd != NULL);
        ss_dassert(trans != NULL);
        ss_dassert(ref_relh_entname != NULL);

        SS_PUSHNAME("solid_create_unresolved_foreign_keys");
        
        tb_dd_get_from_fkeys_unresolved(cd, trans,
                                        rs_entname_getname(ref_relh_entname),
                                        rs_entname_getschema(ref_relh_entname),
                                        rs_entname_getcatalog(ref_relh_entname),
                                        0,
                                        &n_forkeys,
                                        &forkeys_unres);
                                        
        if (n_forkeys > 0 && forkeys_unres != NULL) {
            for (i = 0; i < n_forkeys; ++i) {
            
                rs_relh_t*      cre_relh;
                tb_relh_t*      cre_table;
                rs_entname_t*   cre_relh_en;
                tb_sqlforkey_t* forkey;
                void*           dummy = NULL;
                
                cre_relh = tb_dd_getrelhbyid_ex(cd, trans, forkeys_unres[i].cre_rel_id, &errh);
                ss_dassert(cre_relh != NULL);
                
                cre_table = tb_relh_new(cd, cre_relh, NULL);
                ss_dassert(cre_table != NULL);
                
                cre_relh_en = rs_relh_entname(cd, cre_relh);
                
                forkey = (tb_sqlforkey_t*)(&forkeys_unres[i]);

                bret = tb_addforkey(cd, trans, cre_table,
                                    rs_entname_getschema(cre_relh_en),
                                    rs_entname_getcatalog(cre_relh_en),
                                    rs_entname_getname(cre_relh_en),
                                    forkey,
                                    &dummy,
                                    &errh);

                if (!bret) {
                    error = handle_error(errh);
                    
                    /* tb_relh_free also frees cre_relh*/
                    tb_relh_free(cd, cre_table);
                    
                    break;
                }
                
                tb_dd_delete_from_fkeys_unresolved(cd, trans, forkeys_unres[i].cre_rel_id);

                /* tb_relh_free also frees cre_relh*/
                tb_relh_free(cd, cre_table);
            }
        }
        
        tb_dd_free_fkeys_unresolved(n_forkeys, forkeys_unres);
        
        SS_POPNAME;

        return (error);
}
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */

/*#***********************************************************************\
*
*              ::create
*
* create() is called to create or alter a table.
* ALTER TABLE is handled as following:
*   - a new table is created with the structure of final table,
*     with a temporary name.
*   - data is copied from old table to new table, row by row.
*   - old table is renamed to a temporary name.
*   - new table is renamed to old table name.
*   - old table is dropped.
*
* All table modifications are handled really via ALTER TABLE
* even if the statement is not really ALTER (f.e. CREATE INDEX is
* handled by ALTER). Only trivial operations like renaming a table
* are handled specially without copying data. In 5.1. an interface
* to handle creation or dropping of indices is also added so in 5.1
* indices are handled different either.
*
* FRM file has already been created so adjusting
* create_info will not do you any good. You can overwrite the .frm file
* at this point if you need to change the table definition.
*
* Called from handle.cc by ha_create_table().
*
* Parameters :
*
*    const char*     name, in, full MySQL table name of new table
*    TABLE*          table_arg, in, use
*    HA_CREATE_INFO* create_info, in, use
*
* Return value : 0 or error code
*
* Globals used :
*/
int ha_soliddb::create(
        const char *name,
        TABLE *table_arg,               // in sql/table.h, struct st_table {
        HA_CREATE_INFO *create_info)    // in sql/handler.h, typedef struct st_ha_create_information
{
        MYSQL_THD     thd;
        SOLID_CONN*   con;
        tb_connect_t* tbcon;
        rs_sysi_t*    cd;
        tb_trans_t*   trans;
        rs_entname_t en;
        rs_entname_t old_name;
        const char* current_table_name_for_error;
/*        LEX_STRING q; */
        int         rc = 0;
        uint        i = 0;
        uint        n_cols = 0;
        rs_relh_t*    relh = NULL;
        tb_sqlunique_t* primkey = NULL;
        rs_ttype_t* ttype = NULL;
        su_err_t*   errh = NULL;
        bool        succp=TRUE;
        char*       aname=NULL;
        char*       catalog = NULL;
        char*       extrainfo = NULL;
        char*       query = NULL;
        void*       cont = NULL;
        long        seq_id = 0;
        rs_atype_t* atype = NULL;
        bool        auto_inc_defined = FALSE;
        bool        sysrelcreate = FALSE;
        solid_bool  finished = FALSE;
        uint        n_forkeys = 0;
        tb_sqlforkey_t* forkeys = NULL;
        su_err_t*   commit_errh = NULL;
        bool commit_succ_p = true;
        su_collation_t* table_collation = NULL;
        su_collation_t* column_collation = NULL;
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        bool  have_forkeys_unresolved = FALSE;
        long* forkeys_unresolved_ids = NULL;
        long  n_forkeys_unres_ids    = 0;
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        

        thd = current_thd;
        rs_entname_initbuf(&old_name,NULL,NULL,NULL);

        SDB_DBUG_ENTER("ha_solid::create");
        ss_pprintf_1(("ha_solid::create SQL: type %d, text [%s].\n",
                       soliddb_sql_command(thd),
                       soliddb_query(thd)));

        if( is_DDL_command( thd ) && !start_solid_DDL(rc) ) {
            SDB_DBUG_RETURN(rc);
        }

        if (is_soliddb_system_relation(name)) {
            ss_pprintf_1(("Trying to create SolidDB system table %s\n", name));
            SDB_DBUG_RETURN(HA_ERR_TABLE_EXIST);
        }

#if MYSQL_VERSION_ID > 50100
        {
            uint errors;
            uint len = (strlen(soliddb_query(thd)) +1 ) * 3;

            query = (char *)SsMemAlloc(len);

            strconvert(soliddb_charset(thd), soliddb_query(thd),
                       system_charset_info, query,
                       len, &errors);
        }
#else
        {
            LEX_STRING q;

            if(thd->convert_string(&q, system_charset_info,
                                   thd->query,
                                   thd->query_length,
                                   thd->charset())) {
                SS_CLEARSQLSTR;
                SDB_DBUG_RETURN(HA_ERR_OUT_OF_MEM);
            }

            query = (char *)SsMemStrdup((char *)q.str);
        }
#endif

        ss_dassert(query != NULL);
        SS_SETSQLSTR((char *) query);

        con = get_solid_connection(thd, name);

        CHK_CONN(con);
        tbcon = con->sc_tbcon;
        cd    = con->sc_cd;
        trans = con->sc_trans;

#if MYSQL_VERSION_ID < 50100
        solid_relname(&en, table->s->path);
#else
        solid_relname(&en, name);
#endif

        current_table_name_for_error = en.en_name;

        if (is_alter_table(thd)) {
            tb_minisql_get_table_name(cd, query, solid_my_caseup_str_if_lower_case_table_names, &old_name);

            current_table_name_for_error = old_name.en_name;
        }

#ifdef SS_MYSQL_AC
            frm_from_disk_to_db(tbcon, cd, trans, &en, name, FALSE);
#endif /* SS_MYSQL_AC */

        ss_pprintf_1(("ha_solid::create: table name %s -> %s.%s\n", name, en.en_schema, en.en_name));

        succp = solid_check_keys(table_arg, thd, current_table_name_for_error);

        if (succp) {
            succp = solid_check_table_charset(create_info, query, current_table_name_for_error);
        }

        if (!succp) {
            rs_entname_done_buf(&en);
            rs_entname_done_buf(&old_name);
            finish_solid_DDL();
            SsMemFree(query);
            SS_CLEARSQLSTR;
            SDB_DBUG_RETURN(HA_ERR_UNKNOWN_CHARSET);
        }

        table_collation = soliddb_collation_init((char*)create_info->default_table_charset->name,
                                                 create_info->default_table_charset->number);

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        n_cols = table_arg->s->fields;

        /*
         * Create table type information (ttype),
         */
        ss_pprintf_1(("ha_solid::create:create table type, sname '%s'\n", name));

        ttype = rs_ttype_create(cd);

        for (i = 0; i < n_cols; i++) {
            Field*      field;
/*            ulong       total_length = 0; */

            field = table_arg->field[i];

            atype = ha_solid_mysql_atype(cd, field);

            rs_atype_setnullallowed(cd, atype, field->null_ptr != NULL);

            /* If this field is auto increment field, create a new sequence for it. */

            if (rs_atype_autoinc(cd, atype)) {
                    char* seq_name;
                    rs_auth_t*  auth = rs_sysi_auth(cd);
                    long seq_no = dbe_db_getnewrelid_log((dbe_db_t*)rs_sysi_db(cd));

                    seq_name = (char *)SsMemAlloc(13+32);

                    sprintf(seq_name,"AUTOINC_%lu_SEQ", seq_no);

                    ss_pprintf_1(("SEQ NAME = %s\n", seq_name));

                    rs_auth_setsystempriv(cd, auth, TRUE);

                    succp = tb_createseqandid(
                                cd,
                                trans,
                                seq_name,
                                (char *)RS_AVAL_SYSNAME,
                                catalog,
                                extrainfo,
                                FALSE,     /* sparse */
                                &cont,
                                &errh,
                                &seq_id);

                    if (errh != NULL) {
                        sql_print_error("%s (%s)", su_err_geterrstr(errh), query);
                        su_err_done(errh);
                        succp = FALSE;
                        errh = NULL;
                    }

                    rs_atype_setautoinc(cd, atype, TRUE, seq_id);

                    ss_pprintf_1(("Field %s (AUTO_INCREMENT_SEQ_ID = %lu)\n", rs_atype_name(cd, atype),
                                  (long) rs_atype_getautoincseqid(cd, atype)));

                    rs_auth_setsystempriv(cd, auth, FALSE);

                    SsMemFree(seq_name);

                    auto_inc_defined = TRUE;
            }

            rs_ttype_setatype(cd, ttype, i, atype);
            rs_atype_free(cd, atype);

            atype = rs_ttype_atype(cd, ttype, i);

            if (soliddb_atype_can_have_collation(cd, atype)) {

                CHARSET_INFO* table_charset = get_charset_by_name((const char*)table_collation->coll_name, MYF(0));

                if (strcmp((char*)((field->charset())->name), table_charset->name)) {
                    column_collation = soliddb_collation_init((char*)((field->charset())->name), (field->charset())->number);
                } else {
                    column_collation = table_collation;
                }

                rs_atype_setcollation(cd, atype, column_collation);
            }

            aname = (char*)SsMemStrdup((char*)(field->field_name));

            solid_my_caseup_str(aname);

            rs_ttype_setaname(cd, ttype, i, aname);

            ss_pprintf_1(("ha_solid::create:create table type:%d, %s, null_ptr %x\n",
                          i, aname, (unsigned long)field->null_ptr));

            SsMemFree(aname);
        }

        succp = tb_schema_find(cd, trans, en.en_schema, catalog);

        if (!succp) {
            ss_pprintf_2(("ha_solid::schema %s not found, create it\n", en.en_schema));

            succp = tb_schema_create(cd, trans, en.en_schema, catalog, NULL, &cont, &errh);

            ss_dassert(cont == NULL);
        }

        sysrelcreate = check_solid_sysrel_create(cd, trans, en.en_name, ttype);

        if (succp && !sysrelcreate) {
            tb_relmode_t  relmode = TB_RELMODE_SYSDEFAULT;

            i = 0;
            primkey = NULL;

            /* Create Primary key */
            while (i < table_arg->s->keys) {

                if (is_mysql_primary_key(&table_arg->key_info[i])) {

                    primkey = ha_mysql_solid_create_unique_init(
                                cd,
                                &table_arg->key_info[i],
                                table_arg->s->keys);

                    if (primkey != NULL) {
                        i++;
                        break;
                    }
                }

                i++;
            }

            /* Check that primary key of the referenced table is not removed */
            if (primkey == NULL && is_alter_table(thd)) {
                rs_relh_t* old_relh;

                old_relh = tb_dd_getrelh(cd, trans, &old_name, NULL, NULL);

                if (old_relh && tb_dd_isforkeyref(cd, trans, old_relh, NULL)) {
                    rs_error_create(&errh, E_FORKEYREFEXIST_S, old_name.en_name);
                    succp = FALSE;
                    SS_MEM_SETUNLINK(old_relh);
                    rs_relh_done(cd, old_relh);
                    goto error_handling;
                } else if (old_relh) {
                    SS_MEM_SETUNLINK(old_relh);
                    rs_relh_done(cd, old_relh);
                }
            }

            /* Parse relation mode if given */
            if (create_info->comment.str) {
                relmode = solid_parse_relmode(&create_info->comment);
            }

            rc = 0;

            /* Create foreign key definitions */
            if (succp && !is_alter_table(thd)) {

                /* Creating a new table. */
                rc = create_foreign_key_definitions(cd, trans, ttype, &en, query,
                                                    &n_forkeys, &forkeys, &errh);
            }

            /* Copy and create foreign key definitions */
            if (succp && is_alter_table(thd)) {

                rc = copy_foreign_key_definitions(cd, trans, ttype, &en, &old_name, query,
                                                  &n_forkeys, &forkeys, &errh);

                /* If foreign keys have been defined for this table check that
                   no index associated to the foreign key is dropped. */
                if (!rc && n_forkeys > 0) {

                    rc = check_foreign_key_indexes(table_arg, cd, ttype, &en, query,
                                                   n_forkeys, forkeys, &errh);
                }
            }
            
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
            if (rc == E_UNRESFKEYS_S && !tb_trans_get_foreign_key_checks(trans)) {
                have_forkeys_unresolved = TRUE;
                rc = 0;
            }
            else
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */            
            if (rc) {
                /* Handle foreign key errors */
                rc = ha_solid_mysql_error(thd, errh, rc);
                goto error_handling;
            }

            /* Create relation */
            succp = tb_createrelation_ext(
                        cd,
                        trans,
                        en.en_name,
                        en.en_schema,   // char*        authid
                        catalog,        // char*        catalog
                        extrainfo,      // char*        extrainfo,
                        ttype,          // rs_ttype_t*  ttype,
                        primkey,        // tb_sqlunique_t* primkey,
                        0,              // uint         unique_c,
                        NULL,           // tb_sqlunique_t* unique,
                        n_forkeys,      // uint         forkey_c,
                        forkeys,        // tb_sqlforkey_t* forkeys,
                        NULL,           // uint*        def,
                        NULL,           // rs_tval_t*   defvalue,
                        0,              // uint         check_c,
                        NULL,           // char**       checks,
                        NULL,           // char**       checknames,
                        TB_DD_CREATEREL_USER,
                        TB_DD_PERSISTENCY_PERMANENT,
                        TB_DD_STORE_DEFAULT,
                        TB_DD_DURABILITY_DEFAULT,
                        relmode,        // optimistic || pessimistic || mainmemory || sysdefaul
                        &relh,
                        &errh);         // rs_err_t**   err

            ha_mysql_solid_create_unique_done(primkey);
            primkey = NULL;

            /* Create secondary keys */
            if (succp) {
                i = 0;

                while (i < table_arg->s->keys) {

                    if (!is_mysql_primary_key(&table_arg->key_info[i])) {
                        rs_key_t* old_key = NULL;

                        /* Check is this key already created while foreign keys were
                           created. */
                        if (n_forkeys) {
                            rs_entname_t en2;
                            char *indexname;

                            /* Here we need to use foreign key names to find a match */
                            indexname = foreignkey_name_from_mysql_to_solid(cd, relh,
                                                                            table_arg->key_info[i].name);

                            rs_entname_initbuf(&en2, NULL, en.en_schema, indexname);

                            old_key = rs_relh_keybyname(cd, relh, &en2);
                            SsMemFree(indexname);
                        }

                        /* Skip key creation if we have already done that */
                        if (!old_key) {

                            succp = ha_solid_createindex(
                                    relh,
                                    &table_arg->key_info[i],
                                    en.en_name,
                                    tbcon,
                                    cd,
                                    trans,
                                    en.en_schema, catalog, extrainfo,
                                    &errh);
                        }
                    }

                    i++;
                }
            }
        }

        /* If the auto increment field has been defined and
           query was ALTER TABLE...AUTO_INCREMENT = x; or
           CREATE TABLE ...AUTO_INCREMENT = x; Set a new value for the
           auto_increment field. */

        if (succp &&
            auto_inc_defined &&
            (create_info->used_fields & HA_CREATE_USED_AUTO) &&
            (create_info->auto_increment_value != 0))
        {
            rs_atype_t* valatype;
            rs_aval_t*  aval;
            ss_int8_t   i8;
            rs_auth_t*  auth = rs_sysi_auth(cd);

            do {
                succp = tb_trans_commit(cd, trans, &finished, &errh);
            } while (rs_sysi_lockwait(cd) || !finished);

            con = get_solid_connection(thd, name);

            CHK_CONN(con);
            tbcon = con->sc_tbcon;
            cd    = con->sc_cd;

            solid_trans_beginif((handlerton *)ht, thd, TRUE);

            trans = con->sc_trans;

            /* Set startup value for the auto_increment sequence */
            if (succp) {
                //dbe_ret_t rc;
                rs_auth_setsystempriv(cd, auth, TRUE);
                valatype = rs_atype_initbigint(cd);
                aval = rs_aval_create(cd, valatype);
                SsInt8SetNativeUint8(&i8, (create_info->auto_increment_value - 1));
                rs_aval_setint8_ext(cd, valatype, aval, i8, NULL);

                finished = FALSE;

                do {
                    succp = tb_seq_set(cd, trans, seq_id, FALSE, valatype, aval,
                                   &finished, &errh);
                } while (rs_sysi_lockwait(cd) || !finished);

                ss_pprintf(("AUTO INC SEQ initial value = %lu\n",
                            (long) create_info->auto_increment_value));

                rs_auth_setsystempriv(cd, auth, FALSE);

                rs_aval_free(cd, valatype, aval);
                rs_atype_free(cd, valatype);

            }

            rs_relh_setautoincrement_seqid(cd, relh, seq_id);
        }

        /* If everything went ok to this point update foreign keys referencing to this table if this
           table is a parent table to some child table */

        if (succp && is_alter_table(thd)) {
            rc = update_child_foreign_keys(thd, cd, trans, relh, ttype, &old_name, &en, &errh);

            if (rc) {
                rc = ha_solid_mysql_error(thd, errh, rc);
                succp = FALSE;
            }
        }
        
#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        /*
         * We have to check if there are some unresolved forkeys referencing this relh.
         * If yes, we have to create those forkeys for those perticular relations.
         */
        if (!rc && relh) {
            rc = solid_create_unresolved_foreign_keys(cd, trans, rs_relh_entname(cd, relh));
            if (rc) {
                succp = FALSE;
            }
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        

error_handling:

#ifdef FOREIGN_KEY_CHECKS_SUPPORTED
        /*
         * We have to update info in unresolved forkeys system table
         * with real relh id
         */
        if (have_forkeys_unresolved && succp) {
            forkeys_unresolved_ids = (long*)SsMemAlloc(n_forkeys * sizeof(long));
            n_forkeys_unres_ids = n_forkeys;
            
            tb_dd_create_fkeys_unresolved_info(cd, trans, relh,
                                               n_forkeys, forkeys, forkeys_unresolved_ids);
            
            SsMemFree(forkeys_unresolved_ids);
        }
#endif /* FOREIGN_KEY_CHECKS_SUPPORTED */        

        if (n_forkeys) {
            ha_mysql_solid_foreign_done(forkeys, n_forkeys);
            forkeys = NULL;
            n_forkeys = 0;
        }

        ss_dassert(forkeys == NULL);

        ha_mysql_solid_create_unique_done(primkey);
        rs_ttype_free(cd, ttype);
        rs_entname_done_buf(&en);
        rs_entname_done_buf(&old_name);

        if (relh != NULL) {
            SS_MEM_SETUNLINK(relh);
            rs_relh_done(cd, relh);
        }
        
        commit_succ_p = internal_solid_end_trans_and_stmt(cd, trans, succp, &commit_errh);

        if (!commit_succ_p) {
            int commit_rc = 0;

            commit_rc = handle_error(commit_errh);

            if (!rc) {
                rc = commit_rc;
            }
        }

        if (rc != 0) {
            if (errh) {
                sql_print_error("%s (%s)", su_err_geterrstr(errh), query);
                su_err_done(errh);
                errh = NULL;
            }

            if (forkeys) {
                ha_mysql_solid_foreign_done(forkeys, n_forkeys);
            }
        } else {
            if (errh != NULL) {
                sql_print_error("%s (%s)", su_err_geterrstr(errh), query);
            }

            if (!succp) {
                rc = ha_solid_mysql_error(thd, errh, rc);
                su_err_done(errh);
            } else {
                rc = 0;
            }
        }

        SsMemFree(query);
        finish_solid_DDL();
        SS_CLEARSQLSTR;
        SDB_DBUG_RETURN(rc);
}

/*#***********************************************************************\
*
*              ::start_bulk_insert
*
* Parameters :
*
* Return value : -
*
* Globals used : current_thd
*/

void ha_soliddb::start_bulk_insert(
        ha_rows rows)
{
        SDB_DBUG_ENTER("ha_solid::start_bulk_insert");

        MYSQL_THD thd = current_thd;

        if (operation_requires_xtable_lock(thd)) {
            rs_entname_t old_name;
            SOLID_CONN *con = get_solid_connection(thd);

            /* getting name of the table being altered */

            rs_entname_initbuf(&old_name, NULL, NULL, NULL);

            tb_minisql_get_table_name(con->sc_cd, (char *)soliddb_query(thd),
                                      solid_my_caseup_str_if_lower_case_table_names, &old_name);

            if (is_soliddb_table(old_name.en_schema, old_name.en_name)) {

                bool succp = false;
                su_err_t *errh = NULL;

                solid_bool finished = false;

                do {
                    succp = tb_trans_commit(con->sc_cd, con->sc_trans, &finished, &errh);
                } while (rs_sysi_lockwait(con->sc_cd) || !finished);

                handle_error(errh);

                tb_trans_settransopt_once(con->sc_cd, con->sc_trans, TB_TRANSOPT_ISOLATION_READCOMMITTED, &errh);
                handle_error(errh);

                solid_trans_beginif((handlerton *)ht, thd, TRUE);

                errh = ::lock_table(con, &old_name, TRUE, DDL_LOCK_TIMEOUT);
                succp = errh == NULL;
                handle_error(errh);
            }

            rs_entname_done_buf(&old_name);
        }

        SDB_DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              ::check
 *
 * Check table implementation for solidDB
 *
 * Parameters :
 *
 *      THD*           thd, in, use
 *      HA_CHECK_OPT*  check_opt, in, not used
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::check(
        MYSQL_THD thd,
        HA_CHECK_OPT* check_opt)
{
        SOLID_CONN* con = NULL;
        dbe_db_t*   db  = NULL;
        rs_sysi_t*  cd  = NULL;
        bool        error = FALSE;

        DBUG_ENTER("ha_soliddb::check");

        ss_dassert(thd != NULL);
        ss_dassert(check_opt != NULL);

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(this->ht, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);
        cd = con->sc_cd;
        ss_dassert(cd != NULL);

        db = (dbe_db_t*)rs_sysi_db(cd);
        ss_dassert(db != NULL);

        /* TODO: These should be done:

           Check that data blocks for this table are sound
           Check that all indexes contain right number of rows
           Check that data blocks for all indexes are sound
        */

        if (error) {
            DBUG_RETURN(HA_ADMIN_CORRUPT);
        } else {
            DBUG_RETURN(HA_ADMIN_OK);
        }
}

/*#***********************************************************************\
 *
 *              soliddb_check_filespec
 *
 * Check syntax on a new database file specification
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      set_var*          var, in, use
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int soliddb_check_filespec(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        const void*                     save,           /*!< in: immediate result
                                                        from check function */
        struct st_mysql_value*          value)          /*!< out: where the
                                                        formal string goes */
{
        char *filename = NULL;
        char* mismatch = NULL;
        char *p = NULL;
        ss_int8_t size;
        ss_int8_t coeff;
        bool b = TRUE;

#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        length= sizeof(buff);
        filename= (char *) SsMemStrdup((char *)value->val_str(value, buff, &length));
#else
        filename = (char*) SsMemStrdup(var->value->name);
#endif

        DBUG_ENTER("soliddb_check_filespec");
        ss_dassert(thd != NULL);
        ss_dassert(var != NULL);
        p = filename;

        /* Find end of filename */
        while(*p != '\0' && !ss_isspace(*p) && *p != ',') {
            p++;
        }

        if (*p == '\0') {
            sql_print_error("solidDB: Error: Parse error on filespec %s", filename);
            SsMemFree(filename);
            DBUG_RETURN(-1);
        }

        *p = '\0';

        /* Find maxsize */
        p++;

        while(*p != '\0' && !ss_isdigit(*p)) {
            p++;
        }

        b = SsStrScanInt8(p, &size, &mismatch);

        if (!b) {
            sql_print_error("solidDB: Error: Illegal or missing argument filesize %s",
                    filename);
            SsMemFree(filename);
            DBUG_RETURN(-1);
        }

        p = mismatch;

        /* Set up database file size */
        switch (*p) {
            case 'k':
            case 'K':
                SsInt8SetUint4(&coeff, 1024U);
                break;
            case 'm':
            case 'M':
                SsInt8SetUint4(&coeff, (ss_uint4_t)1024U * 1024U);
                break;
            case 'g':
            case 'G':
                SsInt8SetUint4(&coeff,
                               (ss_uint4_t)1024U * 1024U * 1024U);
                break;
            default:
                SsInt8SetUint4(&coeff, 1U);
                break;
        }

        b = SsInt8MultiplyByInt8(&size, size, coeff);
        SsInt8SetUint4(&coeff, 102400L); /* FILESPEC_MIN_SIZE */

        if (!b || SsInt8Cmp(size,coeff) < 0) {
            sql_print_error("solidDB: Error: Illegal value for file size.");
            SsMemFree(filename);
            DBUG_RETURN(-1);
        }

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              soliddb_add_filespec
 *
 * Add a new database file to solidDB filespec
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      set_var*          var, in, use
 *
 * Return value : bool
 *
 * Globals used :
 */
void soliddb_add_filespec(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           save,           /*!< out: where the
                                                        formal string goes */
        const void*                     value)           /*!< in: immediate result
                                                        from check function */

{
        DBUG_ENTER("soliddb_add_filespec");

        char *filename = NULL;
        char* mismatch = NULL;
        char *p = NULL;
        ss_int8_t size;
        ss_int8_t coeff;
        bool b = TRUE;
        su_ret_t rc;
        dbe_db_t* db;
        SOLID_CONN* con;
        bool failure = FALSE;
#if MYSQL_VERSION_ID >= 50100
        char buff[STRING_BUFFER_USUAL_SIZE];
        int length;

        length= sizeof(buff);
        filename= (char *) SsMemStrdup((char *)value); 
#else
        filename = (char *) SsMemStrdup(var->value->name);
#endif

        ss_dassert(thd != NULL);
        ss_dassert(var != NULL);

#if MYSQL_VERSION_ID >= 50100
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);

        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);
        p = filename;

        /* Find end of filename */
        while(*p != '\0' && !ss_isspace(*p) && *p != ',') {
            p++;
        }

        if (*p == '\0') {
            sql_print_error("solidDB: Error: Parse error on filespec %s", filename); 
            SsMemFree(filename);
            failure = TRUE;
            goto err;
        }

        *p = '\0';

        /* Find maxsize */
        p++;

        while(*p != '\0' && !ss_isdigit(*p)) {
            p++;
        }

        b = SsStrScanInt8(p, &size, &mismatch);

        if (!b) {
            sql_print_error("solidDB: Error: Illegal or missing argument from filespec %s",
                    filename);
            SsMemFree(filename);
            failure = TRUE;
            goto err;
        }

        p = mismatch;

        /* Set up database file size */
        switch (*p) {
            case 'k':
            case 'K':
                SsInt8SetUint4(&coeff, 1024U);
                break;
            case 'm':
            case 'M':
                SsInt8SetUint4(&coeff, (ss_uint4_t)1024U * 1024U);
                break;
            case 'g':
            case 'G':
                SsInt8SetUint4(&coeff,
                               (ss_uint4_t)1024U * 1024U * 1024U);
                break;
            default:
                SsInt8SetUint4(&coeff, 1U);
                break;
        }

        /* Check size */
        b = SsInt8MultiplyByInt8(&size, size, coeff);
        SsInt8SetUint4(&coeff, 102400L); /* FILESPEC_MIN_SIZE */

        if (!b || SsInt8Cmp(size,coeff) < 0) {
            sql_print_error("solidDB: Error: Illegal value for file size.");
            SsMemFree(filename);
            failure = TRUE;
            goto err;
        }

        /* Add new filespec to dbfile */
        rc = dbe_db_addnewfilespec(db, filename, size, 0);

        if (rc != SU_SUCCESS) {
            char *errmsg;

            sql_print_error("solidDB: Error: Adding a new database file failed.");
            errmsg = su_rc_givetext_noargs(rc);
            sql_print_error("%s", errmsg);
            SsMemFree(errmsg);
            SsMemFree(filename);
            failure = TRUE;
            goto err;
        } else {

            /* If we have entered read only mode set readonly to false */
            if (dbe_db_isreadonly(db)) {
                dbe_db_setreadonly(db, FALSE);
            }

            SsMemFree(filename);
        }

 err:

#if MYSQL_VERSION_ID >= 50100
        DBUG_VOID_RETURN;
#else
        DBUG_RETURN(failure);
#endif

}

/*#***********************************************************************\
 *
 *              soliddb_update_durability_level
 *
 * Update durability level of transactions
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      enum_var_type     type, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
void soliddb_update_durability_level(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           value,          /*!< out: where the
                                                        formal string goes */
        const void*                     save)           /*!< in: immediate result 
                                                        from check function */
{
        SOLID_CONN* con = NULL;
        dbe_db_t* db = NULL;
        su_inifile_t* inifile = NULL;

        DBUG_ENTER("soliddb_update_durability_level");

#if MYSQL_VERSION_ID >= 50100
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
        soliddb_durability_level = *(ulong *)value;
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);
        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        if (soliddb_durability_level < 1 || soliddb_durability_level > 3) {
            DBUG_VOID_RETURN; /* Ignore if error */
        }

        dbe_db_setdurabilitylevel(db, (dbe_durability_t) soliddb_durability_level);
        inifile = dbe_db_getinifile(db);

        if (inifile) {
            solid_set_long(inifile, soliddb_durability_level,
                           SU_DBE_LOGSECTION, SU_DBE_DURABILITYLEVEL);
        }

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              soliddb_update_checkpoint_time
 *
 * Update checkpoint time interval
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      enum_var_type     type, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
void soliddb_update_checkpoint_time(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           value,          /*!< out: where the
                                                        formal string goes */
        const void*                     save)           /*!< in: immediate result
                                                        from check function */
{
        SOLID_CONN* con = NULL;
        dbe_db_t* db = NULL;
        su_inifile_t* inifile = NULL;

        DBUG_ENTER("soliddb_update_checkpoint_time");

#if MYSQL_VERSION_ID >= 50100
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
        soliddb_checkpoint_time = *(longlong *)value;
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);
        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        dbe_db_setcpmintime(db, soliddb_checkpoint_time);

        inifile = dbe_db_getinifile(db);

        if (inifile) {
            solid_set_long(inifile, soliddb_checkpoint_time,
                           SU_DBE_GENERALSECTION, SU_DBE_CPMINTIME);
        }

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              soliddb_update_lock_wait_timeout
 *
 * Update lock wait timeout interval
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      enum_var_type     type, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
void soliddb_update_lock_wait_timeout(
        THD*                            thd,            /*!< in: thread handle */
        struct st_mysql_sys_var*        var,            /*!< in: pointer to
                                                        system variable */
        void*                           value,          /*!< out: where the
                                                        formal string goes */
        const void*                     save)           /*!< in: immediate result
                                                        from check function */
{
        SOLID_CONN* con = NULL;
        dbe_db_t* db = NULL;
        su_inifile_t* inifile = NULL;

        DBUG_ENTER("soliddb_update_lock_wait_timeout");

#if MYSQL_VERSION_ID >= 50100
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
        soliddb_lock_wait_timeout = *(longlong*)value;
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);
        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        dbe_db_setlocktimeout(db, (long *)&soliddb_lock_wait_timeout, (long *)&soliddb_lock_wait_timeout);

        inifile = dbe_db_getinifile(db);

        if (inifile) {
            solid_set_long(inifile, soliddb_lock_wait_timeout,
                           SU_DBE_GENERALSECTION, SU_DBE_PESSIMISTIC_LOCK_TO);

            solid_set_long(inifile, soliddb_lock_wait_timeout,
                           SU_DBE_GENERALSECTION, SU_DBE_OPTIMISTIC_LOCK_TO);
        }

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              soliddb_update_checkpoint_interval
 *
 * Update checkpoint interval i.e. how many log writes before a checkpoint
 *
 * Parameters :
 *
 *      THD*              thd, in, use
 *      enum_var_type     type, in, use
 *
 * Return value : -
 *
 * Globals used :
 */
void soliddb_update_checkpoint_interval(
        THD*                            thd,    /*!< in: thread handle */
        struct st_mysql_sys_var*        var,    /*!< in: pointer to
                                                system variable */
        void*                           value,  /*!< out: where the
                                                formal string goes */
        const void*                     save)   /*!< in: immediate result
                                                from check function */
{
        SOLID_CONN* con = NULL;
        dbe_db_t* db = NULL;
        su_inifile_t* inifile = NULL;

        DBUG_ENTER("soliddb_update_checkpoint_interval");

#if MYSQL_VERSION_ID >= 50100
        /* TODO: Fix below when system variables get handlerton */
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, thd);
        soliddb_checkpoint_interval = *(longlong *)value;
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);

        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        dbe_db_setcpinterval(db, soliddb_checkpoint_interval);

        inifile = dbe_db_getinifile(db);

        if (inifile) {
            solid_set_longlong(inifile, soliddb_checkpoint_interval,
                               SU_DBE_GENERALSECTION, SU_DBE_CPINTERVAL);
        }

        DBUG_VOID_RETURN;
}

/*#***********************************************************************\
 *
 *              soliddb_show_mutex_status
 *
 * Implementation for show soliddb mutex status and show engine soliddb mutex
 * commands
 *
 * Parameters :
 *
 *      handlerton*       hton,in, use, handlerton (used on 5.1.x)
 *      THD*              thd, in, use, MySQL query thread
 *      stat_print_fn*    in, use, print function  (used on 5.1.x)
 *
 * Return value : bool
 *
 * Globals used :
 */
bool soliddb_show_mutex_status(
#if MYSQL_VERSION_ID >= 50100
        handlerton *hton,
        MYSQL_THD thd,
        stat_print_fn *stat_print
#else
        MYSQL_THD thd
#endif
    )
{
#if MYSQL_VERSION_ID >= 50100
        uint hton_name_len = strlen(solid_hton_name);
        uint size1;
        uint size2;
        char buff[IO_SIZE];
        char buff2[IO_SIZE];
#endif

#ifdef SS_PROFILE
        SsSemDbgT** sd_list;
        SsSemDbgT* sd;
#endif

#if MYSQL_VERSION_ID < 50100
        Protocol* protocol = (Protocol *)thd->protocol;
        List<Item> field_list;
#endif

        DBUG_ENTER("solid_show_mutex_status");

#ifndef SS_PROFILE

#if MYSQL_VERSION_ID < 50100
        field_list.push_back(new Item_empty_string("Mutex status", FN_REFLEN));
        protocol->send_fields(&field_list,
                              Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
        protocol->prepare_for_resend();
        protocol->store("Mutex status not available for solidDB storage engine", system_charset_info);
        protocol->write();
        send_eof(thd);
#else /* MYSQL_VERSION_ID >= 50100 i.e. MySQL 5.1.x */
        size1 = my_snprintf(buff, sizeof(buff), "%s",
                            "Mutex status not available for solidDB storage engine");
        stat_print(thd, solid_hton_name, hton_name_len, buff, size1, NULL, 0);
#endif	/* MYSQL_VERSION_ID < 50100 */

        DBUG_RETURN(FALSE);

#else	/* SS_PROFILE defined e.g. SHOW ENGINE SOLIDDB MUTEX */

        sd_list = SsGetSemList();

        if (sd_list != NULL) {
            int i;

#if MYSQL_VERSION_ID < 50100
            field_list.push_back(new Item_empty_string("File", FN_REFLEN));
            field_list.push_back(new Item_uint("Line", 21));
            field_list.push_back(new Item_uint("CallCnt", 21));
            field_list.push_back(new Item_uint("WaitCnt", 21));
            field_list.push_back(new Item_uint("WaitPrc", 21));
            field_list.push_back(new Item_empty_string("Gate", FN_REFLEN));
            field_list.push_back(new Item_empty_string("Note", FN_REFLEN));

            if (protocol->send_fields(&field_list,
                                      Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
                send_eof(thd);
                DBUG_RETURN(FALSE);
            }

#endif	/* MYSQL_VERSION_ID < 50100 */

            for (i = 0; i < SS_SEMNUM_LAST; i++) {
                sd = sd_list[i];

                if (sd != NULL) {
                    ulong waitprc;

                    waitprc = sd->sd_callcnt == 0
                                ? 0L
                                : (ulonglong)(sd->sd_waitcnt * 100.0 / sd->sd_callcnt);

#if MYSQL_VERSION_ID < 50100
                    protocol->prepare_for_resend();
                    protocol->store(sd->sd_file, system_charset_info);
                    protocol->store((ulonglong)sd->sd_line);
                    protocol->store((ulonglong)sd->sd_callcnt);
                    protocol->store((ulonglong)sd->sd_waitcnt);
                    protocol->store((ulonglong)waitprc);
                    protocol->store(sd->sd_gatep ? "gate" : "", system_charset_info);
                    protocol->store(waitprc >= 10 ? "***" : "", system_charset_info);

                    if (protocol->write()) {
                        send_eof(thd);
                        DBUG_RETURN(FALSE);
                    }
#else /* MYSQL_VERSION_ID >= 50100 i.e. MySQL 5.1.x */

                    size1 = my_snprintf(buff, sizeof(buff), "%s:%d",
                                        soliddb_basename(sd->sd_file), sd->sd_line);

                    size2 = my_snprintf(buff2, sizeof(buff2),
                                        "callcnt=%lu, waitcnt=%lu, "
                                        "waitprc=%lu, gate=%s, note=%s",
                                        sd->sd_callcnt, sd->sd_waitcnt,
                                        waitprc, sd->sd_gatep ? "gate" : "",
                                        waitprc >= 10 ? "***" : "");

                    if (stat_print(thd, solid_hton_name, hton_name_len, buff, size1,
                                   buff2, size2)) {
                        DBUG_RETURN(FALSE);
                    }
#endif	/* MYSQL_VERSION_ID < 50100 */
		}
            }
	}

#if MYSQL_VERSION_ID < 50100
        send_eof(thd);
#endif	/* MYSQL_VERSION_ID < 50100 */

        DBUG_RETURN(FALSE);
#endif /* SS_PROFILE */
}

/*#***********************************************************************\
 *
 *              soliddb_show_status
 *
 * Implementation for show soliddb status and show engine soliddb commands
 *
 * Parameters :
 *
 *      handlerton*       hton, in, use, handlerton (used on 5.1.x)
 *      THD*              thd, in, use, MySQL query thread
 *      stat_print_fn*    in, use, print function   (used on 5.1.x)
 *      enum ha_stat_type stat_type, in, use        (used on 5.1.x)
 *
 * Return value : bool
 *
 * Globals used :
 */
bool soliddb_show_status(
#if MYSQL_VERSION_ID >= 50100
        handlerton *hton,
        MYSQL_THD thd,
        stat_print_fn *stat_print,
        enum ha_stat_type stat_type
#else
        MYSQL_THD thd
#endif
        )
{
        const long MAX_STATUS_SIZE = 64000;
        char* status_str = NULL;
        char* buf = NULL;
        char buf2[256];
        long real_length = 0;
        dbe_dbstat_t dbs;
        SsQmemStatT qms;
        double hitrate = 0.0;
        dbe_db_t* db;
        SOLID_CONN* con;
#if MYSQL_VERSION_ID < 50100
        Protocol* protocol = (Protocol *)thd->protocol;
#endif

        DBUG_ENTER("solid_show_status");

#if MYSQL_VERSION_ID < 50100
        if (have_soliddb != SHOW_OPTION_YES) {
            DBUG_RETURN(FALSE);
        }
#endif

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(hton, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);

        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        status_str = (char *)SsMemAlloc(MAX_STATUS_SIZE);
        buf = (char *) SsMemAlloc(MAX_STATUS_SIZE);

        dbe_db_getstat(db, &dbs);
        dbe_db_getmeminfo(db, &qms);

        SsSprintf(status_str, "Memory size:\n");

#ifdef SS_NT64
        SsSprintf(buf, "    %I64lu kilobytes\n",
                  (qms.qms_sysbytecount + qms.qms_slotbytecount) / 1024);
#else
        SsSprintf(buf,   "    %lu kilobytes\n",
                  (qms.qms_sysbytecount + qms.qms_slotbytecount) / 1024);
#endif

        strcat(status_str, buf);
        SsSprintf(buf, "Transaction count statistics:\n");
        strcat(status_str, buf);
        SsSprintf(buf, "    Commit Abort Rollback    Total Read-only  Trxbuf  Active Validate\n");
        strcat(status_str, buf);

#ifdef SS_NT64
        SsSprintf(buf,   "    %6I64lu %5I64lu %8I64lu %8I64lu %9I64lu %7I64lu %7I64lu %8I64lu\n",
                  dbs.dbst_trx_commitcnt, dbs.dbst_trx_abortcnt, dbs.dbst_trx_rollbackcnt,
                  dbs.dbst_trx_commitcnt + dbs.dbst_trx_abortcnt + dbs.dbst_trx_rollbackcnt,
                  dbs.dbst_trx_readonlycnt,
                  dbs.dbst_trx_bufcnt, dbs.dbst_trx_activecnt, dbs.dbst_trx_validatecnt);
#else
        SsSprintf(buf,   "    %6lu %5lu %8lu %8lu %9lu %7lu %7lu %8lu\n",
                  dbs.dbst_trx_commitcnt, dbs.dbst_trx_abortcnt, dbs.dbst_trx_rollbackcnt,
                  dbs.dbst_trx_commitcnt + dbs.dbst_trx_abortcnt + dbs.dbst_trx_rollbackcnt,
                  dbs.dbst_trx_readonlycnt,
                  dbs.dbst_trx_bufcnt, dbs.dbst_trx_activecnt, dbs.dbst_trx_validatecnt);
#endif

        strcat(status_str, buf);

        if (dbs.dbst_cac_findcnt == 0) {
            hitrate = 0.0;
        } else {
            hitrate = (double)100.0 * (dbs.dbst_cac_findcnt - dbs.dbst_cac_readcnt) /
                dbs.dbst_cac_findcnt;
        }

#ifdef SS_WIN
        SsDoubleToAscii(hitrate, buf2, 5);
#else
        SsSprintf(buf2, "%.1lf", hitrate);
#endif

        SsSprintf(buf, "Cache count statistics:\n");
        strcat(status_str, buf);
        SsSprintf(buf, "    Hit rate      Find      Read     Write\n");
        strcat(status_str, buf);
        SsSprintf(buf,   "    %-13s %-9lu %-9lu %-9lu\n",
                  buf2, dbs.dbst_cac_findcnt, dbs.dbst_cac_readcnt, dbs.dbst_cac_writecnt);
        strcat(status_str, buf);
        SsSprintf(buf, "Database statistics:\n");
        strcat(status_str, buf);

        if (dbs.dbst_ind_mergeact) {
            SsSprintf(buf, "    Index merge is active.\n");
            strcat(status_str, buf);
        }

        SsSprintf(buf,   "    Index writes    %9lu  After last merge %6lu\n",
                  dbs.dbst_ind_writecnt, dbs.dbst_ind_writecntsincemerge);
        strcat(status_str, buf);

        SsSprintf(buf,   "    Log writes      %9lu  After last cp    %6lu\n",
                  dbs.dbst_log_writecnt, dbs.dbst_log_writecntsincecp);
        strcat(status_str, buf);

        SsSprintf(buf,   "    Active searches %9lu  Average          %6lu\n",
                  dbs.dbst_sea_activecnt, dbs.dbst_sea_activeavg);
        strcat(status_str, buf);

        SsSprintf(buf,   "    Database size %10lu kilobytes\n", dbs.dbst_ind_filesize);
        strcat(status_str, buf);

        SsSprintf(buf,   "    Log size      %10lu kilobytes\n", dbs.dbst_log_filesize);
        strcat(status_str, buf);
        SsMemFree(buf);

#if MYSQL_VERSION_ID > 50100
        switch (stat_type) {
            case HA_ENGINE_STATUS: {
                   real_length = strlen(status_str) + 1;

                   stat_print(thd, solid_hton_name, strlen(solid_hton_name),
                       STRING_WITH_LEN(""), status_str, real_length);

                   break;
            }
            case HA_ENGINE_MUTEX: 
		   return soliddb_show_mutex_status(hton, thd, stat_print);

		   break;

            default: ss_error; break;
        }

        SsMemFree(status_str);
#else
    {
        List<Item> field_list;

        real_length = strlen(status_str) + 1;
        field_list.push_back(new Item_empty_string("Status", real_length));

        if (protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                               Protocol::SEND_EOF)) {
            SsMemFree(status_str);
            DBUG_RETURN(TRUE);
        }

        protocol->prepare_for_resend();
        protocol->store(status_str, real_length, system_charset_info);
        SsMemFree(status_str);

        if (protocol->write()) {
            DBUG_RETURN(TRUE);
        }

        send_eof(thd);
    }
#endif

    DBUG_RETURN(FALSE);
}

/*#***********************************************************************\
 *
 *              soliddb_export_status
 *
 * Export solidDB status variables to MySQL
 *
 * Parameters : -
 *
 * Return value : -
 *
 * Globals used : current_thd
 */
void soliddb_export_status(void)
{
        dbe_dbstat_t dbs;
        ss_pmon_t pmon;
        SsQmemStatT qms;
        dbe_db_t* db = NULL;
        double hitrate = 0.0;
        SOLID_CONN* con = NULL;
        tb_database_t* tdb;

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection((handlerton *)legacy_soliddb_hton, current_thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, current_thd);
#endif

        CHK_CONN(con);

        db = (dbe_db_t*)rs_sysi_db(con->sc_cd);
        ss_dassert(db != NULL);

        dbe_db_getstat(db, &dbs);
        dbe_db_getmeminfo(db, &qms);

        solid_export_vars.soliddb_memory_size = ((qms.qms_sysbytecount + qms.qms_slotbytecount) / 1024);
        solid_export_vars.soliddb_transaction_commits = dbs.dbst_trx_commitcnt;
        solid_export_vars.soliddb_transaction_rollbacks = dbs.dbst_trx_abortcnt;
        solid_export_vars.soliddb_transaction_aborts = dbs.dbst_trx_rollbackcnt;
        solid_export_vars.soliddb_transaction_total = dbs.dbst_trx_commitcnt + dbs.dbst_trx_abortcnt + dbs.dbst_trx_rollbackcnt;
        solid_export_vars.soliddb_transaction_readonly = dbs.dbst_trx_readonlycnt;
        solid_export_vars.soliddb_transaction_trxbuf = dbs.dbst_trx_bufcnt;
        solid_export_vars.soliddb_transaction_active = dbs.dbst_trx_activecnt;
        solid_export_vars.soliddb_transaction_validate = dbs.dbst_trx_validatecnt;

        if (dbs.dbst_cac_findcnt == 0) {
            hitrate = 0.0;
        } else {
            hitrate = (double)100.0 * (dbs.dbst_cac_findcnt - dbs.dbst_cac_readcnt) /
                dbs.dbst_cac_findcnt;
        }

        solid_export_vars.soliddb_cache_hitrate = (longlong)hitrate;
        solid_export_vars.soliddb_cache_find = dbs.dbst_cac_findcnt;
        solid_export_vars.soliddb_cache_read = dbs.dbst_cac_readcnt;
        solid_export_vars.soliddb_cache_write = dbs.dbst_cac_writecnt;
        solid_export_vars.soliddb_indexmerge_active = dbs.dbst_ind_mergeact;
        solid_export_vars.soliddb_index_writes = dbs.dbst_ind_writecnt;
        solid_export_vars.soliddb_index_writesincemerge = dbs.dbst_ind_writecntsincemerge;
        solid_export_vars.soliddb_log_writes = dbs.dbst_log_writecnt;
        solid_export_vars.soliddb_log_writessincecp = dbs.dbst_log_writecntsincecp;
        solid_export_vars.soliddb_active = dbs.dbst_sea_activecnt;
        solid_export_vars.soliddb_activeavg = dbs.dbst_sea_activeavg;
        solid_export_vars.soliddb_filesize = dbs.dbst_ind_filesize;
        solid_export_vars.soliddb_logsize = dbs.dbst_log_filesize;
        solid_export_vars.soliddb_cp_active= dbe_db_isbackupactive(db);
        solid_export_vars.soliddb_backup_active= dbe_db_iscpactive(db);
        // gather version information
        {
            static char version[128] = {0};
            // has version been initialized?
            if( *version == 0 ) {
                // initilize version
                // there is the diffirence between enteprise and ordinary version
#ifndef SS_MYSQL_ENTER
                // for ordinary version it looks something like that
                //  5.0.27-solidDB-0060
                snprintf(version, sizeof(version), "%s-%s-%04d",
                        MYSQL_SERVER_VERSION,
                        "solidDB",
                        SS_SERVER_VERSNUM_RELEASE
                        );
#else // #ifndef SS_MYSQL_ENTER
                // for enterprise version it looks something like that
                //  5.0.27-solidDB-certified-0060
                snprintf(version, sizeof(version), "%s-%s-%s-%04d",
                        MYSQL_SERVER_VERSION,
                        "solidDB",
                        SS_MYSQL_ENTER,
                        SS_SERVER_VERSNUM_RELEASE
                       );
#endif // #ifndef SS_MYSQL_ENTER
            } // if( *version == 0 )..
            solid_export_vars.soliddb_version = version;
        } // gather version information..


#ifdef SS_MYSQL_AC
        solid_export_vars.soliddb_hsb_role= dbe_db_gethsbrolestr(db);
        solid_export_vars.soliddb_hsb_state= dbe_db_gethsbstatestr(db);
        solid_export_vars.soliddb_hsb_safeness= hsb_sys_get_safenessstr();
#endif /* SS_MYSQL_AC */

/*
        tdb = (tb_database_t*) rs_sysi_tabdb(con->sc_cd);
        ss_dassert(tdb != NULL);
        tb_database_pmonupdate_nomutex(tdb);
        SsPmonGetData(&pmon);

        solid_export_vars.soliddb_pmon_fileopen = pmon.pm_values[SS_PMON_FILEOPEN];
        solid_export_vars.soliddb_pmon_fileread = pmon.pm_values[SS_PMON_FILEREAD];
        solid_export_vars.soliddb_pmon_filewrite = pmon.pm_values[SS_PMON_FILEWRITE];
        solid_export_vars.soliddb_pmon_fileappend = pmon.pm_values[SS_PMON_FILEAPPEND];
        solid_export_vars.soliddb_pmon_fileflush = pmon.pm_values[SS_PMON_FILEFLUSH];
        solid_export_vars.soliddb_pmon_filelock = pmon.pm_values[SS_PMON_FILELOCK];
        solid_export_vars.soliddb_pmon_cachefind = pmon.pm_values[SS_PMON_CACHEFIND];
        solid_export_vars.soliddb_pmon_cachefileread = pmon.pm_values[SS_PMON_CACHEFILEREAD];
        solid_export_vars.soliddb_pmon_cachefilewrite = pmon.pm_values[SS_PMON_CACHEFILEWRITE];
        solid_export_vars.soliddb_pmon_cacheprefetch = pmon.pm_values[SS_PMON_CACHEPREFETCH];
        solid_export_vars.soliddb_pmon_cacheprefetchwait = pmon.pm_values[SS_PMON_CACHEPREFETCHWAIT];
        solid_export_vars.soliddb_pmon_cachepreflush = pmon.pm_values[SS_PMON_CACHEPREFLUSH];
        solid_export_vars.soliddb_pmon_cachelruwrite = pmon.pm_values[SS_PMON_CACHELRUWRITE];
        solid_export_vars.soliddb_pmon_cacheslotwait = pmon.pm_values[SS_PMON_CACHESLOTWAIT];
        solid_export_vars.soliddb_pmon_sqlprepare = pmon.pm_values[SS_PMON_SQLPREPARE];
        solid_export_vars.soliddb_pmon_sqlexecute = pmon.pm_values[SS_PMON_SQLEXECUTE];
        solid_export_vars.soliddb_pmon_sqlfetch = pmon.pm_values[SS_PMON_SQLFETCH];
        solid_export_vars.soliddb_pmon_dbeinsert = pmon.pm_values[SS_PMON_DBEINSERT];
        solid_export_vars.soliddb_pmon_dbedelete = pmon.pm_values[SS_PMON_DBEDELETE];
        solid_export_vars.soliddb_pmon_dbeupdate = pmon.pm_values[SS_PMON_DBEUPDATE];
        solid_export_vars.soliddb_pmon_dbefetch = pmon.pm_values[SS_PMON_DBEFETCH];
        solid_export_vars.soliddb_pmon_dbefetchuniquefound = pmon.pm_values[SS_PMON_DBEFETCHUNIQUEFOUND];
        solid_export_vars.soliddb_pmon_dbefetchuniquenotfound = pmon.pm_values[SS_PMON_DBEFETCHUNIQUENOTFOUND];
        solid_export_vars.soliddb_pmon_transcommit = pmon.pm_values[SS_PMON_TRANSCOMMIT];
        solid_export_vars.soliddb_pmon_transabort = pmon.pm_values[SS_PMON_TRANSABORT];
        solid_export_vars.soliddb_pmon_transrdonly = pmon.pm_values[SS_PMON_TRANSRDONLY];
        solid_export_vars.soliddb_pmon_transbufcnt = pmon.pm_values[SS_PMON_TRANSBUFCNT];
        solid_export_vars.soliddb_pmon_transbufclean = pmon.pm_values[SS_PMON_TRANSBUFCLEAN];
        solid_export_vars.soliddb_pmon_transbufcleanlevel = pmon.pm_values[SS_PMON_TRANSBUFCLEANLEVEL];
        solid_export_vars.soliddb_pmon_transbufabortlevel = pmon.pm_values[SS_PMON_TRANSBUFABORTLEVEL];
        solid_export_vars.soliddb_pmon_transbufadded = pmon.pm_values[SS_PMON_TRANSBUFADDED];
        solid_export_vars.soliddb_pmon_transbufremoved = pmon.pm_values[SS_PMON_TRANSBUFREMOVED];
        solid_export_vars.soliddb_pmon_transvldcnt = pmon.pm_values[SS_PMON_TRANSVLDCNT];
        solid_export_vars.soliddb_pmon_transactcnt = pmon.pm_values[SS_PMON_TRANSACTCNT];
        solid_export_vars.soliddb_pmon_transreadlevel = pmon.pm_values[SS_PMON_TRANSREADLEVEL];
        solid_export_vars.soliddb_pmon_indwrite = pmon.pm_values[SS_PMON_INDWRITE];
        solid_export_vars.soliddb_pmon_indwritesaftermerge = pmon.pm_values[SS_PMON_INDWRITESAFTERMERGE];
        solid_export_vars.soliddb_pmon_logwrites = pmon.pm_values[SS_PMON_LOGWRITES];
        solid_export_vars.soliddb_pmon_logfilewrite = pmon.pm_values[SS_PMON_LOGFILEWRITE];
        solid_export_vars.soliddb_pmon_logwritesaftercp = pmon.pm_values[SS_PMON_LOGWRITESAFTERCP];
        solid_export_vars.soliddb_pmon_logsize = pmon.pm_values[SS_PMON_LOGSIZE];
        solid_export_vars.soliddb_pmon_srchnactive = pmon.pm_values[SS_PMON_SRCHNACTIVE];
        solid_export_vars.soliddb_pmon_dbsize = pmon.pm_values[SS_PMON_DBSIZE];
        solid_export_vars.soliddb_pmon_dbfreesize = pmon.pm_values[SS_PMON_DBFREESIZE];
        solid_export_vars.soliddb_pmon_memsize = pmon.pm_values[SS_PMON_MEMSIZE];
        solid_export_vars.soliddb_pmon_mergequickstep = pmon.pm_values[SS_PMON_MERGEQUICKSTEP];
        solid_export_vars.soliddb_pmon_mergestep = pmon.pm_values[SS_PMON_MERGESTEP];
        solid_export_vars.soliddb_pmon_mergepurgestep = pmon.pm_values[SS_PMON_MERGEPURGESTEP];
        solid_export_vars.soliddb_pmon_mergeuserstep = pmon.pm_values[SS_PMON_MERGEUSERSTEP];
        solid_export_vars.soliddb_pmon_mergeoper = pmon.pm_values[SS_PMON_MERGEOPER];
        solid_export_vars.soliddb_pmon_mergecleanup = pmon.pm_values[SS_PMON_MERGECLEANUP];
        solid_export_vars.soliddb_pmon_mergeact = pmon.pm_values[SS_PMON_MERGEACT];
        solid_export_vars.soliddb_pmon_mergewrites = pmon.pm_values[SS_PMON_MERGEWRITES];
        solid_export_vars.soliddb_pmon_mergefilewrite = pmon.pm_values[SS_PMON_MERGEFILEWRITE];
        solid_export_vars.soliddb_pmon_mergefileread = pmon.pm_values[SS_PMON_MERGEFILEREAD];
        solid_export_vars.soliddb_pmon_mergelevel = pmon.pm_values[SS_PMON_MERGELEVEL];
        solid_export_vars.soliddb_pmon_backupstep = pmon.pm_values[SS_PMON_BACKUPSTEP];
        solid_export_vars.soliddb_pmon_backupact = pmon.pm_values[SS_PMON_BACKUPACT];
        solid_export_vars.soliddb_pmon_checkpointact = pmon.pm_values[SS_PMON_CHECKPOINTACT];
        solid_export_vars.soliddb_pmon_checkpointcount = pmon.pm_values[SS_PMON_CHECKPOINTCOUNT];
        solid_export_vars.soliddb_pmon_checkpointfilewrite = pmon.pm_values[SS_PMON_CHECKPOINTFILEWRITE];
        solid_export_vars.soliddb_pmon_checkpointfileread = pmon.pm_values[SS_PMON_CHECKPOINTFILEREAD];
        solid_export_vars.soliddb_pmon_estsamplesread = pmon.pm_values[SS_PMON_ESTSAMPLESREAD];
        solid_export_vars.soliddb_pmon_logflushes_logical = pmon.pm_values[SS_PMON_LOGFLUSHES_LOGICAL];
        solid_export_vars.soliddb_pmon_logflushes_physical = pmon.pm_values[SS_PMON_LOGFLUSHES_PHYSICAL];
        solid_export_vars.soliddb_pmon_loggroupcommit_wakeups = pmon.pm_values[SS_PMON_LOGGROUPCOMMIT_WAKEUPS];
        solid_export_vars.soliddb_pmon_logflushes_fullpages = pmon.pm_values[SS_PMON_LOGFLUSHES_FULLPAGES];
        solid_export_vars.soliddb_pmon_logwaitflush = pmon.pm_values[SS_PMON_LOGWAITFLUSH];
        solid_export_vars.soliddb_pmon_logmaxwritequeuerecords = pmon.pm_values[SS_PMON_LOGMAXWRITEQUEUERECORDS];
        solid_export_vars.soliddb_pmon_logmaxwritequeuebytes = pmon.pm_values[SS_PMON_LOGMAXWRITEQUEUEBYTES];
        solid_export_vars.soliddb_pmon_ss_threadcount = pmon.pm_values[SS_PMON_SS_THREADCOUNT];
        solid_export_vars.soliddb_pmon_waitreadlevel_count = pmon.pm_values[SS_PMON_WAITREADLEVEL_COUNT];
        solid_export_vars.soliddb_pmon_dbe_lock_ok = pmon.pm_values[SS_PMON_DBE_LOCK_OK];
        solid_export_vars.soliddb_pmon_dbe_lock_timeout = pmon.pm_values[SS_PMON_DBE_LOCK_TIMEOUT];
        solid_export_vars.soliddb_pmon_dbe_lock_deadlock = pmon.pm_values[SS_PMON_DBE_LOCK_DEADLOCK];
        solid_export_vars.soliddb_pmon_dbe_lock_wait = pmon.pm_values[SS_PMON_DBE_LOCK_WAIT];
        solid_export_vars.soliddb_pmon_mysql_rnd_init = pmon.pm_values[SS_PMON_MYSQL_RND_INIT];
        solid_export_vars.soliddb_pmon_mysql_index_read = pmon.pm_values[SS_PMON_MYSQL_INDEX_READ];
        solid_export_vars.soliddb_pmon_mysql_fetch_next = pmon.pm_values[SS_PMON_MYSQL_FETCH_NEXT];
        solid_export_vars.soliddb_pmon_mysql_cursor_create = pmon.pm_values[SS_PMON_MYSQL_CURSOR_CREATE];
        solid_export_vars.soliddb_pmon_mysql_cursor_reset_full = pmon.pm_values[SS_PMON_MYSQL_CURSOR_RESET_FULL];
        solid_export_vars.soliddb_pmon_mysql_cursor_reset_simple = pmon.pm_values[SS_PMON_MYSQL_CURSOR_RESET_SIMPLE];
        solid_export_vars.soliddb_pmon_mysql_cursor_reset_fetch = pmon.pm_values[SS_PMON_MYSQL_CURSOR_RESET_FETCH];
        solid_export_vars.soliddb_pmon_mysql_cursor_cache_find = pmon.pm_values[SS_PMON_MYSQL_CURSOR_CACHE_FIND];
        solid_export_vars.soliddb_pmon_mysql_cursor_cache_hit = pmon.pm_values[SS_PMON_MYSQL_CURSOR_CACHE_HIT];
        solid_export_vars.soliddb_pmon_mysql_connect = pmon.pm_values[SS_PMON_MYSQL_CONNECT];
        solid_export_vars.soliddb_pmon_mysql_commit = pmon.pm_values[SS_PMON_MYSQL_COMMIT];
        solid_export_vars.soliddb_pmon_mysql_rollback = pmon.pm_values[SS_PMON_MYSQL_ROLLBACK];
*/

}

/*#***********************************************************************\
 *
 *              ::referenced_by_foreign_key()
 *
 * Checks if a table is referenced by a foreign key. The MySQL manual states that
 * a REPLACE is either equivalent to an INSERT, or DELETE(s) + INSERT. Only a
 * delete is then allowed internally to resolve a duplicate key conflict in
 * REPLACE, not an update.
 *
 * Parameters : void
 *
 * Return value : 0 if not referenced, 1 if is referenced
 *
 * Globals used : current_thd
 */
uint ha_soliddb::referenced_by_foreign_key(void)
{
        SOLID_CONN* con;
        MYSQL_THD   thd = current_thd;
        rs_relh_t*  relh = NULL;
        rs_err_t*   errh = NULL;
        solid_bool  referenced = FALSE;

        con = get_solid_ha_data_connection((handlerton *)ht, thd);

        CHK_CONN(con);
        relh = solid_table->st_rsrelh;
        ss_dassert(relh != NULL);

        referenced = tb_dd_isforkeyref(con->sc_cd, con->sc_trans, relh, &errh);

        if (errh) {
            sql_print_error("%s (%s)", su_err_geterrstr(errh), soliddb_query(thd));
            su_err_done(errh);
            errh = NULL;
        }

        if (referenced) {
            return (1);
        } else {
            return (0);
        }
}

/*#***********************************************************************\
 *
 *              get_foreign_key_str
 *
 * Create foreign key definition string
 *
 * Parameters :
 *
 *   SOLID_CONN*   con, in, use, solidDB connection
 *   rs_relh_t*    relh, in, use, solidDB relation
 *   rs_key_t*     key, in, use, solidDB key
 *
 * Return value : char*, foreign key string
 *
 * Globals used : -
 */
static char* get_foreign_key_str(
        SOLID_CONN* con,
        rs_relh_t* relh,
        rs_key_t* key)
{
        rs_ano_t j;
        rs_ano_t k;
        rs_ano_t n_parts;
        rs_relh_t *refrelh;
        su_pa_t* forrefkeys = NULL;
        rs_err_t* errh = NULL;
        rs_key_t* forkey = NULL;
        rs_sysi_t* const cd = con->sc_cd;
        char* orig_keyname = rs_key_name(cd, key);
        char* keyname = NULL;
        rs_relh_t* free_relh;
        char* foreign_str = (char *)my_malloc(MAX_FOREIGN_LEN + 1, MYF(0));
        int print_parts = 0;
        sqlrefact_t update_action = (sqlrefact_t) rs_key_update_action(cd, key);
        sqlrefact_t delete_action = (sqlrefact_t) rs_key_delete_action(cd, key);

        foreign_str[0]='\0';

        n_parts = rs_key_nparts(cd, key);

        for (j = 0; j < n_parts; j++) {
            rs_attrtype_t kptype = rs_keyp_parttype(cd, key, j);

            if (kptype == RSAT_USER_DEFINED ||
                kptype == RSAT_COLLATION_KEY) {
                print_parts++;
            }
        }

        ss_dprintf_1(("ha_soliddb::get_foreign_key_str refrelid = %lu\n",
                      rs_key_refrelid(cd, key)));

        refrelh = tb_dd_getrelhbyid(
                        cd,
                        con->sc_trans,
                        rs_key_refrelid(cd, key),
                        NULL,
                        NULL);

        free_relh = refrelh;

        if (refrelh == NULL) {
            ss_dprintf_1(("ha_soliddb::get_foreign_key_str relh not found\n"));
            return foreign_str;
        }

        tb_dd_resolverefkeys(cd, refrelh);
        forrefkeys = rs_relh_keys(cd, refrelh);
        k = 0;

        forkey = NULL;

        ss_assert(rs_keyp_isconstvalue(cd, key, 0));
        ulong sid = va_getlong(rs_keyp_constvalue(cd, key, 0));

        su_pa_do(forrefkeys, k) {
            rs_key_t* rkey = (rs_key_t*)su_pa_getdata(forrefkeys, k);
            ss_assert(rkey != NULL);

            if (rs_key_type(cd, rkey) == RS_KEY_NORMAL &&
                rs_key_id(cd, rkey) == sid) {
                forkey = rkey;
                break;
            }
        }

        if (rs_key_type(cd, key) != RS_KEY_PRIMKEYCHK) {
            rs_relh_t* t = relh;
            rs_key_t* tk;


            ss_dassert(relh != NULL);
            ss_dassert(refrelh != NULL);
            relh = refrelh;
            refrelh = t;

            ss_dassert(key != NULL);
            ss_dassert(forkey != NULL);

            tk = key;
            key = forkey;
            forkey = tk;
        }

        strcat(foreign_str, "CONSTRAINT `");
        keyname = (char *)SsMemStrdup(orig_keyname);
        keyname_from_solid_to_mysql(orig_keyname, keyname);
        strcat(foreign_str, keyname);
        SsMemFree(keyname);
        strcat(foreign_str, "` FOREIGN KEY ");
        /* Here we should not print foreign key index (key) name */
        strcat(foreign_str, "(");

        ss_dassert(forkey != NULL);

        bool first_part = TRUE;
        rs_ano_t k_part;

        int lparts = 0;
        n_parts = rs_key_nparts(cd, forkey);

        for(k_part = 0; k_part < n_parts && lparts < print_parts; k_part++) {
             rs_keypart_t *key_part;
             rs_ano_t     kp_ano;
             rs_attrtype_t kptype;

             key_part = &forkey->k_parts[k_part];
             kp_ano = key_part->kp_ano;
             kptype = rs_keyp_parttype(cd, forkey, k_part);

             if (kptype == RSAT_USER_DEFINED ||
                 kptype == RSAT_COLLATION_KEY) {

                 if (first_part) {
                     first_part = FALSE;
                 } else {
                     strcat(foreign_str, ", ");
                 }

                 strcat(foreign_str, "`");
                 strcat(foreign_str, rs_ttype_aname(cd, rs_relh_ttype(cd, refrelh), kp_ano));
                 strcat(foreign_str, "`");
                 lparts++;
             }
        }

        strcat(foreign_str, ") REFERENCES ");

        if (refrelh == NULL) {
            foreign_str[0]='\0';
            return foreign_str;
        }

        if (errh) {
            su_err_done(errh);
            errh = NULL;
        }

        strcat(foreign_str, "`");
        strcat(foreign_str, rs_relh_name(cd, relh));
        strcat(foreign_str, "` (");
        first_part = TRUE;

        n_parts = rs_key_nparts(cd, key);

        for(j = 0; j < n_parts && lparts > 0; j++) {
            rs_ano_t key_ano = rs_keyp_ano(cd, key, j);
            rs_attrtype_t kptype = rs_keyp_parttype(cd, key, j);

            if (kptype == RSAT_USER_DEFINED ||
                kptype == RSAT_COLLATION_KEY) {

                if (first_part) {
                    first_part = FALSE;
                } else {
                    strcat(foreign_str, ", ");
                }

                strcat(foreign_str, "`");
                strcat(foreign_str, rs_ttype_aname(cd,
                                                   rs_relh_ttype(cd, relh), key_ano));
                strcat(foreign_str, "`");
                lparts--;
            }
        }

        ss_assert(lparts == 0);

        strcat(foreign_str, ") ON DELETE ");
        ha_map_solid_mysql_fkaction(delete_action, foreign_str);
        strcat(foreign_str, " ON UPDATE ");
        ha_map_solid_mysql_fkaction(update_action, foreign_str);

        SS_MEM_SETUNLINK(free_relh);
        rs_relh_done(cd, free_relh);

        ss_assert(strlen(foreign_str) < MAX_FOREIGN_LEN);
        return foreign_str;
}

/*#***********************************************************************\
 *
 *              ::get_foreign_key_create_info()
 *
 * Return foreign key create information for solidDB tables
 *
 * Parameters : void
 *
 * Return value : char*
 *
 * Globals used : current_thd
 */
char* ha_soliddb::get_foreign_key_create_info(void)
{
        SOLID_CONN* con;
        MYSQL_THD   thd = current_thd;
        char*   foreign_str = NULL;
        rs_relh_t*   relh = NULL;
        su_pa_t* refkeys = NULL;
        rs_key_t* key = NULL;
        uint i = 0;
        bool need_relh_done = FALSE;

        SS_PUSHNAME("ha_soliddb::get_foreign_key_create_info");

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        foreign_str = (char *)my_malloc(MAX_FOREIGN_LEN + 1, MYF(0));
        foreign_str[0]='\0';

        if (solid_table == NULL || solid_table->st_rsrelh == NULL) {
            return (foreign_str);
        }

        CHK_CONN(con);
        ss_dassert(solid_table != NULL);
        relh = solid_table->st_rsrelh;
        ss_dassert(relh != NULL);

        /* Reread relation if relation is aborted */
        if (rs_relh_isaborted(con->sc_cd, relh)) {
            relh = tb_dd_readrelh_norbufupdate(con->sc_cd, con->sc_trans, rs_relh_relid(con->sc_cd, relh));
            ss_dassert(relh != NULL);
            need_relh_done = TRUE;
        }

        refkeys = rs_relh_refkeys(con->sc_cd, relh);

        su_pa_do(refkeys, i) {
            key = (rs_key_t*)su_pa_getdata(refkeys, i);

            ss_assert(key != NULL);

            if (rs_key_type(con->sc_cd, key) == RS_KEY_FORKEYCHK) {
                char *key_str = get_foreign_key_str(con, relh, key);
                strcat(foreign_str, ",\n  ");
                strcat(foreign_str, key_str);
                my_free(key_str);
            }
        }

        if (need_relh_done) {
            rs_relh_done(con->sc_cd, relh);
        }

        SS_POPNAME;

        return(foreign_str);
}

/*#***********************************************************************\
 *
 *              ::free_foreign_key_create_info()
 *
 * Free foreign key create information if given
 *
 * Parameters :
 *
 *     char* str, in, use, foreign key definition
 *
 * Return value : -
 *
 * Globals used : -
 */
void ha_soliddb::free_foreign_key_create_info(
        char* str)
{
        if (str) {
            my_free(str);
        }
}

/*#***********************************************************************\
 *
 *              ::get_foreign_key_list()
 *
 * Return foreign key list for solidDB tables
 *
 * Parameters :
 *
 *      THD*                    thd, in, use
 *      List<FOREIGN_KEY_INFO>* f_key_list
 *
 * Return value : 0 or error code
 *
 * Globals used : -
 */
int ha_soliddb::get_foreign_key_list(
        MYSQL_THD thd,
        List<FOREIGN_KEY_INFO> *f_key_list)
{
        SOLID_CONN* con = NULL;
        rs_relh_t*  relh = NULL;
        su_pa_t*    refkeys = NULL;
        rs_key_t*   key = NULL;
        uint        i = 0;

        DBUG_ENTER("ha_soliddb::get_foreign_key_list()");
        SS_PUSHNAME("ha_soliddb::get_foreign_key_list");

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_connection(thd, table->s->path.str);
#else
        con = get_solid_connection(thd, table->s->path);
#endif

        if (solid_table == NULL || solid_table->st_rsrelh == NULL) {
            DBUG_RETURN(0);
        }

        CHK_CONN(con);
        ss_dassert(solid_table != NULL);
        relh = solid_table->st_rsrelh;
        ss_dassert(relh != NULL);

        refkeys = rs_relh_refkeys(con->sc_cd, relh);

        su_pa_do(refkeys, i) {
            key = (rs_key_t*)su_pa_getdata(refkeys, i);

            if (rs_key_type(con->sc_cd, key) == RS_KEY_FORKEYCHK) {
                rs_ano_t j;
                rs_ano_t n_parts;
                rs_relh_t *refrelh;
                rs_ano_t k_part;
                rs_ano_t k;
                FOREIGN_KEY_INFO f_key_info;
                FOREIGN_KEY_INFO *pf_key_info;
                char buf[256];
                LEX_STRING *name= 0;
                char *cname = NULL;
                su_pa_t* forrefkeys = NULL;

                refrelh = tb_dd_getrelhbyid(
                                con->sc_cd,
                                con->sc_trans,
                                rs_key_refrelid(con->sc_cd, key),
                                NULL,
                                NULL);

                if (refrelh == NULL) {
                    /* Concurrent ddl, skip this table, */
                    continue;
                }

                cname = rs_key_name(con->sc_cd, key);

#if MYSQL_VERSION_ID >= 50100

                f_key_info.foreign_id= thd_make_lex_string(thd, 0, cname,
                                                      (uint) strlen(cname), 1);

                cname = rs_relh_schema(con->sc_cd, relh);

                f_key_info.referenced_db= thd_make_lex_string(thd, 0, cname,
                                                          (uint)strlen(cname), 1);

                cname = rs_relh_name(con->sc_cd, refrelh);

                f_key_info.referenced_table= thd_make_lex_string(thd, 0, cname,
                                                             (uint) strlen(cname), 1);

                /* TODO: Maybe here we should provide referenced key name */
                f_key_info.referenced_key_name= thd_make_lex_string(thd, 0, (char *)"", 1, 1);
#else
                f_key_info.forein_id= make_lex_string(thd, 0, cname,
                                                      (uint) strlen(cname), 1);

                cname = rs_relh_schema(con->sc_cd, relh);

                f_key_info.referenced_db= make_lex_string(thd, 0, cname,
                                                          (uint)strlen(cname), 1);

                cname = rs_relh_name(con->sc_cd, refrelh);

                f_key_info.referenced_table= make_lex_string(thd, 0, cname,
                                                             (uint) strlen(cname), 1);
#endif

                n_parts = rs_key_nparts(con->sc_cd, key);

                for(k_part = 0; k_part < n_parts; k_part++) {
                    rs_keypart_t *key_part;
                    rs_ano_t     kp_ano;
                    rs_attrtype_t kptype;

                    key_part = &key->k_parts[k_part];
                    kp_ano = key_part->kp_ano;

                    kptype = rs_keyp_parttype(con->sc_cd, key, k_part);

                    if (kptype == RSAT_USER_DEFINED ||
                        kptype == RSAT_COLLATION_KEY) {

                        cname = rs_ttype_aname(con->sc_cd, rs_relh_ttype(con->sc_cd, relh), kp_ano);

#if MYSQL_VERSION_ID >= 50100
                        name = thd_make_lex_string(thd, name, cname, (uint) strlen(cname), 1);
#else
                        name = make_lex_string(thd, name, cname, (uint) strlen(cname), 1);
#endif

                        f_key_info.foreign_fields.push_back(name);
                    }
                }

                tb_dd_resolverefkeys(con->sc_cd, refrelh);
                forrefkeys = rs_relh_refkeys(con->sc_cd, refrelh);
                k = 0;

                su_pa_do(forrefkeys, k) {
                    rs_key_t* forkey = NULL;

                    forkey = (rs_key_t*)su_pa_getdata(forrefkeys, k);
                    n_parts = rs_key_nparts(con->sc_cd, forkey);

                    if (rs_key_type(con->sc_cd, forkey) != RS_KEY_FORKEYCHK &&
                        rs_key_refrelid(con->sc_cd, forkey) == rs_relh_relid(con->sc_cd, relh)) {

                        for(j = 0; j < n_parts; j++) {

                            rs_ano_t key_ano;
                            rs_attrtype_t kptype;

                            key_ano = rs_keyp_ano(con->sc_cd, forkey, j);
                            kptype = rs_keyp_parttype(con->sc_cd, forkey, j);

                            if (kptype == RSAT_USER_DEFINED ||
                                kptype == RSAT_COLLATION_KEY) {
                                cname = rs_ttype_aname(con->sc_cd, rs_relh_ttype(con->sc_cd, refrelh), key_ano);
#if MYSQL_VERSION_ID >= 50100
                                name = thd_make_lex_string(thd, name, cname, (uint) strlen(cname), 1);
#else
                                name = make_lex_string(thd, name, cname, (uint) strlen(cname), 1);
#endif

                                f_key_info.referenced_fields.push_back(name);
                            }
                        }
                    }
                }

                SS_MEM_SETUNLINK(refrelh);
                rs_relh_done(con->sc_cd, refrelh);

                buf[0] = '\0';
                strcat(buf, "ON DELETE ");
                ha_map_solid_mysql_fkaction((sqlrefact_t)rs_key_delete_action(con->sc_cd, key), buf);

#if MYSQL_VERSION_ID >= 50100
                f_key_info.update_method = thd_make_lex_string(thd,
                                                           f_key_info.update_method,
                                                           buf, (uint) strlen(buf), 1);
                buf[0] = '\0';
#endif
                strcat(buf, " ON UPDATE ");
                ha_map_solid_mysql_fkaction((sqlrefact_t)rs_key_update_action(con->sc_cd, key), buf);

#if MYSQL_VERSION_ID >= 50100
                f_key_info.delete_method = thd_make_lex_string(thd,
                                                           f_key_info.delete_method,
                                                           buf, (uint) strlen(buf), 1);

                pf_key_info= ((FOREIGN_KEY_INFO *)thd_memdup(thd, (const char *) &f_key_info,
                                                              sizeof(FOREIGN_KEY_INFO)));
#else
                f_key_info.constraint_method = make_lex_string(thd,
                                                               f_key_info.constraint_method,
                                                               buf, (uint) strlen(buf), 1);

                pf_key_info= ((FOREIGN_KEY_INFO *)thd->memdup((const char *) &f_key_info,
                                                              sizeof(FOREIGN_KEY_INFO)));
#endif


                f_key_list->push_back(pf_key_info);
            }
        }

        SS_POPNAME;

        DBUG_RETURN(0);
}

/*#***********************************************************************\
*
*              solid_split_myrelname
*
* Splits MySQL full table name in form ./<db-name>/<table-name> in two parts:
* database name and table name.
*
* Parameters :
*
*     char*  mysqlname, in, use, MySQL table name
*     char** schema, inout, database name
*     char** relname, inout, table name
*
* Return value : -
*
* Globals used : -
*/

void solid_split_myrelname(
        const char* mysqlname,
        char** schema,
        char** relname )
{
    const char* p;
    const char* q;

    ss_dassert(mysqlname != NULL);

    *schema = *relname = NULL;
    if (mysqlname == NULL) {
        return;
    }

    for (p = mysqlname; *p; ++p) {
        if (*p != '.' && *p != '/' && *p != '\\') {
            break;
        }
    }

    for (q = p; *q; ++q) {
        if (*q == '/' || *q == '\\') {
            break;
        }
    }

    *schema = (char*)SsMemAlloc(q - p + 1);
    SsTcsncpy(*schema, p, q - p);
    (*schema)[q - p] = '\0';

    for (; *q; ++q) {
        if (*q != '/' && *q != '\\') {
            break;
        }
    }

    *relname = (char*)SsMemAlloc(SsTcslen(q) + 1);

    SsTcscpy(*relname, q);
}

void solid_relname( rs_entname_t *solrel, const char* mysqlname)
{
    rs_entname_initbuf(solrel, NULL, NULL, NULL);
    solid_split_myrelname(mysqlname, &solrel->en_schema, &solrel->en_name);
    if (lower_case_table_names) {
        my_caseup_str( system_charset_info, solrel->en_schema );
        my_caseup_str( system_charset_info, solrel->en_name );
    }
}

void solid_relname( rs_entname_t *solrel, const char* mysqldb, const char* mysqlrel)
{
    rs_entname_initbuf(solrel, NULL, NULL, NULL);
    solrel->en_schema = (char*)SsMemStrdup((char*)mysqldb);
    solrel->en_name = (char*)SsMemStrdup((char*)mysqlrel);
    if (lower_case_table_names) {
        my_caseup_str( system_charset_info, solrel->en_schema );
        my_caseup_str( system_charset_info, solrel->en_name );
    }
}

static void solid_my_caseup_str(char *name)
{
        my_caseup_str( system_charset_info, name );
}

static void solid_my_caseup_str_if_lower_case_table_names(char *name)
{
        if( lower_case_table_names ) {
            my_caseup_str( system_charset_info, name );
        }
}

/*#***********************************************************************\
*
*              is_soliddb_table
*
* Tells if the .FRM file is for solidDB table.
*
* Parameters :
*
*   THD* thd, in, - current my-sql thread.
*   File &frm_file, in - descriptor of .FRM file
*
* Return value :
* true - solidDB table,
* false - something else, can be other table or not a table at all.
*
* See also table.cpp::openfrm()
*/
bool is_soliddb_table( File &frm_file )
{

    SDB_DBUG_ENTER("solid:global:is_soliddb_table( THD* thd, File &frm_file )");
    uchar head[288];

    my_seek( frm_file, 0, MY_SEEK_SET, MYF(0) );

    if ( my_read( frm_file, (SS_MYSQL_ROW*) head, 64, MYF(MY_NABP) ) ) {
        SDB_DBUG_RETURN(false);
    }

    if ( !memcmp( head, STRING_WITH_LEN("TYPE=") ) ) {
        SDB_DBUG_RETURN(false);
    }

    if ( head[0] != (uchar)254 || head[1] != 1 ) {
        SDB_DBUG_RETURN(false);
    }

    if ( head[2] != FRM_VER && head[2] != FRM_VER + 1 &&
        !(head[2] >= FRM_VER + 3 && head[2] <= FRM_VER + 4) ) {
            SDB_DBUG_RETURN(false);
    }

    bool is_soliddb_table =
#if MYSQL_VERSION_ID >= 50100
        (enum legacy_db_type)
        (uint) head[3] == DB_TYPE_SOLID;
#else
        (enum db_type)
        (uint) head[3] == DB_TYPE_SOLID_DB;
#endif
    SDB_DBUG_RETURN(is_soliddb_table);
}

/*#***********************************************************************\
*
*              is_soliddb_table
*
* Tells if the table is solidDB table. Opens .FRM file for it.
*
* Parameters :
*
*   THD* thd, in, - current my-sql thread.
*   const char* dbname, in    - my-sql database name
*   const char* tablename  in - my-sql table name
*
* Return value :
* true - solidDB table,
* false - something else, can be other table or not a table at all.
*
* See also table.cpp::openfrm()
*/
bool is_soliddb_table( const char* dbname, const char* tablename )
{
    File  file;
    char  path[FN_REFLEN];
    bool  result = false;

    SDB_DBUG_ENTER("solid:global:is_soliddb_table(THD* thd, const char* dbname, const char* tablename)");

    strxmov(path, mysql_data_home, "/", dbname, "/", tablename, reg_ext, NullS);

    file = my_open( path, O_RDONLY | O_SHARE, MYF(0) );

    if (file  < 0) {
        SDB_DBUG_RETURN(false);
    }

    result = is_soliddb_table( file );

    my_close( file, MYF(MY_WME) );
    SDB_DBUG_RETURN(result);
}

/*#***********************************************************************\
*              is_DDL_command
* Tells if the currently executed command in the MySQL thread is a DDL
* command.
* Parameters :
*
*    THD* thd, in, MySQL current thread/process
*
* Return value : true or false
* Globals used : none
*/
static inline bool is_DDL_command(
        MYSQL_THD thd )
{
    static enum enum_sql_command DDL_commands[] = {
        SQLCOM_CREATE_TABLE,
        SQLCOM_CREATE_INDEX,
        SQLCOM_ALTER_TABLE,
        SQLCOM_RENAME_TABLE,
        SQLCOM_DROP_TABLE,
        SQLCOM_DROP_INDEX,
        /* These commands may be removed from the list as they do not influence
        * SolidDB database file actually,
        * although formally they are DDL commands.
        */
        SQLCOM_CREATE_DB,
        SQLCOM_DROP_DB,
        SQLCOM_ALTER_DB,
        SQLCOM_CREATE_FUNCTION,
        SQLCOM_DROP_FUNCTION,
        SQLCOM_CREATE_PROCEDURE,
        SQLCOM_CREATE_SPFUNCTION,
        SQLCOM_DROP_PROCEDURE,
        SQLCOM_ALTER_PROCEDURE,
        SQLCOM_ALTER_FUNCTION,
        SQLCOM_CREATE_VIEW,
        SQLCOM_DROP_VIEW,
        SQLCOM_CREATE_TRIGGER,
        SQLCOM_DROP_TRIGGER
    };

    bool is_DDL = false;
    size_t i;

    SDB_DBUG_ENTER("solid:is_DDL_command");

    if(!thd) {
        SDB_DBUG_RETURN(false);
    }

    is_DDL = false;

    for(i = 0; i < sizeof(DDL_commands)/sizeof(DDL_commands[0]); ++i) {
        if(soliddb_sql_command(thd) == DDL_commands[i]) {
            is_DDL = true;
            break;
        }
    }

    SDB_DBUG_RETURN(is_DDL);
}

/*#***********************************************************************\
*              operation_requires_xtable_lock
* Tells if the currently executed command in the MySQL thread is a DDL
* command which requires an exclusive table lock on the table.
* Parameters :
*
*    THD* thd, in, MySQL current thread/process
*
* Return value : true or false
* Globals used : none
*/
static inline bool operation_requires_xtable_lock(MYSQL_THD thd)
{
    static enum enum_sql_command XDDL_commands[] = {
        SQLCOM_CREATE_INDEX,
        SQLCOM_ALTER_TABLE,
        SQLCOM_RENAME_TABLE,
        SQLCOM_DROP_INDEX,
        SQLCOM_DROP_TABLE
    };

    SDB_DBUG_ENTER("operation_requires_xtable_lock");

    if(!thd) {
        SDB_DBUG_RETURN(false);
    }

    bool xlock_required = false;

    for(size_t i = 0; i < sizeof(XDDL_commands)/sizeof(XDDL_commands[0]); ++i ) {
        if(soliddb_sql_command(thd) == XDDL_commands[i]) {
            xlock_required = true;
            break;
        }
    }

    SDB_DBUG_RETURN(xlock_required);
}

/*#***********************************************************************\
*
*              copy_file
*
* Copies source file to specified directory. The destination file is
* given a new name and extension. If the destination directory does not
* exist it is created.
*
* Parameters :
*
*   THD* thd, in, - current my-sql thread.
*   File &src, in   - my-sql descriptor of opened source file
*   const char* dstdir, in - destination directory
*   const char* fname,  in - destination file name
*   const char* ext     in - destination file extension
*
* Return value : true - ok, false - failure.
*/
bool copy_file( File &src, const char* dstdir, const char* fname, const char* ext )
{
    char dstname[FN_REFLEN];
    SS_MYSQL_ROW block[2048];
    unsigned int read;

    DBUG_ENTER("solid:global:copy_file");

    fn_format( dstname, fname, dstdir, ext, MY_UNPACK_FILENAME );

    my_mkdir( dstdir, 0777, MYF(0) );

    File dst = my_open( dstname, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, MYF(0) );

    if ( dst < 0) {
        DBUG_RETURN(false);
    }

    my_seek( src, 0, MY_SEEK_SET, MYF(0) );

    while ( (read = my_read( src, block, sizeof(block), MYF(MY_FAE) ) ) > 0 &&
        read != MY_FILE_ERROR ) {
            my_write( dst, block, read, MYF(MY_NABP) );
    }

    DBUG_RETURN( !my_close( dst, MYF(MY_WME|MY_WME) ) && read != MY_FILE_ERROR );
}

/*#***********************************************************************\
 *
 *              solid_flush_logs
 *
 * Flush logs to the disk
 *
 * Parameters : -
 *
 * Return value : -
 *
 * Globals used : current_thd
 */
bool solid_flush_logs(
#if MYSQL_VERSION_ID >= 50100
        handlerton *hton
#endif
    )
{
        SOLID_CONN* con = NULL;
        MYSQL_THD thd = current_thd;
        DBUG_ENTER("solid_flush_logs");

        ss_dassert(thd != NULL);

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(hton, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        if (con) {
            dbe_db_t* db = NULL;
            rs_sysi_t* cd = NULL;

            cd = con->sc_cd;
            ss_dassert(cd != NULL);

            db = (dbe_db_t*)rs_sysi_db(cd);
            ss_dassert(db != NULL);

            dbe_db_logflushtodisk(db);
        }

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              solid_commit_or_rollback
 *
 * Checks the current error status (0 is OK, \=0 is an error) and commits
 * the current transaction if all is OK. Otherwise rolls the transaction
 * back.
 *
 * Parameters :
 *   int error_status - current error status (0 is OK, \=0 is an error)
 *   bool whole_transaction = false - should we commit or rollback whole
 *                                    transaction. Otherwise we commit or
 *                                    rollback only a statement.
 *
 * Return value : - new error status. (0 is OK, \=0 is an error)
 *
 * Globals used : current_thd
 */

static int solid_commit_or_rollback(int error_status, bool whole_transaction = false)
{
        int rc;
        MYSQL_THD thd = current_thd;
        /* if all is OK we commit and return */
        if (error_status==0) {
            rc = solid_commit(
#if MYSQL_VERSION_ID >= 50100
                            legacy_soliddb_hton,
#endif
                            thd, whole_transaction);
            return rc;
        } else { /* if there is an error we roll back */
            /* we assume rollback cannot fail */
            int rbrc = solid_rollback(
#if MYSQL_VERSION_ID >= 50100
                            legacy_soliddb_hton,
#endif
                            thd, whole_transaction);
            /* NOTE: error status is not changed by rollback */
            return error_status;
        }
}

/************************************************************************/
/* DISABLING/ENABLING OF TABLE KEYS                                     */
/************************************************************************/
int ha_soliddb::disable_foreign_keys()
{
        int rc = 0;
        rs_err_t* errh = NULL;
        int i;
        size_t current_fkey_idx = 0;
        void* kkey;
        const char* table_name = NULL;
        MYSQL_THD thd = NULL;
        SOLID_CONN *con = NULL;
        rs_entname_t *en = NULL;
        su_pa_t* keys = NULL;
        rs_ttype_t* tabletype = NULL;
        size_t n_fkeys = 0;
        table_forkeys_t* tfk = NULL;

        SDB_DBUG_ENTER("ha_soliddb::disable_foreign_keys");

#if MYSQL_VERSION_ID >= 50100
        table_name = table->s->path.str;
#else
        table_name = table->s->path;
#endif

        thd = current_thd;

        con = get_solid_connection(thd, table_name);

        en = rs_relh_entname(con->sc_cd,solid_table->st_rsrelh);
        keys = rs_relh_refkeys(con->sc_cd, solid_table->st_rsrelh);
        tabletype = rs_relh_ttype(con->sc_cd, solid_table->st_rsrelh);

        /* we need to count FKs as there may be NULL elements in 'keys' */
        n_fkeys = 0;
        su_pa_do_get(keys, i, kkey) {
            if (rs_relh_hasrefkey(con->sc_cd, solid_table->st_rsrelh,
                                  rs_key_name(con->sc_cd,(rs_key_t*)kkey))) {
                ++n_fkeys;
            }
        }

        if (!n_fkeys) {
            SDB_DBUG_RETURN(rc);
        }

        tfk = add_table_disabled_forkeys(en, n_fkeys);

        su_pa_do_get(keys, i, kkey) {
            rs_key_t* key = (rs_key_t*)kkey;

            if (rs_key_type(con->sc_cd, key) == RS_KEY_FORKEYCHK) {

                rs_relh_t *referenced_relh = NULL;
                rs_ttype_t* referenced_ttype = NULL;
                rs_key_t *parent_key = NULL;
                size_t n_fkey_parts = 0;
                uint* attids = NULL;
                char** refattnames = NULL;
                void* dummy;
                solid_bool fkdropok = FALSE;

                referenced_relh =
                    tb_dd_getrelhbyid(con->sc_cd,con->sc_trans,
                                      rs_key_refrelid(con->sc_cd, key),NULL,NULL);

                if (referenced_relh == NULL) {
                    /* Concurrent DDL, skip this key */
                    continue;
                }

                ss_output_1( rs_key_print_ex(con->sc_cd,tabletype,key); )

                referenced_ttype = rs_relh_ttype(con->sc_cd,referenced_relh);
                parent_key = rs_relh_basekeyforforgnkey(con->sc_cd, con->sc_trans,
                                               solid_table->st_rsrelh, key);

                /* we need number of explicitly defined key fields
                   i.e. ordering fields without key ID
                   i.e. rs_key_lastordering() + 1 - 1 */
                n_fkey_parts = rs_key_lastordering(con->sc_cd,key) -
                                      rs_key_first_datapart(con->sc_cd,key) + 1;

                /* number of fields in the foreign key and the referenced key must be same */
                ss_dassert(rs_key_lastordering(con->sc_cd,parent_key) -
                           rs_key_first_datapart(con->sc_cd,parent_key) + 1 == n_fkey_parts);

                attids = (uint*)SsMemAlloc(n_fkey_parts*sizeof(uint));
                refattnames = (char**)SsMemAlloc(n_fkey_parts*sizeof(char*));

                for (rs_ano_t kp = rs_key_first_datapart(con->sc_cd,key), kkp = 0;
                              kp <= rs_key_lastordering(con->sc_cd,key); ++kp, ++kkp) {

                    attids[kkp] = rs_ttype_physanotosql(con->sc_cd,
                                                        tabletype,
                                                        rs_keyp_ano(con->sc_cd,key,kp));
                }

                for (rs_ano_t kp = rs_key_first_datapart(con->sc_cd,parent_key), kkp = 0;
                              kp <= rs_key_lastordering(con->sc_cd,parent_key); ++kp, ++kkp) {
                    refattnames[kkp] = rs_ttype_aname(con->sc_cd,
                                                      referenced_ttype,
                                                      rs_keyp_ano(con->sc_cd,parent_key,kp));
                }

                tb_forkey_init_buf(tfk->forkeys + current_fkey_idx,
                        rs_key_name(con->sc_cd,key),
                        #if defined(SS_MYSQL) || defined(SS_MYSQL_AC)
                            rs_key_name(con->sc_cd,key),   /* TODO: is mysql key name same ? */
                        #endif
                        n_fkey_parts,
                        attids,
                        rs_relh_entname(con->sc_cd,referenced_relh), /* referenced (parent) table name */
                        refattnames,
                        (sqlrefact_t)rs_key_delete_action(con->sc_cd,key),
                        (sqlrefact_t)rs_key_update_action(con->sc_cd,key) );

                SsMemFree(attids);
                SsMemFree(refattnames);
                rs_relh_done(con->sc_cd,referenced_relh);
                attids = NULL;
                refattnames = NULL;

                ss_output_1(
                    tb_forkey_print(con->sc_cd,
                                rs_entname_getname(en),
                                tabletype,
                                tfk->forkeys + current_fkey_idx);
                )

                fkdropok =
                    tb_dropconstraint(con->sc_cd, con->sc_trans,
                                      solid_table->st_rsrelh,
                                      en->en_schema,
                                      NULL, rs_key_name(con->sc_cd,key),
                                      &dummy, (rs_err_t**)errh);
                if (!fkdropok) {
                    rc = handle_error(errh);
                    if (rc == 0) {
                        rc = E_FORKEYREFEXIST_S; /* TODO find a way to report the error */
                    }
                    drop_table_disabled_forkeys(tfk);
                    tfk = NULL;
                    break;
                }

                ++current_fkey_idx;
            }
        }

        SDB_DBUG_RETURN(rc);
}

int ha_soliddb::enable_foreign_keys()
{
        int rc = 0;
        rs_err_t* errh = NULL;
        rs_err_t* commit_errh = NULL;
        const char* table_name = NULL;
        MYSQL_THD thd = NULL;
        SOLID_CONN *con = NULL;
        rs_entname_t* en = NULL;
        table_forkeys_t* tfk = NULL;
        tb_relh_t* this_table = NULL;

        SDB_DBUG_ENTER("ha_soliddb::enable_foreign_keys");

#if MYSQL_VERSION_ID >= 50100
        table_name = table->s->path.str;
#else
        table_name = table->s->path;
#endif

        thd = current_thd;

        con = get_solid_connection(thd, table_name);
        en = rs_relh_entname(con->sc_cd,solid_table->st_rsrelh);

        tfk = get_table_disabled_forkeys(en);

        if ( !tfk ) {
            SDB_DBUG_RETURN(0);
        }

        this_table = tb_relh_new(con->sc_cd,solid_table->st_rsrelh,NULL);

        for (size_t k = 0; k < tfk->n_forkeys; ++k) {

            void* dummy = NULL;
            bool fkcreok = false;

            fkcreok = tb_addforkey(con->sc_cd, con->sc_trans,
                               this_table,
                               en->en_schema, en->en_catalog, en->en_name,
                               &tfk->forkeys[k],
                               &dummy, &errh);
            if (!fkcreok) {
                rc = handle_error(errh);
                break;
            }
        }

        if (!rc) {
            drop_table_disabled_forkeys(tfk);
        }

        ss_output_1 ({
            int i;
            void *key;
            rs_ttype_t* tabletype = NULL;
            su_pa_t* keys = NULL;
            tabletype = rs_relh_ttype(con->sc_cd, solid_table->st_rsrelh);
            keys = rs_relh_keys(con->sc_cd, solid_table->st_rsrelh);
            su_pa_do_get(keys, i, key) {
                rs_key_print_ex(con->sc_cd,tabletype,(rs_key_t*)key);
            }
        })

        SDB_DBUG_RETURN(rc);
}

int ha_soliddb::disable_indexes(uint mode)
{
        int rc = 0;
        rs_err_t* errh = NULL;
        rs_err_t* commit_errh = NULL;
        int i;
        size_t k = 0;
        /* rs_key_t* */ void* key;
        const char* table_name = NULL;
        MYSQL_THD thd = NULL;
        SOLID_CONN *con = NULL;
        rs_ttype_t* tabletype = NULL;
        su_pa_t* keys = NULL;
        size_t n_keys = 0;
        size_t n_forkeys = 0;
        table_indexes_t* tk = NULL;

        SDB_DBUG_ENTER("ha_soliddb::disable_indexes");

#if MYSQL_VERSION_ID >= 50100
        table_name = table->s->path.str;
#else
        table_name = table->s->path;
#endif

        thd = current_thd;

        con = get_solid_connection(thd, table_name);

        tabletype = rs_relh_ttype(con->sc_cd, solid_table->st_rsrelh);
        keys = rs_relh_keys(con->sc_cd, solid_table->st_rsrelh);

        ss_output_1(print_disabled_keys();)

        n_keys = 0;
        n_forkeys = 0;
        su_pa_do_get(keys, i, key) {
            rs_key_t* kkey = (rs_key_t*)key;
            char* key_name = NULL;

            key_name = rs_key_name(con->sc_cd,kkey);
            if (!rs_key_issyskey(con->sc_cd,kkey) &&
                !rs_key_isprimary(con->sc_cd,kkey)) {

               rs_relh_hasrefkey(con->sc_cd,solid_table->st_rsrelh,key_name) ? ++n_forkeys : ++n_keys;
            }
        }

        /* there is nothing to do if the table has neither user-defined indices
         * no foreign keys (which in their turn may have supporting indices)
         */
        if (!n_keys && !n_forkeys) {
            SDB_DBUG_RETURN(rc);
        }

        solid_trans_beginif(const_cast<handlerton*>(ht), thd, true);

        if (n_forkeys) {
            rc = disable_foreign_keys();
        }

        if (rc || n_keys == 0) {
            goto epilogue;
        }

        tk = add_table_disabled_keys(rs_relh_entname(con->sc_cd,solid_table->st_rsrelh),
                                     n_keys);

        su_pa_do_get(keys, i, key) {

            rs_key_t* kkey = (rs_key_t*)key;

            /* to this moment foreign keys should be dropped already
             * if they have not been, the code should be modified
             * not to process them in the loop. */
            if (!rs_key_issyskey(con->sc_cd,kkey) &&
                !rs_key_isprimary(con->sc_cd,kkey)) {

                rs_ano_t kp;
                void* dummy;
                solid_bool idropok = FALSE;

                ss_output_1 (
                  rs_key_print_ex(con->sc_cd,tabletype,kkey);
                )

                add_table_disabled_key(tk, k,
                                       rs_key_name(con->sc_cd,kkey),
                                       rs_key_isunique(con->sc_cd,kkey),
                                       /* we need number of explicitly defined key fields
                                          i.e. ordering fields without key ID
                                          i.e. rs_key_lastordering() + 1 - 1 */
                                       rs_key_lastordering(con->sc_cd,kkey));

                for(kp = rs_key_first_datapart(con->sc_cd,kkey);
                    kp <= rs_key_lastordering(con->sc_cd,kkey); ++kp) {

                    add_table_disabled_key_part(tk, k, kp-1,
                        rs_ttype_aname(con->sc_cd,
                                       rs_relh_ttype(con->sc_cd,solid_table->st_rsrelh),
                                       rs_keyp_ano(con->sc_cd,kkey,kp)),
                        !rs_keyp_isascending(con->sc_cd,kkey,kp),
                        #ifdef SS_COLLATION
                            rs_keyp_getprefixlength(con->sc_cd,kkey,kp)
                        #endif // SS_COLLATION
                        );
                }

                idropok =
                    tb_dropindex_relh(con->sc_cd, con->sc_trans,
                                      solid_table->st_rsrelh,
                                      kkey->k_name,
                                      NULL,
                                      NULL,
                                      NULL,
                                      /* (void**) cont = */ &dummy,
                                      (rs_err_t**)errh);
                if (!idropok) {
                    rc = handle_error(errh);
                    drop_table_disabled_keys(tk);
                    tk = NULL;
                    break;
                }

                ++k;
            }
        }

epilogue:
        rc = solid_commit_or_rollback(rc); // commit or rollback only the statement
        SDB_DBUG_RETURN(rc);
}

int ha_soliddb::enable_indexes(uint mode)
{
        int rc = 0;
        rs_err_t* errh = NULL;
        rs_err_t* commit_errh = NULL;
        const char* table_name = NULL;
        MYSQL_THD thd = NULL;
        SOLID_CONN *con = NULL;
        rs_ttype_t* tabletype = NULL;
        table_indexes_t* ti = NULL;
        rs_entname_t* en;
        size_t k = 0;

        SDB_DBUG_ENTER("ha_soliddb::enable_indexes");

#if MYSQL_VERSION_ID >= 50100
        table_name = table->s->path.str;
#else
        table_name = table->s->path;
#endif

        thd = current_thd;

        con = get_solid_connection(thd, table_name);

        tabletype = rs_relh_ttype(con->sc_cd, solid_table->st_rsrelh);
        en = rs_relh_entname(con->sc_cd,solid_table->st_rsrelh);

        ti = get_table_disabled_keys(en);

        if (!ti && !get_table_disabled_forkeys(en)) {
            int rc;
            rc = ha_solid_mysql_error(thd,NULL,E_RELNOTEXIST_S);
            SDB_DBUG_RETURN(rc);
        }

        solid_trans_beginif(const_cast<handlerton*>(ht), thd, true); // start the statement.

        for (k = 0; ti && k < ti->n_indexes; ++k) {

            bool icreok = false;
            icreok =
                tb_createindex_ext(con->sc_cd, con->sc_trans,
                                   ti->indexes[k].indexname,
                                   /* char* authid */ (char*)NULL,
                                   NULL, /* catalog */
                                   solid_table->st_rsrelh,
                                   rs_relh_ttype(con->sc_cd,solid_table->st_rsrelh),
                                   ti->indexes[k].unique,
                                   ti->indexes[k].attr_c,
                                   ti->indexes[k].attrs,
                                   ti->indexes[k].desc,
                                   #ifdef SS_COLLATION
                                       ti->indexes[k].prefix_lengths,
                                   #endif /* SS_COLLATION */
                                   TB_DD_CREATEREL_USER,
                                   &errh);
            if (!icreok) {
                rc = handle_error(errh);
                break;
            }
        }

        if (!rc) {
            rc = enable_foreign_keys();
        }

        rc = solid_commit_or_rollback(rc); // commit or rollback only the statement

        if (!rc && ti) {
            drop_table_disabled_keys(ti);
        }

        ss_output_1 ({
            su_pa_t* keys = NULL;
            int i;
            void *key;
            keys = rs_relh_keys(con->sc_cd, solid_table->st_rsrelh);
            su_pa_do_get(keys, i, key) {
                rs_key_print_ex(con->sc_cd,tabletype,(rs_key_t*)key);
            }
            print_disabled_keys();
        })

        SDB_DBUG_RETURN(rc);
}

/*
  Tests if indexes are disabled.

  SYNOPSIS
    indexes_are_disabled()
      no parameters

  RETURN
    0  indexes are not disabled
    1  all indexes (except the clustering primary key) are disabled.
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/
int ha_soliddb::indexes_are_disabled()
{
        SDB_DBUG_ENTER("ha_soliddb::indexes_are_disabled");

        const char* table_name =
                            #if MYSQL_VERSION_ID >= 50100
                                    table->s->path.str;
                            #else
                                    table->s->path;
                            #endif

        SOLID_CONN *con = get_solid_connection(current_thd, table_name);

        rs_entname_t *en = rs_relh_entname(con->sc_cd,solid_table->st_rsrelh);
        int rc =
            get_table_disabled_keys( en ) || get_table_disabled_forkeys( en ) ? 1 : 0;

        SDB_DBUG_RETURN(rc);
}

#if MYSQL_VERSION_ID >= 50100

/*#***********************************************************************\
 *
 *              ::check_if_incompatible_data
 *
 * Check wheather alter table changes are compatible or not
 *
 * Parameters :
 *
 *      HA_CREATE_INFO* info, in, use
 *      uint            table_changes, in, use
 *
 * Return value :
 *
 *      COMPATIBLE_DATA_YES if data is compatible or
 *      COMPATIBLE_DATA_NO
 *
 * Globals used :
 */

bool ha_soliddb::check_if_incompatible_data(
    HA_CREATE_INFO* info,
    uint        table_changes)
{
    if (table_changes != IS_EQUAL_YES) {

        return COMPATIBLE_DATA_NO;
    }

    /* Check that auto_increment value was not changed */
    if ((info->used_fields & HA_CREATE_USED_AUTO) &&
        info->auto_increment_value != 0) {

        return COMPATIBLE_DATA_NO;
    }

    /* Check that row format didn't change */
    if ((info->used_fields & HA_CREATE_USED_AUTO) &&
        get_row_type() != info->row_type) {

        return COMPATIBLE_DATA_NO;
    }

    return COMPATIBLE_DATA_YES;
}

/*#***********************************************************************\
 *
 *              solid_alter_table_flags
 *
 * Alter table flags currently implemented on solidDB
 *
 * Parameters :
 *
 *      uint  flags, in, not ussed
 *
 * Return value : uint
 *
 * Globals used :
 */
static inline uint solid_alter_table_flags(uint flags)
{
        /* No primary key support at the moment */

        return(HA_INPLACE_ADD_INDEX_NO_READ_WRITE
        | HA_INPLACE_DROP_INDEX_NO_READ_WRITE
        | HA_INPLACE_ADD_UNIQUE_INDEX_NO_READ_WRITE
        | HA_INPLACE_DROP_UNIQUE_INDEX_NO_READ_WRITE);
}

/*#***********************************************************************\
 *
 *              ::add_index
 *
 * Add indexes to a table. Note that at the moment we use FULL table scan
 * for every index added to a table. TODO: use only one FULL table scan
 * and build indexes using this information.
 *
 * Parameters :
 *
 *      TABLE* table_arg, in, use
 *      KEY*   key_info, in, use
 *      uint   num_of_keys, in, use
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::add_index(
        TABLE* table_arg,
        KEY *key_info,
        uint num_of_keys)
{
        rs_relh_t* relh = NULL;
        st_key* key = NULL;
        char* mysqltabname = NULL;
        SOLID_CONN* con = NULL;
        rs_sysi_t* cd = NULL;
        tb_trans_t* trans = NULL;
        char*  extrainfo = NULL;
        rs_entname_t en;
        tb_connect_t* tbcon = NULL;
        su_err_t*  errh = NULL;
        uint nkey = 0;
        bool succp = TRUE;
        solid_bool finished = FALSE;
        MYSQL_THD thd = NULL;
        int rc = 0;

        DBUG_ENTER("ha_soliddb::add_index");
        ss_dassert(table_arg != NULL);
        ss_dassert(key_info != NULL);
        ss_dassert(num_of_keys != 0);

        thd = current_thd;

        mysqltabname = table_arg->s->path.str;

        solid_relname(&en, mysqltabname);

        con = get_solid_connection(thd, mysqltabname);

        CHK_CONN(con);
        cd = con->sc_cd;
        tbcon = con->sc_tbcon;
        trans = con->sc_trans;
        relh = solid_table->st_rsrelh;

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        for(nkey = 0; succp && nkey < num_of_keys; nkey++) {

            key = &key_info[nkey];
            ss_dassert(key != NULL);
            key->table = table;
            ss_dassert(key->table != NULL);

            succp = ha_solid_createindex(relh, key, en.en_name, tbcon, cd, trans,
                                         en.en_schema, NULL, extrainfo, &errh);
        }

        if (succp) {
            do {
                succp = tb_trans_commit(cd, trans, &finished, &errh);

                /* Store MySQL key number to connection if we had a duplicate key error */
                if (errh != NULL) {
                    int solrc = su_err_geterrcode(errh);

                    if (solrc == DBE_ERR_UNIQUE_S || solrc == DBE_ERR_PRIMUNIQUE_S) {
                        char *nerrmsg;
                        char *tmp;
                        char *saveptr = NULL;

                        uint len = strlen(su_err_geterrstr(errh))+1;

                        nerrmsg = (char*) SsMemAlloc(len);
                        strcpy(nerrmsg, su_err_geterrstr(errh));
                        tmp = strtok_r(nerrmsg, "(", &saveptr);
                        tmp = strtok_r(NULL, ")", &saveptr);

                        for(nkey = 0; nkey < num_of_keys; nkey++) {
                            char *indexname;
                            indexname = (char *)SsMemAlloc(strlen(key->name)+64);

                            keyname_from_mysql_to_solid(cd, relh, key_info[nkey ].name,
                                                        indexname);

                            if (!(strcmp(tmp, indexname))) {
                                con->sc_err_tableid = rs_relh_relid(cd, relh);
                                break;
                            }

                            SsMemFree(indexname);
                        }

                        SsMemFree(nerrmsg);
                    }
                }

            } while (rs_sysi_lockwait(cd) || !finished);

        }

        rs_entname_done_buf(&en);

        if (!succp) {
            rc = ha_solid_mysql_error(thd, errh, rc);
            su_err_done(errh);
        } else {
            rc = 0;
        }

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::prepare_drop_index
 *
 * Mark provided indexes to be dropped
 *
 * Parameters :
 *
 *      TABLE* table_arg
 *      uint*  key_num
 *      uint   num_of_keys
 *
 * Return value : 0 or error code
 *
 * Globals used :
 */
int ha_soliddb::prepare_drop_index(
        TABLE* table_arg,
        uint *key_num,
        uint num_of_keys)
{
        SOLID_CONN* con = NULL;
        rs_sysi_t* cd = NULL;
        tb_trans_t* trans = NULL;
        su_err_t*  errh = NULL;
        rs_key_t* solid_key = NULL;
        rs_relh_t* relh = NULL;
        char *extrainfo = NULL;
        void*  cont = NULL;
        bool succp = TRUE;
        MYSQL_THD thd = NULL;
        int rc = 0;
        uint nkey;

        DBUG_ENTER("ha_soliddb::prepare_drop_index");
        ss_dassert(table_arg != NULL);
        ss_dassert(key_num != NULL);
        ss_dassert(num_of_keys != 0);

        thd = current_thd;

        // mysqltabname is in ' table_arg->s->path.str '

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(this->ht, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);

        relh = solid_table->st_rsrelh;

        ss_dassert(con != NULL);
        cd = con->sc_cd;
        ss_dassert(cd != NULL);
        trans = con->sc_trans;

        solid_trans_beginif((handlerton *)ht, thd, TRUE);

        for (nkey = 0; succp && nkey < num_of_keys; nkey++) {
            rs_entname_t* en;
            uint knum = key_num[nkey];

            solid_key = solid_resolve_key(cd, solid_table->st_rsrelh, &table_arg->key_info[knum]);

            ss_dassert(solid_key != NULL);

            en = rs_relh_entname(cd, solid_table->st_rsrelh);

            succp = tb_dropindex_relh(
                        cd,
                        trans,
                        relh,
                        rs_key_name(cd, solid_key),
                        rs_entname_getschema(en),
                        rs_entname_getcatalog(en),
                        extrainfo,
                        &cont,
                        &errh);
        }

        if (!succp) {
            rc = ha_solid_mysql_error(thd, errh, rc);
            su_err_done(errh);
        } else {
            rc = 0;
        }

        DBUG_RETURN(rc);
}

/*#***********************************************************************\
 *
 *              ::final_drop_index
 *
 * Finalize index drop, nothing really to do because index is really dropped
 * at commit.
 *
 * Parameters :
 *
 *      TABLE* table_arg
 *
 * Return value : 0
 *
 * Globals used :
 */
int ha_soliddb::final_drop_index(TABLE* table_arg)
{
        DBUG_ENTER("ha_soliddb::final_drop_index");
        ss_dassert(table_arg != NULL);

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::reset
 *
 * Reset to statement defaults
 *
 * Parameters : -
 *
 * Return value : 0
 *
 *
 * Globals used : THD* current_thd
 */
int ha_soliddb::reset()
{
        DBUG_ENTER("ha_soliddb:reset");

        MYSQL_THD thd = current_thd;

        extra_keyread = FALSE;
        extra_retrieve_primary_key = FALSE;
        extra_retrieve_all_cols = FALSE;
        extra_ignore_duplicate = FALSE;
        extra_replace_duplicate = FALSE;
        extra_update_duplicate = FALSE;

        DBUG_RETURN(0);
}

/*#***********************************************************************\
 *
 *              ::get_auto_increment
 *
 * Reserves an interval of auto_increment values from the handler.
 * offset and increment means that we want values to be of the form
 * offset + N * increment, where N>=0 is integer.
 * If the function sets *first_value to ~(ulonglong)0 it means an error.
 * If the function sets *nb_reserved_values to ULONGLONG_MAX it means it has
 * reserved to "positive infinite".
 *
 * Parameters :
 *
 *     ulonglong  offset, in, use
 *     ulonglong  increment, in, use
 *     ulonglong  nb_desired_values, in, use
 *     ulonglong* first_value, out
 *     ulonglong* nb_reserved_values, out
 *
 * Return value : -
 *
 * Globals used : THD* current_thd
 */
void ha_soliddb::get_auto_increment(
        ulonglong offset,
        ulonglong increment,
        ulonglong nb_desired_values,
        ulonglong *first_value,
        ulonglong *nb_reserved_values)
{
        DBUG_ENTER("ha_soliddb:get_auto_increment");

        ss_int8_t i8;
        SOLID_CONN* con;
        ulonglong solid_value = 0;
        bool result = FALSE;
        rs_relh_t*  relh = NULL;
        long        seq_id=0;
        rs_auth_t*  auth;
        rs_atype_t* atype;
        rs_aval_t*  aval;
        solid_bool  p_finishedp;
        rs_err_t*   errh = NULL;
        MYSQL_THD        thd = current_thd;
        su_ret_t suret;

        ss_dassert(first_value != NULL);
        ss_dassert(nb_reserved_values != NULL);

#if MYSQL_VERSION_ID >= 50100
        con = get_solid_ha_data_connection(this->ht, thd);
#else
        con = get_solid_ha_data_connection((handlerton *)&solid_hton, thd);
#endif

        CHK_CONN(con);
        relh = solid_table->st_rsrelh;
        ss_dassert(relh != NULL);
        auth = rs_sysi_auth(con->sc_cd);
        ss_dassert(auth != NULL);
        rs_auth_setsystempriv(con->sc_cd, auth, TRUE);

        seq_id = rs_relh_readautoincrement_seqid(con->sc_cd, relh);

        if (!seq_id) {
            *first_value = (~(ulonglong) 0);
            rs_auth_setsystempriv(con->sc_cd, auth, FALSE);
            DBUG_VOID_RETURN;
        }

        atype = rs_atype_initbigint(con->sc_cd);
        aval = rs_aval_create(con->sc_cd, atype);

        do {
            suret = tb_seq_lock(con->sc_cd, con->sc_trans, seq_id, &errh);
        } while(rs_sysi_lockwait(con->sc_cd) || suret == DBE_RC_WAITLOCK);


        ss_dassert(suret == DBE_RC_SUCC);

        result = tb_seq_next(con->sc_cd, con->sc_trans, seq_id,
                             FALSE, atype, aval, &p_finishedp, &errh);

        if (result) {
            i8 = rs_aval_getint8(con->sc_cd, atype, aval);

            solid_value = SsInt8GetNativeUint8(i8);

            *first_value = solid_value;

            *nb_reserved_values = ULONGLONG_MAX; /* Because we have a lock to sequence */
        }

        rs_aval_free(con->sc_cd, atype, aval);
        rs_atype_free(con->sc_cd, atype);
        rs_auth_setsystempriv(con->sc_cd, auth, FALSE);

        if (errh) {
            ss_pprintf_1(("solidDB: [Error]::get_auto_increment: %s\n",  su_err_geterrstr(errh)));
            *first_value = (~(ulonglong) 0);
            su_err_done(errh);
        }

        DBUG_VOID_RETURN;
}

#endif /* MYSQL_VERSION_ID >= 50100 */

/* #if defined(HAVE_SOLIDDB_BACKUP_NEW) && !defined(MYSQL_DYNAMIC_PLUGIN) */
/*
WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
W                                                                         W
W                             B A C K U P                                 W
W                                                                         W
WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW
*/
#define SAFE_DELETE(p)    { if (p) { delete(p); } }
#define SAFE_FREE(p,free) { if (p) { free(p);   } }

/*#***********************************************************************\
 *
 *              bkp_show_error
 *
 * Show backup error
 *
 * Parameters :
 *
 *      char*   errmsg, in
 *              error message to show
 *      bool    system_error, in
 *              "true" means that this is internal (system) error
 *
 * Return value :
 *
 * Globals used :
 */
void bkp_show_error(const char* errmsg, bool system_error = false)
{
        MYSQL_THD   thd = current_thd;
        Protocol*   protocol = (Protocol *)thd->protocol;
        List<Item>  field_list;
        Item*       item;

/*
        field_list.push_back(item = new Item_empty_string("status", NAME_LEN * 2));
        item->maybe_null = 1;
        field_list.push_back(item = new Item_empty_string("description", NAME_LEN * 2));
        item->maybe_null = 1;
        protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
*/

        protocol->prepare_for_resend();
        protocol->store(system_error ? "SYSTEM ERROR" : "ERROR", system_charset_info);
        protocol->store(errmsg, system_charset_info);
        protocol->write();
	if (protocol->write())
		sql_print_error("Failed on soliddb_net_write, writing to stderr instead: %s\n", errmsg);
        /* send_eof(thd); */
/*
  *** MYSIAM EAXMPLE ***
  protocol->prepare_for_resend();
  protocol->store(name, length, system_charset_info);
  protocol->store(param->op_name, system_charset_info);
  protocol->store(msg_type, system_charset_info);
  protocol->store(msgbuf, msg_length, system_charset_info);
  if (protocol->write())
    sql_print_error("Failed on my_net_write, writing to stderr instead: %s\n",
                    msgbuf);

*/
}

/************************************************************************
 *
 *              Backup Syntax
 *
 *  BACKUP parameters
 *
 *  parameters :: abort | start | status | wait
 *
 *  abort  :: -ABORT
 *  start  :: [-DIR=path] [-MYISAM[=ON|OFF]] [-SYSTEM[=ON|OFF]]
 *            [-CONFIG[=ON|OFF]] [-EMPTYDIR[=ON|OFF]]
 *  status :: -STATUS
 *  wait   :: -WAIT
 */
typedef enum {  // Backup command defined by parameters
        BKP_CMD_UNDEF,
        BKP_CMD_ABORT,
        BKP_CMD_START,
        BKP_CMD_STATUS,
        BKP_CMD_WAIT,
} BKP_CMD;

#define BKP_OPT_ON      0x0002  // Backup option may be ON,
#define BKP_OPT_OFF     0x0001  // OFF, or
#define BKP_OPT_UNDEF   0x0000  // undefined

typedef struct {  // Result of parameters parsing
        BKP_CMD   cmd;          // backup command
        char*     dir;          // bakup directory
        ulong     opt_myisam;   // -MYISAM option
        ulong     opt_system;   // -SYSTEM option
        ulong     opt_config;   // -CONFIG option
        ulong     opt_dempty;   // -EMPTYDIR option
} BKP_PARAMS;

char* bkp_parse_path(char** text);
int   bkp_parse_onoff(char** text, ulong* opt);

/*#***********************************************************************\
 *
 *              bkp_parse_params
 *
 * Backup parameters parser
 *
 * Parameters :
 *
 *      char*         text, in
 *                    backup parameters text
 *      BKP_PARAMS*   data, out
 *                    result of parameters parsing
 *
 * Return value :
 *
 * Globals used :
 */
void bkp_parse_params(char* text, BKP_PARAMS* data)
{
        BKP_CMD cmd;

        text = text ? text : (char*)"";

        data->cmd  = BKP_CMD_UNDEF;
        data->dir  = NULL;
        data->opt_myisam  = BKP_OPT_UNDEF;
        data->opt_system  = BKP_OPT_UNDEF;
        data->opt_config  = BKP_OPT_UNDEF;
        data->opt_dempty  = BKP_OPT_UNDEF;

        while (true) {
            if (!*(text = SsStrTrimLeft(text))) {
                break;
            }
            if (*text != '-') {
                bkp_show_error("Backup parameters must start with '-'");
                goto ERROR;
            }

            if (!SsStrnicmp(text, "-abort", 6)) {
                text += 6;
                cmd = BKP_CMD_ABORT;

            } else if (!SsStrnicmp(text, "-status", 7)) {
                text += 7;
                cmd = BKP_CMD_STATUS;

            } else if (!SsStrnicmp(text, "-wait", 5)) {
                text += 5;
                cmd = BKP_CMD_WAIT;

            } else if (!SsStrnicmp(text, "-dir", 4)) {
                text += 4;
                cmd = BKP_CMD_START;
                if (!(data->dir = bkp_parse_path(&text))) {
                    goto ERROR;
                }

            } else if (!SsStrnicmp(text, "-myisam", 7)) {
                text += 7;
                cmd = BKP_CMD_START;
                if (bkp_parse_onoff(&text, &data->opt_myisam)) {
                    goto ERROR;
                }

            } else if (!SsStrnicmp(text, "-system", 7)) {
                text += 7;
                cmd = BKP_CMD_START;
                if (bkp_parse_onoff(&text, &data->opt_system)) {
                    goto ERROR;
                }

            } else if (!SsStrnicmp(text, "-config", 7)) {
                text += 7;
                cmd = BKP_CMD_START;
                if (bkp_parse_onoff(&text, &data->opt_config)) {
                    goto ERROR;
                }

            } else if (!SsStrnicmp(text, "-emptydir", 9)) {
                text += 9;
                cmd = BKP_CMD_START;
                if (bkp_parse_onoff(&text, &data->opt_dempty)) {
                    goto ERROR;
                }

            } else {
                bkp_show_error("Unknown backup parameter");
                goto ERROR;
            }

            if (data->cmd == BKP_CMD_UNDEF) {
                data->cmd = cmd;
            } else if (data->cmd != cmd) {
                bkp_show_error("Incompatible backup parameters");
                goto ERROR;
            }
        }

        // Empty parameters can be only in case of START
        if (data->cmd == BKP_CMD_UNDEF) {
            data->cmd = BKP_CMD_START;
        }

        // Nothing should be left in the text
        if (*(text = SsStrTrimLeft(text))) {
            bkp_show_error("Incorrect backup parameters");
            goto ERROR;
        }

        return;

ERROR:
        data->cmd = BKP_CMD_UNDEF;
        return;
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
char* bkp_parse_path(char** text)
{
        char*   ptr = SsStrTrimLeft(*text);
        char*   path = NULL;
        bool    stroke;
        bool    quotat;

        if (*ptr != '=') {
            bkp_show_error("Missed '=' in -dir parameter");
            return NULL;
        }
        ptr = SsStrTrimLeft(++ptr);

        stroke = (*ptr == '\'');
        quotat = (*ptr == '"');
        if (stroke || quotat) {
            ptr = SsStrTrimLeft(++ptr);
        }

        path = ptr;

        for (path = ptr; *ptr; ptr++) {
          if (stroke && *ptr == '\'' ||
              quotat && *ptr == '"'  ||
              !stroke && !quotat && *ptr == ' ') {
              break;
          }
          if (*ptr == '\\' &&
              (*(ptr+1) == '\'' || *(ptr+1) == '"' || *(ptr+1) == ' ')) {
              ptr++;
          }
        }

        if (*ptr) {
            *ptr++ = '\0';
        }

        *text = ptr;
        return path;
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
int bkp_parse_onoff(char** text, ulong* opt)
{
        char*   ptr = *text;
        int     res = 0;

        if (*(ptr = SsStrTrimLeft(ptr)) == '=') {
            ptr += 1;
            ptr = SsStrTrimLeft(ptr);
            if  (!SsStrnicmp(ptr, "ON", 2)) {
                ptr += 2;
                *opt = BKP_OPT_ON;
            } else if (!SsStrnicmp(ptr, "OFF", 3)) {
                ptr += 3;
                *opt = BKP_OPT_OFF;
            } else {
                bkp_show_error("Incorrect value of boolean parameter");
                res = -1;
            }
        } else {
            *opt = BKP_OPT_ON;
        }

        *text = ptr;
        return res;
}

/************************************************************************
 *
 *              Macros: Files and Table Types
 */
#define IS_FRM_FILE(name) ( (name) && strlen(name) > 4 && !SsStricmp((name) + strlen(name) - 4, ".frm") )
#define IS_MYD_FILE(name) ( (name) && strlen(name) > 4 && !SsStricmp((name) + strlen(name) - 4, ".myd") )
#define IS_MYI_FILE(name) ( (name) && strlen(name) > 4 && !SsStricmp((name) + strlen(name) - 4, ".myi") )
#define IS_TRG_FILE(name) ( (name) && strlen(name) > 4 && !SsStricmp((name) + strlen(name) - 4, ".trg") )
#define IS_TRN_FILE(name) ( (name) && strlen(name) > 4 && !SsStricmp((name) + strlen(name) - 4, ".trn") )

#define IS_FILE_FOR_BACKUP(name)  ( IS_FRM_FILE(name) ||  \
                                    IS_MYD_FILE(name) ||  \
                                    IS_MYI_FILE(name) ||  \
                                    IS_TRG_FILE(name) ||  \
                                    IS_TRN_FILE(name)  )
#if MYSQL_VERSION_ID < 50100
#define IS_TYPE_FOR_BACKUP(type)  ( (type) == DB_TYPE_MYISAM  || \
                                    (type) == DB_TYPE_SOLID_DB )
#else
#define IS_TYPE_FOR_BACKUP(type)  ( (type) == DB_TYPE_MYISAM  || \
                                    (type) == DB_TYPE_SOLID)
#endif

#if MYSQL_VERSION_ID >= 50100
#define DB_TYPE   enum legacy_db_type
#else
#define DB_TYPE   enum db_type
#endif

/*#***********************************************************************\
 *
 *              define_table_type
 *
 * Define table type (by .FRM file analyses)
 *
 * Parameters :
 *
 *      File      frm_file, in
 *                opened .FRM file of the table
 *
 * Return value :
 *
 *      DB_TYPE   type of the table
 *
 * Globals used :
 */
DB_TYPE define_table_type(File &frm_file)
{
        uchar head[288];

        my_seek(frm_file, 0, MY_SEEK_SET, MYF(0));

        if (my_read(frm_file, (SS_MYSQL_ROW*) head, 64, MYF(MY_NABP)) ||
            !memcmp(head, STRING_WITH_LEN("TYPE=")) ||
            head[0] != (uchar)254 || head[1] != 1) {
            return DB_TYPE_UNKNOWN;
        }

        if (head[2] != FRM_VER && head[2] != FRM_VER + 1 &&
            head[2] != FRM_VER + 3 && head[2] != FRM_VER + 4) {
            return DB_TYPE_UNKNOWN;
        }

        return (DB_TYPE) (uint) head[3];
}

/*#***********************************************************************\
 *
 *              define_table_type
 *
 * Define table type
 *
 * Parameters :
 *
 *      const char*   dbname, in
 *                    database name
 *      const char*   tablename, in
 *                    table name
 *
 * Return value :
 *
 *      DB_TYPE       type of the table
 *
 * Globals used :
 */
DB_TYPE define_table_type(const char* dbname, const char* tablename)
{
        File file;
        char path[FN_REFLEN];
        DB_TYPE result;

        strxmov(path, mysql_data_home, "/", dbname, "/", tablename, ".frm", NullS);

        if ((file = my_open(path, O_RDONLY|O_SHARE, MYF(0))) < 0) {
            return DB_TYPE_UNKNOWN;
        }

        result = define_table_type(file);
        my_close(file, MYF(MY_WME));
        return result;
}

/*#***********************************************************************\
 *
 *              get_triggers_table
 *
 * Define table name associated with the trigger
 *
 * Parameters :
 *
 *      const char*   dbname, in
 *                    database name
 *      const char*   triggername, in
 *                    trigger name
 *      char*         tablename, out
 *                    buffer to write the table name
 *                    if the function failed to define the table accosiated
 *                    with the trigger, it returns empty tablename.
 *
 * Return value :
 *
 * Globals used :
 */
void get_triggers_table(const char* dbname, const char* triggername, char* tablename)
{
        char  path[FN_REFLEN];
        File  file = (File) -1;
        uint  size = 510;
        char  buff[512];
        char* ptr = buff;
        char* next;

        *tablename = '\0';

        strxnmov(path, FN_REFLEN, mysql_data_home, "/", dbname, "/", triggername, ".TRN", NullS);

        if ((file = my_open(path, O_RDONLY | O_SHARE, MYF(MY_WME))) < 0 ||
            (size = my_read(file, (SS_MYSQL_ROW*) buff, size, MYF(MY_WME))) == MY_FILE_ERROR) {
            goto FINISH;
        }

        buff[size] = '\0';
        if (!(next = strchr(ptr, '\n'))) {
            goto FINISH;
        }

        *next++ = '\0';
        if (SsStrcmp(ptr, "TYPE=TRIGGERNAME")) {
            goto FINISH;
        }

        if (!(next = strchr(ptr = next, '='))) {
            goto FINISH;
        }

        *next++ = '\0';
        if (SsStrcmp(ptr, "trigger_table")) {
            goto FINISH;
        }

        if (!(next = strchr(ptr = next, '\n'))) {
            goto FINISH;
        }

        *next++ = '\0';
        strcpy(tablename, ptr);

FINISH:
        if (file >= 0) {
            my_close(file, MYF(MY_WME));
        }
}

/************************************************************************
 *
 *              Clist class
 *
 *  We need to have several types of lists in the backup:
 *  - list of strings which are names of triggers defined for a table
 *  - list of tables to be backed up
 *  - list of data bases to be backed up
 *  Clist is implemented as a wrapper of the su_list_t type. Note that
 *  additionally to itemdel() function (used in su_list_t) we also use
 *  itemnew() function for creating new item instances which are added
 *  to the list.
 */
typedef su_list_node_t* POSITION;

class CList
{
public:
        static void* operator new(size_t size);
        static void operator delete(void* ptr);

        CList(void* (*itemnew)(void*) = NULL, void (*itemdel)(void*) = NULL);
        ~CList();

        inline  POSITION    add(void* item);
        inline  void        del(POSITION pos);
        inline  POSITION    del_and_next(POSITION pos);
        inline  POSITION    first();
        inline  POSITION    next(POSITION pos);
        inline  bool        is_empty();
        inline  void        clear();

private:
        su_list_t*    m_list;
        void*         (*m_itemnew)(void*);
};

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void* CList::operator new(size_t size) {
        return (void*) SsMemAlloc(size);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void CList::operator delete(void* ptr) {
        SsMemFree(ptr);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
CList::CList(void* (*itemnew)(void*), void (*itemdel)(void*)) {
        m_list = su_list_init(itemdel);
        m_itemnew = itemnew;
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
CList::~CList() {
        su_list_done(m_list);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION CList::add(void* item) {
        if (m_itemnew) {
            item = m_itemnew(item);
        }
        return su_list_insertlast(m_list, item);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void CList::del(POSITION pos) {
        su_list_remove(m_list, pos);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION CList::del_and_next(POSITION pos) {
        return su_list_removeandnext(m_list, pos);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION CList::first() {
        return su_list_first(m_list);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION CList::next(POSITION pos) {
        return su_list_next(m_list, pos);
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
bool CList::is_empty() {
        return m_list->list_length == 0;
}

/*#***********************************************************************\
 *
 *
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void CList::clear() {
        su_list_clear(m_list);
}

/************************************************************************
 *
 *              List of strings
 */

/*#***********************************************************************\
 *
 *              stringnew
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void* stringnew(void* p) {
        ss_assert(p);
        return SsMemStrdup((char*) p);
}

/*#***********************************************************************\
 *
 *              stringdel
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void stringdel(void* p) {
        SAFE_FREE(p, SsMemFree);
}

/*#***********************************************************************\
 *
 *              new_string_list
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline CList* new_string_list() {
        return new CList(stringnew, stringdel);
}

/*#***********************************************************************\
 *
 *              get_string_at
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline char* get_string_at(POSITION pos) {
        return (char*) pos->ln_data;
}

/*#***********************************************************************\
 *
 *              string_list_search
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION string_list_search(CList* list, const char* str)
{
        for (POSITION pos = list->first(); pos; pos = list->next(pos)) {
            if (!SsStricmp(get_string_at(pos), str)) {
                return pos;
            }
        }
        return NULL;
}

typedef struct bkp_table_st  BKP_TABLE;  // table to be backed up
typedef struct bkp_dbase_st  BKP_DBASE;  // database to be backed up

/************************************************************************
 *
 *              Table to be backed up
 */
struct bkp_table_st {
        BKP_DBASE*  dbase;      // database to which the table belongs
        char*       name;       // table name
        DB_TYPE     type;       // table type: DB_TYPE_MYISAM, DB_TYPE_SOLID
        double      size;       // table size
        CList*      triggers;   // list of triggers defined for the table
        bool        failed;     // backup of the table failed
};

/************************************************************************
 *
 *              List of tables
 */


/*#***********************************************************************\
 *
 *              tablenew
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void* tablenew(void* tablename)
{
        ss_assert(tablename);
        BKP_TABLE* table = (BKP_TABLE*) SsMemAlloc(sizeof(BKP_TABLE));
        table->dbase  = NULL;
        table->name   = (char*) SsMemStrdup((char*) tablename);
        table->type   = DB_TYPE_UNKNOWN;
        table->size   = 0;
        table->triggers = new_string_list();
        table->failed = false;
        return table;
}

/*#***********************************************************************\
 *
 *              tabledel
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void tabledel(void* p)
{
        if (p) {
            BKP_TABLE* table = (BKP_TABLE*) p;
            SAFE_FREE(table->name, SsMemFree);
            SAFE_DELETE(table->triggers);
            SsMemFree(table);
        }
}

/*#***********************************************************************\
 *
 *              new_table_list
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline CList* new_table_list() {
        return new CList(tablenew, tabledel);
}

/*#***********************************************************************\
 *
 *              get_table_at
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline BKP_TABLE* get_table_at(POSITION pos) {
        return (BKP_TABLE*) pos->ln_data;
}

/*#***********************************************************************\
 *
 *              table_list_search
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION table_list_search(CList* list, const char* tablename)
{
        for (POSITION pos = list->first(); pos; pos = list->next(pos)) {
            if (!SsStricmp(get_table_at(pos)->name, tablename)) {
                return pos;
            }
        }
        return NULL;
}

/************************************************************************
 *
 *              Database to be backed up
 */
struct bkp_dbase_st {
        BKP_CTX*  ctx;        // backup context
        char*     name;       // database name
        double    size;       // database size
        CList*    tables;     // list of tables
};

/************************************************************************
 *
 *              List of databases
 */

/*#***********************************************************************\
 *
 *              dbasenew
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void* dbasenew(void* dbasename)
{
        ss_assert(dbasename);
        BKP_DBASE* dbase = (BKP_DBASE*) SsMemAlloc(sizeof(BKP_DBASE));
        dbase->ctx  = NULL;
        dbase->name = (char* )SsMemStrdup((char*) dbasename);
        dbase->size = 0;
        dbase->tables = new_table_list();
        return dbase;
}

/*#***********************************************************************\
 *
 *              dbasedel
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void dbasedel(void* p)
{
        if (p) {
            BKP_DBASE* dbase = (BKP_DBASE*) p;
            SAFE_FREE(dbase->name, SsMemFree);
            SAFE_DELETE(dbase->tables);
            SsMemFree(dbase);
        }
}

/*#***********************************************************************\
 *
 *              new_dbase_list
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline CList* new_dbase_list() {
        return new CList(dbasenew, dbasedel);
}

/*#***********************************************************************\
 *
 *              get_dbase_at
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
inline BKP_DBASE* get_dbase_at(POSITION pos) {
        return (BKP_DBASE*) pos->ln_data;
}

/*#***********************************************************************\
 *
 *              dbase_list_search
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
POSITION dbase_list_search(CList* list, const char* dbasename)
{
        for (POSITION pos = list->first(); pos; pos = list->next(pos)) {
            if (!SsStricmp(get_dbase_at(pos)->name, dbasename)) {
                return pos;
            }
        }
        return NULL;
}

/************************************************************************
 *
 *              Backup context
 */
struct bkp_ctx_st {
        bool        active;         // backup is active (is in progress)
        bool        failed;         // backup failed
        bool        abort;          // backup is (to be) aborted
        bool        killed;         // backup thread is killed
        char        dir[FN_REFLEN]; // backup destination directory
        ulong       opt_myisam;     // are myisam tables to be backed up?
        ulong       opt_system;     // are system tables to be backed up?
        ulong       opt_config;     // is option (config) file to be backed up?
        ulong       opt_dempty;     // is backup directory be emptied?
        double      total_size;     // size of all files to be backed up
        double      done_size;      // size of all backed up files
        CList*      databases;      // databases involved in the backup
        MYSQL_THD   thd;            // defined only within bkp_thread_func
        rs_sysi_t*  cd;
        su_err_t**  p_errh;
};

/*#***********************************************************************\
 *
 *              init_bkp_ctx
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void init_bkp_ctx(BKP_CTX* ctx)
{
        ctx->active     = false;
        ctx->failed     = false;
        ctx->abort      = false;
        ctx->killed     = false;
        ctx->dir[0]     ='\0';
        ctx->opt_myisam = 0;
        ctx->opt_system = 0;
        ctx->opt_config = 0;
        ctx->opt_dempty = 0;
        ctx->total_size = 0;
        ctx->done_size  = 0;

        ctx->databases->clear();

        ctx->thd    = NULL;
        ctx->cd     = NULL;
        ctx->p_errh = NULL;
}

/*#***********************************************************************\
 *
 *              create_bkb_ctx
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
BKP_CTX* create_bkp_ctx()
{
        BKP_CTX* ctx = (BKP_CTX*) SsMemAlloc(sizeof(BKP_CTX));
        CList* dblist = new_dbase_list();

        if (!ctx || !dblist) {
            SAFE_FREE(ctx, SsMemFree);
            SAFE_DELETE(dblist);
            bkp_show_error("create_bkp_ctx() failed", true);
            return NULL;
        }

        ctx->databases = dblist;
        init_bkp_ctx(ctx);

        return ctx;
}

/*#***********************************************************************\
 *
 *              free_bkp_ctx
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void free_bkp_ctx(BKP_CTX* ctx)
{
        if (ctx) {
            SAFE_DELETE(ctx->databases);
            SsMemFree(ctx);
        }
}

/* #endif */ /* SOLIDDB_BACKUP_NEW */

/* #ifndef MYSQL_DYNAMIC_PLUGIN */
/*#***********************************************************************\
 *
 *              bkp_show_status
 *
 * Show current status of the backup
 *
 * Parameters :
 *
 *      BKP_CTX*  ctx, in
 *
 * Return value :
 *
 * Globals used :
 */
void bkp_show_status(BKP_CTX* ctx)
{
        MYSQL_THD   thd = current_thd;
        Protocol*   protocol = (Protocol *)thd->protocol;
        List<Item>  field_list;
        Item*       item;
        char*       status;
        double      total_size;
        double      done_size;
        uint32      done;
        char*       directory;
        char*       opt_myisam;
        char*       opt_system;
        char*       opt_config;
        char*       opt_dempty;

        // Hack to avoid loosing a connection (see also
        // sql_parse.cpp: mysql_execute_command(): case SQLCOM_SET_OPTION)
/*        thd->net.no_send_ok = 1; */

        SsFlatMutexLock(soliddb_backup_mutex);
        status  = ctx->abort  ? (char*)"ABORTED" :
                  ctx->killed ? (char*)"KILLED"  :
                  ctx->failed ? (char*)"FAILED"  :
                  ctx->active ? (char*)"ACTIVE"  :
                                (char*)"FINISHED";
        total_size  = ctx->total_size;
        done_size   = ctx->done_size;
        directory   = ctx->dir;
        opt_myisam  = ctx->opt_myisam ? (char*)"ON" : (char*)"OFF";
        opt_system  = ctx->opt_system ? (char*)"ON" : (char*)"OFF";
        opt_config  = ctx->opt_config ? (char*)"ON" : (char*)"OFF";
        opt_dempty  = ctx->opt_dempty ? (char*)"ON" : (char*)"OFF";
        SsFlatMutexUnlock(soliddb_backup_mutex);
        done = (uint32) ((total_size ? done_size / total_size : 0) * 100);

/*
        field_list.push_back(item = new Item_empty_string("status", NAME_LEN * 2));
        item->maybe_null = 1;
        field_list.push_back(new Item_return_int("total size", 7, FIELD_TYPE_LONGLONG));
        item->maybe_null = 1;
        field_list.push_back(new Item_return_int("done size", 7, FIELD_TYPE_LONGLONG));
        item->maybe_null = 1;
        field_list.push_back(new Item_return_int("%% done", 7, FIELD_TYPE_LONG));
        item->maybe_null = 1;
        field_list.push_back(item = new Item_empty_string("directory", NAME_LEN * 2));
        item->maybe_null = 1;
        field_list.push_back(item = new Item_empty_string("empty dir", NAME_LEN * 2));
        item->maybe_null = 1;
        field_list.push_back(item = new Item_empty_string("myisam", NAME_LEN * 2));
        item->maybe_null = 1;
        field_list.push_back(item = new Item_empty_string("system", NAME_LEN * 2));
        item->maybe_null = 1;
        protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);
*/

        protocol->prepare_for_resend();
        protocol->store(status, system_charset_info);
        protocol->store((ulonglong) total_size);
        protocol->store((ulonglong) done_size);
        protocol->store(done);
        protocol->store(directory, system_charset_info);
        protocol->store(opt_dempty, system_charset_info);
        protocol->store(opt_myisam, system_charset_info);
        protocol->store(opt_system, system_charset_info);
        protocol->write();
/*         send_eof(thd); */
}

su_ret_t  bkp_add_table(BKP_DBASE*, FILEINFO*);
su_ret_t  bkp_add_dbase(BKP_CTX*, FILEINFO*);

/*#***********************************************************************\
 *
 *              prepare_bkp
 *
 * Show current status of the backup
 *
 * Parameters :
 *
 *      BKP_CTX*    ctx, out
 *      rs_sysi_t*  cd
 *
 * Return value :   solidDB return code
 *
 * Globals used :
 */
su_ret_t prepare_bkp(BKP_CTX* ctx, rs_sysi_t* cd)
{
        dbe_db_t*   solid_db = (dbe_db_t*) rs_sysi_db(cd);
        MY_DIR*     files;
        su_ret_t    rc = SU_SUCCESS;

        if (!(files = my_dir(mysql_data_home, MYF(MY_DONT_SORT | MY_WANT_STAT)))) {
            bkp_show_error("prepare_bkp() failed", true);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        for (uint i = 0; i < files->number_off_files; i++) {

            FILEINFO* dirinfo = &files->dir_entry[i];

            if (!MY_S_ISDIR(dirinfo->mystat->st_mode) ||
                !SsStrcmp(dirinfo->name, ".") ||
                !SsStrcmp(dirinfo->name, "..") ||
                !SsStricmp(dirinfo->name, "mysql") && !ctx->opt_system) {
                continue;
            }

            if ((rc = bkp_add_dbase(ctx, dirinfo)) != SU_SUCCESS) {
                goto FINISH;
            }
        }

        for (POSITION pos = ctx->databases->first(); pos; ) {

            BKP_DBASE* dbase = get_dbase_at(pos);

            if (dbase->size == 0) {
                pos = ctx->databases->del_and_next(pos);
            } else {
                ctx->total_size =+ dbase->size;
                pos = ctx->databases->next(pos);
            }
        }

        ctx->total_size += dbe_db_getdbsize(solid_db) * 1024 +
                           dbe_db_getlogsize(solid_db) * 1024;

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkb_add_dbase
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
su_ret_t bkp_add_dbase(BKP_CTX* ctx, FILEINFO* dirinfo)
{
        CList*      dbases = ctx->databases;
        char        dbpath[FN_REFLEN];
        MY_DIR*     files;
        BKP_DBASE*  dbase;
        POSITION    pos;
        bool        opt_myisam;
        su_ret_t    rc = SU_SUCCESS;

        if (!(pos = dbases->add(dirinfo->name))) {
            bkp_show_error("bkp_add_table() failed [1]", true);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        dbase = get_dbase_at(pos);
        dbase->ctx = ctx;

        strxmov(dbpath, mysql_data_home, "/", dbase->name, NULL);
        if (!(files = my_dir(dbpath, MYF(MY_DONT_SORT | MY_WANT_STAT)))) {
            bkp_show_error("bkp_add_table() failed [2]", true);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        for (uint i = 0; i < files->number_off_files; i++) {

            FILEINFO* fileinfo = &files->dir_entry[i];

            if (MY_S_ISDIR(fileinfo->mystat->st_mode) ||
                !IS_FILE_FOR_BACKUP(fileinfo->name)) {
                continue;
            }

            if ((rc = bkp_add_table(dbase, fileinfo)) != SU_SUCCESS) {
                goto FINISH;
            }
        }

       opt_myisam = SsStricmp(dbase->name, "mysql") ? ctx->opt_myisam : true;

       for (pos = dbase->tables->first(); pos; ) {

            BKP_TABLE* table = get_table_at(pos);

            if (table->size == 0 ||
                !IS_TYPE_FOR_BACKUP(table->type) ||
                table->type == DB_TYPE_MYISAM && !opt_myisam) {
                pos = dbase->tables->del_and_next(pos);
            } else {
                dbase->size += table->size;
                pos = dbase->tables->next(pos);
            }

        }

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkb_add_table
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
su_ret_t bkp_add_table(BKP_DBASE* dbase, FILEINFO* fileinfo)
{
        char*       filename = fileinfo->name;
        char        tablename[FN_REFLEN];
        char        trigname[FN_REFLEN];
        POSITION    pos;
        BKP_TABLE*  table;
        su_ret_t    rc = SU_SUCCESS;

        if (IS_TRN_FILE(filename)) {
            strncpy(trigname, filename, strlen(filename) - 4);
            trigname[strlen(filename) - 4] = '\0';
            get_triggers_table(dbase->name, trigname, tablename);
        } else {
            strncpy(tablename, filename, strlen(filename) - 4);
            tablename[strlen(filename) - 4] = '\0';
            trigname[0] = '\0';
        }

        if ((pos = table_list_search(dbase->tables, tablename))) {
            table = get_table_at(pos);
        } else if ((pos = dbase->tables->add(tablename))) {
            table = get_table_at(pos);
            table->dbase = dbase;
            table->type = define_table_type(dbase->name, tablename);
        } else {
            bkp_show_error("bkp_add_table() failed", true);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        table->size += fileinfo->mystat->st_size;

        if (*trigname) {
            table->triggers->add(trigname);
        }

FINISH:
        return rc;
}

su_ret_t  bkp_do_abort  (BKP_CTX* ctx);
su_ret_t  bkp_do_start  (BKP_CTX* ctx, rs_sysi_t*, BKP_PARAMS*, su_err_t**);
su_ret_t  bkp_do_status (BKP_CTX* ctx);
su_ret_t  bkp_do_wait   (BKP_CTX* ctx);

/*#***********************************************************************\
 *
 *              do_backup
 *
 * Execute solidDB 'BACKUP paramters' command
 *
 * Parameters :
 *
 *      rs_sysi_t*  cd, in, use
 *      char*       parmstr,  in, use
 *      su_err_t**  p_errh, in out, NULL
 *
 * Return value :
 *
 *      solidDB return code
 *
 * Globals used :
 *
 *      BKP_CTX*  bkp_ctx
 *
 * Added _old temporarily
 */
su_ret_t do_backup_old(rs_sysi_t* cd, char* parmstr, su_err_t** p_errh)
{
        BKP_PARAMS  parmdata;
        su_ret_t    rc = SU_SUCCESS;

        if (!bkp_ctx) {
            bkp_show_error("Backup context is not created", true);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        bkp_parse_params(parmstr, &parmdata);

        switch (parmdata.cmd) {
        case BKP_CMD_ABORT:
            rc = bkp_do_abort(bkp_ctx);
            break;
        case BKP_CMD_START:
            rc = bkp_do_start(bkp_ctx, cd, &parmdata, p_errh);
            break;
        case BKP_CMD_STATUS:
            rc = bkp_do_status(bkp_ctx);
            break;
        case BKP_CMD_WAIT:
            rc = bkp_do_wait(bkp_ctx);
            break;
        case BKP_CMD_UNDEF:
        default:
            rc = DBE_ERR_FAILED;
            break;
        }

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_do_status
 *
 * Execute solidDB 'BACKUP -STATUS' command
 *
 * Parameters :
 *
 *     BKP_CTX*   ctx, in
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_do_status(BKP_CTX* ctx)
{
        bkp_show_status(ctx);
        return SU_SUCCESS;
}

/*#***********************************************************************\
 *
 *              bkp_do_abort
 *
 * Execute solidDB 'BACKUP -ABORT' command
 *
 * Parameters :
 *
 *     BKP_CTX*   ctx, in
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_do_abort(BKP_CTX* ctx)
{
        bool active;

        SsFlatMutexLock(soliddb_backup_mutex);
        active = ctx->active;
        if (active) {
            ctx->abort = true;
        }
        SsFlatMutexUnlock(soliddb_backup_mutex);

        while (active) {
            SsThrSleep(20);
            SsFlatMutexLock(soliddb_backup_mutex);
            active = ctx->active;
            SsFlatMutexUnlock(soliddb_backup_mutex);
        }

        bkp_show_status(ctx);
        return SU_SUCCESS;
}

/*#***********************************************************************\
 *
 *              bkp_do_wait
 *
 * Execute solidDB 'BACKUP -WAIT' command
 *
 * Parameters :
 *
 *     BKP_CTX*   ctx, in
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_do_wait(BKP_CTX* ctx)
{
        bool active = true;

        while (active) {
            SsFlatMutexLock(soliddb_backup_mutex);
            active = ctx->active;
            SsFlatMutexUnlock(soliddb_backup_mutex);
            SsThrSleep(20);
        }

        bkp_show_status(ctx);
        return SU_SUCCESS;
}

pthread_handler_t   bkp_thread_fn(void*);
void                bkp_set_start_options(BKP_CTX*, BKP_PARAMS*);
su_ret_t            bkp_set_dst_directory(BKP_CTX*, BKP_PARAMS*);

/*#***********************************************************************\
 *
 *              bkp_do_start
 *
 * Execute solidDB 'BACKUP start-parameters' command
 *
 * Parameters :
 *
 *      BKP_CTX*      ctx, in
 *      rs_sysi_t*    cd, in
 *      BKP_PARAMS*   parmdata, in
 *      su_err_t**    err, in
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_do_start(BKP_CTX* ctx, rs_sysi_t* cd, BKP_PARAMS* parmdata, su_err_t** err)
{
        pthread_t backup_thread;
        su_ret_t rc = SU_SUCCESS;

        SsFlatMutexLock(soliddb_backup_mutex);
        if (ctx->active) {
            SsFlatMutexUnlock(soliddb_backup_mutex);
            bkp_show_error("Backup can not be started because it is active currently");
            return DBE_ERR_FAILED;
        }
        SsFlatMutexUnlock(soliddb_backup_mutex);

        block_solid_DDL();

        init_bkp_ctx(ctx);
        ctx->cd = cd;
        ctx->p_errh = err;

        bkp_set_start_options(ctx, parmdata);

        if ((rc = bkp_set_dst_directory(ctx, parmdata)) != SU_SUCCESS) {
            goto FAILURE;
        }

        if ((rc = prepare_bkp(ctx, cd)) != SU_SUCCESS) {
            goto FAILURE;
        }

        SsFlatMutexLock(soliddb_backup_mutex);
        ctx->active = true;
        SsFlatMutexUnlock(soliddb_backup_mutex);

        if (pthread_create(&backup_thread, &connection_attrib, bkp_thread_fn, ctx)) {
            bkp_show_error("Failed to start backup thread", true);
            rc = DBE_ERR_FAILED;
            goto FAILURE;
        }

        bkp_show_status(ctx);
        return rc;

FAILURE:
        unblock_solid_DDL();
        SsFlatMutexLock(soliddb_backup_mutex);
        ctx->active = false;
        ctx->failed = true;
        SsFlatMutexUnlock(soliddb_backup_mutex);
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_set_start_options
 *
 * Set backup start options using info from backup start-parameters and
 * from system variables values.
 *
 * Parameters :
 *
 *      BKP_CTX*      ctx, in, out
 *      BKP_PARAMS*   data, in
 *
 * Return value : solidDB return code
 */
void bkp_set_start_options(BKP_CTX* ctx, BKP_PARAMS* data)
{
        if (data->opt_myisam) {
            ctx->opt_myisam = data->opt_myisam >> 1;
        } else {
            ctx->opt_myisam = soliddb_backup_myisam;
        }
        if (data->opt_system) {
            ctx->opt_system = data->opt_system >> 1;
        } else {
            ctx->opt_system = soliddb_backup_system;
        }
        if (data->opt_config) {
            ctx->opt_config = data->opt_config >> 1;
        } else {
            ctx->opt_config = soliddb_backup_config;
        }
        if (data->opt_dempty) {
            ctx->opt_dempty = data->opt_dempty >> 1;
        } else {
            ctx->opt_dempty = soliddb_backup_emptydir;
        }
}

bool bkp_is_dir_empty(const char* path);

/*#***********************************************************************\
 *
 *              bkp_set_dst_directory
 *
 * Set backup destination directory using info from backup start-parameters
 * and from system variables values. Check the directory to be empty or set
 * it entry according to options settings.
 *
 * Parameters :
 *
 *      BKP_CTX*      ctx, in, out
 *      BKP_PARAMS*   data, in
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_set_dst_directory(BKP_CTX* ctx, BKP_PARAMS* parmdata)
{
        dbe_db_t* soliddb = (dbe_db_t*)rs_sysi_db(ctx->cd);
        char      errmsg[FN_REFLEN + 64];
        su_ret_t  rc = SU_SUCCESS;

        // Set directory to be used for backup
        if (parmdata->dir && *parmdata->dir) {
            strcpy(ctx->dir, parmdata->dir);
        } else if (soliddb_backupdir && *soliddb_backupdir) {
            SsFlatMutexLock(soliddb_backupdir_mutex);
            strcpy(ctx->dir, soliddb_backupdir);
            SsFlatMutexUnlock(soliddb_backupdir_mutex);
        } else {
            bkp_show_error("Backup directory is not defined");
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        // Check that backup directory really exists
        rc = dbe_db_backupcheck(soliddb, ctx->dir, ctx->p_errh);

        if (rc != DBE_RC_SUCC) {
            bkp_show_error(su_err_geterrstr(*ctx->p_errh));
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        if (ctx->opt_dempty) {
            if (solid_clear_dir(ctx->dir)) {
                sprintf(errmsg, "Failed to empty the backup directory '%s'", ctx->dir);
                bkp_show_error(errmsg);
                rc = DBE_ERR_FAILED;
                goto FINISH;
            }
        } else if (!bkp_is_dir_empty(ctx->dir)) {
            sprintf(errmsg, "The backup directory '%s' is not empty", ctx->dir);
            bkp_show_error(errmsg);
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_is_dir_empty
 *
 * Is directory is empty?
 *
 * Parameters :
 *
 *      const char*   path, in
 *
 * Return value :     bool
 */
bool bkp_is_dir_empty(const char* path)
{
        MY_DIR* files = my_dir(path, MYF(MY_DONT_SORT | MY_WANT_STAT));

        for (uint i = 0; i < files->number_off_files; i++) {

            FILEINFO* dirinfo = &files->dir_entry[i];

            if (!MY_S_ISDIR(dirinfo->mystat->st_mode) ||
                !SsStrcmp(dirinfo->name, ".") ||
                !SsStrcmp(dirinfo->name, "..")) {
                continue;
            }

            return false;
        }

        return true;
}

MYSQL_THD bkp_open_thd_for_query();
void      bkp_close_thd_for_query(MYSQL_THD thd);
su_ret_t  bkp_exec_query(MYSQL_THD thd, const char* dbname, const char* query);
su_ret_t  bkp_exec_lock_table(MYSQL_THD thd, const char* dbname, const char* tablename);
su_ret_t  bkp_exec_unlock_tables(MYSQL_THD thd);
su_ret_t  bkp_dbfile(BKP_CTX* ctx);
su_ret_t  bkp_dbase(BKP_DBASE* dbase);
su_ret_t  bkp_table(BKP_TABLE* table);
su_ret_t  bkp_triggers(BKP_TABLE* table);
su_ret_t  bkp_soliddb(BKP_CTX* ctx);
su_ret_t  bkp_config(BKP_CTX* ctx);

su_ret_t  bkp_file(
          const char* dbname,
          const char* tablename,
          const char* extension,
          const char* dst_dir);

/*#***********************************************************************\
 *
 *              bkb_killed_or_aborted
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
bool bkp_killed_or_aborted(BKP_CTX* ctx)
{
        bool res;

        SsThrSleep(20);
        SsFlatMutexLock(soliddb_backup_mutex);
        ctx->killed = ctx->thd->killed;
        res = ctx->abort || ctx->killed;
        SsFlatMutexUnlock(soliddb_backup_mutex);

        return res;
}

/*#***********************************************************************\
 *
 *              bkp_thread_fn
 *
 * Backup thread function
 *
 * Parameters :
 *
 *      void*   arg, in
 *              actual argument is the backup context (BKP_CTX*)
 *
 * Return value :
 */
pthread_handler_t bkp_thread_fn(void* arg)
{
        BKP_CTX*  ctx = (BKP_CTX*) arg;
        POSITION  pos;
        su_ret_t  rc = SU_SUCCESS;

        if (my_thread_init()) {
            goto FINISH;
        }

        ctx->thd = bkp_open_thd_for_query();

        if (!ctx->thd) {
            goto FINISH;
        }

        for (pos = ctx->databases->first(); pos; pos = ctx->databases->next(pos)) {
            if ((rc = bkp_dbase(get_dbase_at(pos))) != SU_SUCCESS) {
                goto FINISH;
            }
        }

        if ((rc = bkp_soliddb(ctx)) != SU_SUCCESS) {
            goto FINISH;
        }

        if ((rc = bkp_config(ctx)) != SU_SUCCESS) {
            goto FINISH;
        }

FINISH:
        unblock_solid_DDL();
        SAFE_FREE(ctx->thd, bkp_close_thd_for_query);

        SsFlatMutexLock(soliddb_backup_mutex);
        ctx->active = false;
        SsFlatMutexUnlock(soliddb_backup_mutex);

        my_thread_end();
        pthread_exit(0);

        return 0;
}

/*#***********************************************************************\
 *
 *              bkp_soliddb
 *
 * Parameters :
 *
 *      BKP_CTX*  ctx, in, out
 *
 * Return value : solidDB return code
 */
su_ret_t bkp_soliddb(BKP_CTX* ctx)
{
        dbe_db_t*     soliddb = (dbe_db_t*)rs_sysi_db(ctx->cd);
        dbe_cfg_t*    cfg = soliddb->db_go->go_cfg;
        tb_connect_t* tc;
        double        before_size;
        double        file_size;
        long          block_size;
        double        count_size;
        su_ret_t      rc = SU_SUCCESS;

        before_size = ctx->done_size;
        file_size   = ((double) dbe_db_getdbsize(soliddb)) * 1024 +
                      ((double) dbe_db_getlogsize(soliddb)) * 1024;
        dbe_cfg_getbackup_blocksize(cfg, &block_size);
        count_size = 0;

        tc = tb_sysconnect_init((tb_database_t*)rs_sysi_tabdb(ctx->cd));
        rc = tb_createcheckpoint(tc, /*splitlog = */ TRUE);

        if (rc != DBE_RC_SUCC) {
            su_err_init(ctx->p_errh, rc);
        }

        tb_sysconnect_done(tc);

        if (rc != DBE_RC_SUCC) {
            goto FINISH;
        }

        rc = dbe_db_backupstart(soliddb, ctx->dir, FALSE, ctx->p_errh);

        if (rc != DBE_RC_SUCC) {
            return rc;
        }

        while (!bkp_killed_or_aborted(ctx)) {

            rc = dbe_db_backupadvance(soliddb, ctx->p_errh);
            if (rc != DBE_RC_CONT) {
                break;
            }

            if ((count_size += (double) block_size) > file_size) {
                count_size = file_size;
            }
            SsFlatMutexLock(soliddb_backup_mutex);
            if ((ctx->done_size = before_size + count_size) > ctx->total_size) {
                ctx->done_size = ctx->total_size;
            }
            SsFlatMutexUnlock(soliddb_backup_mutex);
        }

        dbe_db_backupstop(soliddb);

        if (rc == DBE_RC_END && count_size < file_size) {
            SsFlatMutexLock(soliddb_backup_mutex);
            if ((ctx->done_size = before_size + file_size) > ctx->total_size) {
                ctx->done_size = ctx->total_size;
            }
            SsFlatMutexUnlock(soliddb_backup_mutex);
        }

FINISH:
        return (rc == DBE_RC_END) ? SU_SUCCESS : DBE_ERR_FAILED;
}

/*#***********************************************************************\
 *
 *              bkp_config
 *
 * Back up option (configuration) file
 *
 * Parameters :
 *
 *      BKP_CTX*    ctx, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_config(BKP_CTX* ctx)
{
        su_ret_t  rc = SU_SUCCESS;

        if (!ctx->opt_config) {
            goto FINISH;
        }

        // TODO: Implement

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_dbase
 *
 * Back up database (backup all tables from the database)
 *
 * Parameters :
 *
 *      BKP_DBASE*  dbase, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_dbase(BKP_DBASE* dbase)
{
        char      dstdir[FN_REFLEN];
        POSITION  pos;
        su_ret_t  rc = SU_SUCCESS;

        strxmov(dstdir, dbase->ctx->dir, "/", dbase->name, NULL);

        if (my_mkdir(dstdir, 0777, MYF(0))) {
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        for (pos = dbase->tables->first(); pos; pos = dbase->tables->next(pos)) {
            if (bkp_killed_or_aborted(dbase->ctx)) {
                goto FINISH;
            }
            if ((rc = bkp_table(get_table_at(pos))) != SU_SUCCESS) {
                goto FINISH;
            }
        }

FINISH:
        return rc;
}

su_ret_t  bkp_myisam_table  (BKP_TABLE*);
su_ret_t  bkp_soliddb_table (BKP_TABLE*);

/*#***********************************************************************\
 *
 *              bkp_table
 *
 * Back up table
 *
 * Parameters :
 *
 *      BKP_TABLE*  table, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_table(BKP_TABLE* table)
{
        su_ret_t  rc = SU_SUCCESS;

        switch (table->type) {
        case DB_TYPE_MYISAM:
            rc = bkp_myisam_table(table);
            break;
#if MYSQL_VERSION_ID < 50100
        case DB_TYPE_SOLID_DB:
#else
        case DB_TYPE_SOLID:
#endif
            rc = bkp_soliddb_table(table);
            break;
        default:
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        if ((rc = bkp_triggers(table)) != SU_SUCCESS) {
            goto FINISH;
        }

FINISH:
        SsFlatMutexLock(soliddb_backup_mutex);
        table->dbase->ctx->done_size += table->size;
        table->failed = (rc != SU_SUCCESS);
        SsFlatMutexUnlock(soliddb_backup_mutex);
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_myisam_table
 *
 * Back up MyISAM table
 *
 * Parameters :
 *
 *      BKP_TABLE*  table, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_myisam_table(BKP_TABLE* table)
{
        char*     dbasename = table->dbase->name;
        char      dstdir[FN_REFLEN];
        MYSQL_THD thd = table->dbase->ctx->thd;
        su_ret_t  rc = SU_SUCCESS;

        strxmov(dstdir, table->dbase->ctx->dir, "/", dbasename, NULL);

        if ((rc = bkp_exec_lock_table(thd, dbasename, table->name)) != SU_SUCCESS) {
            goto FINISH;
        }

        if ((rc = bkp_file(dbasename, table->name, ".frm", dstdir)) != SU_SUCCESS) {
            goto FINISH;
        }
        if ((rc = bkp_file(dbasename, table->name, ".MYD", dstdir)) != SU_SUCCESS) {
            goto FINISH;
        }
        if ((rc = bkp_file(dbasename, table->name, ".MYI", dstdir)) != SU_SUCCESS) {
            goto FINISH;
        }

FINISH:
        bkp_exec_unlock_tables(thd);
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_soliddb_table
 *
 * Back up solidDB table
 *
 * Parameters :
 *
 *      BKP_TABLE*  table, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_soliddb_table(BKP_TABLE* table)
{
        char*     dbasename = table->dbase->name;
        char      dstdir[FN_REFLEN];
        su_ret_t  rc = SU_SUCCESS;

        strxmov(dstdir, table->dbase->ctx->dir, "/", dbasename, NULL);

        if ((rc = bkp_file(dbasename, table->name, ".frm", dstdir)) != SU_SUCCESS) {
            goto FINISH;
        }

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_triggers
 *
 * Back up triggers associated with the table
 *
 * Parameters :
 *
 *      BKP_TABLE*  table, in, out
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_triggers(BKP_TABLE* table)
{
        char*       dbasename = table->dbase->name;
        char        dstdir[FN_REFLEN];
        POSITION    pos;
        su_ret_t    rc = SU_SUCCESS;

        if (table->triggers->is_empty()) {
            goto FINISH;
        }

        strxmov(dstdir, table->dbase->ctx->dir, "/", dbasename, NULL);

        if ((rc = bkp_file(dbasename, table->name, ".TRG", dstdir)) != SU_SUCCESS) {
            goto FINISH;
        }

        for (pos = table->triggers->first(); pos; pos = table->triggers->next(pos)) {
            char* trigname = get_string_at(pos);
            if ((rc = bkp_file(dbasename, trigname, ".TRN", dstdir)) != SU_SUCCESS) {
                goto FINISH;
            }
        }

FINISH:
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_file
 *
 * Copy file
 *
 * Parameters :
 *
 *      const char*   dbname, in
 *      const char*   tablename, in
 *      const char*   extension, in
 *      const char*   dst_dir, in
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_file(
        const char*   dbname,
        const char*   tablename,
        const char*   extension,
        const char*   dst_dir)
{
        char  src_path[FN_REFLEN];
        char  dst_path[FN_REFLEN];
        File  src_file = (File) -1;
        File  dst_file = (File) -1;
        SS_MYSQL_ROW  buffer[2048];
        uint  cnt;
        su_ret_t rc = SU_SUCCESS;

        strxmov(src_path, mysql_data_home, "/", dbname, "/", tablename, extension, NULL);
        strxmov(dst_path, dst_dir, "/", tablename, extension, NULL);

        if ((src_file = my_open(src_path, O_RDONLY | O_SHARE, MYF(0))) < 0) {
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        if ((dst_file = my_open(dst_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, MYF(0))) < 0) {
            rc = DBE_ERR_FAILED;
            goto FINISH;
        }

        while ((cnt = my_read(src_file, buffer, sizeof(buffer), MYF(MY_FAE)))) {
            if ((int)cnt < 0 || my_write(dst_file, buffer, cnt, MYF(0)) != cnt) {
                rc = DBE_ERR_FAILED;
                goto FINISH;
            }
        }

FINISH:
        if (src_file >= 0) {
            my_close(src_file, MYF(MY_WME));
        }

        if (dst_file >= 0) {
            my_close(dst_file, MYF(MY_WME));
        }

        return rc;
}

/************************************************************************
 *
 *              Executing MySQL queries
 *
 * Currently, this is needed to execute LOCK/UNLOCK TABLES.
 * NOTE. This part is implemented similar to slave.cpp and probably
 * should be worked out more carefully.
*/

#define BACKUP_NET_TIMEOUT  3600

MYSQL_THD bkp_open_thd_for_query()
{
        MYSQL_THD thd = new THD;

        if (!thd) {
            return NULL;
        }

        thd->thread_stack = (char*)&thd;
        thd->security_ctx->skip_grants();
        my_net_init(&thd->net, 0);
        thd->net.read_timeout = BACKUP_NET_TIMEOUT;
        thd->real_id = pthread_self();

        /* pthread_mutex_lock(&LOCK_thread_count); */
        mysql_mutex_lock(&LOCK_thread_count); 
        thd->thread_id = thread_id++;
        /* pthread_mutex_unlock(&LOCK_thread_count);  */
        mysql_mutex_unlock(&LOCK_thread_count); 

        if (init_thr_lock() || thd->store_globals()) {
            thd->cleanup();
            delete thd;
            return NULL;
        }

#if !defined(__WIN__)
        sigset_t set;
        VOID(sigemptyset(&set));
  #if MYSQL_VERSION_ID < 50100
        VOID(pthread_sigmask(SIG_UNBLOCK, &set, &thd->block_signals));
  #else
        VOID(pthread_sigmask(SIG_UNBLOCK, &set, &thd->signals));
  #endif
#endif

        thd->proc_info= "Running backup thread";
/*        thd->version = refresh_version; */
        thd->set_time();

        thd->init_for_queries();

        /* pthread_mutex_lock(&LOCK_thread_count); */
        mysql_mutex_lock(&LOCK_thread_count); 
        threads.append(thd);
        /* pthread_mutex_unlock(&LOCK_thread_count); */
        mysql_mutex_unlock(&LOCK_thread_count);

        return thd;
}

/*#***********************************************************************\
 *
 *              bkb_close_thd_for_query
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
void bkp_close_thd_for_query(MYSQL_THD thd)
{
        if (!thd) {
            return;
        }

        thd->db = NULL;
        thd->db_length = 0;

        delete thd;
}

/*#***********************************************************************\
 *
 *              bkp_exec_query
 *
 * TODO: add description
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
su_ret_t bkp_exec_query(
        MYSQL_THD thd,
        const char* dbname,
        const char* query)
{
        su_ret_t  rc  = SU_SUCCESS;

        thd->db = (char*) dbname;
        thd->db_length = dbname ? strlen(dbname) : 0;

	thd->set_query((char*) query, query? strlen(query) : 0);

/*
        thd->query = (char*) query;
        thd->query_length = query ? strlen(query) : 0;

        thd->query_error = 0; 
        thd->net.no_send_ok = 1;
*/

        /* VOID(pthread_mutex_lock(&LOCK_thread_count)); */
        VOID(mysql_mutex_lock(&LOCK_thread_count));
#if MYSQL_VERSION_ID < 50100
        thd->query_id = next_query_id();
#endif
        /* VOID(pthread_mutex_unlock(&LOCK_thread_count)); */
        VOID(mysql_mutex_unlock(&LOCK_thread_count));

#if MYSQL_VERSION_ID < 50045
        mysql_parse(thd, thd->query, thd->query_length);
#else
        mysql_parse(thd, thd->query(), thd->query_length(), NULL);
#endif

/*
        if (thd->query_error) {
            rc = DBE_ERR_FAILED;
        }
*/

	if (thd->is_error()) {
            rc = DBE_ERR_FAILED;
        }

        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_exec_lock_table
 *
 * Execute query: LOCK TABLE <table> READ
 *
 * Parameters :
 *
 *      const char*   dbname, in
 *      const char*   tablename, in
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_exec_lock_table(
        MYSQL_THD thd,
        const char* dbname, const
        char* tablename)
{
        char* query;
        su_ret_t rc = SU_SUCCESS;

        if (!(query = (char*) SsMemAlloc(strlen(tablename) + 18))) {
            return DBE_ERR_FAILED;
        }

        sprintf(query, "LOCK TABLE %s READ", tablename);
        rc = bkp_exec_query(thd, dbname, query);

        SsMemFree(query);
        return rc;
}

/*#***********************************************************************\
 *
 *              bkp_exec_unlock_tables
 *
 * Execute query: UNLOCK TABLES
 *
 * Parameters :
 *
 * Return value :   solidDB return code
 */
su_ret_t bkp_exec_unlock_tables(MYSQL_THD thd)
{
        return bkp_exec_query(thd, NULL, "UNLOCK TABLES");
}

#ifdef SS_DEBUG
static su_ret_t do_unittests( rs_sysi_t* cd, char* parameters, su_err_t** p_errh )
{
    *p_errh = NULL;
    return SU_SUCCESS;
}
#endif // SS_DEBUG


/* #endif */ /*  MYSQL_DYNAMIC_PLUGIN   MYSQL_VERSION_ID < 50100 *** REMOVE BACKUP *** */

/************************************************************************\
 *
 *              Collations specific routines
 *
 ************************************************************************/

#define BYTE_TRAILING_1     0x00    /* latin1, binary charsets and  */
#define BYTE_TRAILING_2     0x20    /* '_bin' collations            */
#define WORD_TRAILING_1     0x0000  /* utf8 collations              */
#define WORD_TRAILING_2     0x2000
#define WORD_TRAILING_3     0x0902  /* utf8_swedish_ci              */
#define WORD_TRAILING_4     0x2020  /* ucs2                         */

/*#***********************************************************************\
 *
 *              soliddb_collation_general_trimright_weightstr
 *
 * Description: this routine trims trailing zeros from weight string
 *
 * Parameters :
 *
 *  su_collation_t* coll,           [in]        collation
 *  void*           str,            [in]        weight string
 *  size_t*         p_str_bytelen,  [in, out]   weight string size in bytes
 *
 * Return value :   void
 *
 * Globals used :
*/
static void soliddb_collation_general_trimright_weightstr(
        su_collation_t* coll,   /* [in] */
        void* str,              /* [in] */
        size_t* p_str_bytelen)  /* [in, out] */
{
    ss_byte_t*    bytes = (ss_byte_t*)str;
    ss_byte_t*    bytes_end;
    ss_uint2_t*   words = (ss_uint2_t*)str;
    ss_uint2_t*   words_end;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;

    if (!coll || !str || !p_str_bytelen || *p_str_bytelen <= 0) {
        return;
    }

    /*
        0. 8 bits charsets:
            trailing: 0x00 || 0x20

        1. my_strnxfrm_utf8
            trailing: 0x00 0x20

        2. my_strnxfrm_mb_bin (utf8_bin)
            trailing: 0x00 0x00

        3. my_strnxfrm_uca (utf8_swedish_ci)
            trailing: 0x02 0x09

        4. my_strnxfrm_ucs2 (ucs2 collations)
            trailing: 0x20 0x20
    */

    if (strcmp(csinfo->csname, "latin1") == 0 ||
        strcmp(csinfo->csname, "binary") == 0 ||
        strstr(csinfo->name, "_bin")) {
        bytes_end = bytes + *p_str_bytelen;

        while (bytes_end > bytes) {
            --bytes_end;
            if (*bytes_end != BYTE_TRAILING_1 &&
                *bytes_end != BYTE_TRAILING_2) { /* order is significant*/
                ++bytes_end;
                break;
            }
        }

        *p_str_bytelen = (size_t)(bytes_end - bytes);
    } else {
        /*
        utf8_general_cs
        utf8_general_ci
        utf8_swedish_ci
        ucs2_<collation>
        */

        *p_str_bytelen /= 2;

        words_end = words + *p_str_bytelen;

        while (words_end > words) {
            --words_end;

            if (*words_end != WORD_TRAILING_1 &&
                *words_end != WORD_TRAILING_2 &&
                *words_end != WORD_TRAILING_3 &&
                *words_end != WORD_TRAILING_4) {
                ++words_end;
                break;
            }
        }

        *p_str_bytelen = (size_t)(words_end - words) * 2;
    }
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_donebuf
 *
 * Description: this routine frees collation's internal data
 *
 * Parameters :
 *
 *  su_collation_t* coll    [in]    collation
 *
 * Return value :   void
 *
 * Globals used :
 */
static void soliddb_collation_general_donebuf(
        su_collation_t* coll)   /* [in] */
{
    ss_dprintf_1(("soliddb_collation_general_donebuf\n"));

    if (coll->coll_name != NULL) {
        ss_dassert(coll->coll_data != NULL);
        coll->coll_name = NULL;
        coll->coll_data = NULL;
    }
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_get_bytes_in_chars
 *
 * Description: this routine calculates the size in bytes of the corresponding
 *              number of characters
 *
 * Parameters :
 *
 *  su_collation_t* coll        [in]    collation
 *  void*           str         [in]    source string
 *  size_t          str_bytelen [in]    string size in bytes
 *  size_t          str_charlen [in]    the length of the string prefix to calculate
 *
 * Return value :   size_t
 *
 *  size in bytes of the corresponding number of characters
 *
 * Globals used :
 */
static size_t soliddb_collation_general_get_bytes_in_chars(
        su_collation_t* coll,   /* [in] */
        void* str,              /* [in] */
        size_t str_bytelen,     /* [in] */
        size_t str_charlen)     /* [in] */
{
    size_t     chbytes;
    ss_byte_t* bytes     = (ss_byte_t*)str;
    ss_byte_t* bytes_end = bytes + str_bytelen;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_CHARSET_HANDLER* cs_handler = csinfo->cset;

    if (!str_charlen)
        return str_bytelen;

    chbytes = (*cs_handler->charpos)(csinfo,
                                     (const char*)bytes,
                                     (const char*)bytes_end,
                                     str_charlen);

    return chbytes;
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_create_weightstr
 *
 * Description: this routine creates the weight string using str_prefix_charlen number
 *              of characters from the source string
 *
 * Parameters :
 *
 *  su_collation_t* coll                [in]    collation
 *  void*           str                 [in]    string
 *  size_t          str_bytelen         [in]    string size in bytes
 *  size_t          str_prefix_charlen  [in] if 0 ignore this parameter     prefix index length in chars
 *  void*           weightstr_buf       [out]   buffer to store wieght string
 *  size_t          weightstr_bufsize   [in]    the size of the buffer
 *  size_t*         p_weightstr_bytelen [out]   the real size of the weight string in bytes
 *
 * Return value :   solid_bool
 *
 *  TRUE    - if success
 *  FALSE   - if fails
 *
 * Globals used :
 */
static solid_bool soliddb_collation_general_create_weightstr(
        su_collation_t* coll,           /* [in] */
        void* str,                      /* [in] */
        size_t str_bytelen,             /* [in] */
        size_t str_prefix_charlen,      /* [in] */  /* if 0 ignore this parameter */
        void* weightstr_buf,            /* [out] */
        size_t weightstr_bufsize,       /* [in] */
        size_t* p_weightstr_bytelen)    /* [out] */
{
    size_t dest_len;
    uint idx_bytelen = (uint)str_bytelen;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_COLLATION_HANDLER* coll_handler = csinfo->coll;

    ss_dprintf_1(("soliddb_collation_general_create_weightstr\n"));

    if (csinfo->number == 47 ||     /* latin1_bin collation */
        csinfo->number == 63 ||     /* binary collation     */
        csinfo->number == 90) {     /* ucs2_bin             */

        if (str_prefix_charlen) {
            if (csinfo->number == 90) {
                str_prefix_charlen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
                idx_bytelen = (uint)soliddb_collation_general_get_bytes_in_chars(coll, str, str_bytelen, str_prefix_charlen);

                *p_weightstr_bytelen = idx_bytelen;
            } else {
                *p_weightstr_bytelen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
            }
        } else {
            *p_weightstr_bytelen = str_bytelen;
        }

        memcpy(weightstr_buf, str, *p_weightstr_bytelen);

        goto exit;
    }

    if (str_prefix_charlen) {
        str_prefix_charlen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
        idx_bytelen = (uint)soliddb_collation_general_get_bytes_in_chars(coll, str, str_bytelen, str_prefix_charlen);
    }

    dest_len = (*coll_handler->strnxfrm)(csinfo,
                                         (ss_byte_t*)weightstr_buf,
                                         (uint)weightstr_bufsize,
                                         (ss_byte_t*)str,
                                         (uint)idx_bytelen);

    ss_dassert(dest_len <= weightstr_bufsize);

    *p_weightstr_bytelen = dest_len;

    soliddb_collation_general_trimright_weightstr(coll, weightstr_buf, p_weightstr_bytelen);

exit:;
    if (*p_weightstr_bytelen < weightstr_bufsize) {
        ((ss_byte_t*)weightstr_buf)[*p_weightstr_bytelen] = 0x00;
        (*p_weightstr_bytelen)++;
    }

#ifdef SS_DEBUG
    {
        size_t i;
        ss_byte_t *str = (ss_byte_t *)weightstr_buf;
        ss_dprintf_1(("Generated weightstr :["));
        for(i = 0; i < *p_weightstr_bytelen; i++) {
            ss_dprintf_1(("%02x", str[i]));
        }

        ss_dprintf_1(("]\n"));
    }
#endif

    return TRUE;
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_compare
 *
 * Description: this routine compares two strings using corresponding collation rules
 *
 * Parameters :
 *
 *  su_collation_t* coll            [in]
 *  void*           str1            [in]
 *  size_t          str1_bytelen    [in]
 *  void*           str2            [in]
 *  size_t          str2_bytelen    [in]
 *
 * Return value :   int
 *
 *  < 0     - if str1 < str2
 *  == 0    - if str1 == str2
 *  >0      - if str1 > str2
 *
 * Globals used :
 */
static int soliddb_collation_general_compare(
        su_collation_t* coll,   /* [in] */
        void* str1,             /* [in] */
        size_t str1_bytelen,    /* [in] */
        void* str2,             /* [in] */
        size_t str2_bytelen)    /* [in] */
{
    int cmp;
    size_t len;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_COLLATION_HANDLER* coll_handler = csinfo->coll;

    ss_dprintf_1(("soliddb_collation_general_compare\n"));

    if (csinfo->number == 47 ||     /* latin1_bin collation */
        csinfo->number == 63 ||     /* binary collation     */
        csinfo->number == 90) {     /* ucs2_bin             */

        len = SS_MIN(str1_bytelen, str2_bytelen);
        cmp = SsMemcmp(str1, str2, len);

        if (!cmp) {
            return (str1_bytelen - str2_bytelen);
        }

        return cmp;
    }

    cmp = (*coll_handler->strnncoll)(csinfo,
                                     (ss_byte_t*)str1, (uint)str1_bytelen,
                                     (ss_byte_t*)str2, (uint)str2_bytelen,
                                     (my_bool)FALSE);
    return (cmp);
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_get_maxlen_for_weightstr
 *
 * Description: this routine calculates the max length of the weight string to be created
 *
 * Parameters :
 *
 *  su_collation_t* coll                [in]    collation
 *  void*           str                 [in]    string
 *  size_t          str_bytelen         [in]    string size in bytes
 *  size_t          str_prefix_charlen  [in] if 0 ignore this parameter     prefix index length in chars
 *
 * Return value :   size_t
 *
 *  max length of the weight string to be created
 *
 * Globals used :
 */
static size_t soliddb_collation_general_get_maxlen_for_weightstr(
        su_collation_t* coll,       /* [in] */
        void* str,                  /* [in] */
        size_t str_bytelen ,        /* [in] */
        size_t str_prefix_charlen   /* [in] */  /* if 0 ignore this parameter */)
{
    uint idx_bytelen = (uint)str_bytelen;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_COLLATION_HANDLER* coll_handler = csinfo->coll;

    ss_dprintf_1(("soliddb_collation_general_get_maxlen_for_weightstr\n"));

    if (csinfo->mbmaxlen == 1) { /* for all 8 bit charsets */

        if (str_prefix_charlen) {
            idx_bytelen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
        } else{
            idx_bytelen = str_bytelen;
        }

        return idx_bytelen;
    }

    if (str_prefix_charlen) {
        str_prefix_charlen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
        idx_bytelen = (uint)soliddb_collation_general_get_bytes_in_chars(coll, str, str_bytelen, str_prefix_charlen);
    }

    return ((size_t)(*coll_handler->strnxfrmlen)(csinfo, idx_bytelen));
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_get_charset
 *
 * Description: this routine returns the charset of the corresponding collation
 *
 * Parameters :
 *
 *  su_collation_t* coll    [in]    collation
 *
 * Return value :   su_charset_t
 *
 *  charset of the corresponding collation
 *
 * Globals used :
 */
static su_charset_t soliddb_collation_general_get_charset(su_collation_t* coll)
{
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    su_charset_t  cs     = SUC_DEFAULT;

    ss_dprintf_1(("soliddb_collation_get_charset\n"));

    if (strcmp(csinfo->csname, "binary") == 0) {
        cs = SUC_BIN;
    } else if (strcmp(csinfo->csname, "big5") == 0) {
        cs = SUC_BIG5;
    } else if (strcmp(csinfo->csname, "cp932") == 0) {
        cs = SUC_CP932;
    } else if (strcmp(csinfo->csname, "eucjpms") == 0) {
        cs = SUC_EUCJPMS;
    } else if (strcmp(csinfo->csname, "euckr") == 0) {
        cs = SUC_EUCKR;
    } else if (strcmp(csinfo->csname, "gb2312") == 0) {
        cs = SUC_GB2312;
    } else if (strcmp(csinfo->csname, "gbk") == 0) {
        cs = SUC_GBK;
    } else if (strcmp(csinfo->csname, "latin1") == 0) {
        cs = SUC_LATIN1;
    } else if (strcmp(csinfo->csname, "latin2") == 0) {
        cs = SUC_LATIN2;
    } else if (strcmp(csinfo->csname, "sjis") == 0) {
        cs = SUC_SJIS;
    } else if (strcmp(csinfo->csname, "tis620") == 0) {
        cs = SUC_TIS620;
    } else if (strcmp(csinfo->csname, "ucs2") == 0) {
        cs = SUC_UCS2;
    } else if (strcmp(csinfo->csname, "ujis") == 0) {
        cs = SUC_UJIS;
    } else if (strcmp(csinfo->csname, "utf8") == 0) {
        cs = SUC_UTF8;
    } else if (strcmp(csinfo->csname, "cp1250") == 0) {
        cs = SUC_CP1250;
    }

    return cs;
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_numcells
 *
 * Description: this routine calculates the number of characters in the string
 *
 * Parameters :
 *
 *  su_collation_t* coll    [in]    collation
 *  const char*     begin   [in]    the pointer to the string
 *  const char*     end     [in]    the pointer to the end of string
 *
 * Return value :   size_t
 *
 *  number of characters in the string
 *
 * Globals used :
 */
static size_t soliddb_collation_general_numcells(
        su_collation_t* coll,   /* [in] */
        const char* begin,      /* [in] */
        const char* end)        /* [in] */
{
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_CHARSET_HANDLER* cshandler = csinfo->cset;
    size_t num;

    ss_dprintf_1(("soliddb_collation_general_numcells\n"));
    num = (*cshandler->numcells)(csinfo, begin, end);

    return num;
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_mb_wc
 *
 * Description: this routine converts the set of charset letters into wide one
 *
 * Parameters :
 *
 *  su_collation_t*         coll        [in]        collation
 *  unsigned long*          wide_char   [in, out]   wide char
 *  const unsigned char*    begin       [in]        the pointer to the begin of string
 *  const unsigned char*    end         [in]        the ointer to the end of string
 *
 * Return value :   int
 *
 *  number of characters in the string
 *
 * Globals used :
 */
static int soliddb_collation_general_mb_wc(
        su_collation_t* coll,       /* [in] */
        unsigned long* wide_char,   /* [in, out] */
        const unsigned char* begin, /* [in] */
        const unsigned char* end)   /* [in] */
{
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_CHARSET_HANDLER* cshandler = csinfo->cset;
    int res;

    ss_dprintf_1(("soliddb_collation_general_mb_wc\n"));
    res = (*cshandler->mb_wc)(csinfo, wide_char, begin, end);

    return res;
}

/*#***********************************************************************\
 *
 *              soliddb_collation_general_initbuf
 *
 * Description: this routine inits the collation structure
 *
 * Parameters :
 *
 *  su_collation_t* coll            [in]
 *  char*           collation_name  [in]    UTF8 encoding!
 *  void*           data            [in]    collation data
 *
 * Return value :   solid_bool
 *
 *  TRUE    - if success
 *  FALSE   - if fails
 *
 * Globals used :
 */
static solid_bool soliddb_collation_general_initbuf(
        su_collation_t* coll,   /* [in] */
        char* collation_name,   /* [in] */  /* UTF8 encoding! */
        void* data)             /* [in] */
{
    CHARSET_INFO* csinfo = (CHARSET_INFO*)data;

    ss_dprintf_1(("soliddb_collation_general_initbuf\n"));

    if (coll->coll_name != NULL) {
        return (TRUE);
    }
    coll->coll_name = (char*)csinfo->name; /* note: holds reference! */
    coll->coll_data = data;

    coll->coll_initbuf                  = soliddb_collation_general_initbuf;
    coll->coll_donebuf                  = soliddb_collation_general_donebuf;
    coll->coll_compare                  = soliddb_collation_general_compare;
    coll->coll_get_maxlen_for_weightstr = soliddb_collation_general_get_maxlen_for_weightstr;
    coll->coll_create_weightstr         = soliddb_collation_general_create_weightstr;
    coll->coll_get_bytes_in_chars       = soliddb_collation_general_get_bytes_in_chars;
    coll->coll_get_charset              = soliddb_collation_general_get_charset;
    coll->coll_numcells                 = soliddb_collation_general_numcells;
    coll->coll_mb_wc                    = soliddb_collation_general_mb_wc;

    return (TRUE);
}

/*#***********************************************************************\
 *
 *              soliddb_collation_utf8_get_maxlen_for_weightstr
 *
 * Description: this routine calculates the max length of the weight string to be created
 *              for UTF8 collations
 *
 * Parameters :
 *
 *  su_collation_t* coll                [in]    collation
 *  void*           str                 [in]    string
 *  size_t          str_bytelen         [in]    string size in bytes
 *  size_t          str_prefix_charlen  [in] if 0 ignore this parameter     prefix index length in chars
 *
 * Return value :   size_t
 *
 *  max length of the weight string to be created
 *
 * Globals used :
 */
static size_t soliddb_collation_utf8_get_maxlen_for_weightstr(
        su_collation_t* coll __attribute__((unused)),   /* [in] */
        void* str __attribute__((unused)),              /* [in] */
        size_t str_bytelen,                             /* [in] */
        size_t str_prefix_charlen                       /* [in] */  /* if 0 ignore this parameter */)
{
    uint idx_bytelen = (uint)str_bytelen;
    CHARSET_INFO* csinfo = (CHARSET_INFO*)coll->coll_data;
    MY_COLLATION_HANDLER* coll_handler = csinfo->coll;

    if (str_prefix_charlen) {
        str_prefix_charlen = str_bytelen < str_prefix_charlen ? str_bytelen : str_prefix_charlen;
        idx_bytelen = (uint)soliddb_collation_general_get_bytes_in_chars(coll, str, str_bytelen, str_prefix_charlen);
    }

    /*
    We are calculating the maxlen for weight string as for
    worst case scenario (if all characters are plain ascii)
    In this case maxlen is str_bytelen * 2
    */
    return (2 * idx_bytelen);
}

/*#***********************************************************************\
 *
 *              soliddb_collation_utf8_initbuf
 *
 * Description: initialize utf8 collations
 *
 * Parameters :
 *
 *   su_collation_t* coll           [in,out]    collation structure
 *   char*           collation_name [in]        use, collation name to be initialized
 *   void*           data           [in]        use, collation data
 *
 * Return value : solid_bool
 *
 * Globals used :
 */
solid_bool soliddb_collation_utf8_initbuf(
        su_collation_t* coll,   /* [in] */
        char* collation_name,   /* [in] */  /* UTF8 encoding! */
        void* data)             /* [in] */
{
    CHARSET_INFO* csinfo = (CHARSET_INFO*)data;

    ss_dprintf_1(("soliddb_collation_utf8_initbuf\n"));

    if (coll->coll_name != NULL) {
        return (TRUE);
    }

    coll->coll_name = (char*)csinfo->name; /* note: holds reference! */
    coll->coll_data = data;
    coll->coll_initbuf                  = soliddb_collation_general_initbuf;
    coll->coll_donebuf                  = soliddb_collation_general_donebuf;
    coll->coll_compare                  = soliddb_collation_general_compare;
    coll->coll_get_maxlen_for_weightstr = soliddb_collation_utf8_get_maxlen_for_weightstr;
    coll->coll_create_weightstr         = soliddb_collation_general_create_weightstr;
    coll->coll_get_bytes_in_chars       = soliddb_collation_general_get_bytes_in_chars;
    coll->coll_get_charset              = soliddb_collation_general_get_charset;
    coll->coll_numcells                 = soliddb_collation_general_numcells;
    coll->coll_mb_wc                    = soliddb_collation_general_mb_wc;

    return (TRUE);
}

/*#***********************************************************************\
 *
 *              soliddb_collation_init
 *
 * Initialize collation routines
 *
 * Parameters :
 *
 *   char*    collation_name, in, use, collation name to be initialized
 *   uint     collation_id    in, use, collation identifier
 *
 * Return value : su_collation_t*, collation structure
 *
 * Globals used :
 */
static su_collation_t* soliddb_collation_init(
        char* collation_name,
        uint  collation_id)
{
    su_collation_t* collation = NULL;
    CHARSET_INFO* csinfo = NULL;

    csinfo = get_charset_by_name((const char*)collation_name, MYF(0));

    if (collation_id == 0) {
        collation_id = csinfo->number;
    }

    ss_dprintf_1(("soliddb_collation_init:collation_name=%s id=%u\n", collation_name, collation_id));

    SsSemEnter(solid_collation_mutex);

    if (soliddb_collations == NULL) {
        soliddb_collations = su_pa_init();
    }

    if (su_pa_indexinuse(soliddb_collations, collation_id)) {
        collation = (su_collation_t*)su_pa_getdata(soliddb_collations, collation_id);
    } else {
        collation = NULL;
    }

    if (collation == NULL) {
        ss_dprintf_2(("soliddb_collation_init:not found, add new to supa\n"));

        collation = (su_collation_t*)SsMemCalloc(sizeof(su_collation_t), 1);

        if (strcmp(csinfo->csname, "utf8") == 0) {
            soliddb_collation_utf8_initbuf(collation, NULL, csinfo);
        } else {
            soliddb_collation_general_initbuf(collation, NULL, csinfo);
        }

        su_pa_insertat(soliddb_collations, collation_id, collation);
    }

    SsSemExit(solid_collation_mutex);

    return(collation);
}

/*#***********************************************************************\
 *
 *              soliddb_field_can_have_collation
 *
 * Discover if atype could be associated wih collation
 *
 * Parameters :
 *
 *   rs_atype_t* atype  - corresponding atype
 *
 * Return value : bool
 *
 * Globals used :
 */
static bool soliddb_atype_can_have_collation(rs_sysi_t* cd, rs_atype_t* atype)
{
    rs_datatype_t       type;
    ss_int1_t           sqltype;
    rs_mysqldatatype_t  mysqldatatype;
    bool bret = FALSE;

    if (!atype) {
        return bret;
    }

    type            = rs_atype_datatype(cd, atype);
    sqltype         = rs_atype_sqldatatype(cd, atype);
    mysqldatatype   = rs_atype_mysqldatatype(cd, atype);

    switch (mysqldatatype) {
        case RS_MYSQLTYPE_VAR_STRING:
        case RS_MYSQLTYPE_VARCHAR:
        case RS_MYSQLTYPE_STRING:
        case RS_MYSQLTYPE_TINY_BLOB:
        case RS_MYSQLTYPE_MEDIUM_BLOB:
        case RS_MYSQLTYPE_BLOB:
        case RS_MYSQLTYPE_LONG_BLOB:
            if ((type == RSDT_CHAR) ||
                ((type == RSDT_BINARY) &&
                 ((sqltype == RSSQLDT_LONGVARBINARY) ||
                  (sqltype == RSSQLDT_VARBINARY) ||
                  (sqltype == RSSQLDT_BINARY)))) {
                bret = TRUE;
            }
            break;
        default:
            break;
    }

    return bret;
}

/*#***********************************************************************\
 *
 *              soliddb_aval_print_externaldatatype
 *
 * Print external datatype value to the string
 *
 * Parameters :
 *
 *   rs_sysi_t*   cd, in, use, system information
 *   rs_atype_t*  atype, in, use, attribute type
 *   rs_aval_t*   aval, in, use, attribute value
 *
 * Return value : char *, attribute value in string representation
 *
 * Globals used :
 */
static char* soliddb_aval_print_externaldatatype(
        rs_sysi_t* cd,
        rs_atype_t* atype,
        rs_aval_t* aval)
{
        switch (rs_atype_mysqldatatype(cd, atype)) {
            case RS_MYSQLTYPE_NEWDECIMAL:
            {
                void* data;
                ulong length;
                char* p;
                uint precision;
                uint dec;
                int buflen;
                char buf[DECIMAL_MAX_PRECISION*3];
                my_decimal decimal_value;

                rs_atype_getextlenscale(cd, atype, &precision, &dec);

                if (precision == 0) {
                    p = (char*)SsMemStrdup((char *)"...");
                } else {
                    data = rs_aval_getdata(cd, atype, aval, &length);
#if MYSQL_VERSION_ID >= 50120
                    binary2my_decimal(E_DEC_FATAL_ERROR, (const uchar*)data, &decimal_value,
                                      precision, dec);
#else
                    binary2my_decimal(E_DEC_FATAL_ERROR, (const char*)data, &decimal_value,
                                      precision, dec);
#endif
                    buflen = sizeof(buf);
                    decimal2string(
                        (decimal_t*)&decimal_value,
                        buf,
                        &buflen,
                        0,
                        0,
                        0);
                    p = (char*)SsMemStrdup((char*)buf);
                }

                return(p);
            }
            case RS_MYSQLTYPE_DECIMAL:
            {
                void* data;
                ulong length;
                String str;
                char* p;

                data = rs_aval_getdata(cd, atype, aval, &length);
                for (p = (char*)data; *p == ' ' ; p++, length--) {
                    continue;
                }
                str.set_ascii((const char*)p, length);
                p = (char*)SsMemStrdup((char*)str.ptr());
                return(p);
            }
            default:
                return((char*)NULL);
        }
}
/*#***********************************************************************\
 *
 *              soliddb_collation_init
 *
 * Initialize system table for collations
 *
 * Parameters :
 *
 * tb_database_t* tdb, solidDB database
 *
 * Return value : -
 *
 * Globals used : Before calling this function you must call su_collation_init();
 */
static void soliddb_collations_init(
        tb_database_t* tdb)
{
        su_pa_t* collations = NULL;
        CHARSET_INFO* charset = NULL;
        uint i=0;

        SS_PUSHNAME("soliddb_collations_init");

        collations = su_collation_get_collations();
        ss_dassert(collations != NULL);

        su_pa_do(collations, i) {
            uint* collation_id = (uint*)su_pa_getdata(collations, i);

            charset = get_charset(*collation_id, MYF(0));
            ss_dassert(charset != NULL);

            tb_dd_update_system_collations(tdb, charset->number, charset->name, charset->csname);
        }

        SS_POPNAME;
}

#if MYSQL_VERSION_ID >= 50100

/*#***********************************************************************\
 *
 *              show_soliddb_vars
 *
 * Show solidDB variables
 *
 * Parameters :
 *
 *   MSYQL_THD  thd, in, use, MySQL thread
 *   SHOW_VAR*  var, in, use, variables
 *   char*      buff, in, not used
 *
 * Return value : 0
 *
 * Globals used :
 */

static inline int show_soliddb_vars(
        MYSQL_THD thd,
        SHOW_VAR *var,
        char *buff)
{
        soliddb_export_status();
        var->type = SHOW_ARRAY;
        var->value= (char *)soliddb_status_variables;
        return 0;
}

SHOW_VAR soliddb_status_variables_export[]= {
  {"solidDB",                   (char*) &show_soliddb_vars, SHOW_FUNC},
  {NullS, NullS, SHOW_LONG}
};

static struct st_mysql_storage_engine soliddb_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION};

/* solidDB for MySQL plugin configuration variables */

static MYSQL_SYSVAR_LONGLONG(cache_size, soliddb_cache_size,
                             PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                             "The size of the main memory solidDB allocates for the cache",
                             NULL, NULL, 64*1024*1024L, 1024*1024L, LONGLONG_MAX, 1024*1024L);

static MYSQL_SYSVAR_LONGLONG(checkpoint_interval, soliddb_checkpoint_interval,
                             PLUGIN_VAR_RQCMDARG,
                             "The number of writes to the log files made in the database which causes automatic checkpoint creation.",
                             NULL, soliddb_update_checkpoint_interval, 50000L, 1024L, LONGLONG_MAX, 0*0L);

static MYSQL_SYSVAR_ULONG(durability_level, soliddb_durability_level,
                         PLUGIN_VAR_RQCMDARG,
                         "Durability level of the logging in solidDB",
                         NULL, soliddb_update_durability_level, 3, 1, 3, 0);

static MYSQL_SYSVAR_ULONG(lock_wait_timeout, soliddb_lock_wait_timeout,
                         PLUGIN_VAR_RQCMDARG,
                         "Lock wait timeout for solidDB for MySQL storage engine.",
                         NULL, soliddb_update_lock_wait_timeout, 30, 1, 1000, 0);

static MYSQL_SYSVAR_ULONG(db_block_size, soliddb_db_block_size,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                         "Sets the block size of database files.",
                         NULL, NULL, 
			 8192, 			/* default setting */
			 512, 			/* minimum value */
			 LONG_MAX, 0);		/* maximum value */

static MYSQL_SYSVAR_ULONG(log_block_size, soliddb_log_block_size,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                         "Sets the block size of log files.",
                         NULL, NULL, 0, 512, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(backup_block_size, soliddb_backup_block_size,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                         "Sets the block size of backup file.",
                         NULL, NULL, 0, 512, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(io_threads, soliddb_io_threads,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                         "Number of helper I/O threads (per IO device) for read and write purposes.",
                         NULL, NULL, 0, 1, LONG_MAX, 0);

static MYSQL_SYSVAR_LONGLONG(lockhash_size, soliddb_lockhash_size,
                             PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                             "Number of elements in lock hash table.",
                             NULL, NULL, 1000000, 100000, LONGLONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(checkpoint_time, soliddb_checkpoint_time,
                          PLUGIN_VAR_RQCMDARG,
                          "Specifies the minimum time in seconds between two checkpoint operations.",
                          NULL, soliddb_update_checkpoint_time, 0, 0, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(write_threads, soliddb_write_threads,
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Number of helper threads dedicated to writing task (per I/O drive).",
                          NULL, NULL, 0, 1, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(extend_increment, soliddb_extend_increment,
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Sets the number of blocks of disk space that are allocated at one time.",
                          NULL, NULL, 500, 1, LONG_MAX, 0);

static MYSQL_SYSVAR_ULONG(readahead, soliddb_readahead,
                          PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                          "Sets the number of prefetched index reads during long sequential searches",
                          NULL, NULL, 4, 1, LONG_MAX, 0);

static MYSQL_SYSVAR_STR(logdir, soliddb_logdir,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Log files are created automatically to the directory specified",
                        NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(backupdir, soliddb_backupdir,
                        PLUGIN_VAR_RQCMDARG,
                        "Backup files are created automatically to the directory specified",
                        NULL, soliddb_update_backupdir, NULL); 
/*                         soliddb_check_backupdir, soliddb_update_backupdir, NULL); */

static MYSQL_SYSVAR_STR(admin_command, soliddb_admin_command,
                        PLUGIN_VAR_RQCMDARG, 
                        "User can specify admin commands using this parameter",
/*                        NULL, soliddb_update_admin_command, NULL);	*/
                        soliddb_check_admin_command, soliddb_update_admin_command, NULL); 

static MYSQL_SYSVAR_STR(filespec, soliddb_filespec,
                        PLUGIN_VAR_RQCMDARG,
                        "Describes the location and the maximum size of the database file(s).",
                        NULL, soliddb_add_filespec, NULL);
/*                        soliddb_check_filespec, soliddb_add_filespec, NULL); */

static MYSQL_SYSVAR_BOOL(checkpoint_deletelog, soliddb_checkpoint_deletelog,
                         PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
                         "If checkpoint_deletelog parameter is set to true, then the SoliDB server deletes "
                         "the transaction log files after each successful checkpoint.",
                         NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(log_enabled, soliddb_log_enabled,
                         PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
                         "Specifies whether transaction logging is enabled or not.",
                         NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(pessimistic, soliddb_pessimistic,
                         PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
                         "Specifies whether pessimistic concurrency control is used.",
                         NULL, NULL, TRUE);

static struct st_mysql_sys_var* soliddb_system_variables[] = {
    MYSQL_SYSVAR(cache_size),
    MYSQL_SYSVAR(checkpoint_interval),
    MYSQL_SYSVAR(durability_level),
    MYSQL_SYSVAR(lock_wait_timeout),
    MYSQL_SYSVAR(db_block_size),
    MYSQL_SYSVAR(log_block_size),
    MYSQL_SYSVAR(backup_block_size),
    MYSQL_SYSVAR(io_threads),
    MYSQL_SYSVAR(lockhash_size),
    MYSQL_SYSVAR(checkpoint_time),
    MYSQL_SYSVAR(write_threads),
    MYSQL_SYSVAR(extend_increment),
    MYSQL_SYSVAR(readahead),
    MYSQL_SYSVAR(logdir),
    MYSQL_SYSVAR(backupdir),
    MYSQL_SYSVAR(admin_command),
    MYSQL_SYSVAR(filespec),
    MYSQL_SYSVAR(checkpoint_deletelog),
    MYSQL_SYSVAR(log_enabled),
    MYSQL_SYSVAR(pessimistic),
    NULL
};

/* static st_mysql_information_schema soliddb_system = { MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION}; */

/*#***********************************************************************\
 *
 *              mysql_declare_plugin
 *
 * Declare solidDB plugins
 *
 * Parameters :
 *
 * Return value :
 *
 * Globals used :
 */
mysql_declare_plugin(soliddb)
{
      MYSQL_STORAGE_ENGINE_PLUGIN,
      &soliddb_storage_engine,
      solid_hton_name,
      "Solid Information Technology Ltd",
      solid_hton_comment,
      PLUGIN_LICENSE_GPL,
      solid_init,   /* Plugin Init */
      solid_deinit, /* Plugin Deinit */
      0x0100 /* 1.0 */,
      soliddb_status_variables_export,
      soliddb_system_variables,
      NULL  /* TODO: config options   */
},
i_s_soliddb_sys_tables,
i_s_soliddb_sys_columns,
i_s_soliddb_sys_columns_aux,
i_s_soliddb_sys_keys,
i_s_soliddb_sys_keyparts,
i_s_soliddb_sys_keyparts_aux,
i_s_soliddb_sys_forkeys,
i_s_soliddb_sys_forkeyparts,
i_s_soliddb_sys_schemas,
i_s_soliddb_sys_sequences,
i_s_soliddb_sys_cardinal,
i_s_soliddb_sys_tablemodes,
i_s_soliddb_sys_info,
i_s_soliddb_sys_blobs
mysql_declare_plugin_end;

#endif /* MYSQL_VERSION_ID >= 50100 */ 
