/*************************************************************************\
**  source       * dbe0db.h
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


#ifndef DBE0DB_H
#define DBE0DB_H

#include <sstime.h>
#include <sssem.h>
#include <ssint8.h>
#include <uti0vtpl.h>

#include <su0inifi.h>
#include <su0cfgl.h>
#include <su0list.h>
#include <su0parr.h>
#include <su0time.h>

#include <rs0types.h>
#include <rs0rbuf.h>
#include <rs0sysi.h>
#include <rs0entna.h>
#include <rs0admev.h>
#include <rs0pla.h>

#include "dbe9type.h"
#include "dbe0type.h"

SS_INLINE dbe_index_t* dbe_db_getindex(
        dbe_db_t* db);

SS_INLINE dbe_mme_t* dbe_db_getmme(
        dbe_db_t* db);

#include "dbe9bhdr.h"
#include "dbe8trxl.h"
#include "dbe8seql.h"
#include "dbe8srec.h"
#include "dbe8flst.h"
#include "dbe7hdr.h"
#include "dbe7cfg.h"
#include "dbe7ctr.h"
#include "dbe7gtrs.h"
#include "dbe7logf.h"
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
#include "dbe0erro.h"
#include "dbe0trx.h"
#include "dbe0blobg2.h"
#include "dbe0brb.h"
#include "dbe0crypt.h"
#include "dbe0tref.h"
#include "dbe0brb.h"
#include "dbe0catchup.h"
#include "dbe0hsbstate.h"
#include "dbe0hsbg2.h"

#define CHK_DB(db) ss_dassert(SS_CHKPTR(db) && (db)->db_chk == DBE_CHK_DB)

typedef struct {
        size_t  dbst_trx_commitcnt;     /* Count of committed transactions. */
        size_t  dbst_trx_abortcnt;      /* Count of aborted transactions. */
        size_t  dbst_trx_rollbackcnt;   /* Count of rollbacked transactions. */
        size_t  dbst_trx_readonlycnt;   /* Count of read only transactions. */
        size_t  dbst_trx_bufcnt;        /* Count of unmerged transactions. */
        size_t  dbst_trx_validatecnt;   /* Count of transactions under validation. */
        size_t  dbst_trx_activecnt;     /* Count of active transaction. */

        size_t  dbst_cac_findcnt;       /* Count of cache find operations. */
        size_t  dbst_cac_readcnt;       /* Count of disk reads from cache. */
        size_t  dbst_cac_writecnt;      /* Count of disk writes from cache. */
        size_t  dbst_cac_prefetchcnt;
        size_t  dbst_cac_preflushcnt;

        size_t  dbst_ind_writecnt;      /* Count of index writes. */
        size_t  dbst_ind_writecntsincemerge; /* Count of index writes after last merge. */
        size_t  dbst_ind_mergewrites;   /* Count of merge writes for merge. */
        size_t  dbst_ind_mergeact;      /* Is merge active?. */
        size_t  dbst_ind_filesize;      /* Index (database) file size in bytes. */
        size_t  dbst_ind_freesize;

        size_t  dbst_log_writecnt;      /* Count of log writes. */
        size_t  dbst_log_writecntsincecp; /* Count of log writes after last checkpoint. */
        size_t  dbst_log_filewritecnt;
        size_t  dbst_log_filesize;      /* Log file size in bytes. */

        size_t  dbst_sea_activecnt;     /* Count of active searches. */
        size_t  dbst_sea_activeavg;     /* Average number of active searches. */
        float   dbst_cp_thruput;        /* Disk thruput of checkpoint writing. */
} dbe_dbstat_t;

typedef enum {
        DBE_DB_OPEN_NONE,
        DBE_DB_OPEN_DBFILE,
        DBE_DB_OPEN_LOGFILE
} dbe_db_openstate_t;

#ifdef SS_MME
/* MME checkpoint task's state */
typedef enum {
        MME_CP_INIT = 0,
        MME_CP_STARTED,
        MME_CP_GETPAGE,
        MME_CP_FLUSH_DATAPAGE,
        MME_CP_FLUSH_ADDRESSPAGE,
        MME_CP_COMPLETED,
        MME_CP_DONE
} mme_cpstate_t;
#endif

/* General database data structure.
 */
struct dbe_db_st {
        ss_debug(dbe_chk_t db_chk;)
        dbe_index_t*    db_index;
#ifdef SS_MME
        dbe_mme_t*      db_mme;
#endif
        bool            db_defaultstoreismemory;
        dbe_durability_t db_durabilitylevel;
        dbe_bkeyinfo_t  db_bkeyinfo;
        rs_rbuf_t*      db_rbuf;         /* Relation buffer. */
        dbe_dbstate_t   db_dbstate;      /* database open status */
        dbe_gobj_t*     db_go;           /* Global object structure. */
        dbe_file_t*     db_dbfile;       /* Database file object. */
        dbe_cpmgr_t*    db_cpmgr;        /* Checkpoint manager */
        bool            db_cpactive;
        bool            db_allowcheckpointing;
        su_ret_t        (*db_checkpointcallback)(rs_sysi_t* cd);
        su_gate_t*      db_actiongate;
        su_pa_t*        db_users;

        SsSemT*         db_nsearchsem;
        uint            db_nsearch;
        double          db_avgnsearch;
        dbe_backup_t*   db_backup;
        dbe_indmerge_t* db_indmerge;
        dbe_indmerge_t* db_quickmerge;
        uint            db_indmergenumber;
        su_gate_t*      db_mergesem;
        int             db_isloader;
        long            db_quickmergelimit;
        bool            db_mergeidletime;
        long            db_mergelimit;
        long            db_backup_stepstoskip;
        int             db_mergedisablecount;
        long            db_mergemintime;
        long            db_mergelasttime;
        long            db_quickmergelasttime;
        dbe_trxid_t     db_forcemergetrxid;
        dbe_trxnum_t    db_forcemergetrxnum;
        dbe_trxnum_t    db_lastmergecleanup;
        long            db_cplimit;
        long            db_cpmintime;
        long            db_cplasttime;
        long            db_tmpcplimit;
        size_t          db_poolsize;
        bool            db_earlyvld;
        bool            db_readonly;
        bool            db_hsbshutdown;
        bool            db_disableidlemerge;    /*set disable idle merge flag*/
        bool            db_changed;
        bool            db_force_checkpoint;
        int             db_final_checkpoint;    /* 0=No, 1=Pending, 2=Started */
        SsSemT*         db_changedsem;
        SsSemT*         db_sem;
        int             db_ddopactivecnt;       /* Number of active dd operation */
        dbe_escalatelimits_t  db_escalatelimits;
        su_list_t*      db_dropcardinallist;

        SsTimeT         db_prevlogsizemeastime;
        ulong           db_prevlogsize;
        su_ret_t        db_fatalerrcode;        /* Fatal error, cannot close the
                                                   database or create a
                                                   checkpoint. */
        dbe_seq_t*      db_seq;                 /* Sequence object. */

#ifndef SS_NOLOCKING
        bool            db_pessimistic;
        dbe_lockmgr_t*  db_lockmgr;
        long            db_pessimistic_lock_to; /* Pessimistic lock timeout */
        long            db_optimistic_lock_to;  /* Optimistic lock timeout. */
#endif /* SS_NOLOCKING */
        long            db_table_lock_to;       /* for SYNC conflict resolution */

#ifdef DBE_REPLICATION
        bool            db_hsbenabled;
        dbe_ret_t       (*db_replicatefun)(
                            void* rep_ctx,
                            rep_type_t type,
                            rep_params_t* rp);
        dbe_ret_t       (*db_commitreadyfun)(dbe_trxid_t trxid);
        void            (*db_commitdonefun)(dbe_trxid_t trxid, bool commit);
        void*           db_repctx;
        dbe_hsbmode_t   db_hsbmode;             /* replication mode */
#ifdef SS_HSBG2
        bool            db_hsbg2configured;
        dbe_hsbg2_t*    db_hsbg2svc;            /* HSB service */
        dbe_hsbstate_t* db_hsbstate;
        bool            db_hsbg2_adaptiveif;    /* TRUE if running in PRIMARY_ACTIVE state */
        bool            db_hsbg2safenesslevel_adaptive;
        void            (*db_reloaf_rbuf_fun)(void* ctx);
        void*           db_reloaf_rbuf_ctx;
        SsMesT*         db_hsb_durable_mes;
        rs_sysi_t*      db_hsb_durable_cd;
        dbe_hsbstatelabel_t db_starthsbstate;

        bool            db_hsb_enable_syschanges;
#endif
        dbe_trxid_t     db_reptrxidmax;         /* Max. TRX ID of committed
                                                   repl. trx's */
        long            db_repopcount;          /* Operation count since last
                                                   commit. */
        SsSemT*         db_hsbsem;             /* Replication commit gate,
                                                   for atomic update of
                                                   db_reptrxidmax in cp. */
        bool            db_ishsb;
        bool            db_hsbunresolvedtrxs;   /* TRUE if there are
                                                   currently transactions
                                                   which are waiting for
                                                   HSB engine state change
                                                   in order to resolve the
                                                   status of these trxs,
                                                   FALSE otherwise */
#endif /* DBE_REPLICATION */

#if defined(DBE_MTFLUSH)
        void            (*db_flushwakeupfp)(void*);
        void*           db_flushwakeupctx;
        void            (*db_flusheventresetfp)(void*);
        void*           db_flusheventresetctx;
        void            (*db_flushbatchwakeupfp)(void*);
        void*           db_flushbatchwakeupctx;
#endif /* DBE_MTFLUSH */
        bool db_migratetounicode;
        bool db_migratetoblobg2;
        bool db_migrategeneric;
        bool db_logsplit; /* log file split @latest checkpoint */
        bool db_recoverydone;
#ifdef SS_MME
        mme_cpstate_t           db_mmecp_state;         /* State of the MME-checkpoint */
        dbe_cacheslot_t*        db_mmecp_page;          /* MME page */
        char*                   db_mmecp_pagedata;      /* MME page data area */
        su_daddr_t              db_mmecp_daddr;         /* MME page disk address */
        dbe_cacheslot_t*        db_mmecp_pageaddrpage;  /* MME page addresses page*/
        char*                   db_mmecp_pageaddrdata;  /* MME -"- data area */
        su_daddr_t              db_mmecp_pageaddrdaddr; /* MME -"- disk address */
        dbe_iomgr_flushbatch_t* db_mmecp_flushbatch;    /* Flushbatch */
        su_ret_t                db_mmecp_rc;
        uint                    db_mmecp_npages;
#endif
        su_timer_t              db_cp_timer;            /* Timer to record
                                                           cp thruput. */
        float                   db_cp_thruput;          /* Last cp's thruput
                                                           to disk. */

        SsTimeT                 db_primarystarttime;
        SsTimeT                 db_secondarystarttime;

        su_ret_t        (*db_backupmme_cb)(
                void*                   ctx,
                dbe_backupfiletype_t    ftype,
                void*                   data,
                size_t                  len);
        void*                   db_backupmmectx;
        void*                   db_tbconnect_ctx;
        rs_sysi_t*              (*db_tbconnect_initcd_cb)(void* ctx);
        void                    (*db_tbconnect_donecd_cb)(rs_sysi_t* cd);
};

extern dbe_db_openstate_t dbe_db_openstate; /* Open state, updated from tab0conn.c */

ss_debug(extern bool dbe_db_debugshutdown;)
ss_debug(extern bool dbe_db_going2debugshutdown;)

extern dbe_db_t* h_db;

bool dbe_db_check_overwrite(
        dbe_db_t* db,
        char* path);

bool dbe_db_dbexist(
        su_inifile_t* inifile);

bool dbe_db_dbexistall(
        su_inifile_t* inifile);

bool dbe_db_iscfgreadonly(
        su_inifile_t* inifile);

dbe_db_t* dbe_db_init(
        rs_sysi_t* cd,
        su_inifile_t* inifile,
        dbe_dbstate_t* p_dbstate,
        bool migratetounicode_done,
        bool freelist_globallysorted
#ifdef SS_HSBG2
        , void* hsbg2ctx
#endif /* SS_HSBG2 */
        );

void dbe_db_done(
        dbe_db_t* db);

void dbe_db_startupforcemergeif(
        rs_sysi_t* cd,
        dbe_db_t* db);

dbe_ret_t dbe_db_recover(
        dbe_db_t* db,
        dbe_user_t* user,
        dbe_db_recovcallback_t* recovcallback,
        ulong* p_ncommits);

SS_INLINE dbe_hsbmode_t dbe_db_gethsbmode(
        dbe_db_t* db);

SS_INLINE dbe_hsbmode_t dbe_db_gethsbg2mode(
        dbe_db_t* db);

SS_INLINE bool dbe_db_is2safe(
        dbe_db_t* db);

SS_INLINE bool dbe_db_isbackupactive(
        dbe_db_t* db);

SS_INLINE bool dbe_db_iscpactive(
        dbe_db_t* db);

void dbe_db_hsbenable_syschanges(
        dbe_db_t* db,
        bool enablep);

void dbe_db_setstarthsbstate(
        dbe_db_t* db,
        dbe_hsbstatelabel_t hsbstate);


dbe_hsbstate_t* dbe_db_gethsbstate(
        dbe_db_t* db);

#ifdef SS_MYSQL_AC
char* dbe_db_gethsbrolestr(dbe_db_t* db);
char* dbe_db_gethsbstatestr(dbe_db_t* db);
#endif /* SS_MYSQL_AC */

dbe_trxid_t dbe_db_getreptrxidmax(
        dbe_db_t* db);

void dbe_db_setreptrxidmax(
        dbe_db_t* db,
        dbe_trxid_t reptrxidmax);

long dbe_db_getrepopcount(
        dbe_db_t* db);

void dbe_db_setrepopcount(
        dbe_db_t* db,
        long repopcount);

dbe_trxid_t dbe_db_getcurtrxidmax(
        dbe_db_t* db);

SsSemT* dbe_db_hsbsem_get(
        dbe_db_t* db);

#ifdef SS_DEBUG
bool dbe_db_hsbsem_isentered(
        dbe_db_t* db);
#endif /* SS_DEBUG */

dbe_ret_t dbe_db_open(
        dbe_db_t* db);

dbe_dbstate_t dbe_db_getdbstate(
        dbe_db_t* db);

void dbe_db_setfatalerror(
        dbe_db_t* db,
        su_ret_t rc);

SS_INLINE su_ret_t dbe_db_getfatalerrcode(
        dbe_db_t* db);

bool dbe_db_setchanged(
        dbe_db_t* db,
        rs_err_t** p_errh);

su_inifile_t* dbe_db_getinifile(
        dbe_db_t* db);

#ifdef SS_MME

void dbe_db_resetflushbatch(
        dbe_db_t* db);

void dbe_db_clearmme(
        rs_sysi_t*      cd,
        dbe_db_t*       db);

void dbe_db_gettemporarytablecardin(
        rs_sysi_t*      cd,
        dbe_db_t*       db,
        rs_relh_t*      relh,
        ss_int8_t*      p_nrows,
        ss_int8_t*      p_nbytes);

#endif /* SS_MME */

bool dbe_db_ismme(
        dbe_db_t* db);

bool dbe_db_getdefaultstoreismemory(
        dbe_db_t* db);

void dbe_db_setdefaultstoreismemory(
        dbe_db_t* db, bool val);

void dbe_db_setcpinterval(
        dbe_db_t* db, long val);

void dbe_db_setmergeinterval(dbe_db_t* db, long val);
void dbe_db_setmergemintime(dbe_db_t* db, long val);
void dbe_db_setcpmintime(dbe_db_t* db, long val);

dbe_durability_t dbe_db_getdurabilitylevel(
        dbe_db_t* db);

dbe_durability_t dbe_db_getdurabilitylevel_raw(
        dbe_db_t* db);

void dbe_db_setdurabilitylevel(dbe_db_t* db, dbe_durability_t durability);

bool dbe_db_hsbg2safenesslevel_adaptive(
        dbe_db_t* db);

void dbe_db_set_hsbg2safenesslevel_adaptive(
        dbe_db_t* db,
        bool adaptive);

void dbe_db_insertrbuf(
        dbe_db_t* db,
        rs_rbuf_t* rbuf);

rs_rbuf_t* dbe_db_getrbuf(
        dbe_db_t* db);

SS_INLINE dbe_gtrs_t* dbe_db_getgtrs(
        dbe_db_t* db);

SS_INLINE dbe_gobj_t* dbe_db_getgobj(
        dbe_db_t* db);

SS_INLINE dbe_escalatelimits_t* dbe_db_getescalatelimits(
        dbe_db_t* db);

SS_INLINE long dbe_db_getwritesetescalatelimit(
        dbe_db_t* db);

bool dbe_db_ispessimistic(
        dbe_db_t* db);

SS_INLINE void dbe_db_getlocktimeout(
        dbe_db_t* db,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to);

void dbe_db_setlocktimeout(
        dbe_db_t* db,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to);

su_ret_t dbe_db_removelastfilespec(
        dbe_db_t* db);

su_ret_t dbe_db_addnewfilespec(
        dbe_db_t* db,
        char* filename,
        ss_int8_t maxsize,
        uint diskno);

void dbe_db_fileusageinfo(
        dbe_db_t* db,
        double* maxsize,
        double* currsize,
        float* totalperc,
        uint nth,
        float* perc);

SS_INLINE long dbe_db_gettablelocktimeout(
        dbe_db_t* db);

void dbe_db_settablelocktimeout(
        dbe_db_t* db,
        long val);

SS_INLINE dbe_counter_t* dbe_db_getcounter(
        dbe_db_t* db);

SS_INLINE void* dbe_db_gettrxbuf(
        dbe_db_t* db);

SS_INLINE dbe_blobmgr_t* dbe_db_getblobmgr(
        dbe_db_t* db);

SS_INLINE dbe_lockmgr_t* dbe_db_getlockmgr(
        dbe_db_t* db);

SS_INLINE dbe_seq_t* dbe_db_getseq(
        dbe_db_t* db);

void dbe_db_setcheckpointcallback(
        dbe_db_t* db,
        su_ret_t (*cpcallbackfun)(rs_sysi_t* cd));

dbe_ret_t dbe_db_checkcreatecheckpoint(
        dbe_db_t* db);

dbe_ret_t dbe_db_createcheckpoint(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool displayprogress,
        bool splitlog);

dbe_ret_t dbe_db_createsnapshot(
        rs_sysi_t* cd,
        dbe_db_t* db);

dbe_ret_t dbe_db_createcheckpoint_start(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool splitlog);

bool dbe_db_last_checkpoint_split_log(
        rs_sysi_t* cd,
        dbe_db_t* db);

bool dbe_db_lastcheckpoint_split_log(
        rs_sysi_t* cd,
        dbe_db_t* db);

dbe_ret_t dbe_db_createsnapshot_start(
        rs_sysi_t* cd,
        dbe_db_t* db);

dbe_ret_t dbe_db_createcp_step(
        rs_sysi_t* cd,
        dbe_db_t* db,
        bool dislpayprogress);

dbe_ret_t dbe_db_createcp_end(
        rs_sysi_t* cd,
        dbe_db_t* db);

dbe_ret_t dbe_db_deletesnapshot(
        dbe_db_t *db,
        dbe_cpnum_t cpnum);

void dbe_db_setddopactive(
        dbe_db_t *db,
        bool startp);

void dbe_db_enteraction(
        dbe_db_t* db,
        rs_sysi_t* cd);

void dbe_db_enteraction_exclusive(
        dbe_db_t* db,
        rs_sysi_t* cd);

void dbe_db_exitaction(
        dbe_db_t* db,
        rs_sysi_t* cd);

void dbe_db_enteraction_hsb(
        dbe_db_t* db);

void dbe_db_exitaction_hsb(
        dbe_db_t* db);

int dbe_db_adduser(
        dbe_db_t* db,
        dbe_user_t* user);

void dbe_db_removeuser(
        dbe_db_t* db,
        uint userid);

int dbe_db_getusercount(
        dbe_db_t* db);

dbe_user_t* dbe_db_getuserbyid(
        dbe_db_t* db,
        uint userid);

void dbe_db_abortsearchesrelid(
        dbe_db_t* db,
        ulong relid);

void dbe_db_abortsearcheskeyid(
        dbe_db_t* db,
        ulong keyid);

void dbe_db_newplan(
        dbe_db_t*   db,
        ulong       relid);

void dbe_db_signaltouser(
        dbe_db_t* db,
        int userid,
        rs_signaltype_t signaltype);

void dbe_db_searchstarted(
        dbe_db_t* db);

void dbe_db_searchstopped(
        dbe_db_t* db);

ulong dbe_db_getnewrelid_log(
        dbe_db_t* db);

ulong dbe_db_getnewattrid_log(
        dbe_db_t* db);

ulong dbe_db_getnewkeyid_log(
        dbe_db_t* db);

ulong dbe_db_getnewuserid_log(
        dbe_db_t* db);

void dbe_db_inctuplenum(
        dbe_db_t* db);

void dbe_db_inctupleversion(
        dbe_db_t* db);

#ifdef DBE_REPLICATION

void dbe_db_getreplicacounters(
        dbe_db_t* db,
        bool hsbg2,
        char** p_data,
        int* p_size);

void dbe_db_setreplicacounters(
        dbe_db_t* db,
        bool hsbg2,
        char* data,
        int size);

void dbe_db_sethsbmode(
        dbe_db_t* db,
        rs_sysi_t* cd,
        dbe_hsbmode_t hsbmode);

void dbe_db_sethsbg2_adaptive_loggingif(
        dbe_db_t* db,
        bool adaptive_loggingif);

void dbe_db_sethsb(
        dbe_db_t* db);

SS_INLINE bool dbe_db_ishsb(
        dbe_db_t* db);

void dbe_db_reset_logpos(
        dbe_db_t* db);

bool dbe_db_ishsbcopy(
        dbe_db_t* db);

bool dbe_db_ishsbconfigured(
        dbe_db_t* db);

bool dbe_db_ishsbg2configured(
        dbe_db_t* db);

void dbe_db_sethsbunresolvedtrxs(
        dbe_db_t* db,
        bool b);

bool dbe_db_gethsbunresolvedtrxs(
        dbe_db_t* db);

#endif /* DBE_REPLICATION */

SS_INLINE dbe_file_t* dbe_db_getdbfile(
        dbe_db_t* db);

SS_INLINE bool dbe_db_isearlyvld(
        dbe_db_t* db);

SS_INLINE bool dbe_db_isreadonly(
        dbe_db_t* db);

void dbe_db_setreadonly(
        dbe_db_t* db,
        bool readonlyp);

void dbe_db_starthsbshutdown(
        dbe_db_t* db);

void dbe_db_sethsbshutdown(
        dbe_db_t* db);

void dbe_db_addindexwrites(
        dbe_db_t* db,
        rs_sysi_t* cd,
        long nindexwrites);

void dbe_db_addmergewrites(
        dbe_db_t* db,
        long nmergewrites);

long dbe_db_getmergewrites(
        dbe_db_t* db);

long dbe_db_getmergelimit(
        dbe_db_t* db);

void dbe_db_addlogwrites(
        dbe_db_t* db,
        long nlogwrites);

void dbe_db_addtrxstat(
        dbe_db_t* db,
        dbe_db_trxtype_t trxtype,
        bool read_only,
        long stmtcnt);

void dbe_db_startloader(
        dbe_db_t* db);

void dbe_db_stoploader(
        dbe_db_t* db);

bool dbe_db_cpchecklimit(
        dbe_db_t* db);

bool dbe_db_mergecleanup(
        dbe_db_t* db);

int dbe_db_mergechecklimit(
        dbe_db_t* db);

int dbe_db_mergechecklimit_loader(
        dbe_db_t* db);

uint dbe_db_mergestart(
        rs_sysi_t* cd,
        dbe_db_t* db);

uint dbe_db_mergestart_full(
        rs_sysi_t* cd,
        dbe_db_t* db);

void dbe_db_mergestop(
        dbe_db_t* db);

void dbe_db_setnmergetasks(
        dbe_db_t* db,
        int nmergetasks);

dbe_mergeadvance_t dbe_db_mergeadvance(
        dbe_db_t* db,
        rs_sysi_t* cd,
        uint nstep);

dbe_mergeadvance_t dbe_db_mergeadvance_ex(
        dbe_db_t* db,
        rs_sysi_t* cd,
        uint nstep,
        bool mergetask,
        uint* p_mergenumber);

bool dbe_db_quickmergechecklimit(
        dbe_db_t* db);

bool dbe_db_quickmergestart(
        rs_sysi_t* cd,
        dbe_db_t* db);

void dbe_db_quickmergestop(
        dbe_db_t* db);

dbe_mergeadvance_t dbe_db_quickmergeadvance(
        dbe_db_t* db,
        rs_sysi_t* cd);

bool dbe_db_mergeidletimebegin(
        rs_sysi_t* cd,
        dbe_db_t* db,
        uint* p_mergenumber);

void dbe_db_mergeidletimeend(
        dbe_db_t* db);

void dbe_db_setmergedisabled(
        dbe_db_t* db,
        bool disabled);

void dbe_db_logidleflush(
        dbe_db_t* db);

void dbe_db_logflushtodisk(
        dbe_db_t* db);

dbe_ret_t dbe_db_backupcheck(
        dbe_db_t* db,
        char* backupdir,
        rs_err_t** p_errh);

dbe_ret_t dbe_db_backupstart(
        dbe_db_t* db,
        char* backupdir,
        bool replicap,
        rs_err_t** p_errh);

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
        rs_err_t** p_errh);

dbe_ret_t dbe_db_backupsetmmecallback(
        dbe_db_t*       db,
        su_ret_t        (*callbackfp)(
                void*                   ctx,
                dbe_backupfiletype_t    ftype,
                void*                   data,
                size_t                  len),
        void*           callbackctx);

void dbe_db_backupstop(
        dbe_db_t* db);

dbe_ret_t dbe_db_backupadvance(
        dbe_db_t* db,
        rs_err_t** p_errh);

void dbe_db_backuplogfnumrange(
        dbe_db_t* db,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end);

char* dbe_db_getcurbackupdir(
        dbe_db_t* db);

char* dbe_db_givedefbackupdir(
        dbe_db_t* db);

uint dbe_db_blocksize(
        dbe_db_t* db);

size_t dbe_db_poolsize(
        dbe_db_t* db);

long dbe_db_poolsizeforquery(
        dbe_db_t* db);

void dbe_db_addcfgtocfgl(
        dbe_db_t* db,
        su_cfgl_t* cfgl);

void dbe_db_printinfo(
        void* fp,
        dbe_db_t* db);

void dbe_db_printtree(
        dbe_db_t* db,
        bool values);

void dbe_db_errorprinttree(
        dbe_db_t* db,
        bool values);

ulong dbe_db_getlockcount(
        dbe_db_t* db);

void dbe_db_setlockdisablerowspermessage(
        dbe_db_t* db,
        bool lockdisablerowspermessage);

ulong dbe_db_getmergecount(
        dbe_db_t* db);

bool dbe_db_checkindex(
        dbe_db_t* db,
        bool silentp,
        bool full_check);

bool dbe_db_checkdbfile(
        long startblock,
        bool data,
        bool report_only,
        bool content,
        bool only_one_block);

bool dbe_db_filldbblock(
        char* params);

ulong dbe_db_getdbsize(
        dbe_db_t* db);

ulong dbe_db_getdbfreeblocks(
        dbe_db_t* db);

ulong dbe_db_getdbfreesize(
        dbe_db_t* db);

ulong dbe_db_getlogsize(
        dbe_db_t* db);

void dbe_db_getstat(
        dbe_db_t* db,
        dbe_dbstat_t* p_dbst);

void dbe_db_setlogerrorhandler(
        dbe_db_t* db,
        void (*errorfunc)(void*),
        void* errorctx);

void dbe_db_getkeysamples(
        dbe_db_t* db,
        rs_sysi_t* cd,
        rs_relh_t* relh,
        vtpl_t* range_min,
        vtpl_t* range_max,
        dynvtpl_t* sample_vtpl,
        int sample_size);

void dbe_db_gettvalsamples(
        dbe_db_t* db,
        rs_sysi_t* cd,
        rs_relh_t* relh,
        rs_tval_t** sample_tvalarr,
        size_t sample_size);

void dbe_db_getmergekeysamples(
        dbe_db_t* db,
        dynvtpl_t* sample_vtpl,
        int sample_size);

int dbe_db_getequalrowestimate(
        rs_sysi_t* cd,
        dbe_db_t* db,
        vtpl_t* range_begin,
        vtpl_t* range_end);

SsTimeT dbe_db_getcreatime(
        dbe_db_t* db);

SsTimeT dbe_db_gethsbtime(
        dbe_db_t* db);

void dbe_db_sethsbtime_outofsync(
        dbe_db_t* db);

ss_uint4_t dbe_db_getctc(
        dbe_db_t* db);

/* Buffer allocation interface.
 */
typedef struct dbe_hbuf_st dbe_hbuf_t;
typedef struct dbe_bufpool_st dbe_bufpool_t;

dbe_bufpool_t* dbe_db_getbufpool(
        dbe_db_t* db);

void dbe_db_releasebufpool(
        dbe_db_t* db,
        dbe_bufpool_t* bufpool);

dbe_hbuf_t* dbe_bufpool_alloc(
        dbe_bufpool_t* bufpool,
        void* pp_buf);

void dbe_bufpool_free(
        dbe_bufpool_t* bufpool,
        dbe_hbuf_t* hbuf);

void* dbe_hbuf_getbuf(
        dbe_hbuf_t* hbuf);

#ifdef DBE_REPLICATION

/* Replication interface.
 */

void dbe_db_setrepctx(
        dbe_db_t* db,
        void* rep_ctx);

void dbe_db_setreplication(
        dbe_db_t* db,
        bool enable,
        dbe_ret_t (*replicatefun)(
                void* rep_ctx,
                rep_type_t type,
                rep_params_t* rp),
        dbe_ret_t (*commitreadyfun)(dbe_trxid_t trxid),
        void (*commitdonefun)(dbe_trxid_t trxid, bool commit),
        void* rep_ctx);

dbe_ret_t dbe_db_replicate(
        dbe_db_t* db,
        rep_type_t type,
        rep_params_t* rp);

void dbe_db_commitdone(
        dbe_db_t* db,
        dbe_trxid_t trxid,
        bool commit);

dbe_ret_t dbe_db_commitready(
        dbe_db_t* db,
        dbe_trxid_t trxid);

#endif /* DBE_REPLICATION */

#if defined(DBE_MTFLUSH)

void dbe_db_setflushwakeupcallback(
        dbe_db_t* db,
        void (*flushwakeupfp)(void*),
        void* flushwakeupctx,
        void  (*flusheventresetfp)(void*),
        void* flusheventresetctx,
        void (*flushbatchwakeupfp)(void*),
        void* flushbatchwakeupctx);

#endif /* DBE_MTFLUSH */

void dbe_db_adddropcardinal(
        dbe_db_t* db,
        long relid);

su_list_t* dbe_db_givedropcardinallist(
        dbe_db_t* db);

dbe_ret_t dbe_db_copyblobtval(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_trx_t* trx,
        rs_ttype_t* ttype,
        rs_tval_t* tval);

bool dbe_db_migrateneeded(
        dbe_db_t* db);

bool dbe_db_migratetoblobg2(
        dbe_db_t* db);

bool dbe_db_migratetounicode(
        dbe_db_t* db);

void dbe_db_migratetounicodecompleted(
        dbe_db_t* db);

void dbe_db_migratetounicodemarkheader(
        dbe_db_t* db);

void dbe_db_migratetocatalogsuppmarkheader(
        dbe_db_t* db);

void dbe_db_initcd(
        dbe_db_t* db,
        rs_sysi_t* cd);

void dbe_db_donecd(
        dbe_db_t* db,
        rs_sysi_t* cd);

rs_sysinfo_t* dbe_db_getsyscd(
        dbe_db_t* db);

void dbe_db_setinitconnectcdfun(
        dbe_db_t* db,
        void* ctx,
        rs_sysi_t* (*connect_initcd)(void* ctx),
        void (*connect_donecd)(rs_sysi_t* cd));

rs_sysi_t* dbe_db_inittbconcd(
        dbe_db_t* db);

void dbe_db_donetbconcd(
        dbe_db_t* db,
        rs_sysi_t* cd);

dbe_brb_t* dbe_db_getblobrefbuf(
        dbe_db_t* db);

long dbe_db_getstepstoskip(
        dbe_db_t* db);

void dbe_server_setsysifuns(
        void (*server_sysi_init)(rs_sysi_t* sysi),
        void (*server_sysi_done)(rs_sysi_t* sysi),
        void (*server_sysi_initi_functions)(rs_sysi_t* sysi));

void dbe_server_sysi_init_functions(rs_sysi_t *cd);

#ifdef SS_FAKE
void dbe_db_dummycommit(
        dbe_db_t* db,
        dbe_trxid_t* p_trxid);
#endif /* SS_FAKE */

su_ret_t dbe_db_alloc_n_pages(
        dbe_db_t* db,
        su_daddr_t* p_daddrarr_out,
        size_t numpages_requested,
        size_t* p_numpages_gotten,
        su_daddr_t prev_daddr_for_optimization,
        bool mmepage);

su_ret_t dbe_db_free_n_pages(
        dbe_db_t* db,
        size_t numpages,
        su_daddr_t* daddr_array,
        dbe_cpnum_t page_cpnum,
        bool mmepage);

void dbe_db_prefetch_n_pages(
        dbe_db_t* db,
        size_t numpages,
        su_daddr_t* daddr_array);

void dbe_db_setcheckpointing(
        dbe_db_t* db,
        bool setting);

bool dbe_db_getcheckpointing(
        dbe_db_t* db);

uint dbe_db_getreadaheadsize(
        dbe_db_t* db);

#ifdef SS_HSBG2

void dbe_db_hsbg2_mme_newdb(
        dbe_db_t*   db);

SS_INLINE dbe_hsbg2_t* dbe_db_gethsbsvc(
        dbe_db_t* db);

void dbe_db_hsbg2_sendandwaitdurablemark(
        dbe_db_t* db);

#endif /* SS_HSBG2 */


#ifdef SS_MME

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
        void* recovctx);

#if !defined(SS_PURIFY) && defined(SS_FFMEM)
void dbe_db_getmeminfo_mme(
        dbe_db_t* db,
        SsQmemStatT* p_mmememstat);
#endif /* !SS_PURIFY && SS_FFMEM */

void dbe_db_lockmmemutex(rs_sysi_t* cd, dbe_db_t* db);
void dbe_db_unlockmmemutex(rs_sysi_t* cd, dbe_db_t* db);

#endif /* SS_MME */

void dbe_db_getmeminfo(
        dbe_db_t* db,
        SsQmemStatT* p_memstat);

dbe_ret_t dbe_db_logblobg2dropmemoryref(
        rs_sysi_t* cd,
        dbe_db_t* db,
        dbe_blobg2id_t blobid);

#ifdef SS_HSBG2

void dbe_db_setreloadrbuffun(
        dbe_db_t* db,
        void (*reloaf_rbuf_fun)(void* ctx),
        void* ctx);

void dbe_db_callreloadrbuffun(
        dbe_db_t* db);

int dbe_db_abortall(
        dbe_db_t* db,
        bool* p_reload_rbuf);

void dbe_db_abortallactive(
        dbe_db_t* db);

void dbe_db_hsbabortalluncertain(
        dbe_db_t* db);

void dbe_db_hsbcommituncertain(
        dbe_db_t* db,
        dbe_trxid_t trxid);

bool dbe_db_getlogenabled(
        dbe_db_t* db);

#endif /* SS_HSBG2 */

void dbe_db_convert_init(
        dbe_db_t* db,
        bool *attrid_used,
        bool *keyid_used);

void dbe_db_convert_done(
        dbe_db_t* db);

void dbe_db_convert_set(
        dbe_db_t* db,
        bool b);

dbe_ret_t dbe_db_force_checkpoint(dbe_db_t* db);

void dbe_db_setfinalcheckpoint(dbe_db_t* db);

void dbe_db_endhsbg2migrate(
        dbe_db_t* db);

void dbe_db_logfnumrange(
        dbe_db_t* db,
        dbe_logfnum_t* p_logfnum_start,
        dbe_logfnum_t* p_logfnum_end);

void dbe_db_getdbandlogfilenames(dbe_db_t* db, su_list_t** fname_list);

bool dbe_db_deletedbbyfnamelist(su_list_t* fname_list);

SsTimeT dbe_db_getprimarystarttime(
        dbe_db_t*   db);

void dbe_db_setprimarystarttime(
        dbe_db_t*   db);

SsTimeT dbe_db_getsecondarystarttime(
        dbe_db_t*   db);

void dbe_db_setsecondarystarttime(
        dbe_db_t*   db);

void dbe_db_writedisklessmmepage(
        dbe_backupfiletype_t    ftype,
        ss_byte_t*              data,
        size_t                  len);

void dbe_db_cleardisklessmmepages(void);

SS_INLINE dbe_log_t* dbe_db_getlog(
        dbe_db_t* db);

void dbe_db_setoutofdiskspace(
        dbe_db_t* db,
        su_ret_t rc);

#if defined(DBE0DB_C) || defined(SS_USE_INLINE)

/*##**********************************************************************\
 *
 *              dbe_db_getgtrs
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
SS_INLINE dbe_gtrs_t* dbe_db_getgtrs(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_go->go_gtrs);
}

/*##**********************************************************************\
 *
 *              dbe_db_getgobj
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
SS_INLINE dbe_gobj_t* dbe_db_getgobj(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_go);
}

/*##**********************************************************************\
 *
 *              dbe_db_getlog
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
SS_INLINE dbe_log_t* dbe_db_getlog(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_dbfile->f_log);
}

/*##**********************************************************************\
 *
 *              dbe_db_gethsbsvc
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
SS_INLINE dbe_hsbg2_t* dbe_db_gethsbsvc(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_hsbg2svc);
}

/*##**********************************************************************\
 *
 *              dbe_db_getcounter
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
SS_INLINE dbe_counter_t* dbe_db_getcounter(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_go->go_ctr);
}

/*##**********************************************************************\
 *
 *              dbe_db_gettrxbuf
 *
 * Returns pointer to trxbuf object.
 *
 * Parameters :
 *
 *      db -
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
SS_INLINE void* dbe_db_gettrxbuf(
        dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_go->go_trxbuf);
}

/*##**********************************************************************\
 *
 *              dbe_db_getblobmgr
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
SS_INLINE dbe_blobmgr_t* dbe_db_getblobmgr(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_go->go_blobmgr);
}

/*##**********************************************************************\
 *
 *              dbe_db_getlockmgr
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
SS_INLINE dbe_lockmgr_t* dbe_db_getlockmgr(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_lockmgr);
}

/*##**********************************************************************\
 *
 *              dbe_db_getseq
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
SS_INLINE dbe_seq_t* dbe_db_getseq(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_seq);
}

SS_INLINE bool dbe_db_ishsb(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_ishsb);
}

/*##**********************************************************************\
 *
 *              dbe_db_getdbfile
 *
 * Get pointer to db file object
 *
 * Parameters :
 *
 *      db - in, use
 *              database object
 *
 * Return value - ref :
 *      pointer to db file object
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_file_t* dbe_db_getdbfile(dbe_db_t* db)
{
        CHK_DB(db);

        return (db->db_dbfile);
}

/*##**********************************************************************\
 *
 *              dbe_db_isearlyvld
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
SS_INLINE bool dbe_db_isearlyvld(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_earlyvld);
}

/*##**********************************************************************\
 *
 *              dbe_db_isreadonly
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
SS_INLINE bool dbe_db_isreadonly(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_readonly);
}

SS_INLINE dbe_hsbmode_t dbe_db_gethsbg2mode(dbe_db_t* db)
{
        if (db == NULL || db->db_hsbstate == NULL || db->db_hsb_enable_syschanges) {
            ss_dprintf_1(("dbe_db_gethsbg2mode:DBE_HSB_STANDALONE\n"));
            return(DBE_HSB_STANDALONE);
        }

        CHK_DB(db);

        return (dbe_hsbstate_getdbehsbmode(db->db_hsbstate));
}

SS_INLINE bool dbe_db_is2safe(dbe_db_t* db)
{
        bool is2safe;
        if (db == NULL || db->db_hsbstate == NULL || db->db_hsb_enable_syschanges) {
            return(FALSE);
        }

        CHK_DB(db);

        is2safe = dbe_hsbstate_is2safe(db->db_hsbstate);

        if (is2safe) {
            if (dbe_db_hsbg2safenesslevel_adaptive(db) &&
                dbe_db_getdurabilitylevel_raw(db) != DBE_DURABILITY_STRICT) {
                is2safe = FALSE;
            }
        }

        return(is2safe);
}

/*##**********************************************************************\
 *
 *              dbe_db_gettablelocktimeout
 *
 * Returns timeout value for table locks.
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
SS_INLINE long dbe_db_gettablelocktimeout(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_table_lock_to);
}


/*##**********************************************************************\
 *
 *              dbe_db_getlocktimeout
 *
 *
 *
 * Parameters :
 *
 *      db -
 *
 *
 *      p_pessimistic_lock_to -
 *
 *
 *      p_optimistic_lock_to -
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
SS_INLINE void dbe_db_getlocktimeout(
        dbe_db_t* db,
        long* p_pessimistic_lock_to,
        long* p_optimistic_lock_to)
{
        CHK_DB(db);

        if (p_pessimistic_lock_to != NULL) {
            *p_pessimistic_lock_to = db->db_pessimistic_lock_to;
        }
        if (p_optimistic_lock_to != NULL) {
            *p_optimistic_lock_to = db->db_optimistic_lock_to;
        }
}

/*##**********************************************************************\
 *
 *              dbe_db_getescalatelimits
 *
 * Returns escalate limit object.
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value - ref:
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_escalatelimits_t* dbe_db_getescalatelimits(dbe_db_t* db)
{
        CHK_DB(db);

        return(&db->db_escalatelimits);
}

/*##**********************************************************************\
 *
 *              dbe_db_getwritesetescalatelimit
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
SS_INLINE long dbe_db_getwritesetescalatelimit(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_escalatelimits.esclim_check);
}

/*##**********************************************************************\
 *
 *              dbe_db_getfatalerrcode
 *
 * Returns the database wide error code, or SU_SUCCESS if database
 * is not in error state.
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
SS_INLINE su_ret_t dbe_db_getfatalerrcode(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_fatalerrcode);
}

/*##**********************************************************************\
 *
 *              dbe_db_gethsbmode
 *
 * Gets replication mode after recovery
 *
 * Parameters :
 *
 *      db - use
 *              db object
 *
 * Return value :
 *      replication mode
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE dbe_hsbmode_t dbe_db_gethsbmode(dbe_db_t* db)
{
        if (db == NULL) {
            return(DBE_HSB_STANDALONE);
        }

        CHK_DB(db);

        return (db->db_hsbmode);
}

/*##**********************************************************************\
 *
 *              dbe_db_getindex
 *
 * Returns the index system associated to the database object.
 *
 * Parameters :
 *
 *      db - in, use
 *          Database object.
 *
 * Return value - ref :
 *
 *      Pointer to the index system object.
 *
 * Limitations  :
 *
 * Globals used :
 */
SS_INLINE dbe_index_t* dbe_db_getindex(dbe_db_t* db)
{
        CHK_DB(db);

        return(db->db_index);
}

SS_INLINE dbe_mme_t* dbe_db_getmme(
        dbe_db_t*       db)
{
        CHK_DB(db);

        return db->db_mme;
}

/*##**********************************************************************\
 *
 *              dbe_db_isbackupactive
 *
 * Check wheather backup is active
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value : true if backup is active
 *                false otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE bool dbe_db_isbackupactive(
        dbe_db_t* db)
{
        bool backup_active = FALSE;
        CHK_DB(db);

        SsSemEnter(db->db_sem);

        if (db->db_backup != NULL) {
            backup_active = TRUE;
        }

        SsSemExit(db->db_sem);

        return(backup_active);
}

/*##**********************************************************************\
 *
 *              dbe_db_iscpactive
 *
 * Check wheather checkpoint is active
 *
 * Parameters :
 *
 *      db -
 *
 *
 * Return value : true if checkpoint is active
 *                false otherwise
 *
 * Comments :
 *
 * Globals used :
 *
 * See also :
 */
SS_INLINE bool dbe_db_iscpactive(
        dbe_db_t* db)
{
        bool cp_active = FALSE;
        CHK_DB(db);

        SsSemEnter(db->db_sem);
        cp_active = db->db_cpactive;
        SsSemExit(db->db_sem);

        return(cp_active);
}


#endif /* defined(DBE0DB_C) || defined(SS_USE_INLINE) */

#endif /* DBE0DB_H */
